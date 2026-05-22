// SPDX-License-Identifier: GPL-2.0
#include <asm-generic/errno.h>
#include <irqdomain.h>
#include <irqreturn.h>
#include <print.h>
#include <irq.h>
#include <irqdesc.h>
#include <interrupt.h>

static struct irqaction g_irq_actions[IRQ_NUMS];
static struct irq_desc g_irq_descs[IRQ_NUMS];

static struct irq_chip *g_irq_chip;

void enable_irq(unsigned int hwirq)
{
	g_irq_chip->irq_unmask(&g_irq_descs[hwirq].irq_data);
}

void disable_irq(unsigned int hwirq)
{
	g_irq_chip->irq_mask(&g_irq_descs[hwirq].irq_data);
}

int request_irq(unsigned int hwirq, irq_handler_t handler, unsigned long flags,
	    const char *name, void *dev)
{
	struct irqaction *action = &g_irq_actions[hwirq];
	struct irq_desc *desc = &g_irq_descs[hwirq];

	if ((hwirq >= IRQ_NUMS) || (hwirq < 0)) {
		printf("Irq num error:%d\n", hwirq);
		return -EINVAL;
	}

	if (desc->action) {
		printf("Hwirq:%d, has been requested by %s\n", hwirq, desc->action->name);
		return -EEXIST;
	}

	action->irq = hwirq;
	action->name = name;
	action->handler = handler;
	action->flags = flags;
	action->dev_id = dev;
	desc->action = &g_irq_actions[hwirq];

	desc->irq_data.chip = g_irq_chip;
	desc->irq_data.hwirq = hwirq;

	enable_irq(hwirq);
	return 0;
}

static irqreturn_t irq_default_handler(int hwirq, void *dev)
{
	(void)dev;
	printf("%s:hwirq:%d\n", hwirq);
	return IRQ_NONE;
}

static void handle_fasteoi_irq(struct irq_desc *desc)
{
	irqreturn_t irqreturn;
	unsigned int hwirq = desc->irq_data.hwirq;

	if (desc->action)
		irqreturn = desc->action->handler(hwirq, desc->action->dev_id);
	else
		irqreturn = irq_default_handler(hwirq, NULL);

	if (irqreturn != IRQ_HANDLED)
		printf("HWIRQ:%d, not be properly handled\n", hwirq);
}

static int handle_irq_desc(struct irq_desc *desc)
{
	handle_fasteoi_irq(desc);

	return 0;
}

int generic_handle_domain_irq(struct irq_domain *domain, unsigned int hwirq)
{
	(void)domain;

	return handle_irq_desc(&g_irq_descs[hwirq]);
}

int irq_chip_register(struct irq_chip *chip)
{
	if (!chip) {
		printf("Param error, chip is NULL.\n");
		return -EINVAL;
	}

	if (g_irq_chip) {
		printf("Other chip is exist.\n");
		return -EEXIST;
	}

	g_irq_chip = chip;

	return 0;
}
