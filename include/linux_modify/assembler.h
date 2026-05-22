/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Based on arch/arm/include/asm/assembler.h, arch/arm/mm/proc-macros.S
 *
 * Copyright (C) 1996-2000 Russell King
 * Copyright (C) 2012 ARM Ltd.
 */
#ifndef __ASSEMBLY__
#error "Only include this from assembly code"
#endif

#ifndef __ASM_ASSEMBLER_H
#define __ASM_ASSEMBLER_H
#include <asm/ptrace.h>
	/*
	 * Provide a wxN alias for each wN register so what we can paste a xN
	 * reference after a 'w' to obtain the 32-bit version.
	 */
	.irp	n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
	wx\n	.req	w\n
	.endr

	.macro disable_daif
	msr	daifset, #0xf
	.endm

/*
 * Save/restore interrupts.
 */
	.macro save_and_disable_daif, flags
	mrs	\flags, daif
	msr	daifset, #0xf
	.endm

	.macro	save_and_disable_irq, flags
	mrs	\flags, daif
	msr	daifset, #3
	.endm

	.macro	restore_irq, flags
	msr	daif, \flags
	.endm

	.macro	disable_step_tsk, flgs, tmp
	tbz	\flgs, #TIF_SINGLESTEP, 9990f
	mrs	\tmp, mdscr_el1
	bic	\tmp, \tmp, #MDSCR_EL1_SS
	msr	mdscr_el1, \tmp
	isb	// Take effect before a subsequent clear of DAIF.D
9990:
	.endm

	/* call with daif masked */
	.macro	enable_step_tsk, flgs, tmp
	tbz	\flgs, #TIF_SINGLESTEP, 9990f
	mrs	\tmp, mdscr_el1
	orr	\tmp, \tmp, #MDSCR_EL1_SS
	msr	mdscr_el1, \tmp
9990:
	.endm

/*
 * Value prediction barrier
 */
	.macro	csdb
	hint	#20
	.endm

/*
 * Clear Branch History instruction
 */
	.macro clearbhb
	hint	#22
	.endm

/*
 * Speculation barrier
 */
	.macro	sb
	dsb	nsh
	isb
	.endm

/*
 * NOP sequence
 */
	.macro	nops, num
	.rept	\num
	nop
	.endr
	.endm

/*
 * Register aliases.
 */
lr	.req	x30		// link register

/*
 * Vector entry
 */
	 .macro	ventry	label
	.align	7
	b	\label
	.endm

/*
 * Select code when configured for BE.
 */
#ifdef CONFIG_CPU_BIG_ENDIAN
#define CPU_BE(code...) code
#else
#define CPU_BE(code...)
#endif

/*
 * Select code when configured for LE.
 */
#ifdef CONFIG_CPU_BIG_ENDIAN
#define CPU_LE(code...)
#else
#define CPU_LE(code...) code
#endif

/*
 * Define a macro that constructs a 64-bit value by concatenating two
 * 32-bit registers. Note that on big endian systems the order of the
 * registers is swapped.
 */
#ifndef CONFIG_CPU_BIG_ENDIAN
	.macro	regs_to_64, rd, lbits, hbits
#else
	.macro	regs_to_64, rd, hbits, lbits
#endif
	orr	\rd, \lbits, \hbits, lsl #32
	.endm

/*
 * Pseudo-ops for PC-relative adr/ldr/str <reg>, <symbol> where
 * <symbol> is within the range +/- 4 GB of the PC.
 */
	/*
	 * @dst: destination register (64 bit wide)
	 * @sym: name of the symbol
	 */
	.macro	adr_l, dst, sym
	adrp	\dst, \sym
	add	\dst, \dst, :lo12:\sym
	.endm

	/*
	 * @dst: destination register (32 or 64 bit wide)
	 * @sym: name of the symbol
	 * @tmp: optional 64-bit scratch register to be used if <dst> is a
	 *       32-bit wide register, in which case it cannot be used to hold
	 *       the address
	 */
	.macro	ldr_l, dst, sym, tmp=
	.ifb	\tmp
	adrp	\dst, \sym
	ldr	\dst, [\dst, :lo12:\sym]
	.else
	adrp	\tmp, \sym
	ldr	\dst, [\tmp, :lo12:\sym]
	.endif
	.endm

	/*
	 * @src: source register (32 or 64 bit wide)
	 * @sym: name of the symbol
	 * @tmp: mandatory 64-bit scratch register to calculate the address
	 *       while <src> needs to be preserved.
	 */
	.macro	str_l, src, sym, tmp
	adrp	\tmp, \sym
	str	\src, [\tmp, :lo12:\sym]
	.endm

	/*
	 * @dst: Result of per_cpu(sym, smp_processor_id()) (can be SP)
	 * @sym: The name of the per-cpu variable
	 * @tmp: scratch register
	 */
	.macro adr_this_cpu, dst, sym, tmp
	adrp	\tmp, \sym
	add	\dst, \tmp, #:lo12:\sym
	get_this_cpu_offset \tmp
	add	\dst, \dst, \tmp
	.endm

	/*
	 * @dst: Result of READ_ONCE(per_cpu(sym, smp_processor_id()))
	 * @sym: The name of the per-cpu variable
	 * @tmp: scratch register
	 */
	.macro ldr_this_cpu dst, sym, tmp
	adr_l	\dst, \sym
	get_this_cpu_offset \tmp
	ldr	\dst, [\dst, \tmp]
	.endm

