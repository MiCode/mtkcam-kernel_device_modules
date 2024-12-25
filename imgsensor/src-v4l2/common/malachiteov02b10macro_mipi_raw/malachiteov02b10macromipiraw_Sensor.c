// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 malachiteov02b10macromipiraw_Sensor.c
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
#include "malachiteov02b10macromipiraw_Sensor.h"

#define read_cmos_sensor_8(...) subdrv_i2c_rd_u8_8bit(__VA_ARGS__)

#define write_cmos_sensor_8(...) subdrv_i2c_wr_u8_8bit(__VA_ARGS__)


static void malachiteov02b10macro_set_dummy(struct subdrv_ctx *ctx);
static int malachiteov02b10macro_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int  malachiteov02b10macro_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int macro_vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt);
static int malachiteov02b10macro_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int malachiteov02b10macro_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int malachiteov02b10macro_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int malachiteov02b10macro_streamoff(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int malachiteov02b10macro_streamon(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int malachiteov02b10macro_streaming_control(struct subdrv_ctx *ctx, bool enable);
static int malachiteov02b10macro_close(struct subdrv_ctx *ctx);
static int malachiteov02b10macro_get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id);
//static void malachiteov02b10macro_write_init_setting(void *arg);
static int malachiteov02b10macro_get_csi_param(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id, struct mtk_csi_param *csi_param);

static int control(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data);
static int open(struct subdrv_ctx *ctx);

/* STRUCT */
static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN,				malachiteov02b10macro_set_test_pattern},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO,	malachiteov02b10macro_set_max_framerate_by_scenario},
	{SENSOR_FEATURE_SET_GAIN,						malachiteov02b10macro_set_gain},
	{SENSOR_FEATURE_SET_ESHUTTER,					malachiteov02b10macro_set_shutter},
	{SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME,			malachiteov02b10macro_set_shutter_frame_length},
	{SENSOR_FEATURE_SET_STREAMING_SUSPEND,			malachiteov02b10macro_streamoff},
	{SENSOR_FEATURE_SET_STREAMING_RESUME,			malachiteov02b10macro_streamon}
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		// .header_id = 0x010B00FF,
		// .header_id = 0x0,
		// .addr_header_id = 0x0000000B,
		.i2c_write_id = 0xA4,

		.pdc_support = TRUE,
		.pdc_size = 720,
		.addr_pdc = 0x12D2,
		.sensor_reg_addr_pdc = 0x5F80,

	},
};

// mode 0: 1600x1200@30fps, normal preview
static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1600,
			.vsize = 1200,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

// mode 0: 1600x1200@30fps, normal preview
//static const struct subdrv_mode_struct preview_mode_struct =
#define preview_mode_struct \
{ \
	.frame_desc = frame_desc_prev, \
	.num_entries = ARRAY_SIZE(frame_desc_prev),\
	.mode_setting_table = malachiteov02b10macro_preview_setting,\
	.mode_setting_len = ARRAY_SIZE(malachiteov02b10macro_preview_setting),\
	.seamless_switch_group = PARAM_UNDEFINED,\
	.seamless_switch_mode_setting_table = PARAM_UNDEFINED,\
	.seamless_switch_mode_setting_len = PARAM_UNDEFINED,\
	.hdr_mode = HDR_NONE,\
	.raw_cnt = 1,\
	.exp_cnt = 1,\
	.pclk = 16500000,\
	.linelength = 448,\
	.framelength = 1221,\
	.max_framerate = 300,\
	.mipi_pixel_rate = 66000000,\
	.readout_length = 0,\
	.read_margin = 7,\
	.framelength_step = 2,\
	.imgsensor_winsize_info = {\
		.full_w = 1600,\
		.full_h = 1200,\
		.x0_offset = 0,\
		.y0_offset = 0,\
		.w0_size = 1600,\
		.h0_size = 1200,\
		.scale_w = 1600,\
		.scale_h = 1200,\
		.x1_offset = 0,\
		.y1_offset = 0,\
		.w1_size = 1600,\
		.h1_size = 1200,\
		.x2_tg_offset = 0,\
		.y2_tg_offset = 0,\
		.w2_tg_size = 1600,\
		.h2_tg_size = 1200,\
	},\
	.pdaf_cap = FALSE,\
	.imgsensor_pd_info = PARAM_UNDEFINED,\
	.ae_binning_ratio = 1000,\
	.fine_integ_line = 0,\
	.delay_frame = 2,\
}
//};
static struct subdrv_mode_struct mode_struct[] = {
	preview_mode_struct,		//mode 0
	preview_mode_struct,		//mode 1
	preview_mode_struct,		//mode 2
	preview_mode_struct,		//mode 3
	preview_mode_struct,		//mode 4
	preview_mode_struct,		//mode 5
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = MALACHITEOV02B10MACRO_SENSOR_ID,
	.reg_addr_sensor_id = {0x02, 0x03},
	.i2c_addr_table = {0x78, 0x7A, 0xFF}, // TBD
	.i2c_burst_write_support = FALSE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {1600, 1200},
	.mirror = IMAGE_NORMAL, // TBD

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_lane_num = SENSOR_MIPI_1_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 15.5,
	.ana_gain_type = 0,
	.ana_gain_step = 1,
	.ana_gain_table = malachiteov02b10macro_ana_gain_table,
	.ana_gain_table_size = sizeof(malachiteov02b10macro_ana_gain_table),
	.min_gain_iso = 50,
	.exposure_def = 0x3D0,
	.exposure_min = 4,
	.exposure_max = 0x7fff - 7,
	.exposure_step = 2,
	.exposure_margin = 7,

