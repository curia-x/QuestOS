/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __MEMBLOCK_H
#define __MEMBLOCK_H

#include <linux/types.h>
#include <init.h>
#include <nodemask_types.h>
#include <print.h>

#ifdef DEBUG
#define memblock_dbg(fmt, ...)                         \
	do {                                               \
		printf(fmt, ##__VA_ARGS__);                    \
	} while (0)
#else
#define memblock_dbg(fmt, ...)                         \
	do {                                               \
		if (0)                                         \
			printf(fmt, ##__VA_ARGS__);         \
	} while (0)
#endif

#ifndef CONFIG_ARCH_KEEP_MEMBLOCK
#define __init_memblock __meminit
#define __initdata_memblock __meminitdata
void memblock_discard(void);
#else
#define __init_memblock
#define __initdata_memblock
static inline void memblock_discard(void) {}
#endif

/* Flags for memblock allocation APIs */
#define MEMBLOCK_ALLOC_ANYWHERE	(~(phys_addr_t)0)
#define MEMBLOCK_ALLOC_ACCESSIBLE	0
/*
 *  MEMBLOCK_ALLOC_NOLEAKTRACE avoids kmemleak tracing. It implies
 *  MEMBLOCK_ALLOC_ACCESSIBLE
 */
#define MEMBLOCK_ALLOC_NOLEAKTRACE	1

/**
 * enum memblock_flags - definition of memory region attributes
 * @MEMBLOCK_NONE: no special request
 * @MEMBLOCK_HOTPLUG: memory region indicated in the firmware-provided memory
 * map during early boot as hot(un)pluggable system RAM (e.g., memory range
 * that might get hotunplugged later). With "movable_node" set on the kernel
 * commandline, try keeping this memory region hotunpluggable. Does not apply
 * to memblocks added ("hotplugged") after early boot.
 * @MEMBLOCK_MIRROR: mirrored region
 * @MEMBLOCK_NOMAP: don't add to kernel direct mapping and treat as
 * reserved in the memory map; refer to memblock_mark_nomap() description
 * for further details
 * @MEMBLOCK_DRIVER_MANAGED: memory region that is always detected and added
 * via a driver, and never indicated in the firmware-provided memory map as
 * system RAM. This corresponds to IORESOURCE_SYSRAM_DRIVER_MANAGED in the
 * kernel resource tree.
 * @MEMBLOCK_RSRV_NOINIT: memory region for which struct pages are
 * not initialized (only for reserved regions).
 * @MEMBLOCK_RSRV_KERN: memory region that is reserved for kernel use,
 * either explictitly with memblock_reserve_kern() or via memblock
 * allocation APIs. All memblock allocations set this flag.
 * @MEMBLOCK_KHO_SCRATCH: memory region that kexec can pass to the next
 * kernel in handover mode. During early boot, we do not know about all
 * memory reservations yet, so we get scratch memory from the previous
 * kernel that we know is good to use. It is the only memory that
 * allocations may happen from in this phase.
 */
enum memblock_flags {
	MEMBLOCK_NONE		= 0x0,	/* No special request */
	MEMBLOCK_HOTPLUG	= 0x1,	/* hotpluggable region */
	MEMBLOCK_MIRROR		= 0x2,	/* mirrored region */
	MEMBLOCK_NOMAP		= 0x4,	/* don't add to kernel direct mapping */
	MEMBLOCK_DRIVER_MANAGED = 0x8,	/* always detected via a driver */
	MEMBLOCK_RSRV_NOINIT	= 0x10,	/* don't initialize struct pages */
	MEMBLOCK_RSRV_KERN	= 0x20,	/* memory reserved for kernel use */
	MEMBLOCK_KHO_SCRATCH	= 0x40,	/* scratch memory for kexec handover */
};

/**
 * struct memblock_region - represents a memory region
 * @base: base address of the region
 * @size: size of the region
 * @flags: memory region attributes
 * @nid: NUMA node id
 */
struct memblock_region {
	phys_addr_t base;
	phys_addr_t size;
	enum memblock_flags flags;
#ifdef CONFIG_NUMA
	int nid;
#endif
};

/**
 * struct memblock_type - collection of memory regions of certain type
 * @cnt: number of regions
 * @max: size of the allocated array
 * @total_size: size of all regions
 * @regions: array of regions
 * @name: the memory type symbolic name
 */
struct memblock_type {
	unsigned long cnt;
	unsigned long max;
	phys_addr_t total_size;
	struct memblock_region *regions;
	char *name;
};

/**
 * struct memblock - memblock allocator metadata
 * @bottom_up: is bottom up direction?
 * @current_limit: physical address of the current allocation limit
 * @memory: usable memory regions
 * @reserved: reserved memory regions
 */
struct memblock {
	bool bottom_up;  /* is bottom up direction? */
	phys_addr_t current_limit;
	struct memblock_type memory;
	struct memblock_type reserved;
};

extern struct memblock memblock;

void __init memblock_allow_resize(void);
int __init_memblock memblock_phys_free(phys_addr_t base, phys_addr_t size);
void __init_memblock memblock_free(void *ptr, size_t size);
int __memblock_reserve(phys_addr_t base, phys_addr_t size, int nid,
		       enum memblock_flags flags);

/* Low level functions */
void __next_mem_range(u64 *idx, int nid, enum memblock_flags flags,
		      struct memblock_type *type_a,
		      struct memblock_type *type_b, phys_addr_t *out_start,
		      phys_addr_t *out_end, int *out_nid);

void __next_mem_range_rev(u64 *idx, int nid, enum memblock_flags flags,
			  struct memblock_type *type_a,
			  struct memblock_type *type_b, phys_addr_t *out_start,
			  phys_addr_t *out_end, int *out_nid);

/**
 * __for_each_mem_range - iterate through memblock areas from type_a and not
 * included in type_b. Or just type_a if type_b is NULL.
 * @i: u64 used as loop variable
 * @type_a: ptr to memblock_type to iterate
 * @type_b: ptr to memblock_type which excludes from the iteration
 * @nid: node selector, %NUMA_NO_NODE for all nodes
 * @flags: pick from blocks based on memory attributes
 * @p_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @p_end: ptr to phys_addr_t for end address of the range, can be %NULL
 * @p_nid: ptr to int for nid of the range, can be %NULL
 */
#define __for_each_mem_range(i, type_a, type_b, nid, flags,		\
			   p_start, p_end, p_nid)			\
	for (i = 0, __next_mem_range(&i, nid, flags, type_a, type_b,	\
				     p_start, p_end, p_nid);		\
	     i != (u64)ULLONG_MAX;					\
	     __next_mem_range(&i, nid, flags, type_a, type_b,		\
			      p_start, p_end, p_nid))

/**
 * __for_each_mem_range_rev - reverse iterate through memblock areas from
 * type_a and not included in type_b. Or just type_a if type_b is NULL.
 * @i: u64 used as loop variable
 * @type_a: ptr to memblock_type to iterate
 * @type_b: ptr to memblock_type which excludes from the iteration
 * @nid: node selector, %NUMA_NO_NODE for all nodes
 * @flags: pick from blocks based on memory attributes
 * @p_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @p_end: ptr to phys_addr_t for end address of the range, can be %NULL
 * @p_nid: ptr to int for nid of the range, can be %NULL
 */
#define __for_each_mem_range_rev(i, type_a, type_b, nid, flags,		\
				 p_start, p_end, p_nid)			\
	for (i = (u64)ULLONG_MAX,					\
		     __next_mem_range_rev(&i, nid, flags, type_a, type_b, \
					  p_start, p_end, p_nid);	\
	     i != (u64)ULLONG_MAX;					\
	     __next_mem_range_rev(&i, nid, flags, type_a, type_b,	\
				  p_start, p_end, p_nid))

