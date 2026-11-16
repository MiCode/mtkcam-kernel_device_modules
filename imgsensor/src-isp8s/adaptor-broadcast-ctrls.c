// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.


#include "mtk_camera-v4l2-controls.h"
#include "adaptor.h"
#include "adaptor-trace.h"
#include "adaptor-fsync-ctrls.h"
#include "adaptor-common-ctrl.h"
#include "adaptor-tsrec-cb-ctrl-impl.h"
#include "adaptor-sentest-ctrl.h"

#include "adaptor-command.h"
#include "adaptor-broadcast-ctrls.h"


static inline void dump_broadcast_info(struct adaptor_ctx *ctx,
	const struct mtk_cam_broadcast_info *p_info,
	const char *caller)
{
	adaptor_logd(ctx,
		"[%s] bc_info(type:%u(%u), s_idx:%u/inf:%u, req_id:%u, ts(sof:%llu, worker:(%llu/%llu/%llu))), curr_ts:%llu",
		caller,
		p_info->type,
		p_info->need_broadcast_to_itself,
		p_info->sensor_idx,
		p_info->seninf_idx,
		p_info->req_id,
		p_info->sof_timestamp,
		p_info->queue_work_ts_ns,
		p_info->wakeup_work_ts_ns,
		p_info->done_work_ts_ns,
		ktime_get_boottime_ns());
}

static inline int chk_current_status(struct adaptor_ctx *ctx,
	struct mtk_cam_broadcast_info *p_info,
	const char *caller)
{
	int ret = 0;

	if (unlikely(!ctx->is_streaming)) {
		ret = 1;
		adaptor_loge(ctx,
			"[%s] while streamoff, ctx->is_streaming:%u, ret:%d\n",
			caller, ctx->is_streaming, ret);
	}
	if (unlikely(ctx->subctx.sof_no == 0)) {
		ret = 2;
		adaptor_loge(ctx,
			"[%s] before 1SOF, ctx->is_streaming:%u ctx->subctx.sof_no:%u, ret:%d\n",
			caller, ctx->is_streaming, ctx->subctx.sof_no, ret);
	}
	if (unlikely(ctx->is_sensor_reset_stream_off)) {
		ret = 3;
		adaptor_loge(ctx,
			"[%s] ctx->is_sensor_reset_stream_off:%u, ret:%d skip\n",
			caller, ctx->is_sensor_reset_stream_off, ret);
	}
	if (unlikely(ctx->subctx.fast_mode_on)) {
		ret = 4;
		adaptor_loge(ctx,
			"[%s] while seamless, ctx->subctx.fast_mode_on:%u, ret:%d skip\n",
			caller, ctx->subctx.fast_mode_on, ret);
	}

	if (unlikely(ret != 0))
		dump_broadcast_info(ctx, p_info, __func__);

	return 0;
}

static inline int do_cb_ctrl_execute(struct adaptor_ctx *ctx,
	struct mtk_cam_broadcast_info *broadcast_info)
{
	return (adaptor_tsrec_cb_ctrl_execute(ctx,
		TSREC_CB_CMD_SETUP_BROADCAST_EVENT, broadcast_info, __func__));
}

static inline void init_broadcast_event_info(struct adaptor_ctx *ctx,
	struct mtk_cam_broadcast_info *broadcast_info,
	enum adaptor_broadcast_event_id flag,
	bool need_broadcast_to_itself)
{
	broadcast_info->type = flag;
	broadcast_info->need_broadcast_to_itself = need_broadcast_to_itself;
	broadcast_info->sensor_idx = ctx->idx;
	broadcast_info->seninf_idx = ctx->seninf_idx;
	broadcast_info->req_id = ctx->req_id;

	/* update while receive V4L2_CID_UPDATE_SOF_CNT */
	broadcast_info->sof_timestamp = ctx->sys_ts_update_sof_cnt;
}

int adaptor_push_broadcast_event(struct adaptor_ctx *ctx,
	enum adaptor_broadcast_event_id flag,
	bool need_broadcast_to_itself)
{
	int ret;
	struct mtk_cam_broadcast_info broadcast_info = {};

	init_broadcast_event_info(ctx, &broadcast_info, flag, need_broadcast_to_itself);
	ret = do_cb_ctrl_execute(ctx, &broadcast_info);

	return ret;
}

int adaptor_get_broadcast_event(struct adaptor_ctx *ctx,
	struct mtk_cam_broadcast_info *p_info)
{
	int ret = 0;

	mutex_lock(&ctx->broadcast_lock);
	ret = chk_current_status(ctx, p_info, __func__);

	if (unlikely(ret))
		goto end_adaptor_get_broadcast_event;

	/* !!! put broadcast event receive handler here !!! */
	notify_fsync_mgr_get_broadcast_event(ctx, p_info);

	dump_broadcast_info(ctx, p_info, __func__);

end_adaptor_get_broadcast_event:
	mutex_unlock(&ctx->broadcast_lock);
	return ret;
}
