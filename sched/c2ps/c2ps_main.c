// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/timer.h>

#include "c2ps_common.h"
#include "c2ps_monitor.h"
#include "c2ps_uclamp_regulator.h"
#include "c2ps_usedext.h"
#include "c2ps_sysfs.h"

enum C2PS_NOTIFIER_PUSH_TYPE {
	C2PS_NOTIFIER_INIT			= 0x00,
	C2PS_NOTIFIER_UNINIT		= 0x01,
	C2PS_NOTIFIER_ADD_TASK		= 0x02,
	C2PS_NOTIFIER_TASK_START	= 0x03,
	C2PS_NOTIFIER_TASK_END		= 0x04,
	C2PS_NOTIFIER_SCENE_CHANGE	= 0x05,
	C2PS_NOTIFIER_CAMFPS        = 0x06,
	C2PS_NOTIFIER_VSYNC         = 0x07,
};

struct C2PS_NOTIFIER_PUSH_TAG {
	enum C2PS_NOTIFIER_PUSH_TYPE ePushType;
	int camfps;
	int pid;
	int task_id;
	int group_head;
	int scene_mode;
	u32 task_target_time;
	u32 default_uclamp;
	u32 task_group_target_time;
	u64 cur_ts;
	bool is_vip_task;
	bool is_dynamic_tid;
	struct list_head queue_list;
};

static struct task_struct *c2ps_tsk;
static LIST_HEAD(head);
static int condition_notifier_wq;
static bool condition_notifier_exit;
static DEFINE_MUTEX(notifier_wq_lock);
static DECLARE_WAIT_QUEUE_HEAD(notifier_wq_queue);
static void self_uninit_timer_callback(struct timer_list *t);

struct timer_list self_uninit_timer;

static void c2ps_notifier_wq_cb_init(void)
{
	if (unlikely(init_c2ps_common())) {
		C2PS_LOGD("init_c2ps_common failed\n");
		return;
	}

	self_uninit_timer.expires = jiffies + 5*HZ;
	timer_setup(&self_uninit_timer, self_uninit_timer_callback, 0);
	add_timer(&self_uninit_timer);
}

static void c2ps_notifier_wq_cb_uninit(void)
{
	C2PS_LOGD("[C2PS_CB] uninit\n");
	// disable sugov per-gear uclamp max feature
	set_gear_uclamp_ctrl(0);
	// disable sugov curr_uclamp feature
	set_curr_uclamp_ctrl(0);
	c2ps_uclamp_regulator_flush();
	exit_c2ps_common();
	del_timer_sync(&self_uninit_timer);
}

static void c2ps_notifier_wq_cb_add_task(
	u32 task_id, u32 task_target_time, u32 default_uclamp,
	int group_head, u32 task_group_target_time, bool is_vip_task,
	bool is_dynamic_tid)
{
	struct c2ps_task_info *tsk_info = NULL;
	C2PS_LOGD("[C2PS_CB] add_task task_id: %d\n", task_id);

	tsk_info = kzalloc(sizeof(*tsk_info), GFP_KERNEL);

	if (unlikely(!tsk_info)) {
		C2PS_LOGE("OOM\n");
		return;
	}

	tsk_info->task_id = task_id;
	tsk_info->task_target_time = task_target_time * NSEC_PER_MSEC;
	tsk_info->default_uclamp = default_uclamp;
	tsk_info->is_vip_task = is_vip_task;
	tsk_info->is_dynamic_tid = is_dynamic_tid;

	if (unlikely(c2ps_add_task_info(tsk_info))) {
		C2PS_LOGE("add task failed\n");
		return;
	}

	// task group enabled
	if (group_head > 0) {
		// is task group head
		if (tsk_info->task_id == group_head) {
			if (unlikely(c2ps_create_task_group(
					group_head, task_group_target_time))) {
				C2PS_LOGE("create task group failed\n");
				return;
			}
		}
		if (unlikely(c2ps_add_task_to_group(tsk_info, group_head))) {
			C2PS_LOGE("add task to group failed\n");
			return;
		}
	}
}

