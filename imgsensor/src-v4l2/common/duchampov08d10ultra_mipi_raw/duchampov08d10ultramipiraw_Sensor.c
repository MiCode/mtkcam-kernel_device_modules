// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 DUCHAMPOV08D10ULTRAmipi_Sensor.c
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
#define PFX "DUCHAMPOV08D10ULTRA_camera_sensor"


#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"

#include "duchampov08d10ultra_ana_gain_table.h"
#include "duchampov08d10ultramipiraw_Sensor.h"

#include "adaptor-subdrv.h"
#include "adaptor-i2c.h"

#define read_cmos_sensor_8(...) subdrv_i2c_rd_u8_u8(__VA_ARGS__)
#define read_cmos_sensor(...) subdrv_i2c_rd_u16(__VA_ARGS__)
#define write_cmos_sensor_8(...) subdrv_i2c_wr_u8_u8(__VA_ARGS__)
#define write_cmos_sensor(...) subdrv_i2c_wr_u16(__VA_ARGS__)
#define duchampov08d10ultra_table_write_cmos_sensor(...) subdrv_i2c_wr_regs_u8_u8(__VA_ARGS__)
#define LOG_TAG "[duchampov08d10ultra]"
#define OV08D10_LOG_INF(format, args...) pr_info(LOG_TAG "[%s] " format, __func__, ##args)
#define OV08D10_LOG_DBG(format, args...) pr_info(LOG_TAG "[%s] " format, __func__, ##args)

#undef VENDOR_EDIT

#define I2C_BUFFER_LEN 255 /* trans# max is 255, each 3 bytes */
#define SENSOR_MODE_NUM 7

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = DUCHAMPOV08D10ULTRA_SENSOR_ID,

	.checksum_value = 0xb7c53a42,

	.pre = {
		.pclk = 36000000,
		.linelength = 460,
		.framelength = 2608,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 288000000,
		.max_framerate = 300,
	},
	.cap = { /*same preview*/
		.pclk = 36000000,
		.linelength = 460,
		.framelength = 2608,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 288000000,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 36000000,
		.linelength = 460,
		.framelength = 2608,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 1840,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 288000000,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 36000000,
		.linelength = 460,
		.framelength = 2608,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 288000000,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 36000000,
		.linelength = 460,
		.framelength = 2608,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 288000000,
		.max_framerate = 300,
	},
	.custom1 = {
		.pclk = 36000000,
		.linelength = 478,
		.framelength = 1252,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1632,
		.grabwindow_height = 920,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 144000000,
		.max_framerate = 600,
	},
	.custom2 = {
		.pclk = 36000000,
		.linelength = 460,
		.framelength = 3256,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2880,
		.grabwindow_height = 2160,
		.mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 288000000,
		.max_framerate = 240,
	},
	.margin       = 20, /* sensor framelength & shutter margin */
	.min_shutter  = 4, /* min shutter */
	.min_gain     = BASEGAIN, //1x
	.max_gain     = 15.5 * BASEGAIN,//15.5x
	.min_gain_iso = 50,
	.exp_step     = 2,
	.gain_step    = 1,
	.gain_type    = 0,

	.max_frame_length           = 0x3fffff,
	.ae_shut_delay_frame        = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame     = 2, /* isp gain delay frame for AE cycle */
	.frame_time_delay_frame     = 2,

	.ihdr_support               = 0, /* 1, support; 0,not support */
	.ihdr_le_firstline          = 0, /* 1,le first ; 0, se first */
	.temperature_support        = 0, /* 1, support; 0,not support */

	.sensor_mode_num    = SENSOR_MODE_NUM, /* support sensor mode num */
	.cap_delay_frame        = 2, /* enter capture delay frame num */
	.pre_delay_frame        = 2, /* enter preview delay frame num */
	.video_delay_frame      = 2, /* enter video delay frame num */
	.hs_video_delay_frame   = 2,
	.slim_video_delay_frame = 2, /* enter slim video delay frame num */
	.custom1_delay_frame    = 2, /* enter custom1 delay frame num */
	.custom2_delay_frame    = 2, /* enter custom2 delay frame num */
	.custom3_delay_frame    = 2, /* enter custom3 delay frame num */
	.custom4_delay_frame    = 2, /* enter custom4 delay frame num */
	.custom5_delay_frame    = 2, /* enter custom5 delay frame num */
	.custom6_delay_frame	= 2, /* enter custom6 delay frame num */

	.isp_driving_current      = ISP_DRIVING_6MA,
	.sensor_interface_type    = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type         = MIPI_OPHY_NCSI2, /* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode   = 0,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24, /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num            = SENSOR_MIPI_2_LANE,
	.i2c_addr_table           = {0x6C, 0x20, 0xff},
	.i2c_speed                = 400, /* i2c read/write speed */
	.ob_pedestal              = 0x40,
};

/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
	{3264, 2448,   0,   0, 3264, 2448, 3264, 2448, 0000, 0000, 3264, 2448, 0, 0, 3264, 2448}, /* Preview */
	{3264, 2448,   0,   0, 3264, 2448, 3264, 2448, 0000, 0000, 3264, 2448, 0, 0, 3264, 2448}, /* capture */
	{3264, 2448,   0, 304, 3264, 1840, 3264, 1840, 0000, 0000, 3264, 1840, 0, 0, 3264, 1840}, /* video */
	{3264, 2448,   0,   0, 3264, 2448, 3264, 2448, 0000, 0000, 3264, 2448, 0, 0, 3264, 2448},
	{3264, 2448,   0,   0, 3264, 2448, 3264, 2448, 0000, 0000, 3264, 2448, 0, 0, 3264, 2448},
	{3264, 2448,   0, 304, 3264, 1840, 1632,  920, 0000, 0000, 1632,  920, 0, 0, 1632,  920}, /*custom 1*/
	{3264, 2448, 192, 144, 2880, 2160, 2880, 2160, 0000, 0000, 2880, 2160, 0, 0, 2880, 2160}, /*custom 2*/
};
static bool soft_power_status = false; //false = soft power down, true = soft power up
static void soft_power_up(struct subdrv_ctx *ctx);

static void set_dummy(struct subdrv_ctx *ctx)
{
	int dummy_line = 0;

	if(!soft_power_status){
		soft_power_up(ctx);
	}

	dummy_line = (ctx->frame_length - ctx->vblank_convert) * 2;
	if(dummy_line < 0 || dummy_line > 0xffff){
		OV08D10_LOG_INF("enable auto extended vts\n");
		ctx->dummy_line = 0xffff;
	} else {
		ctx->dummy_line = dummy_line;
	}
	OV08D10_LOG_INF("vts = %d, dummy_line = %d\n",
		ctx->frame_length, dummy_line);

	write_cmos_sensor_8(ctx, 0xfd, 0x01);
	write_cmos_sensor_8(ctx, 0x05, ctx->dummy_line >> 8);
	write_cmos_sensor_8(ctx, 0x06, ctx->dummy_line & 0xff);
	write_cmos_sensor_8(ctx, 0x01, 0x01);

	/* update FL RG value after setting buffer for writting RG */
	ctx->frame_length_rg = ctx->frame_length;
}	/*	set_dummy  */


