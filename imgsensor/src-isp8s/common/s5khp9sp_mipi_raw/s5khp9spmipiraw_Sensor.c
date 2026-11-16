// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5khp9spmipiraw_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include "s5khp9spmipiraw_Sensor.h"
#include "adaptor-subdrv-ctrl.h"
#include "adaptor-subdrv.h"
#include "adaptor-ctrls.h"
#include <linux/of.h>
#include "adaptor-i2c.h"
#include <linux/regulator/consumer.h>

static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static int s5khp9sp_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int s5khp9sp_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int s5khp9sp_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt, u64 sof_ts);
static int s5khp9sp_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int s5khp9sp_i3c_pre_config(struct subdrv_ctx *ctx);
static int s5khp9sp_set_ctrl_locker(struct subdrv_ctx *ctx, u32 cid, bool *is_lock);
static int s5khp9sp_chk_streaming_st(void *arg);

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, s5khp9sp_set_test_pattern},
	{SENSOR_FEATURE_SET_TEST_PATTERN_DATA, s5khp9sp_set_test_pattern_data},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, s5khp9sp_seamless_switch},
	{SENSOR_FEATURE_SET_AWB_GAIN, s5khp9sp_set_awb_gain},
};


static struct subdrv_static_ctx_ext_ops static_ext_ops = {
	.i2c_addr_table = {0x20, 0xFF},
	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_streaming_st = s5khp9sp_chk_streaming_st,

#ifdef S5KHP9SP_ISF_DBG
	.debug_check_with_exist_s_ctx = &s5khp9sp_legacy_s_ctx,
#endif
};

static struct subdrv_ops ops = {
	.get_id = common_get_imgsensor_id,
	.init_ctx = common_init_ctx,
	.open = common_open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = common_close,
	.get_frame_desc = common_get_frame_desc,
	.get_csi_param = common_get_csi_param,
	.update_sof_cnt = common_update_sof_cnt,
	.vsync_notify = vsync_notify,
	.i3c_pre_config = s5khp9sp_i3c_pre_config,
	.set_ctrl_locker = s5khp9sp_set_ctrl_locker,
};

const struct subdrv_entry s5khp9sp_mipi_raw_entry = {
	.name = "s5khp9sp_mipi_raw",
	.ops = &ops,
	.is_fw_support = TRUE,
	.fw_ext_ops = &static_ext_ops,
};

/* FUNCTION */
static int get_sensor_temperature(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u16 temperature = 0;
	int temperature_convert = 0;

	temperature = subdrv_ixc_rd_u16(ctx, ctx->s_ctx.reg_addr_temp_read);

	temperature_convert = (temperature>>8)&0xFF;

	temperature_convert = (temperature_convert > 100) ? 100: temperature_convert;

	DRV_LOG(ctx, "temperature: %d degrees\n", temperature_convert);
	return temperature_convert;
}

static void seamless_switch_update_hw_re_init_time(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM pre_seamless_scenario_id,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id)
{
	if (ctx->s_ctx.mode[pre_seamless_scenario_id].pclk != ctx->s_ctx.mode[scenario_id].pclk)
		ctx->s_ctx.seamless_switch_hw_re_init_time_ns = 16000000;
	else
		ctx->s_ctx.seamless_switch_hw_re_init_time_ns = 4000000;

	DRV_LOG_MUST(ctx,
			"[%s] scen:%u(pclk:%llu) => scen:%u(pclk:%llu), hw_re_init_time_ns:%u\n",
			__func__,
			pre_seamless_scenario_id,
			ctx->s_ctx.mode[pre_seamless_scenario_id].pclk,
			scenario_id,
			ctx->s_ctx.mode[scenario_id].pclk,
			ctx->s_ctx.seamless_switch_hw_re_init_time_ns);
}


