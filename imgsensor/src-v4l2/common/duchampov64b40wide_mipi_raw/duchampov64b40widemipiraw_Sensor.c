// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 duchampov64b40widemipiraw_Sensor.c
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
#include "duchampov64b40widemipiraw_Sensor.h"

static void set_init_setting(void *arg);
static u32 evaluate_frame_rate_by_scenario(void *arg, enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 framerate);
static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static void duchampov64b40wide_set_dummy(struct subdrv_ctx *ctx);
static int duchampov64b40wide_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static u16 get_gain2reg(u32 gain);
static int duchampov64b40wide_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int  duchampov64b40wide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int duchampov64b40wide_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt);

/* STRUCT */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, duchampov64b40wide_set_test_pattern},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, duchampov64b40wide_seamless_switch},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO, duchampov64b40wide_set_max_framerate_by_scenario},
	{SENSOR_FEATURE_SET_MULTI_DIG_GAIN, duchampov64b40wide_set_multi_dig_gain},
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		//.header_id = 0x01, // vendor id : 0x01 == sunny
		//.addr_header_id = 0x01, // vendor id addr
		.i2c_write_id = 0xA2,

		.pdc_support = TRUE,
		.pdc_size = 720,
		.addr_pdc = 0x12E2,
		.sensor_reg_addr_pdc = 0x5F80,

		.qsc_support = TRUE,
		.qsc_size = 288,
		.addr_qsc = 0x2956,
		.sensor_reg_addr_qsc = 0x5A40,
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX	= 40,
	.i4OffsetY	= 16,
	.i4PitchX	= 16,
	.i4PitchY	= 16,
	.i4PairNum	= 8,
	.i4SubBlkW	= 8,
	.i4SubBlkH	= 4,
	.iMirrorFlip = 3,
	.i4PosL = {
		{47, 18}, {55, 18}, {43, 22}, {51, 22},
		{47, 26}, {55, 26}, {43, 30}, {51, 30},
	},
	.i4PosR = {
		{46, 18}, {54, 18}, {42, 22}, {50, 22},
		{46, 26}, {54, 26}, {42, 30}, {50, 30},
	},
	.i4BlockNumX = 284,
	.i4BlockNumY = 215,
	.i4FullRawH = 3472,
	.i4FullRawW = 4624,
	.i4ModeIndex = 3,
	.sPDMapInfo[0] = {
		.i4PDPattern = 2,
		.i4BinFacX = 0,
		.i4BinFacY = 0,
		.i4PDRepetition = 2,
		.i4PDOrder = {1},
	},
	.i4Crop = {
		{0,0}, {0,0},{0,432},{0,0},{0,0},{0,0},{0,0},{0,432},
		{0,0},{2312,1736},{0,0},{0,432},{0,0},{0,432},{0,0},{0,0}
	},
};


