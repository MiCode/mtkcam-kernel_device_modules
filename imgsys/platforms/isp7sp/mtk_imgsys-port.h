/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Floria Huang <floria.huang@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_PORT_H_
#define _MTK_IMGSYS_PORT_H_

struct mtk_imgsys_port_table;
#include "modules/mtk_imgsys-pqdip.h"
#include "modules/mtk_imgsys-me.h"
#include "modules/mtk_imgsys-traw.h"

#ifdef IMGSYS_TF_DUMP_7SP_L
#include <dt-bindings/memory/mt6989-larb-port.h>
static struct mtk_imgsys_port_table imgsys_dma_port_mt6989[] = {
	/* Larb9 -- 9*/
	{SMMU_L9_P0_IMGI_T1_B         , imgsys_traw_tfault_callback},
	{SMMU_L9_P1_IMGI_T1_N_B       , imgsys_traw_tfault_callback},
	{SMMU_L9_P2_IMGCI_T1_B        , imgsys_traw_tfault_callback},
	{SMMU_L9_P3_IMGCI_T1_N_B      , imgsys_traw_tfault_callback},
	{SMMU_L9_P4_SMTI_T1_B         , imgsys_traw_tfault_callback},
	{SMMU_L9_P5_YUVO_T1_B         , imgsys_traw_tfault_callback},
	{SMMU_L9_P6_YUVO_T1_N_B       , imgsys_traw_tfault_callback},
	{SMMU_L9_P7_YUVCO_T1_B        , imgsys_traw_tfault_callback},
	{SMMU_L9_P8_YUVO_T2_B         , imgsys_traw_tfault_callback},
	{SMMU_L9_P9_YUVO_T3_B         , imgsys_traw_tfault_callback},
	{SMMU_L9_P10_TNCSTO_T1_B      , imgsys_traw_tfault_callback},
	{SMMU_L9_P11_TNCSTI_T1_B      , imgsys_traw_tfault_callback},
	{SMMU_L9_P12_TNCSTI_T4_B      , imgsys_traw_tfault_callback},
	{SMMU_L9_P17_SMTO_T1_B        , imgsys_traw_tfault_callback},
	{SMMU_L9_P18_TNCSO_T1_B       , imgsys_traw_tfault_callback},
	/* Larb11 -- 11*/
	{SMMU_L11_P0_WPE_RDMA_0       , imgsys_wpe_tfault_callback},
	{SMMU_L11_P1_WPE_RDMA_1       , imgsys_wpe_tfault_callback},
	{SMMU_L11_P2_WPE_RDMA_2       , imgsys_wpe_tfault_callback},
	{SMMU_L11_P3_PIMGI_P1         , imgsys_pqdip_tfault_callback},
	{SMMU_L11_P4_PIMGBI_P1        , imgsys_pqdip_tfault_callback},
	{SMMU_L11_P5_STSCMD_P1        , NULL},
	{SMMU_L11_P6_STSCMD_P2        , imgsys_pqdip_tfault_callback},
	{SMMU_L11_P7_STSCMD_P3        , imgsys_pqdip_tfault_callback},
	{SMMU_L11_P8_WPE_WDMA_0       , imgsys_wpe_tfault_callback},
	{SMMU_L11_P9_WROT_P1          , imgsys_pqdip_tfault_callback},
	{SMMU_L11_P10_TCCSO_P1        , imgsys_pqdip_tfault_callback},
	/* Larb22 -- 11*/
	{SMMU_L22_P0_WPE_RDMA_0       , imgsys_wpe_tfault_callback},
	{SMMU_L22_P1_WPE_RDMA_1       , imgsys_wpe_tfault_callback},
	{SMMU_L22_P2_WPE_RDMA_2       , imgsys_wpe_tfault_callback},
	{SMMU_L22_P3_PIMGI_P1         , imgsys_pqdip_tfault_callback},
	{SMMU_L22_P4_PIMGBI_P1        , imgsys_pqdip_tfault_callback},
	{SMMU_L22_P5_STSCMD_P1        , NULL},
	{SMMU_L22_P6_STSCMD_P2        , imgsys_pqdip_tfault_callback},
	{SMMU_L22_P7_STSCMD_P3        , imgsys_pqdip_tfault_callback},
	{SMMU_L22_P8_WPE_WDMA_0       , imgsys_wpe_tfault_callback},
	{SMMU_L22_P9_WROT_P1          , imgsys_pqdip_tfault_callback},
	{SMMU_L22_P10_TCCSO_P1        , imgsys_pqdip_tfault_callback},
	/* Larb23 -- 11*/
	{SMMU_L23_P0_WPE_RDMA_0       , imgsys_wpe_tfault_callback},
	{SMMU_L23_P1_WPE_RDMA_1       , imgsys_wpe_tfault_callback},
	{SMMU_L23_P2_WPE_RDMA_2       , imgsys_wpe_tfault_callback},
	{SMMU_L23_P3_PIMGI_P1         , NULL},
	{SMMU_L23_P4_PIMGBI_P1        , NULL},
	{SMMU_L23_P5_STSCMD_P1        , NULL},
	{SMMU_L23_P6_STSCMD_P2        , NULL},
	{SMMU_L23_P7_STSCMD_P3        , NULL},
	{SMMU_L23_P8_WPE_WDMA_0       , imgsys_wpe_tfault_callback},
	{SMMU_L23_P9_WROT_P1          , NULL},
	{SMMU_L23_P10_TCCSO_P1        , NULL},
	/* Larb12 */
	{SMMU_L12_P2_ME_RDMA_0        , ME_TranslationFault_callback},
	{SMMU_L12_P3_ME_WDMA_0        , ME_TranslationFault_callback},
	{SMMU_L12_P4_MEMMG_RDMA_0     , MMG_TranslationFault_callback},
	{SMMU_L12_P5_MEMMG_WDMA_0     , MMG_TranslationFault_callback},
	/* Larb28 */
	{SMMU_L28_P0_IMGI_T1_A        , imgsys_traw_tfault_callback},
	{SMMU_L28_P1_IMGI_T1_N_A      , imgsys_traw_tfault_callback},
	{SMMU_L28_P2_IMGCI_T1_A       , imgsys_traw_tfault_callback},
	{SMMU_L28_P3_IMGCI_T1_N_A     , imgsys_traw_tfault_callback},
	{SMMU_L28_P4_YUVO_T1_A        , imgsys_traw_tfault_callback},
	{SMMU_L28_P5_YUVO_T1_N_A      , imgsys_traw_tfault_callback},
	{SMMU_L28_P6_YUVO_T2_A        , imgsys_traw_tfault_callback},
	{SMMU_L28_P7_YUVO_T3_A        , imgsys_traw_tfault_callback},
	/* Larb40 */
	{SMMU_L40_P0_SMTI_T1_A        , imgsys_traw_tfault_callback},
	{SMMU_L40_P1_SMTI_T4_A        , imgsys_traw_tfault_callback},
	{SMMU_L40_P2_TNCSTI_T1_A      , imgsys_traw_tfault_callback},
	{SMMU_L40_P3_TNCSTI_T4_A      , imgsys_traw_tfault_callback},
	{SMMU_L40_P4_LTMSTI_T1_A      , imgsys_traw_tfault_callback},
	{SMMU_L40_P5_YUVCO_T1_A       , imgsys_traw_tfault_callback},
	{SMMU_L40_P6_TNCSTO_T1_A      , imgsys_traw_tfault_callback},
	{SMMU_L40_P7_STSCMD_T2_A      , imgsys_traw_tfault_callback},
	{SMMU_L40_P8_STSCMD_T3_A      , imgsys_traw_tfault_callback},
	{SMMU_L40_P9_RESERVED         , imgsys_traw_tfault_callback},
	{SMMU_L40_P10_RESERVED        , imgsys_traw_tfault_callback},
	{SMMU_L40_P11_TNCSO_T1_A      , imgsys_traw_tfault_callback},
	{SMMU_L40_P12_SMTO_T1_A       , imgsys_traw_tfault_callback},
	{SMMU_L40_P13_SMTO_T4_A       , imgsys_traw_tfault_callback},
	{SMMU_L40_P14_LTMSO_T1_A      , imgsys_traw_tfault_callback},

};
#endif

