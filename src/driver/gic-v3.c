// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2026 <Nino Zhang>
 *
 * Parts of the design/logic are inspired by U-Boot or Linux kernel.
 */

#include <uapi/asm-generic/errno-base.h>
#include <linux/types.h>
#include <linux/sizes.h>
#include <vdso/time64.h>
#include <linux/wordpart.h>
#include <asm/cputype.h>
#include <io.h>
#include <arm-gic.h>
#include <arm-gic-v3.h>
#include <irqdesc.h>
#include <iopoll.h>
#include <early_ioremap.h>
#include <print.h>
#include <mmio.h>
#include <system.h>
#include <irq.h>

#define FLAGS_WORKAROUND_INSECURE		(1ULL << 3)

#define get_smp_processor_id() 0

/* Our default, arbitrary priority value. Linux only uses one anyway. */
#define DEFAULT_PMR_VALUE	0xf0

struct rdist {
		void __iomem	*rd_base;
		struct page	*pend_page;
		phys_addr_t	phys_base;
		u64             flags;
		void		*vpe_l1_base;
} __percpu;

struct my_rdists {
	struct rdist	*rdist;
	phys_addr_t		prop_table_pa;
	void			*prop_table_va;
	u64			flags;
	u32			gicd_typer;
	u32			gicd_typer2;
	int                     cpuhp_memreserve_state;
	bool			has_vlpis;
	bool			has_rvpeid;
	bool			has_direct_lpi;
	bool			has_vpend_valid_dirty;
};

struct redist_region {
	void __iomem		*redist_base;
	phys_addr_t		phys_base;
	bool			single_redist;
};

struct gic_chip_data {
	struct fwnode_handle	*fwnode;
	phys_addr_t		dist_phys_base;
	void __iomem		*dist_base;
	struct redist_region	*redist_regions;
	struct my_rdists		rdists;
	struct irq_domain	*domain;
	u64			redist_stride;
	u32			nr_redist_regions;
	u64			flags;
	bool			has_rss;
	unsigned int		ppi_nr;
	struct partition_desc	**ppi_descs;
};

static struct gic_chip_data gic_data;

static struct rdist g_rdist;

static bool supports_pseudo_nmis;

#define GIC_ID_NR	(1U << GICD_TYPER_ID_BITS(gic_data.rdists.gicd_typer))
#define GIC_LINE_NR	min(GICD_TYPER_SPIS(gic_data.rdists.gicd_typer), 1020U)
#define GIC_ESPI_NR	GICD_TYPER_ESPIS(gic_data.rdists.gicd_typer)

static bool nmi_support_forbidden;

static bool cpus_have_security_disabled;
static bool cpus_have_group0;

static bool g_cpu_interface_hasrss;

/*
 * There are 16 SGIs, though we only actually use 8 in Linux. The other 8 SGIs
 * are potentially stolen by the secure side. Some code, especially code dealing
 * with hwirq IDs, is simplified by accounting for all 16.
 */
#define SGI_NR		16

#define MPIDR_RS(mpidr)			(((mpidr) & 0xF0UL) >> 4)
#define gic_data_rdist()		(gic_data.rdists.rdist)
#define gic_data_rdist_rd_base()	(gic_data_rdist()->rd_base)
#define gic_data_rdist_sgi_base()	(gic_data_rdist_rd_base() + SZ_64K)

static struct redist_region g_rdist_regs[REDISTRIBUTOR_REGIONS];

static u8 dist_prio_irq = GICV3_PRIO_IRQ;
static u8 dist_prio_nmi = GICV3_PRIO_NMI;

static bool gic_irqnr_is_special(u32 irqnr)
{
	return irqnr >= 1020 && irqnr <= 1023;
}

static void __gic_handle_irq(u32 irqnr, struct pt_regs *regs)
{
	if (gic_irqnr_is_special(irqnr))
		return;

	if (generic_handle_domain_irq(gic_data.domain, irqnr))
		printf("Unexpected interrupt (irqnr %u)\n", irqnr);

	write_gicreg(irqnr, ICC_EOIR1_EL1);
	isb();
}

