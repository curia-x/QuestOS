/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __MEMORY_H
#define __MEMORY_H

#include <linux/sizes.h>
#include <linux/types.h>

/*
 * Physical memory map:
 * 0x00000000 - 0x03FFFFFF : ROM (64MB)
 * 0x04000000 - 0x07FFFFFF : ROM2 (64MB)
 * 0x08000000 - 0x08FFFFFF : MMIO (1GB)
 * 0x40000000 - 0x7FFFFFFF : RAM (1GB)
 * 0x78000000 - 0x7FFFFFFF : Image (256MB) (at ram_top - 256MB)
 */
#define ROM_BASE			0x00000000ULL
#define ROM_SIZE			SZ_64M
#define ROM2_BASE			0x04000000ULL
#define ROM2_SIZE			SZ_64M
#define MMIO_BASE			0x08000000ULL
#define MMIO_SIZE			(RAM_BASE - MMIO_BASE)
#define RAM_BASE			0x40000000ULL
#define RAM_SIZE			SZ_1G
#define IMAGE_RELOCATE_BASE	0x70000000ULL
#define IMAGE_RELOCATE_SIZE	SZ_256M

/*
 * Alignment of kernel segments (e.g. .text, .data).
 *
 *  4 KB granule:  16 level 3 entries, with contiguous bit
 * 16 KB granule:   4 level 3 entries, without contiguous bit
 * 64 KB granule:   1 level 3 entry
 */
#define SEGMENT_ALIGN		SZ_64K

#define BOOTLOADER_TMP_IDMAP_TABLE_ADDR	(IMAGE_RELOCATE_BASE - 10 * SZ_1M)	// 10MB for temporary page table

#define _PAGE_END(va)		(-(UL(1) << ((va) - 1)))

/*
 * PAGE_OFFSET - the virtual address of the start of the linear map, at the
 *               start of the TTBR1 address space.
 * PAGE_END - the end of the linear map, where all other kernel mappings begin.
 * IMAGE_VADDR - the virtual address of the start of the kernel image.
 * VA_BITS - the maximum number of bits for virtual addresses.
 */
#undef VA_BITS
#define VA_BITS			(48)
#define _PAGE_OFFSET(va)	(-(UL(1) << (va)))
#define PAGE_OFFSET		(_PAGE_OFFSET(VA_BITS))
#define IMAGE_VADDR		(_PAGE_END(VA_BITS))
#undef KERNEL_END
#define KERNEL_END		kimage_end

#define FIXADDR_TOP		(-UL(SZ_8M))
#define VMEMMAP_END		(-UL(SZ_1G))
/*
 * VMEMMAP_SIZE - allows the whole linear region to be covered by
 *                a struct page array
 *
 * If we are configured with a 52-bit kernel VA then our VMEMMAP_SIZE
 * needs to cover the memory region from the beginning of the 52-bit
 * PAGE_OFFSET all the way to PAGE_END for 48-bit. This allows us to
 * keep a constant PAGE_OFFSET and "fallback" to using the higher end
 * of the VMEMMAP where 52-bit support is not available in hardware.
 */
#ifndef __ASSEMBLY__
struct page {
	unsigned long flags;
};
#endif
#undef VMEMMAP_RANGE
#define VMEMMAP_RANGE	(_PAGE_END(VA_BITS) - PAGE_OFFSET)	// 线性映射的范围
#define VMEMMAP_SIZE	((VMEMMAP_RANGE >> PAGE_SHIFT) * sizeof(struct page))
#define VMEMMAP_START		(VMEMMAP_END - VMEMMAP_SIZE)

/*
 * VMALLOC range.
 *
 * VMALLOC_START: beginning of the kernel vmalloc space
 * VMALLOC_END: extends to the available space below vmemmap
 */
#define VMALLOC_START		(_PAGE_END(VA_BITS))
#define VMALLOC_END		(VMEMMAP_START - SZ_8M)
#define VMALLOC_FREE_AREA_START KERNEL_END

