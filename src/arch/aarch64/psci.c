// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/string.h>
#include <smc.h>
#include <psci.h>
#include <print.h>

#define PSCI_FN_NATIVE(version, name)	PSCI_##version##_FN64_##name

typedef unsigned long (psci_fn)(unsigned long, unsigned long,
				unsigned long, unsigned long);
static psci_fn *invoke_psci_fn;

static __always_inline unsigned long
__invoke_psci_fn_smc(unsigned long function_id,
		     unsigned long arg0, unsigned long arg1,
		     unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}

static __always_inline unsigned long
__invoke_psci_fn_hvc(unsigned long function_id,
		     unsigned long arg0, unsigned long arg1,
		     unsigned long arg2)
{
	struct arm_smccc_res res;

	arm_smccc_hvc(function_id, arg0, arg1, arg2, 0, 0, 0, 0, &res);
	return res.a0;
}

static inline u32 psci_0_2_get_version(void)
{
	return invoke_psci_fn(PSCI_0_2_FN_PSCI_VERSION, 0, 0, 0);
}

static __always_inline int psci_to_linux_errno(int errno)
{
	switch (errno) {
	case PSCI_RET_SUCCESS:
		return 0;
	case PSCI_RET_NOT_SUPPORTED:
		return -EOPNOTSUPP;
	case PSCI_RET_INVALID_PARAMS:
	case PSCI_RET_INVALID_ADDRESS:
		return -EINVAL;
	case PSCI_RET_DENIED:
		return -EPERM;
	}

	return -EINVAL;
}

static int __psci_cpu_on(u32 fn, unsigned long cpuid, unsigned long entry_point)
{
	int err;

	err = invoke_psci_fn(fn, cpuid, entry_point, 0);
	return psci_to_linux_errno(err);
}

static inline int psci_0_2_cpu_on(unsigned long cpuid, unsigned long entry_point)
{
	return __psci_cpu_on(PSCI_FN_NATIVE(0_2, CPU_ON), cpuid, entry_point);
}

static int psci_0_2_init(const char *method)
{
	if (!strcmp("hvc", method)) {
		invoke_psci_fn = __invoke_psci_fn_hvc;
	} else if (!strcmp("smc", method)) {
		invoke_psci_fn = __invoke_psci_fn_smc;
	} else {
		pr_warn("invalid \"method\" property: %s\n", method);
		return -EINVAL;
	}

	return 0;
}

struct psci_operations psci_0_2_ops = {
	.init = psci_0_2_init,
	.get_version = psci_0_2_get_version,
	.cpu_on = psci_0_2_cpu_on
};

static struct psci_operations *g_psci_ops;

u32 psci_get_version(unsigned long cpuid, unsigned long entry_point)
{
	return g_psci_ops->cpu_on(cpuid, entry_point);
}

int psci_cpu_on(unsigned long cpuid, unsigned long entry_point)
{
	return g_psci_ops->cpu_on(cpuid, entry_point);
}

int psci_init(void)
{
	g_psci_ops = &psci_0_2_ops;

	return g_psci_ops->init("hvc");
}
