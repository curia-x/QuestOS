
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SCHED_H__
#define __SCHED_H__

#include <process.h>
#include <cpuhp.h>
#include <percpu.h>

typedef enum sched_type {
	SCHED_ROUND_ROBIN,
	SCHED_TYPE_MAX
} sched_type_t;

struct sched_class {
	sched_type_t type;
	char *name;
	int (*init)(void);
	struct process_struct *(*pick_next)(void);
	bool (*need_preempt)(struct process_struct *process);
	int (*add_process)(unsigned int cpu, struct process_struct *process);
	u64 (*get_timeslice)(void);
};

extern struct sched_class *g_sched_class;
#define CURRENT_SCHED_CLASS() g_sched_class

int sched_init(sched_type_t sched_type);
static inline int sched_register_process(unsigned int cpu, struct process_struct *process)
{
	return g_sched_class->add_process(cpu, process);
}
u64 get_percpu_base(int i);
DECLARE_PER_CPU(u8, __need_resched);
static inline void set_resched(bool need_resched)
{
	this_cpu_write(__need_resched, 1);
}

static inline bool need_resched(void)
{
	return this_cpu_read(__need_resched) != 0;
}

int sched_class_register(struct sched_class *class);
int round_robin_sched_register(void);

void run_idle(void);

void preempt_check_resched(void);

void local_sched_timer_start(void);

#endif /* __SCHED_H__ */
