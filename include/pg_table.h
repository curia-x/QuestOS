/* SPDX-License-Identifier: GPL-2.0 */
#ifndef PG_TABLE_H
#define PG_TABLE_H

#include <pgtable-hwdef.h>
#include <stub.h>
#include <linker_symbol.h>
#include <page.h>

#ifndef LINKER_SCRIPT
#include <mmu.h>
#endif

#if !defined (LINKER_SCRIPT) && !defined (__ASSEMBLY__)
#include <fixmap.h>
#include <barrier.h>
#include <process.h>
typedef unsigned int pgtbl_mod_mask;
#endif

#define FLAGS_MAP_PUD_BLOCK (1 << 0)
#define FLAGS_MAP_PMD_BLOCK (1 << 1)

#ifndef ALIGN_DOWN
#define ALIGN_DOWN(addr, size) ((addr) & ~((size) - 1))
#endif
#define ALIGN_UP(addr, size) ((((unsigned long)(addr)) + (size) - 1) & ~((size) - 1))
#ifndef PAGE_ALIGN_DOWN
#define PAGE_ALIGN_DOWN(x) ALIGN_DOWN(x, PAGE_SIZE)
#endif
#define PAGE_ALIGN_UP(x)   ALIGN_UP(x, PAGE_SIZE)
#define PG_TABLE_BITS 9
#define PG_ENTRY_BYTES (1 << 3)

#define PA_BITS 48
#define PG_TABLE_ENTRY_GET_ADDR(entry) (((entry) & PAGE_MASK) & ((1ULL << PA_BITS) - 1))

#undef PGD_ENTRY_BITS_MASK
#define PGD_ENTRY_BITS_MASK (~(((1ULL << PG_TABLE_BITS) - 1) << PGDIR_SHIFT))

#define PGD_ENTRY_BITS_MASK_MORE_BIT (~(((1ULL << (PG_TABLE_BITS + 1)) - 1) << PGDIR_SHIFT))
#define PGD_ENTRY(ppgd, va) \
	((u64 *)((uintptr_t)ppgd + PG_ENTRY_BYTES * \
	((((uintptr_t)va) & ~(uintptr_t)(PGD_ENTRY_BITS_MASK)) >> PGDIR_SHIFT)))
#define PGD_ALIGN_UP(addr) ALIGN_UP(addr, 1ULL << PGDIR_SHIFT)

#undef PUD_ENTRY_BITS_MASK
#define PUD_ENTRY_BITS_MASK (~(((1ULL << PG_TABLE_BITS) - 1) << PUD_SHIFT))

#define PUD_ENTRY_BITS_MASK_MORE_BIT (~(((1ULL << (PG_TABLE_BITS + 1)) - 1) << PUD_SHIFT))
/* 之所以用PUD_ENTRY_BITS_MASK_MORE_BIT是因为pud_map_range的pmd_entry_end可能在页表的尾部 */
#define PUD_ENTRY(ppud, va) \
	((u64 *)((uintptr_t)ppud + PG_ENTRY_BYTES * \
	((((uintptr_t)va) & ~(uintptr_t)(PUD_ENTRY_BITS_MASK)) >> PUD_SHIFT)))
#define PUD_ALIGN_UP(addr) ALIGN_UP(addr, 1ULL << PUD_SHIFT)

#undef PMD_ENTRY_BITS_MASK
#define PMD_ENTRY_BITS_MASK (~(((1ULL << PG_TABLE_BITS) - 1) << PMD_SHIFT))

#define PMD_ENTRY_BITS_MASK_MORE_BIT (~(((1ULL << (PG_TABLE_BITS + 1)) - 1) << PMD_SHIFT))
#define PMD_ENTRY(ppmd, va) \
	((u64 *)((uintptr_t)ppmd + PG_ENTRY_BYTES * \
	((((uintptr_t)va) & ~(uintptr_t)(PMD_ENTRY_BITS_MASK)) >> PMD_SHIFT)))
#define PMD_ALIGN_UP(addr) ALIGN_UP(addr, 1ULL << PMD_SHIFT)

#undef PTE_ENTRY_BITS_MASK
#define PTE_ENTRY_BITS_MASK (~(((1ULL << PG_TABLE_BITS) - 1) << PAGE_SHIFT))

#define PTE_ENTRY_BITS_MASK_MORE_BIT (~(((1ULL << (PG_TABLE_BITS + 1)) - 1) << PAGE_SHIFT))
#define PTE_ENTRY(ppte, va) \
	((u64 *)((uintptr_t)(ppte) + PG_ENTRY_BYTES * \
	((((uintptr_t)(va)) & ~(uintptr_t)(PTE_ENTRY_BITS_MASK)) >> PAGE_SHIFT)))
#define PTE_ALIGN_UP(addr) ALIGN_UP(addr, 1ULL << PAGE_SHIFT)

/* For memory accesses from Non-secure state,
 * including all accesses in the EL2 or EL2&0 translation
 * regime, this bit is RES0 and is ignored by the PE.
 */
#define PG_DESC_NSTABLE_OFFSET 63
#define PG_DESC_APTABLE_OFFSET 61
#define PG_DESC_XNTABLE_UXNTABLE_OFFSET 60
#define PG_DESC_PXNTABLE_OFFSET 59
#define PG_DESC_NSTABLE_BIT          (1ULL << PG_DESC_NSTABLE_OFFSET)
#define PG_DESC_APTABLE_BIT          (1ULL << PG_DESC_APTABLE_OFFSET)
#define PG_DESC_XNTABLE_UXNTABLE_BIT (1ULL << PG_DESC_XNTABLE_UXNTABLE_OFFSET)
#define PG_DESC_PXNTABLE_BIT         (1ULL << PG_DESC_PXNTABLE_OFFSET)

#define PG_DESC_NSTABLE_S  (0b0ULL << PG_DESC_NSTABLE_OFFSET)
#define PG_DESC_NSTABLE_NS (0b1ULL << PG_DESC_NSTABLE_OFFSET)

#define PG_DESC_APTABLE_RW1_RW0 (0b00ULL << PG_DESC_APTABLE_OFFSET)
#define PG_DESC_APTABLE_RW1_0   (0b01ULL << PG_DESC_APTABLE_OFFSET)
#define PG_DESC_APTABLE_RO1_RO0 (0b10ULL << PG_DESC_APTABLE_OFFSET)
#define PG_DESC_APTABLE_RO1_0   (0b11ULL << PG_DESC_APTABLE_OFFSET)

