// SPDX-License-Identifier: GPL-2.0
#include <asm-generic/memory_model.h>
#include <linker_symbol.h>
#include <linux/types.h>
#include <linux/minmax.h>
#include <linux/string.h>
#include <vdso/page.h>
#include <pg_table.h>
#include <print.h>
#include <memblock.h>
#include <system.h>
#include <mmu.h>

#define NO_BLOCK_MAPPINGS	BIT(0)
#define NO_CONT_MAPPINGS	BIT(1)
#define NO_EXEC_MAPPINGS	BIT(2)	/* assumes FEAT_HPDS is not used */

static inline bool pgd_entry_present(u64 *pgd_entry)
{
	return (*pgd_entry & 1) != 0;
}

static inline bool pud_entry_present(u64 *pud_entry)
{
	return (*pud_entry & 1) != 0;
}

static inline bool pmd_entry_present(u64 *pmd_entry)
{
	return (*pmd_entry & 1) != 0;
}

static void pte_map_range(u64 *pte, u64 *pnext, u64 start, u64 end, u64 va_start, u64 attr)
{
	u64 va_end = va_start + (end - start);
	u64 *pte_entry_start = PTE_ENTRY(pte, va_start);
	u64 *pte_entry_end = PTE_ENTRY(pte, PTE_ALIGN_UP(va_end) - 1);

	while (pte_entry_start <= pte_entry_end) {
		*pte_entry_start = (start & PAGE_MASK) | attr | PG_DESC_TYPE_PAGE;
		start += PAGE_SIZE;
		pte_entry_start++;
	}
}

static void pmd_map_range(u64 *pmd, u64 *pnext, u64 start, u64 end, u64 va_start, u64 attr, u32 flags)
{
	u64 va_end = va_start + (end - start);
	u64 *pmd_entry_start = PMD_ENTRY(pmd, va_start);
	u64 *pmd_entry_end = PMD_ENTRY(pmd, PMD_ALIGN_UP(va_end) - 1);
	u64 pte_map_end;
	u64 map_size;
	/* Table entry always have rwx. */
	u64 pmd_table_attr = PG_DESC_TYPE_TABLE | PG_TABLE_ENTRY_RWX;

	do {
		/* map range with pmd block */
		if (!(va_start & ((1ULL << PMD_SHIFT) - 1)) &&
			!(start & ((1ULL << PMD_SHIFT) - 1)) &&
			(va_start + (1ULL << PMD_SHIFT) <= va_end) &&
			flags & FLAGS_MAP_PMD_BLOCK) {
			*pmd_entry_start = start | attr | PG_DESC_TYPE_BLOCK;
			start += 1 << PMD_SHIFT;
			va_start += 1 << PMD_SHIFT;
			pmd_entry_start++;
			continue;
		}

		/* create pte table and link it */
		if (!pmd_entry_present(pmd_entry_start)) {
			*pmd_entry_start = (*pnext & PAGE_MASK) | pmd_table_attr;
			*pnext += PAGE_SIZE;
		}

		map_size = ((va_start + (1ULL << PMD_SHIFT)) & ~((1ULL << PMD_SHIFT) - 1)) - va_start;
		pte_map_end = min(start + map_size, end);
		map_size = pte_map_end - start;
		pte_map_range((u64 *)PG_TABLE_ENTRY_GET_ADDR(*pmd_entry_start), pnext, start, pte_map_end, va_start, attr);

		va_start += map_size;
		start = pte_map_end;
		pmd_entry_start++;
	} while (pmd_entry_start <= pmd_entry_end);
}

static void pud_map_range(u64 *pud, u64 *pnext, u64 start, u64 end, u64 va_start, u64 attr, u32 flags)
{
	u64 va_end = va_start + (end - start);
	u64 *pud_entry_start = PUD_ENTRY(pud, va_start);
	u64 *pud_entry_end = PUD_ENTRY(pud, PUD_ALIGN_UP(va_end) - 1);
	u64 pmd_map_end;
	u64 map_size;
	/* Table entry always have rwx. */
	u64 pud_table_attr = PG_DESC_TYPE_TABLE | PG_TABLE_ENTRY_RWX;

	do {
		/* map range with pud block */
		if (!(va_start & ((1ULL << PUD_SHIFT) - 1)) &&
			!(start & ((1ULL << PUD_SHIFT) - 1)) &&
			va_start + (1ULL << PUD_SHIFT) <= va_end &&
			flags & FLAGS_MAP_PUD_BLOCK) {
			*pud_entry_start = start | attr | PG_DESC_TYPE_BLOCK;
			start += 1 << PUD_SHIFT;
			va_start += 1 << PUD_SHIFT;
			pud_entry_start++;
			continue;
		}

		/* create pmd table and link it */
		if (!pud_entry_present(pud_entry_start)) {
			*pud_entry_start = (*pnext & PAGE_MASK) | pud_table_attr;
			*pnext += PAGE_SIZE;
		}

		map_size = ((va_start + (1ULL << PUD_SHIFT)) & ~((1ULL << PUD_SHIFT) - 1)) - va_start;
		pmd_map_end = min(start + map_size, end);
		map_size = pmd_map_end - start;
		pmd_map_range((u64 *)PG_TABLE_ENTRY_GET_ADDR(*pud_entry_start), pnext, start, pmd_map_end, va_start, attr, flags);

		va_start += map_size;
		start = pmd_map_end;
		pud_entry_start++;
	} while (pud_entry_start <= pud_entry_end);
}

