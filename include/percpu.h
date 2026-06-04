// SPDX-License-Identifier: GPL-2.0
#ifndef __PER_CPU_H__
#define __PER_CPU_H__

#include <linux/types.h>
#include <smp.h>

extern char __percpu_start[];
extern char __percpu_end[];

extern u64 per_cpu_base[MAX_CPUS];

#define __per_cpu __attribute__((section(".data..percpu")))

#define DEFINE_PER_CPU(type, name) \
	type __per_cpu name

#define DECLARE_PER_CPU(type, name) \
	extern type __per_cpu name

#define per_cpu_ptr(ptr, cpu) \
	((typeof(*(ptr)) *)((u64)ptr - (u64)__percpu_start + per_cpu_base[cpu]))

#define per_cpu(name, cpu) \
	(*per_cpu_ptr(&name, cpu))

#define this_cpu_ptr(ptr) \
	per_cpu_ptr(ptr, get_smp_processor_id())

#define this_cpu_read(name) \
	per_cpu(name, get_smp_processor_id())

#define this_cpu_write(name, val) \
	per_cpu(name, get_smp_processor_id()) = (val)

#define per_cpu_read(name, cpu) \
	per_cpu(name, cpu)

#define per_cpu_write(name, cpu, val) \
	per_cpu(name, cpu) = (val)

int per_cpu_init(void);

#endif /* __PER_CPU_H__ */
