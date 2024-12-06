/*
 * Copyright (C) 2022 FMSH, Inc.
 *
 * Used for FMQLMP Platform.
 * Li Li <lili@fmsh.com.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __DT_BINDINGS_CLOCK_FMSH_H
#define __DT_BINDINGS_CLOCK_FMSH_H

/* clocks number */
#define NCLK_ARMPLL		0
#define NCLK_DDRPLL		1
#define NCLK_IOPLL		2
#define NCLK_APU		3
#define NCLK_AXI		4
#define NCLK_AHB		5
#define NCLK_APB		6
#define NCLK_VPU    	7
#define NCLK_AXIDMAC	8
#define NCLK_CSU        9
#define NCLK_GPU		10
#define NCLK_GPU_S  	11
#define NCLK_DDRX1	    12
#define NCLK_AXI_DDR	13
#define NCLK_APB_DDR	14
#define NCLK_FCLK0  	15
#define NCLK_FCLK1		16
#define NCLK_FCLK2		17
#define NCLK_FCLK3		18
#define NCLK_GMAC0_TX	19
#define NCLK_GMAC0_RX	20
#define NCLK_AXI_GMAC0	21
#define NCLK_AHB_GMAC0	22
#define NCLK_GMAC0_TS	23
#define NCLK_GMAC1_TX	24
#define NCLK_GMAC1_RX	25
#define NCLK_AXI_GMAC1	26
#define NCLK_AHB_GMAC1	27
#define NCLK_GMAC1_TS	28
#define NCLK_UART0		29
#define NCLK_UART1  	30
#define NCLK_APB_UART0	31
#define NCLK_APB_UART1	32
#define NCLK_SPI0		33
#define NCLK_SPI1   	34
#define NCLK_APB_SPI0	35
#define NCLK_APB_SPI1	36
#define NCLK_QSPI		37
#define NCLK_APB_QSPI	38
#define NCLK_AHB_QSPI	39
#define NCLK_SDIO0		40
#define NCLK_SDIO1		41
#define NCLK_AHB_SDIO0	42
#define NCLK_AHB_SDIO1	43
#define NCLK_TRACE	    44
#define NCLK_TRACETS	45
#define NCLK_USB0   	46
#define NCLK_AHB_USB0	47
#define NCLK_USB0_PHY	48
#define NCLK_USB1   	49
#define NCLK_AHB_USB1	50
#define NCLK_USB1_PHY	51
#define NCLK_TTC0_REF1	52
#define NCLK_TTC0_REF2	53
#define NCLK_TTC0_REF3	54
#define NCLK_APB_TTC0	55
#define NCLK_TTC1_REF1	56
#define NCLK_TTC1_REF2	57
#define NCLK_TTC1_REF3	58
#define NCLK_APB_TTC1	59
#define NCLK_WDT    	60
#define NCLK_APB_WDT	61
#define NCLK_AHB_SMC	62
#define NCLK_AHB_NFC	63
#define NCLK_NFC    	64
#define NCLK_APB_CAN0  	65
#define NCLK_APB_CAN1  	66
#define NCLK_APB_GPIO  	67
#define NCLK_APB_I2C0  	68
#define NCLK_APB_I2C1  	69
#define NCLK_AHB_DMAC  	70
//#define NCLK_GTC      	71

#endif