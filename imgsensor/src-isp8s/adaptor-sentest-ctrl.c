// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include "adaptor-sentest-ctrl.h"

#include <linux/mutex.h>

/******************************************************************************/
// adaptor sentest call back for ioctl & adaptor framework using
/******************************************************************************/

#define SEAMLESS_REG_IGNORE_TABLE_LEN 400

struct seamless_ignore_reg_table {
	struct reg_ *(*func)(struct adaptor_ctx *ctx);
};

int notify_sentest_tsrec_time_stamp(struct adaptor_ctx *ctx,
					struct mtk_cam_seninf_tsrec_timestamp_info *info)
{
	struct mtk_cam_sentest_cfg_info *sentest_info;
	static u64 prev_ts0;

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(info == NULL)) {
		pr_info("[%s][ERROR] info is NULL\n", __func__);
		return -EINVAL;
	}

	sentest_info = &ctx->sentest_cfg_info;
	if (unlikely(sentest_info == NULL)) {
		pr_info("[%s][ERROR] sentest_info is NULL\n", __func__);
		return -EINVAL;
	}

	if (prev_ts0 == info->exp_recs[0].ts_us[0])
		return 0;

	mutex_lock(&sentest_info->sentest_update_tsrec_mutex);

	prev_ts0 = info->exp_recs[0].ts_us[0];
	sentest_info->ts.sentest_tsrec_frame_cnt++;
	sentest_info->ts.sys_time_ns = info->irq_sys_time_ns;

	memcpy(sentest_info->ts.exp_recs,
		info->exp_recs,
		(sizeof(struct mtk_cam_seninf_tsrec_timestamp_exp)) * TSREC_EXP_MAX_CNT);

	mutex_unlock(&sentest_info->sentest_update_tsrec_mutex);

	return 0;
}

