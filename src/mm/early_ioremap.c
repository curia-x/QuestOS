// SPDX-License-Identifier: GPL-2.0
#include <init.h>
#include <fixmap.h>
#include <system.h>
#include <print.h>
#include <mm.h>

static void __iomem *prev_map[FIX_BTMAPS_SLOTS] __initdata;
static unsigned long prev_size[FIX_BTMAPS_SLOTS] __initdata;
static unsigned long slot_virt[FIX_BTMAPS_SLOTS] __initdata;

static int early_ioremap_debug __initdata;

static void __init __iomem *
__early_ioremap(resource_size_t phys_addr, unsigned long size, pgprot_t prot)
{
	unsigned long offset;
	resource_size_t last_addr;
	unsigned int nrpages;
	enum fixed_addresses idx;
	int i, slot;

	slot = -1;
	for (i = 0; i < FIX_BTMAPS_SLOTS; i++) {
		if (!prev_map[i]) {
			slot = i;
			break;
		}
	}

	if (WARN(slot < 0, "%s(%pa, %08lx) not found slot\n",
		 __func__, &phys_addr, size))
		return NULL;

	/* Don't allow wraparound or zero size */
	last_addr = phys_addr + size - 1;
	if (WARN_ON(!size || last_addr < phys_addr))
		return NULL;

	prev_size[slot] = size;
	/*
	 * Mappings have to be page-aligned
	 */
	offset = offset_in_page(phys_addr);
	phys_addr &= PAGE_MASK;
	size = PAGE_ALIGN(last_addr + 1) - phys_addr;

	/*
	 * Mappings have to fit in the FIX_BTMAP area.
	 */
	nrpages = size >> PAGE_SHIFT;
	if (WARN_ON(nrpages > NR_FIX_BTMAPS))
		return NULL;

	/*
	 * Ok, go for it..
	 */
	idx = FIX_BTMAP_BEGIN - NR_FIX_BTMAPS*slot;
	while (nrpages > 0) {
		__early_set_fixmap(idx, phys_addr, prot);
		phys_addr += PAGE_SIZE;
		--idx;
		--nrpages;
	}
	WARN(early_ioremap_debug, "%s(%pa, %08lx) [%d] => %08lx + %08lx\n",
	     __func__, &phys_addr, size, slot, offset, slot_virt[slot]);

	prev_map[slot] = (void __iomem *)(offset + slot_virt[slot]);
	return prev_map[slot];
}

/* Remap an IO device */
void __init __iomem *
early_ioremap(resource_size_t phys_addr, unsigned long size)
{
	return __early_ioremap(phys_addr, size, FIXMAP_PAGE_IO);
}

void __init early_ioremap_setup(void)
{
	int i;

	for (i = 0; i < FIX_BTMAPS_SLOTS; i++) {
		WARN_ON_ONCE(prev_map[i]);
		slot_virt[i] = __fix_to_virt(FIX_BTMAP_BEGIN - NR_FIX_BTMAPS*i);
	}
}
