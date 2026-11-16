/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __MTK_CAM_SV_DF_H
#define __MTK_CAM_SV_DF_H

#include <linux/mutex.h>
#include <linux/string.h>

#define MAX_SV_HW_NUM 6
#define MAX_DMA_CORE 3
#define MAX_SV_DF_HW_NUM 3
#define DF_UNIT 2
#define DF_CORE_IDX (MAX_DMA_CORE - 1)

enum SV_DF_REQ {
	SV_DF_REQ_NONE = 0,
	SV_DF_REQ_ENQ,
	SV_DF_REQ_DEQ,
};

enum SV_DF_STATE {
	SV_DF_ST_NONE = 0,
	SV_DF_ST_IDLE,
	SV_DF_ST_PENDING,
	SV_DF_ST_EXE,
};

struct sv_df_port_info {
	unsigned int non_cfg_fifo_num;
	unsigned int cfg_fifo_num;
};

struct sv_df_port_action {
	enum SV_DF_REQ action;
	unsigned int req_num;
};

struct sv_df_action {
	struct sv_df_port_action actions[MAX_DMA_CORE];
};

struct sv_df_dev_info {
	unsigned long long applied_bw;
	enum SV_DF_STATE state;
	unsigned int non_cfg_fifo_num;
	unsigned int cfg_fifo_num;
	struct sv_df_port_info port_info[MAX_DMA_CORE];
	struct sv_df_action pending_action;
};

struct mtk_cam_sv_df_mgr {
	unsigned int avai_fifo_num;
	struct sv_df_dev_info dev_info[MAX_SV_HW_NUM];
	struct mutex op_lock;
};

int mtk_cam_sv_df_mgr_init(struct mtk_cam_sv_df_mgr *sv_df_mgr);
unsigned int mtk_cam_sv_df_get_non_cfg_fifo_num(unsigned int sv_idx,
		unsigned int core_idx);
unsigned int mtk_cam_sv_df_get_cfg_fifo_num(unsigned int sv_idx,
		unsigned int core_idx);
unsigned int mtk_cam_sv_df_get_runtime_fifo_size(
		struct mtk_cam_sv_df_mgr *sv_df_mgr,
		unsigned int sv_idx, unsigned int core_idx);
int mtk_cam_sv_df_update_bw(struct mtk_cam_sv_df_mgr *sv_df_mgr,
		unsigned int sv_idx, unsigned long long bw);
int mtk_cam_sv_df_action_ack(struct mtk_cam_sv_df_mgr *sv_df_mgr,
		unsigned int sv_idx);
bool mtk_cam_sv_df_next_action(struct mtk_cam_sv_df_mgr *sv_df_mgr,
		unsigned int sv_idx, struct sv_df_action *sv_df_action);
int mtk_cam_sv_df_reset(struct mtk_cam_sv_df_mgr *sv_df_mgr,
		unsigned int sv_idx);
bool mtk_cam_sv_check_df_action_done(
		struct sv_df_action *sv_df_action);
void mtk_cam_sv_df_debug_dump(struct mtk_cam_sv_df_mgr *sv_df_mgr,
		char *dbg_msg);

#endif
