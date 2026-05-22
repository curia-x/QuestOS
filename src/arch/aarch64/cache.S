// SPDX-License-Identifier: GPL-2.0
#include <asm/assembler.h>

.global icache_invalid_all_pou;
.type icache_invalid_all_pou @function;
.balign 8;
icache_invalid_all_pou:
	ic iallu
	dsb sy
	isb
	ret

.size icache_invalid_all_pou, .-icache_invalid_all_pou

// enable_icache_el1:
//   作用：Invalidate I-cache，然后置位 SCTLR_EL1.I 打开 I-cache
SYM_FUNC_START(enable_icache_el1)
    // 1) 失效 I-cache（到 PoU）
    ic      iallu
    dsb     sy
    isb

    // 2) 打开 SCTLR_EL1.I
    mrs     x0, sctlr_el1
    orr     x0, x0, #(1 << 12)     // SCTLR_EL1.I bit[12]
    msr     sctlr_el1, x0
    isb

    ret
SYM_FUNC_END(enable_icache_el1)

// porting from linux

/*
 *	dcache_inval_poc(start, end)
 *
 * 	Ensure that any D-cache lines for the interval [start, end)
 * 	are invalidated. Any partial lines at the ends of the interval are
 *	also cleaned to PoC to prevent data loss.
 *
 *	- start   - kernel start address of region
 *	- end     - kernel end address of region
 */
SYM_FUNC_START(dcache_inval_poc)
	mrs x2, ctr_el0
	sub	x3, x2, #1
	tst	x1, x3				// end cache line aligned?
	bic	x1, x1, x3
	b.eq	1f
	dc	civac, x1			// clean & invalidate D / U line
1:	tst	x0, x3				// start cache line aligned?
	bic	x0, x0, x3
	b.eq	2f
	dc	civac, x0			// clean & invalidate D / U line
	b	3f
2:	dc	ivac, x0			// invalidate D / U line
3:	add	x0, x0, x2
	cmp	x0, x1
	b.lo	2b
	dsb	sy
	ret
SYM_FUNC_END(dcache_inval_poc)

SYM_FUNC_START(dcache_clean_poc)
	mrs x2, ctr_el0
	sub	x3, x2, #1
	bic	x0, x0, x3
1:	dc	cvac, x0
	add	x0, x0, x2
	cmp	x0, x1
	b.lo	1b
	dsb	sy
	ret
SYM_FUNC_END(dcache_clean_poc)

SYM_FUNC_START(icache_inval_pou)
	mrs x2, ctr_el0
	sub	x3, x2, #1
	bic	x0, x0, x3
1:	ic	ivau, x0			// invalidate D / U line
	add	x0, x0, x2
	cmp	x0, x1
	b.lo	1b
	dsb	sy
	ret
SYM_FUNC_END(icache_inval_pou)