// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

#include <linux/delay.h>

#include "mtk_camera-v4l2-controls.h"
#include "adaptor.h"
#include "adaptor-ctrls.h"
#include "adaptor-trace.h"
#include "adaptor-fsync-ctrls.h"
#include "adaptor-common-ctrl.h"
#include "adaptor-tsrec-cb-ctrl-impl.h"
#include "adaptor-eint-cb-ctrl-impl.h"
#include "adaptor-sentest-ctrl.h"

#include "adaptor-command.h"
#include "adaptor-broadcast-ctrls.h"

/*---------------------------------------------------------------------------*/
// define
/*---------------------------------------------------------------------------*/
#define sd_to_ctx(__sd) container_of(__sd, struct adaptor_ctx, sd)


#define REDUCE_ADAPTOR_COMMAND_LOG


/*---------------------------------------------------------------------------*/
// utilities / static functions
/*---------------------------------------------------------------------------*/
static inline int chk_input_arg(const struct adaptor_ctx *ctx, const void *arg,
	int *p_ret, const char *caller)
{
	*p_ret = 0;
	if (unlikely(ctx == NULL)) {
		*p_ret = -EINVAL;
		return *p_ret;
	}
	if (unlikely(arg == NULL)) {
		*p_ret = -EINVAL;
		adaptor_logi(ctx,
			"[%s] ERROR: idx:%d, invalid arg:(nullptr), ret:%d\n",
			caller, ctx->idx, *p_ret);
		return *p_ret;
	}
	return *p_ret;
}


static int get_sensor_mode_info(struct adaptor_ctx *ctx, u32 mode_id,
				struct mtk_sensor_mode_info *info)
{
	int ret = 0;

	/* Test arguments */
	if (unlikely(info == NULL)) {
		ret = -EINVAL;
		adaptor_logi(ctx, "ERROR: invalid argumet info is nullptr\n");
		return ret;
	}
	if (unlikely(mode_id >= SENSOR_SCENARIO_ID_MAX)) {
		ret = -EINVAL;
		adaptor_logi(ctx, "ERROR: invalid argumet scenario %u\n", mode_id);
		return ret;
	}

	info->scenario_id = mode_id;
	info->mode_exposure_num = g_scenario_exposure_cnt(ctx, mode_id);
	info->active_line_num = ctx->mode[mode_id].active_line_num;
	info->avg_linetime_in_ns = ctx->mode[mode_id].linetime_in_ns_readout;

	return 0;
}


/*---------------------------------------------------------------------------*/
// functions that called by in-kernel drivers.
/*---------------------------------------------------------------------------*/
/* GET */

static int g_cmd_fake_sensor_info(struct adaptor_ctx *ctx, void *arg)
{
	int ret = 0;
	struct mtk_fake_sensor_info *p_info = NULL;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	p_info = arg;
	memset(p_info, 0, sizeof(struct mtk_fake_sensor_info));

	p_info->is_fake_sensor = (ctx->subctx.s_ctx.sensor_id == 0xFAFA) ? 1 : 0;
	p_info->fps = ctx->subctx.s_ctx.mode[ctx->cur_mode->id].max_framerate;
	p_info->hdr_mode = ctx->subctx.s_ctx.mode[ctx->cur_mode->id].hdr_mode;
	p_info->sensor_output_dataformat =
		ctx->subctx.s_ctx.mode[ctx->cur_mode->id].sensor_output_dataformat;
	p_info->sensor_output_dataformat_cell_type =
		ctx->subctx.s_ctx.mode[ctx->cur_mode->id].sensor_output_dataformat_cell_type;

	return ret;
};

static int g_cmd_sensor_mode_config_info(struct adaptor_ctx *ctx, void *arg)
{
	int i;
	int ret = 0;
	int mode_cnt = 0;
	struct mtk_sensor_mode_config_info *p_info = NULL;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	p_info = arg;

	memset(p_info, 0, sizeof(struct mtk_sensor_mode_config_info));

	p_info->current_scenario_id = ctx->cur_mode->id;
	if (!get_sensor_mode_info(ctx, ctx->cur_mode->id, p_info->seamless_scenario_infos))
		++mode_cnt;

	for (i = 0; i < SENSOR_SCENARIO_ID_MAX; i++) {
		if (ctx->seamless_scenarios[i] == SENSOR_SCENARIO_ID_NONE)
			break;
		else if (ctx->seamless_scenarios[i] == ctx->cur_mode->id)
			continue;
		else if (!get_sensor_mode_info(ctx, ctx->seamless_scenarios[i],
					       p_info->seamless_scenario_infos + mode_cnt))
			++mode_cnt;
	}

	p_info->count = mode_cnt;

	return ret;
}

