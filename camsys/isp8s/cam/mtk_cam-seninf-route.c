// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>
#include <linux/delay.h>
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
#include "mtk_cam-seninf-event-handle.h"
#include "mtk_cam-seninf-utils.h"
#include "mtk_cam-seninf-if.h"
#include "mtk_cam-seninf-hw.h"
#include "mtk_cam-seninf-tsrec.h"
#include "imgsensor-user.h"
#include "mtk_cam-seninf-ca.h"
#include "mtk_cam-seninf-pkvm.h"
#include <aee.h>

#include "mtk_cam-defs.h"
#include "mtk_cam-seninf-sentest-ctrl.h"

enum mtk_cam_seninf_rdy_set_cmd {
	MTK_CAM_SENINF_RDY_SET_SW_STATUS,
	MTK_CAM_SENINF_RDY_SET_CQ_ENABLE,
};

static inline size_t seninf_list_count(struct list_head *head)
{
	struct list_head *pos;
	size_t count = 0;

	list_for_each(pos, head)
		count++;

	return count;
}

static struct seninf_vc *mtk_cam_seninf_get_curr_vc_by_pad(struct seninf_ctx *ctx, int idx);
void mtk_cam_seninf_release_rdy_msk_grp_id(struct seninf_ctx *ctx)
{
	struct seninf_core *core = ctx->core;
	struct rdy_msk_grp_id_info *ent = NULL, *tmp = NULL;

	if (unlikely(ctx == NULL)) {
		pr_info("[%s] ctx is NULL\n" , __func__);
		return;
	}

	mutex_lock(&core->rdy_msk_list_mutex);

	list_for_each_entry_safe(ent, tmp, &ctx->list_using_rdy_msk_grp_id, list) {
		list_move_tail(&ent->list, &core->list_rdy_msk_grp_id);
		seninf_logd(ctx, "[%s] release rdy_msk grp id(%d) successfully\n", __func__, ent->grp_id);
	}

	mutex_unlock(&core->rdy_msk_list_mutex);
}

struct rdy_msk_grp_id_info *mtk_cam_seninf_get_rdy_msk_grp_id(struct seninf_ctx *ctx)
{
	struct seninf_core *core = ctx->core;
	struct rdy_msk_grp_id_info *ret_ent = NULL;
	struct rdy_msk_grp_id_info *ent = NULL, *tmp = NULL;

	if (unlikely(ctx == NULL)) {
		pr_info("[%s] ctx is NULL\n" , __func__);
		return NULL;
	}

	mutex_lock(&core->rdy_msk_list_mutex);

	list_for_each_entry_safe(ent, tmp, &core->list_rdy_msk_grp_id, list) {
		list_move_tail(&ent->list, &ctx->list_using_rdy_msk_grp_id);
		ret_ent = ent;
		break;
	}
	mutex_unlock(&core->rdy_msk_list_mutex);

	if (ret_ent)
		seninf_logd(ctx, "[%s] get rdy_msk ent id (%d) successfully\n", __func__, ret_ent->grp_id);
	else
		seninf_logi(ctx, "[%s] get rdy_msk ent failed\n", __func__);

	return ret_ent;
}

void mtk_cam_seninf_alloc_outmux(struct seninf_ctx *ctx)
{
	int i;
	struct seninf_core *core = ctx->core;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	struct seninf_vc *vc;
	struct seninf_outmux *ent;
	bool auto_alloc;
	struct mtk_cam_seninf_rdy_msk_cfg rdy_msk_cfg;
	struct rdy_msk_grp_id_info *grp_id_ent;

	pr_info("[%s]+\n", __func__);

	mutex_lock(&core->mutex);


	/* prepared default rdy msk config for UT */
	memset(&rdy_msk_cfg, 0, sizeof(struct mtk_cam_seninf_rdy_msk_cfg));

	rdy_msk_cfg.rdy_sw_en = true;
	rdy_msk_cfg.rdy_cq_en = false;
	rdy_msk_cfg.rdy_grp_en = true;

	/* get new grp id when grp en is enable */
	if (rdy_msk_cfg.rdy_grp_en) {
		grp_id_ent = mtk_cam_seninf_get_rdy_msk_grp_id(ctx);
		if (grp_id_ent == NULL) {
			dev_info(ctx->dev, "[%s][ERR] grp_id_ent is NULL, skip continue flow\n", __func__);
			return;
		}

		rdy_msk_cfg.rdy_grp_id = grp_id_ent->grp_id;
	}

	/* store rdy msk setting for stream mux config used */
	ctx->rdy_msk_config = rdy_msk_cfg;

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

		/* check if pad is existed in current mode */
		if (mtk_cam_seninf_get_curr_vc_by_pad(ctx, vc->out_pad) == NULL)
			continue;

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
		vc->bit_depth = 16;

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
		vc->bit_depth = 16;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 1;
		vc->dt = 0x2b;
		vc->feature = VC_STAGGER_ME;
		vc->out_pad = PAD_SRC_RAW1;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;
		vc->bit_depth = 16;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 2;
		vc->dt = 0x2b;
		vc->feature = VC_STAGGER_SE;
		vc->out_pad = PAD_SRC_RAW2;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;
		vc->bit_depth = 16;

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
		vc->bit_depth = 16;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x30;
		vc->feature = VC_PDAF_STATS_PIX_1;
		vc->out_pad = PAD_SRC_PDAF1;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;
		vc->bit_depth = 16;

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
		vc->bit_depth = 16;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 1;
		vc->dt = 0x2b;
		vc->feature = VC_PDAF_STATS_PIX_1;
		vc->out_pad = PAD_SRC_RAW1;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;
		vc->bit_depth = 16;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 2;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW2;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;
		vc->bit_depth = 16;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 3;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_PDAF0;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;
		vc->bit_depth = 16;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 4;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_PDAF1;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;
		vc->bit_depth = 16;

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
		vc->bit_depth = 16;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 1;
		vc->dt = 0x2b;
		vc->feature = VC_RAW_W_DATA;
		vc->out_pad = PAD_SRC_RAW_W0;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;
		vc->bit_depth = 16;

		ctx->cur_first_vs = 0;
		ctx->cur_last_vs = 1;
	} else if (ctx->is_test_model == 6) {
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 1;
		vc->dt = 0x2a;
		vc->feature = VC_META_DATA_1;
		vc->out_pad = PAD_SRC_META1;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x2b;
		vc->feature = VC_STAGGER_NE;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 2;
		vc->dt = 0x2a;
		vc->feature = VC_META_DATA_0;
		vc->out_pad = PAD_SRC_META0;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;

		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 3;
		vc->dt = 0x2e;
		vc->feature = VC_STAGGER_ME;
		vc->out_pad = PAD_SRC_RAW1;
		vc->group = 0;
		vc->exp_hsize = TEST_MODEL_HSIZE;
		vc->exp_vsize = TEST_MODEL_VSIZE;
		ctx->cur_first_vs = 0;
		ctx->cur_last_vs = 4;
	}
}

static struct seninf_vc *mtk_cam_seninf_get_curr_vc_by_pad(struct seninf_ctx *ctx, int idx)
{
	int i;
	struct seninf_vcinfo *vcinfo = &ctx->cur_vcinfo;

	for (i = 0; i < vcinfo->cnt; i++) {
		if (vcinfo->vc[i].out_pad == idx)
			return &vcinfo->vc[i];
	}

	return NULL;
}

struct seninf_vc *mtk_cam_seninf_get_vc_by_pad(struct seninf_ctx *ctx, int idx)
{
	int i;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	unsigned int format_code, cur_dt, dt_remap, pad_dt;

	// get current scenraio output bit(/data type)
	format_code = to_std_fmt_code(ctx->fmt[PAD_SRC_RAW0].format.code);
	cur_dt = get_code2dt(format_code);

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

	/* if can not target any vc, print vc info */
	seninf_logd(ctx, "[%s] target_pad %u format_code: 0x%x, cur_dt:0x%x vcinfo->cnt %d\n",
		__func__, idx, format_code, cur_dt, vcinfo->cnt);

	for (i = 0; i < vcinfo->cnt; i++)
		seninf_logd(ctx, "[%s] vc[%d],vc[%d] dt 0x%x remap to %d, pad %d\n",
		__func__, i,
		vcinfo->vc[i].vc,
		vcinfo->vc[i].dt,
		vcinfo->vc[i].dt_remap_to_type,
		vcinfo->vc[i].out_pad);

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
	case MEDIA_BUS_FMT_RGB888_1X24:
		dev_info(ctx->dev, "Set to MEDIA_BUS_FMT_RGB888_1X24\n");
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x24;
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW0;
		vc->group = 0;
		break;
	case MEDIA_BUS_FMT_YUYV8_2X8:
		dev_info(ctx->dev, "Set to MEDIA_BUS_FMT_YUYV8_2X8\n");
		vc = &vcinfo->vc[vcinfo->cnt++];
		vc->vc = 0;
		vc->dt = 0x1e;
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
	int aov_csi_port = ctx->portNum;

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
	seninf_logi(ctx,
		"get_ctrl ret:%d %d|%d|%d|%d|%d|%d|%d|%d|%d dphy_init_deskew_en:%d, cphy_lrte_en:%d\n",
		ret, csi_param->cphy_settle,
		csi_param->dphy_clk_settle,
		csi_param->dphy_data_settle,
		csi_param->dphy_trail,
		csi_param->not_fixed_trail_settle,
		csi_param->legacy_phy,
		csi_param->dphy_csi2_resync_dmy_cycle,
		csi_param->not_fixed_dphy_settle,
		csi_param->clk_lane_no_initial_flow,
		csi_param->dphy_init_deskew_support,
		csi_param->cphy_lrte_support);

#if AOV_GET_PARAM
	if (aov_csi_port < 0 || aov_csi_port >= AOV_SENINF_NUM) {
		pr_info("[%s]Error: aov_csi_port(%d) out of bound\n", __func__, aov_csi_port);
		return -1;
	}
	if (!(core->aov_sensor_id < 0) &&
		!(ctx->current_sensor_id < 0) &&
		(ctx->current_sensor_id == core->aov_sensor_id)) {
		g_aov_ctrl[aov_csi_port].aov_param.cphy_settle = csi_param->cphy_settle;
		g_aov_ctrl[aov_csi_port].aov_param.dphy_clk_settle = csi_param->dphy_clk_settle;
		g_aov_ctrl[aov_csi_port].aov_param.dphy_data_settle = csi_param->dphy_data_settle;
		g_aov_ctrl[aov_csi_port].aov_param.dphy_trail = csi_param->dphy_trail;
		g_aov_ctrl[aov_csi_port].aov_param.legacy_phy = csi_param->legacy_phy;
		g_aov_ctrl[aov_csi_port].aov_param.not_fixed_trail_settle = csi_param->not_fixed_trail_settle;
		g_aov_ctrl[aov_csi_port].aov_param.dphy_csi2_resync_dmy_cycle = csi_param->dphy_csi2_resync_dmy_cycle;
		g_aov_ctrl[aov_csi_port].aov_param.not_fixed_dphy_settle = csi_param->not_fixed_dphy_settle;
	}
#endif

