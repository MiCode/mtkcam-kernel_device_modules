// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 rothkoimx906widemipiraw_Sensor.c
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
#include "rothkoimx906widemipiraw_Sensor.h"

static u32 evaluate_frame_rate_by_scenario(void *arg, enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 framerate);
static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static int rothkoimx906wide_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int  rothkoimx906wide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkoimx906wide_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void rothkoimx906wide_get_dispatch_gain(struct subdrv_ctx *ctx, u32 tgain, u32 *again, u32 *dgain);
static int rothkoimx906wide_get_readout_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt);

#define SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE                 1
#define SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC 2
#define SEAMLESS_SWITCH_GROUP_VIDEO_MODE                      3
#define SEAMLESS_SWITCH_GROUP_HD_CAP_MODE                     4
#define SEAMLESS_SWITCH_GROUP_VIDEO_MI_CINELOOK_MODE          5
#define SEAMLESS_SWITCH_GROUP_NORMAL_HDR_VIDEO_MODE           6
#define SEAMLESS_SWITCH_GROUP_SUPER_NIGHT_VIDEO               7
#define SEAMLESS_SWITCH_GROUP_NORMAL_60FPS_HDR_VIDEO_MODE     8

/* STRUCT */

static u32 rothkoimx906_dcg_ratio_table_12bit[] = {4000};
static u32 rothkoimx906_dcg_ratio_table_14bit[] = {16000};

static struct mtk_sensor_saturation_info rothkoimx906_saturation_info_10bit = {
	.gain_ratio = 1000,
	.OB_pedestal = 64,
	.saturation_level = 1023,
};

static struct mtk_sensor_saturation_info rothkoimx906_saturation_info_12bit = {
	.gain_ratio = 4000,
	.OB_pedestal = 64, // Figure 8-77 Saturation level for DAG-HDR
	.saturation_level = 3900, // Figure 8-77 Saturation level for DAG-HDR
	.adc_bit = 10,
	.ob_bm = 64,
};

static struct mtk_sensor_saturation_info rothkoimx906_saturation_info_14bit = {
	.gain_ratio = 16000,
	.OB_pedestal = 64, // Figure 8-77 Saturation level for DAG-HDR
	.saturation_level = 15408, // Figure 8-77 Saturation level for DAG-HDR
	.adc_bit = 10,
	.ob_bm = 64,
};

static struct mtk_sensor_saturation_info rothkoimx906_saturation_info_fake12bit = {
	.gain_ratio = 1000,
	.OB_pedestal = 256,
	.saturation_level = 4092,
	.adc_bit = 12,
};

static struct mtk_sensor_saturation_info rothkoimx906_saturation_info_fake14bit = {
	.gain_ratio = 1000,
	.OB_pedestal = 1024,
	.saturation_level = 16368,
	.adc_bit = 14,
};


