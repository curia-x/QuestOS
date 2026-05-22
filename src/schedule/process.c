// SPDX-License-Identifier: GPL-2.0
#include <linux/types.h>
#include <linux/errno.h>
#include <uapi/linux/elf.h>
#include <linker_symbol.h>
#include <print.h>
#include <process.h>
#include <sched.h>
#include <kmalloc.h>
#include <mm.h>
#include <memory.h>
#include <pg_table.h>
#include <string.h>

struct process_struct g_kernel_init_process = {
	.name = "idle",
	.pid = 0,
	.mm.pg_dir = swapper_pg_dir,
	.kernel_stack = init_stack,
	.kernel_stack_size = THREAD_SIZE,
};

static struct process_struct *g_processes[PROCESS_MAX_COUNT];
static int g_num_processes;

#define for_each_process(proc, idx) \
	for ((idx) = 0, (proc) = g_processes[0]; (idx) < g_num_processes; ++(idx), (proc) = g_processes[idx])

static int max_pid = 1;

#define foreach_mem_segment(mm, segment) \
	for (segment = mm->vm_segments; segment; segment = segment->next)

static pgprot_t process_get_segment_pgprot(struct vmem_segment *segment)
{
	u64 prot_val = PTE_SHARED | PTE_AF | PTE_PXN | PTE_USER | PTE_TYPE_PAGE;

	if (!(segment->flags & PF_X))
		prot_val |= PTE_UXN;

	if (segment->flags & PF_W)
		prot_val |= PTE_WRITE;
	else if (segment->flags & PF_R)
		prot_val |= PTE_RDONLY;
	else
		prot_val &= ~PTE_USER;

	return __pgprot(prot_val);
}

#define PGPROT_USER_RW __pgprot(PTE_SHARED | PTE_AF | PTE_WRITE | \
								PTE_PXN | PTE_UXN | PTE_USER | PTE_TYPE_PAGE)

static int process_map_memory(struct memory_struct *mm, phys_addr_t phys,
								unsigned long virt, u64 size, pgprot_t pgprot)
{
	int err;
	struct pgtable_mem *mem, *tmp;

	err = vmap_phys_range_noflush(mm, phys, virt, size, pgprot);
	if (err) {
		printf("process memory map failed:virt=0x%x, size=%llu\n", virt, size);
		goto fail_clean_up;
	}

	return 0;

fail_clean_up:
	while ((mem = mm->pgtable_mem)) {
		tmp = mem;
		mem = mem->next;
		kfree(tmp->virt);
		kfree(tmp);
	}

	return err;
}

static inline int
process_map_rw(struct memory_struct *mm, phys_addr_t phys,
				unsigned long virt, u64 size)
{
	return process_map_memory(mm, phys, virt, size, PGPROT_USER_RW);
}

static int process_map_segments(struct memory_struct *mm)
{
	int err;
	pgprot_t pgprot;
	struct vmem_segment *segment;

	foreach_mem_segment(mm, segment) {
		pgprot = process_get_segment_pgprot(segment);
		err = process_map_memory(mm, segment->pa, segment->va_start,
							segment->va_end - segment->va_start, pgprot);
		if (err) {
			printf("process segment:0x%x~0x%x map failed\n",
						segment->va_start, segment->va_end);
			return err;
		}
	}

	return 0;
}

static inline int process_map_stack(struct memory_struct *mm)
{
	return process_map_rw(mm, virt_to_phys((void *)mm->stack),
					PROCESS_STACK_TOP - mm->stack_size, mm->stack_size);
}

static inline int process_map_meta(struct process_struct *proc)
{
	return process_map_rw(&proc->mm, virt_to_kimg_phys((void *)proc->image.meta_data),
					PROCESS_META_START, PAGE_ALIGN_UP(proc->image.meta_size));
}

