// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/module.h>

#include "mtk_cam-sv-df.h"

#define MAX_SV_DF_DUMP_LEN 50

#define SV_DF_SNPRINTF(buf, len, fmt, ...) { \
	len += snprintf(buf + len, MAX_SV_DF_DUMP_LEN - len, fmt, ##__VA_ARGS__); \
}

static int enable_sv_df_log;
module_param(enable_sv_df_log, int, 0644);

static const unsigned int non_cfg_fifo_num[MAX_SV_HW_NUM][MAX_DMA_CORE] = {
	{3, 3, 1},
	{3, 3, 1},
	{3, 1, 1},
	{2, 0, 0},
	{1, 0, 0},
	{1, 0, 0},
};

static const unsigned int cfg_fifo_num[MAX_SV_HW_NUM][MAX_DMA_CORE] = {
	{0, 0, 2},
	{0, 0, 2},
	{0, 2, 2},
	{0, 2, 0},
	{0, 0, 0},
	{0, 0, 0},
};

static void sv_df_policy_make(struct mtk_cam_sv_df_mgr *sv_df_mgr)
{
	struct sv_df_dev_info *dev_info;
	struct sv_df_port_info *port_info;
	struct sv_df_action *pending_action;
	unsigned int i;
	unsigned int optimal_fifo_num, dev_fifo_num;
	unsigned int cfg_fifo_num, avai_fifo_num, free_fifo_num;
	unsigned long long total_bw = 0;
	unsigned int total_fifo_num = 0;

	/* reset pending action */
	for (i = 0; i < MAX_SV_DF_HW_NUM; i++) {
		dev_info = &sv_df_mgr->dev_info[i];
		if (dev_info->state == SV_DF_ST_IDLE ||
			dev_info->state == SV_DF_ST_PENDING) {
			pending_action = &dev_info->pending_action;
			if (pending_action->actions[DF_CORE_IDX].action ==
					SV_DF_REQ_ENQ) {
				dev_info->cfg_fifo_num -=
					pending_action->actions[DF_CORE_IDX].req_num;
				dev_info->port_info[DF_CORE_IDX].cfg_fifo_num -=
					pending_action->actions[DF_CORE_IDX].req_num;
				sv_df_mgr->avai_fifo_num +=
					pending_action->actions[DF_CORE_IDX].req_num;

				pending_action->actions[DF_CORE_IDX].action =
					SV_DF_REQ_NONE;
				pending_action->actions[DF_CORE_IDX].req_num = 0;
			}
		}
	}

	for (i = 0; i < MAX_SV_DF_HW_NUM; i++) {
		dev_info = &sv_df_mgr->dev_info[i];
		if (dev_info->state == SV_DF_ST_IDLE ||
			dev_info->state == SV_DF_ST_PENDING) {
			pending_action = &dev_info->pending_action;
			if (pending_action->actions[DF_CORE_IDX].action ==
					SV_DF_REQ_DEQ) {
				dev_info->cfg_fifo_num +=
					pending_action->actions[DF_CORE_IDX].req_num;
				dev_info->port_info[DF_CORE_IDX].cfg_fifo_num +=
					pending_action->actions[DF_CORE_IDX].req_num;
				sv_df_mgr->avai_fifo_num -=
					pending_action->actions[DF_CORE_IDX].req_num;

				pending_action->actions[DF_CORE_IDX].action =
					SV_DF_REQ_NONE;
				pending_action->actions[DF_CORE_IDX].req_num = 0;
			}
		}
	}

	avai_fifo_num = sv_df_mgr->avai_fifo_num;
	sv_df_mgr->avai_fifo_num = 0;
	total_fifo_num += avai_fifo_num;

	for (i = 0; i < MAX_SV_DF_HW_NUM; i++) {
		dev_info = &sv_df_mgr->dev_info[i];
		if (dev_info->state == SV_DF_ST_IDLE ||
			dev_info->state == SV_DF_ST_PENDING) {
			total_bw += dev_info->applied_bw;
			total_fifo_num += dev_info->non_cfg_fifo_num +
				dev_info->cfg_fifo_num;
		}
	}

	if (enable_sv_df_log) {
		pr_info("%s total_bw:%llu total_fifo_num:%d\n",
			__func__, total_bw, total_fifo_num);
		mtk_cam_sv_df_debug_dump(sv_df_mgr, "original");
	}

	/* deq */
	for (i = 0; i < MAX_SV_DF_HW_NUM; i++) {
		dev_info = &sv_df_mgr->dev_info[i];
		pending_action = &dev_info->pending_action;
		if ((dev_info->state == SV_DF_ST_IDLE ||
				dev_info->state == SV_DF_ST_PENDING) &&
			(pending_action->actions[DF_CORE_IDX].action ==
				SV_DF_REQ_NONE)) {
			optimal_fifo_num =
				total_fifo_num * dev_info->applied_bw / total_bw;
			dev_fifo_num = dev_info->non_cfg_fifo_num +
				dev_info->cfg_fifo_num;

			if (optimal_fifo_num < dev_fifo_num) {
				port_info = &dev_info->port_info[DF_CORE_IDX];
				cfg_fifo_num = port_info->cfg_fifo_num;
				free_fifo_num = 0;
				while (cfg_fifo_num &&
					((dev_fifo_num - free_fifo_num - DF_UNIT) >=
						optimal_fifo_num)) {
					pending_action->actions[DF_CORE_IDX].action =
						SV_DF_REQ_DEQ;
					pending_action->actions[DF_CORE_IDX].req_num +=
						DF_UNIT;

					cfg_fifo_num -= DF_UNIT;
					free_fifo_num += DF_UNIT;
					avai_fifo_num += DF_UNIT;
				};
				dev_info->cfg_fifo_num -= free_fifo_num;
				port_info->cfg_fifo_num -= free_fifo_num;
			}
		}
	}

	if (enable_sv_df_log)
		mtk_cam_sv_df_debug_dump(sv_df_mgr, "after_deq");

	/* enq */
	for (i = 0; i < MAX_SV_DF_HW_NUM; i++) {
		dev_info = &sv_df_mgr->dev_info[i];
		pending_action = &dev_info->pending_action;
		if ((dev_info->state == SV_DF_ST_IDLE ||
				dev_info->state == SV_DF_ST_PENDING) &&
			(pending_action->actions[DF_CORE_IDX].action ==
				SV_DF_REQ_NONE)) {
			optimal_fifo_num =
				total_fifo_num * dev_info->applied_bw / total_bw;
			dev_fifo_num = dev_info->non_cfg_fifo_num +
				dev_info->cfg_fifo_num;

			if (optimal_fifo_num > dev_fifo_num) {
				port_info = &dev_info->port_info[DF_CORE_IDX];
				cfg_fifo_num = 0;
				while (avai_fifo_num) {
					pending_action->actions[DF_CORE_IDX].action =
						SV_DF_REQ_ENQ;
					pending_action->actions[DF_CORE_IDX].req_num +=
						DF_UNIT;

					cfg_fifo_num += DF_UNIT;
					avai_fifo_num -= DF_UNIT;

					if (dev_fifo_num + cfg_fifo_num >= optimal_fifo_num)
						break;
				};
				dev_info->cfg_fifo_num += cfg_fifo_num;
				port_info->cfg_fifo_num += cfg_fifo_num;
			}
		}
	}

	sv_df_mgr->avai_fifo_num = avai_fifo_num;

	/* update state */
	for (i = 0; i < MAX_SV_DF_HW_NUM; i++) {
		dev_info = &sv_df_mgr->dev_info[i];
		pending_action = &dev_info->pending_action;

		if ((dev_info->state == SV_DF_ST_IDLE) &&
			!mtk_cam_sv_check_df_action_done(pending_action))
			dev_info->state = SV_DF_ST_PENDING;

		if ((dev_info->state == SV_DF_ST_PENDING) &&
			mtk_cam_sv_check_df_action_done(pending_action))
			dev_info->state = SV_DF_ST_IDLE;
	}

	if (enable_sv_df_log) {
		mtk_cam_sv_df_debug_dump(sv_df_mgr, "after_enq");
		pr_info("%s avai_fifo_num:%d\n",
			__func__, sv_df_mgr->avai_fifo_num);
	}
}

