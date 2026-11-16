// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/string.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#include "c2ps_common.h"
#include "c2ps_monitor.h"
#include "c2ps_regulator.h"
#include "c2ps_stat_selector.h"

struct timer_list c2ps_reg_check_timer;
struct workqueue_struct *proxy_wq;

static atomic_t enable_reg_check_timer = ATOMIC_INIT(0);
static DEFINE_HASHTABLE(reg_check_task_list, 3);
static DEFINE_MUTEX(reg_check_task_list_lock);
static struct c2ps_reg_check_proxy_task proxy_work;

static void update_reg_check_timer(void)
{
	unsigned long cur_timeout = c2ps_reg_check_timer.expires;
	struct reg_check_task *tsk = NULL;
	bool need_update_timer = false;
	bool at_least_one_task_active = false;
	u32 tmp = 0;

	mutex_lock(&reg_check_task_list_lock);

	if (unlikely(hash_empty(reg_check_task_list))) {
		C2PS_LOGD("empty reg_check_task_list\n");
		goto out;
	}
	hash_for_each(reg_check_task_list, tmp, tsk, hlist) {
		at_least_one_task_active |= tsk->active;

		if (tsk->active && tsk->start_timing > 0 && tsk->start_timing < cur_timeout) {
			cur_timeout = tsk->start_timing;
			need_update_timer = true;
		}
	}

out:
	mutex_unlock(&reg_check_task_list_lock);

	if (need_update_timer) {
		atomic_set(&enable_reg_check_timer, 1);
		mod_timer(&c2ps_reg_check_timer, cur_timeout);
	} else if(!at_least_one_task_active) {
		atomic_set(&enable_reg_check_timer, 0);
		// set the timeout to 10 sec later to keep the timer pending
		mod_timer(&c2ps_reg_check_timer, jiffies + 10*HZ);
	}
}

struct reg_check_task *c2ps_find_reg_check_task_by_id(u32 id)
{
	struct reg_check_task *tsk = NULL;

	C2PS_LOGD("+\n");
	mutex_lock(&reg_check_task_list_lock);

	if (unlikely(hash_empty(reg_check_task_list))) {
		C2PS_LOGD("empty reg_check_task_list\n");
		goto out;
	}
	hash_for_each_possible(reg_check_task_list, tsk, hlist, id) {
		C2PS_LOGD("find reg check task with id: %u\n", id);
		goto out;
	}

out:
	mutex_unlock(&reg_check_task_list_lock);
	C2PS_LOGD("-\n");
	return tsk;
}

int register_reg_check_task(struct reg_check_task *tsk)
{
	if (unlikely(!tsk)) {
		C2PS_LOGE("tsk is null\n");
		return -EINVAL;
	}
	mutex_lock(&reg_check_task_list_lock);
	hash_add(reg_check_task_list, &tsk->hlist, tsk->id);
	mutex_unlock(&reg_check_task_list_lock);
	return 0;
}

void clear_reg_check_task_list(void)
{
	struct reg_check_task *tsk = NULL;
	struct hlist_node *tmp = NULL;
	int bkt = 0;

	C2PS_LOGD("+\n");
	mutex_lock(&reg_check_task_list_lock);
	if (unlikely(hash_empty(reg_check_task_list))) {
		C2PS_LOGD("task info table is empty\n");
		goto out;
	}

	hash_for_each_safe(
		reg_check_task_list, bkt, tmp, tsk, hlist) {
		hash_del(&tsk->hlist);
		kfree(tsk);
		tsk = NULL;
	}

out:
	mutex_unlock(&reg_check_task_list_lock);
	C2PS_LOGD("-\n");
}

// set start_in_ms to -1 to notify task stop
int set_reg_check_task(struct reg_check_input_data input_data, void (*procfunc)(struct reg_check_input_data))
{
	struct reg_check_task *tsk = c2ps_find_reg_check_task_by_id(input_data.target_id);

	if (unlikely(tsk == NULL)) {
		// return directly if a task never starts but calling stop
		if (input_data.start_in_ms < 0)
			return -EINVAL;

		tsk = kzalloc(sizeof(*tsk), GFP_KERNEL);
		if (unlikely(!tsk)) {
			C2PS_LOGE("OOM\n");
			return -EINVAL;
		}

		tsk->id = input_data.target_id;
		tsk->cache_data.num_thread = MAX_PERFMONITOR_THREAD_NUM;

		if (unlikely(procfunc != NULL))
			tsk->procfunc = procfunc;

		if (unlikely(register_reg_check_task(tsk))) {
			C2PS_LOGE("add reg check task failed\n");
			return -EINVAL;
		}
	}

	if (input_data.start_in_ms >= 0) {
		input_data.cache_data = &(tsk->cache_data);
		input_data.cache_data->start_timing = c2ps_get_time();
		tsk->input_data = input_data;
		tsk->start_timing = jiffies + input_data.start_in_ms*HZ/1000;
		tsk->active = true;
	} else {
		if (likely(procfunc != NULL && tsk->active)) {
			input_data.cache_data = &(tsk->cache_data);
			procfunc(input_data);
		}
		tsk->start_timing = 0;
		tsk->active = false;
	}

	update_reg_check_timer();

	return 0;
}

