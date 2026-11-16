// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */

#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include "mtk_imgsys-dip.h"
#include "mtk-hcp.h"
#include "mtk_imgsys-v4l2-debug.h"

// GCE header
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "../../cmdq/isp8s/mtk_imgsys-cmdq-qof.h"

const struct mtk_imgsys_init_array mtk_imgsys_dip_init_ary[] = {
	{0x0B0, 0x00000001}, /* DIPCTL_D1A_DIPCTL_INT0_EN */
	{0x240, 0x00000001}, /* DIPCTL_QOF_CTL */
	{0x018, 0x00000007}, /* WIF_TIF_DL_EN */
};

static struct DIPRegDumpInfo g_DIPRegDumpTopIfo[] = {
	{ 0x0000, 0x03E4},
	{ 0x1010, 0x101C},
	{ 0x1070, 0x1194},
	{ 0x1200, 0x138C},
	{ 0x14A0, 0x1B1C},
	{ 0x2000, 0x250C},
	{ 0x26E0, 0x2CB8},
	{ 0x3000, 0x3430},
	{ 0x4000, 0x455C},
	{ 0x5000, 0x5440},
	{ 0x584C, 0x5E64},
	{ 0x7400, 0x7648},
	{ 0x9000, 0x994C},
};

static struct DIPRegDumpInfo g_DIPRegDumpNr1Ifo[] = {
	{ 0x0000, 0x0230},
	{ 0x1000, 0x1030},
	{ 0x2000, 0x2414},
	{ 0x3000, 0x36AC},
	{ 0x4000, 0x413C},
	{ 0x4540, 0x4968},
	{ 0x5004, 0x50EC},
	{ 0x6534, 0x6874},
	{ 0x7400, 0x7400},
	{ 0x7804, 0x7804},
	{ 0x8000, 0x8088},
	{ 0xCF00, 0xCF08},
};

static struct DIPRegDumpInfo g_DIPRegDumpNr2Ifo[] = {
	{ 0x0000, 0x0230},
	{ 0x1014, 0x1024},
	{ 0x1064, 0x11B8},
	{ 0x1200, 0x1DBC},
	{ 0x2000, 0x224C},
	{ 0x3000, 0x38F8},
	{ 0x4000, 0x4430},
	{ 0x5400, 0x5994},
	{ 0x6000, 0x6A14},
	{ 0x7000, 0x75D0},
	{ 0x8E00, 0x8EA0},
};

static struct DIPRegDumpInfo g_DIPRegDumpCineIfo[] = {
	{ 0x0000, 0x0230},
	{ 0x18E4, 0x19B4},
	{ 0x1D00, 0x1DCC},
	{ 0x2000, 0x2040},
	{ 0x2440, 0x2604},
	{ 0x27C0, 0x282C},
};

static struct DIPDmaDebugInfo g_DMATopDbgIfo[] = {
	{"IMGI", DIP_ORI_RDMA_UFO_DEBUG, 0x0},
	{"IMGI_N", DIP_ORI_RDMA_UFO_DEBUG, 0x1000},
	{"IMGBI", DIP_ORI_RDMA_UFO_DEBUG, 0x1},
	{"IMGBI_N", DIP_ORI_RDMA_UFO_DEBUG, 0x1001},
	{"IMGCI", DIP_ORI_RDMA_UFO_DEBUG, 0x2},
	{"IMGCI_N", DIP_ORI_RDMA_UFO_DEBUG, 0x1002},
	{"IMGDI", DIP_ORI_RDMA_UFO_DEBUG, 0x3},
	{"IMGDI_N", DIP_ORI_RDMA_UFO_DEBUG, 0x1003},
	{"SMTI1", DIP_ULC_RDMA_DEBUG, 0x4},
	{"SMTCI1", DIP_ULC_RDMA_DEBUG, 0x5},
	{"SMTI2", DIP_ULC_RDMA_DEBUG, 0x6},
	{"SMTCI2", DIP_ULC_RDMA_DEBUG, 0x7},
	{"SMTI3", DIP_ULC_RDMA_DEBUG, 0x8},
	{"SMTI10", DIP_ULC_RDMA_DEBUG, 0x9},
	{"SMTI11", DIP_ULC_RDMA_DEBUG, 0xA},
	{"TNRSI", DIP_ULC_RDMA_DEBUG, 0xB},
	{"TNRWI", DIP_ULC_RDMA_DEBUG, 0xC},
	{"TNRMI", DIP_ULC_RDMA_DEBUG, 0xD},
	{"TNRLYI", DIP_ULC_RDMA_DEBUG, 0xE},
	{"TNRLCI", DIP_ULC_RDMA_DEBUG, 0xF},
	{"TNRVBI", DIP_ULC_RDMA_DEBUG, 0x10},
	{"SNRSGMI", DIP_ULC_RDMA_DEBUG, 0x11},
	{"TNRCI", DIP_ULC_RDMA_DEBUG, 0x12},
	{"TNRCI_N", DIP_ULC_RDMA_DEBUG, 0x1012},
	{"TNRFGMI", DIP_ULC_RDMA_DEBUG, 0x13},
	{"TNRFGMI_N", DIP_ULC_RDMA_DEBUG, 0x1013},
	{"RECI1", DIP_ULC_RDMA_DEBUG, 0x14},
	{"RECI1_N", DIP_ULC_RDMA_DEBUG, 0x1014},
	{"RECBI1", DIP_ULC_RDMA_DEBUG, 0x15},
	{"RECI2", DIP_ULC_RDMA_DEBUG, 0x16},
	{"RECI2_N", DIP_ULC_RDMA_DEBUG, 0x1016},
	{"RECBI2", DIP_ULC_RDMA_DEBUG, 0x17},
	{"RECI3", DIP_ULC_RDMA_DEBUG, 0x18},
	{"RECBI3", DIP_ULC_RDMA_DEBUG, 0x19},
	{"YUFETI2", DIP_ULC_RDMA_DEBUG, 0x1A},
	{"YUFETCI2", DIP_ULC_RDMA_DEBUG, 0x1B},
	{"YUFETI3", DIP_ULC_RDMA_DEBUG, 0x1C},
	{"YUFETCI3", DIP_ULC_RDMA_DEBUG, 0x1D},
	{"IMG3O", DIP_ORI_WDMA_DEBUG, 0x1E},
	{"IMG3O_N", DIP_ORI_WDMA_DEBUG, 0x101E},
	{"IMG3BO", DIP_ORI_WDMA_DEBUG, 0x1F},
	{"IMG3BO_N", DIP_ORI_WDMA_DEBUG, 0x101F},
	{"IMG3CO", DIP_ORI_WDMA_DEBUG, 0x20},
	{"IMG3CO_N", DIP_ORI_WDMA_DEBUG, 0x1020},
	{"IMG3DO", DIP_ORI_WDMA_DEBUG, 0x21},
	{"IMG3DO_N", DIP_ORI_WDMA_DEBUG, 0x1021},
	{"IMG2O", DIP_ORI_WDMA_DEBUG, 0x22},
	{"IMG2O_N", DIP_ORI_WDMA_DEBUG, 0x1022},
	{"IMG2BO", DIP_ORI_WDMA_DEBUG, 0x23},
	{"IMG2CO", DIP_ORI_WDMA_DEBUG, 0x24},
	{"IMG2CO_N", DIP_ORI_WDMA_DEBUG, 0x1024},
	{"IMG2DO", DIP_ORI_WDMA_DEBUG, 0x25},
	{"SMTO1", DIP_ULC_WDMA_DEBUG, 0x26},
	{"SMTCO1", DIP_ULC_WDMA_DEBUG, 0x27},
	{"SMTO2", DIP_ULC_WDMA_DEBUG, 0x28},
	{"SMTCO2", DIP_ULC_WDMA_DEBUG, 0x29},
	{"SMTO3", DIP_ULC_WDMA_DEBUG, 0x2A},
	{"SMTO10", DIP_ULC_WDMA_DEBUG, 0x2B},
	{"SMTO11", DIP_ULC_WDMA_DEBUG, 0x2C},
	{"TNRMO", DIP_ULC_WDMA_DEBUG, 0x2D},
	{"TNRMO_N", DIP_ULC_WDMA_DEBUG, 0x102D},
	{"TNRSO", DIP_ULC_WDMA_DEBUG, 0x2E},
	{"TNRWO", DIP_ULC_WDMA_DEBUG, 0x2F},
	{"YUFETO2", DIP_ULC_WDMA_DEBUG, 0x30},
	{"YUFETCO2", DIP_ULC_WDMA_DEBUG, 0x31},
	{"YUFETO3", DIP_ULC_WDMA_DEBUG, 0x32},
	{"YUFETCO3", DIP_ULC_WDMA_DEBUG, 0x33},
	{"FHO2", DIP_ULC_WDMA_DEBUG, 0x34},
	{"FHO3", DIP_ULC_WDMA_DEBUG, 0x35},
	{"FHO4", DIP_ULC_WDMA_DEBUG, 0x36},
};

