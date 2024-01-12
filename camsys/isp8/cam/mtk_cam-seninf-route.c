// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_graph.h>
#include <linux/of_device.h>

#include <linux/videodev2.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>

#include "mtk_cam-seninf.h"
#include "mtk_cam-seninf-route.h"
#include "mtk_cam-seninf-if.h"
#include "mtk_cam-seninf-hw.h"
#include "mtk_cam-seninf-tsrec.h"
#include "imgsensor-user.h"
#include "mtk_cam-seninf-ca.h"
#include <aee.h>

#include "mtk_cam-defs.h"

#define to_std_fmt_code(code) \
	((code) & 0xFFFF)

#define get_scenario_from_fmt_code(code) \
({ \
	int __val = 0; \
	__val = ((code) >> 16) & 0xFF; \
	__val; \
})

#define PORTING_FIXME 0

void mtk_cam_seninf_alloc_outmux(struct seninf_ctx *ctx)
{
	int i;
	struct seninf_core *core = ctx->core;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	struct seninf_outmux *ent;
	bool auto_alloc;

	pr_info("[%s]+\n", __func__);

	mutex_lock(&core->mutex);

	/* allocate outmuxs if assigned */
	for (i = 0; i < vcinfo->cnt; i++) {
		auto_alloc = true;
		vc = &vcinfo->vc[i];

		/* cam is assigned */
		if (ctx->pad2cam[vc->out_pad][0] != 0xff) {
			// search local ctx
			list_for_each_entry(ent, &ctx->list_outmux, list) {
				if (ent->idx == ctx->pad2cam[vc->out_pad][0]) {
					dev_info(ctx->dev, "pad%d -> outmux%d, tag%d\n",
						 vc->out_pad,
						 ctx->pad2cam[vc->out_pad][0],
						 ctx->pad_tag_id[vc->out_pad][0]);
					auto_alloc = false;
					break;
				}
			}

			if (auto_alloc) {
				// search core
				list_for_each_entry(ent, &core->list_outmux, list) {
					if (ent->idx == ctx->pad2cam[vc->out_pad][0]) {
						list_move_tail(&ent->list,
							       &ctx->list_outmux);
						dev_info(ctx->dev, "pad%d -> outmux%d, tag%d\n",
							 vc->out_pad,
							 ctx->pad2cam[vc->out_pad][0],
							 ctx->pad_tag_id[vc->out_pad][0]);
						auto_alloc = false;
						break;
					}
				}
			}

			if (auto_alloc) {
				dev_info(ctx->dev, "outmux%d had been occupied\n",
					 ctx->pad2cam[vc->out_pad][0]);
				ctx->pad2cam[vc->out_pad][0] = 0xff;
			}
		}
	}

	/* auto allocate outmuxs */
	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];
		if (ctx->pad2cam[vc->out_pad][0] == 0xff) {

			// alloc from core
			list_for_each_entry(ent, &core->list_outmux, list) {
				list_move_tail(&ent->list,
					       &ctx->list_outmux);
				ctx->pad2cam[vc->out_pad][0] = ent->idx;
				ctx->pad_tag_id[vc->out_pad][0] = 0;//always tag0
				vc->dest_cnt = 1;
				dev_info(ctx->dev, "pad%d -> outmux%d, tag%d\n",
					 vc->out_pad,
					 ctx->pad2cam[vc->out_pad][0],
					 ctx->pad_tag_id[vc->out_pad][0]);
				break;
			}
		}
	}

	mutex_unlock(&core->mutex);

	pr_info("[%s]-\n", __func__);
}

enum CAM_TYPE_ENUM outmux2camtype(struct seninf_ctx *ctx, int outmux)
{
	struct seninf_core *core = ctx->core;
	enum CAM_TYPE_ENUM ret = TYPE_CAMSV;

	if (outmux >= 0 && outmux < SENINF_OUTMUX_NUM)
		ret = core->outmux[outmux].cam_type;

	return ret;
}

static int cammux_tag_2_fsync_target_id(struct seninf_ctx *ctx, int cammux, int tag)
{
#if PORTING_FIXME
	unsigned int const raw_cammux_factor = 2;
	int cammux_factor = 8;
	int fsync_camsv_start_id = 5;
	int fsync_pdp_start_id = 56;
	struct seninf_core *core = ctx->core;
	enum CAM_TYPE_ENUM type = outmux2camtype(ctx, cammux);
	int ret = 0xff;

	if (cammux < 0 || cammux >= 0xff) {
		ret = 0xff;
	} else if (type == TYPE_CAMSV_SAT) {
		ret = fsync_camsv_start_id
			+ (cammux - core->cammux_range[TYPE_CAMSV_SAT].first);
	} else if (type == TYPE_CAMSV_NORMAL) {
		ret = ((cammux - core->cammux_range[TYPE_CAMSV_NORMAL].first) * cammux_factor)
			+ core->cammux_range[TYPE_CAMSV_NORMAL].first
			+ fsync_camsv_start_id + tag;
	} else if (type == TYPE_RAW) {
		ret = 1 +
			((cammux - core->cammux_range[TYPE_RAW].first) / raw_cammux_factor);
	} else if (type == TYPE_PDP) {
		ret = fsync_pdp_start_id + (cammux - core->cammux_range[TYPE_PDP].first);
	}

	dev_dbg(ctx->dev, "[%s] cammux = %d, tag = %d, target_id = %d\n",
		 __func__, cammux, tag, ret);

	return ret;
#else
	return 0;
#endif
}

static void setup_fsync_vsync_src_pad(struct seninf_ctx *ctx,
	const u64 fsync_ext_vsync_pad_code)
{
	const unsigned int has_processed_data = (unsigned int)
		((fsync_ext_vsync_pad_code >> PAD_SRC_RAW_EXT0) & (u64)1);
	const unsigned int has_general_embedded = (unsigned int)
		((fsync_ext_vsync_pad_code >> PAD_SRC_GENERAL0) & (u64)1);
	const unsigned int has_pdaf_0 = (unsigned int)
		((fsync_ext_vsync_pad_code >> PAD_SRC_PDAF0) & (u64)1);
	const unsigned int has_pdaf_1 = (unsigned int)
		((fsync_ext_vsync_pad_code >> PAD_SRC_PDAF1) & (u64)1);
	const unsigned int has_pdaf_2 = (unsigned int)
		((fsync_ext_vsync_pad_code >> PAD_SRC_PDAF2) & (u64)1);
	const bool has_multi_expo = has_multiple_expo_mode(ctx);

	/* default using raw0 vsync signal */
	ctx->fsync_vsync_src_pad = PAD_SRC_RAW0;

	/* check case to overwrite */
	/* --- if pre-isp case */
	if (has_processed_data) {
		if (has_multi_expo && has_pdaf_0)
			ctx->fsync_vsync_src_pad = PAD_SRC_PDAF0;
		else if (has_multi_expo && has_pdaf_1)
			ctx->fsync_vsync_src_pad = PAD_SRC_PDAF1;
		else if (has_multi_expo && has_pdaf_2)
			ctx->fsync_vsync_src_pad = PAD_SRC_PDAF2;
		else if (has_general_embedded)
			ctx->fsync_vsync_src_pad = PAD_SRC_GENERAL0;
		else {
			ctx->fsync_vsync_src_pad = PAD_SRC_RAW0;

			dev_info(ctx->dev,
				"[%s] WARNING: fsync_ext_vsync_pad_code:%#llx, has processed_data:%u, but pdaf(0:%u/1:%u/2:%u), general_embedded:%u, force set fsync_vsync_src_pad:%d(RAW0:%d/pdaf(0:%d/1:%d/2:%d)/GENERAL0:%d)\n",
				__func__,
				fsync_ext_vsync_pad_code,
				has_processed_data,
				has_pdaf_0, has_pdaf_1, has_pdaf_2,
				has_general_embedded,
				ctx->fsync_vsync_src_pad,
				PAD_SRC_RAW0,
				PAD_SRC_PDAF0, PAD_SRC_PDAF1, PAD_SRC_PDAF2,
				PAD_SRC_GENERAL0);

			return;
		}

		dev_info(ctx->dev,
			"[%s] NOTICE: set fsync_vsync_src_pad:%d(RAW0:%d/pdaf(0:%d/1:%d/2:%d)/GENERAL0:%d), fsync_ext_vsync_pad_code:%#llx(processed_data:%u/pdaf(0:%u/1:%u/2:%u)/general_embedded:%u)\n",
			__func__,
			ctx->fsync_vsync_src_pad,
			PAD_SRC_RAW0,
			PAD_SRC_PDAF0, PAD_SRC_PDAF1, PAD_SRC_PDAF2,
			PAD_SRC_GENERAL0,
			fsync_ext_vsync_pad_code,
			has_processed_data,
			has_pdaf_0, has_pdaf_1, has_pdaf_2,
			has_general_embedded);
	}
}

/*static */void chk_is_fsync_vsync_src(struct seninf_ctx *ctx, const int pad_id)
{
	const int vsync_src_pad = ctx->fsync_vsync_src_pad;

	if (vsync_src_pad != pad_id)
		return;

	if (vsync_src_pad == PAD_SRC_RAW0) {
		// notify vc->cam
		notify_fsync_listen_target_with_kthread(ctx, 0);
	} else if (vsync_src_pad == PAD_SRC_PDAF0
		|| vsync_src_pad == PAD_SRC_PDAF1
		|| vsync_src_pad == PAD_SRC_PDAF2
		|| vsync_src_pad == PAD_SRC_GENERAL0) {

		dev_info(ctx->dev,
			"[%s] NOTICE: pad_id:%d, fsync_vsync_src_pad:%d(RAW0:%d/pdaf(0:%d/1:%d/2:%d)/GENERAL0:%d), fsync listen extra vsync signal\n",
			__func__,
			pad_id,
			vsync_src_pad,
			PAD_SRC_RAW0,
			PAD_SRC_PDAF0, PAD_SRC_PDAF1, PAD_SRC_PDAF2,
			PAD_SRC_GENERAL0);

		notify_fsync_listen_target_with_kthread(ctx, 0);
	} else {
		/* unexpected case */
		dev_info(ctx->dev,
			"[%s] ERROR: unknown fsync_vsync_src_pad:%d(RAW0:%d/pdaf(0:%d/1:%d/2:%d)/GENERAL0:%d) type, pad_id:%d\n",
			__func__,
			vsync_src_pad,
			PAD_SRC_RAW0,
			PAD_SRC_PDAF0, PAD_SRC_PDAF1, PAD_SRC_PDAF2,
			PAD_SRC_GENERAL0,
			pad_id);
	}
}

void mtk_cam_seninf_outmux_put(struct seninf_ctx *ctx, struct seninf_outmux *outmux)
{
	struct seninf_core *core = ctx->core;
	struct seninf_outmux *ent = NULL;

	// disable mux and the cammux if cammux already disabled
	g_seninf_ops->_disable_outmux(ctx, outmux->idx, true);

	mutex_lock(&core->mutex);
	list_move_tail(&outmux->list, &core->list_outmux);
	list_for_each_entry(ent, &core->list_outmux, list) {
		seninf_logd(ctx, "[%s] ent = %d\n", __func__, ent->idx);
	}
	mutex_unlock(&core->mutex);
}

void mtk_cam_seninf_get_vcinfo_test(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;

	vcinfo->cnt = 0;

	if (ctx->is_test_model == 1) {
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		ctx->cur_first_vs = 0;
		ctx->cur_last_vs = 0;
	} else if (ctx->is_test_model == 2) {
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_STAGGER_NE;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 1;
		vc->dt = 0x2b;
		vc->feature = VC_STAGGER_ME;
		vc->out_pad = PAD_SRC_RAW1;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 2;
		vc->dt = 0x2b;
		vc->feature = VC_STAGGER_SE;
		vc->out_pad = PAD_SRC_RAW2;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		ctx->cur_first_vs = 0;
		ctx->cur_last_vs = 2;
	} else if (ctx->is_test_model == 3) {
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x30;
		vc->feature = VC_PDAF_STATS;
		vc->out_pad = PAD_SRC_PDAF0;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		ctx->cur_first_vs = 0;
		ctx->cur_last_vs = 0;
	} else if (ctx->is_test_model == 4) {
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 1;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW1;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 2;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW2;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 3;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_PDAF0;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 4;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_PDAF1;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		ctx->cur_first_vs = 0;
		ctx->cur_last_vs = 4;
	} else if (ctx->is_test_model == 5) {
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 1;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_W_DATA;
		vc->out_pad = PAD_SRC_RAW_W0;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		ctx->cur_first_vs = 0;
		ctx->cur_last_vs = 1;
	}
}

