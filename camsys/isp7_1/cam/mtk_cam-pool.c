// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Louis Kuo <louis.kuo@mediatek.com>
 */

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/mm.h>
#include <linux/remoteproc.h>
#include <linux/spinlock.h>
#include <linux/version.h>

#include "mtk_cam.h"
#include "mtk_cam-smem.h"
#include "mtk_cam-pool.h"
#include "mtk_heap.h"

#ifndef CONFIG_MTK_SCP
#include <linux/platform_data/mtk_ccd.h>
#endif

#define WORKING_BUF_SIZE	round_up(CQ_BUF_SIZE, PAGE_SIZE)
#define MSG_BUF_SIZE		round_up(IPI_FRAME_BUF_SIZE, PAGE_SIZE)

static struct dma_buf *mtk_cam_buffer_alloc_from_heap(const char *heap_name,
						      size_t size)
{
	struct dma_heap *dma_heap;
	struct dma_buf *dmabuf;

	dma_heap = dma_heap_find(heap_name);
	if (!dma_heap) {
		pr_info("failed to find dma heap: %s\n", heap_name);
		return NULL;
	}

	dmabuf = dma_heap_buffer_alloc(dma_heap, size,
				       O_RDWR | O_CLOEXEC,
				       DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR(dmabuf))  {
		pr_info("dma_heap_buffer_alloc failed\n");
		return NULL;
	}

	return dmabuf;
}

struct dma_buf *mtk_cam_cached_buffer_alloc(size_t size)
{
	return mtk_cam_buffer_alloc_from_heap("mtk_mm", size);
}

struct dma_buf *mtk_cam_noncached_buffer_alloc(size_t size)
{
	return mtk_cam_buffer_alloc_from_heap("mtk_mm-uncached", size);
}

static struct dma_buf *_alloc_dma_buf(const char *name,
				      int size, bool cacheable)
{
	struct dma_buf *dbuf;

	if (cacheable)
		dbuf = mtk_cam_cached_buffer_alloc(size);
	else
		dbuf = mtk_cam_noncached_buffer_alloc(size);

	if (!dbuf) {
		pr_info("%s: failed\n", __func__);
		return NULL;
	}

	mtk_dma_buf_set_name(dbuf, name);
	return dbuf;
}

static unsigned long _get_contiguous_size(struct sg_table *sgt)
{
	struct scatterlist *s;
	dma_addr_t expected = sg_dma_address(sgt->sgl);
	unsigned int i;
	unsigned long size = 0;

	for_each_sgtable_dma_sg(sgt, s, i) {
		if (sg_dma_address(s) != expected)
			break;
		expected += sg_dma_len(s);
		size += sg_dma_len(s);
	}
	return size;
}

int mtk_cam_device_buf_init(struct mtk_cam_device_buf *buf,
			    struct dma_buf *dbuf,
			    struct device *dev,
			    size_t expected_size)
{
	unsigned long size;

	memset(buf, 0, sizeof(*buf));

	buf->dbuf = dbuf;
	buf->db_attach = dma_buf_attach(dbuf, dev);
	if (IS_ERR(buf->db_attach)) {
		pr_info("failed to attach dbuf: %s\n", dev_name(dev));
		return -1;
	}
#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
	buf->dma_sgt = dma_buf_map_attachment_unlocked(buf->db_attach,
					      DMA_BIDIRECTIONAL);
#else
	buf->dma_sgt = dma_buf_map_attachment(buf->db_attach,
					      DMA_BIDIRECTIONAL);
#endif
	if (IS_ERR(buf->dma_sgt)) {
		pr_info("failed to map attachment\n");
		goto fail_detach;
	}

	/* check size */
	size = _get_contiguous_size(buf->dma_sgt);
	if (expected_size > size) {
		pr_info(
			 "%s: dma_sgt size(%zu) smaller than expected(%zu)\n",
			 __func__, size, expected_size);
		goto fail_attach_unmap;
	}

	buf->size = expected_size;
	buf->daddr = sg_dma_address(buf->dma_sgt->sgl);

	get_dma_buf(dbuf);

	return 0;

fail_attach_unmap:
#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
	dma_buf_unmap_attachment_unlocked(buf->db_attach, buf->dma_sgt,
				 DMA_BIDIRECTIONAL);
#else
	dma_buf_unmap_attachment(buf->db_attach, buf->dma_sgt,
				 DMA_BIDIRECTIONAL);
#endif
	buf->dma_sgt = NULL;
fail_detach:
	dma_buf_detach(buf->dbuf, buf->db_attach);
	buf->db_attach = NULL;
	buf->dbuf = NULL;
	return -1;
}

