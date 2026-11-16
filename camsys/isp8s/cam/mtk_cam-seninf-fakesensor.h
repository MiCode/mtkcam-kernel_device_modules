/* SPDX-License-Identifier: GPL-2.0 */
// Copyright (c) 2019 MediaTek Inc.

#ifndef __MTK_CAM_SENINF_FAKESENSOR_H__
#define __MTK_CAM_SENINF_FAKESENSOR_H__


void seninf_fakesensor_set_testmdl(struct seninf_ctx *ctx);
int seninf_fakesensor_link_info(struct seninf_ctx *ctx, struct mtk_cam_seninf_ops *g_seninf_ops);

#endif
