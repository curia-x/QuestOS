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
#include <percpu.h>
#include <smp.h>
#include <psci.h>

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

	per_cpu_init();

	cpu_init();

	pr_notice("\r\n===============Welcome to Quest OS!===============\r\n");

	ret = gic_v3_init();
	if (ret < 0) {
		pr_err("Failed to initialize GICv3: %d\n", ret);
		goto exit;
	}

	ret = arch_timer_init();
	if (ret < 0) {
		pr_err("Failed to initialize Arch Timer: %d\n", ret);
		goto exit;
	}

	local_interrupt_enable();

	round_robin_sched_register();

	ret = sched_init(SCHED_ROUND_ROBIN);
	if (ret) {
		pr_err("sched init failed, %d\n", ret);
		goto exit;
	}

	ret = kthread_init();
	if (ret) {
		pr_err("kthread init failed, err=%d\n", ret);
		goto exit;
	}

	ret = psci_init();
	if (ret) {
		pr_err("psci init failed, err=%d\n", ret);
		goto exit;
	}

	ret = smp_init();
	if (ret) {
		pr_err("smp init failed, err=%d\n", ret);
		goto exit;
	}

	user_process_init();

	pr_notice("\n=========Press any key to start user processes=========\n\n");
	uart_recv();

	user_processes_register();

	local_sched_timer_start();

	run_idle();

	unreachable();

exit:
	for (;;)
		;
}