static int g_cmd_send_sensor_mode_config_info(struct adaptor_ctx *ctx, void *arg)
{
	int ret = 0;
	int send_scenario_id = 0;
	struct mtk_sensor_mode_info *p_mode_info = NULL;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	p_mode_info = arg;
	send_scenario_id = p_mode_info->scenario_id;
	memset(p_mode_info, 0, sizeof(struct mtk_sensor_mode_info));

	ret = get_sensor_mode_info(ctx, send_scenario_id, p_mode_info);

	if (ret) {
		adaptor_logi(ctx,"get_sensor_mode_info fail , scenario_id:%d, ret:%d\n",
			send_scenario_id, ret);
	}

	return ret;
}

static int g_cmd_sensor_in_reset(struct adaptor_ctx *ctx, void *arg)
{
	int ret = 0;
	bool *in_reset = NULL;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	in_reset = arg;

	*in_reset = !!(ctx->is_sensor_reset_stream_off);

	return ret;
}

static int g_cmd_sensor_has_ebd_parser(struct adaptor_ctx *ctx, void *arg)
{
	int ret = 0;
	bool *has_parser = NULL;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	has_parser = arg;

	*has_parser = !!(ctx->subdrv->ops->parse_ebd_line);

	return ret;
}

static int g_cmd_sensor_ebd_info_by_scenario(struct adaptor_ctx *ctx, void *arg)
{
	int i;
	int ret = 0;
	struct mtk_sensor_ebd_info_by_scenario *p_info = NULL;
	u32 scenario_id;
	struct mtk_mbus_frame_desc fd_tmp = {0};
	struct mtk_mbus_frame_desc_entry_csi2 *entry;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	p_info = arg;

	scenario_id = p_info->input_scenario_id;

	memset(p_info, 0, sizeof(struct mtk_sensor_ebd_info_by_scenario));
	p_info->input_scenario_id = scenario_id;

	ret = subdrv_call(ctx, get_frame_desc, scenario_id, &fd_tmp);
	if (!ret) {
		for (i = 0; i < fd_tmp.num_entries; ++i) {
			entry = &fd_tmp.entry[i].bus.csi2;
			if (entry->user_data_desc == VC_GENERAL_EMBEDDED &&
			    entry->ebd_parsing_type != MTK_EBD_PARSING_TYPE_MIPI_RAW_NA) {

				p_info->exp_hsize = entry->hsize;
				p_info->exp_vsize = entry->vsize;
				p_info->dt_remap_to_type = entry->dt_remap_to_type;
				p_info->data_type = entry->data_type;
				p_info->ebd_parsing_type = entry->ebd_parsing_type;

				adaptor_logd(ctx,
					"[%s] scenario %u desc %u/%u/%u/0x%x/%u\n",
					__func__,
					scenario_id,
					p_info->exp_hsize,
					p_info->exp_vsize,
					p_info->dt_remap_to_type,
					p_info->data_type,
					p_info->ebd_parsing_type);

				break;
			}

		}
	}

	return ret;
}

static int g_cmd_sensor_frame_cnt(struct adaptor_ctx *ctx, void *arg)
{
	u32 *frame_cnt = arg;
	union feature_para para;
	u32 len;
	int ret = 0;

	para.u64[0] = 0;

	/* error handling (unexpected case) */
	if (unlikely(arg == NULL)) {
		ret = -ENOIOCTLCMD;
		adaptor_logi(ctx,
			"ERROR: V4L2_CMD_G_SENSOR_FRAME_CNT, idx:%d, input arg is nullptr, return:%d\n",
			ctx->idx, ret);
		return ret;
	}

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_FRAME_CNT,
		para.u8, &len);

	*frame_cnt = para.u64[0];

	return ret;
}

static int g_cmd_sensor_get_lbmf_type_by_secnario(struct adaptor_ctx *ctx, void *arg)
{
	int ret = 0;
	struct mtk_seninf_lbmf_info *info = NULL;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	info = arg;
	info->is_lbmf = 0;

	if (unlikely(ctx->subctx.s_ctx.mode == NULL))
		return -EINVAL;


	info->is_lbmf =
			(ctx->subctx.s_ctx.mode[info->scenario].hdr_mode == HDR_RAW_LBMF) ?
				true : false;

	return ret;
}

static int g_cmd_sensor_glp_dt(struct adaptor_ctx *ctx, void *arg)
{
	u32 *buf = NULL;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	buf = (u32 *)arg;
	memcpy(buf, ctx->subctx.s_ctx.glp_dt, sizeof(ctx->subctx.s_ctx.glp_dt));

	return ret;
}

