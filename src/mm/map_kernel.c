// SPDX-License-Identifier: GPL-2.0
#include <linux/minmax.h>
#include <linux/linkage.h>
#include <asm/boot.h>
#include <asm/string.h>
#include <barrier.h>
#include <linker_symbol.h>
#include <pg_table.h>
#include <elf.h>
#include <memory.h>
#include <tlbflush.h>

static void map_fdt(u64 fdt)
{
	static u8 ptes[INIT_IDMAP_FDT_SIZE] __aligned(PAGE_SIZE);
	u64 efdt = fdt + MAX_FDT_SIZE;
	u64 ptep = (u64)ptes;

	/*
	 * Map up to MAX_FDT_SIZE bytes, but avoid overlap with
	 * the kernel image.
	 */
	pg_map_range((u64 *)LDSYM_ADDR(__pi_init_idmap_pg_dir), &ptep, fdt,
				 (u64)LDSYM_ADDR(_stext) > fdt ? min_t(u64, (u64)LDSYM_ADDR(_stext), efdt) : efdt,
				 fdt, (u64)_PAGE_KERNEL, FLAGS_MAP_PUD_BLOCK | FLAGS_MAP_PMD_BLOCK);
	dsb(ishst);
}

static void map_segment(u64 *pgd, u64 *pg_dir, u64 va_offset,
			       void *start, void *end, u64 prot)
{
	pg_map_range(pgd, pg_dir, (u64)start, (u64)end, (u64)start + va_offset, prot, FLAGS_MAP_PUD_BLOCK | FLAGS_MAP_PMD_BLOCK);
}

static void unmap_segment(u64 *pg_dir, u64 va_offset, void *start,
				 void *end)
{
	map_segment(pg_dir, NULL, va_offset, start, end, 0);
}

extern const Elf64_Rela __pi_rela_start[], __pi_rela_end[];

void relocate_kernel(void)
{
	const Elf64_Rela *rela;

	for (rela = (Elf64_Rela *)LDSYM_ADDR(__pi_rela_start); rela < (Elf64_Rela *)LDSYM_ADDR(__pi_rela_end); rela++) {
		if (ELF64_R_TYPE(rela->r_info) != R_AARCH64_RELATIVE)
			continue;
		*(u64 *)rela->r_offset = rela->r_addend;
	}
}

extern void idmap_cpu_replace_ttbr1(u64 pgd);

static void map_kernel(u64 va_offset)
{
	u64 pgdp = (u64)LDSYM_ADDR(__pi_init_pg_dir) + PAGE_SIZE;
	u64 text_prot = _PAGE_KERNEL_ROX;
	u64 data_prot = _PAGE_KERNEL;
	u64 prot;

	prot = data_prot;

	map_segment((u64 *)LDSYM_ADDR(__pi_init_pg_dir), &pgdp, va_offset, (void *)LDSYM_ADDR(_stext), (void *)LDSYM_ADDR(_etext), prot);
	map_segment((u64 *)LDSYM_ADDR(__pi_init_pg_dir), &pgdp, va_offset, (void *)LDSYM_ADDR(__start_rodata),
				(void *)LDSYM_ADDR(__init_begin), data_prot);
	map_segment((u64 *)LDSYM_ADDR(__pi_init_pg_dir), &pgdp, va_offset, (void *)LDSYM_ADDR(__inittext_begin),
				(void *)LDSYM_ADDR(__inittext_end), prot);
	map_segment((u64 *)LDSYM_ADDR(__pi_init_pg_dir), &pgdp, va_offset, (void *)LDSYM_ADDR(__initdata_begin),
		    (void *)LDSYM_ADDR(__initdata_end), data_prot);
	map_segment((u64 *)LDSYM_ADDR(__pi_init_pg_dir), &pgdp, va_offset, (void *)LDSYM_ADDR(_data), (void *)LDSYM_ADDR(kimage_end), data_prot);
	dsb(ishst);

	idmap_cpu_replace_ttbr1((u64)LDSYM_ADDR(__pi_init_pg_dir));

	relocate_kernel();

	/*
	 * Unmap the text region before remapping it, to avoid
	 * potential TLB conflicts when creating the contiguous
	 * descriptors.
	 */
	unmap_segment((u64 *)LDSYM_ADDR(__pi_init_pg_dir), va_offset, (void *)LDSYM_ADDR(_stext), (void *)LDSYM_ADDR(_etext));
	dsb(ishst);
	isb();
	__tlbi(vmalle1);
	isb();

	/*
	 * Remap these segments with different permissions
	 * No new page table allocations should be needed
	 */
	map_segment((u64 *)LDSYM_ADDR(__pi_init_pg_dir), NULL, va_offset, (void *)LDSYM_ADDR(_stext),
				(void *)LDSYM_ADDR(_etext), text_prot);
	map_segment((u64 *)LDSYM_ADDR(__pi_init_pg_dir), NULL, va_offset, (void *)LDSYM_ADDR(__inittext_begin),
				(void *)LDSYM_ADDR(__inittext_end), text_prot);

	/* Copy the root page table to its final location */
	memcpy((void *)(u64)LDSYM_ADDR(swapper_pg_dir) + va_offset, (void *)(u64)LDSYM_ADDR(__pi_init_pg_dir), PAGE_SIZE);
	dsb(ishst);
	idmap_cpu_replace_ttbr1((u64)LDSYM_ADDR(swapper_pg_dir));
}

asmlinkage void early_map_kernel(u64 boot_status, void *fdt)
{
	map_fdt((u64)fdt);

	/* Clear BSS and the initial page tables */
	memset((void *)LDSYM_ADDR(__bss_start), 0, (u64)LDSYM_ADDR(__pi_init_pg_end) - (u64)LDSYM_ADDR(__bss_start));

	map_kernel(IMAGE_VADDR - (u64)LDSYM_ADDR(_stext));
}

u64 mmu_init_idmap_table(u64 *pgd)
{
	u64 pnext = ((u64)pgd) + PAGE_SIZE;

	/* map rom */
	pg_map_range(pgd, &pnext, (u64)LDSYM_ADDR(_stext),
				 (u64)LDSYM_ADDR(__initdata_begin), (u64)LDSYM_ADDR(_stext), PG_PAGE_ENTRY_NORMAL_CACHEABLE_RX, FLAGS_MAP_PUD_BLOCK | FLAGS_MAP_PMD_BLOCK);

	/* map ram */
	pg_map_range(pgd, &pnext, (u64)LDSYM_ADDR(__initdata_begin),
				 (u64)LDSYM_ADDR(kimage_end), (u64)LDSYM_ADDR(__initdata_begin), PG_PAGE_ENTRY_NORMAL_CACHEABLE_RWX, FLAGS_MAP_PUD_BLOCK | FLAGS_MAP_PMD_BLOCK);

	return pnext;
}
