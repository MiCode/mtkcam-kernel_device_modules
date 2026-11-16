// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

/* for sensor related struct/enum/define */
#include "kd_imgsensor_define_v4l2.h"
#include "imgsensor-user.h"

#include "adaptor-fsync-def.h"
#include "adaptor-fsync-utils.h"

#include "frame-sync/frame_sync.h"
#include "frame-sync/sensor_recorder.h"


/*******************************************************************************
 * fsync utils log define
 ******************************************************************************/
#define PFX "AdaptFsyncUtils"

#define REDUCE_FSYNC_UTILS_LOG
#define REDUCE_FSYNC_UTILS_DBG_LOG


/*******************************************************************************
 * fsync utils static functions
 ******************************************************************************/
static inline unsigned int g_stg_dol_type(struct adaptor_ctx *ctx,
	const u32 mode_id, const char *caller)
{
	enum IMGSENSOR_HDR_SUPPORT_TYPE_ENUM type = HDR_SUPPORT_NA;
	unsigned int res;
	u32 ret;

	ret = g_sensor_stagger_type(ctx, mode_id, &type);
	switch (type) {
	case HDR_SUPPORT_STAGGER_FDOL:
		res = STAGGER_DOL_TYPE_FDOL;
		break;
	case HDR_SUPPORT_STAGGER_DOL:
		res = STAGGER_DOL_TYPE_DOL;
		break;
	case HDR_SUPPORT_STAGGER_NDOL:
		res = STAGGER_DOL_TYPE_NDOL;
		break;
	default:
		res = STAGGER_DOL_TYPE_FDOL;
#ifndef REDUCE_FSYNC_UTILS_LOG
		FSYNC_MGR_LOGI(ctx,
			"[%s] ERROR: sidx:%d, g_stagger_info return vc info is stagger, but type is unexpected (ret:%u/type:%d), treat as FDOL\n",
			caller, ctx->idx, ret, type);
#endif
		break;
	}

	return res;
}


static inline unsigned int g_lbmf_exp_order(struct adaptor_ctx *ctx,
	const struct adaptor_sensor_lbmf_property_st *prop,
	const char *caller)
{
	const enum IMGSENSOR_HDR_MODE_ENUM type = prop->hdr_type;
	const unsigned int prop_exp_order = prop->exp_order;
	unsigned int ret = 0;

	switch (prop_exp_order) {
	case IMGSENSOR_LBMF_EXPOSURE_LE_FIRST:
		ret = EXP_ORDER_LE_1ST;
		break;
	case IMGSENSOR_LBMF_EXPOSURE_SE_FIRST:
		ret = EXP_ORDER_SE_1ST;
		break;
	case IMGSENSOR_LBMF_EXPOSURE_ORDER_SUPPORT_NONE:
	default:
		ret = (type == HDR_RAW_LBMF)
			? EXP_ORDER_SE_1ST : EXP_ORDER_LE_1ST;

		/**
		 * when in LBMF mode => drv need to report the exp order => print.
		 * when in DCG+VSL mode => currently force default LE 1st.
		 */
		if (type == HDR_RAW_LBMF) {
			FSYNC_MGR_LOGI(ctx,
				"[%s] ERROR: sidx:%d, detect sensor mode is LBMF type %d, but exp order is %d(LE_1st:%d/SE_1st:%d/NONE:%d/unknown:others) => treat as LBMF(hdr_mode:%d => SE_1st) / other(=> LE_1st, DCG+VSL(hdr_mode:%d,%d) => ret:%u(LE_1st:%u/SE_1st:%u)\n",
				caller,
				ctx->idx,
				type,
				prop_exp_order,
				IMGSENSOR_LBMF_EXPOSURE_LE_FIRST,
				IMGSENSOR_LBMF_EXPOSURE_SE_FIRST,
				IMGSENSOR_LBMF_EXPOSURE_ORDER_SUPPORT_NONE,
				HDR_RAW_LBMF,
				HDR_RAW_DCG_RAW_VS, HDR_RAW_DCG_COMPOSE_VS,
				ret,
				EXP_ORDER_LE_1ST, EXP_ORDER_SE_1ST);
		}
		break;
	}

	return ret;
}