/*
 * read_ctr - read CTR_EL0. If the system has mismatched register fields,
 * provide the system wide safe value from arm64_ftr_reg_ctrel0.sys_val
 */
	.macro	read_ctr, reg
	mrs	\reg, ctr_el0			// read CTR
	.endm


/*
 * raw_dcache_line_size - get the minimum D-cache line size on this CPU
 * from the CTR register.
 */
	.macro	raw_dcache_line_size, reg, tmp
	mrs	\tmp, ctr_el0			// read CTR
	ubfm	\tmp, \tmp, #16, #19		// cache line size encoding
	mov	\reg, #4			// bytes per word
	lsl	\reg, \reg, \tmp		// actual cache line size
	.endm

/*
 * dcache_line_size - get the safe D-cache line size across all CPUs
 */
	.macro	dcache_line_size, reg, tmp
	read_ctr	\tmp
	ubfm		\tmp, \tmp, #16, #19	// cache line size encoding
	mov		\reg, #4		// bytes per word
	lsl		\reg, \reg, \tmp	// actual cache line size
	.endm

/*
 * raw_icache_line_size - get the minimum I-cache line size on this CPU
 * from the CTR register.
 */
	.macro	raw_icache_line_size, reg, tmp
	mrs	\tmp, ctr_el0			// read CTR
	and	\tmp, \tmp, #0xf		// cache line size encoding
	mov	\reg, #4			// bytes per word
	lsl	\reg, \reg, \tmp		// actual cache line size
	.endm

/*
 * icache_line_size - get the safe I-cache line size across all CPUs
 */
	.macro	icache_line_size, reg, tmp
	read_ctr	\tmp
	and		\tmp, \tmp, #0xf	// cache line size encoding
	mov		\reg, #4		// bytes per word
	lsl		\reg, \reg, \tmp	// actual cache line size
	.endm

/*
 * tcr_set_t0sz - update TCR.T0SZ so that we can load the ID map
 */
	.macro	tcr_set_t0sz, valreg, t0sz
	bfi	\valreg, \t0sz, #TCR_T0SZ_OFFSET, #TCR_TxSZ_WIDTH
	.endm

/*
 * tcr_set_t1sz - update TCR.T1SZ
 */
	.macro	tcr_set_t1sz, valreg, t1sz
	bfi	\valreg, \t1sz, #TCR_T1SZ_OFFSET, #TCR_TxSZ_WIDTH
	.endm

/*
 * tcr_compute_pa_size - set TCR.(I)PS to the highest supported
 * ID_AA64MMFR0_EL1.PARange value
 *
 *	tcr:		register with the TCR_ELx value to be updated
 *	pos:		IPS or PS bitfield position
 *	tmp{0,1}:	temporary registers
 */
	.macro	tcr_compute_pa_size, tcr, pos, tmp0, tmp1
	mrs	\tmp0, ID_AA64MMFR0_EL1
	// Narrow PARange to fit the PS field in TCR_ELx
	ubfx	\tmp0, \tmp0, #ID_AA64MMFR0_EL1_PARANGE_SHIFT, #3
	mov	\tmp1, #ID_AA64MMFR0_EL1_PARANGE_MAX
	cmp	\tmp0, \tmp1
	csel	\tmp0, \tmp1, \tmp0, hi
	bfi	\tcr, \tmp0, \pos, #3
	.endm

