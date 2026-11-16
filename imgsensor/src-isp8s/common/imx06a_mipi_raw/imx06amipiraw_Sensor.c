// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 imx06amipiraw_Sensor.c
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
#include "imx06amipiraw_Sensor.h"

#define FOR_DCG_VS 1

static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static int imx06a_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int imx06a_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int imx06a_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt, u64 sof_ts);

static int imx06a_mcss_init(void *arg);
static int imx06a_mcss_set_mask_frame(struct subdrv_ctx *ctx, u32 num, u32 is_critical);

/* STRUCT */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, imx06a_set_test_pattern},
	{SENSOR_FEATURE_SET_TEST_PATTERN_DATA, imx06a_set_test_pattern_data},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, imx06a_seamless_switch},
};

#ifdef FOR_DCG_VS
/* 1000 base for dcg gain ratio */
static u32 imx06a_dcg_ratio_table_ratio16[] = {16000};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_14bit = {
	.OB_pedestal = 64,
	.adc_bit = 10,
	.ob_bm = 64,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_for_dcg_compose = {
	.OB_pedestal = 64,
	.adc_bit = 10,
	.ob_bm = 64,
	.bit_depth = 14,
	.valid_bit = 14,
	.dummy_padding = NONE_PADDING,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_for_vs = {
	.OB_pedestal = 64,
	.adc_bit = 10,
	.ob_bm = 64,
	.bit_depth = 14,
	.valid_bit = 10,
	.dummy_padding = LSB_PADDING,
};

static struct mtk_mbus_frame_desc_entry frame_desc_regA[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 0x1000, /* 4096 */
			.vsize = 0x0900, /* 2304 */
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
			.saturation_info = &imgsensor_saturation_info_for_dcg_compose,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2d,
			.hsize = 0x1000, /* 4096 */
			.vsize = 0x0900, /* 2304 */
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
			.saturation_info = &imgsensor_saturation_info_for_vs,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000, /* 4096 */
			.vsize = 0x240, /* 576 */
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_regB[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000, /* 4096 */
			.vsize = 0x0900, /* 2304 */
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000, /* 4096 */
			.vsize = 0x240, /* 576 */
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_regC[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 0x1000, /* 4096 */
			.vsize = 0x0900, /* 2304 */
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
			.valid_bit = 10,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000, /* 4096 */
			.vsize = 0x240, /* 576 */
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_regD[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000, /* 4096 */
			.vsize = 0x0900, /* 2304 */
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000, /* 4096 */
			.vsize = 0x0900, /* 2304 */
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 2,
			.data_type = 0x2b,
			.hsize = 0x1000, /* 4096 */
			.vsize = 0x0900, /* 2304 */
			.user_data_desc = VC_STAGGER_SE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000, /* 4096 */
			.vsize = 0x240, /* 576 */
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_regE[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000, /* 4096 */
			.vsize = 0x0900, /* 2304 */
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_FIRST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000, /* 4096 */
			.vsize = 0x0900, /* 2304 */
			.user_data_desc = VC_STAGGER_ME,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_LAST,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000, /* 4096 */
			.vsize = 0x240, /* 576 */
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

#else

static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = { //mode 0
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000, //4096
			.vsize = 0x0c00, //3072
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000, //4096
			.vsize = 0x0300, //768
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = { //mode 0
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000, //4096
			.vsize = 0x0c00, //3072
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000, //4096
			.vsize = 0x0300, //768
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = { //mode 2
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000, //4096
			.vsize = 0x0900, //2304
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000, //4096
			.vsize = 0x0240, //576
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = { //mode 0
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000, //4096
			.vsize = 0x0c00, //3072
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000, //4096
			.vsize = 0x0300, //768
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = { //mode 0
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000, //4096
			.vsize = 0x0c00, //3072
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000, //4096
			.vsize = 0x0300, //768
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = { //mode 1
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000, //4096
			.vsize = 0x0c00, //3072
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
			.hsize = 0x1000, //4096
			.vsize = 0x0300, //768
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x1000, //4096
			.vsize = 0x0300, //768
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = { //mode 2
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000, //4096
			.vsize = 0x0c00, //3072
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x0800, //2048
			.vsize = 0x0300, //768
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
#endif


static struct subdrv_mode_struct mode_struct[] = {
#ifndef FOR_DCG_VS
	{// mode 0, Normal Obin_4096_3072_30fps_PD_S2,
// reg12 IMX06A_MTK-CPHY-1.8V-24M_RegisterSetting_ver5.00-7.00_231221_test.xlsx,
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = imx06a_preview_setting,
		.mode_setting_len = ARRAY_SIZE(imx06a_preview_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06a_seamless_preview,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06a_seamless_preview),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 3340800000,// VTPXCK system pixel rate
		.linelength = 8832,//line_length_pck
		.framelength = 12608,//frame_length_lines
		.max_framerate = 300,
		.mipi_pixel_rate = 2445940000,// OPSYCK system pixel rate
		.readout_length = 3142, // normal mode no need
		.read_margin = 32,  // normal mode no need
		.framelength_step = 4,
		.coarse_integ_step = 4,
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
		.ae_binning_ratio = 1000,// Output Pixel Level Ratio
		.fine_integ_line = 6326, // rg read:55872. 55872/8832=6.326
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
		.csi_param = {
			.cphy_settle = 76,
		},
		.support_mcss = 1,
	},
	{// mode 0, Normal Obin_4096_3072_30fps_PD_S2,
// reg12 IMX06A_MTK-CPHY-1.8V-24M_RegisterSetting_ver5.00-7.00_231221_test.xlsx,
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = imx06a_preview_setting,
		.mode_setting_len = ARRAY_SIZE(imx06a_preview_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06a_seamless_preview,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06a_seamless_preview),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 3340800000,// VTPXCK system pixel rate
		.linelength = 8832,//line_length_pck
		.framelength = 12608,//frame_length_lines
		.max_framerate = 300,
		.mipi_pixel_rate = 2445940000,// OPSYCK system pixel rate
		.readout_length = 3142, // normal mode no need
		.read_margin = 32,  // normal mode no need
		.framelength_step = 4,
		.coarse_integ_step = 4,
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
		.ae_binning_ratio = 1000,// Output Pixel Level Ratio
		.fine_integ_line = 6326, // rg read:55872. 55872/8832=6.326
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
		.csi_param = {
			.cphy_settle = 76,
		},
		.support_mcss = 1,
	},
	{//mode 2
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = imx06a_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(imx06a_normal_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 3340800000,// VTPXCK system pixel rate
		.linelength = 8832,//line_length_pck
		.framelength = 12608,//frame_length_lines
		.max_framerate = 300,
		.mipi_pixel_rate = 2445940000,// OPSYCK system pixel rate
		.readout_length = 3142, // normal mode no need
		.read_margin = 32,  // normal mode no need
		.framelength_step = 1,  // no need
		.coarse_integ_step = 1,  // no need
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
		.ae_binning_ratio = 1000,
		.fine_integ_line = 6326,//TBD
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
		.csi_param = {
			.cphy_settle = 76,
		},
		.support_mcss = 1,
	},
	{// mode 0, Normal Obin_4096_3072_30fps_PD_S2,
// reg12 IMX06A_MTK-CPHY-1.8V-24M_RegisterSetting_ver5.00-7.00_231221_test.xlsx,
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = imx06a_preview_setting,
		.mode_setting_len = ARRAY_SIZE(imx06a_preview_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06a_seamless_preview,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06a_seamless_preview),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 3340800000,// VTPXCK system pixel rate
		.linelength = 8832,//line_length_pck
		.framelength = 12608,//frame_length_lines
		.max_framerate = 300,
		.mipi_pixel_rate = 2445940000,// OPSYCK system pixel rate
		.readout_length = 3142, // normal mode no need
		.read_margin = 32,  // normal mode no need
		.framelength_step = 4,
		.coarse_integ_step = 4,
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
		.ae_binning_ratio = 1000,// Output Pixel Level Ratio
		.fine_integ_line = 6326, // rg read:55872. 55872/8832=6.326
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
		.csi_param = {
			.cphy_settle = 76,
		},
		.support_mcss = 1,
	},
	{// mode 0, Normal Obin_4096_3072_30fps_PD_S2,
// reg12 IMX06A_MTK-CPHY-1.8V-24M_RegisterSetting_ver5.00-7.00_231221_test.xlsx,
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table = imx06a_preview_setting,
		.mode_setting_len = ARRAY_SIZE(imx06a_preview_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06a_seamless_preview,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06a_seamless_preview),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 3340800000,// VTPXCK system pixel rate
		.linelength = 8832,//line_length_pck
		.framelength = 12608,//frame_length_lines
		.max_framerate = 300,
		.mipi_pixel_rate = 2445940000,// OPSYCK system pixel rate
		.readout_length = 3142, // normal mode no need
		.read_margin = 32,  // normal mode no need
		.framelength_step = 4,
		.coarse_integ_step = 4,
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
		.ae_binning_ratio = 1000,// Output Pixel Level Ratio
		.fine_integ_line = 6326, // rg read:55872. 55872/8832=6.326
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
		.csi_param = {
			.cphy_settle = 76,
		},
		.support_mcss = 1,
	},
	{// mode 1,
// F2-DOL 2DOL_4096_3072_30fps_PD_S2, 2DOL 1ST, 2DOL_4096_3072_30fps_PD_S2, 2DOL 2ND,
// reg13 IMX06A_MTK-CPHY-1.8V-24M_RegisterSetting_ver5.00-7.00_231221_test.xlsx,
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = imx06a_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(imx06a_custom1_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06a_seamless_custom1,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06a_seamless_custom1),
		.hdr_mode = HDR_RAW_STAGGER,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 3340800000,// VTPXCK system pixel rate
		.linelength = 8832,//line_length_pck
		.framelength = 6304*2,//frame_length_lines
		.max_framerate = 300,
		.mipi_pixel_rate = 2445940000,// OPSYCK system pixel rate
		.readout_length = 3142*2, // 32+(112+(Y_ADD_END-Y_ADD_STA+65))/2 = 32+(112+(6143-0+65))/2
		.read_margin = 32,  // fix value, see SRM
		.framelength_step = 8*2,
		.coarse_integ_step = 8*2,
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
		.ae_binning_ratio = 1000,// Output Pixel Level Ratio
		.fine_integ_line = 6326,//TBD
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
		.csi_param = {
			.cphy_settle = 76,
		},
		.support_mcss = 1,
	},
	{// mode 2,
// Normal Full_Crop_QBC_RAW_4096_3072_30fps_PD_S2
// reg14 IMX06A_MTK-CPHY-1.8V-24M_RegisterSetting_ver5.00-7.00_231221_test.xlsx,
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = imx06a_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(imx06a_custom2_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06a_seamless_custom2,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06a_seamless_custom2),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 3340800000,// VTPXCK system pixel rate
		.linelength = 17248,//line_length_pck
		.framelength = 6456,//frame_length_lines
		.max_framerate = 300,
		.mipi_pixel_rate = 2445940000,// OPSYCK system pixel rate
		.readout_length = 3142, // normal mode no need
		.read_margin = 32,  // normal mode no need
		.framelength_step = 4,
		.coarse_integ_step = 2,
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
		.ae_binning_ratio = 1000,// Output Pixel Level Ratio
		.fine_integ_line = 3239, // rg read:55872. 55872/17248=3.239
		.delay_frame = 2,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_R,
		.csi_param = {
			.cphy_settle = 76,
		},
		.support_mcss = 1,
	},
#else
/* DCG VS */
	{
		.frame_desc = frame_desc_regB,
		.num_entries = ARRAY_SIZE(frame_desc_regB),
		.mode_setting_table = imx06a_dcg_vs_regB,
		.mode_setting_len = ARRAY_SIZE(imx06a_dcg_vs_regB),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06a_dcg_vs_regB,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06a_dcg_vs_regB),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2803200000,// VTPXCK system pixel rate
		.linelength = 8832,//line_length_pck
		.framelength = 10576,//frame_length_lines
		.max_framerate = 300,
		.mipi_pixel_rate = 2445940000,// OPSYCK system pixel rate
		.readout_length = 3142, // normal mode no need
		.read_margin = 32,  // normal mode no need
		.framelength_step = 1,  // no need
		.coarse_integ_step = 1,  // no need
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
		.ae_binning_ratio = 1000,
		.fine_integ_line = 6326,//TBD
		.delay_frame = 3,
	},
	{
		.frame_desc = frame_desc_regB,
		.num_entries = ARRAY_SIZE(frame_desc_regB),
		.mode_setting_table = imx06a_dcg_vs_regB,
		.mode_setting_len = ARRAY_SIZE(imx06a_dcg_vs_regB),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06a_dcg_vs_regB,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06a_dcg_vs_regB),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2803200000,// VTPXCK system pixel rate
		.linelength = 8832,//line_length_pck
		.framelength = 10576,//frame_length_lines
		.max_framerate = 300,
		.mipi_pixel_rate = 2445940000,// OPSYCK system pixel rate
		.readout_length = 3142, // normal mode no need
		.read_margin = 32,  // normal mode no need
		.framelength_step = 1,  // no need
		.coarse_integ_step = 1,  // no need
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
		.ae_binning_ratio = 1000,
		.fine_integ_line = 6326,//TBD
		.delay_frame = 3,
	},
	{
		.frame_desc = frame_desc_regB,
		.num_entries = ARRAY_SIZE(frame_desc_regB),
		.mode_setting_table = imx06a_dcg_vs_regB,
		.mode_setting_len = ARRAY_SIZE(imx06a_dcg_vs_regB),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06a_dcg_vs_regB,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06a_dcg_vs_regB),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2803200000,// VTPXCK system pixel rate
		.linelength = 8832,//line_length_pck
		.framelength = 10576,//frame_length_lines
		.max_framerate = 300,
		.mipi_pixel_rate = 2445940000,// OPSYCK system pixel rate
		.readout_length = 3142, // normal mode no need
		.read_margin = 32,  // normal mode no need
		.framelength_step = 1,  // no need
		.coarse_integ_step = 1,  // no need
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
		.ae_binning_ratio = 1000,
		.fine_integ_line = 6326,//TBD
		.delay_frame = 3,
	},
	{
		.frame_desc = frame_desc_regA,
		.num_entries = ARRAY_SIZE(frame_desc_regA),
		.mode_setting_table = imx06a_dcg_vs_regA,
		.mode_setting_len = ARRAY_SIZE(imx06a_dcg_vs_regA),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06a_dcg_vs_regA,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06a_dcg_vs_regA),
		.hdr_mode = HDR_RAW_DCG_COMPOSE_VS,
		.raw_cnt = 2,
		.exp_cnt = 3,
		.pclk = 2803200000,// VTPXCK system pixel rate
		.linelength = 16096,//line_length_pck
		.framelength = 2744 * 2,//frame_length_lines
		.max_framerate = 300,
		.mipi_pixel_rate = 1747100000,
		.readout_length = 2304,
		.read_margin = 64,
		.framelength_step = 8,
		.coarse_integ_step = 4,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].min = 8,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].min = 8,
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

		.ae_binning_ratio = 1000,
		.fine_integ_line = -436,
		.delay_frame = 3,
		.saturation_info = &imgsensor_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06a_dcg_ratio_table_ratio16,
			.dcg_gain_table_size = sizeof(imx06a_dcg_ratio_table_ratio16),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_SE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_SE].max = BASEGAIN * 64,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 1390,  // 8.023ms
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 1390,  // 8.023ms
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_SE].max = 278,  // 0.99ms
		.multiexp_s_info[IMGSENSOR_EXPOSURE_SE].belong_to_lut_id = IMGSENSOR_LUT_B,
		.mode_lut_s_info[IMGSENSOR_LUT_B].linelength = 9952,//line_length_pck
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
	},

	{
		.frame_desc = frame_desc_regC,
		.num_entries = ARRAY_SIZE(frame_desc_regC),
		.mode_setting_table = imx06a_dcg_vs_regC,
		.mode_setting_len = ARRAY_SIZE(imx06a_dcg_vs_regC),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06a_dcg_vs_regC,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06a_dcg_vs_regC),
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 2803200000,// VTPXCK system pixel rate
		.linelength = 17664,//line_length_pck
		.framelength = 5288,//frame_length_lines
		.max_framerate = 300,
		.mipi_pixel_rate = 1747100000,// OPSYCK system pixel rate
		.readout_length = 3142, // normal mode no need
		.read_margin = 32,  // normal mode no need
		.framelength_step = 1,  // no need
		.coarse_integ_step = 1,  // no need
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
		.ae_binning_ratio = 1000,
		.fine_integ_line = 6326,//TBD
		.delay_frame = 3,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_R,
	},
	{
		.frame_desc = frame_desc_regD,
		.num_entries = ARRAY_SIZE(frame_desc_regD),
		.mode_setting_table = imx06a_dcg_vs_regD,
		.mode_setting_len = ARRAY_SIZE(imx06a_dcg_vs_regD),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06a_dcg_vs_regD,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06a_dcg_vs_regD),
		.hdr_mode = HDR_RAW_DCG_RAW_VS,
		.raw_cnt = 3,
		.exp_cnt = 3,
		.pclk = 2803200000,// VTPXCK system pixel rate
		.linelength = 16096,//line_length_pck
		.framelength = 2912 * 2,//frame_length_lines
		.max_framerate = 300,
		.mipi_pixel_rate = 2445940000,// OPSYCK system pixel rate
		.readout_length = 2848,
		.read_margin = 64,
		.framelength_step = 8,
		.coarse_integ_step = 4,
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
		.ae_binning_ratio = 1000,
		.fine_integ_line = -436,
		.delay_frame = 3,
		.saturation_info = &imgsensor_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06a_dcg_ratio_table_ratio16,
			.dcg_gain_table_size = sizeof(imx06a_dcg_ratio_table_ratio16),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 4,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_SE].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_SE].max = BASEGAIN * 64,
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 1390,  // 8.023ms
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 1390,  // 8.023ms
		.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_SE].max = 278,  // 0.99ms
		.multiexp_s_info[IMGSENSOR_EXPOSURE_SE].belong_to_lut_id = IMGSENSOR_LUT_B,
		.mode_lut_s_info[IMGSENSOR_LUT_B].linelength = 8832,//line_length_pck
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	},
	{
		.frame_desc = frame_desc_regE,
		.num_entries = ARRAY_SIZE(frame_desc_regE),
		.mode_setting_table = imx06a_dcg_vs_regE,
		.mode_setting_len = ARRAY_SIZE(imx06a_dcg_vs_regE),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx06a_dcg_vs_regE,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx06a_dcg_vs_regE),
		.hdr_mode = HDR_RAW_DCG_RAW,
		.raw_cnt = 2,
		.exp_cnt = 2,
		.pclk = 2803200000,// VTPXCK system pixel rate
		.linelength = 32192,//line_length_pck
		.framelength = 2900,//frame_length_lines
		.max_framerate = 300,
		.mipi_pixel_rate = 2445940000,// OPSYCK system pixel rate
		.readout_length = 2304,
		.read_margin = 64,
		.framelength_step = 8,
		.coarse_integ_step = 4,
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
		.ae_binning_ratio = 1000,
		.fine_integ_line = -436,
		.delay_frame = 3,
		.saturation_info = &imgsensor_saturation_info_14bit,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_RAW,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx06a_dcg_ratio_table_ratio16,
			.dcg_gain_table_size = sizeof(imx06a_dcg_ratio_table_ratio16),
		},
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].min = BASEGAIN * 16,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 64,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].min = BASEGAIN * 1,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_ME].max = BASEGAIN * 4,
		//.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_LE].max = 1390,  // 8.023ms
		//.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_ME].max = 1390,  // 8.023ms
		//.multi_exposure_shutter_range[IMGSENSOR_EXPOSURE_SE].max = 278,  // 0.99ms
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	},
#endif
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = IMX06A_SENSOR_ID, // 0xa18a,
	.reg_addr_sensor_id = {0x0016, 0x0017},
	.i2c_addr_table = {0x35, 0x34, 0xFF}, //{0x35, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = 0,
	.eeprom_num = 0,
	.mirror = IMAGE_NORMAL,

	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
	.ana_gain_def = BASEGAIN * 4, // hardcode
	.ana_gain_min = BASEGAIN * 1, // 0dB
	.ana_gain_max = BASEGAIN * 256, // 24dB --> 10^(24/10) = 251.189
	.ana_gain_type = 0, //0:sony, 1:ov or samusng....etc no used
	.ana_gain_step = 4,
	.ana_gain_table = imx06a_ana_gain_table,
	.ana_gain_table_size = sizeof(imx06a_ana_gain_table),
	.tuning_iso_base = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 4, // Constraints of COARSE_INTEG_TIME
	.exposure_max = 128 * (0xFFFC - 64), // Constraints of COARSE_INTEG_TIME
	.exposure_step = 4, // Constraints of COARSE_INTEG_TIME
	.exposure_margin = 64, // Constraints of COARSE_INTEG_TIME
	.dig_gain_min = BASE_DGAIN * 1, // no need (only use ana gain)
	.dig_gain_max = BASEGAIN * 256, // no need (only use ana gain)
	.dig_gain_step = 4, // no need (only use ana gain)

	.frame_length_max = 0xFFFC,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 500000, // tuning for sensor fusion

	.pdaf_type = PDAF_SUPPORT_CAMSV_QPD, // ask vendor
	.hdr_type = HDR_SUPPORT_STAGGER_FDOL|HDR_SUPPORT_DCG_VS,
	.seamless_switch_support = TRUE,
	.seamless_switch_type = SEAMLESS_SWITCH_CUT_VB_INIT_SHUT,
	.seamless_switch_hw_re_init_time_ns = 3500000,
	.seamless_switch_prsh_hw_fixed_value = 48,
	.seamless_switch_prsh_length_lc = 0,
	.reg_addr_prsh_length_lines = {0x3058, 0x3059, 0x305a, 0x305b},
	.reg_addr_prsh_mode = 0x3056,

	.temperature_support = FALSE,

	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.s_cali = set_sensor_cali,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {
			{0x0202, 0x0203},
			{0x0224, 0x0225},
	},
	.long_exposure_support = TRUE,
	.reg_addr_exposure_lshift = 0x3150,
	.reg_addr_ana_gain = {
			{0x0204, 0x0205},
			{0x0216, 0x0217},
	},
	.reg_addr_dig_gain = {
			{0x020E, 0x020F},
			{0x0218, 0x0219},
	},
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_temp_en = 0x0138,
	.reg_addr_temp_read = 0x013A,
	.reg_addr_auto_extend = 0x0350,
	.reg_addr_frame_count = 0x0005,
	.reg_addr_fast_mode = 0x3010,

	.reg_addr_exposure_in_lut = {
			{0x0E20, 0x0E21},
			{0x0E80, 0x0E81},
	},
	.reg_addr_ana_gain_in_lut = {
			{0x0E22, 0x0E23},
			{0x0E82, 0x0E83},
	},
	.reg_addr_frame_length_in_lut = {
			{0x0E28, 0x0E29},
			{0x0E88, 0x0E89},
	},
	.reg_addr_dcg_ratio = 0x3172,

