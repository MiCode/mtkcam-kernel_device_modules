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
#include "mtk_imgsys-me.h"
#include "iommu_debug.h"
#include "mtk_imgsys-v4l2-debug.h"
#include "mtk-hcp.h"

// GCE header
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "../../cmdq/isp8s/mtk_imgsys-cmdq-qof.h"


#define PSEUDO_DESC_TUNING_ME 0xf

struct clk_bulk_data imgsys_isp8s_me_clks[] = {
	{ .id = "ME_CG_IPE" },
	{ .id = "ME_CG_IPE_TOP" },
	{ .id = "ME_CG" },
	{ .id = "ME_CG_LARB12" },
};

struct mtk_imgsys_me_dtable {
	uint32_t cmd1;
	uint32_t addr;
	uint32_t cmd2; // it contains addr_msb at the endianness 4 bit
};

const struct mtk_imgsys_init_array mtk_imgsys_me_init_ary[] = {
	{0x198, 0x00000001}, /* ME_DDREN */
};

const struct mtk_imgsys_init_array mtk_imgsys_mmg_init_ary[] = {
	{0x74, 0x00000001}, /* MMG_DDREN */
};


void imgsys_me_updatecq(struct mtk_imgsys_dev *imgsys_dev,
			struct img_swfrm_info *user_info,
			struct private_data *priv_data,
			int req_fd,
			u64 tuning_iova,
			unsigned int mode)
{
	const struct mtk_hcp_ops *hcp_ops = mtk_hcp_fetch_ops(imgsys_dev->scp_pdev);
	u64 iova_addr = tuning_iova;
	u64 *cq_desc = NULL;
	struct mtk_imgsys_me_dtable *dtable = NULL;
	unsigned int i = 0, tun_ofst = 0;
	struct flush_buf_info me_buf_info;
	void *cq_base = NULL;

	if (hcp_ops && hcp_ops->fetch_me_cq_mb_virt)
		cq_base = hcp_ops->fetch_me_cq_mb_virt(imgsys_dev->scp_pdev, mode);

	/* HWID defined in hw_definition.h */
	if (priv_data->need_update_desc) {
		if (iova_addr) {
			cq_desc = (u64 *)((void *)(cq_base +
				priv_data->desc_offset));
			for (i = 0; i < ME_CQ_DESC_NUM; i++) {
				dtable = (struct mtk_imgsys_me_dtable *)cq_desc + i;
				if (!dtable) {
					pr_info("dtable is NULL!\n");
					return;
				}
				if ((dtable->cmd2 & PSEUDO_DESC_TUNING_ME) == PSEUDO_DESC_TUNING_ME) {
					tun_ofst = dtable->addr;
					dtable->addr = (tun_ofst + iova_addr) & 0xFFFFFFFF;
					dtable->cmd2 = (dtable->cmd2 & 0xFFFFFFF0) |
						(((tun_ofst + iova_addr) >> 32) & 0xF);
					if (imgsys_me_8s_dbg_enable())
						pr_debug("%s: tuning_buf_iova(0x%llx) des_ofst(0x%08x) cq_kva(0x%p) dtable(0x%x/0x%x/0x%x)\n",
							__func__, iova_addr,
							priv_data->desc_offset,
							cq_desc, dtable->cmd1, dtable->addr,
							dtable->cmd2);
				}
			}
		}
		//
		if (hcp_ops && hcp_ops->fetch_me_cq_mb_fd)
			me_buf_info.fd = hcp_ops->fetch_me_cq_mb_fd(imgsys_dev->scp_pdev, mode);
		me_buf_info.offset = priv_data->desc_offset;
		me_buf_info.len =
			(sizeof(struct mtk_imgsys_me_dtable) * ME_CQ_DESC_NUM) + ME_REG_SIZE;
		me_buf_info.mode = mode;
		me_buf_info.is_tuning = false;
		if (imgsys_me_8s_dbg_enable())
			pr_debug("imgsys_fw cq me_buf_info (%d/%d/%d), mode(%d)\n",
				me_buf_info.fd, me_buf_info.len,
				me_buf_info.offset, me_buf_info.mode);
		if (hcp_ops && hcp_ops->flush_mb && hcp_ops->fetch_me_cq_mb_id)
			hcp_ops->flush_mb(
				imgsys_dev->scp_pdev,
				hcp_ops->fetch_me_cq_mb_id(imgsys_dev->scp_pdev, mode),
				me_buf_info.offset,
				me_buf_info.len);
	}
}


//static struct ipesys_me_device *me_dev;
static void __iomem *g_meRegBA;
static void __iomem *g_mmgRegBA;
static void __iomem *gVcoreRegBA;

