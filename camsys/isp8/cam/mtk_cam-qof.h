/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_CAM_QOF_H
#define __MTK_CAM_QOF_H

struct mtk_raw_device;
struct mtk_cam_ctx;

int qof_reset(struct mtk_raw_device *dev);

void qof_setup_ctrl(struct mtk_raw_device *dev, int on);
void qof_sof_src_sel(struct mtk_raw_device *dev,
	bool with_dcif, bool with_tg, u8 exp);
void qof_setup_hw_timer(struct mtk_raw_device *dev, u32 interval_us);
void qof_setup_rtc(struct mtk_raw_device *dev);
int qof_setup_twin(struct mtk_raw_device *dev, bool is_master);

bool qof_is_enabled(struct mtk_raw_device *dev);
int qof_enable(struct mtk_raw_device *dev, bool enable);

int qof_mtcmos_voter(struct mtk_cam_ctx *ctx, bool enable);
int qof_reset_mtcmos_voter(struct mtk_cam_ctx *ctx);

void qof_dump_trigger_cnt(struct mtk_raw_device *dev);
void qof_dump_voter(struct mtk_raw_device *dev);
void qof_dump_power_state(struct mtk_raw_device *raw);

#endif /*__MTK_CAM_QOF_H */
