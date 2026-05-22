// SPDX-License-Identifier: GPL-2.0
#include <linux/types.h>
#include <linux/overflow.h>
#include <linux/container_of.h>
#include <linux/log2.h>
#include <memblock.h>
#include <memory.h>
#include <print.h>
#include <linux/string.h>
#include <mem_alloc.h>
#include <gfp.h>
#include <pg_table.h>

static bool kmalloc_ready;

void kmalloc_set_ready(void)
{
	kmalloc_ready = true;
}

bool kmalloc_is_ready(void)
{
	return kmalloc_ready;
}

void *kmalloc(size_t size, gfp_t flags)
{
	phys_addr_t pa;
	phys_addr_t align;
	struct malloc_mem_region *region;

	if (is_power_of_2(size))
		align = size;
	else
		align = 1UL << fls(size);

	size = ALIGN_UP(size, align);

	/* alloc at least 2x size to make sure the returned address can be aligned to size. */
	pa = memblock_phys_alloc_range(struct_size(region, buf, size << 1), 0, 0, 0);

	region = phys_to_virt(pa);

	region->buf_size = size;

	if (flags & __GFP_ZERO)
		memset(region->buf, 0, size);

	return region->buf;
}

void kfree(const void *ptr)
{
	struct malloc_mem_region *region = container_of(ptr, struct malloc_mem_region, buf);

	memblock_phys_free(virt_to_phys(region), struct_size(region, buf, region->buf_size));
}
