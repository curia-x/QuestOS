// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/string.h>
#include <asm/sysreg.h>
#include <process.h>
#include <sched.h>
#include <print.h>
#include <barrier.h>
#include <current.h>
#include <arch_timer.h>
#include <percpu.h>
#include <preempt.h>
#include <irq.h>

static struct sched_class *g_sched_classes[SCHED_TYPE_MAX];

struct sched_class *g_sched_class;

DEFINE_PER_CPU(u8, __need_resched);
DEFINE_PER_CPU(struct process_struct *, entry_task);

struct sched_class *current_sched_class(void)
{
	return g_sched_class;
}

int sched_class_register(struct sched_class *class)
{
	if (!class || class->type >= SCHED_TYPE_MAX) {
		pr_err("sched class register failed.\n");
		return -EINVAL;
	}

	g_sched_classes[class->type] = class;

	return 0;
}

int sched_init(sched_type_t sched_type)
{
	if (!g_sched_classes[sched_type]) {
		pr_err("sched type %u not exist\n", sched_type);
		return -ENOENT;
	}

	g_sched_class = g_sched_classes[sched_type];

	pr_notice("sched_class:%s\n", g_sched_class->name);

	return g_sched_class->init();
}

static inline struct process_struct *pick_next(void)
{
	return g_sched_class->pick_next();
}

extern void process_cpu_context_switch(u64 *kernel_sp, phys_addr_t ttbr0);

static void run_process(struct process_struct *proc)
{
	struct kernel_thread *kthread = proc->kthread;
	u64 ttbr0 = virt_to_phys(proc->mm.pg_dir);

	/* run_process will always in idle context, set idle to PROCESS_READY */
	current->state = PROCESS_READY;

	/* preempt occured */
	if (this_cpu_read(entry_task) && this_cpu_read(entry_task)->state == PROCESS_RUNNING)
		this_cpu_read(entry_task)->state = PROCESS_READY;

	proc->state = PROCESS_RUNNING;

	proc->last_wake = this_cpu_read(entry_task);
	this_cpu_write(entry_task, proc);

	proc->last_run_timestamp = COUNT_TO_NS(arch_counter_get_val());

	/* We already in idle context, just run idle func. */
	if (strncmp(proc->name, "idle", 4) == 0)
		kthread->entry(kthread->arg);
	else /* switch to kernel or process */
		process_cpu_context_switch(&proc->sp, ttbr0);

	/* we are back to idle, so set it to PROCESS_RUNNING */
	current->state = PROCESS_RUNNING;
}

void local_sched_timer_start(void)
{
	u64 tick_interval = CURRENT_SCHED_CLASS()->get_timeslice();

	arch_timer_start(tick_interval);
}

static void schdule(void)
{
	struct process_struct *process;

	local_irq_disable();
	preempt_enable();
	process = pick_next();
	if (!process)
		return;
	set_resched(false);
	run_process(process);
}

void run_idle(void)
{
	while (true) {
		preempt_disable();
		local_irq_enable();
		if (need_resched())
			schdule();
		else
			asm volatile("wfi");
	}
}

void preempt_check_resched(void)
{
	if (CURRENT_SCHED_CLASS()->need_preempt(current))
		set_resched(true);
}
