// SPDX-License-Identifier: GPL-2.0
#include <linux/compiler.h>
#include <linux/compiler_attributes.h>
#include <linux/errno.h>
#include <uaccess.h>
#include <unistd_64.h>
#include <syscall.h>
#include <sched.h>
#include <current.h>
#include <print.h>

static long do_exit(long code)
{
	printf("process:%s exited(%d).\n", current->name, code);

	current->state = PROCESS_ZOMBIE;

	return 0;
}

SYSCALL_DEFINE1(exit, int, error_code)
{
	return do_exit((error_code&0xff)<<8);
}

static ssize_t ksys_write(unsigned int fd, const char __user *buf, size_t count)
{
	char tmp_buf[128];

	if (copy_from_user(tmp_buf, buf, count)) {
		printf("%s, copy_from_user failed\n", current->name);
		return -EFAULT;
	}

	tmp_buf[count] = '\0';

	printf("%s", tmp_buf);

	return count;
}

SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf,
		size_t, count)
{
	return ksys_write(fd, buf, count);
}
