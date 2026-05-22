// SPDX-License-Identifier: GPL-2.0
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/log2.h>
#include <linux/minmax.h>
#include <linux/kern_levels.h>
#include <asm-generic/memory_model.h>
#include <xarray.h>
#include <init.h>
#include <mm.h>
#include <vmalloc.h>
#include <rbtree.h>
#include <list.h>
#include <rbtree_augmented.h>
#include <kmalloc.h>
#include <linker_symbol.h>
#include <pg_table.h>
#include <system.h>
#include <nodemask_types.h>
#include <memblock.h>
#include <mem_alloc.h>
#include <mapping.h>
#include <stub.h>
#include <memory.h>
#include <tlbflush.h>
#include <process.h>

/* A simple iterator over all vmap-nodes. */
#define for_each_vmap_node(vn)	\
	for ((vn) = &vmap_nodes[0];	\
		(vn) < &vmap_nodes[nr_vmap_nodes]; (vn)++)

/*
 * This structure defines a single, solid model where a list and
 * rb-tree are part of one entity protected by the lock. Nodes are
 * sorted in ascending order, thus for O(1) access to left/right
 * neighbors a list is used as well as for sequential traversal.
 */
struct rb_list {
	struct rb_root root;
	struct list_head head;
};

/*
 * A fast size storage contains VAs up to 1M size. A pool consists
 * of linked between each other ready to go VAs of certain sizes.
 * An index in the pool-array corresponds to number of pages + 1.
 */
#define MAX_VA_SIZE_PAGES 256

struct vmap_pool {
	struct list_head head;
	unsigned long len;
};

/*
 * An effective vmap-node logic. Users make use of nodes instead
 * of a global heap. It allows to balance an access and mitigate
 * contention.
 */
static struct vmap_node {
	/* Simple size segregated storage. */
	struct vmap_pool pool[MAX_VA_SIZE_PAGES];
	bool skip_populate;

	struct rb_list busy;
	struct rb_list lazy;

	/*
	 * Ready-to-free areas.
	 */
	struct list_head purge_list;
	unsigned long nr_purged;
} single;

static struct vmap_node *vmap_nodes = &single;
static unsigned int nr_vmap_nodes = 1;

static unsigned long vmap_lazy_nr;

/*
 * This linked list is used in pair with free_vmap_area_root.
 * It gives O(1) access to prev/next to perform fast coalescing.
 */
static LIST_HEAD(free_vmap_area_list);

/*
 * This augment red-black tree represents the free vmap space.
 * All vmap_area objects in this tree are sorted by va->va_start
 * address. It is used for allocation and merging when a vmap
 * object is released.
 *
 * Each vmap_area node contains a maximum available free block
 * of its sub-tree, right or left. Therefore it is possible to
 * find a lowest match of free area.
 */
static struct rb_root free_vmap_area_root = RB_ROOT;

static __always_inline unsigned long
va_size(struct vmap_area *va)
{
	return (va->va_end - va->va_start);
}

RB_DECLARE_CALLBACKS_MAX(static, free_vmap_area_rb_augment_cb,
	struct vmap_area, rb_node, unsigned long, subtree_max_size, va_size)

static bool vmap_initialized;
static struct vm_struct *vmlist __initdata;

static __always_inline unsigned long
get_subtree_max_size(struct rb_node *node)
{
	struct vmap_area *va;

	va = rb_entry_safe(node, struct vmap_area, rb_node);
	return va ? va->subtree_max_size : 0;
}

static __always_inline bool
is_within_this_va(struct vmap_area *va, unsigned long size,
	unsigned long align, unsigned long vstart)
{
	unsigned long nva_start_addr;

	if (va->va_start > vstart)
		nva_start_addr = ALIGN(va->va_start, align);
	else
		nva_start_addr = ALIGN(vstart, align);

	/* Can be overflowed due to big size or alignment. */
	if (nva_start_addr + size < nva_start_addr ||
			nva_start_addr < vstart)
		return false;

	return (nva_start_addr + size <= va->va_end);
}

/*
 * Find the first free block(lowest start address) in the tree,
 * that will accomplish the request corresponding to passing
 * parameters. Please note, with an alignment bigger than PAGE_SIZE,
 * a search length is adjusted to account for worst case alignment
 * overhead.
 */
static __always_inline struct vmap_area *
find_vmap_lowest_match(struct rb_root *root, unsigned long size,
	unsigned long align, unsigned long vstart, bool adjust_search_size)
{
	struct vmap_area *va;
	struct rb_node *node;
	unsigned long length;

	/* Start from the root. */
	node = root->rb_node;

	/* Adjust the search size for alignment overhead. */
	length = adjust_search_size ? size + align - 1 : size;

	while (node) {
		va = rb_entry(node, struct vmap_area, rb_node);

		if (get_subtree_max_size(node->rb_left) >= length &&
				vstart < va->va_start) {
			node = node->rb_left;
		} else {
			if (is_within_this_va(va, size, align, vstart))
				return va;

			/*
			 * Does not make sense to go deeper towards the right
			 * sub-tree if it does not have a free block that is
			 * equal or bigger to the requested search length.
			 */
			if (get_subtree_max_size(node->rb_right) >= length) {
				node = node->rb_right;
				continue;
			}

			/*
			 * OK. We roll back and find the first right sub-tree,
			 * that will satisfy the search criteria. It can happen
			 * due to "vstart" restriction or an alignment overhead
			 * that is bigger then PAGE_SIZE.
			 */
			while ((node = rb_parent(node))) {
				va = rb_entry(node, struct vmap_area, rb_node);
				if (is_within_this_va(va, size, align, vstart))
					return va;

				if (get_subtree_max_size(node->rb_right) >= length &&
						vstart <= va->va_start) {
					/*
					 * Shift the vstart forward. Please note, we update it with
					 * parent's start address adding "1" because we do not want
					 * to enter same sub-tree after it has already been checked
					 * and no suitable free block found there.
					 */
					vstart = va->va_start + 1;
					node = node->rb_right;
					break;
				}
			}
		}
	}

	return NULL;
}

enum fit_type {
	NOTHING_FIT = 0,
	FL_FIT_TYPE = 1,	/* full fit */
	LE_FIT_TYPE = 2,	/* left edge fit */
	RE_FIT_TYPE = 3,	/* right edge fit */
	NE_FIT_TYPE = 4		/* no edge fit */
};

static __always_inline enum fit_type
classify_va_fit_type(struct vmap_area *va,
	unsigned long nva_start_addr, unsigned long size)
{
	enum fit_type type;

	/* Check if it is within VA. */
	if (nva_start_addr < va->va_start ||
			nva_start_addr + size > va->va_end)
		return NOTHING_FIT;

	/* Now classify. */
	if (va->va_start == nva_start_addr) {
		if (va->va_end == nva_start_addr + size)
			type = FL_FIT_TYPE;
		else
			type = LE_FIT_TYPE;
	} else if (va->va_end == nva_start_addr + size) {
		type = RE_FIT_TYPE;
	} else {
		type = NE_FIT_TYPE;
	}

	return type;
}

static __always_inline void
__unlink_va(struct vmap_area *va, struct rb_root *root, bool augment)
{
	if (WARN_ON(RB_EMPTY_NODE(&va->rb_node)))
		return;

	if (augment)
		rb_erase_augmented(&va->rb_node,
			root, &free_vmap_area_rb_augment_cb);
	else
		rb_erase(&va->rb_node, root);

	list_del_init(&va->list);
	RB_CLEAR_NODE(&va->rb_node);
}

static __always_inline void
unlink_va_augment(struct vmap_area *va, struct rb_root *root)
{
	__unlink_va(va, root, true);
}

/*
 * This function populates subtree_max_size from bottom to upper
 * levels starting from VA point. The propagation must be done
 * when VA size is modified by changing its va_start/va_end. Or
 * in case of newly inserting of VA to the tree.
 *
 * It means that __augment_tree_propagate_from() must be called:
 * - After VA has been inserted to the tree(free path);
 * - After VA has been shrunk(allocation path);
 * - After VA has been increased(merging path).
 *
 * Please note that, it does not mean that upper parent nodes
 * and their subtree_max_size are recalculated all the time up
 * to the root node.
 *
 *       4--8
 *        /\
 *       /  \
 *      /    \
 *    2--2  8--8
 *
 * For example if we modify the node 4, shrinking it to 2, then
 * no any modification is required. If we shrink the node 2 to 1
 * its subtree_max_size is updated only, and set to 1. If we shrink
 * the node 8 to 6, then its subtree_max_size is set to 6 and parent
 * node becomes 4--6.
 */
