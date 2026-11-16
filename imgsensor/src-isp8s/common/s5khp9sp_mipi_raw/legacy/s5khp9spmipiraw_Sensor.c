// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5khp9spmipiraw_Sensor.c
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
#include "s5khp9spmipiraw_Sensor.h"
#include "adaptor-subdrv-ctrl.h"
#include "adaptor-subdrv.h"
#include "adaptor-ctrls.h"
#include "s5khp9sp_ana_gain_table.h"
#include <linux/of.h>
#include "adaptor-i2c.h"
#include <linux/regulator/consumer.h>

static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static int s5khp9sp_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int s5khp9sp_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static void s5khp9sp_sensor_init(struct subdrv_ctx *ctx);
static int open(struct subdrv_ctx *ctx);
static int s5khp9sp_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt, u64 sof_ts);
static int s5khp9sp_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int s5khp9sp_i3c_pre_config(struct subdrv_ctx *ctx);

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, s5khp9sp_set_test_pattern},
	{SENSOR_FEATURE_SET_TEST_PATTERN_DATA, s5khp9sp_set_test_pattern_data},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, s5khp9sp_seamless_switch},
	{SENSOR_FEATURE_SET_AWB_GAIN, s5khp9sp_set_awb_gain},
};


static struct mtk_sensor_saturation_info imgsensor_saturation_info_10bit = {
	.gain_ratio = 1000,
	.OB_pedestal = 64,
	.saturation_level = 1023,
	.adc_bit = 10,
	.ob_bm = 64,
};

