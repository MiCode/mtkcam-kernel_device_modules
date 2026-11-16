/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: Yuhsuan.chang <yuhsuan.chang@mediatek.com>
 *
 */

#ifndef MTK_IMGSYS_HWQOS_DATA_H_
#define MTK_IMGSYS_HWQOS_DATA_H_

#include <linux/bitfield.h>
#include <linux/bits.h>

#include "mtk_imgsys-hwqos-reg.h"

#define FLOAT2FIX(x, bits) ((u32)((x) * (1 << (bits))))

/* BWR register field */
#define BWR_IMG_RPT_START                   0
#define BWR_IMG_RPT_END                     1
#define BWR_IMG_RPT_RST                     2

/* BWR register field */
#define BWR_IMG_RPT_IDLE                    0
#define BWR_IMG_RPT_WAIT                    1
#define BWR_IMG_RPT_SEND                    2

/* Fill in the value */
#define OCC_FACTOR                          1.2890625  /* 1.25 * (1 + 1/32) (with TCU BW) */
#define BUS_URATE                           1.421875
#define BW_RAT                              1
#define BLS_UNIT                            16  /* byte */

#define BWR_OCC_FACTOR_POINT                7
#define BWR_BW_RAT_POINT                    7
#define BWR_BW_POINT                        3

#define BWR_IMG_RPT_TIMER                   2600  /* 100 us */
#define BWR_IMG_DBC_CYC                     260   /* 10 us */
#define BWR_IMG_OCC_FACTOR                  FLOAT2FIX(OCC_FACTOR, BWR_OCC_FACTOR_POINT)
#define BWR_IMG_BUS_URATE                   FLOAT2FIX(BUS_URATE,  BWR_OCC_FACTOR_POINT)
#define BWR_IMG_EMI_DVFS_FREQ               1     /* 16 MB/s (change to 64 MB/s by cmdq cmd) */
#define BWR_IMG_RW_DVFS_FREQ                1     /* 16 MB/s */

#define BWR_IMG_SRT_ENG_BW_RAT              FLOAT2FIX(BW_RAT, BWR_BW_RAT_POINT)

/* BWR engine ID */
#define BWR_WPE_EIS__PQDIP_A     0
#define BWR_OMC_TNR__PQDIP_B__ME 1
#define BWR_WPE_LITE__OMC_LITE   2
#define BWR_TRAW                 3
#define BWR_DIP                  4
#define BWR_IMG_MAIN             5
#define BWR_LTRAW                6
#define BWR_MAE                  7
#define BWR_ADL_RD__ADL_WR       8 /* not in use */
#define BWR_DWPE__DFP__DVS       9

/* Enable engine for each core & sub-common for BWR */
#define BWR_IMG_CORE0_SC0_ENG_EN	(BIT(BWR_WPE_EIS__PQDIP_A) | BIT(BWR_DIP))
#define BWR_IMG_CORE0_SC1_ENG_EN	(BIT(BWR_DIP))
#define BWR_IMG_CORE1_SC0_ENG_EN	(BIT(BWR_OMC_TNR__PQDIP_B__ME) | BIT(BWR_WPE_LITE__OMC_LITE) | \
					BIT(BWR_DIP) | BIT(BWR_LTRAW))
#define BWR_IMG_CORE2_SC0_ENG_EN	(BIT(BWR_MAE))
#define BWR_IMG_CORE3_SC0_ENG_EN	(BIT(BWR_TRAW) | BIT(BWR_MAE) | \
					BIT(BWR_DWPE__DFP__DVS))
#define BWR_IMG_EMI_ENG_EN		(BWR_IMG_CORE0_SC0_ENG_EN | BWR_IMG_CORE0_SC1_ENG_EN | \
					BWR_IMG_CORE1_SC0_ENG_EN | BWR_IMG_CORE2_SC0_ENG_EN | \
					BWR_IMG_CORE3_SC0_ENG_EN)
