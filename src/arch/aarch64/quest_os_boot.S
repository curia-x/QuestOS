// SPDX-License-Identifier: GPL-2.0
#include <asm/sysreg.h>
#include <asm/sysreg-defs.h>
#include <asm/generated/asm-offsets.h>
#include <linkage.h>
#include <pgtable-hwdef.h>
#include <cfi_types.h>
#include <pg_table.h>
#include <asm/virt.h>
#include <assembler.h>
#include <memory.h>

.section ".idmap.text","ax"

SYM_FUNC_START(quest_os_entry)
	mov	x21, x0	/* preserve FDT pointer */
	bl	record_mmu_state

	adrp	x1, early_init_stack
	mov	sp, x1
	mov	x29, xzr
	adrp	x0, __pi_init_idmap_pg_dir
	bl	mmu_init_idmap_table
	mov	x1, x0
	/*
	 * If the page tables have been populated with non-cacheable
	 * accesses (MMU disabled), invalidate those tables again to
	 * remove any speculatively loaded cache lines.
	 */
	cbnz	x19, 0f
	dmb     sy
	adrp    x0, __pi_init_idmap_pg_dir
	adr_l	x2, dcache_inval_poc
	blr	x2
	b	1f

	/*
	 * If we entered with the MMU and caches on, clean the ID mapped part
	 * of the primary boot code to the PoC so we can safely execute it with
	 * the MMU off.
	 */
0:	adrp	x0, __idmap_text_start
	adr_l	x1, __idmap_text_end
	adr_l	x2, dcache_clean_poc
	blr	x2

1:	mov	x0, x19
	/* Initialize EL-related pstate and sctlr_elx. */
	bl	init_kernel_el
	/* w0=cpu_boot_mode */
	mov	x20, x0

	/* Set TCR and MAIR, return the SCTLR configuration (x0) with MMU enabled. */
	bl	__cpu_setup
	b	__primary_switch
	SYM_FUNC_END(quest_os_entry)

SYM_CODE_START_LOCAL(record_mmu_state)
	mrs	x19, CurrentEL
	cmp	x19, #CurrentEL_EL2
	mrs	x19, sctlr_el1
	b.eq	proc_hang	// TODO: 目前不支持EL2，后续实现了EL2再改这里

	tbnz	x19, #SCTLR_ELx_EE_SHIFT, 1f	// 如果不是小端模式，则进行修正
	tst	x19, #SCTLR_ELx_C		// Z := (C == 0)
	and	x19, x19, #SCTLR_ELx_M		// isolate M bit
	csel	x19, xzr, x19, eq		// clear x19 if Z
	ret

	/*
	 * Set the correct endianness early so all memory accesses issued
	 * before init_kernel_el() occur in the correct byte order. Note that
	 * this means the MMU must be disabled, or the active ID map will end
	 * up getting interpreted with the wrong byte order.
	 */
1:	eor	x19, x19, #SCTLR_ELx_EE
	bic	x19, x19, #SCTLR_ELx_M
	msr	sctlr_el1, x19
	isb
	mov	x19, xzr
	ret
SYM_CODE_END(record_mmu_state)

SYM_FUNC_START(init_kernel_el)
	mrs	x1, CurrentEL
	cmp	x1, #CurrentEL_EL2
	b.eq	proc_hang

SYM_INNER_LABEL(init_el1, SYM_L_LOCAL)
	mov_q	x0, INIT_SCTLR_EL1_MMU_OFF
	msr	sctlr_el1, x0
	isb
	mov_q	x0, INIT_PSTATE_EL1
	msr	spsr_el1, x0
	msr	elr_el1, lr
	mov	w0, #BOOT_CPU_MODE_EL1
	eret
SYM_FUNC_END(init_kernel_el)

#define MAIR_EL1_SET							\
	(MAIR_ATTRIDX(MAIR_ATTR_DEVICE_nGnRnE, MT_DEVICE_nGnRnE) |	\
	 MAIR_ATTRIDX(MAIR_ATTR_DEVICE_nGnRE, MT_DEVICE_nGnRE) |	\
	 MAIR_ATTRIDX(MAIR_ATTR_NORMAL_NC, MT_NORMAL_NC) |		\
	 MAIR_ATTRIDX(MAIR_ATTR_NORMAL, MT_NORMAL) |			\
	 MAIR_ATTRIDX(MAIR_ATTR_NORMAL, MT_NORMAL_TAGGED))

/* PTWs cacheable, inner/outer WBWA */
#define TCR_CACHE_FLAGS	TCR_IRGN_WBWA | TCR_ORGN_WBWA

