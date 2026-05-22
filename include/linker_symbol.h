/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LINKER_SYMBOL_H
#define LINKER_SYMBOL_H

#ifndef __ASSEMBLY__
#include <linux/types.h>

extern u64 kimage_voffset;
extern u8 swapper_pg_dir[];

#define LDSYM_ADDR(sym)                                      \
({                                                           \
	uintptr_t __v;                                           \
	asm volatile(                                            \
		"adrp %0, " #sym "\n"                                \
		"add  %0, %0, :lo12:" #sym "\n"                      \
		: "=r"(__v) :: "memory");                            \
	__v;                                                     \
})
#define LDSYM_PTR(sym) ((void *)LDSYM_ADDR(sym))

#define LDSYM_PHYS_ADDR(sym)                               \
({                                                         \
	u64 __pa, __voff = kimage_voffset;                     \
	asm volatile(                                          \
		"adrp %0, " #sym "\n"                              \
		"add  %0, %0, :lo12:" #sym "\n"                    \
		"sub  %0, %0, %1\n"                                \
		: "=&r"(__pa)                                      \
		: "r"(__voff)                                      \
		: "memory");                                       \
		__pa;                                                  \
})

#define LDSYM_PHYS_PTR(sym) ((void *)LDSYM_ADDR(sym))

#endif /* __ASSEMBLY__ */

#endif /* LINKER_SYMBOL_H */