static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, rothkoimx906wide_set_test_pattern},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, rothkoimx906wide_seamless_switch},
	{SENSOR_FEATURE_SET_MULTI_DIG_GAIN, rothkoimx906wide_set_multi_dig_gain},
	{SENSOR_FEATURE_GET_READOUT_BY_SCENARIO, rothkoimx906wide_get_readout_by_scenario},
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		//.header_id = 0x01, // vendor id : 0x01 == sunny
		//.addr_header_id = 0x01, // vendor id addr
		.i2c_write_id = 0xA2,

		// LRC/SPC part1
		.lrc_support = true,
		.lrc_size = 192,
		.addr_lrc = 0x0EEF,
		.sensor_reg_addr_lrc = 0x2B00,
		// LRC/SPC part2
		.pdc_support = true,
		.pdc_size = 192,
		.addr_pdc = 0x0FAF,
		.sensor_reg_addr_pdc = 0x2C00,

		// QSC Calibration
		.qsc_support = true,
		.qsc_size = 3072,
		.addr_qsc = 0x2958,
		.sensor_reg_addr_qsc = 0x1000,
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0}},
	.i4PosR = {{0, 0}},
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <<slim_video>>
		{0, 0}, {0, 0}, {0, 384}, {0,192}, {0, 0},
		// <<cust1>> <<cust2>> <<cust3>> <cust4> <cust5>
		{0, 0}, {0, 192}, {0, 384}, {0,0}, {2048,1536},
		// <cust6> <cust7> <cust8> cust9 cust10
		{0, 0}, {0, 384}, {640,480}, {0,384}, {0, 0},
		// cust11 cust12 cust13 <cust14> <cust15>
		{0, 192}, {0, 0}, {2048,1536}, {0,0}, {0, 0},
		// <cust16> <cust17> cust18 <cust19> cust20
		{0, 0}, {0, 384}, {0, 384}, {256, 912}, {0, 0},
		// <cust21> <cust22> cust23 <cust24> cust25
		{0, 0}, {0, 0}, {2048,1536}, {2048,1536}, {0, 0},
		// <cust26> <cust27> cust28 <cust29> cust30
		{0, 0}, {640, 480}, {0,384}, {0,384}, {0, 384},
		// <cust31> <cust32> <cust33> <cust34> <cust35>
		{0, 384},{0, 384}, {0, 384}, {0, 384}, {0, 384}
	},
	.iMirrorFlip = 0,
	.i4FullRawW = 4096,
	.i4FullRawH = 3072,
	.i4ModeIndex = 2,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,
		.i4BinFacX = 2,
		.i4BinFacY = 4,
		.i4PDOrder = {1},
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_partial = {
	.i4OffsetX = 16,
	.i4OffsetY = 32,
	.i4PitchX = 16,
	.i4PitchY = 32,
	.i4PairNum = 8,
	.i4SubBlkW = 8,
	.i4SubBlkH = 8,
	.i4PosL = {{20, 35},{20, 43},{28, 35},{28, 43},{19,48},{19,56},{27,48},{27,56}},
	.i4PosR = {{16, 33},{16, 41},{24, 33},{24, 41},{23,50},{23,58},{31,50},{31,58}},
	.i4BlockNumX = 256,
	.i4BlockNumY = 94,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV,
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <<slim_video>>
		{0, 0}, {0, 0}, {0, 384}, {0,192}, {0, 0},
		// <<cust1>> <<cust2>> <<cust3>> <cust4> <cust5>
		{0, 0}, {0, 192}, {0, 384}, {0,0}, {2048,1536},
		// <cust6> <cust7> <cust8> cust9 cust10
		{0, 0}, {0, 384}, {640,480}, {0,384}, {0, 0},
		// cust11 cust12 cust13 <cust14> <cust15>
		{0, 192}, {0, 0}, {2048,1536}, {0,0}, {0, 0},
		// <cust16> <cust17> cust18 <cust19> cust20
		{0, 0}, {0, 384}, {0, 384}, {256, 912}, {0, 0},
		// <cust21> <cust22> cust23 <cust24> cust25
		{0, 0}, {0, 0}, {2048,1536}, {2048,1536}, {0, 0},
		// <cust26> <cust27> cust28 <cust29> cust30
		{0, 0}, {640, 480}, {0,384}, {0,384}, {0, 384},
		// <cust31> <cust32> <cust33> <cust34> <cust35>
		{0, 384},{0, 384}, {0, 384}, {0, 384}, {0, 384}
	},
	.iMirrorFlip = 0,
	.i4FullRawW = 4096,
	.i4FullRawH = 3072,
	.i4ModeIndex = 0,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		.i4PDPattern = 3,
		.i4BinFacX = 0,
		.i4BinFacY = 0,
		.i4PDOrder = {1,1,0,0,0,0,1,1},
		.i4PDRepetition = 8,
	}
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_video_partial = {
	.i4OffsetX = 16,
	.i4OffsetY = 32,
	.i4PitchX = 16,
	.i4PitchY = 32,
	.i4PairNum = 8,
	.i4SubBlkW = 8,
	.i4SubBlkH = 8,
	.i4PosL = {{20, 35},{20, 43},{28, 35},{28, 43},{19,48},{19,56},{27,48},{27,56}},
	.i4PosR = {{16, 33},{16, 41},{24, 33},{24, 41},{23,50},{23,58},{31,50},{31,58}},
	.i4BlockNumX = 256,
	.i4BlockNumY = 72,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV,
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <<slim_video>>
		{0, 0}, {0, 0}, {0, 384}, {0,192}, {0, 0},
		// <<cust1>> <<cust2>> <<cust3>> <cust4> <cust5>
		{0, 0}, {0, 192}, {0, 384}, {0,0}, {2048,1536},
		// <cust6> <cust7> <cust8> cust9 cust10
		{0, 0}, {0, 384}, {640,480}, {0,384}, {0, 0},
		// cust11 cust12 cust13 <cust14> <cust15>
		{0, 192}, {0, 0}, {2048,1536}, {0,0}, {0, 0},
		// <cust16> <cust17> cust18 <cust19> cust20
		{0, 0}, {0, 384}, {0, 384}, {256, 912}, {0, 0},
		// <cust21> <cust22> cust23 <cust24> cust25
		{0, 0}, {0, 0}, {2048,1536}, {2048,1536}, {0, 0},
		// <cust26> <cust27> cust28 <cust29> cust30
		{0, 0}, {640, 480}, {0,384}, {0,384}, {0, 384},
		// <cust31> <cust32> <cust33> <cust34> <cust35>
		{0, 384},{0, 384}, {0, 384}, {0, 384}, {0, 384}
	},
	.iMirrorFlip = 0,
	.i4FullRawW = 4096,
	.i4FullRawH = 3072,
	.i4ModeIndex = 0,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		.i4PDPattern = 3,
		.i4BinFacX = 0,
		.i4BinFacY = 0,
		.i4PDOrder = {1,1,0,0,0,0,1,1},
		.i4PDRepetition = 8,
	}
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_smvr = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0}},
	.i4PosR = {{0, 0}},
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <<slim_video>>
		{0, 0}, {0, 0}, {0, 384}, {0,192}, {0, 0},
		// <<cust1>> <<cust2>> <<cust3>> <cust4> <cust5>
		{0, 0}, {0, 192}, {0, 384}, {0,0}, {2048,1536},
		// <cust6> <cust7> <cust8> cust9 cust10
		{0, 0}, {0, 384}, {640,480}, {0,384}, {0, 0},
		// cust11 cust12 cust13 <cust14> <cust15>
		{0, 192}, {0, 0}, {2048,1536}, {0,0}, {0, 0},
		// <cust16> <cust17> cust18 <cust19> cust20
		{0, 0}, {0, 384}, {0, 384}, {256, 912}, {0, 0},
		// <cust21> <cust22> cust23 <cust24> cust25
		{0, 0}, {0, 0}, {2048,1536}, {2048,1536}, {0, 0},
		// <cust26> <cust27> cust28 <cust29> cust30
		{0, 0}, {640, 480}, {0,384}, {0,384}, {0, 384},
		// <cust31> <cust32> <cust33> <cust34> <cust35>
		{0, 384},{0, 384}, {0, 384}, {0, 384}, {0, 384}
	},
	.iMirrorFlip = 0,
	.i4FullRawW = 2048,
	.i4FullRawH = 1536,
	.i4ModeIndex = 2,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,
		.i4BinFacX = 2,
		.i4BinFacY = 4,
		.i4PDOrder = {1},
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_smvr_partial = {
	.i4OffsetX = 8,
	.i4OffsetY = 16,
	.i4PitchX = 8,
	.i4PitchY = 16,
	.i4PairNum = 4,
	.i4SubBlkW = 4,
	.i4SubBlkH = 8,
	.i4PosL = {{10, 21},{14, 21},{9, 24},{13, 24}},
	.i4PosR = {{8, 17},{12, 17},{11, 28},{15, 28}},
	.i4BlockNumX = 256,
	.i4BlockNumY = 72,
	.i4VCPackNum = 1,
	.PDAF_Support = PDAF_SUPPORT_CAMSV,
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <<slim_video>>
		{0, 0}, {0, 0}, {0, 384}, {0,192}, {0, 0},
		// <<cust1>> <<cust2>> <<cust3>> <cust4> <cust5>
		{0, 0}, {0, 192}, {0, 384}, {0,0}, {2048,1536},
		// <cust6> <cust7> <cust8> cust9 cust10
		{0, 0}, {0, 384}, {640,480}, {0,384}, {0, 0},
		// cust11 cust12 cust13 <cust14> <cust15>
		{0, 192}, {0, 0}, {2048,1536}, {0,0}, {0, 0},
		// <cust16> <cust17> cust18 <cust19> cust20
		{0, 0}, {0, 384}, {0, 384}, {256, 912}, {0, 0},
		// <cust21> <cust22> cust23 <cust24> cust25
		{0, 0}, {0, 0}, {2048,1536}, {2048,1536}, {0, 0},
		// <cust26> <cust27> cust28 <cust29> cust30
		{0, 0}, {640, 480}, {0,384}, {0,384}, {0, 384},
		// <cust31> <cust32> <cust33> <cust34> <cust35>
		{0, 384},{0, 384}, {0, 384}, {0, 384}, {0, 384}
	},
	.iMirrorFlip = 0,
	.i4FullRawW = 2048,
	.i4FullRawH = 1536,
	.i4ModeIndex = 0,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		.i4PDPattern = 3,
		.i4BinFacX = 0,
		.i4BinFacY = 0,
		.i4PDOrder = {1,0,0,1},
		.i4PDRepetition = 4,
	},
};