#define PG_DESC_XNTABLE_UXNTABLE_X  (0b0ULL << PG_DESC_XNTABLE_UXNTABLE_OFFSET)
#define PG_DESC_XNTABLE_UXNTABLE_NX (0b1ULL << PG_DESC_XNTABLE_UXNTABLE_OFFSET)

#define PG_DESC_PXNTABLE_X  (0b0ULL << PG_DESC_PXNTABLE_OFFSET)
#define PG_DESC_PXNTABLE_NX (0b1ULL << PG_DESC_PXNTABLE_OFFSET)

#define PG_DESC_PAGE_PBHA_OFFSET 59
#define PG_DESC_PAGE_XN_UXN_OFFSET 54
#define PG_DESC_PAGE_PXN_OFFSET 53
#define PG_DESC_PAGE_CON_OFFSET 52
#define PG_DESC_PAGE_DBM_OFFSET 51
#define PG_DESC_PAGE_GP_OFFSET 50
#define PG_DESC_PAGE_nT_OFFSET 16
#define PG_DESC_PAGE_nG_OFFSET 11
#define PG_DESC_PAGE_AF_OFFSET 10
#define PG_DESC_PAGE_SH_OFFSET 8
#define PG_DESC_PAGE_AP_OFFSET 6
#define PG_DESC_PAGE_NS_OFFSET 5
#define PG_DESC_PAGE_ATTR_INDEX_OFFSET 2
// #define PG_DESC_PAGE_PBHA_BIT (1ULL << PG_DESC_PAGE_PBHA_OFFSET)
#define PG_DESC_PAGE_XN_UXN_BIT   (1ULL << PG_DESC_PAGE_XN_UXN_OFFSET)
#define PG_DESC_PAGE_PXN_BIT  (1ULL << PG_DESC_PAGE_PXN_OFFSET)
#define PG_DESC_PAGE_CON_BIT  (1ULL << PG_DESC_PAGE_CON_OFFSET)
#define PG_DESC_PAGE_DBM_BIT  (1ULL << PG_DESC_PAGE_DBM_OFFSET)
// #define PG_DESC_PAGE_GP_BIT   (1ULL << PG_DESC_PAGE_GP_OFFSET)

// #define PG_DESC_PAGE_nT_BIT   (1ULL << PG_DESC_PAGE_nT_OFFSET)
#define PG_DESC_PAGE_nG_BIT   (1ULL << PG_DESC_PAGE_nG_OFFSET)
#define PG_DESC_PAGE_AF_BIT   (1ULL << PG_DESC_PAGE_AF_OFFSET)
#define PG_DESC_PAGE_SH_BIT   (1ULL << PG_DESC_PAGE_SH_OFFSET)
#define PG_DESC_PAGE_AP_BIT   (1ULL << PG_DESC_PAGE_AP_OFFSET)
/* For memory accesses from Non-secure state this bit is RES0 and is ignored by the PE. */
#define PG_DESC_PAGE_NS_BIT   (1ULL << PG_DESC_PAGE_NS_OFFSET)
#define PG_DESC_PAGE_ATTR_INDEX_BIT (1ULL << PG_DESC_PAGE_ATTR_INDEX_OFFSET)

#define PG_DESC_PAGE_XN_UXN_X (0b0ULL << PG_DESC_PAGE_XN_UXN_OFFSET)
#define PG_DESC_PAGE_XN_UXN_NX (0b1ULL << PG_DESC_PAGE_XN_UXN_OFFSET)
#define PG_DESC_PAGE_PXN_X (0b0ULL << PG_DESC_PAGE_PXN_OFFSET)
#define PG_DESC_PAGE_PXN_NX (0b1ULL << PG_DESC_PAGE_PXN_OFFSET)

#define PG_DESC_PAGE_CON_NONCONTIGUOUS (0b0ULL << PG_DESC_PAGE_CON_OFFSET)
#define PG_DESC_PAGE_CON_CONTIGUOUS    (0b1ULL << PG_DESC_PAGE_CON_OFFSET)

#define PG_DESC_PAGE_DBM_NONMODIFY (0b0ULL << PG_DESC_PAGE_DBM_OFFSET)
#define PG_DESC_PAGE_DBM_MODIFYED    (0b1ULL << PG_DESC_PAGE_DBM_OFFSET)

#define PG_DESC_PAGE_nG_GLOBAL (0b0ULL << PG_DESC_PAGE_nG_OFFSET)
#define PG_DESC_PAGE_nG_NONGLOBAL (0b1ULL << PG_DESC_PAGE_nG_OFFSET)

#define PG_DESC_PAGE_AF_NONACCESS (0b0ULL << PG_DESC_PAGE_AF_OFFSET)
#define PG_DESC_PAGE_AF_ACCESSED (0b1ULL << PG_DESC_PAGE_AF_OFFSET)

#define PG_DESC_PAGE_SH_NONSHAREABLE (0b0ULL << PG_DESC_PAGE_SH_OFFSET)
#define PG_DESC_PAGE_SH_OUTERSHAREABLE (0b10ULL << PG_DESC_PAGE_SH_OFFSET)
#define PG_DESC_PAGE_SH_INNERSHAREABLE (0b11ULL << PG_DESC_PAGE_SH_OFFSET)

#define	PG_DESC_PAGE_AP_RW1_0   (0b00ULL << PG_DESC_PAGE_AP_OFFSET)
#define	PG_DESC_PAGE_AP_RW1_RW0 (0b01ULL << PG_DESC_PAGE_AP_OFFSET)
#define	PG_DESC_PAGE_AP_RO1_0   (0b10ULL << PG_DESC_PAGE_AP_OFFSET)
#define	PG_DESC_PAGE_AP_RO1_RO0 (0b11ULL << PG_DESC_PAGE_AP_OFFSET)

/* For memory accesses from Non-secure state this bit is RES0 and is ignored by the PE. */
#define PG_DESC_PAGE_NS_S (0b0ULL << PG_DESC_PAGE_NS_OFFSET)
#define PG_DESC_PAGE_NS_NS (0b1ULL << PG_DESC_PAGE_NS_OFFSET)

#define PG_DESC_TYPE_TABLE 0b11
#define PG_DESC_TYPE_BLOCK 0b01
#define PG_DESC_TYPE_PAGE 0b11

