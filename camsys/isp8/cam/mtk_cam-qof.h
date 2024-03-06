/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_CAM_QOF_H
#define __MTK_CAM_QOF_H

struct mtk_raw_device;
struct mtk_cam_ctx;

#define qof_dump_ctx(ctx, func) \
	do { \
		int i; \
		for (i = 0; i < ARRAY_SIZE(ctx->hw_raw); i++) { \
			if (ctx->hw_raw[i]) { \
				struct mtk_raw_device *raw_dev = \
					dev_get_drvdata(ctx->hw_raw[i]); \
				func(raw_dev); \
			} \
		} \
	} while (0)

int qof_reset(struct mtk_raw_device *dev);

void qof_setup_ctrl(struct mtk_raw_device *dev, int on);
void qof_sof_src_sel(struct mtk_raw_device *dev,
	bool with_dcif, bool with_tg, int sv_last_tag);
void qof_init_timer_freq(struct mtk_raw_device *dev);
void qof_setup_hw_timer(struct mtk_raw_device *dev, u32 interval_us);
void qof_setup_rtc(struct mtk_raw_device *dev);
int qof_setup_twin(struct mtk_raw_device *dev, bool is_master);
void qof_set_cq_start_max(struct mtk_raw_device *dev, u32 start_max);

bool qof_is_enabled(struct mtk_raw_device *dev);
int qof_enable(struct mtk_raw_device *dev, bool enable);
int qof_enable_cq_trigger_by_qof(struct mtk_raw_device *dev, bool enable);

int qof_mtcmos_voter(struct mtk_cam_ctx *ctx, bool enable);
int qof_mtcmos_raw_voter(struct mtk_raw_device *raw, bool enable);
int qof_reset_mtcmos_voter(struct mtk_cam_ctx *ctx);

void qof_dump_trigger_cnt(struct mtk_raw_device *dev);
void qof_dump_voter(struct mtk_raw_device *dev);
void qof_dump_power_state(struct mtk_raw_device *raw);
void qof_dump_hw_timer(struct mtk_raw_device *raw);
void qof_dump_cq_addr(struct mtk_raw_device *raw);
void qof_dump_ctrl(struct mtk_raw_device *raw);
void qof_dump_qoftop_status(struct mtk_raw_device *raw);

void mtk_cam_enable_itc(struct mtk_raw_device *raw);
#endif /*__MTK_CAM_QOF_H */