int sentest_get_current_tsrec_info(struct adaptor_ctx *ctx,
					struct mtk_cam_seninf_sentest_ts *info)
{
	struct mtk_cam_sentest_cfg_info *sentest_info;

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(info == NULL)) {
		pr_info("[%s][ERROR] info is NULL\n", __func__);
		return -EINVAL;
	}

	sentest_info = &ctx->sentest_cfg_info;
	if (unlikely(sentest_info == NULL)) {
		pr_info("[%s][ERROR] sentest_info is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&sentest_info->sentest_update_tsrec_mutex);
	if (copy_to_user(info,
			&sentest_info->ts,
			sizeof(struct mtk_cam_seninf_sentest_ts))) {
		dev_info(ctx->dev,
			"[%s][ERR] copy_to_user return failed\n", __func__);
		mutex_unlock(&sentest_info->sentest_update_tsrec_mutex);
		return -EFAULT;
	}
	mutex_unlock(&sentest_info->sentest_update_tsrec_mutex);

	return 0;
}


static inline struct reg_ *get_reg_addr_exposure(struct adaptor_ctx *ctx)
{
	return ctx->subctx.s_ctx.reg_addr_exposure;
}

static inline struct reg_ *get_reg_addr_ana_gain(struct adaptor_ctx *ctx)
{
	return ctx->subctx.s_ctx.reg_addr_ana_gain;
}

static inline struct reg_ *get_reg_addr_dig_gain(struct adaptor_ctx *ctx)
{
	return ctx->subctx.s_ctx.reg_addr_dig_gain;
}

static inline struct reg_ *get_reg_addr_exposure_in_lut(struct adaptor_ctx *ctx)
{
	return ctx->subctx.s_ctx.reg_addr_exposure_in_lut;
}

static inline struct reg_ *get_reg_addr_ana_gain_in_lut(struct adaptor_ctx *ctx)
{
	return ctx->subctx.s_ctx.reg_addr_ana_gain_in_lut;
}

static inline struct reg_ *get_reg_addr_dig_gain_in_lut(struct adaptor_ctx *ctx)
{
	return ctx->subctx.s_ctx.reg_addr_dig_gain_in_lut;
}

static inline struct reg_ *get_reg_addr_frame_length_in_lut(struct adaptor_ctx *ctx)
{
	return ctx->subctx.s_ctx.reg_addr_frame_length_in_lut;
}

static int sentest_get_seamless_ignore_table(struct adaptor_ctx *ctx,
	u32 *ignore_table, u32 *table_len)
{
	u32 i = 0;
	u32 len = 0;
	u32 exposure_cnt = 0;
	u32 ignore_reg_table_len = 0;
	struct reg_ *reg  = 0;
	struct reg_ *__reg  = 0;
	struct seamless_ignore_reg_table table[] = {
		{get_reg_addr_exposure},
		{get_reg_addr_ana_gain},
		{get_reg_addr_dig_gain},
		{get_reg_addr_exposure_in_lut},
		{get_reg_addr_ana_gain_in_lut},
		{get_reg_addr_frame_length_in_lut},
	};

	if (unlikely(ignore_table == NULL)) {
		pr_info("[%s][ERROR] ignore_table is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(table_len == NULL)) {
		pr_info("[%s][ERROR] table_len is NULL\n", __func__);
		return -EINVAL;
	}

	if (ctx->subctx.s_ctx.reg_addr_exposure_lshift)
		ignore_table[len++] = ctx->subctx.s_ctx.reg_addr_exposure_lshift;

	if (ctx->subctx.s_ctx.reg_addr_frame_length.addr[0])
		ignore_table[len++] = ctx->subctx.s_ctx.reg_addr_frame_length.addr[0];

	if (ctx->subctx.s_ctx.reg_addr_frame_length.addr[1])
		ignore_table[len++] = ctx->subctx.s_ctx.reg_addr_frame_length.addr[1];

	if (ctx->subctx.s_ctx.reg_addr_frame_length.addr[2])
		ignore_table[len++] = ctx->subctx.s_ctx.reg_addr_frame_length.addr[2];

	if (ctx->subctx.s_ctx.reg_addr_frame_length.addr[3])
		ignore_table[len++] = ctx->subctx.s_ctx.reg_addr_frame_length.addr[3];

	/* add HDR related perframe info to ignore table */
	ignore_reg_table_len = sizeof(table) / sizeof(struct seamless_ignore_reg_table);

	for (i = 0; i < ignore_reg_table_len; i++) {
		reg = table[i].func(ctx);

		if (reg == NULL) {
			pr_info("[%s][ERROR] reg is NULL\n", __func__);
			return -EINVAL;
		}

		for (exposure_cnt = 0; exposure_cnt < IMGSENSOR_STAGGER_EXPOSURE_CNT;
			exposure_cnt++) {
			__reg = reg + exposure_cnt;

			if (__reg == NULL) {
				pr_info("[%s][WARN] skip exposure cnt %d\n",
					__func__, exposure_cnt);
				continue;
			}

			if (__reg->addr[0])
				ignore_table[len++] = __reg->addr[0];

			if (__reg->addr[1])
				ignore_table[len++] = __reg->addr[1];

			if (__reg->addr[2])
				ignore_table[len++] = __reg->addr[2];

			if (__reg->addr[3])
				ignore_table[len++] = __reg->addr[3];
		}
	}

	*table_len = len;
	return 0;
}

int sentest_get_sensor_setting_info(struct adaptor_ctx *ctx,
		struct mtk_sentest_sensor_setting_info *info)
{
	int ret = 0;
	u32 ignore_table[SEAMLESS_REG_IGNORE_TABLE_LEN];
	u32 ignore_table_len = 0;

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(info == NULL)) {
		pr_info("[%s][ERROR] info is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(info->param_ptr == NULL)) {
		pr_info("[%s][ERROR] param_ptr is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(info->scenario > ctx->subctx.s_ctx.sensor_mode_num)) {
		pr_info("[%s][ERROR] scenario %d is exceed max scenario %d\n",
			__func__,
			info->scenario,
			ctx->subctx.s_ctx.sensor_mode_num);
		return -EINVAL;
	}

	switch (info->type) {
	case SENTEST_SENSOR_SETTING:
		if (info->table_length <
		sizeof(*ctx->subctx.s_ctx.mode[info->scenario].mode_setting_table)) {
			pr_info("[%s][ERROR] user table len is smaller than src table\n", __func__);
			return -EINVAL;
		}

		info->table_length = ctx->subctx.s_ctx.mode[info->scenario].mode_setting_len;
		ret = copy_to_user(info->param_ptr,
				ctx->subctx.s_ctx.mode[info->scenario].mode_setting_table,
				sizeof(*ctx->subctx.s_ctx.mode[info->scenario].mode_setting_table));

		break;

	case SENTEST_SEAMLESS_SETTING:
		if (info->table_length <
		sizeof(
		*ctx->subctx.s_ctx.mode[info->scenario].seamless_switch_mode_setting_table)) {
			pr_info("[%s][ERROR] user table len is smaller than src table\n", __func__);
			return -EINVAL;
		}

		info->table_length =
		ctx->subctx.s_ctx.mode[info->scenario].seamless_switch_mode_setting_len;
		ret = copy_to_user(info->param_ptr,
		ctx->subctx.s_ctx.mode[info->scenario].seamless_switch_mode_setting_table,
		sizeof(*ctx->subctx.s_ctx.mode[info->scenario].seamless_switch_mode_setting_table));
		break;

	case SENTEST_IGNORE_SETTING: {
		if (info->table_length < sizeof(ignore_table)) {
			pr_info("[%s][ERROR] user table len is smaller than src table\n", __func__);
			return -EINVAL;
		}

		memset(ignore_table, 0, sizeof(u32[SEAMLESS_REG_IGNORE_TABLE_LEN]));

		if (sentest_get_seamless_ignore_table(ctx, ignore_table, &ignore_table_len)) {
			pr_info("[%s][ERROR] sentest_get_seamless_ignore_table return failed\n",
				__func__);
			return -EINVAL;
		}

		info->table_length = ignore_table_len;

		ret = copy_to_user(info->param_ptr,
							ignore_table,
							sizeof(ignore_table));
		break;
	}

	default:
		pr_info("[%s][ERROR] info->type %d is invalid\n",
				__func__, info->type);
		return -EINVAL;
	}

	if (ret) {
		pr_info("[%s][ERROR] info->type %d copy_to_user failed\n", __func__, info->type);
		ret = -EINVAL;
	}

	return ret;
}

int sentest_probe_init(struct adaptor_ctx *ctx)
{
	struct mtk_cam_sentest_cfg_info *sentest_info;

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	sentest_info = &ctx->sentest_cfg_info;
	if (unlikely(sentest_info == NULL)) {
		pr_info("[%s][ERROR] sentest_info is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_init(&sentest_info->sentest_update_tsrec_mutex);

	/*binding sentest related flags into subctx */
	ctx->subctx.power_on_profile_en = &sentest_info->power_on_profile_en;

	return 0;
}

int sentest_flag_init(struct adaptor_ctx *ctx)
{
	struct mtk_cam_sentest_cfg_info *sentest_info;

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	sentest_info = &ctx->sentest_cfg_info;
	if (unlikely(sentest_info == NULL)) {
		pr_info("[%s][ERROR] sentest_info is NULL\n", __func__);
		return -EINVAL;
	}

	sentest_info->listen_tsrec_frame_id = 0;
	sentest_info->power_on_profile_en = 0;
	sentest_info->lbmf_delay_do_ae_en = 0;

	memset(&sentest_info->ts, 0, sizeof(struct mtk_cam_seninf_sentest_ts));

	return 0;
}
