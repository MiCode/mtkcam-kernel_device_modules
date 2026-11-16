// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam-seninf-sentest-ctrl.h"
#include "mtk_cam-seninf_control-8s.h"
#include "mtk_cam-seninf-hw.h"
#include "mtk_cam-seninf-route.h"
#include "mtk_cam-seninf-if.h"
#include "imgsensor-user.h"
#include <linux/version.h>

/******************************************************************************/
// seninf sentest call back ctrl --- function
/******************************************************************************/

#define WATCHDOG_INTERVAL_MS 50
#define FPS30_FRAME_DURATION_IN_MS 33

struct seninf_sentest_work {
	struct kthread_work work;
	struct seninf_ctx *ctx;
	union sentest_work_data {
		unsigned int seamless_scenario;
		void *data_ptr;
	} data;
};

enum SENTEST_TARGET_VSYNC {
	SENTEST_FIRST_VSYNC,
	SENTEST_LAST_VSYNC,
};

int seninf_sentest_probe_init(struct seninf_ctx *ctx)
{
	int ret = 0;

	kthread_init_worker(&ctx->sentest_worker);
	ctx->sentest_kworker_task = kthread_run(kthread_worker_fn,
			&ctx->sentest_worker, "sentest_worker");

	if (IS_ERR(ctx->sentest_kworker_task)) {
		pr_info("[%s][ERROR]: failed to start sentest kthread worker\n", __func__);
		ctx->sentest_kworker_task = NULL;
		return -EFAULT;
	}

	sched_set_fifo(ctx->sentest_kworker_task);

	ret |= seninf_sentest_flag_init(ctx);
	return ret;
}

int seninf_sentest_uninit(struct seninf_ctx *ctx)
{
	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	if (ctx->sentest_kworker_task)
		kthread_stop(ctx->sentest_kworker_task);

	return 0;
}

static void seninf_sentest_seamless_switch_error_handler(struct seninf_ctx *ctx)
{
	if (unlikely(ctx == NULL)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return;
	}

	seninf_sentest_watchingdog_en(&ctx->sentest_watchdog, false);
	ctx->sentest_seamless_ut_status = SENTEST_SEAMLESS_IS_ERR;
	ctx->sentest_seamless_ut_en = false;
}

static void seninf_sentest_reset_seamless_flag(struct seninf_ctx *ctx)
{
	ctx->sentest_seamless_ut_en = false;
	ctx->sentest_seamless_ut_status = SENTEST_SEAMLESS_IS_IDLE;
	ctx->sentest_seamless_irq_ref = 0;
	ctx->sentest_seamless_is_set_camtg_done = 0;
	ctx->sentest_irq_counter = 0;

	memset(&ctx->sentest_seamless_cfg, 0, sizeof(struct mtk_seamless_switch_param));
}

int seninf_sentest_flag_init(struct seninf_ctx *ctx)
{
	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	seninf_sentest_reset_seamless_flag(ctx);
	ctx->sentest_adjust_isp_en = false;
	ctx->sentest_mipi_measure_en = false;
	ctx->sentest_force_tsrec_vc_dt_en = false;
	ctx->sentest_active_frame_en = false;
	ctx->sentest_tsrec_update_sof_cnt_en = false;
	ctx->sentest_active_frame_irq_counter = 0;
	ctx->sentest_active_frame_measure_result = 0;

	return 0;
}


