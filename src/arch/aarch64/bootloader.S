/* SPDX-License-Identifier: GPL-2.0 */
#include <assembler.h>
#include <asm/sysreg.h>
#include <system_reg.h>
#include <elf.h>
#include <mmu.h>
#include <pg_table.h>
#include <memory.h>

.section ".text.boot"

_start:
	mrs x0, mpidr_el1
	and x0, x0, #0xff
	cbz x0, init_flow  /* cpu0继续初始化 */
	b proc_hang  /* 其他cpu原地自旋 */

/*
 * x20: CPU boot mode(BOOT_CPU_MODE_EL1/BOOT_CPU_MODE_EL2)
 */
init_flow:
	mov x19, x0

init_el:
	// get EL info
	mrs x0, CurrentEL
	cmp	x0, #CurrentEL_EL2
	b.ne	init_el1

init_el2:
	// TODO: implement this routine
	b proc_hang

init_el1:
	mov_q x0, INIT_SCTLR_EL1_MMU_OFF
	msr sctlr_el1, x0
	isb
	mov_q	x0, INIT_PSTATE_EL1
	msr	spsr_el1, x0
	adr x0, mmu_init
	msr	elr_el1, x0
	mov	w0, #BOOT_CPU_MODE_EL1
	/* AArch64 里 没有一条指令可以直接“设置整个 PSTATE/模式” */
	/* 借用eret，一次性把 PC 和 PSTATE 一起切换到内核期望的状态 */
	eret

.macro clear_page, addr_reg, tmp_reg1, tmp_reg2
	mov \tmp_reg1, \addr_reg
	add \tmp_reg2, \tmp_reg1, #PAGE_SIZE
.Lclear_page_1\@:
	stp	xzr, xzr, [\tmp_reg1], #16
	cmp \tmp_reg1, \tmp_reg2
	b.lo .Lclear_page_1\@
.endm

mmu_init:
	/* 设置tcr_el1 */
	mov_q x0, TCR_EL1_DEFAULT_VALUE
	msr tcr_el1, x0
	isb

	/* 设置mair_el1 */
	mov x0, #DEVICE_ATTR_nGnRnE
	lsl x0, x0, #(DEVICE_ATTR_nGnRnE_IDX*MAIR_ATTR_WIDTH)
	mov x1, #NORMAL_ATTR_CACHEABLE
	lsl x1, x1, #(NORMAL_ATTR_CACHEABLE_IDX*MAIR_ATTR_WIDTH)
	orr x0, x0, x1
	msr mair_el1, x0
	isb

	mov_q	x2, BOOTLOADER_TMP_IDMAP_TABLE_ADDR
	clear_page x2, x11, x12
	add		x5, x2, #PAGE_SIZE	// pnext
	/* 将ROM整个映射为只读 */
	mov_q	x0, ROM_BASE
	mov_q	x1, (ROM_BASE + ROM_SIZE) // ROM2暂时未使用，不映射
	mov_q	x3, PG_DESC_TYPE_TABLE | PG_TABLE_ENTRY_RWX
	mov_q	x4, PG_DESC_TYPE_PAGE | PG_PAGE_ENTRY_NORMAL_CACHEABLE_RX
	bl pg_table_init

	/* 将RAM整个映射为可读写执行 */
	mov_q	x0, RAM_BASE
	mov_q	x1, (RAM_BASE + RAM_SIZE)
	mov_q	x2, BOOTLOADER_TMP_IDMAP_TABLE_ADDR
	mov_q	x3, PG_DESC_TYPE_TABLE | PG_TABLE_ENTRY_RWX
	mov_q	x4, PG_DESC_TYPE_PAGE | PG_PAGE_ENTRY_NORMAL_CACHEABLE_RWX
	bl		pg_table_init

	/* 将设备内存映射为读写 */
	mov_q	x0, MMIO_BASE
	mov_q	x1, (MMIO_BASE + MMIO_SIZE)
	mov_q	x2, BOOTLOADER_TMP_IDMAP_TABLE_ADDR
	mov_q	x3, PG_DESC_TYPE_TABLE | PG_TABLE_ENTRY_RWX
	mov_q	x4, PG_DESC_TYPE_PAGE | PG_PAGE_ENTRY_DEVICE_RW
	bl		pg_table_init

	/* We assume that mmu is off when reset. So we just need to invalid. */
	mov_q	x0, BOOTLOADER_TMP_IDMAP_TABLE_ADDR
	mov		x1, x5
	adr_l	x2, dcache_inval_poc
	blr		x2

	dsb ishst			// 或 dsb ish：确保页表更新先完成/可见
	tlbi vmalle1is		// 或更精确的 vae1is/aside1is 等
	dsb ish				// 关键：等待TLB失效在shareable域完成

	mov_q	x2, BOOTLOADER_TMP_IDMAP_TABLE_ADDR
	msr		ttbr0_el1, x2
	isb

	mov_q	x0, INIT_SCTLR_EL1_MMU_ON
	msr		sctlr_el1, x0
	isb     // 关键：刷新流水线，让后续指令用新翻译表生效