static __always_inline void
augment_tree_propagate_from(struct vmap_area *va)
{
	/*
	 * Populate the tree from bottom towards the root until
	 * the calculated maximum available size of checked node
	 * is equal to its current one.
	 */
	free_vmap_area_rb_augment_cb_propagate(&va->rb_node, NULL);

#if DEBUG_AUGMENT_PROPAGATE_CHECK
	augment_tree_propagate_check();
#endif
}

/*
 * This function returns back addresses of parent node
 * and its left or right link for further processing.
 *
 * Otherwise NULL is returned. In that case all further
 * steps regarding inserting of conflicting overlap range
 * have to be declined and actually considered as a bug.
 */
static __always_inline struct rb_node **
find_va_links(struct vmap_area *va,
	struct rb_root *root, struct rb_node *from,
	struct rb_node **parent)
{
	struct vmap_area *tmp_va;
	struct rb_node **link;

	if (root) {
		link = &root->rb_node;
		if (unlikely(!*link)) {
			*parent = NULL;
			return link;
		}
	} else {
		link = &from;
	}

	/*
	 * Go to the bottom of the tree. When we hit the last point
	 * we end up with parent rb_node and correct direction, i name
	 * it link, where the new va->rb_node will be attached to.
	 */
	do {
		tmp_va = rb_entry(*link, struct vmap_area, rb_node);

		/*
		 * During the traversal we also do some sanity check.
		 * Trigger the BUG() if there are sides(left/right)
		 * or full overlaps.
		 */
		if (va->va_end <= tmp_va->va_start)
			link = &(*link)->rb_left;
		else if (va->va_start >= tmp_va->va_end)
			link = &(*link)->rb_right;
		else {
			WARN(1, "vmalloc bug: 0x%lx-0x%lx overlaps with 0x%lx-0x%lx\n",
				va->va_start, va->va_end, tmp_va->va_start, tmp_va->va_end);

			return NULL;
		}
	} while (*link);

	*parent = &tmp_va->rb_node;
	return link;
}

static __always_inline void
__link_va(struct vmap_area *va, struct rb_root *root,
	struct rb_node *parent, struct rb_node **link,
	struct list_head *head, bool augment)
{
	/*
	 * VA is still not in the list, but we can
	 * identify its future previous list_head node.
	 */
	if (likely(parent)) {
		head = &rb_entry(parent, struct vmap_area, rb_node)->list;
		if (&parent->rb_right != link)
			head = head->prev;
	}

	/* Insert to the rb-tree */
	rb_link_node(&va->rb_node, parent, link);
	if (augment) {
		/*
		 * Some explanation here. Just perform simple insertion
		 * to the tree. We do not set va->subtree_max_size to
		 * its current size before calling rb_insert_augmented().
		 * It is because we populate the tree from the bottom
		 * to parent levels when the node _is_ in the tree.
		 *
		 * Therefore we set subtree_max_size to zero after insertion,
		 * to let __augment_tree_propagate_from() puts everything to
		 * the correct order later on.
		 */
		rb_insert_augmented(&va->rb_node,
			root, &free_vmap_area_rb_augment_cb);
		va->subtree_max_size = 0;
	} else {
		rb_insert_color(&va->rb_node, root);
	}

	/* Address-sort this list */
	list_add(&va->list, head);
}

static __always_inline void
link_va_augment(struct vmap_area *va, struct rb_root *root,
	struct rb_node *parent, struct rb_node **link,
	struct list_head *head)
{
	__link_va(va, root, parent, link, head, true);
}

static void
insert_vmap_area_augment(struct vmap_area *va,
	struct rb_node *from, struct rb_root *root,
	struct list_head *head)
{
	struct rb_node **link;
	struct rb_node *parent;

	if (from)
		link = find_va_links(va, NULL, from, &parent);
	else
		link = find_va_links(va, root, NULL, &parent);

	if (link) {
		link_va_augment(va, root, parent, link, head);
		augment_tree_propagate_from(va);
	}
}

struct vmap_area *ne_fit_preload_node;

static __always_inline int
va_clip(struct rb_root *root, struct list_head *head,
		struct vmap_area *va, unsigned long nva_start_addr,
		unsigned long size)
{
	struct vmap_area *lva = NULL;
	enum fit_type type = classify_va_fit_type(va, nva_start_addr, size);

	if (type == FL_FIT_TYPE) {
		/*
		 * No need to split VA, it fully fits.
		 *
		 * |               |
		 * V      NVA      V
		 * |---------------|
		 */
		unlink_va_augment(va, root);
		kfree(va);
	} else if (type == LE_FIT_TYPE) {
		/*
		 * Split left edge of fit VA.
		 *
		 * |       |
		 * V  NVA  V   R
		 * |-------|-------|
		 */
		va->va_start += size;
	} else if (type == RE_FIT_TYPE) {
		/*
		 * Split right edge of fit VA.
		 *
		 *         |       |
		 *     L   V  NVA  V
		 * |-------|-------|
		 */
		va->va_end = nva_start_addr;
	} else if (type == NE_FIT_TYPE) {
		/*
		 * Split no edge of fit VA.
		 *
		 *     |       |
		 *   L V  NVA  V R
		 * |---|-------|---|
		 */
		lva = ne_fit_preload_node;
		ne_fit_preload_node = NULL;
		if (unlikely(!lva)) {
			/*
			 * For percpu allocator we do not do any pre-allocation
			 * and leave it as it is. The reason is it most likely
			 * never ends up with NE_FIT_TYPE splitting. In case of
			 * percpu allocations offsets and sizes are aligned to
			 * fixed align request, i.e. RE_FIT_TYPE and FL_FIT_TYPE
			 * are its main fitting cases.
			 *
			 * There are a few exceptions though, as an example it is
			 * a first allocation (early boot up) when we have "one"
			 * big free space that has to be split.
			 *
			 * Also we can hit this path in case of regular "vmap"
			 * allocations, if "this" current CPU was not preloaded.
			 * See the comment in alloc_vmap_area() why. If so, then
			 * GFP_NOWAIT is used instead to get an extra object for
			 * split purpose. That is rare and most time does not
			 * occur.
			 *
			 * What happens if an allocation gets failed. Basically,
			 * an "overflow" path is triggered to purge lazily freed
			 * areas to free some memory, then, the "retry" path is
			 * triggered to repeat one more time. See more details
			 * in alloc_vmap_area() function.
			 */
			lva = kmalloc(sizeof(struct vmap_area), 0);
			if (!lva)
				return -ENOMEM;
		}

		/*
		 * Build the remainder.
		 */
		lva->va_start = va->va_start;
		lva->va_end = nva_start_addr;

		/*
		 * Shrink this VA to remaining size.
		 */
		va->va_start = nva_start_addr + size;
	} else {
		return -EINVAL;
	}

	if (type != FL_FIT_TYPE) {
		augment_tree_propagate_from(va);

		if (lva)	/* type == NE_FIT_TYPE */
			insert_vmap_area_augment(lva, &va->rb_node, root, head);
	}

	return 0;
}

static unsigned long
va_alloc(struct vmap_area *va,
		struct rb_root *root, struct list_head *head,
		unsigned long size, unsigned long align,
		unsigned long vstart, unsigned long vend)
{
	unsigned long nva_start_addr;
	int ret;

	if (va->va_start > vstart)
		nva_start_addr = ALIGN(va->va_start, align);
	else
		nva_start_addr = ALIGN(vstart, align);

	/* Check the "vend" restriction. */
	if (nva_start_addr + size > vend)
		return -ERANGE;

	/* Update the free vmap_area. */
	ret = va_clip(root, head, va, nva_start_addr, size);
	if (WARN_ON_ONCE(ret))
		return ret;

	return nva_start_addr;
}

/*
 * Returns a start address of the newly allocated area, if success.
 * Otherwise an error value is returned that indicates failure.
 */
static __always_inline unsigned long
__alloc_vmap_area(struct rb_root *root, struct list_head *head,
	unsigned long size, unsigned long align,
	unsigned long vstart, unsigned long vend)
{
	bool adjust_search_size = true;
	unsigned long nva_start_addr;
	struct vmap_area *va;

	/*
	 * Do not adjust when:
	 *   a) align <= PAGE_SIZE, because it does not make any sense.
	 *      All blocks(their start addresses) are at least PAGE_SIZE
	 *      aligned anyway;
	 *   b) a short range where a requested size corresponds to exactly
	 *      specified [vstart:vend] interval and an alignment > PAGE_SIZE.
	 *      With adjusted search length an allocation would not succeed.
	 */
	if (align <= PAGE_SIZE || (align > PAGE_SIZE && (vend - vstart) == size))
		adjust_search_size = false;

	va = find_vmap_lowest_match(root, size, align, vstart, adjust_search_size);
	if (unlikely(!va))
		return -ENOENT;

	nva_start_addr = va_alloc(va, root, head, size, align, vstart, vend);

#if DEBUG_AUGMENT_LOWEST_MATCH_CHECK
	if (!IS_ERR_VALUE(nva_start_addr))
		find_vmap_lowest_match_check(root, head, size, align);
#endif

	return nva_start_addr;
}

