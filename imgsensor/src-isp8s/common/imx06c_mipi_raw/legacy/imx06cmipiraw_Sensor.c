// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 imx06cmipiraw_Sensor.c
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
#include "imx06cmipiraw_Sensor.h"

#define IMX06C_EMBEDDED_DATA_EN 1

#define REG2GAIN_ROUNDUP(_reg) ((16384 * BASEGAIN + (16384 - (_reg) - 1))/ (16384 - (_reg)))
#define REG2GAIN_ROUNDDOWN(_reg) (16384 * BASEGAIN / (16384 - (_reg)))

static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static int imx06c_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int imx06c_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int imx06c_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int imx06c_cphy_lrte_mode(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt, u64 sof_ts);
static int imx06c_mcss_init(void *arg);
static int imx06c_mcss_set_mask_frame(struct subdrv_ctx *ctx, u32 num, u32 is_critical);
static int imx06c_mcss_update_subdrv_para(void *arg, int scenario_id);
static int imx06c_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void imx06c_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int imx06c_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int imx06c_get_linetime_in_ns(void *arg, u32 scenario_id, u32 *linetime_in_ns,
	enum GET_LINETIME_ENUM linetime_type, enum IMGSENSOR_EXPOSURE exp_idx);

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, imx06c_set_test_pattern},
	{SENSOR_FEATURE_SET_TEST_PATTERN_DATA, imx06c_set_test_pattern_data},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, imx06c_seamless_switch},
	{SENSOR_FEATURE_SET_CPHY_LRTE_MODE, imx06c_cphy_lrte_mode},
	{ SENSOR_FEATURE_SET_ESHUTTER, imx06c_set_shutter },
	{ SENSOR_FEATURE_SET_GAIN, imx06c_set_gain },
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		.header_id = 0x12163702,
		.addr_header_id = 0x0000000A,
		.i2c_write_id = 0xA0,
		.qsc_support = TRUE,
		.qsc_size = 3072,
		.addr_qsc = 0x2451,
		.sensor_reg_addr_qsc = 0xC000,
	},
};


#define pd_i4Crop { \
		/* pre cap normal_video hs_video(2binning) slim_video(2binning) */\
		{0, 0}, {0, 0}, {0, 256}, {128, 456}, {64, 228},\
		/* cust1(4k) cust2(4k) cust3 cust4(2binning) cust5(rmsc) */\
		{128, 456}, {128, 456}, {0, 384}, {864, 648}, {64, 228},\
		/* cust6 cust7(unused) cust8(rmsc) cust9(rmsc) cust10(unused) */\
		{384, 408}, {384, 408}, {0, 0}, {2048, 1536}, {2048, 1536},\
		/* cust11 cust12 cust13(8k) cust14(unused) cust15 */\
		{2048, 1536}, {0, 0}, {0, 0}, {0, 0}, {0, 256},\
		/* cust16(rmsc) cust17(rmsc) cust18 cust19 cust20(rmsc) */\
		{2048, 1792}, {0, 256}, {2048, 1792}, {0, 256}, {2048, 1792},\
		/* cust21   cust22(rmsc)  cust23   cust24      cust25*/\
		{2048, 1920}, {2048, 1792}, {0, 256}, {0, 256}, {0, 256},\
		/* cust26       cust27       cust28       cust29     cust30*/\
		{0, 256}, {2048, 1792}, {2048, 1792}, {0, 256}, {2048, 1792},\
		/* cust31      cust32       cust33       cust34      cust35*/\
		{0, 256}, {0, 256}, {2048, 1792}, {2048, 1792}, {0, 0},\
		/* cust36        cust37     cust38        cust39     cust40*/\
		{0, 0}, {2048, 1536}, {0, 0}, {2048, 1536}, {0, 0},\
		/* cust41 cust42 cust43 cust44 cust45 */\
		{2048, 1536}, {2048, 1536}, {0, 0}, {0, 0}, {0, 0},\
		/* cust46 cust47 cust48 cust49 cust50 */\
		{240, 180}, {240, 276}, {256, 912}, {256, 912}, {256, 912},\
		/* cust51 cust52 cust53 cust54 cust55 */\
		{240, 276}, {0, 0}, {0, 0}, {512, 384}, {0, 256},\
		/* cust56 cust57 cust58 cust59 cust60 */\
		{0, 256}, {0, 128}, {256, 912}\
}

/* fullsize <binning> <<2bining>> */
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_fullsize = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = pd_i4Crop,
	.iMirrorFlip = 3,
	.i4FullRawW = 8192,
	.i4FullRawH = 6144,
	.i4ModeIndex = 2,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		// .i4VCFeature = VC_PDAF_STATS_NE_PIX_1,
		.i4PDPattern = 1,
		.i4BinFacX = 4,
		.i4BinFacY = 2,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
};
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_fullsize_hbin = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = pd_i4Crop,
	.iMirrorFlip = 3,
	.i4FullRawW = 8192,
	.i4FullRawH = 6144,
	.i4ModeIndex = 2,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		// .i4VCFeature = VC_PDAF_STATS_NE_PIX_1,
		.i4PDPattern = 1,
		.i4BinFacX = 4,
		.i4BinFacY = 2,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
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
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = pd_i4Crop,
	.iMirrorFlip = 3,
	.i4FullRawW = 4096,
	.i4FullRawH = 3072,
	.i4ModeIndex = 2,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		// .i4VCFeature = VC_PDAF_STATS_NE_PIX_1,
		.i4PDPattern = 1,
		.i4BinFacX = 2,
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
};
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_2bin = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = pd_i4Crop,
	.iMirrorFlip = 3,
	.i4FullRawW = 2048,
	.i4FullRawH = 1536,
	.i4ModeIndex = 2,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		// .i4VCFeature = VC_PDAF_STATS_NE_PIX_1,
		.i4PDPattern = 1,
		.i4BinFacX = 2,
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
};
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_shdr = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = pd_i4Crop,
	.iMirrorFlip = 3,
	.i4FullRawW = 4096,
	.i4FullRawH = 3072,
	.i4ModeIndex = 2,
	.sPDMapInfo[0] = {
		.i4VCFeature = VC_PDAF_STATS_NE_PIX_1,
		.i4PDPattern = 1,
		.i4BinFacX = 2,
		.i4BinFacY = 4,
	},
	.sPDMapInfo[1] = {
		.i4VCFeature = VC_PDAF_STATS_ME_PIX_1,
		.i4PDPattern = 1,
		.i4BinFacX = 2,
		.i4BinFacY = 4,
	},
};
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_fullsize_shdr = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = pd_i4Crop,
	.iMirrorFlip = 3,
	.i4FullRawW = 8192,
	.i4FullRawH = 6144,
	.i4ModeIndex = 2,
	.sPDMapInfo[0] = {
		.i4VCFeature = VC_PDAF_STATS_NE_PIX_1,
		.i4PDPattern = 1,
		.i4BinFacX = 4,
		.i4BinFacY = 2,
	},
	.sPDMapInfo[1] = {
		.i4VCFeature = VC_PDAF_STATS_ME_PIX_1,
		.i4PDPattern = 1,
		.i4BinFacX = 4,
		.i4BinFacY = 2,
	},
};
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_partialpd = {
	.i4OffsetX = 16,
	.i4OffsetY = 32,
	.i4PitchX = 16,
	.i4PitchY = 16,
	.i4PairNum = 8,
	.i4SubBlkW = 8,
	.i4SubBlkH = 4,
	.i4PosL = {{18, 33}, {26, 33}, {22, 39}, {30, 39}, {17, 40},
		{25, 40}, {21, 46}, {29, 46}},
	.i4PosR = {{16, 35}, {24, 35}, {20, 37}, {28, 37}, {19, 42},
		{27, 42}, {23, 44}, {31, 44}},
	.i4BlockNumX = 254,
	.i4BlockNumY = 160,
	.i4LeFirst = 0,
	.i4Crop = pd_i4Crop,
	.iMirrorFlip = 3,
	.i4FullRawW = 4096,
	.i4FullRawH = 3072,
	.i4ModeIndex = 2,
	.PDAF_Support = PDAF_SUPPORT_CAMSV,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		// .i4VCFeature = VC_PDAF_STATS_NE_PIX_1,
		.i4PDPattern = 3,
		.i4BinFacX = 0,
		.i4BinFacY = 0,
		.i4PDRepetition = 8,
		.i4PDOrder = {1, 0, 0, 1, 1, 0, 0, 1},
	},
};

