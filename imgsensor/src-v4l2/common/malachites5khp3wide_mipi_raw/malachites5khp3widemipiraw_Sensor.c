// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

/********************************************************************
 *
 * Filename:
 * ---------
 *	 malachites5khp3widemipiraw_Sensor.c
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
 *-------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *===================================================================
 *******************************************************************/
#include "malachites5khp3widemipiraw_Sensor.h"
#define MALACHITES5KHP3WIDE_LOG_INF(format, args...) pr_info(LOG_TAG "[%s] " format, __func__, ##args)
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static int malachites5khp3wide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int malachites5khp3wide_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int malachites5khp3wide_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int malachites5khp3wide_sensor_init(struct subdrv_ctx *ctx);
static int open(struct subdrv_ctx *ctx);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt);

/* STRUCT */
static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, malachites5khp3wide_set_test_pattern},
	{SENSOR_FEATURE_SET_TEST_PATTERN_DATA, malachites5khp3wide_set_test_pattern_data},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, malachites5khp3wide_seamless_switch},
};

#define SEAMLESS_SWITCH_GROUP_NORMAL_14BIT_MODE 1
#define SEAMLESS_SWITCH_GROUP_NORMAL_FULL_MODE  2

static struct eeprom_info_struct eeprom_info[] = {
	{
		//.header_id = 0x010B00FF,
		//.addr_header_id = 0x00000001,
		.i2c_write_id = 0xA2,

		//.xtalk_support = TRUE,
		//.xtalk_size = 2048,
		//.addr_xtalk = 0x150F,
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
		.i4OffsetX = 0,
		.i4OffsetY = 0,
		.i4PitchX  = 0,
		.i4PitchY  = 0,
		.i4PairNum = 0,
		.i4SubBlkW = 0,
		.i4SubBlkH = 0,
		.i4PosL    = {{0, 0} },
		.i4PosR    = {{0, 0} },
		.i4BlockNumX = 0,
		.i4BlockNumY = 0,
		.i4LeFirst   = 0,
		.i4FullRawW = 4080,
		.i4FullRawH = 3060,
		.i4VCPackNum = 1,
		.PDAF_Support = PDAF_SUPPORT_CAMSV_QPD,
		.i4ModeIndex = 3,
		.sPDMapInfo[0] = {
			.i4PDPattern = 1,
			.i4BinFacX = 2,
			.i4BinFacY = 4,
			.i4PDRepetition = 0,
			.i4PDOrder = {0},
		},
		.i4Crop = {
			{0, 0}, {0, 0}, {0, 382}, {60, 225}, {0, 0},
			{0, 0},{60, 225},{0, 382},{0, 24},{0, 0},
			{0, 0},{0, 0},{0, 0}, {0, 382}, {0, 382}
		},
		.iMirrorFlip = IMAGE_NORMAL,
};

static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0ff0,
			.vsize = 0x0bf4,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0xff0,
			.vsize = 0x2fc,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0ff0,
			.vsize = 0x0bf4,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0ff0,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0xff0,
			.vsize = 0x23c,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0780,
			.vsize = 0x0438,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
/*	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 1920,
			.vsize = 2680,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},*/
};

static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0ff0,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0ff0,
			.vsize = 0x0bf4,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 0x0ff0,
			.vsize = 0x0bf4,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 0x0ff0,
			.vsize = 0x0bf4,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
/*	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 4080,
			.vsize = 572,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},*/
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 0x0ff0,
			.vsize = 0x0bf4,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 0x0ff0,
			.vsize = 0x0bf4,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