#define BWR_IMG_MTCMOS_EN_VLD_EN	(BIT(BWR_WPE_EIS__PQDIP_A) | BIT(BWR_OMC_TNR__PQDIP_B__ME) | \
					BIT(BWR_WPE_LITE__OMC_LITE) | BIT(BWR_TRAW) | BIT(BWR_DIP) | \
					BIT(BWR_DWPE__DFP__DVS))
#define BWR_IMG_SW_MODE_EN                  0x0

enum BWR_CTRL {
	BWR_START = 0,
	BWR_STOP,
};

enum BLS_CTRL {
	BLS_INIT = 0,
	BLS_STOP,
	BLS_TRIG,
};

struct reg {
	const uint32_t offset;
	const uint32_t value;
};

struct reg bls_init_data[] = {
	{BLS_IMG_RID_EN_OFT,          0x1},
	{BLS_IMG_WID_EN_OFT,          0x1},
};

struct reg bwr_init_data[] = {
	{BWR_IMG_RPT_TIMER_OFT,              BWR_IMG_RPT_TIMER},
	{BWR_IMG_DBC_CYC_OFT,                BWR_IMG_DBC_CYC},
	{BWR_IMG_SRT_EMI_OCC_FACTOR_OFT,     BWR_IMG_OCC_FACTOR},
	{BWR_IMG_SRT_RW_OCC_FACTOR_OFT,      BWR_IMG_BUS_URATE},
	{BWR_IMG_SRT_EMI_DVFS_FREQ_OFT,      BWR_IMG_EMI_DVFS_FREQ},
	{BWR_IMG_SRT_RW_DVFS_FREQ_OFT,       BWR_IMG_RW_DVFS_FREQ},
};

uint32_t bls_base_array[] = {
	BLS_IMG_E1A_BASE, BLS_IMG_E2A_BASE,
	BLS_IMG_E3A_BASE, BLS_IMG_E4A_BASE,
	BLS_IMG_E5A_BASE,
	/* SLB is not in use.
	 * BLS_IMG_E6A_BASE, BLS_IMG_E7A_BASE,
	 */
	BLS_IMG_E8A_BASE,
	BLS_IMG_E9A_BASE, BLS_IMG_E10A_BASE,
	BLS_IMG_E11A_BASE, BLS_IMG_E12A_BASE,
	BLS_IMG_E13A_BASE, BLS_IMG_E14A_BASE,
	BLS_IMG_E15A_BASE, BLS_IMG_E16A_BASE,
};

uint32_t bwr_base_array[] = {
	BWR_IMG_E1A_BASE,
};


struct bwr_ctrl_reg {
	/* HW MODE: 1, SW MODE: 0 */
	const uint32_t qos_sel_offset;
	/* HW MODE, SW trigger */
	const uint32_t qos_trig_offset;
	/* SW MODE, SW enable */
	const uint32_t qos_en_offset;
	/* ENGINE enable */
	const uint32_t eng_en_value;
};

