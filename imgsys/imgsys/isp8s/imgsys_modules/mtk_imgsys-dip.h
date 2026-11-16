/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#ifndef _MTK_DIP_DIP_H_
#define _MTK_DIP_DIP_H_

// Standard C header file

// kernel header file

// mtk imgsys local header file
#include "./../mtk_imgsys-dev.h"


// Local header file
#include "./../mtk_imgsys-engine-isp8s.h"
#include "./../mtk_imgsys-debug.h"

#include <mtk_imgsys-cmdq.h>


/********************************************************************
 * Global Define
 ********************************************************************/
/* DIP */
#define DIP_TOP_ADDR	0x34190000
#define DIP_TOP_ADDR_P	0x15190000
#define IMGSYS_DIP_BASE		(0x34100000)
#define IMGSYS_DIP_BASE_P	(0x15100000)//YWTBD

#define DIPNR2_CINE_SEL		0x0070//bit24
#define DIPNR2_DMA_ERR		0x1018//bit16 17

#define DIP_DBG_SEL		0x210
#define DIP_DBG_OUT		0x214
#define DIP_DMATOP_DBG_SEL	0x1020
#define DIP_DMATOP_DBG_PORT	0x1024
#define DIP_DMANR2_DBG_SEL	0x1028
#define DIP_DMANR2_DBG_PORT	0x102C

#define DIP_NR3D_DBG_SEL	0x401C
#define DIP_NR3D_DBG_CNT	0x4020
#define DIP_NR3D_DBG_ST		0x4024
#define DIP_NR3D_DBG_POINTS	50

#define DIP_SNRS_DBG_SEL	0x7614
#define DIP_SNRS_DBG_OUT	0x7618
#define DIP_YUFD_DBG_SEL	0x91B0
#define DIP_YUFD_DBG_OUT	0x91B4
#define DIP_YUFE2_DBG_SEL	0x94A8
#define DIP_YUFE2_DBG_OUT	0x94AC
#define DIP_YUFE3_DBG_SEL	0x95E8
#define DIP_YUFE3_DBG_OUT	0x95EC
#define DIP1_URZ6T4T1_DBG_SEL	0x2030
#define DIP1_URZ6T4T1_DBG_OUT	0x2034
#define DIP1_EECNR_DBG_SEL	0x22CC
#define DIP1_EECNR_DBG_OUT	0x22D0
#define DIP1_ANS_DBG_SEL	0x30C8
#define DIP1_ANS_DBG_OUT	0x30CC
#define DIP1_CSMC_DBG_SEL	0x33B0
#define DIP1_CSMC_DBG_OUT	0x33B4
#define DIP1_DWG_DBG_SEL	0x4544
#define DIP1_DWG_DBG_OUT	0x4548
#define DIP1_RAC_DBG_SEL	0x45B8
#define DIP1_RAC_DBG_OUT	0x45BC
#define DIP1_CSMCS2_DBG_SEL	0x47DC
#define DIP1_CSMCS2_DBG_OUT	0x47E0
#define DIP1_TNC_DBG_SEL	0x6870
#define DIP1_TNC_DBG_OUT	0x6874
#define DIP2_SNR_DBG_SEL	0x570C
#define DIP2_SNR_DBG_OUT	0x5710
#define DIP2_CSMCS1_DBG_SEL	0x70DC
#define DIP2_CSMCS1_DBG_OUT	0x70E0
#define DIP2_YUFE1_DBG_SEL	0x74A8
#define DIP2_YUFE1_DBG_OUT	0x74AC
#define DIP2_VDCE_DBG_SEL	0x8E48
#define DIP2_VDCE_DBG_OUT	0x8E4C
#define DIP3_BOK_DBG_SEL	0x19B0
#define DIP3_BOK_DBG_OUT	0x19B4
#define DIP3_FPNR_DBG_SEL	0x2600
#define DIP3_FPNR_DBG_OUT	0x2604