static struct DIPDmaDebugInfo g_DMANrDbgIfo[] = {
	{"VIPI", DIP_ULC_RDMA_DEBUG, 0x0},
	{"VIPI_N", DIP_ULC_RDMA_DEBUG, 0x1000},
	{"VIPBI", DIP_ULC_RDMA_DEBUG, 0x1},
	{"VIPBI_N", DIP_ULC_RDMA_DEBUG, 0x1001},
	{"VIPCI", DIP_ULC_RDMA_DEBUG, 0x2},
	{"SMTI4", DIP_ULC_RDMA_DEBUG, 0x3},
	{"SMTCI4", DIP_ULC_RDMA_DEBUG, 0x4},
	{"SMTI5", DIP_ULC_RDMA_DEBUG, 0x5},
	{"SMTCI5", DIP_ULC_RDMA_DEBUG, 0x6},
	{"SMTI6", DIP_ULC_RDMA_DEBUG, 0x7},
	{"SMTCI6", DIP_ULC_RDMA_DEBUG, 0x8},
	{"SMTI8", DIP_ULC_RDMA_DEBUG, 0x9},
	{"SMTCI8", DIP_ULC_RDMA_DEBUG, 0xA},
	{"SMTI9", DIP_ULC_RDMA_DEBUG, 0xB},
	{"SMTCI9", DIP_ULC_RDMA_DEBUG, 0xC},
	{"SMTI12", DIP_ULC_RDMA_DEBUG, 0xD},
	{"SMTCI12", DIP_ULC_RDMA_DEBUG, 0xE},
	{"SNRCSI1", DIP_ULC_RDMA_DEBUG, 0xF},
	{"SNRCSI2", DIP_ULC_RDMA_DEBUG, 0x10},
	{"SNRAIMI", DIP_ULC_RDMA_DEBUG, 0x11},
	{"SNRAIMI_N", DIP_ULC_RDMA_DEBUG, 0x1011},
	{"SNRGMI", DIP_ULC_RDMA_DEBUG, 0x12},
	{"EECSI1", DIP_ULC_RDMA_DEBUG, 0x13},
	{"EECSI2", DIP_ULC_RDMA_DEBUG, 0x14},
	{"CSMCSI1", DIP_ULC_RDMA_DEBUG, 0x15},
	{"CSMCSI1_N", DIP_ULC_RDMA_DEBUG, 0x1015},
	{"CSMCSI2", DIP_ULC_RDMA_DEBUG, 0x16},
	{"CSMCSI2_N", DIP_ULC_RDMA_DEBUG, 0x1016},
	{"CSMCSTI1", DIP_ULC_RDMA_DEBUG, 0x17},
	{"CSMCSTI2", DIP_ULC_RDMA_DEBUG, 0x18},
	{"CSMCI", DIP_ULC_RDMA_DEBUG, 0x19},
	{"CSMCBI", DIP_ULC_RDMA_DEBUG, 0x1A},
	{"CSMCCI", DIP_ULC_RDMA_DEBUG, 0x1B},
	{"CSMCDI", DIP_ULC_RDMA_DEBUG, 0x1C},
	{"BOKMI", DIP_ULC_RDMA_DEBUG, 0x20},
	{"BOKMI_N", DIP_ULC_RDMA_DEBUG, 0x1020},
	{"BOKPYI", DIP_ULC_RDMA_DEBUG, 0x21},
	{"BOKPCI", DIP_ULC_RDMA_DEBUG, 0x22},
	{"DMGI", DIP_ULC_RDMA_DEBUG, 0x23},
	{"DMGI_N", DIP_ULC_RDMA_DEBUG, 0x1023},
	{"TNCCSI1", DIP_ULC_RDMA_DEBUG, 0x24},
	{"TNCCSI2", DIP_ULC_RDMA_DEBUG, 0x25},
	{"RACWI", DIP_ULC_RDMA_DEBUG, 0x26},
	{"RACCSI", DIP_ULC_RDMA_DEBUG, 0x27},
	{"RACWSI", DIP_ULC_RDMA_DEBUG, 0x28},
	{"LMEI", DIP_ULC_RDMA_DEBUG, 0x29},
	{"VDCECSI", DIP_ULC_RDMA_DEBUG, 0x2A},
	{"VDCELFI", DIP_ULC_RDMA_DEBUG, 0x2B},
	{"VDCELFI_N", DIP_ULC_RDMA_DEBUG, 0x102B},
	{"FPNRCSI", DIP_ULC_RDMA_DEBUG, 0x2C},
	{"YUFETI1", DIP_ULC_RDMA_DEBUG, 0x2D},
	{"YUFETCI1", DIP_ULC_RDMA_DEBUG, 0x2E},
	{"IMG4O", DIP_ORI_WDMA_DEBUG, 0x2F},
	{"IMG4O_N", DIP_ORI_WDMA_DEBUG, 0x102F},
	{"IMG4BO", DIP_ORI_WDMA_DEBUG, 0x30},
	{"IMG4CO", DIP_ORI_WDMA_DEBUG, 0x31},
	{"IMG4CO_N", DIP_ORI_WDMA_DEBUG, 0x1031},
	{"IMG4DO", DIP_ORI_WDMA_DEBUG, 0x32},
	{"IMG5O", DIP_ORI_WDMA_DEBUG, 0x33},
	{"IMG5BO", DIP_ORI_WDMA_DEBUG, 0x34},
	{"IMG6O", DIP_ORI_WDMA_DEBUG, 0x35},
	{"IMG6BO", DIP_ORI_WDMA_DEBUG, 0x36},
	{"SMTO4", DIP_ULC_WDMA_DEBUG, 0x37},
	{"SMTCO4", DIP_ULC_WDMA_DEBUG, 0x38},
	{"SMTO5", DIP_ULC_WDMA_DEBUG, 0x39},
	{"SMTCO5", DIP_ULC_WDMA_DEBUG, 0x3A},
	{"SMTO6", DIP_ULC_WDMA_DEBUG, 0x3B},
	{"SMTCO6", DIP_ULC_WDMA_DEBUG, 0x3C},
	{"SMTO8", DIP_ULC_WDMA_DEBUG, 0x3D},
	{"SMTCO8", DIP_ULC_WDMA_DEBUG, 0x3E},
	{"SMTO9", DIP_ULC_WDMA_DEBUG, 0x3F},
	{"SMTCO9", DIP_ULC_WDMA_DEBUG, 0x40},
	{"SMTO12", DIP_ULC_WDMA_DEBUG, 0x41},
	{"SMTCO12", DIP_ULC_WDMA_DEBUG, 0x42},
	{"SNRACTO", DIP_ULC_WDMA_DEBUG, 0x43},
	{"SNRACTO_N", DIP_ULC_WDMA_DEBUG, 0x1043},
	{"CSMCSO1", DIP_ULC_WDMA_DEBUG, 0x46},
	{"CSMCSO2", DIP_ULC_WDMA_DEBUG, 0x47},
	{"BOKMO", DIP_ULC_WDMA_DEBUG, 0x48},
	{"BOKMO_N", DIP_ULC_WDMA_DEBUG, 0x1048},
	{"FEO", DIP_ULC_WDMA_DEBUG, 0x49},
	{"RACWO", DIP_ULC_WDMA_DEBUG, 0x4A},
	{"RACWO_N", DIP_ULC_WDMA_DEBUG, 0x104A},
	{"RACWSO", DIP_ULC_WDMA_DEBUG, 0x4B},
	{"RACWSO_N", DIP_ULC_WDMA_DEBUG, 0x104B},
	{"LMEO", DIP_ULC_WDMA_DEBUG, 0x4C},
	{"LMEO_N", DIP_ULC_WDMA_DEBUG, 0x104C},
	{"YUFETO1", DIP_ULC_WDMA_DEBUG, 0x4D},
	{"YUFETCO1", DIP_ULC_WDMA_DEBUG, 0x4E},
	{"FHO1", DIP_ULC_WDMA_DEBUG, 0x4F},
};

