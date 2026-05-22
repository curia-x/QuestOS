// SPDX-License-Identifier: GPL-2.0-only
#ifndef	__PL_UART_H__
#define	__PL_UART_H__

#include <linux/stddef.h>
#include "base.h"
#include "io.h"

#define PL011_BASE       (0x9000000UL)

struct pl011_regs {
	u32 dr;		/* 0x000 */
	union {
		u32 rsr;	/* 0x004, read */
		u32 ecr;	/* 0x004, write */
	};
	u32 reserved0[4];	/* 0x008 - 0x014 */
	u32 fr;			/* 0x018 */
	u32 reserved1;		/* 0x01c */
	u32 ilpr;		/* 0x020 */
	u32 ibrd;		/* 0x024 */
	u32 fbrd;		/* 0x028 */
	u32 lcr_h;		/* 0x02c */
	u32 cr;			/* 0x030 */
	u32 ifls;		/* 0x034 */
	u32 imsc;		/* 0x038 */
	u32 ris;		/* 0x03c */
	u32 mis;		/* 0x040 */
	u32 icr;		/* 0x044 */
	u32 dmacr;		/* 0x048 */
};

struct pl011_port {
	void __iomem *base;
};

#define PL011_REG(_field)	offsetof(struct pl011_regs, _field)

static inline u32 pl011_read(struct pl011_port *port, unsigned int reg)
{
	return readl(port->base + reg);
}

static inline void pl011_write(struct pl011_port *port, unsigned int reg, u32 val)
{
	writel(val, port->base + reg);
}

#endif  /*__PL_UART_H__ */
