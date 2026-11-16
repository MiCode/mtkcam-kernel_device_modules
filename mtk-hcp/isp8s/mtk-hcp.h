/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MTK_HCP_H
#define MTK_HCP_H

#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/dma-heap.h>
#include <linux/fdtable.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <uapi/linux/dma-heap.h>
#include <linux/types.h>

#include "mtk-img-ipi.h"

#include "mtk-hcp_interface.h"


/**
 * HCP (Hetero Control Processor ) is a tiny processor controlling
 * the methodology of register programming. If the module support
 * to run on CM4 then it will send data to CM4 to program register.
 * Or it will send the data to user library and let RED to program
 * register.
 *
 **/

/**
 * struct flush_buf_info - DMA buffer need to partial flush
 *
 *
 * @id:             hcp id
 * @len:            share buffer length
 * @share_buf:      share buffer data
 */
struct flush_buf_info {
	unsigned int fd;
	unsigned int offset;
	unsigned int len;
	unsigned int mode;
	bool is_tuning;
	struct dma_buf *dbuf;
};

/**
 * mtk_hcp_kernel_log_clear(struct platform_device *pdev)
 *
 * This function is used to clear IMG_KERNEL buffer.
 * Will be invoked everytime streamon.
 *
 **/
int mtk_hcp_kernel_log_clear(struct platform_device *pdev);

/**
 * ssize_t mtk_img_kernel_write(struct platform_device *pdev, const char *fmt, ...)
 *
 * Write log to kernel DB for AEE dump
 *
 **/
ssize_t mtk_img_kernel_write(struct platform_device *pdev, const char *fmt, ...);

#endif /* _MTK_HCP_H */
