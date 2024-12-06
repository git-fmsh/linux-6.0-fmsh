/*
 * This file contains common code that is intended to be used across
 * boards so that it's not replicated.
 *
 *  Copyright (C) 2011 FMSH
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cpumask.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clocksource.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/memblock.h>
#include <linux/irqchip.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/mach-types.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/smp_scu.h>
#include <asm/system_info.h>
#include <linux/clk-provider.h>
#include <linux/clk/fmsh.h>

#include "common.h"
#include "smc.h"

#define FMSH_DEVCFG_MCTRL	0x44

static void nor_flash_init(void)
{
	void __iomem * memctl_baseAddr;
	u32 reg_tmp;

	memctl_baseAddr = ioremap(0xe0041000, 0x1000);

	reg_tmp = 0x12354;
	writel(reg_tmp, memctl_baseAddr + MEMCTL_SMTMGR_SET0);
	writel(reg_tmp, memctl_baseAddr + MEMCTL_SMTMGR_SET1);
	writel(reg_tmp, memctl_baseAddr + MEMCTL_SMTMGR_SET2);

	// flash, 256Mb
	reg_tmp = 0x4a;
	writel(reg_tmp, memctl_baseAddr + MEMCTL_SMSKRx(2));
	writel(reg_tmp, memctl_baseAddr + MEMCTL_SMSKRx(3));
	writel(reg_tmp, memctl_baseAddr + MEMCTL_SMSKRx(4));

	reg_tmp = 0x1201; //0x240F;
	writel(reg_tmp, memctl_baseAddr + MEMCTL_SMCTLR);

	iounmap(memctl_baseAddr);
}

/**
 * fmsh_get_revision - Get fmsh PS revision
 *
 * Return: Silicon version or -1 otherwise
 */
static int __init fmsh_get_revision(void)
{
	struct device_node *np;
	void __iomem *fmsh_devcfg_base;
	u32 revision;

	np = of_find_compatible_node(NULL, NULL, "fmsh,fmql-devcfg-1.0");
	if (!np) {
		pr_err("%s: no devcfg node found\n", __func__);
		return -1;
	}

	fmsh_devcfg_base = of_iomap(np, 0);
	if (!fmsh_devcfg_base) {
		pr_err("%s: Unable to map I/O memory\n", __func__);
		return -1;
	}

	revision = readl(fmsh_devcfg_base + FMSH_DEVCFG_MCTRL);

	iounmap(fmsh_devcfg_base);

	return revision;
}

static void __init fmsh_irq_init(void)
{
	fmsh_early_slcr_init();
	irqchip_init();
}

static void __init fmsh_timer_init(void)
{
	fmsh_clock_init();
	of_clk_init(NULL);
	timer_probe();
}

static void __init fmsh_init_late(void)
{
	fmsh_pm_late_init();
}

/**
 * fmsh_init_machine - System specific initialization, intended to be
 *		       called from board specific initialization.
 */
static void __init fmsh_init_machine(void)
{
	struct soc_device_attribute *soc_dev_attr;
	struct soc_device *soc_dev;
	struct device *parent = NULL;

	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		goto out;

	system_rev = fmsh_get_revision();

	soc_dev_attr->family = kasprintf(GFP_KERNEL, "FMSH FMQL");
	soc_dev_attr->revision = kasprintf(GFP_KERNEL, "0x%x", system_rev);
	soc_dev_attr->soc_id = kasprintf(GFP_KERNEL, "0x%x",
					 fmsh_slcr_get_device_id());

	soc_dev = soc_device_register(soc_dev_attr);
	if (IS_ERR(soc_dev)) {
		kfree(soc_dev_attr->family);
		kfree(soc_dev_attr->revision);
		kfree(soc_dev_attr->soc_id);
		kfree(soc_dev_attr);
		goto out;
	}

	parent = soc_device_to_device(soc_dev);

out:
	/*
	 * Finished with the static registrations now; fill in the missing
	 * devices
	 */
	of_platform_default_populate(NULL, NULL, parent);
}

static const char * const fmsh_dt_match[] = {
	"fmsh,fmsh-psoc",
	NULL
};

DT_MACHINE_START(FMSH_PSOC, "FMSH PSOC Platform")
	.smp		= smp_ops(fmsh_smp_ops),
//	.map_io		= fmsh_map_io,
	.init_irq	= fmsh_irq_init,
	.init_late	= fmsh_init_late,
	.init_machine	= fmsh_init_machine,
	.init_time	= fmsh_timer_init,
	.dt_compat	= fmsh_dt_match,
//	.reserve	= fmsh_memory_init,
//	.restart	= fmsh_system_reset,
MACHINE_END