int seninf_sentest_get_debug_reg_result(struct seninf_ctx *ctx, void *arg)
{
	int i;
	struct seninf_core *core;
	struct seninf_ctx *ctx_;
	struct mtk_cam_seninf_vcinfo_debug *vcinfo_debug;
	struct outmux_debug_result *outmux_result;
	struct mtk_cam_seninf_debug debug_result;
	static __u16 last_pkCnt;
	struct mtk_seninf_debug_result *result =
			kmalloc(sizeof(struct mtk_seninf_debug_result), GFP_KERNEL);

	if (unlikely(result == NULL)) {
		pr_info("[%s][ERROR] result is NULL\n", __func__);
		return -EFAULT;
	}

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		goto SENTEST_GET_DBG_ERR_EXIT;
	}

	core = ctx->core;
	if (unlikely(core == NULL)) {
		pr_info("[%s][ERROR] core is NULL\n", __func__);
		goto SENTEST_GET_DBG_ERR_EXIT;
	}

	if (copy_from_user(result, arg, sizeof(struct mtk_seninf_debug_result))) {
		pr_info("[%s][ERROR] copy_from_user return failed\n", __func__);
		goto SENTEST_GET_DBG_ERR_EXIT;
	}

	memset(&debug_result, 0, sizeof(struct mtk_cam_seninf_debug));

	list_for_each_entry(ctx_, &core->list, list) {
		if (unlikely(ctx_ == NULL)) {
			pr_info("[%s][ERROR] ctx_ is NULL\n", __func__);
			goto SENTEST_GET_DBG_ERR_EXIT;
		}

		if (!ctx_->streaming)
			continue;

		g_seninf_ops->get_seninf_debug_core_dump(ctx_, &debug_result);

		result->is_cphy = ctx_->is_cphy;
		result->csi_port = ctx_->port;
		result->seninfAsyncIdx = ctx_->seninfAsyncIdx;
		result->data_lanes = ctx_->num_data_lanes;
		result->valid_result_cnt = debug_result.valid_result_cnt;
		result->seninf_async_irq = debug_result.seninf_async_irq;
		result->csi_mac_irq_status = debug_result.csi_mac_irq_status;

		if (last_pkCnt == debug_result.packet_cnt_status) {

			/* Need fix this logic */
			last_pkCnt = debug_result.packet_cnt_status;
			result->packet_status_err = 0;
		} else {
			result->packet_status_err = 1;
		}

		for (i = 0; i <= debug_result.valid_result_cnt; i++) {
			vcinfo_debug = &debug_result.vcinfo_debug[i];
			outmux_result = &result->outmux_result[i];

			if (unlikely(vcinfo_debug == NULL)) {
				pr_info("[%s][ERROR] vcinfo_debug is NULL\n", __func__);
				goto SENTEST_GET_DBG_ERR_EXIT;
			}

			if (unlikely(outmux_result == NULL)) {
				pr_info("[%s][ERROR] outmux_result is NULL\n", __func__);
				goto SENTEST_GET_DBG_ERR_EXIT;
			}

			outmux_result->vc_feature	= vcinfo_debug->vc_feature;
			outmux_result->tag_id	= vcinfo_debug->tag_id;
			outmux_result->vc		= vcinfo_debug->vc;
			outmux_result->dt		= vcinfo_debug->dt;
			outmux_result->exp_size_h	= vcinfo_debug->exp_size_h;
			outmux_result->exp_size_v	= vcinfo_debug->exp_size_v;
			outmux_result->rec_size_h	= vcinfo_debug->rec_size_h;
			outmux_result->rec_size_v	= vcinfo_debug->rec_size_v;
			outmux_result->outmux_id	= vcinfo_debug->outmux_id;

			outmux_result->done_irq_status		= vcinfo_debug->done_irq_status;
			outmux_result->oversize_irq_status	= vcinfo_debug->oversize_irq_status;
			outmux_result->incomplete_frame_status	= vcinfo_debug->incomplete_frame_status;
			outmux_result->ref_vsync_irq_status	= vcinfo_debug->ref_vsync_irq_status;
		}

		if (copy_to_user(arg, result, sizeof(struct mtk_seninf_debug_result))) {
			pr_info("[%s][ERROR] copy_to_user return failed\n", __func__);
			goto SENTEST_GET_DBG_ERR_EXIT;
		}
	}
	kfree(result);
	return 0;

SENTEST_GET_DBG_ERR_EXIT:
	kfree(result);
	return -EFAULT;
}

static void seninf_sentest_watchdog_timer_callback(struct timer_list *t)
{
	struct mtk_cam_sentest_watchdog *wd = from_timer(wd, t, timer);
	struct seninf_ctx *ctx =
		container_of(wd, struct seninf_ctx, sentest_watchdog);

	if (unlikely(ctx == NULL)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return;
	}

	pr_info("[%s] watchog timeout tirggered", __func__);

	ctx->sentest_seamless_ut_status = SENTEST_SEAMLESS_IS_TIMEOUT;
	ctx->sentest_seamless_ut_en = false;

#if (KERNEL_VERSION(6, 15, 0) > LINUX_VERSION_CODE)
	del_timer(&wd->timer);
#else
	timer_delete(&wd->timer);
#endif

	g_seninf_ops->_debug_current_status(ctx);
}

