/* SPDX-License-Identifier: GPL-2.0 */
#ifndef PRINK_H
#define PRINK_H

int printf(const char *fmt, ...);
void init_printf_done(void);
void printf_set_ready(void);
void printf_set_forbid(void);

#define pr_warn printf
#define pr_err printf
#define pr_warn_ratelimited printf

#endif