/* DIP NR1 */
#define DIP_NR1_ADDR		0x341A0000
#define DIP_NR1_ADDR_P		0x151A0000

/* DIP NR2 */
#define DIP_NR2_ADDR		0x341B0000
#define DIP_NR2_ADDR_P		0x151B0000

/* DIP CINE */
#define DIP_CINE_ADDR		0x341D0000
#define DIP_CINE_ADDR_P		0x151D0000

#define DIP_DMA_NAME_MAX_SIZE	20

#define DIP_IMGI_STATE_CHECKSUM		(0x00100)
#define DIP_IMGI_LINE_PIX_CNT_TMP	(0x00200)
#define DIP_IMGI_LINE_PIX_CNT		(0x00300)
#define DIP_IMGI_IMPORTANT_STATUS	(0x00400)
#define DIP_IMGI_SMI_DEBUG_DATA_CASE0	(0x00500)
#define DIP_IMGI_TILEX_BYTE_CNT		(0x00600)
#define DIP_IMGI_TILEY_CNT			(0x00700)
#define DIP_IMGI_BURST_LINE_CNT		(0x00800)
#define DIP_IMGI_XFER_Y_CNT			(0x00900)
#define DIP_IMGI_UFO_STATE_CHECKSUM		(0x00A00)
#define DIP_IMGI_UFO_LINE_PIX_CNT_TMP	(0x00B00)
#define DIP_IMGI_UFO_LINE_PIX_CNT		(0x00C00)
#define DIP_IMGI_FIFO_DEBUG_DATA_CASE1		(0x10600)
#define DIP_IMGI_FIFO_DEBUG_DATA_CASE3		(0x30600)
#define DIP_YUVO_T1_FIFO_DEBUG_DATA_CASE1	(0x10700)
#define DIP_YUVO_T1_FIFO_DEBUG_DATA_CASE3	(0x30700)

#define DIP_CQ_DESC_NUM		343 // align with userspace
#define DIP_REG_SIZE		(0x2D000) // align with userspace
#define DIP_TDR_BUF_MAXSZ 163840 // align with userspace //YWTBD
#define DIP_CQ_DESC_SIZE		(0x102C) //align with userspace
#define DIP_CINE_SEL_VA_OFST		(DIP_CQ_DESC_SIZE + 0x17070)
/********************************************************************
 * Enum Define
 ********************************************************************/
enum DIPDmaDebugType {
	DIP_ORI_RDMA_DEBUG,
	DIP_ORI_RDMA_UFO_DEBUG,
	DIP_ORI_WDMA_DEBUG,
	DIP_ULC_RDMA_DEBUG,
	DIP_ULC_WDMA_DEBUG,
};

/********************************************************************
 * Structure Define
 ********************************************************************/
struct DIPRegDumpInfo {
	unsigned int oft;
	unsigned int end;
};

struct DIPDmaDebugInfo {
	char DMAName[DIP_DMA_NAME_MAX_SIZE];
	enum DIPDmaDebugType DMADebugType;
	unsigned int DMAIdx;
};

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Public Functions
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void imgsys_dip_set_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_dip_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_dip_cmdq_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev,
		void *pkt, int hw_idx);
void imgsys_dip_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine);

void imgsys_dip_uninit(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_dip_updatecq(struct mtk_imgsys_dev *imgsys_dev,
			 struct img_swfrm_info *user_info,
			 struct private_data *priv_data,
			 int req_fd,
			 u64 tuning_iova,
			 unsigned int mode);
int imgsys_dip_tfault_callback(int port,
			dma_addr_t mva, void *data);
bool imgsys_dip_done_chk(struct mtk_imgsys_dev *imgsys_dev, uint32_t engine);
int imgsys_dip_check_power_domain(struct mtk_imgsys_dev *imgsys_dev,
				  struct img_swfrm_info *user_info,
				  struct private_data *priv_data,
				  unsigned int mode);
#endif /* _MTK_DIP_DIP_H_ */