static __always_inline void
link_va(struct vmap_area *va, struct rb_root *root,
	struct rb_node *parent, struct rb_node **link,
	struct list_head *head)
{
	__link_va(va, root, parent, link, head, false);
}

static void
insert_vmap_area(struct vmap_area *va,
	struct rb_root *root, struct list_head *head)
{
	struct rb_node **link;
	struct rb_node *parent;

	link = find_va_links(va, root, NULL, &parent);
	if (link)
		link_va(va, root, parent, link, head);
}

/*** Per cpu kva allocator ***/

/*
 * vmap space is limited especially on 32 bit architectures. Ensure there is
 * room for at least 16 percpu vmap blocks per CPU.
 */
/*
 * If we had a constant VMALLOC_START and VMALLOC_END, we'd like to be able
 * to #define VMALLOC_SPACE		(VMALLOC_END-VMALLOC_START). Guess
 * instead (we just need a rough idea)
 */

#define NR_CPUS	 1

#if BITS_PER_LONG == 32
#define VMALLOC_SPACE		(128UL*1024*1024)
#else
#define VMALLOC_SPACE		(128UL*1024*1024*1024)
#endif

#define VMALLOC_PAGES		(VMALLOC_SPACE / PAGE_SIZE)
#define VMAP_MAX_ALLOC		BITS_PER_LONG	/* 256K with 4K pages */
#define VMAP_BBMAP_BITS_MAX	1024	/* 4MB with 4K pages */
#define VMAP_BBMAP_BITS_MIN	(VMAP_MAX_ALLOC*2)
#define VMAP_MIN(x, y)		((x) < (y) ? (x) : (y)) /* can't use min() */
#define VMAP_MAX(x, y)		((x) > (y) ? (x) : (y)) /* can't use max() */
#define VMAP_BBMAP_BITS		\
		VMAP_MIN(VMAP_BBMAP_BITS_MAX,	\
		VMAP_MAX(VMAP_BBMAP_BITS_MIN,	\
			VMALLOC_PAGES / roundup_pow_of_two(NR_CPUS) / 16))

#define VMAP_BLOCK_SIZE		(VMAP_BBMAP_BITS * PAGE_SIZE)

/*
 * Purge threshold to prevent overeager purging of fragmented blocks for
 * regular operations: Purge if vb->free is less than 1/4 of the capacity.
 */
#define VMAP_PURGE_THRESHOLD	(VMAP_BBMAP_BITS / 4)

#define VMAP_RAM		0x1 /* indicates vm_map_ram area*/
#define VMAP_BLOCK		0x2 /* mark out the vmap_block sub-type*/
#define VMAP_FLAGS_MASK		0x3

struct vmap_block_queue {
	struct list_head free;

	/*
	 * An xarray requires an extra memory dynamically to
	 * be allocated. If it is an issue, we can use rb-tree
	 * instead.
	 */
	struct xarray vmap_blocks;
};

struct vmap_block {
	struct vmap_area *va;
	unsigned long free, dirty;
	DECLARE_BITMAP(used_map, VMAP_BBMAP_BITS);
	unsigned long dirty_min, dirty_max; /*< dirty range */
	struct list_head free_list;
	struct rcu_head rcu_head;
	struct list_head purge;
	unsigned int cpu;
};

/* Queue of free and dirty vmap blocks, for allocation and flushing purposes */
static struct vmap_block_queue vmap_block_queue;

static bool purge_fragmented_block(struct vmap_block *vb,
		struct list_head *purge_list, bool force_purge)
{
	if (vb->free + vb->dirty != VMAP_BBMAP_BITS ||
	    vb->dirty == VMAP_BBMAP_BITS)
		return false;

	/* Don't overeagerly purge usable blocks unless requested */
	if (!(force_purge || vb->free < VMAP_PURGE_THRESHOLD))
		return false;

	/* prevent further allocs after releasing lock */
	vb->free = 0;
	/* prevent purging it again */
	vb->dirty = VMAP_BBMAP_BITS;
	vb->dirty_min = 0;
	vb->dirty_max = VMAP_BBMAP_BITS;
	list_del(&vb->free_list);
	list_add_tail(&vb->purge, purge_list);
	return true;
}

/*
 * We should probably have a fallback mechanism to allocate virtual memory
 * out of partially filled vmap blocks. However vmap block sizing should be
 * fairly reasonable according to the vmalloc size, so it shouldn't be a
 * big problem.
 */
static unsigned long addr_to_vb_idx(unsigned long addr)
{
	addr -= VMALLOC_START & ~(VMAP_BLOCK_SIZE-1);
	addr /= VMAP_BLOCK_SIZE;
	return addr;
}

static __always_inline void
unlink_va(struct vmap_area *va, struct rb_root *root)
{
	__unlink_va(va, root, false);
}

/*
 * lazy_max_pages is the maximum amount of virtual address space we gather up
 * before attempting to purge with a TLB flush.
 *
 * There is a tradeoff here: a larger number will cover more kernel page tables
 * and take slightly longer to purge, but it will linearly reduce the number of
 * global TLB flushes that must be performed. It would seem natural to scale
 * this number up linearly with the number of CPUs (because vmapping activity
 * could also scale linearly with the number of CPUs), however it is likely
 * that in practice, workloads might be constrained in other ways that mean
 * vmap activity will not scale linearly with CPUs. Also, I want to be
 * conservative and not introduce a big latency on huge systems, so go with
 * a less aggressive log scale. It will still be an improvement over the old
 * code, and it will be simple to change the scale factor if we find that it
 * becomes a problem on bigger systems.
 */
static unsigned long lazy_max_pages(void)
{
	return 32UL * 1024 * 1024 / PAGE_SIZE;
}

static __always_inline struct list_head *
get_va_next_sibling(struct rb_node *parent, struct rb_node **link)
{
	struct list_head *list;

	if (unlikely(!parent))
		/*
		 * The red-black tree where we try to find VA neighbors
		 * before merging or inserting is empty, i.e. it means
		 * there is no free vmap space. Normally it does not
		 * happen but we handle this case anyway.
		 */
		return NULL;

	list = &rb_entry(parent, struct vmap_area, rb_node)->list;
	return (&parent->rb_right == link ? list->next : list);
}

/*
 * Merge de-allocated chunk of VA memory with previous
 * and next free blocks. If coalesce is not done a new
 * free area is inserted. If VA has been merged, it is
 * freed.
 *
 * Please note, it can return NULL in case of overlap
 * ranges, followed by WARN() report. Despite it is a
 * buggy behaviour, a system can be alive and keep
 * ongoing.
 */
static __always_inline struct vmap_area *
__merge_or_add_vmap_area(struct vmap_area *va,
	struct rb_root *root, struct list_head *head, bool augment)
{
	struct vmap_area *sibling;
	struct list_head *next;
	struct rb_node **link;
	struct rb_node *parent;
	bool merged = false;

	/*
	 * Find a place in the tree where VA potentially will be
	 * inserted, unless it is merged with its sibling/siblings.
	 */
	link = find_va_links(va, root, NULL, &parent);
	if (!link)
		return NULL;

	/*
	 * Get next node of VA to check if merging can be done.
	 */
	next = get_va_next_sibling(parent, link);
	if (unlikely(next == NULL))
		goto insert;

	/*
	 * start            end
	 * |                |
	 * |<------VA------>|<-----Next----->|
	 *                  |                |
	 *                  start            end
	 */
	if (next != head) {
		sibling = list_entry(next, struct vmap_area, list);
		if (sibling->va_start == va->va_end) {
			sibling->va_start = va->va_start;

			/* Free vmap_area object. */
			kfree(va);

			/* Point to the new merged area. */
			va = sibling;
			merged = true;
		}
	}

	/*
	 * start            end
	 * |                |
	 * |<-----Prev----->|<------VA------>|
	 *                  |                |
	 *                  start            end
	 */
	if (next->prev != head) {
		sibling = list_entry(next->prev, struct vmap_area, list);
		if (sibling->va_end == va->va_start) {
			/*
			 * If both neighbors are coalesced, it is important
			 * to unlink the "next" node first, followed by merging
			 * with "previous" one. Otherwise the tree might not be
			 * fully populated if a sibling's augmented value is
			 * "normalized" because of rotation operations.
			 */
			if (merged)
				__unlink_va(va, root, augment);

			sibling->va_end = va->va_end;

			/* Free vmap_area object. */
			kfree(va);

			/* Point to the new merged area. */
			va = sibling;
			merged = true;
		}
	}

insert:
	if (!merged)
		__link_va(va, root, parent, link, head, augment);