static void c2ps_notifier_wq_cb_task_start(int pid, int task_id)
{
	C2PS_LOGD("[C2PS_CB] task_start task_id: %d\n", task_id);

	if (unlikely(monitor_task_start(pid, task_id)))
		C2PS_LOGE("notify_task_start failed\n");
}

static void c2ps_notifier_wq_cb_task_end(int pid, int task_id)
{
	C2PS_LOGD("[C2PS_CB] task_end task_id: %d\n", task_id);

	if (unlikely(monitor_task_end(pid, task_id)))
		C2PS_LOGE("notify_task_end failed\n");
}

static void c2ps_notifier_wq_cb_vsync(u64 ts)
{
	C2PS_LOGD("[C2PS_CB] vsync: %llu\n", ts);

	if (unlikely(monitor_vsync(ts)))
		C2PS_LOGE("notify_vsync failed\n");
}

static void c2ps_notifier_wq_cb_camfps(int camfps)
{
	C2PS_LOGD("[C2PS_CB] camfps: %d\n", camfps);

	if (unlikely(monitor_camfps(camfps)))
		C2PS_LOGE("notify_camfps failed\n");
}

static void c2ps_notifier_wq_cb_scene_change(int task_id, int scene_mode)
{
	C2PS_LOGD("[C2PS_CB] scene_change: %d\n", task_id);

	if (unlikely(monitor_task_scene_change(task_id, scene_mode)))
		C2PS_LOGE("notify_task_scene_change failed\n");
}

static void c2ps_queue_work(struct C2PS_NOTIFIER_PUSH_TAG *vpPush)
{
	if (timer_pending(&self_uninit_timer))
		mod_timer(&self_uninit_timer, jiffies + 5*HZ);
	mutex_lock(&notifier_wq_lock);
	list_add_tail(&vpPush->queue_list, &head);
	condition_notifier_wq = 1;
	mutex_unlock(&notifier_wq_lock);

	wake_up_interruptible(&notifier_wq_queue);
}

static int c2ps_uninit_wo_lock(void)
{
	struct C2PS_NOTIFIER_PUSH_TAG *vpPush = NULL;
	int ret = 0;

	C2PS_LOGD("+\n");

	vpPush = (struct C2PS_NOTIFIER_PUSH_TAG *)
		c2ps_alloc_atomic(sizeof(*vpPush));

	if (!vpPush) {
		C2PS_LOGE("OOM\n");
		ret = -ENOMEM;
		goto out;
	}

	if (!c2ps_tsk) {
		C2PS_LOGE("NULL WorkQueue\n");
		c2ps_free(vpPush, sizeof(*vpPush));
		ret = -EINVAL;
		goto out;
	}

	vpPush->ePushType = C2PS_NOTIFIER_UNINIT;
	list_add_tail(&vpPush->queue_list, &head);
	condition_notifier_wq = 1;

	wake_up_interruptible(&notifier_wq_queue);
out:
	return ret;
}

