// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <asm/cacheflush.h>
#include <linux/cdev.h>
#include <linux/compat.h>
#include <linux/compiler.h>
#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/dma-heap.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/firmware.h>
#include <linux/freezer.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include "linux/device.h"
#include "linux/platform_device.h"
#include <linux/pm_runtime.h>
#include <linux/proc_fs.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <linux/version.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-memops.h>
#include <media/videobuf2-v4l2.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>

#include <uapi/linux/dma-heap.h>

#include <aee.h>
#include <mtk_heap.h>

#ifdef CONFIG_MTK_IOMMU_V2
#include "mtk_iommu_ext.h"
#elif defined(CONFIG_DEVICE_MODULES_MTK_IOMMU)
#include "mach/mt_iommu.h"
#elif defined(CONFIG_MTK_M4U)
#include "m4u.h"
#endif

#ifdef CONFIG_MTK_M4U
#include "m4u.h"
#include "m4u_port.h"
#endif

#include "slbc_ops.h"
#include "iommu_debug.h"

#include "mtk-hcp.h"
#include "mtk-hcp_aee.h"
#include "mtk-hcp_interface.h"
#include "mtk-hcp_log.h"

/* platform ops header */
#include "platform/mtk-hcp_data.h"
#include "platform/mtk-hcp_isp8s_plat_f.h"
#include "platform/mtk-hcp_isp8s_plat_p.h"

/**
 * HCP (Hetero Control Processor ) is a tiny processor controlling
 * the methodology of register programming. If the module support
 * to run on CM4 then it will send data to CM4 to program register.
 * Or it will send the data to user library and let RED to program
 * register.
 *
 **/

#define HCP_DEVNAME             "mtk_hcp"
#define HCP_TIMEOUT_MS          (4000U)
#define SYNC_SEND               (1)
#define ASYNC_SEND              (0)
#define IPI_MAX_BUFFER_COUNT    (8)
#define HCP_MSG_NUM_MAX         (96)
#define HCP_SHARE_BUF_SIZE      (1500)


#define HCP_INIT                _IOWR('H', 0, struct share_buf)
#define HCP_GET_OBJECT          _IOWR('H', 1, struct share_buf)
#define HCP_NOTIFY              _IOWR('H', 2, struct share_buf)
#define HCP_COMPLETE            _IOWR('H', 3, struct share_buf)
#define HCP_WAKEUP              _IOWR('H', 4, struct share_buf)
#define HCP_TIMEOUT             _IO('H', 5)

#if IS_ENABLED(CONFIG_COMPAT)
#define COMPAT_HCP_INIT         _IOWR('H', 0, struct share_buf)
#define COMPAT_HCP_GET_OBJECT   _IOWR('H', 1, struct share_buf)
#define COMPAT_HCP_NOTIFY       _IOWR('H', 2, struct share_buf)
#define COMPAT_HCP_COMPLETE     _IOWR('H', 3, struct share_buf)
#define COMPAT_HCP_WAKEUP       _IOWR('H', 4, struct share_buf)
#define COMPAT_HCP_TIMEOUT      _IO('H', 5)
#endif


/**
 * module ID definition
 */
enum module_id {
	MODULE_ISP = 0,
	MODULE_DIP,
	MODULE_IMG = MODULE_DIP,
	MODULE_FD,
	MODULE_RSC,
	MODULE_MAX_ID,
};

/**
 * struct hcp_desc - hcp descriptor
 *
 * @handler:      IPI handler
 * @name:         the name of IPI handler
 * @priv:         the private data of IPI handler
 */
struct hcp_desc {
	hcp_handler_t handler;
	const char *name;
	void *priv;
};

struct object_id {
	union {
		struct send {
			uint32_t hcp: 5;
			uint32_t ack: 1;
			uint32_t req: 10;
			uint32_t seq: 16;
		} __packed send;

		uint32_t cmd;
	};
} __packed;

/**
 * struct share_buf - DTCM (Data Tightly-Coupled Memory) buffer shared with
 *                    RED and HCP
 *
 * @id:             hcp id
 * @len:            share buffer length
 * @share_buf:      share buffer data
 */
struct share_buf {
	uint32_t id;
	uint32_t len;
	uint8_t share_data[HCP_SHARE_BUF_SIZE];
	struct object_id info;
};

/**
 * struct mtk_hcp - hcp driver data
 * @hcp_desc:            hcp descriptor
 * @dev:                 hcp struct device
 * @mem_ops:             memory operations
 * @hcp_mutex:           protect mtk_hcp (except recv_buf) and ensure only
 *                       one client to use hcp service at a time.
 * @data_mutex:          protect shared buffer between kernel user send and
 *                       user thread get&read/copy
 * @file:                hcp daemon file pointer
 * @is_open:             the flag to indicate if hcp device is open.
 * @ack_wq:              the wait queue for each client. When sleeping
 *                       processes wake up, they will check the condition
 *                       "hcp_id_ack" to run the corresponding action or
 *                       go back to sleep.
 * @hcp_id_ack:          The ACKs for registered HCP function.
 * @ipi_got:             The flags for IPI message polling from user.
 * @ipi_done:            The flags for IPI message polling from user again,
 *       which means the previous messages has been dispatched
 *                       done in daemon.
 * @user_obj:            Temporary share_buf used for hcp_msg_get.
 * @dev_no:              The device number of hcp character device
 * @hcp_cdev:            The point of hcp character device.
 * @hcp_class:           The class_create for create hcp device
 * @hcp_device:          hcp struct device
 * @ cm4_support_list    to indicate which module can run in cm4 or it will send
 *                       to user space for running action.
 * @ current_task        hcp current task struct
 */
struct mtk_hcp {
	struct hcp_desc hcp_desc_table[HCP_MAX_ID];
	atomic_t hcp_id_ack[HCP_MAX_ID];
	wait_queue_head_t ack_wq[MODULE_MAX_ID];
	wait_queue_head_t poll_wq[MODULE_MAX_ID];
	struct list_head chans[MODULE_MAX_ID];
	struct device *dev;
	struct device *smmu_dev;
	struct device *acp_dev;
	const struct vb2_mem_ops *mem_ops;
	const struct mtk_hcp_plat_op *plat_op;
	/* for protecting vcu data structure */
	bool   is_open;
	atomic_t seq;
	struct list_head msg_list;
	spinlock_t msglock;
	wait_queue_head_t msg_wq;
	dev_t dev_no;
	struct cdev hcp_cdev;
	struct class *hcp_class;
	struct device *hcp_device;
	struct task_struct *current_task;
	struct hcp_aee_info aee_info;
};

struct packet {
	int32_t module;
	bool more;
	int32_t count;
	struct share_buf *buffer[IPI_MAX_BUFFER_COUNT];
};

struct msg {
	struct list_head entry;
	struct share_buf user_obj;
};

struct mtk_hcp_vb2_buf {
	struct device			*dev;
	void				*vaddr;
	unsigned long			size;
	void				*cookie;
	dma_addr_t			dma_addr;
	unsigned long			attrs;
	enum dma_data_direction		dma_dir;
	struct sg_table			*dma_sgt;
	struct frame_vector		*vec;

	/* MMAP related */
	struct vb2_vmarea_handler	handler;
	refcount_t			refcount;
	struct sg_table			*sgt_base;

	/* DMABUF related */
	struct dma_buf_attachment	*db_attach;

	struct vb2_buffer		*vb;
	bool				non_coherent_mem;
};

struct imgsys_hcp_mb_init_cfg {
	uint32_t size;
	uint8_t  coherent_mode;
	uint8_t  reserved[3];
} __packed;

struct imgsys_hcp_init_info {
	struct imgsys_hcp_mb_init_cfg mod_mb_cfg
		[IMGSYS_MEMORY_MODE_NUM_MAX][IMGSYS_MOD_DRV_NUM_MAX][IMGSYS_MODULE_WORKING_BUF_TYPE_NUM_MAX];
	struct imgsys_hcp_mb_init_cfg gce_mb_cfg[IMGSYS_MEMORY_MODE_NUM_MAX];
	struct imgsys_hcp_mb_init_cfg gce_clr_token_cfg[IMGSYS_MEMORY_MODE_NUM_MAX];
	uint8_t dbg_lvl;
	uint8_t reserved[3];
} __packed;

static struct mtk_hcp *hcp_mtkdev;

/*********************************************/
/*         callbacks for all buffers         */
/*********************************************/

static unsigned long mtk_hcp_vb2_get_contiguous_size(struct sg_table *sgt)
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

static void *mtk_hcp_vb2_cookie(struct vb2_buffer *vb, void *buf_priv)
{
	struct mtk_hcp_vb2_buf *buf = buf_priv;

	return &buf->dma_addr;
}

static void *mtk_hcp_vb2_vaddr(struct vb2_buffer *vb, void *buf_priv)
{
	struct mtk_hcp_vb2_buf *buf = buf_priv;

	if (buf->vaddr)
		return buf->vaddr;

	if (buf->db_attach) {
		struct iosys_map map;

		#if KERNEL_VERSION(6, 4, 0) <= LINUX_VERSION_CODE
		if (!dma_buf_vmap_unlocked(buf->db_attach->dmabuf, &map))
			buf->vaddr = map.vaddr;
		#else
		if (!dma_buf_vmap(buf->db_attach->dmabuf, &map))
			buf->vaddr = map.vaddr;
		#endif
		return buf->vaddr;
	}

	if (buf->non_coherent_mem)
		buf->vaddr = dma_vmap_noncontiguous(buf->dev, buf->size,
						    buf->dma_sgt);
	return buf->vaddr;
}

static void mtk_hcp_vb2_prepare(void *buf_priv)
{
	struct mtk_hcp_vb2_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	/* This takes care of DMABUF and user-enforced cache sync hint */
	if (buf->vb->skip_cache_sync_on_prepare)
		return;

	if (!buf->non_coherent_mem)
		return;

	/* Non-coherent MMAP only */
	if (buf->vaddr)
		flush_kernel_vmap_range(buf->vaddr, buf->size);

	/* For both USERPTR and non-coherent MMAP */
	dma_sync_sgtable_for_device(buf->dev, sgt, buf->dma_dir);
}

static void mtk_hcp_vb2_finish(void *buf_priv)
{
	struct mtk_hcp_vb2_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	/* This takes care of DMABUF and user-enforced cache sync hint */
	if (buf->vb->skip_cache_sync_on_finish)
		return;

	if (!buf->non_coherent_mem)
		return;

	/* Non-coherent MMAP only */
	if (buf->vaddr)
		invalidate_kernel_vmap_range(buf->vaddr, buf->size);

	/* For both USERPTR and non-coherent MMAP */
	dma_sync_sgtable_for_cpu(buf->dev, sgt, buf->dma_dir);
}

static int mtk_hcp_vb2_map_dmabuf(void *mem_priv)
{
	struct mtk_hcp_vb2_buf *buf = mem_priv;
	struct sg_table *sgt;
	unsigned long contig_size;

	if (WARN_ON(!buf->db_attach)) {
		pr_err("trying to pin a non attached buffer\n");
		return -EINVAL;
	}

	if (WARN_ON(buf->dma_sgt)) {
		pr_err("dmabuf buffer is already pinned\n");
		return 0;
	}

	/* get the associated scatterlist for this buffer */
	#if KERNEL_VERSION(6, 4, 0) <= LINUX_VERSION_CODE
	sgt = dma_buf_map_attachment_unlocked(buf->db_attach, buf->dma_dir);
	#else
	sgt = dma_buf_map_attachment(buf->db_attach, buf->dma_dir);
	#endif
	if (IS_ERR(sgt)) {
		pr_err("Error getting dmabuf scatterlist\n");
		return -EINVAL;
	}

	/* checking if dmabuf is big enough to store contiguous chunk */
	contig_size = mtk_hcp_vb2_get_contiguous_size(sgt);
	if (contig_size < buf->size) {
		pr_err("contiguous chunk is too small %lu/%lu\n",
		       contig_size, buf->size);
		#if KERNEL_VERSION(6, 4, 0) <= LINUX_VERSION_CODE
		dma_buf_unmap_attachment_unlocked(buf->db_attach, sgt, buf->dma_dir);
		#else
		dma_buf_unmap_attachment(buf->db_attach, sgt, buf->dma_dir);
		#endif
		return -EFAULT;
	}

	buf->dma_addr = sg_dma_address(sgt->sgl);
	buf->dma_sgt = sgt;
	buf->vaddr = NULL;

	return 0;
}

static void mtk_hcp_vb2_unmap_dmabuf(void *mem_priv)
{
	struct mtk_hcp_vb2_buf *buf = mem_priv;
	struct sg_table *sgt = buf->dma_sgt;
	struct iosys_map map = IOSYS_MAP_INIT_VADDR(buf->vaddr);

	if (WARN_ON(!buf->db_attach)) {
		pr_err("trying to unpin a not attached buffer\n");
		return;
	}

	if (WARN_ON(!sgt)) {
		pr_err("dmabuf buffer is already unpinned\n");
		return;
	}

	if (buf->vaddr) {
		#if KERNEL_VERSION(6, 4, 0) <= LINUX_VERSION_CODE
		dma_buf_vunmap_unlocked(buf->db_attach->dmabuf, &map);
		#else
		dma_buf_vunmap(buf->db_attach->dmabuf, &map);
		#endif
		buf->vaddr = NULL;
	}
	#if KERNEL_VERSION(6, 4, 0) <= LINUX_VERSION_CODE
	dma_buf_unmap_attachment_unlocked(buf->db_attach, sgt, buf->dma_dir);
	#else
	dma_buf_unmap_attachment(buf->db_attach, sgt, buf->dma_dir);
	#endif

	buf->dma_addr = 0;
	buf->dma_sgt = NULL;
}

