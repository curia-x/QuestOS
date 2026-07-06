/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2026 <Nino Zhang>
 *
 * Parts of the design/logic are inspired by U-Boot or Linux kernel.
 */
#ifndef	SYSTEM_H
#define	SYSTEM_H

#include <print.h>

void print_el();
bool in_kernel(void);
void record_enter_in_kernel(void);

#define __BUG(cond_str) do {						\
	const char *__cond_str = (cond_str);					\
	pr_emerg("BUG:%s, %d%s%s\r\n",	\
			__func__, __LINE__, \
			__cond_str[0] == '\0' ? "" : ": ", \
			(__cond_str));	\
	while (1)							\
		;							\
} while (0)

#undef BUG
#define BUG()								\
	__BUG("")

#define BUG_ON(cond) do {						\
	if (unlikely(cond))						\
		__BUG(#cond);						\
} while (0)

#define __WARN_PRINT(func, line, fmt, ...)				\
	pr_warn("WARN:%s, %d: " fmt,				\
	       func, line, ##__VA_ARGS__)

#define WARN(cond, fmt, ...) ({						\
	int __ret_warn = !!(cond);					\
	if (unlikely(__ret_warn))					\
		__WARN_PRINT(__func__, __LINE__, fmt, ##__VA_ARGS__);	\
	unlikely(__ret_warn);						\
})

#define WARN_ON(cond)							\
	WARN(cond, "%s\r\n", #cond)

#define WARN_ONCE(cond, fmt, ...) ({					\
	static bool __warned;						\
	int __ret_warn = !!(cond);					\
	if (unlikely(__ret_warn && !__warned)) {			\
		__warned = true;					\
		__WARN_PRINT(__func__, __LINE__, fmt, ##__VA_ARGS__);	\
	}								\
	unlikely(__ret_warn);						\
})

#define WARN_ON_ONCE(cond) WARN_ONCE(cond, "%s\r\n", #cond)

#define VM_WARN_ON(cond) WARN_ON(cond)

#endif /* SYSTEM_H */
