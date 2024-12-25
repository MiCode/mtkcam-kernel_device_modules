// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 rothkoimx355ultramipiraw_Sensor.c
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
#include "rothkoimx355ultramipiraw_Sensor.h"

static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static void rothkoimx355ultra_set_dummy(struct subdrv_ctx *ctx);
static int rothkoimx355ultra_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static u16 get_gain2reg(u32 gain);

static int rothkoimx355ultra_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int ultra_vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt);
static int rothkoimx355ultra_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkoimx355ultra_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkoimx355ultra_set_multi_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkoimx355ultra_get_csi_param(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id, struct mtk_csi_param *csi_param);

static int longexposure_times = 0;
static int long_exposure_status;

/* STRUCT */
static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, rothkoimx355ultra_set_test_pattern},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO, rothkoimx355ultra_set_max_framerate_by_scenario},
	{SENSOR_FEATURE_SET_ESHUTTER, rothkoimx355ultra_set_shutter},
	{SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME, rothkoimx355ultra_set_shutter_frame_length},
	{SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME, rothkoimx355ultra_set_multi_shutter_frame_length}
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		// .header_id = 0x0,
		// .addr_header_id = 0x0000000B,
		.i2c_write_id = 0xA0,

		.pdc_support = TRUE,
		.pdc_size = 720,
		.addr_pdc = 0x12D2,
		.sensor_reg_addr_pdc = 0x5F80,

	},
};

// mode 0: 3280x2464@30fps, normal preview
static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3280,
			.vsize = 2464,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