//1000 base for dcg gain ratio
static u32 imx06c_dcg_ratio_table_12bit[] = {4000};

static u32 imx06c_dcg_ratio_table_14bit[] = {16000};

static u32 imx06c_dcg_ratio_table_16bit[] = {16000};

static u32 imx06c_dcg_ratio_table_ratio4[] = {4000};

static struct mtk_sensor_saturation_info imgsensor_saturation_info = {
	.OB_pedestal = 64,
};

//preview_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_preview[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//capture_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_capture[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//normal_video_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_normal_video[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//hs_video_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_hs_video[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0xf00,
			.vsize = 0x870,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0xf00,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//slim_video_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_slim_video[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x780,
			.vsize = 0x438,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x780,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom1_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x280,
			.vsize = 0x1e0,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x280,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom2_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x280,
			.vsize = 0x1e0,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x280,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom3_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x140,
			.vsize = 0xf0,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x140,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom4_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x140,
			.vsize = 0xf0,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x140,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom5_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x780,
			.vsize = 0x438,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x780,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom6_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom6[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x500,
			.vsize = 0x2d0,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x500,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom7_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom7[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x500,
			.vsize = 0x2d0,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x500,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom8_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom8[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom9_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom9[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x600,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom10_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom10[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x600,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom11_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom11[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x600,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x600,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom12_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom12[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x2000,
			.vsize = 0x1800,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x2000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom13_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom13[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x2000,
			.vsize = 0x1800,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x2000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom14_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom14[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x800,
			.vsize = 0x600,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x180,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x800,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom15_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom15[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom16_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom16[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x500,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x500,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom17_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom17[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom18_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom18[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x500,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x500,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom19_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom19[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom20_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom20[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x500,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom21_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom21[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x900,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x900,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x480,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom22_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom22[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x500,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom23_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom23[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW12,
		},
	},
#endif
};

//custom24_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom24[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW14,
		},
	},
#endif
};

//custom25_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom25[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW12,
		},
	},
#endif
};

//custom26_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom26[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom27_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom27[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x500,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom28_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom28[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x500,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom29_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom29[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom30_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom30[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x500,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom31_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom31[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom32_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom32[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom33_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom33[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x500,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom34_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom34[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x500,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom35_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom35[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom36_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom36[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom37_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom37[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x600,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x600,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom38_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom38[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0c00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom39_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom39[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x600,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x600,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom40_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom40[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom41_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom41[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x600,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom42_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom42[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x600,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom43_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom43[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW12,
		},
	},
#endif
};

//custom44_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom44[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW14,
		},
	},
#endif
};

//custom45_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom45[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xc00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x300,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom46_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom46[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x620,
			.vsize = 0x498,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x620,
			.vsize = 0x126,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x620,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom47_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom47[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x620,
			.vsize = 0x3d8,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x620,
			.vsize = 0xf6,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x620,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom48_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom48[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1e00,
			.vsize = 0x10e0,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0xf00,
			.vsize = 0x870,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1e00,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom49_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom49[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1e00,
			.vsize = 0x10e0,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0xf00,
			.vsize = 0x870,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1e00,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom50_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom50[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1e00,
			.vsize = 0x10e0,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0xf00,
			.vsize = 0x870,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1e00,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom51_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom51[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x620,
			.vsize = 0x3d8,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x620,
			.vsize = 0xf6,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x620,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom52_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom52[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x780,
			.vsize = 0x438,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x780,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom53_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom53[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x780,
			.vsize = 0x438,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x780,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom54_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom54[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x400,
			.vsize = 0x300,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x400,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom55_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom55[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1FC,
			.vsize = 0x500,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW12,
		},
	},
#endif
};

//custom56_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom56[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};


//custom57_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom57[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x800,
			.vsize = 0x500,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x140,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x800,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom58_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom58[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1e00,
			.vsize = 0x10e0,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0xf00,
			.vsize = 0x870,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1e00,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW10,
		},
	},
#endif
};

//custom59_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom59[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW12,
		},
	},
#endif
};

//custom60_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom60[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x280,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW14,
		},
	},
#endif
};

//custom61_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom61[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x500,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW12,
		},
	},
#endif
};

//custom62_frame_desc
static struct mtk_mbus_frame_desc_entry frame_desc_custom62[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 0x1000,
			.vsize = 0xa00,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x800,
			.vsize = 0x500,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
#if IMX06C_EMBEDDED_DATA_EN
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x12,
			.hsize = 0x1000,
			.vsize = 0x2,
			.user_data_desc = VC_GENERAL_EMBEDDED,
			.ebd_parsing_type = MTK_EBD_PARSING_TYPE_MIPI_RAW14,
		},
	},
#endif
};

