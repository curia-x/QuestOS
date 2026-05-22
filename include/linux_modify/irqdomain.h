/* SPDX-License-Identifier: GPL-2.0 */
/*
 * irq_domain - IRQ Translation Domains
 *
 * See Documentation/core-api/irq/irq-domain.rst for the details.
 */

#ifndef _LINUX_IRQDOMAIN_H
#define _LINUX_IRQDOMAIN_H

#include <linux/types.h>
#include <linux/irqdomain_defs.h>
#include <linux/irqhandler.h>

struct irq_domain;
struct irq_chip;
struct irq_data;
struct irq_desc;
struct cpumask;
struct seq_file;
struct irq_affinity_desc;
struct msi_parent_ops;

#define IRQ_DOMAIN_IRQ_SPEC_PARAMS 16

/**
 * struct irq_domain_ops - Methods for irq_domain objects
 * @match:	Match an interrupt controller device node to a domain, returns
 *		1 on a match
 * @select:	Match an interrupt controller fw specification. It is more generic
 *		than @match as it receives a complete struct irq_fwspec. Therefore,
 *		@select is preferred if provided. Returns 1 on a match.
 * @map:	Create or update a mapping between a virtual irq number and a hw
 *		irq number. This is called only once for a given mapping.
 * @unmap:	Dispose of such a mapping
 * @xlate:	Given a device tree node and interrupt specifier, decode
 *		the hardware irq number and linux irq type value.
 * @alloc:	Allocate @nr_irqs interrupts starting from @virq.
 * @free:	Free @nr_irqs interrupts starting from @virq.
 * @activate:	Activate one interrupt in HW (@irqd). If @reserve is set, only
 *		reserve the vector. If unset, assign the vector (called from
 *		request_irq()).
 * @deactivate:	Disarm one interrupt (@irqd).
 * @translate:	Given @fwspec, decode the hardware irq number (@out_hwirq) and
 *		linux irq type value (@out_type). This is a generalised @xlate
 *		(over struct irq_fwspec) and is preferred if provided.
 * @debug_show:	For domains to show specific data for an interrupt in debugfs.
 *
 * Functions below are provided by the driver and called whenever a new mapping
 * is created or an old mapping is disposed. The driver can then proceed to
 * whatever internal data structures management is required. It also needs
 * to setup the irq_desc when returning from map().
 */
struct irq_domain_ops {
	int	(*map)(struct irq_domain *d, unsigned int virq, irq_hw_number_t hw);
	void	(*unmap)(struct irq_domain *d, unsigned int virq);
#ifdef	CONFIG_IRQ_DOMAIN_HIERARCHY
	/* extended V2 interfaces to support hierarchy irq_domains */
	int	(*alloc)(struct irq_domain *d, unsigned int virq,
			 unsigned int nr_irqs, void *arg);
	void	(*free)(struct irq_domain *d, unsigned int virq,
			unsigned int nr_irqs);
	int	(*activate)(struct irq_domain *d, struct irq_data *irqd, bool reserve);
	void	(*deactivate)(struct irq_domain *d, struct irq_data *irq_data);
#endif
};

extern const struct irq_domain_ops irq_generic_chip_ops;

struct irq_domain_chip_generic;

/**
 * struct irq_domain - Hardware interrupt number translation object
 * @link:	Element in global irq_domain list.
 * @name:	Name of interrupt domain
 * @ops:	Pointer to irq_domain methods
 * @host_data:	Private data pointer for use by owner.  Not touched by irq_domain
 *		core code.
 * @flags:	Per irq_domain flags
 * @mapcount:	The number of mapped interrupts
 * @mutex:	Domain lock, hierarchical domains use root domain's lock
 * @root:	Pointer to root domain, or containing structure if non-hierarchical
 *
 * Optional elements:
 * @fwnode:	Pointer to firmware node associated with the irq_domain. Pretty easy
 *		to swap it for the of_node via the irq_domain_get_of_node accessor
 * @bus_token:	@fwnode's device_node might be used for several irq domains. But
 *		in connection with @bus_token, the pair shall be unique in a
 *		system.
 * @gc:		Pointer to a list of generic chips. There is a helper function for
 *		setting up one or more generic chips for interrupt controllers
 *		drivers using the generic chip library which uses this pointer.
 * @dev:	Pointer to the device which instantiated the irqdomain
 *		With per device irq domains this is not necessarily the same
 *		as @pm_dev.
 * @pm_dev:	Pointer to a device that can be utilized for power management
 *		purposes related to the irq domain.
 * @parent:	Pointer to parent irq_domain to support hierarchy irq_domains
 * @msi_parent_ops: Pointer to MSI parent domain methods for per device domain init
 * @exit:	Function called when the domain is destroyed
 *
 * Revmap data, used internally by the irq domain code:
 * @hwirq_max:		Top limit for the HW irq number. Especially to avoid
 *			conflicts/failures with reserved HW irqs. Can be ~0.
 * @revmap_size:	Size of the linear map table @revmap
 * @revmap_tree:	Radix map tree for hwirqs that don't fit in the linear map
 * @revmap:		Linear table of irq_data pointers
 */
