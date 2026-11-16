/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef _MTK_CCD_H
#define _MTK_CCD_H

#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/rpmsg.h>

#define MAX_RPROC_SUBDEV_NUM 2

struct dma_buf;
struct mtk_ccd_memory;

struct mtk_ccd_client_cb {
	/**
	 * FIXME: phase out id for channel optimization
	 */
	int ipi_id;
	rpmsg_rx_cb_t send_msg_ack;
	void *priv; /* point the client top struct. */
	/* int (*master_destroy)(); */
	/* int (*worker_destroy)(); */
};

/**
 * struct mem_obj - memory buffer allocated in kernel
 *
 * @iova:	iova of buffer
 * @len:	buffer length
 * @va: kernel virtual address
 */
struct mem_obj {
	dma_addr_t iova;
	unsigned int len;
	void *va;
};

struct mtk_ccd {
	struct device *dev;
	struct device *smmu_dev;
	struct rproc *rproc;

	dev_t ccd_devno;
	struct cdev ccd_cdev;
	struct class *ccd_class;

	struct rproc_subdev *channel_center[MAX_RPROC_SUBDEV_NUM];
	struct mtk_ccd_memory *ccd_memory;

	atomic_t open_cnt;
};

int mtk_ccd_client_start(struct mtk_ccd *ccd);
int mtk_ccd_client_stop(struct mtk_ccd *ccd);
int mtk_ccd_client_get_channel(struct mtk_ccd *ccd, struct mtk_ccd_client_cb *cb);
int mtk_ccd_client_put_channel(struct mtk_ccd *ccd, int id_mask);
int mtk_ccd_client_msg_send(struct mtk_ccd *ccd, int id_mask, void *data, int len);

/* ccd memory */
void *mtk_ccd_get_buffer(struct mtk_ccd *ccd,
			 struct mem_obj *mem_buff_data);
int mtk_ccd_put_buffer(struct mtk_ccd *ccd,
			struct mem_obj *mem_buff_data);

int mtk_ccd_get_buffer_fd(struct mtk_ccd *ccd, void *mem_priv);
int mtk_ccd_put_buffer_fd(struct mtk_ccd *ccd,
			struct mem_obj *mem_buff_data,
			unsigned int target_fd);

struct dma_buf *mtk_ccd_get_buffer_dmabuf(struct mtk_ccd *ccd,
			void *mem_priv);

#endif /* _MTK_CCD_H */