static struct subdrv_mode_struct mode_struct[] = {
	{
		.frame_desc = frame_desc_preview,
		.num_entries = ARRAY_SIZE(frame_desc_preview),
		.mode_setting_table = imx06c_preview_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_preview_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_preview,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_preview),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 12900,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
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
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_capture,
		.num_entries = ARRAY_SIZE(frame_desc_capture),
		.mode_setting_table = imx06c_capture_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_capture_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_capture,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_capture),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 12900,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
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
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_normal_video,
		.num_entries = ARRAY_SIZE(frame_desc_normal_video),
		.mode_setting_table = imx06c_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_normal_video_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_normal_video,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_normal_video),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 12900,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_hs_video,
		.num_entries = ARRAY_SIZE(frame_desc_hs_video),
		.mode_setting_table = imx06c_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_hs_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 3224,
		.max_framerate = 1200,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 896,
			.w0_size = 8192,
			.h0_size = 4352,
			.scale_w = 4096,
			.scale_h = 2176,
			.x1_offset = 128,
			.y1_offset = 8,
			.w1_size = 3840,
			.h1_size = 2160,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3840,
			.h2_tg_size = 2160,
		},
		.pdaf_cap = FALSE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info= PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_slim_video,
		.num_entries = ARRAY_SIZE(frame_desc_slim_video),
		.mode_setting_table = imx06c_slim_video_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_slim_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 6160,
		.framelength = 1960,
		.max_framerate = 2400,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 896,
			.w0_size = 8192,
			.h0_size = 4352,
			.scale_w = 2048,
			.scale_h = 1088,
			.x1_offset = 64,
			.y1_offset = 4,
			.w1_size = 1920,
			.h1_size = 1080,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1920,
			.h2_tg_size = 1080,
		},
		.pdaf_cap = FALSE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info= PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom1,
		.num_entries = ARRAY_SIZE(frame_desc_custom1),
		.mode_setting_table = imx06c_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom1_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2166670000,
		.linelength = 9520,
		.framelength = 22608,
		.max_framerate = 100,
		.mipi_pixel_rate = 1368340000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 2112,
			.w0_size = 8192,
			.h0_size = 1920,
			.scale_w = 2048,
			.scale_h = 480,
			.x1_offset = 704,
			.y1_offset = 0,
			.w1_size = 640,
			.h1_size = 480,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 640,
			.h2_tg_size = 480,
		},
		.aov_mode = 1,
		.s_dummy_support = 0,
		.ae_ctrl_support = IMGSENSOR_AE_CONTROL_SUPPORT_SENSING_MODE,
		.pdaf_cap = FALSE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info= PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom2,
		.num_entries = ARRAY_SIZE(frame_desc_custom2),
		.mode_setting_table = imx06c_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom2_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2166670000,
		.linelength = 9520,
		.framelength = 45216,
		.max_framerate = 50,
		.mipi_pixel_rate = 1368340000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 2112,
			.w0_size = 8192,
			.h0_size = 1920,
			.scale_w = 2048,
			.scale_h = 480,
			.x1_offset = 704,
			.y1_offset = 0,
			.w1_size = 640,
			.h1_size = 480,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 640,
			.h2_tg_size = 480,
		},
		.aov_mode = 1,
		.s_dummy_support = 0,
		.ae_ctrl_support = IMGSENSOR_AE_CONTROL_SUPPORT_SENSING_MODE,
		.pdaf_cap = FALSE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info= PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom3,
		.num_entries = ARRAY_SIZE(frame_desc_custom3),
		.mode_setting_table = imx06c_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom3_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2912000000,
		.linelength = 9520,
		.framelength = 30232,
		.max_framerate = 100,
		.mipi_pixel_rate = 1368340000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 2592,
			.w0_size = 8192,
			.h0_size = 960,
			.scale_w = 2048,
			.scale_h = 240,
			.x1_offset = 864,
			.y1_offset = 0,
			.w1_size = 320,
			.h1_size = 240,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 320,
			.h2_tg_size = 240,
		},
		.aov_mode = 1,
		.s_dummy_support = 0,
		.ae_ctrl_support = IMGSENSOR_AE_CONTROL_SUPPORT_SENSING_MODE,
		.pdaf_cap = FALSE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info= PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom4,
		.num_entries = ARRAY_SIZE(frame_desc_custom4),
		.mode_setting_table = imx06c_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom4_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2912000000,
		.linelength = 9520,
		.framelength = 60464,
		.max_framerate = 50,
		.mipi_pixel_rate = 1368340000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 2592,
			.w0_size = 8192,
			.h0_size = 960,
			.scale_w = 2048,
			.scale_h = 240,
			.x1_offset = 864,
			.y1_offset = 0,
			.w1_size = 320,
			.h1_size = 240,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 320,
			.h2_tg_size = 240,
		},
		.aov_mode = 1,
		.s_dummy_support = 0,
		.ae_ctrl_support = IMGSENSOR_AE_CONTROL_SUPPORT_SENSING_MODE,
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_2bin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom5,
		.num_entries = ARRAY_SIZE(frame_desc_custom5),
		.mode_setting_table = imx06c_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom5_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 4352,
		.framelength = 1388,
		.max_framerate = 4803,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 896,
			.w0_size = 8192,
			.h0_size = 4352,
			.scale_w = 2048,
			.scale_h = 1088,
			.x1_offset = 64,
			.y1_offset = 4,
			.w1_size = 1920,
			.h1_size = 1080,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1920,
			.h2_tg_size = 1080,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_2bin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom6,
		.num_entries = ARRAY_SIZE(frame_desc_custom6),
		.mode_setting_table = imx06c_custom6_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom6_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 4352,
		.framelength = 1340,
		.max_framerate = 4975,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 1632,
			.w0_size = 8192,
			.h0_size = 2880,
			.scale_w = 2048,
			.scale_h = 720,
			.x1_offset = 384,
			.y1_offset = 0,
			.w1_size = 1280,
			.h1_size = 720,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1280,
			.h2_tg_size = 720,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_2bin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom7,
		.num_entries = ARRAY_SIZE(frame_desc_custom7),
		.mode_setting_table = imx06c_custom7_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom7_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2848000000,
		.linelength = 4352,
		.framelength = 1348,
		.max_framerate = 4811,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 1632,
			.w0_size = 8192,
			.h0_size = 2880,
			.scale_w = 2048,
			.scale_h = 720,
			.x1_offset = 384,
			.y1_offset = 0,
			.w1_size = 1280,
			.h1_size = 720,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1280,
			.h2_tg_size = 720,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_2bin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom8,
		.num_entries = ARRAY_SIZE(frame_desc_custom8),
		.mode_setting_table = imx06c_custom8_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom8_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom8,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom8),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 12900,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
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
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom9,
		.num_entries = ARRAY_SIZE(frame_desc_custom9),
		.mode_setting_table = imx06c_custom9_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom9_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom9,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom9),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 9424,
		.framelength = 10256,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
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
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize_hbin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom10,
		.num_entries = ARRAY_SIZE(frame_desc_custom10),
		.mode_setting_table = imx06c_custom10_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom10_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom10,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom10),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 9424,
		.framelength = 10256,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
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
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize_hbin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom11,
		.num_entries = ARRAY_SIZE(frame_desc_custom11),
		.mode_setting_table = imx06c_custom11_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom11_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom11,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom11),
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 9424,
		.framelength = 5128 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 3216,/* min_fll:3220, min_fll - read_margin */
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
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
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize_shdr,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFFC,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 0xFFFC,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(15872),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(15872),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom12,
		.num_entries = ARRAY_SIZE(frame_desc_custom12),
		.mode_setting_table = imx06c_custom12_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom12_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom12,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom12),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 11632,
		.framelength = 8332,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
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
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),//64x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom13,
		.num_entries = ARRAY_SIZE(frame_desc_custom13),
		.mode_setting_table = imx06c_custom13_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom13_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom13,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom13),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 11632,
		.framelength = 8332,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
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
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),//64x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom14,
		.num_entries = ARRAY_SIZE(frame_desc_custom14),
		.mode_setting_table = imx06c_custom14_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom14_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom14,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom14),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 9520,
		.framelength = 10124,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
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
			.scale_w = 2048,
			.scale_h = 1536,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2048,
			.h1_size = 1536,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2048,
			.h2_tg_size = 1536,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_2bin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom15,
		.num_entries = ARRAY_SIZE(frame_desc_custom15),
		.mode_setting_table = imx06c_custom15_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom15_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom15,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom15),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 6448 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 3132,/* 3136 - read_margin */
		/* SRM : ((16 + (80 + (Y_END - Y_STA + 1 + 16))/2) - read_margin */
		.read_margin = 4,
		.framelength_step = 4*2,
		.coarse_integ_step = 4*2,
		.min_exposure_line = 4*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFF8*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 0xFFF8*2,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom16,
		.num_entries = ARRAY_SIZE(frame_desc_custom16),
		.mode_setting_table = imx06c_custom16_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom16_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom16,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom16),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 9424,
		.framelength = 5128 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 3132,/* 3136 - read_margin */
		/* SRM : ((16 + (80 + (Y_END - Y_STA + 1 + 16))/2) - read_margin */
		.read_margin = 4,
		.framelength_step = 4*2,
		.coarse_integ_step = 4*2,
		.min_exposure_line = 4*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFF8*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 0xFFF8*2,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1792,
			.w0_size = 4096,
			.h0_size = 2560,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize_hbin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom17,
		.num_entries = ARRAY_SIZE(frame_desc_custom17),
		.mode_setting_table = imx06c_custom17_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom17_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom17,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom17),
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 6448 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 2644,/* min_fll:2648, min_fll - read_margin */
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_shdr,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFFC,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 0xFFFC,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4916),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16292),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom18,
		.num_entries = ARRAY_SIZE(frame_desc_custom18),
		.mode_setting_table = imx06c_custom18_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom18_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom18,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom18),
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 9424,
		.framelength = 5128 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 2704,/* min_fll:2708, min_fll - read_margin */
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1792,
			.w0_size = 4096,
			.h0_size = 2560,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize_shdr,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFFC,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 0xFFFC,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(15872),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(15872),
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom19,
		.num_entries = ARRAY_SIZE(frame_desc_custom19),
		.mode_setting_table = imx06c_custom19_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom19_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom19,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom19),
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 14560,
		.framelength = 6664,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_ratio4),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(13520),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16016),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom20,
		.num_entries = ARRAY_SIZE(frame_desc_custom20),
		.mode_setting_table = imx06c_custom20_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom20_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom20,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom20),
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 18672,
		.framelength = 5194,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1792,
			.w0_size = 4096,
			.h0_size = 2560,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_ratio4),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(13520),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16016),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom21,
		.num_entries = ARRAY_SIZE(frame_desc_custom21),
		.mode_setting_table = imx06c_custom21_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom21_setting),
		.seamless_switch_group = 3,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom21,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom21),
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2864000000,
		.linelength = 18672,
		.framelength = 2546,
		.max_framerate = 600,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1920,
			.w0_size = 4096,
			.h0_size = 2304,
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
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize_hbin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_ratio4),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(13520),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16016),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom22,
		.num_entries = ARRAY_SIZE(frame_desc_custom22),
		.mode_setting_table = imx06c_custom22_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom22_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom22,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom22),
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 18672,
		.framelength = 5194,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1792,
			.w0_size = 4096,
			.h0_size = 2560,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize_hbin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_ratio4),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(13520),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16016),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom23,
		.num_entries = ARRAY_SIZE(frame_desc_custom23),
		.mode_setting_table = imx06c_custom23_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom23_setting),
		.seamless_switch_group = 3,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom23,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom23),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2864000000,
		.linelength = 12304,
		.framelength = 3844,
		.max_framerate = 600,
		.mipi_pixel_rate = 2564570000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_B,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.saturation_info = &imgsensor_saturation_info,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_12bit,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_12bit),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(13520),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16016),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom24,
		.num_entries = ARRAY_SIZE(frame_desc_custom24),
		.mode_setting_table = imx06c_custom24_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom24_setting),
		.seamless_switch_group = 3,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom24,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom24),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2864000000,
		.linelength = 12304,
		.framelength = 3844,
		.max_framerate = 600,
		.mipi_pixel_rate = 2198200000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_B,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.saturation_info = &imgsensor_saturation_info,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_14bit,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_14bit),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(15660),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4800),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(14912),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom25,
		.num_entries = ARRAY_SIZE(frame_desc_custom25),
		.mode_setting_table = imx06c_custom25_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom25_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		// .seamless_switch_group = 2,
		// .seamless_switch_mode_setting_table = imx06c_seamless_custom25,
		// .seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom25),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 3,
		.pclk = 2928000000,
		.linelength = 15696,
		.framelength = 6200,
		.max_framerate = 300,
		.mipi_pixel_rate = 1923430000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.saturation_info = &imgsensor_saturation_info,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_16bit,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_16bit),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(13520),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16016),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom26,
		.num_entries = ARRAY_SIZE(frame_desc_custom26),
		.mode_setting_table = imx06c_custom26_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom26_setting),
		.seamless_switch_group = 3,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom26,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom26),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2864000000,
		.linelength = 15040,
		.framelength = 3148,
		.max_framerate = 600,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom27,
		.num_entries = ARRAY_SIZE(frame_desc_custom27),
		.mode_setting_table = imx06c_custom27_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom27_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom27,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom27),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 9424,
		.framelength = 10256,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1792,
			.w0_size = 4096,
			.h0_size = 2560,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize_hbin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),//64x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom28,
		.num_entries = ARRAY_SIZE(frame_desc_custom28),
		.mode_setting_table = imx06c_custom28_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom28_setting),
		.seamless_switch_group = 3,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom28,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom28),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2864000000,
		.linelength = 9424,
		.framelength = 5000,
		.max_framerate = 600,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1792,
			.w0_size = 4096,
			.h0_size = 2560,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),//64x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom29,
		.num_entries = ARRAY_SIZE(frame_desc_custom29),
		.mode_setting_table = imx06c_custom29_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom29_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom29,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom29),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 12900,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom30,
		.num_entries = ARRAY_SIZE(frame_desc_custom30),
		.mode_setting_table = imx06c_custom30_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom30_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom30,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom30),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 9424,
		.framelength = 10256,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1792,
			.w0_size = 4096,
			.h0_size = 2560,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),//64x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom31,
		.num_entries = ARRAY_SIZE(frame_desc_custom31),
		.mode_setting_table = imx06c_custom31_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom31_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2784000000,
		.linelength = 7520,
		.framelength = 6060,
		.max_framerate = 600,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom32,
		.num_entries = ARRAY_SIZE(frame_desc_custom32),
		.mode_setting_table = imx06c_custom32_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom32_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 6448,
		.max_framerate = 600,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom33,
		.num_entries = ARRAY_SIZE(frame_desc_custom33),
		.mode_setting_table = imx06c_custom33_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom33_setting),
		.seamless_switch_group = 3,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom33,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom33),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2864000000,
		.linelength = 9424,
		.framelength = 5000,
		.max_framerate = 600,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1792,
			.w0_size = 4096,
			.h0_size = 2560,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = FALSE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.imgsensor_pd_info= &imgsensor_pd_info_fullsize,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),//64x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom34,
		.num_entries = ARRAY_SIZE(frame_desc_custom34),
		.mode_setting_table = imx06c_custom34_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom34_setting),
		.seamless_switch_group = 3,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom34,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom34),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2864000000,
		.linelength = 9424,
		.framelength = 5000,
		.max_framerate = 600,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1792,
			.w0_size = 4096,
			.h0_size = 2560,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),//64x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom35,
		.num_entries = ARRAY_SIZE(frame_desc_custom35),
		.mode_setting_table = imx06c_custom35_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom35_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom35,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom35),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 6448,
		.max_framerate = 600,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 4,
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
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom36,
		.num_entries = ARRAY_SIZE(frame_desc_custom36),
		.mode_setting_table = imx06c_custom36_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom36_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom36,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom36),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 6448 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 3132,/* 3136 - read_margin */
		/* SRM : ((16 + (80 + (Y_END - Y_STA + 1 + 16))/2) - read_margin */
		.read_margin = 4,
		.framelength_step = 4*2,
		.coarse_integ_step = 4*2,
		.min_exposure_line = 4*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFF8*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 0xFFF8*2,
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
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom37,
		.num_entries = ARRAY_SIZE(frame_desc_custom37),
		.mode_setting_table = imx06c_custom37_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom37_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom37,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom37),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 9424,
		.framelength = 5128 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 3132,/* 3136 - read_margin */
		/* SRM : ((16 + (80 + (Y_END - Y_STA + 1 + 16))/2) - read_margin */
		.read_margin = 4,
		.framelength_step = 4*2,
		.coarse_integ_step = 4*2,
		.min_exposure_line = 6*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFF8*2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 0xFFF8*2,
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
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize_hbin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),//64x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16128),//64x
		.dpc_enabled = true,
	},
		{
		.frame_desc = frame_desc_custom38,
		.num_entries = ARRAY_SIZE(frame_desc_custom38),
		.mode_setting_table = imx06c_custom38_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom38_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom38,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom38),
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 7520,
		.framelength = 6448 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 3156,/* min_fll:3160, min_fll - read_margin */
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
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
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_shdr,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFFC,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 0xFFFC,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4916),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16292),
		.dpc_enabled = true,
	},
		{
		.frame_desc = frame_desc_custom39,
		.num_entries = ARRAY_SIZE(frame_desc_custom39),
		.mode_setting_table = imx06c_custom39_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom39_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom39,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom39),
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 9424,
		.framelength = 5128 * 2,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 3216,/* min_fll:3220, min_fll - read_margin */
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
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
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize_shdr,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFFC,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 0xFFFC,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(15872),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(15872),
		.dpc_enabled = true,
	},
		{
		.frame_desc = frame_desc_custom40,
		.num_entries = ARRAY_SIZE(frame_desc_custom40),
		.mode_setting_table = imx06c_custom40_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom40_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom40,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom40),
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 14560,
		.framelength = 6664,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4,
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
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_ratio4),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(13520),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16016),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom41,
		.num_entries = ARRAY_SIZE(frame_desc_custom41),
		.mode_setting_table = imx06c_custom41_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom41_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom41,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom41),
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 18672,
		.framelength = 5194,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
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
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize_hbin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_ratio4),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(13520),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16016),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom42,
		.num_entries = ARRAY_SIZE(frame_desc_custom42),
		.mode_setting_table = imx06c_custom42_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom42_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom42,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom42),
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 18672,
		.framelength = 5194,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 6,
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
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_ratio4),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(13520),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16016),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom43,
		.num_entries = ARRAY_SIZE(frame_desc_custom43),
		.mode_setting_table = imx06c_custom43_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom43_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom43,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom43),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 12304,
		.framelength = 7840,
		.max_framerate = 300,
		.mipi_pixel_rate = 2564570000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4,
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
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_B,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.saturation_info = &imgsensor_saturation_info,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_ratio4),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(13520),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16016),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom44,
		.num_entries = ARRAY_SIZE(frame_desc_custom44),
		.mode_setting_table = imx06c_custom44_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom44_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom44,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom44),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 12304,
		.framelength = 7840,
		.max_framerate = 300,
		.mipi_pixel_rate = 2198200000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 12,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 12,
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
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_B,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_14bit,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_14bit),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(15660),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4800),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(14912),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom45,
		.num_entries = ARRAY_SIZE(frame_desc_custom45),
		.mode_setting_table = imx06c_custom45_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom45_setting),
		// .seamless_switch_group = 1,
		// .seamless_switch_mode_setting_table = imx06c_seamless_custom45,
		// .seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom45),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 15696,
		.framelength = 6200,
		.max_framerate = 300,
		.mipi_pixel_rate = 1923430000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
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
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info= PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom46,
		.num_entries = ARRAY_SIZE(frame_desc_custom46),
		.mode_setting_table = imx06c_custom46_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom46_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom46,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom46),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 9520,
		.framelength = 10124,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 704,
			.w0_size = 8192,
			.h0_size = 4736,
			.scale_w = 2048,
			.scale_h = 1184,
			.x1_offset = 240,
			.y1_offset = 4,
			.w1_size = 1568,
			.h1_size = 1176,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1568,
			.h2_tg_size = 1176,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_2bin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom47,
		.num_entries = ARRAY_SIZE(frame_desc_custom47),
		.mode_setting_table = imx06c_custom47_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom47_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 9520,
		.framelength = 10124,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 1088,
			.w0_size = 8192,
			.h0_size = 3968,
			.scale_w = 2048,
			.scale_h = 992,
			.x1_offset = 240,
			.y1_offset = 4,
			.w1_size = 1568,
			.h1_size = 984,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1568,
			.h2_tg_size = 984,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_2bin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom48,
		.num_entries = ARRAY_SIZE(frame_desc_custom48),
		.mode_setting_table = imx06c_custom48_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom48_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2816000000,
		.linelength = 11632,
		.framelength = 8000,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 896,
			.w0_size = 8192,
			.h0_size = 4352,
			.scale_w = 8192,
			.scale_h = 4352,
			.x1_offset = 256,
			.y1_offset = 16,
			.w1_size = 7680,
			.h1_size = 4320,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 7680,
			.h2_tg_size = 4320,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),//64x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom49,
		.num_entries = ARRAY_SIZE(frame_desc_custom49),
		.mode_setting_table = imx06c_custom49_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom49_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 11632,
		.framelength = 4524,
		.max_framerate = 552,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 896,
			.w0_size = 8192,
			.h0_size = 4352,
			.scale_w = 8192,
			.scale_h = 4352,
			.x1_offset = 256,
			.y1_offset = 16,
			.w1_size = 7680,
			.h1_size = 4320,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 7680,
			.h2_tg_size = 4320,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),//64x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom50,
		.num_entries = ARRAY_SIZE(frame_desc_custom50),
		.mode_setting_table = imx06c_custom50_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom50_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1952000000,
		.linelength = 11632,
		.framelength = 5554,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 896,
			.w0_size = 8192,
			.h0_size = 4352,
			.scale_w = 8192,
			.scale_h = 4352,
			.x1_offset = 256,
			.y1_offset = 16,
			.w1_size = 7680,
			.h1_size = 4320,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 7680,
			.h2_tg_size = 4320,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),//64x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom51,
		.num_entries = ARRAY_SIZE(frame_desc_custom51),
		.mode_setting_table = imx06c_custom51_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom51_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 9520,
		.framelength = 5060,
		.max_framerate = 600,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 1088,
			.w0_size = 8192,
			.h0_size = 3968,
			.scale_w = 2048,
			.scale_h = 992,
			.x1_offset = 240,
			.y1_offset = 4,
			.w1_size = 1568,
			.h1_size = 984,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1568,
			.h2_tg_size = 984,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_2bin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom52,
		.num_entries = ARRAY_SIZE(frame_desc_custom52),
		.mode_setting_table = imx06c_custom52_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom52_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 736670000,
		.linelength = 9520,
		.framelength = 3844,
		.max_framerate = 200,
		.mipi_pixel_rate = 1368340000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFFC - 64,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 896,
			.w0_size = 8192,
			.h0_size = 4352,
			.scale_w = 2048,
			.scale_h = 1088,
			.x1_offset = 64,
			.y1_offset = 4,
			.w1_size = 1920,
			.h1_size = 1080,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1920,
			.h2_tg_size = 1080,
		},
		.aov_mode = 1,
		.s_dummy_support = 0,
		.ae_ctrl_support = IMGSENSOR_AE_CONTROL_SUPPORT_SENSING_MODE,
		.pdaf_cap = FALSE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info= PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom53,
		.num_entries = ARRAY_SIZE(frame_desc_custom53),
		.mode_setting_table = imx06c_custom53_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom53_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 736670000,
		.linelength = 9520,
		.framelength = 2564,
		.max_framerate = 300,
		.mipi_pixel_rate = 1368340000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFFC - 64,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 896,
			.w0_size = 8192,
			.h0_size = 4352,
			.scale_w = 2048,
			.scale_h = 1088,
			.x1_offset = 64,
			.y1_offset = 4,
			.w1_size = 1920,
			.h1_size = 1080,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1920,
			.h2_tg_size = 1080,
		},
		.aov_mode = 1,
		.s_dummy_support = 0,
		.ae_ctrl_support = IMGSENSOR_AE_CONTROL_SUPPORT_SENSING_MODE,
		.pdaf_cap = FALSE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info= PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom54,
		.num_entries = ARRAY_SIZE(frame_desc_custom54),
		.mode_setting_table = imx06c_custom54_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom54_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom54,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom54),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 6160,
		.framelength = 3920,
		.max_framerate = 1200,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 8192,
			.h0_size = 3072,
			.scale_w = 2048,
			.scale_h = 768,
			.x1_offset = 512,
			.y1_offset = 0,
			.w1_size = 1024,
			.h1_size = 768,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1024,
			.h2_tg_size = 768,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_2bin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom55,
		.num_entries = ARRAY_SIZE(frame_desc_custom55),
		.mode_setting_table = imx06c_custom55_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom55_setting),
		.seamless_switch_group = 3,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom55,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom55),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 9744,
		.framelength = 4936,
		.max_framerate = 600,
		.mipi_pixel_rate = 2564570000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_B,
		.imgsensor_pd_info  = &imgsensor_pd_info_partialpd,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.saturation_info = &imgsensor_saturation_info,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_ratio4),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(13520),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16016),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom56,
		.num_entries = ARRAY_SIZE(frame_desc_custom56),
		.mode_setting_table = imx06c_custom56_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom56_setting),
		.seamless_switch_group = 3,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom56,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom56),
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 12304,
		.framelength = 3920,
		.max_framerate = 600,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_ratio4,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_ratio4),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(13520),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16016),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom57,
		.num_entries = ARRAY_SIZE(frame_desc_custom57),
		.mode_setting_table = imx06c_custom57_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom57_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom57,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom57),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 9520,
		.framelength = 10124,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 8,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 2048,
			.scale_h = 1280,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2048,
			.h1_size = 1280,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2048,
			.h2_tg_size = 1280,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_2bin,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.dphy_init_deskew_support = 0,
				},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(4916),//1.43x
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16320),//256x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom58,
		.num_entries = ARRAY_SIZE(frame_desc_custom58),
		.mode_setting_table = imx06c_custom58_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom58_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2928000000,
		.linelength = 11632,
		.framelength = 8332,
		.max_framerate = 300,
		.mipi_pixel_rate = 3077490000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 6,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 896,
			.w0_size = 8192,
			.h0_size = 4352,
			.scale_w = 8192,
			.scale_h = 4352,
			.x1_offset = 256,
			.y1_offset = 16,
			.w1_size = 7680,
			.h1_size = 4320,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 7680,
			.h2_tg_size = 4320,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.imgsensor_pd_info  = &imgsensor_pd_info_fullsize,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.dphy_init_deskew_support = 0,
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(0),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16128),//64x
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom59,
		.num_entries = ARRAY_SIZE(frame_desc_custom59),
		.mode_setting_table = imx06c_custom59_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom59_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom59,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom59),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 14560,
		.framelength = 6664,
		.max_framerate = 300,
		.mipi_pixel_rate = 2564570000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_B,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.saturation_info = &imgsensor_saturation_info,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_12bit,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_12bit),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(13520),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16016),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom60,
		.num_entries = ARRAY_SIZE(frame_desc_custom60),
		.mode_setting_table = imx06c_custom60_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom60_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom60,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom60),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 14560,
		.framelength = 6664,
		.max_framerate = 300,
		.mipi_pixel_rate = 2198200000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 512,
			.w0_size = 8192,
			.h0_size = 5120,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_B,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.saturation_info = &imgsensor_saturation_info,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_14bit,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_14bit),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(15660),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4800),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(14912),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom61,
		.num_entries = ARRAY_SIZE(frame_desc_custom61),
		.mode_setting_table = imx06c_custom61_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom61_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom61,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom61),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 18672,
		.framelength = 5194,
		.max_framerate = 300,
		.mipi_pixel_rate = 2564570000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1792,
			.w0_size = 4096,
			.h0_size = 2560,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_B,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.saturation_info = &imgsensor_saturation_info,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_12bit,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_12bit),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(13520),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4928),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(16016),
		.dpc_enabled = true,
	},
	{
		.frame_desc = frame_desc_custom62,
		.num_entries = ARRAY_SIZE(frame_desc_custom62),
		.mode_setting_table = imx06c_custom62_setting,
		.mode_setting_len = ARRAY_SIZE(imx06c_custom62_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx06c_seamless_custom62,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06c_seamless_custom62),
		.hdr_mode = HDR_RAW_DCG_COMPOSE,
		.raw_cnt = 1,
		.exp_cnt = 2,
		.pclk = 2928000000,
		.linelength = 18672,
		.framelength = 5194,
		.max_framerate = 300,
		.mipi_pixel_rate = 2198200000,
		.readout_length = 0,
		.read_margin = 4,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 2048,
			.y0_offset = 1792,
			.w0_size = 4096,
			.h0_size = 2560,
			.scale_w = 4096,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2560,
		},
		.pdaf_cap = TRUE,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_B,
		.imgsensor_pd_info  = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_lrte_support = 0,
		},
		.saturation_info = &imgsensor_saturation_info,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06c_dcg_ratio_table_14bit,
			.dcg_gain_table_size = sizeof(imx06c_dcg_ratio_table_14bit),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = REG2GAIN_ROUNDUP(15660),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = REG2GAIN_ROUNDDOWN(16292),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = REG2GAIN_ROUNDUP(4800),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = REG2GAIN_ROUNDDOWN(14912),
		.dpc_enabled = true,
	},
};

