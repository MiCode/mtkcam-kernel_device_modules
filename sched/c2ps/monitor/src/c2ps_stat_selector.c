// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include "c2ps_stat_selector.h"

int stat_selector_mode = C2PS_STAT_SELECTOR_V2;
module_param(stat_selector_mode, int, 0644);

inline enum c2ps_env_status c2ps_stat_selector_v1(struct global_info *glb_info)
{
	u8 _cluster_index = 0;

	if (unlikely(glb_info->overwrite_util_margin ||
				glb_info->decided_um_placeholder_val))
		return C2PS_STAT_TRANSIENT;

	C2PS_LOGD("fast_lxf_est: %llu, slow_lxf_est: %llu",
		glb_info->fast_lxf_est[0].est_val, glb_info->slow_lxf_est[0].est_val);

	for (; _cluster_index < c2ps_nr_clusters; _cluster_index++) {
		c2ps_cal_kf_est(&(glb_info->slow_lxf_est[_cluster_index]),
					glb_info->s_loadxfreq[_cluster_index]/1000);
		c2ps_cal_kf_est(&(glb_info->fast_lxf_est[_cluster_index]),
					glb_info->s_loadxfreq[_cluster_index]/1000);
	}

	// FIXME: currently only use cluster 0 to test state selector v1
	if (glb_info->fast_lxf_est[0].est_val >
				glb_info->slow_lxf_est[0].est_val + LxF_DIFF_THRES) {
		C2PS_LOGD("fast_lxf_est > slow_lxf_est, diff:%llu",
			glb_info->fast_lxf_est[0].est_val - glb_info->slow_lxf_est[0].est_val);
		glb_info->slow_lxf_est[0].q_val = LxF_S_KF_QVAL/5;
		return C2PS_STAT_TRANSIENT;
	}
	glb_info->slow_lxf_est[0].q_val = LxF_S_KF_QVAL;

	return C2PS_STAT_STABLE;
}

inline enum c2ps_env_status c2ps_stat_selector_v2(struct global_info *glb_info)
{
	if (unlikely(glb_info->overwrite_util_margin ||
				glb_info->decided_um_placeholder_val))
		return C2PS_STAT_TRANSIENT;
	if (glb_info->has_anchor_spec &&
		glb_info->runnable_count_signal == C2PS_RUNNABLE_DANGER)
		return C2PS_STAT_RUNNABLE_BOOST;
	return C2PS_STAT_STABLE;
}

enum c2ps_env_status determine_cur_system_state(struct global_info *glb_info)
{
	switch (stat_selector_mode) {
	case C2PS_STAT_SELECTOR_STABLE:
		if (unlikely(glb_info->overwrite_util_margin ||
				glb_info->decided_um_placeholder_val))
			return C2PS_STAT_TRANSIENT;
		return C2PS_STAT_STABLE;
	case C2PS_STAT_SELECTOR_V1:
		return c2ps_stat_selector_v1(glb_info);
	case C2PS_STAT_SELECTOR_V2:
		return c2ps_stat_selector_v2(glb_info);
	default:
		return C2PS_STAT_NODEF;
	}
}
