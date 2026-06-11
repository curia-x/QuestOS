// SPDX-License-Identifier: GPL-2.0
#include <linux/types.h>
#include <smp.h>
#include <cpuhp.h>
#include <memory.h>
#include <psci.h>
#include <print.h>
#include <sched.h>
#include <irq.h>

DECLARE_BITMAP(__processor_online_mask, MAX_CPUS);

u64 __cpu_logical_map[MAX_CPUS];

int mpidr_to_processor_id(u64 mpidr)
{
	for (int i = 0; i < MAX_CPUS; i++) {
		if (cpu_logical_map(i) == mpidr)
			return i;
	}

	return -EINVAL;
}

int cpu_init(void)
{
	/* For now, we just record the MPIDR of the boot CPU. */
	set_cpu_logical_map(0, get_mpidr());
	set_cpu_logical_map(1, 1);
	set_cpu_logical_map(2, 2);
	set_cpu_logical_map(3, 3);

	cpu_set_online(0, true);

	set_resched(true);

	return 0;
}

static int bringup_nonboot_cpus(void)
{
	int err = 0;

	for (int i = 1; i < num_possible_cpus(); i++) {
		err |= cpuhp_bringup_cpu(i);
		if (err)
			printf("cpu[%d] bringup failed, err=%d\n", i, err);
	}

	return err;
}

static int cpuhp_start_cpu(unsigned int cpu, void *data)
{
	phys_addr_t entry = virt_to_kimg_phys(secondary_entry);

	return psci_cpu_on(cpu, entry);
}

static int cpuhp_online_cpu(unsigned int cpu, void *data)
{
	cpu_set_online(cpu, true);

	set_resched(true);

	local_interrupt_enable();

	run_idle();

	return 0;
}

int smp_init(void)
{
	int err;

	err = cpuhp_state_register(CPUHP_START_CPU, "cpuhp cpu start", cpuhp_start_cpu, NULL);
	if (err) {
		printf("CPUHP_START_CPU callback register failed\n");
		return err;
	}

	err = cpuhp_state_register(CPUHP_ONLINE, "cpuhp set cpu online", cpuhp_online_cpu, NULL);
	if (err) {
		printf("CPUHP_START_CPU callback register failed\n");
		return err;
	}

	return bringup_nonboot_cpus();
}
