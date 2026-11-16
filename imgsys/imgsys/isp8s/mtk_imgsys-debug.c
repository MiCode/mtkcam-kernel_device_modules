// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Christopher Chen <christopher.chen@mediatek.com>
 *
 */

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include "mtk_imgsys-engine-isp8s.h"
#include "mtk_imgsys-debug.h"
#include "mtk-mminfra-debug.h"
/* TODO */
#include "smi.h"

// GCE header
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/delay.h>

#define DL_CHECK_ENG_NUM IMGSYS_HW_NUM_MAX
#define WPE_HW_SET    3
#define ADL_HW_SET    2
#define SW_RST   (0x000C)
#define DBG_SW_CLR   (0x0260)

#define HAVE_IMGSYS_HW_FLAG_ADL_A   0
#define HAVE_IMGSYS_HW_FLAG_ADL_B   0
#define HAVE_IMGSYS_HW_FLAG_XTR     0

#define LOG_LEGNTH (64)
#define FINAL_LOG_LENGTH (LOG_LEGNTH * 4)
#define DDREN_ACK_TIMEOUT_CNT 1000000

const unsigned int g_imgsys_main_reg_base      = (0x34010000);
const unsigned int g_imgsys_dip_top_reg_base   = (0x34100000);
const unsigned int g_imgsys_dip_nr_reg_base    = (0x34120000);
const unsigned int g_imgsys_wpe1_dip1_reg_base = (0x34200000);
const unsigned int g_imgsys_wpe2_dip1_reg_base = (0x34500000);
const unsigned int g_imgsys_wpe3_dip1_reg_base = (0x34600000);
const unsigned int g_imgsys_traw_dip1_reg_base = (0x34700000);

#define AW_MMQOS_EN_MASK       GENMASK(28, 28)
#define AW_MMQOS_NORMAL_MASK   GENMASK(27, 24)
#define AW_MMQOS_PREULTRA_MASK GENMASK(23, 20)
#define AW_MMQOS_ULTRA_MASK    GENMASK(19, 16)

#define AR_MMQOS_EN_MASK       GENMASK(12, 12)
#define AR_MMQOS_NORMAL_MASK   GENMASK(11, 8)
#define AR_MMQOS_PREULTRA_MASK GENMASK(7, 4)
#define AR_MMQOS_ULTRA_MASK    GENMASK(3, 0)

#define IMG_VCORE_MMQOS_CTRL_0_OFT          0x100
#define IMG_VCORE_MMQOS_CTRL_1_OFT          0x104
#define IMG_VCORE_MMQOS_CTRL_2_OFT          0x108

static uint32_t mmqos_remap_enable_r = 1;
static uint32_t mmqos_remap_normal_r = 1;
static uint32_t mmqos_remap_preultra_r = 9;
static uint32_t mmqos_remap_enable_w = 1;
static uint32_t mmqos_remap_normal_w;
static uint32_t mmqos_remap_preultra_w = 9;

bool imgsys_dip_8s_dbg_enable(void)
{
	return imgsys_dip_dbg_en;
}

bool imgsys_traw_8s_dbg_enable(void)
{
	return imgsys_traw_dbg_en;
}

bool imgsys_wpe_8s_dbg_enable(void)
{
	return imgsys_wpe_dbg_en;
}

bool imgsys_omc_8s_dbg_enable(void)
{
	return imgsys_omc_dbg_en;
}

bool imgsys_pqdip_8s_dbg_enable(void)
{
	return imgsys_pqdip_dbg_en;
}

bool imgsys_me_8s_dbg_enable(void)
{
	return imgsys_me_dbg_en;
}

bool imgsys_mae_8s_dbg_enable(void)
{
	return imgsys_mae_dbg_en;
}

bool imgsys_dfp_8s_dbg_enable(void)
{
	return imgsys_dfp_dbg_en;
}

bool imgsys_dpe_8s_dbg_enable(void)
{
	return imgsys_dpe_dbg_en;
}

int imgsys_isc_8s_ctrl(void)
{
	pr_info("%s: %08X", __func__, imgsys_isc_ctrl);
	return imgsys_isc_ctrl;
}

bool imgsys_isc_8s_dbg_log_en(void)
{
	return imgsys_isc_log_en;
}

/* Should follow the order of IMGSYS_HW_FLAG_xxx in enum_imgsys_engine */
struct imgsys_dbg_engine_t dbg_engine_name_list[DL_CHECK_ENG_NUM] = {
	{IMGSYS_HW_FLAG_WPE_EIS,  "WPE_EIS"},
	{IMGSYS_HW_FLAG_WPE_TNR,  "WPE_TNR"},
	{IMGSYS_HW_FLAG_WPE_LITE, "WPE_LITE"},
	{IMGSYS_HW_FLAG_OMC_TNR,  "OMC_TNR"},
	{IMGSYS_HW_FLAG_OMC_LITE, "OMC_LITE"},
	{IMGSYS_HW_FLAG_ADL_A,    "ADL_A"},
	{IMGSYS_HW_FLAG_ADL_B,    "ADL_A"},
	{IMGSYS_HW_FLAG_TRAW,     "TRAW"},
	{IMGSYS_HW_FLAG_LTR,      "LTRAW"},
	{IMGSYS_HW_FLAG_XTR,      "XTRAW"},
	{IMGSYS_HW_FLAG_DIP,      "DIP"},
	{IMGSYS_HW_FLAG_PQDIP_A,  "PQDIP_A"},
	{IMGSYS_HW_FLAG_PQDIP_B,  "PQDIP_B"},
	{IMGSYS_HW_FLAG_ME,       "ME"},
	{IMGSYS_HW_FLAG_MAE,      "MAE"},
	{IMGSYS_HW_FLAG_DFP,      "DFP"},
	{IMGSYS_HW_FLAG_DPE,      "DPE"},
};

void __iomem *imgsysmainRegBA;
void __iomem *wpedip1RegBA;
void __iomem *wpedip2RegBA;
void __iomem *wpedip3RegBA;
void __iomem *dipRegBA;
void __iomem *dip1RegBA;
void __iomem *dip2RegBA;
void __iomem *trawRegBA;
void __iomem *adlARegBA;
void __iomem *adlBRegBA;
void __iomem *imgsysVcoreRegBA;
void __iomem *imgsysiscRegBA;
void __iomem *wpeeispqdipaRegBA;
void __iomem *wpetnrpqdipbRegBA;
void __iomem *mainwpe0RegBA;
void __iomem *mainwpe1RegBA;
void __iomem *mainwpe2RegBA;
void __iomem *mainomcliteRegBA;
int imgsys_ddr_en;

void imgsys_mmqos_remap_get_golden(uint32_t *value)
{
	*value |= FIELD_PREP(AR_MMQOS_EN_MASK, mmqos_remap_enable_r) |
			FIELD_PREP(AR_MMQOS_NORMAL_MASK, mmqos_remap_normal_r) |
			FIELD_PREP(AR_MMQOS_PREULTRA_MASK, mmqos_remap_preultra_r) |
			FIELD_PREP(AR_MMQOS_ULTRA_MASK, mmqos_remap_preultra_r);
	*value |= FIELD_PREP(AW_MMQOS_EN_MASK, mmqos_remap_enable_w) |
			FIELD_PREP(AW_MMQOS_NORMAL_MASK, mmqos_remap_normal_w) |
			FIELD_PREP(AW_MMQOS_PREULTRA_MASK, mmqos_remap_preultra_w) |
			FIELD_PREP(AW_MMQOS_ULTRA_MASK, mmqos_remap_preultra_w);
}

