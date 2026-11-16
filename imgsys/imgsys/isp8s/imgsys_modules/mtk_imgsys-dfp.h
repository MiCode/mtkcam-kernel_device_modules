/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Marvin Lin <Marvin.Lin@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_DFP_H_
#define _MTK_IMGSYS_DFP_H_

#include "./../mtk_imgsys-dev.h"
#include "./../mtk_imgsys-debug.h"

#define DFP_TOP_BASE 0x34670000
#define DFP_FE_BASE 0x34670600
#define DFP_DRZH2N_BASE 0x34670800
#define DFP_FM_BASE 0x34670A00
#define DFP2_TOP_BASE 0x34680000
#define DFP_DVGF_BASE 0x34680C00


#define DFP_TOP_CTL_OFFSET      0x0000
#define DFP_TOP_CTL_RANGE       0x200

#define FE_CTL_OFFSET      0x0000
#define FE_CTL_RANGE       0x100

#define DRZH2N_CTL_OFFSET      0x0000
#define DRZH2N_CTL_RANGE       0x100

#define FM_CTL_OFFSET      0x0000
#define FM_CTL_RANGE       0x100

#define DFP2_TOP_CTL_OFFSET      0x0000
#define DFP2_TOP_CTL_RANGE       0x200

#define DVGF_CTL_OFFSET      0x0000
#define DVGF_CTL_RANGE       0x300

#define MMG_CTL_OFFSET      0x0000
#define MMG_CTL_RANGE       0xA50
#define MMG_CTL_RANGE_TF    0x40



void imgsys_dfp_set_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_dfp_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine);
void imgsys_dfp_uninit(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_dfp_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_dfp_cmdq_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev, void *pkt, int hw_idx);
bool imgsys_dfp_done_chk(struct mtk_imgsys_dev *imgsys_dev, uint32_t engine);
void imgsys_dfp_updatecq(struct mtk_imgsys_dev *imgsys_dev,
			struct img_swfrm_info *user_info, int req_fd, u64 tuning_iova,
			unsigned int mode);
int imgsys_dfp_tfault_callback(int port, dma_addr_t mva, void *data);

#endif /* _MTK_IMGSYS_DFP_H_ */