	.frame_length_max = 0x7fff,
	.ae_effective_frame = 0,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 153800,

	.pdaf_type = PDAF_SUPPORT_NA,
	.hdr_type = HDR_SUPPORT_NA,
	.seamless_switch_support = FALSE,
	.temperature_support = FALSE,

	.g_temp = PARAM_UNDEFINED,
	.g_gain2reg = PARAM_UNDEFINED,
	.s_gph = PARAM_UNDEFINED,

	.reg_addr_stream = 0xc2,
	.reg_addr_mirror_flip = PARAM_UNDEFINED, // TBD
	.reg_addr_exposure = {
		{0x0e, 0x0f}
	},
	.long_exposure_support = FALSE,
	.reg_addr_exposure_lshift = PARAM_UNDEFINED,
	.reg_addr_ana_gain = {
		{0x22}
	},
	.reg_addr_frame_length = {0x14, 0x15},
	.reg_addr_temp_en = PARAM_UNDEFINED,
	.reg_addr_temp_read = PARAM_UNDEFINED,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = PARAM_UNDEFINED,

	.init_setting_table = malachiteov02b10macro_init_setting,
	.init_setting_len = ARRAY_SIZE(malachiteov02b10macro_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 0,
	.chk_s_off_end = 0,

	//TBD
	.checksum_value = 0xb7c53a42,
};

static struct subdrv_ops ops = {
	.get_vendr_id = common_get_vendor_id,
	.get_id = malachiteov02b10macro_get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = control,
	.feature_control = common_feature_control,
	.close = malachiteov02b10macro_close,
	.get_frame_desc = common_get_frame_desc,
	.get_temp = common_get_temp,
	.get_csi_param = malachiteov02b10macro_get_csi_param,
	.update_sof_cnt = common_update_sof_cnt,
	.vsync_notify = macro_vsync_notify,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_RST,   0,       1},
	{HW_ID_DOVDD, 1800000, 1},
	{HW_ID_AVDD,  2800000, 9},
	{HW_ID_MCLK,  24,      1},
	{HW_ID_MCLK_DRIVING_CURRENT, 4, 9},
	{HW_ID_RST,   1,       10},
};

const struct subdrv_entry malachiteov02b10macro_mipi_raw_entry = {
	.name = "malachiteov02b10macro_mipi_raw",
	.id = MALACHITEOV02B10MACRO_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

static void malachiteov02b10macro_set_dummy(struct subdrv_ctx *ctx)
{
}

static int malachiteov02b10macro_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
		malachiteov02b10macro_set_dummy(ctx);

	return ERROR_NONE;
}

static int malachiteov02b10macro_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
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

static int macro_vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt)
{
	DRV_LOG_MUST(ctx, "sensormode(%d) sof_cnt(%d)\n", ctx->current_scenario_id, sof_cnt);
	return 0;
};


static void set_dummy_ov02b10(struct subdrv_ctx *ctx)
{
	//OV02B10_LOG_DBG("dummyline = %d, dummypixels = %d \n", ctx->dummy_line, ctx->dummy_pixel);
	write_cmos_sensor_8(ctx, 0xfd, 0x01);
	write_cmos_sensor_8(ctx, 0x14, (ctx->frame_length  - 0x4c4) >> 8);
	write_cmos_sensor_8(ctx, 0x15, (ctx->frame_length  - 0x4c4) & 0xff);
	write_cmos_sensor_8(ctx, 0xfe, 0x02);
}	/*	set_dummy_ov02b10  */
static void set_max_framerate_ov02b10(struct subdrv_ctx *ctx,
		UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = ctx->frame_length;



	frame_length = ctx->pclk / framerate * 10 / ctx->line_length;
	if (frame_length >= ctx->min_frame_length)
		ctx->frame_length = frame_length;
	else
		ctx->frame_length = ctx->min_frame_length;

	ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;

	if (ctx->frame_length > static_ctx.frame_length_max) {
		ctx->frame_length = static_ctx.frame_length_max;
		ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;
	}
	if (min_framelength_en)
		ctx->min_frame_length = ctx->frame_length;
	set_dummy_ov02b10(ctx);
}	/*	set_max_framerate_ov02b10  */