/*	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 4080,
			.vsize = 764,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},*/
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus6[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 0x0ff0,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus7[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x3fc0,
			.vsize = 0x2fd0,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus8 [] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1fe0,
			.vsize = 0x17e8,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus9 [] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 0xff0,
			.vsize = 0x900,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus10[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0780,
			.vsize = 0x0438,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

//1000 base for dcg gain ratio
static u32 malachites5khp3wide_dcg_ratio_table_cus5[] = {16000};

static u32 malachites5khp3wide_dcg_ratio_table_cus6[] = {16000};

static u32 malachites5khp3wide_dcg_ratio_table_cus9[] = {16000};

static struct mtk_sensor_saturation_info imgsensor_saturation_info = {
	.gain_ratio = 1000,
	.OB_pedestal = 64,
	.saturation_level = 1023,
	.adc_bit = 10,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_cus2 = {
	.gain_ratio = 1000,
	.OB_pedestal = 1024,
	.saturation_level = 16384,
	.adc_bit = 14,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_cus3 = {
	.gain_ratio = 1000,
	.OB_pedestal = 1024,
	.saturation_level = 16384,
	.adc_bit = 14,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_cus4 = {
	.gain_ratio = 1000,
	.OB_pedestal = 1024,
	.saturation_level = 16384,
	.adc_bit = 14,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_cus5 = {
	.gain_ratio = 16000,
	.OB_pedestal = 1024,
	.saturation_level = 16368,
	.adc_bit = 10,
	.ob_bm = 64,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_cus6 = {
	.gain_ratio = 16000,
	.OB_pedestal = 1024,
	.saturation_level = 16368,
	.adc_bit = 10,
	.ob_bm = 64,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_cus9 = {
	.gain_ratio = 16000,
	.OB_pedestal = 1024,
	.saturation_level = 16368,
	.adc_bit = 10,
	.ob_bm = 64,
};

static struct subdrv_mode_struct mode_struct[] = {
	{//preview
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = addr_data_pair_preview,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_preview),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_FULL_MODE,
		.seamless_switch_mode_setting_table = malachites5khp3wide_seamless_preivew,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(malachites5khp3wide_seamless_preivew),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2000000000,
		.linelength = 13056,
		.framelength = 5104,
		.max_framerate = 300,
		.mipi_pixel_rate = 1586880000,
		.readout_length = 0,
		.read_margin = 34,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.ana_gain_max = BASEGAIN * 128,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 128,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 16320,
			.h0_size = 12288,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 6,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{//capture
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = addr_data_pair_preview,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_preview),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2000000000,
		.linelength = 13056,
		.framelength = 5104,
		.max_framerate = 300,
		.mipi_pixel_rate = 1586880000,
		.readout_length = 0,
		.read_margin = 34,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.ana_gain_max = BASEGAIN * 128,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 128,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 16320,
			.h0_size = 12288,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 6,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{//normal video
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = addr_data_pair_normal_video,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_normal_video),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2000000000,
		.linelength = 13056,
		.framelength = 5104,
		.max_framerate = 300,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 34,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.ana_gain_max = BASEGAIN * 128,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 128,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 16320,
			.h0_size = 9216,
			.scale_w = 4080,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{//hs video
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = addr_data_pair_hs_video,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_hs_video),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2000000000,
		.linelength = 5888,
		.framelength = 1412,
		.max_framerate = 2400,
		.mipi_pixel_rate = 1586880000,
		.readout_length = 0,
		.read_margin = 14,
		.coarse_integ_step = 2,
		.min_exposure_line = 16,
		.ana_gain_max = BASEGAIN * 128,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 128,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 480,
			.y0_offset = 1792,
			.w0_size = 15360,
			.h0_size = 8704,
			.scale_w = 1920,
			.scale_h = 1088,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 1920,
			.h1_size = 1080,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1920,
			.h2_tg_size = 1080,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{//slim video
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table = addr_data_pair_slim_video,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_slim_video),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2000000000,
		.linelength = 13056,
		.framelength = 2552,
		.max_framerate = 600,
		.mipi_pixel_rate = 1586880000,
		.readout_length = 0,
		.read_margin = 34,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.ana_gain_max = BASEGAIN * 128,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 128,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 16320,
			.h0_size = 9216,
			.scale_w = 4080,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{//custom1
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = addr_data_pair_custom1,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom1),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2000000000,
		.linelength = 13056,
		.framelength = 5104,
		.max_framerate = 300,
		.mipi_pixel_rate = 1586880000,
		.readout_length = 0,
		.read_margin = 34,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.ana_gain_max = BASEGAIN * 128,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 128,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 16320,
			.h0_size = 12288,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 6,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{//custom2 LN2
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = addr_data_pair_custom2,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom2),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_14BIT_MODE,
		.seamless_switch_mode_setting_table = malachites5khp3wide_seamless_custom2,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(malachites5khp3wide_seamless_custom2),
		.hdr_mode = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2000000000,
		.linelength = 16512,
		.framelength = 4032,
		.max_framerate = 300,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 34,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.ana_gain_max = BASEGAIN * 128,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 128,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 16320,
			.h0_size = 12288,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 6,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_Gr,
		.saturation_info = &imgsensor_saturation_info_cus2,
	},
	{//custom3 2X
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = addr_data_pair_custom3,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom3),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_14BIT_MODE,
		.seamless_switch_mode_setting_table = malachites5khp3wide_seamless_custom3,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(malachites5khp3wide_seamless_custom3),
		.hdr_mode = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1760000000,
		.linelength = 12928,
		.framelength = 4528,
		.max_framerate = 300,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 55,
		.coarse_integ_step = 1,
		.min_exposure_line = 6,
		.ana_gain_max = BASEGAIN * 64,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 4080,
			.y0_offset = 3080,
			.w0_size = 8160,
			.h0_size = 6128,
			.scale_w = 4080,
			.scale_h = 3064,
			.x1_offset = 0,
			.y1_offset = 2,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_Gr,
		.saturation_info = &imgsensor_saturation_info_cus3,
	},
	{//custom4 4X
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = addr_data_pair_custom4,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom4),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_14BIT_MODE,
		.seamless_switch_mode_setting_table = malachites5khp3wide_seamless_custom4,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(malachites5khp3wide_seamless_custom4),
		.raw_cnt = 1,
		.exp_cnt = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 2000000000,
		.linelength = 20352,
		.framelength = 3276,
		.max_framerate = 300,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 164,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.ana_gain_max = BASEGAIN * 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 6120,
			.y0_offset = 4608,
			.w0_size = 4080,
			.h0_size = 3072,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 6,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_Gr,
		.saturation_info = &imgsensor_saturation_info_cus4,
	},
	{//custom5 capture iDCG
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = addr_data_pair_custom5,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom5),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_14BIT_MODE,
		.seamless_switch_mode_setting_table = malachites5khp3wide_seamless_custom5,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(malachites5khp3wide_seamless_custom5),
		.raw_cnt = 1,
		.exp_cnt = 1,
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.pclk = 2000000000,
		.linelength = 16512,
		.framelength = 4032,
		.max_framerate = 300,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 34,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.ana_gain_max = BASEGAIN * 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 16320,
			.h0_size = 12288,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 6,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 4,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_Gr,
		.saturation_info = &imgsensor_saturation_info_cus5,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_base = IMGSENSOR_DCG_GAIN_LCG_BASE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = malachites5khp3wide_dcg_ratio_table_cus5,
			.dcg_gain_table_size = sizeof(malachites5khp3wide_dcg_ratio_table_cus5),
		},
	},
	{//custom6 video iDCG
		.frame_desc = frame_desc_cus6,
		.num_entries = ARRAY_SIZE(frame_desc_cus6),
		.mode_setting_table = addr_data_pair_custom6,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom6),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.pclk = 2000000000,
		.linelength = 16512,
		.framelength = 4032,
		.max_framerate = 300,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 34,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.ana_gain_max = BASEGAIN * 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 16320,
			.h0_size = 9216,
			.scale_w = 4080,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 4,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_Gr,
		.saturation_info = &imgsensor_saturation_info_cus6,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_base = IMGSENSOR_DCG_GAIN_LCG_BASE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = malachites5khp3wide_dcg_ratio_table_cus6,
			.dcg_gain_table_size = sizeof(malachites5khp3wide_dcg_ratio_table_cus6),
		},
	},
	{//custom7 200m
		.frame_desc = frame_desc_cus7,
		.num_entries = ARRAY_SIZE(frame_desc_cus7),
		.mode_setting_table = addr_data_pair_custom7,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom7),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_FULL_MODE,
		.seamless_switch_mode_setting_table = malachites5khp3wide_seamless_custom7,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(malachites5khp3wide_seamless_custom7),
		.raw_cnt = 1,
		.exp_cnt = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 2000000000,
		.linelength = 22528,
		.framelength = 17664,
		.max_framerate = 50,
		.mipi_pixel_rate = 1586880000,
		.readout_length = 0,
		.read_margin = 164,
		.coarse_integ_step = 1,
		.min_exposure_line = 8,
		.ana_gain_max = BASEGAIN * 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 24,
			.w0_size = 16320,
			.h0_size = 12240,
			.scale_w = 16320,
			.scale_h = 12240,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 16320,
			.h1_size = 12240,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 16320,
			.h2_tg_size = 12240,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 4,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		//.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_4X4,
	},
	{//custom 8 50M
		.frame_desc = frame_desc_cus8,
		.num_entries = ARRAY_SIZE(frame_desc_cus8),
		.mode_setting_table = addr_data_pair_custom8,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom8),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_FULL_MODE,
		.seamless_switch_mode_setting_table = malachites5khp3wide_seamless_custom8,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(malachites5khp3wide_seamless_custom8),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1760000000,
		.linelength = 11264,
		.framelength = 11520,
		.max_framerate = 100,
		.mipi_pixel_rate = 1586880000,
		.readout_length = 0,
		.read_margin = 55,
		.coarse_integ_step = 1,
		.min_exposure_line = 6,
		.ana_gain_max = BASEGAIN * 64,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 24,
			.w0_size = 16320,
			.h0_size = 12240,
			.scale_w = 8160,
			.scale_h = 6120,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 8160,
			.h1_size = 6120,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 8160,
			.h2_tg_size = 6120,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{//custom9 24fps video iDCG
		.frame_desc = frame_desc_cus9,
		.num_entries = ARRAY_SIZE(frame_desc_cus9),
		.mode_setting_table = addr_data_pair_custom9,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom9),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.pclk = 2000000000,
		.linelength = 16512,
		.framelength = 5040,
		.max_framerate = 240,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 34,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.ana_gain_max = BASEGAIN * 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 16320,
			.h0_size = 9216,
			.scale_w = 4080,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 4,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_Gr,
		.saturation_info = &imgsensor_saturation_info_cus9,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_base = IMGSENSOR_DCG_GAIN_LCG_BASE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = malachites5khp3wide_dcg_ratio_table_cus9,
			.dcg_gain_table_size = sizeof(malachites5khp3wide_dcg_ratio_table_cus9),
		},
	},
	{//custom10 1080p@240fps
		.frame_desc = frame_desc_cus10,
		.num_entries = ARRAY_SIZE(frame_desc_cus10),
		.mode_setting_table = addr_data_pair_custom10,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom10),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2000000000,
		.linelength = 5888,
		.framelength = 2824,
		.max_framerate = 1200,
		.mipi_pixel_rate = 1586880000,
		.readout_length = 0,
		.read_margin = 14,
		.coarse_integ_step = 2,
		.min_exposure_line = 16,
		.ana_gain_max = BASEGAIN * 128,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 128,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 480,
			.y0_offset = 1792,
			.w0_size = 15360,
			.h0_size = 8704,
			.scale_w = 1920,
			.scale_h = 1088,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 1920,
			.h1_size = 1080,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1920,
			.h2_tg_size = 1080,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 4,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = MALACHITES5KHP3WIDE_SENSOR_ID,
	.reg_addr_sensor_id = {0x0000, 0x0001},
	.i2c_addr_table = {0x20, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_16,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {16320, 12288},
	.mirror = IMAGE_NORMAL,

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 128,
	.ana_gain_type = 2,
	.ana_gain_step = 1,
	.ana_gain_table = malachites5khp3wide_ana_gain_table,
	.ana_gain_table_size = sizeof(malachites5khp3wide_ana_gain_table),
	.min_gain_iso = 50,
	.exposure_def = 0x3D0,
	.exposure_min = 16,
	.exposure_max = 0xFFFF * 128 - 3,
	.exposure_step = 2,
	.exposure_margin = 34,
	.mi_long_exposure_type = 1,
	.dig_gain_min = BASE_DGAIN * 1,
	.dig_gain_max = BASE_DGAIN * 16,
	.dig_gain_step = 4,
	.saturation_info = &imgsensor_saturation_info,

	.frame_length_max = 0xFFFF,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 3000000,

	.pdaf_type = PDAF_SUPPORT_CAMSV_QPD,
	.hdr_type = HDR_SUPPORT_DCG,
	.seamless_switch_support = TRUE,
	.temperature_support = FALSE,
	.g_temp = PARAM_UNDEFINED,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {{0x0202, 0x0203},},
	.long_exposure_support = TRUE,
	.reg_addr_exposure_lshift = 0x0704,
	.reg_addr_framelength_lshift = 0x0702,
	.reg_addr_ana_gain = {{0x0204, 0x0205},},
	.reg_addr_dig_gain = {{0x020e, 0x020f},},
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_temp_en = PARAM_UNDEFINED,
	.reg_addr_temp_read = PARAM_UNDEFINED,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = 0x0005,

	.init_setting_table = PARAM_UNDEFINED,
	.init_setting_len = PARAM_UNDEFINED,
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 1,
	.chk_s_off_end = 1,

	.checksum_value = 0x47a75476,
};