static void __gic_handle_irq_from_irqson(struct pt_regs *regs)
{
	u32 irqnr;

	irqnr = gic_read_iar_common();
	__gic_handle_irq(irqnr, regs);
}

static void gic_v3_handle_irq(struct pt_regs *regs)
{
	__gic_handle_irq_from_irqson(regs);
}

static int gic_validate_dist_version(void *dist_base)
{
	u32 reg = read_u32(dist_base + GICD_PIDR2) & GIC_PIDR2_ARCH_MASK;

	if (reg != GIC_PIDR2_ARCH_GICv3 && reg != GIC_PIDR2_ARCH_GICv4)
		return -ENODEV;

	return 0;
}

static int __gic_update_rdist_properties(struct redist_region *region,
					 void __iomem *ptr)
{
	u64 typer = read_u64(ptr + GICR_TYPER);
	u32 ctlr = read_u32(ptr + GICR_CTLR);

	/* Boot-time cleanup */
	if ((typer & GICR_TYPER_VLPIS) && (typer & GICR_TYPER_RVPEID)) {
		u64 val;

		/* Deactivate any present vPE */
		val = read_u64(ptr + SZ_128K + GICR_VPENDBASER);
		if (val & GICR_VPENDBASER_Valid)
			__raw_writeq(GICR_VPENDBASER_PendingLast,
					      ptr + SZ_128K + GICR_VPENDBASER);

		/* Mark the VPE table as invalid */
		val = read_u64(ptr + SZ_128K + GICR_VPROPBASER);
		val &= ~GICR_VPROPBASER_4_1_VALID;
		__raw_writeq(val, ptr + SZ_128K + GICR_VPROPBASER);
	}

	gic_data.rdists.has_vlpis &= !!(typer & GICR_TYPER_VLPIS);

	/*
	 * TYPER.RVPEID implies some form of DirectLPI, no matter what the
	 * doc says... :-/ And CTLR.IR implies another subset of DirectLPI
	 * that the ITS driver can make use of for LPIs (and not VLPIs).
	 *
	 * These are 3 different ways to express the same thing, depending
	 * on the revision of the architecture and its relaxations over
	 * time. Just group them under the 'direct_lpi' banner.
	 */
	gic_data.rdists.has_rvpeid &= !!(typer & GICR_TYPER_RVPEID);
	gic_data.rdists.has_direct_lpi &= (!!(typer & GICR_TYPER_DirectLPIS) |
					   !!(ctlr & GICR_CTLR_IR) |
					   gic_data.rdists.has_rvpeid);
	gic_data.rdists.has_vpend_valid_dirty &= !!(typer & GICR_TYPER_DIRTY);

	/* Detect non-sensical configurations */
	if (gic_data.rdists.has_rvpeid && !gic_data.rdists.has_vlpis) {
		printf("GICR @0x%p: has RVPEID but no VLPIs, disabling DirectLPI support\n", ptr);
		gic_data.rdists.has_direct_lpi = false;
		gic_data.rdists.has_vlpis = false;
		gic_data.rdists.has_rvpeid = false;
	}

	gic_data.ppi_nr = min(GICR_TYPER_NR_PPIS(typer), gic_data.ppi_nr);

	return 1;
}

static int gic_iterate_rdists(int (*fn)(struct redist_region *, void __iomem *))
{
	int ret = -ENODEV;
	int i;

	for (i = 0; i < gic_data.nr_redist_regions; i++) {
		void __iomem *ptr = gic_data.redist_regions[i].redist_base;
		u64 typer;
		u32 reg;

		reg = read_u32(ptr + GICR_PIDR2) & GIC_PIDR2_ARCH_MASK;
		if (reg != GIC_PIDR2_ARCH_GICv3 &&
		    reg != GIC_PIDR2_ARCH_GICv4) { /* We're in trouble... */
			printf("No redistributor present @0x%p\n", ptr);
			break;
		}

		do {
			typer = read_u64(ptr + GICR_TYPER);
			ret = fn(gic_data.redist_regions + i, ptr);
			if (!ret)
				return 0;

			if (gic_data.redist_regions[i].single_redist)
				break;

			if (gic_data.redist_stride) {
				ptr += gic_data.redist_stride;
			} else {
				ptr += SZ_64K * 2; /* Skip RD_base + SGI_base */
				if (typer & GICR_TYPER_VLPIS)
					ptr += SZ_64K * 2; /* Skip VLPI_base + reserved page */
			}
		} while (!(typer & GICR_TYPER_LAST));
	}

	return ret ? -ENODEV : 0;
}

