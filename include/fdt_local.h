/* SPDX-License-Identifier: GPL-2.0 */
#ifndef FDT_LOCAL_H
#define FDT_LOCAL_H

#include <init.h>
#include <fdtdec.h>

int fdt_init(const void *fdt);
int fdt_modify_chosen(const char *name, const char *value);
int fdt_get_qfw_node(void);
fdt_addr_t fdt_get_addr(int offset);

#include <asm/pgtable-types.h>
void *__init fixmap_remap_fdt(phys_addr_t dt_phys, int *size, pgprot_t prot);

#endif /* FDT_LOCAL_H */
