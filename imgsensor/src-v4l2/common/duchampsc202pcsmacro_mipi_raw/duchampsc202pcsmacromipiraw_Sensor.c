// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 duchampsc202pcsmacromipiraw_Sensor.c
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
#include "duchampsc202pcsmacromipiraw_Sensor.h"

static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static void duchampsc202pcsmacro_set_dummy(struct subdrv_ctx *ctx);
static int duchampsc202pcsmacro_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int duchampsc202pcsmacro_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int duchampsc202pcsmacro_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int duchampsc202pcsmacro_stream_resume(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static u16 get_gain2reg(u32 gain);
static int  duchampsc202pcsmacro_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static void duchampsc202pcsmacro_set_mirror_flip(struct subdrv_ctx *ctx, u8 image_mirror);
static int get_csi_param(struct subdrv_ctx *ctx,	enum SENSOR_SCENARIO_ID_ENUM scenario_id,struct mtk_csi_param *csi_param);

#define DUCHAMPSC202PCSMACRO_SENSOR_GAIN_BASE             1024
#define DUCHAMPSC202PCSMACRO_SENSOR_GAIN_MAX              16384 //(16 * DUCHAMPSC202PCSMACRO_SENSOR_GAIN_BASE)
#define DUCHAMPSC202PCSMACRO_SENSOR_GAIN_MAX_VALID_INDEX  6

/* STRUCT */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, duchampsc202pcsmacro_set_test_pattern},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO, duchampsc202pcsmacro_set_max_framerate_by_scenario},
	{SENSOR_FEATURE_SET_GAIN, duchampsc202pcsmacro_set_gain},
	{SENSOR_FEATURE_SET_ESHUTTER, duchampsc202pcsmacro_set_shutter},
	{SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME, duchampsc202pcsmacro_set_shutter},
	{SENSOR_FEATURE_SET_STREAMING_RESUME,duchampsc202pcsmacro_stream_resume},
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		.header_id = 0x010B00FF,
		.addr_header_id = 0x0000000B,
		.i2c_write_id = 0xA4,

		.pdc_support = TRUE,
		.pdc_size = 720,
		.addr_pdc = 0x12D2,
		.sensor_reg_addr_pdc = 0x5F80,

		.xtalk_support = TRUE,
		.xtalk_size = 288,
		.addr_xtalk = 0x2a71,
		.sensor_reg_addr_xtalk = 0x5A40,
	},
};

// mode 0: 1600*1200@30fps, normal preview + non pd
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

// mode 1: same as preview mode
static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
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

// mode 2: 4624*2604@30fps, noraml video + PD 1136 x 648
static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1600,
			.vsize = 900,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

// mode 3: same as preview mode
static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
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

// mode 4: same as preview mode
static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
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

