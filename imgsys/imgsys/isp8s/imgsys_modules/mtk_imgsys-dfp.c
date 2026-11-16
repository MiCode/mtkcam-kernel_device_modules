// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Marvin Lin <Marvin.Lin@mediatek.com>
 *
 */
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include "./../mtk_imgsys-engine-isp8s.h"
#include "mtk_imgsys-dfp.h"
#include "iommu_debug.h"
#include "mtk_imgsys-v4l2-debug.h"
#include "mtk-hcp.h"

// GCE header
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "../../cmdq/isp8s/mtk_imgsys-cmdq-qof.h"

struct mtk_imgsys_dfp_dtable {
	uint32_t empty;
	uint32_t addr;
	uint32_t addr_msb;
};

const struct mtk_imgsys_init_array mtk_imgsys_dfp1_init_ary[] = {
	{0x158, 0x80000000}, /* FE FM DDREN */
};

const struct mtk_imgsys_init_array mtk_imgsys_dfp2_init_ary[] = {
	{0x158, 0x80000000}, /* DVGF DDREN */
};

//static struct ipesys_dfp_device *me_dev;
static void __iomem *g_dfptopRegBA;
static void __iomem *g_feRegBA;
static void __iomem *g_drzh2nRegBA;
static void __iomem *g_fmRegBA;
static void __iomem *g_dfp2topRegBA;
static void __iomem *g_dvgfRegBA;
static void __iomem *gVcoreRegBA;

int imgsys_dfp_tfault_callback(int port, dma_addr_t mva, void *data)
{

	void __iomem *feRegBA = 0L;
	void __iomem *fmRegBA = 0L;
	void __iomem *dvgfRegBA = 0L;
	unsigned int i;
	int ret = 0;
	bool is_qof = false;

	ret = smi_isp_wpe3_lite_get_if_in_use((void *)&is_qof);
	if (ret == -1) {
		pr_info("smi_isp_wpe3_lite_get_if_in_use = -1.stop dump. return\n");
		return 1;
	}

	/* iomap registers */
	feRegBA = g_feRegBA;
	fmRegBA = g_fmRegBA;
	dvgfRegBA = g_dvgfRegBA;
	if (!feRegBA | !fmRegBA | !dvgfRegBA) {
		pr_info("%s Unable to ioremap DFP registers\n",
		__func__);
		return -1;
	}

	pr_info("FE register dump");
	for (i = FE_CTL_OFFSET; i <= FE_CTL_RANGE; i += 0x10) {
		pr_info("%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(DFP_FE_BASE + i),
		(unsigned int)ioread32((void *)(feRegBA + i)),
		(unsigned int)ioread32((void *)(feRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(feRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(feRegBA + (i+0xC))));
	}

	pr_info("FM register dump");
	for (i = FM_CTL_OFFSET; i <= FM_CTL_RANGE; i += 0x10) {
		pr_info("%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(DFP_FM_BASE + i),
		(unsigned int)ioread32((void *)(fmRegBA + i)),
		(unsigned int)ioread32((void *)(fmRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(fmRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(fmRegBA + (i+0xC))));
	}

	pr_info("DVGF register dump");
	for (i = DVGF_CTL_OFFSET; i <= DVGF_CTL_RANGE; i += 0x10) {
		pr_info("%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(DFP_DVGF_BASE + i),
		(unsigned int)ioread32((void *)(dvgfRegBA + i)),
		(unsigned int)ioread32((void *)(dvgfRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(dvgfRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(dvgfRegBA + (i+0xC))));
	}

	pr_info("[gals_tx dgb addr] 0x34780048 = 0x%08X",
		(unsigned int)ioread32((void *)(gVcoreRegBA + 0x48)));
	pr_info("[gals_tx dgb addr] 0x3478004C = 0x%08X",
		(unsigned int)ioread32((void *)(gVcoreRegBA + 0x4C)));
	pr_info("[gals_tx dgb addr] 0x34780050 = 0x%08X",
		(unsigned int)ioread32((void *)(gVcoreRegBA + 0x50)));
	pr_info("[gals_tx dgb addr] 0x34780054 = 0x%08X",
		(unsigned int)ioread32((void *)(gVcoreRegBA + 0x54)));
	pr_info("[ACK dgb addr] 0x347800C4 = 0x%08X",
		(unsigned int)ioread32((void *)(gVcoreRegBA + 0xC4)));
	pr_info("[ACK dgb addr] 0x347800C8 = 0x%08X",
		(unsigned int)ioread32((void *)(gVcoreRegBA + 0xC8)));
	pr_info("[ACK dgb addr] 0x347800CC = 0x%08X",
		(unsigned int)ioread32((void *)(gVcoreRegBA + 0xCC)));

	smi_isp_wpe3_lite_put((void *)&is_qof);

	return 1;
}

void imgsys_dfp_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	#ifdef ME_CLK_CTRL
	int ret;
	#endif

	pr_info("%s: +\n", __func__);

	g_dfptopRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_DFP_TOP);
	g_feRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_DFP_FE);
	g_drzh2nRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_DFP_DRZH2N);
	g_fmRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_DFP_FM);
	g_dfp2topRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_DFP2_TOP);
	g_dvgfRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_DFP2_DVGF);
	/* iomap registers */
	gVcoreRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_IMG_VCORE);
	if (!gVcoreRegBA) {
		pr_info("%s:unable to iomap Vcore reg, devnode()\n",
			__func__);
	}

	pr_info("%s: -\n", __func__);
}
//EXPORT_SYMBOL(imgsys_dfp_set_initial_value);

