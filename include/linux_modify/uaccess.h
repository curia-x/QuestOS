/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __LINUX_UACCESS_H__
#define __LINUX_UACCESS_H__

#include <linux/string.h>
#include <linux/compiler_attributes.h>
#include <linux/compiler_types.h>
#include <vdso/bits.h>
#include <ucopysize.h>
#include <memory.h>

static inline int __access_ok(const void __user *ptr, unsigned long size)
{
	unsigned long limit = UL(1) << VA_BITS;
	unsigned long addr = (unsigned long)ptr;

	return (size <= limit) && (addr <= (limit - size));
}

static inline int access_ok(const void __user *addr, unsigned long size)
{
	return likely(__access_ok(addr, size));
}
#define access_ok access_ok

static inline bool uaccess_ttbr0_disable(void)
{
	return false;
}

static inline bool uaccess_ttbr0_enable(void)
{
	return false;
}

#define uaccess_mask_ptr(ptr) (__typeof__(ptr))__uaccess_mask_ptr(ptr)
static inline void __user *__uaccess_mask_ptr(const void __user *ptr)
{
	void __user *safe_ptr;

	asm volatile(
	"	bic	%0, %1, %2\n"
	: "=r" (safe_ptr)
	: "r" (ptr),
	  "i" (BIT(55))
	);

	return safe_ptr;
}

extern unsigned long __must_check __arch_copy_from_user(void *to, const void __user *from, unsigned long n);
#define raw_copy_from_user(to, from, n)					\
({									\
	unsigned long __acfu_ret;					\
	uaccess_ttbr0_enable();						\
	__acfu_ret = __arch_copy_from_user((to),			\
				      __uaccess_mask_ptr(from), (n));	\
	uaccess_ttbr0_disable();					\
	__acfu_ret;							\
})

/*
 * Architectures that #define INLINE_COPY_TO_USER use this function
 * directly in the normal copy_to/from_user(), the other ones go
 * through an extern _copy_to/from_user(), which expands the same code
 * here.
 *
 * Rust code always uses the extern definition.
 */
static inline __must_check unsigned long
_inline_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	unsigned long res = n;

	if (!access_ok(from, n))
		goto fail;

	res = raw_copy_from_user(to, from, n);

	if (likely(!res))
		return 0;
fail:
	memset(to + (n - res), 0, res);
	return res;
}

static __always_inline unsigned long __must_check
copy_from_user(void *to, const void __user *from, unsigned long n)
{
	if (!check_copy_size(to, n, false))
		return n;

	return _inline_copy_from_user(to, from, n);
}

#endif		/* __LINUX_UACCESS_H__ */
