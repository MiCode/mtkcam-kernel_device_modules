// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include "mtk_cam.h"

void mtk_cam_tuning_probe(void)
{
}

void mtk_cam_tuning_init(struct mtk_cam_tuning *param)
{
}

void mtk_cam_tuning_uninit(void)
{
}

void mtk_cam_tuning_update(struct mtk_cam_tuning *param)
{
#ifdef SAMPLE_CODE
	/* after algo finished */
	u64 current_ts_ns = ktime_get_boottime_ns();

	if (current_ts_ns - param->begin_ts_ns < CAM_TUNING_ALGO_DEADLINE_NS) {
		/* update shading table */
		memcpy(param->shading_tbl, /*algo_buf*/, MTK_CAM_LSCI_TABLE_SIZE);
	} else {
		/* print warning log */
		pr_info("bypass shading table update");
	}
#endif
}