// mode 2: 3280*1840@30fps, noraml video
static struct mtk_mbus_frame_desc_entry frame_desc_video[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3280,
			.vsize = 1840,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
// mode 6: 2880*2160@24fps bokeh
static struct mtk_mbus_frame_desc_entry frame_desc_bokeh[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2880,
			.vsize = 2160,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

// mode 0: 3280x2464@30fps, normal preview
#define preview_mode_struct \
{\
	.frame_desc = frame_desc_prev,\
	.num_entries = ARRAY_SIZE(frame_desc_prev),\
	.mode_setting_table = rothkoimx355ultra_preview_setting,\
	.mode_setting_len = ARRAY_SIZE(rothkoimx355ultra_preview_setting),\
	.seamless_switch_group = PARAM_UNDEFINED,\
	.seamless_switch_mode_setting_table = PARAM_UNDEFINED,\
	.seamless_switch_mode_setting_len = PARAM_UNDEFINED,\
	.hdr_mode = HDR_NONE,\
	.raw_cnt = 1,\
	.exp_cnt = 1,\
	.pclk = 288000000,\
	.linelength = 3672,\
	.framelength = 2614,\
	.max_framerate = 300,\
	.mipi_pixel_rate = 288000000,\
	.readout_length = 0,\
	.read_margin = 10,\
	.framelength_step = 2,\
	.imgsensor_winsize_info = {\
		.full_w = 3280,\
		.full_h = 2464,\
		.x0_offset = 0,\
		.y0_offset = 0,\
		.w0_size = 3280,\
		.h0_size = 2464,\
		.scale_w = 3280,\
		.scale_h = 2464,\
		.x1_offset = 0,\
		.y1_offset = 0,\
		.w1_size = 3280,\
		.h1_size = 2464,\
		.x2_tg_offset = 0,\
		.y2_tg_offset = 0,\
		.w2_tg_size = 3280,\
		.h2_tg_size = 2464,\
	},\
	.pdaf_cap = FALSE,\
	.imgsensor_pd_info = PARAM_UNDEFINED,\
	.ae_binning_ratio = 1000,\
	.fine_integ_line = 0,\
	.delay_frame = 2,\
}
// mode 2: 3280*1840@30fps, noraml video
#define normal_video_mode_struct  \
{\
	.frame_desc = frame_desc_video,\
	.num_entries = ARRAY_SIZE(frame_desc_video),\
	.mode_setting_table = rothkoimx355ultra_normal_video_setting,\
	.mode_setting_len = ARRAY_SIZE(rothkoimx355ultra_normal_video_setting),\
	.seamless_switch_group = PARAM_UNDEFINED,\
	.seamless_switch_mode_setting_table = PARAM_UNDEFINED,\
	.seamless_switch_mode_setting_len = PARAM_UNDEFINED,\
	.hdr_mode = HDR_NONE,\
	.raw_cnt = 1,\
	.exp_cnt = 1,\
	.pclk = 288000000,\
	.linelength = 3672,\
	.framelength = 2614,\
	.max_framerate = 300,\
	.mipi_pixel_rate = 288000000,\
	.readout_length = 0,\
	.read_margin = 10,\
	.framelength_step = 2,\
	.imgsensor_winsize_info = {\
		.full_w = 3280,\
		.full_h = 2464,\
		.x0_offset = 0,\
		.y0_offset = 312,\
		.w0_size = 3280,\
		.h0_size = 1840,\
		.scale_w = 3280,\
		.scale_h = 1840,\
		.x1_offset = 0,\
		.y1_offset = 0,\
		.w1_size = 3280,\
		.h1_size = 1840,\
		.x2_tg_offset = 0,\
		.y2_tg_offset = 0,\
		.w2_tg_size = 3280,\
		.h2_tg_size = 1840,\
	},\
	.pdaf_cap = FALSE,\
	.imgsensor_pd_info = PARAM_UNDEFINED,\
	.ae_binning_ratio = 1000,\
	.fine_integ_line = 0,\
	.delay_frame = 2,\
}
// mode 6: 2880*2160@24fps bokeh
#define bokeh_mode_struct  \
{\
	.frame_desc = frame_desc_bokeh,\
	.num_entries = ARRAY_SIZE(frame_desc_bokeh),\
	.mode_setting_table = rothkoimx355ultra_custom2_setting,\
	.mode_setting_len = ARRAY_SIZE(rothkoimx355ultra_custom2_setting),\
	.seamless_switch_group = PARAM_UNDEFINED,\
	.seamless_switch_mode_setting_table = PARAM_UNDEFINED,\
	.seamless_switch_mode_setting_len = PARAM_UNDEFINED,\
	.hdr_mode = HDR_NONE,\
	.raw_cnt = 1,\
	.exp_cnt = 1,\
	.pclk = 288000000,\
	.linelength = 3672,\
	.framelength = 3266,\
	.max_framerate = 240,\
	.mipi_pixel_rate = 288000000,\
	.readout_length = 0,\
	.read_margin = 10,\
	.framelength_step = 2,\
	.imgsensor_winsize_info = {\
		.full_w = 3280,\
		.full_h = 2464,\
		.x0_offset = 200,\
		.y0_offset = 152,\
		.w0_size = 2880,\
		.h0_size = 2160,\
		.scale_w = 2880,\
		.scale_h = 2160,\
		.x1_offset = 0,\
		.y1_offset = 0,\
		.w1_size = 2880,\
		.h1_size = 2160,\
		.x2_tg_offset = 0,\
		.y2_tg_offset = 0,\
		.w2_tg_size = 2880,\
		.h2_tg_size = 2160,\
	},\
	.pdaf_cap = FALSE,\
	.imgsensor_pd_info = PARAM_UNDEFINED,\
	.ae_binning_ratio = 1000,\
	.fine_integ_line = 0,\
	.delay_frame = 2,\
}
static struct subdrv_mode_struct mode_struct[] = {
	preview_mode_struct,		//mode 0
	preview_mode_struct,		//mode 1
	normal_video_mode_struct,	//mode 2
	preview_mode_struct,		//mode 3
	preview_mode_struct,		//mode 4
	preview_mode_struct,		//mode 5
	bokeh_mode_struct,			//mode 6
	bokeh_mode_struct			//mode 7 leica authentic Mode
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = ROTHKOIMX355ULTRA_SENSOR_ID,
	.reg_addr_sensor_id = {0x0016, 0x0017},
	.i2c_addr_table = {0x34, 0xFF}, // TBD
	.i2c_burst_write_support = FALSE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {3280, 2464},
	.mirror = IMAGE_NORMAL, // TBD

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.dig_gain_min = BASEGAIN * 1,
	.dig_gain_max = BASEGAIN * 16,
	.dig_gain_step = 4, //1024/256
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 16,
	.ana_gain_type = 1,
	.ana_gain_step = 1,
	.ana_gain_table = rothkoimx355ultra_ana_gain_table,
	.ana_gain_table_size = sizeof(rothkoimx355ultra_ana_gain_table),
	.min_gain_iso = 50,
	.exposure_def = 0x3D0,
	.exposure_min = 8,
	.exposure_max = 0xFFFF * 128 - 10,
	.exposure_step = 2,
	.exposure_margin = 10,

	.frame_length_max = 0xFFFF * 128,
	.ae_effective_frame = 3,
	.frame_time_delay_frame = 3,
	.start_exposure_offset = 153800,

	.pdaf_type = PDAF_SUPPORT_NA,
	.hdr_type = HDR_SUPPORT_NA,
	.seamless_switch_support = FALSE,
	.temperature_support = TRUE,

	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = PARAM_UNDEFINED, // TBD
	.reg_addr_exposure = {
		{0x0202, 0x0203}
	},
	.long_exposure_support = FALSE,
	.reg_addr_exposure_lshift = PARAM_UNDEFINED,
	.reg_addr_ana_gain = {
		{0x0204, 0x0205}
	},
	.reg_addr_dig_gain = {
		{0x020E, 0x020F}
	},
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_temp_en = PARAM_UNDEFINED,
	.reg_addr_temp_read = 0x013A,
	.reg_addr_auto_extend = 0x0350,
	.reg_addr_frame_count = 0x0005,

	.init_setting_table = rothkoimx355ultra_init_setting,
	.init_setting_len = ARRAY_SIZE(rothkoimx355ultra_init_setting),
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
	.get_id = common_get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = common_open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = common_close,
	.get_frame_desc = common_get_frame_desc,
	.get_temp = common_get_temp,
	.get_csi_param = rothkoimx355ultra_get_csi_param,
	.update_sof_cnt = common_update_sof_cnt,
	.vsync_notify = ultra_vsync_notify,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST,   0,	   1},
	{HW_ID_DOVDD, 1800000, 1},
	{HW_ID_DVDD,  1,       1},
	{HW_ID_DVDD1, 1200000, 1},
	{HW_ID_AVDD,  2700000, 2},
	{HW_ID_MCLK,  24,	   0},
	{HW_ID_MCLK_DRIVING_CURRENT, 4, 2},
	{HW_ID_RST,   1,	   1},
};

const struct subdrv_entry rothkoimx355ultra_mipi_raw_entry = {
	.name = "rothkoimx355ultra_mipi_raw",
	.id = ROTHKOIMX355ULTRA_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

/* FUNCTION */
static int get_sensor_temperature(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	uint8_t temperature = 0;
	int32_t temperature_convert;

	temperature = subdrv_i2c_rd_u8(ctx, ctx->s_ctx.reg_addr_temp_read);
	if (temperature >= 0x0 && temperature <= 0x4F)
		temperature_convert = temperature;
	else if (temperature >= 0x50 && temperature <= 0x7F)
		temperature_convert = 80;
	else if (temperature >= 0x81 && temperature <= 0xEC)
		temperature_convert = -20;
	else
		temperature_convert = (uint8_t)temperature;

	DRV_LOG(ctx, "temperature: %d degrees\n", temperature_convert);
	return temperature_convert;
}

static void set_group_hold(void *arg, u8 en)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	if (en) {
		set_i2c_buffer(ctx, 0x0104, 0x01);
	} else {
		set_i2c_buffer(ctx, 0x0104, 0x00);
	}
}

static void rothkoimx355ultra_set_dummy(struct subdrv_ctx *ctx)
{
}

static int rothkoimx355ultra_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
		rothkoimx355ultra_set_dummy(ctx);

	return ERROR_NONE;
}