static void set_max_framerate(struct subdrv_ctx *ctx,
		UINT16 framerate, kal_bool min_framelength_en)
{
	/*kal_int16 dummy_line;*/
	kal_uint32 frame_length = ctx->frame_length;

	OV08D10_LOG_DBG(
		"framerate = %d, min framelength should enable %d\n", framerate,
		min_framelength_en);

	frame_length = ctx->pclk / framerate * 10 / ctx->line_length;
	if (frame_length >= ctx->min_frame_length)
		ctx->frame_length = frame_length;
	else
		ctx->frame_length = ctx->min_frame_length;

	ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length) {
		ctx->frame_length = imgsensor_info.max_frame_length;
		ctx->dummy_line =
			ctx->frame_length - ctx->min_frame_length;
	}
	if (min_framelength_en)
		ctx->min_frame_length = ctx->frame_length;
	set_dummy(ctx);
}	/*	set_max_framerate  */

static void write_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter)
{
	kal_uint16 realtime_fps = 0;

	if(!soft_power_status){
		soft_power_up(ctx);
	}
	if (shutter > ctx->min_frame_length - imgsensor_info.margin)
		ctx->frame_length = shutter + imgsensor_info.margin;
	else
		ctx->frame_length = ctx->min_frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;

	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length -
		imgsensor_info.margin)) ? (imgsensor_info.max_frame_length -
		imgsensor_info.margin) : shutter;

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / ctx->line_length * 10
				/ ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
		else {
			/* Extend frame length */
			set_dummy(ctx);
		}
	} else {
		set_dummy(ctx);
	}

	// Update Shutter
	write_cmos_sensor_8(ctx, 0xfd, 0x01);
	write_cmos_sensor_8(ctx, 0x02, (shutter*2 >> 16) & 0xff);
	write_cmos_sensor_8(ctx, 0x03, (shutter*2 >> 8) & 0xff);
	write_cmos_sensor_8(ctx, 0x04,  shutter*2  & 0xff);
	write_cmos_sensor_8(ctx, 0x01, 0x01);

	OV08D10_LOG_INF("shutter = %d, framelength = %d\n", shutter, ctx->frame_length);
}	/*	write_shutter  */

/*************************************************************************
 * FUNCTION
 *	set_shutter
 *
 * DESCRIPTION
 *	This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *	iShutter : exposured lines
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(struct subdrv_ctx *ctx, kal_uint32 shutter)
{
	ctx->shutter = shutter;

	write_shutter(ctx, shutter);
} /* set_shutter */


static void set_frame_length(struct subdrv_ctx *ctx, kal_uint16 frame_length)
{
	if (frame_length > 1)
		ctx->frame_length = frame_length;

	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;
	if (ctx->min_frame_length > ctx->frame_length)
		ctx->frame_length = ctx->min_frame_length;

	set_dummy(ctx);

	OV08D10_LOG_INF("Framelength: set=%d/input=%d/min=%d\n",
		ctx->frame_length, frame_length, ctx->min_frame_length);
}


/*************************************************************************
 * FUNCTION
 *	set_shutter_frame_length
 *
 * DESCRIPTION
 *	for frame & 3A sync
 *
 *************************************************************************/