int seninf_sentest_watchingdog_en(struct mtk_cam_sentest_watchdog *wd, bool en)
{
	struct seninf_ctx *ctx =
		container_of(wd, struct seninf_ctx, sentest_watchdog);
	u64 shutter_for_timeout = 0;

	if (unlikely(wd == NULL)) {
		pr_info("[Error][%s] wd is NULL", __func__);
		return -EFAULT;
	}

	if (unlikely(ctx == NULL)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return -EFAULT;
	}

	if (en) {

		if (ctx->sentest_seamless_cfg.ae_ctrl[0].exposure.arr[0]) {
			shutter_for_timeout =
				ctx->sentest_seamless_cfg.ae_ctrl[0].exposure.arr[0] > WATCHDOG_INTERVAL_MS ?
				ctx->sentest_seamless_cfg.ae_ctrl[0].exposure.arr[0] :
				WATCHDOG_INTERVAL_MS;
		}

		if (shutter_for_timeout < FPS30_FRAME_DURATION_IN_MS)
			shutter_for_timeout = FPS30_FRAME_DURATION_IN_MS;

		seninf_sentest_watchdog_init(wd);
		// setup timer
		wd->timer.expires = jiffies + msecs_to_jiffies(shutter_for_timeout);
		add_timer(&wd->timer);

	} else {
		// del timer
#if (KERNEL_VERSION(6, 15, 0) > LINUX_VERSION_CODE)
		del_timer_sync(&wd->timer);
#else
		timer_delete_sync(&wd->timer);
#endif

		ctx->sentest_seamless_ut_status = SENTEST_SEAMLESS_IS_IDLE;
	}

	pr_info("[%s] setup sentest swatchdong timeout %llu en: %d done",
			__func__, shutter_for_timeout, en);

	return 0;
}

int seninf_sentest_watchdog_init(struct mtk_cam_sentest_watchdog *wd)
{
	if (unlikely(wd == NULL)) {
		pr_info("[Error][%s] wd is NULL", __func__);
		return -EFAULT;
	}

	timer_setup(&wd->timer, seninf_sentest_watchdog_timer_callback, 0);
	return 0;
}

void seninf_sentest_seamless_ut_disable_outmux(struct seninf_ctx *ctx)
{
	mtk_cam_seninf_release_outmux(ctx);

}



static u32 compose_format_code_by_scenario(u32 target_scenario)
{
	return (target_scenario << 16) & 0xFF0000;
}

static int get_lastest_outmux_id_by_vc_cnt(struct seninf_ctx *ctx, u32 target_count, u32 *outmux)
{
	struct seninf_core *core = ctx->core;
	struct seninf_outmux *ent;
	int count = 0;

	list_for_each_entry(ent, &core->list_outmux, list) {
		if (count == target_count) {
			*outmux = ent->idx;
			return 0;
		}
		count++;
	}

	return -EINVAL;
}

static int seninf_sentest_set_fmt(struct seninf_ctx *ctx)
{
	int i;
	u32 code;
	struct seninf_vcinfo *cur_vcinfo = &ctx->cur_vcinfo;

	if (ctx == NULL) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return -EFAULT;
	}

	mtk_cam_seninf_get_sensor_usage(&ctx->subdev);
	code = compose_format_code_by_scenario(ctx->sentest_seamless_cfg.target_scenario_id);
	ctx->fmt[PAD_SRC_RAW0].format.code = code;
	mtk_cam_sensor_get_vc_info_by_scenario(ctx, code);

	for (i = 0; i < cur_vcinfo->cnt; i++) {
		if (cur_vcinfo->vc[i].out_pad != PAD_SRC_RAW0)
			continue;

		switch (cur_vcinfo->vc[i].dt) {
		case 0x2c:
			ctx->fmt[PAD_SRC_RAW0].format.code |= MEDIA_BUS_FMT_SBGGR12_1X12;
			break;
		case 0x2d:
			ctx->fmt[PAD_SRC_RAW0].format.code |= MEDIA_BUS_FMT_SBGGR14_1X14;
			break;
		default:
			ctx->fmt[PAD_SRC_RAW0].format.code |= MEDIA_BUS_FMT_SBGGR10_1X10;
			break;
		}
	}
	return 0;
}

