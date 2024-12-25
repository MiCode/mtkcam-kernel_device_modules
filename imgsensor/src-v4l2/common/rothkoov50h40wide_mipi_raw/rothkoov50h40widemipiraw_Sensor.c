// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 rothkoov50h40widemipiraw_Sensor.c
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
#include "rothkoov50h40widemipiraw_Sensor.h"
#define EEPROM_READY 1

static u32 evaluate_frame_rate_by_scenario(void *arg, enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 framerate);
static void mi_stream(void *arg,bool enable);
static void mi_read_CGRatio(void *arg);
#if EEPROM_READY
static void set_sensor_cali(void *arg);
#endif
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static int rothkoov50h40wide_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int  rothkoov50h40wide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkoov50h40wide_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt);
static int get_csi_param(struct subdrv_ctx *ctx,	enum SENSOR_SCENARIO_ID_ENUM scenario_id,struct mtk_csi_param *csi_param);
static int rothkoov50h40wide_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkoov50h40wide_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void rothkoov50h40wide_set_multi_gain(struct subdrv_ctx *ctx, u32 *gains, u16 exp_cnt);
static int rothkoov50h40wide_set_hdr_tri_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkoov50h40wide_set_multi_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkoov50h40wide_set_hdr_tri_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkoov50h40wide_get_period_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkoov50h40wide_get_cust_pixel_rate(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkoov50h40wide_get_readout_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkoov50h40wide_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int rothkoov50h40wide_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);

#define SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE 1
#define SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC 2
#define SEAMLESS_SWITCH_GROUP_VIDEO_MODE      3
#define SEAMLESS_SWITCH_GROUP_HD_CAP_MODE     4
#define SEAMLESS_SWITCH_GROUP_VIDEO_MI_CINELOOK_MODE          5
#define SEAMLESS_SWITCH_GROUP_SUPER_NIGHT_VIDEO               6
#define DEBUG_LOG_EN 0

/* STRUCT */
static struct mtk_sensor_saturation_info rothkoov50h40_saturation_info_14bit = {
	.gain_ratio = 5000,
	.OB_pedestal = 64,
	.saturation_level = 16383,
	.adc_bit = 10,
	.ob_bm = 64,
};

static struct mtk_sensor_saturation_info rothkoov50h40_saturation_info_fake14bit = {
	.gain_ratio = 1000,
	.OB_pedestal = 1024,
	.saturation_level = 16368,
	.adc_bit = 14,
};

//static u32 rothkoov50h40_dcg_ratio_table_14bit[] = {16000};

static struct mtk_sensor_saturation_info rothkoov50h40_saturation_info_10bit = {
	.gain_ratio = 1000,
	.OB_pedestal = 64,
	.saturation_level = 1023,
};

struct SET_SENSOR_AWB_GAIN g_last_awb_gain = {0, 0, 0, 0};

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, rothkoov50h40wide_set_test_pattern},
	{SENSOR_FEATURE_SET_TEST_PATTERN_DATA, rothkoov50h40wide_set_test_pattern_data},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, rothkoov50h40wide_seamless_switch},
	{SENSOR_FEATURE_SET_ESHUTTER, rothkoov50h40wide_set_shutter},
	{SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME, rothkoov50h40wide_set_shutter_frame_length},
	//{SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME, rothkoov50h40wide_set_shutter_frame_length},
	{SENSOR_FEATURE_SET_DUAL_GAIN,rothkoov50h40wide_set_hdr_tri_gain},
	{SENSOR_FEATURE_SET_HDR_SHUTTER,rothkoov50h40wide_set_hdr_tri_shutter},
	{SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME,rothkoov50h40wide_set_multi_shutter_frame_length},
	{SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO,rothkoov50h40wide_get_period_by_scenario},
	{SENSOR_FEATURE_GET_CUST_PIXEL_RATE,rothkoov50h40wide_get_cust_pixel_rate},
	{SENSOR_FEATURE_GET_READOUT_BY_SCENARIO,rothkoov50h40wide_get_readout_by_scenario},
	{SENSOR_FEATURE_SET_MULTI_DIG_GAIN, rothkoov50h40wide_set_multi_dig_gain},
	{SENSOR_FEATURE_SET_AWB_GAIN, rothkoov50h40wide_set_awb_gain},
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		//.header_id = 0x01, // vendor id : 0x01 == sunny
		//.addr_header_id = 0x01, // vendor id addr
		.i2c_write_id = 0xA2,

		// PDC Calibration 1th
		.pdc_support = TRUE,
		.pdc_size = 32,
		.addr_pdc = 0x2A03,
		.sensor_reg_addr_pdc = 0x5A20,

		// LRC Calibration 2th
		.lrc_support = TRUE,
		.lrc_size = 3536,
		.addr_lrc = 0x2A23,
		.sensor_reg_addr_lrc = 0x5AC0,

		// LRC Calibration 3th
		.xtalk_support = TRUE,
		.xtalk_size = 16,
		.addr_xtalk = 0x37F3,
		.sensor_reg_addr_xtalk = 0x68AE,
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX	= 0,
	.i4OffsetY	= 0,
	.i4PitchX	= 0,
	.i4PitchY	= 0,
	.i4PairNum	= 0,
	.i4SubBlkW	= 0,
	.i4SubBlkH	= 0,
	.iMirrorFlip = 0,
	.i4PosL = {{0, 0}},
	.i4PosR = {{0, 0}},
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4FullRawW = 4096,
	.i4FullRawH = 3072,
	.i4ModeIndex = 2, /*HVBin 2; VBin 3*/
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,//1: dense pd
		.i4BinFacX = 2,//
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <slim_video>
		{0, 0}, {0, 0}, {0, 384}, {64, 228}, {0, 0},
		// <cust1> <cust2> <cust3> <cust4> <cust5>
		{0, 0}, {0, 192}, {0, 384}, {0, 0}, {2048, 1536},
		// <cust6> <cust7> <cust8> <cust9>< cust10>
		{0, 0}, {0, 384}, {64, 48}, {0, 384}, {0, 0},
		// cust11 cust12 cust13 <cust14> <cust15>
		{480, 462}, {0, 0}, {2048, 1536}, {0, 0}, {0, 0},
		// <cust16> <cust17> cust18 <cust19> <cust20>
		{0, 0}, {0, 384}, {0, 384}, {256, 912},{0, 0},
		// <cust21> <cust22> cust23 <cust24> <cust25>
		{0, 0}, {0, 0}, {2048,1536},{2048,1536}, {0, 0},
		// <cust26> <cust27> cust28 <cust29> <cust30>
		{0, 0}, {64, 48}, {0, 384},{0, 384}, {2048,1536},
		// <cust31> <cust32>
		{896,672}, {896,672},
	},
};


static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_smvr = {
	.i4OffsetX	= 0,
	.i4OffsetY	= 0,
	.i4PitchX	= 0,
	.i4PitchY	= 0,
	.i4PairNum	= 0,
	.i4SubBlkW	= 0,
	.i4SubBlkH	= 0,
	.iMirrorFlip = 0,
	.i4PosL = {{0, 0}},
	.i4PosR = {{0, 0}},
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4FullRawW = 2048,
	.i4FullRawH = 1536,
	.i4ModeIndex = 2, /*HVBin 2; VBin 3*/
	.sPDMapInfo[0] = {
		.i4PDPattern = 1,//1: dense pd
		.i4BinFacX = 1,//?
		.i4BinFacY = 4,
		.i4PDRepetition = 2,
		.i4PDOrder = {1},
	},
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <slim_video>
		{0, 0}, {0, 0}, {0, 384}, {64, 228}, {0, 0},
		// <cust1> <cust2> <cust3> <cust4> <cust5>
		{0, 0}, {0, 192}, {0, 384}, {0, 0}, {2048, 1536},
		// <cust6> <cust7> <cust8> <cust9>< cust10>
		{0, 0}, {0, 384}, {64, 48}, {0, 384}, {0, 0},
		// cust11 cust12 cust13 <cust14> <cust15>
		{480, 462}, {0, 0}, {2048, 1536}, {0, 0}, {0, 0},
		// <cust16> <cust17> cust18 <cust19> <cust20>
		{0, 0}, {0, 384}, {0, 384}, {256, 912},{0, 0},
		// <cust21> <cust22> cust23 <cust24> <cust25>
		{0, 0}, {0, 0}, {2048,1536},{2048,1536}, {0, 0},
		// <cust26> <cust27> cust28 <cust29> <cust30>
		{0, 0}, {64, 48}, {0, 384},{0, 384}, {2048,1536},
		// <cust31> <cust32>
		{896,672}, {896,672},
	},
};