struct irq_domain {
	struct list_head		link;
	const char			*name;
	const struct irq_domain_ops	*ops;
	void				*host_data;
	unsigned int			flags;
	unsigned int			mapcount;
	struct irq_domain		*root;

	/* Optional data */
	enum irq_domain_bus_token	bus_token;
	struct irq_domain_chip_generic	*gc;
#ifdef	CONFIG_IRQ_DOMAIN_HIERARCHY
	struct irq_domain		*parent;
#endif
#ifdef CONFIG_GENERIC_MSI_IRQ
	const struct msi_parent_ops	*msi_parent_ops;
#endif
	void				(*exit)(struct irq_domain *d);

	/* reverse map data. The linear map gets appended to the irq_domain */
	irq_hw_number_t			hwirq_max;
	unsigned int			revmap_size;
	struct irq_data __rcu		*revmap[] __counted_by(revmap_size);
};

/* Irq domain flags */
enum {
	/* Irq domain is hierarchical */
	IRQ_DOMAIN_FLAG_HIERARCHY	= (1 << 0),

	/* Irq domain name was allocated internally */
	IRQ_DOMAIN_NAME_ALLOCATED	= (1 << 1),

	/* Irq domain is an IPI domain with virq per cpu */
	IRQ_DOMAIN_FLAG_IPI_PER_CPU	= (1 << 2),

	/* Irq domain is an IPI domain with single virq */
	IRQ_DOMAIN_FLAG_IPI_SINGLE	= (1 << 3),

	/* Irq domain implements MSIs */
	IRQ_DOMAIN_FLAG_MSI		= (1 << 4),

	/*
	 * Irq domain implements isolated MSI, see msi_device_has_isolated_msi()
	 */
	IRQ_DOMAIN_FLAG_ISOLATED_MSI	= (1 << 5),

	/* Irq domain doesn't translate anything */
	IRQ_DOMAIN_FLAG_NO_MAP		= (1 << 6),

	/* Irq domain is a MSI parent domain */
	IRQ_DOMAIN_FLAG_MSI_PARENT	= (1 << 8),

	/* Irq domain is a MSI device domain */
	IRQ_DOMAIN_FLAG_MSI_DEVICE	= (1 << 9),

	/* Irq domain must destroy generic chips when removed */
	IRQ_DOMAIN_FLAG_DESTROY_GC	= (1 << 10),

	/* Address and data pair is mutable when irq_set_affinity() */
	IRQ_DOMAIN_FLAG_MSI_IMMUTABLE	= (1 << 11),

	/* IRQ domain requires parent fwnode matching */
	IRQ_DOMAIN_FLAG_FWNODE_PARENT	= (1 << 12),

	/*
	 * Flags starting from IRQ_DOMAIN_FLAG_NONCORE are reserved
	 * for implementation specific purposes and ignored by the
	 * core code.
	 */
	IRQ_DOMAIN_FLAG_NONCORE		= (1 << 16),
};

#ifdef CONFIG_IRQ_DOMAIN

struct irq_domain_chip_generic_info;

/**
 * struct irq_domain_info - Domain information structure
 * @fwnode:		firmware node for the interrupt controller
 * @domain_flags:	Additional flags to add to the domain flags
 * @size:		Size of linear map; 0 for radix mapping only
 * @hwirq_max:		Maximum number of interrupts supported by controller
 * @direct_max:		Maximum value of direct maps;
 *			Use ~0 for no limit; 0 for no direct mapping
 * @hwirq_base:		The first hardware interrupt number (legacy domains only)
 * @virq_base:		The first Linux interrupt number for legacy domains to
 *			immediately associate the interrupts after domain creation
 * @bus_token:		Domain bus token
 * @name_suffix:	Optional name suffix to avoid collisions when multiple
 *			domains are added using same fwnode
 * @ops:		Domain operation callbacks
 * @host_data:		Controller private data pointer
 * @dev:		Device which creates the domain
 * @dgc_info:		Geneneric chip information structure pointer used to
 *			create generic chips for the domain if not NULL.
 * @init:		Function called when the domain is created.
 *			Allow to do some additional domain initialisation.
 * @exit:		Function called when the domain is destroyed.
 *			Allow to do some additional cleanup operation.
 */