struct mtk_imgsys_dip_dtable {
	uint32_t empty;
	uint32_t addr;
	uint32_t addr_msb;
};

#define DIP_HW_SET 4
#define SW_RST   (0x000C)

static void __iomem *gdipRegBA[DIP_HW_SET] = {0L};
static void __iomem *gVcoreRegBA;
static unsigned int g_RegBaseAddr = DIP_TOP_ADDR;
static unsigned int g_RegBaseAddrTop = DIP_TOP_ADDR;
static unsigned int g_RegBaseAddrNr1 = DIP_NR1_ADDR;
static unsigned int g_RegBaseAddrNr2 = DIP_NR2_ADDR;
static unsigned int g_RegBaseAddrCine = DIP_CINE_ADDR;

int imgsys_dip_tfault_callback(int port, //YWTBD tf
	dma_addr_t mva, void *data)
{
	void __iomem *dipRegBA = 0L;
	unsigned int larb = 0;
	unsigned int i, j, k;
	int ret = 0;
	bool is_qof = false;

	pr_debug("%s: +\n", __func__);

	ret = smi_isp_dip_get_if_in_use((void *)&is_qof);
	if (ret == -1) {
		pr_info("smi_isp_dip_get_if_in_use = -1.stop dump. return\n");
		return 1;
	}

	larb = ((port>>5) & 0x3F);
	pr_info("%s: iommu port:0x%x, larb:%d, idx:%d, addr:0x%08lx\n", __func__,
		port, larb, (port & 0x1F), (unsigned long)mva);

	/* 0x34190000~ */
	dipRegBA = gdipRegBA[0];

	/* top reg */
	for (j = 0; j < sizeof(g_DIPRegDumpTopIfo)/sizeof(struct DIPRegDumpInfo); j++) {
		k = g_DIPRegDumpTopIfo[j].oft & 0xFFF0;
		for (i = k; i <= g_DIPRegDumpTopIfo[j].end; i += 0x10) {
			pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
				(unsigned int)(g_RegBaseAddrTop + i),
				(unsigned int)ioread32((void *)(dipRegBA + i)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
		}
	}

	/* 0x341A0000~ */
	dipRegBA = gdipRegBA[1];

	/* nr1 reg */
	for (j = 0; j < sizeof(g_DIPRegDumpNr1Ifo)/sizeof(struct DIPRegDumpInfo); j++) {
		k = g_DIPRegDumpNr1Ifo[j].oft & 0xFFF0;
		for (i = k; i <= g_DIPRegDumpNr1Ifo[j].end; i += 0x10) {
			pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
				(unsigned int)(g_RegBaseAddrNr1 + i),
				(unsigned int)ioread32((void *)(dipRegBA + i)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
		}
	}

	/* 0x341B0000~ */
	dipRegBA = gdipRegBA[2];

	/* nr2 reg */
	for (j = 0; j < sizeof(g_DIPRegDumpNr2Ifo)/sizeof(struct DIPRegDumpInfo); j++) {
		k = g_DIPRegDumpNr2Ifo[j].oft & 0xFFF0;
		for (i = k; i <= g_DIPRegDumpNr2Ifo[j].end; i += 0x10) {
			pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
				(unsigned int)(g_RegBaseAddrNr2 + i),
				(unsigned int)ioread32((void *)(dipRegBA + i)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
		}
	}

	/* 0x341D0000~ */
	dipRegBA = gdipRegBA[3];

	/* cine reg */
	for (j = 0; j < sizeof(g_DIPRegDumpCineIfo)/sizeof(struct DIPRegDumpInfo); j++) {
		k = g_DIPRegDumpCineIfo[j].oft & 0xFFF0;
		for (i = k; i <= g_DIPRegDumpCineIfo[j].end; i += 0x10) {
			pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
				(unsigned int)(g_RegBaseAddrCine + i),
				(unsigned int)ioread32((void *)(dipRegBA + i)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
		}
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

	smi_isp_dip_put((void *)&is_qof);
	pr_info("%s: -\n", __func__);
	return 1;
}

void imgsys_dip_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int hw_idx = 0, ary_idx = 0;

	for (hw_idx = REG_MAP_E_DIP; hw_idx <= REG_MAP_E_DIP_CINE; hw_idx++) {
		/* iomap registers */
		ary_idx = hw_idx - REG_MAP_E_DIP;
		gdipRegBA[ary_idx] = of_iomap(imgsys_dev->dev->of_node, hw_idx);
		if (!gdipRegBA[ary_idx]) {
			pr_info("%s:unable to iomap dip_%d reg, devnode()\n",
				__func__, hw_idx);
			continue;
		}
	}
	/* iomap registers */
	gVcoreRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_IMG_VCORE);
	if (!gVcoreRegBA) {
		pr_info("%s:unable to iomap Vcore reg, devnode()\n",
			__func__);
	}

	if (imgsys_dev->dev_ver == 1) {
		g_RegBaseAddrTop = DIP_TOP_ADDR_P;
		g_RegBaseAddrNr1 = DIP_NR1_ADDR_P;
		g_RegBaseAddrNr2 = DIP_NR2_ADDR_P;
		g_RegBaseAddrCine = DIP_CINE_ADDR_P;
	}

}

void imgsys_dip_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *dipRegBA = 0L;
	void __iomem *ofset = NULL;
	unsigned int i;

	/* iomap registers */
	dipRegBA = gdipRegBA[0];

	for (i = 0; i < ARRAY_SIZE(mtk_imgsys_dip_init_ary); i++) {
		ofset = dipRegBA + mtk_imgsys_dip_init_ary[i].ofset;
		writel(mtk_imgsys_dip_init_ary[i].val, ofset);
	}

}

void imgsys_dip_updatecq(struct mtk_imgsys_dev *imgsys_dev,
			struct img_swfrm_info *user_info,
			struct private_data *priv_data,
			int req_fd,
			u64 tuning_iova,
			unsigned int mode)
{
	const struct mtk_hcp_ops *hcp_ops = mtk_hcp_fetch_ops(imgsys_dev->scp_pdev);
	u64 iova_addr = tuning_iova;
	u64 *cq_desc = NULL;
	struct mtk_imgsys_dip_dtable *dtable = NULL;
	unsigned int i = 0, tun_ofst = 0;
	struct flush_buf_info dip_buf_info;
	void *cq_base = NULL;

	if (hcp_ops && hcp_ops->fetch_dip_cq_mb_virt)
		cq_base = hcp_ops->fetch_dip_cq_mb_virt(imgsys_dev->scp_pdev, mode);

	if (cq_base == NULL) {
		//pr_debug("%s: cq_base NULL\n", __func__);
		return;
	}

	/* HWID defined in hw_definition.h */
	if (priv_data->need_update_desc) {
		if (iova_addr) {
			cq_desc = (u64 *)((void *)(cq_base +
					priv_data->desc_offset));

			for (i = 0; i < DIP_CQ_DESC_NUM; i++) {
				dtable = (struct mtk_imgsys_dip_dtable *)cq_desc + i;
				if ((dtable->addr_msb & PSEUDO_DESC_TUNING) == PSEUDO_DESC_TUNING) {
					tun_ofst = dtable->addr;
					dtable->addr = (tun_ofst + iova_addr) & 0xFFFFFFFF;
					dtable->addr_msb |= ((tun_ofst + iova_addr) >> 32) & 0xF;
					if (imgsys_dip_8s_dbg_enable())
						pr_debug("%s: tuning_buf_iova(0x%llx) des_ofst(0x%08x) cq_kva(0x%p) dtable(0x%x/0x%x/0x%x)\n",
							__func__, iova_addr,
							priv_data->desc_offset,
							cq_desc, dtable->empty, dtable->addr,
							dtable->addr_msb);
				}
			}
		}
		//
		if (hcp_ops && hcp_ops->fetch_dip_cq_mb_fd)
			dip_buf_info.fd = hcp_ops->fetch_dip_cq_mb_fd(imgsys_dev->scp_pdev, mode);
		dip_buf_info.offset = priv_data->desc_offset;
		dip_buf_info.len =
			(sizeof(struct mtk_imgsys_dip_dtable) * DIP_CQ_DESC_NUM) + DIP_REG_SIZE;
		dip_buf_info.mode = mode;
		dip_buf_info.is_tuning = false;
		if (imgsys_dip_8s_dbg_enable())
			pr_debug("imgsys_fw cq dip_buf_info (%d/%d/%d), mode(%d)",
				dip_buf_info.fd, dip_buf_info.len,
				dip_buf_info.offset, dip_buf_info.mode);

		if (hcp_ops && hcp_ops->flush_mb && hcp_ops->fetch_dip_cq_mb_id)
			hcp_ops->flush_mb(
				imgsys_dev->scp_pdev,
				hcp_ops->fetch_dip_cq_mb_id(imgsys_dev->scp_pdev, mode),
				dip_buf_info.offset,
				dip_buf_info.len);
	}

	if (priv_data->need_flush_tdr) {
		// tdr buffer
		if (hcp_ops && hcp_ops->fetch_dip_tdr_mb_fd)
			dip_buf_info.fd = hcp_ops->fetch_dip_tdr_mb_fd(imgsys_dev->scp_pdev, mode);
		dip_buf_info.offset = priv_data->tdr_offset;
		dip_buf_info.len = DIP_TDR_BUF_MAXSZ;
		dip_buf_info.mode = mode;
		dip_buf_info.is_tuning = false;
		if (imgsys_dip_8s_dbg_enable())
			pr_debug("imgsys_fw tdr dip_buf_info (%d/%d/%d), mode(%d)",
				dip_buf_info.fd, dip_buf_info.len,
				dip_buf_info.offset, dip_buf_info.mode);
		if (hcp_ops && hcp_ops->flush_mb && hcp_ops->fetch_dip_tdr_mb_id)
			hcp_ops->flush_mb(
				imgsys_dev->scp_pdev,
				hcp_ops->fetch_dip_tdr_mb_id(imgsys_dev->scp_pdev, mode),
				dip_buf_info.offset,
				dip_buf_info.len);
	}
}

int imgsys_dip_check_power_domain(struct mtk_imgsys_dev *imgsys_dev,
				  struct img_swfrm_info *user_info,
				  struct private_data *priv_data,
				  unsigned int mode)
{
	const struct mtk_hcp_ops *hcp_ops = mtk_hcp_fetch_ops(imgsys_dev->scp_pdev);
	uint32_t *cq_desc = NULL, *cine_sel = NULL;
	void *cq_base = NULL;

	if (hcp_ops && hcp_ops->fetch_dip_cq_mb_virt)
		cq_base = hcp_ops->fetch_dip_cq_mb_virt(imgsys_dev->scp_pdev, mode);

	if (cq_base == NULL)
		return 0;

	if (priv_data->desc_offset == 0xffffffff)
		return 0;

	cq_desc = (uint32_t *)((void *)(cq_base +
		priv_data->desc_offset));

	cine_sel = cq_desc + (DIP_CINE_SEL_VA_OFST / sizeof(uint32_t));
	if (*cine_sel & 0x1000000)
		return 1; // DIP_CINE enable
	else
		return 0;
}

void imgsys_dip_cmdq_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev, void *pkt, int hw_idx)
{
	unsigned int ofset;
	unsigned int i;
	unsigned int imgsys_dip_base = IMGSYS_DIP_BASE;
	unsigned int dip_base = DIP_TOP_ADDR;
	struct cmdq_pkt *package = NULL;

	if (imgsys_dev == NULL || pkt == NULL) {
		dump_stack();
		pr_err("[%s][%d] param fatal error!", __func__, __LINE__);
		return;
	}
	package = (struct cmdq_pkt *)pkt;

	if (imgsys_dev->dev_ver == 1) {
		imgsys_dip_base = IMGSYS_DIP_BASE_P;
		dip_base = DIP_TOP_ADDR_P;
	}

	cmdq_pkt_write(package, NULL, (imgsys_dip_base + SW_RST) /*address*/,
			       0x3FC03, 0xffffffff);
	cmdq_pkt_write(package, NULL, (imgsys_dip_base + SW_RST) /*address*/,
			       0x0, 0xffffffff);

	/* iomap registers */
	for (i = 0; i < ARRAY_SIZE(mtk_imgsys_dip_init_ary); i++) {
		ofset = dip_base + mtk_imgsys_dip_init_ary[i].ofset;
		cmdq_pkt_write(package, NULL, ofset /*address*/,
				mtk_imgsys_dip_init_ary[i].val, 0xffffffff);
	}
}

static unsigned int ExeDbgCmd(struct mtk_imgsys_dev *a_pDev,
			void __iomem *a_pRegBA,
			unsigned int a_DdbSel,
			unsigned int a_DbgOut,
			unsigned int a_DbgCmd)
{
	unsigned int DbgData = 0;
	unsigned int DbgOutReg = g_RegBaseAddr + a_DbgOut;
	void __iomem *pDbgSel = (void *)(a_pRegBA + a_DdbSel);
	void __iomem *pDbgPort = (void *)(a_pRegBA + a_DbgOut);

	iowrite32(a_DbgCmd, pDbgSel);
	DbgData = (unsigned int)ioread32(pDbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		a_DbgCmd, DbgOutReg, DbgData);

	return DbgData;
}

static void ExeDbgCmdNr3d(struct mtk_imgsys_dev *a_pDev,
			void __iomem *a_pRegBA,
			unsigned int a_DdbSel,
			unsigned int a_DbgCnt,
			unsigned int a_DbgSt,
			unsigned int a_DbgCmd)
{
	unsigned int DbgCntData = 0, DbgStData = 0;
	unsigned int DbgSelReg = g_RegBaseAddr + a_DdbSel;
	void __iomem *pDbgSel = (void *)(a_pRegBA + a_DdbSel);
	void __iomem *pDbgCnt = (void *)(a_pRegBA + a_DbgCnt);
	void __iomem *pDbgSt = (void *)(a_pRegBA + a_DbgSt);

	iowrite32(a_DbgCmd, pDbgSel);
	DbgCntData = (unsigned int)ioread32(pDbgCnt);
	DbgStData = (unsigned int)ioread32(pDbgSt);
	pr_info("[0x%08X](0x%08X,0x%08X,0x%08X)\n",
		a_DbgCmd, DbgSelReg, DbgCntData, DbgStData);
}

static void imgsys_dip_dump_dma(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut,
				char a_DMANrPort)
{
	unsigned int Idx = 0, DMAIdx = 0;
	unsigned int DbgCmd = 0;
	unsigned int DmaDegInfoSize = sizeof(struct DIPDmaDebugInfo);
	unsigned int DebugCnt = sizeof(g_DMATopDbgIfo)/DmaDegInfoSize;
	enum DIPDmaDebugType DbgTy = DIP_ORI_RDMA_DEBUG;

	/* DMA NR */
	if (a_DMANrPort == 1) {
		DebugCnt = sizeof(g_DMANrDbgIfo)/DmaDegInfoSize;
		pr_info("DIP_NR dump DMA port\n");
	} else {
		pr_info("DIP_TOP dump DMA port\n");
	}

	/* Dump DMA Debug Info */
	for (Idx = 0; Idx < DebugCnt; Idx++) {
		if (a_DMANrPort == 1) {
			DbgTy = g_DMANrDbgIfo[Idx].DMADebugType;
			DMAIdx = g_DMANrDbgIfo[Idx].DMAIdx;
		} else {
			DbgTy = g_DMATopDbgIfo[Idx].DMADebugType;
			DMAIdx = g_DMATopDbgIfo[Idx].DMAIdx;
		}

		/* state_checksum */
		DbgCmd = DIP_IMGI_STATE_CHECKSUM + DMAIdx;
		ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
		/* line_pix_cnt_tmp */
		DbgCmd = DIP_IMGI_LINE_PIX_CNT_TMP + DMAIdx;
		ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
		/* line_pix_cnt */
		DbgCmd = DIP_IMGI_LINE_PIX_CNT + DMAIdx;
		ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);

		/* important_status */
		if (DbgTy == DIP_ULC_RDMA_DEBUG ||
			DbgTy == DIP_ULC_WDMA_DEBUG) {
			DbgCmd = DIP_IMGI_IMPORTANT_STATUS + DMAIdx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
			DbgCmd);
		}

		/* smi_debug_data (case 0) or cmd_data_cnt */
		if (DbgTy == DIP_ORI_RDMA_DEBUG ||
			DbgTy == DIP_ORI_RDMA_UFO_DEBUG ||
			DbgTy == DIP_ULC_RDMA_DEBUG ||
			DbgTy == DIP_ULC_WDMA_DEBUG) {
			DbgCmd = DIP_IMGI_SMI_DEBUG_DATA_CASE0 + DMAIdx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
			DbgCmd);
		}

		/* ULC_RDMA or ULC_WDMA */
		if (DbgTy == DIP_ULC_RDMA_DEBUG ||
			DbgTy == DIP_ULC_WDMA_DEBUG) {
			DbgCmd = DIP_IMGI_TILEX_BYTE_CNT + DMAIdx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
			DbgCmd = DIP_IMGI_TILEY_CNT + DMAIdx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}

		/* smi_dbg_data(case 0) or burst_line_cnt or input_v_cnt */
		if (DbgTy == DIP_ORI_WDMA_DEBUG ||
			DbgTy == DIP_ULC_RDMA_DEBUG ||
			DbgTy == DIP_ULC_WDMA_DEBUG) {
			DbgCmd = DIP_IMGI_BURST_LINE_CNT + DMAIdx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}

		/* xfer_y_cnt */
		if (DbgTy == DIP_ULC_WDMA_DEBUG) {
			DbgCmd = DIP_IMGI_XFER_Y_CNT + DMAIdx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}

		/* ORI_RDMA */
		if (DbgTy == DIP_ORI_RDMA_DEBUG ||
			DbgTy == DIP_ORI_RDMA_UFO_DEBUG) {
			DbgCmd = DIP_IMGI_FIFO_DEBUG_DATA_CASE1 + DMAIdx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
			DbgCmd = DIP_IMGI_FIFO_DEBUG_DATA_CASE3 + DMAIdx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}

		/* ORI_RDMA_UFO or ULC_WDMA */
		if (DbgTy == DIP_ORI_RDMA_UFO_DEBUG ||
			DbgTy == DIP_ULC_WDMA_DEBUG) {
			DbgCmd = DIP_IMGI_UFO_STATE_CHECKSUM + DMAIdx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
			DbgCmd = DIP_IMGI_UFO_LINE_PIX_CNT_TMP + DMAIdx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
			DbgCmd = DIP_IMGI_UFO_LINE_PIX_CNT + DMAIdx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}

		/* ORI_WDMA */
		if (DbgTy == DIP_ORI_WDMA_DEBUG) {
			DbgCmd = DIP_YUVO_T1_FIFO_DEBUG_DATA_CASE1 + DMAIdx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
			DbgCmd = DIP_YUVO_T1_FIFO_DEBUG_DATA_CASE3 + DMAIdx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}

	}
	/* SMI_STATUS */
	if (a_DMANrPort == 1) {
		/* L39R */
		DbgCmd = 0x2C81005A;
		for (Idx = 0; Idx <= 10; Idx++) {
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
			DbgCmd += 0x100;
		}
		/* L15R */
		DbgCmd = 0x2C81005B;
		for (Idx = 0; Idx <= 8; Idx++) {
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
			DbgCmd += 0x100;
		}
		/* L39W */
		DbgCmd = 0x2C81005C;
		for (Idx = 0; Idx <= 8; Idx++) {
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
			DbgCmd += 0x100;
		}
		/* L15W */
		DbgCmd = 0x2C81005D;
		for (Idx = 0; Idx <= 6; Idx++) {
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
			DbgCmd += 0x100;
		}
	} else {
		/* L38R */
		DbgCmd = 0x2C810046;
		for (Idx = 0; Idx <= 9; Idx++) {
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
			DbgCmd += 0x100;
		}
		/* L10R */
		DbgCmd = 0x2C810047;
		for (Idx = 0; Idx <= 5; Idx++) {
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
			DbgCmd += 0x100;
		}
		/* L38W */
		DbgCmd = 0x2C810048;
		for (Idx = 0; Idx <= 9; Idx++) {
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
			DbgCmd += 0x100;
		}
		/* L10W */
		DbgCmd = 0x2C810049;
		for (Idx = 0; Idx <= 4; Idx++) {
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
			DbgCmd += 0x100;
		}
	}
}

