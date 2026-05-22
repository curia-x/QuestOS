// SPDX-License-Identifier: GPL-2.0-only
#ifndef __BARRIER_H__
#define __BARRIER_H__

#include <linux/compiler.h>

#define __nops(n)	".rept	" #n "\nnop\n.endr\n"
#define nops(n)		asm volatile(__nops(n))

#define sev()		asm volatile("sev" : : : "memory")
#define wfe()		asm volatile("wfe" : : : "memory")
#define wfet(val)	asm volatile("msr s0_3_c1_c0_0, %0"	\
				     : : "r" (val) : "memory")
#define wfi()		asm volatile("wfi" : : : "memory")
#define wfit(val)	asm volatile("msr s0_3_c1_c0_1, %0"	\
				     : : "r" (val) : "memory")

#define isb()		asm volatile("isb" : : : "memory")
#define dmb(opt)	asm volatile("dmb " #opt : : : "memory")
#define dsb(opt)	asm volatile("dsb " #opt : : : "memory")

#ifndef nop
#define nop()	asm volatile ("nop")
#endif

/*
 * Architectures that want generic instrumentation can define __ prefixed
 * variants of all barriers.
 */

#ifdef __mb
#define mb()	do { kcsan_mb(); __mb(); } while (0)
#endif

#ifdef __rmb
#define rmb()	do { kcsan_rmb(); __rmb(); } while (0)
#endif

#ifdef __wmb
#define wmb()	do { kcsan_wmb(); __wmb(); } while (0)
#endif

#ifdef __dma_mb
#define dma_mb()	do { kcsan_mb(); __dma_mb(); } while (0)
#endif

#ifdef __dma_rmb
#define dma_rmb()	do { kcsan_rmb(); __dma_rmb(); } while (0)
#endif

#ifdef __dma_wmb
#define dma_wmb()	do { kcsan_wmb(); __dma_wmb(); } while (0)
#endif

/*
 * Force strict CPU ordering. And yes, this is required on UP too when we're
 * talking to devices.
 *
 * Fall back to compiler barriers if nothing better is provided.
 */

#ifndef mb
#define mb()	barrier()
#endif

#ifndef rmb
#define rmb()	mb()
#endif

#ifndef wmb
#define wmb()	mb()
#endif

#ifndef dma_mb
#define dma_mb()	mb()
#endif

#ifndef dma_rmb
#define dma_rmb()	rmb()
#endif

#ifndef dma_wmb
#define dma_wmb()	wmb()
#endif

#ifndef __smp_mb
#define __smp_mb()	mb()
#endif

#ifndef __smp_rmb
#define __smp_rmb()	rmb()
#endif

#ifndef __smp_wmb
#define __smp_wmb()	wmb()
#endif

#endif /* __BARRIER_H__ */