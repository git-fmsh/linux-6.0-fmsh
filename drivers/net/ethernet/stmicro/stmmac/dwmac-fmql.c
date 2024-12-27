/*
 * dwmac-fmql.c - FMQL DWMAC specific glue layer
 *
 * Copyright (C) 2018 FMSH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define DEBUG
#include <linux/stmmac.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/of_net.h>
#include <linux/regulator/consumer.h>

#include "stmmac_platform.h"

struct fmql_priv_data {
	int interface;
	int clk_enabled;
	int gmac_seq;
	struct clk *tx_clk;
	struct clk *rx_clk;
};

#define FMQL_SLCR_BASE			(0xe0026000)
#define GMAC_CTRL_OFFSET		(0x414)
#define FMQL_GMAC_1000_RATE		125000000
#define FMQL_GMAC_100_RATE		25000000
#define FMQL_GMAC_10_RATE		2500000

#define GMAC_CTRL_MASK      (0x7)
#define GMAC_RST_CTRL_AXI_RST (0x1)
#define GMAC_RST_CTRL_AHB_RST (0x2)

static int fmql_gmac_init(struct platform_device *pdev, void *priv)
{
	struct fmql_priv_data *gmac = priv;

	clk_set_rate(gmac->tx_clk, FMQL_GMAC_100_RATE);
	clk_prepare_enable(gmac->tx_clk);
	clk_prepare_enable(gmac->rx_clk);
	gmac->clk_enabled = 1;

	return 0;
}

static void fmql_gmac_exit(struct platform_device *pdev, void *priv)
{
	struct fmql_priv_data *gmac = priv;

	if (gmac->clk_enabled) {
		clk_disable(gmac->tx_clk);
		clk_disable(gmac->rx_clk);
		gmac->clk_enabled = 0;
	}
	clk_unprepare(gmac->tx_clk);
	clk_unprepare(gmac->rx_clk);
}

static void fmql_fix_speed(void *priv, unsigned int speed)
{
	struct fmql_priv_data *gmac = priv;

	if (gmac->clk_enabled) {
		clk_disable(gmac->tx_clk);
		gmac->clk_enabled = 0;
	}
	clk_unprepare(gmac->tx_clk);

	if (speed == 1000) {
		clk_set_rate(gmac->tx_clk, FMQL_GMAC_1000_RATE);
	} else if (speed == 100) {
		clk_set_rate(gmac->tx_clk, FMQL_GMAC_100_RATE);
	} else {
		clk_set_rate(gmac->tx_clk, FMQL_GMAC_10_RATE);
	}

	clk_prepare_enable(gmac->tx_clk);
	gmac->clk_enabled = 1;
}

static int fmql_gmac_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	struct fmql_priv_data *gmac;
	struct device *dev = &pdev->dev;
	int ret;

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

		plat_dat = stmmac_probe_config_dt(pdev, stmmac_res.mac);
	if (IS_ERR(plat_dat))
		return PTR_ERR(plat_dat);

	gmac = devm_kzalloc(dev, sizeof(*gmac), GFP_KERNEL);
	if (!gmac) {
		ret = -ENOMEM;
		goto err_remove_config_dt;
	}

	gmac->interface = plat_dat->interface;

	gmac->tx_clk = devm_clk_get(dev, "fmql-gmac-tx");
	if (IS_ERR(gmac->tx_clk)) {
		dev_err(dev, "could not get tx clock\n");
		ret = PTR_ERR(gmac->tx_clk);
		goto err_remove_config_dt;
	}

	gmac->rx_clk = devm_clk_get(dev, "fmql-gmac-rx");
	if (IS_ERR(gmac->rx_clk)) {
		dev_err(dev, "could not get rx clock\n");
		ret = PTR_ERR(gmac->rx_clk);
		goto err_remove_config_dt;
	}

	/* Get sequence number of gmac from device tree */
	if (of_property_read_u32(np, "fmsh,gmac-number", &gmac->gmac_seq))
		gmac->gmac_seq = 0;

	/* platform data specifying hardware features and callbacks.
	 * hardware features were copied from fmql drivers. */
	plat_dat->has_gmac = true;
	plat_dat->bsp_priv = gmac;
	plat_dat->init = fmql_gmac_init;
	plat_dat->exit = fmql_gmac_exit;
	plat_dat->fix_mac_speed = fmql_fix_speed;

	if (clk_get_rate(plat_dat->stmmac_clk) >= 300000000) {
		plat_dat->clk_csr = STMMAC_CSR_250_300M;
		dev_dbg(dev, "clk_csr=0x%x\n", plat_dat->clk_csr);
	}

	ret = fmql_gmac_init(pdev, plat_dat->bsp_priv);
	if (ret)
		goto err_remove_config_dt;

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret)
		goto err_gmac_exit;

	return 0;

err_gmac_exit:
	fmql_gmac_exit(pdev, plat_dat->bsp_priv);
err_remove_config_dt:
	stmmac_remove_config_dt(pdev, plat_dat);

	return ret;
}

static const struct of_device_id fmql_dwmac_match[] = {
	{ .compatible = "fmsh,fmql-gmac" },
	{ }
};
MODULE_DEVICE_TABLE(of, fmql_dwmac_match);

static struct platform_driver fmql_dwmac_driver = {
	.probe  = fmql_gmac_probe,
	.remove = stmmac_pltfr_remove,
	.driver = {
		.name   = "fmql-dwmac",
		.pm		= &stmmac_pltfr_pm_ops,
		.of_match_table = fmql_dwmac_match,
	},
};
module_platform_driver(fmql_dwmac_driver);

MODULE_AUTHOR("Liu Lei<liulei@fmsh.com.cn>");
MODULE_DESCRIPTION("FMQL DWMAC specific glue layer");
MODULE_LICENSE("GPL");