struct irq_domain_info {
	unsigned int				domain_flags;
	unsigned int				size;
	irq_hw_number_t				hwirq_max;
	int					direct_max;
	unsigned int				hwirq_base;
	unsigned int				virq_base;
	enum irq_domain_bus_token		bus_token;
	const char				*name_suffix;
	const struct irq_domain_ops		*ops;
	void					*host_data;
#ifdef CONFIG_IRQ_DOMAIN_HIERARCHY
	/**
	 * @parent: Pointer to the parent irq domain used in a hierarchy domain
	 */
	struct irq_domain			*parent;
#endif
	struct irq_domain_chip_generic_info	*dgc_info;
	int					(*init)(struct irq_domain *d);
	void					(*exit)(struct irq_domain *d);
};

struct irq_domain *irq_domain_instantiate(const struct irq_domain_info *info);

void irq_set_default_domain(struct irq_domain *domain);
struct irq_domain *irq_get_default_domain(void);
int irq_domain_alloc_descs(int virq, unsigned int nr_irqs, irq_hw_number_t hwirq, int node,
			   const struct irq_affinity_desc *affinity);

void irq_domain_update_bus_token(struct irq_domain *domain, enum irq_domain_bus_token bus_token);

#ifdef CONFIG_IRQ_DOMAIN_NOMAP
unsigned int irq_create_direct_mapping(struct irq_domain *domain);
#endif

void irq_domain_remove(struct irq_domain *domain);

int irq_domain_associate(struct irq_domain *domain, unsigned int irq, irq_hw_number_t hwirq);
void irq_domain_associate_many(struct irq_domain *domain, unsigned int irq_base,
			       irq_hw_number_t hwirq_base, int count);

unsigned int irq_create_mapping_affinity(struct irq_domain *domain, irq_hw_number_t hwirq,
					 const struct irq_affinity_desc *affinity);
void irq_dispose_mapping(unsigned int virq);

/**
 * irq_create_mapping - Map a hardware interrupt into linux irq space
 * @domain:	domain owning this hardware interrupt or NULL for default domain
 * @hwirq:	hardware irq number in that domain space
 *
 * Only one mapping per hardware interrupt is permitted.
 *
 * If the sense/trigger is to be specified, set_irq_type() should be called
 * on the number returned from that call.
 *
 * Returns: Linux irq number or 0 on error
 */
static inline unsigned int irq_create_mapping(struct irq_domain *domain, irq_hw_number_t hwirq)
{
	return irq_create_mapping_affinity(domain, hwirq, NULL);
}

struct irq_desc *__irq_resolve_mapping(struct irq_domain *domain,
				       irq_hw_number_t hwirq,
				       unsigned int *irq);

/**
 * irq_resolve_mapping - Find a linux irq from a hw irq number.
 * @domain:	domain owning this hardware interrupt
 * @hwirq:	hardware irq number in that domain space
 *
 * Returns: Interrupt descriptor
 */
static inline struct irq_desc *irq_resolve_mapping(struct irq_domain *domain,
						   irq_hw_number_t hwirq)
{
	return __irq_resolve_mapping(domain, hwirq, NULL);
}

/**
 * irq_find_mapping() - Find a linux irq from a hw irq number.
 * @domain:	domain owning this hardware interrupt
 * @hwirq:	hardware irq number in that domain space
 *
 * Returns: Linux irq number or 0 if not found
 */
static inline unsigned int irq_find_mapping(struct irq_domain *domain,
					    irq_hw_number_t hwirq)
{
	unsigned int irq;

	if (__irq_resolve_mapping(domain, hwirq, &irq))
		return irq;

	return 0;
}

extern const struct irq_domain_ops irq_domain_simple_ops;

/* IPI functions */
int irq_reserve_ipi(struct irq_domain *domain, const struct cpumask *dest);
int irq_destroy_ipi(unsigned int irq, const struct cpumask *dest);

/* V2 interfaces to support hierarchy IRQ domains. */
struct irq_data *irq_domain_get_irq_data(struct irq_domain *domain, unsigned int virq);
void irq_domain_set_info(struct irq_domain *domain, unsigned int virq, irq_hw_number_t hwirq,
			 const struct irq_chip *chip, void *chip_data, irq_flow_handler_t handler,
			 void *handler_data, const char *handler_name);
void irq_domain_reset_irq_data(struct irq_data *irq_data);
#ifdef	CONFIG_IRQ_DOMAIN_HIERARCHY

int __irq_domain_alloc_irqs(struct irq_domain *domain, int irq_base, unsigned int nr_irqs,
			    int node, void *arg, bool realloc,
			    const struct irq_affinity_desc *affinity);
void irq_domain_free_irqs(unsigned int virq, unsigned int nr_irqs);
int irq_domain_activate_irq(struct irq_data *irq_data, bool early);
void irq_domain_deactivate_irq(struct irq_data *irq_data);

/**
 * irq_domain_alloc_irqs - Allocate IRQs from domain
 * @domain:	domain to allocate from
 * @nr_irqs:	number of IRQs to allocate
 * @node:	NUMA node id for memory allocation
 * @arg:	domain specific argument
 *
 * See __irq_domain_alloc_irqs()' documentation.
 */