	return 0;
}

int mtk_cam_seninf_get_sensor_usage(struct v4l2_subdev *sd)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	ctx->sensor_usage  = MTK_SENSOR_USAGE_SINGLE;

	if (!ctx->sensor_sd)
		return -EINVAL;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler, V4L2_CID_MTK_SENSOR_USAGE);
	if (!ctrl) {
		seninf_logd(ctx, "no V4L2_CID_MTK_SENSOR_USAGE %s set SINGLE\n",
			sensor_sd->name);
		ctx->sensor_usage = MTK_SENSOR_USAGE_SINGLE;
		return ret;
	}

	ctx->sensor_usage = (enum mtk_sensor_usage)v4l2_ctrl_g_ctrl(ctrl);
	dev_info(ctx->dev, "%s sensor_usage:%d\n", __func__, ctx->sensor_usage);
	return ret;
}

int mtk_cam_seninf_set_vc_info_to_tsrec(struct seninf_ctx *ctx, struct seninf_vc *vc,
	enum mtk_cam_seninf_tsrec_exp_id exp_id, u8 pre_latch_exp)
{
	struct mtk_cam_seninf_tsrec_vc_dt_info tsrec_vc_dt_info;

	if (unlikely(ctx == NULL)) {
		pr_info("[Error][%s] ctx is NUll", __func__);
		return -EINVAL;
	}

	if (unlikely(vc == NULL)) {
		pr_info("[Error][%s] vc is NUll", __func__);
		return -EINVAL;
	}

	memset(&tsrec_vc_dt_info, 0, sizeof(struct mtk_cam_seninf_tsrec_vc_dt_info));
	/* update final vc dt info to tsrec */
	tsrec_vc_dt_info.vc = vc->vc;
	tsrec_vc_dt_info.dt = vc->dt;
	tsrec_vc_dt_info.out_pad = vc->out_pad;
	tsrec_vc_dt_info.cust_assign_to_tsrec_exp_id = exp_id;
	tsrec_vc_dt_info.is_sensor_hw_pre_latch_exp = pre_latch_exp;
	mtk_cam_seninf_tsrec_update_vc_dt_info(ctx, ctx->tsrec_idx, &tsrec_vc_dt_info);

	return 0;
}

int mtk_cam_seninf_fill_outpad_to_vc(struct seninf_ctx *ctx,
		struct seninf_vc *vc, int desc, u64 *fsync_ext_vsync_pad_code)
{
	int ret = 0;

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
		*fsync_ext_vsync_pad_code |=
			((u64)1 << PAD_SRC_PDAF0);
		break;
	case VC_PDAF_STATS_PIX_1:
		vc->feature = VC_PDAF_STATS_PIX_1;
		vc->out_pad = PAD_SRC_PDAF1;

		/* for determin fsync vsync signal src (pre-isp) */
		*fsync_ext_vsync_pad_code |=
			((u64)1 << PAD_SRC_PDAF1);
		break;
	case VC_PDAF_STATS_PIX_2:
		vc->feature = VC_PDAF_STATS_PIX_2;
		vc->out_pad = PAD_SRC_PDAF2;

		/* for determin fsync vsync signal src (pre-isp) */
		*fsync_ext_vsync_pad_code |=
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
		*fsync_ext_vsync_pad_code |=
			((u64)1 << PAD_SRC_GENERAL0);
		break;
	case VC_RAW_PROCESSED_DATA:
		vc->feature = VC_RAW_DATA;
		vc->out_pad = PAD_SRC_RAW_EXT0;

		vc->group = VC_CH_GROUP_RAW1;

		/* for determin fsync vsync signal src (pre-isp) */
		*fsync_ext_vsync_pad_code |=
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
	case VC_META_DATA_0:
		vc->feature = VC_META_DATA_0;
		vc->out_pad = PAD_SRC_META0;
		break;
	case VC_META_DATA_1:
		vc->feature = VC_META_DATA_1;
		vc->out_pad = PAD_SRC_META1;
		break;
	default:
		if (vc->dt > 0x29 && vc->dt < 0x2f) {
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
		} else if (vc->dt == 0x24) {
			dev_info(ctx->dev, "Set vcinfo dt 0x24\n");
			vc->feature = VC_RAW_DATA;
			switch (desc) {
			case VC_BRIDGE_RAW_0:
				vc->out_pad = PAD_SRC_RAW0;
				vc->group = VC_CH_GROUP_RAW1;
				break;
			case VC_BRIDGE_RAW_1:
				vc->out_pad = PAD_SRC_RAW1;
				vc->group = VC_CH_GROUP_RAW1;
				break;
			case VC_BRIDGE_RAW_2:
				vc->out_pad = PAD_SRC_RAW2;
				vc->group = VC_CH_GROUP_RAW1;
				break;
			default:
				vc->out_pad = PAD_SRC_RAW0;
				vc->group = VC_CH_GROUP_RAW1;
				break;
			}
		} else if (vc->dt == 0x1e) {
			dev_info(ctx->dev, "Set vcinfo\n");
			vc->feature = VC_RAW_DATA;
			vc->out_pad = PAD_SRC_RAW0;
			switch (desc) {
			case VC_BRIDGE_RAW_0:
				vc->out_pad = PAD_SRC_RAW0;
				break;
			case VC_BRIDGE_RAW_1:
				vc->out_pad = PAD_SRC_RAW1;
				break;
			case VC_BRIDGE_RAW_2:
				vc->out_pad = PAD_SRC_RAW2;
				break;
			case VC_BRIDGE_RAW_3:
				vc->out_pad = PAD_SRC_RAW3;
				break;
			default:
				vc->out_pad = PAD_SRC_RAW0;
				break;
			}
		} else {
			dev_info(ctx->dev, "unknown desc %d, dt 0x%x\n",
				desc, vc->dt);
			ret = -EFAULT;
		}
		break;
	}

	return ret;
}

int mtk_cam_seninf_fill_bit_depth_to_vc(struct seninf_vc *vc,
	struct mtk_mbus_frame_desc_entry_csi2 *entry_csi2)
{
	if (unlikely(vc == NULL)) {
		pr_info("[%s][err]vc is NULL\n,", __func__);
		return -EFAULT;
	}

	if (unlikely(entry_csi2 == NULL)) {
		pr_info("[%s][err]entry_csi2 is NULL\n,", __func__);
		return -EFAULT;
	}

	switch (vc->dt) {
	/* YUV 0x18~0x1F */
	case 0x1E:
		vc->bit_depth = 8;
		break;
	case 0x1F:
		vc->bit_depth = 10;
		break;
	/* RGB 0x20~0x27 */
	case 0x24:
		vc->bit_depth = 8;
		break;
	/* RAW 0x28~0x2F */
	case 0x2A:
		vc->bit_depth = 8;
		break;
	case 0x2B:
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
	case 0x27:
		vc->bit_depth = 24;
		break;
	default:
		vc->bit_depth = 8;
		break;
	}

	/* User Defined 0x30~0x37 */
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
	case MTK_MBUS_FRAME_DESC_REMAP_TO_RAW16:
		vc->bit_depth = 16;
		break;
	default:
		break;
	}

	/* for embedded data */
	if (entry_csi2->data_type >= 0x10 && entry_csi2->data_type <= 0x17) {
		switch (entry_csi2->ebd_parsing_type) {
		case MTK_EBD_PARSING_TYPE_MIPI_RAW8:
			vc->bit_depth = 8;
			break;

		case MTK_EBD_PARSING_TYPE_MIPI_RAW10:
			vc->bit_depth = 10;
			break;

		case MTK_EBD_PARSING_TYPE_MIPI_RAW12:
			vc->bit_depth = 12;
			break;

		case MTK_EBD_PARSING_TYPE_MIPI_RAW14:
			vc->bit_depth = 14;
			break;

		default:
			pr_info("[%s][ERR]unknown entry_csi2.ebd_parsing_type %d\n",
				__func__, entry_csi2->ebd_parsing_type);
			return 0;
		}
	}

	return 0;
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

		if (mtk_cam_seninf_fill_outpad_to_vc(ctx, vc, desc, &fsync_ext_vsync_pad_code))
			continue;

		switch (vc->dt) {
		/* Generic Long 0x10~0x17 */
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
			vc->exp_hsize = fd.entry[i].bus.csi2.hsize;
			break;
		/* YUV 0x18~0x1F */
		case 0x1E:
		case 0x1F:
			vc->exp_hsize = fd.entry[i].bus.csi2.hsize * 2; /* YUV422 */
			break;
		/* RGB 0x20~0x27 */
		case 0x24:
			vc->exp_hsize = fd.entry[i].bus.csi2.hsize * 3; /* RGB888 */
			break;
		/* RAW 0x28~0x2F */
		case 0x2A:
		case 0x2B:
		case 0x2C:
		case 0x2D:
		case 0x2E:
		case 0x2F:
		case 0x27:
		default:
			vc->exp_hsize = fd.entry[i].bus.csi2.hsize;
			break;
		}

