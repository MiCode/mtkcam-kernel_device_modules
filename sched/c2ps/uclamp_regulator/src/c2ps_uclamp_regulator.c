// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include "c2ps_common.h"
#include "c2ps_uclamp_regulator.h"
#include "c2ps_regulator_policy.h"

static struct task_struct *c2ps_regulator_thread;
static struct kmem_cache *regulator_reqs;
static LIST_HEAD(regulator_wq);
static int regulator_condition_notifier_wq;
static bool regulator_condition_notifier_exit;
static DEFINE_MUTEX(regulator_notifier_wq_lock);
static DEFINE_MUTEX(regulator_notifier_flush_lock);
static DECLARE_WAIT_QUEUE_HEAD(regulator_notifier_wq_queue);
static int c2ps_regulator_process_mode = 0;
static unsigned int c2ps_remote_monitor_task = 0;
static unsigned int c2ps_remote_monitor_proc_time = 0;
static unsigned int c2ps_remote_monitor_uclamp = 0;

module_param(c2ps_regulator_process_mode, int, 0644);
module_param(c2ps_remote_monitor_task, int, 0644);
module_param(c2ps_remote_monitor_proc_time, int, 0644);
module_param(c2ps_remote_monitor_uclamp, int, 0644);


#define WAIT_FLUSH_START()                              \
	mutex_lock(&regulator_notifier_flush_lock)

#define WAIT_FLUSH_END()                                \
	do {                                                \
		mutex_lock(&regulator_notifier_flush_lock);     \
		mutex_unlock(&regulator_notifier_flush_lock);   \
	} while (0)

#define NOTIFY_WAIT_FLUSH(lock)                         \
	mutex_unlock(lock)

static inline enum c2ps_regulator_mode
decide_process_type(struct regulator_req *req)
{
	if (req->tsk_info)
		return c2ps_regulator_process_mode;
	return C2PS_REGULATOR_BGMODE_SIMPLE;
}

static void regulator_process(struct regulator_req *req)
{
	if (req == NULL)
		return;
	if (req->flush_lock != NULL) {
		NOTIFY_WAIT_FLUSH(req->flush_lock);
		return;
	}

	if (req->tsk_info && c2ps_remote_monitor_task == req->tsk_info->task_id) {
		c2ps_remote_monitor_uclamp = req->tsk_info->latest_uclamp;
		c2ps_remote_monitor_proc_time = req->tsk_info->hist_proc_time_sum /
							proc_time_window_size;;
	}

	switch (decide_process_type(req)) {
		case C2PS_REGULATOR_MODE_FIX:
			c2ps_regulator_policy_fix_uclamp(req);
			break;

		case C2PS_REGULATOR_MODE_SIMPLE:
			c2ps_regulator_policy_simple(req);
			break;

		case C2PS_REGULATOR_MODE_DEBUG:
			c2ps_regulator_policy_debug_uclamp(req);
			break;

		case C2PS_REGULATOR_BGMODE_SIMPLE:
			c2ps_regulator_bgpolicy_simple(req);
			break;

		default:
			break;
	}

	kmem_cache_free(regulator_reqs, req);
}

static int c2ps_regulator_loop(void *arg)
{
	while (!kthread_should_stop())
	{
		struct regulator_req *req = NULL;
		C2PS_LOGD("[C2PS] c2ps_regulator_loop");
		wait_event_interruptible(regulator_notifier_wq_queue,
					 regulator_condition_notifier_wq ||
					 regulator_condition_notifier_exit);

		if (regulator_condition_notifier_exit)
			return 0;
		mutex_lock(&regulator_notifier_wq_lock);

		if (!list_empty(&regulator_wq)) {
			req = list_first_entry(&regulator_wq,
					      struct regulator_req, queue_list);
			list_del(&req->queue_list);
			if (list_empty(&regulator_wq))
				regulator_condition_notifier_wq = 0;
			mutex_unlock(&regulator_notifier_wq_lock);
			regulator_process(req);
		} else {
			regulator_condition_notifier_wq = 0;
			mutex_unlock(&regulator_notifier_wq_lock);
		}
	}
	C2PS_LOGD("c2ps_regulator_loop -\n");
	return 0;
}

// FIXME: should we use conditional variable here?
void send_regulator_req(struct regulator_req *req)
{
	if (req == NULL) {
		C2PS_LOGD("[C2PS] NULL regulator request");
		return;
	}

	mutex_lock(&regulator_notifier_wq_lock);
	list_add_tail(&req->queue_list, &regulator_wq);
	regulator_condition_notifier_wq = 1;
	mutex_unlock(&regulator_notifier_wq_lock);

	wake_up_interruptible(&regulator_notifier_wq_queue);

	return;
}

struct regulator_req* get_regulator_req(void)
{
	struct regulator_req *req;
	req = kmem_cache_alloc(regulator_reqs, GFP_KERNEL | __GFP_ZERO);
	if (!req) {
		C2PS_LOGD("[C2PS] create regulator req failed");
		return NULL;
	}

	return req;
}

int calculate_uclamp_value(struct c2ps_task_info *tsk_info)
{
	C2PS_LOGD("[C2PS] %s", __func__);
	return 0;
}

void c2ps_uclamp_regulator_flush(void)
{
	struct regulator_req *flush_req = get_regulator_req();

	if (flush_req == NULL) {
		C2PS_LOGE("NULL flush_req");
		return;
	}
	flush_req->flush_lock = &regulator_notifier_flush_lock;
	WAIT_FLUSH_START();
	send_regulator_req(flush_req);
	WAIT_FLUSH_END();
}

int uclamp_regulator_init(void)
{
	C2PS_LOGD("[C2PS] %s", __func__);
	regulator_condition_notifier_exit = false;
	c2ps_regulator_thread =
	       kthread_create(c2ps_regulator_loop, NULL, "c2ps_regulator_loop");
	if (c2ps_regulator_thread == NULL)
		return -EFAULT;

	if (regulator_reqs == NULL) {
		regulator_reqs = kmem_cache_create("regulator_reqs",
		  sizeof(struct regulator_req), 0, SLAB_HWCACHE_ALIGN, NULL);
	}

	wake_up_process(c2ps_regulator_thread);
	return 0;
}

void uclamp_regulator_exit(void)
{
	C2PS_LOGD("+\n");
	regulator_condition_notifier_exit = true;

	if (c2ps_regulator_thread)
		kthread_stop(c2ps_regulator_thread);
	c2ps_regulator_thread = NULL;

	if (regulator_reqs)
		kmem_cache_destroy(regulator_reqs);
	regulator_reqs = NULL;

	C2PS_LOGD("-\n");
}

