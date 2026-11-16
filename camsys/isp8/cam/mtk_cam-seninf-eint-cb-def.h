/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _MTK_CAM_SENINF_EINT_CB_DEF_H
#define _MTK_CAM_SENINF_EINT_CB_DEF_H





/******************************************************************************
 * EINT cb user --- common define / enum
 *****************************************************************************/
enum mtk_cam_seninf_eint_cb_err_type {
	SENINF_EINT_CB_ERR_NONE,
	SENINF_EINT_CB_ERR_NOT_CONNECT_TO_EINT,
	SENINF_EINT_CB_ERR_IRQ_CB_UID_INVALID,
};
/*****************************************************************************/





/******************************************************************************
 * EINT IRQ cb --- define / enum / struct / function / etc.
 *****************************************************************************/
/* call back user id (user should modify) */
enum mtk_cam_seninf_eint_irq_cb_uid {
	SENINF_EINT_IRQ_CB_UID_MIN = 0,
	SENINF_EINT_IRQ_CB_UID_SENINF = SENINF_EINT_IRQ_CB_UID_MIN,
	SENINF_EINT_IRQ_CB_UID_CAMSYS,
	SENINF_EINT_IRQ_CB_UID_MAX
};

/* call back function gave user */
struct mtk_cam_seninf_eint_irq_notify_info {
	struct seninf_ctx *inf_ctx;

	unsigned int index; /* mapping to eint-no in dts*/

	unsigned long long sys_ts_ns; /* ktime_get_boottime_ns() */
	unsigned long long mono_ts_ns; /* ktime_get_ns() */
};

/* call back function prototype */
typedef void (*cb_func_notify_irq_info)(
	const struct mtk_cam_seninf_eint_irq_notify_info *p_info,
	void *p_data);

/* call back info user need to fill in when register cb */
struct mtk_cam_seninf_eint_irq_cb_info {
	cb_func_notify_irq_info func_ptr;
	void *p_data;
};


/* register IRQ cb */
int mtk_cam_seninf_eint_register_irq_cb(struct v4l2_subdev *sd,
	const enum mtk_cam_seninf_eint_irq_cb_uid id,
	const struct mtk_cam_seninf_eint_irq_cb_info *p_info);

/* unregister IRQ cb */
int mtk_cam_seninf_eint_unregister_irq_cb(struct v4l2_subdev *sd,
	const enum mtk_cam_seninf_eint_irq_cb_uid id);
/*****************************************************************************/


#endif
