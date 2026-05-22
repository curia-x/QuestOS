// SPDX-License-Identifier: GPL-2.0
#ifndef __VDSO_LIMITS_H
#define __VDSO_LIMITS_H

#ifndef USHRT_MAX
#define USHRT_MAX	((unsigned short)~0U)
#endif

#ifndef SHRT_MAX
#define SHRT_MAX	((short)(USHRT_MAX >> 1))
#endif

#ifndef SHRT_MIN
#define SHRT_MIN	((short)(-SHRT_MAX - 1))
#endif

#ifndef INT_MAX
#define INT_MAX		((int)(~0U >> 1))
#endif

#ifndef INT_MIN
#define INT_MIN		(-INT_MAX - 1)
#endif

#ifndef UINT_MAX
#define UINT_MAX	(~0U)
#endif

#ifndef LONG_MAX
#define LONG_MAX	((long)(~0UL >> 1))
#endif

#ifndef LONG_MIN
#define LONG_MIN	(-LONG_MAX - 1)
#endif

#ifndef ULONG_MAX
#define ULONG_MAX	(~0UL)
#endif

#ifndef LLONG_MAX
#define LLONG_MAX	((long long)(~0ULL >> 1))
#endif

#ifndef LLONG_MIN
#define LLONG_MIN	(-LLONG_MAX - 1)
#endif

#ifndef ULLONG_MAX
#define ULLONG_MAX	(~0ULL)
#endif

#ifndef UINTPTR_MAX
#define UINTPTR_MAX	ULONG_MAX
#endif

#endif /* __VDSO_LIMITS_H */
