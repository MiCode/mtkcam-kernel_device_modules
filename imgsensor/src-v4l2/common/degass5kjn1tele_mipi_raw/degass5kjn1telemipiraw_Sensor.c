// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 degass5kjn1telemipiraw_Sensor.c
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
#include "degass5kjn1telemipiraw_Sensor.h"

static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static void degass5kjn1tele_set_dummy(struct subdrv_ctx *ctx);
static int degass5kjn1tele_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static u16 get_gain2reg(u32 gain);
static int  degass5kjn1tele_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static void set_mi_init_setting_seq(void *arg);
static int tele_vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt);

#define SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE                 1
#define SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LEICA           2

/* STRUCT */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, degass5kjn1tele_set_test_pattern},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO, degass5kjn1tele_set_max_framerate_by_scenario},
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		// .header_id = 0x010B00FF,
		// .addr_header_id = 0x0000000B,
		.i2c_write_id = 0xA8,

		.pdc_support = TRUE,
		.pdc_size = 720,
		.addr_pdc = 0x12D2,
		.sensor_reg_addr_pdc = 0x5F80,

	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX	= 8,
	.i4OffsetY	= 8,
	.i4PitchX	= 8,
	.i4PitchY	= 8,
	.i4PairNum	= 4,
	.i4SubBlkW	= 8,
	.i4SubBlkH	= 2,
	.iMirrorFlip = 0,
	.i4PosL =   {{9, 8},{11, 11},{15, 12},{13, 15}},
	.i4PosR =   {{8, 8},{10, 11},{14, 12},{12, 15}},
	.i4BlockNumX = 504,
	.i4BlockNumY = 382,
	.i4FullRawW = 4080,
	.i4FullRawH = 3072,
	.i4VCPackNum = 1,
	.i4ModeIndex = 2,//HVbin
	.sPDMapInfo[0] = {
		.i4PDPattern =3,
		.i4BinFacX = 0,//
		.i4BinFacY = 0,
		.i4PDRepetition = 2,
		.i4PDOrder = {1,0},
	},
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <slim_video>
		{0,0}, {0,0},{0,388},{60,228},{0,0},
		// <cust1> <cust2> <cust3> <cust4> <cust5>
		{120,456},{0,0},{0,0},{0,0},{0,0},
		// <cust6> <cust7> <cust8>
		{0,0},{0,0},{0,0}
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_video = {
	.i4OffsetX	= 8,
	.i4OffsetY	= 8,
	.i4PitchX	= 8,
	.i4PitchY	= 8,
	.i4PairNum	= 4,
	.i4SubBlkW	= 8,
	.i4SubBlkH	= 2,
	.iMirrorFlip = 0,
	.i4PosL =   {{9, 8},{11, 11},{15, 12},{13, 15}},
	.i4PosR =   {{8, 8},{10, 11},{14, 12},{12, 15}},
	.i4BlockNumX = 504,
	.i4BlockNumY = 286,
	.i4FullRawW = 4080,
	.i4FullRawH = 3072,
	.i4VCPackNum = 1,
	.i4ModeIndex = 2,//HVbin
	.sPDMapInfo[0] = {
		.i4PDPattern =3,
		.i4BinFacX = 0,//
		.i4BinFacY = 0,
		.i4PDRepetition = 2,
		.i4PDOrder = {1,0},
	},
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <slim_video>
		{0,0}, {0,0},{0,388},{60,228},{0,0},
		// <cust1> <cust2> <cust3> <cust4> <cust5>
		{120,456},{0,0},{0,0},{0,0},{0,0},
		// <cust6> <cust7> <cust8>
		{0,0},{0,0},{0,0}
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_smvr = {
	.i4OffsetX	= 0,
	.i4OffsetY	= 4,
	.i4PitchX	= 4,
	.i4PitchY	= 4,
	.i4PairNum	= 1,
	.i4SubBlkW	= 4,
	.i4SubBlkH	= 4,
	.iMirrorFlip = 0,
	.i4PosL =  {{0,4}},
	.i4PosR = {{1,4}},
	.i4BlockNumX = 480,
	.i4BlockNumY = 268,
	.i4FullRawW = 2040,
	.i4FullRawH = 1536,
	.i4ModeIndex = 2,//HVbin
	.sPDMapInfo[0] = {
		.i4PDPattern =3,
		.i4BinFacX = 0,//
		.i4BinFacY = 0,
		.i4PDRepetition = 2,
		.i4PDOrder = {1,0},
	},
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <slim_video>
		{0,0}, {0,0},{0,388},{60,228},{0,0},
		// <cust1> <cust2> <cust3> <cust4> <cust5>
		{120,456},{0,0},{0,0},{0,0},{0,0},
		// <cust6> <cust7> <cust8>
		{0,0},{0,0},{0,0}
	},
};

