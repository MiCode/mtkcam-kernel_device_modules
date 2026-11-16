// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/compat.h>
#include <linux/dma-mapping.h>
#include <linux/kmemleak.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/remoteproc.h>
#include <linux/platform_data/mtk_ccd.h>
#include <linux/remoteproc/mtk_ccd_mem.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <uapi/linux/mtk_ccd_controls.h>
#include <linux/version.h>

#include "remoteproc_internal.h"
#include "iommu_debug.h"

#define CCD_DEV_NAME	"mtk_ccd"

#undef dev_dbg
#define dev_dbg(dev, fmt, arg...)			\
	do {						\
		if (unlikely(mtk_ccd_debug_enabled()))	\
			dev_info(dev, fmt, ## arg);	\
	} while (0)

static int ccd_load(struct rproc *rproc, const struct firmware *fw)
{
	const struct mtk_ccd *ccd = rproc->priv;
	struct device *dev = ccd->dev;
	int ret = 0;

	dev_info(dev, "%s: %p\n", __func__, dev);

	return ret;
}

static int ccd_start(struct rproc *rproc)
{
	struct mtk_ccd *ccd = (struct mtk_ccd *)rproc->priv;
	struct device *dev = ccd->dev;
	int ret = 0;

	dev_info(dev, "%s: %p\n", __func__, dev);

	return ret;
}

static int ccd_stop(struct rproc *rproc)
{
	struct mtk_ccd *ccd = (struct mtk_ccd *)rproc->priv;
	struct device *dev = ccd->dev;
	int ret = 0;

	dev_info(dev, "%s: %p\n", __func__, dev);

	return ret;
}

static const struct rproc_ops ccd_ops = {
	.start		= ccd_start,
	.stop		= ccd_stop,
	.load		= ccd_load,
};

static struct mtk_ccd_rpmsg_ops ccd_rpmsg_ops = {
	.ccd_send = rpmsg_ccd_ipi_send,
};

static void ccd_create_channel_center(struct mtk_ccd *ccd)
{
	int i;

	for (i = 0; i < MAX_RPROC_SUBDEV_NUM; i++) {
		struct rproc_subdev *subdev;

		subdev = mtk_rpmsg_create_rproc_subdev(to_platform_device(ccd->dev),
						       &ccd_rpmsg_ops,
						       i);
		if (subdev) {
			ccd->channel_center[i] = subdev;
			rproc_add_subdev(ccd->rproc, ccd->channel_center[i]);
			mtk_ccd_center_create_channels(ccd->channel_center[i]);
		}
	}
}

static void ccd_destroy_channel_center(struct mtk_ccd *ccd)
{
	int i;

	for (i = 0; i < MAX_RPROC_SUBDEV_NUM; i++) {
		struct rproc_subdev *subdev = ccd->channel_center[i];

		if (!subdev)
			continue;

		mtk_ccd_center_destroy_channels(subdev);
		rproc_remove_subdev(ccd->rproc, subdev);
		mtk_rpmsg_destroy_rproc_subdev(subdev);
		ccd->channel_center[i] = NULL;
	}
}

static ssize_t ccd_debug_read(struct file *filp,
			      char __user *user_buf,
			      size_t count, loff_t *ppos)
{
	char buf[256];
	u32  len = 0;

	len = snprintf(buf, sizeof(buf), "ccu_debug_read\n");
	if (len >= sizeof(buf))
		pr_info("%s: snprintf fail\n", __func__);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ccd_debug_write(struct file *filp,
			       const char __user *buffer,
			       size_t count, loff_t *data)
{
	char desc[64];
	s32 len = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	return count;
}

static int ccd_open(struct inode *inode,
		    struct file *filp)
{
	int ret = 0;

	struct mtk_ccd *ccd = container_of(inode->i_cdev,
					   struct mtk_ccd,
					   ccd_cdev);
	filp->private_data = ccd;

	dev_info(ccd->dev, "%s: %p, cnt:%d",
		__func__, ccd, atomic_inc_return(&ccd->open_cnt));

	return ret;
}

static int ccd_release(struct inode *inode,
		       struct file *filp)
{
	int ret = 0;
	struct mtk_ccd *ccd = (struct mtk_ccd *)filp->private_data;

	ccd_master_destroy(ccd);  /* TODO: do not destroy ept here */

	dev_info(ccd->dev, "%s: %p, cnt:%d",
		__func__, ccd, atomic_dec_return(&ccd->open_cnt));

	return ret;
}

static long ccd_unlocked_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
	long ret = 0;
	struct mtk_ccd *ccd = (struct mtk_ccd *)filp->private_data;
	unsigned char *user_addr = (unsigned char *)arg;
	struct ccd_master_listen_item listen_obj;
	struct ccd_worker_item work_obj;
	struct ccd_master_status_item master_obj;

	/**
	 * no need memset for listen_obj/work_obj/master_obj,
	 * all of them would be overwrited.
	 */

	switch (cmd) {
	case IOCTL_CCD_MASTER_INIT:
		if (ccd_master_init(ccd)) {
			ret = -EFAULT;  /* no free rproc_subdev */
			break;
		}
		master_obj.state = CCD_MASTER_ACTIVE;

		if (copy_to_user(user_addr, &master_obj, sizeof(master_obj)))
			ret = -EFAULT;

		break;
	case IOCTL_CCD_MASTER_DESTROY:
		if (ccd_master_destroy(ccd)) {
			ret = -EFAULT;  /* no match subdev */
			break;
		}
		master_obj.state = CCD_MASTER_EXIT;

		if (copy_to_user(user_addr, &master_obj, sizeof(master_obj)))
			ret = -EFAULT;

		break;
	case IOCTL_CCD_MASTER_LISTEN:
		/* per stremaing ctrl for on/off */
		if (ccd_master_listen(ccd, &listen_obj)) {  /* wait for ON/OFF */
			ret = -EFAULT;
			break;
		}

		if (copy_to_user(user_addr, &listen_obj,
				 sizeof(struct ccd_master_listen_item)))
			ret = -EFAULT;

		break;
	case IOCTL_CCD_WORKER_READ:
		/* backend read msg */
		if (copy_from_user(&work_obj, user_addr,
				   sizeof(struct ccd_worker_item))) {
			ret = -EFAULT;
			break;
		}

		if (ccd_worker_read(ccd, &work_obj)) {
			ret = -EFAULT;
			break;
		}

		if (copy_to_user(user_addr, &work_obj,
				 sizeof(struct ccd_worker_item)))
			ret = -EFAULT;

		break;
	case IOCTL_CCD_WORKER_WRITE:
		/* backend write ack */
		if (copy_from_user(&work_obj, user_addr,
				   sizeof(struct ccd_worker_item))) {
			ret = -EFAULT;
			break;
		}

		if (ccd_worker_write(ccd, &work_obj))
			ret = -EFAULT;

		break;
	default:
		dev_info(ccd->dev, "Unknown ioctl\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long ccd_ioctl_compat(struct file *filp,
			     unsigned int cmd,
			     unsigned long arg)
{
	long ret = 0;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	ret = filp->f_op->unlocked_ioctl(filp,
					 cmd,
					 (unsigned long)compat_ptr(arg));

	return ret;
}
#endif

static const struct file_operations ccd_fops = {
	.owner = THIS_MODULE,
	.open = ccd_open,
	.release = ccd_release,
	.read = ccd_debug_read,
	.write = ccd_debug_write,
	.unlocked_ioctl = ccd_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ccd_ioctl_compat,
#endif
};

static int ccd_regcdev(struct mtk_ccd *ccd)
{
	int ret = 0;
	struct device *dev;

	ret = alloc_chrdev_region(&ccd->ccd_devno, 0, 1, CCD_DEV_NAME);
	if (ret < 0) {
		dev_info(ccd->dev, "%s: alloc_chrdev_region failed, %d\n",
			__func__, ret);
		return ret;
	}

	/* Attatch file operation. */
	cdev_init(&ccd->ccd_cdev, &ccd_fops);
	ccd->ccd_cdev.owner = THIS_MODULE;
	/* Add to system */
	ret = cdev_add(&ccd->ccd_cdev, ccd->ccd_devno, 1);
	if (ret < 0) {
		dev_info(ccd->dev, "%s: cdev_add failed, %d\n",
			__func__, ret);
		goto err_cdev_add;
	}

	/* Create class register */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	ccd->ccd_class = class_create("mtk_ccd");
#else
	ccd->ccd_class = class_create(THIS_MODULE, "mtk_ccd");
#endif
	if (IS_ERR(ccd->ccd_class)) {
		ret = PTR_ERR(ccd->ccd_class);
		dev_info(ccd->dev, "%s: class_create failed, %d\n",
			__func__, ret);
		goto err_class_create;
	}

	dev = device_create(ccd->ccd_class, NULL,
			    ccd->ccd_devno, NULL,
			    CCD_DEV_NAME);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		dev_info(ccd->dev, "%s: device_create failed: /dev/%s, err = %d\n",
			__func__, CCD_DEV_NAME, ret);
		goto err_device_create;
	}

	return ret;

err_device_create:
	class_destroy(ccd->ccd_class);
	ccd->ccd_class = NULL;
err_class_create:
	cdev_del(&ccd->ccd_cdev);
err_cdev_add:
	unregister_chrdev_region(ccd->ccd_devno, 1);
	return ret;
}

static void ccd_unregcdev(struct mtk_ccd *ccd)
{
	/* Release char driver */
	if (ccd->ccd_class) {
		device_destroy(ccd->ccd_class, ccd->ccd_devno);
		class_destroy(ccd->ccd_class);
		ccd->ccd_class = NULL;
	}

	cdev_del(&ccd->ccd_cdev);
	unregister_chrdev_region(ccd->ccd_devno, 1);
}

static int ccd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *alloc_dev;
	struct device_node *np = dev->of_node;
	struct mtk_ccd *ccd;
	struct rproc *rproc;
	char *fw_name = "remoteproc_scp";
	int ret;

	rproc = rproc_alloc(dev,
			    np->name,
			    &ccd_ops,
			    fw_name,
			    sizeof(*ccd));
	if (!rproc) {
		dev_info(dev, "unable to allocate remoteproc\n");
		return -ENOMEM;
	}

	ccd = (struct mtk_ccd *)rproc->priv;
	ccd->rproc = rproc;
	ccd->dev = dev;
	ccd->smmu_dev = NULL;

	if (smmu_v3_enabled()) {
		ccd->smmu_dev = mtk_smmu_get_shared_device(&pdev->dev);
		if (!ccd->smmu_dev) {
			dev_info(dev, "failed to get smmu device\n");
			ret = -ENODEV;
			goto free_rproc;
		}
	}

	alloc_dev = ccd->smmu_dev ? : dev;
	if (dma_set_mask_and_coherent(alloc_dev, DMA_BIT_MASK(34)))
		dev_info(dev, "No suitable DMA available\n");

	if (!alloc_dev->dma_parms) {
		alloc_dev->dma_parms =
			devm_kzalloc(alloc_dev,
				sizeof(*alloc_dev->dma_parms), GFP_KERNEL);
		if (!alloc_dev->dma_parms) {
			ret = -ENODEV;
			goto free_rproc;
		}
	}

	if (alloc_dev->dma_parms)
		dma_set_max_seg_size(alloc_dev, UINT_MAX);

	platform_set_drvdata(pdev, ccd);

	if (ccd_regcdev(ccd)) {
		dev_info(dev, "Register cdev failed\n");
		ret = -ENODEV;
		goto free_rproc;
	}

	/* If ccd is moved to real micro processor, map to physical address here */

	ccd_create_channel_center(ccd);

	ccd->ccd_memory = mtk_ccd_mem_init(ccd->dev);

	ret = rproc_add(rproc);
	if (ret) {
		dev_info(dev, "rproc_add failed\n");
		goto remove_subdev;
	}

	dev_info(dev, "%s: ccd is created: %p\n", __func__, ccd);

	return 0;

remove_subdev:
	mtk_ccd_mem_release(ccd);
	ccd_destroy_channel_center(ccd);
	ccd_unregcdev(ccd);
free_rproc:
	rproc_free(rproc);

	return ret;
}

static void ccd_remove(struct platform_device *pdev)
{
	struct mtk_ccd *ccd = platform_get_drvdata(pdev);

	dev_info(ccd->dev, "%s: release ccd: %p", __func__, ccd);

	mtk_ccd_mem_release(ccd);
	ccd_unregcdev(ccd);
	ccd_destroy_channel_center(ccd);
	rproc_del(ccd->rproc);
	rproc_free(ccd->rproc);
}

static const struct of_device_id mtk_ccd_of_match[] = {
	{ .compatible = "mediatek,ccd"},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_ccd_of_match);

static struct platform_driver mtk_ccd_driver = {
	.probe = ccd_probe,
	.remove = ccd_remove,
	.driver = {
		.name = CCD_DEV_NAME,
		.of_match_table = of_match_ptr(mtk_ccd_of_match),
	},
};

static int __init ccd_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_ccd_driver);
	return ret;
}

static void __exit ccd_exit(void)
{
	platform_driver_unregister(&mtk_ccd_driver);
}

late_initcall(ccd_init);
module_exit(ccd_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek CCD driver");
