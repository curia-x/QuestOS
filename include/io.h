/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __IO_H__
#define __IO_H__

#include <linux/types.h>
#include <barrier.h>

/*
 * Generic IO read/write.  These perform native-endian accesses.
 */
#define __raw_writeb __raw_writeb
static __always_inline void __raw_writeb(u8 val, volatile void __iomem *addr)
{
	volatile u8 __iomem *ptr = addr;
	asm volatile("strb %w0, %1" : : "rZ" (val), "Qo" (*ptr));
}

#define __raw_writew __raw_writew
static __always_inline void __raw_writew(u16 val, volatile void __iomem *addr)
{
	volatile u16 __iomem *ptr = addr;
	asm volatile("strh %w0, %1" : : "rZ" (val), "Qo" (*ptr));
}

#define __raw_writel __raw_writel
static __always_inline void __raw_writel(u32 val, volatile void __iomem *addr)
{
	volatile u32 __iomem *ptr = addr;
	asm volatile("str %w0, %1" : : "rZ" (val), "Qo" (*ptr));
}

#define __raw_writeq __raw_writeq
static __always_inline void __raw_writeq(u64 val, volatile void __iomem *addr)
{
	volatile u64 __iomem *ptr = addr;
	asm volatile("str %x0, %1" : : "rZ" (val), "Qo" (*ptr));
}

#define __raw_readb __raw_readb
static __always_inline u8 __raw_readb(const volatile void __iomem *addr)
{
	u8 val;
	asm volatile("ldrb %w0, [%1]"
		     : "=r" (val) : "r" (addr));
	return val;
}

#define __raw_readw __raw_readw
static __always_inline u16 __raw_readw(const volatile void __iomem *addr)
{
	u16 val;

	asm volatile("ldrh %w0, [%1]"
		     : "=r" (val) : "r" (addr));
	return val;
}

#define __raw_readl __raw_readl
static __always_inline u32 __raw_readl(const volatile void __iomem *addr)
{
	u32 val;
	asm volatile("ldr %w0, [%1]"
		     : "=r" (val) : "r" (addr));
	return val;
}

#define __raw_readq __raw_readq
static __always_inline u64 __raw_readq(const volatile void __iomem *addr)
{
	u64 val;
	asm volatile("ldr %0, [%1]"
		     : "=r" (val) : "r" (addr));
	return val;
}

/* IO barriers */
#define __io_ar(v)							\
({									\
	unsigned long tmp;						\
									\
	dma_rmb();								\
									\
	/*								\
	 * Create a dummy control dependency from the IO read to any	\
	 * later instructions. This ensures that a subsequent call to	\
	 * udelay() will be ordered due to the ISB in get_cycles().	\
	 */								\
	asm volatile("eor	%0, %1, %1\n"				\
		     "cbnz	%0, ."					\
		     : "=r" (tmp) : "r" ((unsigned long)(v))		\
		     : "memory");					\
})

#define __io_bw()		dma_wmb()
#define __io_br(v)
#define __io_aw(v)

/* arm64-specific, don't use in portable drivers */
#define __iormb(v)		__io_ar(v)
#define __iowmb()		__io_bw()
#define __iomb()		dma_mb()


#if 1
#define __arch_getl(a)			(*(volatile unsigned int *)(a))
#define __arch_putl(v,a)		(*(volatile unsigned int *)(a) = (v))

#define readl(c)	({ unsigned int  __v = __arch_getl(c); __iormb(__v); __v; })
#define writel(v,c)	({ unsigned int  __v = v; __iowmb(); __arch_putl(__v,c);})
#else
static inline void writel(unsigned int value, volatile void __iomem *addr)
{
	*(volatile unsigned int *)addr = value;
}

static inline unsigned int readl(volatile void __iomem *addr)
{
	return *(volatile unsigned int *)addr;
}
#endif


#endif	/* __IO_H__ */
