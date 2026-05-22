// SPDX-License-Identifier: GPL-2.0
#include <linker_symbol.h>
#include <linux/errno.h>
#include <cacheflush.h>
#include <memblock.h>
#include <mmu.h>
#include <pg_table.h>
#include <memory.h>

#define PTEP_SIZE ((EARLY_PAGES(PGTABLE_LEVELS, RAM_BASE, RAM_BASE + RAM_SIZE, 1) + \
					EARLY_IDMAP_EXTRA_PAGES) * PAGE_SIZE)

static u8 g_ptep[PTEP_SIZE] __aligned(PAGE_SIZE);

void linear_mapping_init(void)
{
	u64 g_pnext = (uintptr_t)virt_to_kimg_phys(g_ptep);
	u64 pnext_start = g_pnext;

	/*
	 * The linear mapping should cover the entire RAM range and
	 * should not separately isolate the kernel image region.
	 * Otherwise, during subsequent page table traversal,
	 * phys_to_virt cannot quickly distinguish whether a physical
	 * address stored in the page table belongs to the kernel image
	 * or another region. To ensure that the original permission
	 * protection semantics of the kernel image region are not
	 * compromised by aliasing through the linear mapping, we
	 * need to later correct the permissions of the kernel image's
	 * alias in the linear mapping.
	 */
	pg_map_range((u64 *)LDSYM_ADDR(swapper_pg_dir), (u64 *)&g_pnext,
			RAM_BASE, RAM_BASE + RAM_SIZE,
			(uintptr_t)phys_to_virt(RAM_BASE),
			(u64)PG_PAGE_ENTRY_NORMAL_CACHEABLE_RW,
			FLAGS_MAP_PUD_BLOCK | FLAGS_MAP_PMD_BLOCK);

	dcache_clean_poc(LDSYM_ADDR(swapper_pg_dir), LDSYM_ADDR(swapper_pg_dir) + PAGE_SIZE);
	if (g_pnext != pnext_start)
		dcache_clean_poc(pnext_start, g_pnext);

	asm volatile("dsb ishst" ::: "memory");
	asm volatile("tlbi vmalle1is" ::: "memory");
	asm volatile("dsb ish" ::: "memory");
	asm volatile("isb" ::: "memory");
}

int vmap_range(phys_addr_t phys, unsigned long virt, phys_addr_t size, pgprot_t prot)
{
	int err;

	err = vmap_phys_range_noflush(KERNEL_PGD, phys, virt, size, prot);
	return err;
}
