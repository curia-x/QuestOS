// SPDX-License-Identifier: GPL-2.0
#include <linux/types.h>
#include <irq.h>

struct spinlock {
	u32 state;
};

#define SPINLOCK_INIT	{ .state = 0 }

#define DEFINE_SPINLOCK(name) \
	struct spinlock name = SPINLOCK_INIT

static inline void spin_lock_init(struct spinlock *lock)
{
	lock->state = 0;
}

extern void raw_arch_spin_lock(u32 *state);
extern void raw_arch_spin_unlock(u32 *state);

static inline void spin_lock(struct spinlock *lock)
{
	raw_arch_spin_lock(&lock->state);
}

static inline void spin_unlock(struct spinlock *lock)
{
	raw_arch_spin_unlock(&lock->state);
}

#define spin_lock_irqsave(lock, flags) do {	\
	local_irq_save(flags);	\
	spin_lock(lock);	\
} while(0)

#define spin_unlock_irqrestore(lock, flags) do {	\
	spin_unlock(lock);	\
	local_irq_restore(flags);	\
} while(0)