static void *mtk_hcp_vb2_attach_dmabuf(struct vb2_buffer *vb, struct device *dev,
				  struct dma_buf *dbuf, unsigned long size)
{
	struct mtk_hcp_vb2_buf *buf;
	struct dma_buf_attachment *dba;

	if (dbuf->size < size)
		return ERR_PTR(-EFAULT);

	if (WARN_ON(!dev))
		return ERR_PTR(-EINVAL);

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dev = dev;
	buf->vb = vb;

	/* create attachment for the dmabuf with the user device */
	dba = dma_buf_attach(dbuf, buf->dev);
	if (IS_ERR(dba)) {
		pr_err("failed to attach dmabuf\n");
		kfree(buf);
		return dba;
	}

	buf->dma_dir = vb->vb2_queue->dma_dir;
	buf->size = size;
	buf->db_attach = dba;

	return buf;
}

static void mtk_hcp_vb2_detach_dmabuf(void *mem_priv)
{
	struct mtk_hcp_vb2_buf *buf = mem_priv;

	/* if vb2 works correctly you should never detach mapped buffer */
	if (WARN_ON(buf->dma_addr))
		mtk_hcp_vb2_unmap_dmabuf(buf);

	/* detach this attachment */
	dma_buf_detach(buf->db_attach->dmabuf, buf->db_attach);
	kfree(buf);
}

static unsigned int mtk_hcp_vb2_num_users(void *buf_priv)
{
	struct mtk_hcp_vb2_buf *buf = buf_priv;

	return refcount_read(&buf->refcount);
}

/*hcp vb2 dma config ops*/
static const struct vb2_mem_ops mtk_hcp_dma_contig_memops = {
	/* .alloc		= , */
	/* .put		= , */
	/* .get_dmabuf	= , */
	.cookie		= mtk_hcp_vb2_cookie,
	.vaddr		= mtk_hcp_vb2_vaddr,
	/* .mmap		= , */
	/* .get_userptr	= , */
	/* .put_userptr	= , */
	.prepare	= mtk_hcp_vb2_prepare,
	.finish		= mtk_hcp_vb2_finish,
	.map_dmabuf	= mtk_hcp_vb2_map_dmabuf,
	.unmap_dmabuf	= mtk_hcp_vb2_unmap_dmabuf,
	.attach_dmabuf	= mtk_hcp_vb2_attach_dmabuf,
	.detach_dmabuf	= mtk_hcp_vb2_detach_dmabuf,
	.num_users	= mtk_hcp_vb2_num_users,
};
/* End */

/* plat ops */
static inline void set_plat_op(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = NULL;

	if (unlikely(!pdev)) {
		HCP_PRINT_ERR("pdev is NULL!\n");
		return;
	}

	hcp_dev = platform_get_drvdata(pdev);
	if (unlikely(!hcp_dev)) {
		HCP_PRINT_ERR("hcp_dev is NULL!\n");
		return;
	}

	hcp_dev->plat_op = of_device_get_match_data(&pdev->dev);
	/* hcp_dev->plat_op = &plat_op_isp8s_f; */

	HCP_PRINT_DBG("set plat_op successfully!\n");
}

static inline const struct mtk_hcp_plat_op *fetch_plat_op(
	struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = NULL;

	if (unlikely(!pdev)) {
		HCP_PRINT_ERR("pdev is NULL!\n");
		return NULL;
	}

	hcp_dev = platform_get_drvdata(pdev);
	if (unlikely(!hcp_dev)) {
		HCP_PRINT_ERR("hcp_dev is NULL!\n");
		return NULL;
	}

	return hcp_dev->plat_op;
}

/* communication */
static struct msg *msg_pool_get(struct mtk_hcp *hcp_dev)
{
	unsigned long flag = 0;
	unsigned long empty = 0;
	struct msg *msg = NULL;

	spin_lock_irqsave(&hcp_dev->msglock, flag);
	empty = list_empty(&hcp_dev->msg_list);
	if (!empty) {
		msg = list_first_entry(&hcp_dev->msg_list, struct msg, entry);
		list_del(&msg->entry);
	}
	spin_unlock_irqrestore(&hcp_dev->msglock, flag);

	return msg;
}

static void chans_pool_dump(struct mtk_hcp *hcp_dev)
{
	unsigned long flag = 0;
	struct msg *msg = NULL;
	struct msg *tmp = NULL;
	int i = 0;
	int seq_id = 0;
	int req_fd = 0;
	int hcp_id = 0;

	spin_lock_irqsave(&hcp_dev->msglock, flag);
	for (i = 0; i < MODULE_MAX_ID; i++) {
		HCP_PRINT_WRN("HCP(%d) stalled IPI object+\n", i);

		list_for_each_entry_safe(
			msg,
			tmp,
			&hcp_dev->chans[i],
			entry) {

			seq_id = msg->user_obj.info.send.seq;
			req_fd = msg->user_obj.info.send.req;
			hcp_id = msg->user_obj.info.send.hcp;

			HCP_PRINT_WRN("req_fd(%d), seq_id(%d), hcp_id(%d)\n",
				req_fd, seq_id, hcp_id);

		}

		dev_info(hcp_dev->dev, "HCP(%d) stalled IPI object-\n", i);
	}
	spin_unlock_irqrestore(&hcp_dev->msglock, flag);
}

static struct msg *chan_pool_get(
	struct mtk_hcp *hcp_dev,
	unsigned int module_id)
{
	unsigned long flag = 0;
	int empty = 0;
	struct msg *msg = NULL;

	spin_lock_irqsave(&hcp_dev->msglock, flag);
	empty = list_empty(&hcp_dev->chans[module_id]);
	if (!empty) {
		msg = list_first_entry(&hcp_dev->chans[module_id], struct msg, entry);
		list_del(&msg->entry);
	}
	empty = list_empty(&hcp_dev->chans[module_id]);
	spin_unlock_irqrestore(&hcp_dev->msglock, flag);

	HCP_PRINT_DBG("chan pool empty(%d)\n", empty);

	return msg;
}

static bool chan_pool_available(struct mtk_hcp *hcp_dev, int module_id)
{
	unsigned long flag = 0;
	unsigned long empty = 0;

	spin_lock_irqsave(&hcp_dev->msglock, flag);
	empty = list_empty(&hcp_dev->chans[module_id]);
	spin_unlock_irqrestore(&hcp_dev->msglock, flag);

	HCP_PRINT_DBG("chan pool available(%d)\n", !empty);

	return (!empty);
}


static inline int hcp_id_to_module_id(struct mtk_hcp *hcp_dev, enum hcp_id id)
{
	int module_id = -EINVAL;

	switch (id) {
	case HCP_ISP_CMD_ID:
	case HCP_ISP_FRAME_ID:
		module_id = MODULE_ISP;
		break;
	case HCP_IMGSYS_INIT_ID:
	case HCP_IMGSYS_FRAME_ID:
	case HCP_IMGSYS_HW_TIMEOUT_ID:
	case HCP_IMGSYS_SW_TIMEOUT_ID:
	case HCP_IMGSYS_DEQUE_DUMP_ID:
	case HCP_IMGSYS_DEQUE_DONE_ID:
	case HCP_IMGSYS_ASYNC_DEQUE_DONE_ID:
	case HCP_IMGSYS_DEINIT_ID:
	case HCP_IMGSYS_IOVA_FDS_ADD_ID:
	case HCP_IMGSYS_IOVA_FDS_DEL_ID:
	case HCP_IMGSYS_UVA_FDS_ADD_ID:
	case HCP_IMGSYS_UVA_FDS_DEL_ID:
	case HCP_IMGSYS_SET_CONTROL_ID:
	case HCP_IMGSYS_GET_CONTROL_ID:
	case HCP_IMGSYS_ALLOC_WORKING_BUF_ID:
	case HCP_IMGSYS_FREE_WORKING_BUF_ID:
		module_id = MODULE_DIP;
		break;
	default:
		break;
	}

	HCP_PRINT_DBG("hcp_id(%u) -> module_id:%d\n", (unsigned int)id, module_id);

	return module_id;
}

static inline bool mtk_hcp_running(struct mtk_hcp *hcp_dev)
{
	return hcp_dev->is_open;
}

static void module_notify(struct mtk_hcp *hcp_dev,
					struct share_buf *user_data_addr)
{
	if (unlikely(!user_data_addr)) {
		HCP_PRINT_ERR("invalid null share buffer\n");
		return;
	}

	if (unlikely(user_data_addr->id >= HCP_MAX_ID)) {
		HCP_PRINT_ERR("invalid hcp_id(%d)\n", user_data_addr->id);
		return;
	}

	HCP_PRINT_DBG("send msg_id(%d)\n", user_data_addr->id);

	if (hcp_dev->hcp_desc_table[user_data_addr->id].handler)
		hcp_dev->hcp_desc_table[user_data_addr->id].handler(
			user_data_addr->share_data,
			user_data_addr->len,
			hcp_dev->hcp_desc_table[user_data_addr->id].priv);
}

static int hcp_send_internal(
	struct mtk_hcp *hcp_dev,
	enum hcp_id id,
	void *buf,
	unsigned int len,
	int req_fd,
	unsigned int wait)
{
	struct share_buf send_obj = {0};
	unsigned long timeout = 0;
	unsigned long flag = 0;
	struct msg *msg = NULL;
	int ret = 0;
	unsigned int no = 0;
	int module_id = -1;

	HCP_PRINT_DBG("id(%u) len(%u)\n", (unsigned int)id, len);

	if (unlikely((id >= HCP_MAX_ID) ||
		     (len > sizeof(send_obj.share_data)) ||
		     (!buf))) {
		HCP_PRINT_ERR(
			"failed to send hcp message (Invalid arg.), len/sz(0x%x/%lu)\n",
			len, sizeof(send_obj.share_data));
		return -EINVAL;
	}

	if (unlikely(false == mtk_hcp_running(hcp_dev))) {
		HCP_PRINT_WRN("hcp is not running\n");
		return -EPERM;
	}

	module_id = hcp_id_to_module_id(hcp_dev, id);
	if (unlikely((module_id < MODULE_ISP) ||
		     (module_id >= MODULE_MAX_ID))) {
		HCP_PRINT_ERR("invalid module_id(%d)\n", module_id);
		return -EINVAL;
	}

	timeout = msecs_to_jiffies(HCP_TIMEOUT_MS);
	ret = wait_event_timeout(hcp_dev->msg_wq,
		((msg = msg_pool_get(hcp_dev)) != NULL), timeout);
	switch (ret) {
	case 0:
		HCP_PRINT_WRN("id(%d) refill timeout !\n", id);
		break;
	case -ERESTARTSYS:
		HCP_PRINT_WRN("id(%d) refill interrupted !\n", id);
		break;
	default:
		break;
	}

	if (unlikely(!msg)) {
		HCP_PRINT_WRN("id(%d) msg poll is full\n", id);
		return -EAGAIN;
	}

	atomic_set(&hcp_dev->hcp_id_ack[id], 0);

	memcpy((void *)msg->user_obj.share_data, buf, len);
	msg->user_obj.len = len;

	spin_lock_irqsave(&hcp_dev->msglock, flag);

	no = atomic_inc_return(&hcp_dev->seq);

	msg->user_obj.info.send.hcp = id;
	msg->user_obj.info.send.req = req_fd;
	msg->user_obj.info.send.seq = no;
	msg->user_obj.info.send.ack = (wait ? 1 : 0);
	msg->user_obj.id = id;

	list_add_tail(&msg->entry, &hcp_dev->chans[module_id]);
	spin_unlock_irqrestore(&hcp_dev->msglock, flag);

	switch (id) {
	case HCP_IMGSYS_DEQUE_DONE_ID:
		/* DO NOT wake up */
		break;
	case HCP_IMGSYS_SW_TIMEOUT_ID:
		HCP_PRINT_DBG("wake up id(%d)\n", id);
		wake_up(&hcp_dev->poll_wq[module_id]);
		chans_pool_dump(hcp_dev);
		break;
	default:
		HCP_PRINT_DBG("wake up id(%d)\n", id);
		wake_up(&hcp_dev->poll_wq[module_id]);
		break;
	}

	HCP_PRINT_DBG("no(%d) msg_id(%d) sz(0x%x) send to user space\n", no, id, len);

	if (!wait)
		return 0;

	/* wait for RED's ACK */
	timeout = msecs_to_jiffies(HCP_TIMEOUT_MS);
	ret = wait_event_timeout(
		hcp_dev->ack_wq[module_id],
		atomic_cmpxchg(&(hcp_dev->hcp_id_ack[id]), 1, 0),
		timeout);

	switch (ret) {
	case 0:
		HCP_PRINT_WRN("id(%d) ack timeout!\n", id);
		ret = -EIO;
		break;
	case -ERESTARTSYS:
		HCP_PRINT_WRN("id(%d) ack wait interrupted!\n", id);
		ret = -ERESTARTSYS;
		break;
	default:
		ret = 0;
		break;
	}

	return ret;
}