static u32 process_stack_init(struct process_struct *proc)
{
	struct memory_struct *mm = &proc->mm;
	struct process_image *img = &proc->image;
	void *sp_top = mm->stack + mm->stack_size;
	void *current_sp = sp_top;
	char *meta_base = img->meta_data;

	for (int i = img->auxv_count - 1; i >= 0; i--) {
		current_sp -= sizeof(struct proc_auxv_entry);
		memcpy(current_sp, img->auxv[i], sizeof(struct proc_auxv_entry));
	}

	for (int i = img->env_count - 1; i >= 0; i--) {
		current_sp -= sizeof(char *);
		*(char *)current_sp = img->env[i] - meta_base + PROCESS_META_START;
	}

	for (int i = img->argc - 1; i >= 0; i--) {
		current_sp -= sizeof(char *);
		*(char *)current_sp = img->argv[i] - meta_base + PROCESS_META_START;
	}

	current_sp -= sizeof(u32);
	*(u32 *)current_sp = img->argc;

	return sp_top - current_sp;
}

static int process_memory_init(struct process_struct *proc)
{
	int err;
	u64 stack_size;
	struct memory_struct *mm = &proc->mm;

	stack_size = proc->image.stack_size;
	if (stack_size > PROCESS_MAX_STACK_SIZE) {
		printf("process stack size too large, set to 0x%llx\n", PROCESS_MAX_STACK_SIZE);
		stack_size = PROCESS_MAX_STACK_SIZE;
	} else if (stack_size == 0) {
		printf("set to default stack size 0x%llx\n", PROCESS_DEFAULT_STACK_SIZE);
		stack_size = PROCESS_DEFAULT_STACK_SIZE;
	}

	mm->stack = kmalloc(stack_size, __GFP_ZERO);
	if (!mm->stack) {
		printf("process stack alloc failed\n");
		return -ENOMEM;
	}
	mm->stack_size = stack_size;

	mm->heap = kmalloc(PROCESS_DEFAULT_HEAP_SIZE, __GFP_ZERO);
	if (!mm->heap) {
		printf("process heap alloc failed\n");
		err = -ENOMEM;
		goto heap_alloc_fail;
	}
	mm->heap_size = PROCESS_DEFAULT_HEAP_SIZE;

	mm->pg_dir_unaligned = kmalloc(PAGE_SIZE, __GFP_ZERO);
	if (!mm->pg_dir_unaligned) {
		printf("process page table alloc failed\n");
		err = -ENOMEM;
		goto pg_tbl_alloc_fail;
	}
	mm->pg_dir = (void *)PAGE_ALIGN((unsigned long)mm->pg_dir_unaligned);

	err = process_map_segments(&proc->mm);
	if (err) {
		printf("process_map_segments failed, err=%d\n", err);
		goto memory_map_error;
	}

	err = process_map_meta(proc);
	if (err) {
		printf("process_map_meta failed, err=%d\n", err);
		goto memory_map_error;
	}

	err = process_map_stack(&proc->mm);
	if (err) {
		printf("process_map_stack failed, err=%d\n", err);
		goto memory_map_error;
	}

	return 0;

memory_map_error:
	memset(mm->pg_dir, 0, PAGE_SIZE);
pg_tbl_alloc_fail:
	kfree(mm->heap);

heap_alloc_fail:
	kfree(mm->stack);

	return err;
}

void process_destroy(struct process_struct *proc)
{
	int i;
	struct vmem_segment *segment, *tmp;

	if (!proc)
		return;

	segment = proc->mm.vm_segments;
	for (int i = 0; i < proc->mm.vm_region_count; i++) {
		tmp = segment;
		segment = segment->next;
		kfree(tmp);
	}

	kfree(proc);

	for (i = 0; i < g_num_processes; i++) {
		if (g_processes[i] != proc)
			continue;

		while (i < (g_num_processes - 1)) {
			g_processes[i] = g_processes[i + 1];
			++i;
		}
	}

	g_num_processes--;
}

#include <system_reg.h>

#define INIT_EL0_PSTATE	(PSR_DIT_BIT | PSR_MODE_EL0t)

