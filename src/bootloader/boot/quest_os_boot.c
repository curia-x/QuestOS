// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026 <Nino Zhang>
 *
 * Parts of the design/logic are inspired by U-Boot or Linux kernel.
 */

#include <print.h>
#include <os_boot.h>

extern void quest_os_entry(void);

static struct boot_context quest_os_boot_context = {
	.name = "Quest OS",
	.option = BOOT_OPTION_QUEST_OS,
	.kernel_entry = quest_os_entry,
};

int quest_os_boot_init(void)
{
	int ret;

	ret = register_boot_context(&quest_os_boot_context);
	if (ret < 0) {
		printf("Failed to register Quest OS boot context: %d\n", ret);
		return ret;
	}

	return 0;
}