	return va;
}

static __always_inline struct vmap_area *
merge_or_add_vmap_area(struct vmap_area *va,
	struct rb_root *root, struct list_head *head)
{
	return __merge_or_add_vmap_area(va, root, head, false);
}

static __always_inline struct vmap_area *
merge_or_add_vmap_area_augment(struct vmap_area *va,
	struct rb_root *root, struct list_head *head)
{
	va = __merge_or_add_vmap_area(va, root, head, true);
	if (va)
		augment_tree_propagate_from(va);

	return va;
}

static void
reclaim_list_global(struct list_head *head)
{
	struct vmap_area *va, *n;

	if (list_empty(head))
		return;

	list_for_each_entry_safe(va, n, head, list)
		merge_or_add_vmap_area_augment(va,
			&free_vmap_area_root, &free_vmap_area_list);
}

static void
decay_va_pool_node(struct vmap_node *vn, bool full_decay)
{
	LIST_HEAD(decay_list);
	struct rb_root decay_root = RB_ROOT;
	struct vmap_area *va, *nva;
	unsigned long n_decay, pool_len;
	int i;

	for (i = 0; i < MAX_VA_SIZE_PAGES; i++) {
		LIST_HEAD(tmp_list);

		if (list_empty(&vn->pool[i].head))
			continue;

		/* Detach the pool, so no-one can access it. */
		list_replace_init(&vn->pool[i].head, &tmp_list);

		pool_len = n_decay = vn->pool[i].len;
		vn->pool[i].len = 0;

		/* Decay a pool by ~25% out of left objects. */
		if (!full_decay)
			n_decay >>= 2;
		pool_len -= n_decay;

		list_for_each_entry_safe(va, nva, &tmp_list, list) {
			if (!n_decay--)
				break;

			list_del_init(&va->list);
			merge_or_add_vmap_area(va, &decay_root, &decay_list);
		}

		/*
		 * Attach the pool back if it has been partly decayed.
		 * Please note, it is supposed that nobody(other contexts)
		 * can populate the pool therefore a simple list replace
		 * operation takes place here.
		 */
		if (!list_empty(&tmp_list)) {
			list_replace_init(&tmp_list, &vn->pool[i].head);
			vn->pool[i].len = pool_len;
		}
	}

	reclaim_list_global(&decay_list);
}

static struct vmap_pool *
size_to_va_pool(struct vmap_node *vn, unsigned long size)
{
	unsigned int idx = (size - 1) / PAGE_SIZE;

	if (idx < MAX_VA_SIZE_PAGES)
		return &vn->pool[idx];

	return NULL;
}

static bool
node_pool_add_va(struct vmap_node *n, struct vmap_area *va)
{
	struct vmap_pool *vp;

	vp = size_to_va_pool(n, va_size(va));
	if (!vp)
		return false;

	list_add(&va->list, &vp->head);
	vp->len = vp->len + 1;

	return true;
}

static void purge_vmap_node(void)
{
	struct vmap_node *vn = &vmap_nodes[0];
	unsigned long nr_purged_pages = 0;
	struct vmap_area *va, *n_va;
	LIST_HEAD(local_list);

	vn->nr_purged = 0;

	list_for_each_entry_safe(va, n_va, &vn->purge_list, list) {
		unsigned long nr = va_size(va) >> PAGE_SHIFT;

		list_del_init(&va->list);

		nr_purged_pages += nr;
		vn->nr_purged++;

		if (!vn->skip_populate)
			if (node_pool_add_va(vn, va))
				continue;

		/* Go back to global. */
		list_add(&va->list, &local_list);
	}

	vmap_lazy_nr -= nr_purged_pages;

	reclaim_list_global(&local_list);
}

/*
 * Purges all lazily-freed vmap areas.
 */
static bool __purge_vmap_area_lazy(unsigned long start, unsigned long end,
		bool full_pool_decay)
{
	unsigned long nr_purged_areas = 0;
	struct vmap_node *vn;

	vn = &vmap_nodes[0];

	INIT_LIST_HEAD(&vn->purge_list);
	vn->skip_populate = full_pool_decay;
	// Release excess idle memory from the pool back
	// to the global free_vmap_area_root and free_vmap_area_list
	decay_va_pool_node(vn, full_pool_decay);

	if (RB_EMPTY_ROOT(&vn->lazy.root))
		return false;

	vn->lazy.root.rb_node = NULL;
	list_replace_init(&vn->lazy.head, &vn->purge_list);

	start = min(start, list_first_entry(&vn->purge_list,
		struct vmap_area, list)->va_start);

	end = max(end, list_last_entry(&vn->purge_list,
		struct vmap_area, list)->va_end);

	asm volatile("tlbi vmalle1is" : : : "memory");

	vn = &vmap_nodes[0];

	purge_vmap_node();

	nr_purged_areas += vn->nr_purged;

	return nr_purged_areas > 0;
}

/*
 * Free a vmap area, caller ensuring that the area has been unmapped,
 * unlinked and flush_cache_vunmap had been called for the correct
 * range previously.
 */
static void free_vmap_area_noflush(struct vmap_area *va)
{
	unsigned long nr_lazy_max = lazy_max_pages();
	struct vmap_node *vn;
	unsigned long nr_lazy;

	if (WARN_ON_ONCE(!list_empty(&va->list)))
		return;

	vmap_lazy_nr += va_size(va) >> PAGE_SHIFT;

	nr_lazy = vmap_lazy_nr;

	/*
	 * If it was request by a certain node we would like to
	 * return it to that node, i.e. its pool for later reuse.
	 */
	vn = &vmap_nodes[0];

	insert_vmap_area(va, &vn->lazy.root, &vn->lazy.head);

	/* After this point, we may free va at any time */
	if (unlikely(nr_lazy > nr_lazy_max))
		__purge_vmap_area_lazy(ULONG_MAX, 0, false);
}

static void free_vmap_block(struct vmap_block *vb)
{
	struct vmap_node *vn;
	struct vmap_block *tmp;
	struct xarray *xa;

	xa = &vmap_block_queue.vmap_blocks;
	tmp = xa_erase(xa, addr_to_vb_idx(vb->va->va_start));
	BUG_ON(tmp != vb);

	vn = vmap_nodes;
	unlink_va(vb->va, &vn->busy.root);

	free_vmap_area_noflush(vb->va);
	kfree(vb);
}

static void free_purged_blocks(struct list_head *purge_list)
{
	struct vmap_block *vb, *n_vb;

	list_for_each_entry_safe(vb, n_vb, purge_list, purge) {
		list_del(&vb->purge);
		free_vmap_block(vb);
	}
}

static void purge_fragmented_blocks(void)
{
	LIST_HEAD(purge);
	struct vmap_block *vb;
	struct vmap_block_queue *vbq = &vmap_block_queue;

	list_for_each_entry(vb, &vbq->free, free_list) {
		unsigned long free = vb->free;
		unsigned long dirty = vb->dirty;

		if (free + dirty != VMAP_BBMAP_BITS ||
		    dirty == VMAP_BBMAP_BITS)
			continue;

		purge_fragmented_block(vb, &purge, true);
	}
	free_purged_blocks(&purge);
}

/*
 * Reclaim vmap areas by purging fragmented blocks and purge_vmap_area_list.
 */
static void reclaim_and_purge_vmap_areas(void)

{
	purge_fragmented_blocks();
	__purge_vmap_area_lazy(ULONG_MAX, 0, true);
}

/*
 * Allocate a region of KVA of the specified size and alignment, within the
 * vstart and vend. If vm is passed in, the two will also be bound.
 */
static struct vmap_area *alloc_vmap_area(unsigned long size,
				unsigned long align,
				unsigned long vstart, unsigned long vend,
				int node, gfp_t gfp_mask,
				unsigned long va_flags, struct vm_struct *vm)
{
	struct vmap_node *vn;
	struct vmap_area *va;
	unsigned long addr;
	unsigned int vn_id = 0;
	int purged = 0;

	if (unlikely(!size || offset_in_page(size) || !is_power_of_2(align)))
		return ERR_PTR(-EINVAL);

	if (unlikely(!vmap_initialized))
		return ERR_PTR(-EBUSY);

	va = kmalloc(sizeof(*va), 0);
	if (unlikely(!va))
		return ERR_PTR(-ENOMEM);


retry:
	addr = __alloc_vmap_area(&free_vmap_area_root, &free_vmap_area_list,
		size, align, vstart, vend);

	/*
	 * If an allocation fails, the error value is
	 * returned. Therefore trigger the overflow path.
	 */
	if (IS_ERR_VALUE(addr))
		goto overflow;

	va->va_start = addr;
	va->va_end = addr + size;
	va->vm = NULL;
	va->flags = (va_flags | vn_id);

	if (vm) {
		vm->addr = (void *)va->va_start;
		vm->size = va_size(va);
		va->vm = vm;	// 把vm关联到va中
	}

	vn = &vmap_nodes[0];

	// Insert va into vn->busy to enable unified management
	// by the vmalloc framework.
	insert_vmap_area(va, &vn->busy.root, &vn->busy.head);

	BUG_ON(!IS_ALIGNED(va->va_start, align));
	BUG_ON(va->va_start < vstart);
	BUG_ON(va->va_end > vend);

	return va;

overflow:
	if (!purged) {
		reclaim_and_purge_vmap_areas();
		purged = 1;
		goto retry;
	}

	kfree(va);
	return ERR_PTR(-EBUSY);
}