#ifdef IMX06C_ISF_DBG
	struct subdrv_static_ctx imx06c_legacy_s_ctx = {
#else
	static struct subdrv_static_ctx static_ctx = {
#endif
	.sensor_id = IMX06C_SENSOR_ID,
	.reg_addr_sensor_id = {0x0016, 0x0017},
	.i2c_addr_table = {0x34, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.mirror = IMAGE_HV_MIRROR,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 256,
	.ana_gain_type = 0,
	.ana_gain_step = 1,
	.ana_gain_table = imx06c_ana_gain_table,
	.ana_gain_table_size = sizeof(imx06c_ana_gain_table),
	.tuning_iso_base = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 6,
	.exposure_max = (0xFFFC - 64) << 8,
	.exposure_step = 4,
	.exposure_margin = 64,
	.dig_gain_min = BASE_DGAIN * 1,
	.dig_gain_max = BASE_DGAIN * 15.85,
	.dig_gain_step = 4,

	.frame_length_max = 0xFFFC,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 5693360,

	.pdaf_type = PDAF_SUPPORT_CAMSV_QPD,
	.hdr_type = HDR_SUPPORT_STAGGER_FDOL|HDR_SUPPORT_DCG|HDR_SUPPORT_LBMF,
	.saturation_info = &imgsensor_saturation_info,
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
			{0x3162, 0x3163},
			{0x0224, 0x0225},
	},
	.reg_addr_exposure_in_lut = {
			{0x0E20, 0x0E21},
			{0x0E60, 0x0E61},
			{0x0EA0, 0x0EA1},
	},
	.long_exposure_support = TRUE,
	.reg_addr_exposure_lshift = 0x3160,
	.reg_addr_ana_gain = {
			{0x0204, 0x0205},
			{0x3164, 0x3165},
			{0x0216, 0x0217},
	},
	.reg_addr_ana_gain_in_lut = {
			{0x0E22, 0x0E23},
			{0x0E62, 0x0E63},
			{0x0EA2, 0x0EA3},
	},
	.reg_addr_dig_gain = {
			{0x020E, 0x020F},
			{0x3166, 0x3167},
			{0x0218, 0x0219},
	},
	.reg_addr_dig_gain_in_lut = {
			{0x0E24, 0x0E25},
			{0x0E64, 0x0E65},
			{0x0EA4, 0x0EA5},
	},
	.reg_addr_dcg_ratio = 0x3172,
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_frame_length_in_lut = {
			{0x0E28, 0x0E29},
			{0x0E68, 0x0E69},
			{0x0EA8, 0x0EA9},
	},
	//.reg_addr_wb_gain = {
	//.gr = 0x0B8E,
	//.r = 0x0B90,
	//.b = 0x0B92,
	//.gb = 0x0B94,
	//},
	.reg_addr_temp_en = 0x0138,
	.reg_addr_temp_read = 0x013A,
	.reg_addr_auto_extend = 0x0350,
	.reg_addr_frame_count = 0x0005,
	.reg_addr_fast_mode = 0x3010,
	.reg_addr_fast_mode_in_lbmf = 0x31B7,
	.init_setting_table = imx06c_init_setting,
	.init_setting_len = ARRAY_SIZE(imx06c_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 1,
	.chk_s_off_end = 0,

	.checksum_value = 0xAF3E324F,
	.aov_sensor_support = TRUE,
	.init_in_open = TRUE,
	.streaming_ctrl_imp = FALSE,
	.ebd_info = {
		.frm_cnt_loc = {
			.loc_line = 1,
			.loc_pix = {7},
		},
		.coarse_integ_loc = {
			{  // NE
				.loc_line = 1,
				.loc_pix = {47, 49},
			},
			{  // ME
				.loc_line = 2,
				.loc_pix = {47, 49},
			},
			{  // SE
				.loc_line = 1,
				.loc_pix = {81, 83},
			},
		},
		.ana_gain_loc = {
			{  // NE
				.loc_line = 1,
				.loc_pix = {51, 53},
			},
			{  // ME
				.loc_line = 2,
				.loc_pix = {51, 53},
			},
			{  // SE
				.loc_line = 1,
				.loc_pix = {63, 65},
			},
		},
		.dig_gain_loc = {
			{  // NE
				.loc_line = 1,
				.loc_pix = {57, 59},
			},
			{  // ME
				.loc_line = 2,
				.loc_pix = {55, 57},
			},
			{  // SE
				.loc_line = 1,
				.loc_pix = {67, 69},
			},
		},
		.coarse_integ_shift_loc = {
			.loc_line = 2,
			.loc_pix = {37},
		},
		.dol_loc = {
			.loc_line = 2,
			.loc_pix = {81, 83}, // dol_en and dol_mode
		},
		.framelength_loc = {
			.loc_line = 1,
			.loc_pix = {121, 123},
		},
		.temperature_loc = {
			.loc_line = 1,
			.loc_pix = {37},
		},
	},
	.mcss_update_subdrv_para = imx06c_mcss_update_subdrv_para,
	.cust_get_linetime_in_ns = imx06c_get_linetime_in_ns,
	.cycle_base_ratio = 16,

	/* MCSS */
	.use_mcss_gph_sync = 0,
	.reg_addr_mcss_slave_add_en_2nd = 0x3024,
	.reg_addr_mcss_slave_add_acken_2nd = 0x3025,
	.reg_addr_mcss_controller_target_sel = 0x3050,
	.reg_addr_mcss_xvs_io_ctrl = 0x3030,
	.reg_addr_mcss_extout_en = 0x3051,
	.reg_addr_mcss_complete_sleep_en = 0x385E,
	.reg_addr_mcss_mc_frm_lp_en = 0x306D,
	.reg_addr_mcss_frm_length_reflect_timing =  0x3020,
	.reg_addr_mcss_mc_frm_mask_num = 0x306c,
	.mcss_init = imx06c_mcss_init,
};

static int imx06c_get_linetime_in_ns(void *arg,
	u32 scenario_id, u32 *linetime_in_ns, enum GET_LINETIME_ENUM linetime_type,
	enum IMGSENSOR_EXPOSURE exp_idx)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u32 ret;

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
	// ret = common_get_pixel_clk_base_linetime_in_ns(ctx, scenario_id,
	/* linetime_in_ns, linetime_type, exp_idx); */
	// break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
	case SENSOR_SCENARIO_ID_CUSTOM2:
	case SENSOR_SCENARIO_ID_CUSTOM3:
	case SENSOR_SCENARIO_ID_CUSTOM4:
	case SENSOR_SCENARIO_ID_CUSTOM5:
	// ret = common_get_cycle_base_v1_linetime_in_ns(ctx, scenario_id,
	/* linetime_in_ns, linetime_type, exp_idx); */
	// break;
	case SENSOR_SCENARIO_ID_CUSTOM6:
	case SENSOR_SCENARIO_ID_CUSTOM7:
	case SENSOR_SCENARIO_ID_CUSTOM8:
	case SENSOR_SCENARIO_ID_CUSTOM9:
	case SENSOR_SCENARIO_ID_CUSTOM10:
	// ret = custom_formula_get_linetime_in_ns(ctx, scenario_id,
	/* linetime_in_ns, linetime_type, exp_idx); */
	// break;
	default:
		ret = common_get_cycle_base_v1_linetime_in_ns(ctx, scenario_id,
			linetime_in_ns, linetime_type, exp_idx);
		break;
	}

	DRV_LOG(ctx, "linetime(%d)ns\n", *linetime_in_ns);
	return ret;
}

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
	.vsync_notify = vsync_notify,
	.update_sof_cnt = common_update_sof_cnt,
	.parse_ebd_line = common_parse_ebd_line,
	.mcss_set_mask_frame = imx06c_mcss_set_mask_frame,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST, {0}, 0},
	{HW_ID_MCLK_DRIVING_CURRENT, {2}, 1000},
	{HW_ID_MCLK, {24}, 0},
	{HW_ID_DOVDD, {1800000, 1800000}, 0}, // pmic_gpo(1.8V ldo) for dovdd
	{HW_ID_AVDD, {2700000, 2900000}, 0}, // pmic_ldo5 support min:1.7V ~ max:3.0V
	{HW_ID_AVDD1, {1800000, 1800000}, 0}, // pmic_gpo1.8V to pmic_ldo(1.8V ldo) for avdd1
	// {HW_ID_AFVDD, {3300000, 3300000}, 0}, // pmic_ldo for afvdd
	{HW_ID_AFVDD1, {1800000, 1800000}, 0}, // pmic_gpo(3.1V ldo) for afvdd
	{HW_ID_DVDD1, {720000, 900000}, 1000}, // pmic_ldo8(min:0.72V ~ max:0.9V ldo) for dvdd
	{HW_ID_OISVDD, {3300000, 3300000}, 1000},
	{HW_ID_RST, {1}, 2000}
};

