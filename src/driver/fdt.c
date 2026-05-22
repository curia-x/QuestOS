// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2026 <Nino Zhang>
 *
 * Parts of the design/logic are inspired by U-Boot.
 */

#include <memory.h>
#include <linux/libfdt.h>
#include <fdt_support.h>
#include <dm/of.h>
#include <linux/errno.h>
#include <pgtable-prot.h>
#include <print.h>
#include <init.h>
#include <fixmap.h>
#include <mmu.h>
#include <memblock.h>

char *g_fdt = (char *)FDT_BLOB_ADDR;

/* Max address size we deal with */
#define FDT_MAX_ADDR_CELLS	4
#define FDT_CHECK_COUNTS(ac, sc)	((ac) > 0 && (ac) <= FDT_MAX_ADDR_CELLS && \
			(sc) > 0)

#ifdef printf
static void fdt_dump_addr(const char *s, const fdt32_t *addr, int na)
{
	printf("%s", s);
	while (na--)
		printf(" %08x", *(addr++));
	printf("\n");
}
#else
static void fdt_dump_addr(const char *s, const fdt32_t *addr, int na) { }
#endif

void fdt_count_cells(const void *blob, int parent_offset,
					int *paddr_cells, int *psize_cells)
{
	const fdt32_t *prop;

	if (paddr_cells)
		*paddr_cells = fdt_address_cells(blob, parent_offset);

	if (psize_cells) {
		prop = fdt_getprop(blob, parent_offset, "#size-cells", NULL);
		if (prop)
			*psize_cells = be32_to_cpup(prop);
		else
			*psize_cells = 1;
	}
}

static u64 fdt_bus_map(fdt32_t *addr, const fdt32_t *range,
		int na, int ns, int pna)
{
	u64 cp, s, da;

	cp = fdt_read_number(range, na);
	s  = fdt_read_number(range + na + pna, ns);
	da = fdt_read_number(addr, na);

	printf("OF: default map, cp=%llx, s=%llx, da=%llx\n", cp, s, da);

	if (da < cp || da >= (cp + s))
		return OF_BAD_ADDR;
	return da - cp;
}

static int fdt_bus_translate(fdt32_t *addr, u64 offset, int na)
{
	u64 a = fdt_read_number(addr, na);

	memset(addr, 0, na * 4);
	a += offset;
	if (na > 1)
		addr[na - 2] = cpu_to_fdt32(a >> 32);
	addr[na - 1] = cpu_to_fdt32(a & 0xffffffffu);

	return 0;
}

static int fdt_translate_one(const void *blob, int parent, fdt32_t *addr,
							 int na, int ns, int pna, const char *rprop)
{
	const fdt32_t *ranges;
	int rlen;
	int rone;
	u64 offset = OF_BAD_ADDR;

	ranges = fdt_getprop(blob, parent, rprop, &rlen);
	if (ranges == NULL || rlen == 0) {
		offset = fdt_read_number(addr, na);
		memset(addr, 0, pna * 4);
		printf("FDT: no ranges, 1:1 translation\n");
		goto finish;
	}

	printf("FDT: walking ranges...\n");

	/* Now walk through the ranges */
	rlen /= 4;
	rone = na + pna + ns;
	for (; rlen >= rone; rlen -= rone, ranges += rone) {
		offset = fdt_bus_map(addr, ranges, na, ns, pna);
		if (offset != OF_BAD_ADDR)
			break;
	}
	if (offset == OF_BAD_ADDR) {
		printf("FDT: not found !\n");
		return 1;
	}
	memcpy(addr, ranges + na, 4 * pna);

 finish:
	fdt_dump_addr("FDT: parent translation for:", addr, pna);
	printf("FDT: with offset: %llu\n", offset);

	/* Translate it into parent bus space */
	return fdt_bus_translate(addr, offset, pna);
}

static u64 __of_translate_address(const void *blob, int node_offset,
				  const fdt32_t *in_addr, const char *rprop)
{
	int parent;
	fdt32_t addr[FDT_MAX_ADDR_CELLS];
	int na, ns,pna, pns;
	u64 result = OF_BAD_ADDR;

	parent = fdt_parent_offset(blob, node_offset);
	if (parent < 0)
		goto bail;

	/* Count address cells & copy address locally */
	fdt_count_cells(blob, parent, &na, &ns);
	if (!FDT_CHECK_COUNTS(na, ns)) {
		printf("%s: Bad cell count for %s\n", __func__,
			   fdt_get_name(blob, node_offset, NULL));
		goto bail;
	}
	memcpy(addr, in_addr, na * 4);

	printf("FDT: na=%d, ns=%d on %s\n",
		na, ns, fdt_get_name(blob, parent, NULL));
	fdt_dump_addr("FDT: translating address:", addr, na);

	/* Translate */
	for (;;) {
		/* Switch to parent bus */
		node_offset = parent;
		parent = fdt_parent_offset(blob, node_offset);

		/* If root, we have finished */
		if (parent < 0) {
			printf("FDT: reached root node\n");
			result = fdt_read_number(addr, na);
			break;
		}

		fdt_count_cells(blob, parent, &pna, &pns);
		if (!FDT_CHECK_COUNTS(pna, pns)) {
			printf("%s: Bad cell count for %s\n", __func__,
				fdt_get_name(blob, node_offset, NULL));
			break;
		}

		printf("FDT: parent bus (na=%d, ns=%d) on %s\n",
			pna, pns, fdt_get_name(blob, parent, NULL));

		/* Apply bus translation */
		if (fdt_translate_one(blob, node_offset,
					addr, na, ns, pna, rprop))
			break;

		/* Complete the move up one level */
		na = pna;
		ns = pns;

		fdt_dump_addr("FDT: one level translation:", addr, na);
	}
 bail:

	return result;
}