static void set_shutter_frame_length(struct subdrv_ctx *ctx, kal_uint16 shutter,
				     kal_uint16 frame_length,
				     kal_bool auto_extend_en)
{
	kal_uint16 realtime_fps = 0;
	kal_int32 dummy_line = 0;

	ctx->shutter = shutter;

	/* Change frame time */
	if (frame_length > 1)
		dummy_line = frame_length - ctx->frame_length;

	ctx->frame_length = ctx->frame_length + dummy_line;

	if (shutter > ctx->frame_length - imgsensor_info.margin)
		ctx->frame_length = shutter + imgsensor_info.margin;

	if (ctx->frame_length > imgsensor_info.max_frame_length)
		ctx->frame_length = imgsensor_info.max_frame_length;

	shutter = (shutter < imgsensor_info.min_shutter)
			? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
			? (imgsensor_info.max_frame_length - imgsensor_info.margin)
			: shutter;

	if (ctx->autoflicker_en) {
		realtime_fps = ctx->pclk / ctx->line_length * 10 /
				ctx->frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(ctx, 296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(ctx, 146, 0);
		else {
			set_dummy(ctx);
		}
	} else {
		set_dummy(ctx);
	}

	// Update Shutter
	write_cmos_sensor_8(ctx, 0xfd, 0x01);
	write_cmos_sensor_8(ctx, 0x02, (shutter*2 >> 16) & 0xff);
	write_cmos_sensor_8(ctx, 0x03, (shutter*2 >> 8) & 0xff);
	write_cmos_sensor_8(ctx, 0x04,  shutter*2  & 0xff);
	write_cmos_sensor_8(ctx, 0x01, 0x01);

	OV08D10_LOG_INF("set shutter =%d, framelength =%d\n", shutter, ctx->frame_length);

}	/* set_shutter_frame_length */

static void set_multi_shutter_frame_length(struct subdrv_ctx *ctx,
				kal_uint32 *shutters, kal_uint32 shutter_cnt,
				kal_uint32 frame_length)
{
	if (shutter_cnt == 1) {
		ctx->shutter = shutters[0];

		if (shutters[0] > ctx->min_frame_length - imgsensor_info.margin)
			ctx->frame_length = shutters[0] + imgsensor_info.margin;
		else
			ctx->frame_length = ctx->min_frame_length;
		if (frame_length > ctx->frame_length)
			ctx->frame_length = frame_length;
		if (ctx->frame_length > imgsensor_info.max_frame_length)
			ctx->frame_length = imgsensor_info.max_frame_length;
		if (shutters[0] < imgsensor_info.min_shutter)
			shutters[0] = imgsensor_info.min_shutter;

		set_dummy(ctx);
		write_cmos_sensor_8(ctx, 0xfd, 0x01);
		write_cmos_sensor_8(ctx, 0x02, (shutters[0]*2 >> 16) & 0xff);
		write_cmos_sensor_8(ctx, 0x03, (shutters[0]*2 >> 8) & 0xff);
		write_cmos_sensor_8(ctx, 0x04,  shutters[0]*2	& 0xff);
		write_cmos_sensor_8(ctx, 0x01, 0x01);
		write_cmos_sensor_8(ctx, 0xfd, 0x01);

		OV08D10_LOG_INF("shutter =%d, framelength =%d\n", shutters[0], ctx->frame_length);
	}
}


/*************************************************************************
 * FUNCTION
 *	set_gain
 *
 * DESCRIPTION
 *	This function is to set global gain to sensor.
 *
 * PARAMETERS
 *	iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *	the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 gain2reg(struct subdrv_ctx *ctx, const kal_uint16 gain)
{
	kal_uint16 iReg = 0x0000;

	iReg = gain * 16 / BASEGAIN;

	return iReg;
}
 
static kal_uint16 set_gain(struct subdrv_ctx *ctx, kal_uint16 gain)
{
	kal_uint8  reg_gain;

	if(!soft_power_status){
		soft_power_up(ctx);
	}

	if (gain < imgsensor_info.min_gain || gain > imgsensor_info.max_gain) {
		OV08D10_LOG_DBG("Error gain setting");
		if (gain < imgsensor_info.min_gain)
			gain = imgsensor_info.min_gain;
		else if (gain > imgsensor_info.max_gain)
			gain = imgsensor_info.max_gain;
	}

	reg_gain = gain2reg(ctx, gain);

	ctx->gain = reg_gain;
	OV08D10_LOG_INF("ctx->sensor_mode: %d, gain = %d, ctx->gain = 0x%x ,reg_gain = 0x%x, max_gain:0x%x\n ",
		ctx->sensor_mode, gain, ctx->gain, reg_gain, imgsensor_info.max_gain);

	write_cmos_sensor_8(ctx,0xfd, 0x01);
	write_cmos_sensor_8(ctx,0x24, reg_gain);
	write_cmos_sensor_8(ctx,0x21, 0x02);
	write_cmos_sensor_8(ctx,0x22, 0x00);//1x digital gain
	write_cmos_sensor_8(ctx,0x01, 0x01);

	return gain;
} /* set_gain */

static kal_uint32 duchampov08d10ultra_awb_gain(struct subdrv_ctx *ctx,
		struct SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
    MUINT32 r_Gain = pSetSensorAWB->ABS_GAIN_R;
    MUINT32 g_Gain = pSetSensorAWB->ABS_GAIN_GR;
    MUINT32 b_Gain = pSetSensorAWB->ABS_GAIN_B;


    OV08D10_LOG_DBG("write awb gain r:g:b %d:%d:%d \n", r_Gain, g_Gain, b_Gain);

	return ERROR_NONE;
}


static void duchampov08d10ultra_set_lsc_reg_setting(struct subdrv_ctx *ctx,
		kal_uint8 index, kal_uint16 *regDa, MUINT32 regNum)
{

}
/*************************************************************************
 * FUNCTION
 *	night_mode
 *
 * DESCRIPTION
 *	This function night mode of sensor.
 *
 * PARAMETERS
 *	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/

static void soft_power_up(struct subdrv_ctx *ctx)
{
	write_cmos_sensor_8(ctx, 0xfd, 0x00);
	write_cmos_sensor_8(ctx, 0xc2, 0x30);
	write_cmos_sensor_8(ctx, 0x21, 0x0e);
	write_cmos_sensor_8(ctx, 0x21, 0x00);
	soft_power_status = true;
	OV08D10_LOG_INF("soft_power_up\n");
}

static kal_uint32 streaming_control(struct subdrv_ctx *ctx, kal_bool enable)
{
// @@  Soft power mipi stream on
// 6c fd 00
// 6c c2 30
// 6c 21 0e
// 6c 21 00
// 6c fd 01
// 6c 01 03
// 6c fd 00
// 6c 20 0f
// 6c e7 03
// 6c e7 00
// 6c a0 01

// @@ Soft power mipi stream off
// 6c fd 00
// 6c a0 00
// ;delay 10ms
// 6c 20 0b
// 6c c2 32
// 6c 21 0f
	OV08D10_LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
	if (enable) {
		write_cmos_sensor_8(ctx, 0xfd, 0x00);
		write_cmos_sensor_8(ctx, 0xc2, 0x30);
		write_cmos_sensor_8(ctx, 0x21, 0x0e);
		write_cmos_sensor_8(ctx, 0x21, 0x00);
		write_cmos_sensor_8(ctx, 0xfd, 0x01);
		write_cmos_sensor_8(ctx, 0x01, 0x03);
		write_cmos_sensor_8(ctx, 0xfd, 0x00);
		write_cmos_sensor_8(ctx, 0x20, 0x0f);
		write_cmos_sensor_8(ctx, 0xe7, 0x03);
		write_cmos_sensor_8(ctx, 0xe7, 0x00);
		write_cmos_sensor_8(ctx, 0xa0, 0x01);
		soft_power_status = true;
	} else {
		write_cmos_sensor_8(ctx, 0xfd, 0x00);
		write_cmos_sensor_8(ctx, 0xa0, 0x00);
		mdelay(10);
		write_cmos_sensor_8(ctx, 0x20, 0x0b);
		write_cmos_sensor_8(ctx, 0xc2, 0x32);
		write_cmos_sensor_8(ctx, 0x21, 0x0f);
		soft_power_status = false;
	}
	mdelay(10);

	return ERROR_NONE;
}

static void sensor_init(struct subdrv_ctx *ctx)
{
	OV08D10_LOG_INF("+\n");
	// write_cmos_sensor_8(ctx, 0xfd, 0x00);
	// write_cmos_sensor_8(ctx, 0xa0, 0x00);
	// mdelay(10);
	// write_cmos_sensor_8(ctx, 0xc2, 0x32);
	// write_cmos_sensor_8(ctx, 0x21, 0x0f);
	OV08D10_LOG_INF("-\n");
}	/*	  sensor_init  */

static void preview_setting(struct subdrv_ctx *ctx)
{
	OV08D10_LOG_INF("+\n");
	write_cmos_sensor_8(ctx, 0xFD, 0x00);
	write_cmos_sensor_8(ctx, 0x20, 0x0E);
	mdelay(3);
	duchampov08d10ultra_table_write_cmos_sensor(ctx, duchampov08d10ultra_preview_setting,
		sizeof(duchampov08d10ultra_preview_setting) / sizeof(kal_uint16));

	streaming_control(ctx, false);
	OV08D10_LOG_INF("-\n");
} /* preview_setting */

static void capture_setting(struct subdrv_ctx *ctx)
{
	OV08D10_LOG_INF("+\n");
	preview_setting(ctx);
	OV08D10_LOG_INF("-\n");
}

static void normal_video_setting(struct subdrv_ctx *ctx)
{
	OV08D10_LOG_INF("+\n");
	write_cmos_sensor_8(ctx, 0xFD, 0x00);
	write_cmos_sensor_8(ctx, 0x20, 0x0E);
	mdelay(3);
	duchampov08d10ultra_table_write_cmos_sensor(ctx, duchampov08d10ultra_normal_video_setting,
		sizeof(duchampov08d10ultra_normal_video_setting) / sizeof(kal_uint16));
	streaming_control(ctx, false);
	OV08D10_LOG_INF("-\n");
}

static void hs_video_setting(struct subdrv_ctx *ctx)
{
	OV08D10_LOG_INF("+\n");
	preview_setting(ctx);
	OV08D10_LOG_INF("-\n");
}

static void slim_video_setting(struct subdrv_ctx *ctx)
{
	OV08D10_LOG_INF("+\n");
	preview_setting(ctx);
	OV08D10_LOG_INF("-\n");
}

static void custom1_setting(struct subdrv_ctx *ctx)
{
	OV08D10_LOG_INF("+\n");
	write_cmos_sensor_8(ctx, 0xFD, 0x00);
	write_cmos_sensor_8(ctx, 0x20, 0x0E);
	mdelay(3);
	duchampov08d10ultra_table_write_cmos_sensor(ctx, duchampov08d10ultra_custom1_setting,
		sizeof(duchampov08d10ultra_custom1_setting) / sizeof(kal_uint16));
	streaming_control(ctx, false);
	OV08D10_LOG_INF("-\n");
}

static void custom2_setting(struct subdrv_ctx *ctx)
{
	OV08D10_LOG_INF("+\n");
	write_cmos_sensor_8(ctx, 0xFD, 0x00);
	write_cmos_sensor_8(ctx, 0x20, 0x0E);
	mdelay(3);
	duchampov08d10ultra_table_write_cmos_sensor(ctx, duchampov08d10ultra_custom2_setting,
		sizeof(duchampov08d10ultra_custom2_setting) / sizeof(kal_uint16));
	streaming_control(ctx, false);
	OV08D10_LOG_INF("+\n");
}

static void custom3_setting(struct subdrv_ctx *ctx)
{
	OV08D10_LOG_INF("+\n");
	preview_setting(ctx);
	OV08D10_LOG_INF("-\n");
}

static void custom4_setting(struct subdrv_ctx *ctx)
{
	OV08D10_LOG_INF("+\n");
	preview_setting(ctx);
	OV08D10_LOG_INF("-\n");
}

static void custom5_setting(struct subdrv_ctx *ctx)
{
	OV08D10_LOG_INF("+\n");
	preview_setting(ctx);
	OV08D10_LOG_INF("-\n");
}

static void custom6_setting(struct subdrv_ctx *ctx)
{
	OV08D10_LOG_INF("+\n");
	preview_setting(ctx);
	OV08D10_LOG_INF("-\n");
}

static kal_uint32 return_sensor_id(struct subdrv_ctx *ctx)
{
	kal_uint32 sensor_id = 0;

	write_cmos_sensor_8(ctx, 0xfd, 0x00);
	sensor_id = ((read_cmos_sensor_8(ctx, 0x00) << 8) | read_cmos_sensor_8(ctx, 0x01));

	OV08D10_LOG_INF("[%s] sensor_id: 0x%x", __func__, sensor_id);

	return sensor_id;
}

/*************************************************************************
 * FUNCTION
 *	get_imgsensor_id
 *
 * DESCRIPTION
 *	This function get the sensor ID
 *
 * PARAMETERS
 *	*sensorID : return the sensor ID
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static int get_imgsensor_id(struct subdrv_ctx *ctx, UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			*sensor_id = return_sensor_id(ctx);

			if (*sensor_id == imgsensor_info.sensor_id) {
				OV08D10_LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}

			OV08D10_LOG_INF("Read sensor id fail, id: 0x%x\n",
				ctx->i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		/*if Sensor ID is not correct,
		 *Must set *sensor_id to 0xFFFFFFFF
		 */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}


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
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0;

	OV08D10_LOG_INF("+\n");
	/*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
	 *we should detect the module used i2c address
	 */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		ctx->i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			sensor_id = return_sensor_id(ctx);

			if (sensor_id == imgsensor_info.sensor_id) {
				OV08D10_LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					ctx->i2c_write_id, sensor_id);
				break;
			}
			OV08D10_LOG_INF("Read sensor id fail, id: 0x%x\n",
				ctx->i2c_write_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail sequence write in  */
	sensor_init(ctx);

	ctx->autoflicker_en = KAL_FALSE;
	ctx->sensor_mode = IMGSENSOR_MODE_INIT;
	ctx->shutter = 0x3D0; 
	ctx->gain = 4 * BASEGAIN;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->dummy_pixel = 0;
	ctx->dummy_line = 0;
	ctx->ihdr_mode = 0;
	ctx->test_pattern = KAL_FALSE;
	ctx->current_fps = imgsensor_info.pre.max_framerate;
	OV08D10_LOG_INF("-\n");

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
static int close(struct subdrv_ctx *ctx)
{
	OV08D10_LOG_INF("E\n");
	/* No Need to implement this function */
	streaming_control(ctx, KAL_FALSE);

	return ERROR_NONE;
} /* close */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *	This function start the sensor preview.
 *
 * PARAMETERS
 *	*image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	OV08D10_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	ctx->vblank_convert = 2504;

	preview_setting(ctx);

	return ERROR_NONE;
} /* preview */
static kal_uint32 custom7_15(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	OV08D10_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_PREVIEW;
	ctx->pclk = imgsensor_info.pre.pclk;
	ctx->line_length = imgsensor_info.pre.linelength;
	ctx->frame_length = imgsensor_info.pre.framelength;
	ctx->min_frame_length = imgsensor_info.pre.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	preview_setting(ctx);

	return ERROR_NONE;
} /* preview */

