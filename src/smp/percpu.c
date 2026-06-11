// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <smp.h>
#include <percpu.h>
#include <kmalloc.h>
#include <print.h>
#include <cpuhp.h>

#define PERCPU_SIZE (__percpu_end - __percpu_start)

u64 per_cpu_base[MAX_CPUS];

static int cpuhp_per_cpu_setup(unsigned int cpu, void *data)
{
	(void)data;

	if (cpu >= MAX_CPUS) {
		pr_err("Invalid CPU number %u for percpu setup\n", cpu);
		return -EINVAL;
	}

	asm volatile("msr tpidr_el1, %0" :: "r" (per_cpu_base[cpu]) : "memory");

	return 0;
}

int per_cpu_init(void)
{
	int err;

	pr_debug("PERCPU_SIZE:0x%llx, %llx - %llx\n", PERCPU_SIZE, __percpu_start, __percpu_end);

	for (int i = 0; i < MAX_CPUS; i++) {
		char *percpu_base = kmalloc(PERCPU_SIZE, __GFP_ZERO);

		if (!percpu_base)
			return -ENOMEM;

		per_cpu_base[i] = (u64)percpu_base;

		pr_debug("per_cpu[%d]:0x%llx - 0x%llx\n", i, per_cpu_base[i], per_cpu_base[i] + PERCPU_SIZE);
	}

	asm volatile("msr tpidr_el1, %0" :: "r" (per_cpu_base[0]) : "memory");

	err = cpuhp_state_register(CPUHP_PERCPU_INIT, "percpu_setup", cpuhp_per_cpu_setup, NULL);
	if (err) {
		pr_err("Failed to register percpu setup callback: %d\n", err);
		return err;
	}

	pr_info("per_cpu init success.\n");

	return 0;
}

u64 get_percpu_base(int i)
{
	return per_cpu_base[i];
}