unsigned int get_code2dt(unsigned int code)
{
	switch (code) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		return 0x2a;

	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		return 0x2c;

	case MEDIA_BUS_FMT_SBGGR14_1X14:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		return 0x2d;

	case MEDIA_BUS_FMT_SBGGR10_ALAW8_1X8:
	case MEDIA_BUS_FMT_SGBRG10_ALAW8_1X8:
	case MEDIA_BUS_FMT_SGRBG10_ALAW8_1X8:
	case MEDIA_BUS_FMT_SRGGB10_ALAW8_1X8:
	case MEDIA_BUS_FMT_SBGGR10_DPCM8_1X8:
	case MEDIA_BUS_FMT_SGBRG10_DPCM8_1X8:
	case MEDIA_BUS_FMT_SGRBG10_DPCM8_1X8:
	case MEDIA_BUS_FMT_SRGGB10_DPCM8_1X8:
	case MEDIA_BUS_FMT_SBGGR10_2X8_PADHI_BE:
	case MEDIA_BUS_FMT_SBGGR10_2X8_PADHI_LE:
	case MEDIA_BUS_FMT_SBGGR10_2X8_PADLO_BE:
	case MEDIA_BUS_FMT_SBGGR10_2X8_PADLO_LE:
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	default:
		return 0x2b;
	}
}

static unsigned int dt_remap_to_mipi_dt(unsigned int dt_remap)
{
	switch (dt_remap) {
	case MTK_MBUS_FRAME_DESC_REMAP_TO_RAW12:
		return 0x2c;
	case MTK_MBUS_FRAME_DESC_REMAP_TO_RAW14:
		return 0x2d;
	case MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10:
	default:
		return 0x2b;
	}
}

struct seninf_vc *mtk_cam_seninf_get_vc_by_pad(struct seninf_ctx *ctx, int idx)
{
	int i;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	unsigned int format_code, cur_dt, dt_remap, pad_dt;

	// get current scenraio output bit(/data type)
	format_code = to_std_fmt_code(ctx->fmt[PAD_SRC_RAW0].format.code);
	cur_dt = get_code2dt(format_code);
	seninf_logi(ctx, "[%s] pad %u format_code: 0x%x, cur_dt:0x%x\n",
		__func__, idx, format_code, cur_dt);

	// find vc via vc_dt or dt_remap
	for (i = 0; i < vcinfo->cnt; i++) {
		dt_remap = vcinfo->vc[i].dt_remap_to_type;
		pad_dt = (dt_remap == MTK_MBUS_FRAME_DESC_REMAP_NONE)
				? vcinfo->vc[i].dt : dt_remap_to_mipi_dt(dt_remap);
		if (vcinfo->vc[i].out_pad == idx && pad_dt == cur_dt)
			return &vcinfo->vc[i];
	}

	// if it can't find vc via vc_dt or dt_remap.
	// default: find vc via pad

	for (i = 0; i < vcinfo->cnt; i++) {
		if (vcinfo->vc[i].out_pad == idx)
			return &vcinfo->vc[i];
	}
	return NULL;
}

int mtk_cam_seninf_get_pad_data_info(struct v4l2_subdev *sd,
				unsigned int pad,
				struct mtk_seninf_pad_data_info *result)
{
	struct seninf_vc *pvc = NULL;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);

	if (!result)
		return -1;

	memset(result, 0, sizeof(*result));
	pvc = mtk_cam_seninf_get_vc_by_pad(ctx, pad);
	if (pvc) {
		result->feature = pvc->feature;
		result->exp_hsize = pvc->exp_hsize;
		result->exp_vsize = pvc->exp_vsize;
		result->mbus_code = ctx->fmt[pad].format.code;

		seninf_logd(ctx, "feature = %u, h = %u, v = %u, mbus_code = 0x%x\n",
			    result->feature,
			    result->exp_hsize,
			    result->exp_vsize,
			    result->mbus_code);

		return 0;
	}

	return -1;
}

int mtk_cam_seninf_get_active_line_info(struct v4l2_subdev *sd,
				unsigned int mbus_code,
				struct mtk_seninf_active_line_info *result)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct v4l2_subdev *sensor_sd = NULL;
	struct mtk_sensor_mode_info mode_info;
	struct mtk_seninf_pad_data_info pad_info;
	unsigned int scenario_id;

	if (!result)
		return -1;

	scenario_id = (mbus_code >> 16) & 0xff;
	memset(result, 0, sizeof(*result));

	if (ctx)
		sensor_sd = ctx->sensor_sd;

	mode_info.scenario_id = scenario_id;

	if (sensor_sd &&
	    sensor_sd->ops &&
	    sensor_sd->ops->core &&
	    sensor_sd->ops->core->command) {

		sensor_sd->ops->core->command(sensor_sd,
			V4L2_CMD_GET_SEND_SENSOR_MODE_CONFIG_INFO, &mode_info);

		result->active_line_num = mode_info.active_line_num;
		result->avg_linetime_in_ns = mode_info.avg_linetime_in_ns;

		if (!result->active_line_num) {
			if (mtk_cam_seninf_get_pad_data_info(sd,
				PAD_SRC_RAW0, &pad_info) != -1) {
				result->active_line_num = pad_info.exp_vsize;
			} else if (mtk_cam_seninf_get_pad_data_info(sd,
				PAD_SRC_RAW_EXT0, &pad_info) != -1) {
				result->active_line_num = pad_info.exp_vsize;
			}
		}
		seninf_logi(ctx, "modeid=%u, active_line=%u/%u, avg_tline_ns=%llu\n",
					mode_info.scenario_id,
					mode_info.active_line_num,
					result->active_line_num,
					mode_info.avg_linetime_in_ns);
		dev_info(ctx->dev, "mode_info.scenario_id = %u\n", mode_info.scenario_id);
	}

	return -1;
}

static int get_mbus_format_by_dt(int dt, int remap_type)
{
	int remap_dt = (remap_type == MTK_MBUS_FRAME_DESC_REMAP_NONE)
		? dt : dt_remap_to_mipi_dt(remap_type);

	switch (remap_dt) {
	case 0x2a:
		return MEDIA_BUS_FMT_SBGGR8_1X8;
	case 0x2b:
		return MEDIA_BUS_FMT_SBGGR10_1X10;
	case 0x2c:
		return MEDIA_BUS_FMT_SBGGR12_1X12;
	case 0x2d:
		return MEDIA_BUS_FMT_SBGGR14_1X14;
	default:
		/* default raw8 for other data types */
		return MEDIA_BUS_FMT_SBGGR8_1X8;
	}
}

static int get_vcinfo_by_pad_fmt(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;

	vcinfo->cnt = 0;

	switch (to_std_fmt_code(ctx->fmt[PAD_SINK].format.code)) {
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
		break;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2c;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
		break;
	default:
		return -1;
	}

	return 0;
}

#ifdef SENINF_VC_ROUTING
#define has_op(master, op) \
	(master->ops && master->ops->op)
#define call_op(master, op) \
	(has_op(master, op) ? master->ops->op(master) : 0)

/* Copy the one value to another. */
static void ptr_to_ptr(struct v4l2_ctrl *ctrl,
		       union v4l2_ctrl_ptr from, union v4l2_ctrl_ptr to)
{
	if (ctrl == NULL) {
		pr_info("%s ctrl == NULL\n", __func__);
		return;
	}
	memcpy(to.p, from.p, ctrl->elems * ctrl->elem_size);
}

/* Copy the current value to the new value */
static void cur_to_new(struct v4l2_ctrl *ctrl)
{
	if (ctrl == NULL) {
		pr_info("%s ctrl == NULL\n", __func__);
		return;
	}
	ptr_to_ptr(ctrl, ctrl->p_cur, ctrl->p_new);
}

/* Helper function to get a single control */
static int get_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ctrl *master = ctrl->cluster[0];
	int ret = 0;
	int i;

	if (ctrl->flags & V4L2_CTRL_FLAG_WRITE_ONLY) {
		pr_info("%s ctrl->flags&V4L2_CTRL_FLAG_WRITE_ONLY\n",
			__func__);
		return -EACCES;
	}

	v4l2_ctrl_lock(master);
	if (ctrl->flags & V4L2_CTRL_FLAG_VOLATILE) {
		pr_info("%s master->ncontrols:%d",
			__func__, master->ncontrols);
		for (i = 0; i < master->ncontrols; i++)
			cur_to_new(master->cluster[i]);
		ret = call_op(master, g_volatile_ctrl);
	}
	v4l2_ctrl_unlock(master);

	return ret;
}

int mtk_cam_seninf_get_csi_param(struct seninf_ctx *ctx)
{
	int ret = 0;

	struct mtk_csi_param *csi_param = &ctx->csi_param;
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;
#if AOV_GET_PARAM
	struct seninf_core *core = ctx->core;
#endif

	if (!ctx->sensor_sd)
		return -EINVAL;

	if (ctx->is_aov_real_sensor || core->aov_ut_debug_for_get_csi_param) {
		switch (core->aov_csi_clk_switch_flag) {
		case CSI_CLK_52:
		case CSI_CLK_65:
		case CSI_CLK_104:
		case CSI_CLK_130:
		case CSI_CLK_242:
		case CSI_CLK_260:
		case CSI_CLK_312:
		case CSI_CLK_416:
		case CSI_CLK_499:
			ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
				V4L2_CID_MTK_AOV_SWITCH_RX_PARAM);
			if (!ctrl) {
				dev_info(ctx->dev,
					"no(%s) in subdev(%s)\n",
					__func__, sensor_sd->name);
				return -EINVAL;
			}
			dev_info(ctx->dev,
				"[%s] aov csi clk switch to (%u)\n",
				__func__, core->aov_csi_clk_switch_flag);
			v4l2_ctrl_s_ctrl(ctrl, (unsigned int)core->aov_csi_clk_switch_flag);
			break;
		default:
			dev_info(ctx->dev,
				"[%s] csi clk not support (%u)\n",
				__func__, core->aov_csi_clk_switch_flag);
			return -EINVAL;
		}
	}

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler, V4L2_CID_MTK_CSI_PARAM);
	if (!ctrl) {
		dev_info(ctx->dev, "%s, no V4L2_CID_MTK_CSI_PARAM %s\n",
			__func__, sensor_sd->name);
		return -EINVAL;
	}
	memset(csi_param, 0, sizeof(struct mtk_csi_param));

	ctrl->p_new.p = csi_param;

	ret = get_ctrl(ctrl);
	dev_info(ctx->dev, "%s get_ctrl ret:%d %d|%d|%d|%d|%d|%d|%d|%d|%d\n",
		__func__,
		ret, csi_param->cphy_settle,
		csi_param->dphy_clk_settle,
		csi_param->dphy_data_settle,
		csi_param->dphy_trail,
		csi_param->not_fixed_trail_settle,
		csi_param->legacy_phy,
		csi_param->dphy_csi2_resync_dmy_cycle,
		csi_param->not_fixed_dphy_settle,
		csi_param->dphy_init_deskew_support);

#if AOV_GET_PARAM
	if (!(core->aov_sensor_id < 0) &&
		!(core->current_sensor_id < 0) &&
		(core->current_sensor_id == core->aov_sensor_id)) {
		g_aov_param.cphy_settle = csi_param->cphy_settle;
		g_aov_param.dphy_clk_settle = csi_param->dphy_clk_settle;
		g_aov_param.dphy_data_settle = csi_param->dphy_data_settle;
		g_aov_param.dphy_trail = csi_param->dphy_trail;
		g_aov_param.legacy_phy = csi_param->legacy_phy;
		g_aov_param.not_fixed_trail_settle = csi_param->not_fixed_trail_settle;
		g_aov_param.dphy_csi2_resync_dmy_cycle = csi_param->dphy_csi2_resync_dmy_cycle;
		g_aov_param.not_fixed_dphy_settle = csi_param->not_fixed_dphy_settle;
	}
#endif

	return 0;
}

