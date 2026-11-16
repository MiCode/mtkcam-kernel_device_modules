// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/vmalloc.h>

#include <soc/mediatek/smi.h>

#include "mtk_cam.h"
#include "mtk_cam-pda.h"
#include "mtk_cam-trace.h"
#include "mtk_cam-plat.h"
#include "mtk_cam-pda_regs.h"

#include "iommu_debug.h"

#include "mtk-vmm-notifier.h"

#define OUT_BYTE_PER_ROI 1200

static const struct of_device_id mtk_pda_of_ids[] = {
	{.compatible = "mediatek,pda",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_pda_of_ids);

int mtk_pda_translation_fault_callback(int port, dma_addr_t mva, void *data)
{
	struct mtk_pda_device *pda_dev = (struct mtk_pda_device *)data;
	unsigned int sel_index = 0;
	unsigned int Debug_Sel[] = {0x00008120, 0x0000400e, 0x0000c000};
	unsigned int Length_Arr = sizeof(Debug_Sel)/sizeof(*Debug_Sel);

	dev_info(pda_dev->dev, "%s:check buffer information\n", __func__);

	dev_info(pda_dev->dev, "CFG_0/1/2/3/4/5/6: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_0),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_1),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_2),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_3),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_4),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_5),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_6));
	dev_info(pda_dev->dev, "CFG_14~18: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_14),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_15),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_16),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_17),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_18));

	dev_info(pda_dev->dev, "ROI 0,1, CFG_19~26: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_19),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_20),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_21),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_22),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_23),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_24),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_25),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_26));
	dev_info(pda_dev->dev, "ROI 2,3, CFG_27~34: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_27),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_28),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_29),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_30),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_31),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_32),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_33),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_34));
	dev_info(pda_dev->dev, "ROI 4,5, CFG_35~42: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_35),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_36),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_37),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_38),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_39),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_40),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_41),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_42));
	dev_info(pda_dev->dev, "ROI 6,7, CFG_43~50: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_43),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_44),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_45),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_46),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_47),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_48),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_49),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_50));
	dev_info(pda_dev->dev, "ROI 8,9, CFG_51~58: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_51),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_52),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_53),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_54),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_55),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_56),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_57),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_58));
	dev_info(pda_dev->dev, "ROI 10,11, CFG_59~66: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_59),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_60),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_61),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_62),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_63),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_64),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_65),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_66));
	dev_info(pda_dev->dev, "ROI 12,13, CFG_67~74: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_67),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_68),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_69),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_70),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_71),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_72),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_73),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_74));
	dev_info(pda_dev->dev, "ROI 14,15, CFG_75~82: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_75),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_76),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_77),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_78),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_79),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_80),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_81),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_82));
	dev_info(pda_dev->dev, "ROI 16,17, CFG_83~90: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_83),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_84),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_85),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_86),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_87),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_88),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_89),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_90));
	dev_info(pda_dev->dev, "ROI 18,19, CFG_91~98: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_91),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_92),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_93),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_94),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_95),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_96),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_97),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_98));
	dev_info(pda_dev->dev, "ROI 20,21, CFG_99~106: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_99),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_100),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_101),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_102),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_103),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_104),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_105),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_106));
	dev_info(pda_dev->dev, "ROI 22,23, CFG_107~114: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_107),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_108),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_109),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_110),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_111),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_112),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_113),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_CFG_114));

	dev_info(pda_dev->dev, "DCIF DEBUG0~7: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA0),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA1),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA2),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA3),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA4),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA5),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA6),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA7));
	dev_info(pda_dev->dev, "I_P1/TI_P1/I_P2/TI_P2/Out: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDAI_P1_BASE_ADDR),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDATI_P1_BASE_ADDR),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDAI_P2_BASE_ADDR),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDATI_P2_BASE_ADDR),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDAO_P1_BASE_ADDR));
	dev_info(pda_dev->dev, "[MSB]I_P1/TI_P1/I_P2/TI_P2/Out: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDAI_P1_BASE_ADDR_MSB),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDATI_P1_BASE_ADDR_MSB),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDAI_P2_BASE_ADDR_MSB),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDATI_P2_BASE_ADDR_MSB),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDAO_P1_BASE_ADDR_MSB));
	dev_info(pda_dev->dev, "ERR_STAT_EN/ERR_STAT/TOP_CTL/DCIF_CTL: 0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDA_ERR_STAT_EN),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDA_ERR_STAT),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDA_TOP_CTL),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_DCIF_CTL));
	dev_info(pda_dev->dev, "[ERR_STAT]I_P1/TI_P1/I_P2/TI_P2/Out: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDAI_P1_ERR_STAT),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDATI_P1_ERR_STAT),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDAI_P2_ERR_STAT),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDATI_P2_ERR_STAT),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDAO_P1_ERR_STAT));
	dev_info(pda_dev->dev, "pack_mode/dilation: 0x%x/0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PACK_MODE),
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_DILATION_CFG));
	dev_info(pda_dev->dev, "DMA_EN(0x1f): 0x%x\n",
		readl_relaxed(pda_dev->base_inner + REG_E_PDA_OTF_PDA_DMA_EN));

	// check debug data
	for (sel_index = 0; sel_index < Length_Arr; ++sel_index) {
		writel_relaxed(Debug_Sel[sel_index], pda_dev->base_inner + REG_PDA_PDA_DEBUG_SEL);
		dev_info(pda_dev->dev, "DEBUG_SEL/DEBUG_DATA: 0x%x/0x%x\n",
			readl_relaxed(pda_dev->base_inner + REG_PDA_PDA_DEBUG_SEL),
			readl_relaxed(pda_dev->base_inner + REG_PDA_PDA_DEBUG_DATA));
	}

	return 0;
}
static int reset_msgfifo(struct mtk_pda_device *pda_dev)
{
	atomic_set(&pda_dev->is_fifo_overflow, 0);
	return kfifo_init(&pda_dev->msg_fifo, pda_dev->msg_buffer, pda_dev->fifo_size);
}

void pda_reset(struct mtk_pda_device *pda_dev)
{
	unsigned long end = 0;

	end = jiffies + msecs_to_jiffies(100);

	// reset status
	pda_dev->ts_ns = 0;
	pda_dev->err2_count = 0;

	// clear dma_soft_rst_stat
	writel_relaxed(0x0, pda_dev->base + REG_E_PDA_OTF_PDA_DMA_RST);

	// make reset
	writel_relaxed(0x2, pda_dev->base + REG_E_PDA_OTF_PDA_DMA_RST);

	wmb(); /* TBC */

	while (time_before(jiffies, end)) {
		if ((readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_DMA_RST) & 0x1)) {
			// equivalent to hardware reset
			writel_relaxed(0x2, pda_dev->base + REG_E_PDA_OTF_PDA_TOP_CTL);

			// clear reset signal
			writel_relaxed(0x0, pda_dev->base + REG_E_PDA_OTF_PDA_DMA_RST);

			wmb(); /* TBC */

			// clear hardware reset signal
			writel_relaxed(0x0, pda_dev->base + REG_E_PDA_OTF_PDA_TOP_CTL);
			dev_info(pda_dev->dev, "reset pda success\n");
			return;
		}

		//dev_info(pda_dev->dev, "Wait EMI request, DMA_RST:0x%x\n",
		//	readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_DMA_RST));

		usleep_range(10, 20);
	}

	dev_info(pda_dev->dev, "reset pda timeout, DMA_RST:0x%x\n",
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_DMA_RST));
}


static int check_design_limitation(struct mtk_pda_device *pda_dev)
{
	union _pda_otf_cfg_0_ pda_cfg_0;
	union _pda_otf_cfg_1_ pda_cfg_1;
	union _pda_otf_cfg_2_ pda_cfg_2;
	union _pda_otf_cfg_18_ pda_cfg_18;
	// int i = 0;
	// unsigned int ROInum = 0;

	dev_info(pda_dev->dev, "%s:start to check config data\n", __func__);

	pda_cfg_0.Raw = readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_0);

	// frame constraint
	if (pda_cfg_0.Bits.PDA_WIDTH % 4 != 0) {
		dev_info(pda_dev->dev, "Frame width must be multiple of 4\n");
		return -4;
	}

	if (pda_cfg_0.Bits.PDA_WIDTH < 20 || pda_cfg_0.Bits.PDA_WIDTH > 8191) {
		dev_info(pda_dev->dev, "Frame width (%d) out of range\n",
			pda_cfg_0.Bits.PDA_WIDTH);
		return -12;
	}

	if (pda_cfg_0.Bits.PDA_HEIGHT < 4 || pda_cfg_0.Bits.PDA_HEIGHT > 8191) {
		dev_info(pda_dev->dev, "Frame height (%d) out of range\n",
			pda_cfg_0.Bits.PDA_HEIGHT);
		return -13;
	}

	pda_cfg_1.Raw = readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_1);
	if (pda_cfg_1.Bits.PDA_PR_XNUM < 1 || pda_cfg_1.Bits.PDA_PR_XNUM > 16) {
		dev_info(pda_dev->dev, "PDA_PR_XNUM (%d) out of range, valid:1~16\n",
			pda_cfg_1.Bits.PDA_PR_XNUM);
		return -14;
	}

	if (pda_cfg_1.Bits.PDA_PR_YNUM < 1 || pda_cfg_1.Bits.PDA_PR_YNUM > 16) {
		dev_info(pda_dev->dev, "PDA_PR_YNUM (%d) out of range, valid:1~16\n",
			pda_cfg_1.Bits.PDA_PR_YNUM);
		return -15;
	}

	if (pda_cfg_1.Bits.PDA_PAT_WIDTH < 1 || pda_cfg_1.Bits.PDA_PAT_WIDTH > 512) {
		dev_info(pda_dev->dev, "PDA_PAT_WIDTH (%d) out of range, valid:1~512\n",
			pda_cfg_1.Bits.PDA_PAT_WIDTH);
		return -16;
	}

	if (pda_cfg_1.Bits.PDA_RNG_ST > 40) {
		dev_info(pda_dev->dev, "PDA_RNG_ST (%d) out of range, valid:<40\n",
			pda_cfg_1.Bits.PDA_RNG_ST);
		return -17;
	}

	pda_cfg_2.Raw = readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_2);
	if (pda_cfg_2.Bits.PDA_TBL_STRIDE < 5 || pda_cfg_2.Bits.PDA_TBL_STRIDE > 2048) {
		dev_info(pda_dev->dev, "PDA_TBL_STRIDE (%d) out of range, valid:5~2048\n",
			pda_cfg_2.Bits.PDA_TBL_STRIDE);
		return -18;
	}

	pda_cfg_18.Raw = readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_18);
	if (pda_cfg_18.Bits.PDA_GRAD_DST < 1 || pda_cfg_18.Bits.PDA_GRAD_DST > 5) {
		dev_info(pda_dev->dev, "PDA_GRAD_DST (%d) out of range, valid:1~5\n",
			pda_cfg_18.Bits.PDA_GRAD_DST);
		return -32;
	}

	// TODO: check ROI parameters
	// ROInum = pda_cfg_2.Bits.PDA_RGN_NUM;
	// for (i = 0; i < ROInum; i++) {
		//ROI 0, cfg19~22
		//ROI 1, cfg23~26
		//ROI 2, cfg27~30
	// }

	return 0;
}

static void debug_sel_print(struct mtk_pda_device *pda_dev)
{
	unsigned int sel_index = 0;
	unsigned int Debug_Sel[] = {0x00008120, 0x0000400e, 0x0000c000,
		0x11000000, 0x12000000, 0x13000000, 0x14000000, 0x15000000, 0x16000000,
		0x14000000, 0x14100000, 0x14200000, 0x14300000, 0x14400000, 0x14500000,
		0x21000000, 0x22000000, 0x23000000, 0x24000000, 0x25000000,
		0x24000000, 0x24100000, 0x24200000, 0x24300000, 0x24400000, 0x24500000,
		0x31000000, 0x32000000, 0x33000000, 0x34000000, 0x35000000,
		0x34000000, 0x34100000, 0x34200000, 0x34300000, 0x34400000, 0x34500000,
		0x41000000, 0x42000000, 0x43000000, 0x44000000, 0x45000000,
		0x44000000, 0x44100000, 0x44200000, 0x44300000, 0x44400000, 0x44500000,
		0x51000000, 0x52000000, 0x53000000, 0x54000000, 0x55000000,
		0x54000000, 0x54100000, 0x54200000, 0x54300000, 0x54400000, 0x54500000,
		0xb4480000, 0xb4400000, 0xb4410000};
	unsigned int Length_Arr = sizeof(Debug_Sel)/sizeof(*Debug_Sel);

	// check debug data
	for (sel_index = 0; sel_index < Length_Arr; ++sel_index) {
		writel_relaxed(Debug_Sel[sel_index], pda_dev->base + REG_PDA_PDA_DEBUG_SEL);
		dev_info(pda_dev->dev, "DEBUG_SEL/DEBUG_DATA: 0x%x/0x%x\n",
			readl_relaxed(pda_dev->base + REG_PDA_PDA_DEBUG_SEL),
			readl_relaxed(pda_dev->base + REG_PDA_PDA_DEBUG_DATA));
	}
}

static void debug_csr_print(struct mtk_pda_device *pda_dev)
{
	dev_info(pda_dev->dev, "CFG_0/1/2/3/4/5/6: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_0),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_1),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_2),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_3),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_4),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_5),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_6));
	dev_info(pda_dev->dev, "CFG_14~18: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_14),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_15),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_16),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_17),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_18));
	dev_info(pda_dev->dev, "CFG_19~22: 0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_19),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_20),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_21),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_CFG_22));
	dev_info(pda_dev->dev, "DCIF DEBUG0~7: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA0),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA1),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA2),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA3),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA4),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA5),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA6),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA7));
	dev_info(pda_dev->dev, "I_P1/TI_P1/I_P2/TI_P2/Out: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDAI_P1_BASE_ADDR),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDATI_P1_BASE_ADDR),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDAI_P2_BASE_ADDR),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDATI_P2_BASE_ADDR),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDAO_P1_BASE_ADDR));
	dev_info(pda_dev->dev, "[MSB]I_P1/TI_P1/I_P2/TI_P2/Out: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDAI_P1_BASE_ADDR_MSB),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDATI_P1_BASE_ADDR_MSB),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDAI_P2_BASE_ADDR_MSB),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDATI_P2_BASE_ADDR_MSB),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDAO_P1_BASE_ADDR_MSB));
	dev_info(pda_dev->dev, "ERR_STAT_EN/ERR_STAT/TOP_CTL/DCIF_CTL: 0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_ERR_STAT_EN),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_ERR_STAT),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_TOP_CTL),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_DCIF_CTL));
	dev_info(pda_dev->dev, "[ERR_STAT]I_P1/TI_P1/I_P2/TI_P2/Out: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDAI_P1_ERR_STAT),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDATI_P1_ERR_STAT),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDAI_P2_ERR_STAT),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDATI_P2_ERR_STAT),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDAO_P1_ERR_STAT));
	dev_info(pda_dev->dev, "pack_mode/dilation: 0x%x/0x%x\n",
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PACK_MODE),
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_DILATION_CFG));
	dev_info(pda_dev->dev, "DMA_EN(0x1f): 0x%x\n",
		readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_DMA_EN));

	vmm_cvfs_dump();
}

int mtk_cam_pda_dev_config(struct mtk_pda_device *pda_dev)
{
	// --------- DMA Secure part -------------
	writel_relaxed(0x00000000, pda_dev->base + REG_E_PDA_OTF_PDA_SECURE);
	writel_relaxed(0x00000000, pda_dev->base + REG_E_PDA_OTF_PDA_SECURE_1);

	// --------- config setting hard code part --------------
	// Left image
	writel_relaxed(0x100000cc, pda_dev->base + REG_E_PDA_OTF_PDAI_P1_CON0);
	writel_relaxed(0x10330033, pda_dev->base + REG_E_PDA_OTF_PDAI_P1_CON1);
	writel_relaxed(0x00660066, pda_dev->base + REG_E_PDA_OTF_PDAI_P1_CON2);
	writel_relaxed(0x00880088, pda_dev->base + REG_E_PDA_OTF_PDAI_P1_CON3);
	writel_relaxed(0x00660066, pda_dev->base + REG_E_PDA_OTF_PDAI_P1_CON4);

	// Left table
	writel_relaxed(0x10000033, pda_dev->base + REG_E_PDA_OTF_PDATI_P1_CON0);
	writel_relaxed(0x000c000c, pda_dev->base + REG_E_PDA_OTF_PDATI_P1_CON1);
	writel_relaxed(0x00190019, pda_dev->base + REG_E_PDA_OTF_PDATI_P1_CON2);
	writel_relaxed(0x00220022, pda_dev->base + REG_E_PDA_OTF_PDATI_P1_CON3);
	writel_relaxed(0x00190019, pda_dev->base + REG_E_PDA_OTF_PDATI_P1_CON4);

	// Right image
	writel_relaxed(0x100000cc, pda_dev->base + REG_E_PDA_OTF_PDAI_P2_CON0);
	writel_relaxed(0x10330033, pda_dev->base + REG_E_PDA_OTF_PDAI_P2_CON1);
	writel_relaxed(0x00660066, pda_dev->base + REG_E_PDA_OTF_PDAI_P2_CON2);
	writel_relaxed(0x00880088, pda_dev->base + REG_E_PDA_OTF_PDAI_P2_CON3);
	writel_relaxed(0x00660066, pda_dev->base + REG_E_PDA_OTF_PDAI_P2_CON4);

	// Right table
	writel_relaxed(0x10000033, pda_dev->base + REG_E_PDA_OTF_PDATI_P2_CON0);
	writel_relaxed(0x000c000c, pda_dev->base + REG_E_PDA_OTF_PDATI_P2_CON1);
	writel_relaxed(0x00190019, pda_dev->base + REG_E_PDA_OTF_PDATI_P2_CON2);
	writel_relaxed(0x00220022, pda_dev->base + REG_E_PDA_OTF_PDATI_P2_CON3);
	writel_relaxed(0x00190019, pda_dev->base + REG_E_PDA_OTF_PDATI_P2_CON4);

	// Output
	writel_relaxed(0x10000060, pda_dev->base + REG_E_PDA_OTF_PDAO_P1_CON0);
	writel_relaxed(0x00100010, pda_dev->base + REG_E_PDA_OTF_PDAO_P1_CON1);
	writel_relaxed(0x00200020, pda_dev->base + REG_E_PDA_OTF_PDAO_P1_CON2);
	writel_relaxed(0x00300030, pda_dev->base + REG_E_PDA_OTF_PDAO_P1_CON3);
	writel_relaxed(0x00200020, pda_dev->base + REG_E_PDA_OTF_PDAO_P1_CON4);

	writel_relaxed(OUT_BYTE_PER_ROI, pda_dev->base + REG_E_PDA_OTF_PDAI_STRIDE);

	writel_relaxed((OUT_BYTE_PER_ROI-1), pda_dev->base + REG_E_PDA_OTF_PDAO_P1_XSIZE);

	writel_relaxed(0x0000001f, pda_dev->base + REG_E_PDA_OTF_PDA_DMA_EN);
	writel_relaxed(0x00000802, pda_dev->base + REG_E_PDA_OTF_PDA_DMA_TOP);

	// DCM all off: 0x0000007F
	// DCM all on:  0x00000000
	writel_relaxed(0x0000007F, pda_dev->base + REG_E_PDA_OTF_PDA_DCM_DIS);

	//disable dma error irq: 0x00000000
	//enable dma error irq: 0xffff0000
	writel_relaxed(0xffff0000, pda_dev->base + REG_E_PDA_OTF_PDAI_P1_ERR_STAT);
	writel_relaxed(0xffff0000, pda_dev->base + REG_E_PDA_OTF_PDATI_P1_ERR_STAT);
	writel_relaxed(0xffff0000, pda_dev->base + REG_E_PDA_OTF_PDAI_P2_ERR_STAT);
	writel_relaxed(0xffff0000, pda_dev->base + REG_E_PDA_OTF_PDATI_P2_ERR_STAT);
	writel_relaxed(0xffff0000, pda_dev->base + REG_E_PDA_OTF_PDAO_P1_ERR_STAT);

	//delete
	//writel_relaxed(0x00000000, pda_dev->base + REG_E_PDA_OTF_PDA_TOP_CTL);

	writel_relaxed(0x00000000, pda_dev->base + REG_E_PDA_OTF_PDA_IRQ_TRIG);

	// clear AXSLC
	writel_relaxed(0x00000000, pda_dev->base + REG_E_PDA_OTF_PDA_P1_AXSLC);
	writel_relaxed(0x00000000, pda_dev->base + REG_E_PDA_OTF_PDA_P2_AXSLC);
	writel_relaxed(0x00000000, pda_dev->base + REG_E_PDA_OTF_PDAO_P1_AXSLC);

	// setting read clear
	writel_relaxed(0x0000000F, pda_dev->base + REG_E_PDA_OTF_PDA_ERR_STAT_EN);

	// read ERR_STAT, avoid the impact of previous data
	dev_info(pda_dev->dev, "%s REG_E_PDA_OTF_PDA_ERR_STAT(0x678) = 0x%x\n",
		__func__, readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_ERR_STAT));

	// read clear dma status
	readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDAI_P1_ERR_STAT);
	readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDATI_P1_ERR_STAT);
	readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDAI_P2_ERR_STAT);
	readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDATI_P2_ERR_STAT);
	readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDAO_P1_ERR_STAT);

	//clear pda status
	pda_dev->ts_ns = 0;
	pda_dev->irq_type = 0;
	pda_dev->err2_count = 0;

	//-------- setting otf trigger ---------
	writel_relaxed(0x5, pda_dev->base + REG_E_PDA_OTF_PDA_TOP_CTL);

	wmb(); /* TBC */

	writel_relaxed(0x0, pda_dev->base + REG_E_PDA_OTF_PDA_TOP_CTL);

	wmb(); /* TBC */

	// 0xe:
	// PDA_OTF_otf_db_load_combine = 1
	// PDA_OTF_dcif_en = 1
	// PDA_OTF_otf_mode_trig = 1
	// PDA_OTF_ofl_mode_trig = 0
	writel_relaxed(0xE, pda_dev->base + REG_E_PDA_OTF_DCIF_CTL);
	//--------------------------------------

	if (readl_relaxed(pda_dev->base + REG_E_PDA_OTF_DCIF_CTL) != 0xc)
		dev_info(pda_dev->dev, "REG_E_PDA_OTF_DCIF_CTL(0x6b8) = 0x%x (expected 0xc)\n",
			readl_relaxed(pda_dev->base + REG_E_PDA_OTF_DCIF_CTL));

	return 0;
}

int mtk_cam_pda_debug_dump(struct mtk_pda_device *pda_dev, unsigned int dump_tags)
{
	int need_smi_dump = false;

	// for debug
	dev_info(pda_dev->dev, "%s +\n", __func__);

	// Check for PDA_ERR_2 error
	if (dump_tags & (1 << PDA_ERR_2)) {
		pda_dev->err2_count++;
		check_design_limitation(pda_dev);
		debug_sel_print(pda_dev);

		// Trigger kernel API dump if err2 count exceeds 5
		if (pda_dev->err2_count > 5) {
			dev_info(pda_dev->dev, "Error 2 occurred more than 5 times\n");
			debug_csr_print(pda_dev);
			// WRAP_AEE_EXCEPTION(MSG_STREAM_ON_ERROR,
			//	"PDA wasn't completed five times in a row");
		}
	}

	// TODO: Implement handling for PDA_ERR_3 error
	if (dump_tags & (1 << PDA_ERR_3)) {
		//ofl trig
		writel_relaxed(0x9, pda_dev->base + REG_E_PDA_OTF_DCIF_CTL);

		dev_info(pda_dev->dev, "REG_E_PDA_OTF_DCIF_CTL(0x6b8) = 0x%x  (expected 0x8)\n",
			readl_relaxed(pda_dev->base + REG_E_PDA_OTF_DCIF_CTL));

		wmb(); /* TBC */

		//otg trig
		writel_relaxed(0xE, pda_dev->base + REG_E_PDA_OTF_DCIF_CTL);
		dev_info(pda_dev->dev, "REG_E_PDA_OTF_DCIF_CTL(0x6b8) = 0x%x (expected 0xc)\n",
			readl_relaxed(pda_dev->base + REG_E_PDA_OTF_DCIF_CTL));

		// Call kernel API dump and identify required OTF/OFL CSRs to dump
		// WRAP_AEE_EXCEPTION(MSG_STREAM_ON_ERROR,
		//	"PDA didn't change to OFL mode when camsv change mux");
	}

	if (dump_tags & (1 << PDA_ERR_HANG)) {
		debug_csr_print(pda_dev);
		debug_sel_print(pda_dev);
	}

	// for debug
	dev_info(pda_dev->dev, "%s -\n", __func__);

	return need_smi_dump;
}

static irqreturn_t mtk_irq_pda(int irq, void *data)
{
	struct mtk_pda_device *pda_dev = (struct mtk_pda_device *)data;
	//struct device *dev = pda_dev->dev;
	bool wake_thread = 0;

	unsigned int pda_status = 0;
	unsigned int dma_i_p1 = 0, dma_ti_p1 = 0, dma_i_p2 = 0, dma_ti_p2 = 0, dma_out = 0;

	pda_dev->ts_ns = 0;
	pda_dev->irq_type = 0;
	pda_dev->err_tags = 0;

	pda_status = readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDA_ERR_STAT);

	// Check if it's an error IRQ
	if (pda_status & PDA_OTF_PDA_DONE_ST) {
		pda_dev->ts_ns = ktime_get_boottime_ns();
		pda_dev->irq_type |= (1 << PDA_IRQ_PDA_DONE);

		dev_info(pda_dev->dev, "pda-%d: pda_status:0x%x, ts=%llu",
			pda_dev->id, pda_status, pda_dev->ts_ns / 1000);

		pda_dev->err2_count > 0 ? (pda_dev->err2_count--) : 0;
	} else if (pda_status) {
		pda_dev->irq_type |= (1 << PDA_IRQ_ERROR);

		dev_info(pda_dev->dev, "pda-%d: error status:0x%x",
			pda_dev->id, pda_status);

		if (pda_status & PDA_OTF_PDA_HANG_ST) {
			pda_dev->err_tags |= (1 << PDA_ERR_HANG);
			dev_info(pda_dev->dev, "PDA hang");
		}
		if (pda_status & PDA_OTF_PDA_ERR_STAT_RESERVED2) {
			pda_dev->err_tags |= (1 << PDA_ERR_2);
			dev_info(pda_dev->dev, "PDA imcomplete when next vsync coming");
		}
		if (pda_status & PDA_OTF_PDA_ERR_STAT_RESERVED3) {
			pda_dev->err_tags |= (1 << PDA_ERR_3);
			dev_info(pda_dev->dev, "PDA didn't change to OFL mode when camsv change mux");
		}
	} else if (pda_status == 0) {
		dma_i_p1 = readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDAI_P1_ERR_STAT);
		dma_ti_p1 = readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDATI_P1_ERR_STAT);
		dma_i_p2 = readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDAI_P2_ERR_STAT);
		dma_ti_p2 = readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDATI_P2_ERR_STAT);
		dma_out = readl_relaxed(pda_dev->base + REG_E_PDA_OTF_PDAO_P1_ERR_STAT);

		if ((dma_i_p1 & 0xFFFF) || (dma_ti_p1 & 0xFFFF) || (dma_i_p2 & 0xFFFF) ||
				(dma_ti_p2 & 0xFFFF) || (dma_out & 0xFFFF)) {
			// read clear dma status
			dev_info(pda_dev->dev,
				"%s pda-%d: [ERR_STAT]I_P1/TI_P1/I_P2/TI_P2/Out: 0x%x/0x%x/0x%x/0x%x/0x%x\n",
				__func__, pda_dev->id, dma_i_p1, dma_ti_p1, dma_i_p2, dma_ti_p2, dma_out);
			pda_dev->irq_type |= (1 << PDA_IRQ_ERROR);
			pda_dev->err_tags |= (1 << PDA_ERR_HANG);
			dev_info(pda_dev->dev, "DMA error detected\n");
		} else {
			dev_info(pda_dev->dev, "pda-%d: pda_status:0x%x, Not OTF PDA irq",
				pda_dev->id, pda_status);
			wake_thread = false;
		}
	}

	if (pda_dev->irq_type)
		wake_thread = true;

	return wake_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}

static irqreturn_t mtk_thread_irq_pda(int irq, void *data)
{
	struct mtk_pda_device *pda_dev = (struct mtk_pda_device *)data;

	/* error case */
	if (unlikely(pda_dev->irq_type == (1 << PDA_IRQ_ERROR))) {
		/* dump pda debug data */
		mtk_cam_pda_debug_dump(pda_dev, pda_dev->err_tags);
	}

	return IRQ_HANDLED;
}

static int mtk_pda_pm_suspend(struct device *dev)
{
	//struct mtk_pda_device *pda_dev = dev_get_drvdata(dev);
	int ret = 0;

	dev_info_ratelimited(dev, "- %s\n", __func__);
	dev_info(dev, "%s +\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Force ISP HW to idle */
	ret = pm_runtime_force_suspend(dev);

	dev_info(dev, "%s -\n", __func__);
	return ret;
}

static int mtk_pda_pm_resume(struct device *dev)
{
	//struct mtk_pda_device *pda_dev = dev_get_drvdata(dev);
	int ret = 0;

	dev_info_ratelimited(dev, "- %s\n", __func__);
	dev_info(dev, "%s +\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Force ISP HW to resume */
	ret = pm_runtime_force_resume(dev);
	if (ret)
		return ret;

	dev_info(dev, "%s -\n", __func__);
	return 0;
}

static int mtk_pda_suspend_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct mtk_pda_device *pda_dev =
		container_of(notifier, struct mtk_pda_device, notifier_blk);
	struct device *dev = pda_dev->dev;

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE: /* before enter suspend */
		mtk_pda_pm_suspend(dev);
		return NOTIFY_DONE;
	case PM_POST_SUSPEND: /* after resume */
		mtk_pda_pm_resume(dev);
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}
static int mtk_pda_of_probe(struct platform_device *pdev,
			    struct mtk_pda_device *pda_dev)
{
	struct device *dev = &pdev->dev;
	struct platform_device *larb_pdev;
	struct device_node *larb_node;
	struct of_phandle_args args;
	struct device_link *link;
	struct resource *res;
	unsigned int i;
	int ret, num_clks, num_ports, smmus;

	ret = of_property_read_u32(dev->of_node, "mediatek,pda-id",
						       &pda_dev->id);
	if (ret) {
		dev_dbg(dev, "missing pda id property\n");
		return ret;
	}

	/* base outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	pda_dev->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(pda_dev->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(pda_dev->base);
	}
	dev_dbg(dev, "pda, map_addr=0x%pK\n", pda_dev->base);

	/* base inner register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inner_base");
	if (!res) {
		dev_dbg(dev, "failed to get inner_base\n");
		return -ENODEV;
	}

	pda_dev->base_inner = devm_ioremap_resource(dev, res);
	if (IS_ERR(pda_dev->base_inner)) {
		dev_dbg(dev, "failed to map register inner base\n");
		return PTR_ERR(pda_dev->base_inner);
	}

	dev_dbg(dev, "pda, map_addr=0x%pK\n", pda_dev->base_inner);

	for (i = 0; i < PDA_IRQ_NUM; i++) {
		pda_dev->irq[i] = platform_get_irq(pdev, i);
		if (!pda_dev->irq[i]) {
			dev_dbg(dev, "failed to get irq\n");
			return -ENODEV;
		}
	}

	for (i = 0; i < PDA_IRQ_NUM; i++) {
		ret = devm_request_threaded_irq(dev, pda_dev->irq[i],
					mtk_irq_pda,
					mtk_thread_irq_pda,
					IRQF_TRIGGER_HIGH | IRQF_SHARED, dev_name(dev), pda_dev);

		if (ret) {
			dev_dbg(dev, "failed to request irq=%d, ret = %d\n", pda_dev->irq[i], ret);
			return ret;
		}

		dev_info(dev, "registered irq=%d, ret = %d\n", pda_dev->irq[i], ret);

		disable_irq(pda_dev->irq[i]);
		dev_info(dev, "%s:disable irq %d\n", __func__, pda_dev->irq[i]);
	}


	num_clks = of_count_phandle_with_args(pdev->dev.of_node, "clocks",
			"#clock-cells");

	pda_dev->num_clks = (num_clks < 0) ? 0 : num_clks;
	dev_info(dev, "clk_num:%d\n", pda_dev->num_clks);

	if (pda_dev->num_clks) {
		pda_dev->clks = devm_kcalloc(dev, pda_dev->num_clks, sizeof(*pda_dev->clks),
					 GFP_KERNEL);
		if (!pda_dev->clks)
			return -ENOMEM;
	}

	for (i = 0; i < pda_dev->num_clks; i++) {
		pda_dev->clks[i] = of_clk_get(pdev->dev.of_node, i);
		if (IS_ERR(pda_dev->clks[i])) {
			dev_info(dev, "failed to get clk %d\n", i);
			return -ENODEV;
		}
	}

	pda_dev->num_larbs = of_count_phandle_with_args(
					pdev->dev.of_node, "mediatek,larbs", NULL);
	pda_dev->num_larbs = (pda_dev->num_larbs < 0) ? 0 : pda_dev->num_larbs;
	dev_info(dev, "larb_num:%d\n", pda_dev->num_larbs);

	if (pda_dev->num_larbs) {
		pda_dev->larb_pdev = devm_kcalloc(dev,
					     pda_dev->num_larbs, sizeof(*pda_dev->larb_pdev),
					     GFP_KERNEL);
		if (!pda_dev->larb_pdev)
			return -ENOMEM;
	}

	for (i = 0; i < pda_dev->num_larbs; i++) {
		larb_node = of_parse_phandle(
					pdev->dev.of_node, "mediatek,larbs", i);
		if (!larb_node) {
			dev_info(dev, "failed to get larb node\n");
			continue;
		}

		larb_pdev = of_find_device_by_node(larb_node);
		if (WARN_ON(!larb_pdev)) {
			of_node_put(larb_node);
			dev_info(dev, "failed to get larb pdev\n");
			continue;
		}
		of_node_put(larb_node);

		link = device_link_add(&pdev->dev, &larb_pdev->dev,
						DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
		if (!link)
			dev_info(dev, "unable to link smi larb%d\n", i);
		else {
			pda_dev->larb_pdev[i] = larb_pdev;
			dev_info(dev, "link smi larb%d\n", i);
		}
	}

	num_ports = of_count_phandle_with_args(
					pdev->dev.of_node, "iommus", "#iommu-cells");
	num_ports = (num_ports < 0) ? 0 : num_ports;
	dev_info(dev, "port_num:%d\n", num_ports);

	for (i = 0; i < num_ports; i++) {
		if (!of_parse_phandle_with_args(pdev->dev.of_node,
			"iommus", "#iommu-cells", i, &args)) {
			mtk_iommu_register_fault_callback(
				args.args[0],
				mtk_pda_translation_fault_callback,
				(void *)pda_dev, false);
		}
	}

	smmus = of_property_count_u32_elems(
		pdev->dev.of_node, "mediatek,smmu-dma-axid");
	smmus = (smmus > 0) ? smmus : 0;
	dev_info(dev, "smmu_num:%d\n", smmus);
	for (i = 0; i < smmus; i++) {
		u32 axid;

		if (!of_property_read_u32_index(
			pdev->dev.of_node, "mediatek,smmu-dma-axid", i, &axid)) {
			mtk_iommu_register_fault_callback(
				axid, mtk_pda_translation_fault_callback,
				(void *)pda_dev, false);
		}
	}
#ifdef CONFIG_PM_SLEEP
	pda_dev->notifier_blk.notifier_call = mtk_pda_suspend_pm_event;
	pda_dev->notifier_blk.priority = 0;
	ret = register_pm_notifier(&pda_dev->notifier_blk);
	if (ret) {
		dev_info(dev, "Failed to register PM notifier");
		return -ENODEV;
	}
#endif
	return 0;
}

static int mtk_pda_component_bind(
	struct device *dev,
	struct device *master,
	void *data)
{
	struct mtk_pda_device *pda_dev = dev_get_drvdata(dev);
	struct mtk_cam_device *cam_dev = data;

	pda_dev->cam = cam_dev;
	return mtk_cam_set_dev_pda(cam_dev->dev, pda_dev->id, dev);
}

static void mtk_pda_component_unbind(
	struct device *dev,
	struct device *master,
	void *data)
{
}

static const struct component_ops mtk_pda_component_ops = {
	.bind = mtk_pda_component_bind,
	.unbind = mtk_pda_component_unbind,
};

static int mtk_pda_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_pda_device *pda_dev;
	int ret;

	pda_dev = devm_kzalloc(dev, sizeof(*pda_dev), GFP_KERNEL);
	if (!pda_dev)
		return -ENOMEM;

	pda_dev->dev = dev;
	dev_set_drvdata(dev, pda_dev);

	ret = mtk_pda_of_probe(pdev, pda_dev);
	if (ret)
		return ret;

	ret = mtk_cam_qos_probe(dev, &pda_dev->qos, SMI_PORT_PDA_NUM);
	if (ret)
		goto UNREGISTER_PM_NOTIFIER;


	pda_dev->fifo_size =
		roundup_pow_of_two(8 * sizeof(struct mtk_camsys_irq_info));
	pda_dev->msg_buffer = devm_kzalloc(dev, pda_dev->fifo_size,
					     GFP_KERNEL);
	if (!pda_dev->msg_buffer) {
		ret = -ENOMEM;
		goto UNREGISTER_PM_NOTIFIER;
	}

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_pda_component_ops);

	if (ret)
		goto UNREGISTER_PM_NOTIFIER;

	return ret;

UNREGISTER_PM_NOTIFIER:
	unregister_pm_notifier(&pda_dev->notifier_blk);
	return ret;
}

static void mtk_pda_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_pda_device *pda_dev = dev_get_drvdata(dev);

#ifdef CONFIG_PM_SLEEP
	unregister_pm_notifier(&pda_dev->notifier_blk);
#endif
	pm_runtime_disable(dev);

	component_del(dev, &mtk_pda_component_ops);

}

int mtk_pda_runtime_suspend(struct device *dev)
{
	struct mtk_pda_device *pda_dev = dev_get_drvdata(dev);
	int i;

	pda_reset(pda_dev);

	//-------- setting ofl mode ---------
	// 0x9:
	// PDA_OTF_otf_db_load_combine = 1
	// PDA_OTF_dcif_en = 0
	// PDA_OTF_otf_mode_trig = 0
	// PDA_OTF_ofl_mode_trig = 1
	writel_relaxed(0x9, pda_dev->base + REG_E_PDA_OTF_DCIF_CTL);
	//----------------------------------

	if (readl_relaxed(pda_dev->base + REG_E_PDA_OTF_DCIF_CTL) != 0x8)
		dev_info(pda_dev->dev, "REG_E_PDA_OTF_DCIF_CTL(0x6b8) = 0x%x  (expected 0x8)\n",
			readl_relaxed(pda_dev->base + REG_E_PDA_OTF_DCIF_CTL));

	for (i = 0; i < PDA_IRQ_NUM; i++) {
		disable_irq(pda_dev->irq[i]);
		dev_info(dev, "%s:disable irq %d\n", __func__, pda_dev->irq[i]);
	}

	mtk_cam_isp8s_bwr_clr_bw(pda_dev->cam->bwr, ENGINE_PDA, CAM2_PORT);

	mtk_cam_reset_qos(dev, &pda_dev->qos);
	dev_dbg(dev, "%s:disable clock\n", __func__);

	vmm_disable_cvfs(VMM_CVFS_USR_PDA, VMM_CVFS_CAM_SEL);

	for (i = pda_dev->num_clks - 1; i >= 0; i--)
		clk_disable_unprepare(pda_dev->clks[i]);

	for (i = pda_dev->num_larbs - 1; i >=0 ; i--)
		mtk_smi_larb_disable(&pda_dev->larb_pdev[i]->dev);

	return 0;
}

int mtk_pda_runtime_resume(struct device *dev)
{
	struct mtk_pda_device *pda_dev = dev_get_drvdata(dev);
	int i, ret;

	for (i = 0; i < pda_dev->num_larbs; i++)
		mtk_smi_larb_enable(&pda_dev->larb_pdev[i]->dev);

	/* reset_msgfifo before enable_irq */
	ret = reset_msgfifo(pda_dev);
	if (ret)
		return ret;

	dev_dbg(dev, "%s:enable clock\n", __func__);
	for (i = 0; i < pda_dev->num_clks; i++) {
		ret = clk_prepare_enable(pda_dev->clks[i]);
		if (ret) {
			dev_info(dev, "enable failed at clk #%d, ret = %d\n",
				 i, ret);
			i--;
			while (i >= 0)
				clk_disable_unprepare(pda_dev->clks[i--]);

			return ret;
		}
	}

	vmm_enable_cvfs(VMM_CVFS_USR_PDA, VMM_CVFS_CAM_SEL);

	pda_reset(pda_dev);

	//-------- setting ofl mode ---------
	// 0x9:
	// PDA_OTF_otf_db_load_combine = 1
	// PDA_OTF_dcif_en = 0
	// PDA_OTF_otf_mode_trig = 0
	// PDA_OTF_ofl_mode_trig = 1
	writel_relaxed(0x9, pda_dev->base + REG_E_PDA_OTF_DCIF_CTL);
	//----------------------------------

	if (readl_relaxed(pda_dev->base + REG_E_PDA_OTF_DCIF_CTL) != 0x8)
		dev_info(pda_dev->dev, "REG_E_PDA_OTF_DCIF_CTL(0x6b8) = 0x%x  (expected 0x8)\n",
			readl_relaxed(pda_dev->base + REG_E_PDA_OTF_DCIF_CTL));

	for (i = 0; i < PDA_IRQ_NUM; i++) {
		enable_irq(pda_dev->irq[i]);
		dev_info(dev, "%s:enable irq %d\n", __func__, pda_dev->irq[i]);
	}

	return 0;
}

static const struct dev_pm_ops mtk_pda_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_pda_runtime_suspend, mtk_pda_runtime_resume,
			   NULL)
};

struct platform_driver mtk_cam_pda_driver = {
	.probe   = mtk_pda_probe,
	.remove  = mtk_pda_remove,
	.driver  = {
		.name  = "mtk-cam pda",
		.of_match_table = of_match_ptr(mtk_pda_of_ids),
		.pm     = &mtk_pda_pm_ops,
	}
};
