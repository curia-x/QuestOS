// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2026 <Nino Zhang>
 *
 * Parts of the design/logic are inspired by U-Boot or Linux kernel.
 */
#include <linux/compiler_attributes.h>
#include <linux/types.h>
#include <qfw_wrapper.h>
#include <fdt_local.h>
#include <system.h>
#include <exception.h>
#include <mmu.h>
#include <print.h>
#include <memory.h>
#include <os_boot.h>

static int linux_boot_prepare(struct boot_context *ctx)
{
	int ret;

	// 读取kernel image
	ret = qfw_load_images();
	if (ret < 0) {
		printf("Failed to load images from firmware: %d\n", ret);
		return ret;
	}

	// 填充必要参数
	ret = fdt_modify_chosen("bootargs", qfw_get_bootargs());
	if (ret < 0) {
		printf("Failed to modify device tree: %d\n", ret);
		return ret;
	}

	ctx->kernel_entry = qfw_get_kernel_image_addr();

	return 0;
}

static struct boot_context linux_boot_context = {
	.name = "Linux",
	.option = BOOT_OPTION_LINUX,
	.prepare = linux_boot_prepare,
};

int linux_boot_init(void)
{
	int ret;

	ret = register_boot_context(&linux_boot_context);
	if (ret < 0) {
		printf("Failed to register Linux boot context: %d\n", ret);
		return ret;
	}

	return 0;
}