static struct subdrv_ops ops = {
	.get_vendr_id = common_get_vendor_id,
	.get_id = common_get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = common_close,
	.get_frame_desc = common_get_frame_desc,
	.get_csi_param = common_get_csi_param,
	.vsync_notify = vsync_notify,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST, 0, 1},
	{HW_ID_DOVDD, 1800000, 1},
 	{HW_ID_DVDD, 900000, 1},
	{HW_ID_DVDD1, 900000, 1},
	{HW_ID_AVDD, 2200000, 1},
	{HW_ID_AVDD1, 2200000, 1},
	{HW_ID_RST, 1, 1},
	{HW_ID_MCLK, 24, 1},
	{HW_ID_MCLK_DRIVING_CURRENT, 6, 20},
};
const struct subdrv_entry malachites5khp3wide_mipi_raw_entry = {
	.name = "malachites5khp3wide_mipi_raw",
	.id = MALACHITES5KHP3WIDE_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

/* FUNCTION */

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

static int malachites5khp3wide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	if (mode)
		subdrv_i2c_wr_u16(ctx, 0x0600, 0x0001); /*100% Color bar*/
	else if (ctx->test_pattern)
		subdrv_i2c_wr_u16(ctx, 0x0600, 0x0000); /*No pattern*/

	ctx->test_pattern = mode;

	return 0;
}