static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_isz = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0}},
	.i4PosR = {{0, 0}},
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <<slim_video>>
		{0, 0}, {0, 0}, {0, 384}, {0,192}, {0, 0},
		// <<cust1>> <<cust2>> <<cust3>> <cust4> <cust5>
		{0, 0}, {0, 192}, {0, 384}, {0,0}, {2048,1536},
		// <cust6> <cust7> <cust8> cust9 cust10
		{0, 0}, {0, 384}, {640,480}, {0,384}, {0, 0},
		// cust11 cust12 cust13 <cust14> <cust15>
		{0, 192}, {0, 0}, {2048,1536}, {0,0}, {0, 0},
		// <cust16> <cust17> cust18 <cust19> cust20
		{0, 0}, {0, 384}, {0, 384}, {256, 912}, {0, 0},
		// <cust21> <cust22> cust23 <cust24> cust25
		{0, 0}, {0, 0}, {2048,1536}, {2048,1536}, {0, 0},
		// <cust26> <cust27> cust28 <cust29> cust30
		{0, 0}, {640, 480}, {0,384}, {0,384}, {0, 384},
		// <cust31> <cust32>
		{0, 384},{0, 384}
	},
	.iMirrorFlip = 0,
	.i4FullRawW = 8192,
	.i4FullRawH = 6144,
	.i4ModeIndex = 2,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,
		.i4BinFacX = 4,
		.i4BinFacY = 2,
		.i4PDOrder = {1},
	},
};

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
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 768,
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
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 576,
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
			.hsize = 2048,
			.vsize = 1152,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 288,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
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
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 1536,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 1152,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 288,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
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
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 576,
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
			.valid_bit = 10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 1536,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus8[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2816,
			.vsize = 2112,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2816,
			.vsize = 528,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus9[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 576,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus10[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_ME,
			.valid_bit = 10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_SE,
			.valid_bit = 10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus11[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 1152,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 512,
			.vsize = 288,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus12[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 8192,
			.vsize = 6144,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus13[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 1536,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus14[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 768,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus15[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 768,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus16[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 512,
			.vsize = 752,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus17[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 512,
			.vsize = 576,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus18[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 576,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus19[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 7680,
			.vsize = 4320,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 3840,
			.vsize = 2160,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus28[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 512,
			.vsize = 576,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus29[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 576,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus30[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 576,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus31[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 576,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus32[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 512,
			.vsize = 576,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus34[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 512,
			.vsize = 576,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus35[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 4096,
			.vsize = 2304,
			.user_data_desc = VC_STAGGER_NE,
			.valid_bit = 10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 576,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct subdrv_mode_struct mode_struct[] = {
	// preview mode 0
	// Mode_E_4096x3072 @30fps-RAW10-All PD
	// PD size = 4096x768
	// Tline = 8.81us
	// VB = 5.90ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = rothkoimx906wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_preview_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_HD_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 19360,
		.framelength = 3780,
		.max_framerate = 300,
		.mipi_pixel_rate = 2472690000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// capture mode 1: same as preview mode
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = rothkoimx906wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoimx906wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 19360,
		.framelength = 3780,
		.max_framerate = 300,
		.mipi_pixel_rate = 2472690000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// normal_video mode 2
	// Mode_G_4096x2304 @30fps-RAW10-All PD
	// PD size = 4096x576
	// Tline = 8.81us
	// VB = 12.67ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = rothkoimx906wide_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_normal_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoimx906wide_normal_video_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_normal_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 19360,
		.framelength = 3780,
		.max_framerate = 300,
		.mipi_pixel_rate = 2472690000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// hs_video mode 3 : smvr 240fps
	// Mode_Q_2048x1152 @240fps-RAW10-All PD
	// PD Size = 2048x288
	// Tline = 2.49us
	// VB = 1.24ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = rothkoimx906wide_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_hs_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoimx906wide_hs_video_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_hs_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 5472,
		.framelength = 1672,
		.max_framerate = 2400,
		.mipi_pixel_rate = 2472690000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 2048,
			.scale_h = 1152,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2048,
			.h1_size = 1152,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2048,
			.h2_tg_size = 1152,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_smvr,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// slim_video mode 4: same as preview mode
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = rothkoimx906wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoimx906wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 19360,
		.framelength = 3780,
		.max_framerate = 300,
		.mipi_pixel_rate = 2472690000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// custom1 mode 5 : 10bit rems on ISZ
	// Mode_C-RAW10_4096x3072 @30fps-RMSC_Crop-RAW10-All PD[10bit]
	// PD Size = 2048x1536
	// Tline = 5.20us
	// VB = 16.98ms
	// ver9.00-6.00_MP
	{
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = rothkoimx906wide_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom1_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_HD_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom1_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom1_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 11424,
		.framelength = 6407,
		.max_framerate = 300,
		.mipi_pixel_rate = 2472690000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 16,
		.ana_gain_min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1536,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_R,
		.saturation_info = &rothkoimx906_saturation_info_fake12bit,
	},
	// custom2 mode 6 : smvr 120fps
	// Mode_P_2048x1152 @120fps-RAW10-All PD
	// PD Size = 2048x288
	// Tline = 2.49us
	// VB = 5.40ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = rothkoimx906wide_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom2_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom2_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom2_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 5472,
		.framelength = 3344,
		.max_framerate = 1200,
		.mipi_pixel_rate = 2472690000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 2048,
			.scale_h = 1152,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2048,
			.h1_size = 1152,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2048,
			.h2_tg_size = 1152,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_smvr,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// custom3 mode 7 : 60fps video
	// Mode_H_4096x2304 @60fps-RAW10-All PD
	// PD Size = 4096x576
	// Tline = 4.40us
	// VB = 6.33ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = rothkoimx906wide_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom3_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom3_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom3_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 9680,
		.framelength = 3774,
		.max_framerate = 601,
		.mipi_pixel_rate = 2472690000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// custom4 mode 8 : fullsize
	// Mode_A_8192x6144 @max_fps-RAW10-noPD
	// Tline = 5.20us
	// VB = 0.24ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = rothkoimx906wide_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom4_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_HD_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom4_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom4_setting),
		.hdr_mode = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 11424,
		.framelength = 6406,
		.max_framerate = 300,
		.mipi_pixel_rate = 2472690000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 16,
		.ana_gain_min = BASEGAIN,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// custom5 mode 9 : ISZ
	// Mode_C_4096x3072 @30fps-RMSC_Crop-RAW14-All PD
	// PD Size = 2048x1536
	// Tline = 5.48us
	// VB = 16.08ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = rothkoimx906wide_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom5_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom5_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom5_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2082000000,
		.linelength = 11424,
		.framelength = 6074,
		.max_framerate = 300,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 16,
		.ana_gain_min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1536,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &rothkoimx906_saturation_info_fake14bit,
	},
	// custom6 mode 10: same as preview mode bokeh
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = rothkoimx906wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoimx906wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 19360,
		.framelength = 4724,
		.max_framerate = 240,
		.mipi_pixel_rate = 2472690000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// custom7 mode 11: super night video
	// Mode_N_4096x2304 @30fps-RAW14-All PD
	// PD Size = 4096x576
	// Tline = 4.40us
	// VB = 23.00ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus18,
		.num_entries = ARRAY_SIZE(frame_desc_cus18),
		.mode_setting_table = rothkoimx906wide_custom18_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom18_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_SUPER_NIGHT_VIDEO,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom18_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom18_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 9680,
		.framelength = 9450,
		.max_framerate = 240,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &rothkoimx906_saturation_info_fake14bit,
	},
	// custom8 mode 12: 2x bokeh
	// Mode_T_2816x2112 @30fps-RAW10-All PD[10bit]
	// PD Size = 2816x528
	// Tline = 4.40us
	// VB = 23.84ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus8,
		.num_entries = ARRAY_SIZE(frame_desc_cus8),
		.mode_setting_table = rothkoimx906wide_custom8_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom8_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom8_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom8_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 9680,
		.framelength = 9450,
		.max_framerate = 240,
		.mipi_pixel_rate = 2472690000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 640,
			.y1_offset = 480,
			.w1_size = 2816,
			.h1_size = 2112,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2816,
			.h2_tg_size = 2112,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// custom9 mode 13: hdr video
	// Mode_K_4096x2304 @30fps-DAG[1:16]-RAW14-All PD HSG
	// PD Size = 4096x576
	// Tline = 4.61us
	// VB = 11.52ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus9,
		.num_entries = ARRAY_SIZE(frame_desc_cus9),
		.mode_setting_table = rothkoimx906wide_custom9_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom9_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom9_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom9_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2196000000,
		.linelength = 10128,
		.framelength = 7226,
		.max_framerate = 300,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 22.859376f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1.428711f,
		.saturation_info = &rothkoimx906_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = rothkoimx906_dcg_ratio_table_14bit,
			.dcg_gain_table_size = sizeof(rothkoimx906_dcg_ratio_table_14bit),
			.dcg_ratio_group = {1024, 1024},
		},
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
	},
	// custom10 mode 14: 3exp stagger hdr capture
	// Mode_I_4096x3072 @30fps-DOL3-RAW14-no PD
	// Tline = 2.74us
	// VB = 7.72ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus10,
		.num_entries = ARRAY_SIZE(frame_desc_cus10),
		.mode_setting_table = rothkoimx906wide_custom10_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom10_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom10_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom10_setting),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 3,
		.exp_cnt = 3,
		.pclk = 2082000000,
		.linelength = 5712,
		.framelength = 4048 * 3,
		.max_framerate = 300,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 3112 * 3, // Table 11-2 Readout length of each DOL mode: (80d + (Y_ADD_END-Y_ADD_STA+1)) / 2
		.read_margin = 24 * 3, // READ_MARGIN: This value is fixed as 24d.
		.framelength_step = 4 * 3,
		.coarse_integ_step = 4 * 3,
		.min_exposure_line = 8 * 3,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8 * 3,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 8 * 3,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_SE].min = 8 * 3,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_SE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1.428711f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_SE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &rothkoimx906_saturation_info_fake14bit,
	},
	// custom11 mode 15: smvr 480fps
	// Mode_R_2048x1152 @480fps-RAW10-Partial PD
	// PD Size = 508x288
	// Tline = 1.60us
	// VB = 0.19ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus11,
		.num_entries = ARRAY_SIZE(frame_desc_cus11),
		.mode_setting_table = rothkoimx906wide_custom11_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom11_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom11_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom11_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 3520,
		.framelength = 1296,
		.max_framerate = 4800,
		.mipi_pixel_rate = 2472690000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 768,
			.w0_size = 8192,
			.h0_size = 4608,
			.scale_w = 2048,
			.scale_h = 1152,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2048,
			.h1_size = 1152,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2048,
			.h2_tg_size = 1152,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_smvr_partial,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// custom12 mode 16: fullsize 50M quad bayer RawSR
	// Mode_B_8192x6144 @max_fps-noRMSC-RAW14-noPD
	// Tline = 5.48us
	// VB = 0.24ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus12,
		.num_entries = ARRAY_SIZE(frame_desc_cus12),
		.mode_setting_table = rothkoimx906wide_custom12_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom12_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom12_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom12_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2082000000,
		.linelength = 11424,
		.framelength = 6258,
		.max_framerate = 291,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 16,
		.ana_gain_min = BASEGAIN,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_4CELL_R,
		.saturation_info = &rothkoimx906_saturation_info_fake14bit,
	},
	// custom13 mode 17: fullsize crop 12.5M quad bayer RawSR
	// Mode_D_4096x3072 @30fps-noRMSC-RAW14-PD
	// Tline = 5.48us
	// VB = 16.08ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus13,
		.num_entries = ARRAY_SIZE(frame_desc_cus13),
		.mode_setting_table = rothkoimx906wide_custom13_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom13_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom13_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom13_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2082000000,
		.linelength = 11424,
		.framelength = 6074,
		.max_framerate = 300,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 16,
		.ana_gain_min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1536,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_4CELL_R,
		.saturation_info = &rothkoimx906_saturation_info_fake14bit,
	},
	// custom14 mode 18: fake 14bit binning preview
	// Mode_F-60fps_4096x3072 @30fps-RAW14-All PD
	// PD Size = 4096x768
	// Tline = 4.64us
	// VB = 2.20ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus14,
		.num_entries = ARRAY_SIZE(frame_desc_cus14),
		.mode_setting_table = rothkoimx906wide_custom14_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom14_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom14_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom14_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2082000000,
		.linelength = 9680,
		.framelength = 3584,
		.max_framerate = 600,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &rothkoimx906_saturation_info_fake14bit,
	},
	// custom15 mode 19: DAG HDR preview
	// Mode_J-Tuned_4096x3072 @30fps-DAG[1:16]-RAW14-All PD HSG
	// PD Size = 4096x768
	// Tline = 4.86us
	// VB = 2.86ms
	// ver9.00-6.00_MP
	{
		.frame_desc = frame_desc_cus15,
		.num_entries = ARRAY_SIZE(frame_desc_cus15),
		.mode_setting_table = rothkoimx906wide_custom15_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom15_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom15_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom15_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2082000000,
		.linelength = 10128,
		.framelength = 6852,
		.max_framerate = 300,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 22.859376f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1.428711f,
		.saturation_info = &rothkoimx906_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = rothkoimx906_dcg_ratio_table_14bit,
			.dcg_gain_table_size = sizeof(rothkoimx906_dcg_ratio_table_14bit),
			.dcg_ratio_group = {1024, 1024},
		},
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
	},
	// custom16 mode 20: LN2 fake 14bit preview dark
	// Mode_L_4096x3072 @30fps-LN2-RAW14-Partial PD
	// PD Size = 508x752
	// Tline = 4.75us
	// VB = 18.53ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus16,
		.num_entries = ARRAY_SIZE(frame_desc_cus16),
		.mode_setting_table = rothkoimx906wide_custom16_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom16_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom16_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom16_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2082000000,
		.linelength = 9904,
		.framelength = 7006,
		.max_framerate = 300,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.cms_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_partial,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.ae_binning_ratio = 1000,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &rothkoimx906_saturation_info_fake14bit,
	},
	// custom17 mode 21: LN2 fake 14bit video dark
	// Mode_M_4096x2304 @30fps-LN2-RAW14-Partial PD
	// PD Size = 508x576
	// Tline = 4.51us
	// VB = 22.76ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus17,
		.num_entries = ARRAY_SIZE(frame_desc_cus17),
		.mode_setting_table = rothkoimx906wide_custom17_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom17_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom17_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom17_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 9904,
		.framelength = 7390,
		.max_framerate = 300,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.cms_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_video_partial,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &rothkoimx906_saturation_info_fake14bit,
	},
	// custom18 mode 22: fake 14bit normal video
	// Mode_N_4096x2304 @30fps-RAW14-All PD
	// PD Size = 4096x576
	// Tline = 4.40us
	// VB = 23.00ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus18,
		.num_entries = ARRAY_SIZE(frame_desc_cus18),
		.mode_setting_table = rothkoimx906wide_custom18_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom18_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom18_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom18_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 9680,
		.framelength = 7560,
		.max_framerate = 300,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &rothkoimx906_saturation_info_fake14bit,
	},
	// custom19 mode 23 : 8K video
	// Mode_O_7680x4320 @30fps-RAW10-All PD
	// PD Size = 3840x2160
	// Tline = 5.20us
	// VB = 10.49ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus19,
		.num_entries = ARRAY_SIZE(frame_desc_cus19),
		.mode_setting_table = rothkoimx906wide_custom19_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom19_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom19_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom19_setting),
		.hdr_mode = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 11424,
		.framelength = 6407,
		.max_framerate = 300,
		.mipi_pixel_rate = 2472690000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 1,
		.coarse_integ_step = 1,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 16,
		.ana_gain_min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 256,
			.y0_offset = 912,
			.w0_size = 7680,
			.h0_size = 4320,
			.scale_w = 7680,
			.scale_h = 4320,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 7680,
			.h1_size = 4320,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 7680,
			.h2_tg_size = 4320,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// custom20 mode 24: authentic of LEICA
	// same as custom14 fake 14bit binning preview
	{
		.frame_desc = frame_desc_cus14,
		.num_entries = ARRAY_SIZE(frame_desc_cus14),
		.mode_setting_table = rothkoimx906wide_custom14_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom14_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom14_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom14_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2082000000,
		.linelength = 9680,
		.framelength = 3584,
		.max_framerate = 600,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &rothkoimx906_saturation_info_fake14bit,
	},
	// custom21 mode 25: authentic of LEICA
	// same as custom16 LN2 fake 14bit preview dark
	{
		.frame_desc = frame_desc_cus16,
		.num_entries = ARRAY_SIZE(frame_desc_cus16),
		.mode_setting_table = rothkoimx906wide_custom16_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom16_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom16_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom16_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2082000000,
		.linelength = 9904,
		.framelength = 7006,
		.max_framerate = 300,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.cms_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_partial,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &rothkoimx906_saturation_info_fake14bit,
	},
	// custom22 mode 26: authentic of LEICA
	// same as custom15 DAG HDR preview
	{
		.frame_desc = frame_desc_cus15,
		.num_entries = ARRAY_SIZE(frame_desc_cus15),
		.mode_setting_table = rothkoimx906wide_custom15_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom15_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom15_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom15_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2082000000,
		.linelength = 10128,
		.framelength = 6852,
		.max_framerate = 300,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 22.859376f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1.428711f,
		.saturation_info = &rothkoimx906_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = rothkoimx906_dcg_ratio_table_14bit,
			.dcg_gain_table_size = sizeof(rothkoimx906_dcg_ratio_table_14bit),
			.dcg_ratio_group = {1024, 1024},
		},
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
	},
	// custom23 mode 27 : authentic of LEICA
	// same as custom5 ISZ
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = rothkoimx906wide_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom5_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom5_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom5_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2082000000,
		.linelength = 11424,
		.framelength = 6074,
		.max_framerate = 300,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 16,
		.ana_gain_min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1536,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &rothkoimx906_saturation_info_fake14bit,
	},
	// custom24 mode 28: authentic of LEICA
	// same as custom13 fullsize crop 12.5M quad bayer RawSR
	{
		.frame_desc = frame_desc_cus13,
		.num_entries = ARRAY_SIZE(frame_desc_cus13),
		.mode_setting_table = rothkoimx906wide_custom13_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom13_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom13_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom13_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2082000000,
		.linelength = 11424,
		.framelength = 6074,
		.max_framerate = 300,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 16,
		.ana_gain_min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1536,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_isz,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_4CELL_R,
		.saturation_info = &rothkoimx906_saturation_info_fake14bit,
	},
	// custom25 mode 29: authentic of LEICA
	// same as custom10 3exp stagger hdr capture
	{
		.frame_desc = frame_desc_cus10,
		.num_entries = ARRAY_SIZE(frame_desc_cus10),
		.mode_setting_table = rothkoimx906wide_custom10_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom10_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom10_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom10_setting),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 3,
		.exp_cnt = 3,
		.pclk = 2082000000,
		.linelength = 5712,
		.framelength = 4048 * 3,
		.max_framerate = 300,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 3112 * 3, // Table 11-2 Readout length of each DOL mode: (80d + (Y_ADD_END-Y_ADD_STA+1)) / 2
		.read_margin = 24 * 3, // READ_MARGIN: This value is fixed as 24d.
		.framelength_step = 4 * 3,
		.coarse_integ_step = 4 * 3,
		.min_exposure_line = 8 * 3,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8 * 3,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 8 * 3,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_SE].min = 8 * 3,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_SE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1.428711f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_SE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &rothkoimx906_saturation_info_fake14bit,
	},
	// custom26 mode 30: authentic of LEICA
	// same as preview mode bokeh
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = rothkoimx906wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoimx906wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 19360,
		.framelength = 4724,
		.max_framerate = 240,
		.mipi_pixel_rate = 2472690000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// custom27 mode 31: authentic of LEICA
	// same as custom8 2x bokeh
	{
		.frame_desc = frame_desc_cus8,
		.num_entries = ARRAY_SIZE(frame_desc_cus8),
		.mode_setting_table = rothkoimx906wide_custom8_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom8_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom8_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom8_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 9680,
		.framelength = 9450,
		.max_framerate = 240,
		.mipi_pixel_rate = 2472690000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 640,
			.y1_offset = 480,
			.w1_size = 2816,
			.h1_size = 2112,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2816,
			.h2_tg_size = 2112,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// custom28 mode 32: hdr video 60fps Mi-CineLook
	// Mode_S2_4096x2304 @60fps-DAG[1:16]-RAW14-Partial PD[10bit]
	// PD Size = 508x576
	// Tline = 2.89us
	// VB = 2.98ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus28,
		.num_entries = ARRAY_SIZE(frame_desc_cus28),
		.mode_setting_table = rothkoimx906wide_custom28_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom28_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MI_CINELOOK_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom28_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom28_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2196000000,
		.linelength = 6352,
		.framelength = 5760,
		.max_framerate = 600,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 22.859376f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1.428711f,
		.saturation_info = &rothkoimx906_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = rothkoimx906_dcg_ratio_table_14bit,
			.dcg_gain_table_size = sizeof(rothkoimx906_dcg_ratio_table_14bit),
		},
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_video_partial,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
	},
	// custom29 mode 33 : 60fps fake14bit normal video Mi-CineLook
	// Mode_H_RAW14_4096x2304 @60fps-RAW14-All PD[10bit]
	// PD Size = 4096x576
	// Tline = 4.40us
	// VB = 6.33ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus29,
		.num_entries = ARRAY_SIZE(frame_desc_cus29),
		.mode_setting_table = rothkoimx906wide_custom29_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom29_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MI_CINELOOK_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom29_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom29_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 9680,
		.framelength = 3780,
		.max_framerate = 600,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &rothkoimx906_saturation_info_fake14bit,
	},
	// custom30 mode 34: normal video 1:4 DAG hdr
	// Mode_K-ratio4-RAW12_4096x2304 @30fps-DAG[1:4]-RAW12-All PD[10bit]
	// PD Size = 4096x576
	// Tline = 4.61us
	// VB = 11.52ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus30,
		.num_entries = ARRAY_SIZE(frame_desc_cus30),
		.mode_setting_table = rothkoimx906wide_custom30_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom30_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_HDR_VIDEO_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom30_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom30_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2196000000,
		.linelength = 10128,
		.framelength = 7226,
		.max_framerate = 300,
		.mipi_pixel_rate = 2060570000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 5.714844f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1.428711f,
		.saturation_info = &rothkoimx906_saturation_info_12bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = rothkoimx906_dcg_ratio_table_12bit,
			.dcg_gain_table_size = sizeof(rothkoimx906_dcg_ratio_table_12bit),
		},
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_R,
	},
	// custom31 mode 35: fake 12bit normal video
	// Mode_N-RAW12_4096x2304 @30fps-RAW12-All PD[10bit]
	// PD Size = 4096x576
	// Tline = 4.40us
	// VB = 23.00ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus31,
		.num_entries = ARRAY_SIZE(frame_desc_cus31),
		.mode_setting_table = rothkoimx906wide_custom31_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom31_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_HDR_VIDEO_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom31_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom31_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 9680,
		.framelength = 7560,
		.max_framerate = 300,
		.mipi_pixel_rate = 2060570000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.saturation_info = &rothkoimx906_saturation_info_fake12bit,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_R,
	},
	// custom32 mode 36: LN2 fake 12bit video dark
	// Mode_M-RAW12_4096x2304 @30fps-LN2-RAW12-Partial PD[10bit]
	// PD Size = 508x576
	// Tline = 4.51us
	// VB = 22.76ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus32,
		.num_entries = ARRAY_SIZE(frame_desc_cus32),
		.mode_setting_table = rothkoimx906wide_custom32_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom32_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_HDR_VIDEO_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom32_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom32_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 9904,
		.framelength = 7390,
		.max_framerate = 300,
		.mipi_pixel_rate = 2060570000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.cms_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_video_partial,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.saturation_info = &rothkoimx906_saturation_info_fake12bit,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_R,
	},
	// custom33 mode 37 : super night video
	// Mode_K_4096x2304 @30fps-DAG[1:16]-RAW14-All PD HSG
	// PD Size = 4096x576
	// Tline = 4.61us
	// VB = 11.52ms
	// ver8.10-5.00_240312
	{
		.frame_desc = frame_desc_cus9,
		.num_entries = ARRAY_SIZE(frame_desc_cus9),
		.mode_setting_table = rothkoimx906wide_custom9_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom9_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_SUPER_NIGHT_VIDEO,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom9_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom9_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2196000000,
		.linelength = 10128,
		.framelength = 9032,
		.max_framerate = 240,
		.mipi_pixel_rate = 1766200000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 22.859376f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1.428711f,
		.saturation_info = &rothkoimx906_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = rothkoimx906_dcg_ratio_table_14bit,
			.dcg_gain_table_size = sizeof(rothkoimx906_dcg_ratio_table_14bit),
		},
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
	},
	// custom34 mode 38: 60fps video 1:4 DAG hdr
	// Mode_S-1-RAW12_4096x2304 @60fps-DAG[1:4]-RAW12-Partial PD[10bit]
	// PD Size = 512x576
	// Tline = 2.89us
	// VB = 2.98ms
	// ver9.00-6.00_MP
	{
		.frame_desc = frame_desc_cus34,
		.num_entries = ARRAY_SIZE(frame_desc_cus34),
		.mode_setting_table = rothkoimx906wide_custom34_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom34_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_60FPS_HDR_VIDEO_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom34_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom34_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2196000000,
		.linelength = 6352,
		.framelength = 5760,
		.max_framerate = 600,
		.mipi_pixel_rate = 2060570000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 5.714844f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1.428711f,
		.saturation_info = &rothkoimx906_saturation_info_12bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = rothkoimx906_dcg_ratio_table_12bit,
			.dcg_gain_table_size = sizeof(rothkoimx906_dcg_ratio_table_12bit),
		},
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_video_partial,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_R,
	},
	// custom35 mode 39: fake 12bit 60fps video
	// Mode_H-RAW12_4096x2304 @60fps-RAW12-All PD[10bit]
	// PD Size = 4096x576
	// Tline = 4.40us
	// VB = 6.33ms
	// ver9.00-6.00_MP
	{
		.frame_desc = frame_desc_cus35,
		.num_entries = ARRAY_SIZE(frame_desc_cus35),
		.mode_setting_table = rothkoimx906wide_custom35_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom35_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_60FPS_HDR_VIDEO_MODE,
		.seamless_switch_mode_setting_table = rothkoimx906wide_custom35_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoimx906wide_custom35_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2196000000,
		.linelength = 9680,
		.framelength = 3780,
		.max_framerate = 600,
		.mipi_pixel_rate = 2060570000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1.428711f,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.saturation_info = &rothkoimx906_saturation_info_fake12bit,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_R,
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = ROTHKOIMX906WIDE_SENSOR_ID,
	.reg_addr_sensor_id = {0x0016, 0x0017},
	.i2c_addr_table = {0x20, 0xFF}, // TBD
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {8192, 6144},
	.mirror = IMAGE_NORMAL,

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
	.dig_gain_min = BASEGAIN * 1,
	.dig_gain_max = BASEGAIN * 15,
	.dig_gain_step = 4,  //If the value is 0, SENSOR_FEATURE_SET_MULTI_DIG_GAIN is disabled
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1.428711f,
	.ana_gain_max = BASEGAIN * 64,
	.ana_gain_type = 0,
	.ana_gain_step = 1,
	.ana_gain_table = rothkoimx906wide_ana_gain_table,
	.ana_gain_table_size = sizeof(rothkoimx906wide_ana_gain_table),
	.min_gain_iso = 50,
	.exposure_def = 0x3D0,
	.exposure_min = 6,
	.exposure_max = 0xFFFFFF - 48,
	.exposure_step = 2,
	.exposure_margin = 48,
	.saturation_info = &rothkoimx906_saturation_info_10bit,

	.frame_length_max = 0xFFFF,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 3,
	.start_exposure_offset = 115500,

	.pdaf_type = PDAF_SUPPORT_CAMSV_QPD,
	.hdr_type = HDR_SUPPORT_STAGGER_FDOL|HDR_SUPPORT_DCG,
	.seamless_switch_support = TRUE,
	.temperature_support = TRUE,

	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.s_cali = set_sensor_cali,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {
		{0x0202, 0x0203},
		{0x3172, 0x3173},
		{0x0224, 0x0225},
	},
	.long_exposure_support = TRUE,
	.reg_addr_exposure_lshift = 0x3160,
	.reg_addr_ana_gain = {
		{0x0204, 0x0205},
		{0x3174, 0x3175},
		{0x0216, 0x0217},
	},
	.reg_addr_dig_gain = {
		{0x020E, 0x020F},
		{0x3176, 0x3177},
		{0x0218, 0x0219},
	},
	.reg_addr_dcg_ratio = PARAM_UNDEFINED, //TBD
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_temp_en = 0x0138,
	.reg_addr_temp_read = 0x013A,
	.reg_addr_auto_extend = 0x0350,
	.reg_addr_frame_count = 0x0005,
	.reg_addr_fast_mode = 0x3010,

	.mi_enable_async = 1, // enable async setting
	.mi_disable_set_dummy = 0, // disable set dummy
	.mi_evaluate_frame_rate_by_scenario = evaluate_frame_rate_by_scenario,
	.init_setting_table = rothkoimx906wide_init_setting,
	.init_setting_len = ARRAY_SIZE(rothkoimx906wide_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 1,
	.chk_s_off_end = 0,

	//TBD
	.checksum_value = 0xAF3E324E,
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
	{HW_ID_RST,   0,       0},
	{HW_ID_MCLK,  24,      0},
	{HW_ID_MCLK_DRIVING_CURRENT, 4, 1},
	{HW_ID_AFVDD, 3100000, 3},
	{HW_ID_DVDD1, 1800000, 1}, // vcam_ldo
	{HW_ID_DVDD,  1200000, 0},
	{HW_ID_AVDD1, 1800000, 0},
	{HW_ID_AVDD,  2800000, 0},
	{HW_ID_DOVDD, 1800000, 1},
	{HW_ID_RST,   1,       3},
};

const struct subdrv_entry rothkoimx906wide_mipi_raw_entry = {
	.name = SENSOR_DRVNAME_ROTHKOIMX906WIDE_MIPI_RAW,
	.id = ROTHKOIMX906WIDE_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

static u32 evaluate_frame_rate_by_scenario(void *arg, enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 framerate)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u32 framerateRet = framerate;

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_CUSTOM3:
		if (framerate == 600) {
			framerateRet = 601;
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

	/* QSC data */
	support = info[idx].qsc_support;
	pbuf = info[idx].preload_qsc_table;
	size = info[idx].qsc_size;
	addr = info[idx].sensor_reg_addr_qsc;
	if (support) {
		if (pbuf != NULL && addr > 0 && size > 0) {
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set QSC calibration data done.");
		}
	}

	// LRC/SPC part1
	support = info[idx].lrc_support;
	pbuf = info[idx].preload_lrc_table;
	size = info[idx].lrc_size;
	addr = info[idx].sensor_reg_addr_lrc;
	if (support) {
		if (pbuf != NULL && addr > 0 && size > 0) {
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set LRC/SPC part1 calibration data done.");
		}
	}

	// LRC/SPC part2
	support = info[idx].pdc_support;
	pbuf = info[idx].preload_pdc_table;
	size = info[idx].pdc_size;
	addr = info[idx].sensor_reg_addr_pdc;
	if (support) {
		if (pbuf != NULL && addr > 0 && size > 0) {
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set LRC/SPC part2 calibration data done.");
		}
	}
}

static int get_sensor_temperature(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u8 temperature = 0;
	int temperature_convert = 0;

	temperature = subdrv_i2c_rd_u8(ctx, ctx->s_ctx.reg_addr_temp_read);

	if (temperature < 0x55)
		temperature_convert = temperature;
	else if (temperature < 0x80)
		temperature_convert = 85;
	else if (temperature < 0xED)
		temperature_convert = -20;
	else
		temperature_convert = (char)temperature;

	DRV_LOG(ctx, "temperature: %d degrees\n", temperature_convert);
	return temperature_convert;
}

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
	return (16384 - (16384 * BASEGAIN) / gain);
}

static int rothkoimx906wide_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	struct mtk_hdr_ae *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;
	u32 frame_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 exp_cnt = 0;
	u32 tgain = 0;
	u32 again[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 dgain[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u64 feature_para[4] = {0};
	u32 retLen = 0;

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

	if (scenario_id == SENSOR_SCENARIO_ID_CUSTOM14 ||
		scenario_id == SENSOR_SCENARIO_ID_CUSTOM20) {
		set_max_framerate_by_scenario(ctx, scenario_id, 300);
	}

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
				== IMGSENSOR_DCG_DIRECT_MODE) {
				set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			} else {
				tgain = ae_ctrl->gain.le_gain;
				rothkoimx906wide_get_dispatch_gain(ctx, tgain, again, dgain);
				feature_para[0] = (u64)dgain;
				feature_para[1] = 1;
				rothkoimx906wide_set_multi_dig_gain(ctx, (u8 *)feature_para, &retLen);
				set_gain(ctx, again[0]);
			}
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

static int rothkoimx906wide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	if (mode){
		subdrv_i2c_wr_u8(ctx, 0x0104, 1);
		subdrv_i2c_wr_u8(ctx, 0x0202, 0); 
		subdrv_i2c_wr_u8(ctx, 0x0203, 0); 
		subdrv_i2c_wr_u8(ctx, 0x0204, 0); 
		subdrv_i2c_wr_u8(ctx, 0x0205, 0);
		subdrv_i2c_wr_u8(ctx, 0x020E, 0);
		subdrv_i2c_wr_u8(ctx, 0x020F, 0);
		subdrv_i2c_wr_u8(ctx, 0x0104, 0);
	}
	else if (ctx->test_pattern)
		subdrv_i2c_wr_u8(ctx, 0x0601, 0x00); /*No pattern*/

	ctx->test_pattern = mode;
	return ERROR_NONE;

}

static u16 dgain2reg(struct subdrv_ctx *ctx, u32 dgain)
{
	u32 step = max((ctx->s_ctx.dig_gain_step), (u32)1);
	u8 integ = (u8) (dgain / BASE_DGAIN); // integer parts
	u8 dec = (u8) ((dgain % BASE_DGAIN) / step); // decimal parts
	u16 ret = ((u16)integ << 8) | dec;

	DRV_LOG(ctx, "dgain reg = 0x%x\n", ret);

	return ret;
}

static int rothkoimx906wide_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	u32 *gains  = (u32 *)(*feature_data);
	u16 exp_cnt = (u16) (*(feature_data + 1));

	int i = 0;
	u16 rg_gains[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

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
		gains[i] = dgain2reg(ctx, gains[i]);
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

	//for IMX906 DAG
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE) {
		rg_gains[2] = rg_gains[0];
	}

	for (i = 0;
		 (i < ARRAY_SIZE(rg_gains)) && (i < ARRAY_SIZE(ctx->s_ctx.reg_addr_dig_gain));
		 i++) {
		if (!rg_gains[i])
			continue; // skip zero gain setting

		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[0]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[0],
				(rg_gains[i] >> 8) & 0x0F);
		}
		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[1]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[1],
				rg_gains[i] & 0xFF);
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

