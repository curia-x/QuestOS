/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __KMALLOC_H__
#define __KMALLOC_H__

#include <linux/types.h>
#include <linux/overflow.h>
#include <gfp.h>

void kmalloc_set_ready(void);

void *kmalloc(size_t size, gfp_t flags);
void kfree(const void *ptr);

static inline void *kzalloc(size_t size, gfp_t flags)
{
	return kmalloc(size, flags | __GFP_ZERO);
}

/**
 * kmalloc_array - allocate memory for an array.
 * @n: number of elements.
 * @size: element size.
 * @flags: the type of memory to allocate (see kmalloc).
 */
static inline void *kmalloc_array(size_t n, size_t size, gfp_t flags)
{
	size_t bytes;

	if (unlikely(check_mul_overflow(n, size, &bytes)))
		return NULL;
	return kmalloc(bytes, flags);
}

#endif /* __KMALLOC_H__ */