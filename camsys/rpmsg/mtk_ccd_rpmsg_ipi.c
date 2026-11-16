// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <uapi/linux/mtk_ccd_controls.h>
#include <linux/platform_data/mtk_ccd.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>

#include "mtk_ccd_rpmsg_internal.h"

#undef dev_dbg
#define dev_dbg(dev, fmt, arg...)			\
	do {						\
		if (mtk_ccd_debug_enabled())		\
			dev_info(dev, fmt, ## arg);	\
	} while (0)

static struct mtk_rpmsg_rproc_subdev *
get_mtk_subdev_by_pid(struct mtk_ccd *ccd, pid_t curr_pid)
{
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = NULL;
	int i;

	for (i = 0; i < MAX_RPROC_SUBDEV_NUM; i++) {
		mtk_subdev = to_mtk_subdev(ccd->channel_center[i]);
		if (mtk_subdev->process_id == curr_pid)
			return mtk_subdev;
	}

	return NULL;
}

static struct mtk_ccd_params *
_get_ccd_params(struct mtk_ccd_params_pool *pool)
{
	struct mtk_ccd_params *ccd_params;

	mutex_lock(&pool->lock);
	ccd_params = list_first_entry_or_null(&pool->queue,
					      struct mtk_ccd_params,
					      list_entry);
	if (ccd_params) {
		list_del(&ccd_params->list_entry);
		pool->cnt--;
	}
	mutex_unlock(&pool->lock);

	if (!ccd_params) {
		pr_info("%s create ccd_params", __func__);
		ccd_params = kzalloc(sizeof(*ccd_params), GFP_KERNEL);
	}

	return ccd_params;
}

static void _return_ccd_params(struct mtk_ccd_params_pool *pool,
			       struct mtk_ccd_params *ccd_params)
{
	mutex_lock(&pool->lock);
	if (pool->cnt < MAX_CCD_PARAM_NUM) {
		list_add_tail(&ccd_params->list_entry, &pool->queue);
		pool->cnt++;
		ccd_params = NULL;
	}
	mutex_unlock(&pool->lock);

	if (ccd_params) {
		pr_info("%s delete ccd_params", __func__);
		kfree(ccd_params);
		ccd_params = NULL;
	}
}

int rpmsg_ccd_ipi_send(struct mtk_rpmsg_rproc_subdev *mtk_subdev,
		       struct mtk_ccd_rpmsg_endpoint *mept,
		       void *buf, unsigned int len, unsigned int wait)
{
	int ret = 0;
	struct device *dev;
	struct mtk_ccd *ccd = platform_get_drvdata(mtk_subdev->pdev);
	struct mtk_ccd_params *ccd_params;

	ccd_params = _get_ccd_params(&mtk_subdev->ccd_params_pool);
	if (!ccd_params)
		return -ENOMEM;

	dev = ccd->dev;
	ccd_params->worker_obj.src = mept->ept.addr;
	ccd_params->worker_obj.id = mept->ept.addr;

	/* TODO: Allocate shared memory for additional buffer
	 * If no buffer ready now, wait or not depending on parameter
	 */

	if (len)
		memcpy(ccd_params->worker_obj.sbuf, buf, len);

	ccd_params->worker_obj.len = len;

	/* No need to use spin_lock_irqsave for all non-irq context */
	mutex_lock(&mept->pending_sendq.queue_lock);
	list_add_tail(&ccd_params->list_entry, &mept->pending_sendq.queue);
	mutex_unlock(&mept->pending_sendq.queue_lock);

	atomic_inc(&mept->ccd_cmd_sent);

	wake_up(&mept->worker_readwq);

	dev_dbg(dev, "%s: channel-%d-%d mpet:%p\n",
		__func__, mtk_subdev->id, mept->ept.addr, mept);

	return ret;
}
EXPORT_SYMBOL_GPL(rpmsg_ccd_ipi_send);

int ccd_master_init(struct mtk_ccd *ccd)
{
	struct device *dev;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	pid_t curr_pid;
	int ret = -1, i;

	dev = ccd->dev;
	curr_pid = current->tgid;

	/* find free ipi_data_center */
	for (i = 0; i < MAX_RPROC_SUBDEV_NUM; i++) {
		mtk_subdev = to_mtk_subdev(ccd->channel_center[i]);
		if (mtk_subdev->process_id < 0) {  /* available */
			mtk_subdev->process_id = curr_pid;
			mtk_subdev->master_status = CCD_MASTER_ACTIVE;
			memset(&mtk_subdev->listen_obj, 0,
			       sizeof(mtk_subdev->listen_obj));
			atomic_set(&mtk_subdev->listen_obj_rdy,
				   CCD_LISTEN_OBJECT_PREPARING);
			ret = 0;
			break;
		}
	}

	if (ret)
		dev_info(dev, "%s no free ipi_data_center for %d",
			__func__, curr_pid);
	else
		dev_info(dev, "%s ipi_data_center for %d is ready, at %d",
			__func__, curr_pid, i);

	return ret;  /* -1: no master */
}
EXPORT_SYMBOL_GPL(ccd_master_init);

int ccd_master_destroy(struct mtk_ccd *ccd)
{
	struct device *dev;
	struct mtk_rpmsg_device *srcmdev;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	pid_t curr_pid;
	int channel_id;

	dev = ccd->dev;
	curr_pid = current->tgid;

	mtk_subdev = get_mtk_subdev_by_pid(ccd, curr_pid);
	if(!mtk_subdev) {
		dev_info(dev, "%s, no channel center for %d\n", __func__, curr_pid);
		return -1;
	}

	/* use the src addr to fetch the callback of the appropriate user */
	mutex_lock(&mtk_subdev->endpoints_lock);
	for (channel_id = 0; channel_id < CCD_IPI_MAX; channel_id++) {
		srcmdev = mtk_subdev->channels[channel_id];
		if (mtk_subdev->channels[channel_id] == NULL)
			continue;

		/**
		 * master service killed during streming,
		 * let client handle its ept by themself
		 */
		if (srcmdev->rpdev.ept) {
			/* hint client to stop */
			dev_info(dev, "%s, channel-%d-%d is still streaming\n",
				__func__, mtk_subdev->id, channel_id);

			/* srcmdev->channel_cb->master_destroy() */
			/* lock used, must call API after the loop */
		}
	}
	mutex_unlock(&mtk_subdev->endpoints_lock);

	dev_info(dev, "%s %d at %d", __func__,
		mtk_subdev->process_id, mtk_subdev->id);

	mtk_subdev->process_id = -1;
	mtk_subdev->master_status = CCD_MASTER_INIT;

	return 0;
}
EXPORT_SYMBOL_GPL(ccd_master_destroy);

int ccd_master_listen(struct mtk_ccd *ccd,
		      struct ccd_master_listen_item *listen_obj)
{
	int ret;
	u32 listen_obj_rdy;
	struct device *dev;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	pid_t curr_pid;

	dev = ccd->dev;
	curr_pid = current->tgid;

	mtk_subdev = get_mtk_subdev_by_pid(ccd, curr_pid);
	if(!mtk_subdev) {
		dev_info(dev, "%s, no channel center for %d\n", __func__, curr_pid);
		return -1;
	}

	mutex_lock(&mtk_subdev->master_listen_lock);

	listen_obj_rdy = atomic_read(&mtk_subdev->listen_obj_rdy);
	if (listen_obj_rdy == CCD_LISTEN_OBJECT_PREPARING) {
		mutex_unlock(&mtk_subdev->master_listen_lock);
		ret = wait_event_interruptible
			(mtk_subdev->master_listen_wq,
			 (atomic_read(&mtk_subdev->listen_obj_rdy) ==
			 CCD_LISTEN_OBJECT_READY));
		if (ret != 0) {
			dev_info(dev,
				"master-%d is killed, listen wait error: %d\n",
				mtk_subdev->id, ret);
			/* hint client to stop */
			/* srcmdev->channel_cb->master_destroy() */
			return -1;
		}
		mutex_lock(&mtk_subdev->master_listen_lock);
	}

	/* Could be memory copied directly */
	memcpy(listen_obj->name, mtk_subdev->listen_obj.name,
	       sizeof(listen_obj->name));
	listen_obj->src = mtk_subdev->listen_obj.src;
	listen_obj->cmd = mtk_subdev->listen_obj.cmd;

	atomic_set(&mtk_subdev->listen_obj_rdy, CCD_LISTEN_OBJECT_PREPARING);
	wake_up(&mtk_subdev->ccd_listen_wq);
	mutex_unlock(&mtk_subdev->master_listen_lock);

	dev_info(dev, "%s, channel-%d-%d\n", __func__,
		mtk_subdev->id, mtk_subdev->listen_obj.src);
	return 0;
}
EXPORT_SYMBOL_GPL(ccd_master_listen);

int ccd_worker_read(struct mtk_ccd *ccd, struct ccd_worker_item *read_obj)
{
	int ret;
	struct device *dev;
	struct mtk_ccd_params *ccd_params;
	struct mtk_rpmsg_device *srcmdev;
	struct mtk_ccd_rpmsg_endpoint *mept;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	pid_t curr_pid;

	if (!read_obj || read_obj->src >= CCD_IPI_MAX) {
		ret = -EFAULT;
		return -1;
	}

	dev = ccd->dev;
	curr_pid = current->tgid;

	mtk_subdev = get_mtk_subdev_by_pid(ccd, curr_pid);
	if(!mtk_subdev) {
		dev_info(dev, "%s, no channel center for %d\n", __func__, curr_pid);
		return -1;
	}

	/* use the src addr to fetch the callback of the appropriate user */
	mutex_lock(&mtk_subdev->endpoints_lock);
	srcmdev = mtk_subdev->channels[read_obj->src];
	if (!srcmdev) {
		dev_info(dev, "%s channel-%d-%d is not ready\n", __func__,
			mtk_subdev->id, read_obj->src);
		mutex_unlock(&mtk_subdev->endpoints_lock);
		return 0;
	}
	get_device(&srcmdev->rpdev.dev);

	if (!srcmdev->rpdev.ept) {  /* ept should be created before worker start */
		dev_info(dev, "%s channel-%d-%d ept is not ready\n", __func__,
			mtk_subdev->id, read_obj->src);
		mutex_unlock(&mtk_subdev->endpoints_lock);
		goto err_put;
	}
	kref_get(&srcmdev->rpdev.ept->refcount);
	mutex_unlock(&mtk_subdev->endpoints_lock);

	mept = to_mtk_rpmsg_endpoint(srcmdev->rpdev.ept);

	if (atomic_read(&mept->ccd_mep_state) == CCD_MENDPOINT_DESTROY) {
		dev_info(dev, "%s channel-%d-%d mept:%p is destroyed\n",
			__func__, mtk_subdev->id, read_obj->src, mept);
		goto err_ret;
	}

	dev_dbg(dev, "%s, channel-%d-%d, mept: %p\n", __func__,
		mtk_subdev->id, read_obj->src, mept);

	ret = wait_event_interruptible
		(mept->worker_readwq,
		 (atomic_read(&mept->ccd_cmd_sent) > 0) ||
		 (atomic_read(&mept->ccd_mep_state) != CCD_MENDPOINT_CREATED));
	if (ret != 0) {
		dev_info(dev,
			"worker service-%d-%d is killed, read wait error: %d\n",
			mtk_subdev->id, read_obj->src, ret);
		/* hint client to stop */
		/* srcmdev->channel_cb->worker_destroy() */
		goto err_ret;
	}

	if (atomic_read(&mept->ccd_mep_state) == CCD_MENDPOINT_DESTROY) {
		dev_info(dev, "%s channel-%d-%d mept:%p is destroyed after wait\n",
			__func__, mtk_subdev->id, read_obj->src, mept);
		goto err_ret;
	}

	if (atomic_read(&mept->ccd_cmd_sent) <= 0) {
		dev_info(ccd->dev, "%s warn. no cmd pending on channel-%d-%d\n",
			__func__, mtk_subdev->id, read_obj->src);
		goto err_ret;
	}

	mutex_lock(&mept->pending_sendq.queue_lock);
	ccd_params = list_first_entry_or_null(&mept->pending_sendq.queue,
					      struct mtk_ccd_params,
					      list_entry);
	if (ccd_params != NULL)
		list_del(&ccd_params->list_entry);
	mutex_unlock(&mept->pending_sendq.queue_lock);

	if (ccd_params != NULL) {
		atomic_dec(&mept->ccd_cmd_sent);
		memcpy(read_obj, &ccd_params->worker_obj, sizeof(*read_obj));
		_return_ccd_params(&mtk_subdev->ccd_params_pool, ccd_params);
	} else {
		dev_info(ccd->dev, "warn. ccd_params is null\n");
	}

err_ret:
	kref_put(&mept->ept.refcount, __ept_release);
err_put:
	put_device(&srcmdev->rpdev.dev);
	return 0;
}
EXPORT_SYMBOL_GPL(ccd_worker_read);

int ccd_worker_write(struct mtk_ccd *ccd, struct ccd_worker_item *write_obj)
{
	struct device *dev;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	struct rpmsg_endpoint *ept;
	struct mtk_rpmsg_device *srcmdev;
	struct mtk_ccd_rpmsg_endpoint *mept;
	pid_t curr_pid;

	if (!write_obj || write_obj->src >= CCD_IPI_MAX)
		return -1;

	dev = ccd->dev;
	curr_pid = current->tgid;

	mtk_subdev = get_mtk_subdev_by_pid(ccd, curr_pid);
	if(!mtk_subdev) {
		dev_info(dev, "%s, no channel center for %d\n", __func__, curr_pid);
		return -1;
	}

	mutex_lock(&mtk_subdev->endpoints_lock);
	srcmdev = mtk_subdev->channels[write_obj->src];
	if (!srcmdev) {
		dev_info(dev, "%s channel-%d-%d is not ready\n", __func__,
			mtk_subdev->id, write_obj->src);
		mutex_unlock(&mtk_subdev->endpoints_lock);
		return -1;
	}
	get_device(&srcmdev->rpdev.dev);

	if (!srcmdev->rpdev.ept) {
		dev_info(dev, "%s channel-%d-%d ept is not ready\n", __func__,
			mtk_subdev->id, write_obj->src);
		mutex_unlock(&mtk_subdev->endpoints_lock);
		goto err_put;
	}
	kref_get(&srcmdev->rpdev.ept->refcount);
	mutex_unlock(&mtk_subdev->endpoints_lock);

	mept = to_mtk_rpmsg_endpoint(srcmdev->rpdev.ept);

	if (atomic_read(&mept->ccd_mep_state) == CCD_MENDPOINT_DESTROY) {
		dev_info(dev, "%s channel-%d-%d mept:%p is destroyed\n",
			__func__, mtk_subdev->id, write_obj->src, mept);
		goto err_ret;
	}

	ept = srcmdev->rpdev.ept;

	dev_dbg(dev, "%s, channel-%d-%d, mept: %p\n", __func__,
		mtk_subdev->id, write_obj->src, mept);

	mutex_lock(&ept->cb_lock);

	if (ept->cb)
		ept->cb(ept->rpdev, write_obj->sbuf, write_obj->len, ept->priv,
			write_obj->src);

	mutex_unlock(&ept->cb_lock);

err_ret:
	kref_put(&mept->ept.refcount, __ept_release);
err_put:
	put_device(&srcmdev->rpdev.dev);
	/* TBD: Free shared memory for additional buffer
	 * If no buffer ready now, wait or not depending on parameter
	 */
	return 0;
}
EXPORT_SYMBOL_GPL(ccd_worker_write);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek ccd IPI interface");