void notify_rescue_task(struct reg_check_input_data input_data, bool is_start)
{
	if (is_start) {
		if (unlikely(input_data.start_in_ms < 10)) {
			C2PS_LOGW("do not support time spec less than 10ms");
			return;
		}

		C2PS_LOGD("check start rescue_mode: %d", input_data.rescue_mode);
		switch (input_data.rescue_mode) {
		case C2PS_RESCUE_AGGRESIVE:
			set_reg_check_task(input_data, reg_check_action_aggresive_set_uclamp);
			break;
		case C2PS_RESCUE_CONVERGE:
			set_reg_check_task(input_data, reg_check_action_converge_set_uclamp);
			break;
		case C2PS_RESCUE_NORMAL:
			set_reg_check_task(input_data, reg_check_action_normal_set_uclamp);
			break;
		default:
			break;
		}
	} else {
		C2PS_LOGD("check end rescue_mode: %d", input_data.rescue_mode);
		input_data.start_in_ms = -1;
		switch (input_data.rescue_mode) {
		case C2PS_RESCUE_AGGRESIVE:
			set_reg_check_task(input_data, reg_check_action_aggresive_unset_uclamp);
			break;
		case C2PS_RESCUE_CONVERGE:
			set_reg_check_task(input_data, reg_check_action_converge_unset_uclamp);
			break;
		case C2PS_RESCUE_NORMAL:
			set_reg_check_task(input_data, reg_check_action_normal_unset_uclamp);
			break;
		default:
			break;
		}
	}
}

static bool check_if_update_uclamp(struct c2ps_task_info *tsk_info)
{
	if (unlikely(tsk_info->tsk_group)) {
		u64 remaining_time = tsk_info->tsk_group->group_target_time -
			tsk_info->tsk_group->accumulate_time;
		C2PS_LOGD("task_id: %d, group_head: %d"
			"accumulate_time: %lld, remaining_time: %lld",
			tsk_info->task_id, tsk_info->tsk_group->group_head,
			tsk_info->tsk_group->accumulate_time,
			remaining_time);

		if (tsk_info->task_target_time > remaining_time)
			C2PS_LOGD("Boost is not needed\n");
		else
			C2PS_LOGD("Need boost\n");
	}
	return true;
}

static inline u64 cal_real_exec_runtime(struct c2ps_task_info *tsk_info)
{
	return c2ps_get_sum_exec_runtime(tsk_info->pid) -
		tsk_info->sum_exec_runtime_start;
}

static void reset_history_info(struct c2ps_task_info *tsk_info)
{
	if (unlikely(!tsk_info)) {
		C2PS_LOGE("tsk_info is null\n");
		return;
	}

	c2ps_info_lock(&tsk_info->mlock);
	memset(tsk_info->hist_proc_time, 0, sizeof(tsk_info->hist_proc_time));
	memset(tsk_info->hist_loading, 0, sizeof(tsk_info->hist_loading));
	c2ps_info_unlock(&tsk_info->mlock);
}

int monitor_task_start(int pid, int task_id)
{
	struct c2ps_task_info *tsk_info = c2ps_find_task_info_by_tskid(task_id);

	C2PS_LOGD("+\n");
	if (unlikely(!tsk_info)) {
		C2PS_LOGW_ONCE("tsk_info not found\n");
		C2PS_LOGW("tsk_info not found\n");
		return -1;
	}

	if (tsk_info->is_vip_task) {
		if (!is_task_vip(pid)) {
			C2PS_LOGD("set VIP by monitor: %d", pid);
			set_task_basic_vip(pid);
			tsk_info->vip_set_by_monitor = true;
		}
		if (unlikely(tsk_info->is_enable_dep_thread)) {
			int _i = 0;

			for (; _i < MAX_DEP_THREAD_NUM; _i++) {
				if (tsk_info->dep_thread[_i] <= 0)
					break;
				set_task_basic_vip(tsk_info->dep_thread[_i]);
			}
		}
	}

	tsk_info->pid = pid;

	if (check_if_update_uclamp(tsk_info)) {
		struct global_info *g_info = get_glb_info();
		int action_uclamp = 0;

		if (!g_info)
			return -1;

		action_uclamp = refine_uclamp(g_info, tsk_info->default_uclamp);
		set_uclamp(pid, action_uclamp, action_uclamp);
		tsk_info->latest_uclamp = action_uclamp;

		if (unlikely(tsk_info->is_enable_dep_thread)) {
			int _i = 0;

			for (; _i < MAX_DEP_THREAD_NUM; _i++) {
				if (tsk_info->dep_thread[_i] <= 0)
					break;
				C2PS_LOGD("thread (%d) set dep thread: %d",
					tsk_info->pid, tsk_info->dep_thread[_i]);
				set_uclamp(tsk_info->dep_thread[_i], action_uclamp, action_uclamp);
			}
		}
	}
	C2PS_LOGD("-\n");
	return 0;
}