#ifdef DOUBLE_PIXEL_EN
		/* double pixel mode */
		switch (vc->dt) {
		/* ExtDT */
		case 0x18:
		case 0x1A:
		case 0x1C:
		case 0x1E:
		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
		case 0x24:
		case 0x25:
		case 0x26:
		case 0x28:
		case 0x29:
			if (!strcasecmp(_seninf_ops->iomem_ver, MT6899_IOMOM_VERSIONS))
				vc->exp_hsize = vc->exp_hsize / 2;
			break;
		/* Raw8 */
		case 0x2A:
			vc->exp_hsize = vc->exp_hsize / 2;
			break;
		default:
			break;
		}
#endif

		vc->exp_vsize = fd.entry[i].bus.csi2.vsize;

		mtk_cam_seninf_fill_bit_depth_to_vc(vc, &fd.entry[i].bus.csi2);

#ifdef DOUBLE_PIXEL_EN
		/* double pixel mode */
		switch (vc->dt) {
		/* ExtDT */
		case 0x18:
		case 0x1A:
		case 0x1C:
		case 0x1E:
		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
		case 0x24:
		case 0x25:
		case 0x26:
		case 0x28:
		case 0x29:
			if (!strcasecmp(_seninf_ops->iomem_ver, MT6899_IOMOM_VERSIONS))
				vc->bit_depth = vc->bit_depth * 2;
			break;
		/* Raw8 */
		case 0x2A:
			vc->bit_depth = vc->bit_depth * 2;
			break;
		default:
			break;
		}
#endif

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
		if(vc->dt == 0x24) {
			ctx->fmt[vc->out_pad].format.code =
			get_mbus_format_by_dt(vc->dt, vc->dt_remap_to_type);
		}

		dev_info(ctx->dev,
			"%s vc[%d],vc:0x%x,dt:0x%x,pad:%d,exp:%dx%d,grp:0x%x,code:0x%x,remap to %d fsync_ext_vsync_pad_code:%#llx\n",
			__func__,
			vcinfo->cnt,
			vc->vc,
			vc->dt,
			vc->out_pad,
			vc->exp_hsize,
			vc->exp_vsize,
			vc->group,
			ctx->fmt[vc->out_pad].format.code,
			vc->dt_remap_to_type,
			fsync_ext_vsync_pad_code);

		/* update final vc dt info to tsrec */
		mtk_cam_seninf_set_vc_info_to_tsrec(ctx, vc,
						fd.entry[i].bus.csi2.cust_assign_to_tsrec_exp_id,
						fd.entry[i].bus.csi2.is_sensor_hw_pre_latch_exp);

		/*overwrite tsrec vc info if sentest is on */
		seninf_sentest_set_tsrec_manual_vc_config(ctx, vc);

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

	mtk_cam_seninf_release_rdy_msk_grp_id(ctx);
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

	if (ctx->streaming) {
		seninf_logi(ctx, "Unsupport to change in streaming state, pad_id %d outmux%d pixmode %d",
			    pad_id, camtg, pixelMode);
		return -EINVAL;
	}

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

		if ((outmux2camtype(ctx, camtg) !=
			outmux2camtype(ctx, ctx->pad2cam[pad_id][i]))) {
			seninf_logd(ctx,
			"camtg %d camtype is mismatch ctx->pad2cam[pad:%d][des_cnt:%d] %d\n",
			camtg, pad_id, i, ctx->pad2cam[pad_id][i]);
			continue;
		}

		vc->dest[i].pix_mode = pixelMode;
		seninf_logd(ctx, "camtg:%d, update pixel mode %d for pad2cam[%d][%d]:%d\n",
			    camtg,
			    vc->dest[i].pix_mode,
			    pad_id, i,
			    ctx->pad2cam[pad_id][i]);
	}

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

	if (pad_id < PAD_SRC_RAW0 || pad_id >= PAD_MAXCNT) {
		seninf_logi(ctx, "[err]: no such pad id:%d\n", pad_id);
		return -EINVAL;
	}

	if (ctx->streaming) {
		seninf_logi(ctx, "Unsupport to change in streaming state, pad_id %d pixmode %d",
			    pad_id, pixelMode);
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
			seninf_logd(ctx, "update pixel mode %d for pad2cam[%d][%d]:%d\n",
				    vc->dest[i].pix_mode,
				    pad_id, i,
				    dest_outmux);
		}
	}

	return 0;
}

static int mtk_cam_seninf_outmux_switch_check_done_status(struct seninf_ctx *ctx,
	struct outmux_cfg *cfg, bool from_switch, bool *skip_chk, bool *sensor_delay)
{
	int outmux_idx = cfg->outmux_idx;

	seninf_logd(ctx, "outmux_idx %d", outmux_idx);

	// make sure outmux cg enabled
	if (!g_seninf_ops->_is_outmux_used(ctx, outmux_idx))
		g_seninf_ops->_set_outmux_cg(ctx, outmux_idx, 1);

	// Check if csr_sw_cfg_done == 0
	g_seninf_ops->_wait_outmux_cfg_done(ctx, outmux_idx);

	return 0;
}

static int mtk_cam_seninf_outmux_switch_config(struct seninf_ctx *ctx, struct outmux_cfg *cfg,
					bool from_switch, bool sensor_delay)
{
	int outmux_idx = cfg->outmux_idx;
	int src_mipi = cfg->src_mipi;
	int src_sen = cfg->src_sen;
	int pix_mode = cfg->pix_mode;
	int cfg_mode = MTK_CAM_OUTMUX_CFG_MODE_NORMAL_CFG;

	// pixel mode
	g_seninf_ops->_set_outmux_pixel_mode(ctx, outmux_idx, pix_mode);

	// Program double buffer register
	g_seninf_ops->_config_outmux(ctx, outmux_idx, src_mipi, src_sen, cfg_mode, cfg->tag_cfg);

	// set rdy msk config
	g_seninf_ops->_set_outmux_rdy_msk_cfg(ctx, outmux_idx);

	// set seninf DL EN
	g_seninf_ops->_set_outmux_dl_en(ctx, outmux_idx, true);

	return 0;
}

static int mtk_cam_seninf_outmux_switch_tirgger_cfg_done(struct seninf_ctx *ctx, struct outmux_cfg *cfg)
{
	int outmux_idx = cfg->outmux_idx;
	return g_seninf_ops->_set_outmux_cfg_done(ctx, outmux_idx);
}

static void mtk_cam_seninf_outmux_config_all(struct seninf_ctx *ctx,
		struct list_head *outmux_cfgs, bool from_switch)
{
	struct outmux_cfg *ent;
	bool skip_chk = false;
	bool sensor_delay = false;

	list_for_each_entry(ent, outmux_cfgs, list) {
		mtk_cam_seninf_outmux_switch_check_done_status(ctx, ent, from_switch,
						&skip_chk, &sensor_delay);
	}
	list_for_each_entry(ent, outmux_cfgs, list) {
		mtk_cam_seninf_outmux_switch_config(ctx, ent, from_switch, sensor_delay);
	}
	/* raise all outmux cfg done at same time */
	list_for_each_entry(ent, outmux_cfgs, list) {
		mtk_cam_seninf_outmux_switch_tirgger_cfg_done(ctx, ent);
	}
	seninf_logi(ctx, "raise all outmux cfg done");
}

static void mtk_cam_seninf_outmux_release_all(struct seninf_ctx *ctx,
		struct list_head *outmux_cfgs)
{
	struct list_head *pos, *n;
	struct outmux_cfg *ent;

	list_for_each_safe(pos, n, outmux_cfgs) {
		ent = list_entry(pos, struct outmux_cfg, list);
		seninf_logd(ctx, "remove outmux_cfg %u form list", ent->outmux_idx);
		list_del(pos);
		kfree(ent);
	}
}

static void mtk_cam_seninf_outmux_reset_all(struct seninf_ctx *ctx,
		struct list_head *outmux_cfgs, int is_mux_change)
{
	struct outmux_cfg *ent;

	list_for_each_entry(ent, outmux_cfgs, list) {
		if (is_mux_change){
			/*(seamless only) reset all selected outmux*/
			if (ent->is_reset_immedate)
				g_seninf_ops->_disable_outmux(ctx, ent->outmux_idx, true);
		} else {
			/* force disable outmux before config coming setting */
			g_seninf_ops->_disable_outmux(ctx, ent->outmux_idx, true);
		}
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

			seninf_logd(ctx, "get outmux %d", ret->outmux_idx);

			break;
		}
	}

	if (!ret) {
		ret = kzalloc(sizeof(struct outmux_cfg), GFP_KERNEL);
		if (ret) {
			memset(ret, 0, sizeof(struct outmux_cfg));
			ret->outmux_idx = outmux;

			seninf_logd(ctx, "allocate outmux %d", outmux);
			list_add_tail(&ret->list, outmux_cfgs);
		} else
			seninf_logi(ctx, "allocate outmux %d failed", outmux);
	}

	return ret;
}

int _mtk_cam_seninf_set_camtg_with_dest_idx(struct v4l2_subdev *sd,
	struct mtk_cam_seninf_set_camtg_cfg *cfg, int dest_set)
{
	u16 pad_id = cfg->pad_id;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;
	struct seninf_core *core = ctx->core;
	struct seninf_vc_out_dest *dest;

	mutex_lock(&core->cammux_page_ctrl_mutex);

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
	if (!vc) {
		dev_info(ctx->dev, "no such vc by pad id:%d\n", pad_id);
		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return -EINVAL;
	}

	if (unlikely((dest_set < 0 || dest_set >= MAX_DEST_NUM))) {
		pr_info("dest_set is invalid: %d\n", dest_set);
		return -EINVAL;
	}

