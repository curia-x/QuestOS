// SPDX-License-Identifier: GPL-2.0-only
#include <linkage.h>
#include <asm/sysreg.h>
#include <asm/generated/asm-offsets.h>

/*
 * x0: u32 *state;
 */
SYM_FUNC_START(raw_arch_spin_lock)
1:
	ldr		w1, [x0]
	cbnz	w1, 1b
	ldaxr	w1, [x0]
	cbnz	w1, 2f
	mov		w1, #1
	stxr	w2, w1, [x0]
	cbnz    w2, 1b

	ret

2:
	clrex
	b		1b
SYM_FUNC_END(raw_arch_spin_lock)

/*
 * x0: struct spinlock *lock;
 */
SYM_FUNC_START(raw_arch_spin_unlock)
	stlr	wzr, [x0]
	ret
SYM_FUNC_END(raw_arch_spin_unlock)