int monitor_task_end(int pid, int task_id)
{
	struct c2ps_task_info *tsk_info = c2ps_find_task_info_by_tskid(task_id);

	C2PS_LOGD("+\n");
	if (unlikely(!tsk_info)) {
		C2PS_LOGW_ONCE("tsk_info not found\n");
		C2PS_LOGW("tsk_info not found\n");
		return -1;
	}

	c2ps_critical_task_systrace(tsk_info);

	if (unlikely(tsk_info->is_enable_dep_thread)) {
		if (unlikely(tsk_info->dep_thread_search_count
					< MAX_DEP_THREAD_SEARCH_COUNT)) {
			tsk_info->dep_thread_search_count++;
			c2ps_add_waker_pid_to_task_info(tsk_info);
		}
	}

	reset_task_eas_setting(tsk_info);
	C2PS_LOGD("-\n");
	return 0;
}

inline void cal_um(struct c2ps_anchor *anc)
{
	struct global_info *g_info = get_glb_info();
	struct regulator_req *req = NULL;

	if (unlikely(anc == NULL || g_info == NULL))
		return;

	if (g_info->stat != C2PS_STAT_RUNNABLE_BOOST) {
		req = get_regulator_req();
		if (likely(req != NULL)) {
			req->anc_info = anc;
			req->glb_info = g_info;
			req->stat = g_info->stat;
			send_regulator_req(req);
		}
	}
}

int monitor_anchor(
	int anc_id, u32 order, bool register_fixed, u32 anc_type, u32 anc_order,
	u64 cur_ts, u32 latency_spec, u32 jitter_spec)
{
	struct c2ps_anchor *anc_info = c2ps_find_anchor_by_id(anc_id);
	struct global_info *g_info = get_glb_info();

	C2PS_LOGD("check anchor notifier, anc_id: %d, anc_type: %d", anc_id, anc_type);

	if (unlikely(!g_info))
		return -1;

	if (unlikely((g_info->overwrite_util_margin > 0 ||
			g_info->decided_um_placeholder_val > 0))) {
		C2PS_LOGD("skip anchor monitor, use overwrite um: %u or um_placeholder: %u",
				g_info->overwrite_util_margin, g_info->decided_um_placeholder_val);
		return 0;
	}

	if (unlikely(anc_info == NULL)) {
		C2PS_LOGD("[C2PS_CB] add anchor: %d\n", anc_id);
		anc_info = kzalloc(sizeof(*anc_info), GFP_KERNEL);

		if (unlikely(!anc_info)) {
			C2PS_LOGE("OOM\n");
			return -EINVAL;
		}

		hash_init(anc_info->um_table_stbl);
		anc_info->anchor_id = anc_id;
		anc_info->latency_spec = latency_spec;
		anc_info->jitter_spec = jitter_spec * jitter_spec / 1000;

		if (unlikely(c2ps_add_anchor(anc_info))) {
			C2PS_LOGE("add anchor failed\n");
			return -EINVAL;
		}
	}

	if (unlikely(g_info->switch_um_idle_rate_mode)) {
		g_info->has_anchor_spec = false;
		return 0;
	}

	g_info->has_anchor_spec = true;

	switch (anc_type) {
	// anchor start
	case 1:
		if (likely(order > 0))
			anc_info->s_idx = order % ANCHOR_QUEUE_LEN;
		else
			break;

		anc_info->s_hist_t[anc_info->s_idx] = cur_ts;
		C2PS_LOGD("debug: check anchor %d start timing: %llu",
			anc_id, anc_info->s_hist_t[anc_info->s_idx]);
		break;
	// anchor end
	case 2:
		if (likely(order > 0))
			anc_info->e_idx = order % ANCHOR_QUEUE_LEN;
		else
			break;

		C2PS_LOGD("check anc_id:%d, e_idx:%d, order:%d", anc_id, anc_info->e_idx, order);
		anc_info->e_hist_t[anc_info->e_idx] = cur_ts;

		anc_info->latest_duration = anc_info->e_hist_t[anc_info->e_idx] -
			   anc_info->s_hist_t[anc_info->e_idx];

		C2PS_LOGD("debug: check anchor %d end timing: %llu corr start timing: %llu last end timing:: %llu",
			anc_id, anc_info->e_hist_t[anc_info->e_idx],
			anc_info->s_hist_t[anc_info->e_idx],
			anc_info->e_hist_t[anc_info->e_idx-1 >= 0?
				anc_info->e_idx-1:anc_info->e_idx+ANCHOR_QUEUE_LEN-1]);

		c2ps_update_um_table(anc_info);
		cal_um(anc_info);
		break;
	default:
		break;
	}
	C2PS_LOGD("-\n");
	return 0;
}