relocate_kernel_image:
	/*
	 * x20:src_start
	 * x21:src_end
	 * x22:dest_start
	 * x23:link_vma_start
	 * x24:link_vma_end
	 */
	adr		x20, __relocate_copy_start
	adr_l	x21, __relocate_copy_end
	mov		x22, #IMAGE_RELOCATE_BASE
	ldr		x23, =__relocate_copy_start
	ldr		x24, =__relocate_copy_end

relocate_image:
	mov		x0, x20
	mov		x1, x21
	bl		dcache_inval_poc

	mov		x0, x20
	mov		x1, x22
copy_loop:
	ldp		x2, x3, [x0], #16
	stp		x2, x3, [x1], #16
	cmp		x0, x21
	b.lo	copy_loop

copy_done:
	sub		x9, x22, x20			// delta = dest_start - src_start
	adr_l	x0, __pi_rela_start
	add		x0, x0, x9				// dest __pi_rela_start
	adr_l	x1, __pi_rela_end
	add		x1, x1, x9				// dest __pi_rela_end

fix_loop:
	ldp		x2, x3, [x0], #16		// (x2,x3) <- (SRC location, fixup)
	ldr		x4, [x0], #8			// x4 <- addend
	and		x3, x3, #0xffffffff
	cmp		x3, #R_AARCH64_RELATIVE
	bne		fix_next

	sub		x9, x22, x23			// delta = dest_start - link_vma_start
	add		x2, x2, x9
	add		x4, x4, x9
	str		x4, [x2]
fix_next:
	cmp		x0, x1
	b.lo	fix_loop

fix_done:
	mov		x0, x22					// dest_start
	add		x1, x24, x9				// dest_end
	bl		dcache_clean_poc

	dsb		sy
	ic		iallu
	dsb		sy
	isb

	ldr		x4, =relocate_image_done
	add		x4, x4, x9
	br		x4

relocate_image_done:
set_vbar:
	/* 设置异常向量表基地址到vbar寄存器 */
	adr_l x3, init_exception_vectors
	msr	 vbar_el1, x3
	isb

bss_clear:
	adr_l x0, __bss_start
	adr_l x1, __bss_end

1:
	stp	xzr, xzr, [x0], #16
	cmp	x0, x1
	b.lo	1b

	adr_l	x0, early_init_stack
	mov	sp, x0
	bl	qloader_main
	b	proc_hang

pg_table_init:
	ubfx	x7, x0, #PGDIR_SHIFT, #PG_TABLE_BITS	// 取出PA的[47:39]，作为页表索引
	ubfx	x8, x1, #PGDIR_SHIFT, #PG_TABLE_BITS	// 取出end PA的[47:39]，作为页表索引

	add		x9, x2, x7, lsl #3	// 计算页表项地址
	ldr		x11, [x9]	// 读出当前页表项
	cbz		x11, 1f
	ubfx	x6, x11, #12, #36	// 取出下级页表地址
	lsl		x6, x6, #PAGE_SHIFT
	b		set_pgd_entry
1:
	mov		x6, x5	// 下一层页表地址
set_pgd_entry:
	tbnz	x11, #0, 2f
	orr		x10, x5, x3
	str		x10, [x9]
	clear_page x5, x11, x12
	add		x5, x5, PAGE_SIZE	// 下一层页表地址增加一个