static bool check_if_outmux_setting_repeat(struct mtk_cam_seninf_mux_setting *setting_list,
	u32 valid_cnt, struct mtk_cam_seninf_mux_setting *coming_setting)
{
	int i;
	bool is_repeat = false;

	for (i = 0; i < valid_cnt; i++) {
		if (coming_setting->camtg == setting_list[i].camtg &&
		coming_setting->enable == setting_list[i].enable &&
		coming_setting->source == setting_list[i].source) {
			is_repeat = true;
			break;
		}
	}

	return is_repeat;
}

static int seninf_sentest_set_camtg_for_seamless(struct seninf_ctx *ctx)
{
	int i, out_pad, ret = 0;
	int outmux_id = 0;
	struct seninf_vcinfo *cur_vcinfo = &ctx->cur_vcinfo;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct mtk_cam_seninf_mux_param param;
	struct mtk_cam_seninf_rdy_mask_en rdy_mask_en;
	struct mtk_cam_seninf_mux_setting setting_;
	struct mtk_cam_seninf_mux_setting settings[12];
	struct v4l2_ctrl *ctrl;

	memset(&param, 0, sizeof(struct mtk_cam_seninf_mux_param));

	memset(settings, 0, sizeof(struct mtk_cam_seninf_mux_setting) * ARRAY_SIZE(settings));


	/* prepared outmux which to be disable */
	for (i = 0; i < vcinfo->cnt; i++) {
		out_pad = vcinfo->vc[i].out_pad;

		memset(&setting_, 0, sizeof(struct mtk_cam_seninf_mux_setting));

		setting_.seninf = &ctx->subdev;
		setting_.source = out_pad;
		setting_.camtg = ctx->pad2cam[out_pad][0];
		setting_.enable = false;
		setting_.tag_id = 0;

		if (setting_.camtg == 255)
			continue;

		if (check_if_outmux_setting_repeat(settings, param.num, &setting_))
			continue;

		settings[param.num].seninf = setting_.seninf;
		settings[param.num].source = setting_.source;
		settings[param.num].camtg = setting_.camtg;
		settings[param.num].enable = setting_.enable;
		settings[param.num].tag_id = setting_.tag_id;
		pr_info("[%s][disabe list] pad %d, camtg %d, en %d tag %d num %d",
				__func__,
				settings[param.num].source,
				settings[param.num].camtg,
				settings[param.num].enable,
				settings[param.num].tag_id,
				param.num);
		param.num++;
	}

	if (seninf_sentest_set_fmt(ctx)) {
		pr_info("[Error][%s] seninf_sentest_set_fmt return failed", __func__);
		return -EFAULT;
	}

	ctrl = v4l2_ctrl_find(ctx->sensor_sd->ctrl_handler,
			V4L2_CID_START_SEAMLESS_SWITCH);

	if (!ctrl) {
		pr_info("[%s][ERROR], no V4L2_CID_START_SEAMLESS_SWITCH cid found in %s\n",
			__func__,
			ctx->sensor_sd->name);
		return -EFAULT;
	}

	v4l2_ctrl_s_ctrl_compound(ctrl, V4L2_CTRL_TYPE_U32, &ctx->sentest_seamless_cfg);
	seninf_sentest_watchingdog_en(&ctx->sentest_watchdog, true);

	/* prepared outmux which to be enable */
	for (i = 0; i < cur_vcinfo->cnt; i++) {

		if (get_lastest_outmux_id_by_vc_cnt(ctx, i, &outmux_id)) {
			pr_info("[Error][%s] get_lastest_outmux_id_by_vc_cnt return failed", __func__);
			return -EFAULT;
		}

		memset(&setting_, 0, sizeof(struct mtk_cam_seninf_mux_setting));

		setting_.seninf = &ctx->subdev;
		setting_.source = cur_vcinfo->vc[i].out_pad;
		setting_.camtg = i;
		setting_.enable = true;
		setting_.tag_id = 0;

		if (check_if_outmux_setting_repeat(settings, param.num, &setting_))
			continue;

		settings[param.num].seninf = setting_.seninf;
		settings[param.num].source = setting_.source;
		settings[param.num].camtg = setting_.camtg;
		settings[param.num].enable = setting_.enable;
		settings[param.num].tag_id = setting_.tag_id;

		pr_info("[%s][enable list]pad %d, camtg %d, en %d tag %d num %d",
				__func__,
				settings[param.num].source,
				settings[param.num].camtg,
				settings[param.num].enable,
				settings[param.num].tag_id,
				param.num);

		param.num++;


	}

	rdy_mask_en.rdy_sw_en = true;
	rdy_mask_en.rdy_grp_en = true;
	rdy_mask_en.rdy_cq_en = false;


	param.rdy_mask_en = rdy_mask_en;
	param.settings = settings;
	ret |= mtk_cam_seninf_mux_setup(&ctx->subdev, &param);
	ret |= mtk_cam_seninf_set_mux_sw_rdy(&ctx->subdev, settings[0].camtg, true, false);

	return ret;
}