static unsigned int chk_then_s_dcg_vsl_param(struct adaptor_ctx *ctx,
	struct fs_hdr_exp_st *p_hdr_exp,
	const u32 mode_id, const char *caller)
{
	struct adaptor_sensor_dcg_vsl_property_st prop = {0};
	struct fs_hdr_sen_cascade_mode_info_st *cas_mode_info;
	unsigned int i;
	u32 ret;

	ret = g_sensor_dcg_vsl_property(ctx, mode_id, &prop);
	if (unlikely(ret == 0 || prop.lut_cnt == 0))  /* => NOT DCG+VS/L type */
		return 0;

	/* !!! setup/overwrite all info that needed !!! */

	/* => setup info */
	cas_mode_info = &p_hdr_exp->cas_mode_info;
	/* param.lut_cnt => max is IMGSENSOR_LUT_MAXCNT */
	for (i = 0; (i < prop.lut_cnt && i < FS_HDR_MAX); ++i) {
		cas_mode_info->lineTimeInNs[i] = prop.params[i].linetime_in_ns;
		cas_mode_info->margin_lc[i] = prop.params[i].margin_lc;
		cas_mode_info->read_margin_lc[i] = prop.params[i].read_margin_lc;
		cas_mode_info->cit_loss_lc[i] = prop.params[i].cit_loss_lc;
	}

	/* => overwrite info, exp cnt => 3, but actually only 2 LUT RGs */
	p_hdr_exp->mode_exp_cnt = prop.lut_cnt;

	return 1;
}
/******************************************************************************/


/*******************************************************************************
 * fsync utils functions --- sensor driver related
 ******************************************************************************/
unsigned int fsync_util_sen_chk_and_g_dcg_vsl_seamless_readout_time_us(
	struct adaptor_ctx *ctx, const u32 mode_id, unsigned int exp_no)
{
	struct adaptor_sensor_dcg_vsl_property_st prop = {0};
	unsigned int readout_time_us, readout_length, line_time_ns;
	u32 ret;

	ret = g_sensor_dcg_vsl_property(ctx, mode_id, &prop);
	if (unlikely(ret == 0 || prop.lut_cnt == 0))  /* => NOT DCG+VS/L type */
		return 0;
	if (unlikely(ctx->subctx.s_ctx.mode == NULL))
		return 0;

	/* !!! start to calculate !!! */
	if (exp_no > prop.lut_cnt) {
		/* auto assign to last valid index */
		exp_no = (prop.lut_cnt - 1);
	}
	readout_length = ctx->subctx.s_ctx.mode[mode_id].readout_length;
	line_time_ns = prop.params[exp_no].linetime_in_ns;

	readout_time_us = ((readout_length * line_time_ns) / 1000);

#ifndef REDUCE_FSYNC_UTILS_LOG
	FSYNC_MGR_LOGI(ctx,
		"sidx:%d, rout_time(us):%u (mode_id:%u/exp_no:%u/readout_len:%u/Tline(ns):%u)\n",
		ctx->idx, readout_time_us,
		mode_id, exp_no, readout_length, line_time_ns);
#endif

	return readout_time_us;
}


void fsync_util_sen_chk_and_correct_readout_time(struct adaptor_ctx *ctx,
	struct fs_perframe_st *p_pf_ctrl, const u32 mode_id)
{
	unsigned int exp_order, m_exp_cnt, exp_no, line_time_ns, height;
	unsigned int readout_time_us = 0;

	if (p_pf_ctrl->hdr_exp.multi_exp_type != MULTI_EXP_TYPE_DCG_VSL)
		return;
	/* error handling */
	if (unlikely(!chk_is_valid_scenario_id(ctx, mode_id, __func__)))
		return;
	if (unlikely((ctx->subctx.s_ctx.mode == NULL) || (p_pf_ctrl == NULL)))
		return;