static void mtk_hcp_purge_msg_internal(struct mtk_hcp *hcp_dev)
{
	unsigned long flag = 0;
	int i = 0;
	struct msg *msg = NULL;
	struct msg *tmp = NULL;

	if (!hcp_dev)
		return;

	spin_lock_irqsave(&hcp_dev->msglock, flag);
	for (i = 0; i < MODULE_MAX_ID; i++) {
		list_for_each_entry_safe(msg, tmp,
					&hcp_dev->chans[i], entry){
			list_del(&msg->entry);
			list_add_tail(&msg->entry, &hcp_dev->msg_list);
		}
	}
	atomic_set(&hcp_dev->seq, 0);
	spin_unlock_irqrestore(&hcp_dev->msglock, flag);
}

#ifdef AED_SET_EXTRA_FUNC_READY_ON_K515
int hcp_notify_aee(void)
{
	struct msg *msg = NULL;
	char dummy = 0;

	HCP_PRINT_WRN("HCP trigger AEE dump+\n");
	msg = msg_pool_get(hcp_mtkdev);
	msg->user_obj.id = HCP_IMGSYS_AEE_DUMP_ID;
	msg->user_obj.len = 0;
	msg->user_obj.info.send.hcp = HCP_IMGSYS_AEE_DUMP_ID;
	msg->user_obj.info.send.req = 0;
	msg->user_obj.info.send.ack = 0;

	hcp_send_internal(hcp_mtkdev, HCP_IMGSYS_AEE_DUMP_ID, &dummy, 1, 0, 1);
	module_notify(hcp_mtkdev, &msg->user_obj);

	HCP_PRINT_WRN("HCP trigger AEE dump-\n");

	return 0;
}
#endif

int mtk_hcp_kernel_log_clear(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	struct hcp_aee_info *info;
	struct hcp_proc_data *data;
	int ret = 0;

	// error handle for null pointer
	if (!hcp_dev) {
		// dev_info(&pdev->dev, "HCP device not initialized\n");
		HCP_PRINT_DBG("HCP device not initialized\n");
		return -ENODEV;
	}
	info = &hcp_dev->aee_info;
	data = &info->data[HCP_AEE_PROC_FILE_KERNEL];

	// acquire mutex
	ret = mutex_lock_killable(&data->mtx);
	if (ret != 0) {
		// dev_info(&pdev->dev, "Failed to acquire mutex: %d\n", ret);
		HCP_PRINT_DBG("Failed to acquire mutex: %d\n", ret);
		return ret;
	}

	// check buffer size is valid
	if (data->sz == 0) {
		// dev_info(&pdev->dev, "Invalid buffer\n");
		HCP_PRINT_DBG("Invalid buffer\n");
		ret = -EINVAL;
		goto out;
	}

	// clear the buffer
	memset(data->buf, 0, data->sz);
	data->cnt = 0;

	if (hcp_dbg_enable())
		HCP_PRINT_DBG("HCP IMG_KERNEL cleared\n");

out:
	mutex_unlock(&data->mtx);
	return ret;
}
EXPORT_SYMBOL(mtk_hcp_kernel_log_clear);

ssize_t mtk_img_kernel_write(struct platform_device *pdev, const char *fmt, ...)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	struct hcp_aee_info *info = &hcp_dev->aee_info;
	struct hcp_proc_data *data = (struct hcp_proc_data *)&info->data[HCP_AEE_PROC_FILE_KERNEL];
	ssize_t written = 0;
	va_list args;
	ssize_t ret;
	size_t remaining;

	if (mutex_lock_killable(&data->mtx) != 0) { // killable means that can be interrupted by signal
		HCP_PRINT_DBG("mtx lock failed due to process being killed");
		return -EINTR;
	}

	if (hcp_dbg_enable())
		HCP_PRINT_DBG("==== [IMG_KERNEL] data->cnt = %lu, data->sz = %lu\n", data->cnt, data->sz);

	if (data->cnt >= data->sz) {
		if (hcp_dbg_enable())
			HCP_PRINT_DBG("Buffer already full before writing");
		goto out;
	}

	remaining = data->sz - data->cnt;

	va_start(args, fmt);
	ret = vsnprintf(data->buf + data->cnt, remaining, fmt, args);
	va_end(args);

	if(ret < 0){
		if (hcp_dbg_enable())
			HCP_PRINT_DBG("Error formatting string for AEE kernel write");
		goto out;
	}

	if (ret >= remaining) {
		// Buffer size not enough for dumping the whole data
		if (hcp_dbg_enable())
			HCP_PRINT_DBG("AEE IMG_KERNEL write truncated, buffer would overflow");
		data->cnt = data->sz;
		written = remaining - 1;  // -1 for null terminator
	} else {
		// normal case for dumping
		data->cnt += ret;
		written = ret;
	}

out:
	mutex_unlock(&data->mtx);
	if (hcp_dbg_enable())
		HCP_PRINT_DBG("Done hcp write buffer, written %zd bytes\n", written);
	return written;
}
EXPORT_SYMBOL(mtk_img_kernel_write);

static int mtk_hcp_proc_open(struct inode *inode, struct file *file)
{
	struct mtk_hcp *hcp_dev = hcp_mtkdev;

	const char *name = NULL;

	if (unlikely(!try_module_get(THIS_MODULE)))
		return -ENODEV;

	name = file->f_path.dentry->d_name.name;
	if (!strcmp(name, "daemon")) {
		file->private_data
			= &hcp_dev->aee_info.data[HCP_AEE_PROC_FILE_DAEMON];
	} else if (!strcmp(name, "kernel")) {
		file->private_data
			= &hcp_dev->aee_info.data[HCP_AEE_PROC_FILE_KERNEL];
	} else if (!strcmp(name, "stream")) {
		file->private_data
			= &hcp_dev->aee_info.data[HCP_AEE_PROC_FILE_STREAM];
	} else {
		HCP_PRINT_ERR("unknown proc file(%s)\n", name);
		module_put(THIS_MODULE);
		return -EPERM;
	}

	if (unlikely(!file->private_data)) {
		HCP_PRINT_ERR("failed to allocate proc file(%s) buffer\n", name);
		module_put(THIS_MODULE);
		return -ENOMEM;
	}

	HCP_PRINT_INF("%s opened\n", name);

	return 0;
}

static ssize_t mtk_hcp_proc_read(struct file *file, char __user *buf,
	size_t lbuf, loff_t *ppos)
{
	struct hcp_proc_data *data = (struct hcp_proc_data *)file->private_data;
	int remain = 0;
	int len = 0;
	int ret = 0;

	ret = mutex_lock_killable(&data->mtx);
	if (unlikely(-EINTR == ret)) {
		HCP_PRINT_ERR("mtx lock failed due to process being killed\n");
		return ret;
	}

	// check ppos valid
	if (*ppos < 0 || *ppos > data->cnt || *ppos >= data->sz) {
		mutex_unlock(&data->mtx);
		HCP_PRINT_ERR("Invalid read position: %lld\n", *ppos);
		return 0;
	}

	remain = data->cnt - *ppos;
	len = (remain > lbuf) ? lbuf : remain;
	if (len == 0) {
		mutex_unlock(&data->mtx);
		HCP_PRINT_INF("Reached end of the device on a read\n");
		return 0;
	}

	len = len - copy_to_user(buf, data->buf + *ppos, len);
	*ppos += len;

	mutex_unlock(&data->mtx);

	HCP_PRINT_DBG("Leaving the READ function, len=%d, pos=%d\n", len, (int)*ppos);

	return len;
}

static ssize_t mtk_hcp_proc_write(struct file *file, const char __user *buf,
	size_t lbuf, loff_t *ppos)
{
	struct hcp_proc_data *data = (struct hcp_proc_data *)file->private_data;
	ssize_t remain = 0;
	ssize_t len = 0;
	int ret = 0;

	ret = mutex_lock_killable(&data->mtx);
	if (unlikely(-EINTR == ret)) {
		HCP_PRINT_ERR("mtx lock failed due to process being killed\n");
		return 0;
	}

	remain = data->sz - data->cnt;
	len = (remain > lbuf) ? lbuf : remain;
	if (len == 0) {
		mutex_unlock(&data->mtx);
		HCP_PRINT_ERR("Reached end of the device on a write\n");
		return 0;
	}

	len = len - copy_from_user(data->buf + data->cnt, buf, len);

	data->cnt += len;
	*ppos = data->cnt;

	mutex_unlock(&data->mtx);

	HCP_PRINT_DBG("Leaving the WRITE function, len=%ld, pos=%lu\n", len, data->cnt);

	return len;
}

static int mtk_hcp_proc_close(struct inode *inode, struct file *file)
{
	module_put(THIS_MODULE);
	return 0;
}

static void hcp_aee_reset(struct mtk_hcp *hcp_dev)
{
	int i = 0;
	struct hcp_aee_info *info = &hcp_dev->aee_info;

	HCP_PRINT_DBG("-s\n");

	if (unlikely(!info)) {
		HCP_PRINT_ERR("aee_info is NULL\n");
		return;
	}

	for (i = 0 ; i < HCP_AEE_PROC_FILE_NUM ; i++) {
		memset(info->data[i].buf, 0, info->data[i].sz);
		info->data[i].cnt = 0;
	}

	HCP_PRINT_DBG("-e\n");
}

static const struct proc_ops aee_ops = {
	.proc_open = mtk_hcp_proc_open,
	.proc_read  = mtk_hcp_proc_read,
	.proc_write = mtk_hcp_proc_write,
	.proc_release = mtk_hcp_proc_close
};

static int hcp_aee_init(struct mtk_hcp *hcp_dev)
{
	struct hcp_aee_info *info = NULL;
	struct hcp_proc_data *data = NULL;
	kuid_t uid;
	kgid_t gid;
	int i = 0;

	HCP_PRINT_DBG("-s\n");

	info = &hcp_dev->aee_info;

#ifdef AED_SET_EXTRA_FUNC_READY_ON_K515
	aed_set_extra_func(hcp_notify_aee);
#endif
	info->entry = proc_mkdir("mtk_img_debug", NULL);
	if (unlikely((!info->entry))) {
		HCP_PRINT_ERR("failed to create imgsys debug node\n");
		return -1;
	}

	for (i = 0 ; i < HCP_AEE_PROC_FILE_NUM; i++) {
		data = &info->data[i];
		data->sz = HCP_AEE_MAX_BUFFER_SIZE;
		mutex_init(&data->mtx);
	}

	hcp_aee_reset(hcp_dev);

	info->daemon =
		proc_create("daemon", 0660, info->entry, &aee_ops);
	info->stream =
		proc_create("stream", 0660, info->entry, &aee_ops);
	info->kernel =
		proc_create("kernel", 0660, info->entry, &aee_ops);

	uid = make_kuid(&init_user_ns, 0);
	gid = make_kgid(&init_user_ns, 1000);

	if (likely(info->daemon))
		proc_set_user(info->daemon, uid, gid);
	else
		HCP_PRINT_WRN("failed to create daemon proc\n");

	if (likely(info->stream))
		proc_set_user(info->stream, uid, gid);
	else
		HCP_PRINT_WRN("failed to create stream proc\n");

	if (likely(info->kernel))
		proc_set_user(info->kernel, uid, gid);
	else
		HCP_PRINT_WRN("failed to create kernel proc\n");

	HCP_PRINT_DBG("-s\n");

	return 0;
}

static int hcp_aee_deinit(struct mtk_hcp *hcp_dev)
{
	struct hcp_aee_info *info = &hcp_dev->aee_info;
	struct hcp_proc_data *data = NULL;
	int i = 0;

	for (i = 0 ; i < HCP_AEE_PROC_FILE_NUM; i++) {
		data = &info->data[i];
		data->sz = HCP_AEE_MAX_BUFFER_SIZE;
		mutex_destroy(&data->mtx);
	}

	if (likely(info->kernel))
		proc_remove(info->kernel);

	if (likely(info->daemon))
		proc_remove(info->daemon);

	if (likely(info->stream))
		proc_remove(info->stream);

	if (likely(info->entry))
		proc_remove(info->entry);

	return 0;
}

ssize_t mtk_hcp_write_krn_db(struct platform_device *pdev,
		const char *buf, size_t sz)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	struct hcp_aee_info *info = &hcp_dev->aee_info;
	struct hcp_proc_data *data
		= (struct hcp_proc_data *)&info->data[HCP_AEE_PROC_FILE_KERNEL];
	size_t remain = 0;
	size_t len = 0;
	int ret = 0;

	ret = mutex_lock_killable(&data->mtx);
	if (unlikely(-EINTR == ret)) {
		HCP_PRINT_WRN("mtx lock failed due to process being killed\n");
		return ret;
	}

	remain = data->sz - data->cnt;
	len = (remain > sz) ? sz : remain;

	if (len == 0) {
		HCP_PRINT_DBG("Reach end of the file on write\n");
		mutex_unlock(&data->mtx);
		return 0;
	}

	memcpy(data->buf + data->cnt, buf, len);
	data->cnt += len;

	mutex_unlock(&data->mtx);

	return len;
}

