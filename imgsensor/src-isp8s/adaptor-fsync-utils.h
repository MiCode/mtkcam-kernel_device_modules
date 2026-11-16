/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __ADAPTOR_FSYNC_UTILS_H__
#define __ADAPTOR_FSYNC_UTILS_H__

#include "adaptor.h"
#include "adaptor-common-ctrl.h"	/* e.g., g_sensor_xxx() func */


/*******************************************************************************
 * fsync utils functions --- sensor driver related
 ******************************************************************************/
static inline int fsync_util_sen_g_fine_integ_line(struct adaptor_ctx *ctx,
	const unsigned int mode_id)
{
	return g_sensor_fine_integ_line(ctx, mode_id);
}

static inline unsigned int fsync_util_sen_g_margin(struct adaptor_ctx *ctx,
	const unsigned int mode_id)
{
	return g_sensor_margin(ctx, mode_id);
}

static inline unsigned int fsync_util_sen_g_fdelay(struct adaptor_ctx *ctx,
	const unsigned int mode_id, const char *caller)
{
	return g_sensor_frame_length_delay(ctx, mode_id, caller);
}

static inline int fsync_util_sen_g_stagger_info(struct adaptor_ctx *ctx,
	const unsigned int mode_id)
{
	struct mtk_stagger_info info = {0};

	info.scenario_id = SENSOR_SCENARIO_ID_NONE;
	return g_stagger_info(ctx, mode_id, &info);
}

unsigned int fsync_util_sen_chk_and_g_dcg_vsl_seamless_readout_time_us(
	struct adaptor_ctx *ctx, const u32 mode_id, unsigned int exp_no);

void fsync_util_sen_chk_and_correct_readout_time(struct adaptor_ctx *ctx,
	struct fs_perframe_st *p_pf_ctrl, const u32 mode_id);

unsigned int fsync_util_sen_g_readout_time_us(struct adaptor_ctx *ctx,
	const u32 mode_id);

void fsync_util_sen_g_hw_sync_info(struct adaptor_ctx *ctx,
	struct fs_streaming_st *s_info);

void fsync_util_sen_s_frame_length(struct adaptor_ctx *ctx);

void fsync_util_sen_s_multi_shutter_frame_length(struct adaptor_ctx *ctx,
	u64 *ae_exp_arr, u32 ae_exp_cnt, const unsigned int multi_exp_type);
/******************************************************************************/


/*******************************************************************************
 * fsync utils functions --- data exchange proc (frame-sync <-> sensor driver)
 ******************************************************************************/
void fsync_util_setup_sen_hdr_info_st(struct adaptor_ctx *ctx,
	struct fs_hdr_exp_st *p_hdr_exp, const u32 mode_id);
/******************************************************************************/

#endif
