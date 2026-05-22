// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2026 <Nino Zhang>
 * Parts of the design/logic are inspired by U-Boot.
 */

#include <early_ioremap.h>
#include <system.h>
#include "asm/pl_uart.h"
#include "asm/gpio.h"

struct pl011_port g_uart_port;

void uart_send(char c)
{
	/* wait for transmit FIFO to have an available slot*/
	while (pl011_read(&g_uart_port, PL011_REG(fr)) & (1<<5))
		;

	pl011_write(&g_uart_port, PL011_REG(dr), c);
}

char uart_recv(void)
{
	/* wait for receive FIFO to have data to read */
	while (pl011_read(&g_uart_port, PL011_REG(fr)) & (1<<4))
		;

	return(pl011_read(&g_uart_port, PL011_REG(dr)) & 0xFF);
}

void uart_send_string(char *str)
{
	int i;

	for (i = 0; str[i] != '\0'; i++)
		uart_send((char) str[i]);
}

void uart_init(void)
{
	if (in_kernel())
		g_uart_port.base = (void *)early_ioremap(PL011_BASE, sizeof(struct pl011_regs));
	else
		g_uart_port.base = (void *)PL011_BASE;

	/* disable UART until configuration is done */
	pl011_write(&g_uart_port, PL011_REG(cr), 0);

	/* baud rate divisor, integer part */
	pl011_write(&g_uart_port, PL011_REG(ibrd), 26);
	/* baud rate divisor, fractional part */
	pl011_write(&g_uart_port, PL011_REG(fbrd), 3);

	/* enable FIFOs and 8 bits frames */
	pl011_write(&g_uart_port, PL011_REG(lcr_h), (1<<4) | (3<<5));

	/* mask interupts */
	pl011_write(&g_uart_port, PL011_REG(imsc), 0);
	/* enable UART, receive and transmit */
	pl011_write(&g_uart_port, PL011_REG(cr), 1 | (1<<8) | (1<<9));
}

int putchar(int c)
{
	if (c == '\n')
		uart_send('\r');
	uart_send(c);

	return 0;
}
