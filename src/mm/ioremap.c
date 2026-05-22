// SPDX-License-Identifier: GPL-2.0
#include <init.h>
#include <early_ioremap.h>

/*
 * Must be called after early_fixmap_init
 */
void __init early_ioremap_init(void)
{
	early_ioremap_setup();
}
