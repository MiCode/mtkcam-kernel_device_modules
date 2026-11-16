// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 imx989mipiraw_Sensor.c
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
#include "imx989mipiraw_Sensor.h"

static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static int imx989_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int imx989_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int imx989_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int imx989_cphy_lrte_mode(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int imx989_extend_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt, u64 sof_ts);

/* STRUCT */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, imx989_set_test_pattern},
	{SENSOR_FEATURE_SET_TEST_PATTERN_DATA, imx989_set_test_pattern_data},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, imx989_seamless_switch},
	{SENSOR_FEATURE_SET_CPHY_LRTE_MODE, imx989_cphy_lrte_mode},
	{SENSOR_FEATURE_SET_SEAMLESS_EXTEND_FRAME_LENGTH, imx989_extend_frame_length},
};

static struct subdrv_static_ctx_ext_ops static_ext_ops = {
	.i2c_addr_table = {0x34, 0xFF},
	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.s_cali = set_sensor_cali,
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),

#ifdef IMX989_ISF_DBG
	.debug_check_with_exist_s_ctx = &imx989_legacy_s_ctx,
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
	.vsync_notify = vsync_notify,
	.update_sof_cnt = common_update_sof_cnt,
	.parse_ebd_line = common_parse_ebd_line,
};

const struct subdrv_entry imx989_mipi_raw_entry = {
	.name = "imx989_mipi_raw",
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
			subdrv_i2c_wr_u8(ctx, 0x3206, 0x01);
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

static int imx989_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	enum SENSOR_SCENARIO_ID_ENUM current_scenario_id;
	struct mtk_hdr_ae *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;
	u32 frame_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 exp_cnt = 0;

	current_scenario_id = ctx->current_scenario_id;
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
		/* the time between the end of last frame readout and the next vsync need greater than 10ms */
		common_get_prsh_length_lines_by_time(ctx, ae_ctrl, current_scenario_id, scenario_id, 10);
	}

	if (ctx->s_ctx.seamless_switch_prsh_length_lc > 0) {
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_prsh_mode, 0x01);

		set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_prsh_length_lines.addr[0],
				(ctx->s_ctx.seamless_switch_prsh_length_lc >> 16) & 0xFF);
		set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_prsh_length_lines.addr[1],
				(ctx->s_ctx.seamless_switch_prsh_length_lc >> 8)  & 0xFF);
		set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_prsh_length_lines.addr[2],
				(ctx->s_ctx.seamless_switch_prsh_length_lc) & 0xFF);

		DRV_LOG_MUST(ctx, "seamless switch pre-shutter set(%u)\n",
			ctx->s_ctx.seamless_switch_prsh_length_lc);
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

static int imx989_cphy_lrte_mode(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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

static int imx989_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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

static int imx989_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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

#define EXTEND_FLL_MORE 10000000
static int imx989_extend_frame_length(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	int i;
	u32 ns = *((u32 *)para);
	u32 last_exp_cnt = 1;
	u32 old_fl = ctx->frame_length;
	u32 calc_fl = 0;
	u32 readoutLength = 0;
	u32 readMargin = 0;
	u32 per_frame_ns = (u64)ctx->frame_length *
		(u64)ctx->line_length * 1000000000 / ctx->pclk;

	check_current_scenario_id_bound(ctx);
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_RAW) {
		ns += EXTEND_FLL_MORE;
		DRV_LOG_MUST(ctx, "extend more %dns", EXTEND_FLL_MORE);
	} else if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF) {
		DRV_LOG_MUST(ctx, "do not extend");
		return ERROR_NONE;
	}
	readoutLength = ctx->s_ctx.mode[ctx->current_scenario_id].readout_length;
	readMargin = ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;

	for (i = 1; i < ARRAY_SIZE(ctx->exposure); i++)
		last_exp_cnt += ctx->exposure[i] ? 1 : 0;
	if (ns)
		ctx->frame_length = (u32)(((u64)(per_frame_ns + ns)) *
			ctx->frame_length / per_frame_ns);
	if (last_exp_cnt > 1) {
		calc_fl = (readoutLength + readMargin);
		for (i = 1; i < last_exp_cnt; i++)
			calc_fl += (ctx->exposure[i] + ctx->s_ctx.exposure_margin * last_exp_cnt);
		ctx->frame_length = max(calc_fl, ctx->frame_length);
	}
	set_dummy(ctx);
	ctx->extend_frame_length_en = TRUE;

	ns = (u64)(ctx->frame_length - old_fl) *
		(u64)ctx->line_length * 1000000000 / ctx->pclk;

	DRV_LOG_MUST(ctx, "fll(old/new):%u/%u, add %u ns", old_fl, ctx->frame_length, ns);

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
		subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_prsh_mode, 0x00);
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x00);
		commit_i2c_buffer(ctx);
	}
	return 0;
}