static void gic_update_rdist_properties(void)
{
	gic_data.ppi_nr = UINT_MAX;
	gic_iterate_rdists(__gic_update_rdist_properties);
	if (gic_data.ppi_nr == UINT_MAX) {
		printf("Couldn't determine the number of PPIs, defaulting to 0\n");
		gic_data.ppi_nr = 0;
	}
	printf("GICv3 features: %d PPIs%s%s\n",
		gic_data.ppi_nr,
		gic_data.has_rss ? ", RSS" : "",
		gic_data.rdists.has_direct_lpi ? ", DirectLPI" : "");

	if (gic_data.rdists.has_vlpis)
		printf("GICv4 features: %s%s%s\n",
			gic_data.rdists.has_direct_lpi ? "DirectLPI " : "",
			gic_data.rdists.has_rvpeid ? "RVPEID " : "",
			gic_data.rdists.has_vpend_valid_dirty ? "Valid+Dirty " : "");
}

static void gic_cpu_sys_reg_enable(void)
{
	/*
	 * Need to check that the SRE bit has actually been set. If
	 * not, it means that SRE is disabled at EL2. We're going to
	 * die painfully, and there is nothing we can do about it.
	 *
	 * Kindly inform the luser.
	 */
	if (!gic_enable_sre())
		printf("GIC: unable to set SRE (disabled at EL2), panic ahead\n");

}

static u32 gic_get_pribits(void)
{
	u32 pribits;

	pribits = gic_read_ctlr();
	pribits &= ICC_CTLR_EL1_PRI_BITS_MASK;
	pribits >>= ICC_CTLR_EL1_PRI_BITS_SHIFT;
	pribits++;

	return pribits;
}

static bool gic_has_group0(void)
{
	u32 val;
	u32 old_pmr;

	old_pmr = gic_read_pmr();

	/*
	 * Let's find out if Group0 is under control of EL3 or not by
	 * setting the highest possible, non-zero priority in PMR.
	 *
	 * If SCR_EL3.FIQ is set, the priority gets shifted down in
	 * order for the CPU interface to set bit 7, and keep the
	 * actual priority in the non-secure range. In the process, it
	 * looses the least significant bit and the actual priority
	 * becomes 0x80. Reading it back returns 0, indicating that
	 * we're don't have access to Group0.
	 */
	gic_write_pmr(BIT(8 - gic_get_pribits()));
	val = gic_read_pmr();

	gic_write_pmr(old_pmr);

	return val != 0;
}

static inline bool gic_dist_security_disabled(void)
{
	return read_u32(gic_data.dist_base + GICD_CTLR) & GICD_CTLR_DS;
}

