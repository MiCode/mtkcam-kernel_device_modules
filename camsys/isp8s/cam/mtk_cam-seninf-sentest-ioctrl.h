/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 MediaTek Inc. */

#ifndef __MTK_CAM_SENINF_SENTEST_IOCTRL_H__
#define __MTK_CAM_SENINF_SENTEST_IOCTRL_H__

#include "mtk_cam-seninf.h"

/******************************************************************************/
// seninf sentest call back ioctl --- function
/******************************************************************************/

int seninf_sentest_ioctl_entry(struct seninf_ctx *ctx, void *arg);

int s_sentest_max_isp_clk_clk_en_for_set_ctrl(struct seninf_ctx *ctx, u32 en);

#endif