/* CONFIG_ARM64_4K_PAGES */
#define TCR_TG_FLAGS	TCR_TG0_4K | TCR_TG1_4K

#ifdef CONFIG_RANDOMIZE_BASE
#define TCR_KASLR_FLAGS	TCR_NFD1
#else
#define TCR_KASLR_FLAGS	0
#endif

#ifdef CONFIG_KASAN_SW_TAGS
#define TCR_KASAN_SW_FLAGS TCR_TBI1 | TCR_TBID1
#else
#define TCR_KASAN_SW_FLAGS 0
#endif

#ifdef CONFIG_KASAN_HW_TAGS
#define TCR_MTE_FLAGS TCR_TCMA1 | TCR_TBI1 | TCR_TBID1
#elif defined(CONFIG_ARM64_MTE)
/*
 * The mte_zero_clear_page_tags() implementation uses DC GZVA, which relies on
 * TBI being enabled at EL1.
 */
#define TCR_MTE_FLAGS TCR_TBI1 | TCR_TBID1
#else
#define TCR_MTE_FLAGS 0
#endif

/*
 *	__cpu_setup
 *
 *	Initialise the processor for turning the MMU on.
 *
 * Output:
 *	Return in x0 the value of the SCTLR_EL1 register.
 */
SYM_FUNC_START(__cpu_setup)
	tlbi	vmalle1				// Invalidate local TLB
	dsb	nsh

	msr	cpacr_el1, xzr			// Reset cpacr_el1
	mov	x1, MDSCR_EL1_TDCC		// Reset mdscr_el1 and disable
	msr	mdscr_el1, x1			// access to the DCC from EL0
	reset_pmuserenr_el0 x1			// Disable PMU access from EL0
	reset_amuserenr_el0 x1			// Disable AMU access from EL0

	/*
	 * Default values for VMSA control registers. These will be adjusted
	 * below depending on detected CPU features.
	 */
	mair	.req	x17
	tcr		.req	x16
	mov_q	mair, MAIR_EL1_SET
	mov_q	tcr, TCR_T0SZ(IDMAP_VA_BITS) | TCR_T1SZ(IDMAP_VA_BITS) | TCR_CACHE_FLAGS | \
		     TCR_SHARED | TCR_TG_FLAGS | TCR_KASLR_FLAGS | TCR_ASID16 | \
		     TCR_TBI0 | TCR_A1 | TCR_KASAN_SW_FLAGS | TCR_MTE_FLAGS | TCR_IPS_48BIT

	/*
	 * Enable hardware update of the Access Flags bit.
	 * Hardware dirty bit management is enabled later,
	 * via capabilities.
	 */
	mrs	x9, ID_AA64MMFR1_EL1
	ubfx	x9, x9, ID_AA64MMFR1_EL1_HAFDBS_SHIFT, #4
	cbz	x9, 1f
	orr	tcr, tcr, #TCR_HA		// hardware Access flag update
1:
	msr	mair_el1, mair
	msr	tcr_el1, tcr

	/*
	 * Prepare SCTLR
	 */
	mov_q	x0, INIT_SCTLR_EL1_MMU_ON
	ret

	.unreq	mair
	.unreq	tcr
SYM_FUNC_END(__cpu_setup)

SYM_FUNC_START_LOCAL(__primary_switch)
	adrp	x1, reserved_pg_dir
	adrp	x2, __pi_init_idmap_pg_dir
	/* Configure ttbr_el0/ttbr_el1 and enable MMU. */
	bl		__enable_mmu

	adrp	x1, early_init_stack
	mov		sp, x1
	mov		x0, x20	/* x20: cpu_boot_mode. */
	mov		x1, x21	/* x21: fdt */
	bl		early_map_kernel		// Map and relocate the kernel

	/* Jump to the final kernel mapping. */
	ldr		x8, =__primary_switched
	adr		x0, _stext		// __pa(_stext)

	br		x8
SYM_FUNC_END(__primary_switch)

SYM_FUNC_START(__enable_mmu)
	/* We just support 4KB granule size now. */
	mrs	x3, ID_AA64MMFR0_EL1
	ubfx	x3, x3, #ID_AA64MMFR0_EL1_TGRAN4_SHIFT, 4
	cmp     x3, #ID_AA64MMFR0_EL1_TGRAN_SUPPORTED_MIN
	b.lt    proc_hang
	cmp     x3, #ID_AA64MMFR0_EL1_TGRAN_SUPPORTED_MAX
	b.gt    proc_hang

	/* Load page table. */
	msr		ttbr0_el1, x2
	msr		ttbr1_el1, x1
	isb

	set_sctlr_el1	x0

	ret
