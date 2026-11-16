// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam-seninf-aov-sentest-ctrl.h"
#include "mtk_cam-seninf_control-8.h"
#include "mtk_cam-seninf-hw.h"
#include "mtk_cam-seninf-route.h"
#include "mtk_cam-seninf-if.h"
#include "imgsensor-user.h"
#include <linux/delay.h>

/******************************************************************************/
// seninf sentest call back ctrl --- function
/******************************************************************************/

int aov_apmcu_streaming(struct seninf_ctx *ctx, struct mtk_seninf_aov_streaming_info *aov_streaming_info)
{
	int aov_apmcu_streaming_ret = -1;
	int sensor_idx, enable, aov_mclk_flag, aov_pm_runtime_flag, sensor_real_stream_on;
	struct mtk_seninf_aov_param aov_sentest_seninf_aov_param;
	unsigned int frame_cnt1 = 0, frame_cnt2 = 0;
	int delay_ms = 200;

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(ctx->sensor_sd == NULL)) {
		pr_info("[%s][ERROR] ctx->sensor_sd is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(aov_streaming_info == NULL)) {
		pr_info("[%s][ERROR] aov_streaming_info is NULL\n", __func__);
		return -EINVAL;
	}

	sensor_idx = aov_streaming_info->sensor_idx;
	enable = aov_streaming_info->enable;
	aov_mclk_flag = aov_streaming_info->aov_mclk_flag;
	aov_pm_runtime_flag = aov_streaming_info->aov_pm_runtime_flag;
	sensor_real_stream_on = aov_streaming_info->sensor_on;

	pr_info("[%s]aov_sentest get sensor_idx(%d) enable(%d) from user\n",
		__func__, sensor_idx, enable);
	pr_info("[%s]aov_sentest get aov_mclk_flag(%d) aov_pm_runtime_flag(%d) from user\n",
		__func__, aov_mclk_flag, aov_pm_runtime_flag);

	memset(&aov_sentest_seninf_aov_param, 0, sizeof(struct mtk_seninf_aov_param));

	if (enable) {
		if (sensor_real_stream_on) {
			/* streamon sensor to get frame cnt */
			pr_info("[%s]aov_sentest streaming on enable ++(%d)\n",
				__func__, enable);
			v4l2_subdev_call(ctx->sensor_sd, video, s_stream, enable);
		} else {
			pr_info("[%s]aov_sentest skip streaming on sensor\n", __func__);
		}

		mdelay(delay_ms);
		pr_info("[%s] delay_ms(%d)\n", __func__, delay_ms);
		mtk_cam_sensor_get_frame_cnt(ctx, &frame_cnt1);
		pr_info("[%s] get enable(%d) frame_cnt1(%d)\n",
			__func__, enable, frame_cnt1);
		mdelay(delay_ms);
		pr_info("[%s] delay_ms(%d)\n", __func__, delay_ms);
		mtk_cam_sensor_get_frame_cnt(ctx, &frame_cnt2);
		pr_info("[%s] get enable(%d) frame_cnt2(%d)\n",
			__func__, enable, frame_cnt2);

		/* mtk_cam_seninf_s_aov_param */
		pr_info("[%s]aov_sentest get sensor_idx(%d) from aov_param before\n",
			__func__, aov_sentest_seninf_aov_param.sensor_idx);
		aov_apmcu_streaming_ret = mtk_cam_seninf_s_aov_param(sensor_idx,
			&aov_sentest_seninf_aov_param, INIT_NORMAL);
		pr_info("[%s]aov_sentest get sensor_idx(%d) from aov_param after\n",
			__func__, aov_sentest_seninf_aov_param.sensor_idx);
		pr_info("[%s]aov_sentest get width(%lld) from aov_param after\n",
			__func__, aov_sentest_seninf_aov_param.width);
		pr_info("[%s]aov_sentest get height(%lld) from aov_param after\n",
			__func__, aov_sentest_seninf_aov_param.height);

		aov_streaming_info->aov_param = aov_sentest_seninf_aov_param;

		/* mtk_cam_seninf_aov_runtime_suspend */
		if (aov_pm_runtime_flag) {
			aov_apmcu_streaming_ret = mtk_cam_seninf_aov_runtime_suspend(
				sensor_idx);
		}
		/* V4L2_CID_MTK_SENSOR_SET_AOV_MCLK */
		if (aov_mclk_flag) {
			aov_apmcu_streaming_ret = mtk_cam_seninf_aov_sensor_set_mclk(
				sensor_idx, true);
		}
	} else {

		aov_streaming_info->aov_param.sensor_idx = -1;

		if (aov_pm_runtime_flag) {
			aov_apmcu_streaming_ret = mtk_cam_seninf_aov_runtime_resume(
				sensor_idx, DEINIT_NORMAL);
		}

		mtk_cam_sensor_get_frame_cnt(ctx, &frame_cnt1);
		pr_info("[%s] get enable(%d) frame_cnt1(%d)\n",
			__func__, enable, frame_cnt1);
		mdelay(delay_ms);
		pr_info("[%s] delay_ms(%d)\n", __func__, delay_ms);
		mtk_cam_sensor_get_frame_cnt(ctx, &frame_cnt2);
		pr_info("[%s] get enable(%d) frame_cnt2(%d)\n",
			__func__, enable, frame_cnt2);

		if (sensor_real_stream_on) {
			/* streamoff sensor */
			pr_info("[%s]aov_sentest streaming off enable ++(%d)\n",
				__func__, enable);
			v4l2_subdev_call(ctx->sensor_sd, video, s_stream, enable);
		} else {
			pr_info("[%s]aov_sentest skip streaming off sensor\n", __func__);
		}
	}

	/* MW API to apmcu */
	aov_apmcu_streaming_ret = 0;

	return aov_apmcu_streaming_ret;
}