/*************************************************************************
 * FUNCTION
 *	capture
 *
 * DESCRIPTION
 *	This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *	None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	OV08D10_LOG_INF("E\n");
	ctx->sensor_mode = IMGSENSOR_MODE_CAPTURE;

	if (ctx->current_fps != imgsensor_info.cap.max_framerate)
		OV08D10_LOG_INF(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
			ctx->current_fps,
			imgsensor_info.cap.max_framerate / 10);
	ctx->pclk = imgsensor_info.cap.pclk;
	ctx->line_length = imgsensor_info.cap.linelength;
	ctx->frame_length = imgsensor_info.cap.framelength;
	ctx->min_frame_length = imgsensor_info.cap.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	ctx->vblank_convert = 2504;

	capture_setting(ctx);

	return ERROR_NONE;
}	/* capture(ctx) */
static kal_uint32 normal_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	OV08D10_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_VIDEO;
	ctx->pclk = imgsensor_info.normal_video.pclk;
	ctx->line_length = imgsensor_info.normal_video.linelength;
	ctx->frame_length = imgsensor_info.normal_video.framelength;
	ctx->min_frame_length = imgsensor_info.normal_video.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	ctx->vblank_convert = 2504;
	normal_video_setting(ctx);

	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(struct subdrv_ctx *ctx, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
				MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	OV08D10_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	ctx->pclk = imgsensor_info.hs_video.pclk;
	/*ctx->video_mode = KAL_TRUE;*/
	ctx->line_length = imgsensor_info.hs_video.linelength;
	ctx->frame_length = imgsensor_info.hs_video.framelength;
	ctx->min_frame_length = imgsensor_info.hs_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	/*ctx->current_fps = 300;*/
	ctx->autoflicker_en = KAL_FALSE;
	ctx->vblank_convert = 2504;
	hs_video_setting(ctx);

	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	OV08D10_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	ctx->pclk = imgsensor_info.slim_video.pclk;
	/*ctx->video_mode = KAL_TRUE;*/
	ctx->line_length = imgsensor_info.slim_video.linelength;
	ctx->frame_length = imgsensor_info.slim_video.framelength;
	ctx->min_frame_length = imgsensor_info.slim_video.framelength;
	ctx->dummy_line = 0;
	ctx->dummy_pixel = 0;
	/*ctx->current_fps = 300;*/
	ctx->autoflicker_en = KAL_FALSE;
	ctx->vblank_convert = 2504;
	slim_video_setting(ctx);

	return ERROR_NONE;
}	/* slim_video */


