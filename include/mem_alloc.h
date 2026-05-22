/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __MALLOC_H__
#define __MALLOC_H__

#include <linux/types.h>
#include <linux/compiler_types.h>

struct malloc_mem_region {
	u64 buf_size;
	struct vm_struct *vm;
	u8 buf[] __counted_by(buf_size);
};

#endif /* __MALLOC_H__ */