void pg_map_range(u64 *pgd, u64 *pnext, u64 start, u64 end, u64 va_start, u64 attr, u32 flags)
{
	u64 *pgd_entry_start;
	u64 *pgd_entry_end;
	u64 pud_map_end;
	u64 va_end;
	u64 map_size;
	/* Table entry always have rwx. */
	u64 pgd_table_attr = PG_TABLE_ENTRY_RWX | PG_DESC_TYPE_TABLE;

	start = PAGE_ALIGN_DOWN(start);
	end = PAGE_ALIGN_UP(end);

	va_start = PAGE_ALIGN_DOWN(va_start);
	va_end = va_start + (end - start);

	pgd_entry_start = PGD_ENTRY(pgd, va_start);
	pgd_entry_end = PGD_ENTRY(pgd, PGD_ALIGN_UP(va_end) - 1);

	do {
		/* create pgd table and link it */
		if (!pgd_entry_present(pgd_entry_start)) {
			*pgd_entry_start = (*pnext & PAGE_MASK) | pgd_table_attr;
			*pnext += PAGE_SIZE;
		}

		map_size = ((va_start + (1ULL << PGDIR_SHIFT)) & ~((1ULL << PGDIR_SHIFT) - 1)) - va_start;
		pud_map_end = min(start + map_size, end);
		map_size = pud_map_end - start;
		pud_map_range((u64 *)PG_TABLE_ENTRY_GET_ADDR(*pgd_entry_start), pnext, start, pud_map_end, va_start, attr, flags);

		va_start += map_size;
		start = pud_map_end;
		pgd_entry_start++;
	} while (pgd_entry_start <= pgd_entry_end);
}

/* Reference to kernel implemention. */

void set_swapper_pgd(pgd_t *pgdp, pgd_t pgd)
{
	*pgdp = pgd;
	dsb(ishst);
	isb();
}

bool pgattr_change_is_safe(pteval_t old, pteval_t new)
{
	/*
	 * The following mapping attributes may be updated in live
	 * kernel mappings without the need for break-before-make.
	 */
	pteval_t mask = PTE_PXN | PTE_RDONLY | PTE_WRITE | PTE_NG |
			PTE_SWBITS_MASK;

	/* creating or taking down mappings is always safe */
	if (!pte_valid(__pte(old)) || !pte_valid(__pte(new)))
		return true;

	/* A live entry's pfn should not change */
	if (pte_pfn(__pte(old)) != pte_pfn(__pte(new)))
		return false;

	/* live contiguous mappings may not be manipulated at all */
	if ((old | new) & PTE_CONT)
		return false;

	/* Transitioning from Non-Global to Global is unsafe */
	if (old & ~new & PTE_NG)
		return false;

	/*
	 * Changing the memory type between Normal and Normal-Tagged is safe
	 * since Tagged is considered a permission attribute from the
	 * mismatched attribute aliases perspective.
	 */
	if (((old & PTE_ATTRINDX_MASK) == PTE_ATTRINDX(MT_NORMAL) ||
	     (old & PTE_ATTRINDX_MASK) == PTE_ATTRINDX(MT_NORMAL_TAGGED)) &&
	    ((new & PTE_ATTRINDX_MASK) == PTE_ATTRINDX(MT_NORMAL) ||
	     (new & PTE_ATTRINDX_MASK) == PTE_ATTRINDX(MT_NORMAL_TAGGED)))
		mask |= PTE_ATTRINDX_MASK;

	return ((old ^ new) & ~mask) == 0;
}

int pud_set_huge(pud_t *pudp, phys_addr_t phys, pgprot_t prot)
{
	pud_t new_pud = pfn_pud(__phys_to_pfn(phys), mk_pud_sect_prot(prot));

	/* Only allow permission changes for now */
	if (!pgattr_change_is_safe(pud_val(*pudp), pud_val(new_pud)))
		return 0;

	set_pud(pudp, new_pud);
	return 1;
}