static inline void process_calculate_init_regval(struct process_struct *proc, u64 sp_el0)
{
	struct user_pt_regs *regs;

	/* kernel stack */
	proc->sp = (u64)proc->kernel_stack + THREAD_SIZE - sizeof(struct user_pt_regs);
	regs = (struct user_pt_regs *)proc->sp;
	memset(regs, 0, sizeof(struct user_pt_regs));
	regs->pc = proc->image.entry_point;
	regs->sp = sp_el0;
	regs->pstate = INIT_EL0_PSTATE;
}

void user_processes_struct_init(void)
{
	int err, i;
	struct process_struct *proc;
	u64 sp_el0 = PROCESS_STACK_TOP;

	for_each_process(proc, i) {
		err = process_memory_init(proc);
		if (err) {
			printf("process %d memory init failed\n", i);
			process_destroy(proc);
		}

		proc->kernel_stack = kzalloc(PROCESS_DEFAULT_KERNEL_STACK_SIZE, 0);
		if (!proc->kernel_stack) {
			printf("process %d kernel stack alloc failed\n", i);
			memset(proc->mm.pg_dir, 0, PAGE_SIZE);
			kfree(proc->mm.heap);
			kfree(proc->mm.stack);
			process_destroy(proc);
		}

		sp_el0 -= process_stack_init(proc);

		process_calculate_init_regval(proc, sp_el0);

		proc->pid = max_pid++;
	}
}

static int check_pkg_header(void *base, u64 blob_size)
{
	u64 desc_end;
	u64 payload_start;
	struct proc_pkg_header *h = base;

	if (blob_size < sizeof(*h))
		return -EINVAL;

	if (h->magic != PROC_PKG_MAGIC)
		return -EINVAL;

	if (h->version < 1)
		return -EINVAL;

	if (h->header_size < sizeof(struct proc_pkg_header) || h->header_size > blob_size)
		return -EINVAL;

	if (h->total_size < h->header_size || h->total_size > blob_size)
		return -EINVAL;

	if (h->image_count == 0)
		return -EINVAL;

	if (h->image_count > U32_MAX / sizeof(struct proc_image_desc) ||
		h->desc_size != h->image_count * sizeof(struct proc_image_desc))
		return -EINVAL;

	if (h->desc_offset < h->header_size || h->desc_offset >= h->total_size)
		return -EINVAL;

	if (h->desc_size > h->total_size - h->desc_offset)
		return -EINVAL;

	desc_end = h->desc_offset + h->desc_size;
	payload_start = ALIGN(desc_end, PAGE_SIZE);

	if (payload_start > h->total_size)
		return -EINVAL;

	return 0;
}

#define PROCESS_MAX_IMAGE_SIZE	SZ_128M
#define PROCESS_MAX_META_SIZE	SZ_1M

static int check_image_desc(struct proc_pkg_header *h, struct proc_image_desc *d)
{
	u64 payload_start = ALIGN(h->desc_offset + h->desc_size, PAGE_SIZE);

	if (d->image_offset >= h->total_size || d->meta_offset >= h->total_size) {
		printf("desc error: wrong offset, image=0x%llx, meta=0x%llx, pkg size=0x%llx\n",
				(unsigned long long)d->image_offset,
				(unsigned long long)d->meta_offset,
				(unsigned long long)h->total_size);
		return -EINVAL;
	}

	if ((d->image_offset < payload_start) || (d->meta_offset < payload_start)) {
		printf("desc error: offset error, image=0x%llx, meta=0x%llx, payload start=0x%llx\n",
				(unsigned long long)d->image_offset,
				(unsigned long long)d->meta_offset,
				(unsigned long long)payload_start);
		return -EINVAL;
	}

	if (d->image_size == 0 || d->image_size > PROCESS_MAX_IMAGE_SIZE ||
		d->meta_size == 0 || d->meta_size > PROCESS_MAX_META_SIZE) {
		printf("desc error: wrong size, image=%llu, meta=%llu\n",
			(unsigned long long)d->image_size,
			(unsigned long long)d->meta_size);
		return -EINVAL;
	}

	if ((d->image_size > h->total_size - d->image_offset) ||
		(d->meta_size > h->total_size - d->meta_offset)) {
		printf("desc error: size error, image/meta offset:0x%llx/0x%llx, image/meta size:%llu/%llu, pkg size=%llu\n",
				(unsigned long long)d->image_offset,
				(unsigned long long)d->meta_offset,
				(unsigned long long)d->image_size,
				(unsigned long long)d->meta_size,
				(unsigned long long)h->total_size);
		return -EINVAL;
	}

	/* 4k alignment */
	if ((d->image_offset & 0xfff) != 0 || (d->meta_offset & 0xfff) != 0) {
		printf("desc error: offset not 4k align, image=0x%llx, meta=0x%llx\n",
				(unsigned long long)d->image_offset,
				(unsigned long long)d->meta_offset);
		return -EINVAL;
	}

	switch (d->type) {
	case PROC_IMG_ELF:
	case PROC_IMG_FLAT:
		return 0;
	default:
		printf("desc error: unknown desc type, %d\n", d->type);
		return -EINVAL;
	}
}