#define BASEGAIN_x_1024 1024 * BASEGAIN
static u16 get_gain2reg(u32 gain)
{
	u16 reg_gain = 0x0;
	u16 val = 0x0;

	if ((BASEGAIN_x_1024 % gain) > (gain >> 1)) {
		val = BASEGAIN_x_1024 / gain + 1;
	} else {
		val = BASEGAIN_x_1024 / gain;
	}
	reg_gain = 1024 - val;

	return reg_gain;
}

static int rothkoimx355ultra_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	if (mode){
		subdrv_i2c_wr_u8(ctx, 0x0600, 0);
		subdrv_i2c_wr_u8(ctx, 0x0601, 1);
	} else if (ctx->test_pattern)
		subdrv_i2c_wr_u8(ctx, 0x0601, 0x00); /*No pattern*/

	ctx->test_pattern = mode;
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

static int ultra_vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt)
{
	kal_uint16 sensor_output_cnt;

	sensor_output_cnt = subdrv_i2c_rd_u8(ctx, ctx->s_ctx.reg_addr_frame_count);
	DRV_LOG_MUST(ctx, "sensormode(%d) sof_cnt(%d) sensor_output_cnt(%d)\n",
		ctx->current_scenario_id, sof_cnt, sensor_output_cnt);
	return 0;
};

