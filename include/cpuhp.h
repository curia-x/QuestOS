// SPDX-License-Identifier: GPL-2.0
#ifndef __CPUHP_H__
#define __CPUHP_H__

#include <linux/types.h>

enum cpuhp_state {
	CPUHP_OFFLINE = 0,
	CPUHP_CREATE_IDLE,
	CPUHP_START_CPU,
	CPUHP_PERCPU_INIT,
	CPUHP_INIT_GIC,
	CPUHP_INIT_TIMER,
	CPUHP_ONLINE,
	CPUHP_DEAD,
	CPUHP_MAX,
};

int
cpuhp_state_register(enum cpuhp_state state,
					char *name,
					int (*callback)(unsigned int cpu, void *data),
					void *data);

int cpuhp_bringup_cpu(unsigned int cpu);

#endif /* __CPUHP_H__ */