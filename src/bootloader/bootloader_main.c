// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2026 <Nino Zhang>
 *
 * Parts of the design/logic are inspired by U-Boot or Linux kernel.
 */

#include <uart.h>
#include <print.h>
#include <fdt_local.h>
#include <asm/reg.h>
#include <system.h>
#include <qfw_wrapper.h>
#include <u-boot-arm.h>
#include <os_boot.h>
#include <memory.h>

static void print_option(void)
{
	pr_notice("Please select boot option:\r\n");
	pr_notice("[L/l]: boot Linux\r\n");
	pr_notice("[Q/q]: boot Quest OS\r\n");
}

void cmd_process(void)
{
	char ch;
	int ret;

	boot_option_t option;

	pr_notice("\r\n=======Welcome to QLoader!=======\r\n");
	print_option();
	while (1) {
		ch = uart_recv();
		pr_notice("%c\r\n", ch);
		switch (ch) {
		case '\r':
		case '\n':
			continue;
		case 'L':
		case 'l':
			pr_notice("Booting Linux...\r\n");
			option = BOOT_OPTION_LINUX;
			break;
		case 'Q':
		case 'q':
			pr_notice("Booting Quest OS...\r\n");
			option = BOOT_OPTION_QUEST_OS;
			break;
		case 'H':
		case 'h':
			print_option();
			continue;
		default:
			pr_err("Invalid option.\r\n\r\n");
			print_option();
			continue;
		}

		ret = boot_process(option);
		/* boot_process should not return */
		pr_emerg("Boot %s failed, ret:%d\r\n",
				option == BOOT_OPTION_LINUX ? "Linux" : "Quest OS", ret);
		while (1)
			;
	}
}

void qloader_main(void)
{
	int ret;

	uart_init();

	/* 执行 init_printf_done 之后，printf 才能正常打印 */
	init_printf_done();
	printf_set_forbid();

	ret = fdt_init((void *)(uintptr_t)FDT_BLOB_ADDR);
	if (ret < 0) {
		pr_err("Failed to initialize FDT: %d\n", ret);
		return;
	}
	ret = qfw_init();
	if (ret < 0) {
		pr_err("Failed to initialize firmware interface: %d\n", ret);
		return;
	}

	ret = boot_init();
	if (ret < 0) {
		pr_err("Failed to initialize boot contexts: %d\n", ret);
		return;
	}
	printf_set_ready();
	print_el();

	cmd_process();
}
