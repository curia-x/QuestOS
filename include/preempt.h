// SPDX-License-Identifier: GPL-2.0
#ifndef __PREEMPT_H__
#define __PREEMPT_H__

#include <percpu.h>
DECLARE_PER_CPU(long, preempt_count);

static inline void preempt_disable(void)
{
	long count = per_cpu_read(preempt_count, get_smp_processor_id());
	per_cpu_write(preempt_count, get_smp_processor_id(), count + 1);
}

static inline void preempt_enable(void)
{
	long count = per_cpu_read(preempt_count, get_smp_processor_id());
	per_cpu_write(preempt_count, get_smp_processor_id(), count - 1);
}

#endif /* __PREEMPT_H__ */