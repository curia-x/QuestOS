// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <smp.h>
#include <percpu.h>
#include <kmalloc.h>

#define PERCPU_SIZE (__percpu_end - __percpu_start)

u64 per_cpu_base[MAX_CPUS];

int per_cpu_init(void)
{
	for (int i = 0; i < MAX_CPUS; i++) {
		char *percpu_base = kmalloc(PERCPU_SIZE, __GFP_ZERO);

		if (!percpu_base)
			return -ENOMEM;

		per_cpu_base[i] = (u64)percpu_base;
	}

	return 0;
}
