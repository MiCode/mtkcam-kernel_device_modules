/* SPDX-License-Identifier: GPL-2.0 */
// Copyright (c) 2019 MediaTek Inc.

#ifndef __MTK_CAM_SENINF_EINT_H__
#define __MTK_CAM_SENINF_EINT_H__

#include <linux/module.h>
#include <linux/platform_device.h>
#include "mtk_cam-seninf-tsrec.h"
#include "imgsensor-user.h"

int mtk_cam_seninf_eint_core_init(struct device *dev, struct seninf_core *core);

int mtk_cam_seninf_eint_core_uninit(void);

int mtk_cam_seninf_eint_init(struct platform_device *pdev, struct seninf_ctx *ctx);

int mtk_cam_seninf_eint_uninit(struct platform_device *pdev, struct seninf_ctx *ctx);

int mtk_cam_seninf_eint_irq_en(struct seninf_ctx *ctx, u32 en);

int mtk_cam_seninf_eint_start(struct seninf_ctx *ctx);

int mtk_cam_seninf_eint_reset(struct seninf_ctx *ctx);

#endif