static u16 conv_ebd_hsize_raw14(u16 exp_hsize, u8 ebd_parsing_type)
{
	u16 result = exp_hsize;

	// roundup to 8x
	switch (ebd_parsing_type) {
	case MTK_EBD_PARSING_TYPE_MIPI_RAW10:
		result = (result * 10 + 13) / 14;
		//result = (result + 7) & (~0x7);
		break;
	case MTK_EBD_PARSING_TYPE_MIPI_RAW12:
		result = (result * 12 + 13) / 14;
		//result = (result + 7) & (~0x7);
		break;
	case MTK_EBD_PARSING_TYPE_MIPI_RAW14:
		// do nothing
		break;
	default: // 8
		result = (result * 8 + 13) / 14;
		//result = (result + 7) & (~0x7);
		break;
	}

	return result;
}

int mtk_cam_seninf_get_vcinfo(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	struct v4l2_subdev_format raw_fmt;
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct mtk_mbus_frame_desc fd;
	struct v4l2_ctrl *ctrl;
	u64 fsync_ext_vsync_pad_code = 0;
	int i;
	int desc;
	int ret = 0;
	int *vcid_map = NULL;
	int j, map_cnt;

	if (!ctx->sensor_sd)
		return -EINVAL;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler, V4L2_CID_MTK_FRAME_DESC);
	if (!ctrl) {
		dev_info(ctx->dev, "%s, no V4L2_CID_MTK_FRAME_DESC %s\n",
			__func__, sensor_sd->name);
		return -EINVAL;
	}
	memset(&fd, 0, sizeof(struct mtk_mbus_frame_desc));
	ctrl->p_new.p = &fd;

	ret = get_ctrl(ctrl);

	if (ret || fd.type != MTK_MBUS_FRAME_DESC_TYPE_CSI2 || !fd.num_entries) {
		dev_info(ctx->dev, "%s get_ctrl ret:%d num_entries:%d type:%d\n", __func__,
			ret, fd.num_entries, fd.type);
		return get_vcinfo_by_pad_fmt(ctx);
	}

	vcinfo->cnt = 0;

	vcid_map = kmalloc_array(fd.num_entries, sizeof(int), GFP_KERNEL);
	map_cnt = 0;
	if (!vcid_map)
		return -EINVAL;

	mtk_cam_seninf_tsrec_reset_vc_dt_info(ctx, ctx->tsrec_idx);

	for (i = 0; i < fd.num_entries; i++) {
		struct mtk_cam_seninf_tsrec_vc_dt_info tsrec_vc_dt_info = {0};

		vc = &vcinfo->vc[vcinfo->cnt];
		vc->vc = fd.entry[i].bus.csi2.channel;
		vc->dt = fd.entry[i].bus.csi2.data_type;
		desc = fd.entry[i].bus.csi2.user_data_desc;
		vc->dt_remap_to_type = fd.entry[i].bus.csi2.dt_remap_to_type;

		for (j = 0; j < map_cnt; j++) {
			if (vcid_map[j] == vc->vc)
				break;
		}
		if (map_cnt == j) { /* not found in vc id map */
			vcid_map[j] = vc->vc;
			map_cnt = j + 1;
		}

		switch (desc) {
		case VC_3HDR_Y:
			vc->feature = VC_3HDR_Y;
			vc->out_pad = PAD_SRC_HDR0;
			break;
		case VC_3HDR_AE:
			vc->feature = VC_3HDR_AE;
			vc->out_pad = PAD_SRC_HDR1;
			break;
		case VC_3HDR_FLICKER:
			vc->feature = VC_3HDR_FLICKER;
			vc->out_pad = PAD_SRC_HDR2;
			break;
		case VC_PDAF_STATS:
			vc->feature = VC_PDAF_STATS;
			vc->out_pad = PAD_SRC_PDAF0;

			/* for determin fsync vsync signal src (pre-isp) */
			fsync_ext_vsync_pad_code |=
				((u64)1 << PAD_SRC_PDAF0);
			break;
		case VC_PDAF_STATS_PIX_1:
			vc->feature = VC_PDAF_STATS_PIX_1;
			vc->out_pad = PAD_SRC_PDAF1;

			/* for determin fsync vsync signal src (pre-isp) */
			fsync_ext_vsync_pad_code |=
				((u64)1 << PAD_SRC_PDAF1);
			break;
		case VC_PDAF_STATS_PIX_2:
			vc->feature = VC_PDAF_STATS_PIX_2;
			vc->out_pad = PAD_SRC_PDAF2;

			/* for determin fsync vsync signal src (pre-isp) */
			fsync_ext_vsync_pad_code |=
				((u64)1 << PAD_SRC_PDAF2);
			break;
		case VC_PDAF_STATS_ME_PIX_1:
			vc->feature = VC_PDAF_STATS_ME_PIX_1;
			vc->out_pad = PAD_SRC_PDAF3;
			break;
		case VC_PDAF_STATS_ME_PIX_2:
			vc->feature = VC_PDAF_STATS_ME_PIX_2;
			vc->out_pad = PAD_SRC_PDAF4;
			break;
		case VC_PDAF_STATS_SE_PIX_1:
			vc->feature = VC_PDAF_STATS_SE_PIX_1;
			vc->out_pad = PAD_SRC_PDAF5;
			break;
		case VC_PDAF_STATS_SE_PIX_2:
			vc->feature = VC_PDAF_STATS_SE_PIX_2;
			vc->out_pad = PAD_SRC_PDAF6;
			break;
		case VC_YUV_Y:
			vc->feature = VC_RAW_DATA;
			vc->out_pad = PAD_SRC_RAW0;
			vc->group = VC_CH_GROUP_RAW1;
			break;
		case VC_YUV_UV:
			vc->feature = VC_RAW_DATA;
			vc->out_pad = PAD_SRC_RAW1;
			vc->group = VC_CH_GROUP_RAW2;
			break;
		case VC_GENERAL_EMBEDDED:
			vc->feature = VC_GENERAL_EMBEDDED;
			vc->out_pad = PAD_SRC_GENERAL0;

			/* for determin fsync vsync signal src (pre-isp) */
			fsync_ext_vsync_pad_code |=
				((u64)1 << PAD_SRC_GENERAL0);
			break;
		case VC_RAW_PROCESSED_DATA:
			vc->feature = VC_RAW_DATA;
			vc->out_pad = PAD_SRC_RAW_EXT0;

			vc->group = VC_CH_GROUP_RAW1;

			/* for determin fsync vsync signal src (pre-isp) */
			fsync_ext_vsync_pad_code |=
				((u64)1 << PAD_SRC_RAW_EXT0);
			break;
		case VC_RAW_W_DATA:
			vc->feature = VC_RAW_DATA;
			vc->out_pad = PAD_SRC_RAW_W0;
			break;
		case VC_RAW_ME_W_DATA:
			vc->feature = VC_RAW_ME_W_DATA;
			vc->out_pad = PAD_SRC_RAW_W1;
			break;
		case VC_RAW_SE_W_DATA:
			vc->feature = VC_RAW_SE_W_DATA;
			vc->out_pad = PAD_SRC_RAW_W2;
			break;
		case VC_RAW_FLICKER_DATA:
			vc->feature = VC_RAW_FLICKER_DATA;
			vc->out_pad = PAD_SRC_FLICKER;
			break;
		default:
			if (vc->dt > 0x29 && vc->dt < 0x2e) {
				switch (desc) {
				case VC_STAGGER_ME:
					vc->out_pad = PAD_SRC_RAW1;
					vc->group = VC_CH_GROUP_RAW1;
					break;
				case VC_STAGGER_SE:
					vc->out_pad = PAD_SRC_RAW2;
					vc->group = VC_CH_GROUP_RAW1;
					break;
				case VC_STAGGER_NE:
				default:
					vc->out_pad = PAD_SRC_RAW0;
					vc->group = VC_CH_GROUP_RAW1;
					break;
				}
				vc->feature = VC_RAW_DATA;
			} else {
				dev_info(ctx->dev, "unknown desc %d, dt 0x%x\n",
					desc, vc->dt);
				continue;
			}
			break;
		}

		vc->exp_hsize = fd.entry[i].bus.csi2.hsize;
		vc->exp_vsize = fd.entry[i].bus.csi2.vsize;

		if (vc->dt >= 0x10 && vc->dt <= 0x17) {
			vc->exp_hsize = conv_ebd_hsize_raw14(vc->exp_hsize,
						fd.entry[i].bus.csi2.ebd_parsing_type);
		}

		switch (vc->dt) {
		case 0x28:
			vc->bit_depth = 6;
			break;
		case 0x29:
			vc->bit_depth = 7;
			break;
		case 0x2A:
		case 0x1E:
		case 0x1C:
		case 0x1A:
		case 0x18:
			vc->bit_depth = 8;
			break;
		case 0x2B:
		case 0x1F:
		case 0x19:
		case 0x1D:
			vc->bit_depth = 10;
			break;
		case 0x2C:
			vc->bit_depth = 12;
			break;
		case 0x2D:
			vc->bit_depth = 14;
			break;
		case 0x2E:
			vc->bit_depth = 16;
			break;
		case 0x2F:
			vc->bit_depth = 20;
			break;
		default:
			vc->bit_depth = 8;
			break;
		}

		switch (vc->dt_remap_to_type) {
		case MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10:
			vc->bit_depth = 10;
			break;
		case MTK_MBUS_FRAME_DESC_REMAP_TO_RAW12:
			vc->bit_depth = 12;
			break;
		case MTK_MBUS_FRAME_DESC_REMAP_TO_RAW14:
			vc->bit_depth = 14;
			break;
		default:
			break;
		}


		/* update pad fotmat */
		if (vc->exp_hsize && vc->exp_vsize) {
			ctx->fmt[vc->out_pad].format.width = vc->exp_hsize;
			ctx->fmt[vc->out_pad].format.height = vc->exp_vsize;
		}

		if (vc->feature == VC_RAW_DATA) {
			raw_fmt.pad = ctx->sensor_pad_idx;
			raw_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
			ret = v4l2_subdev_call(ctx->sensor_sd, pad, get_fmt,
					       NULL, &raw_fmt);
			if (ret) {
				dev_info(ctx->dev, "no get_fmt in %s\n",
					ctx->sensor_sd->name);
				ctx->fmt[vc->out_pad].format.code =
					get_mbus_format_by_dt(vc->dt, vc->dt_remap_to_type);
			} else {
				ctx->fmt[vc->out_pad].format.code =
					to_std_fmt_code(raw_fmt.format.code);
			}
		} else {
			ctx->fmt[vc->out_pad].format.code =
				get_mbus_format_by_dt(vc->dt, vc->dt_remap_to_type);
		}

		dev_info(ctx->dev,
			"%s vc[%d],vc:0x%x,dt:0x%x,pad:%d,exp:%dx%d,grp:0x%x,code:0x%x,fsync_ext_vsync_pad_code:%#llx\n",
			__func__,
			vcinfo->cnt, vc->vc, vc->dt, vc->out_pad,
			vc->exp_hsize, vc->exp_vsize, vc->group,
			ctx->fmt[vc->out_pad].format.code,
			fsync_ext_vsync_pad_code);

		/* update final vc dt info to tsrec */
		tsrec_vc_dt_info.vc = vc->vc;
		tsrec_vc_dt_info.dt = vc->dt;
		tsrec_vc_dt_info.out_pad = vc->out_pad;
		tsrec_vc_dt_info.cust_assign_to_tsrec_exp_id =
			fd.entry[i].bus.csi2.cust_assign_to_tsrec_exp_id;
		tsrec_vc_dt_info.is_sensor_hw_pre_latch_exp = (u32)
			fd.entry[i].bus.csi2.is_sensor_hw_pre_latch_exp;
		mtk_cam_seninf_tsrec_update_vc_dt_info(ctx,
			ctx->tsrec_idx, &tsrec_vc_dt_info);

		vcinfo->cnt++;
	}

	mtk_cam_seninf_tsrec_dbg_dump_vc_dt_info(ctx->tsrec_idx, __func__);
	setup_fsync_vsync_src_pad(ctx, fsync_ext_vsync_pad_code);

	kfree(vcid_map);

	return 0;
}
#endif // SENINF_VC_ROUTING

#ifndef SENINF_VC_ROUTING
int mtk_cam_seninf_get_vcinfo(struct seninf_ctx *ctx)
{
	return get_vcinfo_by_pad_fmt(ctx);
}
#endif