unsigned int mtk_cam_sv_df_get_non_cfg_fifo_num(unsigned int sv_idx,
		unsigned int core_idx)
{
	return (sv_idx < MAX_SV_HW_NUM && core_idx < MAX_DMA_CORE) ?
			non_cfg_fifo_num[sv_idx][core_idx] : 0;
}

unsigned int mtk_cam_sv_df_get_cfg_fifo_num(unsigned int sv_idx,
		unsigned int core_idx)
{
	return (sv_idx < MAX_SV_HW_NUM && core_idx < MAX_DMA_CORE) ?
			cfg_fifo_num[sv_idx][core_idx] : 0;
}

int mtk_cam_sv_df_mgr_init(struct mtk_cam_sv_df_mgr *sv_df_mgr)
{
	int ret = 0, i, j;

	sv_df_mgr->avai_fifo_num = 0;

	for (i = 0; i < MAX_SV_HW_NUM; i++) {
		sv_df_mgr->dev_info[i].applied_bw = 0;
		sv_df_mgr->dev_info[i].state = SV_DF_ST_NONE;
		sv_df_mgr->dev_info[i].non_cfg_fifo_num = 0;
		sv_df_mgr->dev_info[i].cfg_fifo_num = 0;
		for (j = 0; j < MAX_DMA_CORE; j++) {
			sv_df_mgr->dev_info[i].port_info[j].non_cfg_fifo_num =
				mtk_cam_sv_df_get_non_cfg_fifo_num(i, j);
			sv_df_mgr->dev_info[i].non_cfg_fifo_num +=
				mtk_cam_sv_df_get_non_cfg_fifo_num(i, j);

			sv_df_mgr->dev_info[i].port_info[j].cfg_fifo_num =
				mtk_cam_sv_df_get_cfg_fifo_num(i, j);
			sv_df_mgr->dev_info[i].cfg_fifo_num +=
				mtk_cam_sv_df_get_cfg_fifo_num(i, j);

			sv_df_mgr->dev_info[i].pending_action.actions[j].action =
				SV_DF_REQ_NONE;
			sv_df_mgr->dev_info[i].pending_action.actions[j].req_num = 0;
		}
	}

	return ret;
}

