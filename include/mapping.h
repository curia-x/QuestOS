/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __MAPPING_H__
#define __MAPPING_H__

#include <linux/types.h>
#include <asm/pgtable-types.h>

void linear_mapping_init(void);
void vmap_range(phys_addr_t phys, unsigned long virt, phys_addr_t size, pgprot_t prot);

#endif /* __MAPPING_H__ */