static int seninf_sentest_ops_before_sensor_seamless(struct seninf_ctx *ctx)
{

	if (unlikely(ctx == NULL)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return -EFAULT;
	}

	if (seninf_sentest_set_camtg_for_seamless(ctx)) {
		pr_info("[Error][%s] seninf_sentest_set_camtg_for_seamless returned false",
				__func__);
		seninf_sentest_seamless_switch_error_handler(ctx);
		return -EFAULT;
	}

	return 0;
}

static int seninf_sentest_seamless_ut_start(struct seninf_ctx *ctx)
{
	int ret;
	struct seninf_sentest_work *sentest_work = NULL;

	sentest_work = kmalloc(sizeof(struct seninf_sentest_work), GFP_ATOMIC);

	if (unlikely(ctx == NULL)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		goto SENTEST_SEAMLESS_UT_START_ERR_EXIT;
	}

	if (unlikely(sentest_work == NULL)) {
		pr_info("[Error][%s] sentest_work is NULL", __func__);
		goto SENTEST_SEAMLESS_UT_START_ERR_EXIT;
	}

	ret = seninf_sentest_ops_before_sensor_seamless(ctx);

	ctx->sentest_seamless_is_set_camtg_done = true;

	kfree(sentest_work);
	return ret;

SENTEST_SEAMLESS_UT_START_ERR_EXIT:
	kfree(sentest_work);
	return -EFAULT;
}

static int is_target_vsync(struct seninf_ctx *ctx,
	const struct mtk_cam_seninf_tsrec_irq_notify_info *p_info,
	enum SENTEST_TARGET_VSYNC vsync_type)
{
	bool ret = 0;
	int i = 0 , mask_shift_cnt = 0, mask = 0x01;
	struct seninf_vcinfo *vcinfo = &ctx->cur_vcinfo;
	struct seninf_vc *vc;

	if (unlikely(p_info == NULL)) {
		pr_info("[Error][%s] p_info is NULL", __func__);
		return 0;
	}

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];
		if ((vc->out_pad == PAD_SRC_RAW0) ||
			(vc->out_pad == PAD_SRC_RAW1) ||
			(vc->out_pad == PAD_SRC_RAW2) ||
			(vc->out_pad == PAD_SRC_RAW_W0) ||
			(vc->out_pad == PAD_SRC_RAW_W1) ||
			(vc->out_pad == PAD_SRC_RAW_W2))
			mask_shift_cnt++;
	}

	if (unlikely(mask_shift_cnt == 0)) {
		pr_err("[%s], mask_shift_cnt should not be 0\n", __func__);
		return -EFAULT;
	}

	if (vsync_type == SENTEST_FIRST_VSYNC) {
		ret = (p_info->vsync_status & 0x01)? true : false;
	} else {
		mask = (mask << (mask_shift_cnt - 1));
		ret = (p_info->vsync_status & mask)? true : false;
	}

	pr_info("[%s], vsync_status 0x%x, mask_shift_cnt %d is_last_vsync %d ret %d",
			__func__,
			p_info->vsync_status,
			mask_shift_cnt,
			vsync_type,
			ret);

	return ret;
}