SYM_FUNC_END(__enable_mmu)

.section	".init.text","ax"

/*
 * x0: init process
 * x1: C entry
 */
SYM_FUNC_START_LOCAL(load_init_task)
	/* current = x0 */
	msr sp_el0, x0

	/* process->pc = c_entry */
	str x1, [x0, #PROCESS_PC_OFFS]
	mov x30, x1

	/*
	 * x1 = process->kernel_stack
	 * x2 = process->kernel_stack_size
	 */
	ldp x1, x2, [x0, #PROCESS_KERNEL_STACK_OFFS]

	/*
	 * process->sp = (u64)process->kernel_stack +
	 *				process->kernel_stack_size - sizeof(struct user_pt_regs);
	 * process->sp &= ~0xfUL;
	 */
	add x1, x1, x2
	sub x1, x1, #USER_PT_REGS_SIZE
	bic x1, x1, #0xf
	mov sp, x1
	str x1, [x0, #PROCESS_SP_OFFS]

	adr_l	x8, exception_vector_table
	msr	vbar_el1, x8
	isb

	ret
SYM_FUNC_END(load_init_task)
/*
 * The following fragment of code is executed with the MMU enabled.
 *
 *   x0 = __pa(_stext)
 */
SYM_FUNC_START_LOCAL(__primary_switched)
	mov x29, xzr
	/* Save the offset between the kernel virtual and physical mappings. */
	adrp	x4, _stext
	sub		x4, x4, x0
	str_l	x4, kimage_voffset, x5

	str_l	x21, __fdt_pointer, x5		// Save FDT pointer

	mov	x0, x20
	bl	set_cpu_boot_mode_flag

	/* Load init process and jump to C routine. */
	ldr x0, =g_kernel_init_process
	ldr x1, =quest_os_main
	b load_init_task
SYM_FUNC_END(__primary_switched)

/*
 * Sets the __boot_cpu_mode flag depending on the CPU boot mode passed
 * in w0.
 */
SYM_FUNC_START_LOCAL(set_cpu_boot_mode_flag)
	adr_l	x1, __boot_cpu_mode
	cmp	w0, #BOOT_CPU_MODE_EL2
	b.ne	1f
	add	x1, x1, #4
1:	str	w0, [x1]			// Save CPU boot mode
	ret
SYM_FUNC_END(set_cpu_boot_mode_flag)

/*
 * void idmap_cpu_replace_ttbr1(phys_addr_t ttbr1)
 *
 * This is the low-level counterpart to cpu_replace_ttbr1, and should not be
 * called by anything else. It can only be executed from a TTBR0 mapping.
 */
SYM_TYPED_FUNC_START(idmap_cpu_replace_ttbr1)
	adrp	x1, reserved_pg_dir
	msr	ttbr1_el1, x1
	isb
	tlbi	vmalle1
	dsb	nsh
	isb

	msr	ttbr1_el1, x0
	isb

	ret
SYM_FUNC_END(idmap_cpu_replace_ttbr1)

.section	".init.text","ax"
SYM_FUNC_START_LOCAL(__secondary_switched)
	mrs x0, mpidr_el1
	bic	x0, x0, #(0xff << 24)
	bl mpidr_to_processor_id

	cmp w0, #0
	b.lt proc_hang

	ldr x1, =per_cpu_base
	ldr x0, [x1, w0, uxtw #3]	// per_cpu_base[cpu]

	ldr	x1, =idle_process
	ldr x2, =__percpu_start
	sub x1, x1, x2
	add	x0, x0, x1
	ldr x0, [x0] // idle process
	ldr x1, =cpuhp_init_to_online
	b load_init_task
SYM_FUNC_END(__secondary_switched)

.section ".idmap.text","ax"
SYM_FUNC_START(secondary_entry)
	mov		x0, xzr
	bl		init_kernel_el			// w0=cpu_boot_mode

	bl		__cpu_setup

	adrp	x1, swapper_pg_dir
	adrp	x2, __pi_init_idmap_pg_dir
	/* 配置ttbr_el0/ttbr_el1, 打开mmu */
	bl		__enable_mmu

	adrp	x1, early_init_stack
	mov		sp, x1
	mov		x29, xzr	// 把 frame pointer 置空，表示“这里是调用栈的根，没有上一级帧”。

	ldr		x8, =__secondary_switched

	br		x8
SYM_FUNC_END(secondary_entry)

proc_hang:
	b proc_hang
