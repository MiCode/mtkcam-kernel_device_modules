// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 duchampov50ewidemipiraw_Sensor.c
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
#include "duchampov50ewidemipiraw_Sensor.h"

#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>
static bool ov50e_streamon = false;
static struct task_struct *task_thread_ov50e = NULL;
DECLARE_WAIT_QUEUE_HEAD(wq_ov50e);


static void set_init_setting(void *arg);
static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static void duchampov50ewide_set_dummy(struct subdrv_ctx *ctx);
static int duchampov50ewide_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static u16 get_gain2reg(u32 gain);
static int duchampov50ewide_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int  duchampov50ewide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int duchampov50ewide_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt);
static int get_csi_param(struct subdrv_ctx *ctx,	enum SENSOR_SCENARIO_ID_ENUM scenario_id,struct mtk_csi_param *csi_param);
static int duchampov50ewide_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int duchampov50ewide_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int duchampov50ewide_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int duchampov50ewide_set_hdr_tri_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void duchampov50ewide_set_multi_gain(struct subdrv_ctx *ctx, u32 *gains, u16 exp_cnt);


/* STRUCT */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, duchampov50ewide_set_test_pattern},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, duchampov50ewide_seamless_switch},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO, duchampov50ewide_set_max_framerate_by_scenario},
	{SENSOR_FEATURE_SET_MULTI_DIG_GAIN, duchampov50ewide_set_multi_dig_gain},
	{SENSOR_FEATURE_SET_ESHUTTER, duchampov50ewide_set_shutter},
	{SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME, duchampov50ewide_set_shutter_frame_length},
	{SENSOR_FEATURE_SET_GAIN,duchampov50ewide_set_gain},
	{SENSOR_FEATURE_SET_DUAL_GAIN,duchampov50ewide_set_hdr_tri_gain},
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
/*

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
	.i4FullRawW = 9248,*/
//	.i4ModeIndex = 3, /*HVBin 2; VBin 3*/
//	.sPDMapInfo[0] = {
//		.i4PDPattern = 2, /*1: Dense; 2: Sparse LR interleaved; 3: Sparse LR non interleaved*/
//		.i4BinFacX = 0, /*for Dense*/
/*		.i4BinFacY = 0,
		.i4PDRepetition = 4,
		.i4PDOrder = {1},
	},
	.i4Crop = {
		{0,0}, {0,0},{0,432},{0,0},{0,0},{0,0},{0,0},{0,432},
		{0,0},{2312,1736},{0,0},{0,432},{0,0},{0,432},{0,0},{0,0}
	},

};*/


static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
/*	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1136,
			.vsize = 860,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},*/
};

static struct mtk_mbus_frame_desc_entry frame_desc_prev_14bit[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
/*	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1136,
			.vsize = 860,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},*/
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
/*	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1136,
			.vsize = 652,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},*/
};


static struct mtk_mbus_frame_desc_entry frame_desc_vid_14bit[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
/*	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1136,
			.vsize = 652,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},*/
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
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
/*	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1136,
			.vsize = 652,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},*/

};

static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 8192,
			.vsize = 6144,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
/*	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 576,
			.vsize = 868,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},*/
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus9[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
/*	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1136,
			.vsize = 652,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},*/
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus10[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
/*	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1136,
			.vsize = 860,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},*/
};

//480fps preview mode 15
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

//DCG preview mode 16
static struct mtk_mbus_frame_desc_entry frame_desc_cus12[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 3072,
			//.user_data_desc = VC_STAGGER_NE,
		},
	},
/*	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1136,
			.vsize = 860,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},*/
};

//DXG video mode 17
static struct mtk_mbus_frame_desc_entry frame_desc_cus13[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 2304,
			//.user_data_desc = VC_STAGGER_NE,
		},
	},
/*	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 1136,
			.vsize = 860,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},*/
};

//DXG video mode 17
static struct mtk_mbus_frame_desc_entry frame_desc_cus14[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 3072,
			//.user_data_desc = VC_STAGGER_NE,
		},
	},

};


//1000 base for dcg gain ratio
//static u32 duchampov50e_dcg_ratio_table_12bit[] = {4000};
//static u32 duchampov50e_dcg_ratio_table_14bit[] = {16000};