int imgsys_mmqos_remap_get(char *buf, const struct kernel_param *kp)
{
	uint32_t value = 0;
	void __iomem *qosRemapRegBA = imgsysVcoreRegBA;

	if  (!qosRemapRegBA)
		return snprintf(buf, PAGE_SIZE, "imgsys pipe had been turned off\n");

	imgsys_mmqos_remap_get_golden(&value);

	return snprintf(buf, PAGE_SIZE,
			"current: 0x%08X 0x%08X 0x%08X\n"
			"expected: 0x%08X\n",
			ioread32((void *)(qosRemapRegBA + IMG_VCORE_MMQOS_CTRL_0_OFT)),
			ioread32((void *)(qosRemapRegBA + IMG_VCORE_MMQOS_CTRL_1_OFT)),
			ioread32((void *)(qosRemapRegBA + IMG_VCORE_MMQOS_CTRL_2_OFT)),
			value);
}

int imgsys_mmqos_remap_set(const char *val, const struct kernel_param *kp)
{
	u32 result;

	result = sscanf(val, "%u %u %u %u %u %u",
		&mmqos_remap_enable_r, &mmqos_remap_normal_r, &mmqos_remap_preultra_r,
		&mmqos_remap_enable_w, &mmqos_remap_normal_w, &mmqos_remap_preultra_w);
	if (result != 6) {
		pr_notice("imgsys mmqos remap set fail\n");
		return result;
	}

	return 0;
}

static const struct kernel_param_ops imgsys_mmqos_remap_ops = {
	.get = imgsys_mmqos_remap_get,
	.set = imgsys_mmqos_remap_set,
};
module_param_cb(imgsys_mmqos_remap, &imgsys_mmqos_remap_ops, NULL, 0644);
MODULE_PARM_DESC(imgsys_mmqos_remap, "modify imgsys mmqos remap setting");