static void c2ps_notifier_wq_cb(void)
{
	struct C2PS_NOTIFIER_PUSH_TAG *vpPush;

	wait_event_interruptible(notifier_wq_queue,
		condition_notifier_wq || condition_notifier_exit);

	if (condition_notifier_exit)
		return;

	mutex_lock(&notifier_wq_lock);

	if (!list_empty(&head)) {
		vpPush = list_first_entry(&head,
			struct C2PS_NOTIFIER_PUSH_TAG, queue_list);
		list_del(&vpPush->queue_list);
		if (list_empty(&head))
			condition_notifier_wq = 0;
		mutex_unlock(&notifier_wq_lock);
	} else {
		condition_notifier_wq = 0;
		mutex_unlock(&notifier_wq_lock);
		return;
	}

	switch (vpPush->ePushType) {
	case C2PS_NOTIFIER_INIT:
		c2ps_notifier_wq_cb_init();
		break;
	case C2PS_NOTIFIER_UNINIT:
		c2ps_notifier_wq_cb_uninit();
		break;
	case C2PS_NOTIFIER_ADD_TASK:
		c2ps_notifier_wq_cb_add_task(
			vpPush->task_id,
			vpPush->task_target_time, vpPush->default_uclamp,
			vpPush->group_head, vpPush->task_group_target_time,
			vpPush->is_vip_task, vpPush->is_dynamic_tid);
		break;
	case C2PS_NOTIFIER_TASK_START:
		c2ps_notifier_wq_cb_task_start(vpPush->pid, vpPush->task_id);
		break;
	case C2PS_NOTIFIER_TASK_END:
		c2ps_notifier_wq_cb_task_end(vpPush->pid, vpPush->task_id);
		break;
	case C2PS_NOTIFIER_SCENE_CHANGE:
		c2ps_notifier_wq_cb_scene_change(vpPush->task_id, vpPush->scene_mode);
		break;
	case C2PS_NOTIFIER_CAMFPS:
		c2ps_notifier_wq_cb_camfps(vpPush->camfps);
		break;
	case C2PS_NOTIFIER_VSYNC:
		c2ps_notifier_wq_cb_vsync(vpPush->cur_ts);
		break;
	default:
		C2PS_LOGE("unhandled push type = %d\n",
				vpPush->ePushType);
		break;
	}
	c2ps_free(vpPush, sizeof(*vpPush));
}

static int c2ps_thread_loop(void *arg)
{
	set_user_nice(current, -20);

	while (!kthread_should_stop())
		c2ps_notifier_wq_cb();

	return 0;
}

int c2ps_notify_init(
	int cfg_camfps, int max_uclamp_cluster0, int max_uclamp_cluster1,
	int max_uclamp_cluster2)
{
	struct C2PS_NOTIFIER_PUSH_TAG *vpPush = NULL;
	int ret = 0;

	C2PS_LOGD(
		"config camfps (frames per 1000 seconds): %d, "
		"max_uclamp_cluster0: %d, max_uclamp_cluster1: %d, "
		"max_uclamp_cluster2: %d",
		cfg_camfps, max_uclamp_cluster0, max_uclamp_cluster1,
		max_uclamp_cluster2);

	// set_config_camfps(cfg_camfps);

	// enable sugov per-gear uclamp max feature
	set_gear_uclamp_ctrl(1);
	set_gear_uclamp_max(0, max_uclamp_cluster0);
	set_gear_uclamp_max(1, max_uclamp_cluster1);
	set_gear_uclamp_max(2, max_uclamp_cluster2);

	// enable sugov curr_uclamp feature
	set_curr_uclamp_ctrl(1);

	vpPush = (struct C2PS_NOTIFIER_PUSH_TAG *)
		c2ps_alloc_atomic(sizeof(*vpPush));

	if (!vpPush) {
		C2PS_LOGE("OOM\n");
		ret = -ENOMEM;
		goto out;
	}

	if (!c2ps_tsk) {
		C2PS_LOGE("NULL WorkQueue\n");
		c2ps_free(vpPush, sizeof(*vpPush));
		ret = -EINVAL;
		goto out;
	}

	vpPush->ePushType = C2PS_NOTIFIER_INIT;
	c2ps_queue_work(vpPush);

out:
	return ret;
}

int c2ps_notify_uninit(void)
{
	struct C2PS_NOTIFIER_PUSH_TAG *vpPush = NULL;
	int ret = 0;

	C2PS_LOGD("+\n");

	vpPush = (struct C2PS_NOTIFIER_PUSH_TAG *)
		c2ps_alloc_atomic(sizeof(*vpPush));

	if (!vpPush) {
		C2PS_LOGE("OOM\n");
		ret = -ENOMEM;
		goto out;
	}

	if (!c2ps_tsk) {
		C2PS_LOGE("NULL WorkQueue\n");
		c2ps_free(vpPush, sizeof(*vpPush));
		ret = -EINVAL;
		goto out;
	}

	vpPush->ePushType = C2PS_NOTIFIER_UNINIT;
	c2ps_queue_work(vpPush);

out:
	return ret;
}