static struct subdrv_pw_seq_entry aov_pw_seq[] = {
	{HW_ID_RST, {0}, 0},
	{HW_ID_MCLK_DRIVING_CURRENT, {2}, 1000},
	{HW_ID_MCLK, {26, MCLK_ULPOSC}, 0},
	{HW_ID_DOVDD, {1800000, 1800000}, 0}, // pmic_gpo(1.8V ldo) for dovdd
	{HW_ID_AVDD, {2700000, 2900000}, 0}, // pmic_ldo5 support min:1.7V ~ max:3.0V
	{HW_ID_AVDD1, {1800000, 1800000}, 0}, // pmic_gpo1.8V to pmic_ldo(1.8V ldo) for avdd1
	// {HW_ID_AFVDD, {3300000, 3300000}, 0}, // pmic_ldo for afvdd
	{HW_ID_AFVDD1, {1800000, 1800000}, 0}, // pmic_gpo(3.1V ldo) for afvdd
	{HW_ID_DVDD1, {720000, 900000}, 1000}, // pmic_ldo8(min:0.72V ~ max:0.9V ldo) for dvdd
	{HW_ID_OISVDD, {3300000, 3300000}, 1000},
	{HW_ID_RST, {1}, 2000}
};


#ifdef IMX06C_ISF_DBG
const struct subdrv_entry imx06c_mipi_raw_entry_legacy = {
#else
const struct subdrv_entry imx06c_mipi_raw_entry = {
#endif
	.name = "imx06c_mipi_raw",
	.id = IMX06C_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
	.aov_pw_seq = aov_pw_seq,
	.aov_pw_seq_cnt = ARRAY_SIZE(aov_pw_seq),
};

/* FUNCTION */

static void set_sensor_cali(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	u16 idx = 0;
	u8 support = FALSE;
	u8 *pbuf = NULL;
	u16 size = 0;
	u16 addr = 0;
	struct eeprom_info_struct *info = ctx->s_ctx.eeprom_info;

	//if (!probe_eeprom(ctx))
	//	return;

	idx = ctx->eeprom_index;

	/* QSC data */
	support = info[idx].qsc_support;
	pbuf = info[idx].preload_qsc_table;
	size = info[idx].qsc_size;
	addr = info[idx].sensor_reg_addr_qsc;
	if (support) {
		if (pbuf != NULL && addr > 0 && size > 0) {
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			// subdrv_i2c_wr_u8(ctx, 0x3206, 0x01);
			DRV_LOG(ctx, "set QSC calibration data done.");
		} else {
			subdrv_i2c_wr_u8(ctx, 0x3206, 0x00);
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

static int imx06c_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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

	set_i2c_buffer(ctx, 0x0104, 0x01);
	set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x02);

	update_mode_info_seamless_switch(ctx, scenario_id);
	set_table_to_buffer(ctx,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);

	if (ctx->s_ctx.reg_addr_fast_mode_in_lbmf &&
		(ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_LBMF ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF))
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode_in_lbmf, 0x4);

	ctx->ae_ctrl_gph_en = 1;
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
		case HDR_RAW_DCG_RAW:
			set_shutter(ctx, ae_ctrl->exposure.le_exposure);
			if (ctx->s_ctx.mode[scenario_id].dcg_info.dcg_gain_mode
				== IMGSENSOR_DCG_DIRECT_MODE)
				set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			else
				set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		default:
			set_shutter(ctx, ae_ctrl->exposure.le_exposure);
			set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		}
	}

	set_i2c_buffer(ctx, 0x0104, 0x00);
	ctx->ae_ctrl_gph_en = 0;
	commit_i2c_buffer(ctx);

	ctx->fast_mode_on = TRUE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->is_seamless = FALSE;
	DRV_LOG(ctx, "X: set seamless switch done\n");
	return ERROR_NONE;
}