static void gic_prio_init(void)
{
	bool ds;

	cpus_have_group0 = gic_has_group0();

	ds = gic_dist_security_disabled();
	if ((gic_data.flags & FLAGS_WORKAROUND_INSECURE) && !ds) {
		if (cpus_have_group0) {
			u32 val;

			val = read_u32(gic_data.dist_base + GICD_CTLR);
			val |= GICD_CTLR_DS;
			__raw_writel(val, gic_data.dist_base + GICD_CTLR);

			ds = gic_dist_security_disabled();
			if (ds)
				printf("Broken GIC integration, security disabled\n");
		} else {
			printf("Broken GIC integration, pNMI forbidden\n");
			nmi_support_forbidden = true;
		}
	}

	cpus_have_security_disabled = ds;

	/*
	 * How priority values are used by the GIC depends on two things:
	 * the security state of the GIC (controlled by the GICD_CTLR.DS bit)
	 * and if Group 0 interrupts can be delivered to Linux in the non-secure
	 * world as FIQs (controlled by the SCR_EL3.FIQ bit). These affect the
	 * way priorities are presented in ICC_PMR_EL1 and in the distributor:
	 *
	 * GICD_CTLR.DS | SCR_EL3.FIQ | ICC_PMR_EL1 | Distributor
	 * -------------------------------------------------------
	 *      1       |      -      |  unchanged  |  unchanged
	 * -------------------------------------------------------
	 *      0       |      1      |  non-secure |  non-secure
	 * -------------------------------------------------------
	 *      0       |      0      |  unchanged  |  non-secure
	 *
	 * In the non-secure view reads and writes are modified:
	 *
	 * - A value written is right-shifted by one and the MSB is set,
	 *   forcing the priority into the non-secure range.
	 *
	 * - A value read is left-shifted by one.
	 *
	 * In the first two cases, where ICC_PMR_EL1 and the interrupt priority
	 * are both either modified or unchanged, we can use the same set of
	 * priorities.
	 *
	 * In the last case, where only the interrupt priorities are modified to
	 * be in the non-secure range, we program the non-secure values into
	 * the distributor to match the PMR values we want.
	 */
	if (cpus_have_group0 && !cpus_have_security_disabled) {
		dist_prio_irq = __gicv3_prio_to_ns(dist_prio_irq);
		dist_prio_nmi = __gicv3_prio_to_ns(dist_prio_nmi);
	}

	printf("GICD_CTLR.DS=%d, SCR_EL3.FIQ=%d\n",
		cpus_have_security_disabled,
		!cpus_have_group0);
}

static void gic_do_wait_for_rwp(void __iomem *base, u32 bit)
{
	u32 val;
	int ret;

	ret = readx_poll_timeout_atomic(read_u32, base + GICD_CTLR, val, !(val & bit),
						1, USEC_PER_SEC);
	if (ret == -ETIMEDOUT)
		printf("RWP timeout, gone fishing\n");
}

/* Wait for completion of a distributor change */
static void gic_dist_wait_for_rwp(void)
{
	gic_do_wait_for_rwp(gic_data.dist_base, GICD_CTLR_RWP);
}

void gic_dist_config(void __iomem *base, int gic_irqs, u8 priority)
{
	unsigned int i;

	/*
	 * Set all global interrupts to be level triggered, active low.
	 */
	for (i = 32; i < gic_irqs; i += 16)
		__raw_writel(GICD_INT_ACTLOW_LVLTRIG,
					base + GIC_DIST_CONFIG + i / 4);

	/*
	 * Set priority on all global interrupts.
	 */
	for (i = 32; i < gic_irqs; i += 4)
		__raw_writel(REPEAT_BYTE_U32(priority),
			       base + GIC_DIST_PRI + i);

	/*
	 * Deactivate and disable all SPIs. Leave the PPI and SGIs
	 * alone as they are in the redistributor registers on GICv3.
	 */
	for (i = 32; i < gic_irqs; i += 32) {
		__raw_writel(GICD_INT_EN_CLR_X32,
			       base + GIC_DIST_ACTIVE_CLEAR + i / 8);
		__raw_writel(GICD_INT_EN_CLR_X32,
			       base + GIC_DIST_ENABLE_CLEAR + i / 8);
	}
}

u64 cpu_logical_map(unsigned int cpu)
{
	(void)cpu;
	return read_sysreg_s(SYS_MPIDR_EL1) & MPIDR_HWID_BITMASK;
}

static u64 gic_cpu_to_affinity(int cpu)
{
	u64 mpidr = cpu_logical_map(cpu);
	u64 aff;

	aff = ((u64)MPIDR_AFFINITY_LEVEL(mpidr, 3) << 32 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 2) << 16 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 1) << 8  |
	       MPIDR_AFFINITY_LEVEL(mpidr, 0));

	return aff;
}

