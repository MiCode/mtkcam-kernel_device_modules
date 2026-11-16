// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam-seninf-aov-sentest-ioctrl.h"
#include "mtk_cam-seninf-aov-sentest-ctrl.h"
#include "mtk_cam-seninf_control-8s.h"
#include "mtk_cam-seninf-hw.h"
#include "imgsensor-user.h"

/******************************************************************************/
// seninf aov_sentest call back ioctrl --- function
/******************************************************************************/

struct seninf_aov_sentest_ioctl {
	enum seninf_aov_sentest_ctrl_id ctrl_id;
	int (*func)(struct seninf_ctx *ctx, void *arg);
};

static int s_sentest_aov_apmcu_ut(struct seninf_ctx *ctx, void *arg)
{
	int ret = -1;
	struct mtk_seninf_aov_streaming_info *aov_streaming_info =
		kmalloc(sizeof(struct mtk_seninf_aov_streaming_info), GFP_KERNEL);

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(arg == NULL)) {
		pr_info("[%s][ERROR] arg is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(aov_streaming_info == NULL)) {
		pr_info("[%s][ERROR] aov_streaming_info is NULL\n", __func__);
		return -EINVAL;
	}

	if (copy_from_user(aov_streaming_info, arg,
		sizeof(struct mtk_seninf_aov_streaming_info))) {
		pr_info("[%s][ERROR] copy_from_user return failed\n", __func__);
		kfree(aov_streaming_info);
		return -EFAULT;
	}

	ret = aov_apmcu_streaming(ctx, aov_streaming_info);

	if (copy_to_user(arg, aov_streaming_info,
		sizeof(struct mtk_seninf_aov_streaming_info))) {
		pr_info("[%s][ERROR] copy_to_user return failed\n", __func__);
		return -EFAULT;
	}

	kfree(aov_streaming_info);

	return ret;
}

static int s_sentest_aov_scp_ut(struct seninf_ctx *ctx, void *arg)
{
	int scp_ut_ret = -1;

	/* MW API to scp */
	scp_ut_ret = 0;

	return scp_ut_ret;
}

static int s_sentest_aov_apmcu_scp_ut(struct seninf_ctx *ctx, void *arg)
{
	int apmcu_scp_ut_ret = -1;

	/* MW API to scp */
	apmcu_scp_ut_ret = 0;

	return apmcu_scp_ut_ret;
}

// static void __iomem *reg_mclk_meter;