static int s5khp9sp_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	struct mtk_hdr_ae *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;
	u32 frame_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 exp_cnt = 0;
	enum SENSOR_SCENARIO_ID_ENUM pre_seamless_scenario_id = ctx->current_scenario_id;

	if (feature_data == NULL) {
		DRV_LOGE(ctx, "input scenario is null!");
		return ERROR_NONE;
	}
	scenario_id = *feature_data;
	if ((feature_data + 1) != NULL)
		ae_ctrl = (struct mtk_hdr_ae *)((uintptr_t)(*(feature_data + 1)));
	else
		DRV_LOGE(ctx, "no ae_ctrl input");

	check_current_scenario_id_bound(ctx);
	DRV_LOG(ctx, "E: set seamless switch %u %u\n", ctx->current_scenario_id, scenario_id);
	if (!ctx->extend_frame_length_en)
		DRV_LOGE(ctx, "please extend_frame_length before seamless_switch!\n");
	ctx->extend_frame_length_en = FALSE;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return ERROR_NONE;
	}
	if (ctx->s_ctx.mode[scenario_id].seamless_switch_group == 0 ||
		ctx->s_ctx.mode[scenario_id].seamless_switch_group !=
			ctx->s_ctx.mode[ctx->current_scenario_id].seamless_switch_group) {
		DRV_LOGE(ctx, "seamless_switch not supported\n");
		return ERROR_NONE;
	}
	if (ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table == NULL) {
		DRV_LOGE(ctx, "Please implement seamless_switch setting\n");
		return ERROR_NONE;
	}

	exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
	ctx->is_seamless = TRUE;

	subdrv_ixc_wr_u8(ctx, 0x0104, 0x01);

	if (ctx->s_ctx.reg_addr_fast_mode_in_lbmf &&
		(ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_LBMF ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF))
		subdrv_ixc_wr_u8(ctx, ctx->s_ctx.reg_addr_fast_mode_in_lbmf, 0x4);

	update_mode_info_seamless_switch(ctx, scenario_id);
	ixc_table_write(ctx,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);

	if (ae_ctrl) {
		switch (ctx->s_ctx.mode[scenario_id].hdr_mode) {
		case HDR_RAW_STAGGER:
			set_multi_shutter_frame_length(ctx, (u64 *)&ae_ctrl->exposure, exp_cnt, 0);
			set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		case HDR_RAW_LBMF:
			set_multi_shutter_frame_length_in_lut(ctx,
				(u64 *)&ae_ctrl->exposure, exp_cnt, 0, frame_length_in_lut);
			set_multi_gain_in_lut(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		case HDR_RAW_DCG_RAW:
			set_shutter(ctx, ae_ctrl->exposure.le_exposure);
			if (ctx->s_ctx.mode[scenario_id].dcg_info.dcg_gain_mode
				== IMGSENSOR_DCG_DIRECT_MODE)
				set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			else
				set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		default:
			set_shutter(ctx, ae_ctrl->exposure.le_exposure);
			set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		}
	}
	commit_i2c_buffer(ctx);
	subdrv_ixc_wr_u8(ctx, 0x0104, 0x00);

	ctx->fast_mode_on = TRUE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->is_seamless = FALSE;
	seamless_switch_update_hw_re_init_time(ctx, pre_seamless_scenario_id, scenario_id);
	DRV_LOG(ctx, "X: set seamless switch done\n");
	return ERROR_NONE;
}

static void set_group_hold(void *arg, u8 en)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	if (en)
		set_i2c_buffer(ctx, 0x0104, 0x01);
	else
		set_i2c_buffer(ctx, 0x0104, 0x00);
}

static u16 get_gain2reg(u32 gain)
{
	return gain * 32 / BASEGAIN;
}

static int s5khp9sp_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	if (mode) {
		if (mode == 5) {
			DRV_LOG(ctx, "use Solid Color replace Black\n");
			mode = 1;
		}
		subdrv_ixc_wr_u16(ctx, 0x0600, mode); /*100% Color bar*/
	} else if (ctx->test_pattern)
		subdrv_ixc_wr_u16(ctx, 0x0600, 0x0000); /*No pattern*/

	ctx->test_pattern = mode;
	return ERROR_NONE;
}

static int s5khp9sp_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	struct mtk_test_pattern_data *data = (struct mtk_test_pattern_data *)para;
	u16 R = (data->Channel_R >> 22) & 0x3ff;
	u16 Gr = (data->Channel_Gr >> 22) & 0x3ff;
	u16 Gb = (data->Channel_Gb >> 22) & 0x3ff;
	u16 B = (data->Channel_B >> 22) & 0x3ff;

	subdrv_ixc_wr_u16(ctx, 0x0602, Gr);
	subdrv_ixc_wr_u16(ctx, 0x0604, R);
	subdrv_ixc_wr_u16(ctx, 0x0606, B);
	subdrv_ixc_wr_u16(ctx, 0x0608, Gb);

	DRV_LOG(ctx, "mode(%u) R/Gr/Gb/B = 0x%04x/0x%04x/0x%04x/0x%04x\n",
		ctx->test_pattern, R, Gr, Gb, B);
	return ERROR_NONE;
}

static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt, u64 sof_ts)
{
	DRV_LOG(ctx, "sof_cnt(%u) ctx->ref_sof_cnt(%u) ctx->fast_mode_on(%d)",
		sof_cnt, ctx->ref_sof_cnt, ctx->fast_mode_on);
	if (ctx->fast_mode_on && (sof_cnt > ctx->ref_sof_cnt)) {
		ctx->fast_mode_on = FALSE;
		ctx->ref_sof_cnt = 0;
		DRV_LOG(ctx, "seamless_switch disabled.");
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x00);
		commit_i2c_buffer(ctx);
	}
	return 0;
}

