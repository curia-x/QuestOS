/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef _LIMITS_H
#define _LIMITS_H

#define SCHAR_MAX   __SCHAR_MAX__
#define SCHAR_MIN   (-SCHAR_MAX - 1)
#define UCHAR_MAX   (SCHAR_MAX * 2 + 1)

#ifdef __CHAR_UNSIGNED__
#define CHAR_MAX    UCHAR_MAX
#define CHAR_MIN    0
#else
#define CHAR_MAX    SCHAR_MAX
#define CHAR_MIN    SCHAR_MIN
#endif

#ifndef SHRT_MAX
#define SHRT_MAX    __SHRT_MAX__
#endif
#ifndef SHRT_MIN
#define SHRT_MIN    (-SHRT_MAX - 1)
#endif
#ifndef USHRT_MAX
#define USHRT_MAX   (SHRT_MAX * 2 + 1)
#endif

#ifndef INT_MAX
#define INT_MAX     __INT_MAX__
#endif
#ifndef INT_MIN
#define INT_MIN     (-INT_MAX - 1)
#endif
#ifndef UINT_MAX
#define UINT_MAX    (INT_MAX * 2U + 1U)
#endif

#ifndef LONG_MAX
#define LONG_MAX    __LONG_MAX__
#endif
#ifndef LONG_MIN
#define LONG_MIN    (-LONG_MAX - 1L)
#endif
#ifndef ULONG_MAX
#define ULONG_MAX   (LONG_MAX * 2UL + 1UL)
#endif

#ifndef LLONG_MAX
#define LLONG_MAX   __LONG_LONG_MAX__
#endif
#ifndef LLONG_MIN
#define LLONG_MIN   (-LLONG_MAX - 1LL)
#endif
#ifndef ULLONG_MAX
#define ULLONG_MAX  (LLONG_MAX * 2ULL + 1ULL)
#endif

#ifndef U8_MAX
#define U8_MAX      UCHAR_MAX
#endif
#ifndef S8_MAX
#define S8_MAX      SCHAR_MAX
#endif
#ifndef S8_MIN
#define S8_MIN      SCHAR_MIN
#endif
#ifndef U16_MAX
#define U16_MAX     USHRT_MAX
#endif
#ifndef S16_MAX
#define S16_MAX     SHRT_MAX
#endif
#ifndef S16_MIN
#define S16_MIN     SHRT_MIN
#endif
#ifndef U32_MAX
#define U32_MAX     UINT_MAX
#endif
#ifndef S32_MAX
#define S32_MAX     INT_MAX
#endif
#ifndef S32_MIN
#define S32_MIN     INT_MIN
#endif
#ifndef U64_MAX
#define U64_MAX     ULLONG_MAX
#endif
#ifndef S64_MAX
#define S64_MAX     LLONG_MAX
#endif
#ifndef S64_MIN
#define S64_MIN     LLONG_MIN
#endif

#define UINT8_MAX   U8_MAX
#define INT8_MAX    S8_MAX
#define INT8_MIN    S8_MIN
#define UINT16_MAX  U16_MAX
#define INT16_MAX   S16_MAX
#define INT16_MIN   S16_MIN
#define UINT32_MAX  U32_MAX
#define INT32_MAX   S32_MAX
#define INT32_MIN   S32_MIN
#define UINT64_MAX  U64_MAX
#define INT64_MAX   S64_MAX
#define INT64_MIN   S64_MIN

#define CHAR_BIT    8

#ifndef UINTPTR_MAX
#if (defined(CONFIG_64BIT) && !defined(CONFIG_SPL_BUILD)) || \
	(defined(CONFIG_SPL_64BIT) && defined(CONFIG_SPL_BUILD))
    #define UINTPTR_MAX UINT64_MAX
#else
    #define UINTPTR_MAX UINT32_MAX
#endif
#endif

#ifndef SIZE_MAX
#define SIZE_MAX    UINTPTR_MAX
#endif
#ifndef SSIZE_MAX
#define SSIZE_MAX   ((ssize_t)(SIZE_MAX >> 1))
#endif

#endif /* _LIMITS_H */
