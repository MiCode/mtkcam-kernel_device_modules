/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Christopher Chen <christopher.chen@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_ENGINE_H_
#define _MTK_IMGSYS_ENGINE_H_

/**
 * enum Pseudo_Desc_Extra_Info
 *
 * Definition about Pseudo Descriptor special Extra Info
 * Distinguish Desc Type (NONE/CTRL/TUNING)
 * align with hw_definition in userspace
 */
enum Pseudo_Desc_Extra_Info {
	PSEUDO_DESC_NON = 0,
	PSEUDO_DESC_TUNING = 0x10,
	PSEUDO_DESC_CTRL = 0x20,
};

/* Module driver definitions */
#define IMGSYS_MOD_DRV_WPE         (0)
#define IMGSYS_MOD_DRV_OMC         (1)
#define IMGSYS_MOD_DRV_ADL         (2)
#define IMGSYS_MOD_DRV_TRAW        (3)
#define IMGSYS_MOD_DRV_DIP         (4)
#define IMGSYS_MOD_DRV_PQDIP       (5)
#define IMGSYS_MOD_DRV_TDR_NUM_MAX (6)
#define IMGSYS_MOD_DRV_ME          (IMGSYS_MOD_DRV_TDR_NUM_MAX)      // 6
#define IMGSYS_MOD_DRV_MAE         (IMGSYS_MOD_DRV_TDR_NUM_MAX + 1)  // 7
#define IMGSYS_MOD_DRV_DFP         (IMGSYS_MOD_DRV_TDR_NUM_MAX + 2 ) // 8
#define IMGSYS_MOD_DRV_DPE         (IMGSYS_MOD_DRV_TDR_NUM_MAX + 3 ) // 9
#define IMGSYS_MOD_DRV_NUM_MAX     (IMGSYS_MOD_DRV_TDR_NUM_MAX + 4)  // 10

#define IMGSYS_MOD_DRV_FLAG_WPE    (0x00000001)
#define IMGSYS_MOD_DRV_FLAG_OMC    (0x00000002)
#define IMGSYS_MOD_DRV_FLAG_ADL    (0x00000004)
#define IMGSYS_MOD_DRV_FLAG_TRAW   (0x00000008)
#define IMGSYS_MOD_DRV_FLAG_DIP    (0x00000010)
#define IMGSYS_MOD_DRV_FLAG_PQDIP  (0x00000020)
#define IMGSYS_MOD_DRV_FLAG_ME     (0x00000040)
#define IMGSYS_MOD_DRV_FLAG_MAE    (0x00000080)
#define IMGSYS_MOD_DRV_FLAG_DFP    (0x00000100)
#define IMGSYS_MOD_DRV_FLAG_DPE    (0x00000200)
#define IMGSYS_MOD_DRV_FLAG_NUM_MAX (IMGSYS_MOD_DRV_NUM_MAX)

/* HW definitions */
#define IMGSYS_HW_WPE_EIS     (0)
#define IMGSYS_HW_WPE_TNR     (1)
#define IMGSYS_HW_WPE_LITE    (2)
#define IMGSYS_HW_WPE_DEPTH   (3) /* Temp value for build pass */
#define IMGSYS_HW_OMC_TNR     (3)
#define IMGSYS_HW_OMC_LITE    (4)
#define IMGSYS_HW_ADL_A       (5)
#define IMGSYS_HW_ADL_B       (6)
#define IMGSYS_HW_TRAW        (7)
#define IMGSYS_HW_LTRAW       (8)
#define IMGSYS_HW_XTRAW       (9)
#define IMGSYS_HW_DIP         (10)
#define IMGSYS_HW_PQDIP_A     (11)
#define IMGSYS_HW_PQDIP_B     (12)
#define IMGSYS_HW_TDR_NUM_MAX (13)
#define IMGSYS_HW_ME          (IMGSYS_HW_TDR_NUM_MAX)     // 13
#define IMGSYS_HW_MAE         (IMGSYS_HW_TDR_NUM_MAX + 1) // 14
#define IMGSYS_HW_DFP         (IMGSYS_HW_TDR_NUM_MAX + 2) // 15
#define IMGSYS_HW_DPE         (IMGSYS_HW_TDR_NUM_MAX + 3) // 16
#define IMGSYS_HW_NUM_MAX     (IMGSYS_HW_TDR_NUM_MAX + 4) // 17

/*TODO: Fill MAE/DFP/DPE in here */
/**
 * enum mtk_imgsys_module
 *
 * Definition about supported hw modules
 */
enum mtk_imgsys_module {
	IMGSYS_MOD_IMGMAIN = 0, /*pure sw, debug dump usage*/
	IMGSYS_MOD_WPE,
	IMGSYS_MOD_OMC,
	IMGSYS_MOD_ADL,
	IMGSYS_MOD_TRAW,
	IMGSYS_MOD_LTRAW,
	IMGSYS_MOD_DIP,
	IMGSYS_MOD_PQDIP,
	IMGSYS_MOD_ME,
	IMGSYS_MOD_MAE,
	IMGSYS_MOD_DFP,
	IMGSYS_MOD_DPE,
	IMGSYS_MOD_MAX,
};

