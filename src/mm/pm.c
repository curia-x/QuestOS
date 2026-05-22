// SPDX-License-Identifier: GPL-2.0
#include <linux/types.h>
#include <linux/array_size.h>
#include <memory.h>
#include <memblock.h>
#include <linker_symbol.h>

struct physmem_region {
	u64 start;
	u64 size;
};

static struct physmem_region g_physmem_table[] = {
	{RAM_BASE, RAM_SIZE},
};

static void reserve_kimage(void)
{
	memblock_reserve(LDSYM_PHYS_ADDR(_stext), LDSYM_ADDR(_etext) - LDSYM_ADDR(_stext));

	memblock_reserve(LDSYM_PHYS_ADDR(__start_rodata), LDSYM_ADDR(__init_begin) - LDSYM_ADDR(__start_rodata));

	memblock_reserve(LDSYM_PHYS_ADDR(__inittext_begin), LDSYM_ADDR(__inittext_end) - LDSYM_ADDR(__inittext_begin));

	memblock_reserve(LDSYM_PHYS_ADDR(__initdata_begin), LDSYM_ADDR(__initdata_end) - LDSYM_ADDR(__initdata_begin));

	memblock_reserve(LDSYM_PHYS_ADDR(_data), LDSYM_ADDR(kimage_end) - LDSYM_ADDR(_data));
}

#define for_each_array_item(iter, arr)                                      \
	for (typeof(&(arr)[0]) iter = &(arr)[0],                           \
	     __end = &(arr)[0] + ARRAY_SIZE(arr);                          \
	     (iter) < __end;                                               \
	     ++(iter))

void physmem_init(void)
{
	for_each_array_item(iter, g_physmem_table)
		memblock_add(iter->start, iter->size);

	reserve_kimage();
}