static int g_cmd_sensor_vc_info_by_scenario(struct adaptor_ctx *ctx, void *arg)
{
	int ret = 0;
	struct mtk_sensor_vc_info_by_scenario *p_info = NULL;
	struct mtk_mbus_frame_desc fd_tmp = {0};

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	p_info = arg;
	ret = subdrv_call(ctx, get_frame_desc, p_info->scenario_id, &fd_tmp);
	memcpy(&p_info->fd.entry, &fd_tmp.entry,
				sizeof(struct mtk_mbus_frame_desc_entry)*fd_tmp.num_entries);
	p_info->fd.num_entries = fd_tmp.num_entries;

	return ret;
}

static int g_cmd_g_sensor_stream_status(struct adaptor_ctx *ctx, void *arg)
{
	int ret = 0;
	bool *is_stream = NULL;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	is_stream = arg;

	*is_stream = !!(ctx->is_streaming);

	return ret;
}

static int g_cmd_g_sensor_connector_status(struct adaptor_ctx *ctx, void *arg)
{
	u32 sensor_id = 0xffffffff;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely(ctx == 0)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	if (ctx->power_refcnt == 0) {
		adaptor_logi(ctx, "sensor do not power on\n");
		return -EINVAL;
	}

	if (subdrv_call(ctx, get_id, &sensor_id)) {
		adaptor_logi(ctx, "sensor i2c test failed, please check connector status\n");
		ret = -EINVAL;
	}

	return ret;
}

static int g_cmd_dgc_vsl_linetime_info(struct adaptor_ctx *ctx, void *arg)
{
	int ret = 0;
	struct struct_cmd_sensor_dcg_vsl_info *info = NULL;
	const struct subdrv_mode_struct *mode_st = NULL;
	u32 scenario_id = 0;
	union feature_para para;
	u32 len = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	info = arg;
	scenario_id = info->scenario_id;
	memset(&info->info, 0, sizeof(struct struct_dcg_vsl_info));

	if (unlikely(ctx->subctx.s_ctx.mode == NULL))
		return -EINVAL;
	if (unlikely(scenario_id > ctx->subctx.s_ctx.sensor_mode_num))
		return -EINVAL;

	/* get the mode's const pointer of the scenario_id */
	mode_st = &ctx->subctx.s_ctx.mode[scenario_id];

	if (!(mode_st->hdr_mode == HDR_RAW_DCG_RAW_VS
			|| mode_st->hdr_mode == HDR_RAW_DCG_COMPOSE_VS)) {
		info->hdr_mode = 0;
		return 0;
	}

	para.u64[0] = scenario_id;
	para.u64[1] = (u64)&info->info;

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_DCG_VSL_INFO,
		para.u8, &len);

	return ret;
}

static int g_cmd_fsync_anchor_info(struct adaptor_ctx *ctx, void *arg)
{
	long long *buf = NULL;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	buf = (long long *)arg;

	/* call to fsync to fill in the info */
	notify_fsync_mgr_g_latest_anchor_info(ctx, buf);

#ifndef REDUCE_ADAPTOR_COMMAND_LOG
	adaptor_logi(ctx,
		"V4L2_CMD_G_FSYNC_ANCHOR_INFO, idx:%d, result:%lld\n",
		ctx->idx, *buf);
#endif

	return ret;
}

static int g_cmd_chk_seamless_switch_done_ts(struct adaptor_ctx *ctx, void *arg)
{
	bool is_seamless_switch_before = false;
	u64 seamless_switch_i2c_done_ts = 0;
	const unsigned int Min_sensor_latch_time_in_us = 3000;
	unsigned int ts_diff_in_us;


	/* unexpected case, arg is nullptr */
	if (unlikely(ctx == 0)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&ctx->seamless_ts_info.seamless_switch_ts_mutex);
	is_seamless_switch_before = ctx->seamless_ts_info.is_seamless_switch_before;
	seamless_switch_i2c_done_ts = ctx->seamless_ts_info.seamless_switch_i2c_done_ts;
	mutex_unlock(&ctx->seamless_ts_info.seamless_switch_ts_mutex);

	if (!is_seamless_switch_before)
		return 0;

	ts_diff_in_us = (ktime_get_boottime_ns() - seamless_switch_i2c_done_ts) / 1000;

	if (ts_diff_in_us < Min_sensor_latch_time_in_us)
		udelay(Min_sensor_latch_time_in_us - ts_diff_in_us);

	clear_seamless_switch_ts_info(ctx);

	return 0;
}

static int s_cmd_sensor_broadcast_event(struct adaptor_ctx *ctx, void *arg)
{
	int ret = 0;
	struct mtk_cam_broadcast_info *p_info = NULL;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	p_info = arg;

	adaptor_get_broadcast_event(ctx, p_info);

	return ret;
}