/*static struct mtk_sensor_saturation_info imgsensor_saturation_info_10bit = {
	.gain_ratio = 1000,
	.OB_pedestal = 64,
	.saturation_level = 1023,
};*/

static struct mtk_sensor_saturation_info imgsensor_saturation_info = {
	.gain_ratio = 1000,
	.OB_pedestal = 64,
	.saturation_level = 1023,
};


static struct mtk_sensor_saturation_info imgsensor_saturation_info_14bit = {
	.gain_ratio = 5000,
	.OB_pedestal = 64,
	.saturation_level = 16383,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_fake14bit = {
	.gain_ratio = 1000,
	.OB_pedestal = 1024,
	.saturation_level = 16383,
};


static struct subdrv_mode_struct mode_struct[] = {
	// preview mode 0 
	// M1 4096*3072_Bin_30fps + PD 
	// Tline =  7.893us
	// VB =  18.084ms
	// RAW 14bit
	{
		.frame_desc = frame_desc_prev_14bit,
		.num_entries = ARRAY_SIZE(frame_desc_prev_14bit),
		.mode_setting_table = duchampov50ewide_preview_setting_14bit,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_preview_setting_14bit),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = duchampov50ewide_preview_setting_14bit,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_preview_setting_14bit),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75100000,
		.linelength = 590,
		.framelength = 4223,
		.max_framerate = 300,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &imgsensor_saturation_info_fake14bit,
	},
	// capture mode 1: same as preview mode
	{
		.frame_desc = frame_desc_prev_14bit,
		.num_entries = ARRAY_SIZE(frame_desc_prev_14bit),
		.mode_setting_table = duchampov50ewide_preview_setting_14bit,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_preview_setting_14bit),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov50ewide_preview_setting_14bit,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_preview_setting_14bit),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75100000,
		.linelength = 590,
		.framelength = 4223,
		.max_framerate = 300,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &imgsensor_saturation_info_fake14bit,
	},
	// normal_video mode 2
	// Mode4_4096x2304_Bin_30fps
	// PD size = 4096x576
	// Tline = 5.33us
	// VB = 21.045ms
	// RAW 14bit
	{
		.frame_desc = frame_desc_vid_14bit,
		.num_entries = ARRAY_SIZE(frame_desc_vid_14bit),
		.mode_setting_table = duchampov50ewide_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_normal_video_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = duchampov50ewide_normal_video_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_normal_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75100000,
		.linelength = 400,
		.framelength = 6250,
		.max_framerate = 300,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 4096,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &imgsensor_saturation_info_fake14bit,
	},
	// hs_video mode 3 : smvr 240fps
	// M8_1920x1080_240.00fps + no PD
	//Tline = 3.16us
	//VB = 0.752ms
	//fps = 240
	//RAW 10bit
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = duchampov50ewide_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_hs_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov50ewide_hs_video_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_hs_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75100000,
		.linelength = 237,
		.framelength = 1318,
		.max_framerate = 2400,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 256,
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
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// slim_video mode 4: same as preview mode
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = duchampov50ewide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov50ewide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75100000,
		.linelength = 590,
		.framelength = 4223,
		.max_framerate = 300,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom1 mode 5: same as preview mode
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = duchampov50ewide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov50ewide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75100000,
		.linelength = 590,
		.framelength = 4236,
		.max_framerate = 300,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
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
		.mode_setting_table = duchampov50ewide_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom2_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov50ewide_custom2_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom2_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75100000,
		.linelength = 400,
		.framelength = 1562,
		.max_framerate = 1200,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 256,
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
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom3 mode 7 : 60fps video
	// Mode5_4096x2304_Bin_60fps
	// PD size = 4096x576
	// Tline = 5.33us
	// VB = 4.373ms
	// RAW 10 bit
	{
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = duchampov50ewide_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom3_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov50ewide_custom3_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom3_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75100000,
		.linelength = 400,
		.framelength = 3124,
		.max_framerate = 600,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 4096,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom4 mode 8 : fullsize
	// M1_8192x6144_15fps
	// PD size = 4096x1536
	// Tline = 7.893us
	// VB = 18.084ms
	// RAW 14bit
	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = duchampov50ewide_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom4_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = duchampov50ewide_custom4_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom4_setting),
		.hdr_mode = PARAM_UNDEFINED,
		.pclk = 75100000,
		.linelength = 592,
		.framelength = 8435,
		.max_framerate = 150,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 8192,
			.scale_h = 6144,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 8192,
			.h1_size = 6144,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 8192,
			.h2_tg_size = 6144,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &imgsensor_saturation_info_fake14bit,
	},
	// custom5 mode 9 : ISZ
	// M2_4624x3472_crop_30fps
	// PD size = 576x868
	// Tline = 7.893us
	// VB = 9.085ms
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = duchampov50ewide_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom5_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = duchampov50ewide_custom5_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom5_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75100000,
		.linelength = 592,
		.framelength = 4223,
		.max_framerate = 300,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1356,
			.w0_size = 4096,
			.h0_size = 3072,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &imgsensor_saturation_info_fake14bit,
	},
	// custom6 mode 10: same as preview mode bokeh
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = duchampov50ewide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov50ewide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75100000,
		.linelength = 888,
		.framelength = 5398,
		.max_framerate = 240,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom7 mode 11: super night video
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = duchampov50ewide_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_normal_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov50ewide_normal_video_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_normal_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75100000,
		.linelength = 936,
		.framelength = 5126,
		.max_framerate = 240,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom8 mode 12: 2x bokeh
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = duchampov50ewide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov50ewide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75100000,
		.linelength = 888,
		.framelength = 4319,
		.max_framerate = 300,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
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
		.mode_setting_table = duchampov50ewide_custom9_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom9_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = duchampov50ewide_custom9_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom9_setting),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 75100000,
		.linelength = 696,
		.framelength = 2758 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 2608 * 2,
		.read_margin = 10 * 2,
		.framelength_step = 2 * 2,
		.coarse_integ_step = 2 * 2,
		.min_exposure_line = 4 * 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4 * 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4 * 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 2608 * 2,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
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
		.mode_setting_table = duchampov50ewide_custom10_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom10_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = duchampov50ewide_custom10_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom10_setting),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 75100000,
		.linelength = 696,
		.framelength = 3592 * 2,
		.max_framerate = 230,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 3472 * 2,
		.read_margin = 10 * 2,
		.framelength_step = 2 * 2,
		.coarse_integ_step = 2 * 2,
		.min_exposure_line = 4 * 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4 * 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4 * 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 3472 * 2,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
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
		.mode_setting_table = duchampov50ewide_custom11_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom11_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = duchampov50ewide_custom11_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom11_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75100000,
		.linelength = 300,
		.framelength = 800,
		.max_framerate = 4800,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
		.fine_integ_line = 0,
		.delay_frame = 2,
	},
	// custom12 mode 16
	// RAW 14bit DXG
	// M1 4096*3072_Bin_30fps + PD 
	// Tline =  us
	// VB =  ms
	{
		.frame_desc = frame_desc_cus12,
		.num_entries = ARRAY_SIZE(frame_desc_cus12),
		.mode_setting_table = duchampov50ewide_custom12_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom12_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = duchampov50ewide_custom12_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom12_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 75100000,
		.linelength = 612,
		.framelength = 4084,
		.max_framerate = 300,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 4260,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &imgsensor_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 5000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = PARAM_UNDEFINED,
			.dcg_gain_table_size = PARAM_UNDEFINED,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 16,
	},
	// custom13 mode 17 
	// RAW 14bit
	// M1 4096*3072_Bin_30fps + PD 
	// Tline =  us
	// VB =  ms
	{
		.frame_desc = frame_desc_cus13,
		.num_entries = ARRAY_SIZE(frame_desc_cus13),
		.mode_setting_table = duchampov50ewide_custom13_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom13_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = duchampov50ewide_custom13_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom13_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75100000,
		.linelength = 612,
		.framelength = 4084,
		.max_framerate = 300,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 4096,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 4260,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &imgsensor_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 5000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = PARAM_UNDEFINED,
			.dcg_gain_table_size = PARAM_UNDEFINED,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 16,
	},
	// custom13 mode 18 
	// M18 4096*3072_Bin_30fps + PD 
	// Tline =  10.453us
	// VB = 1.213ms
	// RAW 14bit CMS
	{
		.frame_desc = frame_desc_cus14,
		.num_entries = ARRAY_SIZE(frame_desc_cus14),
		.mode_setting_table = duchampov50ewide_custom14_setting,
		.mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom14_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = duchampov50ewide_custom14_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(duchampov50ewide_custom14_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75100000,
		.linelength = 784,
		.framelength = 3188,
		.max_framerate = 300,
		.mipi_pixel_rate = 1708000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 3072,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1240,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &imgsensor_saturation_info_14bit,
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = DUCHAMPOV50EWIDE_SENSOR_ID,
	.reg_addr_sensor_id = {0x300A, 0x300B},
	.i2c_addr_table = {0x20, 0xFF}, // TBD
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {8192, 6144},
	.mirror = IMAGE_HV_MIRROR,

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
	.dig_gain_min = BASEGAIN * 1,
	.dig_gain_max = BASEGAIN * 15.99,
	.dig_gain_step = 1,  //If the value is 0, SENSOR_FEATURE_SET_MULTI_DIG_GAIN is disabled
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 15.9375,
	.ana_gain_type = 1,
	.ana_gain_step = 4,
	.ana_gain_table = duchampov50ewide_ana_gain_table,
	.ana_gain_table_size = sizeof(duchampov50ewide_ana_gain_table),
	.min_gain_iso = 50,
	.exposure_def = 0x3D0,
	.exposure_min = 4,
	.exposure_max = 0xFFFFFF - 30,
	.exposure_step = 2,
	.exposure_margin = 30,

	.frame_length_max = 0xFFFFFF,
	.ae_effective_frame = 3,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 115500,

//	.pdaf_type = PDAF_SUPPORT_CAMSV,
//	.hdr_type = HDR_SUPPORT_DCG,
	.hdr_type = HDR_SUPPORT_STAGGER_FDOL|HDR_SUPPORT_DCG,
	.pdaf_type = PDAF_SUPPORT_NA,
//	.hdr_type = HDR_SUPPORT_NA,
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
	.init_setting_table = duchampov50ewide_init_setting_sunny,
	.init_setting_len = ARRAY_SIZE(duchampov50ewide_init_setting_sunny),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 0,
	.chk_s_off_end = 0,

	.saturation_info = &imgsensor_saturation_info,

	//TBD
	.checksum_value = 0xAF3E324F,
};

static int read_frame_cnt(void *data)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)data;

	while(1) {
		wait_event_interruptible(wq_ov50e, ov50e_streamon || kthread_should_stop());
		if (kthread_should_stop()) {
			break;
		}
		usleep_range(30000, 30010);
		printk("read sof_cnt = %d",subdrv_i2c_rd_u8(ctx, ctx->s_ctx.reg_addr_frame_count));
	}

	printk("exit thread !");
	return 0;
}


