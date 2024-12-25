// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 rothkogc02m1macromipiraw_Sensor.c
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
#include "rothkogc02m1macromipiraw_Sensor.h"

static void rothkogc02m1macro_set_dummy(struct subdrv_ctx *ctx);
static int rothkogc02m1macro_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int  rothkogc02m1macro_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int macro_vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt);
static int rothkogc02m1macro_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkogc02m1macro_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkogc02m1macro_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkogc02m1macro_streamoff(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkogc02m1macro_streamon(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkogc02m1macro_streaming_control(struct subdrv_ctx *ctx, bool enable);
static int rothkogc02m1macro_close(struct subdrv_ctx *ctx);
static int rothkogc02m1macro_get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id);
static void rothkogc02m1macro_write_init_setting(void *arg);
static int rothkogc02m1macro_get_csi_param(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id, struct mtk_csi_param *csi_param);


/* STRUCT */
static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN,				rothkogc02m1macro_set_test_pattern},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO,	rothkogc02m1macro_set_max_framerate_by_scenario},
	{SENSOR_FEATURE_SET_GAIN,						rothkogc02m1macro_set_gain},
	{SENSOR_FEATURE_SET_ESHUTTER,					rothkogc02m1macro_set_shutter},
	{SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME,			rothkogc02m1macro_set_shutter_frame_length},
	{SENSOR_FEATURE_SET_STREAMING_SUSPEND,			rothkogc02m1macro_streamoff},
	{SENSOR_FEATURE_SET_STREAMING_RESUME,			rothkogc02m1macro_streamon}
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		// .header_id = 0x010B00FF,
		// .header_id = 0x0,
		// .addr_header_id = 0x0000000B,
		.i2c_write_id = 0xA4,

		.pdc_support = TRUE,
		.pdc_size = 720,
		.addr_pdc = 0x12D2,
		.sensor_reg_addr_pdc = 0x5F80,

	},
};

// mode 0: 1600x1200@30fps, normal preview
static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1600,
			.vsize = 1200,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

// mode 0: 1600x1200@30fps, normal preview
//static const struct subdrv_mode_struct preview_mode_struct =
#define preview_mode_struct \
{ \
	.frame_desc = frame_desc_prev, \
	.num_entries = ARRAY_SIZE(frame_desc_prev),\
	.mode_setting_table = rothkogc02m1macro_preview_setting,\
	.mode_setting_len = ARRAY_SIZE(rothkogc02m1macro_preview_setting),\
	.seamless_switch_group = PARAM_UNDEFINED,\
	.seamless_switch_mode_setting_table = PARAM_UNDEFINED,\
	.seamless_switch_mode_setting_len = PARAM_UNDEFINED,\
	.hdr_mode = HDR_NONE,\
	.raw_cnt = 1,\
	.exp_cnt = 1,\
	.pclk = 84000000,\
	.linelength = 2192,\
	.framelength = 1276,\
	.max_framerate = 300,\
	.mipi_pixel_rate = 67200000,\
	.readout_length = 0,\
	.read_margin = 10,\
	.framelength_step = 2,\
	.imgsensor_winsize_info = {\
		.full_w = 1600,\
		.full_h = 1200,\
		.x0_offset = 0,\
		.y0_offset = 0,\
		.w0_size = 1600,\
		.h0_size = 1200,\
		.scale_w = 1600,\
		.scale_h = 1200,\
		.x1_offset = 0,\
		.y1_offset = 0,\
		.w1_size = 1600,\
		.h1_size = 1200,\
		.x2_tg_offset = 0,\
		.y2_tg_offset = 0,\
		.w2_tg_size = 1600,\
		.h2_tg_size = 1200,\
	},\
	.pdaf_cap = FALSE,\
	.imgsensor_pd_info = PARAM_UNDEFINED,\
	.ae_binning_ratio = 1000,\
	.fine_integ_line = 0,\
	.delay_frame = 2,\
}
//};
static struct subdrv_mode_struct mode_struct[] = {
	preview_mode_struct,		//mode 0
	preview_mode_struct,		//mode 1
	preview_mode_struct,		//mode 2
	preview_mode_struct,		//mode 3
	preview_mode_struct,		//mode 4
	preview_mode_struct,		//mode 5
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = ROTHKOGC02M1MACRO_SENSOR_ID,
	.reg_addr_sensor_id = {0xf0, 0xf1},
	.i2c_addr_table = {0x6E, 0xFF}, // TBD
	.i2c_burst_write_support = FALSE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {1600, 1200},
	.mirror = IMAGE_NORMAL, // TBD

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_lane_num = SENSOR_MIPI_1_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 12,
	.ana_gain_type = 1,
	.ana_gain_step = 1,
	.ana_gain_table = rothkogc02m1macro_ana_gain_table,
	.ana_gain_table_size = sizeof(rothkogc02m1macro_ana_gain_table),
	.min_gain_iso = 50,
	.exposure_def = 0x3D0,
	.exposure_min = 8,
	.exposure_max = 0x3fff - 16,
	.exposure_step = 2,
	.exposure_margin = 16,

