// SPDX-License-Identifier: GPL-2.0
#ifndef __SMP_H__
#define __SMP_H__

#include <linux/bitmap.h>
#include <linux/types.h>
#include <asm/sysreg.h>
#include <iopoll.h>

#define MAX_CPUS 4

#define MPIDR_HWID_BITMASK	UL(0xff00ffffff)

#define cpu_possible(cpu)	((cpu) < MAX_CPUS)

static inline int num_possible_cpus(void)
{
	return MAX_CPUS;
}

extern DECLARE_BITMAP(__processor_online_mask, MAX_CPUS);

#define cpu_online_mask   __processor_online_mask

#define for_each_cpu(cpu, mask)				\
	for_each_set_bit(cpu, mask, MAX_CPUS)

#define for_each_online_cpu(cpu)   for_each_cpu((cpu), cpu_online_mask)

extern u64 __cpu_logical_map[MAX_CPUS];

int smp_init(void);
int cpu_init(void);
int mpidr_to_processor_id(u64 mpidr);

extern void secondary_entry();

static inline bool cpu_set_online(unsigned int cpu, bool online)
{
	if (online)
		return test_and_set_bit(cpu, cpu_online_mask);

	return test_and_clear_bit(cpu, cpu_online_mask);
}

static inline bool cpu_check_online(unsigned int cpu)
{
	return test_bit(cpu, cpu_online_mask);
}

static inline void cpu_wait_online(unsigned int cpu)
{
	while (!cpu_check_online(cpu))
		cpu_relax();
}

static inline u64 get_mpidr(void)
{
	return read_sysreg_s(SYS_MPIDR_EL1) & MPIDR_HWID_BITMASK;
}

static inline int get_smp_processor_id(void)
{
	return mpidr_to_processor_id(get_mpidr());
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