/* SET */
static int s_cmd_fsync_sync_frame_start_end(struct adaptor_ctx *ctx, void *arg)
{
	int *p_flag = NULL;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	/* casting arg to int-pointer for using */
	p_flag = arg;

	adaptor_logd(ctx,
		"V4L2_CMD_FSYNC_SYNC_FRAME_START_END, idx:%d, flag:%d\n",
		ctx->idx, *p_flag);

	notify_fsync_mgr_sync_frame(ctx, *p_flag);

	return ret;
}


static int s_cmd_tsrec_notify_vsync(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_cam_seninf_tsrec_vsync_info *buf = NULL;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	buf = (struct mtk_cam_seninf_tsrec_vsync_info *)arg;

	memcpy(&ctx->vsync_info, buf, sizeof(ctx->vsync_info));


#ifndef REDUCE_ADAPTOR_COMMAND_LOG
	adaptor_logd(ctx,
		"V4L2_CMD_TSREC_NOTIFY_VSYNC, idx:%d, vsync_info(tsrec_no:%u, seninf_idx:%u, sys_ts%llu(ns), tsrec_ts:%llu(us))\n",
		ctx->idx,
		ctx->vsync_info.tsrec_no,
		ctx->vsync_info.seninf_idx,
		ctx->vsync_info.irq_sys_time_ns,
		ctx->vsync_info.irq_tsrec_ts_us);
#endif


	/* tsrec notify vsync, call all APIs that needed this info */
	notify_fsync_mgr_vsync_by_tsrec(ctx);

	return ret;
}


static int s_cmd_tsrec_notify_sensor_hw_pre_latch(
	struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_cam_seninf_tsrec_timestamp_info *ts_info = NULL;
	unsigned long long sys_ts;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	ts_info = (struct mtk_cam_seninf_tsrec_timestamp_info *)arg;
	sys_ts = ktime_get_boottime_ns();

	ADAPTOR_TRACE_FORCE_BEGIN("adaptor::",
		"imgsensor::V4L2_CMD_TSREC_NOTIFY_SENSOR_HW_PRE_LATCH, idx:%d, ts_info(tsrec_no:%u, seninf_idx:%u, tick_factor:%u, sys_ts:%llu(ns), tsrec_ts:%llu(us), tick:%llu, ts(0:(%llu/%llu/%llu/%llu), 1:(%llu/%llu/%llu/%llu), 2:(%llu/%llu/%llu/%llu)), curr_sys_ts:%llu(ns)",
		ctx->idx,
		ts_info->tsrec_no,
		ts_info->seninf_idx,
		ts_info->tick_factor,
		ts_info->irq_sys_time_ns,
		ts_info->irq_tsrec_ts_us,
		ts_info->tsrec_curr_tick,
		ts_info->exp_recs[0].ts_us[0],
		ts_info->exp_recs[0].ts_us[1],
		ts_info->exp_recs[0].ts_us[2],
		ts_info->exp_recs[0].ts_us[3],
		ts_info->exp_recs[1].ts_us[0],
		ts_info->exp_recs[1].ts_us[1],
		ts_info->exp_recs[1].ts_us[2],
		ts_info->exp_recs[1].ts_us[3],
		ts_info->exp_recs[2].ts_us[0],
		ts_info->exp_recs[2].ts_us[1],
		ts_info->exp_recs[2].ts_us[2],
		ts_info->exp_recs[2].ts_us[3],
		sys_ts);

	/* tsrec notify sensor hw pre-latch, call all APIs that needed this info */
	notify_fsync_mgr_sensor_hw_pre_latch_by_tsrec(ctx, ts_info);

	ADAPTOR_TRACE_FORCE_END();

	return ret;
}

static int s_cmd_eint_notify_vsync(
	struct adaptor_ctx *ctx, void *arg)
{
	const unsigned int log_str_len = 800;
	struct mtk_cam_seninf_eint_timestamp_info *ts_info = NULL;
	unsigned long long sys_ts;
	char *log_buf = NULL;
	int ret = 0, len = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	ts_info = (struct mtk_cam_seninf_eint_timestamp_info *)arg;
	sys_ts = ktime_get_boottime_ns();

	log_buf = kcalloc(log_str_len + 1, sizeof(char), GFP_ATOMIC);
	if (unlikely(log_buf == NULL))
		goto s_cmd_eint_notify_vsync_end;