struct vm_struct *__get_vm_area_node(unsigned long size,
		unsigned long align, unsigned long shift, unsigned long flags,
		unsigned long start, unsigned long end, int node,
		gfp_t gfp_mask, const void *caller)
{
	struct vmap_area *va;
	struct vm_struct *area;
	unsigned long requested_size = size;

	size = ALIGN(size, 1ul << shift);
	if (unlikely(!size))
		return NULL;

	area = kmalloc(sizeof(*area), 0);
	if (unlikely(!area))
		return NULL;

	if (!(flags & VM_NO_GUARD))
		size += PAGE_SIZE;

	area->flags = flags;
	area->caller = caller;
	area->requested_size = requested_size;

	va = alloc_vmap_area(size, align, start, end, node, gfp_mask, 0, area);
	if (IS_ERR(va)) {
		kfree(area);
		return NULL;
	}

	return area;
}

static void __init vm_struct_add_early(struct vm_struct *vm)
{
	struct vm_struct *tmp, **p;

	BUG_ON(vmap_initialized);
	for (p = &vmlist; (tmp = *p) != NULL; p = &tmp->next) {
		if (tmp->addr >= vm->addr) {
			BUG_ON(tmp->addr < vm->addr + vm->size);
			break;
		}

		BUG_ON(tmp->addr + tmp->size > vm->addr);
	}
	vm->next = *p;
	*p = vm;
}

static void __init declare_vma(struct vm_struct *vma,
			       void *va_start, void *va_end,
			       unsigned long vm_flags)
{
	phys_addr_t pa_start = __virt_to_kimg_phys(va_start);
	unsigned long size = va_end - va_start;

	BUG_ON(!PAGE_ALIGNED(pa_start));
	BUG_ON(!PAGE_ALIGNED(size));

	if (!(vm_flags & VM_NO_GUARD))
		size += PAGE_SIZE;

	vma->addr	= va_start;
	vma->phys_addr	= pa_start;
	vma->size	= size;
	vma->flags	= VM_MAP | vm_flags;
	vma->caller	= __builtin_return_address(0);

	vm_struct_add_early(vma);
}

static void vmap_base_init(void)
{
	struct vmap_node *vn;

	for_each_vmap_node(vn) {
		vn->busy.root = RB_ROOT;
		INIT_LIST_HEAD(&vn->busy.head);

		vn->lazy.root = RB_ROOT;
		INIT_LIST_HEAD(&vn->lazy.head);
	}
}

/*
 * Declare the VMA areas for the kernel
 */
static void __init declare_kernel_vmas(void)
{
	struct vm_struct *tmp;
	static struct vm_struct vmlinux_seg[KERNEL_SEGMENT_COUNT];
	struct vmap_area *va;
	struct vmap_node *vn = &vmap_nodes[0];

	declare_vma(&vmlinux_seg[0], LDSYM_PTR(_stext), LDSYM_PTR(_etext), VM_NO_GUARD);
	declare_vma(&vmlinux_seg[1], LDSYM_PTR(__start_rodata), LDSYM_PTR(__inittext_begin), VM_NO_GUARD);
	declare_vma(&vmlinux_seg[2], LDSYM_PTR(__inittext_begin), LDSYM_PTR(__inittext_end), VM_NO_GUARD);
	declare_vma(&vmlinux_seg[3], LDSYM_PTR(__initdata_begin), LDSYM_PTR(__initdata_end), VM_NO_GUARD);
	declare_vma(&vmlinux_seg[4], LDSYM_PTR(_data), LDSYM_PTR(_end), 0);

	for (tmp = vmlist; tmp; tmp = tmp->next) {
		va = kmalloc(sizeof(struct vmap_area), 0);
		if (WARN_ON_ONCE(!va))
			continue;

		va->va_start = (unsigned long)tmp->addr;
		va->va_end = va->va_start + tmp->size;
		va->vm = tmp;	// struct vm_struct

		// 插入busy中
		insert_vmap_area(va, &vn->busy.root, &vn->busy.head);
	}
}

static void __init vmap_init_free_space(void)
{
	unsigned long vmap_start = 1;
	const unsigned long vmap_end = ULONG_MAX;
	struct vmap_area *free;
	struct vm_struct *busy;

	/*
	 *     B     F     B     B     B     F
	 * -|-----|.....|-----|-----|-----|.....|-
	 *  |           The KVA space           |
	 *  |<--------------------------------->|
	 */
	for (busy = vmlist; busy; busy = busy->next) {
		if ((unsigned long) busy->addr - vmap_start > 0) {
			free = kmalloc(sizeof(struct vmap_area), 0);
			if (!WARN_ON_ONCE(!free)) {
				free->va_start = vmap_start;
				free->va_end = (unsigned long) busy->addr;

				insert_vmap_area_augment(free, NULL,
					&free_vmap_area_root,
						&free_vmap_area_list);
			}
		}

		vmap_start = (unsigned long) busy->addr + busy->size;
	}

	if (vmap_end - vmap_start > 0) {
		free = kmalloc(sizeof(struct vmap_area), 0);
		if (!WARN_ON_ONCE(!free)) {
			free->va_start = vmap_start;
			free->va_end = vmap_end;

			insert_vmap_area_augment(free, NULL,
				&free_vmap_area_root,
					&free_vmap_area_list);
		}
	}
}

void *vmalloc(unsigned long size)
{
	struct vm_struct *vm;
	phys_addr_t pa;
	struct malloc_mem_region *region;
	unsigned long total_size = PAGE_ALIGN(struct_size(region, buf, size));

	vm = __get_vm_area_node(total_size, PAGE_SIZE, PAGE_SHIFT, 0,
			VMALLOC_START, VMALLOC_END, NUMA_NO_NODE, GFP_KERNEL, __builtin_return_address(0));
	if (!vm)
		return NULL;

	pa = memblock_phys_alloc_range(total_size, PAGE_SIZE, 0, 0);

	vmap_range(pa, (unsigned long)vm->addr, total_size, PAGE_KERNEL);

	vm->phys_addr = pa;

	return vm->addr;
}

static struct vmap_area *__find_vmap_area(unsigned long addr, struct rb_root *root)
{
	struct rb_node *n = root->rb_node;

	while (n) {
		struct vmap_area *va;

		va = rb_entry(n, struct vmap_area, rb_node);
		if (addr < va->va_start)
			n = n->rb_left;
		else if (addr >= va->va_end)
			n = n->rb_right;
		else
			return va;
	}

	return NULL;
}

static struct vmap_area *find_unlink_vmap_area(unsigned long addr)
{
	struct vmap_node *vn;
	struct vmap_area *va;

	/*
	 * Check the comment in the find_vmap_area() about the loop.
	 */
	vn = &vmap_nodes[0];

	va = __find_vmap_area(addr, &vn->busy.root);
	if (va)
		unlink_va(va, &vn->busy.root);

	if (va)
		return va;

	return NULL;
}

int pud_clear_huge(pud_t *pudp)
{
	if (!pud_sect(READ_ONCE(*pudp)))
		return 0;
	pud_clear(pudp);
	return 1;
}

static inline int pmd_none_or_clear_bad(pmd_t *pmd)
{
	if (pmd_none(*pmd))
		return 1;
	if (unlikely(pmd_bad(*pmd))) {
		pmd_clear_bad(pmd);
		return 1;
	}
	return 0;
}

static bool __hugetlb_valid_size(unsigned long size)
{
	switch (size) {
#ifndef __PAGETABLE_PMD_FOLDED
	case PUD_SIZE:
		return pud_sect_supported();
#endif
	case CONT_PMD_SIZE:
	case PMD_SIZE:
	case CONT_PTE_SIZE:
		return true;
	}

	return false;
}

