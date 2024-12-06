/*
 * FMSH PLL driver
 *
 *  Copyright (C) 2018 FMSH
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
 *
 */
#include <linux/clk/fmsh.h>
#include <linux/clk-provider.h>
#include <linux/slab.h>
#include <linux/io.h>

/**
 * struct fmsh_pll
 * @hw:		Handle between common and hardware-specific interfaces
 * @pll_ctrl:	PLL control register
 * @pll_status:	PLL status register
 * @lock:	Register lock
 * @lockbit:	Indicates the associated PLL_LOCKED bit in the PLL status
 *		register.
 */
struct fmsh_pll {
	struct clk_hw	hw;
	void __iomem	*pll_ctrl;
	void __iomem	*pll_status;
	spinlock_t	*lock;
	u8		lockbit;
};
#define to_fmsh_pll(_hw)	container_of(_hw, struct fmsh_pll, hw)

/* Register bitfield defines */
#define PLLCTRL_FDIV_MASK	0x7f0000
#define PLLCTRL_FDIV_SHIFT	16
#define PLLCTRL_BPFORCE_MASK	(1 << 3)
#define PLLCTRL_BPQUAL_MASK	(1 << 2)
#define PLLCTRL_PWRDWN_MASK	(1 << 1)
#define PLLCTRL_PWRDWN_SHIFT	1
#define PLLCTRL_RESET_MASK	1
#define PLLCTRL_RESET_SHIFT	0

#define PLL_FDIV_MIN	1
#define PLL_FDIV_MAX	0x7f

/**
 * fmsh_pll_round_rate() - Round a clock frequency
 * @hw:		Handle between common and hardware-specific interfaces
 * @rate:	Desired clock frequency
 * @prate:	Clock frequency of parent clock
 * Returns frequency closest to @rate the hardware can generate.
 */
static long fmsh_pll_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *prate)
{
	u32 fdiv;

	fdiv = DIV_ROUND_CLOSEST(rate, *prate);
	if (fdiv < PLL_FDIV_MIN)
		fdiv = PLL_FDIV_MIN;
	else if (fdiv > PLL_FDIV_MAX)
		fdiv = PLL_FDIV_MAX;

	return *prate * fdiv;
}

/**
 * fmsh_pll_recalc_rate() - Recalculate clock frequency
 * @hw:			Handle between common and hardware-specific interfaces
 * @parent_rate:	Clock frequency of parent clock
 * Returns current clock frequency.
 */
static unsigned long fmsh_pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct fmsh_pll *clk = to_fmsh_pll(hw);
	u32 fdiv;

	/*
	 * makes probably sense to redundantly save fdiv in the struct
	 * fmsh_pll to save the IO access.
	 */
	fdiv = (readl(clk->pll_ctrl) & PLLCTRL_FDIV_MASK) >>
			PLLCTRL_FDIV_SHIFT;

	return parent_rate * fdiv;
}

/**
 * fmsh_pll_is_enabled - Check if a clock is enabled
 * @hw:		Handle between common and hardware-specific interfaces
 * Returns 1 if the clock is enabled, 0 otherwise.
 *
 * Not sure this is a good idea, but since disabled means bypassed for
 * this clock implementation we say we are always enabled.
 */
static int fmsh_pll_is_enabled(struct clk_hw *hw)
{
	unsigned long flags = 0;
	u32 reg;
	struct fmsh_pll *clk = to_fmsh_pll(hw);

	spin_lock_irqsave(clk->lock, flags);

	reg = readl(clk->pll_ctrl);

	spin_unlock_irqrestore(clk->lock, flags);

	return !(reg & (PLLCTRL_RESET_MASK | PLLCTRL_PWRDWN_MASK));
}

/**
 * fmsh_pll_enable - Enable clock
 * @hw:		Handle between common and hardware-specific interfaces
 * Returns 0 on success
 */
static int fmsh_pll_enable(struct clk_hw *hw)
{
	unsigned long flags = 0;
	u32 reg;
	struct fmsh_pll *clk = to_fmsh_pll(hw);

	if (fmsh_pll_is_enabled(hw))
		return 0;

	/*
	 * print information maybe cause earlyprintk issue because
	 * the clk of uart is disabled
	 */
	/* pr_info("PLL: enable\n"); */

	/* Power up PLL and wait for lock */
	spin_lock_irqsave(clk->lock, flags);

	reg = readl(clk->pll_ctrl);
	reg &= ~(PLLCTRL_RESET_MASK | PLLCTRL_PWRDWN_MASK);
	writel(reg, clk->pll_ctrl);
	while (!(readl(clk->pll_status) & (1 << clk->lockbit)))
		;

	spin_unlock_irqrestore(clk->lock, flags);

	return 0;
}

/**
 * fmsh_pll_disable - Disable clock
 * @hw:		Handle between common and hardware-specific interfaces
 * Returns 0 on success
 */
static void fmsh_pll_disable(struct clk_hw *hw)
{
	unsigned long flags = 0;
	u32 reg;
	struct fmsh_pll *clk = to_fmsh_pll(hw);

	if (!fmsh_pll_is_enabled(hw))
		return;

	/*
	 * print information maybe cause earlyprintk issue because
	 * the clk of uart is disabled
	 */
	/* pr_info("PLL: shutdown\n"); */

	/* shut down PLL */
	spin_lock_irqsave(clk->lock, flags);

	reg = readl(clk->pll_ctrl);
	reg |= PLLCTRL_RESET_MASK | PLLCTRL_PWRDWN_MASK;
	writel(reg, clk->pll_ctrl);

	spin_unlock_irqrestore(clk->lock, flags);
}

static const struct clk_ops fmsh_pll_ops = {
	.enable = fmsh_pll_enable,
	.disable = fmsh_pll_disable,
	.is_enabled = fmsh_pll_is_enabled,
	.round_rate = fmsh_pll_round_rate,
	.recalc_rate = fmsh_pll_recalc_rate
};

/**
 * clk_register_fmsh_pll() - Register PLL with the clock framework
 * @name	PLL name
 * @parent	Parent clock name
 * @pll_ctrl	Pointer to PLL control register
 * @pll_status	Pointer to PLL status register
 * @lock_index	Bit index to this PLL's lock status bit in @pll_status
 * @lock	Register lock
 * Returns handle to the registered clock.
 */
struct clk *clk_register_fmsh_pll(const char *name, const char *parent,
		void __iomem *pll_ctrl, void __iomem *pll_status, u8 lock_index,
		spinlock_t *lock)
{
	struct fmsh_pll *pll;
	struct clk *clk;
	u32 reg;
	const char *parent_arr[1] = {parent};
	unsigned long flags = 0;
	struct clk_init_data initd = {
		.name = name,
		.parent_names = parent_arr,
		.ops = &fmsh_pll_ops,
		.num_parents = 1,
		.flags = 0
	};

	pll = kmalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	/* Populate the struct */
	pll->hw.init = &initd;
	pll->pll_ctrl = pll_ctrl;
	pll->pll_status = pll_status;
	pll->lockbit = lock_index;
	pll->lock = lock;

	spin_lock_irqsave(pll->lock, flags);

	reg = readl(pll->pll_ctrl);
	reg &= ~PLLCTRL_BPQUAL_MASK; /* set bypass by bypass_force bit */
	writel(reg, pll->pll_ctrl);

	spin_unlock_irqrestore(pll->lock, flags);

	clk = clk_register(NULL, &pll->hw);
	if (WARN_ON(IS_ERR(clk)))
		goto free_pll;

	return clk;

free_pll:
	kfree(pll);

	return clk;
}