/* 页表初始化时要把AF位置位，不然就会产生异常，ESR_EL1:0x86000009 */
#define PG_PAGE_ENTRY_NORMAL_CACHEABLE_RX (PG_DESC_PAGE_XN_UXN_NX | PG_DESC_PAGE_PXN_X | PG_DESC_PAGE_AF_BIT | PG_DESC_PAGE_SH_INNERSHAREABLE | PG_DESC_PAGE_AP_RO1_0 | NORMAL_ATTR_CACHEABLE_IDX << PG_DESC_PAGE_ATTR_INDEX_OFFSET)
#define PG_PAGE_ENTRY_NORMAL_CACHEABLE_RW (PG_DESC_PAGE_XN_UXN_NX | PG_DESC_PAGE_PXN_NX | PG_DESC_PAGE_AF_BIT | PG_DESC_PAGE_SH_INNERSHAREABLE | PG_DESC_PAGE_AP_RW1_0 | NORMAL_ATTR_CACHEABLE_IDX << PG_DESC_PAGE_ATTR_INDEX_OFFSET)
#define PG_PAGE_ENTRY_NORMAL_CACHEABLE_RWX (PG_DESC_PAGE_XN_UXN_NX | PG_DESC_PAGE_PXN_X | PG_DESC_PAGE_AF_BIT | PG_DESC_PAGE_SH_INNERSHAREABLE | PG_DESC_PAGE_AP_RW1_0 | NORMAL_ATTR_CACHEABLE_IDX << PG_DESC_PAGE_ATTR_INDEX_OFFSET)
#define PG_PAGE_ENTRY_DEVICE_RW (PG_DESC_PAGE_XN_UXN_NX | PG_DESC_PAGE_PXN_NX | PG_DESC_PAGE_AF_BIT | PG_DESC_PAGE_SH_INNERSHAREABLE | PG_DESC_PAGE_AP_RW1_0 | DEVICE_ATTR_nGnRnE_IDX << PG_DESC_PAGE_ATTR_INDEX_OFFSET)

#define PG_TABLE_ENTRY_RX (PG_DESC_APTABLE_RO1_0 | PG_DESC_XNTABLE_UXNTABLE_NX | PG_DESC_PXNTABLE_X)
#define PG_TABLE_ENTRY_RW (PG_DESC_APTABLE_RW1_0 | PG_DESC_XNTABLE_UXNTABLE_NX | PG_DESC_PXNTABLE_NX)
#define PG_TABLE_ENTRY_RWX (PG_DESC_APTABLE_RW1_0 | PG_DESC_XNTABLE_UXNTABLE_NX | PG_DESC_PXNTABLE_X)

#define PGTABLE_LEVELS	4

#ifndef PTDESC_TABLE_SHIFT
/* Number of VA bits resolved by a single translation table level */
#define PTDESC_TABLE_SHIFT	(PAGE_SHIFT - PTDESC_ORDER)
#endif

#undef SPAN_NR_ENTRIES
#define SPAN_NR_ENTRIES(vstart, vend, shift) \
	(((vend) >> (shift)) - ((vstart) >> (shift)) + 1)

#undef EARLY_ENTRIES
#define EARLY_ENTRIES(lvl, vstart, vend) \
	SPAN_NR_ENTRIES(vstart, vend, PMD_SHIFT + lvl * PTDESC_TABLE_SHIFT)

#undef EARLY_LEVEL
#define EARLY_LEVEL(lvl, lvls, vstart, vend, add) \
	((lvls) > (lvl) ? EARLY_ENTRIES(lvl, vstart, vend) + (add) : 0)

#undef EARLY_PAGES
#define EARLY_PAGES(lvls, vstart, vend, add) (1		/* PGDIR page */				\
	+ EARLY_LEVEL(3, (lvls), (vstart), (vend), add) /* each entry needs a next level page table */	\
	+ EARLY_LEVEL(2, (lvls), (vstart), (vend), add)	/* each entry needs a next level page table */	\
	+ EARLY_LEVEL(1, (lvls), (vstart), (vend), add))/* each entry needs a next level page table */

#define SWAPPER_BLOCK_SHIFT	PMD_SHIFT
#define SWAPPER_SKIP_LEVEL	1
#define SWAPPER_BLOCK_SIZE	(UL(1) << SWAPPER_BLOCK_SHIFT)

#if SWAPPER_BLOCK_SIZE > SEGMENT_ALIGN
#define EARLY_SEGMENT_EXTRA_PAGES (KERNEL_SEGMENT_COUNT + 1)
/*
 * The initial ID map consists of the kernel image, mapped as two separate
 * segments, and may appear misaligned wrt the swapper block size. This means
 * we need 3 additional pages. The DT could straddle a swapper block boundary,
 * so it may need 2.
 */
#define EARLY_IDMAP_EXTRA_PAGES		3
#define EARLY_IDMAP_EXTRA_FDT_PAGES	2
#else
#define EARLY_SEGMENT_EXTRA_PAGES	0
#define EARLY_IDMAP_EXTRA_PAGES		0
#define EARLY_IDMAP_EXTRA_FDT_PAGES	0
#endif

#undef INIT_IDMAP_DIR_PAGES
#undef INIT_IDMAP_DIR_SIZE
#define INIT_IDMAP_DIR_PAGES	(EARLY_PAGES(PGTABLE_LEVELS, IMAGE_VADDR, kimage_limit, 1))
#define INIT_IDMAP_DIR_SIZE	((INIT_IDMAP_DIR_PAGES + EARLY_IDMAP_EXTRA_PAGES) * PAGE_SIZE)

#undef KERNEL_SEGMENT_COUNT
#undef EARLY_SEGMENT_EXTRA_PAGES
#undef INIT_DIR_SIZE
/* The number of segments in the kernel image (text, rodata, inittext, initdata, data+bss) */
#define KERNEL_SEGMENT_COUNT	5
#define EARLY_SEGMENT_EXTRA_PAGES (KERNEL_SEGMENT_COUNT + 1)
#define INIT_DIR_SIZE (PAGE_SIZE * (EARLY_PAGES(PGTABLE_LEVELS, IMAGE_VADDR, _end, 1) \
				    + EARLY_SEGMENT_EXTRA_PAGES))

#define IDMAP_VA_BITS		48
#define IDMAP_LEVELS		ARM64_HW_PGTABLE_LEVELS(IDMAP_VA_BITS)
#define IDMAP_ROOT_LEVEL	(4 - IDMAP_LEVELS)

#define INIT_IDMAP_PGTABLE_LEVELS	(IDMAP_LEVELS)

