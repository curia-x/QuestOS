// SPDX-License-Identifier: GPL-2.0-or-later
#include <linux/errno.h>
#include <linux/compiler_attributes.h>
#include <linux/types.h>
#include <print.h>
#include <memory.h>
#include <mmu.h>
#include <exception.h>
#include <os_boot.h>
#include "linux_boot.h"
#include "quest_os_boot.h"

#define MAX_OS_NUM 2
static int g_context_count;
static struct boot_context *g_boot_context[MAX_OS_NUM];

int register_boot_context(struct boot_context *ctx)
{
	if (g_context_count >= MAX_OS_NUM) {
		printf("Error: Maximum number of OS contexts reached\n");
		return -ENOSPC;
	}

	g_boot_context[g_context_count++] = ctx;
	return 0;
}

__noreturn
static void jump_to_kernel(void *kernel_image)
{
	u64 dtb = (u64)FDT_BLOB_ADDR;
	u64 entry = (u64)kernel_image;

	/* All forms of interrupts must be masked in PSTATE.DAIF (Debug, SError, IRQ and FIQ). */
	raw_write_daif(DAIF_MASK);

	/* Disable MMU */
	raw_mmu_disable();

	__asm__ __volatile__(
		"mov x0, %0\n\t"
		"mov x1, xzr\n\t"
		"mov x2, xzr\n\t"
		"mov x3, xzr\n\t"
		"br  %1\n\t"
		:
		: "r"(dtb), "r"(entry)
		: "x0", "x1", "x2", "x3", "memory"
	);

	__builtin_unreachable();
}

int boot_process(boot_option_t option)
{
	int ret;
	struct boot_context *ctx = NULL;

	for (int i = 0; i < g_context_count; i++) {
		if (g_boot_context[i]->option == option) {
			ctx = g_boot_context[i];
			break;
		}
	}

	if (!ctx) {
		printf("No boot context found for option %d\n", option);
		return -EINVAL;
	}

	if (ctx->prepare) {
		ret = ctx->prepare(ctx);
		if (ret < 0) {
			printf("Failed to prepare boot for %s: %d\n", ctx->name, ret);
			return ret;
		}
	}

	if (ctx->boot)
		return ctx->boot(ctx);
	else if (ctx->kernel_entry)
		jump_to_kernel(ctx->kernel_entry);

	printf("No boot function or kernel entry point defined for %s\n", ctx->name);
	return -EINVAL;
}

int boot_init(void)
{
	int ret;

	ret = linux_boot_init();
	if (ret < 0) {
		printf("Failed to initialize Linux boot context: %d\n", ret);
		return ret;
	}

	ret = quest_os_boot_init();
	if (ret < 0) {
		printf("Failed to initialize Quest OS boot context: %d\n", ret);
		return ret;
	}

	return 0;
}
