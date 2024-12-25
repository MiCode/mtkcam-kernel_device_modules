// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 duchampov16a1qfrontmipiraw_Sensor.c
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
#include "duchampov16a1qfrontmipiraw_Sensor.h"

static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static void duchampov16a1qfront_set_dummy(struct subdrv_ctx *ctx);
static int duchampov16a1qfront_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static u16 get_gain2reg(u32 gain);
static int  duchampov16a1qfront_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int front_vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt);
static int duchampov16a1qfront_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int duchampov16a1qfront_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);

/* STRUCT */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, duchampov16a1qfront_set_test_pattern},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO, duchampov16a1qfront_set_max_framerate_by_scenario},
	{SENSOR_FEATURE_SET_ESHUTTER, duchampov16a1qfront_set_shutter},
	{SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME, duchampov16a1qfront_set_shutter_frame_length},
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		.header_id = 0x010B00FF,
		.addr_header_id = 0x0000000B,
		.i2c_write_id = 0xA2,

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

// mode 0: 2304*1728@30fps, normal preview + non-pd
static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2304,
			.vsize = 1728,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

// mode 1: same as preview mode + non-pd
static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2304,
			.vsize = 1728,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

// mode 2: 2304x1296@30fps, noraml video + non-pd
static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2304,
			.vsize = 1296,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

// mode 3: 1920x1080@120fps, SMVR + non-pd
static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1920,
			.vsize = 1080,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

// mode 4: same as preview mode + non-pd
static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2304,
			.vsize = 1728,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

// mode 5: 2304x1296@60fps, VHDR(m-stream)/ 60fps video + non-pd
static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2304,
			.vsize = 1296,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

