/* SPDX-License-Identifier: GPL-2.0 */
#ifndef ARCH_TIMER_H
#define ARCH_TIMER_H

/* 把其他头文件中udelay和mdelay的定义全部改名，防止和这里的实现冲突 */
#define udelay udelay_not_use
#define mdelay mdelay_not_use

#include <linux/types.h>
#include <asm/vdso/processor.h>
#include <asm/sysreg.h>
#include <system.h>

#define ARCH_TIMER_CNTP_IRQ 30

#define CNTP_CTL_EL0_OFF_ISTATUS 2
#define CNTP_CTL_EL0_OFF_IMASK   1
#define CNTP_CTL_EL0_OFF_ENABLE  0

#define CNTP_CTL_EL0_BIT_ISTATUS (1 << CNTP_CTL_EL0_OFF_ISTATUS)
#define CNTP_CTL_EL0_BIT_IMASK (1 << CNTP_CTL_EL0_OFF_IMASK)
#define CNTP_CTL_EL0_BIT_ENABLE (1 << CNTP_CTL_EL0_OFF_ENABLE)

int arch_timer_init(void);
int arch_timer_start(s32 ns);

/* 放在"#include <...>"后。取消改名，防止把自己定义的函数也改了名字 */
#undef udelay
#undef mdelay

#define NS_PER_USECOND 1000
#define NS_PER_SECOND 1000000000

/* The register width is 64 bit, but only lower 32bit was used. */
#define COUNTER_FREQ ((u32)read_sysreg(CNTFRQ_EL0))

/* The max count frequency is 1GHZ, so resolution must
 * lower than 10^9, u32 is enough.
 */
#define COUNTER_RESOLUTION_NS (NS_PER_SECOND / COUNTER_FREQ)

#define US_TO_COUNT(us) ((u64)(us) * NS_PER_USECOND * COUNTER_FREQ / NS_PER_SECOND)
#define NS_TO_COUNT(ns) ((u64)(ns) * COUNTER_FREQ / NS_PER_SECOND)

#define COUNT_TO_NS(count) ((u64)(count) * NS_PER_SECOND / COUNTER_FREQ)

static inline u64 arch_counter_get_val(void)
{
	return read_sysreg(CNTPCT_EL0);
}

static inline u64 arch_timer_get_counter_val(void)
{
	return read_sysreg(CNTP_CVAL_EL0);
}

static inline u64 arch_timer_get_timer_val(void)
{
	return read_sysreg(CNTP_TVAL_EL0);
}

static inline void arch_timer_mask_irq(void)
{
	write_sysreg(read_sysreg(CNTP_CTL_EL0) | CNTP_CTL_EL0_BIT_IMASK, CNTP_CTL_EL0);
}

static inline void arch_timer_disable(void)
{
	write_sysreg(CNTP_CTL_EL0_BIT_IMASK | ~CNTP_CTL_EL0_BIT_ENABLE, CNTP_CTL_EL0);
}

static inline bool arch_timer_condition_met(void)
{
	return !!(read_sysreg(CNTP_CTL_EL0) & CNTP_CTL_EL0_BIT_ISTATUS);
}

static inline void udelay(u32 us)
{
	u64 target = arch_counter_get_val() + US_TO_COUNT(us);

	while(arch_counter_get_val() < target) {
		/* 防止编译器认为这里什么都没做，从而直接删除整个循环 */
		cpu_relax();
	}
}

static inline void mdelay(u32 ms)
{
	while (ms--) {
		udelay(1000);
	}
}

#endif /* ARCH_TIMER_H */