static int seninf_sentest_update_sof_cnt(struct seninf_ctx *ctx)
{
	struct v4l2_ctrl *ctrl;
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;

	if (unlikely(ctx == NULL)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return -EFAULT;
	}

	/* notify sensor drv Vsync */
	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
				V4L2_CID_UPDATE_SOF_CNT);

	if (!ctrl) {
		pr_info("[%s][ERROR], no V4L2_CID_UPDATE_SOF_CNT %s\n",
			__func__,
			sensor_sd->name);
		return -EFAULT;
	}

	v4l2_ctrl_s_ctrl(ctrl, ctx->sentest_irq_counter);

	return 0;
}

static int seninf_sentest_vsync_notify(struct seninf_ctx *ctx)
{
	struct v4l2_ctrl *ctrl;
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct mtk_sof_info sof_info;

	if (unlikely(ctx == NULL)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return -EFAULT;
	}

	/* notify sensor drv Vsync */
	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
				V4L2_CID_VSYNC_NOTIFY);

	if (!ctrl) {
		pr_info("[%s][ERROR], no V4L2_CID_VSYNC_NOTIFY %s\n",
			__func__,
			sensor_sd->name);
		return -EFAULT;
	}

	sof_info.cnt = ctx->sentest_irq_counter;
	sof_info.ts = ktime_get_boottime_ns();

	v4l2_ctrl_s_ctrl_compound(ctrl, V4L2_CTRL_TYPE_U32, &sof_info);

	return 0;
}

static int seninf_sentest_notify_vsync_event_to_imgsensor(struct seninf_ctx *ctx)
{
	int ret = 0;

	ret |= seninf_sentest_update_sof_cnt(ctx);
	ret |= seninf_sentest_vsync_notify(ctx);

	return ret;
}

static int seninf_sentest_ops_after_sensor_seamless(struct seninf_ctx *ctx)
{
	if (unlikely(ctx == NULL)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return -EFAULT;
	}

	if (!ctx->sentest_seamless_is_set_camtg_done) {
		pr_info("[Error][%s] sentest_seamless_camtg hasn't been processed", __func__);
		seninf_sentest_seamless_switch_error_handler(ctx);
		return -EFAULT;
	}

	if (seninf_sentest_notify_vsync_event_to_imgsensor(ctx)) {
		pr_info("[Error][%s] seninf_sentest_notify_vsync_event_to_imgsensor reutrn failed", __func__);
		return -EFAULT;
	}

	seninf_sentest_watchingdog_en(&ctx->sentest_watchdog, false);
	ctx->sentest_seamless_ut_en = false;
	mtk_cam_seninf_force_disable_out_mux(&ctx->subdev);

	pr_info("[%s] -", __func__);
	return 0;
}

static bool set_sensor_extend_frame_ll(struct seninf_ctx *ctx)
{
	unsigned int action = IMGSENSOR_EXTEND_FRAME_LENGTH_TO_DOL;

	if (unlikely(ctx == NULL)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return -EFAULT;
	}

	ctx->sensor_sd->ops->core->command(ctx->sensor_sd,
						V4L2_CMD_SET_SENSOR_FL_PROLONG,
						&action);
	return 0;
}

