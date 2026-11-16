/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_CAM_DVC_H
#define __MTK_CAM_DVC_H

#include <linux/platform_device.h>

struct mtk_camsys_dvc {
	struct device *dev;
	void __iomem *top_base;
	void __iomem *dvc_a_base;
	void __iomem *dvc_b_base;
	void __iomem *dvc_c_base;
	int irq;

	u32 hwmode[3];
	struct mutex op_lock;
};

extern struct platform_driver mtk_cam_dvc_driver;

u32 is_dvc_support(void);
void mtk_cam_dvc_top_enable(struct mtk_camsys_dvc *dvc);
void mtk_cam_dvc_top_disable(struct mtk_camsys_dvc *dvc);
void mtk_cam_dvc_init(struct mtk_camsys_dvc *dvc,
			u32 raw_id, u32 dvc_hwmode, u32 is_srt, u32 opp, u32 frm_time_ms);
void mtk_cam_dvc_unint(struct mtk_camsys_dvc *dvc, u32 raw_id);
int mtk_cam_dvc_vote(struct mtk_camsys_dvc *dvc, u32 raw_id, u32 index, bool boost);
int mtk_cam_dvc_dbg_dump(struct mtk_camsys_dvc *dvc, u32 raw_id);
int mtk_cam_dvc_probe(struct platform_device *pdev, struct mtk_camsys_dvc *dvc);


#endif /*__MTK_CAM_DVC_H*/
