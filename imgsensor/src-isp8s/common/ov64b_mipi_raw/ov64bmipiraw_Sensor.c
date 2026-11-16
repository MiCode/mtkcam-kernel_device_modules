// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 ov64bmipiraw_Sensor.c
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
#include "ov64bmipiraw_Sensor.h"

static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static void ov64b_set_dummy(struct subdrv_ctx *ctx);
static int ov64b_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static u16 get_gain2reg(u32 gain);
static int ov64b_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int  ov64b_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt, u64 sof_ts);

/* STRUCT */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, ov64b_set_test_pattern},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, ov64b_seamless_switch},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO, ov64b_set_max_framerate_by_scenario},
};

static struct subdrv_static_ctx_ext_ops static_ext_ops = {
	.i2c_addr_table = {0x44, 0xFF}, // TBD
	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.s_cali = set_sensor_cali,
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),

#ifdef OV64B_ISF_DBG
	.debug_check_with_exist_s_ctx = &ov64b_legacy_s_ctx,
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
	.get_temp = common_get_temp,
	.get_csi_param = common_get_csi_param,
	.update_sof_cnt = common_update_sof_cnt,
	.vsync_notify = vsync_notify,
};

const struct subdrv_entry ov64b_mipi_raw_entry = {
	.name = "ov64b_mipi_raw",
	.ops = &ops,
	.is_fw_support = TRUE,
	.fw_ext_ops = &static_ext_ops,
};

/* FUNCTION */

static void set_sensor_cali(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	u16 idx = 0;
	u8 support = FALSE;
	u8 *pbuf = NULL;
	u16 size = 0;
	u16 addr = 0;
	struct eeprom_info_struct *info = ctx->s_ctx.eeprom_info;

	if (!probe_eeprom(ctx))
		return;

	idx = ctx->eeprom_index;

	/* PDC data */
	support = info[idx].pdc_support;
	if (support) {
		pbuf = info[idx].preload_pdc_table;
		if (pbuf != NULL) {
			size = 720;
			addr = 0x5F80;
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set PDC calibration data done.");
		}
	}

	/* xtalk data */
	support = info[idx].xtalk_support;
	if (support) {
		pbuf = info[idx].preload_xtalk_table;
		if (pbuf != NULL) {
			size = 288;
			addr = 0x5A40;
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set xtalk calibration data done.");
		}
	}
}

static int get_sensor_temperature(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	int temperature = 0;

	/*TEMP_SEN_CTL */
	subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_temp_en, 0x01);
	temperature = subdrv_i2c_rd_u8(ctx, ctx->s_ctx.reg_addr_temp_read);
	temperature = (temperature > 0xC0) ? (temperature - 0x100) : temperature;

	DRV_LOG(ctx, "temperature: %d degrees\n", temperature);
	return temperature;
}

static void set_group_hold(void *arg, u8 en)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	if (en) {
		set_i2c_buffer(ctx, 0x3208, 0x00);
	} else {
		set_i2c_buffer(ctx, 0x3208, 0x10);
		set_i2c_buffer(ctx, 0x3208, 0xA0);
	}
}

static void ov64b_set_dummy(struct subdrv_ctx *ctx)
{
	// bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 1);
	write_frame_length(ctx, ctx->frame_length);
	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 0);

	commit_i2c_buffer(ctx);
}

