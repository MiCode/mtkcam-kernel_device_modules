// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/kernel.h>
#include <linux/kmemleak.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/remoteproc.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/slab.h>

#include <uapi/linux/mtk_ccd_controls.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <linux/platform_data/mtk_ccd.h>

#include "rpmsg_internal.h"
#include "mtk_ccd_rpmsg_internal.h"

#undef dev_dbg
#define dev_dbg(dev, fmt, arg...)			\
	do {						\
		if (mtk_ccd_debug_enabled())		\
			dev_info(dev, fmt, ## arg);	\
	} while (0)

static unsigned int ccd_debug;
module_param(ccd_debug, uint, 0644);
MODULE_PARM_DESC(ccd_debug, "ccd debug log");

int mtk_ccd_debug_enabled(void)
{
	return ccd_debug;
}
EXPORT_SYMBOL_GPL(mtk_ccd_debug_enabled);

static const struct rpmsg_endpoint_ops mtk_rpmsg_endpoint_ops;

void __ept_release(struct kref *kref)
{
	struct rpmsg_endpoint *ept = container_of(kref, struct rpmsg_endpoint,
						  refcount);
	struct mtk_ccd_rpmsg_endpoint *mept = to_mtk_rpmsg_endpoint(ept);
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = mept->mtk_subdev;
	struct rpmsg_device *rpdev = ept->rpdev;
	bool clear_rpdev = (ept == rpdev->ept);

	dev_info(&mtk_subdev->pdev->dev, "%s: %p at %d-%d, clear_rpdev:%d\n",
		__func__, mept, mtk_subdev->id, ept->addr, clear_rpdev);
	kfree(to_mtk_rpmsg_endpoint(ept));

	if (clear_rpdev)
		rpdev->ept = NULL;
	put_device(&rpdev->dev);
}

static struct rpmsg_endpoint *
__rpmsg_create_ept(struct mtk_rpmsg_rproc_subdev *mtk_subdev,
		   struct rpmsg_device *rpdev, rpmsg_rx_cb_t cb, void *priv,
		   u32 id)
{
	struct mtk_ccd_rpmsg_endpoint *mept;
	struct rpmsg_endpoint *ept;
	struct platform_device *pdev = mtk_subdev->pdev;

	mept = kzalloc(sizeof(*mept), GFP_KERNEL);
	if (!mept)
		return NULL;

	kmemleak_not_leak(mept);

	mept->mtk_subdev = mtk_subdev;

	ept = &mept->ept;
	kref_init(&ept->refcount);
	mutex_init(&ept->cb_lock);

	get_device(&rpdev->dev);
	ept->rpdev = rpdev;
	ept->cb = cb;
	ept->priv = priv;
	ept->ops = &mtk_rpmsg_endpoint_ops;
	ept->addr = id;  /* channel index */

	INIT_LIST_HEAD(&mept->pending_sendq.queue);
	mutex_init(&mept->pending_sendq.queue_lock);
	init_waitqueue_head(&mept->worker_readwq);
	atomic_set(&mept->ccd_cmd_sent, 0);
	atomic_set(&mept->ccd_mep_state, CCD_MENDPOINT_CREATED);

	if (rpdev->ept)  /* rpdev->ept would be overwritten after this function */
		dev_info(&pdev->dev,
			"%s: prev ept(%p) is alive at %d-%d\n", __func__,
			rpdev->ept, mtk_subdev->id, ept->addr);

	dev_info(&pdev->dev, "%s: %p at %d-%d\n", __func__,
		mept, mtk_subdev->id, ept->addr);
	return ept;
}

static struct rpmsg_endpoint *
mtk_rpmsg_create_ept(struct rpmsg_device *rpdev, rpmsg_rx_cb_t cb, void *priv,
		     struct rpmsg_channel_info chinfo)
{
	struct mtk_rpmsg_device *mdev = to_mtk_rpmsg_device(rpdev);
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = mdev->mtk_subdev;
	struct rpmsg_endpoint *ept =
		__rpmsg_create_ept(mtk_subdev, rpdev, cb, priv, mdev->id);

	if (!ept)
		return NULL;

	return ept;
}

static void mtk_rpmsg_destroy_ept(struct rpmsg_endpoint *ept)
{
	struct mtk_ccd_params *ccd_params;
	struct mtk_ccd_rpmsg_endpoint *mept = to_mtk_rpmsg_endpoint(ept);
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = mept->mtk_subdev;

	atomic_set(&mept->ccd_mep_state, CCD_MENDPOINT_DESTROY);
	wake_up(&mept->worker_readwq);

	while (atomic_read(&mept->ccd_cmd_sent) > 0) {
		dev_info(&mtk_subdev->pdev->dev,
			"%s: %p at %d-%d cmd_sent: %d\n",
			__func__, mept, mtk_subdev->id, ept->addr,
			atomic_read(&mept->ccd_cmd_sent));

		mutex_lock(&mept->pending_sendq.queue_lock);
		ccd_params = list_first_entry(&mept->pending_sendq.queue,
					      struct mtk_ccd_params,
					      list_entry);
		list_del(&ccd_params->list_entry);
		mutex_unlock(&mept->pending_sendq.queue_lock);

		atomic_dec(&mept->ccd_cmd_sent);

		/* Directly call callback to return */
		mutex_lock(&ept->cb_lock);
		if (ept->cb)
			ept->cb(ept->rpdev,
				ccd_params->worker_obj.sbuf,
				ccd_params->worker_obj.len,
				ept->priv, ept->addr);

		mutex_unlock(&ept->cb_lock);

		/* free ccd_param */
		kfree(ccd_params);
	}

	/* make sure new inbound messages can't find this ept anymore */
	mutex_lock(&mtk_subdev->endpoints_lock);
	kref_put(&ept->refcount, __ept_release);
	mutex_unlock(&mtk_subdev->endpoints_lock);
}

static int mtk_rpmsg_send(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct mtk_ccd_rpmsg_endpoint *mept = to_mtk_rpmsg_endpoint(ept);
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = mept->mtk_subdev;

	return mtk_subdev->ops->ccd_send(mtk_subdev, mept, data, len, 0);
}

static int mtk_rpmsg_trysend(struct rpmsg_endpoint *ept, void *data, int len)
{
	struct mtk_ccd_rpmsg_endpoint *mept = to_mtk_rpmsg_endpoint(ept);
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = mept->mtk_subdev;

	/*
	 * TODO: This currently is same as mtk_rpmsg_send, and wait until SCP
	 * received the last command.
	 */
	return mtk_subdev->ops->ccd_send(mtk_subdev, mept, data, len, 0);
}

static const struct rpmsg_endpoint_ops mtk_rpmsg_endpoint_ops = {
	.destroy_ept = mtk_rpmsg_destroy_ept,
	.send = mtk_rpmsg_send,
	.trysend = mtk_rpmsg_trysend,
};

static void mtk_rpmsg_release_device(struct device *dev)
{
	struct rpmsg_device *rpdev = to_rpmsg_device(dev);
	struct mtk_rpmsg_device *mdev = to_mtk_rpmsg_device(rpdev);
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = mdev->mtk_subdev;

	dev_info(dev, "%s: %#x %p at %d-%d\n", __func__,
		 rpdev->src, mdev, mtk_subdev->id, mdev->id);

	kfree(mdev);
}

static const struct rpmsg_device_ops mtk_rpmsg_device_ops = {
	.create_ept = mtk_rpmsg_create_ept,
};

static int set_rpmsg_channel_info(struct rpmsg_channel_info *msg,
				  int center_id, int ipi_id /* channel_id */)
{
	unsigned int msg_id;

	msg_id = (center_id << 16) + ipi_id;

	memset(msg, 0, sizeof(msg));
	msg->src = msg_id;
	(void) snprintf(msg->name, RPMSG_NAME_SIZE,
			"mtk-camsys-\%x-%x", center_id, ipi_id);

	return 0;
}

static void mtk_rpmsg_destroy_rpmsgdev(struct mtk_rpmsg_rproc_subdev *mtk_subdev,
				       struct rpmsg_channel_info *info,
				       unsigned int id)
{
	struct device *dev;
	struct mtk_rpmsg_device *mdev;

	dev = &mtk_subdev->pdev->dev;

	mdev = mtk_subdev->channels[id];
	if (!mdev) {
		dev_info(dev, "%s: no channel at %d-%d\n", __func__,
			mtk_subdev->id, id);
		return;
	}

	mutex_lock(&mtk_subdev->endpoints_lock);
	mtk_subdev->channels[id] = NULL;
	mutex_unlock(&mtk_subdev->endpoints_lock);

	if (rpmsg_unregister_device(&mtk_subdev->pdev->dev, info))
		dev_info(dev, "%s: failed, %s at %d-%d\n", __func__, info->name,
			mtk_subdev->id, id);

	if (mdev->rpdev.ept) {
		WARN_ON(1);
		dev_info(dev, "%s: ept leakage at %d-%d\n", __func__,
			mtk_subdev->id, id);
		rpmsg_destroy_ept(mdev->rpdev.ept);
	}

	dev_info(dev, "%s: %s at %d-%d\n", __func__,
		info->name, mtk_subdev->id, id);
}

void
mtk_ccd_center_destroy_channels(struct rproc_subdev *subdev)
{
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = to_mtk_subdev(subdev);
	struct rpmsg_channel_info msg;
	u32 ipi_id, center_id;

	center_id = mtk_subdev->id;

	/* destroy rpmsg device */
	for (ipi_id = CCD_IPI_INIT; ipi_id < CCD_IPI_MAX; ipi_id++) {
		if (!mtk_subdev->channels[ipi_id])
			continue;

		set_rpmsg_channel_info(&msg, center_id, ipi_id);
		mtk_rpmsg_destroy_rpmsgdev(mtk_subdev, &msg, ipi_id);
	}
}
EXPORT_SYMBOL_GPL(mtk_ccd_center_destroy_channels);

static struct mtk_rpmsg_device *
mtk_rpmsg_create_rpmsgdev(struct mtk_rpmsg_rproc_subdev *mtk_subdev,
			  struct rpmsg_channel_info *info,
			  unsigned int id)
{
	struct rpmsg_device *rpdev;
	struct mtk_rpmsg_device *mdev;
	struct platform_device *pdev = mtk_subdev->pdev;
	int ret = 0;

	mdev = kzalloc(sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return NULL;

	mdev->mtk_subdev = mtk_subdev;
	mdev->channel_cb = NULL;
	mdev->id = id;

	rpdev = &mdev->rpdev;
	rpdev->src = info->src;
	rpdev->dst = info->dst;
	rpdev->ops = &mtk_rpmsg_device_ops;

	/* for rpmsg_find_device */
	strncpy(rpdev->id.name, info->name, RPMSG_NAME_SIZE);

	/* If ccd hw ready, register rpmsg_device/rpmsg_driver here */

	rpdev->dev.parent = &pdev->dev;
	rpdev->dev.release = mtk_rpmsg_release_device;
	/* TODO: check relese flow, it is skipped currently */

	ret = rpmsg_register_device(rpdev);  /* add to parent */
	if (ret) {
		kfree(mdev);
		return NULL;
	}

	mutex_lock(&mtk_subdev->endpoints_lock);
	/* probe state, maybe no need lock */
	mtk_subdev->channels[id] = mdev;  /* MAIN - MRAW */
	mutex_unlock(&mtk_subdev->endpoints_lock);

	dev_info(&pdev->dev, "%s: %#x %p at %d-%d\n", __func__,
		 rpdev->src, mdev, mtk_subdev->id, id);

	return mdev;
}

void
mtk_ccd_center_create_channels(struct rproc_subdev *subdev)
{
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = to_mtk_subdev(subdev);
	struct rpmsg_channel_info msg;
	u32 ipi_id, center_id;

	center_id = mtk_subdev->id;

	/* create client rpmsg device */
	for (ipi_id = CCD_IPI_INIT; ipi_id < CCD_IPI_MAX; ipi_id++) {
		set_rpmsg_channel_info(&msg, center_id, ipi_id);
		if (mtk_rpmsg_create_rpmsgdev(mtk_subdev, &msg, ipi_id) == NULL)
			dev_info(&mtk_subdev->pdev->dev, "%s: %s failed\n",
				__func__, msg.name);
	}
}
EXPORT_SYMBOL_GPL(mtk_ccd_center_create_channels);

/* For ccd client */
int mtk_ccd_get_channel_center_id(struct mtk_ccd *ccd)
{
	struct device *dev;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = NULL;
	pid_t curr_pid;
	int i;

	dev = ccd->dev;
	curr_pid = current->tgid;

	for (i = 0; i < MAX_RPROC_SUBDEV_NUM; i++) {
		mtk_subdev = to_mtk_subdev(ccd->channel_center[i]);
		if (mtk_subdev->process_id == curr_pid) {  /* matched master */
			dev_dbg(dev, "%s %d for %d", __func__, i, curr_pid);
			return i;
		}
	}

	dev_info(dev, "%s for %d failed", __func__, curr_pid);

	return -1;
}

int mtk_ccd_get_channel(struct mtk_ccd *ccd, unsigned int center_id,
			struct mtk_ccd_client_cb *client_cb)
{
	struct device *dev;
	struct rproc_subdev *subdev;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	struct mtk_rpmsg_device *mdev;
	struct rpmsg_channel_info msg;
	int channel_id;

	dev = ccd->dev;
	subdev = ccd->channel_center[center_id];
	if (!subdev) {
		dev_info(dev, "%s center-%d is not ready\n", __func__, center_id);
		return -1;
	}
	mtk_subdev = to_mtk_subdev(subdev);

	/* get channel */
	channel_id = client_cb->ipi_id;
	if (channel_id < 0) {
		dev_info(dev, "%s invalid channel id\n", __func__);
		return -1;
	}

	mdev = mtk_subdev->channels[channel_id];
	if (!mdev) {
		dev_info(dev, "%s channel-%d-%d is not ready\n", __func__,
			center_id, channel_id);
		return -1;
	}
	if (mdev->channel_cb) {
		dev_info(dev, "%s channel-%d-%d is occupied\n", __func__,
			center_id, channel_id);
		WARN_ON(1);
	}
	mdev->channel_cb = client_cb;

	/* create ept */
	set_rpmsg_channel_info(&msg, center_id, channel_id);
	mdev->rpdev.ept = rpmsg_create_ept(&mdev->rpdev,
					   mdev->channel_cb->send_msg_ack,
					   mdev->channel_cb->priv,
					   msg);
	if (IS_ERR(mdev->rpdev.ept)) {
		dev_info(dev, "%s failed rpmsg_create_ept, channel-%d-%d\n",
			__func__, center_id, channel_id);
		mdev->channel_cb = NULL;
		return -1;
	}

	dev_dbg(dev, "%s channel-%d-%d", __func__, center_id, channel_id);
	return channel_id;
}

int mtk_ccd_put_channel(struct mtk_ccd *ccd,
			unsigned int center_id, unsigned int channel_id)
{
	struct device *dev;
	struct rproc_subdev *subdev;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	struct mtk_rpmsg_device *mdev;

	dev = ccd->dev;
	subdev = ccd->channel_center[center_id];
	if (!subdev) {
		dev_info(dev, "%s center-%d is not ready\n", __func__, center_id);
		return -1;
	}
	mtk_subdev = to_mtk_subdev(subdev);

	mdev = mtk_subdev->channels[channel_id];
	if (!mdev) {
		dev_info(dev, "%s channel-%d-%d is not ready\n",
			__func__, center_id, channel_id);
		return -1;
	}

	/* destroy ept */
	if (mdev->rpdev.ept) {
		rpmsg_destroy_ept(mdev->rpdev.ept);
	} else {
		dev_info(dev, "%s channel-%d-%d ept is destroyed\n",
			__func__, center_id, channel_id);
		WARN_ON(1);
	}

	/* put channel */
	mdev->channel_cb = NULL;

	dev_dbg(dev, "%s channel-%d-%d", __func__, center_id, channel_id);
	return 0;
}

int mtk_ccd_channel_init(struct mtk_ccd *ccd,
			 unsigned int center_id, unsigned int channel_id)
{
	struct device *dev;
	struct rproc_subdev *subdev;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	struct mtk_rpmsg_device *mdev;
	int ret;
	u32 listen_obj_rdy;

	dev = ccd->dev;
	subdev = ccd->channel_center[center_id];
	if (!subdev) {
		dev_info(dev, "%s center-%d is not ready\n", __func__, center_id);
		return -1;
	}
	mtk_subdev = to_mtk_subdev(subdev);

	mdev = mtk_subdev->channels[channel_id];
	if (!mdev) {
		dev_info(dev, "%s channel-%d-%d is not ready\n",
			__func__, center_id, channel_id);
		return -1;
	}

	/* start worker */
	dev_dbg(dev, "%s channel-%d-%d +", __func__, center_id, channel_id);

	mutex_lock(&mtk_subdev->master_listen_lock);

	listen_obj_rdy = atomic_read(&mtk_subdev->listen_obj_rdy);
	if (listen_obj_rdy == CCD_LISTEN_OBJECT_READY) {
		mutex_unlock(&mtk_subdev->master_listen_lock);

		ret = wait_event_interruptible_timeout
			(mtk_subdev->ccd_listen_wq,
			 (atomic_read(&mtk_subdev->listen_obj_rdy) ==
			 CCD_LISTEN_OBJECT_PREPARING),
			 msecs_to_jiffies(2000));
		if (ret == 0) {
			dev_info(dev, "%s wait timeout, master-%d might be killed\n",
				__func__, center_id);
			return -1;
		} else if (ret < 0) {
			dev_info(dev, "%s is being canceled %d, master-%d might be killed\n",
				 __func__, ret, center_id);
			return -1;
		}

		mutex_lock(&mtk_subdev->master_listen_lock);
	}

	memcpy(mtk_subdev->listen_obj.name, mdev->rpdev.id.name, RPMSG_NAME_SIZE);
	mtk_subdev->listen_obj.src = mdev->id;
	mtk_subdev->listen_obj.cmd = CCD_MASTER_CMD_CREATE;

	atomic_set(&mtk_subdev->listen_obj_rdy, CCD_LISTEN_OBJECT_READY);
	wake_up(&mtk_subdev->master_listen_wq);

	mutex_unlock(&mtk_subdev->master_listen_lock);

	dev_dbg(dev, "%s channel-%d-%d -", __func__, center_id, channel_id);
	return 0;
}

int mtk_ccd_channel_uninit(struct mtk_ccd *ccd,
			   unsigned int center_id, unsigned int channel_id)
{
	struct device *dev;
	struct rproc_subdev *subdev;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	struct mtk_rpmsg_device *mdev;
	int ret;
	u32 listen_obj_rdy;

	if (channel_id >= CCD_IPI_MAX) {
		pr_err("%s, invalid channel ID", __func__);
		return -1;
	}

	dev = ccd->dev;
	subdev = ccd->channel_center[center_id];
	if (!subdev) {
		dev_info(dev, "%s center-%d is not ready\n", __func__, center_id);
		return -1;
	}
	mtk_subdev = to_mtk_subdev(subdev);

	mdev = mtk_subdev->channels[channel_id];
	if (!mdev) {
		dev_info(dev, "%s channel-%d-%d is not ready\n",
			__func__, center_id, channel_id);
		return -1;
	}

	/* stop worker */
	dev_dbg(dev, "%s channel-%d-%d +", __func__, center_id, channel_id);

	mutex_lock(&mtk_subdev->master_listen_lock);

	listen_obj_rdy = atomic_read(&mtk_subdev->listen_obj_rdy);
	if (listen_obj_rdy == CCD_LISTEN_OBJECT_READY) {
		mutex_unlock(&mtk_subdev->master_listen_lock);

		ret = wait_event_interruptible_timeout
			(mtk_subdev->ccd_listen_wq,
			 (atomic_read(&mtk_subdev->listen_obj_rdy) ==
			 CCD_LISTEN_OBJECT_PREPARING),
			 msecs_to_jiffies(2000));
		if (ret == 0) {
			dev_info(dev, "%s wait timeout, master-%d might be killed\n",
				__func__, center_id);
			return -1;
		} else if (ret < 0) {
			dev_info(dev, "%s is being canceled %d, master-%d might be killed\n",
				 __func__, ret, center_id);
			return -1;
		}

		mutex_lock(&mtk_subdev->master_listen_lock);
	}

	memcpy(mtk_subdev->listen_obj.name, mdev->rpdev.id.name, RPMSG_NAME_SIZE);
	mtk_subdev->listen_obj.src = mdev->id;
	mtk_subdev->listen_obj.cmd = CCD_MASTER_CMD_DESTROY;

	atomic_set(&mtk_subdev->listen_obj_rdy, CCD_LISTEN_OBJECT_READY);
	wake_up(&mtk_subdev->master_listen_wq);

	mutex_unlock(&mtk_subdev->master_listen_lock);

	dev_dbg(dev, "%s channel-%d-%d -", __func__, center_id, channel_id);
	return 0;
}

int mtk_ccd_channel_send(struct mtk_ccd *ccd,
			 unsigned int center_id, unsigned int channel_id,
			 void *data, int len)
{
	struct device *dev;
	struct rproc_subdev *subdev;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	struct mtk_rpmsg_device *mdev;

	if (channel_id >= CCD_IPI_MAX) {
		pr_info("%s invalid channel ID\n", __func__);
		return -1;
	}

	dev = ccd->dev;
	subdev = ccd->channel_center[center_id];
	if (!subdev) {
		dev_info(dev, "%s center-%d is not ready\n", __func__, center_id);
		return -1;
	}
	mtk_subdev = to_mtk_subdev(subdev);

	/* TODO: get mtk endpoint(ccd, cneter_id, channel_id) */
	mdev = mtk_subdev->channels[channel_id];
	if (!mdev) {
		dev_info(dev, "%s channel-%d-%d is not ready\n",
			__func__, center_id, channel_id);
		return -1;
	}

	if (!mdev->rpdev.ept) {
		dev_info(dev, "%s failed, channel-%d-%d",
			__func__, center_id, channel_id);
		return -1;
	}

	rpmsg_send(mdev->rpdev.ept, data, len);

	return 0;
}

/* rproc_subdev */
static void _prepare_ccd_param_pool(struct mtk_ccd_params_pool *pool)
{
	int i;
	struct mtk_ccd_params *ccd_params;

	mutex_init(&pool->lock);
	INIT_LIST_HEAD(&pool->queue);
	pool->cnt = 0;

	for (i = 0; i < 64; i++) {
		ccd_params = kzalloc(sizeof(*ccd_params), GFP_KERNEL);
		if (!ccd_params)
			continue;

		mutex_lock(&pool->lock);
		list_add_tail(&ccd_params->list_entry, &pool->queue);
		pool->cnt++;
		mutex_unlock(&pool->lock);

		if(pool->cnt >= MAX_CCD_PARAM_NUM)
			break;
	}

	pr_info("%s cnt:%d", __func__, pool->cnt);
}

static void _unprepare_ccd_param_pool(struct mtk_ccd_params_pool *pool)
{
	struct mtk_ccd_params *ccd_params;

	mutex_lock(&pool->lock);
	while (!list_empty(&pool->queue)) {
		ccd_params = list_first_entry_or_null(&pool->queue,
						      struct mtk_ccd_params,
						      list_entry);
		if (ccd_params != NULL) {
			list_del(&ccd_params->list_entry);
			kfree(ccd_params);
			pool->cnt--;
		} else {
			pr_err("%s: list_first_entry_or_null returned NULL", __func__);
			break;
		}
	}
	mutex_unlock(&pool->lock);

	pr_info("%s cnt:%d", __func__, pool->cnt);
}

struct rproc_subdev *
mtk_rpmsg_create_rproc_subdev(struct platform_device *pdev,
			      struct mtk_ccd_rpmsg_ops *ops,
			      unsigned int id)
{
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	int i;

	mtk_subdev = kzalloc(sizeof(*mtk_subdev), GFP_KERNEL);
	if (!mtk_subdev)
		return NULL;

	kmemleak_not_leak(mtk_subdev);

	mtk_subdev->pdev = pdev;
	mtk_subdev->ops = ops;
	mtk_subdev->id = id;
	mtk_subdev->master_status = CCD_MASTER_INIT;
	mtk_subdev->process_id = -1;

	mutex_init(&mtk_subdev->endpoints_lock);

	mutex_init(&mtk_subdev->master_listen_lock);
	atomic_set(&mtk_subdev->listen_obj_rdy, CCD_LISTEN_OBJECT_PREPARING);
	init_waitqueue_head(&mtk_subdev->master_listen_wq);
	init_waitqueue_head(&mtk_subdev->ccd_listen_wq);

	_prepare_ccd_param_pool(&mtk_subdev->ccd_params_pool);

	/* channels initialization */
	for (i = 0; i < CCD_IPI_MAX; i++)
		mtk_subdev->channels[i] = NULL;

	return &mtk_subdev->subdev;
}
EXPORT_SYMBOL_GPL(mtk_rpmsg_create_rproc_subdev);

void mtk_rpmsg_destroy_rproc_subdev(struct rproc_subdev *subdev)
{
	struct mtk_rpmsg_rproc_subdev *mtk_subdev = to_mtk_subdev(subdev);
	int i;

	/* channels check */
	for (i = 0; i < CCD_IPI_MAX; i++)
		if (mtk_subdev->channels[i]) {
			pr_info("%s: channel-%d-%d is not released\n", __func__,
				mtk_subdev->id, i);
			WARN_ON(1);
		}

	_unprepare_ccd_param_pool(&mtk_subdev->ccd_params_pool);

	kfree(mtk_subdev);
}
EXPORT_SYMBOL_GPL(mtk_rpmsg_destroy_rproc_subdev);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek ccd rpmsg driver");