#define pd_i4Crop { \
	/* pre cap normal_video hs_video slim_video */\
	{0, 0}, {0, 0}, {0, 256}, {0, 256}, {0, 0},\
	/* cust1       cust2       cust3      cust4     cust5 */\
	{0, 0}, {4080, 3072}, {0, 0}, {6120, 4608}, {6120, 4608},\
	/* cust6        cust7        cust8       cust9     cust10 */\
	{6120, 4608}, {2040, 1536}, {2040, 1536}, {2040, 1536}, {0, 0},\
	/* cust11 cust12 cust13 cust14 cust15 */\
	{0, 256}, {2040, 1792}, {0, 256}, {6120, 4864}, {6120, 4864},\
	/* cust16 cust17 */\
	{2040, 1924}, {0, 0},\
}

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
	.iMirrorFlip = 2,
	.i4FullRawW = 16320,
	.i4FullRawH = 12288,
	.i4ModeIndex = 2,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		.i4VCFeature = VC_PDAF_STATS_NE_PIX_1,
		.i4PDPattern = 1,
		.i4BinFacX = 8,
		.i4BinFacY = 16,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
	/* VC's PD pattern description */
	.sPDMapInfo[1] = {
		.i4VCFeature = VC_PDAF_STATS_NE_PIX_2,
		.i4PDPattern = 1,
		.i4BinFacX = 32,
		.i4BinFacY = 16,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
	.sPDMapInfo[2] = {
		.i4VCFeature = VC_PDAF_STATS_ME_PIX_1,
		.i4PDPattern = 1,
		.i4BinFacX = 8,
		.i4BinFacY = 16,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_fullsize_fdsum = {
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
	.iMirrorFlip = 2,
	.i4FullRawW = 8160,
	.i4FullRawH = 6144,
	.i4ModeIndex = 2,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		.i4VCFeature = VC_PDAF_STATS_NE_PIX_1,
		.i4PDPattern = 1,
		.i4BinFacX = 4,
		.i4BinFacY = 8,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
	/* VC's PD pattern description */
	.sPDMapInfo[1] = {
		.i4VCFeature = VC_PDAF_STATS_NE_PIX_2,
		.i4PDPattern = 1,
		.i4BinFacX = 16,
		.i4BinFacY = 8,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
	.sPDMapInfo[2] = {
		.i4VCFeature = VC_PDAF_STATS_ME_PIX_1,
		.i4PDPattern = 1,
		.i4BinFacX = 4,
		.i4BinFacY = 8,
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
	.iMirrorFlip = 2,
	.i4FullRawW = 4080,
	.i4FullRawH = 3072,
	.i4ModeIndex = 2,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		.i4VCFeature = VC_PDAF_STATS_NE_PIX_1,
		.i4PDPattern = 1,
		.i4BinFacX = 2,
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
	/* VC's PD pattern description */
	.sPDMapInfo[1] = {
		.i4VCFeature = VC_PDAF_STATS_NE_PIX_2,
		.i4PDPattern = 1,
		.i4BinFacX = 8,
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_8Fdsum_4H2V = {
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
	.iMirrorFlip = 2,
	.i4FullRawW = 2040,
	.i4FullRawH = 1536,
	.i4ModeIndex = 2,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		.i4VCFeature = VC_PDAF_STATS_NE_PIX_1,
		.i4PDPattern = 1,
		.i4BinFacX = 2,
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
	/* VC's PD pattern description */
	.sPDMapInfo[1] = {
		.i4VCFeature = VC_PDAF_STATS_NE_PIX_2,
		.i4PDPattern = 1,
		.i4BinFacX = 8,
		.i4BinFacY = 4,
		.i4PDRepetition = 0,
		.i4PDOrder = {1},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_28[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 2296,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 2040,
			.vsize = 286,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 504,
			.vsize = 286,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_29[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_30[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_1[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 16320,
			.vsize = 12288,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 4080,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 1016,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_2[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 8160,
			.vsize = 6144,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 4080,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 1016,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_3[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 8160,
			.vsize = 6144,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 4080,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 1016,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_4[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 8160,
			.vsize = 6144,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 2040,
			.vsize = 384,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 504,
			.vsize = 384,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_5[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 4080,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 1016,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_8_without_AEB[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 1016,
			.vsize = 192,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 248,
			.vsize = 192,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_7[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 1016,
			.vsize = 192,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 248,
			.vsize = 192,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_8[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 1016,
			.vsize = 192,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 248,
			.vsize = 192,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
		{
		.bus.csi2 = {
			.channel = 0x3,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x4,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 1016,
			.vsize = 192,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x5,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 248,
			.vsize = 192,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_9[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 2040,
			.vsize = 384,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 504,
			.vsize = 384,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
		{
		.bus.csi2 = {
			.channel = 0x3,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x4,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 2040,
			.vsize = 384,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x5,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 504,
			.vsize = 384,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_10[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 2040,
			.vsize = 384,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 504,
			.vsize = 384,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_11[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 2040,
			.vsize = 384,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 504,
			.vsize = 384,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_12[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 3072,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 4080,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 1016,
			.vsize = 768,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_13[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 2040,
			.vsize = 1536,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 2040,
			.vsize = 384,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 504,
			.vsize = 384,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_14[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 4080,
			.vsize = 640,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 1016,
			.vsize = 640,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_15[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x2b, /* 0x30, */
			.hsize = 4080,
			.vsize = 640,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x3,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 1016,
			.vsize = 640,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_16[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 2040,
			.vsize = 320,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 504,
			.vsize = 320,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_17[] = {
	{
		.bus.csi2 = {
		.channel = 0x0,
		.data_type = 0x2b,
		.hsize = 4080,
		.vsize = 2560,
		.user_data_desc = VC_STAGGER_NE,
		.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
	.bus.csi2 = {
		.channel = 0x1,
		.data_type = 0x30, /* 0x2b, */
		.hsize = 4080,
		.vsize = 640,
		.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
	.bus.csi2 = {
		.channel = 0x2,
		.data_type = 0x30, /* 0x2b, */
		.hsize = 1016,
		.vsize = 640,
		.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
		.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_18[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 1016,
			.vsize = 160,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 248,
			.vsize = 160,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_19[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 1016,
			.vsize = 160,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 248,
			.vsize = 160,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x3,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x4,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 1016,
			.vsize = 160,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x5,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 248,
			.vsize = 160,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_FCM_23[] = {
	{
		.bus.csi2 = {
			.channel = 0x0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 2560,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x1,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 4080,
			.vsize = 640,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0x2,
			.data_type = 0x30, /* 0x2b, */
			.hsize = 1016,
			.vsize = 640,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_2,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct subdrv_mode_struct mode_struct[] = {
/* prev -> FCM_12 */
/* 17_Volcano1_HP3_8Fdsum_2H1V_12.5Mp_30FPS_4080x3072_direct.sset */
	{
		.frame_desc = frame_desc_FCM_12,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_12),
		.mode_setting_table = FCM_12_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_12_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = FCM_12_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_12_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2002000000,
		.linelength = 7920,
		.framelength = 8374,
		.max_framerate = 300,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* BIN */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080*4)/2,
			.y0_offset = (12288-3072*4)/2,
			.w0_size = 4080*4,
			.h0_size = 3072*4,
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
		.ae_binning_ratio = 2000,
		.fine_integ_line = 360,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
	},
/* cap -> FCM_12 */
/* 17_Volcano1_HP3_8Fdsum_2H1V_12.5Mp_30FPS_4080x3072_direct.sset */
	{
		.frame_desc = frame_desc_FCM_12,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_12),
		.mode_setting_table = FCM_12_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_12_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = FCM_12_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_12_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2002000000,
		.linelength = 7920,
		.framelength = 8374,
		.max_framerate = 300,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {  /* BIN */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080*4)/2,
			.y0_offset = (12288-3072*4)/2,
			.w0_size = 4080*4,
			.h0_size = 3072*4,
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
		.ae_binning_ratio = 2000,
		.fine_integ_line = 360,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
	},
/* video -> FCM_14 */
/* 17_Volcano4_HP3_8Fdsum_2H1V_12.5Mp_60FPS_4080x2616_direct.sset */
	{
		.frame_desc = frame_desc_FCM_14,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_14),
		.mode_setting_table = FCM_14_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_14_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = FCM_14_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_14_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2002000000,
		.linelength = 7920,
		.framelength = 8374,
		.max_framerate = 300,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {  /* 16:10 BIN */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080*4)/2,
			.y0_offset = (12288-2560*4)/2,
			.w0_size = 4080*4,
			.h0_size = 2560*4,
			.scale_w = 4080,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2560,
		},
		.ae_binning_ratio = 2000,
		.fine_integ_line = 360,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
	},
/* hs_video -> FCM_23 */
/* 17_Volcano4_HP3_8Fdsum_2H1V_12.5Mp_60FPS_4080x2616_direct.sset */
	{
		.frame_desc = frame_desc_FCM_23,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_23),
		.mode_setting_table = FCM_23_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_23_setting),
		.seamless_switch_group = 3,
		.seamless_switch_mode_setting_table = FCM_23_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_23_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2002000000,
		.linelength = 7920,
		.framelength = 4198,
		.max_framerate = 600,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {  /* 16:10 BIN */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080*4)/2,
			.y0_offset = (12288-2560*4)/2,
			.w0_size = 4080*4,
			.h0_size = 2560*4,
			.scale_w = 4080,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2560,
		},
		.ae_binning_ratio = 2000,
		.fine_integ_line = 360,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
	},
/* no FPM filled slim_video -> FCM_2 */
/* 06_Volcano_HP3_Fdsum_50Mp_24FPS_8160x6144_direct.sset */
	{
		.frame_desc = frame_desc_FCM_2,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_2),
		.mode_setting_table = FCM_2_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_2_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = FCM_2_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_2_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1759333333,
		.linelength = 11344,
		.framelength = 6452,
		.max_framerate = 240,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* FULL MODE Bayer HW RMSC */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-8160*2)/2,
			.y0_offset = (12288-6144*2)/2,
			.w0_size = 8160*2,
			.h0_size = 6144*2,
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
		.ae_binning_ratio = 2000,
		.fine_integ_line = 316,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize_fdsum,
	},
/* cus1 -> FCM_3 */
/* 07_Volcano_HP3_Fdsum_50Mp_24FPS_8160x6144_direct.sset */
	{
		.frame_desc = frame_desc_FCM_3,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_3),
		.mode_setting_table = FCM_3_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_3_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = FCM_3_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_3_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1759333333,
		.linelength = 11344,
		.framelength = 6452,
		.max_framerate = 240,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* FULL MODE 4cell */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-8160*2)/2,
			.y0_offset = (12288-6144*2)/2,
			.w0_size = 8160*2,
			.h0_size = 6144*2,
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
		.ae_binning_ratio = 2000,
		.fine_integ_line = 316,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2, /* tetra */
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.awb_enabled = 1,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize_fdsum,
	},

/* cus2 -> FCM_4 */
/* 02_Volcano_HP3_Full_50Mp_15.5FPS_8160x6144_direct.sset */
	{
		.frame_desc = frame_desc_FCM_4,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_4),
		.mode_setting_table = FCM_4_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_4_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = FCM_4_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_4_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2002000000,
		.linelength = 20320,
		.framelength = 6351,
		.max_framerate = 155,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* 2X crop 16cell */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-8160)/2,
			.y0_offset = (12288-6144)/2,
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
		.ae_binning_ratio = 2000,
		.fine_integ_line = 360,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_4X4, /* hexdeca */
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.awb_enabled = 1,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize,
	},

/* cus3 -> FCM_5 */
/* 17_Volcano1_HP3_8Fdsum_2H1V_12.5Mp_30FPS_4080x3072_direct.sset */
	{
		.frame_desc = frame_desc_FCM_5,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_5),
		.mode_setting_table = FCM_5_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_5_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = FCM_5_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_5_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2002000000,
		.linelength = 7920,
		.framelength = 4202,
		.max_framerate = 600,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* BIN */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080*4)/2,
			.y0_offset = (12288-3072*4)/2,
			.w0_size = 4080*4,
			.h0_size = 3072*4,
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
		.ae_binning_ratio = 2000,
		.fine_integ_line = 360,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
	},


/* cus4 -> FCM_8 without AEB */
/* 02_Volcano2_HP3_Full_12.5Mp_30FPS_4080x3072_HCGonly.sset */
	{
		.frame_desc = frame_desc_FCM_8_without_AEB,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_8_without_AEB),
		.mode_setting_table = FCM_8_without_AEB_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_8_without_AEB_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = FCM_8_setting_without_AEB_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_8_setting_without_AEB_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2002000000,
		.linelength = 20320,
		.framelength = 3279,
		.max_framerate = 300,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* 4X crop 16cell */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080)/2,
			.y0_offset = (12288-3072)/2,
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
		.ae_binning_ratio = 2000,
		.fine_integ_line = 360,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_4X4, /* hexdeca */
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.awb_enabled = 1,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize,
	},

/* cus5 -> FCM_7 */
/* 02_Volcano1_HP3_Full_12.5Mp_30FPS_4080x3072_direct.sset */
	{
		.frame_desc = frame_desc_FCM_7,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_7),
		.mode_setting_table = FCM_7_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_7_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = FCM_7_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_7_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2002000000,
		.linelength = 20320,
		.framelength = 3279,
		.max_framerate = 300,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* 4X crop HW RM */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080)/2,
			.y0_offset = (12288-3072)/2,
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
		.ae_binning_ratio = 2000,
		.fine_integ_line = 360,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize,
	},

/* cus6 -> FCM_8 */
/* 02_Volcano2_HP3_Full_12.5Mp_30FPS_4080x3072_HCGonly.sset */
	{
		.frame_desc = frame_desc_FCM_8,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_8),
		.mode_setting_table = FCM_8_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_8_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = FCM_8_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_8_setting_seamless),
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2002000000,
		.linelength = 20320,
		.framelength = 3279 * 2,
		.max_framerate = 150,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 3072,
		.read_margin = (3279 - 3072),
		.imgsensor_winsize_info = { /* 4X crop 16cell */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080)/2,
			.y0_offset = (12288-3072)/2,
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
		.ae_binning_ratio = 2000,
		.fine_integ_line = 360,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_4X4, /* hexdeca */
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.awb_enabled = 1,
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = (0xFFFF - 74),
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = (0xFFFF - 74),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 16,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize,
	},

/* cus7 -> FCM_9 */
/* 11_Volcano_HP3_Fdsum_12.5Mp_30FPS_4080x3072_direct.sset */
	{
		.frame_desc = frame_desc_FCM_9,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_9),
		.mode_setting_table = FCM_9_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_9_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = FCM_9_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_9_setting_seamless),
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 1759333333,
		.linelength = 11562,
		.framelength = 5072 * 2,
		.max_framerate = 150,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 3072,
		.read_margin = (5072 - 3072),
		.imgsensor_winsize_info = { /* 2X crop 4cell */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080*2)/2,
			.y0_offset = (12288-3072*2)/2,
			.w0_size = 4080*2,
			.h0_size = 3072*2,
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
		.ae_binning_ratio = 2000,
		.fine_integ_line = 316,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2, /* tetra */
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.awb_enabled = 1,
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = (0xFFFF - 74),
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = (0xFFFF - 74),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 64,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize_fdsum,
	},

/* cus8 -> FCM_10 */
/* 11_Volcano_HP3_Fdsum_12.5Mp_30FPS_4080x3072_direct.sset */
	{
		.frame_desc = frame_desc_FCM_10,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_10),
		.mode_setting_table = FCM_10_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_10_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = FCM_10_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_10_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1759333333,
		.linelength = 11680,
		.framelength = 5006,
		.max_framerate = 300,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* 2X crop HW RM */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080*2)/2,
			.y0_offset = (12288-3072*2)/2,
			.w0_size = 4080*2,
			.h0_size = 3072*2,
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
		.ae_binning_ratio = 2000,
		.fine_integ_line = 316,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize_fdsum,
	},

/* cus9 -> FCM_11 */
/* 11_Volcano_HP3_Fdsum_12.5Mp_30FPS_4080x3072_direct.sset */
	{
		.frame_desc = frame_desc_FCM_11,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_11),
		.mode_setting_table = FCM_11_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_11_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = FCM_11_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_11_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1759333333,
		.linelength = 11680,
		.framelength = 5006,
		.max_framerate = 300,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* 2X crop 4cell */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080*2)/2,
			.y0_offset = (12288-3072*2)/2,
			.w0_size = 4080*2,
			.h0_size = 3072*2,
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
		.ae_binning_ratio = 2000,
		.fine_integ_line = 316,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2, /* tetra */
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.awb_enabled = 1,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize_fdsum,
	},

/* cus10 -> FCM_13 */
/* 63_Volcano2_HP3_8Fdsum_4H2V_FHD_120FPS_1920x1080_direct.sset */
	{
		.frame_desc = frame_desc_FCM_13,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_13),
		.mode_setting_table = FCM_13_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_13_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = FCM_13_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_13_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2002000000,
		.linelength = 12320,
		.framelength = 5402,
		.max_framerate = 300,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* BIN 3M */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-2040*8)/2,
			.y0_offset = (12288-1536*8)/2,
			.w0_size = 2040*8,
			.h0_size = 1536*8,
			.scale_w = 2040,
			.scale_h = 1536,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2040,
			.h1_size = 1536,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2040,
			.h2_tg_size = 1536,
		},
		.ae_binning_ratio = 2000,
		.fine_integ_line = 360,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_8Fdsum_4H2V,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr,

	},


/* cus11 -> FCM_15 (stagger 2-exp) */
/* 31_Volcano1_HP3_8Fdsum_2H1V_stHDR_2exp_12.5Mp_30FPS_3192x2390_direct.sset */
	{
		.frame_desc = frame_desc_FCM_15,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_15),
		.mode_setting_table = FCM_15_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_15_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = FCM_15_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_15_setting_seamless),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2002000000,
		.linelength = 6880,
		.framelength = 9648,
		.max_framerate = 300,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 2560,
		.read_margin = 64,
		.min_vblanking_line = 0, /* TBD */
		.imgsensor_winsize_info = { /* 16:10 BIN */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080*4)/2,
			.y0_offset = (12288-2560*4)/2,
			.w0_size = 4080*4,
			.h0_size = 2560*4,
			.scale_w = 4080,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2560,
		},
		.ae_binning_ratio = 1000,
		.fine_integ_line = 180,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min =  16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min =  16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max =  (0xFFFF - 74),
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max =  (0xFFFF - 74),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,//8SUM2
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 16,//8SUM2
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
	},

/* cus12 -> FCM_16 */
/* 11_Volcano5_HP3_Fdsum_12.5Mp_54.4FPS_4080x2616_direct.sset */
	{
		.frame_desc = frame_desc_FCM_16,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_16),
		.mode_setting_table = FCM_16_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_16_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = FCM_16_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_16_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1759333333,
		.linelength = 11562,
		.framelength = 2796,
		.max_framerate = 544,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* 2X crop 4cell */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080*2)/2,
			.y0_offset = (12288-2560*2)/2,
			.w0_size = 4080*2,
			.h0_size = 2560*2,
			.scale_w = 4080,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2560,
		},
		.ae_binning_ratio = 2000,
		.fine_integ_line = 316,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2, /* tetra */
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.awb_enabled = 1,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize_fdsum,
	},


/* cus13 -> FCM_17 */
/* 17_Volcano4_HP3_8Fdsum_2H1V_12.5Mp_60FPS_4080x2616_direct.sset */
	{
		.frame_desc = frame_desc_FCM_17,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_17),
		.mode_setting_table = FCM_17_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_17_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = FCM_17_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_17_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2002000000,
		.linelength = 15840,
		.framelength = 5402,
		.max_framerate = 234,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* 16:10 BIN */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080*4)/2,
			.y0_offset = (12288-2560*4)/2,
			.w0_size = 4080*4,
			.h0_size = 2560*4,
			.scale_w = 4080,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2560,
		},
		.ae_binning_ratio = 2000,
		.fine_integ_line = 360,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
	},

/* cus14 -> FCM_18 */
/* 02_Volcano1_HP3_Full_12.5Mp_30FPS_4080x3072_direct.sset */
	{
		.frame_desc = frame_desc_FCM_18,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_18),
		.mode_setting_table = FCM_18_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_18_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = FCM_18_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_18_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2002000000,
		.linelength = 20320,
		.framelength = 3279,
		.max_framerate = 300,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* 4X crop 16cell */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080)/2,
			.y0_offset = (12288-2560)/2,
			.w0_size = 4080,
			.h0_size = 2560,
			.scale_w = 4080,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2560,
		},
		.ae_binning_ratio = 2000,
		.fine_integ_line = 360,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 32,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_4X4, /* hexdeca */
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.awb_enabled = 1,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize,
	},

