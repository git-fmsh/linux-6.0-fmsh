/*
 * Copyright (c) 2012-2013 FMSH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <linux/init.h>

#if 0
ENTRY(fmsh_secondary_trampoline)
	ldr	r0, [pc]
	bx	r0
.globl fmsh_secondary_trampoline_jump
fmsh_secondary_trampoline_jump:
	/* Space for jumping address */
	.word	/* cpu 1 */
.globl fmsh_secondary_trampoline_end
fmsh_secondary_trampoline_end:
ENDPROC(fmsh_secondary_trampoline)
#endif

ENTRY(fmsh_secondary_startup)
	bl	v7_invalidate_l1
	b	secondary_startup
ENDPROC(fmsh_secondary_startup)

/**
 * fmsh_cpu_do_lowpower - Enter lowpower mode
 * r0 @slcr_base:     Base address of the SLCR
 */
ENTRY(fmsh_cpu_do_lowpower)
	/* get CPU id */
	mrc	p15, 0, r3, c0, c0, 5
	and	r3, r3, #0xf

	/* r3 = 0x438 + 0x8 * r3 */
	mov	r2, #8
	mul	r2, r3, r2
	ldr	r3, =0x438
	add	r3, r3, r2

1:	dsb	sy
	wfe
	dsb	sy

	/* check address */
	ldr	r2, [r0, r3]
	cmp	r2, #0x0
	beq	1b

	mov	pc, r2
ENDPROC(fmsh_cpu_do_lowpower)
