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
static int s5kjn1_set_ctrl_locker(struct subdrv_ctx *ctx, u32 cid, bool *is_lock);
static int s5kjn1_chk_streaming_st(void *arg);

/* STRUCT */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, s5kjn1_set_test_pattern},
	{SENSOR_FEATURE_SET_TEST_PATTERN_DATA, s5kjn1_set_test_pattern_data},
};

static struct subdrv_static_ctx_ext_ops static_ext_ops = {
	.i2c_addr_table = {0x5A, 0xFF},
	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.chk_streaming_st = s5kjn1_chk_streaming_st,
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
#ifdef S5KJN1_ISF_DBG
	.debug_check_with_exist_s_ctx = &s5kjn1_legacy_s_ctx,
#endif
};

static struct subdrv_ops ops = {
	.get_id = common_get_imgsensor_id,
	.init_ctx = common_init_ctx,
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
	.set_ctrl_locker = s5kjn1_set_ctrl_locker,
};


const struct subdrv_entry s5kjn1_mipi_raw_entry = {
	.name = "s5kjn1_mipi_raw",
	.ops = &ops,
	.is_fw_support = TRUE,
	.fw_ext_ops = &static_ext_ops,
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
	if (mode) {
		if (mode == 5) {
			DRV_LOG(ctx, "use Solid Color replace Black\n");
			mode = 1;
		}
		subdrv_ixc_wr_u16(ctx, 0x0600, mode); /*100% Color bar*/
	} else if (ctx->test_pattern)
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
static int s5kjn1_set_ctrl_locker(struct subdrv_ctx *ctx,
		u32 cid, bool *is_lock)
{
	bool lock_set_ctrl = false;

	if (unlikely(is_lock == NULL)) {
		pr_info("[%s][ERROR] is_lock %p is NULL\n", __func__, is_lock);
		return -EINVAL;
	}

	switch (cid) {
	case V4L2_CID_MTK_STAGGER_AE_CTRL:
	case V4L2_CID_MTK_MAX_FPS:
		if ((ctx->sof_no == 0) && (ctx->is_streaming)) {
			lock_set_ctrl = true;
			DRV_LOG(ctx,
				"[%s] Target lock cid(%u) lock_set_ctrl(%d), sof_no(%d) is_streaming(%d)\n",
				__func__,
				cid,
				lock_set_ctrl,
				ctx->sof_no,
				ctx->is_streaming);
		}
		break;
	default:
		break;
	}

	*is_lock = lock_set_ctrl;
	return ERROR_NONE;
} /* s5kjn1_set_ctrl_locker */

static int s5kjn1_chk_streaming_st(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u64 cur_time = 0;
	u64 start_time = ctx->stream_ctrl_start_time;
	u32 cur_fc = 0xff;
	u64 timeout = 1000000000/(u64)ctx->current_fps*10;
	int ret = ERROR_NONE;

	subdrv_ixc_wr_u16(ctx, 0x6028, 0x4000);
	subdrv_ixc_wr_u16(ctx, 0x602A, 0x0005);
	while (1) {
		cur_time = ktime_get_boottime_ns();
		cur_fc =  subdrv_ixc_rd_u8(ctx, 0x0005);
		if ((cur_fc > 0 && cur_fc != 0xff) || (cur_time - start_time > timeout))
			break;
		DRV_LOG(ctx, "current_fps: %d, timeout: %lld, cur_fc: %d, cur_time: %lld, start_time: %lld\n",
			ctx->current_fps, timeout, cur_fc, cur_time, start_time);
		mdelay(1);
	}
	return ret;
}