void imgsys_main_init(struct mtk_imgsys_dev *imgsys_dev)
{
	struct resource adl;
	int ddr_en = 0;

	imgsys_ddr_en = 0;

	pr_info("%s: +.\n", __func__);

	imgsysmainRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_TOP);
	if (!imgsysmainRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap imgsys_top registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	wpedip1RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE1_DIP1);
	if (!wpedip1RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip1 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	wpedip2RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE2_DIP1);
	if (!wpedip2RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip2 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	wpedip3RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE3_DIP1);
	if (!wpedip3RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip3 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	dipRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_DIP_TOP);
	if (!dipRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap dip_top registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	dip1RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_DIP_TOP_NR);
	if (!dip1RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap dip_top_nr registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	dip2RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_DIP_TOP_NR2);
	if (!dip2RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap dip_top_nr2 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}


	trawRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_TRAW_TOP);
	if (!trawRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap traw_dip1 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	adl.start = 0;
	if (of_address_to_resource(imgsys_dev->dev->of_node, REG_MAP_E_ADL_A, &adl)) {
		dev_info(imgsys_dev->dev, "%s: of_address_to_resource fail\n", __func__);
		return;
	}
	if (adl.start) {
		adlARegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ADL_A);
		if (!adlARegBA) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap adl a registers\n",
									__func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
					__func__, imgsys_dev->dev->of_node->name);
			return;
		}

		adlBRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ADL_B);
		if (!adlBRegBA) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap adl b registers\n",
									__func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
					__func__, imgsys_dev->dev->of_node->name);
			return;
		}
	} else {
		adlARegBA = NULL;
		adlBRegBA = NULL;
		dev_info(imgsys_dev->dev, "%s Do not have ADL hardware.\n", __func__);
	}

	if (of_property_read_u32(imgsys_dev->dev->of_node,
		"mediatek,imgsys-ddr-en", &ddr_en) != 0) {
		dev_info(imgsys_dev->dev, "ddr_en is not exist\n");
	} else {
		imgsys_ddr_en = ddr_en;
		dev_info(imgsys_dev->dev, "ddr_en(%d/%d)\n", ddr_en, imgsys_ddr_en);
	}

	imgsysVcoreRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_IMG_VCORE);
	if (!imgsysVcoreRegBA) {
		dev_info(imgsys_dev->dev, "%s: Unable to ioremap img_vcore registers\n",
				__func__);
		dev_info(imgsys_dev->dev, "%s: of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	imgsysiscRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ISC);
	if (!imgsysiscRegBA) {
		dev_info(imgsys_dev->dev, "%s: Unable to ioremap isc registers\n",
				__func__);
		dev_info(imgsys_dev->dev, "%s: of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	wpeeispqdipaRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE_EIS_PQDIP_A);
	if (!wpeeispqdipaRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_eis_pqdip_a registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	wpetnrpqdipbRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE_TNR_PQDIP_B);
	if (!wpetnrpqdipbRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_tnr_pqdip_b registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	mainwpe0RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_MAIN_WPE0);
	if (!mainwpe0RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap main_wpe0 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	mainwpe1RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_MAIN_WPE1);
	if (!mainwpe1RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap main_wpe1 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	mainwpe2RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_MAIN_WPE2);
	if (!mainwpe2RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap main_wpe2 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	mainomcliteRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_MAIN_OMC_LITE);
	if (!mainomcliteRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap main_omc_lite registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	pr_info("%s: -.\n", __func__);
}

void imgsys_mmqos_remap(void)
{
	uint32_t value = 0;
	void __iomem *qosRemapRegBA = imgsysVcoreRegBA;

	if  (!qosRemapRegBA) {
		pr_err("[%s][%d] qosRemapRegBA is 0", __func__, __LINE__);
		return;
	}

	imgsys_mmqos_remap_get_golden(&value);

	iowrite32(value, (qosRemapRegBA + IMG_VCORE_MMQOS_CTRL_0_OFT));
	iowrite32(value, (qosRemapRegBA + IMG_VCORE_MMQOS_CTRL_1_OFT));
	iowrite32(value, (qosRemapRegBA + IMG_VCORE_MMQOS_CTRL_2_OFT));
}

#define ISC_BASE	(0x34060000)
#define ISC_CTRL	(0x00000004)
#define ISC_ERR_ID	(0x0000001C)
#define ISC_INT_EN	(0x00000010)
#define ISC_INT_STATUS	(0x00000014)
#define ISC_INT_STATUSX	(0x00000018)
#define GID_0_ENTRY	(0x100)
#define GID_ENTRY_OFST	(0x010)
#define GID_TBL(g)	(GID_0_ENTRY + GID_ENTRY_OFST * (g))
#define GID_RG_VAL(g)	((g) << 1)
#define GID_START	(46)
#define GID_END		(65)
#define GID_NUM		(GID_END - GID_START + 1)

void imgsys_main_slc_init(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int i = 0, gid8 = 0, bid = 2, entry, val = 0, opt;
	void *addr = 0, *addr1 = 0;

	if (!imgsysiscRegBA)
		return;


	for (i = 0; i < GID_NUM; i++) {
		gid8 = (i + GID_START) << 1;
		entry = i * bid;
		addr = (void *)(imgsysiscRegBA + GID_TBL(entry));
		iowrite32(GID_RG_VAL(gid8), addr);

		gid8 = gid8 + 1;
		entry++;
		addr1 = (void *)(imgsysiscRegBA + GID_TBL(entry));
		iowrite32(GID_RG_VAL(gid8), addr1);
		if (imgsys_isc_8s_dbg_log_en())
			dev_info(imgsys_dev->dev, "%s: ISC GID(%08X)| %08X %08X\n", __func__,
				gid8 >> 1, (unsigned int)ioread32(addr), (unsigned int)ioread32(addr1));
	}

	addr = (void *)(imgsysiscRegBA + ISC_CTRL);
	opt = imgsys_isc_8s_ctrl();
	if (opt & 0x2)
		val |= 0x2;
	if (opt & 0x4)
		val |= 0x4;
	if (opt & 0x1)
		val |= 0x1;
	if (opt & 0x10000000)
		val |= 0x10000000;
	iowrite32(val, addr);
	dev_info(imgsys_dev->dev, "%s: ISC_CTRL [0x%08X 0x%08X]\n", __func__,
					(ISC_BASE + ISC_CTRL), (unsigned int)ioread32(addr));

	val = 0x21;
	addr = (void *)(imgsysiscRegBA + ISC_INT_EN);
	iowrite32(val, addr);
}

#define BID_NUM	(2)
char log_buf[LOG_LEGNTH * 20] = {0};
struct isc_debug {
	unsigned int gid;
	unsigned int bid;
	unsigned int gid_tbl[4];
	unsigned int int_status;
	unsigned int int_statusx;
} isc_info;
static int primary_irq;

void imgsys_main_slc_dump(struct mtk_imgsys_dev *imgsys_dev, unsigned int irq)
{
	unsigned int i = 0, gid = 0, gid9, entry, b, bid;
	void *addr0 = 0;
	int ret, ofst = 0;
	unsigned int regs_ofst[] = {ISC_CTRL, 0x10, 0x14, 0x18, 0x1C, 0x28, 0x30, 0x34, 0x38};


	if (!imgsysiscRegBA) {
		dev_info(imgsys_dev->dev, "%s: isc already unmapped\n", __func__);
		return;
	}

	log_buf[strlen(log_buf)] = '\0';

	if (irq == 1) {

		addr0 = (void *)(imgsysiscRegBA + ISC_INT_STATUS);
		isc_info.int_status = (unsigned int)ioread32(addr0);
		addr0 = (void *)(imgsysiscRegBA + ISC_INT_STATUSX);
		isc_info.int_statusx = (unsigned int)ioread32(addr0);

		addr0 = (void *)(imgsysiscRegBA + ISC_ERR_ID);
		gid9 = (unsigned int)ioread32(addr0);
		gid = gid9 >> 2;
		bid = gid9 & (0x3);
		isc_info.gid = gid;
		isc_info.bid = bid;

		if ((gid >= GID_START) && (gid <= GID_END)) {
			entry = (gid - GID_START) + (bid >> 1);
			addr0 = (void *)(imgsysiscRegBA + GID_TBL(entry));
			isc_info.gid_tbl[0] = (unsigned int)ioread32(addr0);
			isc_info.gid_tbl[1] = (unsigned int)ioread32(addr0 + 0x4);
			isc_info.gid_tbl[2] = (unsigned int)ioread32(addr0 + 0x8);
			isc_info.gid_tbl[3] = (unsigned int)ioread32(addr0 + 0xC);
		}

		primary_irq = 1;

		return;
	}

	if ((irq == 2) && primary_irq) {
		primary_irq = 0;

		if (isc_info.int_statusx & 0x20)
			dev_info(imgsys_dev->dev, "ISC: unknown GID(%d,%d)\n",
								isc_info.gid, isc_info.bid);
		else if (isc_info.int_statusx & 0x1)
			dev_info(imgsys_dev->dev, "ISC: SFD underflow GID(%d,%d) | %08X %08X %08X %08X\n",
								isc_info.gid, isc_info.bid,
								isc_info.gid_tbl[0],
								isc_info.gid_tbl[1],
								isc_info.gid_tbl[2],
								isc_info.gid_tbl[3]);
		dev_info(imgsys_dev->dev, "ISC status(%08X, %08X)\n", isc_info.int_status, isc_info.int_statusx);

		return;
	}

	for (i = 0; i < ARRAY_SIZE(regs_ofst); i++) {
		addr0 = (void *)(imgsysiscRegBA + regs_ofst[i]);
		ret = snprintf(log_buf + ofst, sizeof(log_buf) - ofst, "ISC 0x%08X| %08X\n",
						(ISC_BASE + regs_ofst[i]), (unsigned int)ioread32(addr0));
		if (ret > 0 && ret < sizeof(log_buf) - ofst)
			ofst += ret;
		else if (ret > sizeof(log_buf) - ofst)
			dev_info(imgsys_dev->dev, "%s: string truncated\n", __func__);
	}
	dev_info(imgsys_dev->dev, "%s:\n%s", __func__, log_buf);

	for (i = 0 ; i < GID_NUM; i++) {
		gid = i + GID_START;
		for (b = 0; b < BID_NUM; b++) {
			entry = i * BID_NUM + b;
			addr0 = (void *)(imgsysiscRegBA + GID_TBL(entry));
			dev_info(imgsys_dev->dev, "ISC GID(%d,%d) 0x%08X| %08X %08X %08X %08X\n",
							gid, b << 1,
							(ISC_BASE + GID_TBL(entry)),
							(unsigned int)ioread32(addr0),
							(unsigned int)ioread32(addr0 + 0x4),
							(unsigned int)ioread32(addr0 + 0x8),
							(unsigned int)ioread32(addr0 + 0xC));
		}
	}
}

void imgsys_main_set_init(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *WpeRegBA = 0L;
	void __iomem *ADLRegBA = 0L;
	void __iomem *pWpeCtrl = 0L;
	void __iomem *DdrRegBA = 0L;
	unsigned int HwIdx = 0;
	uint32_t count;
	uint32_t value;
#ifndef CONFIG_FPGA_EARLY_PORTING
	int i, num;
#endif

	pr_debug("%s: +.\n", __func__);

	DdrRegBA = imgsysVcoreRegBA;

	/* enable WLA & coh protect */
	iowrite32(0xc, (DdrRegBA + 0xf8));
	value = ioread32((void *)(DdrRegBA + 0xf8));
	pr_debug("DdrRegBA + 0xf8 = 0x%08x\n", value);

	/* WLA 2.0 setting */
	iowrite32(0x80006000, (DdrRegBA + 0x9c));
	value = ioread32((void *)(DdrRegBA + 0x9c));
	pr_debug("DdrRegBA + 0x9c = 0x%08x\n", value);

	/* Force SW ddren for debug */
	value = 0x3fd;
	iowrite32(value, (DdrRegBA + 0x10));
	pr_debug("DdrRegBA + 0x10 = 0x%08x\n", value);

	/* Wait platform resources ack */
	count = 0;
	value = ioread32((void *)(DdrRegBA + 0x14));
	while ((value & 0x1fc) != 0x1fc) {
		count++;
		if (count > DDREN_ACK_TIMEOUT_CNT) {
			pr_err("[%s][%d] wait platorm resources done timeout 0x%08x", __func__, __LINE__, value);
			value = ioread32((void *)(DdrRegBA + 0xC4));
			pr_err("[%s][%d] C4: 0x%08x", __func__, __LINE__, value);
			value = ioread32((void *)(DdrRegBA + 0xC8));
			pr_err("[%s][%d] C8: 0x%08x", __func__, __LINE__, value);
			value = ioread32((void *)(DdrRegBA + 0xCC));
			pr_err("[%s][%d] CC: 0x%08x", __func__, __LINE__, value);
			break;
		}
		udelay(5);
		value = ioread32((void *)(DdrRegBA + 0x14));
	}

	if (imgsys_dev == NULL) {
		dump_stack();
		pr_err("[%s][%d] param fatal error!", __func__, __LINE__);
		return;
	}

	imgsys_mmqos_remap();

#ifndef CONFIG_FPGA_EARLY_PORTING
	num = imgsys_dev->larbs_num - 1;
	for (i = 0; i < num; i++)
		mtk_smi_larb_clamp_and_lock(imgsys_dev->larbs[i], 1);
#endif

	for (HwIdx = 0; HwIdx < WPE_HW_SET; HwIdx++) {
		if (HwIdx == 0)
			WpeRegBA = wpedip1RegBA;
		else if (HwIdx == 1)
			WpeRegBA = wpedip2RegBA;
		else
			WpeRegBA = wpedip3RegBA;

		/* Wpe Macro HW Reset */
		pWpeCtrl = (void *)(WpeRegBA + SW_RST);
		iowrite32(0xF, pWpeCtrl);
		/* Clear HW Reset */
		iowrite32(0x0, pWpeCtrl);
	}

	HwIdx = 0;
	if (adlARegBA || adlBRegBA) {
		/* Reset ADL A */
		for (HwIdx = 0; HwIdx < ADL_HW_SET; HwIdx++) {
			if (HwIdx == 0)
				ADLRegBA = adlARegBA;
			else if (HwIdx == 1)
				ADLRegBA = adlBRegBA;
			if (!ADLRegBA)
				continue;
			value = ioread32((void *)(ADLRegBA + 0x300));
			value |= ((0x1 << 8) | (0x1 << 9));
			iowrite32(value, (ADLRegBA + 0x300));
			count = 0;
			while (count < 1000000) {
				value = ioread32((void *)(ADLRegBA + 0x300));
				if ((value & 0x3) == 0x3)
					break;
				count++;
			}
			value = ioread32((void *)(ADLRegBA + 0x300));
			value &= ~((0x1 << 8) | (0x1 << 9));
			iowrite32(value, (ADLRegBA + 0x300));
		}
	}
	iowrite32(0x1FF, (void *)(imgsysmainRegBA + DBG_SW_CLR));
	iowrite32(0x0, (void *)(imgsysmainRegBA + DBG_SW_CLR));

	iowrite32(0xF0, (void *)(imgsysmainRegBA + SW_RST));
	iowrite32(0x0, (void *)(imgsysmainRegBA + SW_RST));

	iowrite32(0x3C, (void *)(trawRegBA + SW_RST));
	iowrite32(0x0, (void *)(trawRegBA + SW_RST));

	iowrite32(0x3FC03, (void *)(dipRegBA + SW_RST));
	iowrite32(0x0, (void *)(dipRegBA + SW_RST));

#ifndef CONFIG_FPGA_EARLY_PORTING
	for (i = 0; i < num; i++)
		mtk_smi_larb_clamp_and_lock(imgsys_dev->larbs[i], 0);
#endif

#ifdef MTK_ISC_SUPPORT
	imgsys_main_slc_init(imgsys_dev);
	if (imgsys_isc_8s_dbg_log_en())
		imgsys_main_slc_dump(imgsys_dev, 0);
#endif

	pr_debug("%s: -. qof ver = %d\n", __func__, imgsys_dev->qof_ver);
}

void imgsys_main_cmdq_set_init(struct mtk_imgsys_dev *imgsys_dev, void *pkt, int hw_idx)
{
	struct cmdq_pkt *package = NULL;

	if (imgsys_dev == NULL || pkt == NULL) {
		dump_stack();
		pr_err("[%s][%d] param fatal error!", __func__, __LINE__);
		return;
	}
	package = (struct cmdq_pkt *)pkt;

	cmdq_pkt_write(package, NULL, (g_imgsys_main_reg_base + SW_RST) /*address*/,
			       0xF0, 0xffffffff);
	cmdq_pkt_write(package, NULL, (g_imgsys_main_reg_base + SW_RST) /*address*/,
			       0x0, 0xffffffff);

	cmdq_pkt_write(package, NULL, (g_imgsys_main_reg_base + DBG_SW_CLR) /*address*/,
			       0x1FF, 0xffffffff);
	cmdq_pkt_write(package, NULL, (g_imgsys_main_reg_base + DBG_SW_CLR) /*address*/,
			       0x0, 0xffffffff);
}

void imgsys_main_dump(struct mtk_imgsys_dev *imgsys_dev, unsigned int engine)
{
	uint32_t value;
	void __iomem *DdrRegBA = imgsysVcoreRegBA;
	if (imgsys_isc_8s_dbg_log_en())
		pr_info("%s: +\n", __func__);
	imgsys_main_slc_dump(imgsys_dev, engine);
	// dump for debug
	value = ioread32((void *)(DdrRegBA + 0xf8));
	pr_info("%s: DdrRegBA 0xF8 : 0x%08x\n", __func__, value);

	value = ioread32((void *)(DdrRegBA + 0x90));
	pr_info("%s: DdrRegBA 0x90 : 0x%08x\n", __func__, value);

	value = ioread32((void *)(DdrRegBA + 0x94));
	pr_info("%s: DdrRegBA 0x94 : 0x%08x\n", __func__, value);

	value = ioread32((void *)(DdrRegBA + 0x98));
	pr_info("%s: DdrRegBA 0x98 : 0x%08x\n", __func__, value);

	value = ioread32((void *)(DdrRegBA + 0x9C));
	pr_info("%s: DdrRegBA 0x9C : 0x%08x\n", __func__, value);

	value = ioread32((void *)(DdrRegBA + 0xA0));
	pr_info("%s: DdrRegBA 0xA0 : 0x%08x\n", __func__, value);

	value = ioread32((void *)(DdrRegBA + 0x10));
	pr_info("%s: DdrRegBA 0x10 : 0x%08x\n", __func__, value);
	//
	mtk_mmpc_resource_dump();
	if (imgsys_isc_8s_dbg_log_en())
		pr_info("%s: -\n", __func__);
}

void imgsys_main_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	pr_debug("%s: +.\n", __func__);

	if (imgsysmainRegBA) {
		iounmap(imgsysmainRegBA);
		imgsysmainRegBA = 0L;
	}

	if (wpedip1RegBA) {
		iounmap(wpedip1RegBA);
		wpedip1RegBA = 0L;
	}

	if (wpedip2RegBA) {
		iounmap(wpedip2RegBA);
		wpedip2RegBA = 0L;
	}

	if (wpedip3RegBA) {
		iounmap(wpedip3RegBA);
		wpedip3RegBA = 0L;
	}

	if (dipRegBA) {
		iounmap(dipRegBA);
		dipRegBA = 0L;
	}

	if (dip1RegBA) {
		iounmap(dip1RegBA);
		dip1RegBA = 0L;
	}

	if (dip2RegBA) {
		iounmap(dip2RegBA);
		dip2RegBA = 0L;
	}

	if (trawRegBA) {
		iounmap(trawRegBA);
		trawRegBA = 0L;
	}

	if (adlARegBA) {
		iounmap(adlARegBA);
		adlARegBA = 0L;
	}

	if (adlBRegBA) {
		iounmap(adlBRegBA);
		adlBRegBA = 0L;
	}

	if (imgsysVcoreRegBA) {
		iounmap(imgsysVcoreRegBA);
		imgsysVcoreRegBA = 0L;
	}

	if (imgsysiscRegBA) {
		iounmap(imgsysiscRegBA);
		imgsysiscRegBA = 0L;
	}

	if (wpeeispqdipaRegBA) {
		iounmap(wpeeispqdipaRegBA);
		wpeeispqdipaRegBA = 0L;
	}

	if (wpetnrpqdipbRegBA) {
		iounmap(wpetnrpqdipbRegBA);
		wpetnrpqdipbRegBA = 0L;
	}

	if (mainwpe0RegBA) {
		iounmap(mainwpe0RegBA);
		mainwpe0RegBA = 0L;
	}

	if (mainwpe1RegBA) {
		iounmap(mainwpe1RegBA);
		mainwpe1RegBA = 0L;
	}

	if (mainwpe2RegBA) {
		iounmap(mainwpe2RegBA);
		mainwpe2RegBA = 0L;
	}

	if (mainomcliteRegBA) {
		iounmap(mainomcliteRegBA);
		mainomcliteRegBA = 0L;
	}

	imgsys_ddr_en = 0;
	pr_debug("%s: -.\n", __func__);
}

void imgsys_debug_dump_routine(struct mtk_imgsys_dev *imgsys_dev,
	const struct module_ops *imgsys_modules,
	int imgsys_module_num, unsigned int hw_comb)
{
	bool module_on[IMGSYS_MOD_MAX] = {
		false, false, false, false, false, false, false, false, false, false, false, false};
	int i = 0;

	dev_info(imgsys_dev->dev,
			"%s: hw comb set: 0x%x\n",
			__func__, hw_comb);

	imgsys_dl_debug_dump(imgsys_dev, hw_comb);

	if ((hw_comb & IMGSYS_HW_FLAG_WPE_EIS) || (hw_comb & IMGSYS_HW_FLAG_WPE_TNR)
		 || (hw_comb & IMGSYS_HW_FLAG_WPE_LITE))
		module_on[IMGSYS_MOD_WPE] = true;
	if ((hw_comb & IMGSYS_HW_FLAG_OMC_TNR) || (hw_comb & IMGSYS_HW_FLAG_OMC_LITE))
		module_on[IMGSYS_MOD_OMC] = true;
	if ((hw_comb & IMGSYS_HW_FLAG_ADL_A) || (hw_comb & IMGSYS_HW_FLAG_ADL_B))
		module_on[IMGSYS_MOD_ADL] = true;
	if ((hw_comb & IMGSYS_HW_FLAG_TRAW) || (hw_comb & IMGSYS_HW_FLAG_LTR)
		 || (hw_comb & IMGSYS_HW_FLAG_XTR))
		module_on[IMGSYS_MOD_TRAW] = true;
	if ((hw_comb & IMGSYS_HW_FLAG_DIP))
		module_on[IMGSYS_MOD_DIP] = true;
	if ((hw_comb & IMGSYS_HW_FLAG_PQDIP_A) || (hw_comb & IMGSYS_HW_FLAG_PQDIP_B))
		module_on[IMGSYS_MOD_PQDIP] = true;
	if ((hw_comb & IMGSYS_HW_FLAG_ME))
		module_on[IMGSYS_MOD_ME] = true;
	if ((hw_comb & IMGSYS_HW_FLAG_MAE))
		module_on[IMGSYS_MOD_MAE] = true;
	if ((hw_comb & IMGSYS_HW_FLAG_DFP))
		module_on[IMGSYS_MOD_DFP] = true;
	if ((hw_comb & IMGSYS_HW_FLAG_DPE))
		module_on[IMGSYS_MOD_DPE] = true;

	/* in case module driver did not set imgsys_modules in module order */
	dev_info(imgsys_dev->dev,
			"%s: imgsys_module_num: %d\n",
			__func__, imgsys_module_num);
	for (i = 0 ; i < imgsys_module_num ; i++) {
		if (module_on[imgsys_modules[i].module_id] && imgsys_modules[i].dump)
			imgsys_modules[i].dump(imgsys_dev, hw_comb);
	}
}

void imgsys_cg_debug_dump(struct mtk_imgsys_dev *imgsys_dev, unsigned int hw_comb)
{
	unsigned int i = 0;

	if (!imgsysmainRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap imgsys_top registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	for (i = 0; i <= 0x500; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(g_imgsys_main_reg_base + i),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + i)),
		(unsigned int)(g_imgsys_main_reg_base + i + 0x4),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + i + 0x4)),
		(unsigned int)(g_imgsys_main_reg_base + i + 0x8),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + i + 0x8)),
		(unsigned int)(g_imgsys_main_reg_base + i + 0xc),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + i + 0xc)));
	}

	if (hw_comb & IMGSYS_HW_FLAG_DIP) {
		if (!dipRegBA) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap dip registers\n",
									__func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
					__func__, imgsys_dev->dev->of_node->name);
			return;
		}

		for (i = 0; i <= 0x100; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			__func__, (unsigned int)(g_imgsys_dip_top_reg_base + i),
			(unsigned int)ioread32((void *)(dipRegBA + i)),
			(unsigned int)(g_imgsys_dip_top_reg_base + i + 0x4),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
			(unsigned int)(g_imgsys_dip_top_reg_base + i + 0x8),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
			(unsigned int)(g_imgsys_dip_top_reg_base + i + 0xc),
			(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
		}

		if (!dip1RegBA) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap dip1 registers\n",
									__func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
					__func__, imgsys_dev->dev->of_node->name);
			return;
		}

		for (i = 0; i <= 0x100; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
			__func__, (unsigned int)(g_imgsys_dip_nr_reg_base + i),
			(unsigned int)ioread32((void *)(dip1RegBA + i)),
			(unsigned int)(g_imgsys_dip_nr_reg_base + i + 0x4),
			(unsigned int)ioread32((void *)(dip1RegBA + i + 0x4)),
			(unsigned int)(g_imgsys_dip_nr_reg_base + i + 0x8),
			(unsigned int)ioread32((void *)(dip1RegBA + i + 0x8)),
			(unsigned int)(g_imgsys_dip_nr_reg_base + i + 0xc),
			(unsigned int)ioread32((void *)(dip1RegBA + i + 0xc)));
		}
	}

	if (hw_comb & IMGSYS_HW_FLAG_WPE_EIS) {
		if (!wpedip1RegBA) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip1 registers\n",
				__func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
			return;
		}

		for (i = 0; i <= 0x100; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
				__func__, (unsigned int)(g_imgsys_wpe1_dip1_reg_base + i),
			(unsigned int)ioread32((void *)(wpedip1RegBA + i)),
			(unsigned int)(g_imgsys_wpe1_dip1_reg_base + i + 0x4),
			(unsigned int)ioread32((void *)(wpedip1RegBA + i + 0x4)),
			(unsigned int)(g_imgsys_wpe1_dip1_reg_base + i + 0x8),
			(unsigned int)ioread32((void *)(wpedip1RegBA + i + 0x8)),
			(unsigned int)(g_imgsys_wpe1_dip1_reg_base + i + 0xc),
			(unsigned int)ioread32((void *)(wpedip1RegBA + i + 0xc)));
		}
	}

	if ((hw_comb & IMGSYS_HW_FLAG_WPE_TNR) || (hw_comb & IMGSYS_HW_FLAG_OMC_TNR)) {
		if (!wpedip2RegBA) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip2 registers\n",
				__func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
			return;
		}

		for (i = 0; i <= 0x100; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
				__func__, (unsigned int)(g_imgsys_wpe2_dip1_reg_base + i),
			(unsigned int)ioread32((void *)(wpedip2RegBA + i)),
			(unsigned int)(g_imgsys_wpe2_dip1_reg_base + i + 0x4),
			(unsigned int)ioread32((void *)(wpedip2RegBA + i + 0x4)),
			(unsigned int)(g_imgsys_wpe2_dip1_reg_base + i + 0x8),
			(unsigned int)ioread32((void *)(wpedip2RegBA + i + 0x8)),
			(unsigned int)(g_imgsys_wpe2_dip1_reg_base + i + 0xc),
			(unsigned int)ioread32((void *)(wpedip2RegBA + i + 0xc)));
		}
	}

	if ((hw_comb & IMGSYS_HW_FLAG_WPE_LITE) || (hw_comb & IMGSYS_HW_FLAG_OMC_LITE)) {
		if (!wpedip3RegBA) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip3 registers\n",
				__func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
			return;
		}

		for (i = 0; i <= 0x100; i += 0x10) {
			dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
				__func__, (unsigned int)(g_imgsys_wpe3_dip1_reg_base + i),
			(unsigned int)ioread32((void *)(wpedip3RegBA + i)),
			(unsigned int)(g_imgsys_wpe3_dip1_reg_base + i + 0x4),
			(unsigned int)ioread32((void *)(wpedip3RegBA + i + 0x4)),
			(unsigned int)(g_imgsys_wpe3_dip1_reg_base + i + 0x8),
			(unsigned int)ioread32((void *)(wpedip3RegBA + i + 0x8)),
			(unsigned int)(g_imgsys_wpe3_dip1_reg_base + i + 0xc),
			(unsigned int)ioread32((void *)(wpedip3RegBA + i + 0xc)));
		}
	}
}