void mtk_cam_seninf_release_outmux(struct seninf_ctx *ctx)
{
	struct seninf_outmux *ent, *tmp;

	list_for_each_entry_safe(ent, tmp, &ctx->list_outmux, list) {
		mtk_cam_seninf_outmux_put(ctx, ent);
	}
}

int mtk_cam_seninf_is_vc_enabled(struct seninf_ctx *ctx, struct seninf_vc *vc)
{
#ifdef SENINF_VC_ROUTING
	return 1;
#else
	int i;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;

#ifdef SENINF_DEBUG
	if (ctx->is_test_streamon)
		return 1;
#endif

	if (vc->out_pad != PAD_SRC_RAW0 &&
		vc->out_pad != PAD_SRC_RAW1 &&
		vc->out_pad != PAD_SRC_RAW2) {
		if (media_pad_remote_pad_first(&ctx->pads[vc->out_pad]))
			return 1;
		else
			return 0;
	}

	for (i = 0; i < vcinfo->cnt; i++) {
		u8 out_pad = vcinfo->vc[i].out_pad;

		if ((out_pad == PAD_SRC_RAW0 ||
			 out_pad == PAD_SRC_RAW1 ||
			 out_pad == PAD_SRC_RAW2) &&
			media_pad_remote_pad_first(&ctx->pads[out_pad]))
			return 1;
	}

	return 0;

#endif
}

int mtk_cam_seninf_is_di_enabled(struct seninf_ctx *ctx, u8 ch, u8 dt)
{
	int i;
	struct seninf_vc *vc;

	for (i = 0; i < ctx->vcinfo.cnt; i++) {
		vc = &ctx->vcinfo.vc[i];
		if (vc->vc == ch && vc->dt == dt) {
#ifdef SENINF_DEBUG
			if (ctx->is_test_streamon)
				return 1;
#endif
			if (media_pad_remote_pad_first(&ctx->pads[vc->out_pad]))
				return 1;
			return 0;
		}
	}

	return 0;
}

int mtk_cam_seninf_set_pixelmode_camsv(struct v4l2_subdev *sd,
				 int pad_id, int pixelMode, int camtg)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;
	int i;
	int outmux;

	if (pad_id < PAD_SRC_RAW0 || pad_id >= PAD_MAXCNT) {
		pr_info("[%s][err]: no such pad id:%d\n", __func__, pad_id);
		return -EINVAL;
	}

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc) {
		pr_info("[%s][err]: invalid pad=%d\n", __func__, pad_id);
		return -EINVAL;
	}

	for (i = 0; i < vc->dest_cnt; i++) {

		dev_info(ctx->dev, "%s camtg: %d, ctx->pad2cam[pad:%d][des_cnt:%d] %d\n",
				__func__,
				camtg,
				pad_id,
				i,
				ctx->pad2cam[pad_id][i]);

		if ((outmux2camtype(ctx, camtg) !=
			outmux2camtype(ctx, ctx->pad2cam[pad_id][i]))) {
			dev_info(ctx->dev,
			"%s camtg %d camtype is mismatch ctx->pad2cam[pad:%d][des_cnt:%d] %d\n",
			__func__, camtg, pad_id, i, ctx->pad2cam[pad_id][i]);
			continue;
		}

		vc->dest[i].pix_mode = pixelMode;
		dev_info(ctx->dev, "%s update pixel mode %d for cam %d\n",
				__func__,
				vc->dest[i].pix_mode,
				ctx->pad2cam[pad_id][i]);

		if (ctx->streaming) {
			update_isp_clk(ctx);
			if (vc->dest[i].outmux == 0xFF) {
				dev_info(ctx->dev, "%s dest[%d].outmux == 0xFF\n", __func__, i);
			} else {
				outmux = vc->dest[i].outmux;

				g_seninf_ops->_wait_outmux_cfg_done(ctx, outmux);
				g_seninf_ops->_set_outmux_pixel_mode(
							ctx, outmux,
							vc->dest[i].pix_mode);

				// Program csr_config_mode. Fix config mode 0

				//Program csr_sw_cfg_done to 1
				g_seninf_ops->_set_outmux_cfg_done(ctx, outmux);

				dev_info(ctx->dev,
					"%s set outmux%d pixel_mode %d done\n",
					__func__, outmux,
					vc->dest[i].pix_mode);
			}
		}
	}
	// if streaming, update ispclk and update pixle mode seninf mux and reset

	return 0;
}

int mtk_cam_seninf_set_pixelmode(struct v4l2_subdev *sd,
				 int pad_id, int pixelMode)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_core *core;
	struct seninf_vc *vc;
	int dest_outmux;
	int i;

	if (ctx == NULL) {
		pr_info("%s [ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	if (ctx->streaming) {
		seninf_logi(ctx, "Unsupport to change in streaming state");
		return -EINVAL;
	}

	core = ctx->core;
	if (core == NULL) {
		dev_info(ctx->dev, "%s [ERROR] core is NULL\n", __func__);
		return -EINVAL;
	}

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc) {
		dev_info(ctx->dev, "no such vc by pad id:%d\n", pad_id);
		return -EINVAL;
	}

	for (i = 0; i < vc->dest_cnt; i++) {
		dest_outmux = ctx->pad2cam[pad_id][i];

		// Only available for raw and mraw in stream off state
		if (outmux2camtype(ctx, dest_outmux) != TYPE_CAMSV) {
			vc->dest[i].pix_mode = pixelMode;
			dev_info(ctx->dev, "%s update pixel mode %d for cam %d\n",
				__func__,
				vc->dest[i].pix_mode,
				dest_outmux);
		}
	}

	return 0;
}

static int mtk_cam_seninf_outmux_switch(struct seninf_ctx *ctx, struct outmux_cfg *cfg)
{
	int outmux_idx = cfg->outmux_idx;
	int src_mipi = cfg->src_mipi;
	int src_sen = cfg->src_sen;
	int pix_mode = cfg->pix_mode;
	int cfg_mode = MTK_CAM_OUTMUX_CFG_MODE_NORMAL_CFG;

	if (ctx->outmux_disable_list[outmux_idx]) {
		cfg_mode = MTK_CAM_OUTMUX_CFG_MODE_EXP_NC;
		ctx->outmux_disable_list[outmux_idx] = false;
	}

	seninf_logi(ctx, "outmux_idx %d, src_mipi %d, src_sen %d, cfg_mode %d",
		    outmux_idx, src_mipi, src_sen, cfg_mode);

	// make sure outmux cg enabled
	if (!g_seninf_ops->_is_outmux_used(ctx, outmux_idx))
		g_seninf_ops->_set_outmux_cg(ctx, outmux_idx, 1);

	// Check if csr_sw_cfg_done == 0
	g_seninf_ops->_wait_outmux_cfg_done(ctx, outmux_idx);

	// Program double buffer register
	g_seninf_ops->_config_outmux(ctx, outmux_idx, src_mipi, src_sen, cfg_mode, cfg->tag_cfg);

	// pixel mode
	g_seninf_ops->_set_outmux_pixel_mode(ctx, outmux_idx, pix_mode);

	// Program csr_config_mode. Fix config mode 0

	//Set csr_cam_cfg_rdy to 0 if SW has not received all cq_done (or other conditions)
	//Wait I2C settings done
	//Program csr_sw_cfg_done to 1
	g_seninf_ops->_set_outmux_cfg_done(ctx, outmux_idx);

	//Set csr_cam_cfg_rdy to 1 after cq_done (or other conditions) of all CAMs on this device
	//Wait cfg_done interrupt

	return 0;
}

static void mtk_cam_seninf_outmux_config_all(struct seninf_ctx *ctx,
		struct list_head *outmux_cfgs)
{
	struct outmux_cfg *ent;

	seninf_logi(ctx, "+");

	list_for_each_entry(ent, outmux_cfgs, list) {
		mtk_cam_seninf_outmux_switch(ctx, ent);
	}
}

static void mtk_cam_seninf_outmux_release_all(struct seninf_ctx *ctx,
		struct list_head *outmux_cfgs)
{
	struct list_head *pos, *n;
	struct outmux_cfg *ent;

	seninf_logi(ctx, "+");

	list_for_each_safe(pos, n, outmux_cfgs) {
		seninf_logi(ctx, "~");
		ent = list_entry(pos, struct outmux_cfg, list);
		seninf_logi(ctx, "remove outmux_cfg %u form list", ent->outmux_idx);
		list_del(pos);
		kfree(ent);
	}
}

static struct outmux_cfg *get_outmux_cfg_from_list(struct seninf_ctx *ctx,
		struct list_head *outmux_cfgs, u8 outmux)
{
	struct outmux_cfg *ret = NULL;
	struct outmux_cfg *ent;

	list_for_each_entry(ent, outmux_cfgs, list) {
		if (ent->outmux_idx == outmux) {
			ret = ent;

			seninf_logi(ctx, "get outmux %d", ret->outmux_idx);

			break;
		}
	}

	if (!ret) {
		ret = kmalloc(sizeof(struct outmux_cfg), GFP_KERNEL);
		if (ret) {
			ret->outmux_idx = outmux;

			seninf_logi(ctx, "allocate outmux %d", outmux);

			list_add_tail(&ret->list, outmux_cfgs);
		} else
			seninf_logi(ctx, "allocate outmux %d failed", outmux);
	}

	return ret;
}

int _mtk_cam_seninf_set_camtg_with_dest_idx(struct v4l2_subdev *sd, int pad_id,
				int camtg, int tag_id, u8 dest_set,
				bool from_set_camtg)
{
	int vc_en, old_outmux;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;
	struct seninf_vc_out_dest *dest;
	bool disable_last = from_set_camtg;
	struct seninf_core *core = ctx->core;

	mutex_lock(&core->cammux_page_ctrl_mutex);

	if (pad_id < PAD_SRC_RAW0 || pad_id >= PAD_MAXCNT) {
		dev_info(ctx->dev, "no such pad id:%d\n", pad_id);
		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return -EINVAL;
	}

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc) {
		dev_info(ctx->dev, "no such vc by pad id:%d\n", pad_id);
		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return -EINVAL;
	}

	if (!from_set_camtg && !ctx->streaming) {
		dev_info(ctx->dev, "%s !from_set_camtg && !ctx->streaming\n", __func__);
		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return -EINVAL;
	}

	if (dest_set >= MAX_DEST_NUM) {
		dev_info(ctx->dev, "%s reach max dest_set %d, vc->dest_cnt = %u\n",
			__func__, dest_set, vc->dest_cnt);
		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return -EINVAL;
	}

	dest = &vc->dest[dest_set];

	ctx->pad2cam[pad_id][dest_set] = camtg;
	ctx->pad_tag_id[pad_id][dest_set] = tag_id;

	vc_en = mtk_cam_seninf_is_vc_enabled(ctx, vc);

	/* change outmux while streaming */
	if (ctx->streaming && vc_en) {
#ifdef SENSOR_SECURE_MTEE_SUPPORT
		if (ctx->is_secure == 1) {
			dev_info(ctx->dev, "secure path has already exisited!");
			mutex_unlock(&core->cammux_page_ctrl_mutex);
			return 0;
		} else {
#endif // SENSOR_SECURE_MTEE_SUPPORT

			/* disable old */
			old_outmux = dest->outmux;

			if (camtg == 0xff) {
				dest->outmux = 0xff;
				if (disable_last)
					g_seninf_ops->_disable_outmux(ctx, old_outmux, false);
			} else {
				/* enable new */
				dest->outmux = camtg;
				dest->tag = tag_id;
				dest->cam_type = outmux2camtype(ctx, dest->outmux);

				seninf_logi(ctx,
					"pad %d intf %d sen %d outmux %d tag %d vc 0x%x dt 0x%x\n",
					vc->out_pad, ctx->seninfAsyncIdx, ctx->seninfSelSensor, dest->outmux,
					dest->tag, vc->vc, vc->dt);

				chk_is_fsync_vsync_src(ctx, pad_id);
			}
			seninf_logi(ctx,
				"pad %d dest[%u] outmux %d -> %d tag %d vc id %d, dt 0x%x, disable_last %d\n",
				vc->out_pad, dest_set, old_outmux, dest->outmux, dest->tag,
				vc->vc, vc->dt, disable_last);

#ifdef SENSOR_SECURE_MTEE_SUPPORT
		}
#endif
	} else {
		seninf_logi(ctx,
			"pad_id %d, dest %u camtg %d, ctx->streaming %d, vc_en %d, tag %d vc id %d, dt 0x%x\n",
			pad_id, dest_set, camtg, ctx->streaming, vc_en, tag_id, vc->vc, vc->dt);
	}

	mutex_unlock(&core->cammux_page_ctrl_mutex);

	return 0;
}

