// SPDX-License-Identifier: GPL-2.0-only
#ifndef __INIT_H
#define __INIT_H

#include <linux/types.h>
#include <linux/compiler_attributes.h>
#include <linux/compiler-gcc.h>
#include <linux/compiler_types.h>

/* Built-in __init functions needn't be compiled with retpoline */
#if defined(__noretpoline) && !defined(MODULE)
#define __noinitretpoline __noretpoline
#else
#define __noinitretpoline
#endif

/* These are for everybody (although not all archs will actually
   discard it in modules) */
#define __init		__section(".init.text") __cold __latent_entropy	\
						__noinitretpoline	\
						__no_kstack_erase
#define __initdata	__section(".init.data")
#define __initconst	__section(".init.rodata")
#define __exitdata	__section(".exit.data")
#define __exit_call	__used __section(".exitcall.exit")

void __init setup_machine_fdt(phys_addr_t dt_phys);

#endif /* __INIT_H */