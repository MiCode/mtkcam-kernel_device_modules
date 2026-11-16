// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 *
 * Author: Ming-Hsuan Chiang <ming-hsuan.chiang@mediatek.com>
 *
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of_address.h>
#include "mtk_imgsys-mae.h"
#include "mtk_imgsys-v4l2-debug.h"
#include "mtk-hcp.h"

/*******************************************************************************
 * Global Define
 *******************************************************************************/
static void __iomem *g_maeRegBA;

#define MAE_BASE  0x34310000
// 0x1000 MAE_CTRL_CENTER
#define MAE_CTRL_CENTER_BASE              (0x1000)
#define MAE_CTRL_CENTER_LEN               (0x308)

// 0x4200 MAE_RSZ0
#define MAE_MAISR_BASE                    (0x4200)
#define MAE_MAISR_LEN                     (0x0600)

// 0x4800 MAE_RSZ0
#define MAE_RSZ0_BASE                     (0x4800)
#define MAE_RSZ0_LEN                      (0x01F4)

// 0x4C00 MAE_UDMA_W
#define MAE_UDMA_W_BASE                   (0x4C00)
#define MAE_UDMA_W_LEN                    (0x01FC)

// 0x5000 MAE_CMP
#define MAE_CMP_BASE                      (0x5000)
#define MAE_CMP_LEN                       (0x0190)

// 0x5200 MAE_UDMA_R
#define MAE_UDMA_R_BASE                   (0x5200)
#define MAE_UDMA_R_LEN                    (0x1FC)

// 0x5800 MAE_RDMA_5
#define MAE_RDMA_5_BASE                   (0x5800)
#define MAE_RDMA_5_LEN                    (0x1FC)

// 0X6000 RSZ1
#define RSZ1_BASE                         (0x6000)
#define RSZ1_LEN                          (0x1FC)

// 0X6200 RSZ2
#define RSZ2_BASE                         (0x6200)
#define RSZ2_LEN                          (0x1FC)

// 0X6400 RSZ3
#define RSZ3_BASE                         (0x6400)
#define RSZ3_LEN                          (0x1FC)

// 0X6A04 MAISR_APB_BASE
#define MAE_MAISR_APB_BASE                (0x6A00)
#define MAE_MAISR_APB_LEN                 (0x1FC)

// 0x7000 MMFD_POST
#define MMFD_POST_BASE                    (0x7000)
#define MMFD_POST_LEN                     (0x0114)

// 0x7400 MAE_DRV
#define MAE_DRV_W_BASE                    (0x7400)
#define MAE_DRV_W_LEN                     (0x0164)

// 0x7600 MAE_DRV_R
#define MAE_DRV_R_BASE                    (0x7600)
#define MAE_DRV_R_LEN                     (0x01FC)

/*******************************************************************************
 * static Functions
 *******************************************************************************/
