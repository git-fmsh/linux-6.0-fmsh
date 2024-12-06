/*
 * FMSH clock tree controller
 *
 *  Copyright (C) 2012 - 2019 FMSH
 *
 *  Liu Lei <liulei@fmsh.com.cn>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/clk/fmsh.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/io.h>

static void __iomem *fmsh_clkc_base;

#define SLCR_ARMPLL_CTRL		(fmsh_clkc_base + 0x00)
#define SLCR_ARMPLL_CLK0_DIV	(fmsh_clkc_base + 0x04)
#define SLCR_ARMPLL_CLK1_DIV	(fmsh_clkc_base + 0x08)
#define SLCR_ARMPLL_CLK2_DIV	(fmsh_clkc_base + 0x0C)
#define SLCR_ARMPLL_CLK3_DIV	(fmsh_clkc_base + 0x20)
#define SLCR_ARMPLL_CLK4_DIV	(fmsh_clkc_base + 0x24)
#define SLCR_ARMPLL_CLK5_DIV	(fmsh_clkc_base + 0x28)
#define SLCR_DDRPLL_CTRL		(fmsh_clkc_base + 0x30)
#define SLCR_DDRPLL_CLK0_DIV	(fmsh_clkc_base + 0x34)
#define SLCR_DDRPLL_CLK1_DIV	(fmsh_clkc_base + 0x38)
#define SLCR_DDRPLL_CLK2_DIV	(fmsh_clkc_base + 0x3C)
#define SLCR_DDRPLL_CLK3_DIV	(fmsh_clkc_base + 0x40)
#define SLCR_DDRPLL_CLK4_DIV	(fmsh_clkc_base + 0x44)
#define SLCR_DDRPLL_CLK5_DIV	(fmsh_clkc_base + 0x48)
#define SLCR_IOPLL_CTRL			(fmsh_clkc_base + 0x50)
#define SLCR_IOPLL_CLK0_DIV		(fmsh_clkc_base + 0x54)
#define SLCR_IOPLL_CLK1_DIV		(fmsh_clkc_base + 0x58)
#define SLCR_IOPLL_CLK2_DIV		(fmsh_clkc_base + 0x5C)
#define SLCR_IOPLL_CLK3_DIV		(fmsh_clkc_base + 0x60)
#define SLCR_IOPLL_CLK4_DIV		(fmsh_clkc_base + 0x64)
#define SLCR_IOPLL_CLK5_DIV		(fmsh_clkc_base + 0x68)
#define SLCR_PLL_STATUS			(fmsh_clkc_base + 0x70)
#define SLCR_ARMPLL_CFG			(fmsh_clkc_base + 0x74)
#define SLCR_DDRPLL_CFG			(fmsh_clkc_base + 0x78)
#define SLCR_IOPLL_CFG			(fmsh_clkc_base + 0x7C)

#define SLCR_621_TRUE			(fmsh_clkc_base + 0x10C)
#define SLCR_ARM_CLK_CTRL		(fmsh_clkc_base + 0x110)
#define SLCR_DDR_CLK_CTRL		(fmsh_clkc_base + 0x118)
#define SLCR_GTIMER_CLK_CTRL	(fmsh_clkc_base + 0x12C)
#define SLCR_FPGA_CLK_CTRL		(fmsh_clkc_base + 0x130)
#define SLCR_GMAC0_CLK_CTRL		(fmsh_clkc_base + 0x168)
#define SLCR_GMAC1_CLK_CTRL		(fmsh_clkc_base + 0x16C)
#define SLCR_SMC_CLK_CTRL		(fmsh_clkc_base + 0x174)
#define SLCR_NFC_CLK_CTRL		(fmsh_clkc_base + 0x17C)
#define SLCR_QSPI_CLK_CTRL		(fmsh_clkc_base + 0x184)
#define SLCR_SDIO_CLK_CTRL		(fmsh_clkc_base + 0x18C)
#define SLCR_UART_CLK_CTRL		(fmsh_clkc_base + 0x194)
#define SLCR_SPI_CLK_CTRL		(fmsh_clkc_base + 0x19C)
#define SLCR_CAN_CLK_CTRL		(fmsh_clkc_base + 0x204)
#define SLCR_GPIO_CLK_CTRL		(fmsh_clkc_base + 0x20C)
#define SLCR_I2C_CLK_CTRL		(fmsh_clkc_base + 0x214)
#define SLCR_USB_CLK_CTRL		(fmsh_clkc_base + 0x21C)
#define SLCR_DMAC_CLK_CTRL		(fmsh_clkc_base + 0x224)
#define SLCR_WDT_CLK_CTRL		(fmsh_clkc_base + 0x22C)
#define SLCR_TTC_CLK_CTRL		(fmsh_clkc_base + 0x234)
#define SLCR_PCAP_CLK_CTRL		(fmsh_clkc_base + 0x23C)

#define PLLCTRL_BPFORCE_SHIFT	(3)

/* the enum must in complete accord the MACRO value in include/dt-bindings/clock/fmsh-clock.h */
enum fmsh_clk {
	armpll, ddrpll, iopll,
//	armpll_clk0, armpll_clk1, armpll_clk2, armpll_clk3, armpll_clk4, armpll_clk5,
//	ddrpll_clk0, ddrpll_clk1, ddrpll_clk2, ddrpll_clk3, ddrpll_clk4, ddrpll_clk5,
//	iopll_clk0, iopll_clk1, iopll_clk2, iopll_clk3, iopll_clk4, iopll_clk5,
	cpu, axi, ahb, apb, axi_cpu,
	ddrx1, ddrx4, axi_ddr, apb_ddr, gtimer,
	gmac0_tx, gmac1_tx, fclk0, fclk1, fclk2, fclk3,
	gmac0_rx, gmac1_rx, axi_gmac0, axi_gmac1, ahb_gmac0, ahb_gmac1,
	ahb_smc, ahb_nfc, nfc, qspi, ahb_qspi, apb_qspi,
	sdio0, sdio1, ahb_sdio0, ahb_sdio1,
	uart0, uart1, apb_uart0, apb_uart1,
	spi0, spi1, apb_spi0, apb_spi1,
	apb_can0, apb_can1, apb_gpio, apb_i2c0, apb_i2c1,
	ahb_usb0, ahb_usb1, usb0_phy, usb1_phy, ahb_dmac, wdt, apb_wdt,
	ttc0_ref1, ttc0_ref2, ttc0_ref3, ttc1_ref1, ttc1_ref2, ttc1_ref3,
	apb_ttc0, apb_ttc1, ahb_pcap,
	clk_max
};