/*
 * Macro to perform a data cache maintenance for the interval
 * [start, end) with dcache line size explicitly provided.
 *
 * 	op:		operation passed to dc instruction
 * 	domain:		domain used in dsb instruciton
 * 	start:          starting virtual address of the region
 * 	end:            end virtual address of the region
 *	linesz:		dcache line size
 * 	fixup:		optional label to branch to on user fault
 * 	Corrupts:       start, end, tmp
 */
	.macro dcache_by_myline_op op, domain, start, end, linesz, tmp, fixup
	sub	\tmp, \linesz, #1
	bic	\start, \start, \tmp
.Ldcache_op\@:
	.ifc	\op, cvau
	__dcache_op_workaround_clean_cache \op, \start
	.else
	.ifc	\op, cvac
	__dcache_op_workaround_clean_cache \op, \start
	.else
	.ifc	\op, cvap
	sys	3, c7, c12, 1, \start	// dc cvap
	.else
	.ifc	\op, cvadp
	sys	3, c7, c13, 1, \start	// dc cvadp
	.else
	dc	\op, \start
	.endif
	.endif
	.endif
	.endif
	add	\start, \start, \linesz
	cmp	\start, \end
	b.lo	.Ldcache_op\@
	dsb	\domain

	_cond_uaccess_extable .Ldcache_op\@, \fixup
	.endm

/*
 * Macro to perform a data cache maintenance for the interval
 * [start, end)
 *
 * 	op:		operation passed to dc instruction
 * 	domain:		domain used in dsb instruciton
 * 	start:          starting virtual address of the region
 * 	end:            end virtual address of the region
 * 	fixup:		optional label to branch to on user fault
 * 	Corrupts:       start, end, tmp1, tmp2
 */
	.macro dcache_by_line_op op, domain, start, end, tmp1, tmp2, fixup
	dcache_line_size \tmp1, \tmp2
	dcache_by_myline_op \op, \domain, \start, \end, \tmp1, \tmp2, \fixup
	.endm

/*
 * Macro to perform an instruction cache maintenance for the interval
 * [start, end)
 *
 * 	start, end:	virtual addresses describing the region
 *	fixup:		optional label to branch to on user fault
 * 	Corrupts:	tmp1, tmp2
 */
	.macro invalidate_icache_by_line start, end, tmp1, tmp2, fixup
	icache_line_size \tmp1, \tmp2
	sub	\tmp2, \tmp1, #1
	bic	\tmp2, \start, \tmp2
.Licache_op\@:
	ic	ivau, \tmp2			// invalidate I line PoU
	add	\tmp2, \tmp2, \tmp1
	cmp	\tmp2, \end
	b.lo	.Licache_op\@
	dsb	ish
	isb

	_cond_uaccess_extable .Licache_op\@, \fixup
	.endm

/*
 * load_ttbr1 - install @pgtbl as a TTBR1 page table
 * pgtbl preserved
 * tmp1/tmp2 clobbered, either may overlap with pgtbl
 */
	.macro		load_ttbr1, pgtbl, tmp1, tmp2
	phys_to_ttbr	\tmp1, \pgtbl
	offset_ttbr1 	\tmp1, \tmp2
	msr		ttbr1_el1, \tmp1
	isb
	.endm

/*
 * To prevent the possibility of old and new partial table walks being visible
 * in the tlb, switch the ttbr to a zero page when we invalidate the old
 * records. D4.7.1 'General TLB maintenance requirements' in ARM DDI 0487A.i
 * Even switching to our copied tables will cause a changed output address at
 * each stage of the walk.
 */
	.macro break_before_make_ttbr_switch zero_page, page_table, tmp, tmp2
	phys_to_ttbr \tmp, \zero_page
	msr	ttbr1_el1, \tmp
	isb
	tlbi	vmalle1
	dsb	nsh
	load_ttbr1 \page_table, \tmp, \tmp2
	.endm

/*
 * reset_pmuserenr_el0 - reset PMUSERENR_EL0 if PMUv3 present
 */
	.macro	reset_pmuserenr_el0, tmpreg
	mrs	\tmpreg, id_aa64dfr0_el1
	ubfx	\tmpreg, \tmpreg, #ID_AA64DFR0_EL1_PMUVer_SHIFT, #4
	cmp	\tmpreg, #ID_AA64DFR0_EL1_PMUVer_NI
	ccmp	\tmpreg, #ID_AA64DFR0_EL1_PMUVer_IMP_DEF, #4, ne
	b.eq	9000f				// Skip if no PMU present or IMP_DEF
	msr	pmuserenr_el0, xzr		// Disable PMU access from EL0
