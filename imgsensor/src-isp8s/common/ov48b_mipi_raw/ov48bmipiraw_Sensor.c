// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 ov48bmipiraw_Sensor.c
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
#include "ov48bmipiraw_Sensor.h"

static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static void ov48b_set_dummy(struct subdrv_ctx *ctx);
static int ov48b_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static u16 get_gain2reg(u32 gain);
static int ov48b_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int ov48b_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);

//static struct subdrv_static_ctx_mode_ext_ops mode_ext_ops[] = {
//};

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, ov48b_set_test_pattern},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, ov48b_seamless_switch},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO, ov48b_set_max_framerate_by_scenario},
};

static struct subdrv_static_ctx_ext_ops static_ext_ops = {
	.i2c_addr_table = {0x6D, 0x20, 0xFF},
	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.s_cali = set_sensor_cali,
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),

//	.mode_ext_ops_list = mode_ext_ops,
//	.mode_ext_ops_list_len = ARRAY_SIZE(mode_ext_ops),

#ifdef OV48B_ISF_DBG
	.debug_check_with_exist_s_ctx = &ov48b_legacy_s_ctx,
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
	.parse_ebd_line = common_parse_ebd_line,
};

const struct subdrv_entry ov48b_mipi_raw_entry = {
	.name = "ov48b_mipi_raw",
	.ops = &ops,
	.is_fw_support = TRUE,
	.fw_ext_ops = &static_ext_ops,
};

/* STRUCT */

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
			size = 8;
			addr = 0x5C0E;
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			pbuf += size;
			size = 720;
			addr = 0x5900;
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set PDC calibration data done.");
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

static void ov48b_set_dummy(struct subdrv_ctx *ctx)
{
	// bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 1);
	write_frame_length(ctx, ctx->frame_length);
	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 0);

	commit_i2c_buffer(ctx);
}

static int ov48b_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *)para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = (enum SENSOR_SCENARIO_ID_ENUM)*feature_data;
	u32 framerate = *(feature_data + 1);
	u32 frame_length;

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

	frame_length = ctx->s_ctx.mode[scenario_id].pclk / framerate * 10
		/ ctx->s_ctx.mode[scenario_id].linelength;
	ctx->frame_length =
		max(frame_length, ctx->s_ctx.mode[scenario_id].framelength);
	ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);
	ctx->current_fps = ctx->pclk / ctx->frame_length * 10 / ctx->line_length;
	ctx->min_frame_length = ctx->frame_length;
	DRV_LOG(ctx, "max_fps(input/output):%u/%u(sid:%u), min_fl_en:1\n",
		framerate, ctx->current_fps, scenario_id);
	if (ctx->frame_length > (ctx->exposure[0] + ctx->s_ctx.exposure_margin))
		ov48b_set_dummy(ctx);
	return ERROR_NONE;
}

static u16 get_gain2reg(u32 gain)
{
	return gain * 256 / BASEGAIN;
}

static int ov48b_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	u32 *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;

	if (feature_data == NULL) {
		DRV_LOGE(ctx, "input scenario is null!");
		return ERROR_NONE;
	}
	scenario_id = *feature_data;
	if ((feature_data + 1) != NULL)
		ae_ctrl = (u32 *)((uintptr_t)(*(feature_data + 1)));
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

	ctx->is_seamless = TRUE;
	update_mode_info_seamless_switch(ctx, scenario_id);

	set_table_to_buffer(ctx, isf_addr_data_pair_seamless_switch_step1_ov48b2q,
		ARRAY_SIZE(isf_addr_data_pair_seamless_switch_step1_ov48b2q));
	set_table_to_buffer(ctx,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);

	ctx->ae_ctrl_gph_en = 1;
	if (ae_ctrl) {
		set_shutter(ctx, ae_ctrl[0]);
		set_gain(ctx, ae_ctrl[5]);
	}
	set_table_to_buffer(ctx, isf_addr_data_pair_seamless_switch_step2_ov48b2q,
		ARRAY_SIZE(isf_addr_data_pair_seamless_switch_step2_ov48b2q));

	if (ae_ctrl) {
		set_shutter(ctx, ae_ctrl[10]);
		set_gain(ctx, ae_ctrl[15]);
	}
	ctx->ae_ctrl_gph_en = 0;

	set_table_to_buffer(ctx, isf_addr_data_pair_seamless_switch_step3_ov48b2q,
		ARRAY_SIZE(isf_addr_data_pair_seamless_switch_step3_ov48b2q));
	commit_i2c_buffer(ctx);


	ctx->is_seamless = FALSE;
	DRV_LOG(ctx, "X: set seamless switch done\n");
	return ERROR_NONE;
}

static int ov48b_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	switch (mode) {
	case 2:
		subdrv_i2c_wr_u8(ctx, 0x5000, 0x81);
		subdrv_i2c_wr_u8(ctx, 0x5001, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x5002, 0x92);
		subdrv_i2c_wr_u8(ctx, 0x5081, 0x01);
		break;
	case 5:
		subdrv_i2c_wr_u8(ctx, 0x3019, 0xF0);
		subdrv_i2c_wr_u8(ctx, 0x4308, 0x01);
		break;
	default:
		break;
	}

	if (mode != ctx->test_pattern)
		switch (ctx->test_pattern) {
		case 2:
			subdrv_i2c_wr_u8(ctx, 0x5000, 0xCB);
			subdrv_i2c_wr_u8(ctx, 0x5001, 0x43);
			subdrv_i2c_wr_u8(ctx, 0x5002, 0x9E);
			subdrv_i2c_wr_u8(ctx, 0x5081, 0x00);
			break;
		case 5:
			subdrv_i2c_wr_u8(ctx, 0x3019, 0xD2);
			subdrv_i2c_wr_u8(ctx, 0x4308, 0x00);
			break;
		default:
			break;
		}

	ctx->test_pattern = mode;
	return ERROR_NONE;
}

