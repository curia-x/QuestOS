// SPDX-License-Identifier: GPL-2.0

#include <linux/types.h>
#include <linux/errno.h>
#include <print.h>
#include <cpuhp.h>
#include <smp.h>
#include <process.h>
#include <current.h>

struct cpuhp_state_callback {
	char *name;
	int (*callback)(unsigned int cpu, void *data);
	void *data;
};

static struct cpuhp_state_callback g_cpuhp_callbacks[CPUHP_MAX];

int
cpuhp_state_register(enum cpuhp_state state,
					char *name,
					int (*callback)(unsigned int cpu, void *data),
					void *data)
{
	if (state <= CPUHP_OFFLINE || state >= CPUHP_MAX || !callback) {
		pr_err("cpuhp state register failed, invalid param\n");
		return -EINVAL;
	}

	g_cpuhp_callbacks[state].callback = callback;
	g_cpuhp_callbacks[state].data = data;
	g_cpuhp_callbacks[state].name = name;

	return 0;
}

int cpuhp_bringup_cpu(unsigned int cpu)
{
	int err;

	for (int i = 0; i <= CPUHP_START_CPU; i++) {
		if (!g_cpuhp_callbacks[i].callback)
			continue;
		err = g_cpuhp_callbacks[i].callback(cpu, g_cpuhp_callbacks[i].data);
		if (err) {
			pr_err("cpuhp:%s for %u fail.\n", g_cpuhp_callbacks[i].name, cpu);
			goto err;
		}
	}

	cpu_wait_online(cpu);

	return 0;

err:
	return err;
}

/* executed by secondary cpus */
void cpuhp_init_to_online(void)
{
	int err;

	struct process_struct *process = get_idle_process();
	unsigned int cpu = process->cpu;

	for (int i = CPUHP_START_CPU + 1; i <= CPUHP_ONLINE; i++) {
		if (!g_cpuhp_callbacks[i].callback)
			continue;
		err = g_cpuhp_callbacks[i].callback(cpu, g_cpuhp_callbacks[i].data);
		if (err) {
			pr_err("cpuhp:%s for %u fail.\n", g_cpuhp_callbacks[i].name, cpu);
			goto err;
		}
	}

err:
	for (;;)
		cpu_relax();
}
