/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __LINUX_RPMSG_MTK_RPMSG_H
#define __LINUX_RPMSG_MTK_RPMSG_H

#include <linux/device.h>
#include <linux/remoteproc.h>
#include <linux/rpmsg.h>

#include <uapi/linux/mtk_ccd_controls.h>

#define NAME_MAX_LEN			(32)
#define MAX_CCD_PARAM_NUM 4

struct mtk_ccd;
struct mtk_ccd_rpmsg_endpoint;
struct mtk_ccd_client_cb;

typedef void (*ipi_handler_t)(void *data, unsigned int len, void *priv);

struct mtk_ccd_listen_item {
	u32 src;
	char name[NAME_MAX_LEN];
	unsigned int cmd;
};

struct mtk_rpmsg_device {
	struct rpmsg_device rpdev;
	struct mtk_rpmsg_rproc_subdev *mtk_subdev;
	struct mtk_ccd_client_cb *channel_cb;
	unsigned int id; /* channels index */
};

struct mtk_ccd_params_pool {
	struct list_head queue;
	unsigned int cnt;
	struct mutex lock;
};

struct mtk_rpmsg_rproc_subdev {
	unsigned int id;  /* center_id, assign at creation */
	struct platform_device *pdev;
	struct mtk_ccd_rpmsg_ops *ops;
	struct rproc_subdev subdev;
	struct mtk_rpmsg_device *channels[CCD_IPI_MAX];
	struct mutex endpoints_lock;

	struct mutex master_listen_lock;
	struct mtk_ccd_listen_item listen_obj;
	wait_queue_head_t master_listen_wq;
	wait_queue_head_t ccd_listen_wq;
	atomic_t listen_obj_rdy;

	struct mtk_ccd_params_pool ccd_params_pool;

	unsigned int master_status;
	pid_t process_id;
};

#define to_mtk_subdev(d) container_of(d, struct mtk_rpmsg_rproc_subdev, subdev)
#define to_mtk_rpmsg_device(r) container_of(r, struct mtk_rpmsg_device, rpdev)

struct mtk_ccd_rpmsg_ops {
	int (*ccd_send)(struct mtk_rpmsg_rproc_subdev *mtk_subdev,
			struct mtk_ccd_rpmsg_endpoint *mept,
			void *buf, unsigned int len, unsigned int wait);
};

/* For ccd */
void mtk_ccd_center_create_channels(struct rproc_subdev *subdev);
void mtk_ccd_center_destroy_channels(struct rproc_subdev *subdev);

struct rproc_subdev *
mtk_rpmsg_create_rproc_subdev(struct platform_device *pdev,
			      struct mtk_ccd_rpmsg_ops *ops,
			      unsigned int id);
void mtk_rpmsg_destroy_rproc_subdev(struct rproc_subdev *subdev);

int rpmsg_ccd_ipi_send(struct mtk_rpmsg_rproc_subdev *mtk_subdev,
		       struct mtk_ccd_rpmsg_endpoint *mept,
		       void *buf, unsigned int len, unsigned int wait);

int ccd_master_init(struct mtk_ccd *ccd);
int ccd_master_destroy(struct mtk_ccd *ccd);
int ccd_master_listen(struct mtk_ccd *ccd,
		      struct ccd_master_listen_item *listen_obj);

int ccd_worker_read(struct mtk_ccd *ccd, struct ccd_worker_item *read_obj);
int ccd_worker_write(struct mtk_ccd *ccd, struct ccd_worker_item *write_obj);

/* debug log */
int mtk_ccd_debug_enabled(void);

#endif
