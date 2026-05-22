/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_NUMA_H
#define _LINUX_NUMA_H
#include <linux/init.h>
#include <linux/types.h>
#include <linux/nodemask_types.h>

#define	NUMA_NO_MEMBLK	(-1)

static inline bool numa_valid_node(int nid)
{
	return nid >= 0 && nid < MAX_NUMNODES;
}

#endif /* _LINUX_NUMA_H */