static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_isz = {
	.i4OffsetX	= 0,
	.i4OffsetY	= 0,
	.i4PitchX	= 0,
	.i4PitchY	= 0,
	.i4PairNum	= 0,
	.i4SubBlkW	= 0,
	.i4SubBlkH	= 0,
	.iMirrorFlip = 0,
	.i4PosL = {{0, 0}},
	.i4PosR = {{0, 0}},
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4FullRawW = 8192,
	.i4FullRawH = 6144,
	.i4ModeIndex = 2, /*HVBin 2; VBin 3*/
	.sPDMapInfo[0] = {
		.i4PDPattern = 1, /*1: Dense; 2: Sparse LR interleaved; 3: Sparse LR non interleaved*/
		.i4BinFacX = 4, /*for Dense*/
		.i4BinFacY = 4,
		.i4PDRepetition = 4,
		.i4PDOrder = {1},
	},
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <slim_video>
		{0, 0}, {0, 0}, {0, 384}, {64, 228}, {0, 0},
		// <cust1> <cust2> <cust3> <cust4> <cust5>
		{0, 0}, {0, 192}, {0, 384}, {0, 0}, {2048, 1536},
		// <cust6> <cust7> <cust8> <cust9>< cust10>
		{0, 0}, {0, 384}, {64, 48}, {0, 384}, {0, 0},
		// cust11 cust12 cust13 <cust14> <cust15>
		{480, 462}, {0, 0}, {2048, 1536}, {0, 0}, {0, 0},
		// <cust16> <cust17> cust18 <cust19> <cust20>
		{0, 0}, {0, 384}, {0, 384}, {256, 912},{0, 0},
		// <cust21> <cust22> cust23 <cust24> <cust25>
		{0, 0}, {0, 0}, {2048,1536},{2048,1536}, {0, 0},
		// <cust26> <cust27> cust28 <cust29> <cust30>
		{0, 0}, {64, 48}, {0, 384},{0, 384}, {2048,1536},
		// <cust31> <cust32>
		{896,672}, {896,672},
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_8k = {
	.i4OffsetX	= 0,
	.i4OffsetY	= 0,
	.i4PitchX	= 0,
	.i4PitchY	= 0,
	.i4PairNum	= 0,
	.i4SubBlkW	= 0,
	.i4SubBlkH	= 0,
	.iMirrorFlip = 0,
	.i4PosL = {{0, 0}},
	.i4PosR = {{0, 0}},
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4FullRawW = 8192,
	.i4FullRawH = 6144,
	.i4ModeIndex = 2, /*HVBin 2; VBin 3*/
	.sPDMapInfo[0] = {
		.i4PDPattern = 1, /*1: Dense; 2: Sparse LR interleaved; 3: Sparse LR non interleaved*/
		.i4BinFacX = 4, /*for Dense*/
		.i4BinFacY = 8,
		.i4PDRepetition = 4,
		.i4PDOrder = {1},
	},
	.i4Crop = {
		// <pre> <cap> <normal_video> <hs_video> <slim_video>
		{0, 0}, {0, 0}, {0, 384}, {64, 228}, {0, 0},
		// <cust1> <cust2> <cust3> <cust4> <cust5>
		{0, 0}, {0, 192}, {0, 384}, {0, 0}, {2048, 1536},
		// <cust6> <cust7> <cust8> <cust9>< cust10>
		{0, 0}, {0, 384}, {64, 48}, {0, 384}, {0, 0},
		// cust11 cust12 cust13 <cust14> <cust15>
		{480, 462}, {0, 0}, {2048, 1536}, {0, 0}, {0, 0},
		// <cust16> <cust17> cust18 <cust19> <cust20>
		{0, 0}, {0, 384}, {0, 384}, {256, 912},{0, 0},
		// <cust21> <cust22> cust23 <cust24> <cust25>
		{0, 0}, {0, 0}, {2048,1536},{2048,1536}, {0, 0},
		// <cust26> <cust27> cust28 <cust29> <cust30>
		{0, 0}, {64, 48}, {0, 384},{0, 384}, {2048,1536}
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 768,
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 576,
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
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 3840,
			.vsize = 270,
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 288,
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 576,
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus8[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2304,
			.vsize = 1728,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2304,
			.vsize = 432,
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 576,
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
			.hsize = 1088,
			.vsize = 612,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2176,
			.vsize = 153,
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 768,
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 768,
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 768,
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 576,
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 576,
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 3840,
			.vsize = 540,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 3840,
			.vsize = 540,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
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
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 576,
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
			.vsize = 2306,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 576,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus30[] = {
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
			.hsize = 2048,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
    {
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
		},
	},
};

static struct subdrv_mode_struct mode_struct[] = {
	// preview mode 0
	// Mode_5 binning_4096x3072 @30fps-RAW10
	// PD size = 4096x768
	// Tline = 12.00us
	// VB = 24.096ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = rothkoov50h40wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_preview_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_HD_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 900,
		.framelength = 2776,
		.max_framerate = 300,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
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
		.mode_setting_table = rothkoov50h40wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 900,
		.framelength = 2776,
		.max_framerate = 300,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// normal_video mode 2
	// Mode_7 binning_crop_4096x2304 @30fps-RAW10
	// PD size = 4096x576
	// Tline = 12us
	// VB = 26.400 ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = rothkoov50h40wide_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_normal_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_normal_video_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_normal_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 900,
		.framelength = 2776,
		.max_framerate = 300,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.dpc_enabled = FALSE,
		.pdc_enabled = FALSE,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// hs_video mode 3 : smvr 240fps
	// Mode_17 binning_crop_1920x1080 @240fps-RAW10
	// PD Size = 4096x288
	// Tline = 12us
	// VB = 0.912ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = rothkoov50h40wide_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_hs_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_hs_video_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_hs_video_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 900,
		.framelength = 346,
		.max_framerate = 2400,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.mode_setting_table = rothkoov50h40wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 900,
		.framelength = 2776,
		.max_framerate = 300,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
	// custom1 mode 5: same as preview mode
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = rothkoov50h40wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 900,
		.framelength = 2776,
		.max_framerate = 300,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
	// custom2 mode 6 : smvr 120fps
	// Mode_16 binning_crop_2048x1152 @120fps-RAW10
	// PD Size = 4096*288
	// Tline = 12us
	// VB = 4.872ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = rothkoov50h40wide_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom2_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom2_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom2_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 900,
		.framelength = 694,
		.max_framerate = 1200,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
	// custom3 mode 7 : 60.56fps video
	// Mode_8_4096x2304 @60fps-RAW10
	// PD Size = 4096x576
	// Tline = 12us
	// VB = 9.6ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = rothkoov50h40wide_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom3_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom3_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom3_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 900,
		.framelength = 1386,
		.max_framerate = 601,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
	// Mode_1_8192x6144 @30fps RAW10
	// Tline = 10.24us
	// VB = 1.802ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = rothkoov50h40wide_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom4_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_HD_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom4_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom4_setting),
		.hdr_mode = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 768,
		.framelength = 3248,
		.max_framerate = 300,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 8,
		.coarse_integ_step = 1,
		.min_exposure_line = 4,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.9375f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 15.9375f,
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
	// Mode_3 fake_14bit_crop_4096x3072 @30fps fake RAW14
	// PD Size = 2048x1536
	// Tline = 10.24 us
	// VB = 17.531ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = rothkoov50h40wide_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom5_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom5_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom5_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 768,
		.framelength = 3248,
		.max_framerate = 300,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 8,
		.coarse_integ_step = 1,
		.min_exposure_line = 4,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.9375f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 15.9375f,
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
		.saturation_info = &rothkoov50h40_saturation_info_fake14bit,
	},
	// custom6 mode 10 (Mode_5): same as preview mode bokeh
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = rothkoov50h40wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 900,
		.framelength = 3470,
		.max_framerate = 240,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
	// Mode_14	fake_14bit_binning_crop_4096x2304
	// PD Size = 4096x576
	// Tline = 14us
	// VB = 25.256ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus18,
		.num_entries = ARRAY_SIZE(frame_desc_cus18),
		.mode_setting_table = rothkoov50h40wide_custom18_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom18_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_SUPER_NIGHT_VIDEO,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom18_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom18_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 1124,
		.framelength = 2780,
		.max_framerate = 240,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.saturation_info = &rothkoov50h40_saturation_info_fake14bit,
	},
	//custom8 mode 12 : 24fps 10bit binning crop for bokeh
	// Mode_26 binning_crop_2304x1728 PD[10bit]
	// PD Size = 2304x432
	// Tline = 10us
	// VB = 36.4ms
	// XiaoMi_N12_OV50H_setting_0328_V2.9
	{
		.frame_desc = frame_desc_cus8,
		.num_entries = ARRAY_SIZE(frame_desc_cus8),
		.mode_setting_table = rothkoov50h40wide_custom8_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom8_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom8_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom8_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 900,
		.framelength = 3472,
		.max_framerate = 240,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 63.75f,
		.ana_gain_min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 1792,
			.y0_offset = 1344,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// custom9 mode 13: hdr video(DXG HDR video)
	// Mode_11 DCG_CMB_4096x2304_LSB @30fps RAW14  DXG
	// PD Size = 4096x576
	// Tline = 25.493 us
	// VB = 18.61 ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus9,
		.num_entries = ARRAY_SIZE(frame_desc_cus9),
		.mode_setting_table = rothkoov50h40wide_custom9_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom9_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MODE,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom9_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom9_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 75000000,
		.linelength = 2448,
		.framelength = 1020,
		.max_framerate = 300,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 3, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN,
		.saturation_info = &rothkoov50h40_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = PARAM_UNDEFINED,
			.dcg_gain_table_size = PARAM_UNDEFINED,
			.dcg_ratio_group = {5836, 1024}, // HCG = 5.7*1024, LCG = 1024
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
	// Mode_9 fake_14bit_stg3_4096x3072 @30fps-DOL3-RAW14-no PD
	// Tline = 41.06 us
	// VB = 1.077 ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus10,
		.num_entries = ARRAY_SIZE(frame_desc_cus10),
		.mode_setting_table = rothkoov50h40wide_custom10_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom10_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom10_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom10_setting),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 3,
		.exp_cnt = 3,
		.pclk = 75000000,
		.linelength = 1040,
		.framelength = 800 * 3,
		.max_framerate = 300,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 768 * 3, // 3072 / 4 * 3
		.read_margin = 24 * 3, // READ_MARGIN: This value is fixed as 24d.
		.framelength_step = 2 * 3,
		.coarse_integ_step = 2 * 3,
		.min_exposure_line = 2 * 3,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2 * 3,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 2 * 3,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_SE].min = 2 * 3,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_SE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_SE].min = BASEGAIN,
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
		.saturation_info = &rothkoov50h40_saturation_info_fake14bit,
	},
	// custom11 mode 15: smvr 480fps
	// Mode_18  binning_crop_1088x612 @480fps-RAW10-Partial PD
	// PD Size = 2176 * 153
	// Tline = 1.60us
	// VB = 0.19ms
	// XiaoMi_N12_OV50H_setting_0328_V2.9
	{
		.frame_desc = frame_desc_cus11,
		.num_entries = ARRAY_SIZE(frame_desc_cus11),
		.mode_setting_table = rothkoov50h40wide_custom11_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom11_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom11_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom11_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 908,
		.framelength = 172,
		.max_framerate = 4800,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 1920,
			.y0_offset = 1848,
			.w0_size = 4352,
			.h0_size = 2448,
			.scale_w = 1088,
			.scale_h = 612,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1088,
			.h1_size = 612,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1088,
			.h2_tg_size = 612,
		},
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_smvr,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// custom12 mode 16: fullsize 50M quad bayer RawSR
	// Mode_2 fake_14bit_8192x6144 @max_fps-noRMSC-RAW14-noPD
	// Tline = 10.88us
	// VB = 0.0.661ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus12,
		.num_entries = ARRAY_SIZE(frame_desc_cus12),
		.mode_setting_table = rothkoov50h40wide_custom12_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom12_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom12_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom12_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 856,
		.framelength = 3200,
		.max_framerate = 270,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 8,
		.coarse_integ_step = 1,
		.min_exposure_line = 4,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.9375f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 15.9375f,
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
		.saturation_info = &rothkoov50h40_saturation_info_fake14bit,
	},
	// custom13 mode 17: fullsize crop 12.5M quad bayer RawSR
	// Mode_4 fake_14bit_crop_QPD_4096x3072 @30fps-noRMSC-RAW14-noPD
	// Tline = 10.24us
	// VB = 17.53088ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus13,
		.num_entries = ARRAY_SIZE(frame_desc_cus13),
		.mode_setting_table = rothkoov50h40wide_custom13_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom13_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom13_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom13_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 768,
		.framelength = 3248,
		.max_framerate = 300,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 8,
		.coarse_integ_step = 1,
		.min_exposure_line = 4,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.9375f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 15.9375f,
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
		.saturation_info = &rothkoov50h40_saturation_info_fake14bit,
	},
	// custom14 mode 18: fake 14bit binning preview
	// Mode_6 fake_14bit_binning_4096x3072 @30fps-RAW14-All PD
	// PD Size = 4096x768
	// Tline = 13.33us
	// VB = 23.08ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus14,
		.num_entries = ARRAY_SIZE(frame_desc_cus14),
		.mode_setting_table = rothkoov50h40wide_custom14_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom14_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom14_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom14_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 1408,
		.framelength = 886,
		.max_framerate = 600,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.saturation_info = &rothkoov50h40_saturation_info_fake14bit,
	},
	// custom15 mode 19: DXG HDR preview
	// Mode_10 DCG_CMB_4096x3072_LSB @30fps-DXG[1:16]-RAW14-All PD HSG
	// PD Size = 4096x768
	// Tline = 25.493us
	// VB = 13.715ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus15,
		.num_entries = ARRAY_SIZE(frame_desc_cus15),
		.mode_setting_table = rothkoov50h40wide_custom15_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom15_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom15_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom15_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 75000000,
		.linelength = 2448,
		.framelength = 1020,
		.max_framerate = 300,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 3, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN,
		.saturation_info = &rothkoov50h40_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = PARAM_UNDEFINED,
			.dcg_gain_table_size = PARAM_UNDEFINED,
			.dcg_ratio_group = {5836, 1024}, // HCG = 5.7*1024, LCG = 1024
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
	// custom16 mode 20: CMS fake 14bit preview dark
	// Mode_12	fake_14bit_CMS_4096x3072
	// PD Size = 4096x768
	// Tline = 30.72us
	// VB = 9.71ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus16,
		.num_entries = ARRAY_SIZE(frame_desc_cus16),
		.mode_setting_table = rothkoov50h40wide_custom16_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom16_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom16_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom16_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 2304,
		.framelength = 1084,
		.max_framerate = 300,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 255,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 255,
		.ana_gain_min = BASEGAIN,
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
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &rothkoov50h40_saturation_info_fake14bit,
	},
	// custom17 mode 21: CMS fake 14bit video dark
	// Mode_13 fake_14bit_CMS_4096x3072
	// PD Size = 4096x768
	// Tline = 30.72us
	// VB = 15.606ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus17,
		.num_entries = ARRAY_SIZE(frame_desc_cus17),
		.mode_setting_table = rothkoov50h40wide_custom17_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom17_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MODE,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom17_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom17_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 2304,
		.framelength = 1084,
		.max_framerate = 300,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 255,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 255,
		.ana_gain_min = BASEGAIN,
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
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &rothkoov50h40_saturation_info_fake14bit,
	},
	// custom18 mode 22: fake 14bit normal video
	// Mode_14	fake_14bit_binning_crop_4096x2304
	// PD Size = 4096x576
	// Tline = 14us
	// VB = 25.256ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus18,
		.num_entries = ARRAY_SIZE(frame_desc_cus18),
		.mode_setting_table = rothkoov50h40wide_custom18_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom18_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MODE,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom18_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom18_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 1124,
		.framelength = 2224,
		.max_framerate = 300,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.saturation_info = &rothkoov50h40_saturation_info_fake14bit,
	},
	// custom19 mode 23 : 8K video
	// Mode_15 crop_7680x4320 @30fps 10bit
	// PD Size = 3840x2160
	// Tline = 10.24us
	// VB = 11.14ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus19,
		.num_entries = ARRAY_SIZE(frame_desc_cus19),
		.mode_setting_table = rothkoov50h40wide_custom19_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom19_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom19_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom19_setting),
		.hdr_mode = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 768,
		.framelength = 3248,
		.max_framerate = 300,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 8,
		.coarse_integ_step = 1,
		.min_exposure_line = 4,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.9375f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN ,
		.ana_gain_max = BASEGAIN * 15.9375f,
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
		.imgsensor_pd_info = &imgsensor_pd_info_8k,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// custom20 mode 24: authentic of LEICA
	// same as custom14 fake 14bit binning preview
	{
		.frame_desc = frame_desc_cus14,
		.num_entries = ARRAY_SIZE(frame_desc_cus14),
		.mode_setting_table = rothkoov50h40wide_custom14_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom14_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom14_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom14_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 1408,
		.framelength = 886,
		.max_framerate = 600,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.saturation_info = &rothkoov50h40_saturation_info_fake14bit,
	},
	// custom21 mode 25: authentic of LEICA
	// same as custom16 (Mode_12)CMS fake 14bit preview dark
	{
		.frame_desc = frame_desc_cus16,
		.num_entries = ARRAY_SIZE(frame_desc_cus16),
		.mode_setting_table = rothkoov50h40wide_custom16_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom16_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom16_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom16_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 2304,
		.framelength = 1084,
		.max_framerate = 300,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 255,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 255,
		.ana_gain_min = BASEGAIN,
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
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
		.saturation_info = &rothkoov50h40_saturation_info_fake14bit,
	},
	// custom22 mode 26: authentic of LEICA
	// same as custom15(Mode_10) DXG HDR preview
	{
		.frame_desc = frame_desc_cus15,
		.num_entries = ARRAY_SIZE(frame_desc_cus15),
		.mode_setting_table = rothkoov50h40wide_custom15_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom15_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom15_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom15_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 75000000,
		.linelength = 2448,
		.framelength = 1020,
		.max_framerate = 300,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 3, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN,
		.saturation_info = &rothkoov50h40_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = PARAM_UNDEFINED,
			.dcg_gain_table_size = PARAM_UNDEFINED,
			.dcg_ratio_group = {5836, 1024}, // HCG = 5.7*1024, LCG = 1024
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
	// same as custom5(Mode_3) ISZ
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = rothkoov50h40wide_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom5_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom5_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom5_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 768,
		.framelength = 3248,
		.max_framerate = 300,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 8,
		.coarse_integ_step = 1,
		.min_exposure_line = 4,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.9375f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 15.9375f,
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
		.saturation_info = &rothkoov50h40_saturation_info_fake14bit,
	},
	// custom24 mode 28: authentic of LEICA
	// same as custom13(Mode_4) fullsize crop 12.5M quad bayer RawSR
	{
		.frame_desc = frame_desc_cus13,
		.num_entries = ARRAY_SIZE(frame_desc_cus13),
		.mode_setting_table = rothkoov50h40wide_custom13_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom13_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom13_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom13_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 768,
		.framelength = 3248,
		.max_framerate = 300,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 8,
		.coarse_integ_step = 1,
		.min_exposure_line = 4,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.9375f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 15.9375f,
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
		.saturation_info = &rothkoov50h40_saturation_info_fake14bit,
	},
	// custom25 mode 29: authentic of LEICA
	// same as custom10(Mode_9) 3exp stagger hdr capture
	{
		.frame_desc = frame_desc_cus10,
		.num_entries = ARRAY_SIZE(frame_desc_cus10),
		.mode_setting_table = rothkoov50h40wide_custom10_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom10_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_NORMAL_CAP_MODE_LCICA_AUTHENTIC,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom10_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom10_setting),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 3,
		.exp_cnt = 3,
		.pclk = 75000000,
		.linelength = 1040,
		.framelength = 800 * 3,
		.max_framerate = 300,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 768 * 3, // 3072 / 4 * 3
		.read_margin = 24 * 3, // READ_MARGIN: This value is fixed as 24d.
		.framelength_step = 2 * 3,
		.coarse_integ_step = 2 * 3,
		.min_exposure_line = 2* 3,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2 * 3,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 2 * 3,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_SE].min = 2 * 3,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_SE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_SE].min = BASEGAIN,
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
		.saturation_info = &rothkoov50h40_saturation_info_fake14bit,
	},
	// custom26 mode 30: authentic of LEICA
	// same as preview*(Mode_5) mode bokeh
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = rothkoov50h40wide_preview_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_preview_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_preview_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 900,
		.framelength = 2776,
		.max_framerate = 300,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
	// custom27 mode 31 : 24fps 10bit binning crop for bokeh leica
	// Mode_26 binning_crop_2304x1728 PD[10bit]
	// PD Size = 2304x432
	// Tline = 10us
	// VB = 36.4ms
	// XiaoMi_N12_OV50H_setting_0328_V2.9
	{
		.frame_desc = frame_desc_cus8,
		.num_entries = ARRAY_SIZE(frame_desc_cus8),
		.mode_setting_table = rothkoov50h40wide_custom8_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom8_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom8_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom8_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 900,
		.framelength = 3472,
		.max_framerate = 240,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 63.75f,
		.ana_gain_min = BASEGAIN,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 1792,
			.y0_offset = 1344,
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
		.dpc_enabled = true,
		.pdc_enabled = true,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
	},
	// custom28 mode 32: hdr video 60fps Mi-CineLook
	// Mode_23 4C2PlusDCGCombine_14bit_4096x2304_60fps_LSB PD[10bit]
	// PD Size = 508x576
	// Tline = 25.493us
	// VB = 1.836ms
	// XiaoMi_N12_OV50H_setting_0328_V2.9_for_XIAOMI
	{
		.frame_desc = frame_desc_cus28,
		.num_entries = ARRAY_SIZE(frame_desc_cus28),
		.mode_setting_table = rothkoov50h40wide_custom28_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom28_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MI_CINELOOK_MODE,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom28_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom28_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 75000000,
		.linelength = 1932,
		.framelength = 646,
		.max_framerate = 600,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 3, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN,
		.saturation_info = &rothkoov50h40_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = PARAM_UNDEFINED,
			.dcg_gain_table_size = PARAM_UNDEFINED,
			.dcg_ratio_group = {5836, 1024}, // HCG = 5.7*1024, LCG = 1024
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
	// custom29 mode 33 : 60fps fake14bit normal video Mi-CineLook
	// Mode_24 RAW14_4096x2304 @60fps-RAW14-All PD[10bit]
	// PD Size = 4096x576
	// Tline = 14.00us
	// VB = 8.456ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus29,
		.num_entries = ARRAY_SIZE(frame_desc_cus29),
		.mode_setting_table = rothkoov50h40wide_custom29_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom29_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_VIDEO_MI_CINELOOK_MODE,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom29_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom29_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 1124,
		.framelength = 1102,
		.max_framerate = 600,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 2, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
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
		.saturation_info = &rothkoov50h40_saturation_info_fake14bit,
	},
	// custom30 mode 34 : 24fps 10bit normal full size crop for bokek
	// Mode_25 crop_4096x3072 PD[10bit]
	// PD Size = 2048 *1356
	// Tline = 10.240us
	// VB = 15.729ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus30,
		.num_entries = ARRAY_SIZE(frame_desc_cus30),
		.mode_setting_table = rothkoov50h40wide_custom30_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom30_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom30_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom30_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 768,
		.framelength = 4060,
		.max_framerate = 240,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 8,
		.coarse_integ_step = 1,
		.min_exposure_line = 4,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.9375f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 15.9375f,
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
	},
	// custom31 mode 35 : 24fps 10bit normal full size crop for bokek leica
	// Mode_25 crop_4096x3072 PD[10bit]
	// PD Size = 2048 *1356
	// Tline = 10.240us
	// VB = 15.729ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus30,
		.num_entries = ARRAY_SIZE(frame_desc_cus30),
		.mode_setting_table = rothkoov50h40wide_custom30_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom30_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom30_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom30_setting),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 75000000,
		.linelength = 768,
		.framelength = 4060,
		.max_framerate = 240,
		.mipi_pixel_rate = 2233000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 8,
		.coarse_integ_step = 1,
		.min_exposure_line = 4,
		.mi_mode_type = 1, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.9375f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.ana_gain_max = BASEGAIN * 15.9375f,
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
	},
	// custom32 mode 36 : DXG super night video
	// Mode_11 DCG_CMB_4096x2304_LSB @30fps RAW14  DXG
	// PD Size = 4096x576
	// Tline = 25.493 us
	// VB = 18.61 ms
	// XiaoMi_N12_OV50H_setting_0523_V3.3_for_XIAOMI
	{
		.frame_desc = frame_desc_cus9,
		.num_entries = ARRAY_SIZE(frame_desc_cus9),
		.mode_setting_table = rothkoov50h40wide_custom9_setting,
		.mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom9_setting),
		.seamless_switch_group = SEAMLESS_SWITCH_GROUP_SUPER_NIGHT_VIDEO,
		.seamless_switch_mode_setting_table = rothkoov50h40wide_custom9_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(rothkoov50h40wide_custom9_setting),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 75000000,
		.linelength = 2448,
		.framelength = 1272,
		.max_framerate = 240,
		.mipi_pixel_rate = 1595000000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 2,
		.mi_mode_type = 3, //defu :0 ; full size: 1; bining 2; DXG: 3; CMS: 4;
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 63.75f,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN,
		.saturation_info = &rothkoov50h40_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_DIRECT_MODE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = PARAM_UNDEFINED,
			.dcg_gain_table_size = PARAM_UNDEFINED,
			.dcg_ratio_group = {5836, 1024}, // HCG = 5.7*1024, LCG = 1024
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
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = ROTHKOOV50H40WIDE_SENSOR_ID,
	.reg_addr_sensor_id = {0x300A, 0x300B},
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
	.dig_gain_min = BASEGAIN,
	.dig_gain_max = BASEGAIN * 32,
	.dig_gain_step = 1,  //If the value is 0, SENSOR_FEATURE_SET_MULTI_DIG_GAIN is disabled
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN,
	.ana_gain_max = BASEGAIN * 63.75f,
	.ana_gain_type = 0,
	.ana_gain_step = 1,
	.ana_gain_table = rothkoov50h40wide_ana_gain_table,
	.ana_gain_table_size = sizeof(rothkoov50h40wide_ana_gain_table),
	.min_gain_iso = 50,
	.exposure_def = 0x3D0,
	.exposure_min = 4,
	.exposure_max = 0xFFFFFF - 40,
	.exposure_step = 2,
	.exposure_margin = 40,
	.saturation_info = &rothkoov50h40_saturation_info_10bit,

	.frame_length_max = 0xFFFFFF,
	.ae_effective_frame = 3,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 115500,

	.pdaf_type = PDAF_SUPPORT_CAMSV_QPD,
	.hdr_type = HDR_SUPPORT_STAGGER_FDOL|HDR_SUPPORT_DCG,
	.seamless_switch_support = TRUE,
	.temperature_support = TRUE,

	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.s_mi_stream = mi_stream,
	.s_mi_read_CGRatio = mi_read_CGRatio,