static void gic_dist_init(void)
{
	unsigned int i;
	u64 affinity;
	void __iomem *base = gic_data.dist_base;
	u32 val;

	/* Disable the distributor */
	__raw_writel(0, base + GICD_CTLR);
	gic_dist_wait_for_rwp();

	/*
	 * Configure SPIs as non-secure Group-1. This will only matter
	 * if the GIC only has a single security state. This will not
	 * do the right thing if the kernel is running in secure mode,
	 * but that's not the intended use case anyway.
	 */
	for (i = 32; i < GIC_LINE_NR; i += 32)
		__raw_writel(~0, base + GICD_IGROUPR + i / 8);

	/* Extended SPI range, not handled by the GICv2/GICv3 common code */
	for (i = 0; i < GIC_ESPI_NR; i += 32) {
		__raw_writel(~0U, base + GICD_ICENABLERnE + i / 8);
		__raw_writel(~0U, base + GICD_ICACTIVERnE + i / 8);
	}

	for (i = 0; i < GIC_ESPI_NR; i += 32)
		__raw_writel(~0U, base + GICD_IGROUPRnE + i / 8);

	/* 全部默认电平触发 */
	for (i = 0; i < GIC_ESPI_NR; i += 16)
		__raw_writel(0, base + GICD_ICFGRnE + i / 4);

	/* 配置默认优先级 */
	for (i = 0; i < GIC_ESPI_NR; i += 4)
		__raw_writel(REPEAT_BYTE_U32(dist_prio_irq),
			       base + GICD_IPRIORITYRnE + i);

	/* Now do the common stuff */
	gic_dist_config(base, GIC_LINE_NR, dist_prio_irq);

	val = GICD_CTLR_ARE_NS | GICD_CTLR_ENABLE_G1A | GICD_CTLR_ENABLE_G1;
	if (gic_data.rdists.gicd_typer2 & GICD_TYPER2_nASSGIcap) {
		printf("Enabling SGIs without active state\n");
		val |= GICD_CTLR_nASSGIreq;
	}

	/* Enable distributor with ARE, Group1, and wait for it to drain */
	/* 使能distributor */
	__raw_writel(val, base + GICD_CTLR);
	gic_dist_wait_for_rwp();

	/*
	 * Set all global interrupts to the boot CPU only. ARE must be
	 * enabled.
	 */
	/* 设置默认路由，全都路由到boot cpu */
	affinity = gic_cpu_to_affinity(get_smp_processor_id());
	for (i = 32; i < GIC_LINE_NR; i++)
		__raw_writeq(affinity, base + GICD_IROUTER + i * 8);

	for (i = 0; i < GIC_ESPI_NR; i++)
		__raw_writeq(affinity, base + GICD_IROUTERnE + i * 8);
}

static int __gic_populate_rdist(struct redist_region *region, void __iomem *ptr)
{
	unsigned long mpidr;
	u64 typer;
	u32 aff;

	/*
	 * Convert affinity to a 32bit value that can be matched to
	 * GICR_TYPER bits [63:32].
	 */
	mpidr = gic_cpu_to_affinity(get_smp_processor_id());

	aff = (MPIDR_AFFINITY_LEVEL(mpidr, 3) << 24 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 2) << 16 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 1) << 8 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 0));

	typer = read_u64(ptr + GICR_TYPER);
	if ((typer >> 32) == aff) {
		u64 offset = ptr - region->redist_base;

		gic_data_rdist_rd_base() = ptr;
		gic_data_rdist()->phys_base = region->phys_base + offset;

		printf("CPU%d: found redistributor %lx region %d:0x%p\n",
			get_smp_processor_id(), mpidr,
			(int)(region - gic_data.redist_regions),
			&gic_data_rdist()->phys_base);
		return 0;
	}

	/* Try next one */
	return 1;
}

