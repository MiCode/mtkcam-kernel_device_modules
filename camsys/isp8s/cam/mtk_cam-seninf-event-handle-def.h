/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_CAM_SENINF_EVENT_HANDLE_DEF_H__
#define __MTK_CAM_SENINF_EVENT_HANDLE_DEF_H__

#include "mtk_cam-seninf.h"
#include "mtk_cam-seninf-tsrec.h"


/*----------------------------------------------------------------------------*/
/* => common / utilities */
/*----------------------------------------------------------------------------*/
/**
 * return:
 *      1 => match target event (should call your handler func);
 *      0 => NOT match
 */
static inline int chk_user_event(const int events, const unsigned int user)
{
	return ((events >> user) & 0x1);
}

static inline void set_user_event(int *p_event, const unsigned int user)
{
	*p_event |= (1UL << user);
}
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/* => for tsrec event/handle */
/*----------------------------------------------------------------------------*/
enum mtk_cam_seninf_tsrec_irq_event_user_tags {
	TSREC_IRQ_EVENT_USER_NONE = 0,
	TSREC_IRQ_EVENT_USER_SENTEST,
	TSREC_IRQ_EVENT_USER_MIPI_ERR_DETECT,
	TSREC_IRQ_EVENT_USER_SENINF_RDY_MSK_DEFER,
};

static inline int mtk_cam_seninf_tsrec_irq_notify_chk_users(struct seninf_ctx *ctx)
{
	int ret = 0;

	/* case handling */
	if (unlikely(ctx == NULL)) {
		pr_info("[%s] Noti: seninf_ctx is NULL, ret:0\n", __func__);
		return 0;
	}

	if (unlikely(ctx->sentest_seamless_ut_en ||
				ctx->sentest_active_frame_en ||
				ctx->sentest_tsrec_update_sof_cnt_en))
		set_user_event(&ret, TSREC_IRQ_EVENT_USER_SENTEST);

	if (unlikely(ctx->core->vsync_irq_en_flag || ctx->core->csi_irq_en_flag))
		set_user_event(&ret, TSREC_IRQ_EVENT_USER_MIPI_ERR_DETECT);

	if (unlikely(ctx->rdy_msk_defer_info.defer_to_tsrec_en))
		set_user_event(&ret, TSREC_IRQ_EVENT_USER_SENINF_RDY_MSK_DEFER);

	return ret;
}

/**
 * call this function ONLY after checking the event users status
 * by 'mtk_cam_seninf_tsrec_irq_notify_chk_users' function.
 * (ONLY when there are users waiting to be notified --- @event_users).
 */
void mtk_cam_seninf_tsrec_irq_notify(const int event_users,
	const struct mtk_cam_seninf_tsrec_irq_notify_info *p_info);
/*----------------------------------------------------------------------------*/


#endif /* __MTK_CAM_SENINF_EVENT_HANDLE_DEF_H__ */
