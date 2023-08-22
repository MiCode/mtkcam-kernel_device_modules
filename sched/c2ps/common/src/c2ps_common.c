// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/cputime.h>
#include <linux/sched/clock.h>
#include <linux/prefetch.h>
#include <linux/hashtable.h>
#include <linux/preempt.h>
#include <linux/kernel.h>
#include <trace/trace.h>

#include "sched/sched.h"
#include "c2ps_common.h"
#include "c2ps_sysfs.h"

#ifndef CREATE_TRACE_POINTS
#define CREATE_TRACE_POINTS
#endif
#include "c2ps_trace_event.h"

static DEFINE_HASHTABLE(task_info_tbl, 3);
static DEFINE_MUTEX(task_info_tbl_lock);
static DEFINE_HASHTABLE(task_group_info_tbl, 3);
static DEFINE_MUTEX(task_group_info_tbl_lock);

static struct kobject *base_kobj;
static struct global_info *glb_info;

int proc_time_window_size = 1;
int debug_log_on = 0;
int background_idlerate_alert = 12;
module_param(proc_time_window_size, int, 0644);
module_param(debug_log_on, int, 0644);
module_param(background_idlerate_alert, int, 0644);

struct c2ps_task_info *c2ps_find_task_info_by_tskid(int task_id)
{
	struct c2ps_task_info *tsk_info = NULL;

	C2PS_LOGD("+ \n");
	c2ps_task_info_tbl_lock(__func__);

	if (hash_empty(task_info_tbl)) {
		C2PS_LOGD("empty task info table\n");
		goto out;
	}
	C2PS_LOGD("task_id: %d\n", task_id);
	C2PS_LOGD("find +\n");
	hash_for_each_possible(task_info_tbl, tsk_info, hlist, task_id) {
		if (tsk_info->task_id == task_id) {
			C2PS_LOGD("task_id: %d\n", tsk_info->task_id);
			goto out;
		}
	}
	C2PS_LOGD("find -\n");

out:
	c2ps_task_info_tbl_unlock(__func__);
	C2PS_LOGD("- \n");
	return tsk_info;
}

int c2ps_add_task_info(struct c2ps_task_info *tsk_info)
{
	if (!tsk_info) {
		C2PS_LOGE("tsk_info is null\n");
		return -EINVAL;
	}
	C2PS_LOGD("task_id: %d", tsk_info->task_id);
	c2ps_task_info_tbl_lock(__func__);
	hash_add(task_info_tbl, &tsk_info->hlist, tsk_info->task_id);
	c2ps_task_info_tbl_unlock(__func__);
	C2PS_LOGD("- \n");
	return 0;
}

void c2ps_clear_task_info_table(void)
{
	struct c2ps_task_info *tsk_info = NULL;
	struct hlist_node *tmp = NULL;
	int bkt = 0;

	C2PS_LOGD("+ \n");
	c2ps_task_info_tbl_lock(__func__);
	if (hash_empty(task_info_tbl)) {
		C2PS_LOGD("task info table is empty\n");
		goto out;
	}

	hash_for_each_safe(
		task_info_tbl, bkt, tmp, tsk_info, hlist) {
		hash_del(&tsk_info->hlist);
		kfree(tsk_info);
		tsk_info = NULL;
	}

out:
	c2ps_task_info_tbl_unlock(__func__);
	C2PS_LOGD("- \n");
}

int c2ps_create_task_group(int group_head, u64 task_group_target_time)
{
	struct task_group_info *tsk_grp_info = NULL;

	C2PS_LOGD("group_head: %d, task_group_target_time: %lld",
			group_head, task_group_target_time);

	tsk_grp_info = kzalloc(sizeof(*tsk_grp_info), GFP_KERNEL);

	if (unlikely(!tsk_grp_info)) {
		C2PS_LOGE("OOM\n");
		return -ENOMEM;
	}

	tsk_grp_info->group_head = group_head;
	tsk_grp_info->group_target_time = task_group_target_time * NSEC_PER_MSEC;
	mutex_init(&tsk_grp_info->mlock);

	c2ps_task_group_info_tbl_lock(__func__);
	hash_add(
		task_group_info_tbl, &tsk_grp_info->hlist, tsk_grp_info->group_head);
	c2ps_task_group_info_tbl_unlock(__func__);
	C2PS_LOGD("- \n");
	return 0;
}

struct task_group_info *c2ps_find_task_group_info_by_grphd(int group_head)
{
	struct task_group_info *tsk_grp_info = NULL;

