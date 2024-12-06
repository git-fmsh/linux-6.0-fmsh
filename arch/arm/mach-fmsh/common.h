/*
 * This file contains common function prototypes to avoid externs
 * in the c files.
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

#ifndef __MACH_FMSH_COMMON_H__
#define __MACH_FMSH_COMMON_H__

extern int fmsh_early_slcr_init(void);
extern bool fmsh_slcr_cpu_state_read(int cpu);
extern void fmsh_slcr_cpu_state_write(int cpu, bool die);
extern u32 fmsh_slcr_get_device_id(void);

#ifdef CONFIG_SMP
extern void fmsh_secondary_startup(void);
extern char fmsh_secondary_trampoline;
extern char fmsh_secondary_trampoline_jump;
extern char fmsh_secondary_trampoline_end;
extern int fmsh_cpun_start(u32 address, int cpu);
extern struct smp_operations fmsh_smp_ops __initdata;
#endif

extern void __iomem *fmsh_slcr_base;
extern void __iomem *fmsh_scu_base;

void fmsh_pm_late_init(void);
extern unsigned int fmsh_sys_suspend_sz;
int fmsh_sys_suspend(void __iomem *ddrc_umc_base, void __iomem *slcr_base, void __iomem *ddrc_pub_base);


/* Hotplug */
extern void fmsh_cpu_do_lowpower(void __iomem *slcr_base);

#endif
