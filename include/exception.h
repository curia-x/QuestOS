// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026 <Nino Zhang>
 *
 * Parts of the design/logic are inspired by U-Boot or Linux kernel.
 */
#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include <linux/types.h>
#include <arch_exception.h>

#define ESR_EC_FIELD(esr) (((esr) >> 26) & 0x3f)

enum ESR_EC_TYPE {
	ESR_EC_UNKNOWN = 0x0,
	ESR_EC_WF,
	ESR_EC_SVE = 0x7,
	ESR_EC_ILLEGAL = 0xe,
	ESR_EC_SVC = 0x15,
};

extern u64 exception_vector_table[];

int exception_init(void);

#endif /* __EXCEPTION_H__ */