int monitor_vsync(u64 ts)
{
	C2PS_LOGD("+\n");
	update_vsync_time(ts);
	C2PS_LOGD("-\n");
	return 0;
}

int monitor_camfps(int camfps)
{
	C2PS_LOGD("+\n");
	update_camfps(camfps);
	C2PS_LOGD("-\n");
	return 0;
}

void monitor_system_info(void)
{
	struct global_info *g_info = get_glb_info();

	// Only timer callback will call this function, shouldn't lock in the following
	// functions
	update_cpu_idle_rate();

	if (likely(g_info)) {
		g_info->stat = g_info->is_cpu_boost ? C2PS_STAT_TRANSIENT
							: determine_cur_system_state(g_info);
	}
}

int monitor_task_scene_change(int task_id, int scene_mode)
{
	struct c2ps_task_info *tsk_info = c2ps_find_task_info_by_tskid(task_id);

	C2PS_LOGD("+\n");
	if (unlikely(!tsk_info)) {
		C2PS_LOGW_ONCE("tsk_info not found\n");
		C2PS_LOGW("tsk_info not found\n");
		return -1;
	}

	reset_history_info(tsk_info);
	C2PS_LOGD("-\n");
	return 0;
}

static void c2ps_proxy_wq_process(struct work_struct *work)
{
	if (unlikely(hash_empty(reg_check_task_list) || !work)) {
		C2PS_LOGD("no registered reg_check_task\n");
		return;
	}

	{
		struct reg_check_task *tsk = NULL;
		u32 tmp = 0;
		unsigned long curr_ns_time = c2ps_get_time();

		mutex_lock(&reg_check_task_list_lock);
		hash_for_each(reg_check_task_list, tmp, tsk, hlist) {
			if (tsk->active && tsk->start_timing > 0 &&
				tsk->procfunc != NULL) {
				if (curr_ns_time - tsk->input_data.cache_data->start_timing >=
						tsk->input_data.start_in_ms * 1000000)
					tsk->procfunc(tsk->input_data);
			}
		}
		mutex_unlock(&reg_check_task_list_lock);
	}
}

static void c2ps_reg_check_timer_callback(struct timer_list *t)
{
	if (atomic_read(&enable_reg_check_timer) > 0) {
		mod_timer(&c2ps_reg_check_timer, jiffies);
	} else {
		mod_timer(&c2ps_reg_check_timer, jiffies + 10*HZ);
		return;
	}

	c2ps_main_systrace("reg check timer callback");
	if (likely(proxy_wq != NULL))
		queue_work(proxy_wq, &proxy_work.m_work);
}

void c2ps_monitor_init(void)
{
	c2ps_reg_check_timer.expires = jiffies + 10*HZ;
	timer_setup(&c2ps_reg_check_timer, c2ps_reg_check_timer_callback, 0);
	add_timer(&c2ps_reg_check_timer);
	atomic_set(&enable_reg_check_timer, 0);

	if (proxy_wq == NULL)
		proxy_wq = alloc_ordered_workqueue("c2ps_proxy_wq", 0);
}

void c2ps_monitor_uninit(void)
{
	atomic_set(&enable_reg_check_timer, 0);
#if (KERNEL_VERSION(6, 15, 0) > LINUX_VERSION_CODE)
	del_timer_sync(&c2ps_reg_check_timer);
#else
	timer_delete_sync(&c2ps_reg_check_timer);
#endif
	clear_reg_check_task_list();
	if (proxy_wq) {
		flush_workqueue(proxy_wq);
		destroy_workqueue(proxy_wq);
		proxy_wq = NULL;
	}
}

void monitor_module_init(void)
{
	mutex_init(&reg_check_task_list_lock);
	INIT_WORK(&proxy_work.m_work, c2ps_proxy_wq_process);
}
