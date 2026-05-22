// SPDX-License-Identifier: GPL-2.0
#include <ptrace.h>
#include <syscall.h>
#include <unistd_64.h>
#include <sched.h>
#include <current.h>

void el0t_64_handle_svc(struct pt_regs *regs)
{
	syscall_fn_t syscall_fn;
	u64 syscall_nr = regs->regs[SYSCALL_NR_REG_IDX];

	syscall_fn = sys_call_table[syscall_nr];

	regs->regs[0] = syscall_fn(regs);

	if (current->state != PROCESS_RUNNING)
		set_resched();
}