#if EEPROM_READY
	.s_cali = set_sensor_cali,
#else
	.s_cali = NULL,
#endif	

	//.reg_addr_stream = 0x0100,
	//.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {
		{0x3500, 0x3501, 0x3502},
		{0x3540, 0x3541, 0x3542},
		{0x3580, 0x3581, 0x3582}
	},
	.long_exposure_support = TRUE,
//	.reg_addr_exposure_lshift = 0x3160,
	.reg_addr_ana_gain = {
		{0x3508, 0x3509},
		{0x3548, 0x3549},
		{0x3588, 0x3589}
	},
	.reg_addr_dig_gain = {
		{0x350A, 0x350B, 0x350C},
		{0x354A, 0x354B, 0x354C},
		{0x358A, 0x358B, 0x358C}
	},
	.reg_addr_dcg_ratio = PARAM_UNDEFINED, //TBD
	.reg_addr_frame_length = {0x3840, 0x380E, 0x380F},
	.reg_addr_temp_en = 0x4D12,
	.reg_addr_temp_read = 0x4D13,
//	.reg_addr_auto_extend = 0x0350,
	.reg_addr_frame_count = 0x387F,
//	.reg_addr_fast_mode = 0x3010,

//	.mi_enable_async = 1, // enable async setting
//	.mi_i2c_type = 1,
	.mi_hb_vb_cal = true,
	.mi_dxg_reg = 0x5060,
	.mi_disable_set_dummy = 1, // disable set dummy
	.mi_evaluate_frame_rate_by_scenario = evaluate_frame_rate_by_scenario,
	.init_setting_table = rothkoov50h40wide_init_setting,
	.init_setting_len = ARRAY_SIZE(rothkoov50h40wide_init_setting),
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
	.get_csi_param = get_csi_param,
	.update_sof_cnt = common_update_sof_cnt,
	.vsync_notify = vsync_notify,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST,   0,       1},
	{HW_ID_AFVDD, 3300000, 3},
	{HW_ID_DVDD1, 1800000, 2}, // vcam_ldo
	{HW_ID_AVDD,  2800000, 2},
	{HW_ID_DOVDD, 1800000, 2},
	{HW_ID_DVDD,  1200000, 2},
	{HW_ID_MCLK,  24,	   0},
	{HW_ID_MCLK_DRIVING_CURRENT, 6, 5},
	{HW_ID_RST,   1,       10},
};

