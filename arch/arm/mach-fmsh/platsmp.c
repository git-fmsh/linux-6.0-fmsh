/*
 * This file contains FMSH specific SMP code, used to start up
 * the second processor.
 *
 * Copyright (C) 2011-2013 FMSH
 *
 * based on linux/arch/arm/mach-realview/platsmp.c
 *
 * Copyright (C) 2002 ARM Ltd.
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

#include <linux/export.h>
#include <linux/jiffies.h>
#include <linux/init.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <asm/smp_plat.h>
#include <asm/smp_scu.h>
#include <asm/cp15.h>
#include <linux/irqchip/arm-gic.h>
#include "common.h"

#define SRC_BASE		(0xE0026000)
// #define SRC_UNLOCK		(0x008)
// #define SRC_LOCK		(0x004)
// #define SRC_LOCK_VAL		(0xDF0D767B)
#define CORE0_ENTRY_OFFSET	(0x438)
// #define CORE0_ENTRY_OFFSET	(0x434)
#define CORE1_ENTRY_OFFSET	(0x43c)
#define CORE2_ENTRY_OFFSET	(0x444)
#define CORE3_ENTRY_OFFSET	(0x44c)

// static int ncores;

int fmsh_cpun_start(u32 address, int cpu)
{
	/* MS: Expectation that SLCR are directly map and accessible */
	/* Not possible to jump to non aligned address */
	if (!(address & 3) && (address)) {
#if 0 // use DDR
		static u8 __iomem *base;
		u32 word_size = 4;

		base = ioremap((0x100000 + (cpu * 4)), word_size);
//		base = ioremap((0x100000), trampoline_code_size);
		if (!base) {
			pr_warn("BOOTUP jump vectors not accessible\n");
			return -1;
		}
		writel(address, base);

		flush_cache_all();
	//	outer_flush_range((0x100000 + (cpu * 4)), word_size);
	//	outer_flush_range(0, trampoline_size);

		smp_wmb();

		iounmap(base);
		sev();
#endif
#if 1 // use reg
		static u8 __iomem *base_lock;
		static u8 __iomem *base;

		base_lock = ioremap(SRC_BASE, 0x10);
		base = ioremap(SRC_BASE + CORE0_ENTRY_OFFSET + (cpu * 8), 8);

		pr_debug("smp: write entry:0x%x to reg: 0x%x for wakeup cpu%d\n",
			   address, SRC_BASE + CORE0_ENTRY_OFFSET + (cpu * 8), cpu);
		writel(address, base);

		fmsh_slcr_cpu_state_write(cpu, false);

		smp_wmb();

		iounmap(base);
		iounmap(base_lock);
		sev();
#endif
		return 0;
	}

	pr_warn("Can't start CPU%d: Wrong starting address %x\n", cpu, address);

	return -1;
}

static int fmsh_boot_secondary(unsigned int cpu,
						struct task_struct *idle)
{
	if (fmsh_slcr_cpu_state_read(cpu))
		return fmsh_cpun_start(virt_to_phys(fmsh_secondary_startup), cpu);
	else
		return fmsh_cpun_start((u32)fmsh_secondary_startup, cpu);
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
static void __init fmsh_smp_init_cpus(void)
{
	int i, ncores;
	// u32 me = smp_processor_id();

	unsigned long val;

	/* CA7 core number, [25:24] of CP15 L2CTLR */
	asm volatile("mrc p15, 1, %0, c9, c0, 2" : "=r" (val));
	ncores = ((val >> 24) & 0x3) + 1;

	if (setup_max_cpus < ncores)
		ncores = (setup_max_cpus) ? setup_max_cpus : 1;

	// ncores = 1;
	for (i = 0; i < ncores; i++)
		set_cpu_possible(i, true);

	for (i = ncores; i < NR_CPUS; i++)
		set_cpu_possible(i, false);
}

static void __init fmsh_smp_prepare_cpus(unsigned int max_cpus)
{
	// scu_enable(scu_base);
}

/**
 * fmsh_secondary_init - Initialize secondary CPU cores
 * @cpu:	CPU that is initialized
 *
 * This function is in the hotplug path. Don't move it into the
 * init section!!
 */
static void fmsh_secondary_init(unsigned int cpu) { };

#ifdef CONFIG_HOTPLUG_CPU
static inline void cpu_enter_lowpower(void)
{
	unsigned int v;

	flush_cache_all();
	asm volatile(
	"	mcr	p15, 0, %1, c7, c5, 0\n"
	"	mcr	p15, 0, %1, c7, c10, 4\n"
	/*
	 * Turn off coherency and L1 D-cache
	 */
	"	mrc	p15, 0, %0, c1, c0, 1\n"
	"	bic	%0, %0, %3\n"
	"	mcr	p15, 0, %0, c1, c0, 1\n"
	"	mrc	p15, 0, %0, c1, c0, 0\n"
	"	bic	%0, %0, %2\n"
	"	mcr	p15, 0, %0, c1, c0, 0\n"
	  : "=&r" (v)
	  : "r" (0), "Ir" (CR_C), "Ir" (0x40)
	  : "cc");
}

/**
 * fmsh_cpu_die - Let a CPU core die
 * @cpu:	Dying CPU
 *
 * Platform-specific code to shutdown a CPU.
 * Called with IRQs disabled on the dying CPU.
 */
static void fmsh_cpu_die(unsigned int cpu)
{
	fmsh_slcr_cpu_state_write(cpu, true);

	writel(0x0, fmsh_slcr_base + CORE0_ENTRY_OFFSET + (cpu * 8));

	cpu_enter_lowpower();

	/*
	 * there is no power-control hardware on this platform, so all
	 * we can do is put the core into WFE; this is safe as the calling
	 * code will have already disabled interrupts
	 */
	for (;;)
		fmsh_cpu_do_lowpower(fmsh_slcr_base);

	pr_info("cpu %d failed to shutdown\n", cpu);
}


static int fmsh_cpu_kill(unsigned cpu)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(50);

	while (fmsh_slcr_cpu_state_read(cpu))
		if (time_after(jiffies, timeout))
			return 0;

	return 1;
}

#endif

struct smp_operations fmsh_smp_ops __initdata = {
	.smp_init_cpus		= fmsh_smp_init_cpus,
	.smp_prepare_cpus   = fmsh_smp_prepare_cpus,
	.smp_boot_secondary	= fmsh_boot_secondary,
	.smp_secondary_init	= fmsh_secondary_init,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_die		= fmsh_cpu_die,
	.cpu_kill		= fmsh_cpu_kill,
#endif
};