static int rothkoimx355ultra_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
	/* enable auto extend */
	if (ctx->s_ctx.reg_addr_auto_extend)
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_auto_extend, 0x01);

	/* long exposure */
	longexposure_times = 0;
	while (ctx->exposure[0] >= 65535) {
		ctx->exposure[0] = ctx->exposure[0] / 2;
		longexposure_times += 1;
	}
	if (longexposure_times > 0) {
		DRV_LOG_MUST(ctx, "enter long exposure mode, time is %d", longexposure_times);
		long_exposure_status = 1;
		ctx->frame_length = ctx->min_frame_length;
		set_i2c_buffer(ctx, 0x3060, longexposure_times & 0x07);
	} else if (long_exposure_status == 1) {
		long_exposure_status = 0;
		set_i2c_buffer(ctx, 0x3060, longexposure_times & 0x00);
		DRV_LOG_MUST(ctx, "exit long exposure mode");
	}
	/* write framelength */
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx,ctx->frame_length);
	/* write shutter */
	if (ctx->s_ctx.reg_addr_exposure[0].addr[2]) {
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[0],
			(ctx->exposure[0] >> 16) & 0xFF);
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[1],
			(ctx->exposure[0] >> 8) & 0xFF);
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[2],
			ctx->exposure[0] & 0xFF);
	} else {
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[0],
			(ctx->exposure[0] >> 8) & 0xFF);
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[1],
			ctx->exposure[0] & 0xFF);
	}
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