static int malachiteov02b10macro_set_shutter(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	//malachiteov02b10macro_set_shutter_frame_length(ctx, para, len);
	kal_uint16 realtime_fps = 0;
	u64 shutter = *((u64 *)para);

	if (shutter > ctx->min_frame_length - static_ctx.exposure_margin)
		ctx->frame_length = shutter + static_ctx.exposure_margin;
	else
		ctx->frame_length = ctx->min_frame_length;

	if (ctx->frame_length > ctx->max_frame_length)
		ctx->frame_length = ctx->max_frame_length;

	shutter = (shutter < static_ctx.exposure_min) ? static_ctx.exposure_min : shutter;
	shutter = (shutter > (static_ctx.frame_length_max -
		static_ctx.exposure_margin)) ? (static_ctx.frame_length_max -
		static_ctx.exposure_margin) : shutter;

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / ctx->line_length * 10
				/ ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate_ov02b10(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate_ov02b10(ctx, 146, 0);
		else {
			/* Extend frame length */
			set_dummy_ov02b10(ctx);
		}
	} else {
			set_dummy_ov02b10(ctx);
	}

	// Update Shutter
	write_cmos_sensor_8(ctx, 0xfd, 0x01);
	write_cmos_sensor_8(ctx, 0x0e, (shutter >> 8) & 0xff);
	write_cmos_sensor_8(ctx, 0x0f, shutter  & 0xff);
	write_cmos_sensor_8(ctx, 0xfe, 0x02);
	//OV02B10_LOG_DBG("shutter =%d, framelength =%d\n", shutter, ctx->frame_length);
	return 0;
}
static int malachiteov02b10macro_set_shutter_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 shutter = *((u64 *)para);
	u32 frame_length = 0;
	//u32 fine_integ_line = 0;

	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	ctx->shutter = shutter;

	/* Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - ctx->frame_length;

	ctx->frame_length = ctx->frame_length + dummy_line;

	if (shutter > ctx->frame_length - static_ctx.exposure_margin)
		ctx->frame_length = shutter + static_ctx.exposure_margin;

	if (ctx->frame_length > static_ctx.frame_length_max)
		ctx->frame_length = static_ctx.frame_length_max;

	shutter = (shutter < static_ctx.exposure_min)
			? static_ctx.exposure_min : shutter;
	shutter = (shutter > (static_ctx.frame_length_max - static_ctx.exposure_margin))
			? (static_ctx.frame_length_max - static_ctx.exposure_margin)
			: shutter;

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / ctx->line_length * 10 /
				ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate_ov02b10(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate_ov02b10(ctx, 146, 0);
		else {
			set_dummy_ov02b10(ctx);
		}
	} else {
		set_dummy_ov02b10(ctx);
	}

	// Update Shutter
	write_cmos_sensor_8(ctx, 0xfd, 0x01);
	write_cmos_sensor_8(ctx, 0x0e, (shutter >> 8) & 0xff);
	write_cmos_sensor_8(ctx, 0x0f,  shutter  & 0xff);
	write_cmos_sensor_8(ctx, 0xfe, 0x02);
	//OV02B10_LOG_DBG("set shutter =%d, framelength =%d\n", shutter, ctx->frame_length);
	return 0;
}

static int malachiteov02b10macro_set_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *) para;
	u32 gain = *feature_data;

	kal_uint8  iReg;

	if (gain < static_ctx.ana_gain_min || gain > static_ctx.ana_gain_max) {
		if (gain < static_ctx.ana_gain_min)
			gain = static_ctx.ana_gain_min;
		else if (gain > static_ctx.ana_gain_max)
			gain = static_ctx.ana_gain_max;
	}

	iReg = 0x10 * gain/BASEGAIN;        //change mtk gain base to sensor gain 

	if(iReg<=0x10) // min Again
	{
		write_cmos_sensor_8(ctx,0xfd, 0x01);
		write_cmos_sensor_8(ctx,0x22, 0x10);
		write_cmos_sensor_8(ctx,0xfe, 0x02);	//fresh
		//SetGain = 16
	}
	else if(iReg>= 0xf8) //max Again
	{
		write_cmos_sensor_8(ctx,0xfd, 0x01);
		write_cmos_sensor_8(ctx,0x22, 0xf8);
		write_cmos_sensor_8(ctx,0xfe, 0x02);	//fresh
		//SetGain = 160
	}
	else
	{
		write_cmos_sensor_8(ctx,0xfd, 0x01);
		write_cmos_sensor_8(ctx,0x22, (kal_uint8)iReg);
		write_cmos_sensor_8(ctx,0xfe, 0x02);	//fresh
	}

	return 0;
}

static int  malachiteov02b10macro_streamon(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	return malachiteov02b10macro_streaming_control(ctx, TRUE);
}
static int malachiteov02b10macro_streamoff(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	return malachiteov02b10macro_streaming_control(ctx, FALSE);
}
static int  malachiteov02b10macro_streaming_control(struct subdrv_ctx *ctx, bool enable)
{

	int ret = 0;
	write_cmos_sensor_8(ctx, 0xfd, 0x03);

	if (enable) {
		write_cmos_sensor_8(ctx, 0xc2, 0x01);
	} else {
		write_cmos_sensor_8(ctx, 0xc2, 0x00);
	}
 
	mdelay(10);
	ctx->sof_no = 0;
	ctx->is_streaming = enable;
	DRV_LOG_MUST(ctx, "X! enable:%u\n", enable);
	return ret;
}
#if 0
void sensor_init(struct subdrv_ctx *ctx)
{
	int i = 0;

    // Global
        write_cmos_sensor_8(ctx, 0xfc, 0x01);
        mdelay(5);
	//malachiteov02b10macro_table_write_cmos_sensor(ctx, malachiteov02b10macro_init_setting,
	//	sizeof(malachiteov02b10macro_init_setting) / sizeof(kal_uint16));
    for(i = 0; i< sizeof(malachiteov02b10macro_init_setting) / sizeof(kal_uint16); i+=2)
	{
		write_cmos_sensor_8(ctx, malachiteov02b10macro_init_setting[i], malachiteov02b10macro_init_setting[i+1]);
	}
}	/*	  sensor_init  */
#endif
/*************************************************************************
 * FUNCTION
 *	open
 *
 * DESCRIPTION
 *	This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int open(struct subdrv_ctx *ctx)
{
	u32 i = 0;
	kal_uint8 retry = 2;
	u32 sensor_id = 0;

	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	write_cmos_sensor_8(ctx, 0xfd, 0x00);
	while (ctx->s_ctx.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = ctx->s_ctx.i2c_addr_table[i];
		do {
			malachiteov02b10macro_get_imgsensor_id(ctx, &sensor_id);

			if (sensor_id == ctx->s_ctx.sensor_id) {
				printk("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, sensor_id);
				break;
			}

			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == ctx->s_ctx.sensor_id)
			break;
		retry = 2;
	}
	if (ctx->s_ctx.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	//sensor_init(ctx);
	
	ctx->autoflicker_en = KAL_FALSE;
	//ctx->sensor_mode = IMGSENSOR_MODE_INIT;
	ctx->shutter = 0x3D0; 
	ctx->gain = 4 * BASEGAIN;
	ctx->current_fps = ctx->s_ctx.mode[SENSOR_SCENARIO_ID_NORMAL_PREVIEW].max_framerate;
	ctx->pclk = ctx->s_ctx.mode[SENSOR_SCENARIO_ID_NORMAL_PREVIEW].pclk;
	ctx->line_length = ctx->s_ctx.mode[SENSOR_SCENARIO_ID_NORMAL_PREVIEW].linelength;
	ctx->frame_length = ctx->s_ctx.mode[SENSOR_SCENARIO_ID_NORMAL_PREVIEW].framelength;
	ctx->min_frame_length = ctx->s_ctx.mode[SENSOR_SCENARIO_ID_NORMAL_PREVIEW].framelength;
	ctx->dummy_pixel = 0;
	ctx->dummy_line = 0;
	ctx->ihdr_mode = 0;
	ctx->test_pattern = KAL_FALSE;


	return ERROR_NONE;
} /* open */