static int gic_populate_rdist(void)
{
	if (gic_iterate_rdists(__gic_populate_rdist) == 0)
		return 0;

	/* We couldn't even deal with ourselves... */
	printf("CPU%d: mpidr %lx has no re-distributor!\n",
	     get_smp_processor_id(),
	     (unsigned long)cpu_logical_map(get_smp_processor_id()));
	return -ENODEV;
}

static void gic_enable_redist(bool enable)
{
	void __iomem *rbase;
	u32 val;
	int ret;

	rbase = gic_data_rdist_rd_base();

	val = read_u32(rbase + GICR_WAKER);
	if (enable)
		/* Wake up this CPU redistributor */
		val &= ~GICR_WAKER_ProcessorSleep;
	else
		val |= GICR_WAKER_ProcessorSleep;
	__raw_writel(val, rbase + GICR_WAKER);

	if (!enable) {		/* Check that GICR_WAKER is writeable */
		val = read_u32(rbase + GICR_WAKER);
		if (!(val & GICR_WAKER_ProcessorSleep))
			return;	/* No PM support in this redistributor */
	}

	ret = readx_poll_timeout_atomic(read_u32, rbase + GICR_WAKER, val,
						enable ^ (bool)(val & GICR_WAKER_ChildrenAsleep),
						1, USEC_PER_SEC);
	if (ret == -ETIMEDOUT) {
		printf("redistributor failed to %s...\n",
				   enable ? "wakeup" : "sleep");
	}
}

void gic_cpu_config(void __iomem *base, int nr, u8 priority)
{
	int i;

	/*
	 * Deal with the banked PPI and SGI interrupts - disable all
	 * private interrupts. Make sure everything is deactivated.
	 */
	for (i = 0; i < nr; i += 32) {
		__raw_writel(GICD_INT_EN_CLR_X32,
			       base + GIC_DIST_ACTIVE_CLEAR + i / 8);	// deactive
		__raw_writel(GICD_INT_EN_CLR_X32,
			       base + GIC_DIST_ENABLE_CLEAR + i / 8);	// disable
	}

	/*
	 * Set priority on PPI and SGI interrupts
	 */
	/* 设置优先级 */
	for (i = 0; i < nr; i += 4)
		__raw_writel(REPEAT_BYTE_U32(priority),
					base + GIC_DIST_PRI + i * 4 / 4);
}

/* Wait for completion of a redistributor change */
static void gic_redist_wait_for_rwp(void)
{
	gic_do_wait_for_rwp(gic_data_rdist_rd_base(), GICR_CTLR_RWP);
}

static inline bool gic_supports_nmi(void)
{
	return supports_pseudo_nmis;
}