static int ov64b_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *)para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = (enum SENSOR_SCENARIO_ID_ENUM)*feature_data;
	u32 framerate = *(feature_data + 1);
	u32 frame_length, calc_fl, exp_cnt, i;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}

	if (framerate == 0) {
		DRV_LOG(ctx, "framerate should not be 0\n");
		return ERROR_NONE;
	}

	if (ctx->s_ctx.mode[scenario_id].linelength == 0) {
		DRV_LOG(ctx, "linelength should not be 0\n");
		return ERROR_NONE;
	}

	if (ctx->line_length == 0) {
		DRV_LOG(ctx, "ctx->line_length should not be 0\n");
		return ERROR_NONE;
	}

	if (ctx->frame_length == 0) {
		DRV_LOG(ctx, "ctx->frame_length should not be 0\n");
		return ERROR_NONE;
	}
	exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
	calc_fl = ctx->exposure[0];
	for (i = 1; i < exp_cnt; i++)
		calc_fl += ctx->exposure[i];
	calc_fl += ctx->s_ctx.exposure_margin*exp_cnt*exp_cnt;

	frame_length = ctx->s_ctx.mode[scenario_id].pclk / framerate * 10
		/ ctx->s_ctx.mode[scenario_id].linelength;
	ctx->frame_length =
		max(frame_length, ctx->s_ctx.mode[scenario_id].framelength);
	ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);
	ctx->current_fps = ctx->s_ctx.mode[scenario_id].pclk / ctx->frame_length * 10
		/ ctx->s_ctx.mode[scenario_id].linelength;
	ctx->min_frame_length = ctx->frame_length;
	DRV_LOG(ctx, "max_fps(input/output):%u/%u(sid:%u), frame_length:%u, calc_fl:%u, min_fl_en:1\n",
		framerate, ctx->current_fps, scenario_id, ctx->frame_length, calc_fl);
	if (ctx->frame_length > calc_fl)
		ov64b_set_dummy(ctx);
	else
		ctx->frame_length = calc_fl;

	return ERROR_NONE;
}

static u16 get_gain2reg(u32 gain)
{
	return gain * 256 / BASEGAIN;
}

static int ov64b_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	enum IMGSENSOR_HDR_MODE_ENUM scen1_hdr, scen2_hdr;
	struct mtk_hdr_ae *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;
	u32 exp_cnt = 0;

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

	scen1_hdr = ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode;
	scen2_hdr = ctx->s_ctx.mode[scenario_id].hdr_mode;
	exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
	ctx->is_seamless = TRUE;
	update_mode_info_seamless_switch(ctx, scenario_id);

	set_table_to_buffer(ctx, isf_addr_data_pair_seamless_switch_step1_ov64b,
		ARRAY_SIZE(isf_addr_data_pair_seamless_switch_step1_ov64b));
	set_table_to_buffer(ctx,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);

	ctx->ae_ctrl_gph_en = 1;
	if (ae_ctrl) {
		switch (ctx->s_ctx.mode[scenario_id].hdr_mode) {
		case HDR_RAW_STAGGER:
			set_multi_shutter_frame_length(ctx, (u64 *)&ae_ctrl->exposure, exp_cnt, 0);
			set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		default:
			set_shutter(ctx, ae_ctrl->exposure.le_exposure);
			set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		}
	}
	ctx->ae_ctrl_gph_en = 0;

	set_table_to_buffer(ctx, isf_addr_data_pair_seamless_switch_step2_ov64b,
		ARRAY_SIZE(isf_addr_data_pair_seamless_switch_step2_ov64b));
	if (scen1_hdr == HDR_RAW_STAGGER) {
		set_table_to_buffer(ctx, isf_addr_data_pair_seamless_switch_step3_HDR_ov64b,
			ARRAY_SIZE(isf_addr_data_pair_seamless_switch_step3_HDR_ov64b));
			DRV_LOG(ctx, "do hdr to linear mode\n");
	} else {
		set_table_to_buffer(ctx, isf_addr_data_pair_seamless_switch_step3_ov64b,
			ARRAY_SIZE(isf_addr_data_pair_seamless_switch_step3_ov64b));
			DRV_LOG(ctx, "do linear to hdr/linear mode\n");
	}
	commit_i2c_buffer(ctx);

	ctx->is_seamless = FALSE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->fast_mode_on = TRUE;
	DRV_LOG(ctx, "X: set seamless switch done\n");
	return ERROR_NONE;
}

static int ov64b_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOGE(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	switch (mode) {
	case 2:
		subdrv_i2c_wr_u8(ctx, 0x50c1, 0x01);
		break;
	case 5:
		subdrv_i2c_wr_u8(ctx, 0x350a, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x401a, 0x00);
		break;
	default:
		break;
	}

	if (mode != ctx->test_pattern)
		switch (ctx->test_pattern) {
		case 2:
			subdrv_i2c_wr_u8(ctx, 0x50c1, 0x00);
			break;
		case 5:
			subdrv_i2c_wr_u8(ctx, 0x350a, 0x01);
			subdrv_i2c_wr_u8(ctx, 0x401a, 0x40);
			break;
		default:
			break;
		}

	ctx->test_pattern = mode;
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
	}
	return 0;
}
