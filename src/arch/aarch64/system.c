// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2026 <Nino Zhang>
 *
 * Parts of the design/logic are inspired by U-Boot or Linux kernel.
 */
#include <asm/reg.h>
#include <linux/types.h>
#include <linux/compiler_attributes.h>
#include <asm/sysreg.h>
#include <ptrace.h>
#include <mmu.h>
#include <print.h>

static const char *const EL_TABLE[] = {"EL0", "EL1", "EL2", "EL3"};
static const char *const EL2_STATE[] = {"EL2 is not implemented", "EL2 can be executed in AArch64 state only", "EL2 can be executed in either AArch64 or AArch32 state"};
static const char *const EL3_STATE[] = {"EL3 is not implemented", "EL3 can be executed in AArch64 state only", "EL3 can be executed in either AArch64 or AArch32 state"};

static bool g_in_kernel;

bool in_kernel(void)
{
	return g_in_kernel;
}

void record_enter_in_kernel(void)
{
	g_in_kernel = true;
}

static inline u64 read_currentel_raw(void)
{
	u64 v;

	__asm__ __volatile__("mrs %0, CurrentEL" : "=r"(v));

	return v;
}

static inline u64 read_aa64pfr0_el1(void)
{
	u64 v;

	__asm__ __volatile__("mrs %0, id_aa64pfr0_el1" : "=r"(v));

	return v;
}

static inline void get_el_info(u64 *cur_el,
							   u64 *el2_field,
							   u64 *el3_field)
{
	u64 cel = read_currentel_raw();
	u64 pfr = read_aa64pfr0_el1();

	*cur_el	= (cel >> 2)  & 0x3;   // CurrentEL[3:2]
	*el2_field = (pfr >> 8)  & 0xF;   // [11:8]
	*el3_field = (pfr >> 12) & 0xF;   // [15:12]
}

void print_el(void)
{
	u64 current_el;
	u64 el2_state;
	u64 el3_state;

	get_el_info(&current_el, &el2_state, &el3_state);

	printf("\r\nCurrentEL:%s\r\n", EL_TABLE[current_el]);
	printf("%s, %s\r\n", EL2_STATE[el2_state], EL3_STATE[el3_state]);
}