static int malachites5khp3wide_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	struct mtk_test_pattern_data *data = (struct mtk_test_pattern_data *)para;
	u16 R = (data->Channel_R >> 22) & 0x3ff;
	u16 Gr = (data->Channel_Gr >> 22) & 0x3ff;
	u16 Gb = (data->Channel_Gb >> 22) & 0x3ff;
	u16 B = (data->Channel_B >> 22) & 0x3ff;

	subdrv_i2c_wr_u16(ctx, 0x0602, Gr);
	subdrv_i2c_wr_u16(ctx, 0x0604, R);
	subdrv_i2c_wr_u16(ctx, 0x0606, B);
	subdrv_i2c_wr_u16(ctx, 0x0608, Gb);

	DRV_LOG(ctx, "mode(%u) R/Gr/Gb/B = 0x%04x/0x%04x/0x%04x/0x%04x\n",
		ctx->test_pattern, R, Gr, Gb, B);

	return 0;
}

static int malachites5khp3wide_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	struct mtk_hdr_ae *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;
	u32 frame_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
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

	exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
	ctx->is_seamless = TRUE;

	subdrv_i2c_wr_u8(ctx, 0x0104, 0x01);
	subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x02);
	if (ctx->s_ctx.reg_addr_fast_mode_in_lbmf &&
		(ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_LBMF ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF))
		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_fast_mode_in_lbmf, 0x4);

	update_mode_info(ctx, scenario_id);
	i2c_table_write(ctx,
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
		case HDR_RAW_DCG_COMPOSE:
			set_shutter(ctx, ae_ctrl->exposure.le_exposure);
			if (ctx->s_ctx.mode[scenario_id].dcg_info.dcg_gain_mode
				== IMGSENSOR_DCG_DIRECT_MODE)
				set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			else
				set_gain(ctx, ae_ctrl->gain.me_gain);  //使用me_gain避免使用le_gain导致的切过去过曝
			break;
		default:
			set_shutter(ctx, ae_ctrl->exposure.le_exposure);
			set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		}
	}
	subdrv_i2c_wr_u8(ctx, 0x0104, 0x00);

	ctx->fast_mode_on = TRUE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->is_seamless = FALSE;
	DRV_LOG(ctx, "X: set seamless switch done\n");
	return ERROR_NONE;
}