/* cus15 -> FCM_19 */
/* 02_Volcano2_HP3_Full_12.5Mp_30FPS_4080x3072_HCGonly.sset */
	{
		.frame_desc = frame_desc_FCM_19,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_19),
		.mode_setting_table = FCM_19_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_19_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = FCM_19_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_19_setting_seamless),
		.hdr_mode = HDR_RAW_LBMF,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2002000000,
		.linelength = 20320,
		.framelength = 3279 * 2,
		.max_framerate = 150,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 2560,
		.read_margin = (3279 - 2560),
		.imgsensor_winsize_info = { /* 4X crop 16cell */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080)/2,
			.y0_offset = (12288-2560)/2,
			.w0_size = 4080,
			.h0_size = 2560,
			.scale_w = 4080,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2560,
		},
		.ae_binning_ratio = 2000,
		.fine_integ_line = 360,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.coarse_integ_step = 16,
		.framelength_step = 8,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_4X4, /* hexdeca */
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.awb_enabled = 1,
		.exposure_order_in_lbmf = IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
		.mode_type_in_lbmf = IMGSENSOR_LBMF_MODE_MANUAL,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 0xFFFF,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 0xFFFF,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = (0xFFFF - 74),
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = (0xFFFF - 74),
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 16,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize,
	},