static int check_elf64_aarch64(void *base, uint64_t size)
{
	Elf64_Ehdr *eh = base;

	if (size < sizeof(Elf64_Ehdr))
		return -EINVAL;

	if (eh->e_ident[EI_MAG0] != ELFMAG0 ||
		eh->e_ident[EI_MAG1] != ELFMAG1 ||
		eh->e_ident[EI_MAG2] != ELFMAG2 ||
		eh->e_ident[EI_MAG3] != ELFMAG3)
		return -EINVAL;

	if (eh->e_ident[EI_CLASS] != ELFCLASS64)
		return -EINVAL;

	if (eh->e_ident[EI_DATA] != ELFDATA2LSB)
		return -EINVAL;

	if (eh->e_machine != EM_AARCH64)
		return -EINVAL;

	if (eh->e_phentsize != sizeof(Elf64_Phdr))
		return -EINVAL;

	if (eh->e_phnum == 0)
		return -EINVAL;

	if (eh->e_phoff + eh->e_phnum * sizeof(Elf64_Phdr) > size)
		return -EINVAL;

	return 0;
}

static int check_elf_ph(Elf64_Phdr *ph, u64 elf_size)
{
	if (ph->p_offset > elf_size)
		return -EINVAL;

	if (ph->p_filesz > elf_size - ph->p_offset)
		return -EINVAL;

	if (ph->p_memsz < ph->p_filesz)
		return -EINVAL;

	if ((ph->p_vaddr & 0xfff) != (ph->p_offset & 0xfff))
		return -EINVAL;

	return 0;
}

static int check_vmem_region_overlap(struct process_struct *process, u64 start, u64 end)
{
	struct vmem_segment *segment = process->mm.vm_segments;

	if (process->mm.vm_region_count == 0)
		return 0;

	while (segment) {
		if (segment->va_start >= end || segment->va_end <= start)
			segment = segment->next;
		else
			return -EINVAL;
	}

	return 0;
}

static int check_section_region_valid(struct process_struct *process, Elf64_Phdr *ph)
{
	int err;

	if (ph->p_vaddr < PROCESS_IMAGE_START ||
		(ph->p_vaddr + ph->p_memsz) > PROCESS_IMAGE_END ||
		(ph->p_vaddr + ph->p_memsz) < ph->p_vaddr) {
		printf("segment:0x%x ~ 0x%x invalid\n", ph->p_vaddr, ph->p_vaddr + ph->p_memsz);
		return -EINVAL;
	}

	err = check_vmem_region_overlap(process, ph->p_vaddr, ph->p_memsz);
	if (err) {
		printf("segment overlap with exist segment\n");
		return err;
	}

	return 0;
}

static inline void process_add_to_global(struct process_struct *process)
{
	g_processes[g_num_processes++] = process;
}