int mtk_cam_seninf_forget_camtg_setting(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	int i, j;

	// Only apply when stream off state
	if (!ctx->streaming) {
		for (i = 0; i < vcinfo->cnt; i++) {
			vc = &vcinfo->vc[i];
			vc->dest_cnt = 0;
		}
		for (i = 0; i < PAD_MAXCNT; i++)
			for (j = 0; j < MAX_DEST_NUM; j++)
				ctx->pad2cam[i][j] = 0xff;
		dev_info(ctx->dev, "%s forget all cammux and set all pad2cam to 0xff\n", __func__);
	}

	return 0;
}

static int _mtk_cam_seninf_reset_outmux(struct seninf_ctx *ctx, int pad_id)
{
	struct seninf_vc *vc;
	int old_outmux;
	u8 j;

	dev_info(ctx->dev, "[%s] +\n", __func__);

	if (pad_id < PAD_SRC_RAW0 || pad_id >= PAD_MAXCNT) {
		dev_info(ctx->dev, "no such pad id:%d\n", pad_id);
		return -EINVAL;
	}

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc) {
		seninf_logi(ctx, "no such vc by pad id:%d\n", pad_id);
		return -EINVAL;
	}

	if (!ctx->streaming) {
		dev_info(ctx->dev, "%s !ctx->streaming\n", __func__);
		return -EINVAL;
	}

	if (!!vc->dest_cnt) {
		dev_info(ctx->dev, "[%s] disable pad_id %d vc id %d dt 0x%x dest_cnt %d res %dx%d\n",
			 __func__, pad_id, vc->vc, vc->dt,
			vc->dest_cnt, vc->exp_hsize, vc->exp_vsize);
	}
	for (j = 0; j < vc->dest_cnt; j++) {
		old_outmux = vc->dest[j].outmux;

		if (old_outmux != 0xff) {
			//disable old in next sof
			g_seninf_ops->_disable_outmux(ctx, old_outmux, false);
		}

		dev_info(ctx->dev, "disable outer of pad_id(%d) old camtg(%d)\n",
			 pad_id, old_outmux);
	}

	vc->dest_cnt = 0;

	return 0;
}

int _chk_cur_mode_vc (struct seninf_ctx *ctx, struct seninf_vc *vc) {
	struct seninf_vcinfo *vcinfo = &ctx->cur_vcinfo;
	int i;

	for (i=0; i<vcinfo->cnt; i++){
		if (vcinfo->vc[i].vc == vc->vc && vcinfo->vc[i].dt == vc->dt)
			return 0;
	}

	return -1;
}

int _mtk_cam_seninf_set_camtg(struct v4l2_subdev *sd, int pad_id, int camtg, int tag_id,
			      bool from_set_camtg)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;
	int set, i;
	struct seninf_core *core = ctx->core;

	dev_info(ctx->dev, "[%s] +\n", __func__);

	mutex_lock(&core->cammux_page_ctrl_mutex);

	if (pad_id < PAD_SRC_RAW0 || pad_id >= PAD_MAXCNT) {
		dev_info(ctx->dev, "[%s][ERROR] pad_id %d is invalid\n",
			__func__, pad_id);
		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return -EINVAL;
	}

	if (camtg < 0 || camtg == 0xff) {
		/* disable all dest */
		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return _mtk_cam_seninf_reset_outmux(ctx, pad_id);
	}

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc) {
		dev_info(ctx->dev,
			"[%s] mtk_cam_seninf_get_vc_by_pad return failed by using pad %d\n",
			__func__, pad_id);
		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return -EINVAL;
	}

	/*check use vc/dt for current scenario*/
	if(!ctx->is_test_model && _chk_cur_mode_vc(ctx, vc)) {
		dev_info(ctx->dev, "[%s] no such vc/dt in cur_mode, vc 0x%x, dt 0x%x\n",
			__func__, vc->vc, vc->dt);
		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return 0;
	}

	set = vc->dest_cnt;

	if (set == 0)
		for (i = 0; i < MAX_DEST_NUM; i++) {
			vc->dest[i].outmux = 0xff;
		}

	for (i = 0; i < vc->dest_cnt; i++) {
		if (vc->dest[i].outmux == camtg) {
			seninf_logi(ctx,
				"camtg == vc->dest[%d].outmux:%u,redundantly manipulated!\n",
				i, vc->dest[i].outmux);
			mutex_unlock(&core->cammux_page_ctrl_mutex);
			return 0;
		}
	}

	if (set < MAX_DEST_NUM) {
		vc->dest_cnt += 1;
		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return _mtk_cam_seninf_set_camtg_with_dest_idx(sd, pad_id,
						camtg, tag_id, set, from_set_camtg);
	}

	dev_info(ctx->dev,
		"[%s][ERROR] current set (%d) is out of boundary(%d)\n",
		__func__, set, MAX_DEST_NUM);

	mutex_unlock(&core->cammux_page_ctrl_mutex);

	return -EINVAL;
}

int mtk_cam_seninf_set_camtg_camsv(struct v4l2_subdev *sd, int pad_id, int camtg, int tag_id)
{
	return _mtk_cam_seninf_set_camtg(sd, pad_id, camtg, tag_id, true);
}

int mtk_cam_seninf_get_tag_order(struct v4l2_subdev *sd,
		__u32 fmt_code, int pad_id)
{
	/* seninf todo: tag order */
	/* 0: first exposure 1: second exposure 2: last exposure */
	struct seninf_ctx *ctx;
	struct v4l2_subdev *sensor_sd;
	struct mtk_sensor_mode_config_info info;
	int ret = 2;  /* default return last exposure */
	int i = 0;
	int exposure_num = 0;
	int scenario = 0;


	if (sd == NULL) {
		pr_info("[%s][ERROR] sd is NULL\n", __func__);
		return -EINVAL;
	}

	ctx = container_of(sd, struct seninf_ctx, subdev);

	if (ctx == NULL) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	if (!ctx->is_test_model) {
		sensor_sd = ctx->sensor_sd;
		if (sensor_sd == NULL) {
			pr_info("[%s][ERROR] sensor_sd is NULL\n", __func__);
			return -EINVAL;
		}

		sensor_sd->ops->core->command(sensor_sd, V4L2_CMD_GET_SENSOR_MODE_CONFIG_INFO, &info);
		scenario = get_scenario_from_fmt_code(fmt_code);

		for (i = 0; i < info.count; i++) {
			if (info.seamless_scenario_infos[i].scenario_id == scenario) {
				exposure_num = info.seamless_scenario_infos[i].mode_exposure_num;
				break;
			}
		}
	}

	switch (pad_id) {
	case PAD_SRC_RAW0:
	case PAD_SRC_RAW_W0:
	case PAD_SRC_PDAF1:
		ret = 0;
		break;
	case PAD_SRC_RAW1:
	case PAD_SRC_RAW_W1:
	case PAD_SRC_PDAF3:
		switch (exposure_num) {
		case 3:
			ret = 1;
			break;

		default:
			break;
		}
		break;
	case PAD_SRC_RAW2:
	case PAD_SRC_RAW_W2:
	case PAD_SRC_PDAF5:
		ret = 2;
		break;
	default:
		break;
	}
	dev_info(ctx->dev,
			"[%s] input:pad_id(%d),scen(%d),exp_num(%d) output:tag_order(%d)\n",
			__func__,
			pad_id,
			scenario,
			exposure_num,
			ret);

	return ret;
}

int mtk_cam_seninf_get_vsync_order(struct v4l2_subdev *sd)
{
	/* todo: 0: bayer first 1: w first */
	struct seninf_ctx *ctx = NULL;
	struct seninf_vcinfo *vcinfo = NULL;
	struct seninf_vc *vc;
	int i = 0;

	if (sd == NULL) {
		pr_info("sd should not be Nullptr\n");
#ifndef REDUCE_KO_DEPENDANCY_FOR_SMT
		aee_kernel_warning_api(
				__FILE__, __LINE__, DB_OPT_DEFAULT,
				"seninf", "sd should not be Nullptr");
#endif
		return MTKCAM_IPI_ORDER_BAYER_FIRST;
	}

	ctx = container_of(sd, struct seninf_ctx, subdev);

	if (ctx == NULL) {
		pr_info("ctx should not be Nullptr\n");
#ifndef REDUCE_KO_DEPENDANCY_FOR_SMT
		aee_kernel_warning_api(
				__FILE__, __LINE__, DB_OPT_DEFAULT,
				"seninf", "ctx should not be Nullptr");
#endif
		return MTKCAM_IPI_ORDER_BAYER_FIRST;
	}

	vcinfo = &ctx->vcinfo;

	if (vcinfo == NULL) {
		dev_info(ctx->dev, "vcinfo should not be nullptr\n");
#ifndef REDUCE_KO_DEPENDANCY_FOR_SMT
		aee_kernel_warning_api(
				__FILE__, __LINE__, DB_OPT_DEFAULT,
				"seninf", "vcinfo should not be Nullptr");
#endif
		return MTKCAM_IPI_ORDER_BAYER_FIRST;
	}

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];

		if (vc == NULL) {
			dev_info(ctx->dev, "vc is nullptr at i: %d, vcinfo->cnt %d\n",
				i, vcinfo->cnt);
			return MTKCAM_IPI_ORDER_BAYER_FIRST;
		}

		switch (vc->out_pad) {
		case PAD_SRC_RAW0:
		case PAD_SRC_RAW1:
		case PAD_SRC_RAW2:
			return MTKCAM_IPI_ORDER_BAYER_FIRST;

		case PAD_SRC_RAW_W0:
		case PAD_SRC_RAW_W1:
		case PAD_SRC_RAW_W2:
			return MTKCAM_IPI_ORDER_W_FIRST;

		default:
			break;
		}
	}

	return MTKCAM_IPI_ORDER_BAYER_FIRST;
}

int mtk_cam_seninf_get_sentest_param(struct v4l2_subdev *sd,
	__u32 fmt_code,
	struct mtk_cam_seninf_sentest_param *param)
{
	struct seninf_ctx *ctx;
	struct v4l2_subdev *sensor_sd;
	struct mtk_seninf_lbmf_info info;