static kal_uint32 custom1(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	OV08D10_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	ctx->pclk = imgsensor_info.custom1.pclk;
	ctx->line_length = imgsensor_info.custom1.linelength;
	ctx->frame_length = imgsensor_info.custom1.framelength;
	ctx->min_frame_length = imgsensor_info.custom1.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	ctx->vblank_convert = 1252;

	custom1_setting(ctx);

	return ERROR_NONE;
}	/* custom1 */

static kal_uint32 custom2(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	OV08D10_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	ctx->pclk = imgsensor_info.custom2.pclk;
	ctx->line_length = imgsensor_info.custom2.linelength;
	ctx->frame_length = imgsensor_info.custom2.framelength;
	ctx->min_frame_length = imgsensor_info.custom2.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	ctx->vblank_convert = 2504;
	custom2_setting(ctx);

	return ERROR_NONE;
}	/* custom2 */

static kal_uint32 custom3(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	OV08D10_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	ctx->pclk = imgsensor_info.custom3.pclk;
	ctx->line_length = imgsensor_info.custom3.linelength;
	ctx->frame_length = imgsensor_info.custom3.framelength;
	ctx->min_frame_length = imgsensor_info.custom3.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom3_setting(ctx);

	return ERROR_NONE;
}	/* custom3 */

static kal_uint32 custom4(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	OV08D10_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	ctx->pclk = imgsensor_info.custom4.pclk;
	ctx->line_length = imgsensor_info.custom4.linelength;
	ctx->frame_length = imgsensor_info.custom4.framelength;
	ctx->min_frame_length = imgsensor_info.custom4.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	custom4_setting(ctx);

	return ERROR_NONE;
}	/* custom4 */

static kal_uint32 custom5(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	OV08D10_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	ctx->pclk = imgsensor_info.custom5.pclk;
	ctx->line_length = imgsensor_info.custom5.linelength;
	ctx->frame_length = imgsensor_info.custom5.framelength;
	ctx->min_frame_length = imgsensor_info.custom5.framelength;
	ctx->autoflicker_en = KAL_FALSE;
	custom5_setting(ctx);

	return ERROR_NONE;
}	/* custom5 */

static kal_uint32 custom6(struct subdrv_ctx *ctx,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	OV08D10_LOG_INF("E\n");

	ctx->sensor_mode = IMGSENSOR_MODE_CUSTOM6;
	ctx->pclk = imgsensor_info.custom6.pclk;
	ctx->line_length = imgsensor_info.custom6.linelength;
	ctx->frame_length = imgsensor_info.custom6.framelength;
	ctx->min_frame_length = imgsensor_info.custom5.framelength;
	ctx->autoflicker_en = KAL_FALSE;

	custom6_setting(ctx);

	return ERROR_NONE;
}	/* custom6 */
static int get_resolution(struct subdrv_ctx *ctx,
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	int i = 0;

	for (i = SENSOR_SCENARIO_ID_MIN; i < SENSOR_SCENARIO_ID_MAX; i++) {
		if (i < imgsensor_info.sensor_mode_num) {
			sensor_resolution->SensorWidth[i] = imgsensor_winsize_info[i].w2_tg_size;
			sensor_resolution->SensorHeight[i] = imgsensor_winsize_info[i].h2_tg_size;
		} else {
			sensor_resolution->SensorWidth[i] = 0;
			sensor_resolution->SensorHeight[i] = 0;
		}
	}

	return ERROR_NONE;
} /* get_resolution */

static int get_info(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_INFO_STRUCT *sensor_info,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	int i;
	OV08D10_LOG_DBG("scenario_id = %d\n", scenario_id);

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SensorOutputDataFormat =
		imgsensor_info.sensor_output_dataformat;

	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_PREVIEW] =
		imgsensor_info.pre_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_CAPTURE] =
		imgsensor_info.cap_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_NORMAL_VIDEO] =
		imgsensor_info.video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO] =
		imgsensor_info.hs_video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_SLIM_VIDEO] =
		imgsensor_info.slim_video_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM1] =
		imgsensor_info.custom1_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM2] =
		imgsensor_info.custom2_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM3] =
		imgsensor_info.custom3_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM4] =
		imgsensor_info.custom4_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM5] =
		imgsensor_info.custom5_delay_frame;
	sensor_info->DelayFrame[SENSOR_SCENARIO_ID_CUSTOM6] =
		imgsensor_info.custom6_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame =
		imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame =
		imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->TEMPERATURE_SUPPORT = imgsensor_info.temperature_support;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */


	sensor_info->SensorWidthSampling = 0; /* 0 is default 1x */
	sensor_info->SensorHightSampling = 0; /* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	sensor_info->FrameTimeDelayFrame =
		imgsensor_info.frame_time_delay_frame;

	for (i = 0; i < SENSOR_SCENARIO_ID_MAX; i++) {
		sensor_info->gain_ratio[i] = 1000;
		sensor_info->OB_pedestals[i] = imgsensor_info.ob_pedestal;
		sensor_info->saturation_level[i] = 1023;
	}

	return ERROR_NONE;
}	/*	get_info  */