static inline int num_contig_ptes(unsigned long size, size_t *pgsize)
{
	int contig_ptes = 1;

	*pgsize = size;

	switch (size) {
	case CONT_PMD_SIZE:
		*pgsize = PMD_SIZE;
		contig_ptes = CONT_PMDS;
		break;
	case CONT_PTE_SIZE:
		*pgsize = PAGE_SIZE;
		contig_ptes = CONT_PTES;
		break;
	default:
		WARN_ON(!__hugetlb_valid_size(size));
	}

	return contig_ptes;
}

/*
 * Changing some bits of contiguous entries requires us to follow a
 * Break-Before-Make approach, breaking the whole contiguous set
 * before we can change any entries. See ARM DDI 0487A.k_iss10775,
 * "Misprogramming of the Contiguous bit", page D4-1762.
 *
 * This helper performs the break step.
 */
static pte_t get_clear_contig(unsigned long addr,
			     pte_t *ptep,
			     unsigned long pgsize,
			     unsigned long ncontig)
{
	pte_t pte, tmp_pte;
	bool present;

	pte = __ptep_get_and_clear_anysz(ptep, pgsize);
	present = pte_present(pte);
	while (--ncontig) {
		ptep++;
		tmp_pte = __ptep_get_and_clear_anysz(ptep, pgsize);
		if (present) {
			if (pte_dirty(tmp_pte))
				pte = pte_mkdirty(pte);
			if (pte_young(tmp_pte))
				pte = pte_mkyoung(pte);
		}
	}
	return pte;
}

pte_t huge_ptep_get_and_clear(unsigned long addr,
			      pte_t *ptep, unsigned long sz)
{
	int ncontig;
	size_t pgsize;

	ncontig = num_contig_ptes(sz, &pgsize);
	return get_clear_contig(addr, ptep, pgsize, ncontig);
}

static inline pte_t *contpte_align_down(pte_t *ptep)
{
	return PTR_ALIGN_DOWN(ptep, sizeof(*ptep) * CONT_PTES);
}

static void contpte_convert(unsigned long addr, pte_t *ptep, pte_t pte)
{
	unsigned long start_addr;
	pte_t *start_ptep;
	int i;

	start_ptep = ptep = contpte_align_down(ptep);
	start_addr = addr = ALIGN_DOWN(addr, CONT_PTE_SIZE);
	pte = pfn_pte(ALIGN_DOWN(pte_pfn(pte), CONT_PTES), pte_pgprot(pte));

	for (i = 0; i < CONT_PTES; i++, ptep++, addr += PAGE_SIZE) {
		pte_t ptent = __ptep_get_and_clear(addr, ptep);

		if (pte_dirty(ptent))
			pte = pte_mkdirty(pte);

		if (pte_young(ptent))
			pte = pte_mkyoung(pte);
	}

	flush_tlb_all();

	__set_ptes(start_addr, start_ptep, pte, CONT_PTES);
}

void contpte_set_ptes(unsigned long addr,
					pte_t *ptep, pte_t pte, unsigned int nr)
{
	unsigned long next;
	unsigned long end;
	unsigned long pfn;
	pgprot_t prot;

	/*
	 * The set_ptes() spec guarantees that when nr > 1, the initial state of
	 * all ptes is not-present. Therefore we never need to unfold or
	 * otherwise invalidate a range before we set the new ptes.
	 * contpte_set_ptes() should never be called for nr < 2.
	 */
	VM_WARN_ON(nr == 1);

	return __set_ptes(addr, ptep, pte, nr);

	end = addr + (nr << PAGE_SHIFT);
	pfn = pte_pfn(pte);
	prot = pte_pgprot(pte);

	do {
		next = pte_cont_addr_end(addr, end);
		nr = (next - addr) >> PAGE_SHIFT;
		pte = pfn_pte(pfn, prot);

		if (((addr | next | (pfn << PAGE_SHIFT)) & ~CONT_PTE_MASK) == 0)
			pte = pte_mkcont(pte);
		else
			pte = pte_mknoncont(pte);

		__set_ptes(addr, ptep, pte, nr);

		addr = next;
		ptep += nr;
		pfn += nr;

	} while (addr != end);
}

void __contpte_try_unfold(unsigned long addr,
			pte_t *ptep, pte_t pte)
{
	pte = pte_mknoncont(pte);
	contpte_convert(addr, ptep, pte);
}

static void vunmap_pte_range(pmd_t *pmd, unsigned long addr, unsigned long end,
			     pgtbl_mod_mask *mask)
{
	pte_t *pte;
	pte_t ptent;
	unsigned long size = PAGE_SIZE;

	pte = pte_offset_kernel(pmd, addr);

	do {
#ifdef CONFIG_HUGETLB_PAGE
		size = arch_vmap_pte_range_unmap_size(addr, pte);
		if (size != PAGE_SIZE) {
			if (WARN_ON(!IS_ALIGNED(addr, size))) {
				addr = ALIGN_DOWN(addr, size);
				pte = PTR_ALIGN_DOWN(pte, sizeof(*pte) * (size >> PAGE_SHIFT));
			}
			ptent = huge_ptep_get_and_clear(addr, pte, size);
			if (WARN_ON(end - addr < size))
				size = end - addr;
		} else
#endif
			ptent = ptep_get_and_clear(addr, pte);
		WARN_ON(!pte_none(ptent) && !pte_present(ptent));
	} while (pte += (size >> PAGE_SHIFT), addr += size, addr != end);

	*mask |= PGTBL_PTE_MODIFIED;
}

static void vunmap_pmd_range(pud_t *pud, unsigned long addr, unsigned long end,
			     pgtbl_mod_mask *mask)
{
	pmd_t *pmd;
	unsigned long next;
	int cleared;

	pmd = pmd_offset(pud, addr);
	do {
		next = pmd_addr_end(addr, end);

		cleared = pmd_clear_huge(pmd);
		if (cleared || pmd_bad(*pmd))
			*mask |= PGTBL_PMD_MODIFIED;

		if (cleared) {
			WARN_ON(next - addr < PMD_SIZE);
			continue;
		}
		if (pmd_none_or_clear_bad(pmd))
			continue;
		vunmap_pte_range(pmd, addr, next, mask);
	} while (pmd++, addr = next, addr != end);
}

static void vunmap_pud_range(pgd_t *pgd, unsigned long addr, unsigned long end,
			     pgtbl_mod_mask *mask)
{
	pud_t *pud;
	unsigned long next;

	pud = pud_offset(pgd, addr);
	do {
		next = pud_addr_end(addr, end);

		pud_clear_huge(pud);
		if (pud_bad(*pud))
			*mask |= PGTBL_PUD_MODIFIED;

		if (pud_none_or_clear_bad(pud))
			continue;
		vunmap_pmd_range(pud, addr, next, mask);
	} while (pud++, addr = next, addr != end);
}

/*
 * vunmap_range_noflush is similar to vunmap_range, but does not
 * flush caches or TLBs.
 *
 * The caller is responsible for calling flush_cache_vmap() before calling
 * this function, and flush_tlb_kernel_range after it has returned
 * successfully (and before the addresses are expected to cause a page fault
 * or be re-mapped for something else, if TLB flushes are being delayed or
 * coalesced).
 *
 * This is an internal function only. Do not use outside mm/.
 */
void __vunmap_range_noflush(unsigned long start, unsigned long end)
{
	unsigned long next;
	pgd_t *pgd;
	unsigned long addr = start;
	pgtbl_mod_mask mask = 0;

	BUG_ON(addr >= end);
	pgd = pgd_offset_k(addr);
	do {
		next = pgd_addr_end(addr, end);
		if (pgd_bad(*pgd))
			mask |= PGTBL_PGD_MODIFIED;
		if (pgd_none_or_clear_bad(pgd))
			continue;
		vunmap_pud_range(pgd, addr, next, &mask);
	} while (pgd++, addr = next, addr != end);
}

/*
 * Free and unmap a vmap area
 */
static void free_unmap_vmap_area(struct vmap_area *va)
{
	__vunmap_range_noflush(va->va_start, va->va_end);

	free_vmap_area_noflush(va);
}

/**
 * remove_vm_area - find and remove a continuous kernel virtual area
 * @addr:	    base address
 *
 * Search for the kernel VM area starting at @addr, and remove it.
 * This function returns the found VM area, but using it is NOT safe
 * on SMP machines, except for its size or flags.
 *
 * Return: the area descriptor on success or %NULL on failure.
 */
struct vm_struct *remove_vm_area(const void *addr)
{
	struct vmap_area *va;
	struct vm_struct *vm;

	if (WARN(!PAGE_ALIGNED(addr), "Trying to vfree() bad address (%p)\n",
			addr))
		return NULL;

	va = find_unlink_vmap_area((unsigned long)addr);
	if (!va || !va->vm)
		return NULL;
	vm = va->vm;