static int mtk_hcp_register(
	struct platform_device *pdev,
	enum hcp_id id,
	hcp_handler_t handler,
	const char *name,
	void *priv)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	unsigned int idx = (unsigned int)id;

	if (unlikely(!hcp_dev)) {
		HCP_PRINT_ERR("hcp device is not ready\n");
		return -EPROBE_DEFER;
	}

	if (unlikely(!handler)) {
		HCP_PRINT_ERR("handler is NULL\n");
		return -EINVAL;
	}

	if (unlikely(idx >= HCP_MAX_ID)) {
		HCP_PRINT_ERR("hcp_id(%u) >= %u\n", idx, HCP_MAX_ID);
		return -EINVAL;
	}

	hcp_dev->hcp_desc_table[idx].name = name;
	hcp_dev->hcp_desc_table[idx].handler = handler;
	hcp_dev->hcp_desc_table[idx].priv = priv;

	return 0;
}

static int mtk_hcp_unregister(
	struct platform_device *pdev,
	enum hcp_id id)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);
	unsigned int idx = (unsigned int)id;

	if (unlikely(!hcp_dev)) {
		HCP_PRINT_ERR("hcp device is not ready\n");
		return -EPROBE_DEFER;
	}

	if (unlikely(idx >= HCP_MAX_ID)) {
		HCP_PRINT_ERR("register hcp id %u with invalid arg\n", idx);
		return -EINVAL;
	}

	memset((void *)&hcp_dev->hcp_desc_table[idx], 0, sizeof(struct hcp_desc));

	return 0;
}

static int mtk_hcp_send(
	struct platform_device *pdev,
	enum hcp_id id,
	void *buf,
	unsigned int len,
	int req_fd)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	return hcp_send_internal(hcp_dev, id, buf, len, req_fd, SYNC_SEND);
}

static int mtk_hcp_send_async(
	struct platform_device *pdev,
	enum hcp_id id,
	void *buf,
	unsigned int len,
	int req_fd)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	return hcp_send_internal(hcp_dev, id, buf, len, req_fd, ASYNC_SEND);
}

static int mtk_hcp_open(struct inode *inode, struct file *file)
{
	struct mtk_hcp *hcp_dev = NULL;

	hcp_dev = container_of(inode->i_cdev, struct mtk_hcp, hcp_cdev);
	HCP_PRINT_DBG("open inode->i_cdev = 0x%p\n", inode->i_cdev);

	file->private_data = hcp_dev;
	hcp_dev->is_open = true;
	hcp_dev->current_task = current;

	/* purge old messages */
	mtk_hcp_purge_msg_internal(hcp_dev);

	/* TODO: Clear memory blocks ? */
	HCP_PRINT_DBG("hcp opend\n");

	return 0;
}

static unsigned int mtk_hcp_poll(struct file *file, poll_table *wait)
{
	struct mtk_hcp *hcp_dev = (struct mtk_hcp *)file->private_data;

	HCP_PRINT_DBG("poll -s\n");

	if (chan_pool_available(hcp_dev, MODULE_IMG)) {
		HCP_PRINT_DBG("poll(%d) -e\n", POLLIN);
		return POLLIN;
	}

	poll_wait(file, &hcp_dev->poll_wq[MODULE_IMG], wait);
	if (chan_pool_available(hcp_dev, MODULE_IMG)) {
		HCP_PRINT_DBG("poll(%d) -e\n", POLLIN);
		return POLLIN;
	}

	HCP_PRINT_DBG("poll(0) -e\n");
	return 0;
}

static int mtk_hcp_release(struct inode *inode, struct file *file)
{
	struct mtk_hcp *hcp_dev = (struct mtk_hcp *)file->private_data;
	int i = 0;

	HCP_PRINT_INF("-s\n");

	/* clear waiting msg while hcp will be closed */
	for (i = 0; i < HCP_MAX_ID; i++)
		atomic_set(&hcp_dev->hcp_id_ack[i], 1);

	for (i = 0; i < MODULE_MAX_ID; i++)
		wake_up(&hcp_dev->ack_wq[i]);

	hcp_dev->is_open = false;

	HCP_PRINT_INF("-e\n");

	return 0;
}

static int mtk_hcp_mmap(struct file *file, struct vm_area_struct *vma)
{
	return -EOPNOTSUPP;
}

static void module_wake_up(
	struct mtk_hcp *hcp_dev,
	struct share_buf *user_data_addr)
{
	int module_id = 0;

	if (unlikely(!user_data_addr)) {
		HCP_PRINT_ERR("invalid null share buffer\n");
		return;
	}

	if (unlikely(user_data_addr->id >= HCP_MAX_ID)) {
		HCP_PRINT_ERR("invalid hcp_id(%d)\n", user_data_addr->id);
		return;
	}

	module_id = hcp_id_to_module_id(hcp_dev, user_data_addr->id);
	if (unlikely((module_id < MODULE_ISP) ||
		     (module_id >= MODULE_MAX_ID))) {
		HCP_PRINT_ERR("invalid module_id(%d)\n", module_id);
		return;
	}

	atomic_set(&hcp_dev->hcp_id_ack[user_data_addr->id], 1);
	wake_up(&hcp_dev->ack_wq[module_id]);
}

static int mtk_hcp_red_init(
	struct mtk_hcp *hcp_dev,
	struct share_buf *buf)
{
	unsigned int i,j,k;
	struct imgsys_hcp_init_info *init_info;
	struct mtk_hcp_rsv_mb *mb;
	const struct mtk_hcp_plat_op *plat_op;

	if (unlikely(!buf)) {
		HCP_PRINT_ERR("buf is NULL\n");
		return -EINVAL;
	}

	init_info = (struct imgsys_hcp_init_info *)&buf->share_data[0];

	/* log lvl */
	if (init_info->dbg_lvl <= HCP_LOG_LVL_ERR) {
		HCP_PRINT_INF("set dbg lvl to %u\n", init_info->dbg_lvl);
		hcp_dbg_en = init_info->dbg_lvl;
	}

	plat_op = hcp_dev->plat_op;
	if (unlikely(plat_op == NULL)) {
		HCP_PRINT_ERR("plat_op is NULL\n");
		return -EINVAL;
	}


	for (i = 0 ; i < IMGSYS_MEMORY_MODE_NUM_MAX ; i++) {
		mb = plat_op->fetch_gce_mb(i);
		if (likely(mb)) {
			mb->cfg.size = init_info->gce_mb_cfg[i].size;
			mb->cfg.cache_mode = init_info->gce_mb_cfg[i].coherent_mode;
			HCP_PRINT_INF("Set %s sz 0x%llX coherent:%u, mode:%u\n",
				mb->cfg.name, mb->cfg.size, mb->cfg.cache_mode, i);
		}

		mb = plat_op->fetch_gce_clr_token_mb(i);
		if (likely(mb)) {
			mb->cfg.size = init_info->gce_clr_token_cfg[i].size;
			mb->cfg.cache_mode = init_info->gce_clr_token_cfg[i].coherent_mode;
			HCP_PRINT_INF("Set %s sz 0x%llX coherent:%u, mode:%u\n",
				mb->cfg.name, mb->cfg.size, mb->cfg.cache_mode, i);
		}

		for (j = 0 ; j < IMGSYS_MOD_DRV_NUM_MAX ; j++) {
			for (k = 0 ; k < IMGSYS_MODULE_WORKING_BUF_TYPE_NUM_MAX ; k++) {
				mb = plat_op->fetch_mod_mb(i, j, k);
				if (likely(mb)) {
					mb->cfg.size = init_info->mod_mb_cfg[i][j][k].size;
					/* only cq and tdr need determine coherent mode or not */
					if (k == IMGSYS_MODULE_WORKING_BUF_TYPE_CQ ||
							k == IMGSYS_MODULE_WORKING_BUF_TYPE_TDR) {
						mb->cfg.cache_mode = init_info->mod_mb_cfg[i][j][k].coherent_mode;
					}
					HCP_PRINT_INF("Set %s sz 0x%llX coherent:%u, mode:%u\n",
						mb->cfg.name, mb->cfg.size, mb->cfg.cache_mode, i);
				}
			}
		}
	}

	return 0;
}

static long mtk_hcp_ioctl(
	struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	long ret = -1;
	struct mtk_hcp *hcp_dev = (struct mtk_hcp *)file->private_data;
	struct share_buf buffer = {0};
	struct packet data = {0};
	unsigned int index = 0;
	struct msg *msg = NULL;
	unsigned long flag = 0;

	switch (cmd) {
	case HCP_GET_OBJECT:
		(void)copy_from_user(&data, (void *)arg, sizeof(struct packet));
		if (unlikely((data.count > IPI_MAX_BUFFER_COUNT) ||
			     (data.count < 0))) {
			HCP_PRINT_ERR("Get_OBJ # of buf:%u in cmd:%d exceed %u\n",
				data.count, cmd, IPI_MAX_BUFFER_COUNT);
			return -EINVAL;
		}
		for (index = 0; index < IPI_MAX_BUFFER_COUNT; index++) {
			if (unlikely(index >= data.count))
				break;

			if (unlikely(!data.buffer[index])) {
				HCP_PRINT_ERR("Get_OBJ buf[%u] is NULL\n", index);
				return -EINVAL;
			}
			(void)copy_from_user((void *)&buffer, (void *)data.buffer[index],
				sizeof(struct share_buf));
			if (buffer.info.cmd == HCP_COMPLETE) {
				module_notify(hcp_dev, &buffer);
				module_wake_up(hcp_dev, &buffer);
			} else if (buffer.info.cmd == HCP_NOTIFY) {
				module_notify(hcp_dev, &buffer);
			} else {
				HCP_PRINT_ERR(
					"Unknown commands %p, %d\n",
					data.buffer[index],
					buffer.info.cmd);
				return ret;
			}
		}

		index = 0;
		while (chan_pool_available(hcp_dev, MODULE_IMG)) {
			if (unlikely(index >= IPI_MAX_BUFFER_COUNT))
				break;

			msg = chan_pool_get(hcp_dev, MODULE_IMG);
			if (likely(msg != NULL)) {
				ret = copy_to_user(
					(void *)data.buffer[index++],
					&msg->user_obj,
					(unsigned long)sizeof(struct share_buf));

				HCP_PRINT_DBG("copy to user, index(%d)\n", index);

				spin_lock_irqsave(&hcp_dev->msglock, flag);
				list_add_tail(&msg->entry, &hcp_dev->msg_list);
				spin_unlock_irqrestore(&hcp_dev->msglock, flag);
				wake_up(&hcp_dev->msg_wq);
			} else {
				HCP_PRINT_WRN("can't get msg from chan_pool\n");
			}
		}

		put_user(index, (int32_t *)(arg + offsetof(struct packet, count)));
		ret = chan_pool_available(hcp_dev, MODULE_IMG);
		put_user(ret, (bool *)(arg + offsetof(struct packet, more)));
		ret = 0;
		break;
	case HCP_COMPLETE:
		(void)copy_from_user(&buffer, (void *)arg, sizeof(struct share_buf));
		module_notify(hcp_dev, &buffer);
		module_wake_up(hcp_dev, &buffer);
		ret = 0;
		break;
	case HCP_NOTIFY:
		(void)copy_from_user(&buffer, (void *)arg, sizeof(struct share_buf));
		module_notify(hcp_dev, &buffer);
		ret = 0;
		break;
	case HCP_WAKEUP:
		wake_up(&hcp_dev->poll_wq[MODULE_IMG]);
		ret = 0;
		break;
	case HCP_TIMEOUT:
		chans_pool_dump(hcp_dev);
		ret = 0;
		break;
	case HCP_INIT:
		(void)copy_from_user(&buffer, (void *)arg, sizeof(struct share_buf));
		if (buffer.id == HCP_INIT_ID)
			ret = mtk_hcp_red_init(hcp_dev, &buffer);
		else
			ret = -EINVAL;
		break;
	default:
		HCP_PRINT_DBG("Invalid cmd(0x%x)\n", cmd);
		break;
	}

	return ret;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long mtk_hcp_compat_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	struct mtk_hcp *hcp_dev = (struct mtk_hcp *)file->private_data;
	long ret = -1;
	struct share_buf __user *share_data32 = NULL;

	switch (cmd) {
	case COMPAT_HCP_GET_OBJECT:
	case COMPAT_HCP_NOTIFY:
	case COMPAT_HCP_COMPLETE:
	case COMPAT_HCP_WAKEUP:
	case COMPAT_HCP_INIT:
		share_data32 = compat_ptr((uint32_t)arg);
		ret = file->f_op->unlocked_ioctl(file,
				cmd, (unsigned long)share_data32);
		break;
	default:
		dev_info(hcp_dev->dev, "Invalid cmd_number 0x%x.\n", cmd);
		break;
	}
	return ret;
}
#endif

static const struct file_operations hcp_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = mtk_hcp_ioctl,
	.open           = mtk_hcp_open,
	.poll           = mtk_hcp_poll,
	.release        = mtk_hcp_release,
	.mmap           = mtk_hcp_mmap,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl   = mtk_hcp_compat_ioctl,
