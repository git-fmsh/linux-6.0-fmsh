#ifndef FMSH_MEMCTL_HW_H
#define FMSH_MEMCTL_HW_H

// define SMC register's offset
//  x = 0~7
#define MEMCTL_SCONR				(0x0000)
#define MEMCTL_STMG0R				(0x0004)
#define MEMCTL_STMG1R				(0x0008)
#define MEMCTL_SCTLR				(0x000C)
#define MEMCTL_SREFR				(0x0010)
#define MEMCTL_EXN_MODE_REG		(0x00AC)
#define MEMCTL_SCRLRx_LOW(x)		(0x0014 + 0x0004 * x)
#define MEMCTL_SMSKRx(x)			(0x0054 + 0x0004 * x)
#define MEMCTL_CSALIAS0_LOW		(0x0074)
#define MEMCTL_CSALIAS1_LOW		(0x0078)
#define MEMCTL_CSREMAP0_LOW		(0x0084)
#define MEMCTL_CSREMAP1_LOW		(0x0088)
#define MEMCTL_SMTMGR_SET0			(0x0094)
#define MEMCTL_SMTMGR_SET1			(0x0098)
#define MEMCTL_SMTMGR_SET2			(0x009C)
#define MEMCTL_FLASH_TRPDR			(0x00A0)
#define MEMCTL_SMCTLR				(0x00A4)
/***/

 // SMSKRx registers
#define MEMCTL_SMSKRx_REG_SELECT		(0x7 << 8)
#define MEMCTL_SMSKRx_MEM_TYPE			(0x7 << 5)
#define MEMCTL_SMSKRx_MEM_SIZE			(0x1F << 0)
// SMTMGR_SETx registers
#define MEMCTL_SMSETx_SM_READ_PIPE		(0x3 << 28)
#define MEMCTL_SMSETx_LOW_FREQ_SYNC_DEVICE		(0x1 << 27)
#define MEMCTL_SMSETx_READY_MODE		(0x1 << 26)
#define MEMCTL_SMSETx_PAGE_SIZE		(0x3 << 24)
#define MEMCTL_SMSETx_PAGE_MODE		(0x1 << 23)
#define MEMCTL_SMSETx_T_PRC		(0xF << 19)
#define MEMCTL_SMSETx_T_BTA		(0x7 << 16)
#define MEMCTL_SMSETx_T_WP		(0x3F << 10)
#define MEMCTL_SMSETx_T_WR		(0x3 << 8)
#define MEMCTL_SMSETx_T_AS		(0x3 << 6)
#define MEMCTL_SMSETx_T_RC		(0x3F << 0)
// FLASH registers
#define MEMCTL_FLASH_T_RPD		(0xFFF << 0)
// SMCTLR registers
#define MEMCTL_SMCTLR_SM_DATA_WIDTH_SET2	(0x7 << 13)
#define MEMCTL_SMCTLR_SM_DATA_WIDTH_SET1	(0x7 << 10)
#define MEMCTL_SMCTLR_SM_DATA_WIDTH_SET0	(0x7 << 7)
#define MEMCTL_SMCTLR_WP_N					(0x7 << 1)
#define MEMCTL_SMCTLR_SM_RP_N				(0x1 << 0)
/*****/

#endif