static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_video = {
	.i4OffsetX	= 40,
	.i4OffsetY	= 16,
	.i4PitchX	= 16,
	.i4PitchY	= 16,
	.i4PairNum	= 8,
	.i4SubBlkW	= 8,
	.i4SubBlkH	= 4,
	.iMirrorFlip = 3,
	.i4PosL = {
		{47, 18}, {55, 18}, {43, 22}, {51, 22},
		{47, 26}, {55, 26}, {43, 30}, {51, 30},
	},
	.i4PosR = {
		{46, 18}, {54, 18}, {42, 22}, {50, 22},
		{46, 26}, {54, 26}, {42, 30}, {50, 30},
	},
	.i4BlockNumX = 284,
	.i4BlockNumY = 163,
	.i4FullRawH = 3472,
	.i4FullRawW = 4624,
	.i4ModeIndex = 3,
	.sPDMapInfo[0] = {
		.i4PDPattern = 2,
		.i4BinFacX = 0,
		.i4BinFacY = 0,
		.i4PDRepetition = 2,
		.i4PDOrder = {1},
	},
	.i4Crop = {
		{0,0}, {0,0},{0,432},{0,0},{0,0},{0,0},{0,0},{0,432},
		{0,0},{2312,1736},{0,0},{0,432},{0,0},{0,432},{0,0},{0,0}
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_isz = {
	.i4OffsetX	= 80,
	.i4OffsetY	= 32,
	.i4PitchX	= 16,
	.i4PitchY	= 16,
	.i4PairNum	= 4,
	.i4SubBlkW	= 16,
	.i4SubBlkH	= 4,
	.iMirrorFlip = 3,
	.i4PosL = {
		{93, 35}, {93, 37}, {85, 43}, {85, 45},
	},
	.i4PosR = {
		{92, 35}, {92, 37}, {84, 43}, {84, 45},
	},
	.i4BlockNumX = 288,
	.i4BlockNumY = 217,
	.i4FullRawH = 6944,
	.i4FullRawW = 9248,
	.i4ModeIndex = 3, /*HVBin 2; VBin 3*/
	.sPDMapInfo[0] = {
		.i4PDPattern = 2, /*1: Dense; 2: Sparse LR interleaved; 3: Sparse LR non interleaved*/
		.i4BinFacX = 0, /*for Dense*/
		.i4BinFacY = 0,
		.i4PDRepetition = 4,
		.i4PDOrder = {1},
	},
	.i4Crop = {
		{0,0}, {0,0},{0,432},{0,0},{0,0},{0,0},{0,0},{0,432},
		{0,0},{2312,1736},{0,0},{0,432},{0,0},{0,432},{0,0},{0,0}
	},

};


static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4624,
			.vsize = 3472,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1136,
			.vsize = 860,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4624,
			.vsize = 2608,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1136,
			.vsize = 652,
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
			.hsize = 1920,
			.vsize = 1080,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
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

static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4624,
			.vsize = 2608,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1136,
			.vsize = 652,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},

};

