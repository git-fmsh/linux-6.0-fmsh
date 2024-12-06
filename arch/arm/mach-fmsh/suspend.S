/*
 * Suspend support for fmsh
 *
 * Copyright (C) 2011-2013 FMSH
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

#include <linux/linkage.h>

#define SLCR_LOCK_OFFS		0x004
#define SLCR_UNLOCK_OFFS	0x008
#define ARMPLL_CTRL_OFFS	0x100
#define DDRPLL_CTRL_OFFS	0x130
#define PLLSTATUS_OFFS		0x170
#define DDR_CLK_CTRL_OFFS	0x218

#define MODE_STS_OFFS		0x004
#define PWRCTL_OFFS		0x030
#define PSTAT_OFFS		0x3FC
#define PCTRL_AXI_PORT4_OFFS	0x750 // 0x490 + 0xB0 * 4 = 0x750

#define PIR_OFFS		0x004
#define PGSR0_OFFS		0x034

#define PLL_RESET_MASK		(1 << 0)
#define PLL_PWRDWN_MASK		(1 << 1)
#define PLL_BYPASS_MASK		(1 << 3)
#define ARM_LOCK_MASK		(1 << 0)
#define DDR_LOCK_MASK		(1 << 1)
#define UNLOCK_KEY_MASK		0xDF0D767B
#define DDRC_SR_TYPE_MASK	0x30
#define SELFREF_SW_MASK		(1 << 5)
#define DDR_AXI_CLK_MASK	(1 << 2)
#define DDR_APB_CLK_MASK	(1 << 1)
#define DDR_CLK_MASK		(1 << 0)
#define PIR_PLLINIT_MASK	(1 << 4)
#define PIR_INIT_MASK		(1 << 0)

#define DDRC_SR_TYPE_SW		0x20
#define MAXTRIES		100

	.text
	.align 3

/**
 * fmsh_sys_suspend - Enter suspend
 * r0 @ddrc_umc_base: Base address of the DDRC UMC module
 * r1 @slcr_base:     Base address of the SLCR
 * r2 @ddrc_pub_base: Base address of the DDRC PUB module
 * Returns -1 if DRAM subsystem is not gated off, 0 otherwise.
 *
 * This function is moved into OCM and finishes the suspend operation. I.e. DDR
 * related clocks are gated off and the DDR PLL is bypassed.
 */
