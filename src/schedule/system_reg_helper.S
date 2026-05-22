// SPDX-License-Identifier: GPL-2.0
#include <asm/assembler.h>

/*
 * x0: &kernel_sp
 * x1: ttbr0
 */
SYM_FUNC_START(process_cpu_context_switch)
	stp	x29, x30, [sp, #-16]!
	stp	x27, x28, [sp, #-16]!
	stp	x25, x26, [sp, #-16]!
	stp	x23, x24, [sp, #-16]!
	stp	x21, x22, [sp, #-16]!
	stp	x19, x20, [sp, #-16]!

	/* save current sp */
	mov x2, sp
	/* Switch to target kernel stack */
	ldr	x3, [x0]
	mov	sp, x3
	/* backup current sp to current process's proc->sp */
	str x2, [x0]

	/* Restore x4 - x29 first, while x0 still points to regs */
	ldp	x4, x5, [sp, #16 * 2]
	ldp	x6, x7, [sp, #16 * 3]
	ldp	x8, x9, [sp, #16 * 4]
	ldp	x10, x11, [sp, #16 * 5]
	ldp	x12, x13, [sp, #16 * 6]
	ldp	x14, x15, [sp, #16 * 7]
	ldp	x16, x17, [sp, #16 * 8]
	ldp	x18, x19, [sp, #16 * 9]
	ldp	x20, x21, [sp, #16 * 10]
	ldp	x22, x23, [sp, #16 * 11]
	ldp	x24, x25, [sp, #16 * 12]
	ldp	x26, x27, [sp, #16 * 13]
	ldp	x28, x29, [sp, #16 * 14]

	/* Ensure new page tables are visible before installing TTBR0 */
	dsb	ishst

	/* Switch user address space */
	msr		ttbr0_el1, x1
	isb
	/* No ASID case: drop stale EL1/EL0 translations */
	tlbi	vmalle1
	dsb		ish
	isb

	/* Restore x30 and user SP */
	ldp	x0, x1, [sp, #16 * 15]
	mov	x30, x0
	msr	sp_el0, x1

	/* Restore exception return state */
	ldp	x0, x1, [sp, #16 * 16]
	msr	elr_el1, x0
	msr	spsr_el1, x1

	ldp	x0, x1, [sp, #16 * 0]
	ldp	x2, x3, [sp, #16 * 1]

	add sp, sp, #PT_REGS_SIZE

	eret

SYM_FUNC_END(process_cpu_context_switch)
