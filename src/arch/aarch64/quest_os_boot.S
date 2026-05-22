// SPDX-License-Identifier: GPL-2.0
#include <asm/sysreg.h>
#include <asm/sysreg-defs.h>
#include <generated/asm-offsets.h>
#include <linkage.h>
#include <pgtable-hwdef.h>
#include <cfi_types.h>
#include <pg_table.h>
#include <asm/virt.h>
#include <assembler.h>
#include <memory.h>

.section ".idmap.text","ax"

SYM_FUNC_START(quest_os_entry)
	/*
	 * 如果cache已关闭(C==0)，mmu处于关闭状态；
	 * 否则，如果M == 0表示mmu关闭，M == 1表示mmu打开，记录到x19中
	 */
	bl	record_mmu_state
	bl	preserve_boot_args // 保存bootloader传过来的x0 .. x3到boot_args

	adrp	x1, early_init_stack	// 将early_init_stack地址加载到x1。adr只能寻址±1MB，adrp可以寻址±4GB
	mov	sp, x1
	mov	x29, xzr	// 把 frame pointer 置空，表示“这里是调用栈的根，没有上一级帧”。
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
	mrs x2, ctr_el0
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
	/* 初始化el相关pstate和sctlr_elx */
	bl	init_kernel_el			// w0=cpu_boot_mode
	mov	x20, x0

	/* 设置tcr和mair，返回打开mmu的sctlr配置(x0) */
	/*
	 * The following calls CPU setup code, see arch/arm64/mm/proc.S for
	 * details.
	 * On return, the CPU will be ready for the MMU to be turned on and
	 * the TCR will have been set.
	 */
	bl	__cpu_setup			// initialise processor
	b	__primary_switch
	SYM_FUNC_END(quest_os_entry)

SYM_CODE_START_LOCAL(record_mmu_state)
	mrs	x19, CurrentEL
	cmp	x19, #CurrentEL_EL2
	mrs	x19, sctlr_el1
	b.eq	proc_hang	// TODO: 目前不支持EL2，后续实现了EL2再改这里

	tbnz	x19, #SCTLR_ELx_EE_SHIFT, 1f	// 如果不是小端模式，则进行修正
	tst	x19, #SCTLR_ELx_C		// Z := (C == 0)    // C == 0 表示Non-cacheable
	and	x19, x19, #SCTLR_ELx_M		// isolate M bit	// M == 0 表示MMU is off
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

/*
 * Preserve the arguments passed by the bootloader in x0 .. x3
 */