9000:
	.endm

/*
 * reset_amuserenr_el0 - reset AMUSERENR_EL0 if AMUv1 present
 */
	.macro	reset_amuserenr_el0, tmpreg
	mrs	\tmpreg, id_aa64pfr0_el1	// Check ID_AA64PFR0_EL1
	ubfx	\tmpreg, \tmpreg, #ID_AA64PFR0_EL1_AMU_SHIFT, #4
	cbz	\tmpreg, .Lskip_\@		// Skip if no AMU present
	msr_s	SYS_AMUSERENR_EL0, xzr		// Disable AMU access from EL0
.Lskip_\@:
	.endm
/*
 * copy_page - copy src to dest using temp registers t1-t8
 */
	.macro copy_page dest:req src:req t1:req t2:req t3:req t4:req t5:req t6:req t7:req t8:req
9998:	ldp	\t1, \t2, [\src]
	ldp	\t3, \t4, [\src, #16]
	ldp	\t5, \t6, [\src, #32]
	ldp	\t7, \t8, [\src, #48]
	add	\src, \src, #64
	stnp	\t1, \t2, [\dest]
	stnp	\t3, \t4, [\dest, #16]
	stnp	\t5, \t6, [\dest, #32]
	stnp	\t7, \t8, [\dest, #48]
	add	\dest, \dest, #64
	tst	\src, #(PAGE_SIZE - 1)
	b.ne	9998b
	.endm

	/*
	 * Emit a 64-bit absolute little endian symbol reference in a way that
	 * ensures that it will be resolved at build time, even when building a
	 * PIE binary. This requires cooperation from the linker script, which
	 * must emit the lo32/hi32 halves individually.
	 */
	.macro	le64sym, sym
	.long	\sym\()_lo32
	.long	\sym\()_hi32
	.endm

	/*
	 * mov_q - move an immediate constant into a 64-bit register using
	 *         between 2 and 4 movz/movk instructions (depending on the
	 *         magnitude and sign of the operand)
	 */
	.macro	mov_q, reg, val
	.if (((\val) >> 31) == 0 || ((\val) >> 31) == 0x1ffffffff)
	movz	\reg, :abs_g1_s:\val
	.else
	.if (((\val) >> 47) == 0 || ((\val) >> 47) == 0x1ffff)
	movz	\reg, :abs_g2_s:\val
	.else
	movz	\reg, :abs_g3:\val
	movk	\reg, :abs_g2_nc:\val
	.endif
	movk	\reg, :abs_g1_nc:\val
	.endif
	movk	\reg, :abs_g0_nc:\val
	.endm

/*
 * Arrange a physical address in a TTBR register, taking care of 52-bit
 * addresses.
 *
 * 	phys:	physical address, preserved
 * 	ttbr:	returns the TTBR value
 */
	.macro	phys_to_ttbr, ttbr, phys
#ifdef CONFIG_ARM64_PA_BITS_52
	orr	\ttbr, \phys, \phys, lsr #46
	and	\ttbr, \ttbr, #TTBR_BADDR_MASK_52
#else
	mov	\ttbr, \phys
#endif
	.endm

	.macro	phys_to_pte, pte, phys
#ifdef CONFIG_ARM64_PA_BITS_52
	orr	\pte, \phys, \phys, lsr #PTE_ADDR_HIGH_SHIFT
	and	\pte, \pte, #PHYS_TO_PTE_ADDR_MASK
#else
	mov	\pte, \phys
#endif
	.endm

/*
 * Set SCTLR_ELx to the @reg value, and invalidate the local icache
 * in the process. This is called when setting the MMU on.
 */
.macro set_sctlr, sreg, reg
	msr	\sreg, \reg
	isb
	/*
	 * Invalidate the local I-cache so that any instructions fetched
	 * speculatively from the PoC are discarded, since they may have
	 * been dynamically patched at the PoU.
	 */
	ic	iallu
	dsb	nsh
	isb
.endm

.macro set_sctlr_el1, reg
	set_sctlr sctlr_el1, \reg
.endm

.macro set_sctlr_el2, reg
	set_sctlr sctlr_el2, \reg
.endm

/*
 * Branch Target Identifier (BTI)
 */
	.macro  bti, targets
	.equ	.L__bti_targets_c, 34
	.equ	.L__bti_targets_j, 36
	.equ	.L__bti_targets_jc,38
	hint	#.L__bti_targets_\targets
	.endm

#endif	/* __ASM_ASSEMBLER_H */
