// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026 <Nino Zhang>
 *
 * Parts of the design/logic are inspired by U-Boot.
 */

#include <linux/errno.h>
#include <linux/compiler.h>
#include <linux/libfdt.h>
#include <linux/types.h>
#include <memory.h>
#include <qfw.h>
#include <print.h>
#include <compiler.h>
#include <mapmem.h>
#include <fdtdec.h>
#include <fdt_local.h>
#include <cache.h>

struct qfw_mmio {
	/*
	 * Each access to the 64-bit data register can be 8/16/32/64 bits wide.
	 * Need -std=gnu11 to support anonymous union.
	 */
	union {
		u8 data8;
		u16 data16;
		u32 data32;
		u64 data64;
	};
	u16 selector;
	u8 padding[6];
	u64 dma;
};

static u32 g_dma_present = 1;
static volatile struct qfw_mmio *g_qfw_mmio;

static void qfw_mmio_read_item_io(u16 entry, u32 size, void *address)
{
	/*
	 * writing FW_CFG_INVALID will cause read operation to resume at last
	 * offset, otherwise read will start at offset 0
	 *
	 * Note: on platform where the control register is MMIO, the register
	 * is big endian.
	 */
	if (entry != FW_CFG_INVALID)
		g_qfw_mmio->selector = cpu_to_be16(entry);

	/* the endianness of data register is string-preserving */
	while (size >= 8) {
		*(u64 *)address = g_qfw_mmio->data64;
		address += 8;
		size -= 8;
	}
	while (size >= 4) {
		*(u32 *)address = g_qfw_mmio->data32;
		address += 4;
		size -= 4;
	}
	while (size >= 2) {
		*(u16 *)address = g_qfw_mmio->data16;
		address += 2;
		size -= 2;
	}
	while (size >= 1) {
		*(u8 *)address = g_qfw_mmio->data8;
		address += 1;
		size -= 1;
	}
}

/* Read configuration item using fw_cfg DMA interface */
static void qfw_mmio_read_item_dma(struct qfw_dma *dma)
{
	printf("DMA read: addr=%#llx len=%u control=%#x\n",
	       be64_to_cpu(dma->address), be32_to_cpu(dma->length),
	       be32_to_cpu(dma->control));

	/* the DMA address register is big-endian */
	g_qfw_mmio->dma = cpu_to_be64((uintptr_t)dma);

	while (be32_to_cpu(dma->control) & ~FW_CFG_DMA_ERROR)
		;
}

static void qfw_read_item_dma(u16 entry, u32 size, void *address)
{
	struct qfw_dma dma = {
		.length = cpu_to_be32(size),
		.address = cpu_to_be64((uintptr_t)address),
		.control = cpu_to_be32(FW_CFG_DMA_READ),
	};

	/*
	 * writing FW_CFG_INVALID will cause read operation to resume at last
	 * offset, otherwise read will start at offset 0
	 */
	if (entry != FW_CFG_INVALID)
		dma.control |= cpu_to_be32(FW_CFG_DMA_SELECT | (entry << 16));

	printf("%s: entry 0x%x, size %u address 0x%p, control 0x%x\n", __func__,
	      entry, size, address, be32_to_cpu(dma.control));

	barrier();

	qfw_mmio_read_item_dma(&dma);

	dcache_inval_poc((u64)address, (u64)address + size);
}

static void qfw_read_item(u16 entry, u32 size, void *address)
{
	if (g_dma_present)
		qfw_read_item_dma(entry, size, address);
	else
		qfw_mmio_read_item_io(entry, size, address);
}

int qfw_load_images(void)
{
	char *data_addr;
	u32 setup_size, kernel_size, cmdline_size, initrd_size;

	qfw_read_item(FW_CFG_SETUP_SIZE, 4, &setup_size);
	qfw_read_item(FW_CFG_KERNEL_SIZE, 4, &kernel_size);

	if (!kernel_size) {
		printf("fatal: no kernel available\n");
		return -ENOENT;
	}

	data_addr = map_sysmem(KERNEL_IMAGE_LOAD_ADDR, 0);
	if (setup_size) {
		qfw_read_item(FW_CFG_SETUP_DATA,
			       le32_to_cpu(setup_size), data_addr);
		data_addr += le32_to_cpu(setup_size);
	}

	qfw_read_item(FW_CFG_KERNEL_DATA,
		       le32_to_cpu(kernel_size), data_addr);

	/* invalidate icache: we should make sure the icache must not
	 * hold any stale entries corresponding to the loaded kernel image.
	 */
	icache_inval_pou((u64)data_addr, (u64)data_addr + le32_to_cpu(kernel_size));

	data_addr = map_sysmem(INITRD_IMAGE_LOAD_ADDR, 0);
	qfw_read_item(FW_CFG_INITRD_SIZE, 4, &initrd_size);
	if (!initrd_size)
		printf("warning: no initrd available\n");
	else
		qfw_read_item(FW_CFG_INITRD_DATA,
			       le32_to_cpu(initrd_size), data_addr);

	data_addr = map_sysmem(BOOTARGS_LOAD_ADDR, 0);
	qfw_read_item(FW_CFG_CMDLINE_SIZE, 4, &cmdline_size);
	if (cmdline_size) {
		qfw_read_item(FW_CFG_CMDLINE_DATA,
			       le32_to_cpu(cmdline_size), data_addr);
	}

	printf("loading kernel to address %llx size %x", KERNEL_IMAGE_LOAD_ADDR,
		le32_to_cpu(kernel_size));
	if (initrd_size)
		printf(" initrd %llx size %x\n", INITRD_IMAGE_LOAD_ADDR,
		       le32_to_cpu(initrd_size));
	else
		printf("\n");

	return 0;
}

char *qfw_get_bootargs(void)
{
	return (char *)map_sysmem(BOOTARGS_LOAD_ADDR, 0);
}

char *qfw_get_kernel_image_addr(void)
{
	return (char *)map_sysmem(KERNEL_IMAGE_LOAD_ADDR, 0);
}

int qfw_init(void)
{
	int qfw_node;
	fdt_addr_t qfw_addr;

	qfw_node = fdt_get_qfw_node();
	if (qfw_node < 0) {
		printf("Failed to find fw-cfg node in device tree\n");
		return -ENODEV;
	}

	printf("qfw_node=0x%x\r\n", qfw_node);

	qfw_addr = fdt_get_addr(qfw_node);
	if (qfw_addr == FDT_ADDR_T_NONE) {
		printf("Failed to get fw-cfg address\n");
		return -EINVAL;
	}

	g_qfw_mmio = (struct qfw_mmio *)qfw_addr;

	printf("qfw_mmio=0x%llx\r\n", (u64)g_qfw_mmio);

	return 0;
}