static struct subdrv_mode_struct mode_struct[] = {
	// mode 0: 1600*1200@30fps, normal preview + non pd
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = duchampsc202pcsmacro_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampsc202pcsmacro_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72000000,
		.linelength = 1920,
		.framelength = 1250,
		.max_framerate = 300,
		.mipi_pixel_rate = 72000000,
		.readout_length = 0,
		.read_margin = 10,
		.imgsensor_winsize_info = {
			.full_w = 1600,
			.full_h = 1200,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 1600,
			.h0_size = 1200,
			.scale_w = 1600,
			.scale_h = 1200,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1600,
			.h1_size = 1200,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1600,
			.h2_tg_size = 1200,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 10,
	},
	// mode 1: same as preview mode
	{
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = duchampsc202pcsmacro_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampsc202pcsmacro_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72000000,
		.linelength = 1920,
		.framelength = 1250,
		.max_framerate = 300,
		.mipi_pixel_rate = 72000000,
		.readout_length = 0,
		.read_margin = 10,
		.imgsensor_winsize_info = {
			.full_w = 1600,
			.full_h = 1200,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 1600,
			.h0_size = 1200,
			.scale_w = 1600,
			.scale_h = 1200,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1600,
			.h1_size = 1200,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1600,
			.h2_tg_size = 1200,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 10,
	},
	// mode 2: 1600*900@30fps, noraml video + non pd
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = duchampsc202pcsmacro_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampsc202pcsmacro_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72000000,
		.linelength = 1920,
		.framelength = 1250,
		.max_framerate = 300,
		.mipi_pixel_rate = 72000000,
		.readout_length = 0,
		.read_margin = 10,
		.imgsensor_winsize_info = {
			.full_w = 1600,
			.full_h = 1200,
			.x0_offset = 0,
			.y0_offset = 150,
			.w0_size = 1600,
			.h0_size =  900,
			.scale_w = 1600,
			.scale_h =  900,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1600,
			.h1_size =  900,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1600,
			.h2_tg_size =  900,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 10,
	},
	// mode 3: same as preview mode
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = duchampsc202pcsmacro_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampsc202pcsmacro_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72000000,
		.linelength = 1920,
		.framelength = 1250,
		.max_framerate = 300,
		.mipi_pixel_rate = 72000000,
		.readout_length = 0,
		.read_margin = 10,
		.imgsensor_winsize_info = {
			.full_w = 1600,
			.full_h = 1200,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 1600,
			.h0_size = 1200,
			.scale_w = 1600,
			.scale_h = 1200,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1600,
			.h1_size = 1200,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1600,
			.h2_tg_size = 1200,
		},	
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 10,
	},
	// mode 4: same as preview mode
	{
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table = duchampsc202pcsmacro_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampsc202pcsmacro_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 72000000,
		.linelength = 1920,
		.framelength = 1250,
		.max_framerate = 300,
		.mipi_pixel_rate = 72000000,
		.readout_length = 0,
		.read_margin = 10,
		.imgsensor_winsize_info = {
			.full_w = 1600,
			.full_h = 1200,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 1600,
			.h0_size = 1200,
			.scale_w = 1600,
			.scale_h = 1200,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1600,
			.h1_size = 1200,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1600,
			.h2_tg_size = 1200,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 10,
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = DUCHAMPSC202PCSMACRO_SENSOR_ID,
	.reg_addr_sensor_id = {0x3107, 0x3108},
	.i2c_addr_table = {0x6C, 0xFF}, // TBD
	// .i2c_addr_table = {0x48, 0xFF}, // disable 64b
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {1600, 1200},
	.mirror = IMAGE_NORMAL, // TBD

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_lane_num = SENSOR_MIPI_1_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 16,
	.ana_gain_type = 1,
	.ana_gain_step = 1,
	.ana_gain_table = duchampsc202pcsmacro_ana_gain_table,
	.ana_gain_table_size = sizeof(duchampsc202pcsmacro_ana_gain_table),
	.min_gain_iso = 50,
	.exposure_def = 0x3D0,
	.exposure_min = 2,
	.exposure_max = 0xFFFF - 6,
	.exposure_step = 1,
	.exposure_margin = 6,

	.frame_length_max = 0xFFFF,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 0,

	.pdaf_type = PDAF_SUPPORT_CAMSV,
	.hdr_type = HDR_SUPPORT_STAGGER_FDOL,
	.seamless_switch_support = FALSE,
	.temperature_support = FALSE,

	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = PARAM_UNDEFINED, // TBD
	.reg_addr_exposure = {},
	.long_exposure_support = FALSE,
	.reg_addr_exposure_lshift = PARAM_UNDEFINED,
	.reg_addr_ana_gain = {},
	.reg_addr_frame_length = {0x320E, 0x320F},
	.reg_addr_temp_en = PARAM_UNDEFINED,
	.reg_addr_temp_read = PARAM_UNDEFINED,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = 0x0005, // To be verified

	.init_setting_table = duchampsc202pcsmacro_init_setting,
	.init_setting_len = ARRAY_SIZE(duchampsc202pcsmacro_init_setting),
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
	.get_csi_param = get_csi_param,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_DVDD,  1800000, 1}, // pmic_ldo for vcam_ldo
	{HW_ID_RST,   0,       1},
	{HW_ID_DOVDD, 1800000, 1}, // pmic_ldo for dovdd
	{HW_ID_AVDD,  2800000, 1}, // pmic_ldo for avdd
	{HW_ID_RST,   1,       5},
	{HW_ID_MCLK,  24,      0},
	{HW_ID_MCLK_DRIVING_CURRENT, 6, 5}
};

const struct subdrv_entry duchampsc202pcsmacro_mipi_raw_entry = {
	.name = "duchampsc202pcsmacro_mipi_raw",
	.id = DUCHAMPSC202PCSMACRO_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

/* FUNCTION */

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
	return; //FAE suggests don't use group hold function
}

static void duchampsc202pcsmacro_set_dummy(struct subdrv_ctx *ctx)
{
	// bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 1);
	write_frame_length(ctx, ctx->frame_length);
	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 0);