static int g_sentest_aov_apmcu_status(struct seninf_ctx *ctx, void *arg)
{
	struct seninf_core *core = NULL;
	unsigned int frame_cnt = 0;
	int aov_csi_port = -1;
	struct mtk_seninf_aov_status_check *aov_status_check =
		kmalloc(sizeof(struct mtk_seninf_aov_status_check), GFP_KERNEL);
	int mclk_reg_val = 0;

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(arg == NULL)) {
		pr_info("[%s][ERROR] arg is NULL\n", __func__);
		return -EINVAL;
	}

	core = ctx->core;
	aov_csi_port = ctx->portNum;

	if (unlikely(core == NULL)) {
		pr_info("[%s][ERROR] core is NULL\n", __func__);
		return -EINVAL;
	}

	if (aov_csi_port == -1) {
		pr_info("[%s][ERROR] aov_csi_port is -1\n", __func__);
		return -EINVAL;
	}

	if (copy_from_user(aov_status_check, arg,
		sizeof(struct mtk_seninf_aov_status_check))) {
		pr_info("[%s][ERROR] copy_from_user return failed\n", __func__);
		kfree(aov_status_check);
		return -EFAULT;
	}

	aov_status_check->sensor_idx = -1;
	aov_status_check->aov_csi_port = -1;
	aov_status_check->frame_cnt = -1;
	aov_status_check->mclk_reg_val = -1;
	aov_status_check->height = -1;
	aov_status_check->width = -1;

	/* get Frame_cnt */
	mtk_cam_sensor_get_frame_cnt(ctx, &frame_cnt);

	// /* get mclk meter */
	// reg_mclk_meter = ioremap(0x1C016000, 0x1000);
	// mclk_reg_val = SENINF_READ_REG(reg_mclk_meter, 0x0050);
	// pr_info("[%s]get mclk meter(0x%x)\n", __func__, mclk_reg_val);

	/* get pm_runtime cnt */
	pr_info("[%s]get seninf_core pwr_refcnt_for_aov(%d), refcnt(%d)\n",
		__func__, core->pwr_refcnt_for_aov, core->refcnt);
	//call aov pm suspend

	/* aov_param check */
	pr_info("[%s]aov_param check\n", __func__);
	pr_info("[%s]aov_param check aov_csi_port(%d)\n", __func__, aov_csi_port);

	if (aov_csi_port < 0 || aov_csi_port >= AOV_SENINF_NUM) {
		pr_info("[%s]Error: aov_csi_port(%d) out of bound\n", __func__, aov_csi_port);
		return -EFAULT;
	}

	pr_info("[%s]aov_param check sensor_idx(%d)\n", __func__,
		g_aov_ctrl[aov_csi_port].aov_param.sensor_idx);
	pr_info("[%s]aov_param check height(%lld)\n", __func__,
		g_aov_ctrl[aov_csi_port].aov_param.height);
	pr_info("[%s]aov_param check width(%lld)\n", __func__,
		g_aov_ctrl[aov_csi_port].aov_param.width);
	pr_info("[%s]aov_param check camtg(%d)\n", __func__,
		g_aov_ctrl[aov_csi_port].aov_param.camtg);

	aov_status_check->sensor_idx = g_aov_ctrl[aov_csi_port].aov_param.sensor_idx;
	aov_status_check->aov_csi_port = aov_csi_port;
	aov_status_check->frame_cnt = (int)frame_cnt;
	aov_status_check->mclk_reg_val = mclk_reg_val;
	aov_status_check->height = g_aov_ctrl[aov_csi_port].aov_param.height;
	aov_status_check->width = g_aov_ctrl[aov_csi_port].aov_param.width;

	if (g_aov_ctrl[aov_csi_port].aov_ctx) {
		aov_status_check->ctx_is_null = 0;
		pr_info("[%s]aov_param check aov_ctx != NULL\n", __func__);
	} else {
		aov_status_check->ctx_is_null = 1;
		pr_info("[%s]aov_param check aov_ctx is NULL\n", __func__);
	}

	if (copy_to_user(arg, aov_status_check, sizeof(struct mtk_seninf_aov_status_check))) {
		pr_info("[%s][ERROR] copy_to_user return failed\n", __func__);
		return -EFAULT;
	}

	return 0;
}

static int g_sentest_aov_scp_status(struct seninf_ctx *ctx, void *arg)
{
	int scp_ut_ret = -1;

	/* MW API to scp */
	scp_ut_ret = 0;

	return scp_ut_ret;
	// return 0;
}

static int g_sentest_aov_apmcu_scp_status(struct seninf_ctx *ctx, void *arg)
{
	int scp_ut_ret = -1;

	/* MW API to scp */
	scp_ut_ret = 0;

	return scp_ut_ret;
	// return 0;
}

static const struct seninf_aov_sentest_ioctl
	aov_sentest_ioctl_table[SENINF_SENTEST_S_CTRL_ID_MAX] = {
	{SENINF_AOV_SENTEST_S_APMCU_UT, s_sentest_aov_apmcu_ut},
	{SENINF_AOV_SENTEST_S_SCP_UT, s_sentest_aov_scp_ut},
	{SENINF_AOV_SENTEST_S_APMCU_SCP_UT, s_sentest_aov_apmcu_scp_ut},

	{SENINF_AOV_SENTEST_G_APMCU_UT_RESULT, g_sentest_aov_apmcu_status},
	{SENINF_AOV_SENTEST_G_SCP_UT_RESULT, g_sentest_aov_scp_status},
	{SENINF_AOV_SENTEST_G_APMCU_SCP_UT_RESULT, g_sentest_aov_apmcu_scp_status},
};

int seninf_aov_sentest_ioctl_entry(struct seninf_ctx *ctx, void *arg)
{
	int i;
	struct mtk_seninf_aov_sentest_ctrl *ctrl_info = (struct mtk_seninf_aov_sentest_ctrl *)arg;

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(ctrl_info == NULL)) {
		pr_info("[%s][ERROR] ctrl_info is NULL\n", __func__);
		return -EINVAL;
	}

	for (i = SENINF_AOV_SENTEST_G_CTRL_ID_MIN; i < SENINF_AOV_SENTEST_S_CTRL_ID_MAX; i ++) {
		if (ctrl_info->ctrl_id == aov_sentest_ioctl_table[i].ctrl_id)
			return aov_sentest_ioctl_table[i].func(ctx, ctrl_info->param_ptr);
	}

	pr_info("[ERROR][%s] ctrl_id %d not found\n", __func__, ctrl_info->ctrl_id);
	return -EINVAL;
}