void imgsys_dl_checksum_dump(struct mtk_imgsys_dev *imgsys_dev,
	unsigned int hw_comb, char *logBuf_path,
	char *logBuf_inport, char *logBuf_outport, int dl_path)
{
	/*void __iomem *imgsysmainRegBA = 0L;*/
	/*void __iomem *wpedip1RegBA = 0L;*/
	/*void __iomem *wpedip2RegBA = 0L;*/
	/*void __iomem *wpedip3RegBA = 0L;*/
	unsigned int checksum_dbg_sel = 0x0;
	unsigned int original_dbg_sel_value = 0x0;
	char logBuf_final[FINAL_LOG_LENGTH];
	int debug0_req[2] = {0, 0};
	int debug0_rdy[2] = {0, 0};
	int debug0_checksum[2] = {0, 0};
	int debug1_line_cnt[2] = {0, 0};
	int debug1_pix_cnt[2] = {0, 0};
	int debug2_line_cnt[2] = {0, 0};
	int debug2_pix_cnt[2] = {0, 0};
	unsigned int dbg_sel_value[2] = {0x0, 0x0};
	unsigned int debug0_value[2] = {0x0, 0x0};
	unsigned int debug1_value[2] = {0x0, 0x0};
	unsigned int debug2_value[2] = {0x0, 0x0};
	unsigned int wpe_pqdip_mux_v = 0x0;
	unsigned int wpe_pqdip_mux2_v = 0x0;
	unsigned int wpe_pqdip_mux3_v = 0x0;
	char logBuf_temp[LOG_LEGNTH];
	int ret;
	unsigned int dip_pqdip_a_mux_v = 0x0;
	unsigned int dip_pqdip_b_mux_v = 0x0;

	memset((char *)logBuf_final, 0x0, sizeof(logBuf_final));
	logBuf_final[strlen(logBuf_final)] = '\0';
	memset((char *)logBuf_temp, 0x0, sizeof(logBuf_temp));
	logBuf_temp[strlen(logBuf_temp)] = '\0';

	/* Check DIP-PQDIP is MCRP_D1 or MCRP_D2 */
	if (dl_path == IMGSYS_DL_DIP_TO_PQDIP_A) {
		if (!wpeeispqdipaRegBA) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_eis_pqdip_a registers\n",
				__func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
			return;
		}
		dip_pqdip_a_mux_v = (unsigned int)ioread32((void *)(wpeeispqdipaRegBA + 0x0));
		dip_pqdip_a_mux_v = (dip_pqdip_a_mux_v & 0x000000006) >> 1;
		if (dip_pqdip_a_mux_v == 0)
			dl_path = IMGSYS_DL_DIP2_TO_PQDIP_A;
	} else if (dl_path == IMGSYS_DL_DIP_TO_PQDIP_B) {
		if (!wpetnrpqdipbRegBA) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_tnr_pqdip_b registers\n",
				__func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
			return;
		}
		dip_pqdip_b_mux_v = (unsigned int)ioread32((void *)(wpetnrpqdipbRegBA + 0x0));
		dip_pqdip_b_mux_v = (dip_pqdip_b_mux_v & 0x000000006) >> 1;
		if (dip_pqdip_b_mux_v == 0)
			dl_path = IMGSYS_DL_DIP2_TO_PQDIP_B;
	}

	dev_info(imgsys_dev->dev,
		"%s: + hw_comb/path(0x%x/%s) dl_path:%d, start dump\n",
		__func__, hw_comb, logBuf_path, dl_path);
	/* iomap registers */
	/*imgsysmainRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_TOP);*/
	if (!imgsysmainRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap imgsys_top registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	if (hw_comb & IMGSYS_HW_FLAG_WPE_EIS) {
		/* macro_comm status */
		if (!wpedip1RegBA) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip1 registers\n",
				__func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
			return;
		}
		wpe_pqdip_mux_v = (unsigned int)ioread32((void *)(wpedip1RegBA + 0xA8));
	}

	if ((hw_comb & IMGSYS_HW_FLAG_WPE_TNR) || (hw_comb & IMGSYS_HW_FLAG_OMC_TNR)) {
		if (!wpedip2RegBA) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip2 registers\n",
				__func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
			return;
		}
		wpe_pqdip_mux2_v = (unsigned int)ioread32((void *)(wpedip2RegBA + 0xA8));
	}

	if ((hw_comb & IMGSYS_HW_FLAG_WPE_LITE) || (hw_comb & IMGSYS_HW_FLAG_OMC_LITE)) {
		if (!wpedip3RegBA) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip3 registers\n",
				__func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
			return;
		}
		wpe_pqdip_mux3_v = (unsigned int)ioread32((void *)(wpedip3RegBA + 0xA8));
	}

	/* dump information */
	if (dl_path == IMGSYS_DL_NO_CHECK_SUM_DUMP) {
	} else {
		/*dump former engine in DL (imgsys main in port) status */
		checksum_dbg_sel = (unsigned int)((dl_path << 1) | (0 << 0));
		original_dbg_sel_value = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x50));
		original_dbg_sel_value = original_dbg_sel_value & 0xff00ffff; /*clear last time data*/
		dbg_sel_value[0] = (original_dbg_sel_value | 0x1 |
			((checksum_dbg_sel << 16) & 0x00ff0000));
		writel(dbg_sel_value[0], (imgsysmainRegBA + 0x50));
		dbg_sel_value[0] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x50));
		debug0_value[0] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x200));
		debug0_checksum[0] = (debug0_value[0] & 0x0000ffff);
		debug0_rdy[0] = (debug0_value[0] & 0x00800000) >> 23;
		debug0_req[0] = (debug0_value[0] & 0x01000000) >> 24;
		debug1_value[0] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x204));
		debug1_line_cnt[0] = ((debug1_value[0] & 0xffff0000) >> 16) & 0x0000ffff;
		debug1_pix_cnt[0] = (debug1_value[0] & 0x0000ffff);
		debug2_value[0] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x208));
		debug2_line_cnt[0] = ((debug2_value[0] & 0xffff0000) >> 16) & 0x0000ffff;
		debug2_pix_cnt[0] = (debug2_value[0] & 0x0000ffff);

		/*dump later engine in DL (imgsys main out port) status */
		checksum_dbg_sel = (unsigned int)((dl_path << 1) | (1 << 0));
		dbg_sel_value[1] = (original_dbg_sel_value | 0x1 |
			((checksum_dbg_sel << 16) & 0x00ff0000));
		writel(dbg_sel_value[1], (imgsysmainRegBA + 0x50));
		dbg_sel_value[1] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x50));
		debug0_value[1] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x200));
		debug0_checksum[1] = (debug0_value[1] & 0x0000ffff);
		debug0_rdy[1] = (debug0_value[1] & 0x00800000) >> 23;
		debug0_req[1] = (debug0_value[1] & 0x01000000) >> 24;
		debug1_value[1] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x204));
		debug1_line_cnt[1] = ((debug1_value[1] & 0xffff0000) >> 16) & 0x0000ffff;
		debug1_pix_cnt[1] = (debug1_value[1] & 0x0000ffff);
		debug2_value[1] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x208));
		debug2_line_cnt[1] = ((debug2_value[1] & 0xffff0000) >> 16) & 0x0000ffff;
		debug2_pix_cnt[1] = (debug2_value[1] & 0x0000ffff);

		if (debug0_req[0] == 1) {
			ret = snprintf(logBuf_temp, sizeof(logBuf_temp),
				"%s req to send data to %s/",
				logBuf_inport, logBuf_outport);
			if (ret > sizeof(logBuf_temp))
				dev_dbg(imgsys_dev->dev,
					"%s: string truncated\n", __func__);
		} else {
			ret = snprintf(logBuf_temp, sizeof(logBuf_temp),
				"%s not send data to %s/",
				logBuf_inport, logBuf_outport);
			if (ret > sizeof(logBuf_temp))
				dev_dbg(imgsys_dev->dev,
					"%s: string truncated\n", __func__);
		}
		strncat(logBuf_final, logBuf_temp, strlen(logBuf_temp));
		memset((char *)logBuf_temp, 0x0, sizeof(logBuf_temp));
		logBuf_temp[strlen(logBuf_temp)] = '\0';
		if (debug0_rdy[0] == 1) {
			ret = snprintf(logBuf_temp, sizeof(logBuf_temp),
				"%s rdy to receive data from %s",
				logBuf_outport, logBuf_inport);
			if (ret > sizeof(logBuf_temp))
				dev_dbg(imgsys_dev->dev,
					"%s: string truncated\n", __func__);
		} else {
			ret = snprintf(logBuf_temp, sizeof(logBuf_temp),
				"%s not rdy to receive data from %s",
				logBuf_outport, logBuf_inport);
			if (ret >= sizeof(logBuf_temp))
				dev_dbg(imgsys_dev->dev,
					"%s: string truncated\n", __func__);
		}
		strncat(logBuf_final, logBuf_temp, strlen(logBuf_temp));
		dev_info(imgsys_dev->dev,
			"%s: %s", __func__, logBuf_final);

		memset((char *)logBuf_final, 0x0, sizeof(logBuf_final));
		logBuf_final[strlen(logBuf_final)] = '\0';
		memset((char *)logBuf_temp, 0x0, sizeof(logBuf_temp));
		logBuf_temp[strlen(logBuf_temp)] = '\0';
		if (debug0_req[1] == 1) {
			ret = snprintf(logBuf_temp, sizeof(logBuf_temp),
				"%s req to send data to %sPIPE/",
				logBuf_outport, logBuf_outport);
			if (ret >= sizeof(logBuf_temp))
				dev_dbg(imgsys_dev->dev,
					"%s: string truncated\n", __func__);
		} else {
			ret = snprintf(logBuf_temp, sizeof(logBuf_temp),
				"%s not send data to %sPIPE/",
				logBuf_outport, logBuf_outport);
			if (ret >= sizeof(logBuf_temp))
				dev_dbg(imgsys_dev->dev,
					"%s: string truncated\n", __func__);
		}
		strncat(logBuf_final, logBuf_temp, strlen(logBuf_temp));
		memset((char *)logBuf_temp, 0x0, sizeof(logBuf_temp));
		logBuf_temp[strlen(logBuf_temp)] = '\0';
		if (debug0_rdy[1] == 1) {
			ret = snprintf(logBuf_temp, sizeof(logBuf_temp),
				"%sPIPE rdy to receive data from %s",
				logBuf_outport, logBuf_outport);
			if (ret >= sizeof(logBuf_temp))
				dev_dbg(imgsys_dev->dev,
					"%s: string truncated\n", __func__);
		} else {
			ret = snprintf(logBuf_temp, sizeof(logBuf_temp),
				"%sPIPE not rdy to receive data from %s",
				logBuf_outport, logBuf_outport);
			if (ret >= sizeof(logBuf_temp))
				dev_dbg(imgsys_dev->dev,
					"%s: string truncated\n", __func__);
		}
		strncat(logBuf_final, logBuf_temp, strlen(logBuf_temp));
		dev_info(imgsys_dev->dev,
			"%s: %s", __func__, logBuf_final);
		dev_info(imgsys_dev->dev,
			"%s: in_req/in_rdy/out_req/out_rdy = %d/%d/%d/%d,(checksum: in/out) = (%d/%d)",
			__func__,
			debug0_req[0], debug0_rdy[0],
			debug0_req[1], debug0_rdy[1],
			debug0_checksum[0], debug0_checksum[1]);
		dev_info(imgsys_dev->dev,
			"%s: info01 in_line/in_pix/out_line/out_pix = %d/%d/%d/%d",
			__func__,
			debug1_line_cnt[0], debug1_pix_cnt[0], debug1_line_cnt[1],
			debug1_pix_cnt[1]);
		dev_info(imgsys_dev->dev,
			"%s: info02 in_line/in_pix/out_line/out_pix = %d/%d/%d/%d",
			__func__,
			debug2_line_cnt[0], debug2_pix_cnt[0], debug2_line_cnt[1],
			debug2_pix_cnt[1]);
	}
	dev_info(imgsys_dev->dev, "%s: ===(%s): %s DBG INFO===",
		__func__, logBuf_path, logBuf_inport);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(g_imgsys_main_reg_base + 0x50), dbg_sel_value[0]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(g_imgsys_main_reg_base + 0x200), debug0_value[0]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(g_imgsys_main_reg_base + 0x204), debug1_value[0]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(g_imgsys_main_reg_base + 0x208), debug2_value[0]);

	dev_info(imgsys_dev->dev, "%s: ===(%s): %s DBG INFO===",
		__func__, logBuf_path, logBuf_outport);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(g_imgsys_main_reg_base + 0x50), dbg_sel_value[1]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(g_imgsys_main_reg_base + 0x200), debug0_value[1]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(g_imgsys_main_reg_base + 0x204), debug1_value[1]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(g_imgsys_main_reg_base + 0x208), debug2_value[1]);

	dev_info(imgsys_dev->dev, "%s: ===(%s): IMGMAIN CG INFO===",
		__func__, logBuf_path);
	dev_info(imgsys_dev->dev, "%s: CG_CON  0x%08X %08X", __func__,
		(unsigned int)(g_imgsys_main_reg_base + 0x0),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + 0x0)));
	dev_info(imgsys_dev->dev, "%s: CG_SET  0x%08X %08X", __func__,
		(unsigned int)(g_imgsys_main_reg_base + 0x4),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + 0x4)));
	dev_info(imgsys_dev->dev, "%s: CG_CLR  0x%08X %08X", __func__,
		(unsigned int)(g_imgsys_main_reg_base + 0x8),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + 0x8)));

	if (hw_comb & IMGSYS_HW_FLAG_WPE_EIS) {
		dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
			(unsigned int)(g_imgsys_wpe1_dip1_reg_base + 0xA8), wpe_pqdip_mux_v);
	} else if ((hw_comb & IMGSYS_HW_FLAG_WPE_TNR) || (hw_comb & IMGSYS_HW_FLAG_OMC_TNR)) {
		dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
			(unsigned int)(g_imgsys_wpe2_dip1_reg_base + 0xA8), wpe_pqdip_mux2_v);
	} else if (hw_comb & IMGSYS_HW_FLAG_WPE_LITE || (hw_comb & IMGSYS_HW_FLAG_OMC_LITE)) {
		dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
			(unsigned int)(g_imgsys_wpe3_dip1_reg_base + 0xA8), wpe_pqdip_mux3_v);
	}

	/*iounmap(imgsysmainRegBA);*/
}