static void imgsys_dip_dump_dl(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int DbgLineCnt = 0, DbgRdy = 0, DbgReq = 0;
	unsigned int DbgLineCntReg = 0;

	pr_info("dump dl debug\n");

	/* wpe_wif_d1_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000005;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[wpe_wif_d1_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000006;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) >> 16;
	pr_info("[wpe_wif_d1_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000007;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) >> 16;
	pr_info("[wpe_wif_d1_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);
	/* tif_cid_debug */
	DbgCmd = 0x00000008;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	pr_info("[wpe_wif_d1_debug]tif_cid_debug(0x%X)\n", DbgData);

	/* omc_wif_d2_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000105;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[omc_wif_d2_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000106;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) >> 16;
	pr_info("[omc_wif_d2_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000107;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) >> 16;
	pr_info("[omc_wif_d2_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);
	/* tif_cid_debug */
	DbgCmd = 0x00000108;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	pr_info("[omc_wif_d2_debug]tif_cid_debug(0x%X)\n", DbgData);

	/* traw_wif_d3_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000205;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[traw_wif_d3_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000206;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) >> 16;
	pr_info("[traw_wif_d3_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000207;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) >> 16;
	pr_info("[traw_wif_d3_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);
	/* tif_cid_debug */
	DbgCmd = 0x00000208;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	pr_info("[traw_wif_d3_debug]tif_cid_debug(0x%X)\n", DbgData);

	/* mcrp_d1_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000405;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[mcrp_d1_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000406;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) >> 16;
	pr_info("[mcrp_d1_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000407;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) >> 16;
	pr_info("[mcrp_d1_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);

	/* mcrp_d2_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000305;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[mcrp_d2_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000306;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) >> 16;
	pr_info("[mcrp_d2_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000307;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) >> 16;
	pr_info("[mcrp_d2_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);

	/* r2b_d3_debug */
	pr_info("[r2b_d3_debug]\n");
	DbgCmd = 0x00020401;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgCmd = 0x00030401;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);

}

