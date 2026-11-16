// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam-seninf-v4l2-event.h"
#include "mtk_cam-seninf_control-8s.h"
#include "mtk_cam-seninf-eint-cb-def.h"
#include "mtk_camera-videodev2.h"

#include "mtk_cam-seninf.h"


#include <media/v4l2-subdev.h>
#include <linux/videodev2.h>

#define sd_to_ctx(__sd) container_of(__sd, struct seninf_ctx, subdev)

static int mtk_cam_seninf_send_event(struct seninf_ctx *ctx,
				struct v4l2_event *event)
{
	if (unlikely(ctx == 0)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return -EFAULT;
	}

	if (unlikely(event  == 0)) {
		pr_info("[Error][%s] event is NULL", __func__);
		return -EFAULT;
	}

	v4l2_event_queue(ctx->subdev.devnode, event);

	return 0;
}

int mtk_cam_seninf_subscribe_event(struct v4l2_subdev *sd,
				  struct v4l2_fh *fh,
				  struct v4l2_event_subscription *sub)
{
#define EVENT_DEPTH 4

	switch (sub->type) {
	case V4L2_EVENT_FRAME_SYNC:
		return v4l2_event_subscribe(fh, sub, EVENT_DEPTH, NULL);

	default:
		return -EINVAL;
	}
}

void mtk_cam_seninf_v4l2_event_sof_notify(
	const struct mtk_cam_seninf_eint_irq_notify_info *p_info,
	void *p_data)
{
	struct seninf_ctx *ctx;
	struct mtk_cam_event_frame_sync_data data;
	struct v4l2_event event;


	if (unlikely(p_info == NULL)) {
		pr_info("[Error][%s] p_info is NULL", __func__);
		return;
	}
	memset(&data, 0, sizeof(data));
	memset(&event, 0, sizeof(event));

	ctx = p_info->inf_ctx;
	if (unlikely(ctx == NULL)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return;
	}

	mutex_lock(&ctx->mutex_vsync_in);
	ctx->vsync_in_frame_seq_no++;
	data.sensor_sequence = ctx->vsync_in_event_info.sensor_sequence;
	data.sensor_sync_id = ctx->vsync_in_event_info.sensor_sync_id;
	data.fs_anchor_ns = ctx->vsync_in_event_info.fs_anchor_ns;
	data.ts_ns = p_info->sys_ts_ns;
	mutex_unlock(&ctx->mutex_vsync_in);

	memcpy(event.u.data, &data, 32);
	event.type = V4L2_EVENT_FRAME_SYNC;
	event.u.frame_sync.frame_sequence = ctx->vsync_in_frame_seq_no;

	if (mtk_cam_seninf_send_event(ctx, &event)) {
		pr_info("[Error][%s] mtk_cam_seninf_send_event return failed", __func__);
		return;
	}
}

int mtk_cam_seninf_eint_event_handle_cb_register(struct v4l2_subdev *sd, int enable)
{
	struct seninf_ctx *ctx = sd_to_ctx(sd);
	struct mtk_cam_seninf_eint_irq_cb_info p_info = {
		.func_ptr = mtk_cam_seninf_v4l2_event_sof_notify,
		.p_data = NULL,
	};

	if (unlikely(sd == 0)) {
		pr_info("[Error][%s] sd is NULL", __func__);
		return -EFAULT;
	}

	if (unlikely(ctx == 0)) {
		pr_info("[Error][%s] ctx is NULL", __func__);
		return -EFAULT;
	}

	ctx->vsync_in_frame_seq_no = 0;

	if (enable)
		mtk_cam_seninf_eint_register_irq_cb(sd, SENINF_EINT_IRQ_CB_UID_SENINF, &p_info);
	 else
		mtk_cam_seninf_eint_unregister_irq_cb(sd, SENINF_EINT_IRQ_CB_UID_SENINF);

	return 0;
}