void mae_reg_dump(void)
{
	void __iomem *maeRegBA = 0L;
	int i = 0;

	/* iomap registers */
	maeRegBA = g_maeRegBA;
	if (!maeRegBA) {
		pr_info("%s Unable to ioremap MAE registers\n",
			__func__);
		return;
	}

	// 0x1000 MAE_CTRL_CENTER
	for (i = MAE_CTRL_CENTER_BASE; i <= MAE_CTRL_CENTER_BASE + MAE_CTRL_CENTER_LEN; i += 0x10) {
		pr_info("%s: [0x%08X] %08X, %08X, %08X, %08X", __func__,
			(unsigned int)(MAE_BASE + i),
			(unsigned int)ioread32((void *)(maeRegBA + i)),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x4))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x8))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0xC))));
	}

	// 0x4200 MAE_RSZ0
	for (i = MAE_MAISR_BASE; i <= MAE_MAISR_BASE + MAE_MAISR_LEN; i += 0x10) {
		pr_info("%s: [0x%08X] %08X, %08X, %08X, %08X", __func__,
			(unsigned int)(MAE_BASE + i),
			(unsigned int)ioread32((void *)(maeRegBA + i)),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x4))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x8))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0xC))));
	}

	// 0x4800 MAE_RSZ0
	for (i = MAE_RSZ0_BASE; i <= MAE_RSZ0_BASE + MAE_RSZ0_LEN; i += 0x10) {
		pr_info("%s: [0x%08X] %08X, %08X, %08X, %08X", __func__,
			(unsigned int)(MAE_BASE + i),
			(unsigned int)ioread32((void *)(maeRegBA + i)),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x4))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x8))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0xC))));
	}

	// 0x4C00 MAE_UDMA_W
	for (i = MAE_UDMA_W_BASE; i <= MAE_UDMA_W_BASE + MAE_UDMA_W_LEN; i += 0x10) {
		pr_info("%s: [0x%08X] %08X, %08X, %08X, %08X", __func__,
			(unsigned int)(MAE_BASE + i),
			(unsigned int)ioread32((void *)(maeRegBA + i)),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x4))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x8))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0xC))));
	}

	// 0x5000 MAE_CMP
	for (i = MAE_CMP_BASE; i <= MAE_CMP_BASE + MAE_CMP_LEN; i += 0x10) {
		pr_info("%s: [0x%08X] %08X, %08X, %08X, %08X", __func__,
			(unsigned int)(MAE_BASE + i),
			(unsigned int)ioread32((void *)(maeRegBA + i)),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x4))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x8))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0xC))));
	}

	// 0x5200 MAE_UDMA_R
	for (i = MAE_UDMA_R_BASE; i <= MAE_UDMA_R_BASE + MAE_UDMA_R_LEN; i += 0x10) {
		pr_info("%s: [0x%08X] %08X, %08X, %08X, %08X", __func__,
			(unsigned int)(MAE_BASE + i),
			(unsigned int)ioread32((void *)(maeRegBA + i)),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x4))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x8))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0xC))));
	}

	// 0x5800 MAE_RDMA_5
	for (i = MAE_RDMA_5_BASE; i <= MAE_RDMA_5_BASE + MAE_RDMA_5_LEN; i += 0x10) {
		pr_info("%s: [0x%08X] %08X, %08X, %08X, %08X", __func__,
			(unsigned int)(MAE_BASE + i),
			(unsigned int)ioread32((void *)(maeRegBA + i)),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x4))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x8))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0xC))));
	}

	// 0X6000 RSZ1
	for (i = RSZ1_BASE; i <= RSZ1_BASE + RSZ1_LEN; i += 0x10) {
		pr_info("%s: [0x%08X] %08X, %08X, %08X, %08X", __func__,
			(unsigned int)(MAE_BASE + i),
			(unsigned int)ioread32((void *)(maeRegBA + i)),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x4))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x8))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0xC))));
	}

	// 0X6200 RSZ2
	for (i = RSZ2_BASE; i <= RSZ2_BASE + RSZ2_LEN; i += 0x10) {
		pr_info("%s: [0x%08X] %08X, %08X, %08X, %08X", __func__,
			(unsigned int)(MAE_BASE + i),
			(unsigned int)ioread32((void *)(maeRegBA + i)),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x4))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x8))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0xC))));
	}

	// 0X6400 RSZ3
	for (i = RSZ3_BASE; i <= RSZ3_BASE + RSZ3_LEN; i += 0x10) {
		pr_info("%s: [0x%08X] %08X, %08X, %08X, %08X", __func__,
			(unsigned int)(MAE_BASE + i),
			(unsigned int)ioread32((void *)(maeRegBA + i)),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x4))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x8))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0xC))));
	}

	// 0X6A04 MAISR_APB_BASE
	for (i = MAE_MAISR_APB_BASE; i <= MAE_MAISR_APB_BASE + MAE_MAISR_APB_LEN; i += 0x10) {
		pr_info("%s: [0x%08X] %08X, %08X, %08X, %08X", __func__,
			(unsigned int)(MAE_BASE + i),
			(unsigned int)ioread32((void *)(maeRegBA + i)),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x4))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x8))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0xC))));
	}

	// 0x7000 MMFD_POST
	for (i = MMFD_POST_BASE; i <= MMFD_POST_BASE + MMFD_POST_LEN; i += 0x10) {
		pr_info("%s: [0x%08X] %08X, %08X, %08X, %08X", __func__,
			(unsigned int)(MAE_BASE + i),
			(unsigned int)ioread32((void *)(maeRegBA + i)),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x4))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x8))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0xC))));
	}

	// 0x7400 MAE_DRV
	for (i = MAE_DRV_W_BASE; i <= MAE_DRV_W_BASE + MAE_DRV_W_LEN; i += 0x10) {
		pr_info("%s: [0x%08X] %08X, %08X, %08X, %08X", __func__,
			(unsigned int)(MAE_BASE + i),
			(unsigned int)ioread32((void *)(maeRegBA + i)),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x4))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x8))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0xC))));
	}

	// 0x7600 MAE_DRV_R
	for (i = MAE_DRV_R_BASE; i <= MAE_DRV_R_BASE + MAE_DRV_R_LEN; i += 0x10) {
		pr_info("%s: [0x%08X] %08X, %08X, %08X, %08X", __func__,
			(unsigned int)(MAE_BASE + i),
			(unsigned int)ioread32((void *)(maeRegBA + i)),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x4))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0x8))),
			(unsigned int)ioread32((void *)(maeRegBA + (i+0xC))));
	}
}

/*******************************************************************************
 * Functions
 *******************************************************************************/
int MAE_TranslationFault_callback(int port, dma_addr_t mva, void *data)
{
	pr_info("%s: dump mae regs\n", __func__);
	mae_reg_dump();

	return 1;
}

void imgsys_mae_init(struct mtk_imgsys_dev *imgsys_dev)
{
	pr_info("%s: +\n", __func__);

	g_maeRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_MAE);
}

void imgsys_mae_set(struct mtk_imgsys_dev *imgsys_dev)
{
	// set default setting if need
}

void imgsys_mae_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
			unsigned int engine)
{
	dev_info(imgsys_dev->dev, "%s: dump mae regs\n", __func__);
	mae_reg_dump();
}

bool imgsys_mae_done_chk(struct mtk_imgsys_dev *imgsys_dev, uint32_t engine)
{
	bool ret = true; //true: done

	uint32_t value = 0;

	value = (uint32_t)ioread32((void *)(g_maeRegBA + 0x1018));
	if (!(value & 0x1)) {
		ret = false;
		pr_info(
		"%s: hw_comb:0x%x, polling ME done fail!!! [0x%08x] 0x%x",
		__func__, engine,
		(uint32_t)(MAE_BASE + 0x1018), value);
	}

	return ret;
}

void imgsys_mae_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	pr_info("%s: +\n", __func__);

	if (g_maeRegBA) {
		iounmap(g_maeRegBA);
		g_maeRegBA = 0L;
	}
}