	if (unlikely(sd == 0)) {
		pr_info("[%s][ERROR] sd is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(param == 0)) {
		pr_info("[%s][ERROR] param is NULL\n", __func__);
		return -EINVAL;
	}

	ctx = container_of(sd, struct seninf_ctx, subdev);

	if (unlikely(ctx == 0)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	sensor_sd = ctx->sensor_sd;

	if (unlikely(sensor_sd == 0)) {
		pr_info("[%s][ERROR] sensor_sd is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&info, 0, sizeof(struct mtk_seninf_lbmf_info));

	info.scenario = get_scenario_from_fmt_code(fmt_code);

	if (sensor_sd &&
	    sensor_sd->ops &&
	    sensor_sd->ops->core &&
	    sensor_sd->ops->core->command) {

		sensor_sd->ops->core->command(sensor_sd,
			V4L2_CMD_SENSOR_GET_LBMF_TYPE_BY_SCENARIO, &info);
	}

	param->is_lbmf = info.is_lbmf;
	seninf_logd(ctx, "scenario %u is_lbmf = %d\n",
				info.scenario, param->is_lbmf);

	return 0;
}

int mtk_cam_seninf_set_camtg(struct v4l2_subdev *sd, int pad_id, int camtg)
{
	return mtk_cam_seninf_set_camtg_camsv(sd, pad_id, camtg, 0);
}

int mtk_cam_seninf_s_stream_mux(struct seninf_ctx *ctx)
{
	int i;
	u8 j;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	struct seninf_vc_out_dest *dest;
	int vc_sel, dt_sel;
	int intf = ctx->seninfAsyncIdx;
	int sen = ctx->seninfSelSensor;
	struct seninf_core *core = ctx->core;
	struct list_head outmux_cfgs;
	struct outmux_cfg *cfg;

	INIT_LIST_HEAD(&outmux_cfgs);

	// empty disable outmux list
	memset(ctx->outmux_disable_list, 0, sizeof(ctx->outmux_disable_list));

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];

		vc->enable = mtk_cam_seninf_is_vc_enabled(ctx, vc);
		if (!vc->enable) {
			dev_info(ctx->dev, "vc[%d] pad %d. skip\n",
				 i, vc->feature);
			continue;
		}

		if (ctx->is_aov_real_sensor) {
			if (!(core->aov_sensor_id < 0) &&
				!(core->current_sensor_id < 0) &&
				(core->current_sensor_id == core->aov_sensor_id)) {
				dev_info(ctx->dev,
					"[%s] aov streaming mux & cammux workaround on scp\n",
					__func__);
				break;
			}
		}

		if (!vc->dest_cnt) {
			dev_info(ctx->dev, "not set camtg yet, vc[%d] pad %d intf %d dest_cnt %u\n",
				 i, vc->out_pad, intf, vc->dest_cnt);
			continue;
		}

		for (j = 0; j < vc->dest_cnt; j++) {
			dest = &vc->dest[j];

			dest->outmux = ctx->pad2cam[vc->out_pad][j];
			dest->cam_type = outmux2camtype(ctx, dest->outmux);

			if (dest->outmux != 0xff) {
				dest->tag = ctx->pad_tag_id[vc->out_pad][j];

				vc_sel = vc->vc;
				dt_sel = vc->dt;

				// get outmux_cfg
				cfg = get_outmux_cfg_from_list(ctx, &outmux_cfgs, dest->outmux);

				cfg->src_mipi = intf;
				cfg->src_sen = sen;
				cfg->pix_mode = dest->pix_mode;
				cfg->tag_cfg[dest->tag].enable = true;
				cfg->tag_cfg[dest->tag].filt_vc = vc_sel;
				cfg->tag_cfg[dest->tag].filt_dt = dt_sel;
				cfg->tag_cfg[dest->tag].exp_hsize = vc->exp_hsize;
				cfg->tag_cfg[dest->tag].exp_vsize = vc->exp_vsize;

				seninf_logi(ctx,
					"vc[%d] dest[%u] pad %d intf %d sen %d outmux %d tag %d vc 0x%x dt 0x%x pix_mode %u\n",
					i, j, vc->out_pad, intf, sen, dest->outmux,
					dest->tag, vc_sel, dt_sel, dest->pix_mode);
			} else {
				seninf_logi(ctx, "invalid outmux, vc[%d] pad %d intf %d outmux %d\n",
					 i, vc->out_pad, intf, dest->outmux);
			}
		}
	}

	/* enable all selected outmux */
	mtk_cam_seninf_outmux_config_all(ctx, &outmux_cfgs);

	/* Free list */
	mtk_cam_seninf_outmux_release_all(ctx, &outmux_cfgs);

//#ifdef SENSOR_SECURE_MTEE_SUPPORT
//	if (ctx->is_secure != 1)
//		dev_info(ctx->dev,
//			"is not secure, won't Sensor kernel init seninf_ca");
//	else {
//		if (!seninf_ca_open_session())
//			dev_info(ctx->dev, "seninf_ca_open_session fail");

//		dev_info(ctx->dev, "Sensor kernel ca_checkpipe");
//		seninf_ca_checkpipe(ctx->SecInfo_addr);
//	}
//#endif

	return 0;
}

static int mtk_cam_seninf_get_fsync_vsync_src_cam_info(struct seninf_ctx *ctx)
{
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	int i;
	int target_id = -1;

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];

		if (vc->out_pad == ctx->fsync_vsync_src_pad) {
			/* vsync_src_pad must be first-raw or NE PDAF type or general-embedded */
			target_id = cammux_tag_2_fsync_target_id(ctx,
					vc->dest[0].outmux, vc->dest[0].tag);

			dev_info(ctx->dev,
				"[%s] fsync_vsync_src_pad:%d(RAW0:%d/pdaf(0:%d/1:%d/2:%d)/GENERAL0:%d) => vc->outmux:%d, vc->tag:%d => target_id:%d\n",
				__func__,
				ctx->fsync_vsync_src_pad,
				PAD_SRC_RAW0,
				PAD_SRC_PDAF0, PAD_SRC_PDAF1, PAD_SRC_PDAF2,
				PAD_SRC_GENERAL0,
				vc->dest[0].outmux,
				vc->dest[0].tag,
				target_id);

			return target_id;
		}
	}

	dev_info(ctx->dev, "%s: no raw data in vc channel\n", __func__);
	return -1;
}

bool
mtk_cam_seninf_streaming_mux_change(struct mtk_cam_seninf_mux_param *param)
{
	struct v4l2_subdev *sd = NULL;
	int pad_id = -1;
	int camtg = -1;
	int tag_id = -1;
	struct seninf_ctx *ctx;
	int i;
	char *buf = NULL;
	char *strptr = NULL;
	size_t buf_sz = 0;
	size_t remind = 0;
	int num = 0;
	struct list_head outmux_cfgs;
	struct outmux_cfg *cfg;
	struct seninf_vc *vc;

	if (!param)
		return false;

	remind = buf_sz = (param->num) * 50;
	strptr = buf = kzalloc(buf_sz + 1, GFP_KERNEL);
	if (!buf)
		return false;

	INIT_LIST_HEAD(&outmux_cfgs);

	// disable all camtg changing first
	for (i = 0; i < param->num; i++) {
		sd = param->settings[i].seninf;
		pad_id = param->settings[i].source;
		camtg = param->settings[i].camtg;
		ctx = container_of(sd, struct seninf_ctx, subdev);

		_mtk_cam_seninf_reset_outmux(ctx, pad_id);
	}

	// set new camtg
	for (i = 0; i < param->num; i++) {
		sd = param->settings[i].seninf;
		pad_id = param->settings[i].source;
		camtg = param->settings[i].camtg;
		tag_id = param->settings[i].tag_id;
		ctx = container_of(sd, struct seninf_ctx, subdev);

		if (pad_id < PAD_SRC_RAW0 || pad_id >= PAD_MAXCNT) {
			dev_info(ctx->dev, "[%s][ERROR] pad_id %d is invalid\n",
				 __func__, pad_id);
			continue;
		}

		if (camtg < 0 || camtg >= g_seninf_ops->outmux_num) {
			dev_info(ctx->dev, "[%s] skip pad_id %d camtg %d\n",
				 __func__, pad_id, camtg);
			continue;
		}

		if (tag_id < 0 || tag_id >= 8) {
			dev_info(ctx->dev, "[%s] pad_id%d camtg%d, tag_id is %d, fallback to 0\n",
				 __func__, pad_id, camtg, tag_id);
			tag_id = 0;
		}

		vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
		if (!vc) {
			dev_info(ctx->dev,
				 "[%s] mtk_cam_seninf_get_vc_by_pad return failed by using pad %d\n",
				 __func__, pad_id);
			continue;
		}

		dev_info(ctx->dev, "[%s] camtg = %d\n", __func__, camtg);

		mtk_cam_seninf_set_camtg_camsv(sd, pad_id, camtg, tag_id);

		// get outmux_cfg
		cfg = get_outmux_cfg_from_list(ctx, &outmux_cfgs, camtg);

		if (cfg) {
			cfg->src_mipi = ctx->seninfAsyncIdx;
			cfg->src_sen = ctx->seninfSelSensor;
			cfg->tag_cfg[tag_id].enable = true;
			cfg->tag_cfg[tag_id].filt_vc = vc->vc;
			cfg->tag_cfg[tag_id].filt_dt = vc->dt;
			cfg->tag_cfg[tag_id].exp_hsize = vc->exp_hsize;
			cfg->tag_cfg[tag_id].exp_vsize = vc->exp_vsize;
		} else {
			dev_info(ctx->dev, "[%s] get outmux cfg failed\n", __func__);
			mtk_cam_seninf_outmux_release_all(ctx, &outmux_cfgs);
			return true;
		}

		// log
		num = snprintf(strptr, remind, "pad_id[%d] %d, ctx->camtg[%d] %d, ",
			       i, param->settings[i].source,
			       i, param->settings[i].camtg);
		if (num < 0) {
			dev_info(ctx->dev, "snprintf retuns error ret = %d\n", num);
			break;
		}

		remind -= num;
		strptr += num;

	}

	if (ctx) {
		/* enable all selected outmux */
		mtk_cam_seninf_outmux_config_all(ctx, &outmux_cfgs);

		/* Free list */
		mtk_cam_seninf_outmux_release_all(ctx, &outmux_cfgs);

		/* Perform disable outmux */
		for (i = 0; i < SENINF_OUTMUX_NUM; i++) {
			if (ctx->outmux_disable_list[i]) {
				g_seninf_ops->_set_outmux_ref_vsync(ctx, i);
				g_seninf_ops->_set_outmux_cfg_done(ctx, i);
				ctx->outmux_disable_list[i] = false;
			}
		}
	}

	dev_info(ctx->dev,
		 "%s: param->num %d, %s %llu|%llu\n",
		 __func__, param->num,
		 buf,
		 ktime_get_boottime_ns(),
		 ktime_get_ns());

	kfree(buf);

	return true;
}


static void mtk_notify_vsync_fn(struct kthread_work *work)
{
	struct mtk_seninf_work *seninf_work =
		container_of(work, struct mtk_seninf_work, work);
	struct seninf_ctx *ctx = seninf_work->ctx;
	struct v4l2_ctrl *ctrl;
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	unsigned int sof_cnt = seninf_work->data.sof;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
				V4L2_CID_VSYNC_NOTIFY);
	if (!ctrl) {
		dev_info(ctx->dev, "%s, no V4L2_CID_VSYNC_NOTIFY %s\n",
			__func__,
			sensor_sd->name);
		return;
	}

//	dev_info(ctx->dev, "%s sof %s cnt %d\n",
//		__func__,
//		sensor_sd->name,
//		sof_cnt);
	v4l2_ctrl_s_ctrl(ctrl, sof_cnt);

	kfree(seninf_work);
}


void
mtk_cam_seninf_sof_notify(struct mtk_seninf_sof_notify_param *param)
{
	struct v4l2_subdev *sd = param->sd;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct mtk_seninf_work *seninf_work = NULL;
	struct v4l2_ctrl *ctrl;
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;

	if (ctx->is_test_model) {
		dev_info(ctx->dev, "[%s] test model mode, skip sof notify\n",
			__func__);
		return;
	}

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
				V4L2_CID_UPDATE_SOF_CNT);
	if (!ctrl) {
		dev_info(ctx->dev, "%s, no V4L2_CID_UPDATE_SOF_CNT %s\n",
			__func__,
			sensor_sd->name);
		return;
	}

//	dev_info(ctx->dev, "%s sof %s cnt %d\n",
//		__func__,
//		sensor_sd->name,
//		param->sof_cnt);
	v4l2_ctrl_s_ctrl(ctrl, param->sof_cnt);

	if (ctx->streaming) {
		seninf_work = kmalloc(sizeof(struct mtk_seninf_work),
				GFP_ATOMIC);
		if (seninf_work) {
			kthread_init_work(&seninf_work->work,
					mtk_notify_vsync_fn);
			seninf_work->ctx = ctx;
			seninf_work->data.sof = param->sof_cnt;
			kthread_queue_work(&ctx->core->seninf_worker,
					&seninf_work->work);
		}
	}
}

u8 is_reset_by_user(struct seninf_ctx *ctx)
{
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
			V4L2_CID_MTK_SENSOR_RESET_BY_USER);

	return (ctrl) ? v4l2_ctrl_g_ctrl(ctrl) : 0;
}

int reset_sensor(struct seninf_ctx *ctx)
{
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
			V4L2_CID_MTK_SENSOR_RESET);
	if (!ctrl) {
		dev_info(ctx->dev, "V4L2_CID_MTK_SENSOR_RESET %s\n",
			sensor_sd->name);
		return -EINVAL;
	}

	v4l2_ctrl_s_ctrl(ctrl, 1);

	return 0;
}

void mtk_cam_sensor_get_frame_cnt(struct seninf_ctx *ctx, u32 *frame_cnt)
{
	if (!ctx)
		return;

	if (ctx->sensor_sd &&
	    ctx->sensor_sd->ops &&
	    ctx->sensor_sd->ops->core &&
	    ctx->sensor_sd->ops->core->command) {
		ctx->sensor_sd->ops->core->command(ctx->sensor_sd,
						V4L2_CMD_G_SENSOR_FRAME_CNT,
						frame_cnt);
	} else {
		dev_info(ctx->dev,
			"%s: find sensor command failed\n",	__func__);
	}
}