static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt)
{
	DRV_LOG(ctx, "sof_cnt(%u) ctx->ref_sof_cnt(%u) ctx->fast_mode_on(%d)",
		sof_cnt, ctx->ref_sof_cnt, ctx->fast_mode_on);
	if (ctx->fast_mode_on && (sof_cnt > ctx->ref_sof_cnt)) {
		ctx->fast_mode_on = FALSE;
		ctx->ref_sof_cnt = 0;
		DRV_LOG(ctx, "seamless_switch finished.");
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x00);
		commit_i2c_buffer(ctx);
	}
	return 0;
}


static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;

	return 0;
}

static int malachites5khp3wide_sensor_init(struct subdrv_ctx *ctx)
{
	DRV_LOG(ctx, "E\n");
	subdrv_i2c_wr_u16(ctx, 0xFCFC, 0x4000);
	subdrv_i2c_wr_u16(ctx, 0x0000, 0x0009);
	subdrv_i2c_wr_u16(ctx, 0x0000, 0x1B73);
	subdrv_i2c_wr_u16(ctx, 0x6012, 0x0001);
	subdrv_i2c_wr_u16(ctx, 0x7002, 0x0008);
	subdrv_i2c_wr_u16(ctx, 0x6014, 0x0001);
	mdelay(10);
	subdrv_i2c_wr_u16(ctx, 0x6214, 0xFF7D);
	subdrv_i2c_wr_u16(ctx, 0x6218, 0x0000);
	subdrv_i2c_wr_u16(ctx, 0xA100, 0x2F06);
	subdrv_i2c_wr_u16(ctx, 0xA102, 0x0000);

	i2c_table_write(ctx, sensor_init_addr_data, sizeof(sensor_init_addr_data)/sizeof(u16));
	DRV_LOG(ctx, "X\n");

	return 0;
}

