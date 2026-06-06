// SPDX-License-Identifier: GPL-2.0
#ifndef __PSCI_H__
#define __PSCI_H__

#include <linux/types.h>
#include <uapi/linux/psci.h>

#define PSCI_0_2_FN_PSCI_VERSION		PSCI_0_2_FN(0)

struct psci_operations {
	int (*init) (const char *method);
	u32 (*get_version)(void);
	int (*cpu_suspend)(u32 state, unsigned long entry_point);
	int (*cpu_off)(u32 state);
	int (*cpu_on)(unsigned long cpuid, unsigned long entry_point);
	int (*migrate)(unsigned long cpuid);
	int (*affinity_info)(unsigned long target_affinity,
			unsigned long lowest_affinity_level);
	int (*migrate_info_type)(void);
};

u32 psci_get_version(unsigned long cpuid, unsigned long entry_point);
int psci_cpu_on(unsigned long cpuid, unsigned long entry_point);
int psci_init(void);

#endif /* __PSCI_H__ */