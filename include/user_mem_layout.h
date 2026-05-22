/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __USER_MEM_LAYOUT_H__
#define __USER_MEM_LAYOUT_H__

#ifndef SZ_1G
#define SZ_1G	0x40000000UL
#endif

#ifndef SZ_4G
#define SZ_4G	(4 * SZ_1G)
#endif

#ifndef SZ_256G
#define SZ_256G	(256 * SZ_1G)
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE	0x1000
#endif

#define KERNEL_SPACE_START		0xFFFF000000000000

#define USER_IMAGE_BASE			0x40000000
#define USER_IMAGE_SIZE			SZ_4G
#define USER_HEAP_BASE			0x180000000
#define USER_STACK_TOP			0x4180000000
#define USER_HEAP_STACK_SIZE	SZ_256G
#define USER_MMAP_BASE			0x41C0000000
#define USER_MMAP_END			(KERNEL_SPACE_START - SZ_1G)

#endif /* __USER_MEM_LAYOUT_H__ */