static int imx06c_cphy_lrte_mode(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	u8 cphy_lrte_en = 0;

	scenario_id = *((u64 *)para);
	cphy_lrte_en =
		ctx->s_ctx.mode[scenario_id].csi_param.cphy_lrte_support;

	if (cphy_lrte_en) {
		/*cphy lrte enable*/
		subdrv_i2c_wr_u8(ctx, 0x0860, 0x80);//enable cphy lrte and short packet 110 spacers
		subdrv_i2c_wr_u8(ctx, 0x0861, 0x6E);
		subdrv_i2c_wr_u8(ctx, 0x0862, 0x00);//long packet 40 spacers
		subdrv_i2c_wr_u8(ctx, 0x0863, 0x28);
	} else {
		/*cphy lrte disable*/
		subdrv_i2c_wr_u8(ctx, 0x0860, 0x00);//disable cphy lrte
		subdrv_i2c_wr_u8(ctx, 0x0861, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x0862, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x0863, 0x00);
	}

	DRV_LOG_MUST(ctx, "cphy_lrte_en = %d, scen = %u\n",
		cphy_lrte_en, scenario_id);
	return ERROR_NONE;
}

static int imx06c_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	if (mode)
		subdrv_i2c_wr_u8(ctx, 0x0601, mode); /*100% Color bar*/
	else if (ctx->test_pattern)
		subdrv_i2c_wr_u8(ctx, 0x0601, 0x00); /*No pattern*/

	ctx->test_pattern = mode;
	return ERROR_NONE;
}