#define INIT_IDMAP_FDT_PAGES	(EARLY_PAGES(INIT_IDMAP_PGTABLE_LEVELS, 0UL, UL(MAX_FDT_SIZE), 1) - 1)
#define INIT_IDMAP_FDT_SIZE	((INIT_IDMAP_FDT_PAGES + EARLY_IDMAP_EXTRA_FDT_PAGES) * PAGE_SIZE)

/* Must be a compile-time constant, so implement it as a macro */
#define pgd_index(a)  (((a) >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1))
#define pud_index(addr)		(((addr) >> PUD_SHIFT) & (PTRS_PER_PUD - 1))

/*
 * Page Table Modification bits for pgtbl_mod_mask.
 *
 * These are used by the p?d_alloc_track*() set of functions an in the generic
 * vmalloc/ioremap code to track at which page-table levels entries have been
 * modified. Based on that the code can better decide when vmalloc and ioremap
 * mapping changes need to be synchronized to other page-tables in the system.
 */
#define		__PGTBL_PGD_MODIFIED	0
#define		__PGTBL_P4D_MODIFIED	1
#define		__PGTBL_PUD_MODIFIED	2
#define		__PGTBL_PMD_MODIFIED	3
#define		__PGTBL_PTE_MODIFIED	4

#define		PGTBL_PGD_MODIFIED	BIT(__PGTBL_PGD_MODIFIED)
#define		PGTBL_P4D_MODIFIED	BIT(__PGTBL_P4D_MODIFIED)
#define		PGTBL_PUD_MODIFIED	BIT(__PGTBL_PUD_MODIFIED)
#define		PGTBL_PMD_MODIFIED	BIT(__PGTBL_PMD_MODIFIED)
#define		PGTBL_PTE_MODIFIED	BIT(__PGTBL_PTE_MODIFIED)

#ifndef __ASSEMBLY__
#include <pgtable-types.h>

static inline phys_addr_t __pte_to_phys(pte_t pte)
{
	return pte_val(pte) & PTE_ADDR_LOW;
}

static inline pteval_t __phys_to_pte_val(phys_addr_t phys)
{
	return phys;
}

#define pte_pfn(pte)		(__pte_to_phys(pte) >> PAGE_SHIFT)
#define pfn_pte(pfn,prot)	\
	__pte(__phys_to_pte_val((phys_addr_t)(pfn) << PAGE_SHIFT) | pgprot_val(prot))

#define pte_none(pte)		(!pte_val(pte))
#define __pte_clear(mm, addr, ptep) \
				__set_pte(ptep, __pte(0))
#define pte_page(pte)		(pfn_to_page(pte_pfn(pte)))

/*
 * The following only work if pte_present(). Undefined behaviour otherwise.
 */
#define pte_present(pte)	(pte_valid(pte) || pte_present_invalid(pte))
#define pte_young(pte)		(!!(pte_val(pte) & PTE_AF))
#define pte_special(pte)	(!!(pte_val(pte) & PTE_SPECIAL))
#define pte_write(pte)		(!!(pte_val(pte) & PTE_WRITE))
#define pte_rdonly(pte)		(!!(pte_val(pte) & PTE_RDONLY))
#define pte_user(pte)		(!!(pte_val(pte) & PTE_USER))
#define pte_user_exec(pte)	(!(pte_val(pte) & PTE_UXN))
#define pte_cont(pte)		(!!(pte_val(pte) & PTE_CONT))
#define pte_tagged(pte)		((pte_val(pte) & PTE_ATTRINDX_MASK) == \
				 PTE_ATTRINDX(MT_NORMAL_TAGGED))

#define pte_cont_addr_end(addr, end)						\
({	unsigned long __boundary = ((addr) + CONT_PTE_SIZE) & CONT_PTE_MASK;	\
	(__boundary - 1 < (end) - 1) ? __boundary : (end);			\
})

#define pmd_cont_addr_end(addr, end)						\
({	unsigned long __boundary = ((addr) + CONT_PMD_SIZE) & CONT_PMD_MASK;	\
	(__boundary - 1 < (end) - 1) ? __boundary : (end);			\
})

#define pte_hw_dirty(pte)	(pte_write(pte) && !pte_rdonly(pte))
#define pte_sw_dirty(pte)	(!!(pte_val(pte) & PTE_DIRTY))
#define pte_dirty(pte)		(pte_sw_dirty(pte) || pte_hw_dirty(pte))

#define pte_valid(pte)		(!!(pte_val(pte) & PTE_VALID))
#define pte_present_invalid(pte) \
	((pte_val(pte) & (PTE_VALID | PTE_PRESENT_INVALID)) == PTE_PRESENT_INVALID)
/*
 * Execute-only user mappings do not have the PTE_USER bit set. All valid
 * kernel mappings have the PTE_UXN bit set.
 */
#define pte_valid_not_user(pte) \
	((pte_val(pte) & (PTE_VALID | PTE_USER | PTE_UXN)) == (PTE_VALID | PTE_UXN))
/*
 * Returns true if the pte is valid and has the contiguous bit set.
 */
#define pte_valid_cont(pte)	(pte_valid(pte) && pte_cont(pte))
/*
 * Could the pte be present in the TLB? We must check mm_tlb_flush_pending
 * so that we don't erroneously return false for pages that have been
 * remapped as PROT_NONE but are yet to be flushed from the TLB.
 * Note that we can't make any assumptions based on the state of the access
 * flag, since __ptep_clear_flush_young() elides a DSB when invalidating the
 * TLB.
 */
#define pte_accessible(mm, pte)	\
	(mm_tlb_flush_pending(mm) ? pte_present(pte) : pte_valid(pte))

static inline pte_t pgd_pte(pgd_t pgd)
{
	return __pte(pgd_val(pgd));
}

static inline pte_t pud_pte(pud_t pud)
{
	return __pte(pud_val(pud));
}

static inline pud_t pte_pud(pte_t pte)
{
	return __pud(pte_val(pte));
}

static inline pmd_t pud_pmd(pud_t pud)
{
	return __pmd(pud_val(pud));
}

static inline pte_t pmd_pte(pmd_t pmd)
{
	return __pte(pmd_val(pmd));
}

static inline pmd_t pte_pmd(pte_t pte)
{
	return __pmd(pte_val(pte));
}

static inline pmd_t pmd_mkcont(pmd_t pmd)
{
	return __pmd(pmd_val(pmd) | PMD_SECT_CONT);
}