/* cus16 -> FCM_28 */
/* 11_Volcano3_HP3_Fdsum_12.5Mp_60FPS_4080x2296_direct.sset */
	{
		.frame_desc = frame_desc_FCM_28,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_28),
		.mode_setting_table = FCM_28_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_28_setting),
		.seamless_switch_group = 3,
		.seamless_switch_mode_setting_table = FCM_28_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_28_setting_seamless),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 1759333333,
		.linelength = 11680,
		.framelength = 2500, // 2507,
		.max_framerate = 600,
		.mipi_pixel_rate = 2359344000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* 2X crop 4cell */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080*2)/2,
			.y0_offset = (12288-2296*2)/2,
			.w0_size = 4080*2,
			.h0_size = 2296*2,
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
		.ae_binning_ratio = 2000,
		.fine_integ_line = 316,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_2X2, /* tetra */
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.awb_enabled = 1,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize_fdsum,
	},

/* cus17 -> FCM_1 */
/* 01_Volcano_HP3_Full_200Mp_7.5FPS_16320x12288_direct.sset */
	{
		.frame_desc = frame_desc_FCM_1,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_1),
		.mode_setting_table = FCM_1_setting_normal,
		.mode_setting_len = ARRAY_SIZE(FCM_1_setting_normal),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2002000000,
		.linelength = 21136,
		.framelength = 12603,
		.max_framerate = 75,
		.mipi_pixel_rate = 1842285714,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 16320,
			.h0_size = 12288,
			.scale_w = 16320,
			.scale_h = 12288,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 16320,
			.h1_size = 12288,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 16320,
			.h2_tg_size = 12288,
		},
		.ae_binning_ratio = 2000,
		.fine_integ_line = 360,
		.delay_frame = 2,
		.min_exposure_line = 16,
		.coarse_integ_step = 8,
		.framelength_step = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min =  16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 16,//FULL
		.sensor_output_dataformat_cell_type = SENSOR_OUTPUT_FORMAT_CELL_4X4, /* hexdeca */
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
		.awb_enabled = 1,
		.exposure_margin = 164, /* full:164, 4Sum:55, 8Sum_2H1V:34 */
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_fullsize,
	},

	/* cus18 -> FCM_29 (iDCG1:8) */
