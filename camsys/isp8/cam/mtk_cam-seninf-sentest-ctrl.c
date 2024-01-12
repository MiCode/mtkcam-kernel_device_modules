// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam-seninf-sentest-ctrl.h"
#include "mtk_cam-seninf_control-8.h"
#include "mtk_cam-seninf-hw.h"

/******************************************************************************/
// seninf sentest call back ctrl --- function
/******************************************************************************/

int seninf_sentest_get_debug_reg_result(struct seninf_ctx *ctx, void *arg)
{
	int i;
	struct seninf_core *core;
	struct seninf_ctx *ctx_;
	struct mtk_seninf_debug_result *result = arg;
	struct mtk_cam_seninf_vcinfo_debug *vcinfo_debug;
	struct outmux_debug_result *outmux_result;
	struct mtk_cam_seninf_debug debug_result;
	static __u16 last_pkCnt;

	if (unlikely(ctx == NULL)) {
		pr_info("[%s][ERROR] ctx is NULL\n", __func__);
		return -EINVAL;
	}

	core = ctx->core;
	if (unlikely(core == NULL)) {
		pr_info("[%s][ERROR] core is NULL\n", __func__);
		return -EINVAL;
	}

	if (unlikely(result == NULL)) {
		pr_info("[%s][ERROR] result is NULL\n", __func__);
		return -EINVAL;
	}

	memset(&debug_result, 0, sizeof(struct mtk_cam_seninf_debug));

	list_for_each_entry(ctx_, &core->list, list) {
		if (unlikely(ctx_ == NULL)) {
			pr_info("[%s][ERROR] ctx_ is NULL\n", __func__);
			return -EINVAL;
		}

		if (!ctx_->streaming)
			continue;

		g_seninf_ops->get_seninf_debug_core_dump(ctx_, &debug_result);

		result->is_cphy = ctx_->is_cphy;
		result->csi_port = ctx_->port;
		result->seninfAsyncIdx = ctx_->seninfAsyncIdx;
		result->data_lanes = ctx_->num_data_lanes;
		result->valid_result_cnt = debug_result.valid_result_cnt;
		result->seninf_async_irq = debug_result.seninf_async_irq;
		result->csi_mac_irq_status = debug_result.csi_mac_irq_status;

		if (last_pkCnt == debug_result.packet_cnt_status) {

			/* Need fix this logic */
			last_pkCnt = debug_result.packet_cnt_status;
			result->packet_status_err = 0;
		} else {
			result->packet_status_err = 1;
		}


		for (i = 0; i <= debug_result.valid_result_cnt; i++) {
			vcinfo_debug = &debug_result.vcinfo_debug[i];
			outmux_result = &result->outmux_result[i];

			if (unlikely(vcinfo_debug == NULL)) {
				pr_info("[%s][ERROR] vcinfo_debug is NULL\n", __func__);
				return -EINVAL;
			}

			if (unlikely(outmux_result == NULL)) {
				pr_info("[%s][ERROR] outmux_result is NULL\n", __func__);
				return -EINVAL;
			}

			outmux_result->vc_feature   = vcinfo_debug->vc_feature;
			outmux_result->tag_id	   = vcinfo_debug->tag_id;
			outmux_result->vc		   = vcinfo_debug->vc;
			outmux_result->dt		   = vcinfo_debug->dt;
			outmux_result->exp_size_h   = vcinfo_debug->exp_size_h;
			outmux_result->exp_size_v   = vcinfo_debug->exp_size_v;
			outmux_result->outmux_id	= vcinfo_debug->outmux_id;

			outmux_result->done_irq_status		  = vcinfo_debug->done_irq_status;
			outmux_result->oversize_irq_status	  = vcinfo_debug->oversize_irq_status;
			outmux_result->incomplete_frame_status  = vcinfo_debug->incomplete_frame_status;
			outmux_result->ref_vsync_irq_status	 = vcinfo_debug->ref_vsync_irq_status;
		}
	}
	return 0;
}