/*************************************************************************
 * FUNCTION
 *	close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *	None
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int malachiteov02b10macro_close(struct subdrv_ctx *ctx)
{
	malachiteov02b10macro_streaming_control(ctx, FALSE);
	DRV_LOG_MUST(ctx, "malachiteov02b10macro_close\n");
	return ERROR_NONE;
}
static void preview_setting(struct subdrv_ctx *ctx)
{
	int i = 0;
	write_cmos_sensor_8(ctx, 0xfc, 0x01);//sw rst
	mdelay(6);//sw rst need 5ms

	for(i = 0; i< sizeof(malachiteov02b10macro_preview_setting) / sizeof(kal_uint16); i+=2) {
		write_cmos_sensor_8(ctx, malachiteov02b10macro_preview_setting[i] & 0xff, malachiteov02b10macro_preview_setting[i+1] & 0xff);
	}

	write_cmos_sensor_8(ctx, 0xfd, 0x01);
	write_cmos_sensor_8(ctx, 0x12, 0x03);//mirror:filp  bit[1:0]
} /* preview_setting */
static kal_uint32 preview(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	ctx->dummy_pixel = 0;
	ctx->dummy_line = 0;
	ctx->ihdr_mode = 0;
	ctx->test_pattern = KAL_FALSE;
	
    
	ctx->sensor_mode = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	ctx->current_fps = ctx->s_ctx.mode[SENSOR_SCENARIO_ID_NORMAL_PREVIEW].max_framerate;
	ctx->pclk = ctx->s_ctx.mode[SENSOR_SCENARIO_ID_NORMAL_PREVIEW].pclk;
	ctx->line_length = ctx->s_ctx.mode[SENSOR_SCENARIO_ID_NORMAL_PREVIEW].linelength;
	ctx->frame_length = ctx->s_ctx.mode[SENSOR_SCENARIO_ID_NORMAL_PREVIEW].framelength;
	ctx->min_frame_length = ctx->s_ctx.mode[SENSOR_SCENARIO_ID_NORMAL_PREVIEW].framelength;
	ctx->autoflicker_en = KAL_FALSE;

	preview_setting(ctx);

	return ERROR_NONE;
} /* preview */
static int control(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	//OV02B10_LOG_DBG("scenario_id = %d\n", scenario_id);
	ctx->current_scenario_id = scenario_id;
	switch (scenario_id) {

	default:
		//OV02B10_LOG_DBG("Error ScenarioId setting");
		preview(ctx, image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}	/* control(ctx) */

static int malachiteov02b10macro_get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id)
{
	u8 i = 0;
	u8 retry = 2;
	u32 addr_h = ctx->s_ctx.reg_addr_sensor_id.addr[0];
	u32 addr_l = ctx->s_ctx.reg_addr_sensor_id.addr[1];
	u32 addr_ll = ctx->s_ctx.reg_addr_sensor_id.addr[2];

	while (ctx->s_ctx.i2c_addr_table[i] != 0xFF) {
		ctx->i2c_write_id = ctx->s_ctx.i2c_addr_table[i];
		do {
			*sensor_id = (read_cmos_sensor_8(ctx, addr_h) << 8) |
				read_cmos_sensor_8(ctx, addr_l);
			if (addr_ll)
				*sensor_id = ((*sensor_id) << 8) | read_cmos_sensor_8(ctx, addr_ll);
			DRV_LOG_MUST(ctx, "i2c_write_id:0x%x sensor_id(cur/exp):0x%x/0x%x\n",
				ctx->i2c_write_id, *sensor_id, ctx->s_ctx.sensor_id);

			if (*sensor_id == ctx->s_ctx.sensor_id)
				return ERROR_NONE;
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != ctx->s_ctx.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}
#if 0
static void malachiteov02b10macro_write_init_setting(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	int i = 0;
	u16 *plist = ctx->s_ctx.init_setting_table;
	u16 len = ctx->s_ctx.init_setting_len;
	for (i = 0; i < len; i += 2) {
		write_cmos_sensor_8(ctx, plist[i], plist[i+1]&0xff);
	}
	DRV_LOG_MUST(ctx, "malachiteov02b10macro_write_init_setting");
}
#endif
static int malachiteov02b10macro_get_csi_param(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	struct mtk_csi_param *csi_param)
{
	DRV_LOG(ctx, "+ scenario_id:%u,aov_csi_clk:%u\n",scenario_id, ctx->aov_csi_clk);
	switch (scenario_id) {
	default:
		csi_param->legacy_phy = 0;
		csi_param->not_fixed_trail_settle = 1;
		csi_param->not_fixed_dphy_settle = 1;
		csi_param->dphy_data_settle = 0x28;
		csi_param->dphy_clk_settle = 0x28;
		csi_param->dphy_trail = 0x40;
		break;
	}
	return 0;
}

