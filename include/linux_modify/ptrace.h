/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Based on arch/arm/include/asm/ptrace.h
 *
 * Copyright (C) 1996-2003 Russell King
 * Copyright (C) 2012 ARM Ltd.
 */
#ifndef __ASM_PTRACE_H
#define __ASM_PTRACE_H


/* Current Exception Level values, as contained in CurrentEL */
#define CurrentEL_EL1		(1 << 2)
#define CurrentEL_EL2		(2 << 2)

#define INIT_PSTATE_EL1 \
	(PSR_D_BIT | PSR_A_BIT | PSR_I_BIT | PSR_F_BIT | PSR_MODE_EL1h)
#define INIT_PSTATE_EL2 \
	(PSR_D_BIT | PSR_A_BIT | PSR_I_BIT | PSR_F_BIT | PSR_MODE_EL2h)

#include <linux/irqchip/arm-gic-v3-prio.h>

#define GIC_PRIO_IRQON		GICV3_PRIO_UNMASKED
#define GIC_PRIO_IRQOFF		GICV3_PRIO_IRQ

#define GIC_PRIO_PSR_I_SET	GICV3_PRIO_PSR_I_SET

/* Additional SPSR bits not exposed in the UABI */
#define PSR_MODE_THREAD_BIT	(1 << 0)
#define PSR_IL_BIT		(1 << 20)

/* AArch32-specific ptrace requests */
#define COMPAT_PTRACE_GETREGS		12
#define COMPAT_PTRACE_SETREGS		13
#define COMPAT_PTRACE_GET_THREAD_AREA	22
#define COMPAT_PTRACE_SET_SYSCALL	23
#define COMPAT_PTRACE_GETVFPREGS	27
#define COMPAT_PTRACE_SETVFPREGS	28
#define COMPAT_PTRACE_GETHBPREGS	29
#define COMPAT_PTRACE_SETHBPREGS	30

/* SPSR_ELx bits for exceptions taken from AArch32 */
#define PSR_AA32_MODE_MASK	0x0000001f
#define PSR_AA32_MODE_USR	0x00000010
#define PSR_AA32_MODE_FIQ	0x00000011
#define PSR_AA32_MODE_IRQ	0x00000012
#define PSR_AA32_MODE_SVC	0x00000013
#define PSR_AA32_MODE_ABT	0x00000017
#define PSR_AA32_MODE_HYP	0x0000001a
#define PSR_AA32_MODE_UND	0x0000001b
#define PSR_AA32_MODE_SYS	0x0000001f
#define PSR_AA32_T_BIT		0x00000020
#define PSR_AA32_F_BIT		0x00000040
#define PSR_AA32_I_BIT		0x00000080
#define PSR_AA32_A_BIT		0x00000100
#define PSR_AA32_E_BIT		0x00000200
#define PSR_AA32_PAN_BIT	0x00400000
#define PSR_AA32_SSBS_BIT	0x00800000
#define PSR_AA32_DIT_BIT	0x01000000
#define PSR_AA32_Q_BIT		0x08000000
#define PSR_AA32_V_BIT		0x10000000
#define PSR_AA32_C_BIT		0x20000000
#define PSR_AA32_Z_BIT		0x40000000
#define PSR_AA32_N_BIT		0x80000000
#define PSR_AA32_IT_MASK	0x0600fc00	/* If-Then execution state mask */
#define PSR_AA32_GE_MASK	0x000f0000

#ifdef CONFIG_CPU_BIG_ENDIAN
#define PSR_AA32_ENDSTATE	PSR_AA32_E_BIT
#else
#define PSR_AA32_ENDSTATE	0
#endif

/* AArch32 CPSR bits, as seen in AArch32 */
#define COMPAT_PSR_DIT_BIT	0x00200000

/*
 * These are 'magic' values for PTRACE_PEEKUSR that return info about where a
 * process is located in memory.
 */
#define COMPAT_PT_TEXT_ADDR		0x10000
#define COMPAT_PT_DATA_ADDR		0x10004
#define COMPAT_PT_TEXT_END_ADDR		0x10008

/*
 * If pt_regs.syscallno == NO_SYSCALL, then the thread is not executing
 * a syscall -- i.e., its most recent entry into the kernel from
 * userspace was not via SVC, or otherwise a tracer cancelled the syscall.
 *
 * This must have the value -1, for ABI compatibility with ptrace etc.
 */
#define NO_SYSCALL (-1)

#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <asm/stacktrace/frame.h>

/* sizeof(struct user) for AArch32 */
#define COMPAT_USER_SZ	296

/* Architecturally defined mapping between AArch32 and AArch64 registers */
#define compat_usr(x)	regs[(x)]
#define compat_fp	regs[11]
#define compat_sp	regs[13]
#define compat_lr	regs[14]
#define compat_sp_hyp	regs[15]
#define compat_lr_irq	regs[16]
#define compat_sp_irq	regs[17]
#define compat_lr_svc	regs[18]
#define compat_sp_svc	regs[19]
#define compat_lr_abt	regs[20]
#define compat_sp_abt	regs[21]
#define compat_lr_und	regs[22]
#define compat_sp_und	regs[23]
#define compat_r8_fiq	regs[24]
#define compat_r9_fiq	regs[25]
#define compat_r10_fiq	regs[26]
#define compat_r11_fiq	regs[27]
#define compat_r12_fiq	regs[28]
#define compat_sp_fiq	regs[29]
#define compat_lr_fiq	regs[30]

/*
 * User structures for general purpose, floating point and debug registers.
 */
struct user_pt_regs {
	__u64		regs[31];
	__u64		sp;
	__u64		pc;
	__u64		pstate;
};

struct user_fpsimd_state {
	__uint128_t	vregs[32];
	__u32		fpsr;
	__u32		fpcr;
	__u32		__reserved[2];
};

struct user_hwdebug_state {
	__u32		dbg_info;
	__u32		pad;
	struct {
		__u64	addr;
		__u32	ctrl;
		__u32	pad;
	}		dbg_regs[16];
};

/* SVE/FP/SIMD state (NT_ARM_SVE & NT_ARM_SSVE) */

struct user_sve_header {
	__u32 size; /* total meaningful regset content in bytes */
	__u32 max_size; /* maxmium possible size for this thread */
	__u16 vl; /* current vector length */
	__u16 max_vl; /* maximum possible vector length */
	__u16 flags;
	__u16 __reserved;
};

/*
 * This struct defines the way the registers are stored on the stack during an
 * exception. struct user_pt_regs must form a prefix of struct pt_regs.
 */
struct pt_regs {
	union {
		struct user_pt_regs user_regs;
		struct {
			u64 regs[31];
			u64 sp;
			u64 pc;
			u64 pstate;
		};
	};
};



#endif /* __ASSEMBLY__ */
#endif
