/* SPDX-License-Identifier: GPL-2.0 */
#ifndef CACHE_H
#define CACHE_H

#include <linux/compiler_attributes.h>
#include <asm/sysreg.h>

extern void icache_invalid_all_pou(void);
extern void enable_icache_el1(void);
extern void dcache_inval_poc(u64 start, u64 end);
extern void icache_inval_pou(u64 start, u64 end);

static inline void raw_cache_disable(void)
{
	u64 sctlr;

	__asm__ __volatile__("mrs %0, sctlr_el1" : "=r"(sctlr));
	sctlr &= ~(SCTLR_EL1_C | SCTLR_EL1_I);
	__asm__ __volatile__(
		"dsb sy\n\t"
		"msr sctlr_el1, %0\n\t"
		"isb\n\t"
		 : : "r"(sctlr) : "memory");
}

#endif /* CACHE_H */