const struct subdrv_entry rothkoov50h40wide_mipi_raw_entry = {
	.name = SENSOR_DRVNAME_ROTHKOOV50H40WIDE_MIPI_RAW,
	.id = ROTHKOOV50H40WIDE_SENSOR_ID,
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

static void mi_stream(void *arg,bool enable)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u32 len = 0;
	DRV_LOG_MUST(ctx, "Enter %s enable = %d \n", __FUNCTION__,enable);
	if(enable){
		rothkoov50h40wide_set_awb_gain(ctx, (u8 *)&g_last_awb_gain, &len);
		subdrv_i2c_wr_u8(ctx,0x3016, 0xf1);
		subdrv_i2c_wr_u8(ctx,0x3a88, 0x44);
		subdrv_i2c_wr_u8(ctx,0x3bd7, 0xc8);
		subdrv_i2c_wr_u8(ctx,0x3bdc, 0x40);
		subdrv_i2c_wr_u8(ctx,0x3be5, 0x01);
		subdrv_i2c_wr_u8(ctx,0x392e, 0x5a);
		subdrv_i2c_wr_u8(ctx,0x0100, 0x01);
		mdelay(4);
		subdrv_i2c_wr_u8(ctx,0x3a88, 0x04);
		subdrv_i2c_wr_u8(ctx,0x3bd7, 0x48);
		subdrv_i2c_wr_u8(ctx,0x3bdc, 0x00);
		subdrv_i2c_wr_u8(ctx,0x3be5, 0x00);
		subdrv_i2c_wr_u8(ctx,0x392e, 0x0a);
		subdrv_i2c_wr_u8(ctx,0x3016, 0xf0);
	}else{
		subdrv_i2c_wr_u8(ctx,0x0100, 0x00);
	}
}

