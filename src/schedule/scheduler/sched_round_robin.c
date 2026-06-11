// SPDX-License-Identifier: GPL-2.0
#include <process.h>
#include <sched.h>
#include <linux/errno.h>
#include <arch_timer.h>
#include <current.h>

#define MAX_PROCESS_COUNT 128

#define TIME_SLICE_NS	(10 * 1000 * 1000) /* 10ms */

DEFINE_PER_CPU(int, g_num_processes);
DEFINE_PER_CPU_ARRAY(struct process_struct *, g_processes, MAX_PROCESS_COUNT);
DEFINE_PER_CPU(int, g_next_process);

static struct process_struct *round_robin_pick_next(void)
{
	int *num_processes = this_cpu_ptr(&g_num_processes);
	int *next_process = this_cpu_ptr(&g_next_process);
	struct process_struct **processes = this_cpu_ptr(&g_processes[0]);

	if (!*num_processes)
		return NULL;

	for (int i = *next_process, j = 0; j < *num_processes; i = (i + 1) % *num_processes, j++) {
		if (processes[i]->state != PROCESS_READY &&
			processes[i]->state != PROCESS_RUNNING)
			continue;
		*next_process = (i + 1) % *num_processes;

		return processes[i];
	}

	return NULL;
}

static int round_robin_load_process(unsigned int cpu, struct process_struct *process)
{
	int *num_processes = per_cpu_ptr(&g_num_processes, cpu);
	struct process_struct **processes = per_cpu_ptr(&g_processes[0], cpu);

	if (!process)
		return -EINVAL;

	process->state = PROCESS_READY;

	process->cpu = cpu;

	processes[*num_processes] = process;

	*num_processes = *num_processes + 1;

	return 0;
}

static bool round_robin_need_resched(struct process_struct *proc)
{
	u64 diff_ns = COUNT_TO_NS(arch_counter_get_val()) - current->last_run_timestamp;

	if (diff_ns > TIME_SLICE_NS)
		return true;

	return false;
}

static u64 round_robin_get_timeslice(void)
{
	return TIME_SLICE_NS;
}

static int round_robin_init(void)
{
	return 0;
}

struct sched_class sched_round_robin = {
	.type = SCHED_ROUND_ROBIN,
	.name = "sched round robin",
	.init = round_robin_init,
	.pick_next = round_robin_pick_next,
	.need_preempt = round_robin_need_resched,
	.add_process = round_robin_load_process,
	.get_timeslice = round_robin_get_timeslice
};

int round_robin_sched_register(void)
{
	return sched_class_register(&sched_round_robin);
}
