/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2026 <Nino Zhang>
 *
 * Parts of the design/logic are inspired by U-Boot or Linux kernel.
 */
#ifndef OS_BOOT_H
#define OS_BOOT_H

typedef enum boot_option {
	BOOT_OPTION_LINUX,
	BOOT_OPTION_QUEST_OS,
} boot_option_t;

struct boot_context {
	char *name;
	boot_option_t option;
	int (*prepare)(struct boot_context *ctx);
	int (*boot)(struct boot_context *ctx);
	void *kernel_entry;
};

int boot_init(void);
int register_boot_context(struct boot_context *ctx);
int boot_process(boot_option_t option);

#endif
