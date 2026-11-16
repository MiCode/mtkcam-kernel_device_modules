/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#ifndef _MTK_DFP_V4L2_VNODE_H_
#define _MTK_DFP_V4L2_VNODE_H_

/*#include <linux/platform_device.h>
 * #include <linux/module.h>
 * #include <linux/of_device.h>
 * #include <linux/pm_runtime.h>
 * #include <linux/remoteproc.h>
 * #include <linux/remoteproc/mtk_scp.h>
 */
#include <linux/videodev2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-event.h>
#include "./../mtk_imgsys-dev.h"
#include "./../mtk_imgsys-hw.h"

//#include "mtk_imgsys_v4l2.h"
#include "mtk_header_desc.h"

#define defaultdesc 0


static const struct mtk_imgsys_dev_format in_fmts[] = {
	{
		.format = V4L2_META_FMT_MTISP_DESC,
		.num_planes = 1,
#if defaultdesc
		.depth	 = { 8 },
		.row_depth = { 8 },
		.num_cplanes = 1,
#endif
		.buffer_size = sizeof(struct header_desc),
	},
	{
		.format = V4L2_META_FMT_MTISP_DESC_NORM,
		.num_planes = 1,
		.buffer_size = sizeof(struct header_desc_norm),
	},
};

static const struct mtk_imgsys_dev_format out_fmts[] = {
	{
		.format = V4L2_META_FMT_MTISP_DESC,
		.num_planes = 1,
#if defaultdesc
		.depth = { 8 },
		.row_depth = { 8 },
		.num_cplanes = 1,
#endif
		.buffer_size = sizeof(struct header_desc),
	},
	{
		.format = V4L2_META_FMT_MTISP_DESC_NORM,
		.num_planes = 1,
		.buffer_size = sizeof(struct header_desc_norm),
	},

};

static const struct v4l2_frmsizeenum in_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_DIP_CAPTURE_MAX_WIDTH,
	.stepwise.min_width = MTK_DIP_CAPTURE_MIN_WIDTH,
	.stepwise.max_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
	.stepwise.min_height = MTK_DIP_CAPTURE_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct v4l2_frmsizeenum out_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_DIP_OUTPUT_MAX_WIDTH,
	.stepwise.min_width = MTK_DIP_OUTPUT_MIN_WIDTH,
	.stepwise.max_height = MTK_DIP_OUTPUT_MAX_HEIGHT,
	.stepwise.min_height = MTK_DIP_OUTPUT_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct mtk_imgsys_video_device_desc DFP_setting[] = {
	/*FE Input and Output*/
};

#endif // _MTK_DFP_V4L2_VNODE_H_
