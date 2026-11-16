// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.
/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 ov50dmipiraw_Sensor.c
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
#include "ov50dmipiraw_Sensor.h"
#define OV50D_EMBEDDED_DATA_EN 0
static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static void ov50d_set_dummy(struct subdrv_ctx *ctx);
static int ov50d_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static u16 get_gain2reg(u32 gain);
static int set_streaming_control(void *arg, bool enable);
static int ov50d_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int ov50d_aov_dualsync(struct subdrv_ctx *ctx, u32 role);

/* STRUCT */
static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, ov50d_set_test_pattern},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO, ov50d_set_max_framerate_by_scenario},
};
static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0bb8,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0bb8,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0a28,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0a28,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0500,
			.vsize = 0x02d0,
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
			.hsize = 0x0280,
			.vsize = 0x01e0,
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
			.hsize = 0x0280,
			.vsize = 0x01e0,
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
			.hsize = 0x0280,
			.vsize = 0x01e0,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2a, //raw8
			.hsize = 0x0140,
			.vsize = 0x00f0,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2a, //raw8
			.hsize = 0x0140,
			.vsize = 0x00f0,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus6[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2a, //raw8
			.hsize = 0x0140,
			.vsize = 0x00f0,
			.user_data_desc = VC_STAGGER_NE,
			.fs_seq = MTK_FRAME_DESC_FS_SEQ_ONLY_ONE,
		},
	},
};
static struct subdrv_mode_struct mode_struct[] = {
	// mode 00 - 4000x3000@30fps, 1exp, non-pd
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = addr_data_pair_preview_ov50d2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_preview_ov50d2q),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 425, //01A9
		.framelength = 7840, //1EA0
		.max_framerate = 300,
		.mipi_pixel_rate = 760800000, //DPHY: 1902M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 48,
			.y1_offset = 36,
			.w1_size = 4000,
			.h1_size = 3000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 3000,
		},
		.aov_mode = 0,
		.s_dummy_support = 1,
		.ae_ctrl_support = 1,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 62,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	{
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = addr_data_pair_capture_ov50d2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_capture_ov50d2q),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 425, //01A9
		.framelength = 7840, //1EA0
		.max_framerate = 300,
		.mipi_pixel_rate = 760800000, //DPHY: 1902M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 48,
			.y1_offset = 36,
			.w1_size = 4000,
			.h1_size = 3000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 3000,
		},
		.aov_mode = 0,
		.s_dummy_support = 1,
		.ae_ctrl_support = 1,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 62,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = addr_data_pair_video_ov50d2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_video_ov50d2q),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 425, //01A9
		.framelength = 7840, //1EA0
		.max_framerate = 300,
		.mipi_pixel_rate = 760800000, //DPHY: 1902M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 48,
			.y1_offset = 236,
			.w1_size = 4000,
			.h1_size = 2600,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 2600,
		},
		.aov_mode = 0,
		.s_dummy_support = 1,
		.ae_ctrl_support = 1,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 62,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = addr_data_pair_hs_video_ov50d2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_hs_video_ov50d2q),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 425, //01A9
		.framelength = 7840, //1EA0
		.max_framerate = 300,
		.mipi_pixel_rate = 760800000, //DPHY: 1902M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 4096,
			.scale_h = 3072,
			.x1_offset = 48,
			.y1_offset = 236,
			.w1_size = 4000,
			.h1_size = 2600,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 2600,
		},
		.aov_mode = 0,
		.s_dummy_support = 1,
		.ae_ctrl_support = 1,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 62,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	{
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table = addr_data_pair_slim_video_ov50d2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_slim_video_ov50d2q),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 1300, //0514
		.framelength = 852, //0354
		.max_framerate = 300,
		.mipi_pixel_rate = 760800000, //DPHY: 1902M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 2048,
			.scale_h = 1536,
			.x1_offset = 384,
			.y1_offset = 408,
			.w1_size = 1280,
			.h1_size = 720,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1280,
			.h2_tg_size = 720,
		},
		.aov_mode = 0,
		.s_dummy_support = 1,
		.ae_ctrl_support = 1,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.5,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	{
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = addr_data_pair_custom1_ov50d2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom1_ov50d2q),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 4696,
		.framelength = 2130,
		.max_framerate = 100,
		.mipi_pixel_rate = 760800000, //DPHY: 1902M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 2048,
			.scale_h = 1536,
			.x1_offset = 704,
			.y1_offset = 528,
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
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.5,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	{
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = addr_data_pair_custom2_ov50d2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom2_ov50d2q),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 11736, //2dd8
		.framelength = 1704, //6A8
		.max_framerate = 50,
		.mipi_pixel_rate = 760800000, //DPHY: 1902M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 2048,
			.scale_h = 1536,
			.x1_offset = 704,
			.y1_offset = 528,
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
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.5,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	{
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = addr_data_pair_custom3_ov50d2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom3_ov50d2q),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 23472,  //5BB0
		.framelength = 2130, //0852
		.max_framerate = 20,
		.mipi_pixel_rate = 760800000, //DPHY: 1902M*4(lane)/10(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 2048,
			.scale_h = 1536,
			.x1_offset = 704,
			.y1_offset = 528,
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
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.5,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = addr_data_pair_custom4_ov50d2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom4_ov50d2q),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 296, //0128
		.framelength = 33792, //8400
		.max_framerate = 100,
		.mipi_pixel_rate = 1023750000, //DPHY: 2047.5M*4(lane)/8(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 2048,
			.scale_h = 1536,
			.x1_offset = 864,
			.y1_offset = 648,
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
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.5,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_SENSING_MODE_RAW_MONO,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = addr_data_pair_custom5_ov50d2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom5_ov50d2q),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 296, //0128
		.framelength = 67584, //10800
		.max_framerate = 50,
		.mipi_pixel_rate = 1023750000, //DPHY: 2047.5M*4(lane)/8(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 2048,
			.scale_h = 1536,
			.x1_offset = 864,
			.y1_offset = 648,
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
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.5,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_SENSING_MODE_RAW_MONO,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
	{
		.frame_desc = frame_desc_cus6,
		.num_entries = ARRAY_SIZE(frame_desc_cus6),
		.mode_setting_table = addr_data_pair_custom6_ov50d2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom6_ov50d2q),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.raw_cnt = 1,
		.exp_cnt = 1,
		.pclk = 100000000,
		.linelength = 296, //0128
		.framelength = 168960, //29400
		.max_framerate = 20,
		.mipi_pixel_rate = 1023750000, //DPHY: 2047.5M*4(lane)/8(raw bit)
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8192,
			.full_h = 6144,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8192,
			.h0_size = 6144,
			.scale_w = 2048,
			.scale_h = 1536,
			.x1_offset = 864,
			.y1_offset = 648,
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
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.multi_exposure_ana_gain_range[IMGSENSOR_EXPOSURE_LE].max = BASEGAIN * 15.5,
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_SENSING_MODE_RAW_MONO,
		.dpc_enabled = TRUE,
		.pdc_enabled = TRUE,
	},
};
static struct subdrv_static_ctx static_ctx = {
	.sensor_id = OV50D_SENSOR_ID,
	.reg_addr_sensor_id = {0x300A, 0x300B, 0x300C},
	.i2c_addr_table = {0x6C, 0x20, 0x44, 0x46, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = PARAM_UNDEFINED,
	.eeprom_num = PARAM_UNDEFINED,
	.mirror = IMAGE_NORMAL,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type =  MIPI_OPHY_CSI2, //DPHY: MIPI_OPHY_CSI2 or MIPI_OPHY_NCSI2
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.ob_pedestal = 0x40,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 62,
	.ana_gain_type = 1,
	.ana_gain_step = 4,
	.ana_gain_table = ov50d_ana_gain_table,
	.ana_gain_table_size = sizeof(ov50d_ana_gain_table),
	.tuning_iso_base = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 20, //units: rows / lines
	.exposure_max = 0xFFFFFF - 20, //VTS - 20rows
	.exposure_step = 2,
	.exposure_margin = 20,
	.frame_length_max = 0xFFFFFF,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 1000000,
	.pdaf_type = PDAF_SUPPORT_CAMSV,
	.hdr_type = HDR_SUPPORT_NA,
	.seamless_switch_support = FALSE,
	.temperature_support = TRUE,
	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.s_cali = set_sensor_cali,
	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = PARAM_UNDEFINED,
	.reg_addr_exposure = {{0x3500, 0x3501, 0x3502}, {0x3540, 0x3541, 0x3542}}, //{{0x4E00, 0x4E01, 0x4E02}}
	.long_exposure_support = FALSE,
	.reg_addr_exposure_lshift = PARAM_UNDEFINED,
	.reg_addr_ana_gain = {{0x3508, 0x3509}, {0x3548, 0x3549}}, //{{0x4E08, 0x4E09}}
	.reg_addr_frame_length = {0x3840, 0x380E, 0x380F},
	.reg_addr_temp_en = 0x4D12,
	.reg_addr_temp_read = 0x4D13,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = 0x387F,
	.init_setting_table = addr_data_pair_init_ov50d2q,
	.init_setting_len = ARRAY_SIZE(addr_data_pair_init_ov50d2q),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 0,
	.chk_s_off_end = 0,
	.checksum_value = 0x388C7147,
	.aov_sensor_support = TRUE,
	.init_in_open = TRUE,
	.streaming_ctrl_imp = TRUE,
	.s_streaming_control = set_streaming_control,
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
	.aov_dualsync = ov50d_aov_dualsync,
};
static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST, {0}, 0},
	{HW_ID_MCLK, {26}, 1000},
	{HW_ID_MCLK_DRIVING_CURRENT, {8}, 0},
	{HW_ID_DOVDD, {1800000, 1800000}, 0}, // pmic_ldo/gpio(1.8V ldo) for dovdd
	{HW_ID_AVDD, {2800000, 2800000}, 0}, // pmic_ldo for avdd
	{HW_ID_DVDD, {1140000, 1200000}, 1000}, // pmic_ldo for dvdd
	{HW_ID_RST, {1}, 5000},
};
static struct subdrv_pw_seq_entry aov_pw_seq[] = {
	{HW_ID_RST, {0}, 0},
	{HW_ID_MCLK, {26, MCLK_ULPOSC}, 0}, // temp using normal 26m mclk
	{HW_ID_MCLK_DRIVING_CURRENT, {8}, 0},
	{HW_ID_DOVDD, {1800000, 1800000}, 0}, // pmic_ldo/gpio(1.8V ldo) for dovdd
	{HW_ID_AVDD, {2800000, 2800000}, 0}, // pmic_ldo for avdd
	{HW_ID_DVDD, {1140000, 1200000}, 1000}, // pmic_ldo for dvdd
	{HW_ID_RST, {1}, 5000},
};
const struct subdrv_entry ov50d_mipi_raw_entry = {
	.name = "ov50d_mipi_raw",
	.id = OV50D_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
	.aov_pw_seq = aov_pw_seq,
	.aov_pw_seq_cnt = ARRAY_SIZE(aov_pw_seq),
};
/* STRUCT */
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
			size = 8;
			addr = 0x5C0E;
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			pbuf += size;
			size = 720;
			addr = 0x5900;
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set PDC calibration data done.");
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
static void ov50d_set_dummy(struct subdrv_ctx *ctx)
{
	// bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 1);
	write_frame_length(ctx, ctx->frame_length);
	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 0);
	commit_i2c_buffer(ctx);
}
static int ov50d_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
		ov50d_set_dummy(ctx);
	return ERROR_NONE;
}
static u16 get_gain2reg(u32 gain)
{
	return gain * 256 / BASEGAIN;
}