	/* !!! Due to the mode (e.g., DCG+VS/L) has 2 different Line-Time !!! */
	exp_order = p_pf_ctrl->hdr_exp.exp_order;
	m_exp_cnt = p_pf_ctrl->hdr_exp.mode_exp_cnt;
	exp_no = ctx->fsync_mgr->fs_g_sync_target_exp_no(ctx->idx,
		exp_order, m_exp_cnt);
	height = ctx->subctx.s_ctx.mode[mode_id].imgsensor_winsize_info.h2_tg_size;
	line_time_ns =
		p_pf_ctrl->hdr_exp.cas_mode_info.lineTimeInNs[exp_no];

	readout_time_us = ((height * line_time_ns) / 1000);

#ifndef REDUCE_FSYNC_UTILS_LOG
	FSYNC_MGR_LOGI(ctx,
		"sidx:%d, rout_time(us):%u->%u (mode_id:%u/exp_no:%u/exp_order:%u/m_exp_cnt:%u/height:%u/Tline(ns):%u(%u/%u/%u/%u/%u))\n",
		ctx->idx,
		p_pf_ctrl->readout_time_us, readout_time_us,
		mode_id, exp_no, exp_order, m_exp_cnt, height, line_time_ns,
		p_pf_ctrl->hdr_exp.cas_mode_info.lineTimeInNs[0],
		p_pf_ctrl->hdr_exp.cas_mode_info.lineTimeInNs[1],
		p_pf_ctrl->hdr_exp.cas_mode_info.lineTimeInNs[2],
		p_pf_ctrl->hdr_exp.cas_mode_info.lineTimeInNs[3],
		p_pf_ctrl->hdr_exp.cas_mode_info.lineTimeInNs[4]);
#endif
	/* correct/overwrite */
	p_pf_ctrl->readout_time_us = readout_time_us;
}


unsigned int fsync_util_sen_g_readout_time_us(struct adaptor_ctx *ctx,
	const u32 mode_id)
{
	union feature_para para = {0};
	unsigned int ret = 0;
	u32 len = 0;

	para.u64[0] = mode_id;
	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_READOUT_BY_SCENARIO,
		para.u8, &len);

	ret = (unsigned int)para.u64[1] / 1000;

#ifndef REDUCE_FSYNC_UTILS_LOG
	FSYNC_MGR_LOGI(ctx,
		"sidx:%d, mode_id:%u => para.u64[1]:%llu, readout(us):%u\n",
		ctx->idx, mode_id, para.u64[1], ret);
#endif

	return ret;
}


void fsync_util_sen_g_hw_sync_info(struct adaptor_ctx *ctx,
	struct fs_streaming_st *s_info)
{
	union feature_para para = {0};
	u32 len;

	subdrv_call(ctx, feature_control,
		SENSOR_FEATURE_GET_SENSOR_SYNC_MODE,
		para.u8, &len);

	/* sync operate mode. none/master/slave */
	s_info->sync_mode = para.u32[0];
	/* hw sync group ID, e.g., FS_HW_SYNC_GROUP_ID_MCSS */
	s_info->hw_sync_group_id = para.u32[1];
	/* legacy:0, MCSS:1 */
	s_info->hw_sync_method = para.u32[2];

	FSYNC_MGR_LOGD(ctx,
		"sidx:%d, hw_sync_info(sync_mode:%u/hw_sync_group_id:%u/hw_sync_method:%u)\n",
		ctx->idx,
		s_info->sync_mode,
		s_info->hw_sync_group_id,
		s_info->hw_sync_method);
}


void fsync_util_sen_s_frame_length(struct adaptor_ctx *ctx)
{
	enum ACDK_SENSOR_FEATURE_ENUM cmd;
	union feature_para para;
	u32 len;

	para.u64[0] = ctx->fsync_out_fl;
	para.u64[1] = (u64)ctx->fsync_out_fl_arr;

	cmd = (ctx->fsync_out_fl_arr[0])
		? SENSOR_FEATURE_SET_FRAMELENGTH_IN_LUT
		: SENSOR_FEATURE_SET_FRAMELENGTH;

	FSYNC_TRACE_BEGIN(
		"%s::imgsensor:subdrv_call, cmd:%u(SET_FL:%u/SET_FL_IN_LUT:%u)",
		__func__,
		cmd,
		SENSOR_FEATURE_SET_FRAMELENGTH,
		SENSOR_FEATURE_SET_FRAMELENGTH_IN_LUT);
	subdrv_call(ctx, feature_control, cmd, para.u8, &len);
	FSYNC_TRACE_END();
}