int seninf_sentest_set_csi_chk_ctrl(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *cur_vcinfo = &ctx->cur_vcinfo;
	int i, max_cnt = 0;

	if (unlikely(ctx == NULL)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return -EFAULT;
	}

	memset(&ctx->sentest_mac_chk_result, 0, sizeof(struct mtk_cam_csi_checker));

	max_cnt = min(cur_vcinfo->cnt, MAX_CSI_CHECKER_NUM);

	for (i = 0; i < max_cnt; i++) {
		if ((cur_vcinfo->vc[i].out_pad < PAD_SRC_RAW0) ||
			(cur_vcinfo->vc[i].out_pad > PAD_SRC_RAW_W2))
			continue;

		g_seninf_ops->_set_mac_chk_ctrl(
				ctx, cur_vcinfo->vc[i].vc, cur_vcinfo->vc[i].dt, i);


		ctx->sentest_mac_chk_result.info[i].vc = cur_vcinfo->vc[i].vc;
		ctx->sentest_mac_chk_result.info[i].dt = cur_vcinfo->vc[i].dt;
		ctx->sentest_mac_chk_result.info[i].exp =
			(cur_vcinfo->vc[i].exp_vsize << 16) + cur_vcinfo->vc[i].exp_hsize;
		ctx->sentest_mac_chk_result.valid_measure_cnt++;

		pr_info("[%s] vc 0x%x dt 0x%x, exp_size: 0x%x, valid_cnt %d",
				__func__,
				ctx->sentest_mac_chk_result.info[i].vc,
				ctx->sentest_mac_chk_result.info[i].dt,
				ctx->sentest_mac_chk_result.info[i].exp,
				ctx->sentest_mac_chk_result.valid_measure_cnt);


	}

	return 0;
}

static int seninf_sentest_dump_last_frame_size(struct seninf_ctx *ctx)
{
	int i, max_cnt = 0;

	if (unlikely(ctx == NULL)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return -EFAULT;
	}

	max_cnt = ctx->sentest_mac_chk_result.valid_measure_cnt;

	g_seninf_ops->_get_mac_chk_result(ctx);

	for (i = 0; i < max_cnt; i++) {
		pr_info("[%s] exp_size: 0x%x, chk_rcv %d",
				__func__,
				ctx->sentest_mac_chk_result.info[i].exp,
				ctx->sentest_mac_chk_result.info[i].rcv);
	}

	return 0;
}

int notify_sentest_irq_for_seamless_switch(struct seninf_ctx *ctx,
					const struct mtk_cam_seninf_tsrec_irq_notify_info *p_info)
{
	struct mtk_seninf_sof_notify_param param;

	if (is_target_vsync(ctx, p_info , SENTEST_FIRST_VSYNC)) {
		ctx->sentest_irq_counter++;
		param.sd = &ctx->subdev;
		param.sof_cnt = ctx->sentest_irq_counter;
		param.sof_ts = ktime_get_boottime_ns();
		mtk_cam_seninf_sof_notify(&param);
	}

	pr_info("[%s] sentest_seamless_irq_ref %llu, sentest_irq_counter %llu\n",
			__func__,
			ctx->sentest_seamless_irq_ref,
			ctx->sentest_irq_counter);

	if ((ctx->sentest_seamless_irq_ref + 1) == ctx->sentest_irq_counter) {
		set_sensor_extend_frame_ll(ctx);

	} else if ((ctx->sentest_seamless_irq_ref + 2) == ctx->sentest_irq_counter) {
		pr_info("[%s] stay turn for N-1 frame duration ", __func__);

	} else if ((ctx->sentest_seamless_irq_ref + 3) == ctx->sentest_irq_counter) {

		if (is_target_vsync(ctx, p_info , SENTEST_FIRST_VSYNC))
			g_seninf_ops->_show_mac_chk_status(ctx, true);

		if (is_target_vsync(ctx, p_info , SENTEST_LAST_VSYNC))
			seninf_sentest_seamless_ut_start(ctx);

		pr_info("[%s] sentest seamless switch config done ", __func__);
	} else if ((ctx->sentest_seamless_irq_ref + 4) == ctx->sentest_irq_counter) {

		seninf_sentest_dump_last_frame_size(ctx);
		seninf_sentest_ops_after_sensor_seamless(ctx);
	}


	return 0;
}

static bool set_sensor_max_fps(struct seninf_ctx *ctx)
{
	struct mtk_fps_by_scenario info;

	if (unlikely(ctx == NULL)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return -EFAULT;
	}

	info.fps = ctx->sentest_avtive_frame_fps;
	ctx->sensor_sd->ops->core->command(ctx->sensor_sd,
						V4L2_CMD_SET_SENSOR_FRAME_LENGTH,
						&info);
	return 0;
}