u64 fdt_translate_address(const void *blob, int node_offset,
			  const fdt32_t *in_addr)
{
	return __of_translate_address(blob, node_offset, in_addr, "ranges");
}

fdt_addr_t fdt_get_addr_index_parent(int index, int offset, int parent)
{
	fdt_addr_t addr;

	const fdt32_t *reg;
	int len = 0;
	int na, ns;

	na = fdt_address_cells(g_fdt, parent);
	if (na < 1) {
		printf("bad #address-cells\n");
		return FDT_ADDR_T_NONE;
	}

	printf("fdt:na=0x%x\n", na);

	ns = fdt_size_cells(g_fdt, parent);
	if (ns < 0) {
		printf("bad #size-cells\n");
		return FDT_ADDR_T_NONE;
	}

	printf("fdt:ns=0x%x\n", ns);

	reg = fdt_getprop(g_fdt, offset, "reg", &len);
	if (!reg || (len < ((index + 1) * sizeof(fdt32_t) * (na + ns)))) {
		printf("Req index out of range\n");
		return FDT_ADDR_T_NONE;
	}

	reg += index * (na + ns);

	if (ns) {
		/* Use the full-fledged translate function for complex bus setups. */
		addr = fdt_translate_address((void *)g_fdt,
							offset, reg);
	} else {
		/* Non translatable if #size-cells == 0 */
		addr = fdt_read_number(reg, na);
	}

	printf("fdt:addr=0x%llx\n", addr);

	return addr;
}

fdt_addr_t fdt_get_addr_index(int offset, int index)
{
	int parent = fdt_parent_offset(g_fdt, offset);

	return fdt_get_addr_index_parent(index, offset, parent);
}

fdt_addr_t fdt_get_addr(int offset)
{
	return fdt_get_addr_index(offset, 0);
}

fdt_addr_t fdt_get_node_by_compatible(const char *compatible)
{
	int offset;

	offset = fdt_node_offset_by_compatible(g_fdt, -1, compatible);
	if (offset < 0)
		return -ENOENT;

	return offset;
}

int fdt_get_qfw_node(void)
{
	return fdt_node_offset_by_compatible(g_fdt, -1, "qemu,fw-cfg-mmio");
}

int fdt_modify_chosen(const char *prop_name, const char *value)
{
	int offset;
	int len;

	if (!prop_name || !value) {
		printf("Invalid property name or value, do not modify\n");
		return 0;
	}

	offset = fdt_path_offset(g_fdt, "/chosen");
	if (offset < 0) {
		printf("Failed to find 'chosen' node in device tree\n");
		return -ENOENT;
	}

	len = strlen(value) + 1; // 包括字符串结尾的'\0'
	return fdt_setprop(g_fdt, offset, prop_name, value, len);
}

/* For bootloader. */
int fdt_init(const void *fdt)
{
	int ret;

	// 这里可以添加一些对设备树的检查，比如检查magic number，或者检查某些必须存在的节点
	ret = fdt_check_header(fdt);
	if (ret != 0) {
		printf("Invalid FDT blob: %d\r\n", ret);
		return -EFAULT;
	}

	g_fdt = (char *)FDT_BLOB_ADDR;

	printf("FDT blob is valid\r\n");
	return 0;
}

/* For kernel. */

static void *g_dt_virt;

void *__init fixmap_remap_fdt(phys_addr_t dt_phys, int *size, pgprot_t prot)
{
	const u64 dt_virt_base = __fix_to_virt(FIX_FDT);
	phys_addr_t dt_phys_base;
	int offset;
	void *dt_virt;

	if (!dt_phys || dt_phys % MIN_FDT_ALIGN)
		return NULL;

	dt_phys_base = round_down(dt_phys, PAGE_SIZE);
	offset = dt_phys % PAGE_SIZE;
	dt_virt = (void *)dt_virt_base + offset;

	/* map the first chunk so we can read the size from the header */
	create_mapping_noalloc(dt_phys_base, dt_virt_base, PAGE_SIZE, prot);

	if (fdt_magic(dt_virt) != FDT_MAGIC)
		return NULL;

	*size = fdt_totalsize(dt_virt);
	if (*size > MAX_FDT_SIZE)
		return NULL;

	if (offset + *size > PAGE_SIZE) {
		create_mapping_noalloc(dt_phys_base, dt_virt_base,
				       offset + *size, prot);
	}

	return dt_virt;
}

void __init setup_machine_fdt(phys_addr_t dt_phys)
{
	int size = 0;
	void *dt_virt;

	dt_virt = fixmap_remap_fdt(dt_phys, &size, PAGE_KERNEL);
	if (!dt_virt) {
		printf("%d, fixmap_remap_fdt failed\r\n", __LINE__);
		return;
	}

	memblock_reserve(dt_phys, size);

	/* Do some fixup. */

	/* Early fixups are done, map the FDT as read-only now */
	g_dt_virt = fixmap_remap_fdt(dt_phys, &size, PAGE_KERNEL_RO);
	if (!g_dt_virt)
		printf("%d, fixmap_remap_fdt failed\r\n", __LINE__);
	else
		printf("fixmap_remap_fdt success\r\n");
}
