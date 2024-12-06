// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * fmsh power management
 *
 * Copyright (C) 2011-2013 FMSH
 *
 */

#include <linux/clk/fmsh.h>
#include <linux/genalloc.h>
#include <linux/suspend.h>
#include <asm/cacheflush.h>
#include <asm/fncpy.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/mach/map.h>
#include <asm/suspend.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include "common.h"

/* register offsets */
#define DDRC_PUB_BASE		0xE0027000

static void __iomem *ddrc_umc_base;
static void __iomem *ddrc_pub_base;
void __iomem *gpio_base;

#ifdef CONFIG_SUSPEND
static int (*fmsh_suspend_ptr)(void __iomem *, void __iomem *, void __iomem *);

static int fmsh_pm_prepare_late(void)
{
	return fmsh_clk_suspend_early();
}

static void fmsh_pm_wake(void)
{
	fmsh_clk_resume_late();
}

static int fmsh_pm_suspend(unsigned long arg)
{
	int do_ddrpll_bypass = 1;

	if (!fmsh_suspend_ptr || !ddrc_umc_base || !ddrc_pub_base)
		do_ddrpll_bypass = 0;

	if (do_ddrpll_bypass) {
		/*
		 * Going this way will turn off DDR related clocks and the DDR
		 * PLL. I.e. We might brake sub systems relying on any of this
		 * clocks. And even worse: If there are any other masters in the
		 * system (e.g. in the PL) accessing DDR they are screwed.
		 */
		flush_cache_all();
		if (fmsh_suspend_ptr(ddrc_umc_base, fmsh_slcr_base, ddrc_pub_base))
			pr_info("DDR self refresh failed.\n");
	} else {
		WARN_ONCE(1, "DRAM self-refresh not available\n");
		cpu_do_idle();
	}

	return 0;
}

static int fmsh_pm_enter(suspend_state_t suspend_state)
{
	switch (suspend_state) {
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		cpu_suspend(0, fmsh_pm_suspend);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static const struct platform_suspend_ops fmsh_pm_ops = {
	.prepare_late	= fmsh_pm_prepare_late,
	.enter		= fmsh_pm_enter,
	.wake		= fmsh_pm_wake,
	.valid		= suspend_valid_only_mem,
};

/**
 * fmsh_pm_remap_ocm() - Remap OCM
 * Returns a pointer to the mapped memory or NULL.
 *
 * Remap the OCM.
 */
static void __iomem *fmsh_pm_remap_ocm(void)
{
	struct device_node *np;
	const char *comp = "fmsh,fmql-ocmc-1.0";
	void __iomem *base = NULL;

	np = of_find_compatible_node(NULL, NULL, comp);
	if (np) {
		struct device *dev;
		unsigned long pool_addr;
		unsigned long pool_addr_virt;
		struct gen_pool *pool;

		of_node_put(np);

		dev = &(of_find_device_by_node(np)->dev);

		/* Get OCM pool from device tree or platform data */
		pool = gen_pool_get(dev, NULL);
		if (!pool) {
			pr_warn("%s: OCM pool is not available\n", __func__);
			return NULL;
		}

		pool_addr_virt = gen_pool_alloc(pool, fmsh_sys_suspend_sz);
		if (!pool_addr_virt) {
			pr_warn("%s: Can't get OCM poll\n", __func__);
			return NULL;
		}
		pool_addr = gen_pool_virt_to_phys(pool, pool_addr_virt);
		if (!pool_addr) {
			pr_warn("%s: Can't get physical address of OCM pool\n",
				__func__);
			return NULL;
		}
		base = __arm_ioremap_exec(pool_addr, fmsh_sys_suspend_sz,
				     MT_MEMORY_RWX);
		if (!base) {
			pr_warn("%s: IOremap OCM pool failed\n", __func__);
			return NULL;
		}
		pr_debug("%s: Remap OCM %s from %lx to %lx\n", __func__, comp,
			 pool_addr_virt, (unsigned long)base);
	} else {
		pr_warn("%s: no compatible node found for '%s'\n", __func__,
				comp);
	}

	return base;
}

static void fmsh_pm_suspend_init(void)
{
	void __iomem *ocm_base = fmsh_pm_remap_ocm();

	if (!ocm_base) {
		pr_warn("%s: Unable to map OCM.\n", __func__);
	} else {
		/*
		 * Copy code to suspend system into OCM. The suspend code
		 * needs to run from OCM as DRAM may no longer be available
		 * when the PLL is stopped.
		 */
		fmsh_suspend_ptr = fncpy((__force void *)ocm_base,
					 (__force void *)&fmsh_sys_suspend,
					 fmsh_sys_suspend_sz);
	}

	suspend_set_ops(&fmsh_pm_ops);
}
#else	/* CONFIG_SUSPEND */
static void fmsh_pm_suspend_init(void) { };
#endif	/* CONFIG_SUSPEND */

/**
 * fmsh_pm_ioremap() - Create IO mappings
 * @comp:	DT compatible string
 * Return: Pointer to the mapped memory or NULL.
 *
 * Remap the memory region for a compatible DT node.
 */
static void __iomem *fmsh_pm_ioremap(const char *comp)
{
	struct device_node *np;
	void __iomem *base = NULL;

	np = of_find_compatible_node(NULL, NULL, comp);
	if (np) {
		base = of_iomap(np, 0);
		of_node_put(np);
	} else {
		pr_warn("%s: no compatible node found for '%s'\n", __func__,
				comp);
	}

	return base;
}

/**
 * fmsh_pm_late_init() - Power management init
 *
 * Initialization of power management related features and infrastructure.
 */
void __init fmsh_pm_late_init(void)
{
	ddrc_umc_base = fmsh_pm_ioremap("fmsh,psoc-ddr-umc");
	if (!ddrc_umc_base)
		pr_warn("%s: Unable to map DDRC UMC IO memory.\n", __func__);

	ddrc_pub_base = ioremap(DDRC_PUB_BASE, 0x1000);
	if (!ddrc_pub_base)
		pr_warn("%s: Unable to map DDRC PUB IO memory.\n", __func__);

	/* set up suspend */
	fmsh_pm_suspend_init();
}