static int s5khp9sp_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	struct SET_SENSOR_AWB_GAIN *awb_gain = (struct SET_SENSOR_AWB_GAIN *)para;

	subdrv_ixc_wr_u16(ctx, 0x0D82,
		awb_gain->ABS_GAIN_R * 2); /* red 1024(1x) */
	subdrv_ixc_wr_u16(ctx, 0x0D86,
		awb_gain->ABS_GAIN_B * 2); /* blue */

	DRV_LOG(ctx, "[test] ABS_GAIN_GR(%d) ABS_GAIN_R(%d) ABS_GAIN_B(%d) ABS_GAIN_GB(%d)",
			awb_gain->ABS_GAIN_GR,
			awb_gain->ABS_GAIN_R,
			awb_gain->ABS_GAIN_B,
			awb_gain->ABS_GAIN_GB);
	DRV_LOG(ctx, "[test] 0x0D82(red) = (0x%x)", subdrv_ixc_rd_u16(ctx, 0x0D82));
	DRV_LOG(ctx, "[test] 0x0D84(green) = (0x%x)", subdrv_ixc_rd_u16(ctx, 0x0D84));
	DRV_LOG(ctx, "[test] 0x0D86(blue) = (0x%x)", subdrv_ixc_rd_u16(ctx, 0x0D86));
	return 0;
}

static int s5khp9sp_i3c_pre_config(struct subdrv_ctx *ctx)
{
	int ret = 0;

	if ((ctx->ixc_client.protocol == I3C_PROTOCOL)
		&& (ctx->i2c_vir_client.i2c_dev)) {
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0xFCFC, 0x4000);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x0000, 0x0001);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x0000, 0x1B73);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x6012, 0x0001);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x7002, 0x1008);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x6066, 0x0216);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x6068, 0x1B73);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x6014, 0x0001);
		mdelay(20);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x6094, 0x0000);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x6062, 0x0000);
		if (ret) {
			DRV_LOGE(ctx, "fail. ret=%d\n", ret);
			return ERROR_SENSOR_CONNECT_FAIL;
		}
		DRV_LOG_MUST(ctx, "success. ret=%d, setting_len: %d\n",
				ret, ctx->s_ctx.i3c_precfg_setting_len);
	}
	return ERROR_NONE;
} /* pre_config */

static int s5khp9sp_set_ctrl_locker(struct subdrv_ctx *ctx,
		u32 cid, bool *is_lock)
{
	bool lock_set_ctrl = false;
	u32 lock_f =
			(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF)
			? 2 : 1;

	if (unlikely(is_lock == NULL)) {
		pr_info("[%s][ERROR] is_lock %p is NULL\n", __func__, is_lock);
		return -EINVAL;
	}

	switch (cid) {
	case V4L2_CID_MTK_STAGGER_AE_CTRL:
	case V4L2_CID_MTK_MAX_FPS:

		if ((ctx->sof_no < lock_f) && (ctx->is_streaming)) {
			lock_set_ctrl = true;
			DRV_LOG(ctx,
				"[%s] Target lock cid(%u) lock_set_ctrl(%d), sof_no(%d) is_streaming(%d), lock_f(%d)\n",
				__func__,
				cid,
				lock_set_ctrl,
				ctx->sof_no,
				ctx->is_streaming,
				lock_f);
		}
		break;
	default:
		break;
	}

	*is_lock = lock_set_ctrl;
	return ERROR_NONE;
} /* s5khp9sp_set_ctrl_locker */

static int s5khp9sp_chk_streaming_st(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u64 cur_time = 0;
	u64 start_time = ctx->stream_ctrl_start_time;
	u32 cur_fc = 0xff;
	u64 timeout = 1000000000/(u64)ctx->current_fps*10;
	int ret = ERROR_NONE;

	subdrv_ixc_wr_u16(ctx, 0xFCFC, 0x4000);
	while (1) {
		cur_time = ktime_get_boottime_ns();
		cur_fc = subdrv_ixc_rd_u8(ctx, 0x0005);
		if ((cur_fc > 0 && cur_fc != 0xff) || (cur_time - start_time > timeout))
			break;
		DRV_LOG(ctx, "current_fps: %d, timeout: %lld, cur_fc: %d, cur_time: %lld, start_time: %lld\n",
			ctx->current_fps, timeout, cur_fc, cur_time, start_time);
		mdelay(1);
	}
	return ret;
}