void mtk_cam_device_buf_uninit(struct mtk_cam_device_buf *buf)
{
	struct iosys_map map = IOSYS_MAP_INIT_VADDR(buf->vaddr);

	WARN_ON(!buf->dbuf || !buf->size);

	if (buf->dma_sgt) {
#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
	dma_buf_unmap_attachment_unlocked(buf->db_attach, buf->dma_sgt,
				 DMA_BIDIRECTIONAL);
#else
	dma_buf_unmap_attachment(buf->db_attach, buf->dma_sgt,
				 DMA_BIDIRECTIONAL);
#endif
		buf->dma_sgt = NULL;
		buf->daddr = 0;
		buf->size = 0;
	}

	if (buf->vaddr) {
#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
		dma_buf_vunmap_unlocked(buf->dbuf, &map);
#else
		dma_buf_vunmap(buf->dbuf, &map);
#endif
		buf->vaddr = NULL;
	}

	if (buf->db_attach) {
		dma_buf_detach(buf->dbuf, buf->db_attach);
		buf->db_attach = NULL;
	}

	dma_heap_buffer_free(buf->dbuf);
	buf->dbuf = NULL;
}

int mtk_cam_device_buf_vmap(struct mtk_cam_device_buf *buf)
{
	int ret = 0;
	struct iosys_map map = IOSYS_MAP_INIT_VADDR(buf->vaddr);

	WARN_ON(buf->vaddr);
#if KERNEL_VERSION(6, 6, 0) <= LINUX_VERSION_CODE
	ret = dma_buf_vmap_unlocked(buf->dbuf, &map);
#else
	ret = dma_buf_vmap(buf->dbuf, &map);
#endif
	if (!ret)
		buf->vaddr = map.vaddr;

	return ret;
}

static int _alloc_device_buf(const char *name, int total_size, bool cacheable,
			     struct mtk_cam_device_buf *buf,
			     struct device *dev_to_attach)
{
	struct dma_buf *dbuf;
	int ret;

	dbuf = _alloc_dma_buf(name, total_size, cacheable);
	if (!dbuf)
		return -1;

	ret = mtk_cam_device_buf_init(buf, dbuf, dev_to_attach, total_size);
	if (ret)
		return ret;

	ret = mtk_cam_device_buf_vmap(buf);
	if (ret) {
		mtk_cam_device_buf_uninit(buf);
		dma_heap_buffer_free(buf->dbuf);
		return ret;
	}

	dma_heap_buffer_free(buf->dbuf);
	return 0;
}