	C2PS_LOGD("+ \n");
	c2ps_task_group_info_tbl_lock(__func__);

	if (hash_empty(task_group_info_tbl)) {
		C2PS_LOGD("empty task group info table\n");
		goto out;
	}
	C2PS_LOGD("group_head: %d\n", group_head);
	C2PS_LOGD("find +\n");
	hash_for_each_possible(task_group_info_tbl, tsk_grp_info, hlist, group_head) {
		if (tsk_grp_info->group_head == group_head) {
			C2PS_LOGD("group_head: %d\n", tsk_grp_info->group_head);
			goto out;
		}
	}
	C2PS_LOGD("find -\n");

out:
	c2ps_task_group_info_tbl_unlock(__func__);
	C2PS_LOGD("- \n");
	return tsk_grp_info;
}

int c2ps_add_task_to_group(struct c2ps_task_info *tsk_info, int group_head)
{
	struct task_group_info *tsk_grp_info =
		c2ps_find_task_group_info_by_grphd(group_head);

	C2PS_LOGD("+ \n");
	if (!tsk_info) {
		C2PS_LOGE("tsk_info is null\n");
		return -EINVAL;
	}

	if (!tsk_grp_info) {
		C2PS_LOGE("task group not exist\n");
		return -EINVAL;
	}
	tsk_info->tsk_group = tsk_grp_info;
	C2PS_LOGD("- \n");
	return 0;
}

void c2ps_clear_task_group_info_table(void)
{
	struct task_group_info *tsk_grp_info = NULL;
	struct hlist_node *tmp = NULL;
	int bkt = 0;

	C2PS_LOGD("+ \n");
	c2ps_task_group_info_tbl_lock(__func__);
	if (hash_empty(task_group_info_tbl)) {
		C2PS_LOGD("task group info table is empty\n");
		goto out;
	}

	hash_for_each_safe(
		task_group_info_tbl, bkt, tmp, tsk_grp_info, hlist) {
		hash_del(&tsk_grp_info->hlist);
		kfree(tsk_grp_info);
		tsk_grp_info = NULL;
	}

out:
	c2ps_task_group_info_tbl_unlock(__func__);
	C2PS_LOGD("- \n");
}

static inline void c2ps_prefetch_curr_exec_start(struct task_struct *p)
{
#if IS_ENABLED(CONFIG_FAIR_GROUP_SCHED)
	struct sched_entity *curr = (&p->se)->cfs_rq->curr;
#else
	struct sched_entity *curr = (&task_rq(p)->cfs)->curr;
#endif
	prefetch(curr);
	prefetch(&curr->exec_start);
}

extern struct rq *task_rq_lock(struct task_struct *p, struct rq_flags *rf);
/*
 * Return accounted runtime for the task.
 * In case the task is currently running, return the runtime plus current's
 * pending runtime that have not been accounted yet.
 */
u64 c2ps_task_sched_runtime(struct task_struct *p)
{
	struct rq_flags rf;
	struct rq *rq;
	u64 ns;

#if IS_ENABLED(CONFIG_64BIT) && IS_ENABLED(CONFIG_SMP)
	if (!p->on_cpu || !task_on_rq_queued(p))
		return p->se.sum_exec_runtime;
#endif

	rq = task_rq_lock(p, &rf);

	if (task_current(rq, p) && task_on_rq_queued(p)) {
		c2ps_prefetch_curr_exec_start(p);
		update_rq_clock(rq);
		p->sched_class->update_curr(rq);
	}
	ns = p->se.sum_exec_runtime;
	task_rq_unlock(rq, p, &rf);

	return ns;
}

u64 c2ps_get_sum_exec_runtime(int pid)
{
	struct task_struct *tsk;
	u64 curr_sum_exec_runtime = 0;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (tsk)
		get_task_struct(tsk);
	rcu_read_unlock();

	if (!tsk)
		return 0;
	curr_sum_exec_runtime = c2ps_task_sched_runtime(tsk);
	put_task_struct(tsk);

	return curr_sum_exec_runtime;
}

inline void c2ps_task_info_tbl_lock(const char *tag)
{
	mutex_lock(&task_info_tbl_lock);
}

inline void c2ps_task_info_tbl_unlock(const char *tag)
{
	mutex_unlock(&task_info_tbl_lock);
}

inline void c2ps_task_group_info_tbl_lock(const char *tag)
{
	mutex_lock(&task_group_info_tbl_lock);
}