#define __BWR_CTRL_REG(rw, x, y)  \
	{.qos_sel_offset  = BWR_IMG_SRT_ ## rw ## x ## _BW_QOS_SEL ## y ## _OFT, \
	.qos_trig_offset = BWR_IMG_SRT_ ## rw ## x ## _SW_QOS_TRIG ## y ## _OFT,\
	.qos_en_offset   = BWR_IMG_SRT_ ## rw ## x ## _SW_QOS_EN ## y ## _OFT,  \
	.eng_en_value    = BWR_IMG_CORE ## x ## _SC ## y ## _ENG_EN}
#define _BWR_CTRL_REG(rw, x, y) __BWR_CTRL_REG(rw, x, y)
#define BWR_CTRL_REG(rw, x, y) _BWR_CTRL_REG(rw, x, y)

struct bwr_ctrl_reg bwr_ctrl_data[] = {
	{.qos_sel_offset  = BWR_IMG_SRT_EMI_BW_QOS_SEL_OFT,
	 .qos_trig_offset = BWR_IMG_SRT_EMI_SW_QOS_TRIG_OFT,
	 .qos_en_offset   = BWR_IMG_SRT_EMI_SW_QOS_EN_OFT,
	 .eng_en_value    = BWR_IMG_EMI_ENG_EN},
	BWR_CTRL_REG(R, 0, 0),
	BWR_CTRL_REG(W, 0, 0),
	BWR_CTRL_REG(R, 0, 1),
	BWR_CTRL_REG(W, 0, 1),
	BWR_CTRL_REG(R, 1, 0),
	BWR_CTRL_REG(W, 1, 0),
	/* SLB is not in use.
	 * BWR_CTRL_REG(R, 2, 0),
	 * BWR_CTRL_REG(W, 2, 0),
	 */
	BWR_CTRL_REG(R, 3, 0),
	BWR_CTRL_REG(W, 3, 0),
};

struct qos_map {
	const uint32_t bls_base;
	const uint8_t  core;
	const uint8_t  sub_common;
	const uint8_t  engine;
	const uint8_t  larb;
	const uint32_t bwr_r_offset;
	const uint32_t bwr_r_rat_offset;
	const uint32_t bwr_w_offset;
	const uint32_t bwr_w_rat_offset;
};

#define __BLS_REG(x)  \
	.bls_base            = BLS_IMG_E ## x ## A_BASE
#define _BLS_REG(x) __BLS_REG(x)
#define BLS_REG(x) _BLS_REG(x)

#define __BWR_BW_REG(x, y, z, l)  \
	.core             = x, \
	.sub_common       = y, \
	.engine           = z, \
	.larb             = l, \
	.bwr_r_offset     = BWR_IMG_SRT_R ## x ## _ENG_BW ## y ## _ ## z ## _OFT,     \
	.bwr_r_rat_offset = BWR_IMG_SRT_R ## x ## _ENG_BW_RAT ## y ## _ ## z ## _OFT, \
	.bwr_w_offset     = BWR_IMG_SRT_W ## x ## _ENG_BW ## y ## _ ## z ## _OFT,     \
	.bwr_w_rat_offset = BWR_IMG_SRT_W ## x ## _ENG_BW_RAT ## y ## _ ## z ## _OFT
#define _BWR_BW_REG(x, y, z, l) __BWR_BW_REG(x, y, z, l)
#define BWR_BW_REG(x, y, z, l) _BWR_BW_REG(x, y, z, l)

struct qos_map qos_map_data[] = {
	/* IMG_VCORE_SC 0, IMG_SC 0 */
	{BLS_REG(2), BWR_BW_REG(0, 0, BWR_DIP,                  10)},	/* 0:  DIP_TOP */
	{BLS_REG(3), BWR_BW_REG(0, 0, BWR_WPE_EIS__PQDIP_A,     11)},	/* 1:  WPE_EIS + PQDIP_A */
	/* IMG_VCORE_SC 1, IMG_SC 1 */
	{BLS_REG(4), BWR_BW_REG(1, 0, BWR_OMC_TNR__PQDIP_B__ME, 48)},	/* 2:  OMC_TNR + PQDIP_B + ME */
	{BLS_REG(5), BWR_BW_REG(1, 0, BWR_DIP,                  39)},	/* 3:  DIP_NR */
	/* SLB is not in use, IMG_SC 2,
	 * {BLS_REG(6), BWR_BW_REG(2, 0, BWR_MAE,                  12)},
	 * {BLS_REG(7), BWR_BW_REG(2, 0, BWR_ADL_RD__ADL_WR,       18)},
	 */
	/* IMG_VCORE_SC 0, IMG_SC 3: index 4 & 5 with same BWR reg */
	{BLS_REG(8), BWR_BW_REG(0, 1, BWR_DIP,                  38)},	/* 4:  DIP_TOP */
	{BLS_REG(9), BWR_BW_REG(0, 1, BWR_DIP,                  15)},	/* 5:  DIP_NR */
	/* IMG_VCORE_SC 1, IMG_SC 4 */
	{BLS_REG(10), BWR_BW_REG(1, 0, BWR_WPE_LITE__OMC_LITE,  49)},	/* 6:  WPE_LITE + OMC_LITE */
	{BLS_REG(11), BWR_BW_REG(1, 0, BWR_LTRAW,               9)},	/* 7:  LTRAW */
	/* IMG_VCORE_SC 2, IMG_SC 5 */
	{BLS_REG(12), BWR_BW_REG(3, 0, BWR_MAE,                 12)},	/* 8: MAE */
	/* IMG_VCORE_SC 2, IMG_SC 6: index 9 & 10 with same BWR reg */
	{BLS_REG(13), BWR_BW_REG(3, 0, BWR_TRAW,                40)},	/* 9: TRAW */
	{BLS_REG(14), BWR_BW_REG(3, 0, BWR_TRAW,                28)},	/* 10: TRAW */
	{BLS_REG(15), BWR_BW_REG(3, 0, BWR_DWPE__DFP__DVS,      50)},	/* 11: DWPE + DFP + DEPTH */
	/* ROOTCQ + ADL is not in use
	 * {BLS_REG(16), BWR_BW_REG(3, 0, BWR_ADL_RD__ADL_WR,      18)},
	 */
};

struct qos_limiter_rw {
	const uint32_t ostdl_right_shift;
	const uint32_t ostdl_reg_l;
	const uint32_t ostdl_reg_mask;
	const uint32_t bwl;
	const uint32_t bwl_dbg;
};

struct qos_limiter {
	const uint32_t ostdl_en;
	const uint32_t ostdl;
	const uint32_t axi_limiter_en;
	const uint32_t ostdbl;
	const uint32_t ostdbl_dbg;
	struct qos_limiter_rw r;
	struct qos_limiter_rw w;
};

#define OSTDL_R0_RIGHT_SHIFT (6)  /* 64 byte, 1 us delay */
#define OSTDL_W0_RIGHT_SHIFT (7)  /* 128 byte, 1 us delay */
#define OSTDL_R1_RIGHT_SHIFT (6)  /* 64 byte, 1 us delay */
#define OSTDL_W1_RIGHT_SHIFT (7)  /* 128 byte, 1 us delay */
#define OSTDL_R2_RIGHT_SHIFT (7)  /* 128 byte, 1 us delay */
#define OSTDL_W2_RIGHT_SHIFT (7)  /* 128 byte, 1 us delay */

#define __LIMITER_REG(x)  \
	.ostdl_en            = OSTDL_IMG_COMM ## x ## _EN_ADDR, \
	.ostdl               = OSTDL_IMG_COMM ## x ## _ADDR, \
	.axi_limiter_en = IMG_VCORE_AXI_MONITOR ## x ## _BASE + MON_BMAN2_OFT, \
	.ostdbl         = IMG_VCORE_AXI_MONITOR ## x ## _BASE + MON_BWLMTE3_OFT, \
	.ostdbl_dbg     = IMG_VCORE_AXI_MONITOR ## x ## _BASE + MON_TTYPE3_CONB_OFT, \
	.r.ostdl_right_shift = OSTDL_R ## x ## _RIGHT_SHIFT, \
	.r.ostdl_reg_l = OSTDL_IMG_COMM ## x ## _R_REG_L, \
	.r.ostdl_reg_mask = OSTDL_IMG_COMM ## x ## _R_REG_MASK, \
	.r.bwl          = IMG_VCORE_AXI_MONITOR ## x ## _BASE + MON_BWLMTE1_OFT, \
	.r.bwl_dbg      = IMG_VCORE_AXI_MONITOR ## x ## _BASE + MON_TTYPE2_CONB_OFT, \
	.w.ostdl_right_shift = OSTDL_W ## x ## _RIGHT_SHIFT, \
	.w.ostdl_reg_l = OSTDL_IMG_COMM ## x ## _W_REG_L, \
	.w.ostdl_reg_mask = OSTDL_IMG_COMM ## x ## _W_REG_MASK, \
	.w.bwl          = IMG_VCORE_AXI_MONITOR ## x ## _BASE + MON_BWLMTE2_OFT, \
	.w.bwl_dbg      = IMG_VCORE_AXI_MONITOR ## x ## _BASE + MON_TTYPE3_CONA_OFT

#define _LIMITER_REG(x) __LIMITER_REG(x)
#define LIMITER_REG(x) _LIMITER_REG(x)

struct qos_limiter limiter_data[] = {
	{LIMITER_REG(0)},
	{LIMITER_REG(1)},
	{LIMITER_REG(2)},
};

struct gear {
	const uint8_t level;
	const uint16_t freq;
};

/* order from low to high */
struct gear mminfra_gear_data[] = {
	{4, 312},
	{7, 364},
	{10, 458},
	{13, 628},
	{14, 728},
	{15, 916},
};

struct ostdbl_lut {
	const uint16_t min_freq;
	const uint16_t nps_r;
	const uint16_t nps_w;
};

/* order from low to high */
struct ostdbl_lut ostdbl_lut_data[] = {
	{0,   170, 100},
	{400, 190, 110},
	{700, 210, 120},
	{900, 256, 128},
};

struct reg_addr_mask {
	const uint32_t addr;
	const uint32_t mask;
};

struct reg_addr_mask bwr_ttl_bw_array[] = {
	{BWR_IMG_E1A_BASE + BWR_IMG_SRT_EMI_ENG_BW0_OFT, GENMASK(31, 3)},
	{BWR_IMG_E1A_BASE + BWR_IMG_SRT_EMI_ENG_BW1_OFT, GENMASK(31, 3)},
	{BWR_IMG_E1A_BASE + BWR_IMG_SRT_EMI_ENG_BW2_OFT, GENMASK(31, 3)},
	{BWR_IMG_E1A_BASE + BWR_IMG_SRT_EMI_ENG_BW3_OFT, GENMASK(31, 3)},
	{BWR_IMG_E1A_BASE + BWR_IMG_SRT_EMI_ENG_BW4_OFT, GENMASK(31, 3)},
	{BWR_IMG_E1A_BASE + BWR_IMG_SRT_EMI_ENG_BW6_OFT, GENMASK(31, 3)},
	{BWR_IMG_E1A_BASE + BWR_IMG_SRT_EMI_ENG_BW7_OFT, GENMASK(31, 3)},
	{BWR_IMG_E1A_BASE + BWR_IMG_SRT_EMI_ENG_BW8_OFT, GENMASK(31, 3)},
	{BWR_IMG_E1A_BASE + BWR_IMG_SRT_EMI_ENG_BW9_OFT, GENMASK(31, 3)},
};

struct reg_addr_mask img_hw_bw_reg_array[] = {
	{HFRP_DVFSRC_VMM_IMG_HW_BW_0, IMG_BW_REG_MASK0},
	{HFRP_DVFSRC_VMM_IMG_HW_BW_1, IMG_BW_REG_MASK0},
	{HFRP_DVFSRC_VMM_IMG_HW_BW_12, IMG_BW_REG_MASK0},
	{HFRP_DVFSRC_VMM_IMG_HW_BW_13, IMG_BW_REG_MASK0},
	{HFRP_DVFSRC_VMM_IMG_HW_BW_4, IMG_BW_REG_MASK0},
	{HFRP_DVFSRC_VMM_IMG_HW_BW_5, IMG_BW_REG_MASK0},
	{HFRP_DVFSRC_VMM_IMG_HW_BW_SRT, IMG_BW_REG_MASK0},
};

#endif  // MTK_IMGSYS_HWQOS_DATA_H_