static int load_user_elf(void *elf_base, u64 elf_size, struct process_struct **out_process)
{
	int err;
	Elf64_Ehdr *eh;
	Elf64_Phdr *ph;
	struct process_struct *process;
	struct vmem_segment *segment, *tmp;

	err = check_elf64_aarch64(elf_base, elf_size);
	if (err) {
		printf("%s, check elf header failed, base:0x%x, size0x%x\n", __func__, elf_base, elf_size);
		return err;
	}

	eh = elf_base;
	ph = (void *)((char *)elf_base + eh->e_phoff);

	process = kzalloc(sizeof(struct process_struct), 0);
	if (!process) {
		printf("process_struct alloc failed, no enough memory.\n");
		return -ENOMEM;
	}

	process->image.image_date = elf_base;
	process->image.image_date_size = elf_size;

	for (int i = 0; i < eh->e_phnum; i++, ph++) {
		if (ph->p_type != PT_LOAD)
			continue;

		err = check_elf_ph(ph, elf_size);
		if (err) {
			printf("segment[%d] invalid.\n", i);
			goto section_load_failed;
		}

		err = check_section_region_valid(process, ph);
		if (err) {
			printf("check_vmem_region_valid failed, err:%d\n", err);
			goto section_load_failed;
		}

		segment = kzalloc(sizeof(struct vmem_segment), 0);
		segment->va_start = ph->p_vaddr;
		segment->va_end = ph->p_vaddr + ph->p_memsz;
		segment->pa = virt_to_kimg_phys(elf_base + ph->p_offset);
		segment->next = NULL;
		segment->flags = ph->p_flags;
		segment->zero_start = (unsigned long)elf_base + ph->p_offset + ph->p_filesz;
		segment->zero_size = ph->p_memsz - ph->p_filesz;
		process->mm.vm_region_count++;

		if (!process->mm.vm_region_last) {
			process->mm.vm_region_last = segment;
			process->mm.vm_segments = segment;
		} else {
			process->mm.vm_region_last->next = segment;
			process->mm.vm_region_last = segment;
		}
	}

	process->image.entry_point = eh->e_entry;

	*out_process = process;

	return 0;

section_load_failed:
	segment = process->mm.vm_segments;
	while (segment) {
		tmp = segment;
		segment = segment->next;
		kfree(tmp);
	}

	kfree(process);

	return err;
}

static void free_user_elf(struct process_struct *process)
{
	struct vmem_segment *tmp;
	struct vmem_segment *segment = process->mm.vm_segments;

	while (segment) {
		tmp = segment;
		segment = segment->next;
		kfree(tmp);
	}

	kfree(process);
}

static int check_meta_array(u32 offset, u32 count, u64 elem_size, u64 meta_size)
{
	u64 bytes;

	if (count == 0)
		return 0;

	if (offset >= meta_size)
		return -EINVAL;

	bytes = (u64)count * elem_size;

	if (bytes > meta_size - offset)
		return -EINVAL;

	return 0;
}

static int check_string_ref(const char *string_table, u32 string_table_size,
			    u32 str_off, const char *name, int index)
{
	size_t remain;
	size_t len;

	if (str_off >= string_table_size) {
		printf("%s[%d] string offset out of range: off=%u, size=%u\n",
		       name, index, str_off, string_table_size);
		return -EINVAL;
	}

	remain = string_table_size - str_off;
	len = strnlen(string_table + str_off, remain);

	if (len == remain) {
		printf("%s[%d] string is not nul-terminated\n", name, index);
		return -EINVAL;
	}

	return 0;
}

