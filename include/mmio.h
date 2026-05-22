/* SPDX-License-Identifier: GPL-2.0 */
#ifndef MMIO_H
#define MMIO_H
#include <linux/compiler_types.h>

static inline u32 read_u32(const volatile void __iomem *addr)
{
	return *(const volatile u32 __force *)addr;
}

static inline u64 read_u64(const volatile void __iomem *addr)
{
	return *(const volatile u64 __force *)addr;
}

#endif /* MMIO_H */