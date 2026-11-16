/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Christopher Chen <christopher.chen@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_PLAT_H_
#define _MTK_IMGSYS_PLAT_H_

#include <linux/clk.h>

struct clk_bulk_data imgsys_isp8s_clks_mt6993[] = {
	{  // cksys_mm_clk CKSYS_MM_CAMTM_SEL
		.id = "CKSYS_MM_CAMTM_SEL",
	},
	{  // img_vcore_d1a_clk IMG_VCORE_GALS_DISP_CAM_P2
		.id = "VCORE_GALS",
	},
	{  // img_vcore_d1a_clk IMG_VCORE_MAIN_CAM_P2
		.id = "VCORE_MAIN",
	},
	{  // img_vcore_d1a_clk IMG_VCORE_SUB0_CAM_P2
		.id = "VCORE_SUB0",
	},
	{  // img_vcore_d1a_clk IMG_VCORE_SUB1_CAM_P2
		.id = "VCORE_SUB1",
	},
	{  // img_vcore_d1a_clk IMG_VCORE_SUB2_CAM_P2
		.id = "VCORE_SUB2",
	},
	{  // imgsys_main_clk IMG_FDVT_CAM_P2
		.id = "IMG_FDVT",
	},
	{  // imgsys_main_clk IMG_LARB12_CAM_P2
		.id = "IMG_LARB12",
	},
	{  // imgsys_main_clk IMG_IPESYS_CVFS26_CAM_P2
		.id = "IMG_IPE_CVFS26",
	},
	{  // imgsys_main_clk IMG_LARB9_CAM_P2
		.id = "IMG_LARB9",
	},
	{  // imgsys_main_clk IMG_TRAW0_CAM_P2
		.id = "IMG_TRAW0",
	},
	{  // imgsys_main_clk IMG_TRAW1_CAM_P2
		.id = "IMG_TRAW1",
	},
	{  // imgsys_main_clk IMG_DIP0_CAM_P2
		.id = "IMG_DIP0",
	},
	{  // imgsys_main_clk IMG_WPE0_CAM_P2
		.id = "IMG_WPE0",
	},
	{  // imgsys_main_clk IMG_IPE_CAM_P2
		.id = "IMG_IPE",
	},
	{  // imgsys_main_clk IMG_WPE1_CAM_P2
		.id = "IMG_WPE1",
	},
	{  // imgsys_main_clk IMG_WPE2_CAM_P2
		.id = "IMG_WPE2",
	},
	{  // imgsys_main_clk IMG_AVS_CAM_P2
		.id = "IMG_AVS",
	},
	{  // imgsys_main_clk IMG_IPS_CAM_P2
		.id = "IMG_IPS",
	},
	{  // imgsys_main_clk IMG_ADLWR1_CAM_P2
		.id = "IMG_ADLWR1",
	},
	{  // imgsys_main_clk IMG_ROOTCQ_CAM_P2
		.id = "IMG_ROOTCQ",
	},
	{  // imgsys_main_clk IMG_BLS_CAM_P2
		.id = "IMG_BLS",
	},
	{  // imgsys_main_clk IMG_SUB_COMMON0_CAM_P2
		.id = "IMG_SUB_COMMON0",
	},
	{  // imgsys_main_clk IMG_SUB_COMMON1_CAM_P2
		.id = "IMG_SUB_COMMON1",
	},
	{  // imgsys_main_clk IMG_SUB_COMMON2_CAM_P2
		.id = "IMG_SUB_COMMON2",
	},
	{  // imgsys_main_clk IMG_SUB_COMMON3_CAM_P2
		.id = "IMG_SUB_COMMON3",
	},
	{  // imgsys_main_clk IMG_SUB_COMMON4_CAM_P2
		.id = "IMG_SUB_COMMON4",
	},
	{  // imgsys_main_clk IMG_SUB_COMMON5_CAM_P2
		.id = "IMG_SUB_COMMON5",
	},
	{  // imgsys_main_clk IMG_SUB_COMMON6_CAM_P2
		.id = "IMG_SUB_COMMON6",
	},
	{  // imgsys_main_clk IMG26_CAM_P2
		.id = "IMG26",
	},
	{  // imgsys_main_clk IMG_BWR_CAM_P2
		.id = "IMG_BWR",
	},
	{  // imgsys_main_clk IMG_ISC_CAM_P2
		.id = "IMG_ISC",
	},
	{  // imgsys_main_clk IMGSYS_CVFS26_CAM_P2
		.id = "IMG_CVFS26",
	},
	{  // dip_top_dip1_clk DIP_TOP_DIP1_DIP_TOP_CAM_P2
		.id = "DIP_TOP_DIP1_DIP_TOP",
	},
	{  // dip_top_dip1_clk DIP_TOP_DIP1_LARB10_CAM_P2
		.id = "DIP_TOP_DIP1_LARB10",
	},
	{  // dip_top_dip1_clk DIP_TOP_DIP1_LARB15_CAM_P2
		.id = "DIP_TOP_DIP1_LARB15",
	},
	{  // dip_top_dip1_clk DIP_TOP_DIP1_LARB38_CAM_P2
		.id = "DIP_TOP_DIP1_LARB38",
	},
	{  // dip_top_dip1_clk DIP_TOP_DIP1_LARB39_CAM_P2
		.id = "DIP_TOP_DIP1_LARB39",
	},
	{  // dip_nr1_dip1_clk DIP_NR1_DIP1_LARB_CAM_P2
		.id = "DIP_NR1_DIP1_LARB",
	},
	{  // dip_nr1_dip1_clk DIP_NR1_DIP1_DIP_NR1_CAM_P2
		.id = "DIP_NR1_DIP1_DIP_NR1",
	},
	{  // dip_nr2_dip1_clk DIP_NR2_DIP1_DIP_NR_CAM_P2
		.id = "DIP_NR2_DIP1_DIP_NR",
	},
	{  // dip_nr2_dip1_clk DIP_NR2_DIP1_LARB15_CAM_P2
		.id = "DIP_NR2_DIP1_LARB15",
	},
	{  // dip_nr2_dip1_clk DIP_NR2_DIP1_LARB39_CAM_P2
		.id = "DIP_NR2_DIP1_LARB39",
	},
	{  // wpe_eis_dip1_clk WPE_EIS_DIP1_LARB_U0_CAM_P2
		.id = "WPE_EIS_DIP1_LARB_U0",
	},
	{  // wpe_eis_dip1_clk WPE_EIS_DIP1_LARB_U1_CAM_P2
		.id = "WPE_EIS_DIP1_LARB_U1",
	},
	{  // wpe_eis_dip1_clk WPE_EIS_DIP1_WPE_MACRO_CAM_P2
		.id = "WPE_EIS_DIP1_WPE_MACRO",
	},
	{  // wpe_eis_dip1_clk WPE_EIS_DIP1_WPE_CAM_P2
		.id = "WPE_EIS_DIP1_WPE",
	},
	{  // wpe_eis_dip1_clk WPE_EIS_DIP1_PQDIP_CAM_P2
		.id = "WPE_EIS_DIP1_PQDIP",
	},
	{  // wpe_eis_dip1_clk WPE_EIS_DIP1_PQDIP_DMA_CAM_P2
		.id = "WPE_EIS_DIP1_PQDIP_DMA",
	},
	{  // wpe_eis_dip1_clk WPE_EIS_DIP1_OMC_CAM_P2
		.id = "WPE_EIS_DIP1_OMC",
	},
	{  // wpe_eis_dip1_clk WPE_EIS_DIP1_DPE_CAM_P2
		.id = "WPE_EIS_DIP1_DPE",
	},
	{  // wpe_eis_dip1_clk WPE_EIS_DIP1_DFPS_CAM_P2
		.id = "WPE_EIS_DIP1_DFPS",
	},
	{  // wpe_eis_dip1_clk WPE_EIS_DIP1_DFP0_CAM_P2
		.id = "WPE_EIS_DIP1_DFP0",
	},
	{  // wpe_eis_dip1_clk WPE_EIS_DIP1_DFP1_CAM_P2
		.id = "WPE_EIS_DIP1_DFP1",
	},
	{  // wpe_eis_dip1_clk WPE_EIS_DIP1_ME_CAM_P2
		.id = "WPE_EIS_DIP1_ME",
	},
	{  // wpe_eis_dip1_clk WPE_EIS_DIP1_MMG_CAM_P2
		.id = "WPE_EIS_DIP1_MMG",
	},
	{  // wpe_eis_dip1_clk WPE_EIS_DIP1_WPE_26M_EN_CAM_P2
		.id = "WPE_EIS_DIP1_WPE_26M_EN",
	},
	{  // wpe_tnr_dip1_clk WPE_TNR_DIP1_LARB_U0_CAM_P2
		.id = "WPE_TNR_DIP1_LARB_U0",
	},
	{  // wpe_tnr_dip1_clk WPE_TNR_DIP1_LARB_U1_CAM_P2
		.id = "WPE_TNR_DIP1_LARB_U1",
	},
	{  // wpe_tnr_dip1_clk WPE_TNR_DIP1_WPE_MACRO_CAM_P2
		.id = "WPE_TNR_DIP1_WPE_MACRO",
	},
	{  // wpe_tnr_dip1_clk WPE_TNR_DIP1_WPE_CAM_P2
		.id = "WPE_TNR_DIP1_WPE",
	},
	{  // wpe_tnr_dip1_clk WPE_TNR_DIP1_PQDIP_CAM_P2
		.id = "WPE_TNR_DIP1_PQDIP",
	},
	{  // wpe_tnr_dip1_clk WPE_TNR_DIP1_PQDIP_DMA_CAM_P2
		.id = "WPE_TNR_DIP1_PQDIP_DMA",
	},
	{  // wpe_tnr_dip1_clk WPE_TNR_DIP1_OMC_CAM_P2
		.id = "WPE_TNR_DIP1_OMC",
	},
	{  // wpe_tnr_dip1_clk WPE_TNR_DIP1_DPE_CAM_P2
		.id = "WPE_TNR_DIP1_DPE",
	},
	{  // wpe_tnr_dip1_clk WPE_TNR_DIP1_DFPS_CAM_P2
		.id = "WPE_TNR_DIP1_DFPS",
	},
	{  // wpe_tnr_dip1_clk WPE_TNR_DIP1_DFP0_CAM_P2
		.id = "WPE_TNR_DIP1_DFP0",
	},
	{  // wpe_tnr_dip1_clk WPE_TNR_DIP1_DFP1_CAM_P2
		.id = "WPE_TNR_DIP1_DFP1",
	},
	{  // wpe_tnr_dip1_clk WPE_TNR_DIP1_ME_CAM_P2
		.id = "WPE_TNR_DIP1_ME",
	},
	{  // wpe_tnr_dip1_clk WPE_TNR_DIP1_MMG_CAM_P2
		.id = "WPE_TNR_DIP1_MMG",
	},
	{  // wpe_lite_dip1_clk WPE_LITE_DIP1_LARB_U0_CAM_P2
		.id = "WPE_LITE_DIP1_LARB_U0",
	},
	{  // wpe_lite_dip1_clk WPE_LITE_DIP1_LARB_U1_CAM_P2
		.id = "WPE_LITE_DIP1_LARB_U1",
	},
	{  // wpe_lite_dip1_clk WPE_LITE_DIP1_WPE_MACRO_CAM_P2
		.id = "WPE_LITE_DIP1_WPE_MACRO",
	},
	{  // wpe_lite_dip1_clk WPE_LITE_DIP1_WPE_CAM_P2
		.id = "WPE_LITE_DIP1_WPE",
	},
	{  // wpe_lite_dip1_clk WPE_LITE_DIP1_PQDIP_CAM_P2
		.id = "WPE_LITE_DIP1_PQDIP",
	},
	{  // wpe_lite_dip1_clk WPE_LITE_DIP1_PQDIP_DMA_CAM_P2
		.id = "WPE_LITE_DIP1_PQDIP_DMA",
	},
	{  // wpe_lite_dip1_clk WPE_LITE_DIP1_OMC_CAM_P2
		.id = "WPE_LITE_DIP1_OMC",
	},
	{  // wpe_lite_dip1_clk WPE_LITE_DIP1_DPE_CAM_P2
		.id = "WPE_LITE_DIP1_DPE",
	},
	{  // wpe_lite_dip1_clk WPE_LITE_DIP1_DFPS_CAM_P2
		.id = "WPE_LITE_DIP1_DFPS",
	},
	{  // wpe_lite_dip1_clk WPE_LITE_DIP1_DFP0_CAM_P2
		.id = "WPE_LITE_DIP1_DFP0",
	},
	{  // wpe_lite_dip1_clk WPE_LITE_DIP1_DFP1_CAM_P2
		.id = "WPE_LITE_DIP1_DFP1",
	},
	{  // wpe_lite_dip1_clk WPE_LITE_DIP1_ME_CAM_P2
		.id = "WPE_LITE_DIP1_ME",
	},
	{  // wpe_lite_dip1_clk WPE_LITE_DIP1_MMG_CAM_P2
		.id = "WPE_LITE_DIP1_MMG",
	},
	{  // traw_dip1_clk TRAW_DIP1_LARB28_CAM_P2
		.id = "TRAW_DIP1_LARB28",
	},
	{  // traw_dip1_clk TRAW_DIP1_LARB40_CAM_P2
		.id = "TRAW_DIP1_LARB40",
	},
	{  // traw_dip1_clk TRAW_DIP1_TRAW_CAM_P2
		.id = "TRAW_DIP1_TRAW",
	},
	{  // traw_cap_dip1_clk TRAW_CAP_DIP1_TRAW_CAP_CAM_P2
		.id = "TRAW_CAP_DIP1_TRAW_CAP",
	},
	{  // dip_cine_dip1_clk DIP_CINE_DIP1_LARB_CAM_P2
		.id = "DIP_CINE_DIP1_LARB",
	},
	{  // dip_cine_dip1_clk DIP_CINE_DIP1_DIP_CINE_CAM_P2
		.id = "DIP_CINE_DIP1",
	},
};
#define MTK_IMGSYS_CLK_NUM_MT6993	ARRAY_SIZE(imgsys_isp8s_clks_mt6993)
//#define MTK_IMGSYS_CLK_NUM_MT6899	ARRAY_SIZE(imgsys_isp8_clks_mt6899)

#endif /* _MTK_IMGSYS_PLAT_H_ */