static int imx06c_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	struct mtk_test_pattern_data *data = (struct mtk_test_pattern_data *)para;
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
		ctx->test_pattern, R, Gr, Gb, B);
	return ERROR_NONE;
}

static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id)
{
#ifdef IMX06C_ISF_DBG
	memcpy(&(ctx->s_ctx), &imx06c_legacy_s_ctx, sizeof(struct subdrv_static_ctx));
#else
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
#endif
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt, u64 sof_ts)
{
	DRV_LOG(ctx, "sof_cnt(%u) ctx->ref_sof_cnt(%u) ctx->fast_mode_on(%d)",
		sof_cnt, ctx->ref_sof_cnt, ctx->fast_mode_on);
	if (ctx->fast_mode_on && (sof_cnt > ctx->ref_sof_cnt)) {
		ctx->fast_mode_on = FALSE;
		ctx->ref_sof_cnt = 0;
		DRV_LOG(ctx, "seamless_switch disabled.");
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x00);
		commit_i2c_buffer(ctx);
	}
	return 0;
}

static int imx06c_mcss_update_subdrv_para(void *arg, int scenario_id)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u32 framerate = ctx->s_ctx.mode[scenario_id].max_framerate;
	u32 linetime_in_ns = 0;

	if (ctx->s_ctx.sensor_mode_num <= scenario_id) {
		DRV_LOGE(ctx, "scenario_id:%d invalid", scenario_id);
		return -EINVAL;
	}

	/* cycle_base_v1_linetime formula to get linetime to calculate framerate */
	common_get_cycle_base_v1_linetime_in_ns(ctx, scenario_id, &linetime_in_ns, 0, 0);

	ctx->min_frame_length = max(ctx->min_frame_length, ctx->s_ctx.mode[scenario_id].framelength);
	ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;

	if (linetime_in_ns == 0) {
		ctx->current_fps = ctx->s_ctx.mode[scenario_id].max_framerate;
		DRV_LOGE(ctx, "linetime_in_ns is zero using default max_framerate\n");
	} else {
		framerate = 1000000000 / linetime_in_ns / ctx->frame_length  * 10;
		ctx->current_fps = framerate;
	}

	DRV_LOG_MUST(ctx, "scenario_id(%d) framelength(%d) min_frame_length(%d)\n",
			scenario_id,
			ctx->s_ctx.mode[scenario_id].framelength,
			ctx->min_frame_length);
	DRV_LOG_MUST(ctx, "ctx->frame_length(%d), ctx->current_fps(%d) linetime_in_ns(%d)\n",
			ctx->frame_length,
			ctx->current_fps,
			linetime_in_ns);

	return ERROR_NONE;
}