/* 26_Volcano1_HP3_8Fdsum_2H1V_12.5Mp_iDCG_60FPS_4080x2616_direct.sset */
	{
		.frame_desc = frame_desc_FCM_29,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_29),
		.mode_setting_table = FCM_29_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_29_setting),
		.seamless_switch_group = 3,
		.seamless_switch_mode_setting_table = FCM_29_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_29_setting_seamless),
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2002000000,
		.linelength = 12016,
		.framelength = 2774,
		.max_framerate = 600,
		.mipi_pixel_rate = 2359344000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* 16:10 BIN */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080*4)/2,
			.y0_offset = (12288-2560*4)/2,
			.w0_size = 4080*4,
			.h0_size = 2560*4,
			.scale_w = 4080,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2560,
		},
		.ae_binning_ratio = 16000,
		.fine_integ_line = 500,
		.delay_frame = 2,
		.min_exposure_line = 8,
		.csi_param = {0},
		.saturation_info = &imgsensor_saturation_info_10bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_base = IMGSENSOR_DCG_GAIN_LCG_BASE,
			.dcg_gain_ratio_min = 8000,
			.dcg_gain_ratio_max = 8000,
			.dcg_gain_ratio_step = 0,
		},
		.coarse_integ_step = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min =  16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min =  16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 8,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 1,
	},

	/* cus19 -> FCM_30 (iDCG1:4) */
/* 26_Volcano5_HP3_8Fdsum_2H1V_12.5Mp_iDCG_60FPS_4080x2616_direct.sset */
	{
		.frame_desc = frame_desc_FCM_30,
		.num_entries = ARRAY_SIZE(frame_desc_FCM_30),
		.mode_setting_table = FCM_30_setting,
		.mode_setting_len = ARRAY_SIZE(FCM_30_setting),
		.seamless_switch_group = 3,
		.seamless_switch_mode_setting_table = FCM_30_setting_seamless,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(FCM_30_setting_seamless),
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2002000000,
		.linelength = 12016,
		.framelength = 2774,
		.max_framerate = 600,
		.mipi_pixel_rate = 2359344000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = { /* 16:10 BIN */
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = (16320-4080*4)/2,
			.y0_offset = (12288-2560*4)/2,
			.w0_size = 4080*4,
			.h0_size = 2560*4,
			.scale_w = 4080,
			.scale_h = 2560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 2560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2560,
		},
		.ae_binning_ratio = 16000,
		.fine_integ_line = 500,
		.delay_frame = 2,
		.min_exposure_line = 8,
		.csi_param = {0},
		.saturation_info = &imgsensor_saturation_info_10bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_base = IMGSENSOR_DCG_GAIN_LCG_BASE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
		},
		.coarse_integ_step = 2,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min =  16,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min =  16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 1,
	},
};