static inline pgprot_t mk_pud_sect_prot(pgprot_t prot)
{
	return __pgprot((pgprot_val(prot) & ~PUD_TYPE_MASK) | PUD_TYPE_SECT);
}

static inline pgprot_t mk_pmd_sect_prot(pgprot_t prot)
{
	return __pgprot((pgprot_val(prot) & ~PMD_TYPE_MASK) | PMD_TYPE_SECT);
}

#define __pgd_to_phys(pgd)	__pte_to_phys(pgd_pte(pgd))
#define __phys_to_pgd_val(phys)	__phys_to_pte_val(phys)

#define pgd_addr_end(addr, end)						\
({	unsigned long __boundary = ((addr) + PGDIR_SIZE) & PGDIR_MASK;	\
	(__boundary - 1 < (end) - 1)? __boundary: (end);		\
})

#define pud_addr_end(addr, end)						\
({	unsigned long __boundary = ((addr) + PUD_SIZE) & PUD_MASK;	\
	(__boundary - 1 < (end) - 1)? __boundary: (end);		\
})

#define pmd_addr_end(addr, end)						\
({	unsigned long __boundary = ((addr) + PMD_SIZE) & PMD_MASK;	\
	(__boundary - 1 < (end) - 1) ? __boundary : (end);		\
})

static inline pud_t *pud_set_fixmap(unsigned long addr)
{
	return (pud_t *)set_fixmap_offset(FIX_PUD, addr);
}

static inline phys_addr_t pgd_page_paddr(pgd_t pgd)
{
	return __pgd_to_phys(pgd);
}

static inline phys_addr_t pud_offset_phys(pgd_t *pgdp, unsigned long addr)
{
	return pgd_page_paddr(*pgdp) + pud_index(addr) * sizeof(pud_t);
}

static inline pud_t *pud_set_fixmap_offset(pgd_t *pgdp, unsigned long addr)
{
	return pud_set_fixmap(pud_offset_phys(pgdp, addr));
}

static inline void pud_clear_fixmap(void)
{
	clear_fixmap(FIX_PUD);
}

static inline pgd_t *pgd_offset_pgd(pgd_t *pgd, unsigned long address)
{
	return (pgd + pgd_index(address));
};

/*
 * a shortcut which implies the use of the kernel's pgd, instead
 * of a process's
 */
#define pgd_offset_k(address)		pgd_offset_pgd((void *)(u64)LDSYM_ADDR(swapper_pg_dir), (address))

static inline pud_t pud_mkhuge(pud_t pud)
{
	/*
	 * It's possible that the pud is present-invalid on entry
	 * and in that case it needs to remain present-invalid on
	 * exit. So ensure the VALID bit does not get modified.
	 */
	pudval_t mask = PUD_TYPE_MASK & ~PTE_VALID;
	pudval_t val = PUD_TYPE_SECT & ~PTE_VALID;

	return __pud((pud_val(pud) & ~mask) | val);
}

#define __pud_to_phys(pud)	__pte_to_phys(pud_pte(pud))
#define __phys_to_pud_val(phys)	__phys_to_pte_val(phys)
#define pud_pfn(pud)		((__pud_to_phys(pud) & PUD_MASK) >> PAGE_SHIFT)
#define pfn_pud(pfn,prot)	__pud(__phys_to_pud_val((phys_addr_t)(pfn) << PAGE_SHIFT) | pgprot_val(prot))

extern void set_swapper_pgd(pgd_t *pgdp, pgd_t pgd);

#define pgd_none(pgd)		(!pgd_val(pgd))
#define pgd_bad(pgd)		(((pgd_val(pgd) & PGD_TYPE_MASK) != PGD_TYPE_TABLE))
#define pgd_present(pgd)	(!pgd_none(pgd))

static inline void set_pgd(pgd_t *pgdp, pgd_t pgd)
{
	set_swapper_pgd(pgdp, pgd);
	return;
}

#define pud_none(pud)		(!pud_val(pud))
#define pud_bad(pud)		((pud_val(pud) & PUD_TYPE_MASK) != \
				 PUD_TYPE_TABLE)
#define pud_present(pud)	pte_present(pud_pte(pud))

static inline void set_pud(pud_t *pudp, pud_t pud)
{
	set_swapper_pgd((pgd_t *)pudp, __pgd(pud_val(pud)));
	return;
}

#define pmd_none(pmd)		(!pmd_val(pmd))

#define pmd_table(pmd)		((pmd_val(pmd) & PMD_TYPE_MASK) == \
				 PMD_TYPE_TABLE)
#define pmd_sect(pmd)		((pmd_val(pmd) & PMD_TYPE_MASK) == \
				 PMD_TYPE_SECT)
#define pmd_leaf(pmd)		(pmd_present(pmd) && !pmd_table(pmd))
#define pmd_bad(pmd)		(!pmd_table(pmd))
#define __pmd_to_phys(pmd)	__pte_to_phys(pmd_pte(pmd))
#define __phys_to_pmd_val(phys)	__phys_to_pte_val(phys)
#define pmd_pfn(pmd)		((__pmd_to_phys(pmd) & PMD_MASK) >> PAGE_SHIFT)
#define pfn_pmd(pfn,prot)	__pmd(__phys_to_pmd_val((phys_addr_t)(pfn) << PAGE_SHIFT) | pgprot_val(prot))

static inline phys_addr_t pud_page_paddr(pud_t pud)
{
	return __pud_to_phys(pud);
}

static inline void __pgd_populate(pgd_t *pgdp, phys_addr_t pudp, pgdval_t prot)
{
	set_pgd(pgdp, __pgd(__phys_to_pgd_val(pudp) | prot));
}

static inline void __pud_populate(pud_t *pudp, phys_addr_t pmdp, pudval_t prot)
{
	set_pud(pudp, __pud(__phys_to_pud_val(pmdp) | prot));
}

#define pud_young(pud)		pte_young(pud_pte(pud))
#define pud_mkyoung(pud)	pte_pud(pte_mkyoung(pud_pte(pud)))
#define pud_write(pud)		pte_write(pud_pte(pud))

