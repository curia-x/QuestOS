// SPDX-License-Identifier: GPL-2.0
#include <mm.h>
#include <fixmap.h>
#include <early_ioremap.h>
#include <vmalloc.h>
#include <mapping.h>
#include <physmem.h>
#include <kmalloc.h>

void mm_early_init(void)
{
	early_fixmap_init();

	early_ioremap_init();
}

void mm_post_init(void)
{
	physmem_init();

	linear_mapping_init();

	kmalloc_set_ready();

	vmalloc_init();
}
