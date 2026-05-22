/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Based on arch/arm/include/asm/cacheflush.h
 *
 * Copyright (C) 1999-2002 Russell King.
 * Copyright (C) 2012 ARM Ltd.
 */
#ifndef __ASM_CACHEFLUSH_H
#define __ASM_CACHEFLUSH_H

#include <barrier.h>
/*
 *	MM Cache Management
 *	===================
 *
 *	The arch/arm64/mm/cache.S implements these methods.
 *
 *	Start addresses are inclusive and end addresses are exclusive; start
 *	addresses should be rounded down, end addresses up.
 *
 *	See Documentation/core-api/cachetlb.rst for more information. Please note that
 *	the implementation assumes non-aliasing VIPT D-cache and (aliasing)
 *	VIPT I-cache.
 *
 *	All functions below apply to the interval [start, end)
 *		- start  - virtual start address (inclusive)
 *		- end    - virtual end address (exclusive)
 *
 *	caches_clean_inval_pou(start, end)
 *
 *		Ensure coherency between the I-cache and the D-cache region to
 *		the Point of Unification.
 *
 *	caches_clean_inval_user_pou(start, end)
 *
 *		Ensure coherency between the I-cache and the D-cache region to
 *		the Point of Unification.
 *		Use only if the region might access user memory.
 *
 *	icache_inval_pou(start, end)
 *
 *		Invalidate I-cache region to the Point of Unification.
 *
 *	dcache_clean_inval_poc(start, end)
 *
 *		Clean and invalidate D-cache region to the Point of Coherency.
 *
 *	dcache_inval_poc(start, end)
 *
 *		Invalidate D-cache region to the Point of Coherency.
 *
 *	dcache_clean_poc(start, end)
 *
 *		Clean D-cache region to the Point of Coherency.
 *
 *	dcache_clean_pop(start, end)
 *
 *		Clean D-cache region to the Point of Persistence.
 *
 *	dcache_clean_pou(start, end)
 *
 *		Clean D-cache region to the Point of Unification.
 */
extern void caches_clean_inval_pou(unsigned long start, unsigned long end);
extern void icache_inval_pou(unsigned long start, unsigned long end);
extern void dcache_clean_inval_poc(unsigned long start, unsigned long end);
extern void dcache_inval_poc(unsigned long start, unsigned long end);
extern void dcache_clean_poc(unsigned long start, unsigned long end);
extern void dcache_clean_pop(unsigned long start, unsigned long end);
extern void dcache_clean_pou(unsigned long start, unsigned long end);
extern long caches_clean_inval_user_pou(unsigned long start, unsigned long end);
extern void sync_icache_aliases(unsigned long start, unsigned long end);

static inline void flush_icache_range(unsigned long start, unsigned long end)
{
	caches_clean_inval_pou(start, end);

	// kick_all_cpus_sync();
}
#define flush_icache_range flush_icache_range

static __always_inline void icache_inval_all_pou(void)
{
	asm("ic	ialluis");
	dsb(ish);
}

#endif /* __ASM_CACHEFLUSH_H */