#define pmd_present(pmd)	pte_present(pmd_pte(pmd))
#define pmd_dirty(pmd)		pte_dirty(pmd_pte(pmd))
#define pmd_young(pmd)		pte_young(pmd_pte(pmd))
#define pmd_valid(pmd)		pte_valid(pmd_pte(pmd))
#define pmd_user(pmd)		pte_user(pmd_pte(pmd))
#define pmd_user_exec(pmd)	pte_user_exec(pmd_pte(pmd))
#define pmd_cont(pmd)		pte_cont(pmd_pte(pmd))
#define pmd_wrprotect(pmd)	pte_pmd(pte_wrprotect(pmd_pte(pmd)))
#define pmd_mkold(pmd)		pte_pmd(pte_mkold(pmd_pte(pmd)))
#define pmd_mkwrite_novma(pmd)	pte_pmd(pte_mkwrite_novma(pmd_pte(pmd)))
#define pmd_mkclean(pmd)	pte_pmd(pte_mkclean(pmd_pte(pmd)))
#define pmd_mkdirty(pmd)	pte_pmd(pte_mkdirty(pmd_pte(pmd)))
#define pmd_mkyoung(pmd)	pte_pmd(pte_mkyoung(pmd_pte(pmd)))
#define pmd_mkinvalid(pmd)	pte_pmd(pte_mkinvalid(pmd_pte(pmd)))
#ifdef CONFIG_HAVE_ARCH_USERFAULTFD_WP
#define pmd_uffd_wp(pmd)	pte_uffd_wp(pmd_pte(pmd))
#define pmd_mkuffd_wp(pmd)	pte_pmd(pte_mkuffd_wp(pmd_pte(pmd)))
#define pmd_clear_uffd_wp(pmd)	pte_pmd(pte_clear_uffd_wp(pmd_pte(pmd)))
#define pmd_swp_uffd_wp(pmd)	pte_swp_uffd_wp(pmd_pte(pmd))
#define pmd_swp_mkuffd_wp(pmd)	pte_pmd(pte_swp_mkuffd_wp(pmd_pte(pmd)))
#define pmd_swp_clear_uffd_wp(pmd) \
				pte_pmd(pte_swp_clear_uffd_wp(pmd_pte(pmd)))
#endif /* CONFIG_HAVE_ARCH_USERFAULTFD_WP */

#define pmd_write(pmd)		pte_write(pmd_pte(pmd))

static inline pmd_t pmd_mkhuge(pmd_t pmd)
{
	/*
	 * It's possible that the pmd is present-invalid on entry
	 * and in that case it needs to remain present-invalid on
	 * exit. So ensure the VALID bit does not get modified.
	 */
	pmdval_t mask = PMD_TYPE_MASK & ~PTE_VALID;
	pmdval_t val = PMD_TYPE_SECT & ~PTE_VALID;

	return __pmd((pmd_val(pmd) & ~mask) | val);
}

#ifndef pmd_index
static inline unsigned long pmd_index(unsigned long address)
{
	return (address >> PMD_SHIFT) & (PTRS_PER_PMD - 1);
}
#define pmd_index pmd_index
#endif

/* Find an entry in the second-level page table. */
#define pmd_offset_phys(dir, addr)	(pud_page_paddr(*(dir)) + pmd_index(addr) * sizeof(pmd_t))

#define pmd_set_fixmap(addr)		((pmd_t *)set_fixmap_offset(FIX_PMD, addr))
#define pmd_set_fixmap_offset(pud, addr)	pmd_set_fixmap(pmd_offset_phys(pud, addr))
#define pmd_clear_fixmap()		clear_fixmap(FIX_PMD)

static inline void emit_pte_barriers(void)
{
	/*
	 * These barriers are emitted under certain conditions after a pte entry
	 * was modified (see e.g. __set_pte_complete()). The dsb makes the store
	 * visible to the table walker. The isb ensures that any previous
	 * speculative "invalid translation" marker that is in the CPU's
	 * pipeline gets cleared, so that any access to that address after
	 * setting the pte to valid won't cause a spurious fault. If the thread
	 * gets preempted after storing to the pgtable but before emitting these
	 * barriers, __switch_to() emits a dsb which ensure the walker gets to
	 * see the store. There is no guarantee of an isb being issued though.
	 * This is safe because it will still get issued (albeit on a
	 * potentially different CPU) when the thread starts running again,
	 * before any access to the address.
	 */
	dsb(ishst);
	isb();
}

static inline void set_pmd(pmd_t *pmdp, pmd_t pmd)
{
	*pmdp = pmd;

	if (pmd_valid(pmd))
		emit_pte_barriers();
}

static inline void __pmd_populate(pmd_t *pmdp, phys_addr_t ptep,
				  pmdval_t prot)
{
	set_pmd(pmdp, __pmd(__phys_to_pmd_val(ptep) | prot));
}

static inline void pmd_clear(pmd_t *pmdp)
{
	set_pmd(pmdp, __pmd(0));
}

static inline phys_addr_t pmd_page_paddr(pmd_t pmd)
{
	return __pmd_to_phys(pmd);
}

static inline unsigned long pmd_page_vaddr(pmd_t pmd)
{
	return (unsigned long)__va(pmd_page_paddr(pmd));
}

static inline void __set_pte_nosync(pte_t *ptep, pte_t pte)
{
	*ptep = pte;
}

/*
 * Execute-only user mappings do not have the PTE_USER bit set. All valid
 * kernel mappings have the PTE_UXN bit set.
 */
#define pte_valid_not_user(pte) \
	((pte_val(pte) & (PTE_VALID | PTE_USER | PTE_UXN)) == (PTE_VALID | PTE_UXN))

static inline void __set_pte_complete(pte_t pte)
{
	/*
	 * Only if the new pte is valid and kernel, otherwise TLB maintenance
	 * has the necessary barriers.
	 */
	if (pte_valid_not_user(pte))
		emit_pte_barriers();
}

static inline void __set_pte(pte_t *ptep, pte_t pte)
{
	__set_pte_nosync(ptep, pte);
	__set_pte_complete(pte);
}

/* Find an entry in the third-level page table. */
#define pte_offset_phys(dir,addr)	(pmd_page_paddr(*(dir)) + pte_index(addr) * sizeof(pte_t))

#define pte_set_fixmap(addr)		((pte_t *)set_fixmap_offset(FIX_PTE, addr))
#define pte_set_fixmap_offset(pmd, addr)	pte_set_fixmap(pte_offset_phys(pmd, addr))
#define pte_clear_fixmap()		clear_fixmap(FIX_PTE)

static inline pte_t __ptep_get(pte_t *ptep)
{
	return *ptep;
}