#endif
};

void mtk_hcp_purge_msg(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	mtk_hcp_purge_msg_internal(hcp_dev);
}
EXPORT_SYMBOL(mtk_hcp_purge_msg);

static inline struct mtk_hcp_rsv_mb *mtk_hcp_fetch_gce_mb(
	struct platform_device *pdev,
	unsigned int mem_mode)
{
	const struct mtk_hcp_plat_op *plat_op = fetch_plat_op(pdev);

	if (unlikely(!plat_op)) {
		HCP_PRINT_ERR("plat_op is NULL\n");
		return NULL;
	}

	if (unlikely(!plat_op->fetch_gce_mb)) {
		HCP_PRINT_ERR("plat_op->fetch_gce_mb is NULL\n");
		return NULL;
	}

	return plat_op->fetch_gce_mb(mem_mode);
}

static inline struct mtk_hcp_rsv_mb *mtk_hcp_fetch_gce_clr_token_mb(
	struct platform_device *pdev,
	unsigned int mem_mode)
{
	const struct mtk_hcp_plat_op *plat_op = fetch_plat_op(pdev);

	if (unlikely(!plat_op)) {
		HCP_PRINT_ERR("plat_op is NULL\n");
		return NULL;
	}

	if (unlikely(!plat_op->fetch_gce_clr_token_mb)) {
		HCP_PRINT_ERR("plat_op->fetch_gce_mb is NULL\n");
		return NULL;
	}

	return plat_op->fetch_gce_clr_token_mb(mem_mode);
}

static inline struct mtk_hcp_rsv_mb *mtk_hcp_fetch_mod_mb(
	struct platform_device *pdev,
	unsigned int mem_mode,
	unsigned int mod_id,
	unsigned int mb_type)
{
	const struct mtk_hcp_plat_op *plat_op = fetch_plat_op(pdev);

	if (unlikely(!plat_op)) {
		HCP_PRINT_ERR("plat_op is NULL\n");
		return NULL;
	}

	if (unlikely(!plat_op->fetch_gce_mb)) {
		HCP_PRINT_ERR("plat_op->fetch_gce_mb is NULL\n");
		return NULL;
	}

	return plat_op->fetch_mod_mb(mem_mode, mod_id, mb_type);
}

static int mtk_hcp_allocate_gce_mb(
	struct platform_device *pdev,
	unsigned int mem_mode)
{
	int ret = -EINVAL;
	struct mtk_hcp_rsv_mb *mb = NULL;

	mb = mtk_hcp_fetch_gce_mb(pdev, mem_mode);

	if (unlikely(!mb)) {
		HCP_PRINT_ERR("mb is NULL of mode(%u)\n", mem_mode);
		return ret;
	}

	if (likely(mb->alloc))
		ret = mb->alloc(mb);

	return ret;
}

static int mtk_hcp_free_gce_mb(
	struct platform_device *pdev,
	unsigned int mem_mode)
{
	int ret = -EINVAL;
	struct mtk_hcp_rsv_mb *mb = NULL;

	mb = mtk_hcp_fetch_gce_mb(pdev, mem_mode);

	if (unlikely(!mb)) {
		HCP_PRINT_ERR("mb is NULL of mode(%u)\n", mem_mode);
		return ret;
	}

	if (likely(mb->free))
		ret = mb->free(mb);

	return ret;
}

static int mtk_hcp_get_gce_mb(
	struct platform_device *pdev,
	unsigned int mem_mode)
{
	int ret = -EINVAL;
	struct mtk_hcp_rsv_mb *mb = NULL;

	mb = mtk_hcp_fetch_gce_mb(pdev, mem_mode);

	if (unlikely(!mb)) {
		HCP_PRINT_ERR("mb is NULL of mode(%u)\n", mem_mode);
		return ret;
	}

	if (likely(mb->get_ref))
		ret = mb->get_ref(mb);

	return ret;
}

static int mtk_hcp_put_gce_mb(
	struct platform_device *pdev,
	unsigned int mem_mode)
{
	int ret = -EINVAL;
	struct mtk_hcp_rsv_mb *mb = NULL;

	mb = mtk_hcp_fetch_gce_mb(pdev, mem_mode);

	if (unlikely(!mb)) {
		HCP_PRINT_ERR("mb is NULL of mode(%u)\n", mem_mode);
		return ret;
	}

	if (likely(mb->put_ref))
		ret = mb->put_ref(mb);

	return ret;
}

static int mtk_hcp_allocate_gce_clr_token_mb(
	struct platform_device *pdev,
	unsigned int mem_mode)
{
	int ret = -EINVAL;
	struct mtk_hcp_rsv_mb *mb = NULL;

	mb = mtk_hcp_fetch_gce_clr_token_mb(pdev, mem_mode);

	if (unlikely(!mb)) {
		HCP_PRINT_ERR("mb is NULL\n");
		return ret;
	}

	if (likely(mb->alloc))
		ret = mb->alloc(mb);

	return ret;
}

static int mtk_hcp_free_gce_clr_token_mb(
	struct platform_device *pdev,
	unsigned int mem_mode)
{
	int ret = -EINVAL;
	struct mtk_hcp_rsv_mb *mb = NULL;

	mb = mtk_hcp_fetch_gce_clr_token_mb(pdev, mem_mode);

	if (unlikely(!mb)) {
		HCP_PRINT_ERR("mb is NULL\n");
		return ret;
	}

	if (likely(mb->free))
		ret = mb->free(mb);

	return ret;
}

static int mtk_hcp_allocate_mod_mbs(
	struct platform_device *pdev,
	unsigned int mem_mode)
{
	int ret = -EINVAL;
	unsigned int mod_id;
	unsigned int mb_type;
	struct mtk_hcp_rsv_mb *mb = NULL;

	for (mod_id = 0; mod_id < IMGSYS_MOD_DRV_NUM_MAX ; mod_id++) {
		for (mb_type = 0; mb_type < IMGSYS_MODULE_WORKING_BUF_TYPE_NUM_MAX ; mb_type++) {
			mb = mtk_hcp_fetch_mod_mb(pdev, mem_mode, mod_id, mb_type);
			if (unlikely(!mb)) {
				HCP_PRINT_WRN("mb is NULL of mode(%u) mod_id(%u) mb_type(%u)\n",
					mem_mode, mod_id, mb_type);
				continue;
			}
			if (likely(mb->alloc))
				ret = mb->alloc(mb);

			if (likely(ret == 0)) {
				HCP_PRINT_DBG("alloc fd(%d) mode(%u) mod_id(%u) mb_type(%u)\n",
					mb->fd, mem_mode, mod_id, mb_type);
			} else {
				HCP_PRINT_DBG("alloc failed, mode(%u) mod_id(%u) mb_type(%u)\n",
					mem_mode, mod_id, mb_type);
			}
		}
	}

	return ret;
}


static int mtk_hcp_free_mod_mbs(
	struct platform_device *pdev,
	unsigned int mem_mode)
{
	int ret = -EINVAL;
	unsigned int mod_id;
	unsigned int mb_type;
	struct mtk_hcp_rsv_mb *mb = NULL;

	for (mod_id = 0; mod_id < IMGSYS_MOD_DRV_NUM_MAX ; mod_id++) {
		for (mb_type = 0; mb_type < IMGSYS_MODULE_WORKING_BUF_TYPE_NUM_MAX ; mb_type++) {
			mb = mtk_hcp_fetch_mod_mb(pdev, mem_mode, mod_id, mb_type);
			if (unlikely(!mb)) {
				HCP_PRINT_WRN("mb is NULL of mode(%u) mod_id(%u) mb_type(%u)\n",
					mem_mode, mod_id, mb_type);
				continue;
			}
			if (likely(mb->free))
				ret = mb->free(mb);

			if (unlikely(ret == 0)) {
				HCP_PRINT_DBG("free mode(%u) mod_id(%u) mb_type(%u)\n",
					mem_mode, mod_id, mb_type);
			} else {
				HCP_PRINT_DBG("free failed, fd(%d) mode(%u) mod_id(%u) mb_type(%u)\n",
					mb->fd, mem_mode, mod_id, mb_type);
			}
		}
	}

	return ret;
}

static int mtk_hcp_register_cbs(
	struct platform_device *pdev,
	const struct mtk_hcp_module_callbacks *cbs,
	void *priv)
{
	int ret = 0;

	if (unlikely(!pdev)) {
		HCP_PRINT_ERR("pdev is NULL\n");
		return -1;
	}

	if (unlikely(!cbs)) {
		HCP_PRINT_ERR("cbs is NULL\n");
		return -1;
	}

	if (unlikely(!cbs->send_frames)) {
		HCP_PRINT_ERR("cbs->send_frames is NULL\n");
		return -1;
	}

	if (unlikely(!cbs->clear_hw_token)) {
		HCP_PRINT_ERR("cbs->clear_hw_token is NULL\n");
		return -1;
	}

	if (unlikely(!cbs->init_done))
		HCP_PRINT_WRN("cbs->init_done is NULL\n");

	if (unlikely(!cbs->dump_for_aee))
		HCP_PRINT_WRN("cbs->dump_for_aee is NULL\n");

	ret = mtk_hcp_register(
		pdev,
		HCP_IMGSYS_FRAME_ID,
		cbs->send_frames,
		"frm_to_krn",
		priv);
	if (unlikely(ret != 0)) {
		HCP_PRINT_ERR("register send_frames cb failed\n");
		return ret;
	}

	ret = mtk_hcp_register(
		pdev,
		HCP_IMGSYS_CLEAR_HWTOKEN_ID,
		cbs->clear_hw_token,
		"clr_token",
		priv);
	if (unlikely(ret != 0)) {
		HCP_PRINT_ERR("register clear_hw_token cb failed\n");
		return ret;
	}

	ret = mtk_hcp_register(
		pdev,
		HCP_IMGSYS_INIT_ID,
		cbs->init_done,
		"init_done",
		priv);
	if (unlikely(ret != 0))
		HCP_PRINT_ERR("register init_done cb failed\n");

	ret = mtk_hcp_register(
		pdev,
		HCP_IMGSYS_AEE_DUMP_ID,
		cbs->dump_for_aee,
		"dump_for_aee",
		priv);
	if (unlikely(ret != 0))
		HCP_PRINT_ERR("register dump_for_aee cb failed\n");

	return 0;
}

static int mtk_hcp_unregister_cbs(struct platform_device *pdev)
{
	int ret = 0;

	if (unlikely(!pdev))
		return -1;

	ret |= mtk_hcp_unregister(pdev, HCP_IMGSYS_FRAME_ID);
	ret |= mtk_hcp_unregister(pdev, HCP_IMGSYS_INIT_ID);
	/* TODO: check whether need to unregister clear hw token and aee dump?*/
#if defined(CHECK_UNREG_DONE)
	ret |= mtk_hcp_unregister(pdev, HCP_IMGSYS_CLEAR_HWTOKEN_ID);
	ret |= mtk_hcp_unregister(pdev, HCP_IMGSYS_AEE_DUMP_ID);
#endif
	if (unlikely(ret))
		HCP_PRINT_WRN("failed to unreg cbs\n");

	return 0;
}

static int mtk_hcp_flush_dma_buf(
	struct platform_device *pdev,
	struct dma_buf *dbuf,
	const unsigned int offset,
	const unsigned int len)
{
	if (unlikely(!pdev)) {
		HCP_PRINT_ERR("pdev is NULL\n");
		return -1;
	}

	if (unlikely(!dbuf)) {
		HCP_PRINT_ERR("dbuf is NULL\n");
		return -1;
	}

	HCP_PRINT_DBG("dbuf(%p) len(%u) ofst(%u)\n",
		dbuf, len, offset);

	return dma_buf_end_cpu_access_partial(dbuf,
				DMA_BIDIRECTIONAL,
				offset,
				len);
}

static int mtk_hcp_flush_mb(
	struct platform_device *pdev,
	const unsigned int mb_id,
	const unsigned int offset,
	const unsigned int len)
{
	const struct mtk_hcp_plat_op *plat_op = NULL;
	struct mtk_hcp_rsv_mb *mb = NULL;

	if (unlikely(!pdev)) {
		HCP_PRINT_ERR("pdev is NULL\n");
		return -ENXIO;
	}

	HCP_PRINT_DBG("mb_id(0x%X) len(%u) ofst(%u)\n",
		mb_id, len, offset);

	plat_op = fetch_plat_op(pdev);
	if (likely(plat_op))
		mb = plat_op->fetch_mb(mb_id);

	if (unlikely(!mb)) {
		HCP_PRINT_ERR("failed to get mb(0x%X)\n", mb_id);
		return -EINVAL;
	}

	if (unlikely(!mb->d_buf)) {
		HCP_PRINT_ERR("failed to get dbuf of (0x%X)\n", mb_id);
		return -EINVAL;
	}

	if (mb->cfg.cache_mode == HCP_MB_CACHE_ON)
		return dma_buf_end_cpu_access_partial(mb->d_buf,
					DMA_BIDIRECTIONAL,
					offset,
					len);
	else
		return 0;
}