void mtk_cam_sensor_get_glp_dt(struct seninf_ctx *ctx, struct seninf_glp_dt *info)
{
	u32 *glp = NULL;
	u32 cnt = 0;
	int i;

	if (!ctx || !info)
		return;
	glp = info->dt;

	if (ctx->sensor_sd &&
	    ctx->sensor_sd->ops &&
	    ctx->sensor_sd->ops->core &&
	    ctx->sensor_sd->ops->core->command) {
		ctx->sensor_sd->ops->core->command(ctx->sensor_sd,
						V4L2_CMD_G_SENSOR_GLP_DT,
						glp);
	} else {
		dev_info(ctx->dev,
			"%s: find sensor command failed\n",	__func__);
	}

	for (i=0; i<SEQ_DT_MAX_CNT; i++ ){
		if(glp[i])
			cnt++;
	}

	seninf_logi(ctx,
		"glp[0/1/2/3]:0x%x/0x%x/0x%x/0x%x,cnt:%d\n",
		glp[0], glp[1], glp[2], glp[3], cnt);

	info->cnt = cnt;
}

void mtk_cam_sensor_get_vc_info_by_scenario(struct seninf_ctx *ctx, u32 code)
{
	int i = 0;
	struct mtk_sensor_vc_info_by_scenario vc_sid= {0};
	struct seninf_vcinfo *vcinfo = &ctx->cur_vcinfo;
	struct seninf_vc *vc;
	int first_vc = -1;
	int last_vc = -1;
	int tmp_vc;
	bool only_one_vc = true;

	if (!ctx)
		return;

	vc_sid.scenario_id = get_scenario_from_fmt_code(code);
	if (ctx->sensor_sd &&
	    ctx->sensor_sd->ops &&
	    ctx->sensor_sd->ops->core &&
	    ctx->sensor_sd->ops->core->command) {
		ctx->sensor_sd->ops->core->command(ctx->sensor_sd,
						V4L2_CMD_G_SENSOR_VC_INFO_BY_SCENARIO,
						&vc_sid);
	} else {
		dev_info(ctx->dev,
			"%s: find sensor command failed\n",	__func__);
	}
	memset(vcinfo, 0, sizeof(struct seninf_vcinfo));

	for (i = 0; i < vc_sid.fd.num_entries; i++) {
		vc = &vcinfo->vc[i];
		vc->vc = vc_sid.fd.entry[i].bus.csi2.channel;
		vc->dt = vc_sid.fd.entry[i].bus.csi2.data_type;
		vc->dt_remap_to_type = vc_sid.fd.entry[i].bus.csi2.dt_remap_to_type;
		if (i == 0)
			tmp_vc = vc->vc;
		else if (tmp_vc != vc->vc)
			only_one_vc = false;

		if (vc_sid.fd.entry[i].bus.csi2.fs_seq == MTK_FRAME_DESC_FS_SEQ_FIRST) {
			if (first_vc != -1 && first_vc != vc->vc) {
				// TODO: assert
				dev_info(ctx->dev, "dup first_vc(%d) vc->vc(%d)\n",
					 first_vc, vc->vc);
			}
			first_vc = vc->vc;
		} else if (vc_sid.fd.entry[i].bus.csi2.fs_seq == MTK_FRAME_DESC_FS_SEQ_LAST) {
			if (last_vc != -1 && last_vc != vc->vc) {
				// TODO: assert
				dev_info(ctx->dev, "dup last_vc(%d) vc->vc(%d) is not valid\n",
					 last_vc, vc->vc);
			}
			last_vc = vc->vc;
		}
	}
	vcinfo->cnt = vc_sid.fd.num_entries;

	if (only_one_vc && first_vc != -1)
		last_vc = first_vc;

	if (first_vc == -1 || last_vc == -1) {
		// TODO: assert
		dev_info(ctx->dev, "first_vc(%d) last_vc(%d) is not valid\n",
			 first_vc, last_vc);
	}

	ctx->cur_first_vs = first_vc;
	ctx->cur_last_vs = last_vc;

	dev_info(ctx->dev, "current first vc(%d) current last vc(%d)\n",
		ctx->cur_first_vs, ctx->cur_last_vs);
}

int notify_fsync_listen_target(struct seninf_ctx *ctx)
{
	int cam_idx = mtk_cam_seninf_get_fsync_vsync_src_cam_info(ctx);
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	if (cam_idx < 0 || cam_idx >= 0xff)
		return -EINVAL;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
			V4L2_CID_FSYNC_LISTEN_TARGET);
	if (!ctrl) {
		dev_info(ctx->dev, "no fsync listen target in %s\n",
			sensor_sd->name);
		return -EINVAL;
	}

	dev_info(ctx->dev, "raw cammux usage = %d\n", cam_idx);

	v4l2_ctrl_s_ctrl(ctrl, cam_idx);

	return 0;
}

static void mtk_notify_listen_target_fn(struct kthread_work *work)
{
	struct mtk_seninf_work *seninf_work = NULL;
	struct seninf_ctx *ctx = NULL;

	// --- change to use kthread_delayed_work.
	// seninf_work = container_of(work, struct mtk_seninf_work, work);
	seninf_work = container_of(work, struct mtk_seninf_work, dwork.work);

	if (seninf_work) {
		ctx = seninf_work->ctx;
		if (ctx)
			notify_fsync_listen_target(ctx);

		kfree(seninf_work);
	}
}

void notify_fsync_listen_target_with_kthread(struct seninf_ctx *ctx,
	const unsigned int mdelay)
{
	struct mtk_seninf_work *seninf_work = NULL;

	if (ctx->streaming) {
		seninf_work = kmalloc(sizeof(struct mtk_seninf_work),
					GFP_ATOMIC);
		if (seninf_work) {
			// --- change to use kthread_delayed_work.
			// kthread_init_work(&seninf_work->work,
			//		mtk_notify_listen_target_fn);
			kthread_init_delayed_work(&seninf_work->dwork,
					mtk_notify_listen_target_fn);

			seninf_work->ctx = ctx;

			// --- change to use kthread_delayed_work.
			// kthread_queue_work(&ctx->core->seninf_worker,
			//		&seninf_work->work);
			kthread_queue_delayed_work(&ctx->core->seninf_worker,
					&seninf_work->dwork,
					msecs_to_jiffies(mdelay));
		}
	}
}


int seninf_get_fmeter_clk(struct seninf_core *core, int clk_fmeter_idx, unsigned int *out_clk)
{
	struct clk_fmeter_info *fmeter;

	if (!core || !out_clk || clk_fmeter_idx < 0 || clk_fmeter_idx >= CLK_FMETER_MAX)
		return -EINVAL;

	fmeter = &core->fmeter[clk_fmeter_idx];

#ifndef REDUCE_KO_DEPENDANCY_FOR_SMT
	if (fmeter->fmeter_no) {
		*out_clk = mt_get_fmeter_freq(fmeter->fmeter_no, fmeter->fmeter_type);
		return 0;
	}
#endif

	*out_clk = 0;

	return -EPERM;
}

bool has_multiple_expo_mode(struct seninf_ctx *ctx)
{
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct mtk_sensor_mode_config_info info;
	bool ret = false;
	int i;

	if (sensor_sd &&
	    sensor_sd->ops &&
	    sensor_sd->ops->core &&
	    sensor_sd->ops->core->command) {

		sensor_sd->ops->core->command(sensor_sd,
			V4L2_CMD_GET_SENSOR_MODE_CONFIG_INFO, &info);

		dev_info(ctx->dev, "info.cur_mode = %u, info.count = %u\n",
			 info.current_scenario_id, info.count);

		for (i = 0; i < info.count; i++) {
			dev_info(ctx->dev, "mode[%d] mode id = %u, exp_num = %u\n",
				 i,
				 info.seamless_scenario_infos[i].scenario_id,
				 info.seamless_scenario_infos[i].mode_exposure_num);

			if (info.seamless_scenario_infos[i].mode_exposure_num > 1) {
				ret = true;
				break;
			}
		}
	}

	dev_info(ctx->dev, "%s , ret = %d\n", __func__, ret);

	return ret;
}

bool is_fsync_listening_on_pd(struct v4l2_subdev *sd)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	bool ret = false;

	if (ctx->fsync_vsync_src_pad >= PAD_SRC_PDAF0 &&
	    ctx->fsync_vsync_src_pad <= PAD_SRC_PDAF6)
		ret = true;

	dev_info(ctx->dev, "%s , ret = %d\n", __func__, ret);

	return ret;
}

bool has_embedded_parser(struct v4l2_subdev *sd)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	bool ret = false;

	if (sensor_sd &&
	    sensor_sd->ops &&
	    sensor_sd->ops->core &&
	    sensor_sd->ops->core->command) {

		sensor_sd->ops->core->command(sensor_sd,
			V4L2_CMD_SENSOR_HAS_EBD_PARSER, &ret);
	}

	seninf_logd(ctx, "ret = %d\n", ret);

	return ret;
}

int mtk_cam_seninf_get_ebd_info_by_scenario(struct v4l2_subdev *sd,
				u32 scenario_mbus_code,
				struct mtk_seninf_pad_data_info *result)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct mtk_sensor_ebd_info_by_scenario ebd_info;
	int ret = -1;

	memset(result, 0, sizeof(*result));

	if (sensor_sd &&
	    sensor_sd->ops &&
	    sensor_sd->ops->core &&
	    sensor_sd->ops->core->command) {

		ebd_info.input_scenario_id = get_scenario_from_fmt_code(scenario_mbus_code);

		sensor_sd->ops->core->command(sensor_sd,
			V4L2_CMD_GET_SENSOR_EBD_INFO_BY_SCENARIO, &ebd_info);

		result->feature = VC_GENERAL_EMBEDDED;
		result->exp_hsize = ebd_info.exp_hsize;
		result->exp_vsize = ebd_info.exp_vsize;
		result->mbus_code = get_mbus_format_by_dt(ebd_info.data_type,
						ebd_info.dt_remap_to_type);

		if (ebd_info.data_type >= 0x10 && ebd_info.data_type <= 0x17) {
			result->mbus_code = MEDIA_BUS_FMT_SBGGR14_1X14;
			result->exp_hsize = conv_ebd_hsize_raw14(result->exp_hsize,
					ebd_info.ebd_parsing_type);
		}

		seninf_logd(ctx, "mode = %u, result(%u,%u,%u,0x%x)\n",
			    ebd_info.input_scenario_id,
			    result->feature,
			    result->exp_hsize,
			    result->exp_vsize,
			    result->mbus_code);

		if (result->exp_hsize && result->exp_vsize) // has ebd info
			ret = 0;
	}

	seninf_logd(ctx, "ret = %d\n", ret);

	return ret;
}

void mtk_cam_seninf_parse_ebd_line(struct v4l2_subdev *sd,
				unsigned int req_id,
				char *req_fd_desc,
				char *buf, u32 buf_sz,
				u32 stride, u32 scenario_mbus_code)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct mtk_recv_sensor_ebd_line ebd_line;
	struct mtk_sensor_ebd_info_by_scenario ebd_info;

	seninf_logd(ctx, "req_id = %u, req_fd_desc = %s, mbus = 0x%x\n",
		    req_id, req_fd_desc, scenario_mbus_code);

	if (sensor_sd &&
	    sensor_sd->ops &&
	    sensor_sd->ops->core &&
	    sensor_sd->ops->core->command) {

		ebd_info.input_scenario_id = get_scenario_from_fmt_code(scenario_mbus_code);

		sensor_sd->ops->core->command(sensor_sd,
			V4L2_CMD_GET_SENSOR_EBD_INFO_BY_SCENARIO, &ebd_info);

		ebd_line.req_id = req_id;
		ebd_line.req_fd_desc = req_fd_desc;
		ebd_line.stride = stride;
		ebd_line.buf_sz = buf_sz;
		ebd_line.buf = buf;
		ebd_line.ebd_parsing_type = ebd_info.ebd_parsing_type;
		ebd_line.mbus_code = get_mbus_format_by_dt(ebd_info.data_type,
						ebd_info.dt_remap_to_type);

		sensor_sd->ops->core->command(sensor_sd,
			V4L2_CMD_SENSOR_PARSE_EBD, &ebd_line);
	}
}