	free_unmap_vmap_area(va);
	return vm;
}

/**
 * vfree - Release memory allocated by vmalloc()
 * @addr:  Memory base address
 *
 * Free the virtually continuous memory area starting at @addr, as obtained
 * from one of the vmalloc() family of APIs.  This will usually also free the
 * physical memory underlying the virtual allocation, but that memory is
 * reference counted, so it will not be freed until the last user goes away.
 *
 * If @addr is NULL, no operation is performed.
 *
 * Context:
 * May sleep if called *not* from interrupt context.
 * Must not be called in NMI context (strictly speaking, it could be
 * if we have CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG, but making the calling
 * conventions for vfree() arch-dependent would be a really bad idea).
 */
void vfree(const void *addr)
{
	struct vm_struct *vm;

	if (!addr)
		return;

	vm = remove_vm_area(addr);
	if (unlikely(!vm)) {
		WARN(1, KERN_ERR "Trying to vfree() nonexistent vm area (%p)\n",
				addr);
		return;
	}
	memblock_phys_free(vm->phys_addr, vm->size);
	kfree(vm);
}

static inline void pgd_populate(pgd_t *pgdp, pud_t *pudp)
{
	pgdval_t pgdval = PGD_TYPE_TABLE | PGD_TABLE_AF;

	__pgd_populate(pgdp, __pa(pudp), pgdval);
}

static int mm_record_pgtable(struct memory_struct *mm, void *table)
{
	struct pgtable_mem *pgtable_mem;

	if (!mm)
		return 0;

	pgtable_mem = kmalloc(sizeof(struct pgtable_mem), 0);
	if (!pgtable_mem)
		return -ENOMEM;

	pgtable_mem->virt = table;
	pgtable_mem->next = mm->pgtable_mem;
	mm->pgtable_mem = pgtable_mem;

	return 0;
}

static inline pud_t *pud_alloc_one(struct memory_struct *mm)
{
	pud_t *new;

	new = kmalloc(PAGE_SIZE, __GFP_ZERO);
	if (!new)
		return NULL;

	if (mm_record_pgtable(mm, new)) {
		kfree(new);
		return NULL;
	}

	if (!PAGE_ALIGNED(new))
		new = (pud_t *)PAGE_ALIGN_UP(new);

	return new;
}

/*
 * Allocate page upper directory.
 * We've already handled the fast-path in-line.
 */
int __pud_alloc(struct memory_struct *mm, pgd_t *pgd, unsigned long address)
{
	pud_t *new;

	if (!pgd_present(*pgd)) {
		new = pud_alloc_one(mm);
		if (!new)
			return -ENOMEM;
		smp_wmb(); /* See comment in pmd_install() */
		pgd_populate(pgd, new);
	}

	return 0;
}

static inline pud_t *pud_alloc_track(struct memory_struct *mm, pgd_t *pgd, unsigned long address)
{
	if (unlikely(pgd_none(*pgd))) {
		if (__pud_alloc(mm, pgd, address))
			return NULL;
	}

	return pud_offset(pgd, address);
}

int pmd_free_pte_page(pmd_t *pmdp, unsigned long addr)
{
	pte_t *table;
	pmd_t pmd;

	pmd = READ_ONCE(*pmdp);

	if (!pmd_table(pmd)) {
		VM_WARN_ON(1);
		return 1;
	}

	table = pte_offset_kernel(pmdp, addr);
	pmd_clear(pmdp);
	__flush_tlb_kernel_pgtable(addr);
	pte_free_kernel(table);
	return 1;
}

int pud_free_pmd_page(pud_t *pudp, unsigned long addr)
{
	pmd_t *table;
	pmd_t *pmdp;
	pud_t pud;
	unsigned long next, end;

	pud = READ_ONCE(*pudp);

	if (!pud_table(pud)) {
		VM_WARN_ON(1);
		return 1;
	}

	table = pmd_offset(pudp, addr);
	pmdp = table;
	next = addr;
	end = addr + PMD_SIZE;
	do {
		if (pmd_present(pmdp_get(pmdp)))
			pmd_free_pte_page(pmdp, next);
	} while (pmdp++, next += PMD_SIZE, next != end);

	pud_clear(pudp);
	__flush_tlb_kernel_pgtable(addr);
	pmd_free(table);
	return 1;
}

extern int pud_set_huge(pud_t *pudp, phys_addr_t phys, pgprot_t prot);
static int vmap_try_huge_pud(pud_t *pud, unsigned long addr, unsigned long end,
			phys_addr_t phys_addr, pgprot_t prot)
{
	if ((end - addr) != PUD_SIZE)
		return 0;

	if (!IS_ALIGNED(addr, PUD_SIZE))
		return 0;

	if (!IS_ALIGNED(phys_addr, PUD_SIZE))
		return 0;

	if (pud_present(*pud) && !pud_free_pmd_page(pud, addr))
		return 0;

	return pud_set_huge(pud, phys_addr, prot);
}

static inline pmd_t *pmd_alloc_one(struct memory_struct *mm)
{
	pmd_t *new;

	new = kmalloc(PAGE_SIZE, __GFP_ZERO);
	if (!new)
		return NULL;

	if (mm_record_pgtable(mm, new)) {
		kfree(new);
		return NULL;
	}

	if (!PAGE_ALIGNED(new))
		new = (pmd_t *)PAGE_ALIGN_UP(new);

	return new;
}

static inline void pud_populate(pud_t *pudp, pmd_t *pmdp)
{
	pudval_t pudval = PUD_TYPE_TABLE | PUD_TABLE_AF;

	__pud_populate(pudp, __pa(pmdp), pudval);
}

/*
 * Allocate page upper directory.
 * We've already handled the fast-path in-line.
 */
int __pmd_alloc(struct memory_struct *mm, pud_t *pud, unsigned long address)
{
	pmd_t *new;

	if (!pud_present(*pud)) {
		new = pmd_alloc_one(mm);
		if (!new)
			return -ENOMEM;
		smp_wmb(); /* See comment in pmd_install() */
		pud_populate(pud, new);
	}

	return 0;
}

static inline pte_t *pte_alloc_one(struct memory_struct *mm)
{
	pte_t *new;

	new = kmalloc(PAGE_SIZE, __GFP_ZERO);
	if (!new)
		return NULL;

	if (mm_record_pgtable(mm, new)) {
		kfree(new);
		return NULL;
	}

	if (!PAGE_ALIGNED(new))
		new = (pte_t *)PAGE_ALIGN_UP(new);

	return new;
}

static inline void
pmd_populate(pmd_t *pmdp, pte_t *ptep)
{
	__pmd_populate(pmdp, __pa(ptep),
		       PMD_TYPE_TABLE | PMD_TABLE_AF | PMD_TABLE_PXN);
}

/*
 * Allocate page upper directory.
 * We've already handled the fast-path in-line.
 */
int __pte_alloc(struct memory_struct *mm, pmd_t *pmd, unsigned long address)
{
	pte_t *new;

	if (!pmd_present(*pmd)) {
		new = pte_alloc_one(mm);
		if (!new)
			return -ENOMEM;
		smp_wmb(); /* See comment in pmd_install() */
		pmd_populate(pmd, new);
	}

	return 0;
}

static inline pmd_t *pmd_alloc_track(struct memory_struct *mm, pud_t *pud, unsigned long address)
{
	if (unlikely(pud_none(*pud))) {
		if (__pmd_alloc(mm, pud, address))
			return NULL;
	}

	return pmd_offset(pud, address);
}

extern int pmd_set_huge(pmd_t *pmdp, phys_addr_t phys, pgprot_t prot);
static int vmap_try_huge_pmd(pmd_t *pmd, unsigned long addr, unsigned long end,
			phys_addr_t phys_addr, pgprot_t prot)
{
	if ((end - addr) != PMD_SIZE)
		return 0;

	if (!IS_ALIGNED(addr, PMD_SIZE))
		return 0;

	if (!IS_ALIGNED(phys_addr, PMD_SIZE))
		return 0;

	if (pmd_present(*pmd) && !pmd_free_pte_page(pmd, addr))
		return 0;

	return pmd_set_huge(pmd, phys_addr, prot);
}

static inline pte_t *pte_alloc_kernel_track(struct memory_struct *mm, pmd_t *pmd, unsigned long address)
{
	if (unlikely(pmd_none(*pmd))) {
		if (__pte_alloc(mm, pmd, address))
			return NULL;
	}

	return pte_offset_kernel(pmd, address);
}