int notify_sentest_irq_for_active_frame(struct seninf_ctx *ctx,
					const struct mtk_cam_seninf_tsrec_irq_notify_info *p_info)
{
	if (is_target_vsync(ctx, p_info , SENTEST_FIRST_VSYNC))
		ctx->sentest_active_frame_irq_counter++;

	pr_info("[%s] active_frame_irq_ref_counter %llu, active_frame_irq_counter %llu\n",
		__func__,
		ctx->sentest_active_frame_irq_ref_counter,
		ctx->sentest_active_frame_irq_counter);

	if ((ctx->sentest_active_frame_irq_ref_counter + 1) ==
		ctx->sentest_active_frame_irq_counter) {

		if (is_target_vsync(ctx, p_info , SENTEST_LAST_VSYNC))
			set_sensor_max_fps(ctx);
	}

	return 0;
}

int notify_sentest_update_imgsensor_sof_cnt(struct seninf_ctx *ctx,
					const struct mtk_cam_seninf_tsrec_irq_notify_info *p_info)
{
	if (!is_target_vsync(ctx, p_info , SENTEST_FIRST_VSYNC))
		return 0;

	ctx->sentest_irq_counter++;
	return seninf_sentest_notify_vsync_event_to_imgsensor(ctx);
}

int notify_sentest_irq(struct seninf_ctx *ctx,
					const struct mtk_cam_seninf_tsrec_irq_notify_info *p_info)
{
	if (unlikely(ctx == NULL)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return -EFAULT;
	}

	if (unlikely(p_info == NULL)) {
		pr_info("[Error][%s] p_info is NULL", __func__);
		return -EFAULT;
	}

	if (ctx->sentest_seamless_ut_en)
		return notify_sentest_irq_for_seamless_switch(ctx, p_info);

	if (ctx->sentest_active_frame_en)
		return notify_sentest_irq_for_active_frame(ctx, p_info);

	if (ctx->sentest_tsrec_update_sof_cnt_en)
		return notify_sentest_update_imgsensor_sof_cnt(ctx, p_info);

	return 0;
}

int seninf_sentest_get_csi_mipi_measure_result(struct seninf_ctx *ctx,
		struct mtk_cam_seninf_meter_info *info)
{
	struct v4l2_mbus_framefmt *format;
	int i, valid_measure_req = 0;

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(info == NULL)) {
		pr_info("[%s][ERROR] info is NULL\n", __func__);
		return -EINVAL;
	}

	format = &ctx->fmt[PAD_SRC_RAW0].format;

	/* check if measure size is invalid */
	for (i = 0; i < CSIMAC_MEASURE_MAX_NUM; i++) {
		if ((info->probes[i].measure_line >= (format->height - 1)) ||
			(info->probes[i].measure_line == 0))
			continue;

		valid_measure_req++;
	}

	info->valid_measure_line = valid_measure_req;
	if (g_seninf_ops->_get_csi_HV_HB_meter(ctx, info, valid_measure_req)) {
		pr_info("[%s][ERROR] _get_csi_HV_HB_meter return failed\n", __func__);
		return -EFAULT;
	}

	return 0;
}

int seninf_sentest_set_tsrec_manual_vc_config(struct seninf_ctx *ctx, struct seninf_vc *vc)
{
	int i = 0;

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(vc == NULL)) {
		pr_info("[%s][ERROR] vc is NULL\n", __func__);
		return -EINVAL;
	}

	if (ctx->sentest_force_tsrec_vc_dt_en == 0)
		return 0;

	pr_info("[%s] start overwrite tsrec vc setting\n", __func__);

	/* sentest only pass two cfg to seninf drv */
	for (i = 0; i < 2; i++) {
		if (ctx->sentest_vsync_order_info.cfg[i].vc != vc->vc)
			continue;

		if (ctx->sentest_vsync_order_info.cfg[i].dt != vc->dt)
			continue;

		mtk_cam_seninf_set_vc_info_to_tsrec(ctx, vc,
			ctx->sentest_vsync_order_info.cfg[i].tsrec_id, 0);
		pr_info("[%s] VC %u DT 0x%x using TSREC %d config done\n",
			__func__,
			ctx->sentest_vsync_order_info.cfg[i].vc,
			ctx->sentest_vsync_order_info.cfg[i].dt,
			ctx->sentest_vsync_order_info.cfg[i].tsrec_id);
		break;
	}
	return 0;
}
