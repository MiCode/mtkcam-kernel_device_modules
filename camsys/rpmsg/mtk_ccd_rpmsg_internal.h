/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_RPMSG_INTERNAL_H__
#define __MTK_RPMSG_INTERNAL_H__

#include <linux/device.h>
#include <linux/remoteproc.h>
#include <linux/rpmsg.h>

struct mtk_ccd;
struct mtk_ccd_client_cb;
struct ccd_worker_item;

enum ccd_mept_state {
	CCD_MENDPOINT_CREATED = 1,
	CCD_MENDPOINT_DESTROY
};

struct mtk_ccd_queue {
	struct list_head queue;
	struct mutex queue_lock; /* Protect queue operation */
};

struct mtk_ccd_params {
	struct list_head list_entry;
	struct ccd_worker_item worker_obj;
};

struct mtk_ccd_rpmsg_endpoint {
	struct rpmsg_endpoint ept;  /* ept->addr is the channel id */
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	wait_queue_head_t worker_readwq;
	struct mtk_ccd_queue pending_sendq;
	atomic_t ccd_cmd_sent;	/* Should be 0, 1, ..., N */
	atomic_t ccd_mep_state;	/* enum ccd_mept_state */
};

#define to_mtk_rpmsg_endpoint(r) \
	container_of(r, struct mtk_ccd_rpmsg_endpoint, ept)

void __ept_release(struct kref *kref);

/* For ccd client */
int mtk_ccd_get_channel_center_id(struct mtk_ccd *ccd);

int mtk_ccd_get_channel(struct mtk_ccd *ccd, unsigned int center_id,
			struct mtk_ccd_client_cb *client_cb);
int mtk_ccd_put_channel(struct mtk_ccd *ccd,
			unsigned int center_id, unsigned int channel_id);

int mtk_ccd_channel_init(struct mtk_ccd *ccd,
			 unsigned int center_id, unsigned int channel_id);
int mtk_ccd_channel_uninit(struct mtk_ccd *ccd,
			   unsigned int center_id, unsigned int channel_id);

int mtk_ccd_channel_send(struct mtk_ccd *ccd,
			 unsigned int center_id, unsigned int channel_id,
			 void *data, int len);

#endif