2:
	add		x7, x7, #1
	add		x9, x2, x7, lsl #3	// 计算页表项地址
	ldr		x11, [x9]	// 读出当前页表项
	cmp		x7, x8
	b.le	set_pgd_entry

set_pgd_entry_done:
	ubfx	x7, x0, #PUD_SHIFT, #(PG_TABLE_BITS << 1)	// 取出PA的[47:30]，作为页表索引
	ubfx	x8, x1, #PUD_SHIFT, #(PG_TABLE_BITS << 1)	// 取出end PA的[47:30]，作为页表索引
	sub		x8, x8, x7	// 计算出需要设置的页表项数量
	ubfx	x7, x7,	#0,	#PG_TABLE_BITS
	mov		x2, x6

	add		x9, x2, x7, lsl #3	// 计算页表项地址
	ldr		x11, [x9]	// 读出当前页表项
	cbz		x11, 1f
	ubfx	x6, x11, #12, #36	// 取出下级页表地址
	lsl		x6, x6, #PAGE_SHIFT
	b		set_pud_entry
1:
	mov		x6, x5	// 下一层页表地址

set_pud_entry:
	tbnz	x11, #0, 2f
	orr		x10, x5, x3
	str		x10, [x9]
	clear_page x5, x11, x12
	add		x5, x5, PAGE_SIZE	// 下一层页表地址增加一个
2:
	add		x7, x7, #1
	add		x9, x2, x7, lsl #3	// 计算页表项地址
	ldr		x11, [x9]	// 读出当前页表项
	mov		x10, x8
	sub		x8, x8, #1
	cbnz	x10, set_pud_entry	// 已经设置完了所有的页表项

set_pud_entry_done:
	ubfx	x7, x0, #PMD_SHIFT, #(PG_TABLE_BITS * 3)	// 取出PA的[47:21]，作为页表索引
	ubfx	x8, x1, #PMD_SHIFT, #(PG_TABLE_BITS * 3)	// 取出end PA的[47:21]，作为页表索引
	sub		x8, x8, x7	// 计算出需要设置的页表项数量
	ubfx	x7, x7,	#0,	#PG_TABLE_BITS
	mov		x2, x6

	add		x9, x2, x7, lsl #3	// 计算页表项地址
	ldr		x11, [x9]	// 读出当前页表项
	cbz		x11, 1f
	ubfx	x6, x11, #12, #36	// 取出下级页表地址
	lsl		x6, x6, #PAGE_SHIFT
	b		set_pmd_entry
1:
	mov		x6, x5	// 下一层页表地址

set_pmd_entry:
	tbnz	x11, #0, 2f
	orr		x10, x5, x3
	str		x10, [x9]
	clear_page x5, x11, x12
	add		x5, x5, PAGE_SIZE	// 下一层页表地址增加一个
2:
	add		x7, x7, #1
	add		x9, x2, x7, lsl #3	// 计算页表项地址
	ldr		x11, [x9]	// 读出当前页表项
	mov		x10, x8
	sub		x8, x8, #1
	cbnz	x10, set_pmd_entry	// 已经设置完了所有的页表项

set_pmd_entry_done:
	ubfx	x7, x0, #PAGE_SHIFT, #(PG_TABLE_BITS << 2)	// 取出PA的[47:12]，作为页表索引
	ubfx	x8, x1, #PAGE_SHIFT, #(PG_TABLE_BITS << 2)	// 取出end PA的[47:12]，作为页表索引
	sub		x8, x8, x7	// 计算出需要设置的页表项数量
	mov		x10, x7, lsl #PAGE_SHIFT	// 物理地址
	ubfx	x7, x7,	#0,	#PG_TABLE_BITS
	mov		x2, x6

set_pte_entry:
	add		x9, x2, x7, lsl #3	// 计算页表项地址
	orr		x10, x10, x4
	str		x10, [x9]
	add		x7, x7, #1
	add		x10, x10, #PAGE_SIZE
	mov		x11, x8
	sub		x8, x8, #1
	cbnz	x11, set_pte_entry	// 已经设置完了所有的页表项

set_pte_entry_done:
	ret

proc_hang:
	b proc_hang