static void mi_read_CGRatio(void *arg){
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	int i = 0;
	DRV_LOG_MUST(ctx, "Enter %s\n", __FUNCTION__);
	if(ctx->s_ctx.mi_dxg_reg){
		subdrv_i2c_wr_u8(ctx,0x3d8c,0x9b);
		subdrv_i2c_wr_u8(ctx,0x3d8d,0xa0);
		subdrv_i2c_wr_u8(ctx,0x3016,0xf1);
		subdrv_i2c_wr_u8(ctx,0x0100,0x01);
		mdelay(4);
		ctx->s_ctx.mi_dxg_ratio = subdrv_i2c_rd_u16(ctx,ctx->s_ctx.mi_dxg_reg);
		subdrv_i2c_wr_u8(ctx,0x0100,0x00);
		DRV_LOG(ctx, "mi_dxg_ratio(%d) \n",ctx->s_ctx.mi_dxg_ratio);
	}
	if(ctx->s_ctx.mi_dxg_ratio < 5734 || ctx->s_ctx.mi_dxg_ratio > 6144){
		ctx->s_ctx.mi_dxg_ratio = 5900;
		DRV_LOGE(ctx, "E: mi_dxg_ratio(%d) ratio need (5734,6144) \n",ctx->s_ctx.mi_dxg_ratio);
	}
	for (i = 0; i < ctx->s_ctx.sensor_mode_num ; i++) {
		if(ctx->s_ctx.mode[i].hdr_mode == HDR_RAW_DCG_COMPOSE){
			ctx->s_ctx.mode[i].dcg_info.dcg_ratio_group[0] = ctx->s_ctx.mi_dxg_ratio;
			DRV_LOG(ctx, "mode(%d) dcg_ratio_group[0](%d) \n",i,ctx->s_ctx.mode[i].dcg_info.dcg_ratio_group[0]);
		}

		if(ctx->s_ctx.mode[i].mi_mode_type == 1){//full size / full size crop mode
			if(ctx->s_ctx.mi_dxg_ratio > 6025)//CG ratio * 1000 / 1024 > 5884
				ctx->s_ctx.mode[i].ae_binning_ratio = (5884 * 1024) / ctx->s_ctx.mi_dxg_ratio;
			else
				ctx->s_ctx.mode[i].ae_binning_ratio = 1000;
		} else {
			if(ctx->s_ctx.mi_dxg_ratio > 6025)//CG ratio * 1000 / 1024 > 5884
				ctx->s_ctx.mode[i].ae_binning_ratio = 1000;
			else
				ctx->s_ctx.mode[i].ae_binning_ratio = (5884 * 1024) / ctx->s_ctx.mi_dxg_ratio;
		}
		DRV_LOG(ctx,"mode(%d) mi_dxg_ratio(%d) ae_binning_ratio = %d\n",
				i,ctx->s_ctx.mi_dxg_ratio,ctx->s_ctx.mode[i].ae_binning_ratio);
	}
}