static void gic_cpu_sys_reg_init(void)
{
	// int i;
	// int cpu = get_smp_processor_id();
	// u64 mpidr = gic_cpu_to_affinity(cpu);
	// u64 need_rss = MPIDR_RS(mpidr);
	bool group0;
	u32 pribits;

	pribits = gic_get_pribits();

	group0 = gic_has_group0();

	write_gicreg(DEFAULT_PMR_VALUE, ICC_PMR_EL1);

	/*
	 * Some firmwares hand over to the kernel with the BPR changed from
	 * its reset value (and with a value large enough to prevent
	 * any pre-emptive interrupts from working at all). Writing a zero
	 * to BPR restores is reset value.
	 */
	gic_write_bpr1(0);

	/* EOI deactivates interrupt too (mode 0) */
	gic_write_ctlr(ICC_CTLR_EL1_EOImode_drop_dir);

	/* Always whack Group0 before Group1 */
	/* 从cpu interface层级取消激活所有中断源 */
	if (group0) {
		switch (pribits) {
		case 8:
		case 7:
			write_gicreg(0, ICC_AP0R3_EL1);
			write_gicreg(0, ICC_AP0R2_EL1);
			fallthrough;
		case 6:
			write_gicreg(0, ICC_AP0R1_EL1);
			fallthrough;
		case 5:
		case 4:
			write_gicreg(0, ICC_AP0R0_EL1);
		}

		isb();
	}

	switch (pribits) {
	case 8:
	case 7:
		write_gicreg(0, ICC_AP1R3_EL1);
		write_gicreg(0, ICC_AP1R2_EL1);
		fallthrough;
	case 6:
		write_gicreg(0, ICC_AP1R1_EL1);
		fallthrough;
	case 5:
	case 4:
		write_gicreg(0, ICC_AP1R0_EL1);
	}

	isb();

	/* ... and let's hit the road... */
	gic_write_grpen1(1);

	/* Keep the RSS capability status in per_cpu variable */
	g_cpu_interface_hasrss = !!(gic_read_ctlr() & ICC_CTLR_EL1_RSS);

	/* Check all the CPUs have capable of sending SGIs to other CPUs */
	// for_each_online_cpu(i) {
	// 	bool have_rss = g_cpu_interface_hasrss;
	// 	// bool have_rss = per_cpu(has_rss, i) && per_cpu(has_rss, cpu);

	// 	need_rss |= MPIDR_RS(gic_cpu_to_affinity(i));
	// 	if (need_rss && (!have_rss))
	// 		pr_crit("CPU%d (%lx) can't SGI CPU%d (%lx), no RSS\n",
	// 			cpu, (unsigned long)mpidr,
	// 			i, (unsigned long)gic_cpu_to_affinity(i));
	// }

	/**
	 * GIC spec says, when ICC_CTLR_EL1.RSS==1 and GICD_TYPER.RSS==0,
	 * writing ICC_ASGI1R_EL1 register with RS != 0 is a CONSTRAINED
	 * UNPREDICTABLE choice of :
	 *   - The write is ignored.
	 *   - The RS field is treated as 0.
	 */
	// if (need_rss && (!gic_data.has_rss))
	// 	pr_crit_once("RSS is required but GICD doesn't support it\n");
}

static void gic_cpu_init(void)
{
	void __iomem *rbase;
	int i;

	/* Register ourselves with the rest of the world */
	/* 填充每个GICR的寄存器地址信息, GICR_TYPER含有affinity信息，
	 * 通过判断cpu的mpidr与GICR_TYPER的affinity是否相等就能判断是否
	 * 是这个cpu的GICR，从而通过percpu来绑定cpu特定的GICR
	 */
	if (gic_populate_rdist())
		return;

	/* 设置GICR_WAKER寄存器，唤醒GICR */
	gic_enable_redist(true);

	if ((gic_data.ppi_nr > 16 || GIC_ESPI_NR != 0) &
	     !(gic_read_ctlr() & ICC_CTLR_EL1_ExtRange))
		printf("Distributor has extended ranges, but CPU%d doesn't\n",
			get_smp_processor_id());

	rbase = gic_data_rdist_sgi_base();

	/* Configure SGIs/PPIs as non-secure Group-1 */
	/* 设置分组 non-secure Group-1 */
	for (i = 0; i < gic_data.ppi_nr + SGI_NR; i += 32)
		__raw_writel(~0, rbase + GICR_IGROUPR0 + i / 8);

	gic_cpu_config(rbase, gic_data.ppi_nr + SGI_NR, dist_prio_irq);
	gic_redist_wait_for_rwp();

	/* initialise system registers */
	gic_cpu_sys_reg_init();
}

