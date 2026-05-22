// SPDX-License-Identifier: GPL-2.0
#include <linux/types.h>
#include <tlbflush.h>
#include <pg_table.h>
#include <linker_symbol.h>
#include <memory.h>
#include <system.h>

#define NR_BM_PTE_TABLES \
	SPAN_NR_ENTRIES(FIXADDR_TOT_START, FIXADDR_TOP, PMD_SHIFT)
#define NR_BM_PMD_TABLES \
	SPAN_NR_ENTRIES(FIXADDR_TOT_START, FIXADDR_TOP, PUD_SHIFT)

#define __BM_TABLE_IDX(addr, shift) \
	(((addr) >> (shift)) - (FIXADDR_TOT_START >> (shift)))

#define BM_PTE_TABLE_IDX(addr)	__BM_TABLE_IDX(addr, PMD_SHIFT)

static pte_t bm_pte[NR_BM_PTE_TABLES][PTRS_PER_PTE] __aligned(PAGE_SIZE);
static pmd_t bm_pmd[PTRS_PER_PMD] __aligned(PAGE_SIZE) __maybe_unused;
static pud_t bm_pud[PTRS_PER_PUD] __aligned(PAGE_SIZE) __maybe_unused;

static inline pte_t *fixmap_pte(unsigned long addr)
{
	return &bm_pte[BM_PTE_TABLE_IDX(addr)][pte_index(addr)];
}

void __set_fixmap(enum fixed_addresses idx,
			       phys_addr_t phys, pgprot_t flags)
{
	unsigned long addr = __fix_to_virt(idx);
	pte_t *ptep;

	BUG_ON(idx <= FIX_HOLE || idx >= __end_of_fixed_addresses);

	ptep = fixmap_pte(addr);

	if (pgprot_val(flags)) {
		__set_pte(ptep, pfn_pte(phys >> PAGE_SHIFT, flags));
	} else {
		__set_pte(ptep, __pte(0));
		flush_tlb_all();
	}
}

static void fixmap_init_pmd(pmd_t *pmd, u64 va_start, u64 va_end)
{
	u64 *pmd_entry_start = PMD_ENTRY(pmd, va_start);
	u64 *pmd_entry_end = PMD_ENTRY(pmd, va_end - 1);
	// u64 *pte_table;

	do {
		*pmd_entry_start = virt_to_kimg_phys(bm_pte[BM_PTE_TABLE_IDX(va_start)]) |
							PG_DESC_TYPE_TABLE |
							PG_TABLE_ENTRY_RWX;
		pmd_entry_start++;
		va_start = pmd_addr_end(va_start, va_end);
	} while (pmd_entry_start <= pmd_entry_end);
}

static void fixmap_init_pud(pud_t *pud, u64 va_start, u64 va_end)
{
	u64 *pud_entry = PUD_ENTRY(pud, va_start);

	*pud_entry = virt_to_kimg_phys(bm_pmd) | PG_DESC_TYPE_TABLE | PG_TABLE_ENTRY_RWX;

	fixmap_init_pmd(bm_pmd, va_start, va_end);
}

void early_fixmap_init(void)
{
	u64 va_start = FIXADDR_TOT_START;
	u64 va_end = FIXADDR_TOP;

	u64 *pgd = (void *)LDSYM_ADDR(swapper_pg_dir);
	u64 *pud = PGD_ENTRY(pgd, FIXADDR_TOT_START);

	*pud = (u64)(virt_to_kimg_phys(bm_pud) | PG_DESC_TYPE_TABLE | PG_TABLE_ENTRY_RWX);

	fixmap_init_pud(bm_pud, va_start, va_end);
}
