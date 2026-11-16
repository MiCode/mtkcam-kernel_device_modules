/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 MediaTek Inc. */

#ifndef __MTK_CAM_SENINF_AOV_SENTEST_CTRL_H__
#define __MTK_CAM_SENINF_AOV_SENTEST_CTRL_H__

#include "mtk_cam-seninf.h"
#include "mtk_cam-seninf-tsrec.h"
#include "mtk_cam-seninf_control-8.h"
#include "mtk_cam-seninf-event-handle.h"

/******************************************************************************/
// seninf aov_sentest call back ctrl --- function
/******************************************************************************/

int aov_apmcu_streaming(struct seninf_ctx *ctx, struct mtk_seninf_aov_streaming_info *aov_streaming_info);

#endif