	if (unlikely(pad_id >= PAD_MAXCNT)) {
		pr_info("pad_id is invalid: %d\n", pad_id);
		return -EINVAL;
	}

	ctx->pad2cam[pad_id][dest_set] = cfg->camtg;
	ctx->pad_tag_id[pad_id][dest_set] = cfg->tag_id;
	ctx->rdy_msk_config = cfg->rdy_msk_cfg;
	vc->enable = true;

	dest = &vc->dest[dest_set];
	dest->outmux = cfg->camtg;
	dest->tag = cfg->tag_id;
	dest->cam_type = outmux2camtype(ctx, dest->outmux);

	mutex_unlock(&core->cammux_page_ctrl_mutex);

	seninf_logi(ctx,
			"ctx->streaming %d, pad_id %d, dest_idx %u camtg %d,  tag %d vc id %d, dt 0x%x rdy_msk_sw_en %d rdy_cq_en %d rdy_grp_en %d rdy_grp_id %u\n",
			ctx->streaming,
			cfg->pad_id,
			dest_set,
			ctx->pad2cam[pad_id][dest_set],
			ctx->pad_tag_id[pad_id][dest_set],
			vc->vc,
			vc->dt,
			ctx->rdy_msk_config.rdy_sw_en,
			ctx->rdy_msk_config.rdy_cq_en,
			ctx->rdy_msk_config.rdy_grp_en,
			ctx->rdy_msk_config.rdy_grp_id);

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

static int _mtk_cam_seninf_reset_outmux_outer(struct seninf_ctx *ctx, int pad_id)
{
	struct seninf_vc *vc;
	struct seninf_vcinfo *vcinfo = &ctx->vcinfo;
	int old_outmux;
	u8 i, j;

	if (pad_id < PAD_SRC_RAW0 || pad_id >= PAD_MAXCNT) {
		dev_info(ctx->dev, "no such pad id:%d\n", pad_id);
		return -EINVAL;
	}

	for (i = 0; i < vcinfo->cnt; i++) {
		if (vcinfo->vc[i].out_pad != pad_id)
			continue;

		vc = &vcinfo->vc[i];
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
				//disable vc dt filter when next sof
				g_seninf_ops->_disable_outmux(ctx, old_outmux, false);
			}

			/**
			 *	disable rdy msk grp_en & sw rdy  for all using out mux to prevent
			 *  causing out mux masking by old outmux
			 */
			g_seninf_ops->_set_outmux_rdy_msk_sw_rdy_status(ctx, old_outmux, false);
			g_seninf_ops->_set_outmux_rdy_msk_grp_en(ctx, old_outmux, false);

			seninf_logd(ctx, "disable outer of pad_id(%d) old camtg(%d)\n",
				 pad_id, old_outmux);
		}

		vc->dest_cnt = 0;
	}

	return 0;
}

int _chk_cur_mode_vc (struct seninf_ctx *ctx, struct seninf_vc *vc)
{
	struct seninf_vcinfo *vcinfo = &ctx->cur_vcinfo;
	int i;

	for (i=0; i<vcinfo->cnt; i++){
		if (vcinfo->vc[i].vc == vc->vc && vcinfo->vc[i].dt == vc->dt)
			return 0;
	}

	return -1;
}

int mtk_cam_seninf_is_non_comb_ic(struct v4l2_subdev *sd)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);

	if (ctx->sensor_usage == MTK_SENSOR_USAGE_NONCOMB) {
		pr_info("%s: yes\n", __func__);
		return 1;
	}
	return 0;
}

int _mtk_cam_seninf_set_camtg(struct v4l2_subdev *sd,
	struct mtk_cam_seninf_set_camtg_cfg *camtg_cfg)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_vc *vc;
	int set, i;
	struct seninf_core *core = ctx->core;

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][Err]ctx is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(camtg_cfg == NULL)) {
		dev_info(ctx->dev, "camtg_cfg is NULL\n");
		return -EINVAL;
	}

	mutex_lock(&core->cammux_page_ctrl_mutex);

	if (camtg_cfg->pad_id < PAD_SRC_RAW0 || camtg_cfg->pad_id >= PAD_MAXCNT) {
		dev_info(ctx->dev, "[%s][ERROR] pad_id %d is invalid\n",
			__func__, camtg_cfg->pad_id);
		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return -EINVAL;
	}

	if (camtg_cfg->camtg == 0xff) {
		/* disable all dest */
		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return _mtk_cam_seninf_reset_outmux_outer(ctx, camtg_cfg->pad_id);
	}

	vc = mtk_cam_seninf_get_vc_by_pad(ctx, camtg_cfg->pad_id);
	if (!vc) {
		dev_info(ctx->dev,
			"[%s] mtk_cam_seninf_get_vc_by_pad return failed by using pad %d\n",
			__func__, camtg_cfg->pad_id);
		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return -EINVAL;
	}

	/*check use vc/dt for current scenario*/
	if(!ctx->is_test_model && _chk_cur_mode_vc(ctx, vc)) {
		dev_info_ratelimited(ctx->dev, "[%s] no such vc/dt in cur_mode, vc 0x%x, dt 0x%x\n",
			__func__, vc->vc, vc->dt);
		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return 0;
	}

	set = vc->dest_cnt;

	if (set == 0)
		for (i = 0; i < MAX_DEST_NUM; i++)
			vc->dest[i].outmux = 0xff;

	/* check if camtg already exists in dest[] */
	for (i = 0; i < vc->dest_cnt; i++) {
		if (vc->dest[i].outmux != camtg_cfg->camtg)
			continue;

		if (vc->dest[i].tag != camtg_cfg->tag_id)
			continue;

		seninf_logi(ctx,
			"camtg == vc->dest[%d].outmux:%u, tag %u redundantly manipulated!\n",
			i,
			vc->dest[i].outmux,
			vc->dest[i].tag);

		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return 0;
	}

	if (set >= MAX_DEST_NUM) {
		dev_info(ctx->dev, "[%s][ERROR] current set (%d) is out of boundary(%d)\n",
			__func__, set, MAX_DEST_NUM);
		mutex_unlock(&core->cammux_page_ctrl_mutex);
		return -EINVAL;
	}

	vc->dest_cnt += 1;
	mutex_unlock(&core->cammux_page_ctrl_mutex);
	return _mtk_cam_seninf_set_camtg_with_dest_idx(sd, camtg_cfg, set);
}

static int mtk_cam_seninf_set_camtg_for_stream_mux(struct mtk_cam_seninf_mux_param *param)
{
	struct mtk_cam_seninf_set_camtg_cfg cfg;
	struct seninf_ctx *ctx = NULL;
	struct v4l2_subdev *sd = NULL;
	struct rdy_msk_grp_id_info *grp_id_ent;
	int i, ret = 0;

	if (unlikely(param == NULL)) {
		pr_info("[%s][ERR] param is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(param->num == 0)) {
		pr_info("[%s][ERR] param->num is NULL\n", __func__);
		return -EINVAL;
	}

	sd = param->settings[0].seninf;
	ctx = container_of(sd, struct seninf_ctx, subdev);

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	/* get new grp id when grp en is enable */
	if (param->rdy_mask_en.rdy_grp_en) {
		grp_id_ent = mtk_cam_seninf_get_rdy_msk_grp_id(ctx);
		if (grp_id_ent == NULL) {
			pr_info("[%s][ERR] grp_id_ent is NULL, skip continue flow\n", __func__);
			return -EFAULT;
		}
	}

	/* compose configuration for each out mux */
	for (i = 0; i < param->num; i++) {
		sd = param->settings[i].seninf;
		ctx = container_of(sd, struct seninf_ctx, subdev);

		memset(&cfg, 0, sizeof(struct mtk_cam_seninf_set_camtg_cfg));

		cfg.pad_id = param->settings[i].source;
		cfg.camtg = param->settings[i].camtg;
		cfg.tag_id = param->settings[i].tag_id;

		cfg.rdy_msk_cfg.rdy_sw_en = param->rdy_mask_en.rdy_sw_en;
		cfg.rdy_msk_cfg.rdy_cq_en = param->rdy_mask_en.rdy_cq_en;

		if (param->rdy_mask_en.rdy_grp_en) {
			cfg.rdy_msk_cfg.rdy_grp_id = grp_id_ent->grp_id;
			cfg.rdy_msk_cfg.rdy_grp_en = param->rdy_mask_en.rdy_grp_en;
		}

		ret |= _mtk_cam_seninf_set_camtg(sd, &cfg);
	}

	return ret;
}

static bool mtk_cam_seninf_set_mux_rdy_msk_cfg_by_cmd(struct seninf_ctx *ctx, u8 camtg,
		enum mtk_cam_seninf_rdy_set_cmd cmd, bool value)
{
	u16 i, j;
	bool ret = false;
	struct seninf_vc *vc;
	struct seninf_vcinfo *vcinfo;

	vcinfo = &ctx->vcinfo;

	if (unlikely(vcinfo == NULL)) {
		dev_info(ctx->dev, "vcinfo is NULL\n");
		return -EINVAL;
	}

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];

		if (!vc->dest_cnt)
			continue;

		for (j =0; j < vc->dest_cnt; j++) {
			switch (cmd) {
			case MTK_CAM_SENINF_RDY_SET_SW_STATUS:
				ret |= g_seninf_ops->_set_outmux_rdy_msk_sw_rdy_status(
					ctx, ctx->pad2cam[vc->out_pad][j], value);
				break;

			case MTK_CAM_SENINF_RDY_SET_CQ_ENABLE:
				ret |= g_seninf_ops->_set_outmux_rdy_msk_cq_rdy_en(
					ctx, ctx->pad2cam[vc->out_pad][j], value);
				break;
			default:
				dev_info(ctx->dev, "[%s][ERR] cmd(%d) is invalid\n", __func__, cmd);
				break;
			}
		}
		seninf_logd(ctx, "[%s]set_cmd(%d)camtg(%d)value(%d) done\n",
			__func__, cmd, camtg, value);
	}
	dev_info(ctx->dev, "[%s]set_cmd(%d) value(%d) done\n",__func__, cmd, value);
	return ret;
}