	.frame_length_max = 0x3fff,
	.ae_effective_frame = 3,
	.frame_time_delay_frame = 3,
	.start_exposure_offset = 153800,

	.pdaf_type = PDAF_SUPPORT_NA,
	.hdr_type = HDR_SUPPORT_NA,
	.seamless_switch_support = FALSE,
	.temperature_support = FALSE,

	.g_temp = PARAM_UNDEFINED,
	.g_gain2reg = PARAM_UNDEFINED,
	.s_gph = PARAM_UNDEFINED,

	.reg_addr_stream = 0x3e,
	.reg_addr_mirror_flip = PARAM_UNDEFINED, // TBD
	.reg_addr_exposure = {
		{0x03, 0x04}
	},
	.long_exposure_support = FALSE,
	.reg_addr_exposure_lshift = PARAM_UNDEFINED,
	.reg_addr_ana_gain = {
		{0xb1, 0xb2}
	},
	.reg_addr_frame_length = {0x41, 0x42},
	.reg_addr_temp_en = PARAM_UNDEFINED,
	.reg_addr_temp_read = PARAM_UNDEFINED,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = PARAM_UNDEFINED,

	.s_mi_init_setting = rothkogc02m1macro_write_init_setting,
	.init_setting_table = rothkogc02m1macro_init_setting,
	.init_setting_len = ARRAY_SIZE(rothkogc02m1macro_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 0,
	.chk_s_off_end = 0,

	//TBD
	.checksum_value = 0xAF3E324F,
};

static struct subdrv_ops ops = {
	.get_id = rothkogc02m1macro_get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = common_open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = rothkogc02m1macro_close,
	.get_frame_desc = common_get_frame_desc,
	.get_temp = common_get_temp,
	.get_csi_param = rothkogc02m1macro_get_csi_param,
	.update_sof_cnt = common_update_sof_cnt,
	.vsync_notify = macro_vsync_notify,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST,		0,		1},
	{HW_ID_DOVDD,	1800000,1},
	{HW_ID_AVDD,	2800000,2},
	{HW_ID_MCLK,	24,		0},
	{HW_ID_MCLK_DRIVING_CURRENT, 6, 1},
	{HW_ID_RST,		1,		5},
};

const struct subdrv_entry rothkogc02m1macro_mipi_raw_entry = {
	.name = "rothkogc02m1macro_mipi_raw",
	.id = ROTHKOGC02M1MACRO_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

static void rothkogc02m1macro_set_dummy(struct subdrv_ctx *ctx)
{
}

static int rothkogc02m1macro_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
		rothkogc02m1macro_set_dummy(ctx);

	return ERROR_NONE;
}

static int rothkogc02m1macro_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	return ERROR_NONE;
}

static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static int macro_vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt)
{
	DRV_LOG_MUST(ctx, "sensormode(%d) sof_cnt(%d)\n", ctx->current_scenario_id, sof_cnt);
	return 0;
};

