// SPDX-License-Identifier: GPL-2.0
#include <linux/compiler_attributes.h>
#include <pgtable-prot.h>
#include <cacheflush.h>
#include <print.h>
#include <linker_symbol.h>
#include <memory.h>
#include <pg_table.h>

#define PTEP_SIZE ((EARLY_PAGES(PGTABLE_LEVELS, MMIO_BASE, MMIO_BASE + MMIO_SIZE, 1) + \
					EARLY_IDMAP_EXTRA_PAGES) * PAGE_SIZE)

static u8 ptep[PTEP_SIZE] __aligned(PAGE_SIZE);

void map_mmio(void)
{
	u64 pnext = (uintptr_t)virt_to_kimg_phys(ptep);

	pg_map_range((void *)LDSYM_ADDR(__pi_init_idmap_pg_dir), &pnext, MMIO_BASE, MMIO_BASE + MMIO_SIZE,
				MMIO_BASE, (u64)PG_PAGE_ENTRY_DEVICE_RW, FLAGS_MAP_PUD_BLOCK | FLAGS_MAP_PMD_BLOCK);
	dcache_clean_poc((uintptr_t)ptep, (uintptr_t)ptep + sizeof(ptep));
}