#if AOV_GET_PARAM
#ifdef SENSING_MODE_READY
/**
 * @brief: switch i2c bus scl aux function.
 *
 * GPIO 183 for R_CAM3_SCL4, its aux function on apmcu side
 * is 1 (default). So, we need to switch its aux function to 3 for
 * aov use on scp side.
 *
 */
int aov_switch_i2c_bus_scl_aux(struct seninf_ctx *ctx,
	enum mtk_cam_sensor_i2c_bus_scl aux)
{
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
		V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SCL_AUX);
	if (!ctrl) {
		dev_info(ctx->dev,
			"no(%s) in subdev(%s)\n",
			__func__, sensor_sd->name);
		return -EINVAL;
	}
	dev_info(ctx->dev,
		"[%s] find ctrl (V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SCL_AUX)\n",
		__func__);
	v4l2_ctrl_s_ctrl(ctrl, (unsigned int)aux);

	return 0;
}

/**
 * @brief: switch i2c bus sda aux function.
 *
 * GPIO 184 for R_CAM3_SDA4, its aux function on apmcu side
 * is 1 (default). So, we need to switch its aux function to 3 for
 * aov use on scp side.
 *
 */
int aov_switch_i2c_bus_sda_aux(struct seninf_ctx *ctx,
	enum mtk_cam_sensor_i2c_bus_sda aux)
{
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
		V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SDA_AUX);
	if (!ctrl) {
		dev_info(ctx->dev,
			"no(%s) in subdev(%s)\n",
			__func__, sensor_sd->name);
		return -EINVAL;
	}
	dev_info(ctx->dev,
		"[%s] find ctrl (V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SDA_AUX)\n",
		__func__);
	v4l2_ctrl_s_ctrl(ctrl, (unsigned int)aux);

	return 0;
}
#endif

/**
 * @brief: switch aov pm ops.
 *
 * switch __pm_relax/__pm_stay_awake.
 *
 */
int aov_switch_pm_ops(struct seninf_ctx *ctx,
	enum mtk_cam_sensor_pm_ops pm_ops)
{
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
		V4L2_CID_MTK_AOV_SWITCH_PM_OPS);
	if (!ctrl) {
		dev_info(ctx->dev,
			"no(%s) in subdev(%s)\n",
			__func__, sensor_sd->name);
		return -EINVAL;
	}
	dev_info(ctx->dev,
		"[%s] find ctrl (V4L2_CID_MTK_AOV_SWITCH_PM_OPS)\n",
		__func__);
	v4l2_ctrl_s_ctrl(ctrl, (unsigned int)pm_ops);

	return 0;
}

/**
 * @brief: send apmcu param to scp.
 *
 * As a callee, For sending value/address to caller: scp.
 *
 */
int mtk_cam_seninf_s_aov_param(unsigned int sensor_id,
	void *param, enum AOV_INIT_TYPE aov_seninf_init_type)
{
	unsigned int real_sensor_id = 0;
	struct seninf_ctx *ctx = NULL;
	struct seninf_vc *vc;
	struct seninf_core *core = NULL;
	struct mtk_seninf_aov_param *aov_seninf_param = (struct mtk_seninf_aov_param *)param;

	pr_info("[%s]+ sensor_id(%d),aov_seninf_init_type(%u)\n",
		__func__, sensor_id, aov_seninf_init_type);

	if (g_aov_param.is_test_model) {
		real_sensor_id = 5;
	} else {
		if (sensor_id == g_aov_param.sensor_idx) {
			real_sensor_id = g_aov_param.sensor_idx;
			pr_info("[%s] input sensor id(%u)(success)\n",
				__func__, real_sensor_id);
		} else {
			real_sensor_id = sensor_id;
			pr_info("input sensor id(%u)(fail)\n", real_sensor_id);
			seninf_aee_print(
				"[AEE] [%s] input sensor id(%u)(fail)",
				__func__, real_sensor_id);
			return -ENODEV;
		}
	}

	if (aov_ctx[real_sensor_id] != NULL) {
		pr_info("[%s] sensor idx(%u)\n", __func__, real_sensor_id);
		ctx = aov_ctx[real_sensor_id];
		core = ctx->core;
#ifdef SENSING_MODE_READY
		switch (aov_seninf_init_type) {
		case INIT_ABNORMAL_SCP_READY:
			dev_info(ctx->dev,
				"[%s] init type is abnormal(%u)!\n",
				__func__, aov_seninf_init_type);
			core->aov_abnormal_init_flag = 1;
			/* seninf/sensor streaming on */
			v4l2_subdev_call(&ctx->subdev, video, s_stream, 1);
			break;
		case INIT_NORMAL:
		default:
			dev_info(ctx->dev,
				"[%s] init type is normal(%u)!\n",
				__func__, aov_seninf_init_type);
			break;
		}
		if (!g_aov_param.is_test_model) {
			/* switch i2c bus scl from apmcu to scp */
			aov_switch_i2c_bus_scl_aux(ctx, SCL7);
			/* switch i2c bus sda from apmcu to scp */
			aov_switch_i2c_bus_sda_aux(ctx, SDA7);
			/* switch aov pm ops: pm_relax */
			aov_switch_pm_ops(ctx, AOV_PM_RELAX);
		}
#endif
		vc = mtk_cam_seninf_get_vc_by_pad(ctx, PAD_SRC_RAW0);
	} else {
		pr_info("[%s] Can't find ctx from input sensor id!\n", __func__);
		return -ENODEV;
	}
	if (!vc) {
		pr_info("[%s] vc should not be NULL!\n", __func__);
		return -ENODEV;
	}

	g_aov_param.vc = *vc;
	/* workaround */
	if (!g_aov_param.is_test_model) {
		g_aov_param.vc.dest_cnt = 1;
		//g_aov_param.vc.dest[0].mux = 14;
		//g_aov_param.vc.dest[0].mux_vr = 54;
		//g_aov_param.vc.dest[0].cam = 44;
		g_aov_param.vc.dest[0].pix_mode = 0;
		g_aov_param.camtg = 44;
	}

	if (aov_seninf_param != NULL) {
		pr_info("[%s] memcpy aov_seninf_param\n", __func__);
		memcpy((void *)aov_seninf_param, (void *)&g_aov_param,
			sizeof(struct mtk_seninf_aov_param));
		// debug use
		pr_debug(
			"[%s] port(%d)\n", __func__, aov_seninf_param->port);
		pr_debug(
			"[%s] portA(%d)\n", __func__, aov_seninf_param->portA);
		pr_debug(
			"[%s] portB(%d)\n", __func__, aov_seninf_param->portB);
		pr_debug(
			"[%s] is_4d1c(%u)\n", __func__, aov_seninf_param->is_4d1c);
		pr_debug(
			"[%s] seninfAsyncIdx(%d)\n", __func__, aov_seninf_param->seninfAsyncIdx);
		pr_debug(
			"[%s] vcinfo_cnt(%d)\n", __func__, aov_seninf_param->cnt);
		pr_debug(
			"[%s] seninf_dphy_settle_delay_dt(%d)\n",
			__func__, aov_seninf_param->seninf_dphy_settle_delay_dt);
		pr_debug(
			"[%s] cphy_settle_delay_dt(%d)\n",
			__func__, aov_seninf_param->cphy_settle_delay_dt);
		pr_debug(
			"[%s] dphy_settle_delay_dt(%d)\n",
			__func__, aov_seninf_param->dphy_settle_delay_dt);
		pr_debug(
			"[%s] settle_delay_ck(%d)\n",
			__func__, aov_seninf_param->settle_delay_ck);
		pr_debug(
			"[%s] hs_trail_parameter(%d)\n",
			__func__, aov_seninf_param->hs_trail_parameter);
		pr_debug(
			"[%s] width(%lld)\n", __func__, aov_seninf_param->width);
		pr_debug(
			"[%s] height(%lld)\n", __func__, aov_seninf_param->height);
		pr_debug(
			"[%s] hblank(%lld)\n", __func__, aov_seninf_param->hblank);
		pr_debug(
			"[%s] vblank(%lld)\n", __func__, aov_seninf_param->vblank);
		pr_debug(
			"[%s] fps_n(%d)\n", __func__, aov_seninf_param->fps_n);
		pr_debug(
			"[%s] fps_d(%d)\n", __func__, aov_seninf_param->fps_d);
		pr_debug(
			"[%s] customized_pixel_rate(%lld)\n",
			__func__, aov_seninf_param->customized_pixel_rate);
		pr_debug(
			"[%s] mipi_pixel_rate(%lld)\n",
			__func__, aov_seninf_param->mipi_pixel_rate);
		pr_debug(
			"[%s] is_cphy(%u)\n",
			__func__, aov_seninf_param->is_cphy);
		pr_debug(
			"[%s] num_data_lanes(%d)\n",
			__func__, aov_seninf_param->num_data_lanes);
		pr_debug(
			"[%s] isp_freq(%d)\n",
			__func__, aov_seninf_param->isp_freq);
		pr_debug(
			"[%s] cphy_settle(%u)\n",
			__func__, aov_seninf_param->cphy_settle);
		pr_debug(
			"[%s] dphy_clk_settle(%u)\n",
			__func__, aov_seninf_param->dphy_clk_settle);
		pr_debug(
			"[%s] dphy_data_settle(%u)\n",
			__func__, aov_seninf_param->dphy_data_settle);
		pr_debug(
			"[%s] dphy_trail(%d)\n",
			__func__, aov_seninf_param->dphy_trail);
		pr_debug(
			"[%s] legacy_phy(%d)\n",
			__func__, aov_seninf_param->legacy_phy);
		pr_debug(
			"[%s] not_fixed_trail_settle(%d)\n",
			__func__, aov_seninf_param->not_fixed_trail_settle);
		pr_debug(
			"[%s] dphy_csi2_resync_dmy_cycle(%u)\n",
			__func__, aov_seninf_param->dphy_csi2_resync_dmy_cycle);
		pr_debug(
			"[%s] not_fixed_dphy_settle(%u)\n",
			__func__, aov_seninf_param->not_fixed_dphy_settle);
		pr_debug(
			"[%s] vc(%d)\n", __func__, aov_seninf_param->vc.vc);
		pr_debug(
			"[%s] dt(%d)\n", __func__, aov_seninf_param->vc.dt);
		pr_debug(
			"[%s] feature(%d)\n", __func__, aov_seninf_param->vc.feature);
		pr_debug(
			"[%s] out_pad(%d)\n", __func__, aov_seninf_param->vc.out_pad);
		pr_debug(
			"[%s] pixel_mode(%d)\n", __func__, aov_seninf_param->vc.dest[0].pix_mode);
		pr_debug(
			"[%s] group(%d)\n", __func__, aov_seninf_param->vc.group);
		//pr_debug(
		//	"[%s] mux(%d)\n", __func__, aov_seninf_param->vc.dest[0].mux);
		//pr_debug(
		//	"[%s] mux_vr(%d)\n", __func__, aov_seninf_param->vc.dest[0].mux_vr);
		//pr_debug(
		//	"[%s] cam(%d)\n", __func__, aov_seninf_param->vc.dest[0].cam);
		pr_debug(
			"[%s] tag(%d)\n", __func__, aov_seninf_param->vc.dest[0].tag);
		pr_debug(
			"[%s] cam_type(%d)\n", __func__, aov_seninf_param->vc.dest[0].cam_type);
		pr_debug(
			"[%s] enable(%d)\n", __func__, aov_seninf_param->vc.enable);
		pr_debug(
			"[%s] exp_hsize(%d)\n", __func__, aov_seninf_param->vc.exp_hsize);
		pr_debug(
			"[%s] exp_vsize(%d)\n", __func__, aov_seninf_param->vc.exp_vsize);
		pr_debug(
			"[%s] bit_depth(%d)\n", __func__, aov_seninf_param->vc.bit_depth);
		pr_debug(
			"[%s] dt_remap_to_type(%d)\n",
			__func__, aov_seninf_param->vc.dt_remap_to_type);
	} else {
		pr_info("[%s] Must allocate buffer first!\n", __func__);
		return -ENOMEM;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_cam_seninf_s_aov_param);
#endif