/* For linux boot. */
#define FDT_BLOB_ADDR			RAM_BASE
#define IMAGE_LOAD_ADDR			(RAM_BASE + SZ_2M) /* linux要求镜像要放到2MB对齐的地方 */
#define KERNEL_IMAGE_LOAD_ADDR	(IMAGE_LOAD_ADDR)
#define KERNEL_IMAGE_LOAD_SIZE	(SZ_128M)	// 128MB for kernel image
#define INITRD_IMAGE_LOAD_ADDR	(KERNEL_IMAGE_LOAD_ADDR + KERNEL_IMAGE_LOAD_SIZE)
#define INITRD_IMAGE_LOAD_SIZE	(SZ_4M)		// 4MB for initrd image
#define BOOTARGS_LOAD_ADDR		(INITRD_IMAGE_LOAD_ADDR + INITRD_IMAGE_LOAD_SIZE)
#define BOOTARGS_LOAD_SIZE		(SZ_1K)		// 1KB for kernel cmdline

/*
 * Memory types available.
 *
 * IMPORTANT: MT_NORMAL must be index 0 since vm_get_page_prot() may 'or' in
 *	      the MT_NORMAL_TAGGED memory type for PROT_MTE mappings. Note
 *	      that protection_map[] only contains MT_NORMAL attributes.
 */
#define MT_NORMAL		0
#define MT_NORMAL_TAGGED	1
#define MT_NORMAL_NC		2
#define MT_DEVICE_nGnRnE	3
#define MT_DEVICE_nGnRE		4

#define MIN_THREAD_SHIFT	14

/*
 * VMAP'd stacks are allocated at page granularity, so we must ensure that such
 * stacks are a multiple of page size.
 */
#if (MIN_THREAD_SHIFT < PAGE_SHIFT)
#define THREAD_SHIFT		PAGE_SHIFT
#else
#define THREAD_SHIFT		MIN_THREAD_SHIFT
#endif

#if THREAD_SHIFT >= PAGE_SHIFT
#define THREAD_SIZE_ORDER	(THREAD_SHIFT - PAGE_SHIFT)
#endif

#define THREAD_SIZE		(UL(1) << THREAD_SHIFT)

/*
 * By aligning VMAP'd stacks to 2 * THREAD_SIZE, we can detect overflow by
 * checking sp & (1 << THREAD_SHIFT), which we can do cheaply in the entry
 * assembly.
 */
#define THREAD_ALIGN		(2 * THREAD_SIZE)

#ifndef __ASSEMBLY__

extern struct process_struct g_kernel_init_process;

#define KERNEL_PGD	(&g_kernel_init_process.mm)

extern u64 kimage_voffset;
#define __virt_to_kimg_phys(x)	((u64)(x) - kimage_voffset)
#define __kimg_phys_to_virt(x)	((u64)(x) + kimage_voffset)

#define LINER_MAP_OFFSET (PAGE_OFFSET - RAM_BASE)
#define __virt_to_phys(x)	((u64)(x) - LINER_MAP_OFFSET)
#define __phys_to_virt(x)	((u64)(x) + LINER_MAP_OFFSET)

#define __pa(x)			__virt_to_phys((unsigned long)(x))
#define __va(x)			((void *)__phys_to_virt((phys_addr_t)(x)))
#define pfn_to_kaddr(pfn)	__va((pfn) << PAGE_SHIFT)

/*
 * Note: Drivers should NOT use these.  They are the wrong
 * translation for translating DMA addresses.  Use the driver
 * DMA support - see dma-mapping.h.
 */
#define virt_to_kimg_phys virt_to_kimg_phys
static inline phys_addr_t virt_to_kimg_phys(const volatile void *x)
{
	return __virt_to_kimg_phys((unsigned long)(x));
}

#define kimg_phys_to_virt kimg_phys_to_virt
static inline void *kimg_phys_to_virt(phys_addr_t x)
{
	return (void *)(__kimg_phys_to_virt(x));
}

#define virt_to_phys virt_to_phys
static inline phys_addr_t virt_to_phys(const volatile void *x)
{
	return __virt_to_phys((unsigned long)(x));
}

#define phys_to_virt phys_to_virt
static inline void *phys_to_virt(phys_addr_t x)
{
	return (void *)(__phys_to_virt(x));
}

static inline u64 __pure read_tcr(void)
{
	u64  tcr;

	// read_sysreg() uses asm volatile, so avoid it here
	asm("mrs %0, tcr_el1" : "=r"(tcr));
	return tcr;
}

void map_mmio(void);
#endif /* __ASSEMBLY__ */
#endif /* __MEMORY_H */
