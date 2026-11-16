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
	.vsync_notify = vsync_notify,
	.update_sof_cnt = common_update_sof_cnt,
	.parse_ebd_line = common_parse_ebd_line,
	.mcss_set_mask_frame = imx06c_mcss_set_mask_frame,
};

struct subdrv_static_ctx_ext_ops static_ext_ops = {
	.i2c_addr_table = {0x34, 0xFF},
	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.s_cali = set_sensor_cali,
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),

	.mcss_update_subdrv_para = imx06c_mcss_update_subdrv_para,
	.cust_get_linetime_in_ns = imx06c_get_linetime_in_ns,
	.mcss_init = imx06c_mcss_init,
#ifdef IMX06C_ISF_DBG
	.debug_check_with_exist_s_ctx = &imx06c_legacy_s_ctx,
#endif
};

const struct subdrv_entry imx06c_mipi_raw_entry = {
	.name = "imx06c_mipi_raw",
	.ops = &ops,
	.is_fw_support = TRUE,
	.fw_ext_ops = &static_ext_ops,
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
	u64 seamless_i2c_start_time = 0;
	u64 seamless_i2c_finish_time = 0;

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
	seamless_i2c_start_time = ktime_get_boottime_ns();
	commit_i2c_buffer(ctx);
	seamless_i2c_finish_time = ktime_get_boottime_ns();

	ctx->fast_mode_on = TRUE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->is_seamless = FALSE;
	DRV_LOG_MUST(ctx, "X: set seamless switch done, using (%llu)ns\n",
		seamless_i2c_finish_time - seamless_i2c_start_time);
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

void imx06c_set_long_exposure(struct subdrv_ctx *ctx)
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