inline void c2ps_task_group_info_tbl_unlock(const char *tag)
{
	mutex_unlock(&task_group_info_tbl_lock);
}

inline void c2ps_info_lock(struct mutex *mlock)
{
	mutex_lock(mlock);
}

inline void c2ps_info_unlock(struct mutex *mlock)
{
	mutex_unlock(mlock);
}

u64 c2ps_get_time(void)
{
	u64 tmp;

	preempt_disable();
	tmp = cpu_clock(smp_processor_id());
	preempt_enable();

	return tmp;
}

void c2ps_update_task_info_hist(struct c2ps_task_info *tsk_info)
{
	C2PS_LOGD("+ \n");
	c2ps_info_lock(&tsk_info->mlock);

	if (tsk_info->hist_proc_time_sum >=
		tsk_info->hist_proc_time[tsk_info->nr_hist_info]) {
		tsk_info->hist_proc_time_sum -=
	                          tsk_info->hist_proc_time[tsk_info->nr_hist_info];
	} else {
		tsk_info->hist_proc_time_sum = 0;
	}

	tsk_info->hist_proc_time[tsk_info->nr_hist_info] = tsk_info->proc_time;
	tsk_info->hist_proc_time_sum +=
		tsk_info->hist_proc_time[tsk_info->nr_hist_info];
	tsk_info->hist_loading[tsk_info->nr_hist_info] = tsk_info->loading;
	++tsk_info->nr_hist_info;
	tsk_info->nr_hist_info %= (min(proc_time_window_size , MAX_WINDOW_SIZE));
	c2ps_info_unlock(&tsk_info->mlock);
	C2PS_LOGD("- \n");
}

struct global_info *get_glb_info(void)
{
	return glb_info;
}

inline void set_config_camfps(int camfps)
{
	if (!glb_info) {
		C2PS_LOGE("glb_info is null\n");
		return;
	}
	c2ps_info_lock(&glb_info->mlock);
	glb_info->cfg_camfps = camfps;
	c2ps_info_unlock(&glb_info->mlock);
}

inline void set_glb_info_bg_uclamp_max(void)
{
	if (!glb_info) {
		C2PS_LOGE("glb_info is null\n");
		return;
	}
	c2ps_info_lock(&glb_info->mlock);
	{
		short _idx = 0;
		for (; _idx < NUMBER_OF_CLUSTER; _idx++) {
			glb_info->max_uclamp[_idx] = get_gear_uclamp_max(_idx);
			glb_info->curr_max_uclamp[_idx] = glb_info->max_uclamp[_idx];
		}
	}
	c2ps_info_unlock(&glb_info->mlock);
}

inline void update_vsync_time(u64 ts)
{
	if (!glb_info) {
		C2PS_LOGE("glb_info is null\n");
		return;
	}
	c2ps_info_lock(&glb_info->mlock);
	glb_info->vsync_time = ts;
	c2ps_info_unlock(&glb_info->mlock);
}

inline void update_camfps(int camfps)
{
	if (!glb_info) {
		C2PS_LOGE("glb_info is null\n");
		return;
	}
	c2ps_info_lock(&glb_info->mlock);
	glb_info->camfps = camfps;
	c2ps_info_unlock(&glb_info->mlock);
}

inline bool is_group_head(struct c2ps_task_info *tsk_info)
{
	if (!tsk_info) {
		C2PS_LOGE("tsk_info is null\n");
		return false;
	}
	return tsk_info->task_id == tsk_info->tsk_group->group_head;
}

void c2ps_systrace_c(pid_t pid, int val, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;
	char buf[256];

	if (!trace_c2ps_systrace_enabled())
		return;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	len = snprintf(buf, sizeof(buf), "C|%d|%s|%d\n", pid, log, val);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf[255] = '\0';

	trace_c2ps_systrace(buf);
}

void c2ps_main_systrace(const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;
	char buf[256];

	if (!trace_c2ps_main_trace_enabled())
		return;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	len = snprintf(buf, sizeof(buf), "%s\n", log);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf[255] = '\0';

	trace_c2ps_main_trace(buf);
}

void c2ps_bg_info_systrace(const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;
	char buf[256];

	if (!trace_c2ps_bg_info_enabled())
		return;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	len = snprintf(buf, sizeof(buf), "%s\n", log);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf[255] = '\0';

	trace_c2ps_bg_info(buf);
}