unsigned int mtk_cam_sv_df_get_runtime_fifo_size(
		struct mtk_cam_sv_df_mgr *sv_df_mgr,
		unsigned int sv_idx, unsigned int core_idx)
{
	unsigned int fifo_num = 0;

	mutex_lock(&sv_df_mgr->op_lock);
	fifo_num += sv_df_mgr->dev_info[sv_idx].port_info[core_idx].cfg_fifo_num;
	fifo_num += sv_df_mgr->dev_info[sv_idx].port_info[core_idx].non_cfg_fifo_num;
	mutex_unlock(&sv_df_mgr->op_lock);

	return fifo_num * 512;
}

int mtk_cam_sv_df_update_bw(struct mtk_cam_sv_df_mgr *sv_df_mgr,
		unsigned int sv_idx, unsigned long long applied_bw)
{
	int ret = 0;
	bool is_bw_changed = false;

	if (sv_idx >= MAX_SV_DF_HW_NUM)
		goto EXIT;

	mutex_lock(&sv_df_mgr->op_lock);

	if ((sv_df_mgr->dev_info[sv_idx].state == SV_DF_ST_NONE) &&
		(applied_bw > 0))
		sv_df_mgr->dev_info[sv_idx].state = SV_DF_ST_IDLE;

	is_bw_changed =
		(sv_df_mgr->dev_info[sv_idx].applied_bw != applied_bw) ?
		true : false;
	sv_df_mgr->dev_info[sv_idx].applied_bw = applied_bw;
	/* bw changed or stream off */
	if (is_bw_changed || applied_bw == 0)
		sv_df_policy_make(sv_df_mgr);

	if (enable_sv_df_log)
		pr_info("%s sv_idx:%d applied_bw:%llu_%llu state:%d\n",
			__func__, sv_idx,
			sv_df_mgr->dev_info[sv_idx].applied_bw,
			applied_bw,
			sv_df_mgr->dev_info[sv_idx].state);

	mutex_unlock(&sv_df_mgr->op_lock);

EXIT:
	return ret;
}

int mtk_cam_sv_df_action_ack(struct mtk_cam_sv_df_mgr *sv_df_mgr,
		unsigned int sv_idx)
{
	struct sv_df_action *pending_action;
	int ret = 0;
	unsigned int i;

	mutex_lock(&sv_df_mgr->op_lock);
	if (sv_df_mgr->dev_info[sv_idx].state == SV_DF_ST_EXE) {
		sv_df_mgr->dev_info[sv_idx].state = SV_DF_ST_IDLE;

		pending_action = &sv_df_mgr->dev_info[sv_idx].pending_action;
		for (i = 0; i < MAX_DMA_CORE; i++) {
			pending_action->actions[i].action = SV_DF_REQ_NONE;
			pending_action->actions[i].req_num = 0;
		}

		if (enable_sv_df_log)
			pr_info("%s sv_idx:%d\n", __func__, sv_idx);
	}
	mutex_unlock(&sv_df_mgr->op_lock);

	return ret;
}