bool mtk_cam_seninf_set_mux_sw_rdy_immediate(
	struct v4l2_subdev *sd, u8 camtg, bool sw_rdy_status)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][Err]ctx is NULL\n", __func__);
		return -EINVAL;
	}

	/* check sensor latch time when pull sw rdy to high */
	if ((sw_rdy_status == true ) && (!ctx->is_test_model))
		ctx->sensor_sd->ops->core->command(
			ctx->sensor_sd, V4L2_CMD_G_CHECK_SENSOR_SEAMLESS_DONE_TS, NULL);

	return mtk_cam_seninf_set_mux_rdy_msk_cfg_by_cmd(
		ctx, camtg, MTK_CAM_SENINF_RDY_SET_SW_STATUS, sw_rdy_status);
}

static inline bool mtk_cam_seninf_set_mux_sw_rdy_deferred_to_tsrec(
	struct v4l2_subdev *sd, u8 camtg, bool sw_rdy_status)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][Err]ctx is NULL\n", __func__);
		return -EINVAL;
	}

	ctx->rdy_msk_defer_info.camtg = camtg;
	ctx->rdy_msk_defer_info.sw_rdy_status = sw_rdy_status;
	ctx->rdy_msk_defer_info.defer_to_tsrec_en = true;

	dev_info(ctx->dev, "[%s] sw_rdy_deferred_to_tsrec done with camtg %d sw_rdy %d\n",
		__func__, camtg, sw_rdy_status);
	return 0;
}

int mtk_cam_seninf_rdy_msk_defer_info_init(struct seninf_ctx *ctx)
{
	if (unlikely(ctx == NULL)) {
		pr_info("[%s] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&ctx->rdy_msk_defer_info, 0, sizeof(struct mtk_cam_seninf_rdy_msk_defer_info));
	return 0;
}

bool mtk_cam_seninf_set_mux_sw_rdy(struct v4l2_subdev *sd, u8 camtg, bool sw_rdy_status,
	bool is_defered_by_tsrec)
{
	return (is_defered_by_tsrec) ?
		mtk_cam_seninf_set_mux_sw_rdy_deferred_to_tsrec(sd, camtg, sw_rdy_status) :
		mtk_cam_seninf_set_mux_sw_rdy_immediate(sd, camtg, sw_rdy_status);
}

bool mtk_cam_seninf_set_mux_cq_en(struct v4l2_subdev *sd, u8 camtg, bool cq_rdy_en)
{
	bool ret = false;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][Err]ctx is NULL\n", __func__);
		return -EINVAL;
	}

	ret |= mtk_cam_seninf_set_mux_rdy_msk_cfg_by_cmd(
			ctx, camtg, MTK_CAM_SENINF_RDY_SET_CQ_ENABLE, cq_rdy_en);

	return ret;
}

int mtk_cam_seninf_get_tag_order(struct v4l2_subdev *sd,
		__u32 fmt_code, int input_pad_id)
{
	/* seninf todo: tag order */
	/* 0: first exposure 1: second exposure 2: last exposure */
	struct seninf_ctx *ctx;
	struct v4l2_subdev *sensor_sd;
	struct mtk_sensor_mode_config_info info;
	struct seninf_vc *vc;
	struct mtk_sensor_vc_info_by_scenario *vc_sid;
	u64 fsync_ext_vsync_pad_code = 0;
	int ret = EXPOSURE_LAST;  /* default return last exposure */
	int i = 0;
	int exposure_num = 0;
	int scenario = 0;
	int desc;
	int pad_id = 0;


	if (sd == NULL) {
		pr_info("[%s][ERROR] sd is NULL\n", __func__);
		return -EINVAL;
	}

	ctx = container_of(sd, struct seninf_ctx, subdev);

	if (ctx == NULL) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	/* if test pattern, do default return flow */
	if (ctx->is_test_model)
		return (input_pad_id == PAD_SRC_RAW0) ? EXPOSURE_FIRST: EXPOSURE_LAST;

	/*due to PD / W data will use the same VC with raw */
	switch (input_pad_id) {
	case PAD_SRC_RAW0:
	case PAD_SRC_RAW_W0:
	case PAD_SRC_PDAF0:
	case PAD_SRC_PDAF1:
	case PAD_SRC_PDAF2:
		pad_id = PAD_SRC_RAW0;
		break;

	case PAD_SRC_RAW1:
	case PAD_SRC_RAW_W1:
	case PAD_SRC_PDAF3:
	case PAD_SRC_PDAF4:
		pad_id = PAD_SRC_RAW1;
		break;

	case PAD_SRC_RAW2:
	case PAD_SRC_RAW_W2:
	case PAD_SRC_PDAF5:
	case PAD_SRC_PDAF6:
		pad_id = PAD_SRC_RAW2;
		break;

	default:
		pad_id = input_pad_id;
		break;
	}

	/*The following flow is for real sensor only */
	sensor_sd = ctx->sensor_sd;
	if (sensor_sd == NULL) {
		pr_info("[%s][ERROR] sensor_sd is NULL\n", __func__);
		return -EINVAL;
	}

	sensor_sd->ops->core->command(
		sensor_sd, V4L2_CMD_GET_SENSOR_MODE_CONFIG_INFO, &info);

	scenario = get_scenario_from_fmt_code(fmt_code);

	for (i = 0; i < info.count; i++) {
		if (info.seamless_scenario_infos[i].scenario_id == scenario) {
			exposure_num = info.seamless_scenario_infos[i].mode_exposure_num;
			break;
		}
	}


	vc_sid = kmalloc(sizeof(struct mtk_sensor_vc_info_by_scenario), GFP_KERNEL);
	if (unlikely(vc_sid == NULL)) {
		seninf_logi(ctx, "[Error][line %d] kzalloc fail\n", __LINE__);
		return -EINVAL;
	}


	/* get fs_seq info by pad */
	vc_sid->scenario_id = scenario;
	if (ctx->sensor_sd &&
	    ctx->sensor_sd->ops &&
	    ctx->sensor_sd->ops->core &&
	    ctx->sensor_sd->ops->core->command) {
		ctx->sensor_sd->ops->core->command(ctx->sensor_sd,
						   V4L2_CMD_G_SENSOR_VC_INFO_BY_SCENARIO,
						   vc_sid);
	} else {
		seninf_logi(ctx, "find sensor command failed\n");
	}

	vc = kmalloc(sizeof(struct seninf_vc), GFP_KERNEL);
	if (vc == NULL) {
		kfree(vc_sid);
		return -EINVAL;
	}

	for (i = 0; i < vc_sid->fd.num_entries; i++) {
		desc = vc_sid->fd.entry[i].bus.csi2.user_data_desc;
		vc->vc = vc_sid->fd.entry[i].bus.csi2.channel;
		vc->dt = vc_sid->fd.entry[i].bus.csi2.data_type;

		mtk_cam_seninf_fill_outpad_to_vc(
				ctx, vc, desc, &fsync_ext_vsync_pad_code);

		if (vc->out_pad != pad_id)
			continue;

		if (vc_sid->fd.entry[i].bus.csi2.fs_seq == MTK_FRAME_DESC_FS_SEQ_ONLY_ONE) {
			ret = EXPOSURE_FIRST;
			break;
		}

		if (vc_sid->fd.entry[i].bus.csi2.fs_seq == MTK_FRAME_DESC_FS_SEQ_LAST) {
			ret = EXPOSURE_LAST;
			break;
		}

		if ((pad_id == PAD_SRC_RAW1) && (exposure_num == 2)) {
			ret = EXPOSURE_LAST;
			break;
		}

		if (pad_id == PAD_SRC_RAW2) {
			ret = EXPOSURE_LAST;
			break;
		}

		ret = EXPOSURE_MIDDLE;
		break;
	}


	dev_info(ctx->dev,
			"[%s]input:pad_id(%d)conv_pad(%d),scen(%d),exp_num(%d)out_tag_order(%d)\n",
			__func__,
			input_pad_id,
			pad_id,
			scenario,
			exposure_num,
			ret);

	kfree(vc_sid);
	kfree(vc);
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
	bool is_sensor_stream = false;

	INIT_LIST_HEAD(&outmux_cfgs);

	// empty disable outmux list
	memset(ctx->outmux_force_disable_list, 0, sizeof(ctx->outmux_force_disable_list));

	for (i = 0; i < vcinfo->cnt; i++) {
		vc = &vcinfo->vc[i];

		if (ctx->is_aov_real_sensor) {
			if (!(core->aov_sensor_id < 0) &&
				!(ctx->current_sensor_id < 0) &&
				(ctx->current_sensor_id == core->aov_sensor_id)) {
				seninf_logd(ctx,
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

				if (cfg) {
					cfg->src_mipi = intf;
					cfg->src_sen = sen;
					cfg->pix_mode = dest->pix_mode;
					cfg->tag_cfg[dest->tag].enable = true;
					cfg->tag_cfg[dest->tag].filt_vc = vc_sel;
					cfg->tag_cfg[dest->tag].filt_dt = dt_sel;
					cfg->tag_cfg[dest->tag].exp_hsize = vc->exp_hsize;
					cfg->tag_cfg[dest->tag].exp_vsize = vc->exp_vsize;
					cfg->tag_cfg[dest->tag].bit_depth = vc->bit_depth;

					seninf_logi(ctx,
						    "vc[%d] dest[%u] pad %d intf %d sen %d outmux %d tag %d vc 0x%x dt 0x%x pix_mode %u bit_depth %d\n",
						    i, j, vc->out_pad, intf, sen, dest->outmux,
						    dest->tag, vc_sel, dt_sel, dest->pix_mode, vc->bit_depth);
				} else {
					seninf_logi(ctx, "get outmux%d cfg failed\n", dest->outmux);
				}
			} else {
				seninf_logi(ctx, "invalid outmux, vc[%d] pad %d intf %d outmux %d\n",
					 i, vc->out_pad, intf, dest->outmux);
			}
		}
	}

	if (!ctx->is_test_model) {
		/* query if sensor in reset */
		if (ctx->sensor_sd &&
		    ctx->sensor_sd->ops &&
		    ctx->sensor_sd->ops->core &&
		    ctx->sensor_sd->ops->core->command) {
			ctx->sensor_sd->ops->core->command(ctx->sensor_sd,
						V4L2_CMD_G_SENSOR_STREAM_STATUS, &is_sensor_stream);
		} else {
			seninf_logi(ctx, "find sensor command failed\n");
		}
		seninf_logd(ctx, "is sensor streamed: %u, config outmux cnt: %lu\n",
			    is_sensor_stream, seninf_list_count(&outmux_cfgs));
	}
	/* reset all selected outmux */
	mtk_cam_seninf_outmux_reset_all(ctx, &outmux_cfgs, 0);
	/* enable all selected outmux */
	mtk_cam_seninf_outmux_config_all(ctx, &outmux_cfgs, false);

	/* Free list */
	mtk_cam_seninf_outmux_release_all(ctx, &outmux_cfgs);

#ifdef SENSOR_SECURE_MTEE_SUPPORT
	if (ctx->is_secure != 1)
		seninf_logd(ctx,
			"is not secure, won't Sensor kernel init seninf_ca");
	else {
		if (!is_protected_kvm_enabled()) {
			if (!seninf_ca_open_session())
				dev_info(ctx->dev, "seninf_ca_open_session fail");

			dev_info(ctx->dev, "Sensor kernel ca_checkpipe");
			seninf_ca_checkpipe(ctx->SecInfo_addr);
		} else {
			if (!seninf_pkvm_open_session())
				dev_info(ctx->dev, "seninf_pkvm_open_session fail");

			dev_info(ctx->dev, "Sensor kernel pkvm_checkpipe");
#ifdef SECURE_UT
			seninf_pkvm_checkpipe(ctx->SecInfo_addr);
#else
			seninf_pkvm_checkpipe(get_chk_pa());
#endif
		}
	}
#endif

	return 0;
}

bool mtk_cam_seninf_force_disable_out_mux(struct v4l2_subdev *sd)
{
	u8 camtg;
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERR] ctx is NULL\n", __func__);
		return -EFAULT;
	}

	for (camtg = 0; camtg < SENINF_OUTMUX_NUM; camtg ++) {
		if (ctx->outmux_force_disable_list[camtg] == false)
			continue;

		g_seninf_ops->_disable_outmux(ctx, camtg, true);
		dev_info(ctx->dev, "[%s] force reset outmux %d done\n", __func__, camtg);
	}

	memset(ctx->outmux_force_disable_list, 0, sizeof(ctx->outmux_force_disable_list));

	return 0;
}