static inline int irq_domain_alloc_irqs(struct irq_domain *domain, unsigned int nr_irqs,
					int node, void *arg)
{
	return __irq_domain_alloc_irqs(domain, -1, nr_irqs, node, arg, false, NULL);
}

int irq_domain_set_hwirq_and_chip(struct irq_domain *domain, unsigned int virq,
				  irq_hw_number_t hwirq, const struct irq_chip *chip,
				  void *chip_data);
void irq_domain_free_irqs_common(struct irq_domain *domain, unsigned int virq,
				 unsigned int nr_irqs);
void irq_domain_free_irqs_top(struct irq_domain *domain, unsigned int virq, unsigned int nr_irqs);

int irq_domain_push_irq(struct irq_domain *domain, int virq, void *arg);
int irq_domain_pop_irq(struct irq_domain *domain, int virq);

int irq_domain_alloc_irqs_parent(struct irq_domain *domain, unsigned int irq_base,
				 unsigned int nr_irqs, void *arg);

void irq_domain_free_irqs_parent(struct irq_domain *domain, unsigned int irq_base,
				 unsigned int nr_irqs);

int irq_domain_disconnect_hierarchy(struct irq_domain *domain, unsigned int virq);

static inline bool irq_domain_is_hierarchy(struct irq_domain *domain)
{
	return domain->flags & IRQ_DOMAIN_FLAG_HIERARCHY;
}

static inline bool irq_domain_is_ipi(struct irq_domain *domain)
{
	return domain->flags & (IRQ_DOMAIN_FLAG_IPI_PER_CPU | IRQ_DOMAIN_FLAG_IPI_SINGLE);
}

static inline bool irq_domain_is_ipi_per_cpu(struct irq_domain *domain)
{
	return domain->flags & IRQ_DOMAIN_FLAG_IPI_PER_CPU;
}

static inline bool irq_domain_is_ipi_single(struct irq_domain *domain)
{
	return domain->flags & IRQ_DOMAIN_FLAG_IPI_SINGLE;
}

static inline bool irq_domain_is_msi(struct irq_domain *domain)
{
	return domain->flags & IRQ_DOMAIN_FLAG_MSI;
}

static inline bool irq_domain_is_msi_parent(struct irq_domain *domain)
{
	return domain->flags & IRQ_DOMAIN_FLAG_MSI_PARENT;
}

static inline bool irq_domain_is_msi_device(struct irq_domain *domain)
{
	return domain->flags & IRQ_DOMAIN_FLAG_MSI_DEVICE;
}

static inline bool irq_domain_is_msi_immutable(struct irq_domain *domain)
{
	return domain->flags & IRQ_DOMAIN_FLAG_MSI_IMMUTABLE;
}
#else	/* CONFIG_IRQ_DOMAIN_HIERARCHY */
static inline int irq_domain_alloc_irqs(struct irq_domain *domain, unsigned int nr_irqs,
					int node, void *arg)
{
	return -1;
}

static inline void irq_domain_free_irqs(unsigned int virq, unsigned int nr_irqs) { }

static inline bool irq_domain_is_hierarchy(struct irq_domain *domain)
{
	return false;
}

static inline bool irq_domain_is_ipi(struct irq_domain *domain)
{
	return false;
}

static inline bool irq_domain_is_ipi_per_cpu(struct irq_domain *domain)
{
	return false;
}

static inline bool irq_domain_is_ipi_single(struct irq_domain *domain)
{
	return false;
}

static inline bool irq_domain_is_msi(struct irq_domain *domain)
{
	return false;
}

static inline bool irq_domain_is_msi_parent(struct irq_domain *domain)
{
	return false;
}

static inline bool irq_domain_is_msi_device(struct irq_domain *domain)
{
	return false;
}

#endif	/* CONFIG_IRQ_DOMAIN_HIERARCHY */

#ifdef CONFIG_GENERIC_MSI_IRQ
int msi_device_domain_alloc_wired(struct irq_domain *domain, unsigned int hwirq, unsigned int type);
void msi_device_domain_free_wired(struct irq_domain *domain, unsigned int virq);
#else
static inline int msi_device_domain_alloc_wired(struct irq_domain *domain, unsigned int hwirq,
						unsigned int type)
{
	WARN_ON_ONCE(1);
	return -EINVAL;
}
static inline void msi_device_domain_free_wired(struct irq_domain *domain, unsigned int virq)
{
	WARN_ON_ONCE(1);
}
#endif

#else /* CONFIG_IRQ_DOMAIN */
static inline void irq_dispose_mapping(unsigned int virq) { }
#endif /* !CONFIG_IRQ_DOMAIN */

#endif /* _LINUX_IRQDOMAIN_H */