bool mtk_cam_sv_df_next_action(struct mtk_cam_sv_df_mgr *sv_df_mgr,
		unsigned int sv_idx, struct sv_df_action *sv_df_action)
{
	bool is_next_action = false;
	char msg[MAX_SV_DF_DUMP_LEN];
	unsigned int i, len = 0;

	mutex_lock(&sv_df_mgr->op_lock);
	if (sv_df_mgr->dev_info[sv_idx].state == SV_DF_ST_PENDING) {
		is_next_action = true;
		sv_df_mgr->dev_info[sv_idx].state = SV_DF_ST_EXE;
		memcpy(sv_df_action, &sv_df_mgr->dev_info[sv_idx].pending_action,
			sizeof(struct sv_df_action));

		if (enable_sv_df_log) {
			for (i = 0; i < MAX_DMA_CORE; i++) {
				SV_DF_SNPRINTF(msg, len, " port%d:%d_%d", i,
					sv_df_action->actions[i].action,
					sv_df_action->actions[i].req_num);
			}
			pr_info("%s sv_idx:%d%s\n", __func__, sv_idx, msg);
		}
	}
	mutex_unlock(&sv_df_mgr->op_lock);

	return is_next_action;
}

int mtk_cam_sv_df_reset(struct mtk_cam_sv_df_mgr *sv_df_mgr,
		unsigned int sv_idx)
{
	struct sv_df_dev_info *dev_info;
	struct sv_df_action *pending_action;
	struct sv_df_port_info *port_info;
	int ret = 0;
	unsigned int i;

	mutex_lock(&sv_df_mgr->op_lock);
	sv_df_mgr->dev_info[sv_idx].state = SV_DF_ST_NONE;
	sv_df_mgr->dev_info[sv_idx].applied_bw = 0;

	dev_info = &sv_df_mgr->dev_info[sv_idx];
	pending_action = &dev_info->pending_action;
	for (i = 0; i < MAX_DMA_CORE; i++) {
		port_info = &dev_info->port_info[i];
		if (i == DF_CORE_IDX) {
			sv_df_mgr->avai_fifo_num += port_info->cfg_fifo_num;
			dev_info->cfg_fifo_num -= port_info->cfg_fifo_num;
			port_info->cfg_fifo_num = 0;
			pending_action->actions[i].action = SV_DF_REQ_NONE;
			pending_action->actions[i].req_num = 0;
		} else {
			pending_action->actions[i].action =
				mtk_cam_sv_df_get_cfg_fifo_num(sv_idx, i) ?
				SV_DF_REQ_ENQ : SV_DF_REQ_NONE;
			pending_action->actions[i].req_num =
				mtk_cam_sv_df_get_cfg_fifo_num(sv_idx, i);
		}
	}

	if (enable_sv_df_log)
		pr_info("%s avai_fifo:%d sv_idx:%d\n", __func__,
			sv_df_mgr->avai_fifo_num, sv_idx);

	mutex_unlock(&sv_df_mgr->op_lock);

	return ret;
}

bool mtk_cam_sv_check_df_action_done(struct sv_df_action *sv_df_action)
{
	bool is_done = true;
	int i;

	for (i = 0; i < MAX_DMA_CORE; i++) {
		if (sv_df_action->actions[i].action != SV_DF_REQ_NONE ||
			sv_df_action->actions[i].req_num != 0) {
			is_done = false;
			break;
		}
	}

	return is_done;
}

void mtk_cam_sv_df_debug_dump(struct mtk_cam_sv_df_mgr *sv_df_mgr,
		char *dbg_msg)
{
	struct sv_df_dev_info *dev_info;
	struct sv_df_action *pending_action;
	unsigned int i, j;

	for (i = 0; i < MAX_SV_DF_HW_NUM; i++) {
		dev_info = &sv_df_mgr->dev_info[i];
		pending_action = &dev_info->pending_action;
		pr_info("%s:%s sv_idx:%d applied_bw:%llu state:%d non_cfg_fifo_num:%d cfg_fifo_num:%d\n",
			__func__, dbg_msg, i, dev_info->applied_bw, dev_info->state,
			dev_info->non_cfg_fifo_num, dev_info->cfg_fifo_num);
		for (j = 0; j < MAX_DMA_CORE; j++) {
			pr_info("%s:%s sv_idx:%d core_idx:%d pending_action:%d_%d port_info:%d_%d\n",
				__func__, dbg_msg, i, j,
				pending_action->actions[j].action,
				pending_action->actions[j].req_num,
				dev_info->port_info[j].non_cfg_fifo_num,
				dev_info->port_info[j].cfg_fifo_num);
		}
	}
}