bool mtk_cam_seninf_check_vs_order_has_change(struct seninf_ctx *ctx, u8 outmux)
{
	u8 curr_first_vc = 0;
	u8 curr_last_vc = 0;

	if (g_seninf_ops->_get_outmux_curr_vs_order(ctx, outmux, &curr_first_vc, &curr_last_vc)) {
		pr_info("[%s][ERR] _get_outmux_curr_vs_order return failed\n", __func__);
		return true;
	}

	return (ctx->cur_first_vs != curr_first_vc || ctx->cur_last_vs != curr_last_vc) ?
		true : false;
}

bool mtk_cam_seninf_check_tag_vc_has_change(struct seninf_ctx *ctx, struct outmux_cfg *cfg, u8 tag_id)
{
	u8 curr_tag_vc = 0;
	bool curr_tag_vc_en = 0;

	if (g_seninf_ops->_get_outmux_curr_tag_vc_by_tag(ctx, cfg->outmux_idx, tag_id, &curr_tag_vc, &curr_tag_vc_en)) {
		pr_info("[%s][ERR] _get_outmux_curr_tag_vc_by_tag return failed\n", __func__);
		return true;
	}

	seninf_logd(ctx, "[%s] cfg(curr_immedate %d outmux %d, tad %d, vc 0x%x dt 0x%x) cur(vc 0x%x vc_en %d)\n",
		__func__,
		cfg->is_reset_immedate,
		cfg->outmux_idx,
		tag_id,
		cfg->tag_cfg[tag_id].filt_vc,
		cfg->tag_cfg[tag_id].filt_dt,
		curr_tag_vc,
		curr_tag_vc_en);

	/* if this tag has no used before, no need to reset */
	if (curr_tag_vc_en == false)
		return false;

	/* if ve_en is true,  then check vc has changed nor not */
	return (curr_tag_vc != cfg->tag_cfg[tag_id].filt_vc) ?
		true : false;
}

bool
mtk_cam_seninf_streaming_mux_change(struct mtk_cam_seninf_mux_param *param, bool grp_en)
{
	struct v4l2_subdev *sd = NULL;
	int pad_id = -1;
	int camtg = -1;
	int tag_id = -1;
	struct seninf_ctx *ctx = NULL;
	int i;
	char *buf = NULL;
	char *strptr = NULL;
	size_t buf_sz = 0;
	size_t remind = 0;
	int num = 0;
	struct list_head outmux_cfgs;
	struct outmux_cfg *cfg;
	struct seninf_vc *vc;
	struct seninf_vc *cur_vc = NULL;
	struct mtk_cam_seninf_set_camtg_cfg set_camtg_cfg;
	struct rdy_msk_grp_id_info *grp_id_ent;

	bool outmux_to_be_disable_list[SENINF_OUTMUX_NUM];
	bool outmux_continue_used_list[SENINF_OUTMUX_NUM];

	if (unlikely(param == NULL)) {
		pr_info("[%s][ERR] param is NULL\n", __func__);
		return -EFAULT;
	}

	if (param->num == 0) {
		pr_info("[%s][ERR] param->num is 0", __func__);
		return -EFAULT;
	}

	remind = buf_sz = (param->num) * 50;
	strptr = buf = kzalloc(buf_sz + 1, GFP_KERNEL);
	if (!buf)
		return false;

	INIT_LIST_HEAD(&outmux_cfgs);

	memset(outmux_to_be_disable_list, 0, sizeof(outmux_to_be_disable_list));
	memset(outmux_continue_used_list, 0, sizeof(outmux_continue_used_list));

	// disable all camtg changing first
	for (i = 0; i < param->num; i++) {
		sd = param->settings[i].seninf;
		pad_id = param->settings[i].source;
		camtg = param->settings[i].camtg;
		ctx = container_of(sd, struct seninf_ctx, subdev);

		if (camtg < 0 || camtg >= SENINF_OUTMUX_NUM) {
			pr_info("[%s][ERR] camtg id %d\n", __func__, camtg);
			return -EFAULT;
		}

		/* disable outer setting & clr dest cnt to 0 */
		_mtk_cam_seninf_reset_outmux_outer(ctx, pad_id);

		if (param->settings[i].enable == false)
			outmux_to_be_disable_list[camtg] = true;
		else
			outmux_continue_used_list[camtg] = true;

		// log
		num = snprintf(strptr, remind, "(pad %d -> outmux %d, enable %d), ",
			       param->settings[i].source,
			       param->settings[i].camtg,
			       param->settings[i].enable);
		if (num < 0) {
			dev_info(ctx->dev, "snprintf retuns error ret = %d\n", num);
			break;
		}

		remind -= num;
		strptr += num;
	}

	/* initial force disable list */
	memset(ctx->outmux_force_disable_list, 0, sizeof(ctx->outmux_force_disable_list));

	/* release old rdy msk grp_id */
	mtk_cam_seninf_release_rdy_msk_grp_id(ctx);

	/* get new rdy msk grp id*/
	if (param->rdy_mask_en.rdy_grp_en) {
		grp_id_ent = mtk_cam_seninf_get_rdy_msk_grp_id(ctx);

		if (grp_id_ent == NULL) {
			dev_info(ctx->dev, "[%s][ERR] grp_id_ent is NULL, skip continue flow\n", __func__);
			return -EFAULT;
		}
	}

	// set new camtg
	for (i = 0; i < param->num; i++) {
		sd = param->settings[i].seninf;
		pad_id = param->settings[i].source;
		camtg = param->settings[i].camtg;
		tag_id = param->settings[i].tag_id;
		ctx = container_of(sd, struct seninf_ctx, subdev);

		if (unlikely(ctx == NULL)) {
			pr_info("[%s][ERR] ctx is NULL\n", __func__);
			return -EFAULT;
		}

		if (unlikely(pad_id < PAD_SRC_RAW0 || pad_id >= PAD_MAXCNT)) {
			dev_info(ctx->dev, "[%s][ERROR] pad_id %d is invalid\n", __func__, pad_id);
			continue;
		}

		if (camtg < 0 || camtg >= g_seninf_ops->outmux_num) {
			dev_info(ctx->dev, "[%s] skip pad_id %d camtg %d\n", __func__, pad_id, camtg);
			continue;
		}

		/* for out mux which no longer be used --> add to force disable list */
		if (outmux_to_be_disable_list[camtg] == true &&
			outmux_continue_used_list[camtg] == false) {
			ctx->outmux_force_disable_list[camtg] = true;
			dev_info(ctx->dev, "[%s] add camtg %d to force_disable_list\n",
				__func__, camtg);
		}

		if (param->settings[i].enable == false) {
			seninf_logd(ctx, "[%s] due to enable= %d, no need to set camtg pad_id %d camtg %d\n",
				__func__,
				param->settings[i].enable,
				param->settings[i].source,
				param->settings[i].camtg);
			continue;
		}

		if (tag_id < 0 || tag_id >= 8) {
			dev_info(ctx->dev, "[%s] pad_id%d camtg%d, tag_id is %d, fallback to 0\n",
				 __func__, pad_id, camtg, tag_id);
			tag_id = 0;
		}

		vc = mtk_cam_seninf_get_vc_by_pad(ctx, pad_id);
		cur_vc = mtk_cam_seninf_get_curr_vc_by_pad(ctx, pad_id);

		if (!vc) {
			dev_info(ctx->dev,
				 "[%s] mtk_cam_seninf_get_vc_by_pad return failed by using pad %d\n",
				 __func__, pad_id);
			continue;
		}

		if (!cur_vc) {
			dev_info(ctx->dev,
				 "[%s] mtk_cam_seninf_get_curr_vc_by_pad return failed by using pad %d\n",
				 __func__, pad_id);
			continue;
		}

		seninf_logd(ctx, "[%s] camtg = %d\n", __func__, camtg);

		/* reset camtg_cfg for each camtg used */
		memset(&set_camtg_cfg, 0, sizeof(struct mtk_cam_seninf_set_camtg_cfg));

		set_camtg_cfg.pad_id = pad_id;
		set_camtg_cfg.camtg = camtg;
		set_camtg_cfg.tag_id = tag_id;

		set_camtg_cfg.rdy_msk_cfg.rdy_sw_en = param->rdy_mask_en.rdy_sw_en;
		set_camtg_cfg.rdy_msk_cfg.rdy_cq_en = param->rdy_mask_en.rdy_cq_en;

		if (param->rdy_mask_en.rdy_grp_en) {
			set_camtg_cfg.rdy_msk_cfg.rdy_grp_id = grp_id_ent->grp_id;
			set_camtg_cfg.rdy_msk_cfg.rdy_grp_en = param->rdy_mask_en.rdy_grp_en;
		}

		if (_mtk_cam_seninf_set_camtg(sd, &set_camtg_cfg)) {
			dev_info(ctx->dev, "[%s] _mtk_cam_seninf_set_camtg return failed\n", __func__);
			return -EFAULT;
		}

		// get outmux_cfg
		cfg = get_outmux_cfg_from_list(ctx, &outmux_cfgs, camtg);

		if (cfg) {
			cfg->src_mipi = ctx->seninfAsyncIdx;
			cfg->src_sen = ctx->seninfSelSensor;
			cfg->pix_mode = param->settings[i].pixelmode;
			cfg->tag_cfg[tag_id].enable = true;
			cfg->tag_cfg[tag_id].filt_vc = vc->vc;
			cfg->tag_cfg[tag_id].filt_dt = vc->dt;
			cfg->tag_cfg[tag_id].exp_hsize = cur_vc->exp_hsize;
			cfg->tag_cfg[tag_id].exp_vsize = cur_vc->exp_vsize;
			cfg->tag_cfg[tag_id].bit_depth = cur_vc->bit_depth;


			/* for outmux reset, it need to satify the following condition
			*  1. this outmux has been checked to froce reset already (in previous tag)
			*  2. VC at the same tag has changed
			*  3. vs order has change
			* then need to reset outmux immediate to avoid mux hang issue
			*/

			cfg->is_reset_immedate |=
					((outmux_continue_used_list[camtg] == true) &&
					(mtk_cam_seninf_check_vs_order_has_change(ctx, camtg) ||
					mtk_cam_seninf_check_tag_vc_has_change(ctx, cfg, tag_id)) ?
					true : false);


		} else {
			dev_info(ctx->dev, "[%s] get outmux cfg failed\n", __func__);
			mtk_cam_seninf_outmux_release_all(ctx, &outmux_cfgs);
			goto SENINF_MUX_CHANGE_LOG_AND_EXIT;
		}
	}

	/* (seamless only) reset all selected outmux firstly*/
	mtk_cam_seninf_outmux_reset_all(ctx, &outmux_cfgs, 1);

	/* enable all selected outmux */
	mtk_cam_seninf_outmux_config_all(ctx, &outmux_cfgs, true);

	/* Free list */
	mtk_cam_seninf_outmux_release_all(ctx, &outmux_cfgs);

SENINF_MUX_CHANGE_LOG_AND_EXIT:
	dev_info(ctx->dev,
		 "%s: param->num %d, %s %llu|%llu\n",
		 __func__, param->num,
		 buf,
		 ktime_get_boottime_ns(),
		 ktime_get_ns());

	kfree(buf);

	/* show mac chk status and clear it (is_clear = 1) */
	/* but avoid write clear when sentest seamless switch */
	if (!ctx->sentest_seamless_ut_en)
		g_seninf_ops->_show_mac_chk_status(ctx, 1);
	return false;
}

