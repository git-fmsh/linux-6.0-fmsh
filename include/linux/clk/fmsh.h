/*
 * Copyright (C) 2018 FMSH Inc.
 * Copyright (C) 2012 National Instruments
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LINUX_CLK_FMSH_H_
#define __LINUX_CLK_FMSH_H_

#include <linux/spinlock.h>

int fmsh_clk_suspend_early(void);
void fmsh_clk_resume_late(void);
void fmsh_clock_init(void);

struct clk *clk_register_fmsh_pll(const char *name, const char *parent,
		void __iomem *pll_ctrl, void __iomem *pll_status, u8 lock_index,
		spinlock_t *lock);

#endif
