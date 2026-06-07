// SPDX-License-Identifier: GPL-2.0-only
#include <linux/stddef.h>
#include <smc.h>
#include <process.h>
#include <ptrace.h>

#define DEFINE(sym, val) \
	asm volatile("\n->" #sym " %0" : : "i" (val))

void asm_offsets(void)
{
	/* For struct arm_smccc_res */
	DEFINE(ARM_SMCCC_RES_X0_OFFS,		offsetof(struct arm_smccc_res, a0));
	DEFINE(ARM_SMCCC_RES_X2_OFFS,		offsetof(struct arm_smccc_res, a2));

	/* For struct process_struct */
	DEFINE(PROCESS_PC_OFFS,		offsetof(struct process_struct, pc));
	DEFINE(PROCESS_SP_OFFS,		offsetof(struct process_struct, sp));
	DEFINE(PROCESS_KERNEL_STACK_OFFS,		offsetof(struct process_struct, kernel_stack));
	DEFINE(PROCESS_KERNEL_STACK_SIZE_OFFS,	offsetof(struct process_struct, kernel_stack_size));

	/* For struct user_pt_regs */
	DEFINE(USER_PT_REGS_SIZE,	sizeof(struct user_pt_regs));
}