#define MTK_HCP_DECLARE_MOD_W_BUF_OPS_EXT(_mod_, m_id, _mb_, mb_id) \
	static unsigned int mtk_hcp_fetch_##_mod_##_##_mb_##_mb_id( \
		struct platform_device *pdev, \
		unsigned int mem_mode) \
	{ \
		struct mtk_hcp_rsv_mb *mb = NULL; \
							\
		mb = mtk_hcp_fetch_mod_mb(pdev, mem_mode, m_id, mb_id); \
		if (unlikely(!mb)) { \
			HCP_PRINT_ERR("mb is NULL\n"); \
			return UINT_MAX; \
		} \
		return mb->cfg.id; \
	} \
		\
	static int mtk_hcp_fetch_##_mod_##_##_mb_##_mb_fd( \
		struct platform_device *pdev, \
		unsigned int mem_mode) \
	{ \
		struct mtk_hcp_rsv_mb *mb = NULL; \
							\
		mb = mtk_hcp_fetch_mod_mb(pdev, mem_mode, m_id, mb_id); \
		if (unlikely(!mb)) { \
			HCP_PRINT_ERR("mb is NULL\n"); \
			return -1; \
		} \
		return mb->fd; \
	} \
		\
	static void *mtk_hcp_fetch_##_mod_##_##_mb_##_mb_virt( \
		struct platform_device *pdev, \
		unsigned int mem_mode) \
	{ \
		struct mtk_hcp_rsv_mb *mb = NULL; \
							\
		mb = mtk_hcp_fetch_mod_mb(pdev, mem_mode, m_id, mb_id); \
		if (unlikely(!mb)) { \
			HCP_PRINT_ERR("mb is NULL\n"); \
			return NULL; \
		} \
		return mb->start_virt; \
	} \
		\
	static u64 mtk_hcp_fetch_##_mod_##_##_mb_##_mb_phy( \
		struct platform_device *pdev, \
		unsigned int mem_mode) \
	{ \
		struct mtk_hcp_rsv_mb *mb = NULL; \
							\
		mb = mtk_hcp_fetch_mod_mb(pdev, mem_mode, m_id, mb_id); \
		if (unlikely(!mb)) { \
			HCP_PRINT_ERR("mb is NULL\n"); \
			return 0; \
		} \
		return mb->start_phys; \
	} \
		\
	static u64 mtk_hcp_fetch_##_mod_##_##_mb_##_mb_dma( \
		struct platform_device *pdev, \
		unsigned int mem_mode) \
	{ \
		struct mtk_hcp_rsv_mb *mb = NULL; \
							\
		mb = mtk_hcp_fetch_mod_mb(pdev, mem_mode, m_id, mb_id); \
		if (unlikely(!mb)) { \
			HCP_PRINT_ERR("mb is NULL\n"); \
			return 0; \
		} \
		return mb->start_dma; \
	} \
		\
	static size_t mtk_hcp_fetch_##_mod_##_##_mb_##_mb_size( \
		struct platform_device *pdev, \
		unsigned int mem_mode) \
	{ \
		struct mtk_hcp_rsv_mb *mb = NULL; \
							\
		mb = mtk_hcp_fetch_mod_mb(pdev, mem_mode, m_id, mb_id); \
		if (unlikely(!mb)) { \
			HCP_PRINT_ERR("mb is NULL\n"); \
			return 0; \
		} \
		return mb->cfg.size; \
	}

#define MTK_HCP_DECLARE_MOD_W_BUF_OPS_CQ(mod, m_id) \
	MTK_HCP_DECLARE_MOD_W_BUF_OPS_EXT( \
		mod, m_id, cq, IMGSYS_MODULE_WORKING_BUF_TYPE_CQ)

#define MTK_HCP_DECLARE_MOD_W_BUF_OPS_TDR(mod, m_id) \
	MTK_HCP_DECLARE_MOD_W_BUF_OPS_EXT( \
		mod, m_id, tdr, IMGSYS_MODULE_WORKING_BUF_TYPE_TDR)

#define MTK_HCP_DECLARE_MOD_W_BUF_OPS_C_MISC(mod, m_id) \
	MTK_HCP_DECLARE_MOD_W_BUF_OPS_EXT( \
		mod, m_id, c_misc, IMGSYS_MODULE_WORKING_BUF_TYPE_C_MISC)

#define MTK_HCP_DECLARE_MOD_W_BUF_OPS_NC_MISC(mod, m_id) \
	MTK_HCP_DECLARE_MOD_W_BUF_OPS_EXT( \
		mod, m_id, nc_misc, IMGSYS_MODULE_WORKING_BUF_TYPE_NC_MISC)

MTK_HCP_DECLARE_MOD_W_BUF_OPS_CQ(wpe, IMGSYS_MOD_DRV_WPE)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_TDR(wpe, IMGSYS_MOD_DRV_WPE)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_C_MISC(wpe, IMGSYS_MOD_DRV_WPE)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_NC_MISC(wpe, IMGSYS_MOD_DRV_WPE)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_CQ(omc, IMGSYS_MOD_DRV_OMC)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_TDR(omc, IMGSYS_MOD_DRV_OMC)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_C_MISC(omc, IMGSYS_MOD_DRV_OMC)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_NC_MISC(omc, IMGSYS_MOD_DRV_OMC)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_CQ(adl, IMGSYS_MOD_DRV_ADL)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_TDR(adl, IMGSYS_MOD_DRV_ADL)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_C_MISC(adl, IMGSYS_MOD_DRV_ADL)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_NC_MISC(adl, IMGSYS_MOD_DRV_ADL)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_CQ(traw, IMGSYS_MOD_DRV_TRAW)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_TDR(traw, IMGSYS_MOD_DRV_TRAW)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_C_MISC(traw, IMGSYS_MOD_DRV_TRAW)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_NC_MISC(traw, IMGSYS_MOD_DRV_TRAW)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_CQ(dip, IMGSYS_MOD_DRV_DIP)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_TDR(dip, IMGSYS_MOD_DRV_DIP)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_C_MISC(dip, IMGSYS_MOD_DRV_DIP)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_NC_MISC(dip, IMGSYS_MOD_DRV_DIP)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_CQ(pqdip, IMGSYS_MOD_DRV_PQDIP)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_TDR(pqdip, IMGSYS_MOD_DRV_PQDIP)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_C_MISC(pqdip, IMGSYS_MOD_DRV_PQDIP)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_NC_MISC(pqdip, IMGSYS_MOD_DRV_PQDIP)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_CQ(me, IMGSYS_MOD_DRV_ME)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_TDR(me, IMGSYS_MOD_DRV_ME)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_C_MISC(me, IMGSYS_MOD_DRV_ME)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_NC_MISC(me, IMGSYS_MOD_DRV_ME)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_CQ(mae, IMGSYS_MOD_DRV_MAE)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_TDR(mae, IMGSYS_MOD_DRV_MAE)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_C_MISC(mae, IMGSYS_MOD_DRV_MAE)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_NC_MISC(mae, IMGSYS_MOD_DRV_MAE)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_CQ(dfp, IMGSYS_MOD_DRV_DFP)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_TDR(dfp, IMGSYS_MOD_DRV_DFP)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_C_MISC(dfp, IMGSYS_MOD_DRV_DFP)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_NC_MISC(dfp, IMGSYS_MOD_DRV_DFP)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_CQ(dpe, IMGSYS_MOD_DRV_DPE)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_TDR(dpe, IMGSYS_MOD_DRV_DPE)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_C_MISC(dpe, IMGSYS_MOD_DRV_DPE)
MTK_HCP_DECLARE_MOD_W_BUF_OPS_NC_MISC(dpe, IMGSYS_MOD_DRV_DPE)

#define MTK_HCP_DECLARE_W_BUF_OPS(_mod_) \
	static unsigned int mtk_hcp_fetch_##_mod_##_mb_id( \
		struct platform_device *pdev, \
		unsigned int mem_mode) \
	{ \
		struct mtk_hcp_rsv_mb *mb = NULL; \
							\
		mb = mtk_hcp_fetch_##_mod_##_mb(pdev, mem_mode); \
		if (unlikely(!mb)) { \
			HCP_PRINT_ERR("mb is NULL\n"); \
			return UINT_MAX; \
		} \
		return mb->cfg.id; \
	} \
		\
	static int mtk_hcp_fetch_##_mod_##_mb_fd( \
		struct platform_device *pdev, \
		unsigned int mem_mode) \
	{ \
		struct mtk_hcp_rsv_mb *mb = NULL; \
							\
		mb = mtk_hcp_fetch_##_mod_##_mb(pdev, mem_mode); \
		if (unlikely(!mb)) { \
			HCP_PRINT_ERR("mb is NULL\n"); \
			return -1; \
		} \
		return mb->fd; \
	} \
		\
	static void *mtk_hcp_fetch_##_mod_##_mb_virt( \
		struct platform_device *pdev, \
		unsigned int mem_mode) \
	{ \
		struct mtk_hcp_rsv_mb *mb = NULL; \
							\
		mb = mtk_hcp_fetch_##_mod_##_mb(pdev, mem_mode); \
		if (unlikely(!mb)){ \
			HCP_PRINT_ERR("mb is NULL\n"); \
			return NULL; \
		} \
		return mb->start_virt; \
	} \
		\
	static u64 mtk_hcp_fetch_##_mod_##_mb_phy( \
		struct platform_device *pdev, \
		unsigned int mem_mode) \
	{ \
		struct mtk_hcp_rsv_mb *mb = NULL; \
							\
		mb = mtk_hcp_fetch_##_mod_##_mb(pdev, mem_mode); \
		if (unlikely(!mb)) { \
			HCP_PRINT_ERR("mb is NULL\n"); \
			return 0; \
		} \
		return mb->start_phys; \
	} \
		\
	static u64 mtk_hcp_fetch_##_mod_##_mb_dma( \
		struct platform_device *pdev, \
		unsigned int mem_mode) \
	{ \
		struct mtk_hcp_rsv_mb *mb = NULL; \
							\
		mb = mtk_hcp_fetch_##_mod_##_mb(pdev, mem_mode); \
		if (unlikely(!mb)) { \
			HCP_PRINT_ERR("mb is NULL\n"); \
			return 0; \
		} \
		return mb->start_dma; \
	} \
		\
	static size_t mtk_hcp_fetch_##_mod_##_mb_size( \
		struct platform_device *pdev, \
		unsigned int mem_mode) \
	{ \
		struct mtk_hcp_rsv_mb *mb = NULL; \
							\
		mb = mtk_hcp_fetch_##_mod_##_mb(pdev, mem_mode); \
		if (unlikely(!mb)) { \
			HCP_PRINT_ERR("mb is NULL\n"); \
			return 0; \
		} \
		return mb->cfg.size; \
	}

MTK_HCP_DECLARE_W_BUF_OPS(gce)
MTK_HCP_DECLARE_W_BUF_OPS(gce_clr_token)