static int control(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	OV08D10_LOG_DBG("scenario_id = %d\n", scenario_id);
	ctx->current_scenario_id = scenario_id;
	switch (scenario_id) {

	case SENSOR_SCENARIO_ID_CUSTOM7:
	case SENSOR_SCENARIO_ID_CUSTOM8:
	case SENSOR_SCENARIO_ID_CUSTOM9:
	case SENSOR_SCENARIO_ID_CUSTOM10:
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM12:
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
		//imgsensor.sensor_mode = scenario_id;
		custom7_15(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		preview(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		capture(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		normal_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		hs_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		slim_video(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		custom1(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		custom2(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		custom3(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM4:
		custom4(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		custom5(ctx, image_window, sensor_config_data);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM6:
		custom6(ctx, image_window, sensor_config_data);
		break;
	default:
		OV08D10_LOG_DBG("Error ScenarioId setting");
		preview(ctx, image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}

	return ERROR_NONE;
}	/* control(ctx) */



static kal_uint32 set_video_mode(struct subdrv_ctx *ctx, UINT16 framerate)
{
	OV08D10_LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	if ((framerate == 300) && (ctx->autoflicker_en == KAL_TRUE))
		ctx->current_fps = 296;
	else if ((framerate == 150) && (ctx->autoflicker_en == KAL_TRUE))
		ctx->current_fps = 146;
	else
		ctx->current_fps = framerate;

	set_max_framerate(ctx, ctx->current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(struct subdrv_ctx *ctx,
		kal_bool enable, UINT16 framerate)
{
	if (enable) /*enable auto flicker*/ {
		ctx->autoflicker_en = KAL_TRUE;
		OV08D10_LOG_DBG("enable! fps = %d", framerate);
	} else {
		 /*Cancel Auto flick*/
		ctx->autoflicker_en = KAL_FALSE;
	}

	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	OV08D10_LOG_DBG("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_CUSTOM7:
	case SENSOR_SCENARIO_ID_CUSTOM8:
	case SENSOR_SCENARIO_ID_CUSTOM9:
	case SENSOR_SCENARIO_ID_CUSTOM10:
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM12:
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10
				/ imgsensor_info.pre.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
		? (frame_length - imgsensor_info.pre.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.pre.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk /
				framerate * 10 /
				imgsensor_info.normal_video.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.normal_video.framelength)
		? (frame_length - imgsensor_info.normal_video.framelength)
		: 0;
		ctx->frame_length =
			imgsensor_info.normal_video.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	if (ctx->current_fps != imgsensor_info.cap.max_framerate)
		OV08D10_LOG_DBG(
			"Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n"
			, framerate, imgsensor_info.cap.max_framerate/10);
		frame_length = imgsensor_info.cap.pclk / framerate * 10
				/ imgsensor_info.cap.linelength;
			ctx->dummy_line =
			(frame_length > imgsensor_info.cap.framelength)
			  ? (frame_length - imgsensor_info.cap.framelength) : 0;
			ctx->frame_length =
				imgsensor_info.cap.framelength
				+ ctx->dummy_line;
			ctx->min_frame_length = ctx->frame_length;

		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10
				/ imgsensor_info.hs_video.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.hs_video.framelength)
			  ? (frame_length - imgsensor_info.hs_video.framelength)
			  : 0;
		ctx->frame_length =
			imgsensor_info.hs_video.framelength
				+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10
			/ imgsensor_info.slim_video.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.slim_video.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10
				/ imgsensor_info.custom1.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom1.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10
				/ imgsensor_info.custom2.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom2.framelength)
			? (frame_length - imgsensor_info.custom2.framelength)
			: 0;
		ctx->frame_length =
			imgsensor_info.custom2.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		frame_length = imgsensor_info.custom3.pclk / framerate * 10
				/ imgsensor_info.custom3.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom3.framelength)
		? (frame_length - imgsensor_info.custom3.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom3.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM4:
		frame_length = imgsensor_info.custom4.pclk / framerate * 10
				/ imgsensor_info.custom4.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom4.framelength)
		? (frame_length - imgsensor_info.custom4.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom4.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		frame_length = imgsensor_info.custom5.pclk / framerate * 10
				/ imgsensor_info.custom5.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom5.framelength)
		? (frame_length - imgsensor_info.custom5.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom5.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	case SENSOR_SCENARIO_ID_CUSTOM6:
		frame_length = imgsensor_info.custom6.pclk / framerate * 10
				/ imgsensor_info.custom6.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.custom6.framelength)
		? (frame_length - imgsensor_info.custom6.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.custom6.framelength
			+ ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		break;
	default:  /*coding with  preview scenario by default*/
		frame_length = imgsensor_info.pre.pclk / framerate * 10
			/ imgsensor_info.pre.linelength;
		ctx->dummy_line =
			(frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		ctx->frame_length =
			imgsensor_info.pre.framelength + ctx->dummy_line;
		ctx->min_frame_length = ctx->frame_length;
		if (ctx->frame_length > ctx->shutter)
			set_dummy(ctx);
		OV08D10_LOG_DBG("error scenario_id = %d, we use preview scenario\n",
			scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	OV08D10_LOG_DBG("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {

	case SENSOR_SCENARIO_ID_CUSTOM7:
	case SENSOR_SCENARIO_ID_CUSTOM8:
	case SENSOR_SCENARIO_ID_CUSTOM9:
	case SENSOR_SCENARIO_ID_CUSTOM10:
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM12:
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM4:
		*framerate = imgsensor_info.custom4.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM5:
		*framerate = imgsensor_info.custom5.max_framerate;
		break;
	case SENSOR_SCENARIO_ID_CUSTOM6:
		*framerate = imgsensor_info.custom6.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(struct subdrv_ctx *ctx, kal_bool enable)
{
	OV08D10_LOG_DBG("enable: %d\n", enable);
	return ERROR_NONE;
}

static kal_uint32 get_sensor_temperature(struct subdrv_ctx *ctx)
{
	return 0;
}

static int feature_control(
		struct subdrv_ctx *ctx,
		MSDK_SENSOR_FEATURE_ENUM feature_id,
		UINT8 *feature_para,
		UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	/* unsigned long long *feature_return_para
	 *  = (unsigned long long *) feature_para;
	 */
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	struct SENSOR_VC_INFO_STRUCT *pvcinfo;
	/* SET_SENSOR_AWB_GAIN *pSetSensorAWB
	 *  = (SET_SENSOR_AWB_GAIN *)feature_para;
	 */
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data
		= (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	/*OV08D10_LOG_DBG("feature_id = %d\n", feature_id);*/
	switch (feature_id) {
	case SENSOR_FEATURE_GET_OUTPUT_FORMAT_BY_SCENARIO:
		switch (*feature_data) {
		default:
			*(feature_data + 1)
			= (enum ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM)
				imgsensor_info.sensor_output_dataformat;
			break;
		}
		OV08D10_LOG_DBG("SENSOR_FEATURE_GET_OUTPUT_FORMAT_BY_SCENARIO get:%llu\n",*(feature_data + 1));
	break;
	case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
			break;
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
		if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL) {
			*(feature_data + 0) =
				sizeof(duchampov08d10ultra_ana_gain_table);
		} else {
			memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)duchampov08d10ultra_ana_gain_table,
			sizeof(duchampov08d10ultra_ana_gain_table));
		}
		break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		*(feature_data + 2) = imgsensor_info.exp_step;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.pclk;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom4.pclk;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom5.pclk;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.pclk;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.cap.framelength << 16)
				+ imgsensor_info.cap.linelength;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.normal_video.framelength << 16)
				+ imgsensor_info.normal_video.linelength;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.hs_video.framelength << 16)
				+ imgsensor_info.hs_video.linelength;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.slim_video.framelength << 16)
				+ imgsensor_info.slim_video.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom1.framelength << 16)
				+ imgsensor_info.custom1.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom2.framelength << 16)
				+ imgsensor_info.custom2.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom3.framelength << 16)
				+ imgsensor_info.custom3.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom4.framelength << 16)
				+ imgsensor_info.custom4.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom5.framelength << 16)
				+ imgsensor_info.custom5.linelength;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.custom6.framelength << 16)
				+ imgsensor_info.custom6.linelength;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
			= (imgsensor_info.pre.framelength << 16)
				+ imgsensor_info.pre.linelength;
			break;
		}
		break;

	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = ctx->line_length;
		*feature_return_para_16 = ctx->frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = ctx->pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		 set_shutter(ctx, *feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		 /* night_mode((BOOL) *feature_data); */
		break;
	#ifdef VENDOR_EDIT
	case SENSOR_FEATURE_CHECK_MODULE_ID:
		*feature_return_para_32 = imgsensor_info.module_id;
		break;
	#endif
	case SENSOR_FEATURE_SET_GAIN:
		set_gain(ctx, (UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor_8(ctx, sensor_reg_data->RegAddr,
				    sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData =
			read_cmos_sensor_8(ctx, sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/*get the lens driver ID from EEPROM
		 * or just return LENS_DRIVER_ID_DO_NOT_CARE
		 * if EEPROM does not exist in camera module.
		 */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(ctx, *feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(ctx, feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode(ctx, (BOOL)*feature_data_16,
				      *(feature_data_16+1));
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_32 = get_sensor_temperature(ctx);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		 set_max_framerate_by_scenario(ctx,
				(enum MSDK_SCENARIO_ID_ENUM)*feature_data,
				*(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		 get_default_framerate_by_scenario(ctx,
				(enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
				(MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		OV08D10_LOG_DBG("SENSOR_FEATURE_GET_PDAF_DATA\n");
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode(ctx, (BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		OV08D10_LOG_DBG("current fps :%d\n", (UINT32)*feature_data_32);
		ctx->current_fps = *feature_data_32;
		break;
	case SENSOR_FEATURE_SET_HDR:
		OV08D10_LOG_DBG("ihdr enable :%d\n", (BOOL)*feature_data_32);
		ctx->ihdr_mode = *feature_data_32;
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		wininfo =
	(struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

		switch (*feature_data_32) {
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[2],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[5],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo,
				(void *)&imgsensor_winsize_info[6],
				sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM3:
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM5:
		case SENSOR_SCENARIO_ID_CUSTOM6:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			memcpy((void *)wininfo,
			(void *)&imgsensor_winsize_info[0],
			sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		OV08D10_LOG_DBG("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
			(UINT16)*feature_data,
			(UINT16)*(feature_data+1),
			(UINT16)*(feature_data+2));
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		set_shutter_frame_length(ctx, (UINT16) (*feature_data),
					(UINT16) (*(feature_data + 1)),
					(BOOL) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		/*
		 * 1, if driver support new sw frame sync
		 * set_shutter_frame_length(ctx) support third para auto_extend_en
		 */
		*(feature_data + 1) = 1;
		/* margin info by scenario */
		*(feature_data + 2) = imgsensor_info.margin;
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		OV08D10_LOG_DBG("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",
			(UINT16)*feature_data, (UINT16)*(feature_data+1));
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		OV08D10_LOG_DBG("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(ctx, KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		OV08D10_LOG_DBG("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n",
			*feature_data);
		if (*feature_data != 0)
			set_shutter(ctx, *feature_data);
		streaming_control(ctx, KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		switch (*(feature_data + 1)) {
		default:
			*feature_return_para_32 = 1000; /*BINNING_AVERAGED*/
			break;
		}
		OV08D10_LOG_DBG("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
			*feature_return_para_32);
		*feature_para_len = 4;

		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		switch (*feature_data) {
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM1:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom1.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM2:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom2.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM3:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom3.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM4:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom4.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM5:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom5.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_CUSTOM6:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.custom6.mipi_pixel_rate;
			break;
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
	}
		break;
	case SENSOR_FEATURE_GET_VC_INFO:
		pvcinfo =
		 (struct SENSOR_VC_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM3:
		case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
		case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
		case SENSOR_SCENARIO_ID_SLIM_VIDEO:
		case SENSOR_SCENARIO_ID_CUSTOM1:
		case SENSOR_SCENARIO_ID_CUSTOM2:
		case SENSOR_SCENARIO_ID_CUSTOM4:
		case SENSOR_SCENARIO_ID_CUSTOM5:
		case SENSOR_SCENARIO_ID_CUSTOM6:
		case SENSOR_SCENARIO_ID_CUSTOM7:
		case SENSOR_SCENARIO_ID_CUSTOM8:
		case SENSOR_SCENARIO_ID_CUSTOM9:
		case SENSOR_SCENARIO_ID_CUSTOM10:
		case SENSOR_SCENARIO_ID_CUSTOM11:
		case SENSOR_SCENARIO_ID_CUSTOM12:
		case SENSOR_SCENARIO_ID_CUSTOM13:
		case SENSOR_SCENARIO_ID_CUSTOM14:
		case SENSOR_SCENARIO_ID_CUSTOM15:
		default:
			break;
		}
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		/* modify to separate 3hdr and remosaic */
		duchampov08d10ultra_awb_gain(ctx,
			(struct SET_SENSOR_AWB_GAIN *) feature_para);
		break;
	case SENSOR_FEATURE_SET_LSC_TBL:
		{
		kal_uint8 index =
			*(((kal_uint8 *)feature_para) + (*feature_para_len));

		duchampov08d10ultra_set_lsc_reg_setting(ctx, index, feature_data_16,
					  (*feature_para_len)/sizeof(UINT16));
		}
		break;
	case SENSOR_FEATURE_SET_FRAMELENGTH:
		set_frame_length(ctx, (UINT16) (*feature_data));
		break;
	case SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME:
		set_multi_shutter_frame_length(ctx, (UINT32 *)(*feature_data),
				(UINT32) (*(feature_data + 1)),
				(UINT32) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_MULTI_EXP_SHUTTER_RANGE_BY_SCENARIO:
		*(feature_data + 1)= ctx->s_ctx.mode[*feature_data].exp_cnt
			? ctx->s_ctx.mode[ *feature_data].exp_cnt : 1;
		memcpy((void *)(*(feature_data + 2)),
			(void *)ctx->s_ctx.mode[ *feature_data].multi_exposure_shutter_range,
			sizeof(struct u64_min_max)*IMGSENSOR_EXPOSURE_CNT);
		break;
	case SENSOR_FEATURE_GET_MAX_EXP_LINE:
	case SENSOR_FEATURE_GET_STAGGER_MAX_EXP_TIME:
		*(feature_data + 2) = ctx->s_ctx.exposure_max;
		break;
	default:
		break;
	}

	return ERROR_NONE;
} /* feature_control(ctx) */


#ifdef IMGSENSOR_VC_ROUTING
static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 2448,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_video[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 3264,
			.vsize = 1840,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_custom1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 1632,
			.vsize =  920,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_custom2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2880,
			.vsize = 2160,
		},
	},
};


static int get_frame_desc(struct subdrv_ctx *ctx,
		int scenario_id, struct mtk_mbus_frame_desc *fd)
{
	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_video);
		memcpy(fd->entry, frame_desc_video, sizeof(frame_desc_video));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM1:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_custom1);
		memcpy(fd->entry, frame_desc_custom1, sizeof(frame_desc_custom1));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM2:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_custom2);
		memcpy(fd->entry, frame_desc_custom2, sizeof(frame_desc_custom2));
		break;
	case SENSOR_SCENARIO_ID_CUSTOM7:
	case SENSOR_SCENARIO_ID_CUSTOM8:
	case SENSOR_SCENARIO_ID_CUSTOM9:
	case SENSOR_SCENARIO_ID_CUSTOM10:
	case SENSOR_SCENARIO_ID_CUSTOM11:
	case SENSOR_SCENARIO_ID_CUSTOM12:
	case SENSOR_SCENARIO_ID_CUSTOM13:
	case SENSOR_SCENARIO_ID_CUSTOM14:
	case SENSOR_SCENARIO_ID_CUSTOM15:
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
	case SENSOR_SCENARIO_ID_CUSTOM3:
	case SENSOR_SCENARIO_ID_CUSTOM4:
	case SENSOR_SCENARIO_ID_CUSTOM6:
	default:
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ARRAY_SIZE(frame_desc_prev);
		memcpy(fd->entry, frame_desc_prev, sizeof(frame_desc_prev));
		break;
	}

	return 0;
}
#endif


static const struct subdrv_ctx defctx = {

	.ana_gain_def  = 4 * BASEGAIN,
	.ana_gain_max  = 15.5 * BASEGAIN,
	.ana_gain_min  = 1 * BASEGAIN,
	.ana_gain_step = 1,
	.exposure_def  = 0x3D0,
	.exposure_max  = 0x3FFFEB,
	.exposure_min  = 4,
	.exposure_step = 1,
	.frame_time_delay_frame = 2,
	.margin                 = 20,
	.max_frame_length       = 0x3fffff,

	.mirror                 = IMAGE_NORMAL, /* mirrorflip information */
	.sensor_mode            = IMGSENSOR_MODE_INIT,
	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
	 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
	 */
	.shutter        = 0x3D0,	/* current shutter */
	.gain           = 4 * BASEGAIN,		/* current gain */
	.dummy_pixel    = 0,	/* current dummypixel */
	.dummy_line     = 0,	/* current dummyline */
	.current_fps    = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern   = KAL_FALSE,
	.current_scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW,
	.ihdr_mode      = 0, /* sensor need support LE, SE with HDR feature */
	.i2c_write_id   = 0x6C, /* record current sensor's i2c write id */
};

static struct subdrv_mode_struct mode_struct[SENSOR_MODE_NUM] = {};
static void static_ctx_init(struct subdrv_ctx *ctx)
{
	int i, j, exp_cnt;
	ctx->s_ctx.sensor_output_dataformat = imgsensor_info.sensor_output_dataformat;
	ctx->s_ctx.mode = mode_struct;
	ctx->s_ctx.sensor_mode_num = SENSOR_MODE_NUM;
	ctx->s_ctx.ana_gain_min = ctx->ana_gain_min;
	ctx->s_ctx.ana_gain_max = ctx->ana_gain_max;
	ctx->s_ctx.exposure_min = ctx->exposure_min;
	ctx->s_ctx.exposure_max = ctx->exposure_max;

	//copy from subdrv_ctx_init
	for (i = 0; i < ctx->s_ctx.sensor_mode_num ; i++) {
		if (!ctx->s_ctx.mode[i].sensor_output_dataformat)
			ctx->s_ctx.mode[i].sensor_output_dataformat =
				ctx->s_ctx.sensor_output_dataformat;
		if (!ctx->s_ctx.mode[i].ana_gain_min)
			ctx->s_ctx.mode[i].ana_gain_min = ctx->s_ctx.ana_gain_min;
		if (!ctx->s_ctx.mode[i].ana_gain_max)
			ctx->s_ctx.mode[i].ana_gain_max = ctx->s_ctx.ana_gain_max;
		if (!ctx->s_ctx.mode[i].dig_gain_min)
			ctx->s_ctx.mode[i].dig_gain_min = ctx->s_ctx.dig_gain_min;
		if (!ctx->s_ctx.mode[i].dig_gain_max)
			ctx->s_ctx.mode[i].dig_gain_max = ctx->s_ctx.dig_gain_max;
		if (!ctx->s_ctx.mode[i].min_exposure_line)
			ctx->s_ctx.mode[i].min_exposure_line = ctx->exposure_min;
		if (!ctx->s_ctx.mode[i].saturation_info)
			ctx->s_ctx.mode[i].saturation_info = ctx->s_ctx.saturation_info;
		exp_cnt = ctx->s_ctx.mode[i].exp_cnt ?
					ctx->s_ctx.mode[i].exp_cnt : 1;
		for (j = 0; j < exp_cnt; j++) {
			if (!ctx->s_ctx.mode[i].multi_exposure_ana_gain_range[j].min)
				ctx->s_ctx.mode[i].multi_exposure_ana_gain_range[j].min =
					ctx->s_ctx.ana_gain_min;
			if (!ctx->s_ctx.mode[i].multi_exposure_ana_gain_range[j].max)
				ctx->s_ctx.mode[i].multi_exposure_ana_gain_range[j].max =
					ctx->s_ctx.ana_gain_max;
			if (!ctx->s_ctx.mode[i].multi_exposure_shutter_range[j].min)
				ctx->s_ctx.mode[i].multi_exposure_shutter_range[j].min =
					ctx->s_ctx.exposure_min;
			if (!ctx->s_ctx.mode[i].multi_exposure_shutter_range[j].max)
				ctx->s_ctx.mode[i].multi_exposure_shutter_range[j].max =
					ctx->s_ctx.exposure_max;
		}
	}
}

static int init_ctx(struct subdrv_ctx *ctx,
		struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(ctx, &defctx, sizeof(*ctx));
	static_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static int get_temp(struct subdrv_ctx *ctx, int *temp)
{
	*temp = get_sensor_temperature(ctx) * 1000;
	return 0;
}


static int get_csi_param(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	struct mtk_csi_param *csi_param)
{
	csi_param->legacy_phy = 0;
	csi_param->not_fixed_trail_settle = 0;

	switch (scenario_id) {
	case SENSOR_SCENARIO_ID_NORMAL_PREVIEW:
	case SENSOR_SCENARIO_ID_NORMAL_CAPTURE:
	case SENSOR_SCENARIO_ID_NORMAL_VIDEO:
	case SENSOR_SCENARIO_ID_HIGHSPEED_VIDEO:
	case SENSOR_SCENARIO_ID_SLIM_VIDEO:
	case SENSOR_SCENARIO_ID_CUSTOM1:
	case SENSOR_SCENARIO_ID_CUSTOM2:
		csi_param->dphy_trail = 75;
		break;
	default:
		csi_param->dphy_trail = 75;
		break;
	}

	return 0;
}

static int vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt)
{
	OV08D10_LOG_INF("sensormode(%d) sof_cnt(%d)\n", ctx->sensor_mode, sof_cnt);
	return 0;
};

static struct subdrv_ops ops = {
	.get_id          = get_imgsensor_id,
	.init_ctx        = init_ctx,
	.open            = open,
	.get_info        = get_info,
	.get_resolution  = get_resolution,
	.control         = control,
	.feature_control = feature_control,
	.close           = close,
	.get_csi_param = get_csi_param,
#ifdef IMGSENSOR_VC_ROUTING
	.get_frame_desc  = get_frame_desc,
#endif
	.get_temp        = get_temp,
	.vsync_notify    = vsync_notify,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_MCLK,  24,      0},
	{HW_ID_RST,   0,       1},
	{HW_ID_DOVDD, 1800000, 1},
	{HW_ID_AVDD,  1,       1},
	{HW_ID_DVDD,  1800000, 1},
	{HW_ID_DVDD1, 1,       6},
	{HW_ID_RST,   1,       1},
	{HW_ID_MCLK_DRIVING_CURRENT, 6, 8},
};

const struct subdrv_entry duchampov08d10ultra_mipi_raw_entry = {
	.name = "duchampov08d10ultra_mipi_raw",
	.id = DUCHAMPOV08D10ULTRA_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