static int gic_init_bases(phys_addr_t dist_phys_base,
				 void __iomem *dist_base,
				 struct redist_region *rdist_regs,
				 u32 nr_redist_regions,
				 u64 redist_stride)
{
	int ret;
	u32 typer;

	gic_data.dist_phys_base = dist_phys_base;
	gic_data.dist_base = dist_base;
	gic_data.redist_regions = rdist_regs;
	gic_data.nr_redist_regions = nr_redist_regions;
	gic_data.redist_stride = redist_stride;

	/*
	 * Find out how many interrupts are supported.
	 */
	typer = read_u32(gic_data.dist_base + GICD_TYPER);
	gic_data.rdists.gicd_typer = typer;

	printf("%d SPIs implemented\n", GIC_LINE_NR - 32);
	printf("%d Extended SPIs implemented\n", GIC_ESPI_NR);

	gic_data.rdists.gicd_typer2 = read_u32(gic_data.dist_base + GICD_TYPER2);

	gic_data.rdists.rdist = &g_rdist;

	/* 先赋值成true，后面再根据每个GICR的情况做&=处理 */
	gic_data.rdists.has_rvpeid = true;
	gic_data.rdists.has_vlpis = true;
	gic_data.rdists.has_direct_lpi = true;
	gic_data.rdists.has_vpend_valid_dirty = true;

	gic_data.has_rss = !!(typer & GICD_TYPER_RSS);

	ret = set_handle_irq(gic_v3_handle_irq);
	if (ret < 0) {
		printf("Failed to set GICv3 IRQ handler: %d\n", ret);
		return ret;
	}

	gic_update_rdist_properties();

	gic_cpu_sys_reg_enable();
	gic_prio_init();
	gic_dist_init();
	gic_cpu_init();

	return 0;
}

int gic_irq_enable(int hw_irqn)
{
	u32 val;

	if (hw_irqn < IRQ_SGI_NUM + IRQ_PPI_NUM) {
		val = read_u32(gic_data_rdist_sgi_base() + GIC_DIST_ENABLE_SET);
		val |= 1 << hw_irqn;
		__raw_writel(val,
			       gic_data_rdist_sgi_base() + GIC_DIST_ENABLE_SET);
	}

	return 0;
}

static void __gic_v3_irq_mask(unsigned long hwirq, bool mask)
{
	u32 val;
	u32 reg_offset;

	if (mask)
		reg_offset = GIC_DIST_ENABLE_CLEAR;
	else
		reg_offset = GIC_DIST_ENABLE_SET;

	if (hwirq < IRQ_SGI_NUM + IRQ_PPI_NUM) {
		val = read_u32(gic_data_rdist_sgi_base() + reg_offset);
		val |= 1 << hwirq;
		__raw_writel(val, gic_data_rdist_sgi_base() + reg_offset);
	}
}

static void gic_v3_irq_unmask(struct irq_data *data)
{
	__gic_v3_irq_mask(data->hwirq, false);
}

static void gic_v3_irq_mask(struct irq_data *data)
{
	__gic_v3_irq_mask(data->hwirq, true);
}

static struct irq_chip gic_v3_chip = {
	.irq_unmask = gic_v3_irq_unmask,
	.irq_mask = gic_v3_irq_mask,
};

int gic_v3_init(void)
{
	int ret;

	void *gicd_base;
	void *gicr_base;

	if (in_kernel()) {
		gicd_base = (void *)early_ioremap(GICD_MEMMAP_BASE, GICD_MEMMAP_SIZE);
		gicr_base = (void *)early_ioremap(GICR_MEMMAP_BASE, GICR_MEMMAP_SIZE * REDISTRIBUTOR_REGIONS);
	} else {
		gicd_base = (void *)(uintptr_t)GICD_MEMMAP_BASE;
		gicr_base = (void *)(uintptr_t)GICR_MEMMAP_BASE;
	}

	ret = gic_validate_dist_version(gicd_base);
	if (ret < 0) {
		printf("GICv3 distributor version validation failed: %d\n", ret);
		return ret;
	}

	for (int i = 0; i < REDISTRIBUTOR_REGIONS; i++) {
		g_rdist_regs[i].phys_base = (phys_addr_t)(GICR_MEMMAP_BASE + i * GICR_MEMMAP_SIZE);
		g_rdist_regs[i].redist_base = (void *)(gicr_base + i * GICR_MEMMAP_SIZE);

	}

	ret = gic_init_bases(GICD_MEMMAP_BASE, gicd_base, g_rdist_regs,
			     REDISTRIBUTOR_REGIONS, REDISTRIBUTOR_STRIDE);
	if (ret)
		return ret;

	ret = irq_chip_register(&gic_v3_chip);
	if (ret)
		return ret;

	printf("gic-v3 init success.\n");

	return 0;
}