int pmd_set_huge(pmd_t *pmdp, phys_addr_t phys, pgprot_t prot)
{
	pmd_t new_pmd = pfn_pmd(__phys_to_pfn(phys), mk_pmd_sect_prot(prot));

	/* Only allow permission changes for now */
	if (!pgattr_change_is_safe(pmd_val(*pmdp), pmd_val(new_pmd)))
		return 0;

	VM_BUG_ON(phys & ~PMD_MASK);
	set_pmd(pmdp, new_pmd);
	return 1;
}

static void init_pte(pte_t *ptep, unsigned long addr, unsigned long end,
		     phys_addr_t phys, pgprot_t prot)
{
	do {
		pte_t old_pte = __ptep_get(ptep);

		/*
		 * Required barriers to make this visible to the table walker
		 * are deferred to the end of alloc_init_cont_pte().
		 */
		__set_pte_nosync(ptep, pfn_pte(__phys_to_pfn(phys), prot));

		/*
		 * After the PTE entry has been populated once, we
		 * only allow updates to the permission attributes.
		 */
		BUG_ON(!pgattr_change_is_safe(pte_val(old_pte),
					      pte_val(__ptep_get(ptep))));

		phys += PAGE_SIZE;
	} while (ptep++, addr += PAGE_SIZE, addr != end);
}

phys_addr_t pgtable_alloc(void)
{
	phys_addr_t pa;

	pa = memblock_phys_alloc_range(PAGE_SIZE, PAGE_SIZE, 0, 0);
	if (!pa) {
		printf("Failed to allocate memory for page table\n");
		while (true)
			;
	}

	return pa;
}

static void init_clear_pgtable(void *table)
{
	memset(table, 0, PAGE_SIZE);

	/* Ensure the zeroing is observed by page table walks. */
	dsb(ishst);
}

static void init_cont_pte(pmd_t *pmdp, unsigned long addr,
				unsigned long end, phys_addr_t phys,
				pgprot_t prot, int flags)
{
	unsigned long next;
	pmd_t pmd = *pmdp;
	pte_t *ptep;

	BUG_ON(pmd_sect(pmd));
	BUG_ON(pmd_bad(pmd));

	if (pmd_none(pmd)) {
		pmdval_t pmdval = PMD_TYPE_TABLE | PMD_TABLE_UXN | PMD_TABLE_AF;
		phys_addr_t pte_phys;

		if (flags & NO_EXEC_MAPPINGS)
			pmdval |= PMD_TABLE_PXN;
		pte_phys = pgtable_alloc();
		ptep = pte_set_fixmap(pte_phys);
		init_clear_pgtable(ptep);
		ptep += pte_index(addr);
		__pmd_populate(pmdp, pte_phys, pmdval);
	} else {
		BUG_ON(pmd_bad(pmd));
		ptep = pte_set_fixmap_offset(pmdp, addr);
	}

	do {
		pgprot_t __prot = prot;

		next = pte_cont_addr_end(addr, end);

		/* use a contiguous mapping if the range is suitably aligned */
		if ((((addr | next | phys) & ~CONT_PTE_MASK) == 0) &&
		    (flags & NO_CONT_MAPPINGS) == 0)
			__prot = __pgprot(pgprot_val(prot) | PTE_CONT);

		init_pte(ptep, addr, next, phys, __prot);

		ptep += pte_index(next) - pte_index(addr);
		phys += next - addr;
	} while (addr = next, addr != end);

	/*
	 * Note: barriers and maintenance necessary to clear the fixmap slot
	 * ensure that all previous pgtable writes are visible to the table
	 * walker.
	 */
	pte_clear_fixmap();
}

static void init_pmd(pmd_t *pmdp, unsigned long addr, unsigned long end,
		     phys_addr_t phys, pgprot_t prot, int flags)
{
	unsigned long next;

	do {
		pmd_t old_pmd = *pmdp;

		next = pmd_addr_end(addr, end);

		/* try section mapping first */
		if (((addr | next | phys) & ~PMD_MASK) == 0 &&
		    (flags & NO_BLOCK_MAPPINGS) == 0) {
			pmd_set_huge(pmdp, phys, prot);

			/*
			 * After the PMD entry has been populated once, we
			 * only allow updates to the permission attributes.
			 */
			BUG_ON(!pgattr_change_is_safe(pmd_val(old_pmd),
						      READ_ONCE(pmd_val(*pmdp))));
		} else {
			init_cont_pte(pmdp, addr, next, phys, prot, flags);

			BUG_ON(pmd_val(old_pmd) != 0 &&
			       pmd_val(old_pmd) != READ_ONCE(pmd_val(*pmdp)));
		}
		phys += next - addr;
	} while (pmdp++, addr = next, addr != end);
}