static int set_streaming_control(void *arg, bool enable)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	int ret = 0;

	DRV_LOG(ctx, "E!\n");

	DRV_LOG_MUST(ctx,
		"streaming_enable(0=Sw Standby,1=streaming):(%d)\n", enable);

	if (ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode) {
		DRV_LOG_MUST(ctx,
			"AOV mode(%d) streaming control on apmcu side\n",
			ctx->sensor_mode);
	}

	if (enable) {
		/* do streamon after config HW framesync */

		subdrv_i2c_wr_u8(ctx, 0x0100, 0X01);
		DRV_LOG_MUST(ctx,
			"MODE_SEL(%08x)\n", subdrv_i2c_rd_u8(ctx, 0x0100));

		ctx->test_pattern = 0;
	} else {
		subdrv_i2c_wr_u8(ctx, 0x0100, 0x00);
		DRV_LOG_MUST(ctx,
			"MODE_SEL(%08x)\n", subdrv_i2c_rd_u8(ctx, 0x0100));
	}

	return ret;
}

static int ov50d_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	switch (mode) {
	case 2:
		subdrv_i2c_wr_u8(ctx, 0x50c1, 0x01);
		break;
	case 5:
		subdrv_i2c_wr_u8(ctx, 0x350a, 0x00); //d_gain_coarse_b set 0
		subdrv_i2c_wr_u8(ctx, 0x401a, 0x00);
		break;
	default:
		break;
	}
	if (mode != ctx->test_pattern)
		switch (ctx->test_pattern) {
		case 2:
			subdrv_i2c_wr_u8(ctx, 0x50c1, 0x00);
			break;
		case 5:
			subdrv_i2c_wr_u8(ctx, 0x350a, 0x01); //reset d_gain_coarse_b
			subdrv_i2c_wr_u8(ctx, 0x401a, 0x40);
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

static int ov50d_aov_dualsync(struct subdrv_ctx *ctx, u32 role)
{
	u32 fl = ctx->s_ctx.mode[ctx->current_scenario_id].framelength;

	if (role == 1) {
		fl = fl + 20;
		DRV_LOG_MUST(ctx, "with master role:%u fl=%u\n", role, fl);

		/* FL */
		subdrv_i2c_wr_u8(ctx,	ctx->s_ctx.reg_addr_frame_length.addr[0], (fl >> 16) & 0xFF);
		subdrv_i2c_wr_u8(ctx,	ctx->s_ctx.reg_addr_frame_length.addr[1], (fl >> 8) & 0xFF);
		subdrv_i2c_wr_u8(ctx,	ctx->s_ctx.reg_addr_frame_length.addr[2],  fl & 0xFF);

		/* VSYNC output via VSYNC pad Master Sensor */
		subdrv_i2c_wr_u8(ctx, 0x3002, 0x80);
		subdrv_i2c_wr_u8(ctx, 0x3008, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x3841, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x381a, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x381b, 0x08);
		subdrv_i2c_wr_u8(ctx, 0x381c, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x381d, 0x08);
		subdrv_i2c_wr_u8(ctx, 0x381e, 0x00); /*0x381e 0x381f: width*/
		subdrv_i2c_wr_u8(ctx, 0x381f, 0xff);
		subdrv_i2c_wr_u8(ctx, 0x382e, 0x00);
	} else if (role == 2) {
		DRV_LOG_MUST(ctx, "with slave role:%u fl=%u\n", role, fl);

		/* FL */
		subdrv_i2c_wr_u8(ctx,	ctx->s_ctx.reg_addr_frame_length.addr[0], (fl >> 16) & 0xFF);
		subdrv_i2c_wr_u8(ctx,	ctx->s_ctx.reg_addr_frame_length.addr[1], (fl >> 8) & 0xFF);
		subdrv_i2c_wr_u8(ctx,	ctx->s_ctx.reg_addr_frame_length.addr[2],  fl & 0xFF);
		/* FSIN input via VSYNC pad Slave Sensor */
		subdrv_i2c_wr_u8(ctx, 0x3690, 0x08);
		subdrv_i2c_wr_u8(ctx, 0x3865, 0x40);
		subdrv_i2c_wr_u8(ctx, 0x382a, 0xbb);
		subdrv_i2c_wr_u8(ctx, 0x3834, 0x10);
		subdrv_i2c_wr_u8(ctx, 0x3843, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x3824, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x3825, 0x56);
		subdrv_i2c_wr_u8(ctx, 0x3835, 0x06);
		subdrv_i2c_wr_u8(ctx, 0x3836, 0x1F); /* 0x3835,0x3836: offset */
		subdrv_i2c_wr_u8(ctx, 0x382e, 0x00);
	} else {
		DRV_LOG_MUST(ctx, "unknown role:%u\n", role);
	}

	return 0;
}