int imgsys_me_tfault_callback(int port, dma_addr_t mva, void *data)
{

	void __iomem *meRegBA = 0L;
	void __iomem *mmgRegBA = 0L;
	unsigned int i;
	int ret = 0;
	bool is_qof = false;

	ret = smi_isp_wpe2_tnr_get_if_in_use((void *)&is_qof);
	if (ret == -1) {
		pr_info("smi_isp_wpe2_tnr_get_if_in_use = -1.stop dump. return\n");
		return 1;
	}

	/* iomap registers */
	meRegBA = g_meRegBA;
	if (!meRegBA) {
		pr_info("%s Unable to ioremap me registers\n",
		__func__);
		return -1;
	}
	/* iomap registers */
	mmgRegBA = g_mmgRegBA;
	if (!mmgRegBA) {
		pr_info("%s Unable to ioremap mmg registers\n",
		__func__);
	}

	for (i = ME_CTL_OFFSET; i <= ME_CTL_OFFSET + ME_CTL_RANGE_TF; i += 0x10) {
		pr_info("%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x34550000 + i),
		(unsigned int)ioread32((void *)(meRegBA + i)),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0xC))));
	}

	for (i = MMG_CTL_OFFSET; i <= MMG_CTL_OFFSET + MMG_CTL_RANGE_TF; i += 0x10) {
		pr_info("%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x34560000 + i),
		(unsigned int)ioread32((void *)(mmgRegBA + i)),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0xC))));
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


	smi_isp_wpe2_tnr_put((void *)&is_qof);

	return 1;
}

int imgsys_mmg_tfault_callback(int port, dma_addr_t mva, void *data)
{

	void __iomem *mmgRegBA = 0L;
	unsigned int i;
	int ret = 0;
	bool is_qof = false;

	ret = smi_isp_wpe2_tnr_get_if_in_use((void *)&is_qof);
	if (ret == -1) {
		pr_info("smi_isp_wpe2_tnr_get_if_in_use = -1.stop dump. return\n");
		return 1;
	}

	/* iomap registers */
	mmgRegBA = g_mmgRegBA;
	if (!mmgRegBA) {
		pr_info("%s Unable to ioremap mmg registers\n",
		__func__);
		return -1;
	}

	for (i = MMG_CTL_OFFSET; i <= MMG_CTL_OFFSET + MMG_CTL_RANGE_TF; i += 0x10) {
		pr_info("%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x34560000 + i),
		(unsigned int)ioread32((void *)(mmgRegBA + i)),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0xC))));
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

	smi_isp_wpe2_tnr_put((void *)&is_qof);

	return 1;
}

void imgsys_me_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	#ifdef ME_CLK_CTRL
	int ret;
	#endif

	pr_info("%s: +\n", __func__);

	g_meRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ME);
	g_mmgRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ME_MMG);

	gVcoreRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_IMG_VCORE);
	if (!gVcoreRegBA) {
		pr_info("%s:unable to iomap Vcore reg, devnode()\n",
			__func__);
	}

	pr_info("%s: -\n", __func__);
}
//EXPORT_SYMBOL(imgsys_me_set_initial_value);

void imgsys_me_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	if (g_meRegBA) {
		iounmap(g_meRegBA);
		g_meRegBA = 0L;
	}
	if (g_mmgRegBA) {
		iounmap(g_mmgRegBA);
		g_mmgRegBA = 0L;
	}
	if (gVcoreRegBA) {
		iounmap(gVcoreRegBA);
		gVcoreRegBA = 0L;
	}
}
//EXPORT_SYMBOL(ipesys_me_uninit);

void imgsys_me_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
			unsigned int engine)
{
	void __iomem *meRegBA = 0L;
	void __iomem *mmgRegBA = 0L;
	unsigned int i;

	/* iomap registers */
	meRegBA = g_meRegBA;
	if (!meRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap ME registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
		return;
	}
	mmgRegBA = g_mmgRegBA;
	if (!mmgRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap MMG registers\n",
			__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
		return;
	}


	dev_info(imgsys_dev->dev, "%s: dump me regs\n", __func__);
	for (i = ME_CTL_OFFSET; i <= ME_CTL_OFFSET + ME_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x34550000 + i),
		(unsigned int)ioread32((void *)(meRegBA + i)),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0xC))));

		ssize_t written = mtk_img_kernel_write(imgsys_dev->scp_pdev,
		"%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x34070000 + i),
		(unsigned int)ioread32((void *)(meRegBA + i)),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0xC))));

		if(written < 0)
			pr_debug("Failed to write ME register values to AEE buffer\n");
	}
	dev_info(imgsys_dev->dev, "%s: dump mmg regs\n", __func__);
	for (i = MMG_CTL_OFFSET; i <= MMG_CTL_OFFSET + MMG_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x34560000 + i),
		(unsigned int)ioread32((void *)(mmgRegBA + i)),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0xC))));

		ssize_t written = mtk_img_kernel_write(imgsys_dev->scp_pdev,
		"%s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x34070000 + i),
		(unsigned int)ioread32((void *)(meRegBA + i)),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0xC))));

		if(written < 0)
			pr_debug("Failed to write ME register values to AEE buffer\n");
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
//EXPORT_SYMBOL(ipesys_me_debug_dump);