void c2ps_critical_task_systrace(struct c2ps_task_info *tsk_info)
{
	int len;
	unsigned int curr_cpu = 0;
	unsigned long curr_freq = 0;
	unsigned int curr_util = 0;
	char buf[256];
	struct task_struct *p;

	if (!tsk_info) {
		C2PS_LOGE("tsk_info is null\n");
		return;
	}
	if (!trace_c2ps_critical_task_enabled())
		return;

	curr_util = tsk_info->latest_uclamp;

	rcu_read_lock();
	p = find_task_by_vpid(tsk_info->pid);

	if (likely(p)) {
		get_task_struct(p);
		curr_cpu = task_cpu(p);
		curr_freq = c2ps_get_uclamp_freq(curr_cpu, curr_util);
		put_task_struct(p);
	}
	rcu_read_unlock();

	len = snprintf(buf, sizeof(buf),
		"task_name=%s_%d util=%d freq=%ld\n",
		tsk_info->task_name,  tsk_info->task_id,
		curr_util, curr_freq);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf[255] = '\0';

	trace_c2ps_critical_task(buf);
}

void *c2ps_alloc_atomic(int i32Size)
{
	void *pvBuf;

	if (i32Size <= PAGE_SIZE)
		pvBuf = kmalloc(i32Size, GFP_ATOMIC);
	else
		pvBuf = vmalloc(i32Size);

	return pvBuf;
}

void c2ps_free(void *pvBuf, int i32Size)
{
	if (!pvBuf)
		return;

	if (i32Size <= PAGE_SIZE)
		kfree(pvBuf);
	else
		vfree(pvBuf);
}

unsigned long c2ps_get_uclamp_freq(int cpu,  unsigned int uclamp)
{
	unsigned long am_util = 0;

	am_util = (uclamp * get_adaptive_margin(cpu)) >> SCHED_CAPACITY_SHIFT;
	return pd_get_util_freq(cpu, am_util);
}

void update_cpu_idle_rate(void)
{
	u64 idle_time, wall_time;
	unsigned int _cpu_index = 0;

	if (!glb_info)
		return;

	// Only timer callback will call this function, shouldn't lock
	for (; _cpu_index < MAX_CPU_NUM; _cpu_index++) {
		struct per_cpu_idle_rate *idle_rate =
			&glb_info->cpu_idle_rates[_cpu_index];
		int _cluster_idx = 0;

		idle_time = get_cpu_idle_time(_cpu_index, &wall_time, 1);

		idle_rate->idle = (100 * (idle_time - idle_rate->idle_time)) /
					(wall_time - idle_rate->wall_time);

		idle_rate->idle_time = idle_time;
		idle_rate->wall_time = wall_time;
		C2PS_LOGD("check idle rate: %u for cpu: %d", idle_rate->idle, _cpu_index);

		if (_cpu_index <= LCORE_ID)
			_cluster_idx = 0;
		else if (_cpu_index <= MCORE_ID)
			_cluster_idx = 1;
		else if (_cpu_index <= BCORE_ID)
			_cluster_idx = 2;

		if (idle_rate->idle < background_idlerate_alert) {
			glb_info->need_update_uclamp[0] = 1;
			glb_info->need_update_uclamp[1 + _cluster_idx] = 1;
		} else if (glb_info->curr_max_uclamp[_cluster_idx] >
					glb_info->max_uclamp[_cluster_idx]) {
			glb_info->need_update_uclamp[0] = 1;
			glb_info->need_update_uclamp[1 + _cluster_idx] = -1;
		}
	}

	c2ps_bg_info_systrace(
		"cluster_0_util=%d cluster_1_util=%d cluster_2_util=%d "
		"cluster_0_freq=%ld cluster_1_freq=%ld cluster_2_freq=%ld",
		glb_info->curr_max_uclamp[0], glb_info->curr_max_uclamp[1],
		glb_info->curr_max_uclamp[2],
		c2ps_get_uclamp_freq(LCORE_ID, glb_info->curr_max_uclamp[0]),
		c2ps_get_uclamp_freq(MCORE_ID, glb_info->curr_max_uclamp[1]),
		c2ps_get_uclamp_freq(BCORE_ID, glb_info->curr_max_uclamp[2]));
}

inline bool need_update_background(void)
{
	if (!glb_info)
		return false;
	return glb_info->need_update_uclamp[0];
}

inline void reset_need_update_status(void)
{
	if (!glb_info)
		return;

	glb_info->need_update_uclamp[0] = 0;
}

