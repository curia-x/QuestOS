/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __STUB_H__
#define __STUB_H__

#ifndef __ASSAMBLY__

#define DECLARE_PER_CPU(struct_type, var) struct_type var
#define DEFINE_PER_CPU(struct_type, var) struct_type var
#define EXPORT_PER_CPU_SYMBOL_GPL(var)
#define call_rcu(head, func) do { \
	if (head) { \
		func(head); \
	} \
} while (0)

#define rcu_dereference_raw(ptr) ptr

#define gfpflags_allow_blocking(flags) ({ (void)(flags); true; })

#define xas_lock_type(...) do { (void)xas; (void)lock_type; } while (0)
#define xas_unlock_type(xas, lock_type) do { (void)xas; (void)lock_type; } while (0)

#define xa_lock(...) do { } while (0)
#define xa_unlock(...) do { } while (0)
#define xa_lock_bh(...) do { } while (0)
#define xa_unlock_bh(...) do { } while (0)
#define xa_lock_irq(...) do { } while (0)
#define xa_unlock_irq(...) do { } while (0)
#define xas_lock(...) do { } while (0)
#define xas_unlock(...) do { } while (0)

#define RCU_INIT_POINTER(p, v) \
	do { \
		p = v; \
	} while (0)

#define rcu_assign_pointer(p, v)					      \
do {									      \
	p = v; \
} while (0)

#define rcu_read_lock() do { } while (0)
#define rcu_read_unlock() do { } while (0)

#define rcu_dereference(ptr)	ptr
#define rcu_dereference_protected(ptr, c)	ptr

#define local_unlock(lock) do { (void)(lock); } while (0)
#define local_lock(lock) do { (void)(lock); } while (0)

#define spinlock_t int
#define local_lock_t int

#define spin_lock_init(lock) do { (void)(lock); } while (0)
#define spin_lock(lock) do { (void)(lock); } while (0)
#define spin_unlock(lock) do { (void)(lock); } while (0)

#define INIT_LOCAL_LOCK(lock) 0

#define in_interrupt() false
#define in_softirq() false
#define in_irq() false
#define in_nmi() false

#define this_cpu_ptr(ptr) (ptr)

#define per_cpu(var, cpu) (var)

#define kmem_cache_free(cache, ptr) kfree(ptr)
#define kmem_cache_create(name, size, align, flags, ctor) NULL

#define cpuhp_setup_state_nocalls(state, name, arg, func) 0

#define xas_lock_irqsave(xas, flags) do { (void)(xas); (void)(flags); } while (0)
#define xas_unlock_irqrestore(xas, flags) do { (void)(xas); (void)(flags); } while (0)
#define xa_lock_irqsave(xa, flags) do { (void)(xa); (void)(flags); } while (0)
#define xa_unlock_irqrestore(xa, flags) do { (void)(xa); (void)(flags); } while (0)

#define might_alloc(gfp) do { (void)(gfp); } while (0)

#undef READ_ONCE
#define READ_ONCE(x) (x)

#undef WRITE_ONCE
#define WRITE_ONCE(x, val) do { (x) = (val); } while (0)

#endif /* __ASSEMBLY__ */
#endif /* __STUB_H__ */