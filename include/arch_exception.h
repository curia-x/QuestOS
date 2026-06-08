// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026 <Nino Zhang>
 *
 * Parts of the design/logic are inspired by U-Boot or Linux kernel.
 */
#ifndef __ARCH_EXCEPTION_H__
#define __ARCH_EXCEPTION_H__

#define DAIF_MASK (0xf << 6)
#define DAIF_OFF_F 0
#define DAIF_OFF_I 1
#define DAIF_OFF_A 2
#define DAIF_OFF_D 3

#define DAIF_BIT_F (1 << DAIF_OFF_F)
#define DAIF_BIT_I (1 << DAIF_OFF_I)
#define DAIF_BIT_A (1 << DAIF_OFF_A)
#define DAIF_BIT_D (1 << DAIF_OFF_D)

#define daif_set(flags)							\
	do {								\
		__asm__ __volatile__(					\
			"msr daifset, #%c0\n\t"			\
			: : "i" (flags) : "memory");		\
	} while (0)

#define daif_clr(flags)							\
	do {								\
		__asm__ __volatile__(					\
			"msr daifclr, #%c0\n\t"			\
			: : "i" (flags) : "memory");		\
	} while (0)

static inline void raw_write_daif(unsigned long daif)
{
	__asm__ __volatile__("msr daif, %x0\n" : : "r"(daif) : "memory");
}

static inline unsigned long raw_read_daif(void)
{
	unsigned long daif;

	__asm__ __volatile__("mrs %x0, daif\n" : "=r"(daif) :: "memory");

	return daif;
}

static inline void arch_local_irq_disable(void)
{
	daif_set(DAIF_BIT_I);
}

static inline void arch_local_irq_enable(void)
{
    daif_clr(DAIF_BIT_I);
}

static inline unsigned long arch_local_irq_save(void)
{
	unsigned long flags;

	flags = raw_read_daif();
	arch_local_irq_disable();

	return flags;
}

static inline void arch_local_irq_restore(unsigned long flags)
{
    raw_write_daif(flags);
}

static inline void arch_local_interrupt_enable(void)
{
    daif_clr(DAIF_BIT_I | DAIF_BIT_F);
}

static inline void arch_local_interrupt_disable(void)
{
    daif_set(DAIF_BIT_I | DAIF_BIT_F);
}

#endif /* __ARCH_EXCEPTION_H__ */