static int check_image_meta(struct proc_image_meta *meta, u64 meta_size)
{
	if (meta_size < sizeof(*meta))
		return -EINVAL;

	if (meta->argv_count > PROCESS_MAX_ARGV_COUNT ||
		meta->env_count > PROCESS_MAX_ENV_COUNT ||
		meta->auxv_count > PROCESS_MAX_AUXV_COUNT) {
		printf("meta count exceed the limit, argc:%u, env:%u, auxv:%u\n",
			meta->argv_count, meta->env_count, meta->auxv_count);
		return -EINVAL;
	}

	if (check_meta_array(meta->argv_offset,
			     meta->argv_count,
			     sizeof(u32),
			     meta_size) ||
	    check_meta_array(meta->env_offset,
			     meta->env_count,
			     sizeof(u32),
			     meta_size) ||
	    check_meta_array(meta->auxv_offset,
			     meta->auxv_count,
			     sizeof(struct proc_auxv_entry),
			     meta_size)) {
		printf("image meta: offset/count range error\n");
		printf("\tmeta size=%llu\n", (unsigned long long)meta_size);
		printf("\targv count:%u, argv_offset:0x%x\n",
		       meta->argv_count, meta->argv_offset);
		printf("\tenv count:%u, env_offset:0x%x\n",
		       meta->env_count, meta->env_offset);
		printf("\tauxv count:%u, auxv_offset:0x%x\n",
		       meta->auxv_count, meta->auxv_offset);
		return -EINVAL;
	}

	if (meta->auxv_count && (meta->auxv_offset & 0x7)) {
		printf("image meta: auxv_offset is not 8-byte aligned: 0x%x\n",
		       meta->auxv_offset);
		return -EINVAL;
	}

	if (meta->string_table_offset > meta_size ||
	    meta->string_table_size > meta_size - meta->string_table_offset) {
		printf("image meta: string table range error, offset=0x%x, size=%u, meta size=%llu\n",
		       meta->string_table_offset,
		       meta->string_table_size,
		       (unsigned long long)meta_size);
		return -EINVAL;
	}

	if ((meta->argv_count || meta->env_count) && meta->string_table_size == 0) {
		printf("image meta: empty string table with argv/env entries\n");
		return -EINVAL;
	}

	char *base = (char *)meta;
	char *string_table = base + meta->string_table_offset;

	if (check_string_ref(string_table,
		     meta->string_table_size,
		     meta->name_offset,
		     "name",
		     0))
		return -EINVAL;

	u32 *argv_offsets = (u32 *)(base + meta->argv_offset);

	for (int i = 0; i < meta->argv_count; i++) {
		if (check_string_ref(string_table,
				     meta->string_table_size,
				     argv_offsets[i],
				     "argv",
				     i))
			return -EINVAL;
	}

	u32 *env_offsets = (u32 *)(base + meta->env_offset);

	for (int i = 0; i < meta->env_count; i++) {
		if (check_string_ref(string_table,
				     meta->string_table_size,
				     env_offsets[i],
				     "env",
				     i))
			return -EINVAL;
	}

	if (meta->reserved != 0)
		printf("image meta warning: reserved field should be zero.\n");

	/* stack size will be checked when allocating stack space for process */
	if (meta->stack_size & (PAGE_SIZE - 1)) {
		printf("image meta: stack size should be aligned to PAGE_SIZE\n");
		return -EINVAL;
	}

	return 0;
}

static int
load_user_meta(void *meta_base, u64 meta_size, struct process_struct *process)
{
	struct proc_image_meta *meta = meta_base;
	char *string_table;
	u32 *argv_offsets;
	u32 *env_offsets;
	struct proc_auxv_entry *auxv_array;

	int err;

	err = check_image_meta(meta, meta_size);
	if (err) {
		printf("image meta check failed, err=%d\n", err);
		return err;
	}

	process->image.meta_data = meta_base;
	process->image.meta_size = meta_size;

	string_table = (char *)meta + meta->string_table_offset;

	argv_offsets = (u32 *)((char *)meta + meta->argv_offset);
	env_offsets = (u32 *)((char *)meta + meta->env_offset);
	auxv_array = (struct proc_auxv_entry *)((char *)meta + meta->auxv_offset);

	process->image.argc = meta->argv_count;
	process->image.env_count = meta->env_count;
	process->image.auxv_count = meta->auxv_count;
	process->image.stack_size = meta->stack_size;

	strscpy(process->name, string_table + meta->name_offset, PROCESS_MAX_NAME_LEN);

	for (u32 i = 0; i < meta->argv_count; i++) {
		u32 offset = argv_offsets[i];

		process->image.argv[i] = string_table + offset;
	}

	for (u32 i = 0; i < meta->env_count; i++) {
		u32 offset = env_offsets[i];

		process->image.env[i] = string_table + offset;
	}

	for (u32 i = 0; i < meta->auxv_count; i++)
		process->image.auxv[i] = &auxv_array[i];

	return 0;
}

