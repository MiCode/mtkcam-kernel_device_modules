// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include "c2ps_regulator_policy.h"
#include <linux/math64.h>
#include <linux/string.h>


static unsigned int c2ps_regulator_debug_max_uclamp = 1000;
static unsigned int c2ps_regulator_debug_min_uclamp = 1000;
static unsigned int c2ps_regulator_base_update_uclamp = 0;
static unsigned int c2ps_regulator_simple_min_uclamp = 50;
static unsigned int c2ps_uclamp_up_margin = 0;
static unsigned int c2ps_uclamp_down_margin = 0;

module_param(c2ps_regulator_debug_max_uclamp, int, 0644);
module_param(c2ps_regulator_debug_min_uclamp, int, 0644);
module_param(c2ps_regulator_base_update_uclamp, int, 0644);
module_param(c2ps_uclamp_up_margin, int, 0644);
module_param(c2ps_uclamp_down_margin, int, 0644);
module_param(c2ps_regulator_simple_min_uclamp, int, 0644);

void set_uclamp(const int pid, unsigned int max_util, unsigned int min_util)
{
	int ret = -1;
	struct task_struct *p;
	// unsigned long task_load;
	struct sched_attr attr = {};

	if (pid < 0)
		return;

	min_util = clamp(min_util, 1U, 1024U);
	max_util = clamp(max_util, 1U, 1024U);

	attr.sched_policy = SCHED_NORMAL;
	attr.sched_flags = 0;
	attr.sched_flags = SCHED_FLAG_KEEP_ALL   |
			   SCHED_FLAG_UTIL_CLAMP |
			   SCHED_FLAG_RESET_ON_FORK;

	attr.sched_util_min = min_util;
	attr.sched_util_max = max_util;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (likely(p))
		get_task_struct(p);
	rcu_read_unlock();

	if (likely(p)) {
		C2PS_LOGD("check pid name: %s, pid: %d", p->comm, pid);
		ret = sched_setattr_nocheck(p, &attr);
		/* set this task to break system uclamp max limitation */
		set_curr_uclamp_hint(pid, 1);

		if (ret == 0) {
			C2PS_LOGD("set uclamp(%d, %d) to %s successfully",
				      attr.sched_util_min, attr.sched_util_max,
				      p->comm);
		} else {
			C2PS_LOGD("set uclamp(%d, %d) to %s failed",
				      attr.sched_util_min, attr.sched_util_max,
				      p->comm);
		}
		put_task_struct(p);
	}
	c2ps_systrace_c(pid, min_util, "uclamp min");
	c2ps_systrace_c(pid, max_util, "uclamp max");
}

/**
 * @brief      map to C2PS_REGULATOR_MODE_FIX
 */
void c2ps_regulator_policy_fix_uclamp(struct regulator_req *req)
{
	C2PS_LOGD("task_id (%d) use fix uclamp policy", req->tsk_info->task_id);
	req->tsk_info->latest_uclamp = req->tsk_info->default_uclamp;
	set_uclamp(req->tsk_info->pid,
		req->tsk_info->default_uclamp,
		req->tsk_info->default_uclamp);
}

/**
 * @brief      map to C2PS_REGULATOR_MODE_SIMPLE
 */
void c2ps_regulator_policy_simple(struct regulator_req *req)
{
	u64 average_proc_time = req->tsk_info->hist_proc_time_sum /
							proc_time_window_size;
	s64 update_ratio_numer =
		(s64)average_proc_time - (s64)req->tsk_info->task_target_time;
	s64 update_ratio_denom = req->tsk_info->task_target_time;
	s64 update_uclamp = 0;
	int new_uclamp = 0;

	C2PS_LOGD("task_id (%d) use simple uclamp policy",
		  req->tsk_info->task_id);

	if (!req->tsk_info->task_target_time) {
		C2PS_LOGD("task target time equals to zero");
		return;
	}

	C2PS_LOGD("check task_target_time: %lld, proc_time: %llu, "
		 "proc_time_avg: %llu, diff: %lld task_id: %d",
		 req->tsk_info->task_target_time, req->tsk_info->proc_time,
		 average_proc_time, update_ratio_numer,
		 req->tsk_info->task_id);

	if (req->tsk_info->latest_uclamp <= 0) {
		req->tsk_info->latest_uclamp = req->tsk_info->default_uclamp;
	}

	update_ratio_numer =
		c2ps_regulator_base_update_uclamp * update_ratio_numer * 100;
	update_uclamp =
		div64_s64(update_ratio_numer, update_ratio_denom) / 100;

	new_uclamp = (int)req->tsk_info->latest_uclamp + (int)update_uclamp;

	/* limit new_uclamp in default uclamp +- deviation */
	new_uclamp = max((int)(req->tsk_info->default_uclamp *
		                  (100 - c2ps_uclamp_down_margin) / 100),
		         min((int)(req->tsk_info->default_uclamp *
		         	      (100 + c2ps_uclamp_up_margin) / 100),
		         new_uclamp));

	new_uclamp = max((int)c2ps_regulator_simple_min_uclamp,
		             min(1024, new_uclamp));
	req->tsk_info->latest_uclamp = new_uclamp;


	set_uclamp(req->tsk_info->pid,
		req->tsk_info->latest_uclamp,
		req->tsk_info->latest_uclamp);

	/* debug tool tag */
	c2ps_systrace_d(
		"c2ps simple policy: task: %d average_proc_time: %llu, "
		"realtime_proc_time: %llu",
		req->tsk_info->task_id, average_proc_time,
		req->tsk_info->proc_time);
}

/**
 * @brief      map to C2PS_REGULATOR_MODE_DEBUG
 */
void c2ps_regulator_policy_debug_uclamp(struct regulator_req *req)
{
	C2PS_LOGD("task_id (%d) use fix uclamp policy", req->tsk_info->task_id);
	req->tsk_info->latest_uclamp = c2ps_regulator_debug_max_uclamp;
	set_uclamp(req->tsk_info->pid,
		c2ps_regulator_debug_max_uclamp,
		c2ps_regulator_debug_min_uclamp);
}