static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 9248,
			.vsize = 6944,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4624,
			.vsize = 3472,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 576,
			.vsize = 868,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus9[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4624,
			.vsize = 2608,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4624,
			.vsize = 2608,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1136,
			.vsize = 652,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus10[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4624,
			.vsize = 3472,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4624,
			.vsize = 3472,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1136,
			.vsize = 860,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus11[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1280,
			.vsize = 720,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

static struct subdrv_mode_struct mode_struct[] = {
	// preview mode 0
	// M1_4624x3472_Bin_30fps + PD 1136 x 860
	// Tline = 7.7083us
	// VB = 6.59ms
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = duchampov64b40wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_preview_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = duchampov64b40wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 115200000,
		.linelength = 888,
		.framelength = 4319,
		.max_framerate = 300,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 9248,
			.h0_size = 6944,
			.scale_w = 4624,
			.scale_h = 3472,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4624,
			.h1_size = 3472,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4624,
			.h2_tg_size = 3472,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1470,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// capture mode 1: same as preview mode
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = duchampov64b40wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov64b40wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 115200000,
		.linelength = 888,
		.framelength = 4319,
		.max_framerate = 300,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 9248,
			.h0_size = 6944,
			.scale_w = 4624,
			.scale_h = 3472,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4624,
			.h1_size = 3472,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4624,
			.h2_tg_size = 3472,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1470,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// normal_video mode 2
	// M4_4624x2608_Bin_30fps
	// PD size = 1136x652
	// Tline = 8.125us
	// VB = 17.03ms
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = duchampov64b40wide_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_normal_video_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = duchampov64b40wide_normal_video_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_normal_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 115200000,
		.linelength = 936,
		.framelength = 4102,
		.max_framerate = 300,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 0,
			.y0_offset = 864,
			.w0_size = 9248,
			.h0_size = 5216,
			.scale_w = 4624,
			.scale_h = 2608,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4624,
			.h1_size = 2608,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4624,
			.h2_tg_size = 2608,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_video,
		.ae_binning_ratio = 1470,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// hs_video mode 3 : smvr 240fps
	// M8_1920x1080_240.00fps + no PD
	// Tline = 3.333us
	// VB = 0.567ms
	// fps = 240
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = duchampov64b40wide_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_hs_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov64b40wide_hs_video_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_hs_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 115200000,
		.linelength = 384,
		.framelength = 1250,
		.max_framerate = 2400,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 0,
			.y0_offset = 864,
			.w0_size = 9248,
			.h0_size = 5216,
			.scale_w = 2312,
			.scale_h = 1304,
			.x1_offset = 196,
			.y1_offset = 112,
			.w1_size = 1920,
			.h1_size = 1080,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1920,
			.h2_tg_size = 1080,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1470,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// slim_video mode 4: same as preview mode
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = duchampov64b40wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov64b40wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 115200000,
		.linelength = 888,
		.framelength = 4319,
		.max_framerate = 300,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 9248,
			.h0_size = 6944,
			.scale_w = 4624,
			.scale_h = 3472,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4624,
			.h1_size = 3472,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4624,
			.h2_tg_size = 3472,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1470,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom1 mode 5: same as preview mode
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = duchampov64b40wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov64b40wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 115200000,
		.linelength = 888,
		.framelength = 4319,
		.max_framerate = 300,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 9248,
			.h0_size = 6944,
			.scale_w = 4624,
			.scale_h = 3472,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4624,
			.h1_size = 3472,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4624,
			.h2_tg_size = 3472,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1470,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom2 mode 6 : smvr 120fps
	// M7_1920x1080_120.00fps
	// no PD
	// Tline = 4.236us
	// VB = 3.75ms
	{
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = duchampov64b40wide_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_custom2_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov64b40wide_custom2_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_custom2_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 115200000,
		.linelength = 488,
		.framelength = 1966,
		.max_framerate = 1200,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 0,
			.y0_offset = 864,
			.w0_size = 9248,
			.h0_size = 5216,
			.scale_w = 2312,
			.scale_h = 1304,
			.x1_offset = 196,
			.y1_offset = 112,
			.w1_size = 1920,
			.h1_size = 1080,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1920,
			.h2_tg_size = 1080,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1470,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom3 mode 7 : 60fps video
	// M5_4624x2608_Bin_60fps
	// PD size = 1136x652
	// Tline = 6.04167us
	// VB = 0.955ms
	{
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = duchampov64b40wide_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_custom3_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov64b40wide_custom3_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_custom3_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 115200000,
		.linelength = 696,
		.framelength = 2748,
		.max_framerate = 600,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 0,
			.y0_offset = 864,
			.w0_size = 9248,
			.h0_size = 5216,
			.scale_w = 4624,
			.scale_h = 2608,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4624,
			.h1_size = 2608,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4624,
			.h2_tg_size = 2608,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_video,
		.ae_binning_ratio = 1470,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom4 mode 8 : fullsize
	// M3_9248x6944_15fps
	// PD size = 1136x1720
	// Tline = 9.375us
	// VB = 1.565ms
	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = duchampov64b40wide_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_custom4_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = duchampov64b40wide_custom4_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_custom4_setting),
		.hdr_mode = PARAM_UNDEFINED,
		.pclk = 115200000,
		.linelength = 1080,
		.framelength = 7112,
		.max_framerate = 150,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 9248,
			.h0_size = 6944,
			.scale_w = 9248,
			.scale_h = 6944,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 9248,
			.h1_size = 6944,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 9248,
			.h2_tg_size = 6944,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom5 mode 9 : ISZ
	// M2_4624x3472_crop_30fps
	// PD size = 576x868
	// Tline = 8.9583us
	// VB = 2.221ms
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = duchampov64b40wide_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_custom5_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = duchampov64b40wide_custom5_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_custom5_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 115200000,
		.linelength = 888,
		.framelength = 4319,
		.max_framerate = 300,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 2312,
			.y0_offset = 1736,
			.w0_size = 4624,
			.h0_size = 3472,
			.scale_w = 4624,
			.scale_h = 3472,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4624,
			.h1_size = 3472,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4624,
			.h2_tg_size = 3472,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom6 mode 10: same as preview mode bokeh
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = duchampov64b40wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov64b40wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 115200000,
		.linelength = 888,
		.framelength = 5398,
		.max_framerate = 240,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 9248,
			.h0_size = 6944,
			.scale_w = 4624,
			.scale_h = 3472,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4624,
			.h1_size = 3472,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4624,
			.h2_tg_size = 3472,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1470,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom7 mode 11: super night video
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = duchampov64b40wide_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_normal_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov64b40wide_normal_video_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_normal_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 115200000,
		.linelength = 936,
		.framelength = 5126,
		.max_framerate = 240,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 0,
			.y0_offset = 864,
			.w0_size = 9248,
			.h0_size = 5216,
			.scale_w = 4624,
			.scale_h = 2608,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4624,
			.h1_size = 2608,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4624,
			.h2_tg_size = 2608,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_video,
		.ae_binning_ratio = 1470,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom8 mode 12: 2x bokeh
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = duchampov64b40wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov64b40wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 115200000,
		.linelength = 888,
		.framelength = 4319,
		.max_framerate = 300,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 9248,
			.h0_size = 6944,
			.scale_w = 4624,
			.scale_h = 3472,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4624,
			.h1_size = 3472,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4624,
			.h2_tg_size = 3472,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1470,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom9 mode 13: 2exp stagger hdr video
	// M6_4624x2608_Bin_HDR2_30fps
	// PD size = 1136x652
	// Tline = 6.04167us
	// VB = 0.955ms
	{
		.frame_desc = frame_desc_cus9,
		.num_entries = ARRAY_SIZE(frame_desc_cus9),
		.mode_setting_table = duchampov64b40wide_custom9_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_custom9_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = duchampov64b40wide_custom9_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_custom9_setting),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 115200000,
		.linelength = 696,
		.framelength = 2758 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 2608 * 2,
		.read_margin = 10 * 2,
		.framelength_step = 2 * 2,
		.coarse_integ_step = 2 * 2,
		.min_exposure_line = 4 * 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4 * 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4 * 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 2608 * 2,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 0,
			.y0_offset = 864,
			.w0_size = 9248,
			.h0_size = 5216,
			.scale_w = 4624,
			.scale_h = 2608,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4624,
			.h1_size = 2608,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4624,
			.h2_tg_size = 2608,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_video,
		.ae_binning_ratio = 1470,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom10 mode 14: 2exp stagger hdr capture
	// M9_4624x3472_Bin_HDR2_22fps
	// PD size = 1136x860
	// Tline = 6.04167us
	// VB = 0.955ms
	{
		.frame_desc = frame_desc_cus10,
		.num_entries = ARRAY_SIZE(frame_desc_cus10),
		.mode_setting_table = duchampov64b40wide_custom10_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_custom10_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = duchampov64b40wide_custom10_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_custom10_setting),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 115200000,
		.linelength = 696,
		.framelength = 3592 * 2,
		.max_framerate = 230,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 3472 * 2,
		.read_margin = 10 * 2,
		.framelength_step = 2 * 2,
		.coarse_integ_step = 2 * 2,
		.min_exposure_line = 4 * 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4 * 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4 * 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 3472 * 2,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 9248,
			.h0_size = 6944,
			.scale_w = 4624,
			.scale_h = 3472,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4624,
			.h1_size = 3472,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4624,
			.h2_tg_size = 3472,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1470,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom11 mode 15: smvr 480fps
	// M10_1280x720_480.00fps
	// no PD
	// Tline = 2.6042us
	// V-bank=0.21ms
	// fps = 480.00
	{
		.frame_desc = frame_desc_cus11,
		.num_entries = ARRAY_SIZE(frame_desc_cus11),
		.mode_setting_table = duchampov64b40wide_custom11_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov64b40wide_custom11_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov64b40wide_custom11_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov64b40wide_custom11_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 115200000,
		.linelength = 300,
		.framelength = 800,
		.max_framerate = 4800,
		.mipi_pixel_rate = 1136150000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 9248,
			.full_h = 6944,
			.x0_offset = 0,
			.y0_offset = 864,
			.w0_size = 9248,
			.h0_size = 5216,
			.scale_w = 2312,
			.scale_h = 1304,
			.x1_offset = 516,
			.y1_offset = 292,
			.w1_size = 1280,
			.h1_size = 720,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1280,
			.h2_tg_size = 720,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1470,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = DUCHAMPOV64B40WIDE_SENSOR_ID,
	.reg_addr_sensor_id = {0x300A, 0x300B},
	.i2c_addr_table = {0x20, 0xFF}, // TBD
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {9248, 6944},
	.mirror = IMAGE_HV_MIRROR,

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
	.dig_gain_min = BASEGAIN * 1,
	.dig_gain_max = BASEGAIN * 15.99,
	.dig_gain_step = 1,  //If the value is 0, SENSOR_FEATURE_SET_MULTI_DIG_GAIN is disabled
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 15.9375,
	.ana_gain_type = 1,
	.ana_gain_step = 4,
	.ana_gain_table = duchampov64b40wide_ana_gain_table,
	.ana_gain_table_size = sizeof(duchampov64b40wide_ana_gain_table),
	.min_gain_iso = 50,
	.exposure_def = 0x3D0,
	.exposure_min = 4,
	.exposure_max = 0xFFFFFF - 30,
	.exposure_step = 2,
	.exposure_margin = 30,

	.frame_length_max = 0xFFFFFF,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 115500,

	.pdaf_type = PDAF_SUPPORT_CAMSV,
	.hdr_type = HDR_SUPPORT_STAGGER_FDOL,
	.seamless_switch_support = TRUE,
	.temperature_support = TRUE,

	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.s_cali = set_sensor_cali,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = PARAM_UNDEFINED, // TBD
	.reg_addr_exposure = {
		{0x3500, 0x3501, 0x3502},
		{0x3580, 0x3581, 0x3582},
		{0x3540, 0x3541, 0x3542}
	},
	.long_exposure_support = FALSE,
	.reg_addr_exposure_lshift = PARAM_UNDEFINED,
	.reg_addr_ana_gain = {
		{0x3508, 0x3509},
		{0x3588, 0x3589},
		{0x3548, 0x3549}
	},
	.reg_addr_dig_gain = {
		{0x350A, 0x350B, 0x350C},
		{0x358A, 0x358B, 0x358C},
		{0x354A, 0x354B, 0x354C},
	},
	.reg_addr_frame_length = {0x3840, 0x380E, 0x380F},
	.reg_addr_temp_en = 0x4D12,
	.reg_addr_temp_read = 0x4D13,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = 0x387F, // To be verified

	.mi_enable_async = 1, // enable async setting
	.mi_disable_set_dummy = 1, // disable set dummy
	.s_mi_init_setting = set_init_setting,
	.mi_evaluate_frame_rate_by_scenario = evaluate_frame_rate_by_scenario,
	.init_setting_table = duchampov64b40wide_init_setting_sunny,
	.init_setting_len = ARRAY_SIZE(duchampov64b40wide_init_setting_sunny),
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
	.vsync_notify = vsync_notify,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_AFVDD, 2800000, 1}, // pmic_ldo for afvdd
	{HW_ID_DVDD,  1800000, 1}, // pmic_ldo for vcam_ldo
	{HW_ID_MCLK,  24,      0},
	{HW_ID_RST,   0,       1},
	{HW_ID_AVDD,  2800000, 1}, // pmic_ldo for avdd
	{HW_ID_DVDD1, 1100000, 1}, // pmic_ldo for dvdd1
	{HW_ID_DOVDD, 1800000, 1}, // pmic_ldo for dovdd
	{HW_ID_MCLK_DRIVING_CURRENT, 6, 5},
	{HW_ID_RST,   1,       7}
};

const struct subdrv_entry duchampov64b40wide_mipi_raw_entry = {
	.name = "duchampov64b40wide_mipi_raw",
	.id = DUCHAMPOV64B40WIDE_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

/* FUNCTION */

static void set_init_setting(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	switch (ctx->s_ctx.mi_vendor_id) {
	case MI_VENDOR_SUNNY:
		i2c_table_write(ctx, duchampov64b40wide_init_setting_sunny,
			ARRAY_SIZE(duchampov64b40wide_init_setting_sunny));
		DRV_LOG_MUST(ctx, "vendor id(0x%02x), use sunny setting!",
			ctx->s_ctx.mi_vendor_id);
		break;
	case MI_VENDOR_AAC:
		i2c_table_write(ctx, duchampov64b40wide_init_setting_aac,
			ARRAY_SIZE(duchampov64b40wide_init_setting_aac));
		DRV_LOG_MUST(ctx, "vendor id(0x%02x), use aac setting!",
			ctx->s_ctx.mi_vendor_id);
		break;
	default:
		i2c_table_write(ctx, duchampov64b40wide_init_setting_sunny,
			ARRAY_SIZE(duchampov64b40wide_init_setting_sunny));
		DRV_LOG_MUST(ctx, "unknown vendor id(0x%02x), use sunny setting!",
			ctx->s_ctx.mi_vendor_id);
		break;
	}
}

static u32 evaluate_frame_rate_by_scenario(void *arg, enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 framerate)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u32 framerateRet = framerate;

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_CUSTOM3:
		if (framerate == 600) {
			framerateRet = 602;
		}
		break;
	default:
		break;
	}

	DRV_LOG(ctx, "input/output:%d/%d scenario_id:%d", framerate, framerateRet, scenario_id);
	return framerateRet;
}

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
			size = info[idx].pdc_size;
			addr = info[idx].sensor_reg_addr_pdc;
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set PDC calibration data done.");
		}
	}

	/* QSC data */
	support = info[idx].qsc_support;
	if (support) {
		pbuf = info[idx].preload_qsc_table;
		if (pbuf != NULL) {
			size = info[idx].qsc_size;
			addr = info[idx].sensor_reg_addr_qsc;
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set QSC calibration data done.");
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

static void duchampov64b40wide_set_dummy(struct subdrv_ctx *ctx)
{
	// bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	if (ctx->s_ctx.mi_disable_set_dummy) {
		return;
	}

	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 1);
	write_frame_length(ctx, ctx->frame_length);
	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 0);

	commit_i2c_buffer(ctx);
}