int mtk_cam_working_buf_pool_init(struct mtk_cam_ctx *ctx)
{
	int i;
	void *working_buf_va;
	void *msg_buf_va;
	dma_addr_t working_buf_iova;
	struct mtk_ccd *ccd;
	int ret = 0;

	ccd = (struct mtk_ccd *)ctx->cam->rproc_handle->priv;

	/* working buffer */
	ret = _alloc_device_buf("CAM_MEM_CQ_ID",
				CAM_CQ_BUF_NUM * WORKING_BUF_SIZE,
				false,
				&ctx->buf_pool.working_device_buf,
				ccd->dev);
	if (ret)
		return ret;
	ctx->buf_pool.working_buf_fd =
		mtk_cam_device_buf_fd(&ctx->buf_pool.working_device_buf);

	/* msg buffer */
	ret = _alloc_device_buf("CAM_MEM_MSG_ID",
				CAM_CQ_BUF_NUM * MSG_BUF_SIZE,
				true,
				&ctx->buf_pool.msg_device_buf,
				ccd->dev);
	if (ret) {
		mtk_cam_device_buf_uninit(&ctx->buf_pool.working_device_buf);
		ctx->buf_pool.working_buf_fd = -1;
		return ret;
	}
	ctx->buf_pool.msg_buf_fd =
		mtk_cam_device_buf_fd(&ctx->buf_pool.msg_device_buf);

	working_buf_va = ctx->buf_pool.working_device_buf.vaddr;
	msg_buf_va = ctx->buf_pool.msg_device_buf.vaddr;
	working_buf_iova = ctx->buf_pool.working_device_buf.daddr;

	INIT_LIST_HEAD(&ctx->buf_pool.cam_freelist.list);
	spin_lock_init(&ctx->buf_pool.cam_freelist.lock);
	ctx->buf_pool.cam_freelist.cnt = 0;

	spin_lock(&ctx->buf_pool.cam_freelist.lock);
	for (i = 0; i < CAM_CQ_BUF_NUM; i++) {
		struct mtk_cam_working_buf_entry *buf = &ctx->buf_pool.working_buf[i];
		int offset, offset_msg;

		buf->ctx = ctx;
		offset = i * WORKING_BUF_SIZE;
		offset_msg = i * MSG_BUF_SIZE;

		buf->buffer.va = working_buf_va + offset;
		buf->buffer.iova = working_buf_iova + offset;
		buf->buffer.size = WORKING_BUF_SIZE;
		buf->msg_buffer.va = msg_buf_va + offset_msg;
		buf->msg_buffer.size = MSG_BUF_SIZE;
		buf->s_data = NULL;

		dev_dbg(ctx->cam->dev, "%s:ctx(%d):buf(%d), cq iova(%pad), msg va(%p)\n",
			__func__, ctx->stream_id, i, &buf->buffer.iova, &buf->msg_buffer.va);

		list_add_tail(&buf->list_entry, &ctx->buf_pool.cam_freelist.list);
		ctx->buf_pool.cam_freelist.cnt++;
	}
	spin_unlock(&ctx->buf_pool.cam_freelist.lock);

	dev_info(ctx->cam->dev,
		"%s: ctx(%d): cq buffers init, freebuf cnt(%d),workingfd(%d),msgfd(%d)\n",
		__func__, ctx->stream_id, ctx->buf_pool.cam_freelist.cnt,
		ctx->buf_pool.working_buf_fd, ctx->buf_pool.msg_buf_fd);

	return 0;
}

void mtk_cam_working_buf_pool_release(struct mtk_cam_ctx *ctx)
{
	/* msg buffer */
	dev_info(ctx->cam->dev,
		"%s:ctx(%d):msg buffers release, msgfd(%d)\n",
		__func__, ctx->stream_id, ctx->buf_pool.msg_buf_fd);
	mtk_cam_device_buf_uninit(&ctx->buf_pool.msg_device_buf);
	ctx->buf_pool.msg_buf_fd = -1;

	/* working buffer */
	dev_info(ctx->cam->dev,
		"%s:ctx(%d):cq buffers release, workingfd(%d)\n",
		__func__, ctx->stream_id, ctx->buf_pool.working_buf_fd);
	mtk_cam_device_buf_uninit(&ctx->buf_pool.working_device_buf);
	ctx->buf_pool.working_buf_fd = -1;
}

void
mtk_cam_working_buf_put(struct mtk_cam_working_buf_entry *buf_entry)
{
	struct mtk_cam_ctx *ctx = buf_entry->ctx;
	int cnt;

	spin_lock(&ctx->buf_pool.cam_freelist.lock);

	list_add_tail(&buf_entry->list_entry,
		      &ctx->buf_pool.cam_freelist.list);
	cnt = ++ctx->buf_pool.cam_freelist.cnt;

	spin_unlock(&ctx->buf_pool.cam_freelist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(%pad), free cnt(%d)\n",
		__func__, ctx->stream_id, &buf_entry->buffer.iova, cnt);
}

