// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam.h"
#include "mtk_cam-job-stagger.h"
#include "mtk_cam-job_utils.h"
#include "mtk_cam-raw_ctrl.h"

int fill_imgo_buf_to_ipi_stagger(
	struct req_buffer_helper *helper, struct mtk_cam_buffer *buf,
	struct mtk_cam_video_device *node)
{
	int ret = 0;
	struct mtk_cam_job *job = helper->job;

	if (is_processed_raw_node(job, node)) {  /* main stream */
		struct mtkcam_ipi_frame_param *fp = helper->fp;
		struct mtkcam_ipi_img_output *out;

		out = &fp->img_outs[helper->io_idx++];

		ret = fill_img_out(helper, out, buf, node);
	} else {  /* pure raw */
		ret = fill_imgo_buf_as_working_buf(helper, buf, node);
	}

	return ret;
}