static struct clk *ps_clk;
static struct clk *osc_clk;
static struct clk *clks[clk_max];
static struct clk_onecell_data clk_data;

static DEFINE_SPINLOCK(psclk_lock);
static DEFINE_SPINLOCK(armpll_lock);
static DEFINE_SPINLOCK(ddrpll_lock);
static DEFINE_SPINLOCK(iopll_lock);
static DEFINE_SPINLOCK(armclk_lock);
static DEFINE_SPINLOCK(ddrclk_lock);
// static DEFINE_SPINLOCK(ioclk_lock);
static DEFINE_SPINLOCK(gmac0clk_lock);
static DEFINE_SPINLOCK(gmac1clk_lock);
static DEFINE_SPINLOCK(smcclk_lock);
static DEFINE_SPINLOCK(nfcclk_lock);
static DEFINE_SPINLOCK(qspiclk_lock);
static DEFINE_SPINLOCK(sdioclk_lock);
static DEFINE_SPINLOCK(uartclk_lock);
static DEFINE_SPINLOCK(spiclk_lock);
static DEFINE_SPINLOCK(canclk_lock);
static DEFINE_SPINLOCK(gpioclk_lock);
static DEFINE_SPINLOCK(i2cclk_lock);
static DEFINE_SPINLOCK(usbclk_lock);
static DEFINE_SPINLOCK(dmacclk_lock);
static DEFINE_SPINLOCK(wdtclk_lock);
static DEFINE_SPINLOCK(ttcclk_lock);
static DEFINE_SPINLOCK(pcapclk_lock);
static DEFINE_SPINLOCK(fpgaclk_lock);

#define NUM_MIO_PINS	54

#define DBG_CLK_CTRL_CLKACT_TRC		BIT(0)
#define DBG_CLK_CTRL_CPU_1XCLKACT	BIT(1)

static const char *const armpll_parents[] __initconst = {"armpll_int",
	"ps_clk"};
static const char *const ddrpll_parents[] __initconst = {"ddrpll_int",
	"ps_clk"};
static const char *const iopll_parents[] __initconst = {"iopll_int",
	"ps_clk"};

// static const char *const wdt_ext_clk_input_names[] __initconst = {
//	"wdt_ext_clk"};

#ifdef CONFIG_SUSPEND
static struct clk *iopll_save_parent;

int fmsh_clk_suspend_early(void)
{
	int ret;

	iopll_save_parent = clk_get_parent(clks[iopll]);

	ret = clk_set_parent(clks[iopll], ps_clk);
	if (ret)
		pr_info("%s: reparent iopll failed %d\n", __func__, ret);

	return 0;
}

void fmsh_clk_resume_late(void)
{
	clk_set_parent(clks[iopll], iopll_save_parent);
}
#endif