struct mtk_cam_working_buf_entry*
mtk_cam_working_buf_get(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_working_buf_entry *buf_entry;
	int cnt;

	/* get from free list */
	spin_lock(&ctx->buf_pool.cam_freelist.lock);
	if (list_empty(&ctx->buf_pool.cam_freelist.list)) {
		spin_unlock(&ctx->buf_pool.cam_freelist.lock);

		dev_info(ctx->cam->dev, "%s:ctx(%d):no free buf\n",
			 __func__, ctx->stream_id);
		return NULL;
	}

	buf_entry = list_first_entry(&ctx->buf_pool.cam_freelist.list,
				     struct mtk_cam_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	cnt = --ctx->buf_pool.cam_freelist.cnt;
	buf_entry->ctx = ctx;

	spin_unlock(&ctx->buf_pool.cam_freelist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(%pad), free cnt(%d)\n",
		__func__, ctx->stream_id, &buf_entry->buffer.iova, cnt);

	return buf_entry;
}

int mtk_cam_img_working_buf_pool_init(struct mtk_cam_ctx *ctx, int buf_num,
									  int working_buf_size)
{
	int i;
	int ret = 0;
	struct mtk_ccd *ccd;

	if (buf_num > CAM_IMG_BUF_NUM) {
		dev_info(ctx->cam->dev,
		"%s: ctx(%d): image buffers number too large(%d)\n",
		__func__, ctx->stream_id, buf_num);
		WARN_ON(1);
		return 0;
	}

	INIT_LIST_HEAD(&ctx->img_buf_pool.cam_freeimglist.list);
	spin_lock_init(&ctx->img_buf_pool.cam_freeimglist.lock);
	ctx->img_buf_pool.cam_freeimglist.cnt = 0;
	ctx->img_buf_pool.working_img_buf_size = buf_num * working_buf_size;
	ccd = (struct mtk_ccd *)ctx->cam->rproc_handle->priv;

	ret = _alloc_device_buf("CAM_MEM_IMG_ID",
				buf_num * working_buf_size,
				false,
				&ctx->img_buf_pool.working_img_device_buf,
				ccd->dev);
	if (ret)
		return ret;

	/* if use mtk_cam_device_buf_fd to get fd, need to handle close */
	ctx->img_buf_pool.working_img_buf_fd = -1;

	spin_lock(&ctx->img_buf_pool.cam_freeimglist.lock);
	for (i = 0; i < buf_num; i++) {
		struct mtk_cam_img_working_buf_entry *buf = &ctx->img_buf_pool.img_working_buf[i];
		int offset;

		offset = i * working_buf_size;

		buf->ctx = ctx;
		buf->img_buffer.va = ctx->img_buf_pool.working_img_device_buf.vaddr + offset;
		buf->img_buffer.iova = ctx->img_buf_pool.working_img_device_buf.daddr + offset;
		buf->img_buffer.size = working_buf_size;
		dev_info(ctx->cam->dev, "%s:ctx(%d):buf(%d), iova(0x%llx)\n",
			__func__, ctx->stream_id, i, buf->img_buffer.iova);

		list_add_tail(&buf->list_entry, &ctx->img_buf_pool.cam_freeimglist.list);
		ctx->img_buf_pool.cam_freeimglist.cnt++;
	}
	spin_unlock(&ctx->img_buf_pool.cam_freeimglist.lock);

	dev_info(ctx->cam->dev,
		 "%s: ctx(%d): image buffers init, freebuf cnt(%d)\n",
		 __func__, ctx->stream_id, ctx->img_buf_pool.cam_freeimglist.cnt);

	return 0;
}

void mtk_cam_img_working_buf_pool_release(struct mtk_cam_ctx *ctx)
{
	dev_info(ctx->cam->dev,
		"%s:ctx(%d):img buffers release, mem iova(0x%llx), sz(%zu)\n",
		__func__, ctx->stream_id,
		ctx->img_buf_pool.working_img_device_buf.daddr,
		ctx->img_buf_pool.working_img_device_buf.size);

	mtk_cam_device_buf_uninit(&ctx->img_buf_pool.working_img_device_buf);
	ctx->img_buf_pool.working_img_buf_fd = -1;

	ctx->img_buf_pool.working_img_buf_size = 0;
}

void mtk_cam_img_working_buf_put(struct mtk_cam_img_working_buf_entry *buf_entry)
{
	struct mtk_cam_ctx *ctx = buf_entry->ctx;
	int cnt;

	spin_lock(&ctx->img_buf_pool.cam_freeimglist.lock);

	list_add_tail(&buf_entry->list_entry,
		      &ctx->img_buf_pool.cam_freeimglist.list);
	cnt = ++ctx->img_buf_pool.cam_freeimglist.cnt;

	spin_unlock(&ctx->img_buf_pool.cam_freeimglist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(0x%llx), free cnt(%d)\n",
		__func__, ctx->stream_id, buf_entry->img_buffer.iova, cnt);
}

struct mtk_cam_img_working_buf_entry*
mtk_cam_img_working_buf_get(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_img_working_buf_entry *buf_entry;
	int cnt;

	/* get from free list */
	spin_lock(&ctx->img_buf_pool.cam_freeimglist.lock);
	if (list_empty(&ctx->img_buf_pool.cam_freeimglist.list)) {
		spin_unlock(&ctx->img_buf_pool.cam_freeimglist.lock);

		dev_info(ctx->cam->dev, "%s:ctx(%d):no free buf\n",
			 __func__, ctx->stream_id);
		return NULL;
	}

	buf_entry = list_first_entry(&ctx->img_buf_pool.cam_freeimglist.list,
				     struct mtk_cam_img_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	cnt = --ctx->img_buf_pool.cam_freeimglist.cnt;

	spin_unlock(&ctx->img_buf_pool.cam_freeimglist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(0x%llx), free cnt(%d)\n",
		__func__, ctx->stream_id, buf_entry->img_buffer.iova, cnt);

	return buf_entry;
}

int mtk_cam_sv_working_buf_pool_init(struct mtk_cam_ctx *ctx)
{
	int i;

	INIT_LIST_HEAD(&ctx->buf_pool.sv_freelist.list);
	spin_lock_init(&ctx->buf_pool.sv_freelist.lock);
	ctx->buf_pool.sv_freelist.cnt = 0;

	for (i = 0; i < CAMSV_WORKING_BUF_NUM; i++) {
		struct mtk_camsv_working_buf_entry *buf = &ctx->buf_pool.sv_working_buf[i];
		buf->ctx = ctx;

		list_add_tail(&buf->list_entry,
			      &ctx->buf_pool.sv_freelist.list);
		ctx->buf_pool.sv_freelist.cnt++;
	}
	dev_info(ctx->cam->dev, "%s:ctx(%d):freebuf cnt(%d)\n", __func__,
		 ctx->stream_id, ctx->buf_pool.sv_freelist.cnt);

	return 0;
}

void
mtk_cam_sv_working_buf_put(struct mtk_camsv_working_buf_entry *buf_entry)
{
	struct mtk_cam_ctx *ctx = buf_entry->ctx;

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):s\n", __func__, ctx->stream_id);

	if (!buf_entry)
		return;

	spin_lock(&ctx->buf_pool.sv_freelist.lock);
	list_add_tail(&buf_entry->list_entry,
		      &ctx->buf_pool.sv_freelist.list);
	ctx->buf_pool.sv_freelist.cnt++;
	spin_unlock(&ctx->buf_pool.sv_freelist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):e\n", __func__, ctx->stream_id);
}

struct mtk_camsv_working_buf_entry*
mtk_cam_sv_working_buf_get(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsv_working_buf_entry *buf_entry;

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):s\n", __func__, ctx->stream_id);

	spin_lock(&ctx->buf_pool.sv_freelist.lock);
	if (list_empty(&ctx->buf_pool.sv_freelist.list)) {
		spin_unlock(&ctx->buf_pool.sv_freelist.lock);
		return NULL;
	}

	buf_entry = list_first_entry(&ctx->buf_pool.sv_freelist.list,
				     struct mtk_camsv_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	ctx->buf_pool.sv_freelist.cnt--;
	spin_unlock(&ctx->buf_pool.sv_freelist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):e\n", __func__, ctx->stream_id);
	return buf_entry;
}

int mtk_cam_mraw_working_buf_pool_init(struct mtk_cam_ctx *ctx)
{
	int i;
	const int working_buf_size = round_up(CQ_BUF_SIZE, PAGE_SIZE);
	void *working_buf_va;
	dma_addr_t working_buf_iova;

	if (!ctx->buf_pool.working_device_buf.vaddr ||
		!ctx->buf_pool.working_device_buf.daddr) {
		dev_info(ctx->cam->dev,
			"%s:ctx(%d):invalid working_device_buf (va=%p, iova=%pad)\n",
			__func__, ctx->stream_id,
			ctx->buf_pool.working_device_buf.vaddr,
			&ctx->buf_pool.working_device_buf.daddr);
		return -EINVAL;
	}

	working_buf_va = ctx->buf_pool.working_device_buf.vaddr;
	working_buf_iova = ctx->buf_pool.working_device_buf.daddr;

	INIT_LIST_HEAD(&ctx->buf_pool.mraw_freelist.list);
	spin_lock_init(&ctx->buf_pool.mraw_freelist.lock);
	ctx->buf_pool.mraw_freelist.cnt = 0;

	for (i = 0; i < MRAW_WORKING_BUF_NUM; i++) {
		struct mtk_mraw_working_buf_entry *buf
				= &ctx->buf_pool.mraw_working_buf[i];
		int offset;

		offset = i * working_buf_size;

		buf->buffer.va = working_buf_va + offset;
		buf->buffer.iova = working_buf_iova + offset;
		buf->buffer.size = working_buf_size;
		buf->s_data = NULL;
		dev_dbg(ctx->cam->dev, "%s:ctx(%d):buf(%d), iova(%pad)\n",
			__func__, ctx->stream_id, i, &buf->buffer.iova);

		list_add_tail(&buf->list_entry,
			      &ctx->buf_pool.mraw_freelist.list);
		ctx->buf_pool.mraw_freelist.cnt++;
	}

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):freebuf cnt(%d)\n", __func__,
		 ctx->stream_id, ctx->buf_pool.mraw_freelist.cnt);

	return 0;
}

