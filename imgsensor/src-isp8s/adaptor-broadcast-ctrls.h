/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2022 MediaTek Inc. */

#ifndef __ADAPTOR_BROADCAST_CTRLS_H__
#define __ADAPTOR_BROADCAST_CTRLS_H__

enum adaptor_broadcast_event_id {
	ADAPT_BC_EVENT_NONE = 0,

	/* for fsync ctrls */
	ADAPT_BC_EVENT_FSYNC_RE_CALC_FL,
};

int adaptor_push_broadcast_event(struct adaptor_ctx *ctx,
	enum adaptor_broadcast_event_id flag,
	bool need_broadcast_to_itself);

int adaptor_get_broadcast_event(struct adaptor_ctx *ctx,
	struct mtk_cam_broadcast_info *p_info);

#endif
