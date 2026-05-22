// SPDX-License-Identifier: GPL-2.0-only
#ifndef __INIT_TASK_H__
#define __INIT_TASK_H__

/* Attach to the thread_info data structure for proper alignment */
#define __init_thread_info __section(".data..init_thread_info")

#endif /* __INIT_TASK_H__ */