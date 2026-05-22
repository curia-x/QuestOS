
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <linux/types.h>
#include <ptrace.h>
#include <page.h>
#include <memory.h>

#define PROCESS_MAX_COUNT	128
#define PROCESS_MAX_NAME_LEN	128

#define PROCESS_MEM_HOLE_SIZE SZ_1G

#define PROCESS_IMAGE_START			0x40000000UL
#define PROCESS_IMAGE_SIZE			SZ_4G
#define PROCESS_IMAGE_END			(PROCESS_IMAGE_START + PROCESS_IMAGE_SIZE)
#define PROCESS_META_START			PROCESS_IMAGE_END
#define PROCESS_META_SIZE			SZ_1G
#define PROCESS_META_END			(PROCESS_META_START + PROCESS_META_SIZE)
#define PROCESS_HEAP_STACK_START	(PROCESS_META_END + PROCESS_MEM_HOLE_SIZE)
#define PROCESS_HEAP_STACK_SIZE		SZ_256G
#define PROCESS_HEAP_STACK_END		(PROCESS_HEAP_STACK_START + PROCESS_HEAP_STACK_SIZE)
#define PROCESS_STACK_TOP			(PROCESS_HEAP_STACK_END)

#define PROCESS_DEFAULT_KERNEL_STACK_SIZE THREAD_SIZE
#define PROCESS_DEFAULT_STACK_SIZE (2 * PAGE_SIZE)
#define PROCESS_MAX_STACK_SIZE	(16 * PAGE_SIZE)
#define PROCESS_DEFAULT_HEAP_SIZE (16 * PAGE_SIZE)

#define PROC_PKG_MAGIC		0x504B4750u  /* 'PGKP' */
#define PROC_MIN_VERSION	1
#define PROC_IMG_ELF		1
#define PROC_IMG_FLAT		2

struct proc_pkg_header {
	u32 magic;
	u16 version;
	u16 header_size;
	u32 image_count;
	u32 desc_offset;
	u32 desc_size;
	u32 reserved;
	u64 total_size;
};

struct proc_auxv_entry {
	u64 type;
	u64 value;
};

struct proc_image_desc {
    u32 type;
    u32 flags;

    u64 image_offset;
    u64 image_size;

    u64 meta_offset;
    u64 meta_size;

    u64 load_hint;	/* load address(for flat/raw binary) */
    u64 entry_hint;	/* entry point(for flat/raw binary) */
};

struct proc_image_meta {
    u32 argv_count;
    u32 env_count;
    u32 auxv_count;
	u32 string_table_size;

	u32 name_offset;
    u32 argv_offset;
    u32 env_offset;
    u32 auxv_offset;
    u32 string_table_offset;
	u32 reserved;

    u64 stack_size;
};

typedef enum process_state {
	PROCESS_NEW = 0,
	PROCESS_READY,
	PROCESS_RUNNING,
	PROCESS_BLOCKED,
	PROCESS_ZOMBIE,
} process_state_t;

struct vmem_segment {
	phys_addr_t	pa;
	unsigned long va_start;
	unsigned long va_end;
	unsigned long flags;
	unsigned long zero_start;
	unsigned long zero_size;
	struct vmem_segment *next;
};

#define PROCESS_MAX_ARGV_COUNT	128
#define PROCESS_MAX_ENV_COUNT	128
#define PROCESS_MAX_AUXV_COUNT	128
struct process_image {
	void *image_date;
	u32 image_date_size;

	void *meta_data;
	u32 meta_size;

	u32 argc;
	char *argv[PROCESS_MAX_ARGV_COUNT];

	u32 env_count;
	char *env[PROCESS_MAX_ENV_COUNT];

	u32 auxv_count;
	struct proc_auxv_entry *auxv[PROCESS_MAX_AUXV_COUNT];

	u64 stack_size;

	u64 entry_point;
};

struct pgtable_mem {
	void *virt;
	struct pgtable_mem *next;
};

struct memory_struct {
	void *pg_dir_unaligned;
	void *pg_dir;
	void *stack;
	unsigned long stack_size;
	void *heap;
	unsigned long heap_size;
	int vm_region_count;
	struct vmem_segment *vm_segments;
	struct vmem_segment *vm_region_last;
	struct pgtable_mem *pgtable_mem;
};

struct process_struct {
	u64 sp;
	char name[PROCESS_MAX_NAME_LEN];
	int pid;
	u64 pc;
	void *kernel_stack;
	u64 kernel_stack_size;
	u64 last_run_timestamp;
	struct process_image image;
	struct memory_struct mm;
	struct process_struct *last_wake;
	process_state_t state;
};

extern unsigned long init_stack[THREAD_SIZE / sizeof(unsigned long)];

void process_init(void);
extern void kernel_init_task_entry(void);

#endif /* __PROCESS_H__ */