static void __init fmsh_clk_setup(struct device_node *np)
{
	int i;
	u32 tmp;
	int ret;
	u32 fclk_enable;
	struct clk *clk;
//	char *clk_name;
	const char *clk_output_name[clk_max];
	const char *cpu_parents[4];
	const char *gmactx_parents[4];
	const char *gmacrx_parents[2];
	const char *qspi_parents[2];
	const char *sdio_parents[2];
	const char *uart_parents[2];
	const char *spi_parents[2];
	const char *wdt_parents[4];
	const char *ttc_parents[4];

	pr_info("FMSH clock init\n");

	/* get clock output names from DT */
	for (i = 0; i < clk_max; i++) {
		if (of_property_read_string_index(np, "clock-output-names",
				  i, &clk_output_name[i])) {
			pr_err("%s: clock output name not in DT\n", __func__);
			BUG();
		}
	}

	cpu_parents[0] = "armpll_clk0";
	cpu_parents[1] = "armpll_clk0";
	cpu_parents[2] = "ddrpll_clk0";
	cpu_parents[3] = "iopll_clk0";

//	of_property_read_u32(np, "fclk-enable", &fclk_enable);
	/* osc_clk */
	ret = of_property_read_u32(np, "osc-clk-frequency", &tmp);
	if (ret) {
		pr_warn("osc_clk frequency not specified, using 32.768 KHz.\n");
		tmp = 32768;
	}
	osc_clk = clk_register_fixed_rate(NULL, "osc_clk", NULL, 0, tmp);

	/* ps_clk */
	ret = of_property_read_u32(np, "ps-clk-frequency", &tmp);
	if (ret) {
		pr_warn("ps_clk frequency not specified, using 33 MHz.\n");
		tmp = 33333333;
	}
	ps_clk = clk_register_fixed_rate(NULL, "ps_clk", NULL, 0, tmp);

	/* PLLs */
	clk = clk_register_fmsh_pll("armpll_int", "ps_clk", SLCR_ARMPLL_CTRL,
			SLCR_PLL_STATUS, 0, &armpll_lock);
	clks[armpll] = clk_register_mux(NULL, clk_output_name[armpll],
			armpll_parents, 2, CLK_SET_RATE_NO_REPARENT,
			SLCR_ARMPLL_CTRL, PLLCTRL_BPFORCE_SHIFT, 1, 0, &armpll_lock);

	clk = clk_register_fmsh_pll("ddrpll_int", "ps_clk", SLCR_DDRPLL_CTRL,
			SLCR_PLL_STATUS, 1, &ddrpll_lock);
	clks[ddrpll] = clk_register_mux(NULL, clk_output_name[ddrpll],
			ddrpll_parents, 2, CLK_SET_RATE_NO_REPARENT,
			SLCR_DDRPLL_CTRL, PLLCTRL_BPFORCE_SHIFT, 1, 0, &ddrpll_lock);

	clk = clk_register_fmsh_pll("iopll_int", "ps_clk", SLCR_IOPLL_CTRL,
			SLCR_PLL_STATUS, 2, &iopll_lock);
	clks[iopll] = clk_register_mux(NULL, clk_output_name[iopll],
			iopll_parents, 2, CLK_SET_RATE_NO_REPARENT,
			SLCR_IOPLL_CTRL, PLLCTRL_BPFORCE_SHIFT, 1, 0, &iopll_lock);

	/* PLL DIV clocks */
	clk = clk_register_divider(NULL, "armpll_clk0", "armpll", 0,
			SLCR_ARMPLL_CLK0_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&armclk_lock);
	clk = clk_register_divider(NULL, "armpll_clk1", "armpll", 0,
			SLCR_ARMPLL_CLK1_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&armclk_lock);
	clk = clk_register_divider(NULL, "armpll_clk2", "armpll", 0,
			SLCR_ARMPLL_CLK2_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&armclk_lock);
	clk = clk_register_divider(NULL, "armpll_clk3", "armpll", 0,
			SLCR_ARMPLL_CLK3_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&armclk_lock);
	clk = clk_register_divider(NULL, "armpll_clk4", "armpll", 0,
			SLCR_ARMPLL_CLK4_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&armclk_lock);
	clk = clk_register_divider(NULL, "armpll_clk5", "armpll", 0,
			SLCR_ARMPLL_CLK5_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&armclk_lock);

	clk = clk_register_divider(NULL, "ddrpll_clk0", "ddrpll", 0,
			SLCR_DDRPLL_CLK0_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&ddrclk_lock);
	clk = clk_register_divider(NULL, "ddrpll_clk1", "ddrpll", 0,
			SLCR_DDRPLL_CLK1_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&ddrclk_lock);

	clk = clk_register_divider(NULL, "ddrpll_clk5", "ddrpll", 0,
			SLCR_DDRPLL_CLK2_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&ddrclk_lock);
	clk = clk_register_divider(NULL, "fclk0_div", "ddrpll_clk2", 0,
			SLCR_FPGA_CLK_CTRL, 4, 6, CLK_DIVIDER_ONE_BASED,
			&fpgaclk_lock);
	clks[fclk0] = clk_register_gate(NULL, clk_output_name[fclk0],
			"fclk0_div", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED | CLK_IS_CRITICAL,
			SLCR_FPGA_CLK_CTRL, 0, 0, &fpgaclk_lock);

	clk = clk_register_divider(NULL, "ddrpll_clk4", "ddrpll", 0,
			SLCR_DDRPLL_CLK3_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&ddrclk_lock);
	clk = clk_register_divider(NULL, "fclk1_div", "ddrpll_clk3", 0,
			SLCR_FPGA_CLK_CTRL, 10, 6, CLK_DIVIDER_ONE_BASED,
			&fpgaclk_lock);
	clks[fclk1] = clk_register_gate(NULL, clk_output_name[fclk1],
			"fclk1_div", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED | CLK_IS_CRITICAL,
			SLCR_FPGA_CLK_CTRL, 1, 0, &fpgaclk_lock);

	clk = clk_register_divider(NULL, "ddrpll_clk3", "ddrpll", 0,
			SLCR_DDRPLL_CLK4_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&ddrclk_lock);
	clk = clk_register_divider(NULL, "fclk2_div", "ddrpll_clk4", 0,
			SLCR_FPGA_CLK_CTRL, 16, 6, CLK_DIVIDER_ONE_BASED,
			&fpgaclk_lock);
	clks[fclk2] = clk_register_gate(NULL, clk_output_name[fclk2],
			"fclk2_div", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED | CLK_IS_CRITICAL,
			SLCR_FPGA_CLK_CTRL, 2, 0, &fpgaclk_lock);

	clk = clk_register_divider(NULL, "ddrpll_clk2", "ddrpll", 0,
			SLCR_DDRPLL_CLK5_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&ddrclk_lock);
	clk = clk_register_divider(NULL, "fclk3_div", "ddrpll_clk5", 0,
			SLCR_FPGA_CLK_CTRL, 22, 6, CLK_DIVIDER_ONE_BASED,
			&fpgaclk_lock);
	clks[fclk3] = clk_register_gate(NULL, clk_output_name[fclk3],
			"fclk3_div", CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED | CLK_IS_CRITICAL,
			SLCR_FPGA_CLK_CTRL, 3, 0, &fpgaclk_lock);

	of_property_read_u32(np, "fclk-enable", &fclk_enable);
	for (i = fclk0; i <= fclk3; i++) {
		int enable = !!(fclk_enable & BIT(i - fclk0));
		if(enable)
			clk_prepare_enable(clks[i]);
	}

	clk = clk_register_divider(NULL, "iopll_clk0", "iopll", 0,
			SLCR_IOPLL_CLK0_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&ddrclk_lock);
	clk = clk_register_divider(NULL, "iopll_clk1", "iopll", 0,
			SLCR_IOPLL_CLK1_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&ddrclk_lock);
	clk = clk_register_divider(NULL, "iopll_clk2", "iopll", 0,
			SLCR_IOPLL_CLK2_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&ddrclk_lock);
	clk = clk_register_divider(NULL, "iopll_clk3", "iopll", 0,
			SLCR_IOPLL_CLK3_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&ddrclk_lock);
	clk = clk_register_divider(NULL, "iopll_clk4", "iopll", 0,
			SLCR_IOPLL_CLK4_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&ddrclk_lock);
	clk = clk_register_divider(NULL, "iopll_clk5", "iopll", 0,
			SLCR_IOPLL_CLK5_DIV, 0, 7, CLK_DIVIDER_ONE_BASED,
			&ddrclk_lock);

	/* CPU AXI AHB APB clocks */
	tmp = readl(SLCR_621_TRUE) & 1;
	clk = clk_register_mux(NULL, "cpu_mux", cpu_parents, 4,
			0, SLCR_ARM_CLK_CTRL, 0, 2, 0, &armclk_lock);

	clks[cpu] = clk_register_gate(NULL, clk_output_name[cpu], "cpu_mux",
			CLK_SET_RATE_NO_REPARENT | CLK_IGNORE_UNUSED,
			SLCR_ARM_CLK_CTRL, 9, 0, &armclk_lock);

	clks[axi] = clk_register_fixed_factor(NULL, clk_output_name[axi], clk_output_name[cpu],
			0, 1, 2 + tmp);
	clks[ahb] = clk_register_fixed_factor(NULL, clk_output_name[ahb], clk_output_name[axi],
			0, 1, 2);
	clks[apb] = clk_register_fixed_factor(NULL, clk_output_name[apb], clk_output_name[ahb],
			0, 1, 2);

	clks[axi_cpu] = clk_register_gate(NULL, clk_output_name[axi_cpu],
			clk_output_name[axi], CLK_IGNORE_UNUSED,
			SLCR_ARM_CLK_CTRL, 8, 0, &armclk_lock);

	/* DDR clocks */
	clks[ddrx1] = clk_register_gate(NULL, clk_output_name[ddrx1], "ddrpll_clk1",
			CLK_IGNORE_UNUSED, SLCR_DDR_CLK_CTRL, 0, 0, &ddrclk_lock);

	clks[apb_ddr] = clk_register_gate(NULL, clk_output_name[apb_ddr], clk_output_name[apb],
			0, SLCR_DDR_CLK_CTRL, 1, 0, &ddrclk_lock);

	clks[axi_ddr] = clk_register_gate(NULL, clk_output_name[axi_ddr], clk_output_name[axi],
			CLK_IGNORE_UNUSED, SLCR_DDR_CLK_CTRL, 2, 0, &ddrclk_lock);

	clks[ddrx4] = clk_register_fixed_factor(NULL, clk_output_name[ddrx4], clk_output_name[ddrx1],
			0, 1, 4);
	clk_prepare_enable(clks[ddrx1]);
	clk_prepare_enable(clks[axi_ddr]);
	clk_prepare_enable(clks[apb_ddr]);

	/* Timers clocks */
	clks[gtimer] = clk_register_divider(NULL, clk_output_name[gtimer], "ps_clk", CLK_IGNORE_UNUSED,
			SLCR_GTIMER_CLK_CTRL, 0, 6, CLK_DIVIDER_ONE_BASED, &psclk_lock);
	clk_prepare_enable(clks[gtimer]);

	/* GMAC clocks */
	gmactx_parents[0] = "iopll_clk1";
	gmactx_parents[1] = "armpll_clk1";
	gmactx_parents[2] = "gmactx_emio";
	gmactx_parents[3] = "gmactx_emio";
	gmacrx_parents[0] = "gmacrx_mio";
	gmacrx_parents[1] = "gmacrx_emio";
	clk = clk_register_mux(NULL, "gmac0tx_mux", gmactx_parents, 4,
			CLK_SET_RATE_NO_REPARENT, SLCR_GMAC0_CLK_CTRL, 6, 2, 0, &gmac0clk_lock);
	clk = clk_register_mux(NULL, "gmac0rx_mux", gmacrx_parents, 2,
			CLK_SET_RATE_NO_REPARENT, SLCR_GMAC0_CLK_CTRL, 5, 1, 0, &gmac0clk_lock);

	clk = clk_register_divider(NULL, "gmac0tx_div", "gmac0tx_mux", 0,
			SLCR_GMAC0_CLK_CTRL, 9, 6, CLK_DIVIDER_ONE_BASED, &gmac0clk_lock);

	clks[gmac0_tx] = clk_register_gate(NULL, clk_output_name[gmac0_tx], "gmac0tx_div",
			CLK_SET_RATE_PARENT, SLCR_GMAC0_CLK_CTRL, 0, 0, &gmac0clk_lock);

	clks[gmac0_rx] = clk_register_gate(NULL, clk_output_name[gmac0_rx], "gmac0rx_mux",
			0, SLCR_GMAC0_CLK_CTRL, 1, 0, &gmac0clk_lock);

	clks[axi_gmac0] = clk_register_gate(NULL, clk_output_name[axi_gmac0], "axi",
			0, SLCR_GMAC0_CLK_CTRL, 3, 0, &gmac0clk_lock);

	clks[ahb_gmac0] = clk_register_gate(NULL, clk_output_name[ahb_gmac0], "ahb",
			0, SLCR_GMAC0_CLK_CTRL, 4, 0, &gmac0clk_lock);

	clk = clk_register_mux(NULL, "gmac1tx_mux", gmactx_parents, 4,
			0, SLCR_GMAC1_CLK_CTRL, 6, 2, 0, &gmac1clk_lock);
	clk = clk_register_mux(NULL, "gmac1rx_mux", gmacrx_parents, 2,
			0, SLCR_GMAC1_CLK_CTRL, 5, 1, 0, &gmac1clk_lock);

	clk = clk_register_divider(NULL, "gmac1tx_div", "gmac1tx_mux", 0,
			SLCR_GMAC1_CLK_CTRL, 9, 6, CLK_DIVIDER_ONE_BASED, &gmac1clk_lock);

	clks[gmac1_tx] = clk_register_gate(NULL, clk_output_name[gmac1_tx], "gmac1tx_div",
			CLK_SET_RATE_PARENT, SLCR_GMAC1_CLK_CTRL, 0, 0, &gmac1clk_lock);

	clks[gmac1_rx] = clk_register_gate(NULL, clk_output_name[gmac1_rx], "gmac1rx_mux",
			0, SLCR_GMAC1_CLK_CTRL, 1, 0, &gmac1clk_lock);

	clks[axi_gmac1] = clk_register_gate(NULL, clk_output_name[axi_gmac1], "axi",
			0, SLCR_GMAC1_CLK_CTRL, 3, 0, &gmac1clk_lock);

	clks[ahb_gmac1] = clk_register_gate(NULL, clk_output_name[ahb_gmac1], "ahb",
			0, SLCR_GMAC1_CLK_CTRL, 4, 0, &gmac1clk_lock);

	/* SMC clocks */
	/* ignore unused, depend on firmware(uboot) initialization */
	clks[ahb_smc] = clk_register_gate(NULL, clk_output_name[ahb_smc], "ahb",
			CLK_IGNORE_UNUSED, SLCR_SMC_CLK_CTRL, 0, 0, &smcclk_lock);

	/* NFC clocks */
	clk = clk_register_divider(NULL, "nfc_div", "ahb",
			0, SLCR_NFC_CLK_CTRL, 4, 6,
			CLK_DIVIDER_ONE_BASED, &nfcclk_lock);

	clks[nfc] = clk_register_gate(NULL, clk_output_name[nfc], "nfc_div",
			0, SLCR_NFC_CLK_CTRL, 0, 0, &nfcclk_lock);

	clks[ahb_nfc] = clk_register_gate(NULL, clk_output_name[ahb_nfc], "ahb",
			0, SLCR_NFC_CLK_CTRL, 1, 0, &nfcclk_lock);

	/* QSPI clocks */
	qspi_parents[0] = "iopll_clk4";
	qspi_parents[1] = "armpll_clk4";
	clk = clk_register_mux(NULL, "qspi_mux", qspi_parents, 2,
			CLK_SET_RATE_NO_REPARENT, SLCR_QSPI_CLK_CTRL, 4, 1, 0, &qspiclk_lock);

	clks[qspi] = clk_register_gate(NULL, clk_output_name[qspi], "qspi_mux",
			0, SLCR_QSPI_CLK_CTRL, 0, 0, &qspiclk_lock);

	clks[ahb_qspi] = clk_register_gate(NULL, clk_output_name[ahb_qspi], "ahb",
			0, SLCR_QSPI_CLK_CTRL, 2, 0, &qspiclk_lock);

	clks[apb_qspi] = clk_register_gate(NULL, clk_output_name[apb_qspi], "apb",
			0, SLCR_QSPI_CLK_CTRL, 1, 0, &qspiclk_lock);

	/* SDIO clocks */
	sdio_parents[0] = "iopll_clk2";
	sdio_parents[1] = "armpll_clk2";
	clk = clk_register_mux(NULL, "sdio_mux", sdio_parents, 2,
			CLK_SET_RATE_NO_REPARENT, SLCR_SDIO_CLK_CTRL, 4, 1, 0, &sdioclk_lock);

	clks[sdio0] = clk_register_gate(NULL, clk_output_name[sdio0], "sdio_mux",
			0, SLCR_SDIO_CLK_CTRL, 0, 0, &sdioclk_lock);
	clks[sdio1] = clk_register_gate(NULL, clk_output_name[sdio1], "sdio_mux",
			0, SLCR_SDIO_CLK_CTRL, 1, 0, &sdioclk_lock);

	clks[ahb_sdio0] = clk_register_gate(NULL, clk_output_name[ahb_sdio0], "ahb",
			0, SLCR_SDIO_CLK_CTRL, 2, 0, &sdioclk_lock);
	clks[ahb_sdio1] = clk_register_gate(NULL, clk_output_name[ahb_sdio1], "ahb",
			0, SLCR_SDIO_CLK_CTRL, 3, 0, &sdioclk_lock);

	/* UART clocks */
	uart_parents[0] = "iopll_clk5";
	uart_parents[1] = "armpll_clk5";
	clk = clk_register_mux(NULL, "uart_mux", uart_parents, 2,
			CLK_SET_RATE_NO_REPARENT, SLCR_UART_CLK_CTRL, 4, 1, 0, &uartclk_lock);

	clks[uart0] = clk_register_gate(NULL, clk_output_name[uart0], "uart_mux",
			0, SLCR_UART_CLK_CTRL, 0, 0, &uartclk_lock);
	clks[uart1] = clk_register_gate(NULL, clk_output_name[uart1], "uart_mux",
			0, SLCR_UART_CLK_CTRL, 1, 0, &uartclk_lock);

	clks[apb_uart0] = clk_register_gate(NULL, clk_output_name[apb_uart0], "apb",
			0, SLCR_UART_CLK_CTRL, 2, 0, &uartclk_lock);
	clks[apb_uart1] = clk_register_gate(NULL, clk_output_name[apb_uart1], "apb",
			0, SLCR_UART_CLK_CTRL, 3, 0, &uartclk_lock);

	/* SPI clocks */
	spi_parents[0] = "iopll_clk3";
	spi_parents[1] = "armpll_clk3";
	clk = clk_register_mux(NULL, "spi_mux", spi_parents, 2,
			CLK_SET_RATE_NO_REPARENT, SLCR_SPI_CLK_CTRL, 4, 1, 0, &spiclk_lock);

	clks[spi0] = clk_register_gate(NULL, clk_output_name[spi0], "spi_mux",
			0, SLCR_SPI_CLK_CTRL, 0, 0, &spiclk_lock);
	clks[spi1] = clk_register_gate(NULL, clk_output_name[spi1], "spi_mux",
			0, SLCR_SPI_CLK_CTRL, 1, 0, &spiclk_lock);

	clks[apb_spi0] = clk_register_gate(NULL, clk_output_name[apb_spi0], "apb",
			0, SLCR_SPI_CLK_CTRL, 2, 0, &spiclk_lock);
	clks[apb_spi1] = clk_register_gate(NULL, clk_output_name[apb_spi1], "apb",
			0, SLCR_SPI_CLK_CTRL, 3, 0, &spiclk_lock);

	/* CAN clocks */
	clks[apb_can0] = clk_register_gate(NULL, clk_output_name[apb_can0], "apb",
			0, SLCR_CAN_CLK_CTRL, 0, 0, &canclk_lock);
	clks[apb_can1] = clk_register_gate(NULL, clk_output_name[apb_can1], "apb",
			0, SLCR_CAN_CLK_CTRL, 1, 0, &canclk_lock);

	/* GPIO clocks */
	clks[apb_gpio] = clk_register_gate(NULL, clk_output_name[apb_gpio], "apb",
			0, SLCR_GPIO_CLK_CTRL, 0, 0, &gpioclk_lock);
	clk_prepare_enable(clks[apb_gpio]);

	/* I2C clocks */
	clks[apb_i2c0] = clk_register_gate(NULL, clk_output_name[apb_i2c0], "apb",
			0, SLCR_I2C_CLK_CTRL, 0, 0, &i2cclk_lock);
	clks[apb_i2c1] = clk_register_gate(NULL, clk_output_name[apb_i2c1], "apb",
			0, SLCR_I2C_CLK_CTRL, 1, 0, &i2cclk_lock);

	/* USB clocks */
	clks[ahb_usb0] = clk_register_gate(NULL, clk_output_name[ahb_usb0], "ahb",
			0, SLCR_USB_CLK_CTRL, 0, 0, &usbclk_lock);
	clks[ahb_usb1] = clk_register_gate(NULL, clk_output_name[ahb_usb1], "ahb",
			0, SLCR_USB_CLK_CTRL, 1, 0, &usbclk_lock);
	clks[usb0_phy] = clk_register_gate(NULL, clk_output_name[usb0_phy], "usb_mio",
			0, SLCR_USB_CLK_CTRL, 2, 0, &usbclk_lock);
	clks[usb1_phy] = clk_register_gate(NULL, clk_output_name[usb1_phy], "usb_mio",
			0, SLCR_USB_CLK_CTRL, 3, 0, &usbclk_lock);

	/* DMAC clocks */
	clks[ahb_dmac] = clk_register_gate(NULL, clk_output_name[ahb_dmac], "ahb",
			0, SLCR_DMAC_CLK_CTRL, 0, 0, &dmacclk_lock);

	/* WDT clocks */
	wdt_parents[0] = "apb";
	wdt_parents[1] = "osc_clk";
	wdt_parents[2] = "wdt_ext_io";
	wdt_parents[3] = "wdt_ext_io";
	clk = clk_register_mux(NULL, "wdt_mux", wdt_parents, 4,
			CLK_SET_RATE_NO_REPARENT, SLCR_WDT_CLK_CTRL, 0, 2, 0, &wdtclk_lock);

	clks[wdt] = clk_register_gate(NULL, clk_output_name[wdt], "wdt_mux",
			0, SLCR_WDT_CLK_CTRL, 3, 0, &wdtclk_lock);

	clks[apb_wdt] = clk_register_gate(NULL, clk_output_name[apb_wdt], "apb",
			0, SLCR_WDT_CLK_CTRL, 2, 0, &wdtclk_lock);

	/* TTC clocks */
	clks[apb_ttc0] = clk_register_gate(NULL, clk_output_name[apb_ttc0], "apb",
			0, SLCR_TTC_CLK_CTRL, 12, 0, &ttcclk_lock);
	clks[apb_ttc1] = clk_register_gate(NULL, clk_output_name[apb_ttc1], "apb",
			0, SLCR_TTC_CLK_CTRL, 13, 0, &ttcclk_lock);
	ttc_parents[0] = "apb";
	ttc_parents[1] = "osc_clk";
	ttc_parents[2] = "ttc_ext_io";
	ttc_parents[3] = "ttc_ext_io";
	clk = clk_register_mux(NULL, "ttc0_ref1_mux", ttc_parents, 4,
			CLK_SET_RATE_NO_REPARENT, SLCR_TTC_CLK_CTRL, 0, 2, 0, &ttcclk_lock);
	clk = clk_register_mux(NULL, "ttc0_ref2_mux", ttc_parents, 4,
			CLK_SET_RATE_NO_REPARENT, SLCR_TTC_CLK_CTRL, 2, 2, 0, &ttcclk_lock);
	clk = clk_register_mux(NULL, "ttc0_ref3_mux", ttc_parents, 4,
			CLK_SET_RATE_NO_REPARENT, SLCR_TTC_CLK_CTRL, 4, 2, 0, &ttcclk_lock);
	clk = clk_register_mux(NULL, "ttc1_ref1_mux", ttc_parents, 4,
			CLK_SET_RATE_NO_REPARENT, SLCR_TTC_CLK_CTRL, 6, 2, 0, &ttcclk_lock);
	clk = clk_register_mux(NULL, "ttc1_ref2_mux", ttc_parents, 4,
			CLK_SET_RATE_NO_REPARENT, SLCR_TTC_CLK_CTRL, 8, 2, 0, &ttcclk_lock);
	clk = clk_register_mux(NULL, "ttc1_ref3_mux", ttc_parents, 4,
			CLK_SET_RATE_NO_REPARENT, SLCR_TTC_CLK_CTRL, 10, 2, 0, &ttcclk_lock);
	clks[ttc0_ref1] = clk_register_gate(NULL, clk_output_name[ttc0_ref1], "ttc0_ref1_mux",
			0, SLCR_TTC_CLK_CTRL, 14, 0, &ttcclk_lock);
	clks[ttc0_ref2] = clk_register_gate(NULL, clk_output_name[ttc0_ref2], "ttc0_ref2_mux",
			0, SLCR_TTC_CLK_CTRL, 15, 0, &ttcclk_lock);
	clks[ttc0_ref3] = clk_register_gate(NULL, clk_output_name[ttc0_ref3], "ttc0_ref3_mux",
			0, SLCR_TTC_CLK_CTRL, 16, 0, &ttcclk_lock);
	clks[ttc1_ref1] = clk_register_gate(NULL, clk_output_name[ttc1_ref1], "ttc1_ref1_mux",
			0, SLCR_TTC_CLK_CTRL, 17, 0, &ttcclk_lock);
	clks[ttc1_ref2] = clk_register_gate(NULL, clk_output_name[ttc1_ref2], "ttc1_ref2_mux",
			0, SLCR_TTC_CLK_CTRL, 18, 0, &ttcclk_lock);
	clks[ttc1_ref3] = clk_register_gate(NULL, clk_output_name[ttc1_ref3], "ttc1_ref3_mux",
			0, SLCR_TTC_CLK_CTRL, 19, 0, &ttcclk_lock);


	/* PACP clocks */
	clks[ahb_pcap] = clk_register_gate(NULL, clk_output_name[ahb_pcap], "ahb",
			0, SLCR_PCAP_CLK_CTRL, 0, 0, &pcapclk_lock);

	for (i = 0; i < ARRAY_SIZE(clks); i++) {
		if (IS_ERR(clks[i])) {
			pr_err("FMQL clk %d: register failed with %ld\n",
			       i, PTR_ERR(clks[i]));
			BUG();
		}
	}

	clk_data.clks = clks;
	clk_data.clk_num = ARRAY_SIZE(clks);
	of_clk_add_provider(np, of_clk_src_onecell_get, &clk_data);
}

CLK_OF_DECLARE(fmsh_clkc, "fmsh,psoc-clkc", fmsh_clk_setup);

void __init fmsh_clock_init(void)
{
	struct device_node *np;
	struct device_node *slcr;
	struct resource res;

	np = of_find_compatible_node(NULL, NULL, "fmsh,psoc-clkc");
	if (!np) {
		pr_err("%s: clkc node not found\n", __func__);
		goto np_err;
	}

	if (of_address_to_resource(np, 0, &res)) {
		pr_err("%s: failed to get resource\n", np->name);
		goto np_err;
	}

	slcr = of_get_parent(np);

	if (slcr->data) {
		fmsh_clkc_base = (__force void __iomem *)slcr->data + res.start;
	} else {
		pr_err("%s: Unable to get I/O memory\n", np->name);
		of_node_put(slcr);
		goto np_err;
	}

	pr_info("%s: clkc starts at %px\n", __func__, fmsh_clkc_base);

	of_node_put(slcr);
	of_node_put(np);

	return;

np_err:
	of_node_put(np);
	BUG();
}