#ifndef FOR_DCG_VS
	.init_setting_table = imx06a_init_setting,
	.init_setting_len = ARRAY_SIZE(imx06a_init_setting),
#else
	.init_setting_table = imx06a_dcg_vs_init_setting,
	.init_setting_len = ARRAY_SIZE(imx06a_dcg_vs_init_setting),
#endif

	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 1,
	.chk_s_off_end = 0,

	.checksum_value = 0xAF3E324F,

	/* MCSS */
	.use_mcss_gph_sync = 0,
	.reg_addr_mcss_slave_add_en_2nd = 0x3024,
	.reg_addr_mcss_slave_add_acken_2nd = 0x3025,
	.reg_addr_mcss_controller_target_sel = 0x3050,
	.reg_addr_mcss_xvs_io_ctrl = 0x3030,
	.reg_addr_mcss_extout_en = 0x3051,
	// .reg_addr_mcss_sgmsync_sel = 0x404B,
	// .reg_addr_mcss_swdio_io_ctrl = 0x303C,
	// .reg_addr_mcss_gph_sync_mode = 0x3080,
	.reg_addr_mcss_complete_sleep_en = 0x385E,
	.reg_addr_mcss_mc_frm_lp_en = 0x306D,
	.reg_addr_mcss_frm_length_reflect_timing =  0x3020,
	.reg_addr_mcss_mc_frm_mask_num = 0x306c,
	.mcss_init = imx06a_mcss_init,
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
	.vsync_notify = vsync_notify,
	.update_sof_cnt = common_update_sof_cnt,
	.mcss_set_mask_frame = imx06a_mcss_set_mask_frame,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	// the same as imx989
	{HW_ID_MCLK, {24}, 0},
	{HW_ID_RST, {0}, 0},
	{HW_ID_MCLK_DRIVING_CURRENT, {4}, 1000},
	{HW_ID_AVDD, {2900000, 2900000}, 0}, // pmic_ldo for avdd
	{HW_ID_AVDD2, {1800000, 1800000}, 0}, // pmic_ldo/gpio(1.8V ldo) for avdd1
	{HW_ID_AVDD1, {1800000, 1800000}, 0}, // pmic_ldo/gpio(1.8V ldo) for avdd1
	{HW_ID_AFVDD, {3100000, 3100000}, 0}, // pmic_ldo for afvdd
	{HW_ID_AFVDD1, {1800000, 1800000}, 0}, // pmic_gpo(3.1V ldo) for afvdd
	{HW_ID_DOVDD, {1800000, 1800000}, 0}, // pmic_ldo/gpio(1.8V ldo) for dovdd
	{HW_ID_DVDD, {830000, 830000}, 1000}, // pmic_ldo for dvdd
	{HW_ID_OISVDD, {3100000, 3100000}, 1000},
	{HW_ID_RST, {1}, 2000}
};