	log_buf[0] = '\0';
	/* adaptor_logd(ctx, */
	adaptor_snprf(ctx, log_str_len, log_buf, len,
		"imgsensor::V4L2_CMD_EINT_NOTIFY_VSYNC, eint_no:%u, tsrec_idx:%u, ts:%llu(%llu/%u), seq_no:%d, irq(sys:%llu|mono:%llu), ts:(%llu/%llu/%llu/%llu)",
		ts_info->eint_no,
		ts_info->tsrec_idx,
		ts_info->tick / ts_info->tick_factor,
		ts_info->tick,
		ts_info->tick_factor,
		ts_info->irq_seq_no,
		ts_info->irq_sys_time_ns,
		ts_info->irq_mono_time_ns,
		ts_info->ts_us[0],
		ts_info->ts_us[1],
		ts_info->ts_us[2],
		ts_info->ts_us[3]);

	ADAPTOR_SYSTRACE_BEGIN_MUST("%s",log_buf);
	ADAPTOR_TRACE_FORCE_END();
	adaptor_logd(ctx, "%s\n", log_buf);

s_cmd_eint_notify_vsync_end:
	/* notify framesync */
	notify_fsync_mgr_vsync_by_eint(ctx, ts_info);

	return 0;
}

static int s_cmd_eint_notify_irq_en(
	struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_cam_seninf_eint_irq_en_info *info = NULL;
	unsigned long long sys_ts;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	info = (struct mtk_cam_seninf_eint_irq_en_info *)arg;
	sys_ts = ktime_get_boottime_ns();

	adaptor_logi(ctx,
		"eint_no:%u tsrec_idx:%u flag:%u sys_ts:%llu\n",
		info->eint_no,
		info->tsrec_idx,
		info->flag,
		sys_ts);

	notify_fsync_mgr_eint_irq_en(ctx, info->eint_no, info->flag);

	return 0;
}

static int s_cmd_eint_setup_cb_info(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_cam_seninf_eint_cb_info *eint_cb_info = NULL;
	unsigned long long sys_ts;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	eint_cb_info = (struct mtk_cam_seninf_eint_cb_info *)arg;
	sys_ts = ktime_get_boottime_ns();

	adaptor_logi(ctx,
		"eint_no:%u tsrec_idx:%u is_start:%u sys_ts:%llu\n",
		eint_cb_info->eint_no,
		eint_cb_info->tsrec_idx,
		eint_cb_info->is_start,
		sys_ts);

	adaptor_eint_cb_ctrl_info_setup(ctx, eint_cb_info, eint_cb_info->is_start);

	return ret;
}

static int s_cmd_eint_notify_force_maskframe(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_fsync_hw_mcss_mask_frm_info *info = NULL;
	unsigned long long sys_ts;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	info = (struct mtk_fsync_hw_mcss_mask_frm_info *)arg;
	sys_ts = ktime_get_boottime_ns();

	subdrv_call(ctx, mcss_set_mask_frame, info->mask_frm_num, info->is_critical);
	ctx->mask_frm_num_last = info->mask_frm_num;

	adaptor_logi(ctx,
		"V4L2_CMD_EINT_NOTIFY_FORCE_MASKFRAME mask_frm_num:%u, is_critical:%u ctx->mask_frm_num_last:%u sys_ts:%llu\n",
		info->mask_frm_num,
		info->is_critical,
		ctx->mask_frm_num_last,
		sys_ts);

	return ret;
}

static int s_cmd_eint_notify_force_en_xvs(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_fsync_hw_mcss_init_info *info = NULL;
	unsigned long long sys_ts;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	info = (struct mtk_fsync_hw_mcss_init_info *)arg;
	sys_ts = ktime_get_boottime_ns();

	memset(&(ctx->subctx.mcss_init_info), 0, sizeof(struct mtk_fsync_hw_mcss_init_info));
	memcpy(&(ctx->subctx.mcss_init_info),
		info, sizeof(struct mtk_fsync_hw_mcss_init_info));

	adaptor_logi(ctx,
		"V4L2_CMD_EINT_NOTIFY_FORCE_EN_XVS enable_mcss:%u, is_mcss_master:%u sys_ts:%llu\n",
		ctx->subctx.mcss_init_info.enable_mcss,
		ctx->subctx.mcss_init_info.is_mcss_master,
		sys_ts);

	return ret;
}

static int s_cmd_tsrec_send_timestamp_info(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_cam_seninf_tsrec_timestamp_info *buf = NULL;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	buf = (struct mtk_cam_seninf_tsrec_timestamp_info *)arg;
	memcpy(&ctx->ts_info, buf, sizeof(ctx->ts_info));

	if (unlikely(notify_imgsensor_start_streaming_delay(ctx, buf)))
		return -EINVAL;

	if (unlikely(notify_sentest_tsrec_time_stamp(ctx, buf)))
		return -EINVAL;

	return ret;
}