	commit_i2c_buffer(ctx);
}

static int duchampsc202pcsmacro_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
		duchampsc202pcsmacro_set_dummy(ctx);

	return ERROR_NONE;
}

static u16 DUCHAMPSC202PCSMACRO_AGC_Param[DUCHAMPSC202PCSMACRO_SENSOR_GAIN_MAX_VALID_INDEX][2] = {
	{  1024,  0x00 },
	{  2048,  0x01 },
	{  4096,  0x03 },
	{  8192,  0x07 },
	{ 16384,  0x0f },
	{ 32768,  0x1f },
};

static void duchampsc202pcsmacro_gain2reg (const u64 total_analog_gain, u8* analog_reg_gain, u8* digtal_fine_reg_gain)
{
	u16 gain_index;
	u32 temp_digtal_gain;
	u64 gain = total_analog_gain;

	if (gain < DUCHAMPSC202PCSMACRO_SENSOR_GAIN_BASE)
		gain = DUCHAMPSC202PCSMACRO_SENSOR_GAIN_BASE;
	else if (gain > DUCHAMPSC202PCSMACRO_SENSOR_GAIN_MAX)
		gain = DUCHAMPSC202PCSMACRO_SENSOR_GAIN_MAX;

	for (gain_index = DUCHAMPSC202PCSMACRO_SENSOR_GAIN_MAX_VALID_INDEX - 1; gain_index > 0; gain_index--)
		if (gain >= DUCHAMPSC202PCSMACRO_AGC_Param[gain_index][0])
			break;

	*analog_reg_gain = (u8) DUCHAMPSC202PCSMACRO_AGC_Param[gain_index][1];

	temp_digtal_gain =  gain*DUCHAMPSC202PCSMACRO_SENSOR_GAIN_BASE/DUCHAMPSC202PCSMACRO_AGC_Param[gain_index][0];
	temp_digtal_gain = temp_digtal_gain >> 3;

	*digtal_fine_reg_gain = (u8)temp_digtal_gain;
}

static int duchampsc202pcsmacro_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u8 analog_reg_gain, digtal_fine_reg_gain;
	u64 gain = *(u64 *)para;

	duchampsc202pcsmacro_gain2reg(gain, &analog_reg_gain, &digtal_fine_reg_gain);
	set_i2c_buffer(ctx, 0x3E09, analog_reg_gain);
	set_i2c_buffer(ctx, 0x3E06, 0);
	set_i2c_buffer(ctx, 0x3E07, digtal_fine_reg_gain); //1x-2x
	commit_i2c_buffer(ctx);
	DRV_LOG_MUST(ctx,"Gain_Debug: gain = %llu again = 0x%x, dgain = 0x%x\n", gain, analog_reg_gain, digtal_fine_reg_gain);
	DRV_LOG(ctx,"Gain_Debug read again = 0x%x, dgain = 0x%x\n",subdrv_i2c_rd_u8(ctx,0x3E09), subdrv_i2c_rd_u8(ctx,0x3E07));
	return ERROR_NONE;
}

static int duchampsc202pcsmacro_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 shutter = *((u64 *)para);
	u32 frame_length = 0;
	u32 fine_integ_line = 0;

	ctx->frame_length = frame_length ? frame_length : ctx->min_frame_length;
	check_current_scenario_id_bound(ctx);
	// check boundary of shutter
	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;
	shutter = FINE_INTEG_CONVERT(shutter, fine_integ_line);
	shutter = max_t(u64, shutter, (u64)ctx->s_ctx.exposure_min);
	shutter = min_t(u64, shutter, (u64)ctx->s_ctx.exposure_max);
	// check boundary of framelength
	ctx->frame_length = max((u32)shutter + ctx->s_ctx.exposure_margin, ctx->min_frame_length);
	ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);
	// restore shutter
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	ctx->exposure[0] = (u32) shutter;
	// write framelength
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx, ctx->frame_length);
	// write shutter
	if (shutter > 0) {
		set_i2c_buffer(ctx, 0x3e00, (shutter >> 12) & 0x0F);
		set_i2c_buffer(ctx, 0x3e01, (shutter >> 4)&0xFF);
		set_i2c_buffer(ctx, 0x3e02, (shutter<<4) & 0xF0);
	}
	DRV_LOG_MUST(ctx, "Shutter_Debug: exp[0x%x], fll(input/output):%u/%u, flick_en:%u\n",
		ctx->exposure[0], frame_length, ctx->frame_length, ctx->autoflicker_en);

	commit_i2c_buffer(ctx);
	return ERROR_NONE;
}