static int duchampov50ewide_open(struct subdrv_ctx *ctx)
{
	task_thread_ov50e = kthread_run(read_frame_cnt, ctx, "ov50e");
	if (!task_thread_ov50e) {
		pr_err("Failed to create task_thread_ov50e : task_thread_ov50e\n");
		return -1;
	}

	ov50e_streamon = true;
	wake_up_interruptible(&wq_ov50e);
	
	return common_open(ctx);
}

static int duchampov50ewide_close(struct subdrv_ctx *ctx)
{
	if (task_thread_ov50e) {
		kthread_stop(task_thread_ov50e);
		task_thread_ov50e = NULL;
	}
	return common_close(ctx);
}

static struct subdrv_ops ops = {
	.get_id = common_get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = duchampov50ewide_open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = duchampov50ewide_close,
	.get_frame_desc = common_get_frame_desc,
	.get_temp = common_get_temp,
	.get_csi_param = get_csi_param,
	.update_sof_cnt = common_update_sof_cnt,
	.vsync_notify = vsync_notify,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_AFVDD, 2800000, 1}, // pmic_ldo for afvdd
	{HW_ID_DVDD,  1800000, 1}, // pmic_ldo for vcam_ldo
	{HW_ID_MCLK,  24,      0},
	{HW_ID_RST,   0,       1},
	{HW_ID_AVDD,  2800000, 1}, // pmic_ldo for avdd
	{HW_ID_DOVDD, 1800000, 1}, // pmic_ldo for dovdd
	{HW_ID_DVDD1, 1100000, 1}, // pmic_ldo for dvdd1
	{HW_ID_MCLK_DRIVING_CURRENT, 6, 5},
	{HW_ID_RST,   1,       7}
};