int c2ps_notify_add_task(
    u32 task_id, u32 task_target_time, u32 default_uclamp,
	int group_head, u32 task_group_target_time,
	bool is_vip_task, bool is_dynamic_tid)
{
	struct C2PS_NOTIFIER_PUSH_TAG *vpPush = NULL;
	int ret = 0;

	C2PS_LOGD("task_id: %d\n", task_id);

	vpPush = (struct C2PS_NOTIFIER_PUSH_TAG *)
		c2ps_alloc_atomic(sizeof(*vpPush));

	if (!vpPush) {
		C2PS_LOGE("OOM\n");
		ret = -ENOMEM;
		goto out;
	}

	if (!c2ps_tsk) {
		C2PS_LOGE("NULL WorkQueue\n");
		c2ps_free(vpPush, sizeof(*vpPush));
		ret = -EINVAL;
		goto out;
	}

	vpPush->ePushType = C2PS_NOTIFIER_ADD_TASK;
	vpPush->task_id = task_id;
	vpPush->task_target_time = task_target_time;
	vpPush->default_uclamp = default_uclamp;
	vpPush->group_head = group_head;
	vpPush->task_group_target_time = task_group_target_time;
	vpPush->is_vip_task = is_vip_task;
	vpPush->is_dynamic_tid = is_dynamic_tid;

	c2ps_queue_work(vpPush);

out:
	return ret;
}

int c2ps_notify_task_start(int pid, int task_id)
{
	struct C2PS_NOTIFIER_PUSH_TAG *vpPush = NULL;
	int ret = 0;

	C2PS_LOGD("task_id: %d\n", task_id);

	vpPush = (struct C2PS_NOTIFIER_PUSH_TAG *)
		c2ps_alloc_atomic(sizeof(*vpPush));

	if (!vpPush) {
		C2PS_LOGE("OOM\n");
		ret = -ENOMEM;
		goto out;
	}

	if (!c2ps_tsk) {
		C2PS_LOGE("NULL WorkQueue\n");
		c2ps_free(vpPush, sizeof(*vpPush));
		ret = -EINVAL;
		goto out;
	}

	vpPush->ePushType = C2PS_NOTIFIER_TASK_START;
	vpPush->pid = pid;
	vpPush->task_id = task_id;

	c2ps_queue_work(vpPush);

out:
	return ret;
}

int c2ps_notify_task_end(int pid, int task_id)
{
	struct C2PS_NOTIFIER_PUSH_TAG *vpPush = NULL;
	int ret = 0;

	C2PS_LOGD("task_id: %d\n", task_id);

	vpPush = (struct C2PS_NOTIFIER_PUSH_TAG *)
		c2ps_alloc_atomic(sizeof(*vpPush));

	if (!vpPush) {
		C2PS_LOGE("OOM\n");
		ret = -ENOMEM;
		goto out;
	}

	if (!c2ps_tsk) {
		C2PS_LOGE("NULL WorkQueue\n");
		c2ps_free(vpPush, sizeof(*vpPush));
		ret = -EINVAL;
		goto out;
	}

	vpPush->ePushType = C2PS_NOTIFIER_TASK_END;
	vpPush->pid = pid;
	vpPush->task_id = task_id;

	c2ps_queue_work(vpPush);

out:
	return ret;
}

int c2ps_notify_task_scene_change(int task_id, int scene_mode)
{
	struct C2PS_NOTIFIER_PUSH_TAG *vpPush = NULL;
	int ret = 0;

	C2PS_LOGD("task_id: %d\n", task_id);

	vpPush = (struct C2PS_NOTIFIER_PUSH_TAG *)
		c2ps_alloc_atomic(sizeof(*vpPush));

	if (!vpPush) {
		C2PS_LOGE("OOM\n");
		ret = -ENOMEM;
		goto out;
	}

	if (!c2ps_tsk) {
		C2PS_LOGE("NULL WorkQueue\n");
		c2ps_free(vpPush, sizeof(*vpPush));
		ret = -EINVAL;
		goto out;
	}

	vpPush->ePushType = C2PS_NOTIFIER_SCENE_CHANGE;
	vpPush->task_id = task_id;
	vpPush->scene_mode = scene_mode;

	c2ps_queue_work(vpPush);

out:
	return ret;
}

