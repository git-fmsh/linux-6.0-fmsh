/*
 * Copyright (C) 2011 FMSH
 *
 * Based on "Generic on-chip SRAM allocation driver"
 *
 * Copyright (C) 2012 Philipp Zabel, Pengutronix
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/genalloc.h>

#include "common.h"

#define FMSH_OCM_ADDR	0x00020000
#define FMSH_OCM_BLOCK_SIZE	0x40000
#define FMSH_OCM_GRANULARITY	32

struct fmsh_ocm_dev {
	struct gen_pool *pool;
	struct resource res;
};

/**
 * fmsh_ocm_probe - Probe method for the OCM driver
 * @pdev:	Pointer to the platform_device structure
 *
 * This function initializes the driver data structures and the hardware.
 *
 * Return:	0 on success and error value on failure
 */
static int fmsh_ocm_probe(struct platform_device *pdev)
{
	int ret;
	struct fmsh_ocm_dev *fmsh_ocm;
	u32 start, end;
	unsigned long size;
	void __iomem *virt_base;

	fmsh_ocm = devm_kzalloc(&pdev->dev, sizeof(*fmsh_ocm), GFP_KERNEL);
	if (!fmsh_ocm)
		return -ENOMEM;

	fmsh_ocm->pool = devm_gen_pool_create(&pdev->dev,
					      ilog2(FMSH_OCM_GRANULARITY),
					      NUMA_NO_NODE, NULL);
	if (!fmsh_ocm->pool)
		return -ENOMEM;

	/* Setup start and end block addresses for 256kB OCM block */
	start = FMSH_OCM_ADDR;
	end = start + (FMSH_OCM_BLOCK_SIZE - 1);
#if 0//def CONFIG_SMP
	/*
	 * OCM block if placed at 0x0 has special meaning
	 * for SMP because jump trampoline is added there.
	 * Ensure that this address won't be allocated.
	 */
	u32 trampoline_code_size =
		&fmsh_secondary_trampoline_end -
		&fmsh_secondary_trampoline;
	dev_dbg(&pdev->dev,
		"Allocate reset vector table %dB\n",
		trampoline_code_size);
	/* postpone start offset */
	start += trampoline_code_size;
#endif
	/* Resource is always initialized */
	fmsh_ocm->res.start = start;
	fmsh_ocm->res.end = end;
	fmsh_ocm->res.flags = IORESOURCE_MEM;
	dev_dbg(&pdev->dev, "OCM block 0, start %x, end %x\n", start, end);
	pr_info("%s: OCM block 0, start %x, end %x\n", __func__, start, end);

	/* Pool allocation from OCM block detection */
	dev_dbg(&pdev->dev, "OCM resources 0, start %x, end %x\n",
		fmsh_ocm->res.start, fmsh_ocm->res.end);
	size = resource_size(&fmsh_ocm->res);
	virt_base = devm_ioremap_resource(&pdev->dev,
					  &fmsh_ocm->res);
	if (IS_ERR(virt_base))
		return PTR_ERR(virt_base);

	ret = gen_pool_add_virt(fmsh_ocm->pool,
				(unsigned long)virt_base,
				fmsh_ocm->res.start, size, -1);
	if (ret < 0) {
		dev_err(&pdev->dev, "Gen pool failed\n");
		return ret;
	}
	dev_info(&pdev->dev, "FMSH OCM pool: %ld KiB @ 0x%p\n",
		 size / 1024, virt_base);

	platform_set_drvdata(pdev, fmsh_ocm);

	return 0;
}

/**
 * fmsh_ocm_remove - Remove method for the OCM driver
 * @pdev:	Pointer to the platform_device structure
 *
 * This function is called if a device is physically removed from the system or
 * if the driver module is being unloaded. It frees all resources allocated to
 * the device.
 *
 * Return:	0 on success and error value on failure
 */
static int fmsh_ocm_remove(struct platform_device *pdev)
{
	struct fmsh_ocm_dev *fmsh_ocm = platform_get_drvdata(pdev);

	if (gen_pool_avail(fmsh_ocm->pool) < gen_pool_size(fmsh_ocm->pool))
		dev_dbg(&pdev->dev, "removed while SRAM allocated\n");

	return 0;
}

static struct of_device_id fmsh_ocm_dt_ids[] = {
	{ .compatible = "fmsh,fmql-ocmc-1.0" },
	{ /* end of table */ }
};

static struct platform_driver fmsh_ocm_driver = {
	.driver = {
		.name = "fmsh-ocm",
		.of_match_table = fmsh_ocm_dt_ids,
	},
	.probe = fmsh_ocm_probe,
	.remove = fmsh_ocm_remove,
};

static int __init fmsh_ocm_init(void)
{
	return platform_driver_register(&fmsh_ocm_driver);
}

arch_initcall(fmsh_ocm_init);