/*
 * A page table page can be thought of an array like this: pXd_t[PTRS_PER_PxD]
 *
 * The pXx_index() functions return the index of the entry in the page
 * table page which would control the given virtual address
 *
 * As these functions may be used by the same code for different levels of
 * the page table folding, they are always available, regardless of
 * CONFIG_PGTABLE_LEVELS value. For the folded levels they simply return 0
 * because in such cases PTRS_PER_PxD equals 1.
 */

static inline unsigned long pte_index(unsigned long address)
{
	return (address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1);
}

#ifndef pmd_index
static inline unsigned long pmd_index(unsigned long address)
{
	return (address >> PMD_SHIFT) & (PTRS_PER_PMD - 1);
}
#define pmd_index pmd_index
#endif

#ifndef pud_index
static inline unsigned long pud_index(unsigned long address)
{
	return (address >> PUD_SHIFT) & (PTRS_PER_PUD - 1);
}
#define pud_index pud_index
#endif

#ifndef pgd_index
/* Must be a compile-time constant, so implement it as a macro */
#define pgd_index(a)  (((a) >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1))
#endif

void pg_map_range(u64 *pgd, u64 *pnext, u64 start, u64 end, u64 va_start, u64 attr, u32 flags);

static inline uint64_t at_s1e1r(uint64_t va)
{
	uint64_t par;

	asm volatile(
		"at s1e1r, %0\n"
		"isb\n"
		"mrs %0, par_el1\n"
		: "=r"(par)
		: "0"(va)
		: "memory"
	);

	return par;
}

#define pgd_ERROR(e)	\
	pr_err("%s:%d: bad pgd %016llx.\n", __FILE__, __LINE__, pgd_val(e))

static inline void pgd_clear(pgd_t *pgdp)
{
	set_pgd(pgdp, __pgd(0));
}

/*
 * If a p?d_bad entry is found while walking page tables, report
 * the error, before resetting entry to p?d_none.  Usually (but
 * very seldom) called out from the p?d_none_or_clear_bad macros.
 */
static inline void pgd_clear_bad(pgd_t *pgd)
{
	pgd_ERROR(*pgd);
	pgd_clear(pgd);
}

static inline int pgd_none_or_clear_bad(pgd_t *pgd)
{
	if (pgd_none(*pgd))
		return 1;
	if (unlikely(pgd_bad(*pgd))) {
		pgd_clear_bad(pgd);
		return 1;
	}
	return 0;
}

static inline
pud_t *pud_offset_lockless(pgd_t *pgdp, pgd_t pgd, unsigned long addr)
{
	return (pud_t *)__va(pgd_page_paddr(pgd)) + pud_index(addr);
}
#define pud_offset_lockless pud_offset_lockless

static inline pud_t *pud_offset(pgd_t *pgdp, unsigned long addr)
{
	return pud_offset_lockless(pgdp, READ_ONCE(*pgdp), addr);
}
#define pud_offset	pud_offset

#define pud_sect(pud)		((pud_val(pud) & PUD_TYPE_MASK) == \
				 PUD_TYPE_SECT)
#define pud_table(pud)		((pud_val(pud) & PUD_TYPE_MASK) == \
				 PUD_TYPE_TABLE)

static inline void pud_clear(pud_t *pudp)
{
	set_pud(pudp, __pud(0));
}

#define pud_ERROR(e)	\
	pr_err("%s:%d: bad pud %016llx.\n", __FILE__, __LINE__, pud_val(e))

static inline void pud_clear_bad(pud_t *pud)
{
	pud_ERROR(*pud);
	pud_clear(pud);
}

static inline int pud_none_or_clear_bad(pud_t *pud)
{
	if (pud_none(*pud))
		return 1;
	if (unlikely(pud_bad(*pud))) {
		pud_clear_bad(pud);
		return 1;
	}
	return 0;
}

static inline pmd_t *pud_pgtable(pud_t pud)
{
	return (pmd_t *)__va(pud_page_paddr(pud));
}

/* Find an entry in the second-level page table.. */
#ifndef pmd_offset
static inline pmd_t *pmd_offset(pud_t *pud, unsigned long address)
{
	return pud_pgtable(*pud) + pmd_index(address);
}
#define pmd_offset pmd_offset
#endif

static inline int pmd_clear_huge(pmd_t *pmdp)
{
	if (!pmd_sect(READ_ONCE(*pmdp)))
		return 0;
	pmd_clear(pmdp);
	return 1;
}

#define pmd_ERROR(e)	\
	pr_err("%s:%d: bad pmd %016llx.\n", __FILE__, __LINE__, pmd_val(e))

/*
 * Note that the pmd variant below can't be stub'ed out just as for p4d/pud
 * above. pmd folding is special and typically pmd_* macros refer to upper
 * level even when folded
 */
static inline void pmd_clear_bad(pmd_t *pmd)
{
	pmd_ERROR(*pmd);
	pmd_clear(pmd);
}

#ifndef pte_offset_kernel
static inline pte_t *pte_offset_kernel(pmd_t *pmd, unsigned long address)
{
	return (pte_t *)pmd_page_vaddr(*pmd) + pte_index(address);
}
#define pte_offset_kernel pte_offset_kernel
#endif

static inline bool pud_sect_supported(void)
{
	return PAGE_SIZE == SZ_4K;
}

static inline pte_t __ptep_get_and_clear_anysz(pte_t *ptep, unsigned long pgsize)
{
	pte_t pte = *ptep;

	*ptep = __pte(0);

	return pte;
}

static inline pte_t set_pte_bit(pte_t pte, pgprot_t prot)
{
	pte_val(pte) |= pgprot_val(prot);
	return pte;
}

static inline pte_t clear_pte_bit(pte_t pte, pgprot_t prot)
{
	pte_val(pte) &= ~pgprot_val(prot);
	return pte;
}

static inline pte_t pte_mkcont(pte_t pte)
{
	return set_pte_bit(pte, __pgprot(PTE_CONT));
}

static inline pte_t pte_mkdirty(pte_t pte)
{
	pte = set_pte_bit(pte, __pgprot(PTE_DIRTY));

	if (pte_write(pte))
		pte = clear_pte_bit(pte, __pgprot(PTE_RDONLY));

	return pte;
}

static inline pte_t pte_mkyoung(pte_t pte)
{
	return set_pte_bit(pte, __pgprot(PTE_AF));
}

extern void __contpte_try_unfold(unsigned long addr,
			pte_t *ptep, pte_t pte);

static __always_inline void contpte_try_unfold(unsigned long addr, pte_t *ptep, pte_t pte)
{
	if (unlikely(pte_valid_cont(pte)))
		__contpte_try_unfold(addr, ptep, pte);
}

