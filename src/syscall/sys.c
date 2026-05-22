// SPDX-License-Identifier: GPL-2.0-only
/*
 * AArch64-specific system calls implementation
 *
 * Copyright (C) 2012 ARM Ltd.
 * Author: Catalin Marinas <catalin.marinas@arm.com>
 */

#include <linux/compiler.h>
#include <linux/errno.h>
#include <linkage.h>
#include <syscall.h>
#include <unistd_64.h>

/*
 * Non-implemented system calls get redirected here.
 */
asmlinkage long sys_ni_syscall(void)
{
	return -ENOSYS;
}

asmlinkage long __arm64_sys_ni_syscall(const struct pt_regs *__unused)
{
	return sys_ni_syscall();
}

#undef __SYSCALL
#define __SYSCALL(nr, sym)	asmlinkage long __arm64_##sym(const struct pt_regs *);
#include <syscall_table_64.h>

#undef __SYSCALL
#define __SYSCALL(nr, sym)	[nr] = __arm64_##sym,

const syscall_fn_t sys_call_table[__NR_syscalls] = {
	[0 ... __NR_syscalls - 1] = __arm64_sys_ni_syscall,
#include <syscall_table_64.h>
};