static int duchampov64b40wide_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *)para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = (enum SENSOR_SCENARIO_ID_ENUM)*feature_data;
	u32 framerate = *(feature_data + 1);
	u32 frame_length;

	if (ctx->s_ctx.mi_evaluate_frame_rate_by_scenario) {
		framerate = ctx->s_ctx.mi_evaluate_frame_rate_by_scenario((void *)ctx, scenario_id, framerate);
	}

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
		duchampov64b40wide_set_dummy(ctx);

	return ERROR_NONE;
}

static u16 get_gain2reg(u32 gain)
{
	return gain * 256 / BASEGAIN;
}

static int duchampov64b40wide_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	enum IMGSENSOR_HDR_MODE_ENUM src_hdr, tagget_hdr;
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

	src_hdr = ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode;
	tagget_hdr = ctx->s_ctx.mode[scenario_id].hdr_mode;
	exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
	ctx->is_seamless = TRUE;
	update_mode_info(ctx, scenario_id);

	i2c_table_write(ctx, addr_data_pair_seamless_switch_step1_duchampov64b40wide,
		ARRAY_SIZE(addr_data_pair_seamless_switch_step1_duchampov64b40wide));
	i2c_table_write(ctx,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);
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
	i2c_table_write(ctx, addr_data_pair_seamless_switch_step2_duchampov64b40wide,
		ARRAY_SIZE(addr_data_pair_seamless_switch_step2_duchampov64b40wide));

	if (src_hdr == HDR_RAW_STAGGER) {
		i2c_table_write(ctx, addr_data_pair_seamless_switch_step3_duchampov64b40wide_shdr2linear,
			ARRAY_SIZE(addr_data_pair_seamless_switch_step3_duchampov64b40wide_shdr2linear));
	} else {
		i2c_table_write(ctx, addr_data_pair_seamless_switch_step3_duchampov64b40wide_normal,
			ARRAY_SIZE(addr_data_pair_seamless_switch_step3_duchampov64b40wide_normal));
	}

	ctx->is_seamless = FALSE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->fast_mode_on = TRUE;
	DRV_LOG(ctx, "X: set seamless switch done\n");
	return ERROR_NONE;
}