static inline pte_t pte_mknoncont(pte_t pte)
{
	return clear_pte_bit(pte, __pgprot(PTE_CONT));
}

static inline pte_t __ptep_get_and_clear(unsigned long address, pte_t *ptep)
{
	return __ptep_get_and_clear_anysz(ptep, PAGE_SIZE);
}

#define __HAVE_ARCH_PTEP_GET_AND_CLEAR
static inline pte_t ptep_get_and_clear(unsigned long addr, pte_t *ptep)
{
	contpte_try_unfold(addr, ptep, __ptep_get(ptep));
	return __ptep_get_and_clear(addr, ptep);
}

/*
 * Select all bits except the pfn
 */
#define pte_pgprot pte_pgprot
static inline pgprot_t pte_pgprot(pte_t pte)
{
	unsigned long pfn = pte_pfn(pte);

	return __pgprot(pte_val(pfn_pte(pfn, __pgprot(0))) ^ pte_val(pte));
}

#define pte_advance_pfn pte_advance_pfn
static inline pte_t pte_advance_pfn(pte_t pte, unsigned long nr)
{
	return pfn_pte(pte_pfn(pte) + nr, pte_pgprot(pte));
}

static inline void __set_ptes_anysz(pte_t *ptep,
				    pte_t pte, unsigned int nr,
				    unsigned long pgsize)
{
	unsigned long stride = pgsize >> PAGE_SHIFT;

	for (;;) {
		__set_pte_nosync(ptep, pte);
		if (--nr == 0)
			break;
		ptep++;
		pte = pte_advance_pfn(pte, stride);
	}

	__set_pte_complete(pte);
}

static inline void __set_ptes(
			      unsigned long __always_unused addr,
			      pte_t *ptep, pte_t pte, unsigned int nr)
{
	__set_ptes_anysz(ptep, pte, nr, PAGE_SIZE);
}

#ifndef pmdp_get
static inline pmd_t pmdp_get(pmd_t *pmdp)
{
	return READ_ONCE(*pmdp);
}
#endif

#include <kmalloc.h>

/**
 * pte_free_kernel - free PTE-level kernel page table memory
 * @mm: the mm_struct of the current context
 * @pte: pointer to the memory containing the page table
 */
static inline void pte_free_kernel(pte_t *pte)
{
	kfree(pte);
}

/**
 * pte_free_kernel - free PTE-level kernel page table memory
 * @mm: the mm_struct of the current context
 * @pte: pointer to the memory containing the page table
 */
static inline void pmd_free(pmd_t *pmd)
{
	kfree(pmd);
}

extern pte_t contpte_ptep_get(pte_t *ptep, pte_t orig_pte);

/*
 * The below functions constitute the public API that arm64 presents to the
 * core-mm to manipulate PTE entries within their page tables (or at least this
 * is the subset of the API that arm64 needs to implement). These public
 * versions will automatically and transparently apply the contiguous bit where
 * it makes sense to do so. Therefore any users that are contig-aware (e.g.
 * hugetlb, kernel mapper) should NOT use these APIs, but instead use the
 * private versions, which are prefixed with double underscore. All of these
 * APIs except for ptep_get_lockless() are expected to be called with the PTL
 * held. Although the contiguous bit is considered private to the
 * implementation, it is deliberately allowed to leak through the getters (e.g.
 * ptep_get()), back to core code. This is required so that pte_leaf_size() can
 * provide an accurate size for perf_get_pgtable_size(). But this leakage means
 * its possible a pte will be passed to a setter with the contiguous bit set, so
 * we explicitly clear the contiguous bit in those cases to prevent accidentally
 * setting it in the pgtable.
 */

#define ptep_get ptep_get
static inline pte_t ptep_get(pte_t *ptep)
{
	pte_t pte = __ptep_get(ptep);

	if (likely(!pte_valid_cont(pte)))
		return pte;

	return contpte_ptep_get(ptep, pte);
}

extern void __contpte_try_fold(unsigned long addr, pte_t *ptep, pte_t pte);

static __always_inline void contpte_try_fold(
				unsigned long addr, pte_t *ptep, pte_t pte)
{
	/*
	 * Only bother trying if both the virtual and physical addresses are
	 * aligned and correspond to the last entry in a contig range. The core
	 * code mostly modifies ranges from low to high, so this is the likely
	 * the last modification in the contig range, so a good time to fold.
	 * We can't fold special mappings, because there is no associated folio.
	 */

	const unsigned long contmask = CONT_PTES - 1;
	bool valign = ((addr >> PAGE_SHIFT) & contmask) == contmask;

	if (unlikely(valign)) {
		bool palign = (pte_pfn(pte) & contmask) == contmask;

		if (unlikely(palign &&
		    pte_valid(pte) && !pte_cont(pte) && !pte_special(pte)))
			__contpte_try_fold(addr, ptep, pte);
	}
}

extern void contpte_set_ptes(unsigned long addr,
					pte_t *ptep, pte_t pte, unsigned int nr);

#define set_ptes set_ptes
static __always_inline void set_ptes(unsigned long addr,
				pte_t *ptep, pte_t pte, unsigned int nr)
{
	pte = pte_mknoncont(pte);

	if (likely(nr == 1)) {
		contpte_try_unfold(addr, ptep, __ptep_get(ptep));
		__set_ptes(addr, ptep, pte, 1);
		contpte_try_fold(addr, ptep, pte);
	} else {
		contpte_set_ptes(addr, ptep, pte, nr);
	}
}

#define set_pte_at(addr, ptep, pte) set_ptes(addr, ptep, pte, 1)

static inline pte_t pte_mkclean(pte_t pte)
{
	pte = clear_pte_bit(pte, __pgprot(PTE_DIRTY));
	pte = set_pte_bit(pte, __pgprot(PTE_RDONLY));

	return pte;
}

static inline pte_t pte_mkold(pte_t pte)
{
	return clear_pte_bit(pte, __pgprot(PTE_AF));
}

#ifndef __HAVE_ARCH_PTE_SAME
static inline int pte_same(pte_t pte_a, pte_t pte_b)
{
	return pte_val(pte_a) == pte_val(pte_b);
}
#endif

int vmap_phys_range_noflush(struct memory_struct *mm, phys_addr_t phys_addr,
							unsigned long virt, unsigned long size, pgprot_t prot);

#endif	/* __ASSEMBLY__ */

#endif /* PG_TABLE_H */
