// SPDX-License-Identifier: GPL-2.0
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/minmax.h>
#include <page.h>
#include <mm.h>
#include <memory.h>
#include <system.h>

void __copy_overflow(int size, unsigned long count)
{
	WARN(1, "Buffer overflow detected (%d < %lu)!\n", size, count);
}

#define PAR_F		  (1UL << 0)
#define PAR_PA_MASK	0x0000fffffffff000UL   /* 简化：48-bit PA */

static inline int user_access_at(unsigned long uaddr, bool write, phys_addr_t *pa_out)
{
	unsigned long par;

	if (write) {
		asm volatile(
			"at s1e0w, %1\n"
			"isb\n"
			"mrs %0, par_el1\n"
			: "=r"(par)
			: "r"(uaddr)
			: "memory");
	} else {
		asm volatile(
			"at s1e0r, %1\n"
			"isb\n"
			"mrs %0, par_el1\n"
			: "=r"(par)
			: "r"(uaddr)
			: "memory");
	}

	if (par & PAR_F)
		return -EFAULT;

	if (pa_out)
		*pa_out = (par & PAR_PA_MASK) | (uaddr & (PAGE_SIZE - 1));
	return 0;
}

unsigned long __must_check
__arch_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	phys_addr_t pa;
	unsigned long len;
	unsigned long uaddr = (unsigned long)from;
	char *kto = to;

	/* Head: copy until the first page boundary. */
	if (uaddr & (PAGE_SIZE - 1)) {
		if (user_access_at(uaddr, false, &pa))
			return n;

		len = min_t(unsigned long, n,
			    PAGE_SIZE - (uaddr & (PAGE_SIZE - 1)));

		memcpy(kto, phys_to_virt(pa), len);

		n -= len;
		uaddr += len;
		kto += len;
	}

	/* Body: copy full pages. */
	while (n >= PAGE_SIZE) {
		if (user_access_at(uaddr, false, &pa))
			return n;

		memcpy(kto, phys_to_virt(pa), PAGE_SIZE);

		n -= PAGE_SIZE;
		uaddr += PAGE_SIZE;
		kto += PAGE_SIZE;
	}

	/* Tail: copy the final partial page. */
	if (n) {
		if (user_access_at(uaddr, false, &pa))
			return n;

		memcpy(kto, phys_to_virt(pa), n);
	}

	return 0;
}