static void init_cont_pmd(pud_t *pudp, unsigned long addr,
				unsigned long end, phys_addr_t phys,
				pgprot_t prot, int flags)
{
	unsigned long next;
	pud_t pud = *pudp;
	pmd_t *pmdp;

	if (pud_none(pud)) {
		pudval_t pudval = PUD_TYPE_TABLE | PUD_TABLE_UXN | PUD_TABLE_AF;
		phys_addr_t pmd_phys;

		if (flags & NO_EXEC_MAPPINGS)
			pudval |= PUD_TABLE_PXN;
		pmd_phys = pgtable_alloc();
		pmdp = pmd_set_fixmap(pmd_phys);
		init_clear_pgtable(pmdp);
		pmdp += pmd_index(addr);
		__pud_populate(pudp, pmd_phys, pudval);
	} else {
		pmdp = pmd_set_fixmap_offset(pudp, addr);
	}

	do {
		pgprot_t __prot = prot;

		next = pmd_cont_addr_end(addr, end);

		/* use a contiguous mapping if the range is suitably aligned */
		if ((((addr | next | phys) & ~CONT_PMD_MASK) == 0) &&
		    (flags & NO_CONT_MAPPINGS) == 0)
			__prot = __pgprot(pgprot_val(prot) | PTE_CONT);

		init_pmd(pmdp, addr, next, phys, __prot, flags);

		pmdp += pmd_index(next) - pmd_index(addr);
		phys += next - addr;
	} while (addr = next, addr != end);

	pmd_clear_fixmap();
}

static void init_pud(pgd_t *pgdp, unsigned long addr, unsigned long end,
			   phys_addr_t phys, pgprot_t prot, int flags)
{
	unsigned long next;
	pgd_t pgd = *pgdp;
	pud_t *pudp;

	if (pgd_none(pgd)) {
		pgdval_t pgdval = PGD_TYPE_TABLE | PGD_TABLE_UXN | PGD_TABLE_AF;
		phys_addr_t pud_phys;

		if (flags & NO_EXEC_MAPPINGS)
			pgdval |= PGD_TABLE_PXN;
		pud_phys = pgtable_alloc();
		pudp = pud_set_fixmap(pud_phys);
		init_clear_pgtable(pudp);
		pudp += pud_index(addr);
		__pgd_populate(pgdp, pud_phys, pgdval);
	} else {
		pudp = pud_set_fixmap_offset(pgdp, addr);
	}

	do {
		next = pud_addr_end(addr, end);

		/*
		 * For 4K granule only, attempt to put down a 1GB block
		 */
		if (((addr | next | phys) & ~PUD_MASK) == 0 &&
		    (flags & NO_BLOCK_MAPPINGS) == 0) {
			pud_set_huge(pudp, phys, prot);
		} else {
			init_cont_pmd(pudp, addr, next, phys, prot, flags);
		}
		phys += next - addr;
	} while (pudp++, addr = next, addr != end);

	// unmapping FIX_PUD
	pud_clear_fixmap();
}

static void __create_pgd_mapping(pgd_t *pgdir, phys_addr_t phys,
				unsigned long virt, phys_addr_t size, pgprot_t prot, int flags, bool alloc)
{
	unsigned long addr, end, next;
	pgd_t *pgdp = pgd_offset_pgd(pgdir, virt);

	/*
	 * If the virtual and physical address don't have the same offset
	 * within a page, we cannot map the region as the caller expects.
	 */
	if ((phys ^ virt) & ~PAGE_MASK)
		return;

	phys &= PAGE_MASK;
	addr = virt & PAGE_MASK;
	end = PAGE_ALIGN_UP(virt + size);

	do {
		next = pgd_addr_end(addr, end);
		init_pud(pgdp, addr, next, phys, prot, flags);
		phys += next - addr;
	} while (pgdp++, addr = next, addr != end);
}

/*
 * This function can only be used to modify existing table entries,
 * without allocating new levels of table. Note that this permits the
 * creation of new section or page entries.
 */
void __init create_mapping_noalloc(phys_addr_t phys, unsigned long virt,
				   phys_addr_t size, pgprot_t prot)
{
	/* Ensure that virt does not come from the linear mapping area. */
	if (virt < PAGE_OFFSET) {
		printf("BUG: not creating mapping for %pa at 0x%016lx - outside kernel range\n",
			&phys, virt);
		return;
	}
	__create_pgd_mapping((void *)LDSYM_ADDR(swapper_pg_dir), phys, virt, size, prot,
					NO_CONT_MAPPINGS, false);
}

void create_mapping_alloc(phys_addr_t phys, unsigned long virt,
				   phys_addr_t size, pgprot_t prot)
{
	/* Ensure that virt does not come from the linear mapping area. */
	if (virt < PAGE_OFFSET) {
		printf("BUG: not creating mapping for %pa at 0x%016lx - outside kernel range\n",
			&phys, virt);
		return;
	}
	__create_pgd_mapping((void *)LDSYM_ADDR(swapper_pg_dir), phys, virt, size, prot, NO_CONT_MAPPINGS, true);
}