#if (IMGSYS_INIT_INFO_VERSION == 1)
static int mtk_hcp_fill_init_info(
	struct platform_device *pdev,
	void *in)
{
	struct img_init_info *info = (struct img_init_info *)in;
	struct module_init_info *mod_init_info = NULL;
	struct mtk_hcp_rsv_mb *mb = NULL;
	unsigned int mem_mode;
	unsigned int i;

	if (unlikely(!pdev)) {
		HCP_PRINT_ERR("pdev is NULL\n");
		return -1;
	}

	if (unlikely(!info)) {
		HCP_PRINT_ERR("info is NULL\n");
		return -1;
	}

	if (info->is_capture) {
		mem_mode = IMGSYS_MEMORY_MODE_CAPTURE;
		mod_init_info = info->module_info_capture;
	} else {
		if (info->smvr_mode) {
			mem_mode = IMGSYS_MEMORY_MODE_SMVR;
			mod_init_info = info->module_info_smvr;
		} else {
			mem_mode = IMGSYS_MEMORY_MODE_NORMAL_STREAMING;
			mod_init_info = info->module_info_streaming;
		}
	}

	mb = mtk_hcp_fetch_gce_mb(pdev, mem_mode);
	if (unlikely(!mb)) {
		HCP_PRINT_ERR("mem_mode(%u) get gce_mb failed\n", mem_mode);
		return -1;
	}
	info->gce_info[mem_mode].g_wbuf_fd = mb->fd;
	info->gce_info[mem_mode].g_wbuf = mb->start_phys;
	info->gce_info[mem_mode].g_wbuf_sz = mb->cfg.size;

	HCP_PRINT_DBG("mem_mode(%u) gce_mb fd(%d) sz(%llu)\n",
		mem_mode, mb->fd, mb->cfg.size);

	HCP_PRINT_DBG("mem_mode(%u) gce_mb dma(0x%llx)\n",
		mem_mode, mb->start_phys);

	mb = mtk_hcp_fetch_gce_clr_token_mb(pdev, mem_mode);
	if (unlikely(!mb)) {
		HCP_PRINT_ERR("mem_mode(%u) get gce_clr_token_mb failed\n", mem_mode);
		return -1;
	}
	info->g_token_wbuf_fd = mb->fd;
	info->g_token_wbuf = mb->start_phys;
	info->g_token_wbuf_sz = mb->cfg.size;

	HCP_PRINT_INF("mem_mode(%u) gce_clr_token_mb fd(%d) sz(%llu)\n",
		mem_mode, mb->fd, mb->cfg.size);

	HCP_PRINT_INF("mem_mode(%u) gce_clr_token_mb dma(0x%llx)\n",
		mem_mode, mb->start_phys);

	for (i = 0; i < IMGSYS_MOD_DRV_NUM_MAX ; i++) {
		mb = mtk_hcp_fetch_mod_mb(
			pdev,
			mem_mode,
			i,
			IMGSYS_MODULE_WORKING_BUF_TYPE_CQ);
		if (unlikely(!mb)) {
			HCP_PRINT_ERR("mem_mode(%u) mod(%u) mb_type(%u) get mb failed\n",
				mem_mode, i, IMGSYS_MODULE_WORKING_BUF_TYPE_CQ);
			return -1;
		}
		mod_init_info[i].c_wbuf = mb->start_phys;
		mod_init_info[i].c_wbuf_dma = mb->start_dma;
		mod_init_info[i].c_wbuf_sz = mb->cfg.size;
		mod_init_info[i].c_wbuf_fd = mb->fd;

		mb = mtk_hcp_fetch_mod_mb(
			pdev,
			mem_mode,
			i,
			IMGSYS_MODULE_WORKING_BUF_TYPE_TDR);
		if (unlikely(!mb)) {
			HCP_PRINT_ERR("mem_mode(%u) mod(%u) mb_type(%u) get mb failed\n",
				mem_mode, i, IMGSYS_MODULE_WORKING_BUF_TYPE_TDR);
			return -1;
		}
		mod_init_info[i].t_wbuf = mb->start_phys;
		mod_init_info[i].t_wbuf_dma = mb->start_dma;
		mod_init_info[i].t_wbuf_sz = mb->cfg.size;
		mod_init_info[i].t_wbuf_fd = mb->fd;

		HCP_PRINT_INF(
			"mem_mode(%u) mod(%u) c_mb[fd(%d) sz(%u)] t_mb[fd(%d) sz(%u)]\n",
			mem_mode,
			i,
			mod_init_info[i].c_wbuf_fd,
			mod_init_info[i].c_wbuf_sz,
			mod_init_info[i].t_wbuf_fd,
			mod_init_info[i].t_wbuf_sz);
		HCP_PRINT_DBG(
			"mem_mode(%u) mod(%u) c_mb[dma(0x%llX)] t_mb[dma(0x%llX)]\n",
			mem_mode,
			i,
			mod_init_info[i].c_wbuf_dma,
			mod_init_info[i].t_wbuf_dma);
	}

	return 0;
}

#elif (IMGSYS_INIT_INFO_VERSION == 2)
static int mtk_hcp_fill_init_info(
	struct platform_device *pdev,
	void *in)
{
	struct img_init_info *info = (struct img_init_info *)in;
	struct mtk_hcp_rsv_mb *mb = NULL;
	unsigned int mem_mode;
	unsigned int i;

	if (unlikely(!pdev)) {
		HCP_PRINT_ERR("pdev is NULL\n");
		return -1;
	}

	if (unlikely(!info)) {
		HCP_PRINT_ERR("info is NULL\n");
		return -1;
	}

	mem_mode = info->memory_mode;

	mb = mtk_hcp_fetch_gce_mb(pdev, mem_mode);
	if (unlikely(!mb)) {
		HCP_PRINT_ERR("mem_mode(%u) get gce_mb failed\n", mem_mode);
		return -1;
	}
	info->gce_wb_info.wbuf_fd = mb->fd;
	info->gce_wb_info.wbuf_dma = mb->start_phys;
	info->gce_wb_info.wbuf_size = mb->cfg.size;

	HCP_PRINT_INF("mem_mode(%u) gce_mb fd(%d) sz(0x%llx)\n",
		mem_mode, mb->fd, mb->cfg.size);

	HCP_PRINT_DBG("mem_mode(%u) gce_mb dma(0x%llx)\n",
		mem_mode, mb->start_phys);

	mb = mtk_hcp_fetch_gce_clr_token_mb(pdev, mem_mode);
	if (unlikely(!mb)) {
		HCP_PRINT_ERR("mem_mode(%u) get gce_clr_token_mb failed\n", mem_mode);
		return -1;
	}
	info->gce_clr_token_wb_info.wbuf_fd = mb->fd;
	info->gce_clr_token_wb_info.wbuf_dma = mb->start_phys;
	info->gce_clr_token_wb_info.wbuf_size = mb->cfg.size;

	HCP_PRINT_DBG("mem_mode(%u) gce_clr_token_mb fd(%d) sz(0x%llx)\n",
		mem_mode, mb->fd, mb->cfg.size);

	HCP_PRINT_DBG("mem_mode(%u) gce_clr_token_mb dma(0x%llx)\n",
		mem_mode, mb->start_phys);

	for (i = 0; i < IMGSYS_MOD_DRV_NUM_MAX ; i++) {
		mb = mtk_hcp_fetch_mod_mb(
			pdev,
			mem_mode,
			i,
			IMGSYS_MODULE_WORKING_BUF_TYPE_CQ);
		if (unlikely(!mb)) {
			HCP_PRINT_ERR("mem_mode(%u) mod(%u) mb_type(%u) get mb failed\n",
				mem_mode, i, IMGSYS_MODULE_WORKING_BUF_TYPE_CQ);
			return -1;
		}
		info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_CQ].wbuf_dma
			= mb->start_dma;
		info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_CQ].wbuf_size
			= mb->cfg.size;
		info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_CQ].wbuf_fd
			= mb->fd;

		mb = mtk_hcp_fetch_mod_mb(
			pdev,
			mem_mode,
			i,
			IMGSYS_MODULE_WORKING_BUF_TYPE_TDR);
		if (unlikely(!mb)) {
			HCP_PRINT_ERR("mem_mode(%u) mod(%u) mb_type(%u) get mb failed\n",
				mem_mode, i, IMGSYS_MODULE_WORKING_BUF_TYPE_TDR);
			return -1;
		}
		info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_TDR].wbuf_dma
			= mb->start_dma;
		info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_TDR].wbuf_size
			= mb->cfg.size;
		info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_TDR].wbuf_fd
			= mb->fd;

		mb = mtk_hcp_fetch_mod_mb(
			pdev,
			mem_mode,
			i,
			IMGSYS_MODULE_WORKING_BUF_TYPE_C_MISC);
		if (unlikely(!mb)) {
			HCP_PRINT_ERR("mem_mode(%u) mod(%u) mb_type(%u) get mb failed\n",
				mem_mode, i, IMGSYS_MODULE_WORKING_BUF_TYPE_C_MISC);
			return -1;
		}
		info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_C_MISC].wbuf_dma
			= mb->start_dma;
		info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_C_MISC].wbuf_size
			= mb->cfg.size;
		info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_C_MISC].wbuf_fd
			= mb->fd;

		mb = mtk_hcp_fetch_mod_mb(
			pdev,
			mem_mode,
			i,
			IMGSYS_MODULE_WORKING_BUF_TYPE_NC_MISC);
		if (unlikely(!mb)) {
			HCP_PRINT_ERR("mem_mode(%u) mod(%u) mb_type(%u) get mb failed\n",
				mem_mode, i, IMGSYS_MODULE_WORKING_BUF_TYPE_C_MISC);
			return -1;
		}
		info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_NC_MISC].wbuf_dma
			= mb->start_dma;
		info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_NC_MISC].wbuf_size
			= mb->cfg.size;
		info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_NC_MISC].wbuf_fd
			= mb->fd;

		HCP_PRINT_DBG(
			"mem_mode(%u) mod(%u) cq_mb[fd(%d) sz(0x%x)] tdr_mb[fd(%d) sz(0x%x)] c_mb[fd(%d) sz(0x%x)] nc_mb[fd(%d) sz(0x%x)]\n",
			mem_mode,
			i,
			info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_CQ].wbuf_fd,
			info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_CQ].wbuf_size,
			info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_TDR].wbuf_fd,
			info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_TDR].wbuf_size,
			info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_C_MISC].wbuf_fd,
			info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_C_MISC].wbuf_size,
			info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_NC_MISC].wbuf_fd,
			info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_NC_MISC].wbuf_size);
		HCP_PRINT_DBG(
			"mem_mode(%u) mod(%u) cq_mb[dma(0x%llX)] t_mb[dma(0x%llX)] c_mb[dma(0x%llX)] nc_mb[dma(0x%llX)]\n",
			mem_mode,
			i,
			info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_CQ].wbuf_dma,
			info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_TDR].wbuf_dma,
			info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_C_MISC].wbuf_dma,
			info->module_wb_info[i][IMGSYS_MODULE_WORKING_BUF_TYPE_NC_MISC].wbuf_dma);
	}

	return 0;
}
#endif

#define MTK_HCP_REG_MOD_W_BUF_OPS_EXT(_mod_, _mb_) \
	.fetch_##_mod_##_##_mb_##_mb_id = mtk_hcp_fetch_##_mod_##_##_mb_##_mb_id, \
	.fetch_##_mod_##_##_mb_##_mb_fd = mtk_hcp_fetch_##_mod_##_##_mb_##_mb_fd, \
	.fetch_##_mod_##_##_mb_##_mb_virt = mtk_hcp_fetch_##_mod_##_##_mb_##_mb_virt, \
	.fetch_##_mod_##_##_mb_##_mb_phy = mtk_hcp_fetch_##_mod_##_##_mb_##_mb_phy, \
	.fetch_##_mod_##_##_mb_##_mb_dma = mtk_hcp_fetch_##_mod_##_##_mb_##_mb_dma, \
	.fetch_##_mod_##_##_mb_##_mb_size = mtk_hcp_fetch_##_mod_##_##_mb_##_mb_size,

#define MTK_HCP_REG_MOD_W_BUF_OPS_CQ(_mod_) MTK_HCP_REG_MOD_W_BUF_OPS_EXT(_mod_, cq)
#define MTK_HCP_REG_MOD_W_BUF_OPS_TDR(_mod_) MTK_HCP_REG_MOD_W_BUF_OPS_EXT(_mod_, tdr)
#define MTK_HCP_REG_MOD_W_BUF_OPS_C_MISC(_mod_) MTK_HCP_REG_MOD_W_BUF_OPS_EXT(_mod_, c_misc)
#define MTK_HCP_REG_MOD_W_BUF_OPS_NC_MISC(_mod_) MTK_HCP_REG_MOD_W_BUF_OPS_EXT(_mod_, nc_misc)

#define MTK_HCP_REG_W_BUF_OPS(_mod_) \
	.fetch_##_mod_##_mb_id = mtk_hcp_fetch_##_mod_##_mb_id, \
	.fetch_##_mod_##_mb_fd = mtk_hcp_fetch_##_mod_##_mb_fd, \
	.fetch_##_mod_##_mb_virt = mtk_hcp_fetch_##_mod_##_mb_virt, \
	.fetch_##_mod_##_mb_phy = mtk_hcp_fetch_##_mod_##_mb_phy, \
	.fetch_##_mod_##_mb_dma = mtk_hcp_fetch_##_mod_##_mb_dma, \
	.fetch_##_mod_##_mb_size = mtk_hcp_fetch_##_mod_##_mb_size,