ENTRY(fmsh_sys_suspend)
	push	{r4 - r7}

	/* Blocks AXI ports from taking anymore transactions */
	ldr	r5, =0x0
	str	r5, [r0, #PCTRL_AXI_PORT4_OFFS]
	dmb	st

	/* Waits until all AXI ports are idle */
	mov	r4, #MAXTRIES
1:	subs	r4, #1
	movweq	r4, #0xff00
	beq	suspend
	dsb	sy
	ldr	r5, [r0, #PSTAT_OFFS]
	cmp	r5, #0x0
	bne	1b
	dsb	sy

	/* Enable self refresh mode */
	ldr	r5, [r0, #PWRCTL_OFFS]
	orr	r5, #SELFREF_SW_MASK
	str	r5, [r0, #PWRCTL_OFFS]
	dmb	st

	/* Waits until self refresh state is entered */
	mov	r4, #MAXTRIES
1:	subs	r4, #1
	movweq	r4, #0xff00
	beq	suspend
	dsb	sy
	ldr	r5, [r0, #MODE_STS_OFFS]
	and	r5, #DDRC_SR_TYPE_MASK
	cmp	r5, #DDRC_SR_TYPE_SW
	bne	1b
	dsb	sy

	/* Remove AXI clocks, APB Clock */
	ldr	r5, [r1, #DDR_CLK_CTRL_OFFS]
	bic	r5, #(DDR_AXI_CLK_MASK | DDR_APB_CLK_MASK)
	str	r5, [r1, #DDR_CLK_CTRL_OFFS]
	dmb	st

	/* Remove DDRC clock */
	ldr	r5, [r1, #DDR_CLK_CTRL_OFFS]
	bic	r5, #DDR_CLK_MASK
	str	r5, [r1, #DDR_CLK_CTRL_OFFS]
	dmb	st

	/* Bypass and powerdown DDR PLL */
	ldr	r5, [r1, #DDRPLL_CTRL_OFFS]
	orr	r5, #PLL_BYPASS_MASK
	str	r5, [r1, #DDRPLL_CTRL_OFFS]
	orr	r5, #(PLL_PWRDWN_MASK | PLL_RESET_MASK)
	str	r5, [r1, #DDRPLL_CTRL_OFFS]

	/* Bypass and powerdown ARM PLL */
	ldr	r5, [r1, #ARMPLL_CTRL_OFFS]
	orr	r5, #PLL_BYPASS_MASK
	str	r5, [r1, #ARMPLL_CTRL_OFFS]
	orr	r5, #(PLL_PWRDWN_MASK | PLL_RESET_MASK)
	str	r5, [r1, #ARMPLL_CTRL_OFFS]

suspend:
	dsb	sy
	wfi
	dsb	sy
	cmp	r4, #0xff00
	beq	exit

	/* Power up ARM PLL */
	ldr	r5, [r1, #ARMPLL_CTRL_OFFS]
	bic	r5, #(PLL_PWRDWN_MASK | PLL_RESET_MASK)
	str	r5, [r1, #ARMPLL_CTRL_OFFS]
	/* wait for lock */
1:	ldr	r5, [r1, #PLLSTATUS_OFFS]
	ands	r5, #ARM_LOCK_MASK
	beq	1b
	dsb	sy

	/* Disable ARM PLL bypass */
	ldr	r5, [r1, #ARMPLL_CTRL_OFFS]
	bic	r5, #PLL_BYPASS_MASK
	str	r5, [r1, #ARMPLL_CTRL_OFFS]
	dmb	st

	/* Power up DDR PLL */
	ldr	r5, [r1, #DDRPLL_CTRL_OFFS]
	bic	r5, #(PLL_PWRDWN_MASK | PLL_RESET_MASK)
	str	r5, [r1, #DDRPLL_CTRL_OFFS]
	/* wait for lock */
1:	ldr	r5, [r1, #PLLSTATUS_OFFS]
	ands	r5, #DDR_LOCK_MASK
	beq	1b
	dsb	sy

	/* Disable DDR PLL bypass */
	ldr	r5, [r1, #DDRPLL_CTRL_OFFS]
	bic	r5, #PLL_BYPASS_MASK
	str	r5, [r1, #DDRPLL_CTRL_OFFS]
	dmb	st

	/* Enable AXI clocks, APB Clock */
	ldr	r5, [r1, #DDR_CLK_CTRL_OFFS]
	orr	r5, #(DDR_AXI_CLK_MASK | DDR_APB_CLK_MASK)
	str	r5, [r1, #DDR_CLK_CTRL_OFFS]
	dmb	st

	/* Enable DDRC clock */
	ldr	r5, [r1, #DDR_CLK_CTRL_OFFS]
	orr	r5, #DDR_CLK_MASK
	str	r5, [r1, #DDR_CLK_CTRL_OFFS]
	dmb	st

	/* reinit DDRC internal pll */
	ldr	r5, [r2, #PIR_OFFS]
	dmb	st
	orr	r5, #(PIR_PLLINIT_MASK | PIR_INIT_MASK)
	str	r5, [r2, #PIR_OFFS]
	dmb	st

	/* wait for done */
1:	ldr	r5, [r2, #PGSR0_OFFS]
	ands	r5, #0x3
	beq	1b
	dsb	sy

	/* Cause system to exit from self refresh state */
exit:	ldr	r5, [r0, #PWRCTL_OFFS]
	bic	r5, #SELFREF_SW_MASK
	str	r5, [r0, #PWRCTL_OFFS]
	dmb	st

	/* Waits until Self Refresh state exit */
1:	dsb	sy
	ldr	r5, [r0, #MODE_STS_OFFS]
	and	r5, #DDRC_SR_TYPE_MASK
	cmp	r5, #0x0
	bne	1b
	dsb	sy

	/* AXI ports are no longer blocked from taking transactions */
	ldr	r5, =0x1
	str	r5, [r0, #PCTRL_AXI_PORT4_OFFS]
	dmb	st

	dsb	sy
	cmp	r4, #0xff00
	beq	exit1

	mov	r0, #0
	pop	{r4 - r7}
	bx	lr

exit1:	mov	r0, #-1
	pop	{r4 - r7}
	bx	lr

ENTRY(fmsh_sys_suspend_sz)
	.word	. - fmsh_sys_suspend

	ENDPROC(fmsh_sys_suspend)
