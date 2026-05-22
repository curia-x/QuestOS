// SPDX-License-Identifier: GPL-2.0
#include <linux/compiler_attributes.h>
#include <uart.h>
#include <print.h>
#include <exception.h>
#include <arm-gic-v3.h>
#include <arch_timer.h>
#include <irq.h>
#include <memory.h>
#include <init.h>
#include <system_reg.h>
#include <mm.h>
#include <vmalloc.h>
#include <sched.h>

#define CACHELINE_SIZE 64
/*
 * The recorded values of x0 .. x3 upon kernel entry.
 */
u64 __aligned(CACHELINE_SIZE) boot_args[4];
u64 mmu_enabled_at_boot;
phys_addr_t __fdt_pointer __initdata;
u32 __boot_cpu_mode[] = { BOOT_CPU_MODE_EL2, BOOT_CPU_MODE_EL1 };
bool arm64_use_ng_mappings;

u64 kimage_voffset;

void quest_os_main(void)
{
	int ret;

	record_enter_in_kernel();

	mm_early_init();

	setup_machine_fdt(__fdt_pointer);

	uart_init();

	init_printf_done();

	mm_post_init();

	printf("\r\n===============Welcome to Quest OS!===============\r\n");

	ret = gic_v3_init();
	if (ret < 0) {
		printf("Failed to initialize GICv3: %d\n", ret);
		goto exit;
	}

	ret = arch_timer_init();
	if (ret < 0) {
		printf("Failed to initialize Arch Timer: %d\n", ret);
		goto exit;
	}

	system_irq_enable();

	round_robin_sched_register();

	ret = sched_init(SCHED_ROUND_ROBIN);
	if (ret) {
		printf("sched init failed, %d\n", ret);
		goto exit;
	}

	process_init();

	run_scheduler();

	unreachable();

exit:
	for (;;)
		;
}