static inline unsigned int imgsys_hw_id_to_mod_id(unsigned int hw)
{
	unsigned int mod_id = IMGSYS_MOD_MAX;

	switch (hw) {
	case IMGSYS_HW_WPE_EIS:
	case IMGSYS_HW_WPE_TNR:
	case IMGSYS_HW_WPE_LITE:
		mod_id = IMGSYS_MOD_WPE;
		break;
	case IMGSYS_HW_OMC_TNR:
	case IMGSYS_HW_OMC_LITE:
		mod_id = IMGSYS_MOD_OMC;
		break;
	case IMGSYS_HW_ADL_A:
	case IMGSYS_HW_ADL_B:
		mod_id = IMGSYS_MOD_ADL;
		break;
	case IMGSYS_HW_TRAW:
	case IMGSYS_HW_LTRAW:
	case IMGSYS_HW_XTRAW:
		mod_id = IMGSYS_MOD_TRAW;
		break;
	case IMGSYS_HW_DIP:
		mod_id = IMGSYS_MOD_DIP;
		break;
	case IMGSYS_HW_PQDIP_A:
	case IMGSYS_HW_PQDIP_B:
		mod_id = IMGSYS_MOD_PQDIP;
		break;
	case IMGSYS_HW_ME:
		mod_id = IMGSYS_MOD_ME;
		break;
	case IMGSYS_HW_MAE:
		mod_id = IMGSYS_MOD_MAE;
		break;
	case IMGSYS_HW_DFP:
		mod_id = IMGSYS_MOD_DFP;
		break;
	case IMGSYS_HW_DPE:
		mod_id = IMGSYS_MOD_DPE;
		break;
	default:
		break;
	}

	return mod_id;
}

/**
 * enum mtk_imgsys_engine
 *
 * Definition about supported hw engines
 */
enum mtk_imgsys_engine {
	IMGSYS_HW_FLAG_WPE_EIS   = 0x00000001,
	IMGSYS_HW_FLAG_WPE_TNR   = 0x00000002,
	IMGSYS_HW_FLAG_WPE_LITE  = 0x00000004,
	IMGSYS_HW_FLAG_WPE_DEPTH = 0x00000008, /* Temp value for build pass */
	IMGSYS_HW_FLAG_OMC_TNR   = 0x00000008,
	IMGSYS_HW_FLAG_OMC_LITE  = 0x00000010,
	IMGSYS_HW_FLAG_ADL_A     = 0x00000020,
	IMGSYS_HW_FLAG_ADL_B     = 0x00000040,
	IMGSYS_HW_FLAG_TRAW      = 0x00000080,
	IMGSYS_HW_FLAG_LTR       = 0x00000100,
	IMGSYS_HW_FLAG_XTR       = 0x00000200,
	IMGSYS_HW_FLAG_DIP       = 0x00000400,
	IMGSYS_HW_FLAG_PQDIP_A   = 0x00000800,
	IMGSYS_HW_FLAG_PQDIP_B   = 0x00001000,
	IMGSYS_HW_FLAG_ME        = 0x00002000,
	IMGSYS_HW_FLAG_MAE       = 0x00004000,
	IMGSYS_HW_FLAG_DFP       = 0x00008000,
	IMGSYS_HW_FLAG_DPE       = 0x00010000,
};

/*TODO: Fill MAE/DFP/DPE/WPE_DEPTH in here */
/**
 * enum IMGSYS_REG_MAP_E
 *
 * Definition about hw register map id
 * The engine order should be the same as register order in dts
 */
enum IMGSYS_REG_MAP_E {
	REG_MAP_E_TOP = 0,
	REG_MAP_E_TRAW,
	REG_MAP_E_LTRAW,
	REG_MAP_E_DIP,
	REG_MAP_E_DIP_NR1,
	REG_MAP_E_DIP_NR2,
	REG_MAP_E_DIP_CINE,
	REG_MAP_E_PQDIP_A,
	REG_MAP_E_PQDIP_B,
	REG_MAP_E_WPE_EIS,
	REG_MAP_E_WPE_TNR,
	REG_MAP_E_WPE_LITE,
	REG_MAP_E_WPE1_DIP1,
	REG_MAP_E_OMC_TNR,
	REG_MAP_E_OMC_LITE,
	REG_MAP_E_ME,
	REG_MAP_E_ADL_A,
	REG_MAP_E_ADL_B,
	REG_MAP_E_WPE2_DIP1,
	REG_MAP_E_WPE3_DIP1,
	REG_MAP_E_DIP_TOP,
	REG_MAP_E_DIP_TOP_NR,
	REG_MAP_E_DIP_TOP_NR2,
	REG_MAP_E_TRAW_TOP,
	REG_MAP_E_ME_MMG,
	REG_MAP_E_DFP_TOP,
	REG_MAP_E_DFP_FE,
	REG_MAP_E_DFP_DRZH2N,
	REG_MAP_E_DFP_FM,
	REG_MAP_E_DFP2_TOP,
	REG_MAP_E_DFP2_DVGF,
	REG_MAP_E_IMG_VCORE,
	REG_MAP_E_MAE,
	REG_MAP_E_ISC,
	REG_MAP_E_WPE_EIS_PQDIP_A,
	REG_MAP_E_WPE_TNR_PQDIP_B,
	REG_MAP_E_MAIN_WPE0,
	REG_MAP_E_MAIN_WPE1,
	REG_MAP_E_MAIN_WPE2,
	REG_MAP_E_MAIN_OMC_LITE,
};

#endif /* _MTK_IMGSYS_ENGINE_H_ */