#if EEPROM_READY
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

	/* PDC data 1th */
	support = info[idx].pdc_support;
	pbuf = info[idx].preload_pdc_table;
	size = info[idx].pdc_size;
	addr = info[idx].sensor_reg_addr_pdc;
	if (support) {
		if (pbuf != NULL && addr > 0 && size > 0) {
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set 1th PDC calibration data done.");
		} else {
			DRV_LOG(ctx, "pbuf(%p) addr(%d) size(%d) set 1th PDC calibration data fail.",
				pbuf, addr, size);
		}
	}

	/* PDC data 2th */
	support = info[idx].lrc_support;
	pbuf = info[idx].preload_lrc_table;
	size = info[idx].lrc_size;
	addr = info[idx].sensor_reg_addr_lrc;
	if (support) {
		if (pbuf != NULL && addr > 0 && size > 0) {
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set 2th PDC calibration data done.");
		} else {
			DRV_LOG(ctx, "pbuf(%p) addr(%d) size(%d) set 2th PDC calibration data fail.",
				pbuf, addr, size);
		}
	}

	/* PDC data 3th */
	support = info[idx].xtalk_support;
	pbuf = info[idx].preload_xtalk_table;
	size = info[idx].xtalk_size;
	addr = info[idx].sensor_reg_addr_xtalk;
	if (support) {
		if (pbuf != NULL && addr > 0 && size > 0) {
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set 3th PDC calibration data done.");
		} else {
			DRV_LOG(ctx, "pbuf(%p) addr(%d) size(%d) set 3th PDC calibration data fail.",
				pbuf, addr, size);
		}
	}
}
#endif
static int get_sensor_temperature(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	int temperature = 0;

	/*TEMP_SEN_CTL */
	subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_temp_en, 0x01);
	temperature = subdrv_i2c_rd_u8(ctx, ctx->s_ctx.reg_addr_temp_read);
	temperature = (temperature > 0xC0) ? (temperature - 0x100) : temperature;

	if (ctx->sof_cnt < 3 && -1 == temperature)
		temperature = 0;

	DRV_LOG(ctx, "temperature: %d degrees\n", temperature);
	return temperature;

}

static void set_group_hold(void *arg, u8 en)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	if (en)
		set_i2c_buffer(ctx, 0x3208, 0x00);
	else{
		set_i2c_buffer(ctx, 0x3208, 0x10);
		set_i2c_buffer(ctx, 0x3208, 0xA0);
	}
}

static u16 get_gain2reg(u32 gain)
{
	return gain * 256 / BASEGAIN;
}

static int rothkoov50h40wide_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	enum IMGSENSOR_HDR_MODE_ENUM src_hdr;
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

	ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_LE] = ae_ctrl->gain.le_gain;
	ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_ME] = ae_ctrl->gain.me_gain;

	exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
	ctx->is_seamless = TRUE;
	src_hdr = ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode;

	update_mode_info(ctx, scenario_id);

	if(ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE){
		i2c_table_write(ctx,rothkoov50h40wide_seamless_switch2DXG_step1,
			ARRAY_SIZE(rothkoov50h40wide_seamless_switch2DXG_step1));
	}else{
		i2c_table_write(ctx,rothkoov50h40wide_seamless_switch_step1,
			ARRAY_SIZE(rothkoov50h40wide_seamless_switch_step1));
	}

	i2c_table_write(ctx,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);

	if (scenario_id == SENSOR_SCENARIO_ID_CUSTOM14 ||
		scenario_id == SENSOR_SCENARIO_ID_CUSTOM20) {
		set_max_framerate_by_scenario(ctx, scenario_id, 300);
	}
#if 0
	i2c_table_write(ctx,rothkoov50h40wide_seamless_switch_step2,
		ARRAY_SIZE(rothkoov50h40wide_seamless_switch_step2));
#endif
	if (ae_ctrl) {
		switch (ctx->s_ctx.mode[scenario_id].hdr_mode) {
		case HDR_RAW_STAGGER:
			//first frame not Calculate constraint conditions
			set_multi_shutter_frame_length(ctx, (u64 *)&ae_ctrl->exposure, exp_cnt, 0);
			set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		case HDR_RAW_LBMF:
			set_multi_shutter_frame_length_in_lut(ctx,
				(u64 *)&ae_ctrl->exposure, exp_cnt, 0, frame_length_in_lut);
			set_multi_gain_in_lut(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		case HDR_RAW_DCG_COMPOSE:
			rothkoov50h40wide_set_shutter_frame_length(ctx, (u8 *)&ae_ctrl->exposure, len);
			rothkoov50h40wide_set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			break;
		default:
			rothkoov50h40wide_set_shutter(ctx, (u8 *)&ae_ctrl->exposure.le_exposure,len);
			set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		}
	}

	if(src_hdr == HDR_RAW_STAGGER){//use non-voerlap type; step4 is non-overlap seamless setting
		i2c_table_write(ctx,rothkoov50h40wide_stagger_seamless_switch_step4,
			ARRAY_SIZE(rothkoov50h40wide_stagger_seamless_switch_step4));
	}else{
		i2c_table_write(ctx,rothkoov50h40wide_liner_seamless_switch_step3,
			ARRAY_SIZE(rothkoov50h40wide_liner_seamless_switch_step3));
	}

	ctx->fast_mode_on = TRUE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->is_seamless = FALSE;
	DRV_LOG(ctx, "X: ctx->current_scenario_id %d,0x3506 = 0x%x\n",ctx->current_scenario_id,subdrv_i2c_rd_u8(ctx,0x3506));
	DRV_LOG(ctx, "X: set seamless switch done\n");
	return ERROR_NONE;
}

static int rothkoov50h40wide_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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

	ctx->test_pattern = mode;
	return ERROR_NONE;

}

static int rothkoov50h40wide_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
/*	struct mtk_test_pattern_data *data = (struct mtk_test_pattern_data *)para;
	u16 R = (data->Channel_R >> 22) & 0x3ff;
	u16 Gr = (data->Channel_Gr >> 22) & 0x3ff;
	u16 Gb = (data->Channel_Gb >> 22) & 0x3ff;
	u16 B = (data->Channel_B >> 22) & 0x3ff;

	subdrv_i2c_wr_u8(ctx, 0x0602, (R >> 8));
	subdrv_i2c_wr_u8(ctx, 0x0603, (R & 0xff));
	subdrv_i2c_wr_u8(ctx, 0x0604, (Gr >> 8));
	subdrv_i2c_wr_u8(ctx, 0x0605, (Gr & 0xff));
	subdrv_i2c_wr_u8(ctx, 0x0606, (B >> 8));
	subdrv_i2c_wr_u8(ctx, 0x0606, (B & 0xff));
	subdrv_i2c_wr_u8(ctx, 0x0608, (Gb >> 8));
	subdrv_i2c_wr_u8(ctx, 0x0608, (Gb & 0xff));

	DRV_LOG(ctx, "mode(%u) R/Gr/Gb/B = 0x%04x/0x%04x/0x%04x/0x%04x\n",
		ctx->test_pattern, R, Gr, Gb, B);*/
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
		//set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x00);
		//commit_i2c_buffer(ctx);
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
		//	csi_param->cphy_settle = 0x04;
		break;
	default:
		csi_param->legacy_phy = 0;
		csi_param->not_fixed_trail_settle = 0;
		csi_param->not_fixed_dphy_settle = 0;
		break;
	}
	return 0;
}


static int rothkoov50h40wide_set_hdr_tri_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
	rothkoov50h40wide_set_multi_gain(ctx,values, exp_cnt);
	return 0;
}