void fsync_util_sen_s_multi_shutter_frame_length(struct adaptor_ctx *ctx,
	u64 *ae_exp_arr, u32 ae_exp_cnt, const unsigned int multi_exp_type)
{
	enum ACDK_SENSOR_FEATURE_ENUM cmd;
	union feature_para para;
	u64 fsync_exp[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 len = 0;
	int i;

	if (likely(ae_exp_arr != NULL)) {
		for (i = 0;
			(i < ae_exp_cnt) && (i < IMGSENSOR_STAGGER_EXPOSURE_CNT);
			++i)
			fsync_exp[i] = (*(ae_exp_arr + i));
	}

	para.u64[0] = (u64)fsync_exp;
	para.u64[1] = min_t(u32, ae_exp_cnt, (u32)IMGSENSOR_STAGGER_EXPOSURE_CNT);
	para.u64[2] = ctx->fsync_out_fl;
	para.u64[3] = (u64)ctx->fsync_out_fl_arr;

	cmd = (frec_chk_if_lut_is_used(multi_exp_type))
		? SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME_IN_LUT
		: SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME;

	FSYNC_TRACE_BEGIN(
		"%s::imgsensor:subdrv_call, cmd:%u(SET_EXP_FL:%u/SET_EXP_FL_IN_LUT:%u)",
		__func__,
		cmd,
		SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME,
		SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME_IN_LUT);
	subdrv_call(ctx, feature_control, cmd, para.u8, &len);
	FSYNC_TRACE_END();
}
/******************************************************************************/


/*******************************************************************************
 * fsync utils functions --- data exchange proc (frame-sync <-> sensor driver)
 ******************************************************************************/
void fsync_util_setup_sen_hdr_info_st(struct adaptor_ctx *ctx,
	struct fs_hdr_exp_st *p_hdr_exp, const u32 mode_id)
{
	struct adaptor_sensor_lbmf_property_st lbmf_prop = {0};

	/* !!! setup basic info !!! */
	p_hdr_exp->mode_exp_cnt = g_scenario_exposure_cnt(ctx, mode_id);
	p_hdr_exp->readout_len_lc = ctx->subctx.readout_length;
	p_hdr_exp->read_margin_lc = ctx->subctx.read_margin;
	p_hdr_exp->min_vblank_lc = ctx->subctx.min_vblanking_line;

	/* setup multi exp info (HDR type, e.g., stagger, LB-MF, etc.) */
	if (g_sensor_lbmf_property(ctx, mode_id, &lbmf_prop)) {
		/* => LBMF/AEB or DCG+VS/L */
		if (chk_then_s_dcg_vsl_param(ctx, p_hdr_exp, mode_id, __func__))
			p_hdr_exp->multi_exp_type = MULTI_EXP_TYPE_DCG_VSL;
		else
			p_hdr_exp->multi_exp_type = MULTI_EXP_TYPE_LBMF;

		p_hdr_exp->exp_order = g_lbmf_exp_order(ctx, &lbmf_prop, __func__);
	} else if (g_sensor_dcg_property(ctx, mode_id)) {
		/* => DCG */
		p_hdr_exp->mode_exp_cnt = 1;
	} else {
		/* => stagger or 1exp normal using STAGGER_NE (due to flow) */
		p_hdr_exp->dol_type = g_stg_dol_type(ctx, mode_id, __func__);
		p_hdr_exp->multi_exp_type = MULTI_EXP_TYPE_STG;
	}

#ifndef REDUCE_FSYNC_UTILS_DBG_LOG
	FSYNC_MGR_LOGI(ctx,
		"sidx:%d, set p_hdr_exp(multi_exp_type:%u(STG:%d/LBMF:%d)/mode_exp_cnt:%u/exp_order:%u)\n",
		ctx->idx,
		p_hdr_exp->multi_exp_type,
		MULTI_EXP_TYPE_STG,
		MULTI_EXP_TYPE_LBMF,
		p_hdr_exp->mode_exp_cnt,
		p_hdr_exp->exp_order);
#endif
}
/******************************************************************************/