struct mtk_sensor_ctle_param static_ctle_param = {
	.eq_latch_en = true,
	.eq_dg1_en = true,
	.eq_dg0_en = true,
	.eq_is = 0x02,
	.eq_bw = 0x03,
	.cdr_delay = 14,
	.eq_offset = -3,
	.eq_sr0 = 0x7,
};


#ifdef S5KHP9SP_ISF_DBG
struct subdrv_static_ctx s5khp9sp_legacy_s_ctx = {
#else
static struct subdrv_static_ctx static_ctx = {
#endif
	.sensor_id = S5KHP9SP_SENSOR_ID,
	.reg_addr_sensor_id = { 0x0000, 0x0001 },
	.i2c_addr_table = {0x20, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_16,
	.eeprom_info = PARAM_UNDEFINED,
	.eeprom_num = PARAM_UNDEFINED,
	.mirror = IMAGE_V_MIRROR,

	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 64,
	.ana_gain_type = 2,
	.ana_gain_step = 1,
	.ana_gain_table = s5khp9sp_ana_gain_table,
	.ana_gain_table_size = sizeof(s5khp9sp_ana_gain_table),
	.tuning_iso_base = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 6,
	.exposure_max = (0xFFFF - 74),
	.exposure_step = 1,
	.exposure_margin = 34,
	.dig_gain_min = BASE_DGAIN * 1,
	.dig_gain_max = BASE_DGAIN * 16,
	.dig_gain_step = 4,
	.frame_length_max = 0xFFFF,
	/* .frame_length_max_without_lshift = 0xFFFF, */
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 3000000,

	/* stagger behavior by vendor type */
	.stagger_rg_order = IMGSENSOR_STAGGER_RG_SE_FIRST,
	.stagger_fl_type = IMGSENSOR_STAGGER_FL_MANUAL,

	.pdaf_type = PDAF_SUPPORT_CAMSV_QPD,
	.hdr_type = HDR_SUPPORT_STAGGER_DOL,
	.seamless_switch_support = TRUE,
	.temperature_support = TRUE,
	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {
		{ 0x0202, 0x0203 },
		{ 0x022C, 0x022D },
		{ 0x0226, 0x0227 },
	},
	.long_exposure_support = TRUE,
	.reg_addr_exposure_lshift = 0x0704,
	.reg_addr_frame_length_lshift = 0x0702,
	/* .fll_lshift_max = 7, */
	/* .cit_lshift_max = 7, */

	.reg_addr_ana_gain = {
		{ 0x0204, 0x0205 },
		{ 0x0208, 0x0209 },
		{ 0x0206, 0x0207 },
	},

	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_temp_en = PARAM_UNDEFINED,
	.reg_addr_temp_read = 0x0020,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = 0x0005,

	.reg_addr_exposure_in_lut = {
			{0x0E10, 0x0E11}, /* LUT_A_COARSE_INTEG_TIME */
			{0x0E1E, 0x0E1F}, /* LUT_B_COARSE_INTEG_TIME */
	},
	.reg_addr_ana_gain_in_lut = {
			{0x0E12, 0x0E13}, /* LUT_A_ANA_GAIN_GLOBAL */
			{0x0E20, 0x0E21}, /* LUT_B_ANA_GAIN_GLOBAL */
	},
	.reg_addr_dig_gain_in_lut = {
			{0x0E14, 0x0E15},
			{0x0E22, 0x0E23},
	},
	.reg_addr_frame_length_in_lut = {
			{0x0E16, 0x0E17},
			{0x0E24, 0x0E25},
	},


	.init_setting_table = s5khp9sp_init_setting,
	.init_setting_len = ARRAY_SIZE(s5khp9sp_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 1,
	.chk_s_off_end = 0,

	.checksum_value = 0xc88841a,
	.ctle_param = &static_ctle_param,
	.i3c_precfg_setting_table = hp9_i3c_init_global,
	.i3c_precfg_setting_len = ARRAY_SIZE(hp9_i3c_init_global),
};

static struct subdrv_ops ops = {
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
	.update_sof_cnt = common_update_sof_cnt,
	.vsync_notify = vsync_notify,
	.i3c_pre_config = s5khp9sp_i3c_pre_config,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST, {0}, 0},
	{HW_ID_DOVDD, {1800000, 1800000}, 1000},
	{HW_ID_AVDD, {2200000, 2200000}, 1000},
	{HW_ID_DVDD, {900000, 900000}, 1000},
	{HW_ID_OISVDD, {3250000, 3250000}, 5000},
	{HW_ID_AFVDD, {2800000, 2800000}, 1000},
	{HW_ID_RST, {1}, 5000},
	{HW_ID_MCLK_DRIVING_CURRENT, {6}, 1000},
	{HW_ID_MCLK, {26}, 10000},
};


#ifdef S5KHP9SP_ISF_DBG
const struct subdrv_entry s5khp9sp_mipi_raw_entry_legacy = {
#else
const struct subdrv_entry s5khp9sp_mipi_raw_entry = {
#endif
	.name = "s5khp9sp_mipi_raw",
	.id = S5KHP9SP_SENSOR_ID, /* 0x1b73 */
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

/* FUNCTION */
static int get_sensor_temperature(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u16 temperature = 0;
	int temperature_convert = 0;

	temperature = subdrv_ixc_rd_u16(ctx, ctx->s_ctx.reg_addr_temp_read);

	temperature_convert = (temperature>>8)&0xFF;

	temperature_convert = (temperature_convert > 100) ? 100: temperature_convert;

	DRV_LOG(ctx, "temperature: %d degrees\n", temperature_convert);
	return temperature_convert;
}

static void seamless_switch_update_hw_re_init_time(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM pre_seamless_scenario_id,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id)
{
	if (ctx->s_ctx.mode[pre_seamless_scenario_id].pclk != ctx->s_ctx.mode[scenario_id].pclk)
		ctx->s_ctx.seamless_switch_hw_re_init_time_ns = 16000000;
	else
		ctx->s_ctx.seamless_switch_hw_re_init_time_ns = 4000000;

	DRV_LOG_MUST(ctx,
			"[%s] scen:%u(pclk:%llu) => scen:%u(pclk:%llu), hw_re_init_time_ns:%u\n",
			__func__,
			pre_seamless_scenario_id,
			ctx->s_ctx.mode[pre_seamless_scenario_id].pclk,
			scenario_id,
			ctx->s_ctx.mode[scenario_id].pclk,
			ctx->s_ctx.seamless_switch_hw_re_init_time_ns);
}


static int s5khp9sp_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	struct mtk_hdr_ae *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;
	u32 frame_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 exp_cnt = 0;
	enum SENSOR_SCENARIO_ID_ENUM pre_seamless_scenario_id = ctx->current_scenario_id;

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

	subdrv_ixc_wr_u8(ctx, 0x0104, 0x01);

	if (ctx->s_ctx.reg_addr_fast_mode_in_lbmf &&
		(ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_LBMF ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF))
		subdrv_ixc_wr_u8(ctx, ctx->s_ctx.reg_addr_fast_mode_in_lbmf, 0x4);

	update_mode_info(ctx, scenario_id);
	ixc_table_write(ctx,
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
		case HDR_RAW_DCG_RAW:
			set_multi_shutter_frame_length(ctx, (u64 *)&ae_ctrl->exposure, 1, 0);
			if (ctx->s_ctx.mode[scenario_id].dcg_info.dcg_gain_mode
				== IMGSENSOR_DCG_DIRECT_MODE)
				set_multi_gain(ctx, (u32 *)&ae_ctrl->gain, exp_cnt);
			else
				set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		default:
			set_multi_shutter_frame_length(ctx, (u64 *)&ae_ctrl->exposure, 1, 0);
			set_gain(ctx, ae_ctrl->gain.le_gain);
			break;
		}
	}
	subdrv_ixc_wr_u8(ctx, 0x0104, 0x00);

	ctx->fast_mode_on = TRUE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->is_seamless = FALSE;
	seamless_switch_update_hw_re_init_time(ctx, pre_seamless_scenario_id, scenario_id);
	DRV_LOG(ctx, "X: set seamless switch done\n");
	return ERROR_NONE;
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
	return gain * 32 / BASEGAIN;
}

static int s5khp9sp_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	if (mode)
		subdrv_ixc_wr_u16(ctx, 0x0600, mode); /*100% Color bar*/
	else if (ctx->test_pattern)
		subdrv_ixc_wr_u16(ctx, 0x0600, 0x0000); /*No pattern*/