const struct subdrv_entry duchampov50ewide_mipi_raw_entry = {
	.name = "duchampov50ewide_mipi_raw",
	.id = DUCHAMPOV50EWIDE_SENSOR_ID,
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
		i2c_table_write(ctx, duchampov50ewide_init_setting_sunny,
			ARRAY_SIZE(duchampov50ewide_init_setting_sunny));
		DRV_LOG_MUST(ctx, "vendor id(0x%02x), use sunny setting!",
			ctx->s_ctx.mi_vendor_id);
		break;
	case MI_VENDOR_AAC:
		i2c_table_write(ctx, duchampov50ewide_init_setting_aac,
			ARRAY_SIZE(duchampov50ewide_init_setting_aac));
		DRV_LOG_MUST(ctx, "vendor id(0x%02x), use aac setting!",
			ctx->s_ctx.mi_vendor_id);
		break;
	default:
		i2c_table_write(ctx, duchampov50ewide_init_setting_sunny,
			ARRAY_SIZE(duchampov50ewide_init_setting_sunny));
		DRV_LOG_MUST(ctx, "unknown vendor id(0x%02x), use sunny setting!",
			ctx->s_ctx.mi_vendor_id);
		break;
	}
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

	return;

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
		if(ctx->current_scenario_id == 16 || ctx->current_scenario_id == 17 ){
			set_i2c_buffer(ctx, 0x5006, 0x02);
		}else{
			set_i2c_buffer(ctx, 0x5006, 0x00);
		}
	} else {
		set_i2c_buffer(ctx, 0x3208, 0x10);
		set_i2c_buffer(ctx, 0x3208, 0xA0);
	}
}

