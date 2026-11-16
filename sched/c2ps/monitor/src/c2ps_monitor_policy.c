// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include "c2ps_monitor_policy.h"

// reg_check_action
/**
 * @brief      map to C2PS_RESCUE_AGGRESIVE
 * Description:
 *  set uclamp to max MCPU capacity when reg_check_action trigger
 */
void reg_check_action_aggresive_set_uclamp(struct reg_check_input_data input_data)
{
	int idx = 0;
	struct cpu_info *g_cpu_info = get_cpu_info();

	C2PS_LOGD("+");

	if (unlikely(input_data.cache_data == NULL)) {
		C2PS_LOGW("input_data.cache_data is null");
		return;
	}

	if (unlikely(g_cpu_info == NULL)) {
		C2PS_LOGW("g_cpu_info is null");
		return;
	}

	for (; idx < input_data.cache_data->num_thread; idx++) {
		if (input_data.cache_data->threads[idx] > 0)
			set_uclamp(input_data.cache_data->threads[idx], 1024, g_cpu_info->m_core_max_util);
		else
			break;
	}
	c2ps_main_systrace("%s sets uclamps %d", __func__, g_cpu_info->m_core_max_util);
}

void reg_check_action_aggresive_unset_uclamp(struct reg_check_input_data input_data)
{
	int idx = 0;
	bool new_threadid = true;

	C2PS_LOGD("+");
	if (unlikely(input_data.cache_data == NULL)) {
		C2PS_LOGW("input_data.cache_data is null");
		return;
	}

	for (; idx < input_data.cache_data->num_thread; idx++) {
		if (input_data.cache_data->threads[idx] > 0) {
			set_uclamp(input_data.cache_data->threads[idx], 1024, 0);
			if (input_data.cache_data->threads[idx] > 0 == current->pid)
				new_threadid = false;
		} else {
			if (unlikely(new_threadid)) {
				input_data.cache_data->threads[idx] = current->pid;
				set_uclamp(input_data.cache_data->threads[idx], 1024, 0);
			}
			break;
		}
	}
	c2ps_main_systrace("%s unsets", __func__);
}

/**
 * @brief      map to C2PS_RESCUE_CONVERGE
 * Description:
 *  converge uclamp value when reg_check_action trigger
 */
void reg_check_action_converge_set_uclamp(struct reg_check_input_data input_data)
{
	int idx = 0;
	struct cpu_info *g_cpu_info = get_cpu_info();

	C2PS_LOGD("+");

	if (unlikely(input_data.cache_data == NULL)) {
		C2PS_LOGW("input_data.cache_data is null");
		return;
	}

	if (unlikely(g_cpu_info == NULL)) {
		C2PS_LOGW("g_cpu_info is null");
		return;
	}

	// half time 4 frames reciprocal
	int uclamp_min = (input_data.cache_data->curr_uclamp*152) >> 7;

	// aggressively boost for the init rescue, use an untunable value to prevent small value
	if (unlikely(input_data.cache_data->curr_uclamp < 50)) {
		uclamp_min = g_cpu_info->m_core_max_util >> 1;
		input_data.cache_data->last_high_uclamp = g_cpu_info->m_core_max_util;
		input_data.cache_data->last_low_uclamp = 0;
	}

	input_data.cache_data->rescue_count++;

	// aggressively boost for the second rescue
	if (input_data.cache_data->rescue_count >= 2) {
		uclamp_min = g_cpu_info->m_core_max_util;
		input_data.cache_data->last_high_uclamp = g_cpu_info->m_core_max_util;
		c2ps_main_systrace("aggresive set to %d due to rescue count: %d",
			g_cpu_info->m_core_max_util, input_data.cache_data->rescue_count);
		C2PS_LOGD("aggresive set to %d due to rescue count: %d",
			g_cpu_info->m_core_max_util, input_data.cache_data->rescue_count);
	}

	for (; idx < input_data.cache_data->num_thread; idx++) {
		if (input_data.cache_data->threads[idx] > 0)
			set_uclamp(input_data.cache_data->threads[idx], 1024, uclamp_min);
		else
			break;
	}

	if (input_data.cache_data->curr_uclamp < uclamp_min)
		input_data.cache_data->last_low_uclamp =
			(input_data.cache_data->curr_uclamp + input_data.cache_data->last_low_uclamp) >> 1;
	input_data.cache_data->curr_uclamp = uclamp_min;
	c2ps_main_systrace("%s sets uclamps %d", __func__, input_data.cache_data->curr_uclamp);
	C2PS_LOGD("update curr_uclamp to: %d, last_low_uclamp: %d, last_high_uclamp: %d",
			  input_data.cache_data->curr_uclamp,
			  input_data.cache_data->last_low_uclamp,
			  input_data.cache_data->last_high_uclamp);

}

