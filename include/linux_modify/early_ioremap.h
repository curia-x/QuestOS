/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_EARLY_IOREMAP_H_
#define _ASM_EARLY_IOREMAP_H_

#include <linux/types.h>
#include <init.h>

/* Arch-specific initialization */
extern void early_ioremap_init(void);

void __init early_ioremap_setup(void);

/* Remap an IO device */
void __init __iomem *
early_ioremap(resource_size_t phys_addr, unsigned long size);

#endif /* _ASM_EARLY_IOREMAP_H_ */
