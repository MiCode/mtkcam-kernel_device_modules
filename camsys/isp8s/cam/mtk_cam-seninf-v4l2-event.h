/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 MediaTek Inc. */

#ifndef __MTK_CAM_SENINF_V4L2_EVENT_H__
#define __MTK_CAM_SENINF_V4L2_EVENT_H__

#include <linux/videodev2.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>

/******************************************************************************/
// seninf event --- function
/******************************************************************************/

int mtk_cam_seninf_subscribe_event(
				struct v4l2_subdev *sd,
				struct v4l2_fh *fh,
				struct v4l2_event_subscription *sub);

int mtk_cam_seninf_eint_event_handle_cb_register(struct v4l2_subdev *sd, int enable);

#endif