SYM_CODE_START_LOCAL(preserve_boot_args)
	mov	x21, x0				// x21=FDT	// x0: bootloader传入的FDT地址

	adr_l	x0, boot_args			// record the contents of // 保存boot_args地址到x0
	stp	x21, x1, [x0]			// x0 .. x3 at kernel entry	// 保存fdt地址和boot mode到boot_args
	stp	x2, x3, [x0, #16]									// 保存x2和x3到boot_args

	cbnz	x19, 0f		// skip cache invalidation if MMU is on
	dmb	sy				// needed before dc ivac with
						// MMU off

	add	x1, x0, #0x20			// 4 x 8 bytes
	b	dcache_inval_poc		// tail call	// 函数内部有ret指令，因此函数内部会直接返回上层，不会再执行"0:"之后的代码
0:	str_l   x19, mmu_enabled_at_boot, x0
	/* 因为开发者保证不会在mmu关闭的时候去读boot_args，所以这里不需要clean cache */
	/* 另外arm v8中D-cache 的 tag 是按物理地址来的，即使有页表切换，只要“同一个物理页”在新旧页表中属性一致，cache 里的那行照样是合法的 */
	ret
SYM_CODE_END(preserve_boot_args)

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
	/* AArch64 里 没有一条指令可以直接“设置整个 PSTATE/模式” */
	/* 借用eret，一次性把 PC 和 PSTATE 一起切换到内核期望的状态 */
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
	tcr	.req	x16
	tcr2	.req	x15
	mov_q	mair, MAIR_EL1_SET
	mov_q	tcr, TCR_T0SZ(IDMAP_VA_BITS) | TCR_T1SZ(IDMAP_VA_BITS) | TCR_CACHE_FLAGS | \
		     TCR_SHARED | TCR_TG_FLAGS | TCR_KASLR_FLAGS | TCR_ASID16 | \
		     TCR_TBI0 | TCR_A1 | TCR_KASAN_SW_FLAGS | TCR_MTE_FLAGS | TCR_IPS_48BIT
	mov	tcr2, xzr

#ifdef CONFIG_ARM64_HW_AFDBM
	/*
	 * Enable hardware update of the Access Flags bit.
	 * Hardware dirty bit management is enabled later,
	 * via capabilities.
	 */
	mrs	x9, ID_AA64MMFR1_EL1
	ubfx	x9, x9, ID_AA64MMFR1_EL1_HAFDBS_SHIFT, #4
	cbz	x9, 1f
	orr	tcr, tcr, #TCR_HA		// hardware Access flag update
#ifdef CONFIG_ARM64_HAFT
	cmp	x9, ID_AA64MMFR1_EL1_HAFDBS_HAFT
	b.lt	1f
	orr	tcr2, tcr2, TCR2_EL1_HAFT
#endif /* CONFIG_ARM64_HAFT */
1:
#endif	/* CONFIG_ARM64_HW_AFDBM */
	msr	mair_el1, mair
	msr	tcr_el1, tcr

	mrs_s	x1, SYS_ID_AA64MMFR3_EL1
	ubfx	x1, x1, #ID_AA64MMFR3_EL1_S1PIE_SHIFT, #4
	cbz	x1, .Lskip_indirection

	mov_q	x0, PIE_E0_ASM
	msr	REG_PIRE0_EL1, x0
	mov_q	x0, PIE_E1_ASM
	msr	REG_PIR_EL1, x0

	orr	tcr2, tcr2, TCR2_EL1_PIE

.Lskip_indirection:

	mrs_s	x1, SYS_ID_AA64MMFR3_EL1
	ubfx	x1, x1, #ID_AA64MMFR3_EL1_TCRX_SHIFT, #4
	cbz	x1, 1f
	msr	REG_TCR2_EL1, tcr2
1:

	/*
	 * Prepare SCTLR
	 */
	mov_q	x0, INIT_SCTLR_EL1_MMU_ON
	ret					// return to head.S

	.unreq	mair
	.unreq	tcr
	.unreq	tcr2
SYM_FUNC_END(__cpu_setup)

SYM_FUNC_START_LOCAL(__primary_switch)
	adrp	x1, reserved_pg_dir
	adrp	x2, __pi_init_idmap_pg_dir
	/* 配置ttbr_el0/ttbr_el1, 打开mmu */
	bl	__enable_mmu

	adrp	x1, early_init_stack
	mov	sp, x1
	mov	x29, xzr	// 把 frame pointer 置空，表示“这里是调用栈的根，没有上一级帧”。
	mov	x0, x20				// pass the full boot status
	mov	x1, x21				// pass the FDT
	bl	early_map_kernel		// Map and relocate the kernel

	// 如果打开了CONFIG_RELOCATABLE, 那__primary_switched已经重定位成正确的地址，
	// 因此不会出错
	ldr	x8, =__primary_switched
	adr	x0, _stext		// __pa(_start)

	br	x8
SYM_FUNC_END(__primary_switch)

SYM_FUNC_START(__enable_mmu)
	mrs	x3, ID_AA64MMFR0_EL1
	ubfx	x3, x3, #ID_AA64MMFR0_EL1_TGRAN4_SHIFT, 4
	cmp     x3, #ID_AA64MMFR0_EL1_TGRAN_SUPPORTED_MIN
	b.lt    proc_hang
	cmp     x3, #ID_AA64MMFR0_EL1_TGRAN_SUPPORTED_MAX
	b.gt    proc_hang
	msr	ttbr0_el1, x2			// load TTBR0
	msr		ttbr1_el1, x1
	isb

	set_sctlr_el1	x0

	ret
SYM_FUNC_END(__enable_mmu)

.section	".init.text","ax"
/*
 * The following fragment of code is executed with the MMU enabled.
 *
 *   x0 = __pa(_stext)
 */
SYM_FUNC_START_LOCAL(__primary_switched)
	adrp	x4, _stext			// Save the offset between
	sub		x4, x4, x0			// the kernel virtual and
	str_l	x4, kimage_voffset, x5		// physical mappings

	b	switch_to_kernel_init_task
SYM_FUNC_END(__primary_switched)

SYM_FUNC_START(kernel_init_task_entry)
	adr_l	x8, exception_vector_table			// load VBAR_EL1 with virtual
	msr	vbar_el1, x8			// vector table address
	isb

	stp	x29, x30, [sp, #-16]!
	mov	x29, sp

	str_l	x21, __fdt_pointer, x5		// Save FDT pointer

	mov	x0, x20
	bl	set_cpu_boot_mode_flag

	ldp	x29, x30, [sp], #16
	bl	quest_os_main
	b proc_hang
SYM_FUNC_END(kernel_init_task_entry)

/*
 * Sets the __boot_cpu_mode flag depending on the CPU boot mode passed
 * in w0. See arch/arm64/include/asm/virt.h for more info.
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

proc_hang:
	b proc_hang