static void rothkoov50h40wide_set_multi_gain(struct subdrv_ctx *ctx, u32 *gains, u16 exp_cnt)
{
	int i = 0;
	u32 rg_gains[3] = {0};
	u8 has_gains[3] = {0};
	u32 ration = 0, ration_back;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

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

		DRV_LOG(ctx, "mode[%d].multi_exposure_ana_gain_range[%d], max: 0x%x, min:0x%x \n",
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
	if(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE )
		set_i2c_buffer(ctx, 0x5006, 0x02);
	memset(has_gains, 1, sizeof(has_gains));
	switch (exp_cnt) {
	case 2:
		rg_gains[0] = gains[0];
		rg_gains[1] = gains[1];
		has_gains[2] = 0;
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
	for (i = 0; i < 3; i++) {
		if (has_gains[i]) {
			set_i2c_buffer(ctx,ctx->s_ctx.reg_addr_ana_gain[i].addr[0],(rg_gains[i] >> 8) & 0xFF);
			set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_ana_gain[i].addr[1],rg_gains[i] & 0xFF);
		}
	}
	if(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE ){
		if(ctx->s_ctx.mode[ctx->current_scenario_id].max_framerate / 10 <= 30){
			if(rg_gains[0] <= 0xF00){//LCG gain> 0x0F,need shift Dinu mode (0x0F00)
				set_i2c_buffer(ctx, 0x4547, 0x08);
			}else{
				set_i2c_buffer(ctx, 0x4547, 0x42);
			}
		}
		if(ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_LE] > 0 &&
			ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_ME] > 0){
			set_i2c_buffer(ctx, 0x5019, (ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_LE] >> 16)& 0xFF);
			set_i2c_buffer(ctx, 0x501a, (ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_LE] >> 8)& 0xFF);
			set_i2c_buffer(ctx, 0x501b, ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_LE] & 0xFF);
			set_i2c_buffer(ctx, 0x501c, (ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_ME] >> 16)& 0xFF);
			set_i2c_buffer(ctx, 0x501d, (ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_ME] >> 8)& 0xFF);
			set_i2c_buffer(ctx, 0x501e, ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_ME] & 0xFF);
		}
	}
	DRV_LOG(ctx, "total gain[lg/mg]: 0x%x 0x%x\n", ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_LE],
				ctx->s_ctx.mi_dcg_gain[IMGSENSOR_STAGGER_EXPOSURE_ME]);
	DRV_LOG(ctx, "reg[lg/mg/sg]: 0x%x 0x%x 0x%x\n", rg_gains[0], rg_gains[1], rg_gains[2]);

	ration = subdrv_i2c_rd_u16(ctx, 0x6a02);
	ration_back = subdrv_i2c_rd_u16(ctx, 0x5060);

	DRV_LOG(ctx, "ration:0x%x ration_back:0x%x \n",ration,ration_back);


	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 0);
	commit_i2c_buffer(ctx);
	/* group hold end */
}

void rothkoov50h40wide_write_frame_length(struct subdrv_ctx *ctx, u32 fll)
{
	u32 addr_h = ctx->s_ctx.reg_addr_frame_length.addr[0];
	u32 addr_l = ctx->s_ctx.reg_addr_frame_length.addr[1];
	u32 addr_ll = ctx->s_ctx.reg_addr_frame_length.addr[2];
	u32 fll_step = 0;
	u32 dol_cnt = 1;

	check_current_scenario_id_bound(ctx);
	fll_step = ctx->s_ctx.mode[ctx->current_scenario_id].framelength_step;
	if (fll_step)
		fll = roundup(fll, fll_step);
	ctx->frame_length = fll;

	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_STAGGER)
		dol_cnt = ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt;

	if ( dol_cnt == 3){
		fll = 2*fll;
	}

	fll = fll / dol_cnt;

	if (ctx->extend_frame_length_en == FALSE || ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_NONE) {
		if (addr_ll) {
			set_i2c_buffer(ctx,	addr_h,	(fll >> 16) & 0xFF);
			set_i2c_buffer(ctx,	addr_l, (fll >> 8) & 0xFF);
			set_i2c_buffer(ctx,	addr_ll, fll & 0xFF);
		} else {
			set_i2c_buffer(ctx,	addr_h, (fll >> 8) & 0xFF);
			set_i2c_buffer(ctx,	addr_l, fll & 0xFF);
		}
		/* update FL RG value after setting buffer for writting RG */
		ctx->frame_length_rg = ctx->frame_length;

		DRV_LOG(ctx,
			"ctx:(fl(RG):%u), fll[0x%x] multiply %u, fll_step:%u\n",
			ctx->frame_length_rg, fll, dol_cnt, fll_step);
	}
}

static int rothkoov50h40wide_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
		rothkoov50h40wide_write_frame_length(ctx, ctx->frame_length);
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
		DRV_LOG(ctx, "mode id = %d hdr_mode = %d",ctx->current_scenario_id,ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode);
		if(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE ){
			set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[1].addr[0],
				(ctx->exposure[0] >> 16) & 0xFF);
			set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[1].addr[1],
				(ctx->exposure[0] >> 8) & 0xFF);
			set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[1].addr[2],
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


static int rothkoov50h40wide_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	rothkoov50h40wide_set_shutter_frame_length(ctx, para, len);
	return 0;
}

static int rothkoov50h40wide_set_hdr_tri_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	int i = 0;
	u64 values[3] = {0};
	u32 frame_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u64 *shutters = (u64 *)para;
	int exp_cnt = 2;

	if (shutters != NULL) {
		for (i = 0; i < 3; i++)
			values[i] = (u64) *(shutters + i);
	}
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF) {
		set_multi_shutter_frame_length_in_lut(ctx,
			values, exp_cnt, 0, frame_length_in_lut);
		return 0;
	}
	DRV_LOG(ctx, "duchampov50ewide_set_hdr_tri_shutter");
	rothkoov50h40wide_set_multi_shutter_frame_length(ctx, para, len);
	return 0;
}


