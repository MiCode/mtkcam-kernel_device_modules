// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam-seninf-sentest-ioctrl.h"
#include "mtk_cam-seninf-sentest-ctrl.h"
#include "mtk_cam-seninf_control-8.h"
#include "mtk_cam-seninf-hw.h"

/******************************************************************************/
// seninf sentest call back ioctrl --- function
/******************************************************************************/

struct seninf_sentest_ioctl {
	enum seninf_sentest_ctrl_id ctrl_id;
	int (*func)(struct seninf_ctx *ctx, void *arg);
};

int seninf_sentest_flag_init(struct seninf_ctx *ctx)
{
	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	ctx->allow_adjust_isp_en = false;
	ctx->single_raw_streaming_en = false;
	return 0;
}

static int s_sentest_max_isp_clk_en(struct seninf_ctx *ctx, void *arg)
{
	int *en = kmalloc(sizeof(int), GFP_KERNEL);

	if (unlikely(en == NULL)) {
		pr_info("[%s][ERROR] en is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		kfree(en);
		return -EINVAL;
	}

	if (ctx->streaming){
		dev_info(ctx->dev,
				"[ERROR][%s] set max_clk_en failed, due to streaming is %d\n",
				__func__, ctx->streaming);
		kfree(en);
		return -EINVAL;
	}

	if (copy_from_user(en, arg, sizeof(int))) {
		pr_info("[%s][ERROR] copy_from_user return failed\n", __func__);
		kfree(en);
		return -EFAULT;
	}

	ctx->allow_adjust_isp_en = *en;

	dev_info(ctx->dev, "[%s] en: %d, allow_adjust_isp_en is %d\n",
				__func__, *en, ctx->allow_adjust_isp_en);

	kfree(en);
	return 0;
}

static int s_sentest_single_raw_streaming_en(struct seninf_ctx *ctx, void *arg)
{
	int *en = kmalloc(sizeof(int), GFP_KERNEL);

	if (unlikely(en == NULL)) {
		pr_info("[%s][ERROR] en is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		kfree(en);
		return -EINVAL;
	}

	if (ctx->streaming){
		dev_info(ctx->dev,
				"[ERROR][%s] set max_clk_en failed, due to streaming is %d\n",
				__func__, ctx->streaming);
		kfree(en);
		return -EINVAL;
	}

	if (copy_from_user(en, arg, sizeof(int))) {
		pr_info("[%s][ERROR] copy_from_user return failed\n", __func__);
		kfree(en);
		return -EFAULT;
	}

	ctx->single_raw_streaming_en = *en;

	dev_info(ctx->dev, "[%s] en: %d, single_raw_streaming_en is %d\n",
				__func__, *en, ctx->single_raw_streaming_en);

	kfree(en);
	return 0;
}

static inline int g_sentest_debug_result(struct seninf_ctx *ctx, void *arg)
{

	return seninf_sentest_get_debug_reg_result(ctx, arg);
}

static int g_sentest_mipi_result(struct seninf_ctx *ctx, void *arg)
{

	/* Need add get mipi result from  phy*/

	return -EINVAL;
}

static const struct seninf_sentest_ioctl sentest_ioctl_table[] = {
	{SENINF_SENTEST_G_DEBUG_RESULT, g_sentest_debug_result},
	{SENINF_SENTEST_S_MAX_ISP_EN, s_sentest_max_isp_clk_en},
	{SENINF_SENTEST_S_SINGLE_STREAM_RAW, s_sentest_single_raw_streaming_en},
	{SENINF_SENTEST_G_MIPI_RESULT, g_sentest_mipi_result},
};

int seninf_sentest_ioctl_entry(struct seninf_ctx *ctx, void *arg)
{
	int i;
	struct mtk_seninf_sentest_ctrl *ctrl_info = (struct mtk_seninf_sentest_ctrl *)arg;

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(ctrl_info == NULL)) {
		pr_info("[%s][ERROR] ctrl_info is NULL\n", __func__);
		return -EINVAL;
	}

	for (i = SENINF_SENTEST_G_CTRL_ID_MIN; i < SENINF_SENTEST_S_CTRL_ID_MAX; i ++) {
		if (ctrl_info->ctrl_id == sentest_ioctl_table[i].ctrl_id)
			return sentest_ioctl_table[i].func(ctx, ctrl_info->param_ptr);
	}

	pr_info("[ERROR][%s] ctrl_id %d not found\n", __func__, ctrl_info->ctrl_id);
	return -EINVAL;
}