pte_t contpte_ptep_get(pte_t *ptep, pte_t orig_pte)
{
	/*
	 * Gather access/dirty bits, which may be populated in any of the ptes
	 * of the contig range. We are guaranteed to be holding the PTL, so any
	 * contiguous range cannot be unfolded or otherwise modified under our
	 * feet.
	 */

	pte_t pte;
	int i;

	ptep = contpte_align_down(ptep);

	for (i = 0; i < CONT_PTES; i++, ptep++) {
		pte = __ptep_get(ptep);

		if (pte_dirty(pte)) {
			orig_pte = pte_mkdirty(orig_pte);
			for (; i < CONT_PTES; i++, ptep++) {
				pte = __ptep_get(ptep);
				if (pte_young(pte)) {
					orig_pte = pte_mkyoung(orig_pte);
					break;
				}
			}
			break;
		}

		if (pte_young(pte)) {
			orig_pte = pte_mkyoung(orig_pte);
			i++;
			ptep++;
			for (; i < CONT_PTES; i++, ptep++) {
				pte = __ptep_get(ptep);
				if (pte_dirty(pte)) {
					orig_pte = pte_mkdirty(orig_pte);
					break;
				}
			}
			break;
		}
	}

	return orig_pte;
}

pte_t arch_make_huge_pte(pte_t entry, unsigned int pagesize)
{
	switch (pagesize) {
#ifndef __PAGETABLE_PMD_FOLDED
	case PUD_SIZE:
		if (pud_sect_supported())
			return pud_pte(pud_mkhuge(pte_pud(entry)));
		break;
#endif
	case CONT_PMD_SIZE:
		return pmd_pte(pmd_mkhuge(pmd_mkcont(pte_pmd(entry))));
	case PMD_SIZE:
		return pmd_pte(pmd_mkhuge(pte_pmd(entry)));
	case CONT_PTE_SIZE:
		return pte_mkcont(entry);
	default:
		break;
	}
	pr_warn("%s: unrecognized huge page size 0x%lx\n",
		__func__, pagesize);
	return entry;
}

/*
 * Changing some bits of contiguous entries requires us to follow a
 * Break-Before-Make approach, breaking the whole contiguous set
 * before we can change any entries. See ARM DDI 0487A.k_iss10775,
 * "Misprogramming of the Contiguous bit", page D4-1762.
 *
 * This helper performs the break step for use cases where the
 * original pte is not needed.
 */
static void clear_flush(unsigned long addr, pte_t *ptep,
						unsigned long pgsize, unsigned long ncontig)
{
	unsigned long i;

	for (i = 0; i < ncontig; i++, addr += pgsize, ptep++)
		__ptep_get_and_clear_anysz(ptep, pgsize);

	flush_tlb_all();
}

void __contpte_try_fold(unsigned long addr, pte_t *ptep, pte_t pte)
{
	/*
	 * We have already checked that the virtual and pysical addresses are
	 * correctly aligned for a contpte mapping in contpte_try_fold() so the
	 * remaining checks are to ensure that the contpte range is fully
	 * covered by a single folio, and ensure that all the ptes are valid
	 * with contiguous PFNs and matching prots. We ignore the state of the
	 * access and dirty bits for the purpose of deciding if its a contiguous
	 * range; the folding process will generate a single contpte entry which
	 * has a single access and dirty bit. Those 2 bits are the logical OR of
	 * their respective bits in the constituent pte entries. In order to
	 * ensure the contpte range is covered by a single folio, we must
	 * recover the folio from the pfn, but special mappings don't have a
	 * folio backing them. Fortunately contpte_try_fold() already checked
	 * that the pte is not special - we never try to fold special mappings.
	 * Note we can't use vm_normal_page() for this since we don't have the
	 * vma.
	 */

	pte_t expected_pte, subpte;
	unsigned long pfn;
	pte_t *orig_ptep;
	pgprot_t prot;

	int i;

	pfn = ALIGN_DOWN(pte_pfn(pte), CONT_PTES);
	prot = pte_pgprot(pte_mkold(pte_mkclean(pte)));
	expected_pte = pfn_pte(pfn, prot);
	orig_ptep = ptep;
	ptep = contpte_align_down(ptep);

	for (i = 0; i < CONT_PTES; i++) {
		subpte = pte_mkold(pte_mkclean(__ptep_get(ptep)));
		if (!pte_same(subpte, expected_pte))
			return;
		expected_pte = pte_advance_pfn(expected_pte, 1);
		ptep++;
	}

	pte = pte_mkcont(pte);
	contpte_convert(addr, orig_ptep, pte);
}

void set_huge_pte_at(unsigned long addr, pte_t *ptep, pte_t pte, unsigned long sz)
{
	size_t pgsize;
	int i;
	int ncontig;

	ncontig = num_contig_ptes(sz, &pgsize);

	if (!pte_present(pte)) {
		for (i = 0; i < ncontig; i++, ptep++)
			__set_ptes_anysz(ptep, pte, 1, pgsize);
		return;
	}

	/* Only need to "break" if transitioning valid -> valid. */
	if (pte_cont(pte) && pte_valid(__ptep_get(ptep)))
		clear_flush(addr, ptep, pgsize, ncontig);

	__set_ptes_anysz(ptep, pte, ncontig, pgsize);
}

/*** Page table manipulation functions ***/
static int vmap_pte_range(struct memory_struct *mm, pmd_t *pmd, unsigned long addr, unsigned long end,
			phys_addr_t phys_addr, pgprot_t prot)
{
	pte_t *pte;
	u64 pfn;
	unsigned long size = PAGE_SIZE;

	pfn = phys_addr >> PAGE_SHIFT;
	pte = pte_alloc_kernel_track(mm, pmd, addr);
	if (!pte)
		return -ENOMEM;

	do {
		if (unlikely(!pte_none(ptep_get(pte))))
			BUG();

#ifdef CONFIG_HUGETLB_PAGE
		size = arch_vmap_pte_range_map_size(addr, end, pfn);
		if (size != PAGE_SIZE) {
			pte_t entry = pfn_pte(pfn, prot);

			entry = arch_make_huge_pte(entry, ilog2(size));
			set_huge_pte_at(addr, pte, entry, size);
			pfn += PFN_DOWN(size);
			continue;
		}
#endif
		set_pte_at(addr, pte, pfn_pte(pfn, prot));
		pfn++;
	} while (pte += PFN_DOWN(size), addr += size, addr < end);

	return 0;
}

static int vmap_pmd_range(struct memory_struct *mm, pud_t *pud, unsigned long addr, unsigned long end,
			phys_addr_t phys_addr, pgprot_t prot)
{
	pmd_t *pmd;
	unsigned long next;

	pmd = pmd_alloc_track(mm, pud, addr);
	if (!pmd)
		return -ENOMEM;
	do {
		next = pmd_addr_end(addr, end);

		if (vmap_try_huge_pmd(pmd, addr, next, phys_addr, prot))
			continue;

		if (vmap_pte_range(mm, pmd, addr, next, phys_addr, prot))
			return -ENOMEM;
	} while (pmd++, phys_addr += (next - addr), addr = next, addr != end);
	return 0;
}

static int vmap_pud_phys_range(struct memory_struct *mm, pgd_t *pgd, unsigned long addr, unsigned long end,
			phys_addr_t phys_addr, pgprot_t prot)
{
	pud_t *pud;
	unsigned long next;

	pud = pud_alloc_track(mm, pgd, addr);
	if (!pud)
		return -ENOMEM;
	do {
		next = pud_addr_end(addr, end);

		if (vmap_try_huge_pud(pud, addr, next, phys_addr, prot))
			continue;

		if (vmap_pmd_range(mm, pud, addr, next, phys_addr, prot))
			return -ENOMEM;
	} while (pud++, phys_addr += (next - addr), addr = next, addr != end);
	return 0;
}

/*
 * vmap_pages_range_noflush is similar to vmap_pages_range, but does not
 * flush caches.
 *
 * The caller is responsible for calling flush_cache_vmap() after this
 * function returns successfully and before the addresses are accessed.
 *
 * This is an internal function only. Do not use outside mm/.
 */
int vmap_phys_range_noflush(struct memory_struct *mm, phys_addr_t phys_addr, unsigned long virt,
							unsigned long size, pgprot_t prot)
{
	unsigned long start, end, next;
	pgd_t *pgd;
	int err;


	start = virt;
	end = start + size;
	pgd = mm->pg_dir;
	do {
		next = pgd_addr_end(virt, end);
		err = vmap_pud_phys_range(mm, pgd, virt, next, phys_addr, prot);
		if (err)
			break;
	} while (pgd++, phys_addr += (next - virt), virt = next, virt != end);

	return err;
}


static void vmap_init(void)
{
	vmap_base_init();
	declare_kernel_vmas();
	vmap_init_free_space();
	vmap_initialized = true;
}

void __init vmalloc_init(void)
{
	vmap_init();
}