void ipesys_me_debug_dump_local(void)
{
	void __iomem *meRegBA = 0L;
	void __iomem *mmgRegBA = 0L;
	unsigned int i;

	/* iomap registers */
	meRegBA = g_meRegBA;
	if (!meRegBA) {
		pr_info("imgsys %s Unable to ioremap me registers\n",
			__func__);
		return;
	}
	mmgRegBA = g_mmgRegBA;
	if (!mmgRegBA) {
		pr_info("imgsys %s Unable to ioremap mmg registers\n",
			__func__);
		return;
	}
	pr_info("imgsys %s: dump me regs\n", __func__);
	for (i = ME_CTL_OFFSET; i <= ME_CTL_OFFSET + ME_CTL_RANGE; i += 0x10) {
		pr_info("imgsys %s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x34550000 + i),
		(unsigned int)ioread32((void *)(meRegBA + i)),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(meRegBA + (i+0xC))));
	}
	pr_info("imgsys %s: dump mmg regs\n", __func__);
	for (i = MMG_CTL_OFFSET; i <= MMG_CTL_OFFSET + MMG_CTL_RANGE; i += 0x10) {
		pr_info("imgsys %s: 0x%08X %08X, %08X, %08X, %08X", __func__,
		(unsigned int)(0x34560000 + i),
		(unsigned int)ioread32((void *)(mmgRegBA + i)),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0x4))),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0x8))),
		(unsigned int)ioread32((void *)(mmgRegBA + (i+0xC))));
	}
}
//EXPORT_SYMBOL(ipesys_me_debug_dump_local);

void imgsys_me_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{

	void __iomem *meRegBA = 0L;
	void __iomem *mmgRegBA = 0L;

	/* iomap registers */
	meRegBA = g_meRegBA;
	if (!meRegBA) {
		pr_info("imgsys %s Unable to ioremap me registers\n",
			__func__);
		return;
	}
	mmgRegBA = g_mmgRegBA;
	if (!mmgRegBA) {
		pr_info("imgsys %s Unable to ioremap mmg registers\n",
			__func__);
		return;
	}
		// /* ME HW mode ddren */
		iowrite32(0x00000001, (void *)(meRegBA + 0x00000198));

		// /* MMG HW mode ddren */
		iowrite32(0x00000001, (void *)(mmgRegBA + 0x00000074));

}

void imgsys_me_cmdq_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev, void *pkt, int hw_idx)
{
	struct cmdq_pkt *package = NULL;

	package = (struct cmdq_pkt *)pkt;

	unsigned int ofset;
	unsigned int i;
	unsigned int me_base = ME_BASE;
	unsigned int mmg_base = MMG_BASE;

	if (imgsys_dev == NULL || pkt == NULL) {
		dump_stack();
		pr_err("[%s][%d] param fatal error!", __func__, __LINE__);
		return;
	}

	/* init registers */
	for (i = 0; i < ARRAY_SIZE(mtk_imgsys_me_init_ary); i++) {
		ofset = me_base + mtk_imgsys_me_init_ary[i].ofset;
		cmdq_pkt_write(package, NULL, ofset /*address*/,
				mtk_imgsys_me_init_ary[i].val, 0xffffffff);
	}

	/* init registers */
	for (i = 0; i < ARRAY_SIZE(mtk_imgsys_mmg_init_ary); i++) {
		ofset = mmg_base + mtk_imgsys_mmg_init_ary[i].ofset;
		cmdq_pkt_write(package, NULL, ofset /*address*/,
				mtk_imgsys_mmg_init_ary[i].val, 0xffffffff);
	}
}


bool imgsys_me_done_chk(struct mtk_imgsys_dev *imgsys_dev, uint32_t engine)
{
	bool ret = true; //true: done

	uint32_t value = 0;

	if (engine & IMGSYS_HW_FLAG_ME) {
		value = (uint32_t)ioread32((void *)(g_meRegBA + 0x17C));
		if (!(value & 0x1)) {
			ret = false;
			pr_info(
			"%s: hw_comb:0x%x, polling ME done fail!!! [0x%08x] 0x%x",
			__func__, engine,
			(uint32_t)(ME_BASE + 0xec), value);
		}
	}

	return ret;
}