void reg_check_action_converge_unset_uclamp(struct reg_check_input_data input_data)
{
	int idx = 0;

	C2PS_LOGD("+");
	if (unlikely(input_data.cache_data == NULL)) {
		C2PS_LOGW("input_data.cache_data is null");
		return;
	}

	int uclamp_min = 0;
	bool new_threadid = true;

	if (input_data.cache_data->rescue_count == 0) {
		// half time 5 frames
		uclamp_min = (input_data.cache_data->curr_uclamp * 891) >> 10;
		input_data.cache_data->curr_uclamp = uclamp_min;
		C2PS_LOGD("unset rescue_count == 0");
	}

	input_data.cache_data->rescue_count = 0;

	for (; idx < input_data.cache_data->num_thread; idx++) {
		if (input_data.cache_data->threads[idx] > 0) {
			if (uclamp_min > 0)
				set_uclamp(input_data.cache_data->threads[idx], 1024, uclamp_min);
			if (input_data.cache_data->threads[idx] > 0 == current->pid)
				new_threadid = false;
		} else {
			if (unlikely(new_threadid)) {
				input_data.cache_data->threads[idx] = current->pid;
				if (uclamp_min > 0)
					set_uclamp(input_data.cache_data->threads[idx], 1024, uclamp_min);
			}
			break;
		}
	}

	c2ps_main_systrace("%s sets uclamps %d", __func__, input_data.cache_data->curr_uclamp);
	C2PS_LOGD("curr_uclamp to: %d, last_low_uclamp: %d, last_high_uclamp: %d",
			  input_data.cache_data->curr_uclamp,
			  input_data.cache_data->last_low_uclamp,
			  input_data.cache_data->last_high_uclamp);
}

/**
 * @brief      map to C2PS_RESCUE_NORMAL
 * Description:
 *  set uclamp to (max MCPU capacity + last_uclamp)/2 when reg_check_action trigger
 */
void reg_check_action_normal_set_uclamp(struct reg_check_input_data input_data)
{
	int idx = 0;
	struct cpu_info *g_cpu_info = get_cpu_info();

	C2PS_LOGD("+");

	if (unlikely(input_data.cache_data == NULL)) {
		C2PS_LOGW("input_data.cache_data is null");
		return;
	}

	if (unlikely(g_cpu_info == NULL)) {
		C2PS_LOGW("g_cpu_info is null");
		return;
	}

	int uclamp_min = (g_cpu_info->m_core_max_util + input_data.cache_data->curr_uclamp*2)/3;

	input_data.cache_data->curr_uclamp = uclamp_min;
	for (; idx < input_data.cache_data->num_thread; idx++) {
		if (input_data.cache_data->threads[idx] > 0)
			set_uclamp(input_data.cache_data->threads[idx], 1024, uclamp_min);
		else
			break;
	}
	c2ps_main_systrace("%s sets uclamps %d", __func__, input_data.cache_data->curr_uclamp);
}

void reg_check_action_normal_unset_uclamp(struct reg_check_input_data input_data)
{
	int idx = 0;
	bool new_threadid = true;

	C2PS_LOGD("reg_check_action_normal_unset_uclamp");
	if (unlikely(input_data.cache_data == NULL)) {
		C2PS_LOGW("input_data.cache_data is null");
		return;
	}

	input_data.cache_data->curr_uclamp = 0;

	// reset uclamp to 0
	for (; idx < input_data.cache_data->num_thread; idx++) {
		if (input_data.cache_data->threads[idx] > 0) {
			set_uclamp(input_data.cache_data->threads[idx], 1024, 0);
			if (input_data.cache_data->threads[idx] > 0 == current->pid)
				new_threadid = false;
		} else {
			if (unlikely(new_threadid)) {
				input_data.cache_data->threads[idx] = current->pid;
				set_uclamp(input_data.cache_data->threads[idx], 1024, 0);
			}
			break;
		}
	}
	c2ps_main_systrace("%s unsets", __func__);
}