static ssize_t task_info_show(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	struct c2ps_task_info *tsk_info;
	char *temp = NULL;
	int pos = 0;
	int length = 0;
	int bkt = 0;

	temp = kcalloc(C2PS_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!temp)
		goto out;

    length = scnprintf(temp + pos, C2PS_SYSFS_MAX_BUFF_SIZE - pos,
	    "\nTASKID\tPID\tTASK_NAME\tINIT_UCLAMP\tTASK_TARGET_TIME\tVIP_TASK\t");
    pos += length;
    length = scnprintf(temp + pos, C2PS_SYSFS_MAX_BUFF_SIZE - pos,
	    "START_TIME\tEND_TIME\tPROC_TIME\tEXEC_TIME\tLATEST_UCLAMP\n");
    pos += length;

    c2ps_task_info_tbl_lock(__func__);

    hash_for_each(task_info_tbl, bkt, tsk_info, hlist) {
		length = scnprintf(temp + pos,
			C2PS_SYSFS_MAX_BUFF_SIZE - pos,
			"%-2d\t%-5d\t%*s\t%-4u\t\t%-8llu\t\t%d\t\t",
			tsk_info->task_id, tsk_info->pid,
			-MAX_TASK_NAME_SIZE, tsk_info->task_name,
			tsk_info->default_uclamp, tsk_info->task_target_time,
			tsk_info->is_vip_task);
		pos += length;
		length = scnprintf(temp + pos,
			C2PS_SYSFS_MAX_BUFF_SIZE - pos,
			"%-12llu\t%-12llu\t%-10llu\t%-10llu\t%-4d\n",
			tsk_info->start_time, tsk_info->end_time,
			tsk_info->proc_time, tsk_info->real_exec_runtime,
			tsk_info->latest_uclamp);
		pos += length;
    }

    c2ps_task_info_tbl_unlock(__func__);

    length = scnprintf(buf, PAGE_SIZE, "%s", temp);

out:
	kfree(temp);
	return length;
}

static KOBJ_ATTR_RO(task_info);

static ssize_t gear_uclamp_max_store(struct kobject *kobj,
	struct kobj_attribute *attr,
	const char *buf, size_t count)
{
	int gearid = -1;
	int val = -1;
	char *buffer = NULL;

	buffer = kcalloc(C2PS_SYSFS_MAX_BUFF_SIZE, sizeof(char), GFP_KERNEL);
	if (!buffer)
		goto out;

	if ((count > 0) && (count < C2PS_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(buffer, C2PS_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (sscanf(buffer, "%d %d", &gearid, &val) != 2)
				goto out;
		}
	}

	if (gearid < 0 || gearid >= get_nr_gears() ||
		val < 0)
		goto out;

	set_gear_uclamp_max(gearid, val);

out:
	kfree(buffer);
	return count;
}

static ssize_t gear_uclamp_max_show(struct kobject *kobj,
	struct kobj_attribute *attr,
	char *buf)
{
	int length = 0;

	for (int i = 0; i < get_nr_gears(); ++i) {
		length += scnprintf(buf + length, PAGE_SIZE - length,
				"gear: %d, uclamp_max: %d \n",
				i, get_gear_uclamp_max(i));
	}

	return length;
}

static KOBJ_ATTR_RW(gear_uclamp_max);

int init_c2ps_common(void)
{
	int ret = 0;

	hash_init(task_info_tbl);
	hash_init(task_group_info_tbl);
	glb_info = kmalloc(sizeof(*glb_info), GFP_KERNEL);

	if (unlikely(!glb_info)) {
		C2PS_LOGE("OOM\n");
		return -ENOMEM;
	}

	mutex_init(&glb_info->mlock);

	if (!(ret = c2ps_sysfs_create_dir(NULL, "common", &base_kobj))) {
		c2ps_sysfs_create_file(base_kobj, &kobj_attr_task_info);
		c2ps_sysfs_create_file(base_kobj, &kobj_attr_gear_uclamp_max);
	}

	set_glb_info_bg_uclamp_max();

	return ret;
}

void exit_c2ps_common(void)
{
	c2ps_clear_task_info_table();
	c2ps_clear_task_group_info_table();
	kfree(glb_info);
	glb_info = NULL;

	c2ps_sysfs_remove_file(base_kobj, &kobj_attr_task_info);
	c2ps_sysfs_remove_file(base_kobj, &kobj_attr_gear_uclamp_max);
	c2ps_sysfs_remove_dir(&base_kobj);
}