#ifdef IMGSYS_TF_DUMP_7SP_P
#include <dt-bindings/memory/mt6897-larb-port.h>
static struct mtk_imgsys_port_table imgsys_dma_port_mt6897[] = {
	/* larb9 */
	{M4U_L9_P0_IMGI_T1_B    , imgsys_traw_tfault_callback},
	{M4U_L9_P1_IMGI_T1_N_B  , imgsys_traw_tfault_callback},
	{M4U_L9_P2_IMGCI_T1_B   , imgsys_traw_tfault_callback},
	{M4U_L9_P3_IMGCI_T1_N_B , imgsys_traw_tfault_callback},
	{M4U_L9_P4_SMTI_T1_B    , imgsys_traw_tfault_callback},
	{M4U_L9_P5_YUVO_T1_B    , imgsys_traw_tfault_callback},
	{M4U_L9_P6_YUVO_T1_N_B  , imgsys_traw_tfault_callback},
	{M4U_L9_P7_YUVCO_T1_B   , imgsys_traw_tfault_callback},
	{M4U_L9_P8_YUVO_T2_B    , imgsys_traw_tfault_callback},
	{M4U_L9_P9_YUVO_T3_B    , imgsys_traw_tfault_callback},
	{M4U_L9_P10_TNCSTO_T1_B , imgsys_traw_tfault_callback},
	{M4U_L9_P11_TNCSTI_T1_B , imgsys_traw_tfault_callback},
	{M4U_L9_P12_TNCSTI_T4_B , imgsys_traw_tfault_callback},
	{M4U_L9_P14_SMTO_T1_B   , imgsys_traw_tfault_callback},
	{M4U_L9_P15_TNCSO_T1_B  , imgsys_traw_tfault_callback},
	/* larb11 */
	{M4U_L11_P0_WPE_RDMA_0  , imgsys_wpe_tfault_callback},
	{M4U_L11_P1_WPE_RDMA_1  , imgsys_wpe_tfault_callback},
	{M4U_L11_P2_WPE_RDMA_2  , imgsys_wpe_tfault_callback},
	{M4U_L11_P3_PIMGI_P1    , imgsys_pqdip_tfault_callback},
	{M4U_L11_P4_PIMGBI_P1   , imgsys_pqdip_tfault_callback},
	{M4U_L11_P5_WPE_WDMA_0  , imgsys_wpe_tfault_callback},
	{M4U_L11_P6_WROT_P1     , imgsys_pqdip_tfault_callback},
	{M4U_L11_P7_TCCSO_P1    , imgsys_pqdip_tfault_callback},
	/* larb22 */
	{M4U_L22_P0_WPE_RDMA_0  , imgsys_wpe_tfault_callback},
	{M4U_L22_P1_WPE_RDMA_1  , imgsys_wpe_tfault_callback},
	{M4U_L22_P2_WPE_RDMA_2  , imgsys_wpe_tfault_callback},
	{M4U_L22_P3_PIMGI_P1    , imgsys_pqdip_tfault_callback},
	{M4U_L22_P4_PIMGBI_P1   , imgsys_pqdip_tfault_callback},
	{M4U_L22_P5_WPE_WDMA_0  , imgsys_wpe_tfault_callback},
	{M4U_L22_P6_WROT_P1     , imgsys_pqdip_tfault_callback},
	{M4U_L22_P7_TCCSO_P1    , imgsys_pqdip_tfault_callback},
	/* larb23 */
	{M4U_L23_P0_WPE_RDMA_0  , imgsys_wpe_tfault_callback},
	{M4U_L23_P1_WPE_RDMA_1  , imgsys_wpe_tfault_callback},
	{M4U_L23_P2_WPE_RDMA_2  , imgsys_wpe_tfault_callback},
	{M4U_L23_P3_PIMGI_P1    , NULL},
	{M4U_L23_P4_PIMGBI_P1   , NULL},
	{M4U_L23_P5_WPE_WDMA_0  , imgsys_wpe_tfault_callback},
	{M4U_L23_P6_WROT_P1     , NULL},
	{M4U_L23_P7_TCCSO_P1    , NULL},
	/* Larb12 */
	{M4U_L12_P2_ME_RDMA_0        , ME_TranslationFault_callback},
	{M4U_L12_P3_ME_WDMA_0        , ME_TranslationFault_callback},
	{M4U_L12_P4_MEMMG_RDMA_0     , MMG_TranslationFault_callback},
	{M4U_L12_P5_MEMMG_WDMA_0     , MMG_TranslationFault_callback},
	/* larb28 */
	{M4U_L28_P0_IMGI_T1_A    , imgsys_traw_tfault_callback},
	{M4U_L28_P1_IMGI_T1_N_A  , imgsys_traw_tfault_callback},
	{M4U_L28_P2_IMGCI_T1_A   , imgsys_traw_tfault_callback},
	{M4U_L28_P3_IMGCI_T1_N_A , imgsys_traw_tfault_callback},
	{M4U_L28_P4_YUVO_T1_A    , imgsys_traw_tfault_callback},
	{M4U_L28_P5_YUVO_T1_N_A  , imgsys_traw_tfault_callback},
	{M4U_L28_P6_YUVO_T2_A    , imgsys_traw_tfault_callback},
	{M4U_L28_P7_YUVO_T3_A    , imgsys_traw_tfault_callback},
	{M4U_L28_P12_SMTI_T1_A   , imgsys_traw_tfault_callback},
	{M4U_L28_P13_SMTI_T4_A   , imgsys_traw_tfault_callback},
	{M4U_L28_P14_TNCSTI_T1_A , imgsys_traw_tfault_callback},
	{M4U_L28_P15_TNCSTI_T4_A , imgsys_traw_tfault_callback},
	{M4U_L28_P16_LTMSTI_T1_A , imgsys_traw_tfault_callback},
	{M4U_L28_P17_YUVCO_T1_A  , imgsys_traw_tfault_callback},
	{M4U_L28_P18_TNCSTO_T1_A , imgsys_traw_tfault_callback},
	{M4U_L28_P21_TNCSO_T1_A  , imgsys_traw_tfault_callback},
	{M4U_L28_P22_SMTO_T1_A   , imgsys_traw_tfault_callback},
	{M4U_L28_P23_SMTO_T4_A   , imgsys_traw_tfault_callback},
	{M4U_L28_P24_LTMSO_T1_A   , imgsys_traw_tfault_callback},
};
#endif


#endif /* _MTK_IMGSYS_PORT_H_ */
