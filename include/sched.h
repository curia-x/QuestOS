
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __SCHED_H__
#define __SCHED_H__

#include <process.h>

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
	int (*add_process)(struct process_struct *process);
	u64 (*get_timeslice)(void);
};

extern struct sched_class *g_sched_class;
#define CURRENT_SCHED_CLASS() g_sched_class

int sched_init(sched_type_t sched_type);
static inline int sched_register_process(struct process_struct *process)
{
	return g_sched_class->add_process(process);
}

extern bool need_resched;
static inline void set_resched(void)
{
	need_resched = true;
}

int sched_class_register(struct sched_class *class);
int round_robin_sched_register(void);

void run_scheduler(void);

void preempt_check_resched(void);

#endif /* __SCHED_H__ */
