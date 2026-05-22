/* SPDX-License-Identifier: GPL-2.0 */
#ifndef MMU_H
#define MMU_H

#include <asm/sysreg.h>

/* Memory Attribute Indirection Register (MAIR) attribute encodings */
#define MAIR_ATTR_WIDTH 8

#define DEVICE_ATTR_nGnRnE_IDX 3
#define DEVICE_ATTR_nGnRnE 0b0
#define NORMAL_ATTR_CACHEABLE_IDX 0
#define NORMAL_ATTR_CACHEABLE 0xff
#define MAIR_ATTR(idx) ((idx) << (MAIR_ATTR_WIDTH * (idx)))

#define TCR_TBI1_OFFSET 38
#define TCR_TBI0_OFFSET 37
#define TCR_AS_OFFSET 36
#define TCR_IPS_OFFSET 32
#define TCR_TG1_OFFSET 30
#define TCR_SH1_OFFSET 28
#define TCR_ORGN1_OFFSET 26
#define TCR_IRGN1_OFFSET 24
#define TCR_EPD1_OFFSET 23
#define TCR_A1_OFFSET 22
#define TCR_T1SZ_OFFSET 16
#define TCR_TG0_OFFSET 14
#define TCR_SH0_OFFSET 12
#define TCR_ORGN0_OFFSET 10
#define TCR_IRGN0_OFFSET 8
#define TCR_EPD0_OFFSET 7
#define TCR_T0SZ_OFFSET 0

#define TCR_TBI1_BIT (1 << TCR_TBI1_OFFSET)
#define TCR_TBI0_BIT (1 << TCR_TBI0_OFFSET)
#define TCR_AS_BIT (1 << TCR_AS_OFFSET)
#define TCR_IPS_BIT (1 << TCR_IPS_OFFSET)
#define TCR_TG1_BIT (1 << TCR_TG1_OFFSET)
#define TCR_SH1_BIT (1 << TCR_SH1_OFFSET)
#define TCR_ORGN1_BIT (1 << TCR_ORGN1_OFFSET)
#define TCR_IRGN1_BIT (1 << TCR_IRGN1_OFFSET)
#define TCR_EPD1_BIT (1 << TCR_EPD1_OFFSET)
#define TCR_A1_BIT (1 << TCR_A1_OFFSET)
#define TCR_T1SZ_BIT (1 << TCR_T1SZ_OFFSET)
#define TCR_TG0_BIT (1 << TCR_TG0_OFFSET)
#define TCR_SH0_BIT (1 << TCR_SH0_OFFSET)
#define TCR_ORGN0_BIT (1 << TCR_ORGN0_OFFSET)
#define TCR_IRGN0_BIT (1 << TCR_IRGN0_OFFSET)
#define TCR_EPD0_BIT (1 << TCR_EPD0_OFFSET)
#define TCR_T0SZ_BIT (1 << TCR_T0SZ_OFFSET)

#define TCR_IPS_48BIT (0b101 << TCR_IPS_OFFSET)
// #define TCR_TG1_4K (0b10 << TCR_TG1_OFFSET)
#define TCR_SH1_INNERSHAREABLE (0b11 << TCR_SH1_OFFSET)
#define TCR_ORGN1_WBRAWA (0b01 << TCR_ORGN1_OFFSET)
#define TCR_IRGN1_WBRAWA (0b01 << TCR_IRGN1_OFFSET)
#define TCR_T1SZ_48BIT ((64 - 48) << TCR_T1SZ_OFFSET)
// #define TCR_TG0_4K (0b00 << TCR_TG0_OFFSET)
#define TCR_SH0_INNERSHAREABLE (0b11 << TCR_SH0_OFFSET)
#define TCR_ORGN0_WBRAWA (0b01 << TCR_ORGN0_OFFSET)
#define TCR_IRGN0_WBRAWA (0b01 << TCR_IRGN0_OFFSET)
#define TCR_T0SZ_48BIT ((64 - 48) << TCR_T0SZ_OFFSET)

#define TCR_EL1_DEFAULT_VALUE (TCR_TBI1_BIT | TCR_TBI0_BIT | TCR_IPS_48BIT | \
							TCR_TG1_4K | TCR_SH1_INNERSHAREABLE | TCR_ORGN1_WBRAWA | \
							TCR_IRGN1_WBRAWA | TCR_T1SZ_48BIT | \
							TCR_TG0_4K | TCR_SH0_INNERSHAREABLE | TCR_ORGN0_WBRAWA | \
							TCR_IRGN0_WBRAWA | TCR_T0SZ_48BIT)

#define SCTLR_EL1_MMU_ENABLE 1

#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <system.h>

#define VM_BUG_ON BUG_ON

/*
 * 局限于kernel跳转场景，因为mmu和dcache关闭时并不保证dcache中的内容已经全部写回,
 * 这样操作后包括栈等区域的数据很可能都不正确了(修改后没被写回)。所以执行这个函数后
 * 最好立即跳转kernel，避免后续代码访问到不正确的数据。
 */
static inline void raw_mmu_disable(void)
{
	u64 sctlr;

	__asm__ __volatile__("mrs %0, sctlr_el1" : "=r"(sctlr));
	sctlr &= ~(SCTLR_EL1_M | SCTLR_EL1_C);
	__asm__ __volatile__(
		"dsb sy\n\t"
		"msr sctlr_el1, %0\n\t"
		"isb\n\t"
		:: "r"(sctlr) : "memory");
}

#include <pgtable-types.h>
extern void create_mapping_noalloc(phys_addr_t phys, unsigned long virt,
				   phys_addr_t size, pgprot_t prot);
extern void create_mapping_alloc(phys_addr_t phys, unsigned long virt,
				   phys_addr_t size, pgprot_t prot);
#endif /* __ASSEMBLY__ */

#endif /* MMU_H */