static int rothkoimx355ultra_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	rothkoimx355ultra_set_shutter_frame_length(ctx, para, len);
	return 0;
}
static int rothkoimx355ultra_set_multi_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	u64 *shutters = (u64 *)(*feature_data);
	u16 exp_cnt   = (u16)(*(feature_data + 1));
	u16 frame_length = (u16)(*(feature_data + 2));

	int i = 0;
	int fine_integ_line = 0;
	u16 last_exp_cnt = 1;
	u32 calc_fl[3] = {0};
	int readout_diff = 0;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	u32 rg_shutters[3] = {0};
	u32 cit_step = 0;

	ctx->frame_length = frame_length ? frame_length : ctx->min_frame_length;
	if (exp_cnt > ARRAY_SIZE(ctx->exposure)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->exposure));
		exp_cnt = ARRAY_SIZE(ctx->exposure);
	}
	check_current_scenario_id_bound(ctx);

	/* check boundary of shutter */
	for (i = 1; i < ARRAY_SIZE(ctx->exposure); i++)
		last_exp_cnt += ctx->exposure[i] ? 1 : 0;
	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;
	cit_step = ctx->s_ctx.mode[ctx->current_scenario_id].coarse_integ_step;
	for (i = 0; i < exp_cnt; i++) {
		shutters[i] = FINE_INTEG_CONVERT(shutters[i], fine_integ_line);
		shutters[i] = max_t(u64, shutters[i],
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[i].min);
		shutters[i] = min_t(u64, shutters[i],
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[i].max);
		if (cit_step)
			shutters[i] = round_up(shutters[i], cit_step);
	}

	/* check boundary of framelength */
	/* - (1) previous se + previous me + current le */
	calc_fl[0] = (u32) shutters[0];
	for (i = 1; i < last_exp_cnt; i++)
		calc_fl[0] += ctx->exposure[i];
	calc_fl[0] += ctx->s_ctx.exposure_margin*exp_cnt*exp_cnt;

	/* - (2) current se + current me + current le */
	calc_fl[1] = (u32) shutters[0];
	for (i = 1; i < exp_cnt; i++)
		calc_fl[1] += (u32) shutters[i];
	calc_fl[1] += ctx->s_ctx.exposure_margin*exp_cnt*exp_cnt;

	/* - (3) readout time cannot be overlapped */
	calc_fl[2] =
		(ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
		ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
	if (last_exp_cnt == exp_cnt)
		for (i = 1; i < exp_cnt; i++) {
			readout_diff = ctx->exposure[i] - (u32) shutters[i];
			calc_fl[2] += readout_diff > 0 ? readout_diff : 0;
		}
	for (i = 0; i < ARRAY_SIZE(calc_fl); i++)
		ctx->frame_length = max(ctx->frame_length, calc_fl[i]);
	ctx->frame_length =	max(ctx->frame_length, ctx->min_frame_length);
	ctx->frame_length =	min(ctx->frame_length, ctx->s_ctx.frame_length_max);
	/* restore shutter */
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	for (i = 0; i < exp_cnt; i++)
		ctx->exposure[i] = (u32) shutters[i];
	/* group hold start */
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	/* enable auto extend */
	if (ctx->s_ctx.reg_addr_auto_extend)
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_auto_extend, 0x01);

	/* long exposure */
	longexposure_times = 0;
	while (shutters[0] >= 65535) {
		shutters[0] = shutters[0] / 2;
		longexposure_times += 1;
	}
	if (longexposure_times > 0) {
		DRV_LOG_MUST(ctx, "enter long exposure mode, time is %d", longexposure_times);
		long_exposure_status = 1;
		ctx->frame_length = ctx->min_frame_length;
		set_i2c_buffer(ctx, 0x3060, longexposure_times & 0x07);
	} else if (long_exposure_status == 1) {
		long_exposure_status = 0;
		set_i2c_buffer(ctx, 0x3060, longexposure_times & 0x00);
		DRV_LOG_MUST(ctx, "exit long exposure mode");
	}

	/* write framelength */
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx, ctx->frame_length);
	/* write shutter */
	switch (exp_cnt) {
	case 1:
		rg_shutters[0] = (u32) shutters[0] / exp_cnt;
		break;
	case 2:
		rg_shutters[0] = (u32) shutters[0] / exp_cnt;
		rg_shutters[2] = (u32) shutters[1] / exp_cnt;
		break;
	case 3:
		rg_shutters[0] = (u32) shutters[0] / exp_cnt;
		rg_shutters[1] = (u32) shutters[1] / exp_cnt;
		rg_shutters[2] = (u32) shutters[2] / exp_cnt;
		break;
	default:
		break;
	}
	if (ctx->s_ctx.reg_addr_exposure_lshift != PARAM_UNDEFINED) {
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure_lshift, 0);
		ctx->l_shift = 0;
	}
	for (i = 0; i < 3; i++) {
		if (rg_shutters[i]) {
			if (ctx->s_ctx.reg_addr_exposure[i].addr[2]) {
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[0],
					(rg_shutters[i] >> 16) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[1],
					(rg_shutters[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[2],
					rg_shutters[i] & 0xFF);
			} else {
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[0],
					(rg_shutters[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[1],
					rg_shutters[i] & 0xFF);
			}
		}
	}
	DRV_LOG_MUST(ctx, "exp[0x%x/0x%x/0x%x], fll(input/output):%u/%u, flick_en:%d\n",
		rg_shutters[0], rg_shutters[1], rg_shutters[2],
		frame_length, ctx->frame_length, ctx->autoflicker_en);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */
	return 0;
}

static int rothkoimx355ultra_get_csi_param(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	struct mtk_csi_param *csi_param)
{
	DRV_LOG(ctx, "+ scenario_id:%u,aov_csi_clk:%u\n",scenario_id, ctx->aov_csi_clk);
	switch (scenario_id) {
	default:
		csi_param->legacy_phy = 0;
	    csi_param->not_fixed_trail_settle = 1;
		csi_param->not_fixed_dphy_settle = 1;
		csi_param->dphy_data_settle = 0x22;
		csi_param->dphy_clk_settle = 0x22;
		csi_param->dphy_trail = 0x10;
		break;
	}
	return 0;
}