static void rothkoimx906wide_get_dispatch_gain(struct subdrv_ctx *ctx, u32 tgain, u32 *again, u32 *dgain)
{
	int i;
	u32 ag = tgain;
	u32 dg = 0;
	u32 dig_gain_step = ctx->s_ctx.dig_gain_step;
	u32 *ana_gain_table = ctx->s_ctx.ana_gain_table;
	u32 ana_gain_table_size = ctx->s_ctx.ana_gain_table_size;
	u32 ana_gain_table_cnt = 0;
	u32 scenario_id = ctx->current_scenario_id;
	u32 ana_gain_max = ctx->s_ctx.mode[scenario_id].ana_gain_max;

	if (dig_gain_step && ana_gain_table && (tgain > ana_gain_table[0])) {
		ana_gain_table_cnt = (ana_gain_table_size / sizeof(ana_gain_table[0]));
		for (i = 1; i < ana_gain_table_cnt; i++) {
			if (ana_gain_table[i] > tgain) {
				ag = ana_gain_table[i - 1];
				dg = (u32) ((u64)tgain * BASE_DGAIN / ag);
				break;
			}
			if (ana_gain_table[i] > ana_gain_max) {
				ag = ana_gain_table[i - 1];
				dg = (u32) ((u64)tgain * BASE_DGAIN / ag);
				break;
			}
		}
		if (i == ana_gain_table_cnt) {
			ag = ana_gain_table[i - 1];
			dg = (u32) ((u64)tgain * BASE_DGAIN / ag);
		}
	}

	if (again)
		*again = ag;
	if (dgain)
		*dgain = dg;


	DRV_LOG(ctx,
		"X! again tlb cnt = %u sz(%u), gain(t/a/d) = %u / %u / %u\n",
		ana_gain_table_cnt, ana_gain_table_size, tgain, ag, dg);

}

static int rothkoimx906wide_get_readout_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = *feature_data;
	u64 *readout_time = feature_data + 1;

	u32 ratio = 1;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return 0;
	}

	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_STAGGER)
		ratio = ctx->s_ctx.mode[scenario_id].exp_cnt;

	// for IMX906 DAG
	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE)
		ratio = ctx->s_ctx.mode[scenario_id].exp_cnt;

	*readout_time =
		(u64)ctx->s_ctx.mode[scenario_id].linelength
		* ctx->s_ctx.mode[scenario_id].imgsensor_winsize_info.h2_tg_size
		* 1000000000 / ctx->s_ctx.mode[scenario_id].pclk * ratio;

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
		DRV_LOG(ctx, "seamless_switch disabled.");
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x00);
		commit_i2c_buffer(ctx);
	}

	return 0;
};