static void duchampov50ewide_set_dummy(struct subdrv_ctx *ctx)
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

static int duchampov50ewide_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
		duchampov50ewide_set_dummy(ctx);

	return ERROR_NONE;
}

static u16 get_gain2reg(u32 gain)
{
	return gain * 256 / BASEGAIN;
}

//SENSOR_FEATURE_SET_DUAL_GAIN
static int duchampov50ewide_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 gain = *(u32 *)para;
	u16 rg_gain;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	u16 temp_se = subdrv_i2c_rd_u8(ctx, 0x3506);

	DRV_LOG(ctx, "gain = %d temp_se = 0x%x  \n", gain, temp_se);

	/* check boundary of gain */
	gain = max(gain,
		ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[0].min);
	gain = min(gain,
		ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[0].max);
	/* mapping of gain to register value */
	if (ctx->s_ctx.g_gain2reg != NULL)
		rg_gain = ctx->s_ctx.g_gain2reg(gain);
	else
		rg_gain = gain2reg(gain);
	/* restore gain */
	memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
	ctx->ana_gain[0] = gain;
	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	if(ctx->current_scenario_id != 8 && ctx->current_scenario_id != 9 ){
		if(gain >= 4 * 1024){
			set_i2c_buffer(ctx, 0x3506,(temp_se | 0x02));
		}else{
			set_i2c_buffer(ctx, 0x3506,(temp_se & (~0x02)));
		}
	}
	/* write gain */
	set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_ana_gain[0].addr[0],
		(rg_gain >> 8) & 0xFF);
	set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_ana_gain[0].addr[1],
		rg_gain & 0xFF);
	DRV_LOG(ctx, "gain[0x%x]\n", rg_gain);
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 0);
	commit_i2c_buffer(ctx);
	/* group hold end */
	return 0;
}

