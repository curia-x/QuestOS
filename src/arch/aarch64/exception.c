// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2026 <Nino Zhang>
 *
 * Parts of the design/logic are inspired by U-Boot or Linux kernel.
 */
#include <uapi/asm-generic/errno-base.h>
#include <asm/sysreg.h>
#include <print.h>
#include <linux/types.h>
#include <ptrace.h>
#include <exception.h>
#include <exception_declare.h>

/* Interrupt handler must set it through set_handle_irq. */
void (*handle_arch_irq)(struct pt_regs *) = NULL;

int set_handle_irq(void (*handle_irq)(struct pt_regs *))
{
	if (!handle_irq)
		return -EINVAL;

	if (handle_arch_irq)
		return -EBUSY;

	handle_arch_irq = handle_irq;
	printf("Root IRQ handler: 0x%p\n", handle_irq);
	return 0;
}

static const char * const undefined_mode_handler[] = {
	"Sync Abort",
	"IRQ",
	"FIQ",
	"SError"
};

void undefined_mode(struct pt_regs *regs, int reason, u64 esr)
{
	printf("Bad mode for %s handler detected, FAR_EL1:0x%llx ESR_EL1:0x%llx, ELR_EL1:0x%llx\n",
			undefined_mode_handler[reason], read_sysreg(FAR_EL1),
			esr, read_sysreg(ELR_EL1));
}

void el1t_64_sync_handler(struct pt_regs *regs)
{
	printf("EL1t_64 Sync handler called, FAR_EL1:0x%llx ESR_EL1:0x%llx, ELR_EL1:0x%llx\n",
			read_sysreg(FAR_EL1), read_sysreg(ESR_EL1),
			read_sysreg(ELR_EL1));
}

void el1t_64_irq_handler(struct pt_regs *regs)
{
	printf("EL1t_64 IRQ handler called, FAR_EL1:0x%llx ESR_EL1:0x%llx, ELR_EL1:0x%llx\n",
			read_sysreg(FAR_EL1), read_sysreg(ESR_EL1),
			read_sysreg(ELR_EL1));
}

void el1t_64_fiq_handler(struct pt_regs *regs)
{
	printf("EL1t_64 FIQ handler called, FAR_EL1:0x%llx ESR_EL1:0x%llx, ELR_EL1:0x%llx\n",
			read_sysreg(FAR_EL1), read_sysreg(ESR_EL1),
			read_sysreg(ELR_EL1));
}

void el1t_64_error_handler(struct pt_regs *regs)
{
	printf("EL1t_64 Error handler called, FAR_EL1:0x%llx ESR_EL1:0x%llx, ELR_EL1:0x%llx\n",
			read_sysreg(FAR_EL1), read_sysreg(ESR_EL1),
			read_sysreg(ELR_EL1));
}

void el1h_64_sync_handler(struct pt_regs *regs)
{
	printf("EL1h_64 Sync handler called, FAR_EL1:0x%llx ESR_EL1:0x%llx, ELR_EL1:0x%llx\n",
			read_sysreg(FAR_EL1), read_sysreg(ESR_EL1),
			read_sysreg(ELR_EL1));
}

void el1h_64_irq_handler(struct pt_regs *regs)
{
	handle_arch_irq(regs);
}

void el1h_64_fiq_handler(struct pt_regs *regs)
{
	printf("EL1h_64 FIQ handler called, FAR_EL1:0x%llx ESR_EL1:0x%llx, ELR_EL1:0x%llx\n",
			read_sysreg(FAR_EL1), read_sysreg(ESR_EL1),
			read_sysreg(ELR_EL1));
}

void el1h_64_error_handler(struct pt_regs *regs)
{
	printf("EL1h_64 Error handler called, FAR_EL1:0x%llx ESR_EL1:0x%llx, ELR_EL1:0x%llx\n",
			read_sysreg(FAR_EL1), read_sysreg(ESR_EL1),
			read_sysreg(ELR_EL1));
}

void el0t_64_sync_handler(struct pt_regs *regs)
{
	u64 esr = read_sysreg(ESR_EL1);
	u8 esr_ec = ESR_EC_FIELD(esr);

	switch (esr_ec) {
	case ESR_EC_SVC:
		el0t_64_handle_svc(regs);
		break;
	default:
		printf("EL0t_64 sync handler called, FAR_EL1:0x%llx ESR_EL1:0x%llx, ELR_EL1:0x%llx\n",
			read_sysreg(FAR_EL1), read_sysreg(ESR_EL1),
			read_sysreg(ELR_EL1));
		break;
	}
}

void el0t_64_irq_handler(struct pt_regs *regs)
{
	handle_arch_irq(regs);
}

void el0t_64_fiq_handler(struct pt_regs *regs)
{
	printf("EL0t_64 FIQ handler called, FAR_EL1:0x%llx ESR_EL1:0x%llx, ELR_EL1:0x%llx\n",
			read_sysreg(FAR_EL1), read_sysreg(ESR_EL1),
			read_sysreg(ELR_EL1));
}

void el0t_64_error_handler(struct pt_regs *regs)
{
	printf("EL0t_64 Error handler called, FAR_EL1:0x%llx ESR_EL1:0x%llx, ELR_EL1:0x%llx\n",
			read_sysreg(FAR_EL1), read_sysreg(ESR_EL1),
			read_sysreg(ELR_EL1));
}

void el0t_32_sync_handler(struct pt_regs *regs)
{
	printf("EL0t_32 Sync handler called, FAR_EL1:0x%llx ESR_EL1:0x%llx, ELR_EL1:0x%llx\n",
			read_sysreg(FAR_EL1), read_sysreg(ESR_EL1),
			read_sysreg(ELR_EL1));
}

void el0t_32_irq_handler(struct pt_regs *regs)
{
	printf("EL0t_32 IRQ handler called, FAR_EL1:0x%llx ESR_EL1:0x%llx, ELR_EL1:0x%llx\n",
			read_sysreg(FAR_EL1), read_sysreg(ESR_EL1),
			read_sysreg(ELR_EL1));
}

void el0t_32_fiq_handler(struct pt_regs *regs)
{
	printf("EL0t_32 FIQ handler called, FAR_EL1:0x%llx ESR_EL1:0x%llx, ELR_EL1:0x%llx\n",
			read_sysreg(FAR_EL1), read_sysreg(ESR_EL1),
			read_sysreg(ELR_EL1));
}

void el0t_32_error_handler(struct pt_regs *regs)
{
	printf("EL0t_32 Error handler called, FAR_EL1:0x%llx ESR_EL1:0x%llx, ELR_EL1:0x%llx\n",
			read_sysreg(FAR_EL1), read_sysreg(ESR_EL1),
			read_sysreg(ELR_EL1));
}

static inline void vbar_el1_set(u64 vector_base)
{
	write_sysreg(vector_base, VBAR_EL1);
	__asm__ __volatile__("isb" : : : "memory");
}

int exception_init(void)
{
	vbar_el1_set((u64)exception_vector_table);
	return 0;
}