static int s_cmd_sensor_parse_ebd(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_recv_sensor_ebd_line *recv_ebd;
	struct mtk_ebd_dump obj;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	recv_ebd = (struct mtk_recv_sensor_ebd_line *)arg;

	memset(&obj, 0, sizeof(obj));
	ret = subdrv_call(ctx, parse_ebd_line, recv_ebd, &obj);

	mutex_lock(&ctx->ebd_lock);
	ctx->latest_ebd.recv_ts = ktime_get_boottime_ns();
	ctx->latest_ebd.req_no = recv_ebd->req_id;
	memset(ctx->latest_ebd.req_fd_desc, 0, sizeof(ctx->latest_ebd.req_fd_desc));
	strncpy(ctx->latest_ebd.req_fd_desc, recv_ebd->req_fd_desc,
		sizeof(ctx->latest_ebd.req_fd_desc) - 1);
	memcpy(&ctx->latest_ebd.record, &obj, sizeof(struct mtk_ebd_dump));
	mutex_unlock(&ctx->ebd_lock);

	return ret;
}

static int s_cmd_tsrec_setup_cb_info(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_cam_seninf_tsrec_cb_info *tsrec_cb_info = NULL;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	tsrec_cb_info = (struct mtk_cam_seninf_tsrec_cb_info *)arg;

	adaptor_tsrec_cb_ctrl_info_setup(ctx,
		tsrec_cb_info, tsrec_cb_info->is_connected_to_tsrec);

	// adaptor_tsrec_cb_ctrl_dbg_info_dump(ctx, __func__);

	return ret;
}

static u64 set_sensor_fl_prolong (struct adaptor_ctx *ctx)
{
	static const u64 seamless_thr_ns = 25000000;
	u32 exp_count = g_scenario_exposure_cnt(ctx, ctx->subctx.current_scenario_id);
	u32 ecnt = exp_count;
	u64 ext_ftime = 0;
	u64 tline_ns = ctx->cur_mode->linetime_in_ns;
	u32 e_margin = ctx->subctx.margin * exp_count;
	u32 acc_lines = 0;
	u64 t_last_exp_2_next = 0;
	enum IMGSENSOR_HDR_MODE_ENUM hdr_mode;

	hdr_mode = (ctx->subctx.s_ctx.mode == NULL)
		? HDR_NONE
		: ctx->subctx.s_ctx.mode[ctx->cur_mode->id].hdr_mode;

	if (hdr_mode == HDR_RAW_STAGGER) {
		/* apply extend logic only in stagger mode */
		while (exp_count && (--ecnt))
			acc_lines += (e_margin +ctx->subctx.exposure[ecnt]);

		if (acc_lines && (ctx->subctx.frame_length > acc_lines)) {
			t_last_exp_2_next = (ctx->subctx.frame_length - acc_lines) * tline_ns;
			if (t_last_exp_2_next < seamless_thr_ns)
				ext_ftime = seamless_thr_ns - t_last_exp_2_next;
			if (ext_ftime > seamless_thr_ns) {
				adaptor_logi(ctx,
					"calculate ext_ftime (%llu) is large than threshold (%llu)\n",
					ext_ftime, seamless_thr_ns);
				ext_ftime = seamless_thr_ns;
			}
		}
	}

	adaptor_logi(ctx,
		"hdr_mode/exp_cnt/tline/exp_margin/acc_lines/fll/last_exp_2_next/result (%u/%u/%llu/%u/%u/%u/%llu/%llu)\n",
		hdr_mode, exp_count, tline_ns, e_margin, acc_lines,
		ctx->subctx.frame_length, t_last_exp_2_next, ext_ftime);

	/* extend frame time in ns */
	return ext_ftime;
}

static int s_cmd_sensor_fl_prolong(struct adaptor_ctx *ctx, void *arg)
{
	u32 act = *((u32 *)arg), len = 0;
	u64 ext_time = 0;
	union feature_para para;
	int ret = 0;

	if (act & IMGSENSOR_EXTEND_FRAME_LENGTH_TO_DOL) {
		ext_time = set_sensor_fl_prolong(ctx);
		para.u64[0] = ext_time;
		subdrv_call(ctx, feature_control,
						SENSOR_FEATURE_SET_SEAMLESS_EXTEND_FRAME_LENGTH,
						para.u8, &len);
	}
	if (act & IMGSENSOR_EXTEND_FRAME_LENGTH_TO_DOL_DISABLE) {
		ctx->subctx.extend_frame_length_en = FALSE;
		adaptor_logi(ctx, "Disabled extend framelength.");
	}
	adaptor_logi(ctx, "set sensor fl prolong, ext_time:%llu, act:%u",
		ext_time,
		act);

	return ret;
}

static int s_cmd_sensor_aov_dualsync(struct adaptor_ctx *ctx, void *arg)
{
	u32 role = *((u32 *)arg);

	adaptor_logi(ctx, "[%s] role:%u", __func__, role);
	/* role: 1-master, 2-slave */
	subdrv_call(ctx, aov_dualsync, role);
	return 0;
}


