/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_VMALLOC_H
#define _LINUX_VMALLOC_H

#include <linux/types.h>
#include <linux/pfn.h>
#include <vdso/align.h>
#include <init.h>
#include <memory.h>
#include <rbtree_types.h>
#include <pgtable-types.h>
#include <pg_table.h>

/* bits in flags of vmalloc's vm_struct below */
#define VM_IOREMAP		0x00000001	/* ioremap() and friends */
#define VM_ALLOC		0x00000002	/* vmalloc() */
#define VM_MAP			0x00000004	/* vmap()ed pages */
#define VM_USERMAP		0x00000008	/* suitable for remap_vmalloc_range */
#define VM_DMA_COHERENT		0x00000010	/* dma_alloc_coherent */
#define VM_UNINITIALIZED	0x00000020	/* vm_struct is not fully initialized */
#define VM_NO_GUARD		0x00000040      /* ***DANGEROUS*** don't add guard page */
#define VM_KASAN		0x00000080      /* has allocated kasan shadow memory */
#define VM_FLUSH_RESET_PERMS	0x00000100	/* reset direct map and flush TLB on unmap, can't be freed in atomic context */
#define VM_MAP_PUT_PAGES	0x00000200	/* put pages and free array in vfree */
#define VM_ALLOW_HUGE_VMAP	0x00000400      /* Allow for huge pages on archs with HAVE_ARCH_HUGE_VMALLOC */

struct vm_struct {
	struct vm_struct	*next;
	void			*addr;
	unsigned long		size;
	unsigned long		flags;
	struct page		**pages;
#ifdef CONFIG_HAVE_ARCH_HUGE_VMALLOC
	unsigned int		page_order;
#endif
	unsigned int		nr_pages;
	phys_addr_t		phys_addr;
	const void		*caller;
	unsigned long		requested_size;
};

struct vmap_area {
	unsigned long va_start;
	unsigned long va_end;

	struct rb_node rb_node;         /* address sorted rbtree */
	struct list_head list;          /* address sorted list */

	/*
	 * The following two variables can be packed, because
	 * a vmap_area object can be either:
	 *    1) in "free" tree (root is free_vmap_area_root)
	 *    2) or "busy" tree (root is vmap_area_root)
	 */
	union {
		unsigned long subtree_max_size; /* in "free" tree */
		struct vm_struct *vm;           /* in "busy" tree */
	};
	unsigned long flags; /* mark type of vm_map_ram area */
};

void __init vmalloc_init(void);
void *vmalloc(unsigned long size);
void vfree(const void *addr);

#define arch_vmap_pte_range_unmap_size arch_vmap_pte_range_unmap_size
static inline unsigned long arch_vmap_pte_range_unmap_size(unsigned long addr,
							   pte_t *ptep)
{
	/*
	 * The caller handles alignment so it's sufficient just to check
	 * PTE_CONT.
	 */
	return pte_valid_cont(__ptep_get(ptep)) ? CONT_PTE_SIZE : PAGE_SIZE;
}

#define arch_vmap_pte_range_map_size arch_vmap_pte_range_map_size
static inline unsigned long arch_vmap_pte_range_map_size(unsigned long addr,
						unsigned long end, u64 pfn)
{
	if (end - addr < CONT_PTE_SIZE)
		return PAGE_SIZE;

	if (!IS_ALIGNED(addr, CONT_PTE_SIZE))
		return PAGE_SIZE;

	if (!IS_ALIGNED(PFN_PHYS(pfn), CONT_PTE_SIZE))
		return PAGE_SIZE;

	return CONT_PTE_SIZE;
}

#endif /* _LINUX_VMALLOC_H */