void imgsys_dl_debug_dump(struct mtk_imgsys_dev *imgsys_dev, unsigned int hw_comb)
{
	int dl_path = 0;
	char logBuf_path[LOG_LEGNTH];
	char logBuf_inport[LOG_LEGNTH];
	char logBuf_outport[LOG_LEGNTH];
	char logBuf_eng[LOG_LEGNTH];
	int i = 0, get = false;
	int ret = 0;

	memset((char *)logBuf_path, 0x0, sizeof(logBuf_path));
	logBuf_path[strlen(logBuf_path)] = '\0';
	memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
	logBuf_inport[strlen(logBuf_inport)] = '\0';
	memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
	logBuf_outport[strlen(logBuf_outport)] = '\0';

	for (i = 0 ; i < DL_CHECK_ENG_NUM ; i++) {
		memset((char *)logBuf_eng, 0x0, sizeof(logBuf_eng));
		logBuf_eng[strlen(logBuf_eng)] = '\0';
		if (hw_comb & dbg_engine_name_list[i].eng_e) {
			if (get) {
				ret = snprintf(logBuf_eng, sizeof(logBuf_eng), "-%s",
					dbg_engine_name_list[i].eng_name);
				if (ret >= sizeof(logBuf_eng))
					dev_dbg(imgsys_dev->dev,
						"%s: string truncated\n", __func__);
			} else {
				ret = snprintf(logBuf_eng, sizeof(logBuf_eng), "%s",
					dbg_engine_name_list[i].eng_name);
				if (ret >= sizeof(logBuf_eng))
					dev_dbg(imgsys_dev->dev,
						"%s: string truncated\n", __func__);
			}
			get = true;
		}
		strncat(logBuf_path, logBuf_eng, strlen(logBuf_eng));
	}
	memset((char *)logBuf_eng, 0x0, sizeof(logBuf_eng));
	logBuf_eng[strlen(logBuf_eng)] = '\0';
	ret = snprintf(logBuf_eng, sizeof(logBuf_eng), "%s", " FAIL");
	if (ret >= sizeof(logBuf_eng))
		dev_dbg(imgsys_dev->dev,
			"%s: string truncated\n", __func__);
	strncat(logBuf_path, logBuf_eng, strlen(logBuf_eng));

	dev_info(imgsys_dev->dev, "%s: %s\n",
			__func__, logBuf_path);
	switch (hw_comb) {
	/*DL checksum case*/
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_TRAW):
		dl_path = IMGSYS_DL_WPE_EIS_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_EIS");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"TRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_LTR):
		dl_path = IMGSYS_DL_WPE_EIS_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_EIS");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"LTRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