static int duchampov64b40wide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	switch (mode) {
	case 5:
		subdrv_i2c_wr_u8(ctx, 0x50c1, 0x01);
		subdrv_i2c_wr_u8(ctx, 0x50c2, 0x01);
		break;
	default:
		break;
	}

	if (mode != ctx->test_pattern)
		switch (ctx->test_pattern) {
		case 5:
			subdrv_i2c_wr_u8(ctx, 0x50c1, 0x01);
			subdrv_i2c_wr_u8(ctx, 0x50c2, 0x01);
			break;
		default:
			break;
		}

	ctx->test_pattern = mode;
	return ERROR_NONE;
}

static u16 duchampov64b40wide_dgain2reg(struct subdrv_ctx *ctx, u32 dgain)
{
	return dgain; // digitalRealGain * 1024
}

static int duchampov64b40wide_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	int i = 0;
	u16 rg_gains[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	u64 *feature_data = (u64 *)para;
	u32 *gains = (u32 *)(*feature_data);
	u16 exp_cnt = (u16) (*(feature_data + 1));

	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF) {
		set_multi_dig_gain_in_lut(ctx, gains, exp_cnt);
		return 0;
	}
	// skip if no porting digital gain
	if (!ctx->s_ctx.reg_addr_dig_gain[0].addr[0])
		return 0;

	if (exp_cnt > ARRAY_SIZE(ctx->dig_gain)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->dig_gain));
		exp_cnt = ARRAY_SIZE(ctx->dig_gain);
	}
	for (i = 0; i < exp_cnt; i++) {
		/* check boundary of gain */
		gains[i] = max(gains[i], ctx->s_ctx.dig_gain_min);
		gains[i] = min(gains[i], ctx->s_ctx.dig_gain_max);
		gains[i] = duchampov64b40wide_dgain2reg(ctx, gains[i]);
	}

	/* restore gain */
	memset(ctx->dig_gain, 0, sizeof(ctx->dig_gain));
	for (i = 0; i < exp_cnt; i++)
		ctx->dig_gain[i] = gains[i];

	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);

	/* write gain */
	switch (exp_cnt) {
	case 1:
		rg_gains[0] = gains[0];
		break;
	case 2:
		rg_gains[0] = gains[0];
		rg_gains[2] = gains[1];
		break;
	case 3:
		rg_gains[0] = gains[0];
		rg_gains[1] = gains[1];
		rg_gains[2] = gains[2];
		break;
	default:
		break;
	}
	for (i = 0;
	     (i < ARRAY_SIZE(rg_gains)) && (i < ARRAY_SIZE(ctx->s_ctx.reg_addr_dig_gain));
	     i++) {
		if (!rg_gains[i])
			continue; // skip zero gain setting

		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[0]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[0],
				(rg_gains[i] >> 10) & 0x0F);
		}
		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[1]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[1],
				(rg_gains[i] >> 2) & 0xFF);
		}
		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[2]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[2],
				(rg_gains[i] << 6) & 0xC0);
		}
	}

	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}

	DRV_LOG(ctx, "dgain reg[lg/mg/sg]: 0x%x 0x%x 0x%x\n",
		rg_gains[0], rg_gains[1], rg_gains[2]);
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

static int vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt)
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

