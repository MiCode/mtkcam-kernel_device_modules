/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_PDA_H
#define __MTK_CAM_PDA_H

#include <linux/kfifo.h>
#include <linux/suspend.h>

#include "mtk_cam-plat.h"
#include "mtk_cam-engine.h"

#define PDA_IRQ_NUM (1)
#define PDA_ERR_MAX (4)

union _pda_otf_cfg_0_ {
	struct /* 0x0000 */
	{
		unsigned int  PDA_WIDTH        :  16;   /*  0.. 16, 0x0000FFFF */
		unsigned int  PDA_HEIGHT       :  16;   /*  16.. 32, 0xFFFF0080 */
	} Bits;
	unsigned int Raw;
};

union _pda_otf_cfg_1_ {
	struct /* 0x0004 */
	{
		unsigned int  PDA_EN           :  1;    /*  0.. 0, 0x00000001 */
		unsigned int  PDA_PR_XNUM      :  5;    /*  1.. 5, 0x0000003E */
		unsigned int  PDA_PR_YNUM      :  5;    /*  6.. 10, 0x00007C0 */
		unsigned int  PDA_PAT_WIDTH    :  10;   /*  11.. 20, 0x001FF800 */
		unsigned int  PDA_BIN_FCTR     :  2;    /*  21.. 22, 0x00600000 */
		unsigned int  PDA_RNG_ST       :  6;    /*  23.. 28, 0x1F800080 */
		unsigned int  PDA_SHF_0        :  3;    /*  29.. 31, 0xE0000000 */
	} Bits;
	unsigned int Raw;
};

union _pda_otf_cfg_2_ {
	struct /* 0x0008 */
	{
		unsigned int  PDA_SHF_1        :  3;
		unsigned int  PDA_SHF_2        :  3;
		unsigned int  PDA_RGN_NUM      :  7;
		unsigned int  PDA_TBL_STRIDE   :  16;
		unsigned int  rsv_29           :  3;
	} Bits;
	unsigned int Raw;
};

union _pda_otf_cfg_18_ {
	struct /* 0x0048 */
	{
		unsigned int  PDA_FILT_C_12    :  7;
		unsigned int  PDA_GRAD_DST     :  4;
		unsigned int  PDA_GRAD_THD     :  12;
		unsigned int  rsv_23           :  9;
	} Bits;
	unsigned int Raw;
};

enum MTK_PDA_IRQ_EVENT {
	/* with normal_data */
	PDA_IRQ_PDA_DONE = 0,
	/* with error_data */
	PDA_IRQ_ERROR,
	PDA_IRQ_DEBUG_1,
};

enum pda_err_idx {
	PDA_ERR_START = 0,
	PDA_ERR_HANG,
	PDA_ERR_2,
	PDA_ERR_3,
	PDA_ERR_END,
};

struct mtk_pda_device {
	struct device *dev;
	struct mtk_cam_device *cam;
	unsigned int id;
	int irq[PDA_IRQ_NUM];
	void __iomem *base;
	void __iomem *base_inner;
	unsigned int num_clks;
	struct clk **clks;

	int fifo_size;
	void *msg_buffer;
	struct kfifo msg_fifo;
	atomic_t is_fifo_overflow;

#ifdef CONFIG_PM_SLEEP
	struct notifier_block notifier_blk;
#endif
	/* mmqos */
	unsigned int num_larbs;
	struct platform_device **larb_pdev;
	struct mtk_camsys_qos qos;

	// pda done, used to notify camsv
	u64 ts_ns;

	// irq: pda done or error
	int irq_type;

	// error type
	unsigned int err_tags;

	// irq error2 record continuously
	unsigned int err2_count;
};
int mtk_cam_pda_dev_config(struct mtk_pda_device *pda_dev);
void pda_reset(struct mtk_pda_device *pda_dev);
int mtk_pda_runtime_suspend(struct device *dev);
int mtk_pda_runtime_resume(struct device *dev);
extern struct platform_driver mtk_cam_pda_driver;
#endif