/**
 * for_each_free_mem_range - iterate through free memblock areas
 * @i: u64 used as loop variable
 * @nid: node selector, %NUMA_NO_NODE for all nodes
 * @flags: pick from blocks based on memory attributes
 * @p_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @p_end: ptr to phys_addr_t for end address of the range, can be %NULL
 * @p_nid: ptr to int for nid of the range, can be %NULL
 *
 * Walks over free (memory && !reserved) areas of memblock.  Available as
 * soon as memblock is initialized.
 */
#define for_each_free_mem_range(i, nid, flags, p_start, p_end, p_nid)	\
	__for_each_mem_range(i, &memblock.memory, &memblock.reserved,	\
			     nid, flags, p_start, p_end, p_nid)

/**
 * for_each_free_mem_range_reverse - rev-iterate through free memblock areas
 * @i: u64 used as loop variable
 * @nid: node selector, %NUMA_NO_NODE for all nodes
 * @flags: pick from blocks based on memory attributes
 * @p_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @p_end: ptr to phys_addr_t for end address of the range, can be %NULL
 * @p_nid: ptr to int for nid of the range, can be %NULL
 *
 * Walks over free (memory && !reserved) areas of memblock in reverse
 * order.  Available as soon as memblock is initialized.
 */
#define for_each_free_mem_range_reverse(i, nid, flags, p_start, p_end,	\
					p_nid)				\
	__for_each_mem_range_rev(i, &memblock.memory, &memblock.reserved, \
				 nid, flags, p_start, p_end, p_nid)

static __always_inline int memblock_reserve(phys_addr_t base, phys_addr_t size)
{
	return __memblock_reserve(base, size, NUMA_NO_NODE, 0);
}

int __init_memblock memblock_add(phys_addr_t base, phys_addr_t size);

phys_addr_t __init memblock_phys_alloc_range(phys_addr_t size,
					     phys_addr_t align,
					     phys_addr_t start,
					     phys_addr_t end);

/*
 * Check if the allocation direction is bottom-up or not.
 * if this is true, that said, memblock will allocate memory
 * in bottom-up direction.
 */
static inline __init_memblock bool memblock_bottom_up(void)
{
	return memblock.bottom_up;
}

static inline bool memblock_is_hotpluggable(struct memblock_region *m)
{
	return m->flags & MEMBLOCK_HOTPLUG;
}

static inline bool memblock_is_mirror(struct memblock_region *m)
{
	return m->flags & MEMBLOCK_MIRROR;
}

static inline bool memblock_is_nomap(struct memblock_region *m)
{
	return m->flags & MEMBLOCK_NOMAP;
}

static inline bool memblock_is_reserved_noinit(struct memblock_region *m)
{
	return m->flags & MEMBLOCK_RSRV_NOINIT;
}

static inline bool memblock_is_driver_managed(struct memblock_region *m)
{
	return m->flags & MEMBLOCK_DRIVER_MANAGED;
}

static inline bool memblock_is_kho_scratch(struct memblock_region *m)
{
	return m->flags & MEMBLOCK_KHO_SCRATCH;
}

static __always_inline int memblock_reserve_kern(phys_addr_t base, phys_addr_t size)
{
	return __memblock_reserve(base, size, NUMA_NO_NODE, MEMBLOCK_RSRV_KERN);
}

#endif /* __MEMBLOCK_H */