const struct subdrv_entry imx06a_mipi_raw_entry = {
	.name = "imx06a_mipi_raw",
	.id = IMX06A_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
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
			// subdrv_i2c_wr_u8(ctx, 0x86A9, 0x4E); // no such rg RD_QSC_KNOT_VALUE_OFFSET
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			subdrv_i2c_wr_u8(ctx, 0x3206, 0x01); // QSC_EN
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

	if (temperature <= 0x60)
		temperature_convert = temperature;
	else if (temperature >= 0x61 && temperature <= 0x7F)
		temperature_convert = 97;
	else if (temperature >= 0x80 && temperature <= 0xE2)
		temperature_convert = -30;
	else
		temperature_convert = (char)temperature | 0xFFFFFF0;

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

static int imx06a_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	struct mtk_hdr_ae *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;
	u32 exp_cnt = 0;
	enum SENSOR_SCENARIO_ID_ENUM pre_seamless_scenario_id;

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
	pre_seamless_scenario_id = ctx->current_scenario_id;
	update_mode_info_seamless_switch(ctx, scenario_id);

	set_i2c_buffer(ctx, 0x0104, 0x01);
	set_i2c_buffer(ctx, 0x3010, 0x02); //FAST_MODETRANSIT_CTL
	set_table_to_buffer(ctx,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);

	ctx->ae_ctrl_gph_en = 1;
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

	if (ctx->s_ctx.seamless_switch_prsh_length_lc > 0) {
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_prsh_mode, 0x01);

		set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_prsh_length_lines.addr[0],
				(ctx->s_ctx.seamless_switch_prsh_length_lc >> 24) & 0x07);
		set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_prsh_length_lines.addr[1],
				(ctx->s_ctx.seamless_switch_prsh_length_lc >> 16)  & 0xFF);
		set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_prsh_length_lines.addr[2],
				(ctx->s_ctx.seamless_switch_prsh_length_lc >> 8) & 0xFF);
		set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_prsh_length_lines.addr[3],
				(ctx->s_ctx.seamless_switch_prsh_length_lc) & 0xFF);

		DRV_LOG_MUST(ctx, "seamless switch pre-shutter set(%u)\n", ctx->s_ctx.seamless_switch_prsh_length_lc);
	} else
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_prsh_mode, 0x00);

	set_i2c_buffer(ctx, 0x0104, 0x00);
	ctx->ae_ctrl_gph_en = 0;
	commit_i2c_buffer(ctx);

	ctx->fast_mode_on = TRUE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->is_seamless = FALSE;
	DRV_LOG(ctx, "X: set seamless switch done\n");
	return ERROR_NONE;
}