	ctx->test_pattern = mode;
	return ERROR_NONE;
}

static int s5khp9sp_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	struct mtk_test_pattern_data *data = (struct mtk_test_pattern_data *)para;
	u16 R = (data->Channel_R >> 22) & 0x3ff;
	u16 Gr = (data->Channel_Gr >> 22) & 0x3ff;
	u16 Gb = (data->Channel_Gb >> 22) & 0x3ff;
	u16 B = (data->Channel_B >> 22) & 0x3ff;

	subdrv_ixc_wr_u16(ctx, 0x0602, R);
	subdrv_ixc_wr_u16(ctx, 0x0604, Gr);
	subdrv_ixc_wr_u16(ctx, 0x0606, B);
	subdrv_ixc_wr_u16(ctx, 0x0608, Gb);

	DRV_LOG(ctx, "mode(%u) R/Gr/Gb/B = 0x%04x/0x%04x/0x%04x/0x%04x\n",
		ctx->test_pattern, R, Gr, Gb, B);
	return ERROR_NONE;
}


static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id)
{
#ifdef S5KHP9SP_ISF_DBG
	memcpy(&(ctx->s_ctx), &s5khp9sp_legacy_s_ctx, sizeof(struct subdrv_static_ctx));
#else
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
#endif
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;

	return 0;
}