static int duchampov50ewide_set_hdr_tri_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	int i = 0;
	int exp_cnt = 2;
	u32 values[3] = {0};
	u64 *gains = (u64 *)para;

	if (gains != NULL) {
		for (i = 0; i < 3; i++)
			values[i] = (u32) *(gains + i);
	}
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF) {
		set_multi_gain_in_lut(ctx, values, exp_cnt);
		return 0;
	}
	duchampov50ewide_set_multi_gain(ctx,values, exp_cnt);
	return 0;
}

static void duchampov50ewide_set_multi_gain(struct subdrv_ctx *ctx, u32 *gains, u16 exp_cnt)
{
	int i = 0;
	u32 rg_gains[3] = {0};
	u8 has_gains[3] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	u16 temp_se = subdrv_i2c_rd_u8(ctx, 0x3506);
	DRV_LOG(ctx, "temp_se = 0x%x\n", temp_se);

	if (exp_cnt > ARRAY_SIZE(ctx->ana_gain)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->ana_gain));
		exp_cnt = ARRAY_SIZE(ctx->ana_gain);
	}
	for (i = 0; i < exp_cnt; i++) {
		/* check boundary of gain */
		gains[i] = max(gains[i],
			ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].min);
		gains[i] = min(gains[i],
			ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].max);

		DRV_LOGE(ctx, "mode[%d].multi_exposure_ana_gain_range[%d], max: 0x%x, min:0x%x \n",
						ctx->current_scenario_id,i,ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].max,
						ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].min);
		/* mapping of gain to register value */
		if (ctx->s_ctx.g_gain2reg != NULL)
			gains[i] = ctx->s_ctx.g_gain2reg(gains[i]);
		else
			gains[i] = gain2reg(gains[i]);
	}
	/* restore gain */
	memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
	for (i = 0; i < exp_cnt; i++)
		ctx->ana_gain[i] = gains[i];
	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	/* write gain */
	memset(has_gains, 1, sizeof(has_gains));
	switch (exp_cnt) {
	case 2:
		rg_gains[0] = gains[0];
		has_gains[1] = 0;
		rg_gains[2] = gains[1];
		break;
	case 3:
		rg_gains[0] = gains[0];
		rg_gains[1] = gains[1];
		rg_gains[2] = gains[2];
		break;
	default:
		has_gains[0] = 0;
		has_gains[1] = 0;
		has_gains[2] = 0;
		break;
	}
	//set_i2c_buffer(ctx, 0x5006, 0x02);
	for (i = 0; i < 3; i++) {
		if (has_gains[i]) {
			set_i2c_buffer(ctx,ctx->s_ctx.reg_addr_ana_gain[i].addr[0],(rg_gains[i] >> 8) & 0xFF);
			set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_ana_gain[i].addr[1],rg_gains[i] & 0xFF);
		}
	}
	set_i2c_buffer(ctx, 0x501a, ((rg_gains[0] << 2) >> 8)& 0xFF);
	set_i2c_buffer(ctx, 0x501b, (rg_gains[0] << 2) & 0xFF);
	set_i2c_buffer(ctx, 0x501d, ((rg_gains[2] << 2) >> 8)& 0xFF);
	set_i2c_buffer(ctx, 0x501e, (rg_gains[2] << 2) & 0xFF);

	DRV_LOG(ctx, "reg[lg/mg/sg]: 0x%x 0x%x 0x%x\n", rg_gains[0], rg_gains[1], rg_gains[2]);
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 0);
	commit_i2c_buffer(ctx);
	/* group hold end */
}