static int duchampsc202pcsmacro_stream_resume(struct subdrv_ctx *ctx, u8 *para, u32 *len){
	u64 *feature_data = (u64 *)para;
	if (*feature_data) {
		if (ctx->s_ctx.aov_sensor_support &&
			ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode &&
			!ctx->s_ctx.mode[ctx->current_scenario_id].ae_ctrl_support)
			DRV_LOG_MUST(ctx,
						"AOV sensing mode not support ae shutter control!\n");
		else
			duchampsc202pcsmacro_set_shutter(ctx, para,len);
	}
	duchampsc202pcsmacro_set_mirror_flip(ctx,IMAGE_HV_MIRROR);
	streaming_control(ctx, TRUE);
	return ERROR_NONE;
}


static u16 get_gain2reg(u32 gain)
{
	return gain * 256 / BASEGAIN;
}

static int duchampsc202pcsmacro_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	switch (mode) {
	case 2:
		//subdrv_i2c_wr_u8(ctx, 0x50c1, 0x01);
		break;
	case 5:
		//subdrv_i2c_wr_u8(ctx, 0x3019, 0xF0);
		//subdrv_i2c_wr_u8(ctx, 0x4308, 0x01);
		break;
	default:
		break;
	}

	if (mode != ctx->test_pattern)
		switch (ctx->test_pattern) {
		case 2:
			//subdrv_i2c_wr_u8(ctx, 0x50c1, 0x00);
			break;
		case 5:
			//subdrv_i2c_wr_u8(ctx, 0x3019, 0xD2);
			//subdrv_i2c_wr_u8(ctx, 0x4308, 0x00);
			break;
		default:
			break;
		}

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

static int get_csi_param(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	struct mtk_csi_param *csi_param)
{
	DRV_LOG(ctx, "+ scenario_id:%u,aov_csi_clk:%u\n",scenario_id, ctx->aov_csi_clk);
	switch (scenario_id) {
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM2:
			csi_param->legacy_phy = 0;
			csi_param->not_fixed_trail_settle = 1;
			csi_param->not_fixed_dphy_settle = 1;
			csi_param->dphy_data_settle = 0x27;
			csi_param->dphy_clk_settle = 0x27;
			csi_param->dphy_trail = 0x5A;
			//csi_param->dphy_csi2_resync_dmy_cycle = 0x43;
		break;
	default:
		csi_param->legacy_phy = 0;
		csi_param->not_fixed_trail_settle = 0;
		csi_param->not_fixed_dphy_settle = 0;
		break;
	}
	return 0;
}

static void duchampsc202pcsmacro_set_mirror_flip(struct subdrv_ctx *ctx, u8 image_mirror)
{
	u8 itemp;

	DRV_LOG(ctx,"image_mirror = 0x%x\n", image_mirror);
	itemp = subdrv_i2c_rd_u8(ctx, 0x3221);
	itemp &= ~0x66;

	switch (image_mirror) {

	case IMAGE_NORMAL:
	subdrv_i2c_wr_u8(ctx, 0x3221, itemp);
	break;

	case IMAGE_V_MIRROR:
	subdrv_i2c_wr_u8(ctx, 0x3221, itemp | 0x06);
	break;

	case IMAGE_H_MIRROR:
	subdrv_i2c_wr_u8(ctx, 0x3221, itemp | 0x60);
	break;

	case IMAGE_HV_MIRROR:
	subdrv_i2c_wr_u8(ctx, 0x3221, itemp | 0x66);
	break;
	}
}