static void s5khp9sp_sensor_init(struct subdrv_ctx *ctx)
{
	u64 time_boot_begin = 0;
	u64 ixc_time = 0;

	DRV_LOG(ctx, " start\n");

	/* write init setting */
	if (ctx->s_ctx.init_setting_table != NULL) {
		DRV_LOG(ctx, "S: size:%u\n", ctx->s_ctx.init_setting_len);
		if ((ctx->power_on_profile_en != NULL) &&
			(*ctx->power_on_profile_en))
			time_boot_begin = ktime_get_boottime_ns();

		ixc_time = ixc_table_write(ctx, ctx->s_ctx.init_setting_table, ctx->s_ctx.init_setting_len);

		if ((ctx->power_on_profile_en != NULL) &&
			(*ctx->power_on_profile_en)) {
			ctx->sensor_pw_on_profile.i2c_init_period =
				ktime_get_boottime_ns() - time_boot_begin;

			 ctx->sensor_pw_on_profile.i2c_init_table_len =
							ctx->s_ctx.init_setting_len;
		}
		DRV_LOG_MUST(ctx, "X: size:%u, time(us):%lld\n", ctx->s_ctx.init_setting_len,
			ixc_time);
	} else {
		DRV_LOG_MUST(ctx, "please implement initial setting!\n");
	}
	/* enable temperature sensor */
#if IMGSENSOR_AOV_EINT_UT
#else
	if (ctx->s_ctx.temperature_support && ctx->s_ctx.reg_addr_temp_en)
		subdrv_ixc_wr_u8(ctx, ctx->s_ctx.reg_addr_temp_en, 0x01);
	/* enable mirror or flip */
	set_mirror_flip(ctx, ctx->mirror);
#endif

	DRV_LOG(ctx, " end\n");
}


static int open(struct subdrv_ctx *ctx)
{
	u32 sensor_id = 0;
	u32 scenario_id = 0;
	u32 linetime_in_ns = 0;

	/* get sensor id */
	if (common_get_imgsensor_id(ctx, &sensor_id) != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail setting */
	if (ctx->s_ctx.aov_sensor_support && !ctx->s_ctx.init_in_open)
		DRV_LOG_MUST(ctx, "sensor init not in _open stage!\n");
	else
		s5khp9sp_sensor_init(ctx);

	if (ctx->s_ctx.s_cali != NULL)
		ctx->s_ctx.s_cali((void *) ctx);
	else
		write_sensor_Cali(ctx);

	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	memset(ctx->ana_gain, 0, sizeof(ctx->gain));
	ctx->exposure[0] = ctx->s_ctx.exposure_def;
	ctx->ana_gain[0] = ctx->s_ctx.ana_gain_def;
	ctx->current_scenario_id = scenario_id;
	ctx->pclk = ctx->s_ctx.mode[scenario_id].pclk;
	ctx->line_length = ctx->s_ctx.mode[scenario_id].linelength;
	ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;
	ctx->frame_length_rg = ctx->frame_length;

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			ctx->current_scenario_id, &linetime_in_ns, 0, 0);
		ctx->current_fps = 1000000000 / linetime_in_ns * 10 / ctx->frame_length;
	} else {
		ctx->current_fps = ctx->pclk / ctx->line_length * 10 / ctx->frame_length;
	}

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
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF) {
		memset(ctx->frame_length_in_lut, 0,
			sizeof(ctx->frame_length_in_lut));

		switch (ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt) {
		case 2:
			ctx->frame_length_in_lut[0] = ctx->readout_length + ctx->read_margin;
			ctx->frame_length_in_lut[1] = ctx->frame_length -
				ctx->frame_length_in_lut[0];
			break;
		case 3:
			ctx->frame_length_in_lut[0] = ctx->readout_length + ctx->read_margin;
			ctx->frame_length_in_lut[1] = ctx->readout_length + ctx->read_margin;
			ctx->frame_length_in_lut[2] = ctx->frame_length -
				ctx->frame_length_in_lut[1] - ctx->frame_length_in_lut[0];
			break;
		default:
			break;
		}

		memcpy(ctx->frame_length_in_lut_rg, ctx->frame_length_in_lut,
			sizeof(ctx->frame_length_in_lut_rg));
	}

	return ERROR_NONE;
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

static int s5khp9sp_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	struct SET_SENSOR_AWB_GAIN *awb_gain = (struct SET_SENSOR_AWB_GAIN *)para;

	subdrv_ixc_wr_u16(ctx, 0x0D82,
		awb_gain->ABS_GAIN_R * 2); /* red 1024(1x) */
	subdrv_ixc_wr_u16(ctx, 0x0D86,
		awb_gain->ABS_GAIN_B * 2); /* blue */

	DRV_LOG(ctx, "[test] ABS_GAIN_GR(%d) ABS_GAIN_R(%d) ABS_GAIN_B(%d) ABS_GAIN_GB(%d)",
			awb_gain->ABS_GAIN_GR,
			awb_gain->ABS_GAIN_R,
			awb_gain->ABS_GAIN_B,
			awb_gain->ABS_GAIN_GB);
	DRV_LOG(ctx, "[test] 0x0D82(red) = (0x%x)", subdrv_ixc_rd_u16(ctx, 0x0D82));
	DRV_LOG(ctx, "[test] 0x0D84(green) = (0x%x)", subdrv_ixc_rd_u16(ctx, 0x0D84));
	DRV_LOG(ctx, "[test] 0x0D86(blue) = (0x%x)", subdrv_ixc_rd_u16(ctx, 0x0D86));
	return 0;
}

static int s5khp9sp_i3c_pre_config(struct subdrv_ctx *ctx)
{
	int ret = 0;

	if ((ctx->ixc_client.protocol == I3C_PROTOCOL)
		&& (ctx->i2c_vir_client.i2c_dev)) {
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0xFCFC, 0x4000);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x0000, 0x0001);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x0000, 0x1B73);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x6012, 0x0001);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x7002, 0x1008);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x6066, 0x0216);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x6068, 0x1B73);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x6014, 0x0001);
		ret |= adaptor_ixc_wr_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				0x6094, 0x0000);
		if (ret) {
			DRV_LOGE(ctx, "fail. ret=%d\n", ret);
			return ERROR_SENSOR_CONNECT_FAIL;
		}
		DRV_LOG_MUST(ctx, "success. ret=%d, setting_len: %d\n",
				ret, ctx->s_ctx.i3c_precfg_setting_len);
		mdelay(20);
	}
	return ERROR_NONE;
} /* pre_config */