static void imgsys_dip_dump_nr3d(struct mtk_imgsys_dev *a_pDev,
				 void __iomem *a_pRegBA)
{
	unsigned int DdbSel = DIP_NR3D_DBG_SEL;
	unsigned int DbgCnt = DIP_NR3D_DBG_CNT;
	unsigned int DbgSt = DIP_NR3D_DBG_ST;
	unsigned int Idx = 0;

	pr_info("dump nr3d debug\n");

	/* Dump NR3D Debug Info */
	for (Idx = 0; Idx < DIP_NR3D_DBG_POINTS; Idx++)
		ExeDbgCmdNr3d(a_pDev, a_pRegBA, DdbSel, DbgCnt, DbgSt, Idx);

}

static void imgsys_dip_dump_snrsd1(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump snrs_d1 debug\n");

	/* snrs_d1_dbg_sel debug */
	a_DdbSel = DIP_SNRS_DBG_SEL;
	a_DbgOut = DIP_SNRS_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 0; Idx <= 11; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_yufdd1(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump yufd_d1 debug\n");

	/* yufd_d1_dbg_sel debug */
	a_DdbSel = DIP_YUFD_DBG_SEL;
	a_DbgOut = DIP_YUFD_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 1; Idx <= 41; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_yufed2(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump yufe_d2 debug\n");

	/* yufe_d2_dbg_sel debug */
	a_DdbSel = DIP_YUFE2_DBG_SEL;
	a_DbgOut = DIP_YUFE2_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 1; Idx <= 31; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_yufed3(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump yufe_d3 debug\n");

	/* yufe_d3_dbg_sel debug */
	a_DdbSel = DIP_YUFE3_DBG_SEL;
	a_DbgOut = DIP_YUFE3_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 1; Idx <= 31; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_urz6t4t1d1(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump urz6t4t1_d1 debug\n");

	/* urz6t4t1_d1_dbg_sel debug */
	a_DdbSel = DIP1_URZ6T4T1_DBG_SEL;
	a_DbgOut = DIP1_URZ6T4T1_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 0; Idx <= 15; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_eecnr(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump eecnr_d1 debug\n");

	/* eecnr_d1_dbg_sel debug */
	a_DdbSel = DIP1_EECNR_DBG_SEL;
	a_DbgOut = DIP1_EECNR_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 0; Idx <= 12; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_ans(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump ans_d1 debug\n");

	/* ans_d1_dbg_sel debug */
	a_DdbSel = DIP1_ANS_DBG_SEL;
	a_DbgOut = DIP1_ANS_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 0; Idx <= 9; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_csmc(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump csmc_d1 debug\n");

	/* csmc_d1_dbg_sel debug */
	a_DdbSel = DIP1_CSMC_DBG_SEL;
	a_DbgOut = DIP1_CSMC_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 0; Idx <= 17; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_dwg(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump dwg_d1 debug\n");

	/* dwg_d1_dbg_sel debug */
	a_DdbSel = DIP1_DWG_DBG_SEL;
	a_DbgOut = DIP1_DWG_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 0; Idx <= 4; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_rac(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump rac_d1 debug\n");

	/* rac_d1_dbg_sel debug */
	a_DdbSel = DIP1_RAC_DBG_SEL;
	a_DbgOut = DIP1_RAC_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 0; Idx <= 7; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_csmcsd2(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump csmcs_d2 debug\n");

	/* csmcs_d2_dbg_sel debug */
	a_DdbSel = DIP1_CSMCS2_DBG_SEL;
	a_DbgOut = DIP1_CSMCS2_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 0; Idx <= 10; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_tnc(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump tnc_d1 debug\n");

	/* tnc_d1_dbg_sel debug */
	a_DdbSel = DIP1_TNC_DBG_SEL;
	a_DbgOut = DIP1_TNC_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 0; Idx <= 15; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_snr(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump snr_d1 debug\n");

	/* snr_d1_dbg_sel debug */
	a_DdbSel = DIP2_SNR_DBG_SEL;
	a_DbgOut = DIP2_SNR_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 0; Idx <= 30; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_csmcsd1(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump csmcs_d1 debug\n");

	/* csmcs_d1_dbg_sel debug */
	a_DdbSel = DIP2_CSMCS1_DBG_SEL;
	a_DbgOut = DIP2_CSMCS1_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 0; Idx <= 10; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_yufed1(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump yufe_d1 debug\n");

	/* yufe_d1_dbg_sel debug */
	a_DdbSel = DIP2_YUFE1_DBG_SEL;
	a_DbgOut = DIP2_YUFE1_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 1; Idx <= 31; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_vdce(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump vdce_d1 debug\n");

	/* vdce_d1_dbg_sel debug */
	a_DdbSel = DIP2_VDCE_DBG_SEL;
	a_DbgOut = DIP2_VDCE_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 0; Idx <= 13; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_bok(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump bok_d1 debug\n");

	/* bok_d1_dbg_sel debug */
	a_DdbSel = DIP3_BOK_DBG_SEL;
	a_DbgOut = DIP3_BOK_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 1; Idx <= 11; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}

static void imgsys_dip_dump_fpnr(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int Idx = 0;

	pr_info("dump fpnr_d1 debug\n");

	/* fpnr_d1_dbg_sel debug */
	a_DdbSel = DIP3_FPNR_DBG_SEL;
	a_DbgOut = DIP3_FPNR_DBG_OUT;
	DbgCmd = 0x0;
	for (Idx = 0; Idx <= 11; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
	for (Idx = 16; Idx <= 24; Idx++) {
		DbgCmd = Idx;
		DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	}
}


void imgsys_dip_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine)
{
	void __iomem *dipRegBA = 0L;
	unsigned int i, j, k;
	unsigned int DMADdbSel = DIP_DMATOP_DBG_SEL;
	unsigned int DMADbgOut = DIP_DMATOP_DBG_PORT;
	unsigned int CtlDdbSel = DIP_DBG_SEL;
	unsigned int CtlDbgOut = DIP_DBG_OUT;
	char DMANrPort = 0;
	unsigned int CineSel = 0;
	unsigned int Nr2DmaErr = 0;

	pr_info("%s: +\n", __func__);

	/* 0x34190000~ */
	dipRegBA = gdipRegBA[0];

	/* DL debug data */
	imgsys_dip_dump_dl(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);

	/* top reg */
	for (j = 0; j < sizeof(g_DIPRegDumpTopIfo)/sizeof(struct DIPRegDumpInfo); j++) {
		k = g_DIPRegDumpTopIfo[j].oft & 0xFFF0;
		for (i = k; i <= g_DIPRegDumpTopIfo[j].end; i += 0x10) {
			pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
				(unsigned int)(g_RegBaseAddrTop + i),
				(unsigned int)ioread32((void *)(dipRegBA + i)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));

			ssize_t written = mtk_img_kernel_write(imgsys_dev->scp_pdev,
				"[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X\n",
				(unsigned int)(g_RegBaseAddrTop + i),
				(unsigned int)ioread32((void *)(dipRegBA + i)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));

			if(written < 0)
				pr_debug("Failed to write DIP register values to AEE buffer\n");
		}
	}

	/* 0x341A0000~ */
	dipRegBA = gdipRegBA[1];

	/* nr1 reg */
	for (j = 0; j < sizeof(g_DIPRegDumpNr1Ifo)/sizeof(struct DIPRegDumpInfo); j++) {
		k = g_DIPRegDumpNr1Ifo[j].oft & 0xFFF0;
		for (i = k; i <= g_DIPRegDumpNr1Ifo[j].end; i += 0x10) {
			pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
				(unsigned int)(g_RegBaseAddrNr1 + i),
				(unsigned int)ioread32((void *)(dipRegBA + i)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));

			ssize_t written = mtk_img_kernel_write(imgsys_dev->scp_pdev,
				"[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X\n",
				(unsigned int)(g_RegBaseAddrNr1 + i),
				(unsigned int)ioread32((void *)(dipRegBA + i)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));

			if(written < 0)
				pr_debug("Failed to write DIP register values to AEE buffer\n");
		}
	}

	/* 0x341B0000~ */
	dipRegBA = gdipRegBA[2];

	/* nr2 reg */
	for (j = 0; j < sizeof(g_DIPRegDumpNr2Ifo)/sizeof(struct DIPRegDumpInfo); j++) {
		k = g_DIPRegDumpNr2Ifo[j].oft & 0xFFF0;
		for (i = k; i <= g_DIPRegDumpNr2Ifo[j].end; i += 0x10) {
			pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
				(unsigned int)(g_RegBaseAddrNr2 + i),
				(unsigned int)ioread32((void *)(dipRegBA + i)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));

			ssize_t written = mtk_img_kernel_write(imgsys_dev->scp_pdev,
				"[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X\n",
				(unsigned int)(g_RegBaseAddrNr2 + i),
				(unsigned int)ioread32((void *)(dipRegBA + i)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
				(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));

			if(written < 0)
				pr_debug("Failed to write DIP register values to AEE buffer\n");
		}
	}
	/* cine_sel check */
	CineSel = (((unsigned int)ioread32((void *)(dipRegBA + DIPNR2_CINE_SEL))
			& (1 << 24)) ? 1 : 0);

	if (CineSel == 1) {
		/* 0x341D0000~ */
		dipRegBA = gdipRegBA[3];

		/* cine reg */
		for (j = 0; j < sizeof(g_DIPRegDumpCineIfo)/sizeof(struct DIPRegDumpInfo); j++) {
			k = g_DIPRegDumpCineIfo[j].oft & 0xFFF0;
			for (i = k; i <= g_DIPRegDumpCineIfo[j].end; i += 0x10) {
				pr_info("[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
					(unsigned int)(g_RegBaseAddrCine + i),
					(unsigned int)ioread32((void *)(dipRegBA + i)),
					(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
					(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
					(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
			}
		}
	}

	/* img4o 4bo dma err check */
	Nr2DmaErr = (((unsigned int)ioread32((void *)(dipRegBA + DIPNR2_DMA_ERR))
			& 0x30000) ? 1 : 0);

	/* DMA_TOP debug data */
	DMANrPort = 0;
	dipRegBA = gdipRegBA[0];
	DMADdbSel = DIP_DMATOP_DBG_SEL;
	DMADbgOut = DIP_DMATOP_DBG_PORT;
	g_RegBaseAddr = g_RegBaseAddrTop;
	imgsys_dip_dump_dma(imgsys_dev, dipRegBA, DMADdbSel, DMADbgOut, DMANrPort);
	/* DMA_NR debug data */
	DMANrPort = 1;
	dipRegBA = gdipRegBA[2];
	DMADdbSel = DIP_DMANR2_DBG_SEL;
	DMADbgOut = DIP_DMANR2_DBG_PORT;
	g_RegBaseAddr = g_RegBaseAddrNr2;
	imgsys_dip_dump_dma(imgsys_dev, dipRegBA, DMADdbSel, DMADbgOut, DMANrPort);

	dipRegBA = gdipRegBA[0];
	g_RegBaseAddr = g_RegBaseAddrTop;
	/* NR3D debug data */
	imgsys_dip_dump_nr3d(imgsys_dev, dipRegBA);
	/* SNRS_D1 debug data */
	imgsys_dip_dump_snrsd1(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
	/* YUFD_D1 debug data */
	imgsys_dip_dump_yufdd1(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
	/* YUFE_D2 debug data */
	imgsys_dip_dump_yufed2(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
	/* YUFE_D3 debug data */
	imgsys_dip_dump_yufed3(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
	/* YUFE_D3 debug data */
	imgsys_dip_dump_yufed3(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);

	dipRegBA = gdipRegBA[1];
	g_RegBaseAddr = g_RegBaseAddrNr1;
	/* URZ6T4T_D1 debug data */
	imgsys_dip_dump_urz6t4t1d1(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
	/* EECNR debug data */
	imgsys_dip_dump_eecnr(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
	/* ANS debug data */
	imgsys_dip_dump_ans(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
	/* CSMC debug data */
	imgsys_dip_dump_csmc(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
	/* DWG debug data */
	imgsys_dip_dump_dwg(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
	/* RAC debug data */
	imgsys_dip_dump_rac(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
	/* CSMCS_D2 debug data */
	imgsys_dip_dump_csmcsd2(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
	/* TNC debug data */
	imgsys_dip_dump_tnc(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);

	dipRegBA = gdipRegBA[2];
	g_RegBaseAddr = g_RegBaseAddrNr2;
	/* SNR debug data */
	imgsys_dip_dump_snr(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
	/* CSMCS_D1 debug data */
	imgsys_dip_dump_csmcsd1(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
	/* YUFE_D1 debug data */
	imgsys_dip_dump_yufed1(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
	/* VDCE_D1 debug data */
	imgsys_dip_dump_vdce(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);

	if (CineSel == 1) {
		dipRegBA = gdipRegBA[3];
		g_RegBaseAddr = g_RegBaseAddrCine;
		/* BOK_D1 debug data */
		imgsys_dip_dump_bok(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
		/* FPNR_D1 debug data */
		imgsys_dip_dump_fpnr(imgsys_dev, dipRegBA, CtlDdbSel, CtlDbgOut);
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

	// force KE
	if (Nr2DmaErr == 1) {
		pr_info("%s: Nr2DmaErr force KE\n", __func__);
		BUG_ON(1);
	}

	pr_info("%s: -\n", __func__);

}

void imgsys_dip_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int i;

	for (i = 0; i < DIP_HW_SET; i++) {
		iounmap(gdipRegBA[i]);
		gdipRegBA[i] = 0L;
	}
	iounmap(gVcoreRegBA);
	gVcoreRegBA = 0L;
}

bool imgsys_dip_done_chk(struct mtk_imgsys_dev *imgsys_dev, uint32_t engine)
{
	bool ret = true; //true: done
	uint32_t value = 0;
	void __iomem *dipRegBA = 0L;

	dipRegBA = gdipRegBA[0];
	value = (uint32_t)ioread32((void *)(dipRegBA + 0xC8));

	if (!(value & 0x1)) {
		ret = false;
		pr_info(
		"%s: hw_comb:0x%x, polling DIP done fail!!! [0x%08x] 0x%x",
		__func__, engine,
		(uint32_t)(DIP_TOP_ADDR + 0xC8), value);
	}

	return ret;
}