static int rothkoov50h40wide_set_multi_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	u64 *shutters = (u64 *)(* feature_data);
	u16 exp_cnt = (u16) (*(feature_data + 1));
	u16 frame_length = (u16) (*(feature_data + 2));

	int i = 0;
	int fine_integ_line = 0;
	u16 last_exp_cnt = 1;
	u32 calc_fl[3] = {0};
	u32 calc_fll = 0;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	u32 rg_shutters[3] = {0};
	u32 cit_step = 0;
	int dol_cnt = 1;

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
			shutters[i] = roundup(shutters[i], cit_step);
	}

	//ctx->exposure is last exp;shutters and ctx->frame_length is now exp;
	//3072 = ctx->s_ctx.mode[ctx->current_scenario_id].imgsensor_winsize_info.h2_tg_size
	if(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_STAGGER &&
		ctx->exposure[0] > 0 && ctx->exposure[1] > 0 && ctx->exposure[2] > 0){
		dol_cnt = ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt;
		//frame n. Frame n+2 image data is for Exp_L(n+2), Exp_M(n+2), Exp_S(n+2). Exp_L(n+2)
		//starts in frame n+1, Exp_M(n+2) and Exp_S(n+2) start in frame n+2.
		if(shutters[0] + ctx->exposure[1] + ctx->exposure[2] >= ctx->frame_length - 100 * dol_cnt){
			calc_fl[0] = shutters[0] + ctx->exposure[1] + ctx->exposure[2] + (100 + cit_step) * dol_cnt ;
			DRV_LOG(ctx, "calc_fl[0] = %u",calc_fl[0]);
		}
		//Below condition makes sure M/S frame in frame n+1 doesn't conflict to M/S frame in frame n+2
		if(ctx->frame_length + shutters[1] + shutters[2] < ctx->exposure[1] + ctx->exposure[2] + (3072/4) * dol_cnt ){
			if(((int)(ctx->exposure[1] + ctx->exposure[2] + (3072/4) * dol_cnt  - (shutters[1] + shutters[2]))) > 0)
				calc_fl[1] = ctx->exposure[1] + ctx->exposure[2] + (3072/4) * dol_cnt  - (shutters[1] + shutters[2]);
			else
				calc_fl[1] = 0;
		}
		//adjust stagger HDR to non-overlap
		if(shutters[1] + shutters[2] + (3072/4 + 110) * dol_cnt >= ctx->frame_length){
			calc_fl[2] = shutters[1] + shutters[2] + (3072/4 + 110 + cit_step) * dol_cnt;
			calc_fl[2] = calc_fl[2] + ctx->s_ctx.exposure_margin*exp_cnt*exp_cnt;
		}
		for (i = 0; i < ARRAY_SIZE(calc_fl); i++)
			ctx->frame_length = max(ctx->frame_length, calc_fl[i]);
		DRV_LOG(ctx, "dol_cnt[%d] calc_fl[%u/%u/%u] shutters[%llu/%llu/%llu], exposure[%u/%u/%u]\n",
			dol_cnt, calc_fl[0], calc_fl[1], calc_fl[2], shutters[0], shutters[1], shutters[2],
			ctx->exposure[0], ctx->exposure[1], ctx->exposure[2]);
	}
	//frame need > shutter + magin
	for (i = 0; i < exp_cnt; i++)
		calc_fll += (u32) shutters[i];
	calc_fll += ctx->s_ctx.exposure_margin*exp_cnt*exp_cnt;
	ctx->frame_length =	max(ctx->frame_length, calc_fll);
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
	/* write framelength */
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend)
		rothkoov50h40wide_write_frame_length(ctx, ctx->frame_length);
	/* write shutter */
	switch (exp_cnt) {
	case 1:
		rg_shutters[0] = (u32) shutters[0] / exp_cnt;
		break;
	case 2:
		rg_shutters[0] = (u32) shutters[0] / exp_cnt;
		rg_shutters[1] = (u32) shutters[1] / exp_cnt;
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
				//DXG
				DRV_LOG(ctx, "mode id = %d hdr_mode = %d",ctx->current_scenario_id,ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode);
				if(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE ){
					set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[1].addr[0],
						(rg_shutters[i] >> 16) & 0xFF);
					set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[1].addr[1],
						(rg_shutters[i] >> 8) & 0xFF);
					set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[1].addr[2],
						rg_shutters[i] & 0xFF);
				}
			} else {
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[0],
					(rg_shutters[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[1],
					rg_shutters[i] & 0xFF);
			}
		}
	}
	DRV_LOG(ctx, "exp[0x%x/0x%x/0x%x], fll(input/output):%u/%u, flick_en:%d\n",
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

static int rothkoov50h40wide_get_period_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len){
	u32 ratio = 1;
	u32 de_ratio= 1;
	u64 *feature_data = (u64 *) para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = (enum SENSOR_SCENARIO_ID_ENUM)*(feature_data);
	u32 *period = (u32 *)(uintptr_t)(*(feature_data + 1));
	u64 flag =*(feature_data + 2);

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}

	if(ctx->s_ctx.mode[scenario_id].mi_mode_type == 1 && flag & SENSOR_GET_LINELENGTH_FOR_READOUT)
		de_ratio = 2;
	else if(ctx->s_ctx.mode[scenario_id].mi_mode_type == 2 && flag & SENSOR_GET_LINELENGTH_FOR_READOUT)
		de_ratio = 4;
	else if(ctx->s_ctx.mode[scenario_id].mi_mode_type == 3 && flag & SENSOR_GET_LINELENGTH_FOR_READOUT)
		de_ratio = 8;

	DRV_LOG(ctx, "flag:%llu, scenario_id:%u, mode_num:%u de_ratio = %u\n",
		flag, scenario_id, ctx->s_ctx.sensor_mode_num,de_ratio);

	if (flag & SENSOR_GET_LINELENGTH_FOR_READOUT &&
		ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_STAGGER)
		ratio = ctx->s_ctx.mode[scenario_id].exp_cnt;
	*period = (ctx->s_ctx.mode[scenario_id].framelength << 16)
			+ (ctx->s_ctx.mode[scenario_id].linelength * ratio / de_ratio);

	return 0;
}


static int rothkoov50h40wide_get_cust_pixel_rate(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	DRV_LOG_MUST(ctx, "rothkoov50h40wide_get_cust_pixel_rate sensor mode = %llu",*feature_data);
	*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =  3415000000;

	return 0;
}

static int rothkoov50h40wide_get_readout_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = *feature_data;
	u64 *readout_time = feature_data + 1;
	u32 ratio = 1;
	u32 MIratio = 1;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return 0;
	}

	if(ctx->s_ctx.mi_hb_vb_cal){
		if(ctx->s_ctx.mode[scenario_id].mi_mode_type == 1)
			ratio = 2;
		if(ctx->s_ctx.mode[scenario_id].mi_mode_type == 2)
			ratio = 4;
		if(ctx->s_ctx.mode[scenario_id].mi_mode_type == 3)
			ratio = 4;
	}

	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_STAGGER)
		MIratio = ctx->s_ctx.mode[scenario_id].exp_cnt;
	*readout_time =
		(u64)ctx->s_ctx.mode[scenario_id].linelength
		* ctx->s_ctx.mode[scenario_id].imgsensor_winsize_info.h2_tg_size
		* 1000000000 / ctx->s_ctx.mode[scenario_id].pclk / ratio * MIratio;

	DRV_LOG_MUST(ctx, "sid:%u, mode_num:%u, readout_time = %lld, MIratio =%d \n",
			scenario_id, ratio, *readout_time,MIratio);
	return 0;
}


static u32 dgain2reg(struct subdrv_ctx *ctx, u32 dgain)
{
	return dgain * 64;
}

static int rothkoov50h40wide_set_multi_dig_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	u32 *gains  = (u32 *)(*feature_data);
	u16 exp_cnt = (u16) (*(feature_data + 1));

	int i = 0;
	u32 rg_gains[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

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
		rg_gains[1] = gains[1];
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
				(rg_gains[i] >> 16) & 0xFF);
		}
		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[1]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[1],
				(rg_gains[i] >> 8) & 0xFF);
		}
		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[2]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[2],
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

static int rothkoov50h40wide_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	struct SET_SENSOR_AWB_GAIN *awb_gain = (struct SET_SENSOR_AWB_GAIN *)feature_data;
	MUINT32 r_Gain = awb_gain->ABS_GAIN_R << 1;
	MUINT32 g_Gain = awb_gain->ABS_GAIN_GR << 1;
	MUINT32 b_Gain = awb_gain->ABS_GAIN_B << 1;
	u8 data = 0;

	g_last_awb_gain = *awb_gain;

	if (ctx->current_scenario_id != SENSOR_SCENARIO_ID_CUSTOM4) {
		DRV_LOG(ctx, "skip awb gain [r/g/b]: 0x%x 0x%x 0x%x\n",
			r_Gain, g_Gain, b_Gain);
		return 0;
	}

	if (r_Gain == 0 || g_Gain == 0 || b_Gain == 0) {
		DRV_LOG(ctx, "error awb gain [r/g/b]: 0x%x 0x%x 0x%x\n",
			r_Gain, g_Gain, b_Gain);
		return 0;
	}

	// en manual awb gain
	data = subdrv_i2c_rd_u8(ctx, 0x5001);
	data &= ~0x08; // bit[3] = 0
	subdrv_i2c_wr_u8(ctx, 0x5001, data); // Simple AWB enable

	data = subdrv_i2c_rd_u8(ctx, 0x58b5);
	data |= 0x01; // bit[0] = 1
	subdrv_i2c_wr_u8(ctx, 0x58b5, data); // man_qpdwb_gain_enable

	data = subdrv_i2c_rd_u8(ctx, 0x5ab0);
	data |= 0x02; // bit[1] = 1
	subdrv_i2c_wr_u8(ctx, 0x5ab0, data);

	// set awb gain
	subdrv_i2c_wr_u8(ctx, 0x58ac, (b_Gain >> 8) & 0x7F); //B Gain [14:8]
	subdrv_i2c_wr_u8(ctx, 0x58ad, (b_Gain >> 0) & 0xFF); //B Gain [7:0]
	subdrv_i2c_wr_u8(ctx, 0x58ae, (g_Gain >> 8) & 0x7F); //Gb Gain [14:8]
	subdrv_i2c_wr_u8(ctx, 0x58af, (g_Gain >> 0) & 0xFF); //Gb Gain [7:0]
	subdrv_i2c_wr_u8(ctx, 0x58b0, (g_Gain >> 8) & 0x7F); //Gr Gain [14:8]
	subdrv_i2c_wr_u8(ctx, 0x58b1, (g_Gain >> 0) & 0xFF); //Gr Gain [7:0]
	subdrv_i2c_wr_u8(ctx, 0x58b2, (r_Gain >> 8) & 0x7F); //R Gain [14:8]
	subdrv_i2c_wr_u8(ctx, 0x58b3, (r_Gain >> 0) & 0xFF); //R Gain [7:0]

	DRV_LOG(ctx, "awb gain [r/g/b]: 0x%x 0x%x 0x%x\n",
		r_Gain, g_Gain, b_Gain);

	return 0;
}

