// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2026 <Nino Zhang>
 *
 * Parts of the design/logic are inspired by U-Boot or Linux kernel.
 */
#include <asm/assembler.h>

#define BAD_SYNC        0
#define BAD_IRQ         1
#define BAD_FIQ         2
#define BAD_ERROR       3

/*
   处理无效的异常向量
 */
	.macro inv_entry el, reason
	//kernel_entry el
	mov x0, sp
	mov x1, #\reason
	mrs x2, esr_el1
	b undefined_mode

	.endm

	.macro kernel_entry, el
	/* 保存通用寄存器 */
	stp	x0, x1, [sp, #16 * 0]
	stp	x2, x3, [sp, #16 * 1]
	stp	x4, x5, [sp, #16 * 2]
	stp	x6, x7, [sp, #16 * 3]
	stp	x8, x9, [sp, #16 * 4]
	stp	x10, x11, [sp, #16 * 5]
	stp	x12, x13, [sp, #16 * 6]
	stp	x14, x15, [sp, #16 * 7]
	stp	x16, x17, [sp, #16 * 8]
	stp	x18, x19, [sp, #16 * 9]
	stp	x20, x21, [sp, #16 * 10]
	stp	x22, x23, [sp, #16 * 11]
	stp	x24, x25, [sp, #16 * 12]
	stp	x26, x27, [sp, #16 * 13]
	stp	x28, x29, [sp, #16 * 14]

	.if	\el == 0
		/* Save user SP before reusing SP_EL0 as current task pointer. */
		mrs	x21, sp_el0
		/* struct task_struct *entry_task */
		ldr x0, =entry_task
		ldr x1, [x0]
		msr	sp_el0, x1
	.else
		/* Previous kernel SP before pt_regs allocation. */
		add	x21, sp, #PT_REGS_SIZE
	.endif
	/* 获取异常发生时的PC和PSTATE */
	mrs	x22, elr_el1
	mrs	x23, spsr_el1
	/* 保存lr, sp */
	stp	lr, x21, [sp, #S_LR]
	/* 保存异常发生时的PC和PSTATE */
	stp	x22, x23, [sp, #S_PC]
	.endm

	.macro	kernel_exit, el
	.if \el == 0
		ldr x21, [sp, #S_LR + 8]
		msr sp_el0, x21
	.endif

	ldp	x22, x23, [sp, #S_PC]		// load ELR, SPSR
	msr	elr_el1, x22			// set up the return data
	msr	spsr_el1, x23

	ldp	x0, x1, [sp, #16 * 0]
	ldp	x2, x3, [sp, #16 * 1]
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

	ldr	lr, [sp, #S_LR]
	/* 因为sp不能直接用ldp加载，所以即使保存时用的stp lr, x21, [sp, #S_LR]
	 * 这里也不能直接用ldp lr, sp, [sp, #S_LR]来加载(此时x21已经恢复，也不能随便覆盖)
	 */

	add	sp, sp, #PT_REGS_SIZE		// restore sp

	eret
	/* 阻止推测执行，提高安全性 */
	dsb nsh
	isb
	.endm

SYM_CODE_START_LOCAL(ret_to_kernel)
	kernel_exit	1
SYM_CODE_END(ret_to_kernel)

SYM_CODE_START_LOCAL(ret_to_user)
	ldr x21, =need_resched
	ldr x22, [x21]
	cbnz x22, process_resched
	kernel_exit	0

process_resched:
	/* backup current kernel sp */
	mov x21, sp
	/* load idle sp from current->sp */
	mrs	x22, sp_el0
	ldr x23, [x22]
	mov sp, x23
	/* save current kernel sp to current->sp */
	str x21, [x22]

	/* switch ttbr0 to reserved */
	adrp x21, reserved_pg_dir
	msr ttbr0_el1, x21

	/* restore idle kernel context */
	ldr x21, =g_kernel_init_process
	msr sp_el0, x21

	ldp x19, x20, [sp], #16
	ldp x21, x22, [sp], #16
	ldp x23, x24, [sp], #16
	ldp x25, x26, [sp], #16
	ldp x27, x28, [sp], #16
	ldp x29, x30, [sp], #16

	ret

SYM_CODE_END(ret_to_kernel)

	.macro entry_handler, el:req, ht:req, regsize:req, label:req
SYM_CODE_START_LOCAL(el\el\ht\()_\regsize\()_\label)
	/* 保存上下文 */
	kernel_entry \el
	mov x0, sp
	bl	el\el\ht\()_\regsize\()_\label\()_handler
	msr	daifset, #0xf /* 屏蔽调试异常、SError 异常、IRQ、FIQ  */
	.if \el == 0
		b	ret_to_user
	.else
		/* 返回正在执行的kernel代码 */
		b	ret_to_kernel /* 恢复上下文，并eret */
	.endif
SYM_CODE_END(el\el\ht\()_\regsize\()_\label)
	.endm

/*
   vector table entry
   每个表项是128字节， align 7表示128字节对齐
 */
	.macro kernel_ventry label:req
	.align 7
	/* 为保存struct pt_regs预留栈空间 */
	sub	sp, sp, #PT_REGS_SIZE
	b \label
	.endm

/*
 * Init exception vector table
 *
 * ARM64的异常向量表一共占用2048个字节
 * 分成4组，每组4个表项，每个表项占128字节
 * 参见ARMv8 spec v8.6第D1.10节
 * align 11表示2048字节对齐
 */
.align 11
.global init_exception_vectors
init_exception_vectors:
	/* Current EL with SP0
	   当前系统运行在EL1时使用EL0的栈指针SP
	   这是一种异常错误的类型
	 */
	kernel_ventry el1_sync_invalid
	kernel_ventry el1_irq_invalid
	kernel_ventry el1_fiq_invalid
	kernel_ventry el1_error_invalid

	/* Current EL with SPx
	   当前系统运行在EL1时使用EL1的栈指针SP
	   这说明系统在内核态发生了异常

	   Note: 我们暂时只实现IRQ中断
	 */
	kernel_ventry el1_sync_invalid
	kernel_ventry el1_irq_invalid
	kernel_ventry el1_fiq_invalid
	kernel_ventry el1_error_invalid

	/* Lower EL using AArch64
	   在用户态的aarch64的程序发生了异常
	 */
	kernel_ventry el0_sync_invalid
	kernel_ventry el0_irq_invalid
	kernel_ventry el0_fiq_invalid
	kernel_ventry el0_error_invalid

	/* Lower EL using AArch32
	   在用户态的aarch32的程序发生了异常
	 */
	kernel_ventry el0_sync_invalid
	kernel_ventry el0_irq_invalid
	kernel_ventry el0_fiq_invalid
	kernel_ventry el0_error_invalid

el1_sync_invalid:
	inv_entry 1, BAD_SYNC
el1_irq_invalid:
	inv_entry 1, BAD_IRQ
el1_fiq_invalid:
	inv_entry 1, BAD_FIQ
el1_error_invalid:
	inv_entry 1, BAD_ERROR
el0_sync_invalid:
	inv_entry 0, BAD_SYNC
el0_irq_invalid:
	inv_entry 0, BAD_IRQ
el0_fiq_invalid:
	inv_entry 0, BAD_FIQ
el0_error_invalid:
	inv_entry 0, BAD_ERROR

/*
 * Vector Table
 *
 * ARM64的异常向量表一共占用2048个字节
 * 分成4组，每组4个表项，每个表项占128字节
 * 参见ARMv8 spec v8.6第D1.10节
 * align 11表示2048字节对齐
 */
.align 11
.global exception_vector_table
exception_vector_table:
	/* Current EL with SP0
	   当前系统运行在EL1时使用EL0的栈指针SP
	   这是一种异常错误的类型
	 */
	kernel_ventry el1t_64_sync
	kernel_ventry el1t_64_irq
	kernel_ventry el1t_64_fiq
	kernel_ventry el1t_64_error

	/* Current EL with SPx
	   当前系统运行在EL1时使用EL1的栈指针SP
	   这说明系统在内核态发生了异常

	   Note: 我们暂时只实现IRQ中断
	 */
	kernel_ventry el1h_64_sync
	kernel_ventry el1h_64_irq
	kernel_ventry el1h_64_fiq
	kernel_ventry el1h_64_error

	/* Lower EL using AArch64
	   在用户态的aarch64的程序发生了异常
	 */
	kernel_ventry el0t_64_sync
	kernel_ventry el0t_64_irq
	kernel_ventry el0t_64_fiq
	kernel_ventry el0t_64_error

	/* Lower EL using AArch32
	   在用户态的aarch32的程序发生了异常
	 */
	kernel_ventry el0t_32_sync
	kernel_ventry el0t_32_irq
	kernel_ventry el0t_32_fiq
	kernel_ventry el0t_32_error


/* exception handlers */
/* kernel_ventry用于硬件跳转，每个ventry必须小于128字节
 * 而entry_handler被kernel_ventry调用，没有大小限制。
 */
	entry_handler 1, t, 64, sync
	entry_handler 1, t, 64, irq
	entry_handler 1, t, 64, fiq
	entry_handler 1, t, 64, error

	entry_handler 1, h, 64, sync
	entry_handler 1, h, 64, irq
	entry_handler 1, h, 64, fiq
	entry_handler 1, h, 64, error

	entry_handler 0, t, 64, sync
	entry_handler 0, t, 64, irq
	entry_handler 0, t, 64, fiq
	entry_handler 0, t, 64, error

	entry_handler 0, t, 32, sync
	entry_handler 0, t, 32, irq
	entry_handler 0, t, 32, fiq
	entry_handler 0, t, 32, error

string_test:
	.string "t"

/* for test */
.align 2
.global trigger_alignment
trigger_alignment:
	ldr x0, =0x40080002
	ldr x1, [x0]
	ret