#if HAVE_IMGSYS_HW_FLAG_XTR
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_XTR):
		dl_path = IMGSYS_DL_WPEE_XTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_EIS");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"XTRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
#endif
	case (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_TRAW):
		dl_path = IMGSYS_DL_WPE_LITE_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_LITE");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"TRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		dev_info(imgsys_dev->dev,
			"%s: we dont have checksum for WPELITE DL TRAW\n",
			__func__);
		break;
	case (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_LTR):
		dl_path = IMGSYS_DL_WPE_LITE_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_LITE");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"LTRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		dev_info(imgsys_dev->dev,
			"%s: we dont have checksum for WPELITE DL LTRAW\n",
			__func__);
		break;
#if HAVE_IMGSYS_HW_FLAG_XTR
	case (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_XTR):
		dl_path = IMGSYS_DL_WPET_TRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_LITE");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"XTRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		dev_info(imgsys_dev->dev,
			"%s: we dont have checksum for WPELITE DL XTRAW\n",
			__func__);
		break;
#endif
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_TRAW):
		dl_path = IMGSYS_DL_OMC_TNR_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"OMC_TNR");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"TRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_LTR):
		dl_path = IMGSYS_DL_OMC_TNR_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"OMC_TNR");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"LTRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_OMC_LITE | IMGSYS_HW_FLAG_TRAW):
		dl_path = IMGSYS_DL_OMC_LITE_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"OMC_LITE");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"TRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_OMC_LITE | IMGSYS_HW_FLAG_LTR):
		dl_path = IMGSYS_DL_OMC_LITE_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"OMC_LITE");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"LTRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_DIP):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A |
		IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_WPE_EIS_TO_DIP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_EIS");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			 "DIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_A;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPA");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_B;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPB");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_DIP):
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A |
		IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_OMC_TNR_TO_DIP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"OMC_TNR");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_A;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPA");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_B;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPB");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_PQDIP_A |
		IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_NO_CHECK_SUM_DUMP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_EIS");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_PQDIP_A |
		IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_WPE_LITE_TO_PQDIP_A_B;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_LITE");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_PQDIP_A |
		IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_NO_CHECK_SUM_DUMP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_TNR");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_A | IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_WPE_EIS_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_EIS");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"TRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_TRAW_TO_DIP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"TRAW");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_A;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPA");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_B;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPB");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_A | IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_WPE_EIS_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_EIS");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"LTRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_LTRAW_TO_DIP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"LTRAW");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_A;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPA");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_B;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPB");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP):
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_A | IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_OMC_TNR_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"OMC_TNR");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"TRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_TRAW_TO_DIP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
				"TRAW");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_A;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPA");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_B;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPB");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP):
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_A | IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_OMC_TNR_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"OMC_TNR");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"LTRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_LTRAW_TO_DIP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"LTRAW");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_A;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPA");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_B;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPB");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP):
	case (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_A | IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_WPE_LITE_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_LITE");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"TRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_TRAW_TO_DIP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"TRAW");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_A;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPA");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_B;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPB");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP):
	case (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_A | IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_WPE_LITE_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_LITE");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"LTRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_LTRAW_TO_DIP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"LTRAW");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_A;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPA");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_B;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPB");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_DIP):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_DIP |
		IMGSYS_HW_FLAG_PQDIP_A | IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_WPE_EIS_TO_DIP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_EIS");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_OMC_TNR_TO_DIP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"OMC_TNR");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_A;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPA");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_B;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPB");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_TRAW |
		IMGSYS_HW_FLAG_DIP):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_TRAW |
		IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_TRAW |
		IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_TRAW |
		IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A | IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_WPE_EIS_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_EIS");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"TRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_WPE_LITE_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_LITE");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"TRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_TRAW_TO_DIP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"TRAW");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_A;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPA");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_B;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPB");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_LTR |
		IMGSYS_HW_FLAG_DIP):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_LTR |
		IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_LTR |
		IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_LTR |
		IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A | IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_WPE_EIS_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_EIS");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"LTRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_WPE_LITE_TO_TRAW_LTRAW;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"WPE_LITE");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"LTRAW");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_LTRAW_TO_DIP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"LTRAW");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_A;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPA");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_B;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPB");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_TRAW |
		IMGSYS_HW_FLAG_DIP):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_TRAW |
		IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_TRAW |
		IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_TRAW |
		IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A | IMGSYS_HW_FLAG_PQDIP_B):
		dev_info(imgsys_dev->dev,
			"%s: TOBE CHECKED SELECTION BASED ON FMT..\n",
			__func__);
		break;
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_LTR |
		IMGSYS_HW_FLAG_DIP):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_LTR |
		IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_LTR |
		IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_OMC_TNR | IMGSYS_HW_FLAG_LTR |
		IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A | IMGSYS_HW_FLAG_PQDIP_B):
		dev_info(imgsys_dev->dev,
			"%s: TOBE CHECKED SELECTION BASED ON FMT..\n",
			__func__);
		break;
	case (IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP):
	case (IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_TRAW | IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A |
		IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_TRAW_TO_DIP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"TRAW");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_A;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPA");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_B;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPB");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP):
	case (IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_LTR | IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A |
		IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_LTRAW_TO_DIP;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"LTRAW");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_A;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPA");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_B;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPB");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A):
	case (IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_B):
	case (IMGSYS_HW_FLAG_DIP | IMGSYS_HW_FLAG_PQDIP_A | IMGSYS_HW_FLAG_PQDIP_B):
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_A;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPA");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, sizeof(logBuf_inport));
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, sizeof(logBuf_outport));
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_TO_PQDIP_B;
		ret = snprintf(logBuf_inport, sizeof(logBuf_inport), "%s",
			"DIP");
		if (ret >= sizeof(logBuf_inport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		ret = snprintf(logBuf_outport, sizeof(logBuf_outport), "%s",
			"PQDIPB");
		if (ret >= sizeof(logBuf_outport))
			dev_dbg(imgsys_dev->dev,
				"%s: string truncated\n", __func__);
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_HW_FLAG_ADL_A | IMGSYS_HW_FLAG_LTR):
	case (IMGSYS_HW_FLAG_ADL_A | IMGSYS_HW_FLAG_ADL_B | IMGSYS_HW_FLAG_LTR):
		/**
		 * dl_path = IMGSYS_DL_ADLA_LTRAW;
		 * snprintf(logBuf_inport, sizeof(logBuf_inport), "%s", "ADL");
		 * snprintf(logBuf_outport, sizeof(logBuf_outport), "%s", "LTRAW");
		 * imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
		 *  logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		 */
		dev_info(imgsys_dev->dev,
			"%s: we dont have checksum for ADL DL LTRAW\n",
			__func__);
		break;
	case (IMGSYS_HW_FLAG_ME):
		imgsys_cg_debug_dump(imgsys_dev, hw_comb);
		break;
	case IMGSYS_HW_FLAG_MAE:
		/*TODO(Ming-Hsuan): Check whether need to dump something for MAE debug */
		break;
	case IMGSYS_HW_FLAG_DFP:
		/*TODO(Song-you): Check whether need to dump something for MAE debug */
		break;
	case IMGSYS_HW_FLAG_DPE:
		/*TODO(Eric): Check whether need to dump something for MAE debug */
		break;
	default:
		break;
	}

	dev_info(imgsys_dev->dev, "%s: -\n", __func__);
}

