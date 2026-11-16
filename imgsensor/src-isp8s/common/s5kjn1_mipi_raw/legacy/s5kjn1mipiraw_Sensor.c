// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

/********************************************************************
 *
 * Filename:
 * ---------
 *	 s5kjn1mipiraw_Sensor.c
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
#include "s5kjn1mipiraw_Sensor.h"

static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static int s5kjn1_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int s5kjn1_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static void s5kjn1_sensor_init(struct subdrv_ctx *ctx);
static int open(struct subdrv_ctx *ctx);

/* STRUCT */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, s5kjn1_set_test_pattern},
	{SENSOR_FEATURE_SET_TEST_PATTERN_DATA, s5kjn1_set_test_pattern_data},
};

#define pd_i4Crop { \
	/* pre cap normal_video hs_video slim_video */\
	{0, 6}, {0, 6}, {0, 261}, {120, 456}, {0, 0},\
	/* cust1 cust2 cust3 cust4 cust5 */\
	{0, 0}, {204, 156}, {204, 258}, {204, 258}, {0, 0},\
}

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 8,
	.i4OffsetY = 2,
	.i4PitchX = 8,
	.i4PitchY = 8,
	.i4PairNum = 4,
	.i4SubBlkW = 8,
	.i4SubBlkH = 2,
	.i4PosL = {
		{9, 2}, {11, 5}, {15, 6}, {13, 9}
	},
	.i4PosR = {
		{8, 2}, {10, 5}, {14, 6}, {12, 9}
	},
	.i4BlockNumX = 508,
	.i4BlockNumY = 382,
	.i4Crop = pd_i4Crop,

	.iMirrorFlip = 0,

	.i4FullRawW = 4080,
	.i4FullRawH = 3072,
	.i4ModeIndex = 0,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		.i4PDPattern = 3,
		.i4PDRepetition = 2,
		.i4PDOrder = {1, 0},
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_2bin = {
	.i4OffsetX = 4,
	.i4OffsetY = 2,
	.i4PitchX = 4,
	.i4PitchY = 4,
	.i4PairNum = 1,
	.i4SubBlkW = 4,
	.i4SubBlkH = 4,
	.i4PosL = {
		{4, 2},
	},
	.i4PosR = {
		{5, 2},
	},
	.i4BlockNumX = 496,
	.i4BlockNumY = 382,
	.i4Crop = pd_i4Crop,

	.iMirrorFlip = 0,

	.i4FullRawW = 2040,
	.i4FullRawH = 1536,
	.i4ModeIndex = 0,
	/* VC's PD pattern description */
	.sPDMapInfo[0] = {
		.i4PDPattern = 3,
		.i4PDRepetition = 2,
		.i4PDOrder = {1, 0},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0FF0, //4080
			.vsize = 0x0BF4, //3060
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x01FC, //508
			.vsize = 0x0BF0, //3056
			.user_data_desc = VC_PDAF_STATS,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0FF0, //4080
			.vsize = 0x0BF4, //3060
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x01FC, //508
			.vsize = 0x0BF0, //3056
			.user_data_desc = VC_PDAF_STATS,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0FF0, //4080
			.vsize = 0x09F6, //2550
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x01FC, //508
			.vsize = 0x09E0, //2528
			.user_data_desc = VC_PDAF_STATS,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0F00, //3840
			.vsize = 0x0870, //2160
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x01E0, //480
			.vsize = 0x0860, //2144
			.user_data_desc = VC_PDAF_STATS,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0780, //1920
			.vsize = 0x0438, //1080
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0780, //1920
			.vsize = 0x0438, //1080
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0660, //1632
			.vsize = 0x04C8, //1224
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0660, //1632
			.vsize = 0x03FC, //1020
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0660, //1632
			.vsize = 0x03FC, //1020
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0280, //640
			.vsize = 0x01E0, //480
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus6[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0280, //640
			.vsize = 0x01E0, //480
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus7[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0280, //640
			.vsize = 0x01E0, //480
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus8[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0280, //640
			.vsize = 0x01E0, //480
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus9[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0140, //320
			.vsize = 0x00F0, //240
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus10[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0140, //320
			.vsize = 0x00F0, //240
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};

static struct subdrv_mode_struct mode_struct[] = {
	/*mode 0: 4080x3060 30fps 1656Mbps/lane*/
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = s5kjn1_preview_setting,
		.mode_setting_len = ARRAY_SIZE(s5kjn1_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 4584,
		.framelength = 4054,
		.max_framerate = 300,
		.mipi_pixel_rate = 662400000, //DPHY: 1656M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 3,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 12,
			.w0_size = 8160,
			.h0_size = 6120,
			.scale_w = 4080,
			.scale_h = 3060,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.aov_mode = 0,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.s_dummy_support = 1,
		.ae_ctrl_support = 1,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	/*mode 1: 4080x3060 30fps 1656Mbps/lane*/
	{
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = s5kjn1_capture_setting,
		.mode_setting_len = ARRAY_SIZE(s5kjn1_capture_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 4584,
		.framelength = 4054,
		.max_framerate = 300,
		.mipi_pixel_rate = 662400000, //DPHY: 1656M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 3,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 12,
			.w0_size = 8160,
			.h0_size = 6120,
			.scale_w = 4080,
			.scale_h = 3060,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.aov_mode = 0,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.s_dummy_support = 1,
		.ae_ctrl_support = 1,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	/*mode 2: 4080x2550 30fps 1656Mbps/lane*/
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = s5kjn1_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(s5kjn1_normal_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 4584,
		.framelength = 4054,
		.max_framerate = 301,
		.mipi_pixel_rate = 662400000, //DPHY: 1656M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 3,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 520,
			.w0_size = 8160,
			.h0_size = 5104,
			.scale_w = 4080,
			.scale_h = 2552,
			.x1_offset = 0,
			.y1_offset = 1,
			.w1_size = 4080,
			.h1_size = 2550,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2550,
		},
		.aov_mode = 0,
		.pdaf_cap = TRUE,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.s_dummy_support = 1,
		.ae_ctrl_support = 1,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	/*mode 3: 3840x2160 60fps 1752Mbps/lane*/
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = s5kjn1_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(s5kjn1_hs_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 560000000,
		.linelength = 4096,
		.framelength = 2274,
		.max_framerate = 600,
		.mipi_pixel_rate = 700800000, //DPHY: 1752M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
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
		.aov_mode = 0,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.s_dummy_support = 1,
		.ae_ctrl_support = 1,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	/*mode 4: AOV 1920x1080 30fps 1534Mbps/lane*/
	{
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table =  s5kjn1_slim_video_setting,
		.mode_setting_len = ARRAY_SIZE(s5kjn1_slim_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 598000000,
		.linelength = 7808,
		.framelength = 1464,
		.max_framerate = 300,
		.mipi_pixel_rate = 613600000, //DPHY: 1534M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
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
		.aov_mode = 1,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.s_dummy_support = 0, //aov no support
		.ae_ctrl_support = IMGSENSOR_AE_CONTROL_SUPPORT_SENSING_MODE,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	/*mode 5: AOV 1920x1080 20fps 1534Mbps/lane*/
	{
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table =  s5kjn1_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(s5kjn1_custom1_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 598000000,
		.linelength = 7808,
		.framelength = 3808,
		.max_framerate = 200,
		.mipi_pixel_rate = 613600000, //DPHY: 1534M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
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
		.aov_mode = 1,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.s_dummy_support = 0, //aov no support
		.ae_ctrl_support = IMGSENSOR_AE_CONTROL_SUPPORT_SENSING_MODE,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	/*mode 6: 1632x1224 30fps 1656Mbps/lane*/
	{
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table =  s5kjn1_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(s5kjn1_custom2_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 600000000,
		.linelength = 2048,
		.framelength = 9760,
		.max_framerate = 300,
		.mipi_pixel_rate = 662400000, //DPHY: 1656M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.coarse_integ_step = 2,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 816, //(976+(8191-7535))/2
			.y0_offset = 608, //(592+(6175-5551))/2
			.w0_size = 6528,
			.h0_size = 4928,
			.scale_w = 1632,
			.scale_h = 1232,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 1632,
			.h1_size = 1224,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1632,
			.h2_tg_size = 1224,
		},
		.aov_mode = 0,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_2bin,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.s_dummy_support = 1,
		.ae_ctrl_support = 1,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	/*mode 7: 1632x1020 30fps 1656Mbps/lane*/
	{
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table =  s5kjn1_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(s5kjn1_custom3_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 600000000,
		.linelength = 2048,
		.framelength = 9760,
		.max_framerate = 300,
		.mipi_pixel_rate = 662400000, //DPHY: 1656M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.coarse_integ_step = 2,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 816, //(976+(8191-7535))/2
			.y0_offset = 1016, //(1000+(6175-5143))/2
			.w0_size = 6528,
			.h0_size = 4112,
			.scale_w = 1632,
			.scale_h = 1028,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 1632,
			.h1_size = 1020,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1632,
			.h2_tg_size = 1020,
		},
		.aov_mode = 0,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_2bin,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.s_dummy_support = 1,
		.ae_ctrl_support = 1,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	/*mode 8: 1632x1020 60fps 1656Mbps/lane*/
	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table =  s5kjn1_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(s5kjn1_custom4_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 600000000,
		.linelength = 2048,
		.framelength = 4880,
		.max_framerate = 600,
		.mipi_pixel_rate = 662400000, //DPHY: 1656M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.coarse_integ_step = 2,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 816, //(976+(8191-7535))/2
			.y0_offset = 1016, //(1000+(6175-5143))/2
			.w0_size = 6528,
			.h0_size = 4112,
			.scale_w = 1632,
			.scale_h = 1028,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 1632,
			.h1_size = 1020,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1632,
			.h2_tg_size = 1020,
		},
		.aov_mode = 0,
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_2bin,
		.ae_binning_ratio = 4000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.s_dummy_support = 1,
		.ae_ctrl_support = 1,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	/*mode 9: AOV 640x480 30fps 1534Mbps/lane*/
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table =  s5kjn1_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(s5kjn1_custom5_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 598000000,
		.linelength = 2640,
		.framelength = 7536,
		.max_framerate = 300,
		.mipi_pixel_rate = 613600000, //DPHY: 1534M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 2800,
			.y0_offset = 2112,
			.w0_size = 2560,
			.h0_size = 1920,
			.scale_w = 640,
			.scale_h = 480,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 640,
			.h1_size = 480,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 640,
			.h2_tg_size = 480,
		},
		.aov_mode = 1,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.s_dummy_support = 0, //aov no support
		.ae_ctrl_support = IMGSENSOR_AE_CONTROL_SUPPORT_SENSING_MODE,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	/*mode 10: AOV640x480 20fps 1534Mbps/lane*/
	{
		.frame_desc = frame_desc_cus6,
		.num_entries = ARRAY_SIZE(frame_desc_cus6),
		.mode_setting_table =  s5kjn1_custom6_setting,
		.mode_setting_len = ARRAY_SIZE(s5kjn1_custom6_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 598000000,
		.linelength = 2640,
		.framelength = 11264,
		.max_framerate = 200,
		.mipi_pixel_rate = 613600000, //DPHY: 1534M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 2800,
			.y0_offset = 2112,
			.w0_size = 2560,
			.h0_size = 1920,
			.scale_w = 640,
			.scale_h = 480,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 640,
			.h1_size = 480,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 640,
			.h2_tg_size = 480,
		},
		.aov_mode = 1,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.s_dummy_support = 0, //aov no support
		.ae_ctrl_support = IMGSENSOR_AE_CONTROL_SUPPORT_SENSING_MODE,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	/*mode 11: AOV 640x480 10fps 1534Mbps/lane*/
	{
		.frame_desc = frame_desc_cus7,
		.num_entries = ARRAY_SIZE(frame_desc_cus7),
		.mode_setting_table =  s5kjn1_custom7_setting,
		.mode_setting_len = ARRAY_SIZE(s5kjn1_custom7_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 598000000,
		.linelength = 2640,
		.framelength = 22528,
		.max_framerate = 100,
		.mipi_pixel_rate = 613600000, //DPHY: 1534M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 2800,
			.y0_offset = 2112,
			.w0_size = 2560,
			.h0_size = 1920,
			.scale_w = 640,
			.scale_h = 480,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 640,
			.h1_size = 480,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 640,
			.h2_tg_size = 480,
		},
		.aov_mode = 1,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.s_dummy_support = 0, //aov no support
		.ae_ctrl_support = IMGSENSOR_AE_CONTROL_SUPPORT_SENSING_MODE,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	/*mode 12: AOV 640x480 5fps 1534Mbps/lane*/
	{
		.frame_desc = frame_desc_cus8,
		.num_entries = ARRAY_SIZE(frame_desc_cus8),
		.mode_setting_table =  s5kjn1_custom8_setting,
		.mode_setting_len = ARRAY_SIZE(s5kjn1_custom8_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 598000000,
		.linelength = 2640,
		.framelength = 45056,
		.max_framerate = 50,
		.mipi_pixel_rate = 613600000, //DPHY: 1534M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 2800,
			.y0_offset = 2112,
			.w0_size = 2560,
			.h0_size = 1920,
			.scale_w = 640,
			.scale_h = 480,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 640,
			.h1_size = 480,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 640,
			.h2_tg_size = 480,
		},
		.aov_mode = 1,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.s_dummy_support = 0, //aov no support
		.ae_ctrl_support = IMGSENSOR_AE_CONTROL_SUPPORT_SENSING_MODE,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	/*mode 13: AOV 320x240 10fps 1534Mbps/lane*/
	{
		.frame_desc = frame_desc_cus9,
		.num_entries = ARRAY_SIZE(frame_desc_cus9),
		.mode_setting_table =  s5kjn1_custom9_setting,
		.mode_setting_len = ARRAY_SIZE(s5kjn1_custom9_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 598000000,
		.linelength = 2048,
		.framelength = 29136,
		.max_framerate = 100,
		.mipi_pixel_rate = 613600000, //DPHY: 1534M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 3440,
			.y0_offset = 2592,
			.w0_size = 1280,
			.h0_size = 960,
			.scale_w = 320,
			.scale_h = 240,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 320,
			.h1_size = 240,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 320,
			.h2_tg_size = 240,
		},
		.aov_mode = 1,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.s_dummy_support = 0, //aov no support
		.ae_ctrl_support = IMGSENSOR_AE_CONTROL_SUPPORT_SENSING_MODE,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	/*mode 14: AOV 320x240 5fps 1534Mbps/lane*/
	{
		.frame_desc = frame_desc_cus10,
		.num_entries = ARRAY_SIZE(frame_desc_cus10),
		.mode_setting_table =  s5kjn1_custom10_setting,
		.mode_setting_len = ARRAY_SIZE(s5kjn1_custom10_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 598000000,
		.linelength = 2048,
		.framelength = 58272,
		.max_framerate = 50,
		.mipi_pixel_rate = 613600000, //DPHY: 1534M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8160,
			.full_h = 6144,
			.x0_offset = 3440,
			.y0_offset = 2592,
			.w0_size = 1280,
			.h0_size = 960,
			.scale_w = 320,
			.scale_h = 240,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 320,
			.h1_size = 240,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 320,
			.h2_tg_size = 240,
		},
		.aov_mode = 1,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.s_dummy_support = 0, //aov no support
		.ae_ctrl_support = IMGSENSOR_AE_CONTROL_SUPPORT_SENSING_MODE,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
};
#ifdef S5KJN1_ISF_DBG
struct subdrv_static_ctx s5kjn1_legacy_s_ctx = {
#else
static struct subdrv_static_ctx static_ctx = {
#endif
	.sensor_id = S5KJN1_SENSOR_ID,
	.reg_addr_sensor_id = {0x0000, 0x0001},
	.i2c_addr_table = {0x5A, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_16,
	.eeprom_info = PARAM_UNDEFINED,
	.eeprom_num = PARAM_UNDEFINED,
	.resolution = {8160, 6144},
	.mirror = IMAGE_NORMAL,

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 16,
	.ana_gain_type = 2,
	.ana_gain_step = 32,
	.ana_gain_table = s5kjn1_ana_gain_table,
	.ana_gain_table_size = sizeof(s5kjn1_ana_gain_table),
	.tuning_iso_base = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 4,
	.exposure_max = (0xFFFF - 10) << 7,
	.exposure_step = 1,
	.exposure_margin = 10,

	.frame_length_max = 0xFFFF << 7,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 7389333, //11572000 , PD2241H-[B230408-1259]

	.pdaf_type = PDAF_SUPPORT_CAMSV,
	.hdr_type = HDR_SUPPORT_NA,
	.seamless_switch_support = FALSE,
	.temperature_support = TRUE,
	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {{0x0202, 0x0203},},
	.long_exposure_support = TRUE,
	.reg_addr_exposure_lshift = 0x0704,

	/*when setting a long exp and no autoextend FL, extend frame length related member*/
	 .reg_addr_frame_length_lshift = 0x0702,
	 .fll_lshift_max = 7,
	 .cit_lshift_max = 7, //default=7 max=11

	 /* stagger behavior by vendor type */
	 .stagger_rg_order = IMGSENSOR_STAGGER_RG_SE_FIRST,
	 .stagger_fl_type = IMGSENSOR_STAGGER_FL_MANUAL,

	.reg_addr_ana_gain = {{0x0204, 0x0205},},
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_temp_en = PARAM_UNDEFINED,
	.reg_addr_temp_read = 0x0020,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = 0x0005,

	.init_setting_table = s5kjn1_init_setting,
	.init_setting_len = ARRAY_SIZE(s5kjn1_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 1,
	.chk_s_off_end = 0,
	.checksum_value = 0xef68f1f2,
	.aov_sensor_support = TRUE,
	.init_in_open = TRUE,
	.streaming_ctrl_imp = FALSE,
	/* custom stream control to mipi delay time for hw limitation */
	.custom_stream_ctrl_delay = TRUE,
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
	.get_temp = common_get_temp,
	.get_csi_param = common_get_csi_param,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST, {0}, 1000},
	{HW_ID_DOVDD, {1800000, 1800000}, 1000},
	{HW_ID_DVDD, {1800000, 1800000}, 1000},
	{HW_ID_AVDD, {2800000, 2800000}, 1000},
	{HW_ID_AFVDD, {2800000, 2800000}, 1000},
	{HW_ID_RST, {1}, 2000},
	{HW_ID_MCLK_DRIVING_CURRENT, {2}, 0},
	{HW_ID_MCLK, {24}, 5000},
};

static struct subdrv_pw_seq_entry aov_pw_seq[] = {
	{HW_ID_RST, {0}, 1000},
	{HW_ID_DOVDD, {1800000, 1800000}, 1000},
	{HW_ID_DVDD, {1800000, 1800000}, 1000},
	{HW_ID_AVDD, {2800000, 2800000}, 1000},
	{HW_ID_RST, {1}, 2000},
	{HW_ID_MCLK_DRIVING_CURRENT, {2}, 0},
	{HW_ID_MCLK, {26, MCLK_ULPOSC}, 5000},
};

#ifdef S5KJN1_ISF_DBG
const struct subdrv_entry s5kjn1_mipi_raw_entry_legacy = {
#else
const struct subdrv_entry s5kjn1_mipi_raw_entry = {
#endif
	.name = "s5kjn1_mipi_raw",
	.id = S5KJN1_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
	.aov_pw_seq = aov_pw_seq,
	.aov_pw_seq_cnt = ARRAY_SIZE(aov_pw_seq),
};

/* FUNCTION */
static int get_sensor_temperature(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u16 temperature = 0;
	int temperature_convert = 0;

	temperature = subdrv_i2c_rd_u16(ctx, ctx->s_ctx.reg_addr_temp_read);

	temperature_convert = (temperature>>8)&0xFF;

	temperature_convert = (temperature_convert > 100) ? 100: temperature_convert;

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
	return gain * 32 / BASEGAIN;
}

static int s5kjn1_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	if (mode)
		subdrv_i2c_wr_u16(ctx, 0x0600, mode); /*100% Color bar*/
	else if (ctx->test_pattern)
		subdrv_i2c_wr_u16(ctx, 0x0600, 0x0000); /*No pattern*/

	ctx->test_pattern = mode;

	return 0;
}

static int s5kjn1_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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

static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id)
{
#ifdef S5KJN1_ISF_DBG
	memcpy(&(ctx->s_ctx), &s5kjn1_legacy_s_ctx, sizeof(struct subdrv_static_ctx));
#else
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
#endif
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;

	return 0;
}

static void s5kjn1_sensor_init(struct subdrv_ctx *ctx)
{
	u64 time_boot_begin = 0;
	u64 ixc_time = 0;

	DRV_LOG(ctx, " start\n");
	/*Global setting */
	subdrv_i2c_wr_u16(ctx, 0x6028, 0x4000);
	subdrv_i2c_wr_u16(ctx, 0x0000, 0x0003);
	subdrv_i2c_wr_u16(ctx, 0x0000, 0x38E1);
	subdrv_i2c_wr_u16(ctx, 0x001E, 0x0007);
	subdrv_i2c_wr_u16(ctx, 0x6028, 0x4000);
	subdrv_i2c_wr_u16(ctx, 0x6010, 0x0001);
	mdelay(5);
	subdrv_i2c_wr_u16(ctx, 0x6226, 0x0001);
	mdelay(10);

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

	/* get sensor id */
	if (common_get_imgsensor_id(ctx, &sensor_id) != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail setting */
	if (ctx->s_ctx.aov_sensor_support && !ctx->s_ctx.init_in_open)
		DRV_LOG_MUST(ctx, "sensor init not in open_stage!\n");
	else
		s5kjn1_sensor_init(ctx);

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
	ctx->current_fps = ctx->pclk / ctx->line_length * 10 / ctx->frame_length;
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
} /* open */
