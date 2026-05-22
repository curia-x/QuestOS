/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __VDSO_PAGE_H
#define __VDSO_PAGE_H

#include <uapi/linux/const.h>

/*
 * PAGE_SHIFT determines the page size.
 *
 * Note: This definition is required because PAGE_SHIFT is used
 * in several places throughout the codebase.
 */
#define PAGE_SHIFT      CONFIG_PAGE_SHIFT

#define PAGE_SIZE	(_AC(1,UL) << CONFIG_PAGE_SHIFT)

#define PAGE_MASK	(~(PAGE_SIZE - 1))

#endif	/* __VDSO_PAGE_H */