static int rothkogc02m1macro_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 shutter = *((u64 *)para);
	u32 frame_length = 0;
	u32 fine_integ_line = 0;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	ctx->frame_length = frame_length ? frame_length : ctx->min_frame_length;
	check_current_scenario_id_bound(ctx);
	/* check boundary of shutter */
	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;
	shutter = FINE_INTEG_CONVERT(shutter, fine_integ_line);
	shutter = max_t(u64, shutter,
		(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[0].min);
	shutter = min_t(u64, shutter,
		(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[0].max);
	/* check boundary of framelength */
	ctx->frame_length = max((u32)shutter + ctx->s_ctx.exposure_margin, ctx->min_frame_length);
	ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);
	/* restore shutter */
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	ctx->exposure[0] = (u32) shutter;
	/* group hold start */
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	subdrv_i2c_wr_u8_u8(ctx, 0xfe, 0x00);
	/* write framelength */
	subdrv_i2c_wr_u8_u8(ctx,	0xfe, 0x00);
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend){
		write_frame_length(ctx,ctx->frame_length);
		subdrv_i2c_wr_u8_u8(ctx,	ctx->s_ctx.reg_addr_frame_length.addr[0], (ctx->frame_length >> 8) & 0x3f);
		subdrv_i2c_wr_u8_u8(ctx,	ctx->s_ctx.reg_addr_frame_length.addr[1], ctx->frame_length & 0xFF);
	}
	/* write shutter */
	subdrv_i2c_wr_u8_u8(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[0], (ctx->exposure[0] >> 8) & 0x3f);
	subdrv_i2c_wr_u8_u8(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[1], ctx->exposure[0] & 0xFF);

	DRV_LOG_MUST(ctx, "exp[0x%x], fll(input/output):%u/%u, flick_en:%d\n",
		ctx->exposure[0], frame_length, ctx->frame_length, ctx->autoflicker_en);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */
	return 0;
}

static int rothkogc02m1macro_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	rothkogc02m1macro_set_shutter_frame_length(ctx, para, len);
	return 0;
}

#define GC02M1_SENSOR_GAIN_MAX_VALID_INDEX  16
u16 GC02M1_AGC_Param[GC02M1_SENSOR_GAIN_MAX_VALID_INDEX][2] = {
	{  1024,  0 },
	{  1536,  1 },
	{  2035,  2 },
	{  2519,  3 },
	{  3165,  4 },
	{  3626,  5 },
	{  4147,  6 },
	{  4593,  7 },
	{  5095,  8 },
	{  5697,  9 },
	{  6270, 10 },
	{  6714, 11 },
	{  7210, 12 },
	{  7686, 13 },
	{  8214, 14 },
	{ 10337, 15 },
};
static int rothkogc02m1macro_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	u32 gain = *feature_data;

	u16 reg_gain_part1, reg_gain_part2, gain_index;
	if (gain < ctx->s_ctx.ana_gain_min)
		gain = ctx->s_ctx.ana_gain_min;
	else if (gain > ctx->s_ctx.ana_gain_max)
		gain = ctx->s_ctx.ana_gain_max;

	for (gain_index = GC02M1_SENSOR_GAIN_MAX_VALID_INDEX - 1; gain_index >= 0; gain_index--)
		if (gain >= GC02M1_AGC_Param[gain_index][0])
			break;
	reg_gain_part1 = GC02M1_AGC_Param[gain_index][1];
	reg_gain_part2 = gain * BASE_DGAIN / GC02M1_AGC_Param[gain_index][0];

	subdrv_i2c_wr_u8_u8(ctx, 0xfe, 0x00);
	subdrv_i2c_wr_u8_u8(ctx, 0xb6, reg_gain_part1);
	subdrv_i2c_wr_u8_u8(ctx, 0xb1, (reg_gain_part2>>8) & 0x1F);
	subdrv_i2c_wr_u8_u8(ctx, 0xb2, reg_gain_part2 & 0xFF);
	DRV_LOG_MUST(ctx, "gain = %d, reg_gain_part1 = %d, reg_gain_part2 = %d", gain, reg_gain_part1, reg_gain_part2);
	return 0;
}

