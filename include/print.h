/* SPDX-License-Identifier: GPL-2.0 */
#ifndef PRINK_H
#define PRINK_H

int printk(const char *fmt, ...);
#define printf(...) printk(__VA_ARGS__)
void init_printf_done(void);
void printf_set_ready(void);
void printf_set_forbid(void);

enum log_level {
	LOG_DEBUG = 0,
	LOG_INFO,
	LOG_NOTICE,
	LOG_WARN,
	LOG_ERR,
	LOG_CRIT,
	LOG_ALERT,
	LOG_EMERG,
};

extern enum log_level current_log_level;

#define printk_level(level, ...) do {           \
	if ((level) >= current_log_level)           \
		printf(__VA_ARGS__);                    \
} while (0)

#ifdef DEBUG
#define pr_debug(...) do {	\
	printf(__VA_ARGS__)	\
} while(0)
#else
#define pr_debug(...) printk_level(LOG_DEBUG, __VA_ARGS__)
#endif

#define pr_info(...)				printk_level(LOG_INFO,  __VA_ARGS__)
#define pr_notice(...)				printk_level(LOG_NOTICE,  __VA_ARGS__)
#define pr_warn(...)				printk_level(LOG_WARN,  __VA_ARGS__)
#define pr_warn_ratelimited(...)	pr_warn(__VA_ARGS__)
#define pr_err(...) 				printk_level(LOG_ERR,   __VA_ARGS__)
#define pr_crit(...)				printk_level(LOG_CRIT,  __VA_ARGS__)
#define pr_crit_once(...)			pr_crit(__VA_ARGS__)
#define pr_alert(...)				printk_level(LOG_ALERT,  __VA_ARGS__)
#define pr_emerg(...)				printk_level(LOG_EMERG,  __VA_ARGS__)

#endif