static int open(struct subdrv_ctx *ctx)
{
	u32 sensor_id = 0;
	u32 scenario_id = 0;

	/* get sensor id */
	if (common_get_imgsensor_id(ctx, &sensor_id) != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail setting */
	malachites5khp3wide_sensor_init(ctx);

	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	memset(ctx->ana_gain, 0, sizeof(ctx->gain));
	ctx->exposure[0] = ctx->s_ctx.exposure_def;
	ctx->ana_gain[0] = ctx->s_ctx.ana_gain_def;
	ctx->current_scenario_id = scenario_id;
	ctx->pclk = ctx->s_ctx.mode[scenario_id].pclk;
	ctx->line_length = ctx->s_ctx.mode[scenario_id].linelength;
	ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;
	ctx->current_fps = 10 * ctx->pclk / ctx->line_length / ctx->frame_length;
	ctx->readout_length = ctx->s_ctx.mode[scenario_id].readout_length;
	ctx->read_margin = ctx->s_ctx.mode[scenario_id].read_margin;
	ctx->min_frame_length = ctx->frame_length;
	ctx->autoflicker_en = FALSE;
	ctx->test_pattern = 0;
	ctx->ihdr_mode = 0;
	ctx->pdaf_mode = 0;
	ctx->hdr_mode = 0;
	ctx->extend_frame_length_en = 0;
	ctx->is_seamless = 0;
	ctx->fast_mode_on = 0;
	ctx->sof_cnt = 0;
	ctx->ref_sof_cnt = 0;
	ctx->is_streaming = 0;

	return ERROR_NONE;
} /* open */