static int  rothkogc02m1macro_streamon(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	return rothkogc02m1macro_streaming_control(ctx, TRUE);
}
static int rothkogc02m1macro_streamoff(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	return rothkogc02m1macro_streaming_control(ctx, FALSE);
}
static int  rothkogc02m1macro_streaming_control(struct subdrv_ctx *ctx, bool enable)
{
	u64 stream_ctrl_delay_timing = 0;

	DRV_LOG(ctx, "E! enable:%u\n", enable);
	check_current_scenario_id_bound(ctx);

	subdrv_i2c_wr_u8_u8(ctx, 0xfe, 0x00);
	if (enable) {
		subdrv_i2c_wr_u8_u8(ctx, ctx->s_ctx.reg_addr_stream, 0x90);
		ctx->stream_ctrl_start_time = ktime_get_boottime_ns();
	} else {
		ctx->stream_ctrl_end_time = ktime_get_boottime_ns();
		if (ctx->s_ctx.custom_stream_ctrl_delay &&
			ctx->stream_ctrl_start_time && ctx->stream_ctrl_end_time) {
			stream_ctrl_delay_timing =
				(ctx->stream_ctrl_end_time - ctx->stream_ctrl_start_time) / 1000000;
			DRV_LOG(ctx,
				"custom_stream_ctrl_delay/stream_ctrl_delay_timing:%llu/%llu\n",
				ctx->s_ctx.custom_stream_ctrl_delay,
				stream_ctrl_delay_timing);
			if (stream_ctrl_delay_timing < ctx->s_ctx.custom_stream_ctrl_delay)
				mdelay(
					ctx->s_ctx.custom_stream_ctrl_delay - stream_ctrl_delay_timing);
		}
		subdrv_i2c_wr_u8_u8(ctx, ctx->s_ctx.reg_addr_stream, 0x00);
		memset(ctx->exposure, 0, sizeof(ctx->exposure));
		memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
		ctx->autoflicker_en = FALSE;
		ctx->extend_frame_length_en = 0;
		ctx->is_seamless = 0;
		if (ctx->s_ctx.chk_s_off_end)
			check_stream_off(ctx);
		ctx->stream_ctrl_start_time = 0;
		ctx->stream_ctrl_end_time = 0;
	}
	subdrv_i2c_wr_u8_u8(ctx, 0xfe, 0x00);
	ctx->sof_no = 0;
	ctx->is_streaming = enable;
	DRV_LOG_MUST(ctx, "X! enable:%u\n", enable);
	return 0;
}

static int rothkogc02m1macro_close(struct subdrv_ctx *ctx)
{
	rothkogc02m1macro_streaming_control(ctx, FALSE);
	DRV_LOG_MUST(ctx, "rothkogc02m1macro_close\n");
	return ERROR_NONE;
}
static int rothkogc02m1macro_get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id)
{
	u8 i = 0;
	u8 retry = 2;
	u32 addr_h = ctx->s_ctx.reg_addr_sensor_id.addr[0];
	u32 addr_l = ctx->s_ctx.reg_addr_sensor_id.addr[1];
	u32 addr_ll = ctx->s_ctx.reg_addr_sensor_id.addr[2];

	while (ctx->s_ctx.i2c_addr_table[i] != 0xFF) {
		ctx->i2c_write_id = ctx->s_ctx.i2c_addr_table[i];
		do {
			*sensor_id = (subdrv_i2c_rd_u8_u8(ctx, addr_h) << 8) |
				subdrv_i2c_rd_u8_u8(ctx, addr_l);
			if (addr_ll)
				*sensor_id = ((*sensor_id) << 8) | subdrv_i2c_rd_u8_u8(ctx, addr_ll);
			DRV_LOG_MUST(ctx, "i2c_write_id:0x%x sensor_id(cur/exp):0x%x/0x%x\n",
				ctx->i2c_write_id, *sensor_id, ctx->s_ctx.sensor_id);

			if (*sensor_id == ctx->s_ctx.sensor_id)
				return ERROR_NONE;
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != ctx->s_ctx.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}
static void rothkogc02m1macro_write_init_setting(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	int i = 0;
	u16 *plist = ctx->s_ctx.init_setting_table;
	u16 len = ctx->s_ctx.init_setting_len;
	for (i = 0; i < len; i += 2) {
		subdrv_i2c_wr_u8_u8(ctx, plist[i], plist[i+1]&0xff);
	}
	DRV_LOG_MUST(ctx, "rothkogc02m1macro_write_init_setting");
}
static int rothkogc02m1macro_get_csi_param(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	struct mtk_csi_param *csi_param)
{
	DRV_LOG(ctx, "+ scenario_id:%u,aov_csi_clk:%u\n",scenario_id, ctx->aov_csi_clk);
	switch (scenario_id) {
	default:
		csi_param->legacy_phy = 0;
		csi_param->not_fixed_trail_settle = 1;
		csi_param->not_fixed_dphy_settle = 1;
		csi_param->dphy_data_settle = 0x28;
		csi_param->dphy_clk_settle = 0x28;
		csi_param->dphy_trail = 0x40;
		break;
	}
	return 0;
}