int c2ps_notify_vsync(void)
{
	struct C2PS_NOTIFIER_PUSH_TAG *vpPush = NULL;
	int ret = 0;

	C2PS_LOGD("+\n");

	vpPush = (struct C2PS_NOTIFIER_PUSH_TAG *)
		c2ps_alloc_atomic(sizeof(*vpPush));

	if (!vpPush) {
		C2PS_LOGE("OOM\n");
		ret = -ENOMEM;
		goto out;
	}

	if (!c2ps_tsk) {
		C2PS_LOGE("NULL WorkQueue\n");
		c2ps_free(vpPush, sizeof(*vpPush));
		ret = -EINVAL;
		goto out;
	}

	vpPush->ePushType = C2PS_NOTIFIER_VSYNC;
	vpPush->cur_ts = c2ps_get_time();

	c2ps_queue_work(vpPush);

out:
	return ret;
}

int c2ps_notify_camfps(int camfps)
{
	struct C2PS_NOTIFIER_PUSH_TAG *vpPush = NULL;
	int ret = 0;

	C2PS_LOGD("camfps: %d\n", camfps);

	vpPush = (struct C2PS_NOTIFIER_PUSH_TAG *)
		c2ps_alloc_atomic(sizeof(*vpPush));

	if (!vpPush) {
		C2PS_LOGE("OOM\n");
		ret = -ENOMEM;
		goto out;
	}

	if (!c2ps_tsk) {
		C2PS_LOGE("NULL WorkQueue\n");
		c2ps_free(vpPush, sizeof(*vpPush));
		ret = -EINVAL;
		goto out;
	}

	vpPush->ePushType = C2PS_NOTIFIER_CAMFPS;
	vpPush->camfps = camfps;

	c2ps_queue_work(vpPush);

out:
	return ret;
}

static void self_uninit_timer_callback(struct timer_list *t)
{
	C2PS_LOGD("uninit expired");
	c2ps_uninit_wo_lock();
}

static int __init c2ps_init(void)
{
	C2PS_LOGD("+ \n");

	c2ps_tsk = kthread_create(c2ps_thread_loop, NULL, "c2ps_thread_loop");

	if (c2ps_tsk == NULL)
		return -EFAULT;
	wake_up_process(c2ps_tsk);

	c2ps_notify_init_fp = c2ps_notify_init;
	c2ps_notify_uninit_fp = c2ps_notify_uninit;
	c2ps_notify_add_task_fp = c2ps_notify_add_task;
	c2ps_notify_task_start_fp = c2ps_notify_task_start;
	c2ps_notify_task_end_fp = c2ps_notify_task_end;
	c2ps_notify_vsync_fp = c2ps_notify_vsync;
	c2ps_notify_camfps_fp = c2ps_notify_camfps;
	c2ps_notify_task_scene_change_fp = c2ps_notify_task_scene_change;

	c2ps_sysfs_init();
	if (unlikely(uclamp_regulator_init())) {
		C2PS_LOGD("uclamp_regulator_init failed\n");
		return -EFAULT;
	}

	C2PS_LOGD("- \n");
	return 0;
}

static void __exit c2ps_exit(void)
{
	C2PS_LOGD("+ \n");

	condition_notifier_exit = true;
	if (c2ps_tsk)
		kthread_stop(c2ps_tsk);

	c2ps_sysfs_exit();
	uclamp_regulator_exit();
	C2PS_LOGD("- \n");
}

module_init(c2ps_init);
module_exit(c2ps_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek C2PS");
MODULE_AUTHOR("MediaTek Inc.");