static const struct mtk_hcp_ops hcp_ops = {
	.register_cbs = mtk_hcp_register_cbs,
	.unregister_cbs = mtk_hcp_unregister_cbs,
	.send = mtk_hcp_send,
	.send_async = mtk_hcp_send_async,
	.purge_msgs = mtk_hcp_purge_msg,
	.fill_init_info = mtk_hcp_fill_init_info,
	.flush_dma_buf = mtk_hcp_flush_dma_buf,
	.flush_mb = mtk_hcp_flush_mb,
	.write_krn_db = mtk_hcp_write_krn_db,
	.allocate_gce_mb = mtk_hcp_allocate_gce_mb,
	.free_gce_mb = mtk_hcp_free_gce_mb,
	.get_gce_mb = mtk_hcp_get_gce_mb,
	.put_gce_mb = mtk_hcp_put_gce_mb,
	.allocate_gce_clr_token_mb = mtk_hcp_allocate_gce_clr_token_mb,
	.free_gce_clr_token_mb = mtk_hcp_free_gce_clr_token_mb,
	.allocate_mod_mbs = mtk_hcp_allocate_mod_mbs,
	.free_mod_mbs = mtk_hcp_free_mod_mbs,

	MTK_HCP_REG_W_BUF_OPS(gce)
	MTK_HCP_REG_W_BUF_OPS(gce_clr_token)
	MTK_HCP_REG_MOD_W_BUF_OPS_CQ(wpe)
	MTK_HCP_REG_MOD_W_BUF_OPS_TDR(wpe)
	MTK_HCP_REG_MOD_W_BUF_OPS_C_MISC(wpe)
	MTK_HCP_REG_MOD_W_BUF_OPS_NC_MISC(wpe)
	MTK_HCP_REG_MOD_W_BUF_OPS_CQ(omc)
	MTK_HCP_REG_MOD_W_BUF_OPS_TDR(omc)
	MTK_HCP_REG_MOD_W_BUF_OPS_C_MISC(omc)
	MTK_HCP_REG_MOD_W_BUF_OPS_NC_MISC(omc)
	MTK_HCP_REG_MOD_W_BUF_OPS_CQ(adl)
	MTK_HCP_REG_MOD_W_BUF_OPS_TDR(adl)
	MTK_HCP_REG_MOD_W_BUF_OPS_C_MISC(adl)
	MTK_HCP_REG_MOD_W_BUF_OPS_NC_MISC(adl)
	MTK_HCP_REG_MOD_W_BUF_OPS_CQ(traw)
	MTK_HCP_REG_MOD_W_BUF_OPS_TDR(traw)
	MTK_HCP_REG_MOD_W_BUF_OPS_C_MISC(traw)
	MTK_HCP_REG_MOD_W_BUF_OPS_NC_MISC(traw)
	MTK_HCP_REG_MOD_W_BUF_OPS_CQ(dip)
	MTK_HCP_REG_MOD_W_BUF_OPS_TDR(dip)
	MTK_HCP_REG_MOD_W_BUF_OPS_C_MISC(dip)
	MTK_HCP_REG_MOD_W_BUF_OPS_NC_MISC(dip)
	MTK_HCP_REG_MOD_W_BUF_OPS_CQ(pqdip)
	MTK_HCP_REG_MOD_W_BUF_OPS_TDR(pqdip)
	MTK_HCP_REG_MOD_W_BUF_OPS_C_MISC(pqdip)
	MTK_HCP_REG_MOD_W_BUF_OPS_NC_MISC(pqdip)
	MTK_HCP_REG_MOD_W_BUF_OPS_CQ(me)
	MTK_HCP_REG_MOD_W_BUF_OPS_TDR(me)
	MTK_HCP_REG_MOD_W_BUF_OPS_C_MISC(me)
	MTK_HCP_REG_MOD_W_BUF_OPS_NC_MISC(me)
	MTK_HCP_REG_MOD_W_BUF_OPS_CQ(mae)
	MTK_HCP_REG_MOD_W_BUF_OPS_TDR(mae)
	MTK_HCP_REG_MOD_W_BUF_OPS_C_MISC(mae)
	MTK_HCP_REG_MOD_W_BUF_OPS_NC_MISC(mae)
	MTK_HCP_REG_MOD_W_BUF_OPS_CQ(dfp)
	MTK_HCP_REG_MOD_W_BUF_OPS_TDR(dfp)
	MTK_HCP_REG_MOD_W_BUF_OPS_C_MISC(dfp)
	MTK_HCP_REG_MOD_W_BUF_OPS_NC_MISC(dfp)
	MTK_HCP_REG_MOD_W_BUF_OPS_CQ(dpe)
	MTK_HCP_REG_MOD_W_BUF_OPS_TDR(dpe)
	MTK_HCP_REG_MOD_W_BUF_OPS_C_MISC(dpe)
	MTK_HCP_REG_MOD_W_BUF_OPS_NC_MISC(dpe)

	/* legacy deprecated op */
	.fetch_hwid_virt = NULL,
	.set_apu_dc = NULL,
};

const struct mtk_hcp_ops *mtk_hcp_fetch_ops(struct platform_device *pdev)
{
	return &hcp_ops;
}
EXPORT_SYMBOL(mtk_hcp_fetch_ops);

struct platform_device *mtk_hcp_get_plat_device(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *hcp_node = NULL;
	struct platform_device *hcp_pdev = NULL;

	if (hcp_dbg_enable())
		dev_dbg(&pdev->dev, "- E. hcp get platform device.\n");

	hcp_node = of_parse_phandle(dev->of_node, "mediatek,hcp", 0);
	if (hcp_node == NULL) {
		dev_info(&pdev->dev, "%s can't get hcp node.\n", __func__);
		return NULL;
	}

	hcp_pdev = of_find_device_by_node(hcp_node);
	if (WARN_ON(hcp_pdev == NULL) == true) {
		dev_info(&pdev->dev, "%s hcp pdev failed.\n", __func__);
		of_node_put(hcp_node);
		return NULL;
	}

	return hcp_pdev;
}
EXPORT_SYMBOL(mtk_hcp_get_plat_device);

static struct device *get_acp_dev(void)
{
	struct device *acp_dev = NULL;
	struct device_node *acp_node = NULL;
	struct platform_device *imgsys_acp_pdev = NULL;
	const char *coherent_status = NULL;
	int ret;

	acp_node = of_find_node_by_name(NULL, "imgsys-acp");
	if (unlikely(!acp_node)) {
		HCP_PRINT_WRN("Can't find acp node\n");
		return NULL;
	}
	imgsys_acp_pdev = of_find_device_by_node(acp_node);
	if (unlikely(!imgsys_acp_pdev)){
		HCP_PRINT_WRN("Can't find acp dev\n");
		return NULL;
	}

	ret = of_property_read_string(
		acp_node,
		"mediatek,imgsys-coherent",
		&coherent_status);

	if (unlikely(ret != 0)) {
		HCP_PRINT_WRN("Can't find coherent status (%s)\n", coherent_status);
		return NULL;
	}

	HCP_PRINT_DBG("Coherent status %s\n", coherent_status);
	if ((likely(strncmp(coherent_status, "enable", strlen("enable")))) == 0) {
		HCP_PRINT_INF("Coherent status enable\n");
		acp_dev = mtk_smmu_get_shared_device(&imgsys_acp_pdev->dev);
	} else {
		HCP_PRINT_WRN("Coherent status disable (%s)\n", coherent_status);
	}

	return acp_dev;
}

static int mtk_hcp_probe(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = NULL;
	struct msg *msgs = NULL;
	int ret = 0;
	int i = 0;

	dev_info(&pdev->dev, "- S. hcp driver probe.\n");
	hcp_dev = devm_kzalloc(&pdev->dev, sizeof(*hcp_dev), GFP_KERNEL);
	if (!hcp_dev) {
		ret = -ENOMEM;
		goto error;
	}

	hcp_mtkdev = hcp_dev;
	hcp_dev->dev = &pdev->dev;
	hcp_dev->mem_ops = &mtk_hcp_dma_contig_memops;

	platform_set_drvdata(pdev, hcp_dev);
	dev_set_drvdata(&pdev->dev, hcp_dev);

	/* must called after set_drvdata*/
	set_plat_op(pdev);
	hcp_dev->plat_op = of_device_get_match_data(&pdev->dev);

	hcp_dev->smmu_dev = mtk_smmu_get_shared_device(&pdev->dev);
	if (unlikely(!hcp_dev->smmu_dev)) {
		dev_info(hcp_dev->dev, "failed to get hcp smmu device\n");
		ret = -EINVAL;
		goto error;
	}

	if (likely(hcp_dev->plat_op && hcp_dev->plat_op->set_c_dev))
		hcp_dev->plat_op->set_c_dev(hcp_dev->smmu_dev);

	if (likely(hcp_dev->plat_op && hcp_dev->plat_op->set_nc_dev))
		hcp_dev->plat_op->set_nc_dev(hcp_dev->smmu_dev);

	hcp_dev->acp_dev = get_acp_dev();

	if (likely(hcp_dev->plat_op && hcp_dev->plat_op->set_acp_dev))
		hcp_dev->plat_op->set_acp_dev(hcp_dev->acp_dev);

	if (dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(34)))
		dev_info(&pdev->dev, "%s:No DMA available\n", __func__);

	if (!pdev->dev.dma_parms) {
		pdev->dev.dma_parms =
			devm_kzalloc(hcp_dev->dev, sizeof(*hcp_dev->dev->dma_parms), GFP_KERNEL);
	}
	if (hcp_dev->dev->dma_parms) {
		dma_set_max_seg_size(hcp_dev->dev, (unsigned int)DMA_BIT_MASK(34));

	} else {
		ret = -ENOMEM;
		goto error;
	}

	/* init character device */
	ret = alloc_chrdev_region(&hcp_dev->dev_no, 0, 1, HCP_DEVNAME);
	if (ret < 0) {
		dev_info(&pdev->dev, "alloc_chrdev_region failed err= %d", ret);
		goto error;
	}

	cdev_init(&hcp_dev->hcp_cdev, &hcp_fops);

	ret = cdev_add(&hcp_dev->hcp_cdev, hcp_dev->dev_no, 1);
	if (ret < 0) {
		dev_info(&pdev->dev, "cdev_add fail  err= %d", ret);
		goto error;
	}

	hcp_dev->hcp_cdev.owner = THIS_MODULE;

#if KERNEL_VERSION(6, 4, 0) <= LINUX_VERSION_CODE
	hcp_dev->hcp_class = class_create("mtk_hcp_driver");
#else
	hcp_dev->hcp_class = class_create(THIS_MODULE, "mtk_hcp_driver");
#endif

	if (IS_ERR(hcp_dev->hcp_class)) {
		ret = (int)PTR_ERR(hcp_dev->hcp_class);
		dev_info(&pdev->dev, "class create fail, err= %d", ret);
		goto error;
	}

	hcp_dev->hcp_device = device_create(hcp_dev->hcp_class, NULL,
					hcp_dev->dev_no, NULL, HCP_DEVNAME);
	if (IS_ERR(hcp_dev->hcp_device)) {
		ret = (int)PTR_ERR(hcp_dev->hcp_device);
		dev_info(&pdev->dev, "device create fail  err= %d", ret);
		goto error;
	}

	for (i = 0; i < MODULE_MAX_ID; i++) {
		init_waitqueue_head(&hcp_dev->ack_wq[i]);
		init_waitqueue_head(&hcp_dev->poll_wq[i]);
		INIT_LIST_HEAD(&hcp_dev->chans[i]);
	}

	spin_lock_init(&hcp_dev->msglock);
	init_waitqueue_head(&hcp_dev->msg_wq);
	INIT_LIST_HEAD(&hcp_dev->msg_list);

	msgs = devm_kzalloc(hcp_dev->dev, sizeof(*msgs) * HCP_MSG_NUM_MAX, GFP_KERNEL);

	if (msgs) {
		for (i = 0; i < HCP_MSG_NUM_MAX; i++)
			list_add_tail(&msgs[i].entry, &hcp_dev->msg_list);
	} else {
		ret = -ENOMEM;
		goto error;
	}

	hcp_aee_init(hcp_mtkdev);
	hcp_dev->is_open = false;

	dev_info(&pdev->dev, "- E. hcp driver probe done.\n");

error:
	if (ret != 0) {
		if (hcp_dev) {
			if (hcp_dev->hcp_device &&
			    hcp_dev->dev_no &&
			    hcp_dev->hcp_class &&
			    !IS_ERR(hcp_dev->hcp_class))
				device_destroy(hcp_dev->hcp_class, hcp_dev->dev_no);

			if (hcp_dev->hcp_class && !IS_ERR(hcp_dev->hcp_class))
				class_destroy(hcp_dev->hcp_class);

			/* We use cdev owner to note cdev_add successfully before */
			if (hcp_dev->hcp_cdev.owner == THIS_MODULE)
				cdev_del(&hcp_dev->hcp_cdev);

			if (hcp_dev->dev_no)
				unregister_chrdev_region(hcp_dev->dev_no, 1);

			if (hcp_dev->dev) {
				if (hcp_dev->dev->dma_parms)
					devm_kfree(hcp_dev->dev, hcp_dev->dev->dma_parms);
			}

			hcp_dev->dev = NULL;
			hcp_dev->mem_ops = NULL;
			hcp_dev->plat_op = NULL;
			hcp_dev->smmu_dev = NULL;
			hcp_dev->acp_dev = NULL;

			devm_kfree(&pdev->dev, hcp_dev);
		}
		hcp_mtkdev = NULL;
		dev_info(&pdev->dev, "- X. hcp driver probe fail.\n");
	}

	return ret;
}

static void mtk_hcp_remove(struct platform_device *pdev)
{
	struct mtk_hcp *hcp_dev = platform_get_drvdata(pdev);

	HCP_PRINT_DBG("- S. hcp driver remove.\n");

	hcp_aee_deinit(hcp_dev);

	if (hcp_dev->is_open == true) {
		hcp_dev->is_open = false;
		HCP_PRINT_DBG("opened device found\n");
	}
	devm_kfree(&pdev->dev, hcp_dev);

	cdev_del(&hcp_dev->hcp_cdev);
	unregister_chrdev_region(hcp_dev->dev_no, 1);

	HCP_PRINT_DBG("- E. hcp driver removed.\n");
}

static const struct of_device_id mtk_hcp_match[] = {
	{.compatible = "mediatek,hcp8s_f", .data = (void *)&plat_op_isp8s_f},
	{.compatible = "mediatek,hcp8s_p", .data = (void *)&plat_op_isp8s_p},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_hcp_match);

static struct platform_driver mtk_hcp_driver = {
	.probe  = mtk_hcp_probe,
	.remove = mtk_hcp_remove,
	.driver = {
		.name = HCP_DEVNAME,
		.owner  = THIS_MODULE,
		.of_match_table = mtk_hcp_match,
	},
};

module_platform_driver(mtk_hcp_driver);

#if (KERNEL_VERSION(6, 13, 0) > LINUX_VERSION_CODE)
MODULE_IMPORT_NS(DMA_BUF);
#else
MODULE_IMPORT_NS("DMA_BUF");
#endif
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek hetero control process driver");