void imgsys_dfp_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	if (g_dfptopRegBA) {
		iounmap(g_dfptopRegBA);
		g_dfptopRegBA = 0L;
	}

	if (g_feRegBA) {
		iounmap(g_feRegBA);
		g_feRegBA = 0L;
	}

	if (g_drzh2nRegBA) {
		iounmap(g_drzh2nRegBA);
		g_drzh2nRegBA = 0L;
	}

	if (g_fmRegBA) {
		iounmap(g_fmRegBA);
		g_fmRegBA = 0L;
	}

	if (g_dfp2topRegBA) {
		iounmap(g_dfp2topRegBA);
		g_dfp2topRegBA = 0L;
	}

	if (g_dvgfRegBA) {
		iounmap(g_dvgfRegBA);
		g_dvgfRegBA = 0L;
	}

	if (gVcoreRegBA) {
		iounmap(gVcoreRegBA);
		gVcoreRegBA = 0L;
	}
}
//EXPORT_SYMBOL(imgsys_dfp_uninit);

void imgsys_dfp_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
			unsigned int engine)
{
	void __iomem *dfptopRegBA = 0L;
	void __iomem *feRegBA = 0L;
	void __iomem *drzh2nRegBA = 0L;
	void __iomem *fmRegBA = 0L;
	void __iomem *dfp2topRegBA = 0L;
	void __iomem *dvgfRegBA = 0L;
	unsigned int i;

	/* iomap registers */
	dfptopRegBA = g_dfptopRegBA;
	if (!dfptopRegBA ) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap DFP TOP registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
		return;
	}
	feRegBA = g_feRegBA;
	if (!feRegBA ) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap FE registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
		return;
	}
	drzh2nRegBA = g_drzh2nRegBA;
	if (!drzh2nRegBA ) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap DRZH2N registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
		return;
	}
	fmRegBA = g_fmRegBA;
	if (!fmRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap FM registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
		return;
	}
	dfp2topRegBA = g_dfp2topRegBA;
	if (!dfp2topRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap DFP2 TOP registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
		return;
	}
	dvgfRegBA = g_dvgfRegBA;
	if (!dvgfRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap DVGF registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
		return;
	}


	pr_info("DFP TOP register dump");
	for (i = DFP_TOP_CTL_OFFSET; i <= DFP_TOP_CTL_RANGE; i += 0x10) {
		pr_info("%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(DFP_TOP_BASE + i),
		(unsigned int)ioread32((void *)(g_dfptopRegBA + i)),
		(unsigned int)ioread32((void *)(g_dfptopRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(g_dfptopRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(g_dfptopRegBA + (i+0xC))));
	}

	pr_info("FE register dump");
	for (i = FE_CTL_OFFSET; i <= FE_CTL_RANGE; i += 0x10) {
		pr_info("%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(DFP_FE_BASE + i),
		(unsigned int)ioread32((void *)(feRegBA + i)),
		(unsigned int)ioread32((void *)(feRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(feRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(feRegBA + (i+0xC))));
	}

	pr_info("DRZH2N register dump");
	for (i = DRZH2N_CTL_OFFSET; i <= DRZH2N_CTL_RANGE; i += 0x10) {
		pr_info("%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(DFP_DRZH2N_BASE + i),
		(unsigned int)ioread32((void *)(g_drzh2nRegBA + i)),
		(unsigned int)ioread32((void *)(g_drzh2nRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(g_drzh2nRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(g_drzh2nRegBA + (i+0xC))));
	}

	pr_info("FM register dump");
	for (i = FM_CTL_OFFSET; i <= FM_CTL_RANGE; i += 0x10) {
		pr_info("%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(DFP_FM_BASE + i),
		(unsigned int)ioread32((void *)(fmRegBA + i)),
		(unsigned int)ioread32((void *)(fmRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(fmRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(fmRegBA + (i+0xC))));
	}

	pr_info("DFP2 TOP register dump");
	for (i = DFP2_TOP_CTL_OFFSET; i <= DFP2_TOP_CTL_RANGE; i += 0x10) {
		pr_info("%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(DFP2_TOP_BASE + i),
		(unsigned int)ioread32((void *)(g_dfp2topRegBA + i)),
		(unsigned int)ioread32((void *)(g_dfp2topRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(g_dfp2topRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(g_dfp2topRegBA + (i+0xC))));
	}

	pr_info("DVGF register dump");
	for (i = DVGF_CTL_OFFSET; i <= DVGF_CTL_RANGE; i += 0x10) {
		pr_info("%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(DFP_DVGF_BASE + i),
		(unsigned int)ioread32((void *)(dvgfRegBA + i)),
		(unsigned int)ioread32((void *)(dvgfRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(dvgfRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(dvgfRegBA + (i+0xC))));
	}
	pr_info("[gals_tx dgb addr] 0x34780048 = 0x%08X",
		(unsigned int)ioread32((void *)(gVcoreRegBA + 0x48)));
	pr_info("[gals_tx dgb addr] 0x3478004C = 0x%08X",
		(unsigned int)ioread32((void *)(gVcoreRegBA + 0x4C)));
	pr_info("[gals_tx dgb addr] 0x34780050 = 0x%08X",
		(unsigned int)ioread32((void *)(gVcoreRegBA + 0x50)));
	pr_info("[gals_tx dgb addr] 0x34780054 = 0x%08X",
		(unsigned int)ioread32((void *)(gVcoreRegBA + 0x54)));
	pr_info("[ACK dgb addr] 0x347800C4 = 0x%08X",
		(unsigned int)ioread32((void *)(gVcoreRegBA + 0xC4)));
	pr_info("[ACK dgb addr] 0x347800C8 = 0x%08X",
		(unsigned int)ioread32((void *)(gVcoreRegBA + 0xC8)));
	pr_info("[ACK dgb addr] 0x347800CC = 0x%08X",
		(unsigned int)ioread32((void *)(gVcoreRegBA + 0xCC)));
}
EXPORT_SYMBOL(imgsys_dfp_debug_dump);

void imgsys_dfp_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *dfptopRegBA = 0L;
	void __iomem *feRegBA = 0L;
	void __iomem *drzh2nRegBA = 0L;
	void __iomem *fmRegBA = 0L;
	void __iomem *dfp2topRegBA = 0L;
	void __iomem *dvgfRegBA = 0L;

	/* iomap registers */
	dfptopRegBA = g_dfptopRegBA;
	if (!dfptopRegBA ) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap DFP TOP registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
		return;
	}
	feRegBA = g_feRegBA;
	if (!feRegBA ) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap FE registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
		return;
	}
	drzh2nRegBA = g_drzh2nRegBA;
	if (!drzh2nRegBA ) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap DRZH2N registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
		return;
	}
	fmRegBA = g_fmRegBA;
	if (!fmRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap FM registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
		return;
	}
	dfp2topRegBA = g_dfp2topRegBA;
	if (!dfp2topRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap DFP2 TOP registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
		return;
	}
	dvgfRegBA = g_dvgfRegBA;
	if (!dvgfRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap DVGF registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	/* FEFM HW mode ddren */
	iowrite32(0x80000000, (void *)(dfptopRegBA + 0x158));

	/* DVGF HW mode ddren */
	iowrite32(0x80000000, (void *)(dfp2topRegBA + 0x158));

}

void imgsys_dfp_cmdq_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev, void *pkt, int hw_idx)
{
	struct cmdq_pkt *package = NULL;

	package = (struct cmdq_pkt *)pkt;

	unsigned int ofset;
	unsigned int i;
	unsigned int dfp_base = DFP_TOP_BASE;
	unsigned int dfp2_base = DFP2_TOP_BASE;

	if (imgsys_dev == NULL || pkt == NULL) {
		dump_stack();
		pr_err("[%s][%d] param fatal error!", __func__, __LINE__);
		return;
	}

	/* init registers */
	for (i = 0; i < ARRAY_SIZE(mtk_imgsys_dfp1_init_ary); i++) {
		ofset = dfp_base + mtk_imgsys_dfp1_init_ary[i].ofset;
		cmdq_pkt_write(package, NULL, ofset /*address*/,
				mtk_imgsys_dfp1_init_ary[i].val, 0xffffffff);
	}

	for (i = 0; i < ARRAY_SIZE(mtk_imgsys_dfp2_init_ary); i++) {
		ofset = dfp2_base + mtk_imgsys_dfp2_init_ary[i].ofset;
		cmdq_pkt_write(package, NULL, ofset /*address*/,
				mtk_imgsys_dfp2_init_ary[i].val, 0xffffffff);
	}
}


bool imgsys_dfp_done_chk(struct mtk_imgsys_dev *imgsys_dev, uint32_t engine)
{
	bool ret = true; //true: done

	return ret;
}