bool mtk_cam_seninf_mux_setup(struct v4l2_subdev *sd, struct mtk_cam_seninf_mux_param *param)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][Err]ctx is NULL\n", __func__);
		return -EINVAL;
	}

	return (ctx->streaming) ?
			mtk_cam_seninf_streaming_mux_change(param, true) :
			mtk_cam_seninf_set_camtg_for_stream_mux(param);
}

int mtk_cam_seninf_set_cfg_rdy(struct v4l2_subdev *sd, int camtg)
{
	/* need to be delete after rdy msk it*/

	return 0;
}

int mtk_cam_sensor_get_vc_info_by_scenario(struct seninf_ctx *ctx, u32 code)
{
	int i = 0;
	struct mtk_sensor_vc_info_by_scenario vc_sid= {0};
	struct seninf_vcinfo *vcinfo = &ctx->cur_vcinfo;
	struct seninf_vc *vc;
	int first_vc = -1;
	int last_vc = -1;
	int tmp_vc;
	int desc;
	bool only_one_vc = true;
	u64 fsync_ext_vsync_pad_code = 0;

	if (!ctx) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

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
		desc = vc_sid.fd.entry[i].bus.csi2.user_data_desc;
		vc->exp_hsize = vc_sid.fd.entry[i].bus.csi2.hsize;
		vc->exp_vsize = vc_sid.fd.entry[i].bus.csi2.vsize;
		vc->dt_remap_to_type = vc_sid.fd.entry[i].bus.csi2.dt_remap_to_type;

		if (i == 0)
			tmp_vc = vc->vc;
		else if (tmp_vc != vc->vc)
			only_one_vc = false;

		if (vc_sid.fd.entry[i].bus.csi2.fs_seq == MTK_FRAME_DESC_FS_SEQ_FIRST) {
			if (first_vc != -1 && first_vc != vc->vc) {
				/* Assert */
				dev_info(ctx->dev, "dup first_vc(%d) vc->vc(%d)\n",
					 first_vc, vc->vc);
				seninf_aee_print(SENINF_AEE_FS_SEQ,
					"Check sensor's frame desc fs_seq setting\n");
			}
			first_vc = vc->vc;
		} else if (vc_sid.fd.entry[i].bus.csi2.fs_seq == MTK_FRAME_DESC_FS_SEQ_LAST) {
			if (last_vc != -1 && last_vc != vc->vc) {
				/* Assert */
				dev_info(ctx->dev, "dup last_vc(%d) vc->vc(%d) is not valid\n",
					 last_vc, vc->vc);
				seninf_aee_print(SENINF_AEE_FS_SEQ,
					"Check sensor's frame desc fs_seq setting\n");
			}
			last_vc = vc->vc;
		}
		mtk_cam_seninf_fill_bit_depth_to_vc(vc, &vc_sid.fd.entry[i].bus.csi2);
		mtk_cam_seninf_fill_outpad_to_vc(ctx, vc, desc, &fsync_ext_vsync_pad_code);
	}
	vcinfo->cnt = vc_sid.fd.num_entries;

	if (only_one_vc && first_vc != -1)
		last_vc = first_vc;

	if (first_vc == -1 || last_vc == -1) {
		/* Assert */
		dev_info(ctx->dev, "[ERR]first_vc(%d) last_vc(%d) is not valid\n",
			 first_vc, last_vc);
		seninf_aee_print(SENINF_AEE_FS_SEQ,
			"Check sensor's frame desc fs_seq setting\n");
		return -EINVAL;
	}

	ctx->cur_first_vs = first_vc;
	ctx->cur_last_vs = last_vc;

	dev_info(ctx->dev, "current first vc(%d) current last vc(%d)\n",
		ctx->cur_first_vs, ctx->cur_last_vs);
	return 0;
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
 * @brief: switch aov mclk ulposc.
 *
 * switch mclk to ulposc or normal clock.
 * 1: switch to ulposc clock.
 * 0: switch to normal clock.
 *
 */
int aov_switch_mclk_ulposc(struct seninf_ctx *ctx,
	unsigned int enable)
{
	struct v4l2_subdev *sensor_sd = ctx->sensor_sd;
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_find(sensor_sd->ctrl_handler,
		V4L2_CID_MTK_AOV_SWITCH_MCLK_ULPOSC);
	if (!ctrl) {
		dev_info(ctx->dev,
			"no(%s) in subdev(%s)\n",
			__func__, sensor_sd->name);
		return -EINVAL;
	}
	dev_info(ctx->dev,
		"[%s] SWITCH MCLK to %s clock\n",
		__func__,
		enable ? "ulposc" : "normal");
	v4l2_ctrl_s_ctrl(ctrl, enable);

	return 0;
}

/**
 * For camsv stress test, set vc, dt and outmux cfg info
 *
 * @param ctx seninf ctx
 * @param outmux_cfg outmux cfg info
 */
static void mtk_cam_seninf_set_test_mode_info(struct seninf_ctx *ctx,
		struct outmux_cfg_for_camsv_stress *outmux_cfg)
{
	struct seninf_core *core = ctx->core;
	struct seninf_vcinfo *vcinfo = &core->vcinfo_stress_test;
	struct seninf_vc *vc;

	core->outmux_id = SENINF_OUTMUX3; // test camsvD (map to outmux3)
	core->async_id = SENINF_ASYNC_5; // use free seninf_async

	/* set vc info */
	vcinfo->cnt = 0;
	vc = &vcinfo->vc[vcinfo->cnt++];
	vc->vc = 0;
	vc->dt = 0x2b;
	vc->feature = VC_RAW_DATA;
	vc->out_pad = PAD_SRC_RAW0;
	vc->group = 0;
	vc->exp_hsize = 4096;
	vc->exp_vsize = 3072;
	vc->bit_depth = 16;