static int imx06a_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	switch (mode) {
	case 5:
		subdrv_i2c_wr_u8(ctx, 0x020E, 0x00); /* dig_gain = 0 */
		break;
	default:
		subdrv_i2c_wr_u8(ctx, 0x0601, mode);
		break;
	}

	if ((ctx->test_pattern) && (mode != ctx->test_pattern)) {
		if (ctx->test_pattern == 5)
			subdrv_i2c_wr_u8(ctx, 0x020E, 0x01);
		else if (mode == 0)
			subdrv_i2c_wr_u8(ctx, 0x0601, 0x00); /* No pattern */
	}

	ctx->test_pattern = mode;
	return ERROR_NONE;
}

static int imx06a_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
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
		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_prsh_mode, 0x00);
		set_i2c_buffer(ctx, 0x3010, 0x00);
		commit_i2c_buffer(ctx);
	}

	if (sof_cnt == 1) {
		DRV_LOG(ctx, "pre-shutter disabled.");
		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_prsh_mode, 0x00);
		commit_i2c_buffer(ctx);
	}
	return 0;
}

static int imx06a_mcss_init(void *arg)
{
	//struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	//if (!(ctx->mcss_init_info.enable_mcss)) {
	//	memset(&(ctx->mcss_init_info), 0, sizeof(struct mtk_fsync_hw_mcss_init_info));

	//	set_i2c_buffer(ctx,
	//		ctx->s_ctx.reg_addr_mcss_mc_frm_mask_num, 0X00);
	//	subdrv_i2c_wr_u8(ctx,
	//		ctx->s_ctx.reg_addr_mcss_extout_en, 0x00);
	//	DRV_LOG_MUST(ctx, "disable XVS output and clear MCSS mask frame to 0\n");
	//	return ERROR_NONE;
	//}

	//if (ctx->mcss_init_info.is_mcss_master) {
	//	DRV_LOG_MUST(ctx, "common_mcss_init controller (ctx->s_ctx.sensor_id=0x%x)\n",ctx->s_ctx.sensor_id);
	//	subdrv_i2c_wr_u8(ctx,
	//		ctx->s_ctx.reg_addr_mcss_extout_en, 0x01); /* start to output XVS signal */
	//}

	//return ERROR_NONE;

	return 0;
}

static int imx06a_mcss_set_mask_frame(struct subdrv_ctx *ctx, u32 num, u32 is_critical)
{
	ctx->s_ctx.s_gph((void *)ctx, 1);
	set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_mcss_mc_frm_mask_num,  (0x7f & num));
	ctx->s_ctx.s_gph((void *)ctx, 0);

	if (is_critical)
		commit_i2c_buffer(ctx);

	DRV_LOG(ctx, "set mask frame num:%d\n", (0x7f & num));
	return 0;
}