static int set_cb_func_of_fake_sensor(struct adaptor_ctx *ctx, void *arg)
{
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	if (ctx->subctx.s_ctx.fake_sensor_cb_init != NULL)
		ctx->subctx.s_ctx.fake_sensor_cb_init((void *) arg);

	adaptor_logi(ctx, "[%s] arg:%p", __func__, arg);

	return ret;
}

static int g_cmd_ctle_param(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_sensor_ctle_param *input_ctle_param = NULL;
	struct mtk_sensor_ctle_param *sensor_ctle_param = NULL;
	u32 scenario;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	input_ctle_param = (struct mtk_sensor_ctle_param *)arg;
	scenario = ctx->subctx.current_scenario_id;

	if (ctx->subctx.s_ctx.mode[scenario].ctle_param)
		sensor_ctle_param = ctx->subctx.s_ctx.mode[scenario].ctle_param;
	else if (ctx->subctx.s_ctx.ctle_param)
		sensor_ctle_param = ctx->subctx.s_ctx.ctle_param;


	if (sensor_ctle_param != NULL)
		memcpy(input_ctle_param, sensor_ctle_param, sizeof(struct mtk_sensor_ctle_param));
	else
		memset(input_ctle_param, 0, sizeof(struct mtk_sensor_ctle_param));


	return 0;
}

static int s_cmd_sensor_frame_length(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_fps_by_scenario *info = arg;
	union feature_para para;
	u32 len;

	para.u64[0] = ctx->subctx.current_scenario_id;
	para.u64[1] = info->fps;

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO,
		para.u8, &len);

	adaptor_logi(ctx, "[%s] set max frame rate %d", __func__, info->fps);
	return 0;
}

static int g_cmd_insertion_loss_param(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_sensor_insertion_loss *input_param = NULL;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return ret;

	input_param = (struct mtk_sensor_insertion_loss *)arg;

	if (ctx->subctx.s_ctx.insertion_loss)
		memcpy(input_param,
			ctx->subctx.s_ctx.insertion_loss,
			sizeof(struct mtk_sensor_insertion_loss));
	else
		memset(input_param, 0, sizeof(struct mtk_sensor_insertion_loss));

	return 0;
}

static int g_cmd_cust_ctle_config(struct adaptor_ctx *ctx, void *arg)
{
	struct mtk_sensor_ctle_param *input_param = NULL;
	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return 0;

	input_param = (struct mtk_sensor_ctle_param *)arg;

	ret = subdrv_call(ctx, get_customer_ctle_config, input_param);

	if (ret) {
		memset(input_param, 0, sizeof(struct mtk_sensor_ctle_param));
		adaptor_logi(ctx, "[%s] no customer ctle table", __func__);
		return -EFAULT;
	}

	return 0;
}

static int s_cmd_notify_mipi_err_cnt(struct adaptor_ctx *ctx, void *arg)
{	int ret = 0;

	/* unexpected case, arg is nullptr */
	if (unlikely((chk_input_arg(ctx, arg, &ret, __func__)) != 0))
		return 0;

	subdrv_call(ctx, notify_lastest_mipi_err_cnt, (struct mtk_sensor_mipi_error_info *)arg);

	return 0;
}

/*---------------------------------------------------------------------------*/
// adaptor command framework/entry
/*---------------------------------------------------------------------------*/

struct command_entry {
	unsigned int cmd;
	int (*func)(struct adaptor_ctx *ctx, void *arg);
};

