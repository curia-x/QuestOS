// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <asm/sysreg.h>
#include <process.h>
#include <sched.h>
#include <print.h>
#include <barrier.h>
#include <current.h>
#include <arch_timer.h>

struct process_struct *entry_task;

static struct sched_class *g_sched_classes[SCHED_TYPE_MAX];

struct sched_class *g_sched_class;

bool need_resched;

struct sched_class *current_sched_class(void)
{
	return g_sched_class;
}

int sched_class_register(struct sched_class *class)
{
	if (!class || class->type >= SCHED_TYPE_MAX) {
		printf("sched class register failed.\n");
		return -EINVAL;
	}

	g_sched_classes[class->type] = class;

	return 0;
}

int sched_init(sched_type_t sched_type)
{
	if (!g_sched_classes[sched_type]) {
		printf("sched type %u not exist\n", sched_type);
		return -ENOENT;
	}

	g_sched_class = g_sched_classes[sched_type];

	printf("sched_class:%s\n", g_sched_class->name);

	return g_sched_class->init();
}

static inline struct process_struct *pick_next(void)
{
	return g_sched_class->pick_next();
}

extern void process_cpu_context_switch(u64 *kernel_sp, phys_addr_t ttbr0);
static void run_process(struct process_struct *proc)
{
	u64 tick_interval = CURRENT_SCHED_CLASS()->get_timeslice();
	u64 ttbr0 = virt_to_phys(proc->mm.pg_dir);

	/* run_process will always in idle context, set idle to PROCESS_READY */
	current->state = PROCESS_READY;

	/* preempt occured */
	if (entry_task && entry_task->state == PROCESS_RUNNING)
		entry_task->state = PROCESS_READY;

	proc->state = PROCESS_RUNNING;

	proc->last_wake = entry_task;
	entry_task = proc;

	proc->last_run_timestamp = COUNT_TO_NS(arch_counter_get_val());

	if (tick_interval)
		arch_timer_start(tick_interval);

	process_cpu_context_switch(&proc->sp, ttbr0);

	/* we are back to idle, so set it to PROCESS_RUNNING */
	current->state = PROCESS_RUNNING;
}

static void run_idle(void)
{
	static unsigned long long print_count;

	if (!print_count++)
		printf("current:%s running...\n", current->name);
}

void run_scheduler(void)
{
	struct process_struct *process;

	while (true) {
		process = pick_next();
		if (!process) {
			run_idle();
			continue;
		}
		run_process(process);
	}
}

void preempt_check_resched(void)
{
	if (CURRENT_SCHED_CLASS()->need_preempt(current))
		set_resched();
}