void mtk_cam_mraw_working_buf_put(struct mtk_cam_ctx *ctx,
			     struct mtk_mraw_working_buf_entry *buf_entry)
{
	dev_dbg(ctx->cam->dev, "%s:ctx(%d):s\n", __func__, ctx->stream_id);

	if (!buf_entry)
		return;

	spin_lock(&ctx->buf_pool.mraw_freelist.lock);
	list_add_tail(&buf_entry->list_entry,
		      &ctx->buf_pool.mraw_freelist.list);
	ctx->buf_pool.mraw_freelist.cnt++;
	spin_unlock(&ctx->buf_pool.mraw_freelist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):e\n", __func__, ctx->stream_id);
}

struct mtk_mraw_working_buf_entry*
mtk_cam_mraw_working_buf_get(struct mtk_cam_ctx *ctx)
{
	struct mtk_mraw_working_buf_entry *buf_entry;

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):s\n", __func__, ctx->stream_id);

	spin_lock(&ctx->buf_pool.mraw_freelist.lock);
	if (list_empty(&ctx->buf_pool.mraw_freelist.list)) {
		spin_unlock(&ctx->buf_pool.mraw_freelist.lock);
		return NULL;
	}

	buf_entry = list_first_entry(&ctx->buf_pool.mraw_freelist.list,
				     struct mtk_mraw_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	ctx->buf_pool.mraw_freelist.cnt--;
	spin_unlock(&ctx->buf_pool.mraw_freelist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):e\n", __func__, ctx->stream_id);
	return buf_entry;
}