static const struct command_entry command_list[] = {
	/* GET */
	{V4L2_CMD_GET_SENSOR_MODE_CONFIG_INFO, g_cmd_sensor_mode_config_info},
	{V4L2_CMD_GET_SEND_SENSOR_MODE_CONFIG_INFO, g_cmd_send_sensor_mode_config_info},
	{V4L2_CMD_SENSOR_IN_RESET, g_cmd_sensor_in_reset},
	{V4L2_CMD_SENSOR_HAS_EBD_PARSER, g_cmd_sensor_has_ebd_parser},
	{V4L2_CMD_GET_SENSOR_EBD_INFO_BY_SCENARIO, g_cmd_sensor_ebd_info_by_scenario},
	{V4L2_CMD_SENSOR_GET_LBMF_TYPE_BY_SCENARIO, g_cmd_sensor_get_lbmf_type_by_secnario},
	{V4L2_CMD_G_SENSOR_FRAME_CNT, g_cmd_sensor_frame_cnt},
	{V4L2_CMD_G_SENSOR_GLP_DT, g_cmd_sensor_glp_dt},
	{V4L2_CMD_G_SENSOR_VC_INFO_BY_SCENARIO, g_cmd_sensor_vc_info_by_scenario},
	{V4L2_CMD_G_SENSOR_STREAM_STATUS, g_cmd_g_sensor_stream_status},
	{V4L2_CMD_G_SENSOR_FAKE_SENSOR_INFO, g_cmd_fake_sensor_info},
	{V4L2_CMD_G_SENSOR_CTLE_PARAM, g_cmd_ctle_param},
	{V4L2_CMD_G_SENSOR_CONNECTOR_STATUS, g_cmd_g_sensor_connector_status},
	{V4L2_CMD_G_INSERTION_LOSS_PARAM, g_cmd_insertion_loss_param},
	{V4L2_CMD_G_CUST_CTLE_CONFIG, g_cmd_cust_ctle_config},
	{V4L2_CMD_G_DCG_VSL_LINETIME_INFO, g_cmd_dgc_vsl_linetime_info},
	{V4L2_CMD_G_FSYNC_ANCHOR_INFO, g_cmd_fsync_anchor_info},
	{V4L2_CMD_G_CHECK_SENSOR_SEAMLESS_DONE_TS, g_cmd_chk_seamless_switch_done_ts},

	/* SET */
	{V4L2_CMD_SET_CB_FUNC_OF_FAKE_SENSOR, set_cb_func_of_fake_sensor},
	{V4L2_CMD_FSYNC_SYNC_FRAME_START_END, s_cmd_fsync_sync_frame_start_end},
	{V4L2_CMD_TSREC_NOTIFY_VSYNC, s_cmd_tsrec_notify_vsync},
	{V4L2_CMD_TSREC_NOTIFY_SENSOR_HW_PRE_LATCH,
		s_cmd_tsrec_notify_sensor_hw_pre_latch},
	{V4L2_CMD_TSREC_SEND_TIMESTAMP_INFO, s_cmd_tsrec_send_timestamp_info},
	{V4L2_CMD_EINT_NOTIFY_VSYNC, s_cmd_eint_notify_vsync},
	{V4L2_CMD_EINT_NOTIFY_IRQ_EN, s_cmd_eint_notify_irq_en},
	{V4L2_CMD_EINT_SETUP_CB_FUNC_OF_SENSOR, s_cmd_eint_setup_cb_info},
	{V4L2_CMD_EINT_NOTIFY_FORCE_EN_XVS, s_cmd_eint_notify_force_en_xvs},
	{V4L2_CMD_EINT_NOTIFY_FORCE_MASKFRAME, s_cmd_eint_notify_force_maskframe},
	{V4L2_CMD_SENSOR_PARSE_EBD, s_cmd_sensor_parse_ebd},
	{V4L2_CMD_TSREC_SETUP_CB_FUNC_OF_SENSOR, s_cmd_tsrec_setup_cb_info},
	{V4L2_CMD_SET_SENSOR_FL_PROLONG, s_cmd_sensor_fl_prolong},
	{V4L2_CMD_SET_SENSOR_AOV_DUALSYNC, s_cmd_sensor_aov_dualsync},
	{V4L2_CMD_SET_SENSOR_BROADCAST_EVENT, s_cmd_sensor_broadcast_event},
	{V4L2_CMD_SET_SENSOR_FRAME_LENGTH, s_cmd_sensor_frame_length},
	{V4L2_CMD_SET_MIPI_ERR_CNT, s_cmd_notify_mipi_err_cnt},
};

long adaptor_command(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct adaptor_ctx *ctx = NULL;
	int i, ret = -ENOIOCTLCMD, is_cmd_found = 0;

	/* error handling (unexpected case) */
	if (unlikely(sd == NULL)) {
		ret = -ENOIOCTLCMD;
		adaptor_logi(ctx,
			"ERROR: get nullptr of v4l2_subdev (sd), return:%d [cmd id:%#x]\n",
			ret, cmd);
		return ret;
	}

	ctx = sd_to_ctx(sd);

#ifndef REDUCE_ADAPTOR_COMMAND_LOG
	adaptor_logd(ctx,
		"dispatch command request, idx:%d, cmd id:%#x\n",
		ctx->idx, cmd);
#endif

	/* dispatch command request */
	for (i = 0; i < ARRAY_SIZE(command_list); i++) {
		if (command_list[i].cmd == cmd) {
			is_cmd_found = 1;
			ret = command_list[i].func(ctx, arg);
			break;
		}
	}
	if (unlikely(is_cmd_found == 0)) {
		ret = -ENOIOCTLCMD;
		adaptor_logi(ctx,
			"ERROR: get unknown command request, idx:%d, cmd:%u, return:%d\n",
			ctx->idx, cmd, ret);
		return ret;
	}

	return ret;
}

