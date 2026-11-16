/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 *
 * Author: Ming-Hsuan Chiang <ming-hsuan.chiang@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_MAE_H_
#define _MTK_IMGSYS_MAE_H_

#include "./../mtk_imgsys-dev.h"
#include "./../mtk_imgsys-debug.h"
#include "./../mtk_imgsys-engine-isp8s.h"

int MAE_TranslationFault_callback(int port, dma_addr_t mva, void *data);
void imgsys_mae_init(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_mae_set(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_mae_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine);
bool imgsys_mae_done_chk(struct mtk_imgsys_dev *imgsys_dev, uint32_t engine);
void imgsys_mae_uninit(struct mtk_imgsys_dev *imgsys_dev);

#endif /* _MTK_IMGSYS_MAE_H_ */
