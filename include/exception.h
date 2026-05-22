/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2026 <Nino Zhang>
 *
 * Parts of the design/logic are inspired by U-Boot or Linux kernel.
 */
#ifndef EXCEPTION_H
#define EXCEPTION_H
#include <linux/types.h>

#define ESR_EC_FIELD(esr) (((esr) >> 26) & 0x3f)

enum ESR_EC_TYPE {
	ESR_EC_UNKNOWN = 0x0,
	ESR_EC_WF,
	ESR_EC_SVE = 0x7,
	ESR_EC_ILLEGAL = 0xe,
	ESR_EC_SVC = 0x15,
};

#define DAIF_MASK (0xf << 6)
#define DAIF_OFF_F 0
#define DAIF_OFF_I 1
#define DAIF_OFF_A 2
#define DAIF_OFF_D 3

#define DAIF_BIT_F (1 << DAIF_OFF_F)
#define DAIF_BIT_I (1 << DAIF_OFF_I)
#define DAIF_BIT_A (1 << DAIF_OFF_A)
#define DAIF_BIT_D (1 << DAIF_OFF_D)

extern u64 exception_vector_table[];

int exception_init(void);

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

static inline void raw_write_daif(unsigned int daif)
{
	__asm__ __volatile__("msr DAIF, %x0\n\t" : : "r" (daif) : "memory");
}


#endif /* EXCEPTION_H */