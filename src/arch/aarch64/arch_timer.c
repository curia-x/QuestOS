// SPDX-License-Identifier: GPL-2.0
#include <asm/sysreg.h>
#include <arch_timer.h>
#include <print.h>
#include <arm-gic-v3.h>
#include <interrupt.h>
#include <irqreturn.h>
#include <arch_timer.h>
#include <sched.h>

struct timer_context {
	u64 start_time;
	u32 ns;
};

struct timer_context g_context;

irqreturn_t timer_irq_handler(int hwirq, void *param)
{

	if (!arch_timer_condition_met())
		return IRQ_NONE;

	arch_timer_disable();

	preempt_check_resched();

	return IRQ_HANDLED;
}

int arch_timer_init(void)
{
	int ret;

	printf("Arch timer freq:%u HZ\n", COUNTER_FREQ);
	printf("Arch timer resolution:%u ns\n", COUNTER_RESOLUTION_NS);

	/* disable event stream */
	write_sysreg(0, CNTKCTL_EL1);

	/* disable timer */
	write_sysreg(0, CNTP_CTL_EL0);
	write_sysreg(0, CNTV_CTL_EL0);

	ret = request_irq(ARCH_TIMER_CNTP_IRQ, timer_irq_handler, 0, "arch_timer_irq", &g_context);
	if (ret < 0) {
		printf("Arch timer irq request failed:%d\n", ret);
		return ret;
	}

	return 0;
}

int arch_timer_start(s32 ns)
{
	/* Set timer value. */
	write_sysreg((u64)(s32)NS_TO_COUNT(ns), CNTP_TVAL_EL0);

	/* Unmask interrupt and enable timer. */
	write_sysreg((~CNTP_CTL_EL0_BIT_IMASK) | CNTP_CTL_EL0_BIT_ENABLE, CNTP_CTL_EL0);

	g_context.start_time = arch_timer_get_counter_val();
	g_context.ns = ns;

	return 0;
}