static struct subdrv_mode_struct mode_struct[] = {
	// mode 0: 2304x1728@30fps, normal preview + no pd
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = duchampov16a1qfront_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov16a1qfront_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 850,
		.framelength = 3920,
		.max_framerate = 300,
		.mipi_pixel_rate = 580800000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.imgsensor_winsize_info = {
			.full_w = 4656,
			.full_h = 3496,
			.x0_offset = 24,
			.y0_offset = 20,
			.w0_size = 4608,
			.h0_size = 3456,
			.scale_w = 2304,
			.scale_h = 1728,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2304,
			.h1_size = 1728,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2304,
			.h2_tg_size = 1728,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// mode 1: same as preview mode
	{
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = duchampov16a1qfront_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov16a1qfront_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 850,
		.framelength = 3920,
		.max_framerate = 300,
		.mipi_pixel_rate = 580800000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.imgsensor_winsize_info = {
			.full_w = 4656,
			.full_h = 3496,
			.x0_offset = 24,
			.y0_offset = 20,
			.w0_size = 4608,
			.h0_size = 3456,
			.scale_w = 2304,
			.scale_h = 1728,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2304,
			.h1_size = 1728,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2304,
			.h2_tg_size = 1728,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// mode 2: 2304x1296@30fps, noraml video + non-pd
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = duchampov16a1qfront_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov16a1qfront_normal_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 850,
		.framelength = 3920,
		.max_framerate = 300,
		.mipi_pixel_rate = 580800000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.imgsensor_winsize_info = {
			.full_w = 4656,
			.full_h = 3496,
			.x0_offset = 24,
			.y0_offset = 452,
			.w0_size = 4608,
			.h0_size = 2592,
			.scale_w = 2304,
			.scale_h = 1296,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2304,
			.h1_size = 1296,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2304,
			.h2_tg_size = 1296,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// mode 3: 1920x1080@120fps, SMVR + non-pd,
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = duchampov16a1qfront_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov16a1qfront_hs_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 425,
		.framelength = 1962,
		.max_framerate = 1200,
		.mipi_pixel_rate = 580800000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.imgsensor_winsize_info = {
			.full_w = 4656,
			.full_h = 3496,
			.x0_offset = 408,
			.y0_offset = 412,
			.w0_size = 3840,
			.h0_size = 2160,
			.scale_w = 1920,
			.scale_h = 1080,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1920,
			.h1_size = 1080,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1920,
			.h2_tg_size = 1080,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// mode 4: same as preview mode + non-pd
	{
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table = duchampov16a1qfront_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov16a1qfront_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 850,
		.framelength = 3920,
		.max_framerate = 300,
		.mipi_pixel_rate = 580800000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.imgsensor_winsize_info = {
			.full_w = 4656,
			.full_h = 3496,
			.x0_offset = 24,
			.y0_offset = 20,
			.w0_size = 4608,
			.h0_size = 3456,
			.scale_w = 2304,
			.scale_h = 1728,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2304,
			.h1_size = 1728,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2304,
			.h2_tg_size = 1728,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// mode 5: 2304x1296@60fps, VHDR(m-stream)/ 60fps video + non-pd
	{
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = duchampov16a1qfront_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov16a1qfront_custom1_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 850,
		.framelength = 1960,
		.max_framerate = 600,
		.mipi_pixel_rate = 580800000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.imgsensor_winsize_info = {
			.full_w = 4656,
			.full_h = 3496,
			.x0_offset = 24,
			.y0_offset = 452,
			.w0_size = 4608,
			.h0_size = 2592,
			.scale_w = 2304,
			.scale_h = 1296,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2304,
			.h1_size = 1296,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2304,
			.h2_tg_size = 1296,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = DUCHAMPOV16A1QFRONT_SENSOR_ID,
	.reg_addr_sensor_id = {0x300B, 0x300C},
	.i2c_addr_table = {0x20, 0xFF}, // TBD
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {4656, 3496},
	.mirror = IMAGE_NORMAL, // TBD

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 15.5,
	.ana_gain_type = 1,
	.ana_gain_step = 1,
	.ana_gain_table = duchampov16a1qfront_ana_gain_table,
	.ana_gain_table_size = sizeof(duchampov16a1qfront_ana_gain_table),
	.min_gain_iso = 50,
	.exposure_def = 0x3D0,
	.exposure_min = 8,
	.exposure_max = 0x7FFE - 8,
	.exposure_step = 2,
	.exposure_margin = 8,

	.frame_length_max = 0x7FFE,
	.ae_effective_frame = 3,
	.frame_time_delay_frame = 2,
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
		{0x3500, 0x3501, 0x3502},
		{0x3540, 0x3541, 0x3542}
	},
	.long_exposure_support = FALSE,
	.reg_addr_exposure_lshift = PARAM_UNDEFINED,
	.reg_addr_ana_gain = {
		{0x3508, 0x3509},
		{0x3548, 0x3549}
	},
	.reg_addr_frame_length = {0x380E, 0x380F},
	.reg_addr_temp_en = 0x4D12,
	.reg_addr_temp_read = 0x4D13,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = 0x387F, // To be verified

	.init_setting_table = duchampov16a1qfront_init_setting,
	.init_setting_len = ARRAY_SIZE(duchampov16a1qfront_init_setting),
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
	.get_csi_param = common_get_csi_param,
	.update_sof_cnt = common_update_sof_cnt,
	.vsync_notify = front_vsync_notify,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_DVDD,  1800000, 1}, // pmic_ldo for vcam_ldo
	{HW_ID_RST,   0,       5},
	{HW_ID_AVDD,  2800000, 1}, // pmic_ldo for avdd
	{HW_ID_DVDD1, 1200000, 1}, // pmic_ldo for dvdd1
	{HW_ID_DOVDD, 1800000, 1}, // pmic_ldo for dovdd
	{HW_ID_MCLK,  24,	   0},
	{HW_ID_MCLK_DRIVING_CURRENT, 6, 2},
	{HW_ID_RST,   1,       5}
};

const struct subdrv_entry duchampov16a1qfront_mipi_raw_entry = {
	.name = "duchampov16a1qfront_mipi_raw",
	.id = DUCHAMPOV16A1QFRONT_SENSOR_ID,
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
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	if (en) {
		set_i2c_buffer(ctx, 0x3208, 0x00);
	} else {
		set_i2c_buffer(ctx, 0x3208, 0x10);
		set_i2c_buffer(ctx, 0x3208, 0xA0);
	}
}

static void duchampov16a1qfront_set_dummy(struct subdrv_ctx *ctx)
{
	// bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 1);
	write_frame_length(ctx, ctx->frame_length);
	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 0);

	commit_i2c_buffer(ctx);
}

static int duchampov16a1qfront_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
		duchampov16a1qfront_set_dummy(ctx);

	return ERROR_NONE;
}

static u16 get_gain2reg(u32 gain)
{
	return gain * 256 / BASEGAIN;
}

static int duchampov16a1qfront_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	switch (mode) {
	case 5:
		subdrv_i2c_wr_u8(ctx, 0x5081, 0x01);
		subdrv_i2c_wr_u8(ctx, 0x5082, 0x01);
		break;
	default:
		break;
	}

	if (mode != ctx->test_pattern)
		switch (ctx->test_pattern) {
		case 5:
			subdrv_i2c_wr_u8(ctx, 0x5081, 0x01);
			subdrv_i2c_wr_u8(ctx, 0x5082, 0x01);
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

static int front_vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt)
{
	kal_uint16 sensor_output_cnt;

	sensor_output_cnt = subdrv_i2c_rd_u8(ctx, ctx->s_ctx.reg_addr_frame_count);
	DRV_LOG_MUST(ctx, "sensormode(%d) sof_cnt(%d) sensor_output_cnt(%d)\n",
		ctx->current_scenario_id, sof_cnt, sensor_output_cnt);

	if (ctx->fast_mode_on && (sof_cnt > ctx->ref_sof_cnt)) {
		ctx->fast_mode_on = FALSE;
		ctx->ref_sof_cnt = 0;
		DRV_LOG(ctx, "seamless_switch finish.");
	}

	return 0;
};

static int duchampov16a1qfront_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
	/* write framelength */
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend){
		if(shutter > 0x2000 && shutter < 0x7FF6){
			ctx->frame_length = ctx->frame_length - shutter + 0x2000;
			if(ctx->frame_length < 0x2008)	ctx->frame_length = 0x2008;
		}else if(shutter >= 0x7FF6){
			ctx->frame_length = 0x2008;
		}
		write_frame_length(ctx,ctx->frame_length);
	}
	/* write shutter */
	set_long_exposure(ctx);
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
	DRV_LOG(ctx, "exp[0x%x], fll(input/output):%u/%u, flick_en:%d\n",
		ctx->exposure[0], frame_length, ctx->frame_length, ctx->autoflicker_en);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */
	return 0;
}

static int duchampov16a1qfront_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	duchampov16a1qfront_set_shutter_frame_length(ctx, para, len);
	return 0;
}


