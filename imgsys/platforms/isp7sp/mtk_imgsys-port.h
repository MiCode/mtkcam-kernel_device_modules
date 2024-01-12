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

#ifdef IMGSYS_TF_DUMP_7SP_L
#include <dt-bindings/memory/mt6989-larb-port.h>
static struct mtk_imgsys_port_table imgsys_dma_port_mt6989[] = {
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
};
#endif

#ifdef IMGSYS_TF_DUMP_7SP_P
#include <dt-bindings/memory/mt6897-larb-port.h>
static struct mtk_imgsys_port_table imgsys_dma_port_mt6897[] = {
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
};
#endif


#endif /* _MTK_IMGSYS_PORT_H_ */