static int duchampov50ewide_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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

	DRV_LOGE(ctx, "seamless 1\n");
	i2c_table_write(ctx, SCG_DCG_seamless_switch_step1_duchampov50ewide,
		ARRAY_SIZE(SCG_DCG_seamless_switch_step1_duchampov50ewide));
	DRV_LOGE(ctx, "seamless 2 mode = %d \n",scenario_id);
	i2c_table_write(ctx,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);
	subdrv_i2c_wr_u8(ctx, 0x3046, 0x01);
	if (ae_ctrl) {
		switch (ctx->s_ctx.mode[scenario_id].hdr_mode) {
		case HDR_RAW_STAGGER:
			set_multi_shutter_frame_length(ctx, (u64 *)&ae_ctrl->exposure, exp_cnt, 0);
			duchampov50ewide_set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		case HDR_RAW_DCG_COMPOSE:
			set_i2c_buffer(ctx, 0x5006, 0x02);
			duchampov50ewide_set_shutter_frame_length(ctx, (u8 *)&ae_ctrl->exposure, len);
			duchampov50ewide_set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		default:
			duchampov50ewide_set_shutter_frame_length(ctx, (u8 *)&ae_ctrl->exposure.le_exposure,len);
			duchampov50ewide_set_gain(ctx, (u8 *)&ae_ctrl->gain.le_gain,len);
			break;
		}
	}
	DRV_LOGE(ctx, "seamless 3\n");
	i2c_table_write(ctx, SCG_DCG_seamless_switch_step2_duchampov50ewide,
		ARRAY_SIZE(SCG_DCG_seamless_switch_step2_duchampov50ewide));
	DRV_LOGE(ctx, "seamless 4\n");
	
	/*if (src_hdr == HDR_RAW_STAGGER) {
		i2c_table_write(ctx, addr_data_pair_seamless_switch_step3_duchampov50ewide_shdr2linear,
			ARRAY_SIZE(addr_data_pair_seamless_switch_step3_duchampov50ewide_shdr2linear));
	} else {
		i2c_table_write(ctx, addr_data_pair_seamless_switch_step3_duchampov50ewide_normal,
			ARRAY_SIZE(addr_data_pair_seamless_switch_step3_duchampov50ewide_normal));
	}*/

	ctx->is_seamless = FALSE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->fast_mode_on = TRUE;
	DRV_LOG(ctx, "X: set seamless switch done\n");
	return ERROR_NONE;
}

static int duchampov50ewide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	switch (mode) {
	case 5:
		//subdrv_i2c_wr_u8(ctx, 0x50c1, 0x01);
		//subdrv_i2c_wr_u8(ctx, 0x50c2, 0x01);
		break;
	default:
		break;
	}

	if (mode != ctx->test_pattern)
		switch (ctx->test_pattern) {
		case 5:
			//subdrv_i2c_wr_u8(ctx, 0x50c1, 0x01);
			//subdrv_i2c_wr_u8(ctx, 0x50c2, 0x01);
			break;
		default:
			break;
		}

	ctx->test_pattern = mode;
	return ERROR_NONE;
}

static u16 duchampov50ewide_dgain2reg(struct subdrv_ctx *ctx, u32 dgain)
{
	return dgain; // digitalRealGain * 1024
}

static int duchampov50ewide_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
		gains[i] = duchampov50ewide_dgain2reg(ctx, gains[i]);
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
	DRV_LOG_MUST(ctx, "test sensormode(%d) sof_cnt(%d) sensor_output_cnt(%d)\n",
		ctx->current_scenario_id, sof_cnt, sensor_output_cnt);

	if (ctx->fast_mode_on && (sof_cnt > ctx->ref_sof_cnt)) {
		ctx->fast_mode_on = FALSE;
		ctx->ref_sof_cnt = 0;
		DRV_LOG(ctx, "seamless_switch finish.");
	}

	return 0;
};

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
			csi_param->dphy_data_settle = 0x29;
			csi_param->dphy_clk_settle = 0x29;
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

static int duchampov50ewide_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx, ctx->frame_length);
	/* write shutter */
	set_long_exposure(ctx);
	if (ctx->s_ctx.reg_addr_exposure[0].addr[2]) {
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[0],
			(ctx->exposure[0] >> 16) & 0xFF);
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[1],
			(ctx->exposure[0] >> 8) & 0xFF);
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[2],
			ctx->exposure[0] & 0xFF);
		//DXG
		if(ctx->current_scenario_id == 16 || ctx->current_scenario_id == 17 ){
			set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[2].addr[0],
				(ctx->exposure[0] >> 16) & 0xFF);
			set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[2].addr[1],
				(ctx->exposure[0] >> 8) & 0xFF);
			set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[2].addr[2],
				ctx->exposure[0] & 0xFF);
		}
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
	}/* group hold end */
	return 0;
}


static int duchampov50ewide_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	duchampov50ewide_set_shutter_frame_length(ctx, para, len);
	return 0;
}