static int imx06c_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	imx06c_set_shutter_frame_length(ctx, para, 0);

	return 0;
}

static void imx06c_set_long_exposure(struct subdrv_ctx *ctx)
{
	u32 shutter = ctx->exposure[IMGSENSOR_STAGGER_EXPOSURE_LE];
	u32 l_shutter = 0;
	u16 l_shift = 0;

	if (shutter > (ctx->s_ctx.frame_length_max - ctx->s_ctx.exposure_margin)) {
		if (ctx->s_ctx.long_exposure_support == FALSE) {
			DRV_LOGE(ctx, "sensor no support of exposure lshift!\n");
			return;
		}
		if (ctx->s_ctx.reg_addr_exposure_lshift == PARAM_UNDEFINED) {
			DRV_LOGE(ctx, "please implement lshift register address\n");
			return;
		}
		for (l_shift = 1; l_shift < 8; l_shift++) {
			l_shutter = ((shutter - 1) >> l_shift) + 1;
			if (l_shutter
				< (ctx->s_ctx.frame_length_max - ctx->s_ctx.exposure_margin))
				break;
		}
		if (l_shift > 8) {
			DRV_LOGE(ctx, "unable to set exposure:%u, set to max\n", shutter);
			l_shift = 8;
		}
		shutter = ((shutter - 1) >> l_shift) + 1;

		ctx->frame_length = shutter + ctx->s_ctx.exposure_margin;
		DRV_LOG(ctx, "long reg_addr_exposure_lshift %u times", l_shift);
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure_lshift, l_shift);
		ctx->l_shift = l_shift;
		/* Frame exposure mode customization for LE*/
		ctx->ae_frm_mode.frame_mode_1 = IMGSENSOR_AE_MODE_SE;
		ctx->ae_frm_mode.frame_mode_2 = IMGSENSOR_AE_MODE_SE;
		ctx->current_ae_effective_frame = 2;
		if (ctx->s_ctx.reg_addr_frame_length_lshift != PARAM_UNDEFINED) {
			set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_frame_length_lshift, l_shift);
			DRV_LOG(ctx, "long reg_addr_frame_length_lshift %u times", l_shift);
		}
	} else {
		if (ctx->s_ctx.reg_addr_exposure_lshift != PARAM_UNDEFINED)
			set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure_lshift, l_shift);

		if (ctx->s_ctx.reg_addr_frame_length_lshift != PARAM_UNDEFINED)
			set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_frame_length_lshift, l_shift);

		ctx->l_shift = l_shift;
		ctx->current_ae_effective_frame = 2;
	}

	ctx->exposure[IMGSENSOR_STAGGER_EXPOSURE_LE] = shutter;
}

static void imx06c_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	u64 shutter = *feature_data;
	u32 frame_length = (u32) (*(para + 1));

	int fine_integ_line = 0;
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

	DRV_LOG(ctx, "input linecount = %llu", shutter);
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
	/* write shutter */
	imx06c_set_long_exposure(ctx);

	/* write framelength */
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx, ctx->frame_length);
	else if (ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx, ctx->min_frame_length);

	if (ctx->s_ctx.reg_addr_exposure[0].addr[2]) {
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure[0].addr[0],
			(ctx->exposure[0] >> 16) & 0xFF);
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure[0].addr[1],
			(ctx->exposure[0] >> 8) & 0xFF);
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure[0].addr[2],
			ctx->exposure[0] & 0xFF);
	} else {
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure[0].addr[0],
			(ctx->exposure[0] >> 8) & 0xFF);
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure[0].addr[1],
			ctx->exposure[0] & 0xFF);
	}
	DRV_LOG(ctx, "exp[0x%x], fll(input/output):%u/%u, flick_en:%d",
		ctx->exposure[0], frame_length, ctx->frame_length, ctx->autoflicker_en);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */
}

static int imx06c_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	if (ctx->s_ctx.aov_sensor_support &&
		ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode &&
		(ctx->s_ctx.mode[ctx->current_scenario_id].ae_ctrl_support !=
			IMGSENSOR_AE_CONTROL_SUPPORT_VIEWING_MODE))
		DRV_LOG_MUST(ctx,
			"AOV mode not support ae gain control!\n");
	else {
		u32 *feature_data = (u32 *) para;
		u32 gain = *feature_data;
		u16 rg_gain;
		bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);


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
	}
	return 0;
}

static int imx06c_mcss_init(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	if (!(ctx->mcss_init_info.enable_mcss)) {
		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_mcss_mc_frm_mask_num, 0);
		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_mcss_slave_add_en_2nd, 0x00);
		subdrv_i2c_wr_u8(ctx,
			ctx->s_ctx.reg_addr_mcss_slave_add_acken_2nd, 0x00);
		subdrv_i2c_wr_u8(ctx,
			ctx->s_ctx.reg_addr_mcss_controller_target_sel, 0x01);
			// controller mode is default
		subdrv_i2c_wr_u8(ctx,
			ctx->s_ctx.reg_addr_mcss_xvs_io_ctrl, 0x00);
		subdrv_i2c_wr_u8(ctx,
			ctx->s_ctx.reg_addr_mcss_extout_en, 0x00);
		// low-power for deep sleep
		//subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_mcss_mc_frm_lp_en, 0x00);
		// FLL N+1/N+2

		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_mcss_frm_length_reflect_timing, 0x00);
		//0:N+1, 1:N+2

		memset(&(ctx->mcss_init_info), 0, sizeof(struct mtk_fsync_hw_mcss_init_info));
		DRV_LOG_MUST(ctx, "Disable MCSS\n");
		return ERROR_NONE;
	}

	// master or slave
	if (ctx->mcss_init_info.is_mcss_master) {
		DRV_LOG_MUST(ctx, "common_mcss_init controller (ctx->s_ctx.sensor_id=0x%x)\n",
			ctx->s_ctx.sensor_id);
		subdrv_i2c_wr_u8(ctx,
			ctx->s_ctx.reg_addr_mcss_slave_add_en_2nd, 0x01);
		subdrv_i2c_wr_u8(ctx,
			ctx->s_ctx.reg_addr_mcss_slave_add_acken_2nd, 0x01);
		subdrv_i2c_wr_u8(ctx,
			ctx->s_ctx.reg_addr_mcss_controller_target_sel, 0x01);
			// controller mode is default
		subdrv_i2c_wr_u8(ctx,
			ctx->s_ctx.reg_addr_mcss_xvs_io_ctrl, 0x01);
		subdrv_i2c_wr_u8(ctx,
			ctx->s_ctx.reg_addr_mcss_extout_en, 0x01);
	} else {
		DRV_LOG_MUST(ctx, "common_mcss_init target (ctx->s_ctx.sensor_id=0x%x)\n",
			ctx->s_ctx.sensor_id);
		subdrv_i2c_wr_u8(ctx,
			ctx->s_ctx.reg_addr_mcss_slave_add_en_2nd, 0x01);
		subdrv_i2c_wr_u8(ctx,
			ctx->s_ctx.reg_addr_mcss_slave_add_acken_2nd, 0x00);
		subdrv_i2c_wr_u8(ctx,
			ctx->s_ctx.reg_addr_mcss_controller_target_sel, 0x00);
		subdrv_i2c_wr_u8(ctx,
			ctx->s_ctx.reg_addr_mcss_xvs_io_ctrl, 0x00);
		subdrv_i2c_wr_u8(ctx,
			ctx->s_ctx.reg_addr_mcss_extout_en, 0x00);
	}

	// FLL N+1/N+2
	subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_mcss_frm_length_reflect_timing, 0x00);
	//0:N+1, 1:N+2

	return ERROR_NONE;
}

static int imx06c_mcss_set_mask_frame(struct subdrv_ctx *ctx, u32 num, u32 is_critical)
{
	ctx->s_ctx.s_gph((void *)ctx, 1);
	set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_mcss_mc_frm_mask_num,  (0x7f & num));
	ctx->s_ctx.s_gph((void *)ctx, 0);

	if (is_critical)
		commit_i2c_buffer(ctx);

	DRV_LOG(ctx, "set mask frame num:%d\n", (0x7f & num));
	return 0;
}