// mode 0: 4080*3072@30fps, normal preview + pd
static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 504,
			.vsize = 3056,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

// mode 1: same as preview mode + pd
static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
    {
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 504,
			.vsize = 3056,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

// mode 2: 4080x2296@30fps, noraml video + pd
static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 2296,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 504,
			.vsize = 2288,
			.user_data_desc = VC_PDAF_STATS,
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
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 480,
			.vsize = 536,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

// mode 4: same as preview mode + pd
static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 504,
			.vsize = 3056,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

// mode 5: 3840x2160@60fps, VHDR(m-stream)/ 60fps video + pd
static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3840,
			.vsize = 2160,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 480,
			.vsize = 2144,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};

// mode 8 custom4: 8160x6144@10fps sw remosaic + non-pd
static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 8160,
			.vsize = 6144,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

// mode 9 custom5: 4080x3072@10fps sw remosaic + non-pd
static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

static struct subdrv_mode_struct mode_struct[] = {
	// mode 0: 4080x3072@30fps, normal preview + pd
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = degass5kjn1tele_preview_setting,
		.mode_setting_len = ARRAY_SIZE(degass5kjn1tele_preview_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = degass5kjn1tele_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(degass5kjn1tele_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 4240,
		.framelength = 4372,
		.max_framerate = 300,
		.mipi_pixel_rate = 801600000,
		.csi_param = {
			.not_fixed_trail_settle = 1,
			.not_fixed_dphy_settle = 1,
			.dphy_data_settle = 0x1a,
			.dphy_clk_settle = 0x1a,
			.dphy_trail = 0x10,
		},
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8160,
			.h0_size = 6144,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// mode 1: same as preview mode
	{
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = degass5kjn1tele_preview_setting,
		.mode_setting_len = ARRAY_SIZE(degass5kjn1tele_preview_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LEICA,
		.seamless_switch_mode_setting_table = degass5kjn1tele_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(degass5kjn1tele_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 4240,
		.framelength = 4372,
		.max_framerate = 300,
		.mipi_pixel_rate = 801600000,
		.csi_param = {
			.not_fixed_trail_settle = 1,
			.not_fixed_dphy_settle = 1,
			.dphy_data_settle = 0x1a,
			.dphy_clk_settle = 0x1a,
			.dphy_trail = 0x10,
		},
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8160,
			.h0_size = 6144,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// mode 2: 4080x2296@30fps, noraml video + pd
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = degass5kjn1tele_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(degass5kjn1tele_normal_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 4240,
		.framelength = 4400,
		.max_framerate = 300,
		.mipi_pixel_rate = 801600000,
		.csi_param = {
			.not_fixed_trail_settle = 1,
			.not_fixed_dphy_settle = 1,
			.dphy_data_settle = 0x1a,
			.dphy_clk_settle = 0x1a,
			.dphy_trail = 0x10,
		},
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 776,
			.w0_size = 8160,
			.h0_size = 4592,
			.scale_w = 4080,
			.scale_h = 2296,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 2296,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2296,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_video,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// mode 3: 1920x1080@240fps, SMVR + non-pd,
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = degass5kjn1tele_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(degass5kjn1tele_hs_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 600000000,
		.linelength = 2064,
		.framelength = 1210,
		.max_framerate = 2400,
		.mipi_pixel_rate = 801600000,
		.csi_param = {
			.not_fixed_trail_settle = 1,
			.not_fixed_dphy_settle = 1,
			.dphy_data_settle = 0x1a,
			.dphy_clk_settle = 0x1a,
			.dphy_trail = 0x10,
		},
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 240,
			.y0_offset = 912,
			.w0_size = 7680,
			.h0_size = 4320,
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
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_smvr,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// mode 4: same as preview mode + pd
	{
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table = degass5kjn1tele_preview_setting,
		.mode_setting_len = ARRAY_SIZE(degass5kjn1tele_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 4240,
		.framelength = 4372,
		.max_framerate = 300,
		.mipi_pixel_rate = 801600000,
		.csi_param = {
			.not_fixed_trail_settle = 1,
			.not_fixed_dphy_settle = 1,
			.dphy_data_settle = 0x1a,
			.dphy_clk_settle = 0x1a,
			.dphy_trail = 0x10,
		},
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8160,
			.h0_size = 6144,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// mode 5: 3840x3072@60fps, VHDR(m-stream)/ 60fps video + pd
	{
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = degass5kjn1tele_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(degass5kjn1tele_custom1_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 4096,
		.framelength = 2272,
		.max_framerate = 600,
		.mipi_pixel_rate = 801600000,
		.csi_param = {
			.not_fixed_trail_settle = 1,
			.not_fixed_dphy_settle = 1,
			.dphy_data_settle = 0x1a,
			.dphy_clk_settle = 0x1a,
			.dphy_trail = 0x10,
		},
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 240,
			.y0_offset = 912,
			.w0_size = 7680,
			.h0_size = 4320,
			.scale_w = 3840,
			.scale_h = 2160,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 3840,
			.h1_size = 2160,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3840,
			.h2_tg_size = 2160,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// mode 6 custom2: same as preview mode + pd
	{
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table = degass5kjn1tele_preview_setting,
		.mode_setting_len = ARRAY_SIZE(degass5kjn1tele_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 4240,
		.framelength = 4372,
		.max_framerate = 300,
		.mipi_pixel_rate = 801600000,
		.csi_param = {
			.not_fixed_trail_settle = 1,
			.not_fixed_dphy_settle = 1,
			.dphy_data_settle = 0x1a,
			.dphy_clk_settle = 0x1a,
			.dphy_trail = 0x10,
		},
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8160,
			.h0_size = 6144,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// mode 7 custom3: same as preview mode + pd
	{
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table = degass5kjn1tele_preview_setting,
		.mode_setting_len = ARRAY_SIZE(degass5kjn1tele_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 4240,
		.framelength = 4372,
		.max_framerate = 300,
		.mipi_pixel_rate = 801600000,
		.csi_param = {
			.not_fixed_trail_settle = 1,
			.not_fixed_dphy_settle = 1,
			.dphy_data_settle = 0x1a,
			.dphy_clk_settle = 0x1a,
			.dphy_trail = 0x10,
		},
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8160,
			.h0_size = 6144,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// mode 8 custom4: 8160x6144@10fps sw remosaic + non-pd
	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = degass5kjn1tele_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(degass5kjn1tele_custom4_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = degass5kjn1tele_custom4_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(degass5kjn1tele_custom4_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 8688,
		.framelength = 6400,
		.max_framerate = 100,
		.mipi_pixel_rate = 801600000,
		.csi_param = {
			.not_fixed_trail_settle = 1,
			.not_fixed_dphy_settle = 1,
			.dphy_data_settle = 0x1a,
			.dphy_clk_settle = 0x1a,
			.dphy_trail = 0x10,
		},
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 16,
		.ana_gain_min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8160,
			.h0_size = 6144,
			.scale_w = 8160,
			.scale_h = 6144,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 8160,
			.h1_size = 6144,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 8160,
			.h2_tg_size = 6144,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr,  // just for mivi3 sw remosaic
	},
	// mode 9 custom5:fullsize quad bayer crop
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = degass5kjn1tele_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(degass5kjn1tele_custom5_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = degass5kjn1tele_custom5_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(degass5kjn1tele_custom5_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 8192,
		.framelength = 3248,
		.max_framerate = 210,
		.mipi_pixel_rate = 801600000,
		.csi_param = {
			.not_fixed_trail_settle = 1,
			.not_fixed_dphy_settle = 1,
			.dphy_data_settle = 0x1a,
			.dphy_clk_settle = 0x1a,
			.dphy_trail = 0x10,
		},
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 16,
		.ana_gain_min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 2040,
			.y0_offset = 1536,
			.w0_size = 4080,
			.h0_size = 3072,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gr,
	},
	// mode 10 custom6: 24fps bokeh
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = degass5kjn1tele_preview_setting,
		.mode_setting_len = ARRAY_SIZE(degass5kjn1tele_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 4240,
		.framelength = 5465,
		.max_framerate = 240,
		.mipi_pixel_rate = 801600000,
		.csi_param = {
			.not_fixed_trail_settle = 1,
			.not_fixed_dphy_settle = 1,
			.dphy_data_settle = 0x1a,
			.dphy_clk_settle = 0x1a,
			.dphy_trail = 0x10,
		},
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8160,
			.h0_size = 6144,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// mode 11 custom7:fullsize quad bayer crop leica
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = degass5kjn1tele_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(degass5kjn1tele_custom5_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LEICA,
		.seamless_switch_mode_setting_table = degass5kjn1tele_custom5_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(degass5kjn1tele_custom5_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 8192,
		.framelength = 3248,
		.max_framerate = 210,
		.mipi_pixel_rate = 801600000,
		.csi_param = {
			.not_fixed_trail_settle = 1,
			.not_fixed_dphy_settle = 1,
			.dphy_data_settle = 0x1a,
			.dphy_clk_settle = 0x1a,
			.dphy_trail = 0x10,
		},
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 16,
		.ana_gain_min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 2040,
			.y0_offset = 1536,
			.w0_size = 4080,
			.h0_size = 3072,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gr,
	},
	// mode 12 custom8: 24fps leica
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = degass5kjn1tele_preview_setting,
		.mode_setting_len = ARRAY_SIZE(degass5kjn1tele_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 4240,
		.framelength = 5465,
		.max_framerate = 240,
		.mipi_pixel_rate = 801600000,
		.csi_param = {
			.not_fixed_trail_settle = 1,
			.not_fixed_dphy_settle = 1,
			.dphy_data_settle = 0x1a,
			.dphy_clk_settle = 0x1a,
			.dphy_trail = 0x10,
		},
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8160,
			.h0_size = 6144,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = DEGASS5KJN1TELE_SENSOR_ID,
	.reg_addr_sensor_id = {0x0000, 0x0001},
	.i2c_addr_table = {0x5A, 0xFF}, // TBD
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_16,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {8160, 6144},
	.mirror = IMAGE_NORMAL, // TBD

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 64,
	.ana_gain_type = 1,
	.ana_gain_step = 1,
	.ana_gain_table = degass5kjn1tele_ana_gain_table,
	.ana_gain_table_size = sizeof(degass5kjn1tele_ana_gain_table),
	.min_gain_iso = 50,
	.exposure_def = 0x3D0,
	.exposure_min = 6,
	.exposure_max = 0x7FFF800-12,
	.exposure_step = 2,
	.exposure_margin = 12,

	.frame_length_max = 0x7FFF800,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 153800,

	.pdaf_type = PDAF_SUPPORT_CAMSV,
	.hdr_type = HDR_SUPPORT_NA,
	.seamless_switch_support = FALSE,
	.temperature_support = TRUE,

	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.s_mi_init_seq = set_mi_init_setting_seq,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = PARAM_UNDEFINED, // TBD
	.reg_addr_exposure = {
		{0x0202, 0x0203}
	},
	.long_exposure_support = TRUE,
	.mi_long_exposure_type = 1,
	.reg_addr_exposure_lshift = 0x0704,
	.reg_addr_framelength_lshift = 0x0702,
	.reg_addr_ana_gain = {
		{0x0204, 0x0205}
	},
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_temp_read = 0x0020,
//	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = 0x0005, // To be verified

	.init_setting_table = degass5kjn1tele_init_setting,
	.init_setting_len = ARRAY_SIZE(degass5kjn1tele_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 1,
	.chk_s_off_end = 0,
	/* custom stream control delay timing for hw limitation (ms) */
	.custom_stream_ctrl_delay = 5,
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
	.vsync_notify = tele_vsync_notify,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_AFVDD, 2800000, 3},
	{HW_ID_RST,   0,       0},
	{HW_ID_DOVDD, 1800000, 0},
	{HW_ID_DVDD1, 1800000, 1},//vcam_ldo
	{HW_ID_DVDD,  1,       1},
	{HW_ID_AVDD,  2800000, 1},
	{HW_ID_RST,   1,       1},
	{HW_ID_MCLK,  24,      0},
	{HW_ID_MCLK_DRIVING_CURRENT, 6, 10},
};

const struct subdrv_entry degass5kjn1tele_mipi_raw_entry = {
	.name = "degass5kjn1tele_mipi_raw",
	.id = DEGASS5KJN1TELE_SENSOR_ID,
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
	temperature = subdrv_i2c_rd_u16(ctx, ctx->s_ctx.reg_addr_temp_read);
	temperature = temperature / 256;

	DRV_LOG(ctx, "temperature: %d degrees\n", temperature);
	return temperature;
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

static void degass5kjn1tele_set_dummy(struct subdrv_ctx *ctx)
{
	// bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 1);
	write_frame_length(ctx, ctx->frame_length);
	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 0);

	commit_i2c_buffer(ctx);
}

static int degass5kjn1tele_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
		degass5kjn1tele_set_dummy(ctx);

	return ERROR_NONE;
}

static u16 get_gain2reg(u32 gain)
{
	return gain * 32 / BASEGAIN;
}

static int degass5kjn1tele_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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

static void set_mi_init_setting_seq(void *arg)
{
  	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;


	DRV_LOG_MUST(ctx, "Enter %s\n", __FUNCTION__);
	subdrv_i2c_wr_u16(ctx, 0x6028, 0x4000);
	subdrv_i2c_wr_u16(ctx, 0x0000, 0x0003);
	subdrv_i2c_wr_u16(ctx, 0x0000, 0x38E1);
	subdrv_i2c_wr_u16(ctx, 0x001E, 0x0007);
	subdrv_i2c_wr_u16(ctx, 0x6028, 0x4000);
	subdrv_i2c_wr_u16(ctx, 0x6010, 0x0001);
	mdelay(5);
	subdrv_i2c_wr_u16(ctx, 0x6226, 0x0001);
	mdelay(10);
}

static int tele_vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt)
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