	outmux_cfg->outmux_idx = core->outmux_id;
	outmux_cfg->src_mipi = core->async_id;
	outmux_cfg->src_sen = 0; // test mode is not split mode
	outmux_cfg->pix_mode = pix_mode_8p; // use camsv only support 8p
	outmux_cfg->tag_id = 0; // fixed tag0 for stress test
	outmux_cfg->tag_cfg.enable = true;
	outmux_cfg->tag_cfg.filt_vc = vc->vc;
	outmux_cfg->tag_cfg.filt_dt = vc->dt;
	outmux_cfg->tag_cfg.exp_hsize = vc->exp_hsize;
	outmux_cfg->tag_cfg.exp_vsize = vc->exp_vsize;
	outmux_cfg->tag_cfg.bit_depth = vc->bit_depth;
}

/**
 * Notify seninf to start test model for stress test
 *
 * @param sd v4l2_subdev
 * @param mode test model mode
 */
void mtk_cam_seninf_start_test_model_for_camsv(struct v4l2_subdev *sd, u32 mode)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	struct seninf_core *core = ctx->core;
	struct outmux_cfg_for_camsv_stress outmux_cfg;
	int cfg_mode = MTK_CAM_OUTMUX_CFG_MODE_NORMAL_CFG;


	memset(&outmux_cfg, 0, sizeof(struct outmux_cfg_for_camsv_stress));
	mtk_cam_seninf_set_test_mode_info(ctx, &outmux_cfg);

	g_seninf_ops->_set_test_model_camsv_stress(ctx, core->async_id, mode);
	g_seninf_ops->_config_outmux_for_camsv_stress(
					ctx,
					outmux_cfg.outmux_idx,
					outmux_cfg.src_mipi,
					outmux_cfg.src_sen,
					cfg_mode,
					&outmux_cfg);
	ctx->core->camsv_test_mode_en = 1;
	dev_info(ctx->dev, "[%s] start stress test mode %d\n",
		__func__, mode);
}

/**
 * Notify seninf to stoptest model for stress test
 *
 * @param sd v4l2_subdev
 */
void mtk_cam_seninf_stop_test_model_for_camsv(struct v4l2_subdev *sd)
{
	struct seninf_ctx *ctx = container_of(sd, struct seninf_ctx, subdev);
	int outmux = ctx->core->outmux_id;

	g_seninf_ops->_disable_outmux(ctx, outmux, 1);
	g_seninf_ops->_set_test_model_camsv_stress(ctx, ctx->core->async_id, 0);
	dev_info(ctx->dev, "[%s] stop stress test\n",__func__);
	ctx->core->camsv_test_mode_en = 0;
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
	unsigned long flags;
	int aov_csi_port;

	for (aov_csi_port = 0; aov_csi_port < AOV_SENINF_NUM; aov_csi_port++) {
		if (sensor_id == g_aov_ctrl[aov_csi_port].aov_sensor_idx)
			break;
	}

	if (aov_csi_port >= AOV_SENINF_NUM) {
		pr_info("[%s] No match sensor_id(%d) in g_aov_ctrl\n", __func__, sensor_id);
		return -ENODEV;
	}

	if (g_aov_ctrl[aov_csi_port].aov_param.is_test_model) {
		real_sensor_id = 5;
	} else {
		if (sensor_id == g_aov_ctrl[aov_csi_port].aov_param.sensor_idx) {
			real_sensor_id = g_aov_ctrl[aov_csi_port].aov_param.sensor_idx;
			pr_info("[%s] input sensor id(%u)(success)\n",
				__func__, real_sensor_id);
		} else {
			real_sensor_id = sensor_id;
			pr_info("input sensor id(%u)(fail)\n", real_sensor_id);
			seninf_aee_print(SENINF_AEE_GENERAL,
				"[AEE] [%s] input sensor id(%u)(fail)",
				__func__, real_sensor_id);
			return -ENODEV;
		}
	}

	if (g_aov_ctrl[aov_csi_port].aov_ctx != NULL) {
		pr_info("[%s] aov_csi_port(%u)\n", __func__, aov_csi_port);
		ctx = g_aov_ctrl[aov_csi_port].aov_ctx;
	} else {
		pr_info("[%s] Can't find ctx from input sensor id!\n", __func__);
		return -ENODEV;
	}

	if (g_aov_ctrl[aov_csi_port].aov_ctx != NULL) {
		ctx = g_aov_ctrl[aov_csi_port].aov_ctx;
		core = ctx->core;
#ifdef SENSING_MODE_READY
		switch (aov_seninf_init_type) {
		case INIT_ABNORMAL_SCP_READY:
			dev_info(ctx->dev,
				"[%s] init type is abnormal(%u)!\n",
				__func__, aov_seninf_init_type);
			spin_lock_irqsave(&core->spinlock_aov, flags);
			core->aov_abnormal_init_flag = 1;
			spin_unlock_irqrestore(&core->spinlock_aov, flags);
			/* switch to ulposc clk*/
			aov_switch_mclk_ulposc(ctx, 1);
			/* seninf/sensor streaming on */
			seninf_s_stream(&ctx->subdev, 1);
			break;
		case INIT_NORMAL:
		default:
			dev_info(ctx->dev,
				"[%s] init type is normal(%u)!\n",
				__func__, aov_seninf_init_type);
			break;
		}
		if (!g_aov_ctrl[aov_csi_port].aov_param.is_test_model) {
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

	g_aov_ctrl[aov_csi_port].aov_param.vc = *vc;
	/* workaround */
	if (!g_aov_ctrl[aov_csi_port].aov_param.is_test_model) {
		g_aov_ctrl[aov_csi_port].aov_param.vc.dest_cnt = 1;
		g_aov_ctrl[aov_csi_port].aov_param.vc.dest[0].outmux = 9;
		g_aov_ctrl[aov_csi_port].aov_param.vc.dest[0].pix_mode = 3;
		g_aov_ctrl[aov_csi_port].aov_param.vc.dest[0].tag = 0;
		g_aov_ctrl[aov_csi_port].aov_param.vc.dest[0].cam_type = TYPE_UISP;
		g_aov_ctrl[aov_csi_port].aov_param.camtg = 9;
	}

	if (aov_seninf_param != NULL) {
		memcpy((void *)aov_seninf_param, (void *)&g_aov_ctrl[aov_csi_port].aov_param,
			sizeof(struct mtk_seninf_aov_param));
		// debug use
		pr_debug(
			"[%s] port(%d)/portA(%d)/portB(%d)/is_4d1c(%u)/seninfAsyncIdx(%d)/vcinfo_cnt(%d)/is_cphy(%u)/num_data_lanes(%d)/customized_pixel_rate(%lld)/mipi_pixel_rate(%lld)/isp_freq(%d)/legacy_phy(%d)\n",
			__func__,
			aov_seninf_param->port,
			aov_seninf_param->portA,
			aov_seninf_param->portB,
			aov_seninf_param->is_4d1c,
			aov_seninf_param->seninfAsyncIdx,
			aov_seninf_param->cnt,
			aov_seninf_param->is_cphy,
			aov_seninf_param->num_data_lanes,
			aov_seninf_param->customized_pixel_rate,
			aov_seninf_param->mipi_pixel_rate,
			aov_seninf_param->isp_freq,
			aov_seninf_param->legacy_phy);
		pr_debug(
			"[%s] seninf_dphy_settle_delay_dt(%d)/cphy_settle_delay_dt(%d)/dphy_settle_delay_dt(%d)/settle_delay_ck(%d)/hs_trail_parameter(%d)/cphy_settle(%u)/dphy_clk_settle(%u)/dphy_data_settle(%u)/dphy_trail(%d)/not_fixed_trail_settle(%d)/dphy_csi2_resync_dmy_cycle(%u)/not_fixed_dphy_settle(%u)\n",
			__func__,
			aov_seninf_param->seninf_dphy_settle_delay_dt,
			aov_seninf_param->cphy_settle_delay_dt,
			aov_seninf_param->dphy_settle_delay_dt,
			aov_seninf_param->settle_delay_ck,
			aov_seninf_param->hs_trail_parameter,
			aov_seninf_param->cphy_settle,
			aov_seninf_param->dphy_clk_settle,
			aov_seninf_param->dphy_data_settle,
			aov_seninf_param->dphy_trail,
			aov_seninf_param->not_fixed_trail_settle,
			aov_seninf_param->dphy_csi2_resync_dmy_cycle,
			aov_seninf_param->not_fixed_dphy_settle);
		pr_debug(
			"[%s] width(%lld)/height(%lld)/hblank(%lld)/vblank(%lld)/fps_n(%d)/fps_d(%d)/vc(%d)/dt(%d)/feature(%d)/out_pad(%d)/pixel_mode(%d)/tag(%d)/cam_type(%d)/enable(%d)/exp_hsize(%d)/exp_vsize(%d)/bit_depth(%d)/dt_remap_to_type(%d)\n",
			__func__,
			aov_seninf_param->width,
			aov_seninf_param->height,
			aov_seninf_param->hblank,
			aov_seninf_param->vblank,
			aov_seninf_param->fps_n,
			aov_seninf_param->fps_d,
			aov_seninf_param->vc.vc,
			aov_seninf_param->vc.dt,
			aov_seninf_param->vc.feature,
			aov_seninf_param->vc.out_pad,
			aov_seninf_param->vc.dest[0].pix_mode,
			aov_seninf_param->vc.dest[0].tag,
			aov_seninf_param->vc.dest[0].cam_type,
			aov_seninf_param->vc.enable,
			aov_seninf_param->vc.exp_hsize,
			aov_seninf_param->vc.exp_vsize,
			aov_seninf_param->vc.bit_depth,
			aov_seninf_param->vc.dt_remap_to_type);
	} else {
		pr_info("[%s] Must allocate buffer first!\n", __func__);
		return -ENOMEM;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_cam_seninf_s_aov_param);
#endif

