// SPDX-License-Identifier: GPL-2.0
#ifndef __SMP_H__
#define __SMP_H__

#include <linux/bitmap.h>
#include <linux/types.h>
#include <asm/sysreg.h>

#define MAX_CPUS 4

extern DECLARE_BITMAP(__processor_online_mask, MAX_CPUS);

#define cpu_online_mask   __processor_online_mask

#define for_each_cpu(cpu, mask)				\
	for_each_set_bit(cpu, mask, MAX_CPUS)

#define for_each_online_cpu(cpu)   for_each_cpu((cpu), cpu_online_mask)

extern u64 __cpu_logical_map[MAX_CPUS];

int smp_init(void);
int mpidr_to_processor_id(u64 mpidr);

static inline u64 get_mpidr_el1(void)
{
	return read_sysreg_s(SYS_MPIDR_EL1);
}

static inline int get_smp_processor_id(void)
{
	return mpidr_to_processor_id(get_mpidr_el1());
}

static inline u64 cpu_logical_map(int cpu)
{
	return __cpu_logical_map[cpu];
}

static inline void set_cpu_logical_map(int cpu, u64 mpidr)
{
	__cpu_logical_map[cpu] = mpidr;
}

#endif /* __SMP_H__ */