static int load_one_image(struct proc_pkg_header *header, u32 index)
{
	int err = 0;
	u64 image_size, meta_size;
	void *package_base, *image_base, *meta_base;
	struct process_struct *process;
	struct proc_image_desc *desc;

	printf("desc_offset:0x%x, desc_size:0x%x, index:%d\n",
			header->desc_offset, header->desc_size, index);

	package_base = header;

	desc = ((void *)header) + header->desc_offset + index * sizeof(struct proc_image_desc);
	err = check_image_desc(header, desc);
	if (err) {
		printf("%s, check image desc failed, index:%d, err=%d\n", __func__, index, err);
		return err;
	}

	image_base = package_base + desc->image_offset;
	image_size = desc->image_size;

	meta_base = package_base + desc->meta_offset;
	meta_size = desc->meta_size;

	if (desc->type == PROC_IMG_ELF) {
		err = load_user_elf(image_base, image_size, &process);
		if (err) {
			printf("load_user_elf failed, err:0x%x\n", err);
			return err;
		}

		err = load_user_meta(meta_base, meta_size, process);
		if (err) {
			printf("load_user_meta failed, err:0x%x\n", err);
			free_user_elf(process);
			return err;
		}
	} else {
		printf("Image type 0x%x currently not supported\n", desc->type);
		return -EINVAL;
	}

	process_add_to_global(process);

	return 0;
}

static int load_processes(void)
{
	int err;
	u32 image_count;
	u32 loaded_count = 0;
	struct proc_pkg_header *header;

	header = (void *)LDSYM_ADDR(__process_blob_start);

	err = check_pkg_header(header, LDSYM_ADDR(__process_blob_end) - LDSYM_ADDR(__process_blob_start));
	if (err) {
		printf("No valid package header found.\n");
		return -ENOENT;
	}

	image_count = header->image_count;

	for (int i = 0; i < image_count; i++) {
		if (g_num_processes >= PROCESS_MAX_COUNT) {
			printf("process count exceed the limit:%d\n", PROCESS_MAX_COUNT);
			break;
		}

		err = load_one_image(header, i);
		if (err) {
			printf("Load image %d failed\n", i);
			continue;
		}

		loaded_count++;
	}

	return loaded_count;
}

static void user_processes_register(void)
{
	int err;

	for (int i = 0; i < g_num_processes; i++) {
		err = sched_register_process(g_processes[i]);
		if (err)
			printf("process %d register failed\n", i);
	}
}

static void user_processes_init(void)
{
	int count;

	count = load_processes();
	if (count <= 0) {
		printf("No process loaded.\n");
		return;
	}

	g_num_processes = count;

	printf("Load %d processes\n", count);

	user_processes_struct_init();
	user_processes_register();
}

void process_init(void)
{
	user_processes_init();
}

void __noreturn __init switch_to_kernel_init_task(void)
{
	struct process_struct *process = &g_kernel_init_process;

	process->pc = (u64)kernel_init_task_entry;
	process->sp = (u64)process->kernel_stack + process->kernel_stack_size - sizeof(struct user_pt_regs);
	process->sp &= ~0xfUL;

	asm volatile(
		"msr	sp_el0, %0\n"
		"mov	sp, %1\n"
		"mov	x29, xzr\n"	/* optional: clear frame pointer */
		"mov	x30, xzr\n"	/* optional: invalid return address */
		"br		%2\n"
		:
		: "r"(process), "r"(process->sp), "r"(process->pc)
		: "memory"
	);

	unreachable();
}
