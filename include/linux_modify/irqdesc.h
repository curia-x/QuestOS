/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_IRQDESC_H
#define _LINUX_IRQDESC_H

#include <irq.h>

/**
 * struct irq_desc - interrupt descriptor
 * @irq_data:	per irq chip data passed down to chip functions
 * @action:		the irq action chain
 * @name:		flow handler name for /proc/interrupts output
 */
struct irq_desc {
	struct irq_data		irq_data;
	struct irqaction	*action;	/* IRQ action list */
	const char		*name;
};

/*
 * Convert a HW interrupt number to a logical one using a IRQ domain,
 * and handle the result interrupt number. Return -EINVAL if
 * conversion failed.
 */
int generic_handle_domain_irq(struct irq_domain *domain, unsigned int hwirq);
int generic_handle_domain_irq_safe(struct irq_domain *domain, unsigned int hwirq);
int generic_handle_domain_nmi(struct irq_domain *domain, unsigned int hwirq);

#endif
