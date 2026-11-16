// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/printk.h>
#include "mtk_cam-seninf.h"
#include "mtk_cam-seninf-hw.h"
#include "imgsensor-user.h"

#define PFX "SenIfFakeSensor"
#define FakeSensor_INF(format, args...) \
	pr_info(PFX "[%s] " format, __func__, ##args)

struct seninf_ctx *fake_sensor_seninf_ctx;
struct mtk_cam_seninf_ops *fake_g_seninf_ops;
struct seninf_fakesensor_tm_param fakesensor_tm_param;

static inline int get_test_hmargin(u16 w, u16 h, u8 clk_cnt, u16 clk_mhz, u16 fps)
{
	int target_h = clk_mhz * (1000000/fps) / w * max(16/(clk_cnt+1), 1);

	FakeSensor_INF("%u = %u * (1000000/%u) / %u * max(16/(%u+1), 1)\n",
		target_h, clk_mhz, fps, w, clk_cnt);
	return max(target_h - h, 0x80);
}

static void fakesensor_tm_param_prepare(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc = &vcinfo->vc[0];
	int tm_width = vc->exp_hsize;
	int tm_height = vc->exp_vsize;
	int vs_diff = 0x10;
	int width_tm = (tm_width >> 1);
	int bit_depth = 16;
	int width_tm_bit = width_tm * bit_depth;
	const u16 c_fps = 30;
	const u16 c_isp_clk = 346;
	u8 c_clk_div_cnt = 0x7;
	u16 c_dummy_pxl = 0x400;
	int width_cal = (width_tm+tm_height+c_dummy_pxl*8)*2;
	u16 c_dum_vsync = get_test_hmargin(
								width_cal,
								tm_height,
								c_clk_div_cnt,
								c_isp_clk,
								c_fps);

	fakesensor_tm_param.clr_value = 1;
	fakesensor_tm_param.width_tm = width_tm;
	fakesensor_tm_param.tm_height = tm_height;
	fakesensor_tm_param.vs_diff = vs_diff;
	fakesensor_tm_param.c_dummy_pxl = c_dummy_pxl;
	fakesensor_tm_param.width_tm_bit = width_tm_bit;
	fakesensor_tm_param.c_clk_div_cnt = c_clk_div_cnt;
	fakesensor_tm_param.c_dum_vsync = c_dum_vsync;

	/* SMVR-ISP CLK 564M  */
	if (ctx->fake_sensor_info.fps==9600){
		FakeSensor_INF("is SMVR mode\n");
		c_dummy_pxl = 365;
		c_clk_div_cnt = 3;
		width_cal = (width_tm+tm_height+c_dummy_pxl*8)*2;
		c_dum_vsync = get_test_hmargin(
								width_cal,
								tm_height,
								c_clk_div_cnt,
								564,
								960);
		fakesensor_tm_param.c_dummy_pxl = c_dummy_pxl;
		fakesensor_tm_param.c_clk_div_cnt = c_clk_div_cnt;
		fakesensor_tm_param.c_dum_vsync = c_dum_vsync;
	}

	/* 8K60-ISP CLK 564M  */
	if (ctx->fake_sensor_info.fps==600 && tm_width ==8192){
		FakeSensor_INF("is 8K60 mode\n");
		c_dummy_pxl = 400;
		c_clk_div_cnt = 1;
		width_cal = (width_tm+tm_height+c_dummy_pxl*8)*2;
		c_dum_vsync = 350;
		fakesensor_tm_param.c_dummy_pxl = c_dummy_pxl;
		fakesensor_tm_param.c_clk_div_cnt = c_clk_div_cnt;
		fakesensor_tm_param.c_dum_vsync = c_dum_vsync;
	}

	switch (ctx->fake_sensor_info.hdr_mode) {
	case HDR_NONE:
		break;
	case HDR_RAW_DCG_RAW:
		fakesensor_tm_param.vs_diff = 0;
		fakesensor_tm_param.c_dummy_pxl = 632;
		break;
	case HDR_RAW_STAGGER:
		fakesensor_tm_param.vs_diff = 0x10;
		fakesensor_tm_param.c_dummy_pxl = 632;
		break;
	case HDR_RAW_LBMF:
		fakesensor_tm_param.vs_diff = fakesensor_tm_param.tm_height / 2;
		fakesensor_tm_param.c_dummy_pxl = 632;
		break;
	default:
		FakeSensor_INF(
			"ERROR: fake sensor not support: ctx->fake_sensor_info.hdr_mode:%u\n",
			ctx->fake_sensor_info.hdr_mode);
		break;
	}

	if (ctx->fake_sensor_info.sensor_output_dataformat == 0)
		fakesensor_tm_param.tm_core_fmt = 0x0; /* bayer */
	else {
		switch (ctx->fake_sensor_info.sensor_output_dataformat_cell_type) {
		case SENSOR_OUTPUT_FORMAT_CELL_2X2:
			fakesensor_tm_param.tm_core_fmt = 0x2;
			break;
		case SENSOR_OUTPUT_FORMAT_CELL_3X3:
			fakesensor_tm_param.tm_core_fmt = 0x3;
			break;
		case SENSOR_OUTPUT_FORMAT_CELL_4X4:
			fakesensor_tm_param.tm_core_fmt = 0x4;
			break;
		default:
			FakeSensor_INF(
				"ERROR: fake sensor not support sensor_output_dataformat_cell_type:%u\n",
				ctx->fake_sensor_info.sensor_output_dataformat_cell_type);
			break;
		}
	}

	FakeSensor_INF(
			"clr_value:%u width_tm:%u tm_height:%u vs_diff:%u width_tm_bit:%u c_clk_div_cnt:%u c_dummy_pxl:%u c_dum_vsync:%u tm_core_fmt:%u\n",
			fakesensor_tm_param.clr_value,
			fakesensor_tm_param.width_tm,
			fakesensor_tm_param.tm_height,
			fakesensor_tm_param.vs_diff,
			fakesensor_tm_param.width_tm_bit,
			fakesensor_tm_param.c_clk_div_cnt,
			fakesensor_tm_param.c_dummy_pxl,
			fakesensor_tm_param.c_dum_vsync,
			fakesensor_tm_param.tm_core_fmt);
}

void seninf_fakesensor_set_testmdl(struct seninf_ctx *ctx)
{
	struct mtk_fake_sensor_info fake_sensor_info;

	if (ctx->sensor_sd == NULL) {
		FakeSensor_INF("ERROR: ctx->sensor_sd is NULL\n");
		return;
	}

	if (unlikely(fake_sensor_seninf_ctx == NULL)) {
		FakeSensor_INF("ERROR: fake_sensor_seninf_ctx == NULL\n");
		return;
	}

	if (unlikely(fake_g_seninf_ops == NULL)) {
		FakeSensor_INF("ERROR: fake_g_seninf_ops == NULL\n");
		return;
	}

	ctx->sensor_sd->ops->core->command(ctx->sensor_sd,
						V4L2_CMD_G_SENSOR_FAKE_SENSOR_INFO,
						&fake_sensor_info);

	memcpy(&(ctx->fake_sensor_info), &fake_sensor_info, sizeof(struct mtk_fake_sensor_info));

	fakesensor_tm_param_prepare(ctx);

	fake_g_seninf_ops->_set_test_model_fake_sensor(
		fake_sensor_seninf_ctx,
		fake_sensor_seninf_ctx->seninfAsyncIdx, &fakesensor_tm_param, 0);
}

static int fake_sensor_set_gain(
	void *arg, const char *caller)
{
	u64 *feature_data = (u64 *) arg;

	if (unlikely(arg == NULL)) {
		FakeSensor_INF("ERROR: arg == NULL\n");
		return -EINVAL;
	}

	if (unlikely(fake_sensor_seninf_ctx == NULL)) {
		FakeSensor_INF("ERROR: fake_sensor_seninf_ctx == NULL\n");
		return -EINVAL;
	}

	if (unlikely(fake_g_seninf_ops == NULL)) {
		FakeSensor_INF("ERROR: fake_g_seninf_ops == NULL\n");
		return -EINVAL;
	}

	fakesensor_tm_param.clr_value = (*feature_data);
	/*
	 *fake_g_seninf_ops->_set_test_model_fake_sensor(fake_sensor_seninf_ctx,
	 *	fake_sensor_seninf_ctx->seninfAsyncIdx,
	 *	&fakesensor_tm_param, 1);
	 */
	FakeSensor_INF("gain:%llu\n", (*feature_data));

	return 0;
}

static int fake_sensor_set_eshutter(
	void *arg, const char *caller)
{
	u64 *feature_data = (u64 *) arg;

	if (unlikely(arg == NULL)) {
		FakeSensor_INF("ERROR: arg == NULL\n");
		return -EINVAL;
	}

	if (unlikely(fake_sensor_seninf_ctx == NULL)) {
		FakeSensor_INF("ERROR: fake_sensor_seninf_ctx == NULL\n");
		return -EINVAL;
	}

	if (unlikely(fake_g_seninf_ops == NULL)) {
		FakeSensor_INF("ERROR: fake_g_seninf_ops == NULL\n");
		return -EINVAL;
	}

	fakesensor_tm_param.clr_value = (*feature_data);
	/*
	 *fake_g_seninf_ops->_set_test_model_fake_sensor(fake_sensor_seninf_ctx,
	 *	fake_sensor_seninf_ctx->seninfAsyncIdx,
	 *	&fakesensor_tm_param, 1);
	 */
	FakeSensor_INF("exp:%llu\n", (*feature_data));

	return 0;
}

static int fake_sensor_seamless_switch(
	void *arg, const char *caller)
{
	struct mtk_fake_sensor_info *p_info = NULL;

	if (unlikely(arg == NULL)) {
		FakeSensor_INF("ERROR: arg == NULL\n");
		return -EINVAL;
	}

	if (unlikely(fake_sensor_seninf_ctx == NULL)) {
		FakeSensor_INF("ERROR: fake_sensor_seninf_ctx == NULL\n");
		return -EINVAL;
	}

	if (unlikely(fake_g_seninf_ops == NULL)) {
		FakeSensor_INF("ERROR: fake_g_seninf_ops == NULL\n");
		return -EINVAL;
	}

	p_info = (struct mtk_fake_sensor_info *) arg;
	memcpy(&(fake_sensor_seninf_ctx->fake_sensor_info), p_info, sizeof(struct mtk_fake_sensor_info));

	fakesensor_tm_param_prepare(fake_sensor_seninf_ctx);
	g_seninf_ops->_set_test_model_fake_sensor(
		fake_sensor_seninf_ctx,
		fake_sensor_seninf_ctx->seninfAsyncIdx, &fakesensor_tm_param, 1);

	return 0;
}

struct fake_sensor_cb_cmd_entry {
	unsigned int cmd;
	int (*func)(void *arg, const char *caller);
};

static const struct fake_sensor_cb_cmd_entry fake_sensor_cb_cmd_list[] = {
	{FAKE_SENSOR_SET_GAIN, fake_sensor_set_gain},
	{FAKE_SENSOR_SET_ESHUTTER, fake_sensor_set_eshutter},
	{FAKE_SENSOR_SEAMLESS_SWITCH, fake_sensor_seamless_switch},
};

int fake_sensor_cb_handler(
	const unsigned int cmd, void *arg, const char *caller)
{
	int i, is_cmd_found = 0, ret = 0;

	/* !!! dispatch cb cmd request !!! */
	for (i = 0; i < ARRAY_SIZE(fake_sensor_cb_cmd_list); ++i) {
		if (fake_sensor_cb_cmd_list[i].cmd == cmd) {
			is_cmd_found = 1;
			ret = fake_sensor_cb_cmd_list[i].func(arg, caller);
			break;
		}
	}
	if (unlikely(is_cmd_found == 0)) {
		ret = -1;
		FakeSensor_INF("WARNING: FAKE SENSOR callback cmd not found, cmd:%u\n", cmd);
	}

	FakeSensor_INF("cmd=%u, arg=%p, caller:%s\n", cmd, arg, caller);

	return ret;
}

int seninf_fakesensor_link_info(struct seninf_ctx *ctx, struct mtk_cam_seninf_ops *g_seninf_ops)
{
	struct mtk_fake_sensor_info fake_sensor_info;

	if (ctx->sensor_sd == NULL) {
		FakeSensor_INF("ERROR: ctx->sensor_sd is NULL\n");
		return -EINVAL;
	}

	ctx->sensor_sd->ops->core->command(ctx->sensor_sd,
						V4L2_CMD_G_SENSOR_FAKE_SENSOR_INFO,
						&fake_sensor_info);

	memcpy(&(ctx->fake_sensor_info), &fake_sensor_info, sizeof(struct mtk_fake_sensor_info));

	if (ctx->fake_sensor_info.is_fake_sensor) {
		/* call v4l2_subdev_core_ops command to sensor adaptor */
		ctx->sensor_sd->ops->core->command(
			ctx->sensor_sd,
			V4L2_CMD_SET_CB_FUNC_OF_FAKE_SENSOR,
			fake_sensor_cb_handler);

		/* store pointer */
		fake_sensor_seninf_ctx = ctx;
		fake_g_seninf_ops = g_seninf_ops;

		FakeSensor_INF("%s is fake sensor ch_func:%p fake_sensor_seninf_ctx:%p fake_g_seninf_ops:%p\n",
			ctx->sensor_sd->name,
			fake_sensor_cb_handler,
			fake_sensor_seninf_ctx,
			fake_g_seninf_ops);
	}

	return 0;
}
