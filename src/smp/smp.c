// SPDX-License-Identifier: GPL-2.0
#include <linux/types.h>
#include <smp.h>

DECLARE_BITMAP(__processor_online_mask, MAX_CPUS);

u64 __cpu_logical_map[MAX_CPUS];

int mpidr_to_processor_id(u64 mpidr)
{
	for (int i = 0; i < MAX_CPUS; i++) {
		if (cpu_logical_map(i) == mpidr)
			return i;
	}

	return 0;
}

int smp_init(void)
{
	/* For now, we just record the MPIDR of the boot CPU. */
	set_cpu_logical_map(0, get_mpidr_el1());

	return 0;
}
