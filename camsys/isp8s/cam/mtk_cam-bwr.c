// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/vmalloc.h>
#include <linux/suspend.h>
#include <linux/component.h>

#include "mtk_cam.h"
#include "mtk_cam-reg_utils.h"
#include "mtk_cam-bwr.h"
#include "mtk_cam-bwr_regs.h"
#include "mtk_cam-debug_option.h"

// place below all other include
#include "mtk_cam-virt-isp.h"

#define BWR_ENGINES   0xFFF

static int debug_bwr_mode = BWR_ENGINES;
module_param(debug_bwr_mode, int, 0644);
MODULE_PARM_DESC(debug_bwr_mode, "0: sw mode, 0x7ff: all engine hw mode");

static int debug_bwr_eng_filter = BWR_ENGINES;
module_param(debug_bwr_eng_filter, int, 0644);
MODULE_PARM_DESC(debug_bwr_eng_filter, "debug bwr engine channel bw");

static int debug_bwr_log;
module_param(debug_bwr_log, int, 0644);
MODULE_PARM_DESC(debug_bwr_log, "debug bwr settings");

#define CHANNEL_OFFSET   0x100
#define ENGINE_OFFSET    0x4

#define CLK_CYC_PER_US 26 // default 26MHz

#define BWR_BW_DECIMAL_POS       3
#define BWR_BW_DECIMAL_WIDTH     15
#define BWR_RAT_DECIMAL_POS      7

/* todo: check following factor */
//1.33 * 1.03 = 1.369 (1.03 for SMMU TCU)
#define BWR_SRT_EMI_OCC_FACTOR       175
//1.17 * 1.03 = 1.205 (1.03 for SMMU TCU)
#define BWR_HRT_EMI_OCC_FACTOR       154
#define BWR_SRT_BW_OCC_FACTOR        181  //1.42
#define BWR_HRT_BW_OCC_FACTOR        181  //1.42

static inline u32 to_bw_csr(int MBs)
{
	return (MBs > 0 && MBs < (1 << BWR_BW_DECIMAL_WIDTH)) ?
			MBs << BWR_BW_DECIMAL_POS : 0;
}

static inline u32 to_ratio_csr(int MBs)
{
	return MBs << BWR_RAT_DECIMAL_POS;
}

static inline u32 to_bw_val(int MBs)
{
	return MBs >> BWR_BW_DECIMAL_POS;
}

static inline u32 to_ratio_val(int MBs)
{
	return MBs >> BWR_RAT_DECIMAL_POS;
}

static const char *value_to_str(const char * const str_arr[], size_t size,
				int value)
{
	const char *str;

	if (WARN_ON((unsigned int)value >= size))
		return "(not-found)";

	str = str_arr[value];
	return str ? str : "null";
}

const char *str_engine(int event)
{
	static const char * const str[] = {
		[ENGINE_SUB_A] = "sub_a",
		[ENGINE_SUB_B] = "sub_b",
		[ENGINE_SUB_C] = "sub_c",
		[ENGINE_MRAW] = "mraw", /* Jayer no used */
		[ENGINE_CAM_MAIN] = "cam_main",
		[ENGINE_CAMSV_B] = "camsv_b",
		[ENGINE_CAMSV_A] = "camsv_a",
		[ENGINE_CAMSV_C] = "camsv_c",
		[ENGINE_PDA] = "pda",
		[ENGINE_CAMSV_OTHER] = "camsv_other",
		[ENGINE_UISP] = "uisp",
	};

	return value_to_str(str, ARRAY_SIZE(str), event);
}

const char *str_axi_port(int event)
{
	static const char * const str[] = {
		[CAM0_PORT] = "CAM-0",
		[CAM1_PORT] = "CAM-1",
		[CAM2_PORT] = "CAM-2",
		[CAM3_PORT] = "CAM-3",
		[CAM6_PORT] = "CAM-6",
	};

	return value_to_str(str, ARRAY_SIZE(str), event);
}

static void bwr_set_chn_bw(struct mtk_bwr_device *bwr,
			  enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi,
			  int srt_r_bw, int srt_w_bw, int hrt_r_bw, int hrt_w_bw, bool clr)
{
	int bw, offset;

	if (!(debug_bwr_eng_filter & 1 << engine))
		return;

	mutex_lock(&bwr->op_lock);
	if (!bwr->started) {
		pr_info("%s: engine:%d BWR is disabled\n", __func__, engine);
		mutex_unlock(&bwr->op_lock);
		return;
	}

	offset = CHANNEL_OFFSET * axi + ENGINE_OFFSET * engine;

	//SRT bandwidth
	bw = clr ? srt_r_bw :
			to_bw_val(readl(bwr->base + REG_BWR_CAM_SRT_R0_ENG_BW0_0 + offset)) + srt_r_bw;
	writel(to_bw_csr(bw),bwr->base + REG_BWR_CAM_SRT_R0_ENG_BW0_0 + offset);

	bw = clr ? srt_w_bw :
			to_bw_val(readl(bwr->base + REG_BWR_CAM_SRT_W0_ENG_BW0_0 + offset)) + srt_w_bw;
	writel(to_bw_csr(bw), bwr->base + REG_BWR_CAM_SRT_W0_ENG_BW0_0 + offset);

	//HRT bandwidth
	bw = clr ? hrt_r_bw :
			to_bw_val(readl(bwr->base + REG_BWR_CAM_HRT_R0_ENG_BW0_0 + offset)) + hrt_r_bw;
	writel(to_bw_csr(bw), bwr->base + REG_BWR_CAM_HRT_R0_ENG_BW0_0 + offset);

	bw = clr ? hrt_w_bw :
			to_bw_val(readl(bwr->base + REG_BWR_CAM_HRT_W0_ENG_BW0_0 + offset)) + hrt_w_bw;
	writel(to_bw_csr(bw), bwr->base + REG_BWR_CAM_HRT_W0_ENG_BW0_0 + offset);

	if (CAM_DEBUG_ENABLED(MMQOS))
		pr_info("%s: engine:%d, axi:%d SRT(r/w): %d/%d HRT(r/w): %d/%d, clear: %d\n",
		__func__, engine, axi, srt_r_bw, srt_w_bw, hrt_r_bw, hrt_w_bw, clr);

	mutex_unlock(&bwr->op_lock);
}

static void bwr_set_emi_bw(struct mtk_bwr_device *bwr,
		enum BWR_ENGINE_TYPE engine, int srt_emi, int hrt_emi, bool clr)
{
	int bw, offset;

	if (!(debug_bwr_eng_filter & 1 << engine))
		return;

	mutex_lock(&bwr->op_lock);
	if (!bwr->started) {
		pr_info("%s: engine:%d BWR is disabled\n", __func__, engine);
		mutex_unlock(&bwr->op_lock);
		return;
	}

	offset = ENGINE_OFFSET * engine;

	//SRT bandwidth
	bw = clr ? srt_emi :
			to_bw_val(readl(bwr->base + REG_BWR_CAM_SRT_EMI_ENG_BW0 + offset)) + srt_emi;
	writel(to_bw_csr(bw),bwr->base + REG_BWR_CAM_SRT_EMI_ENG_BW0 + offset);

	bw = clr ? hrt_emi :
			to_bw_val(readl(bwr->base + REG_BWR_CAM_HRT_EMI_ENG_BW0 + offset)) + hrt_emi;
	writel(to_bw_csr(bw), bwr->base + REG_BWR_CAM_HRT_EMI_ENG_BW0 + offset);

	if (CAM_DEBUG_ENABLED(MMQOS))
		pr_info("%s: engine:%d, SRT_EMI/HRT_EMI: %d/%d, clear: %d\n",
				__func__, engine, srt_emi, hrt_emi, clr);

	mutex_unlock(&bwr->op_lock);
}

static void bwr_zero_bw(struct mtk_bwr_device *bwr,
			  enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi)
{
	mutex_lock(&bwr->op_lock);
	if (!bwr->started) {
		pr_info("%s: engine:%d BWR is disabled\n", __func__, engine);
		mutex_unlock(&bwr->op_lock);
		return;
	}

	//SRT bandwidth
	writel(0, bwr->base +
		REG_BWR_CAM_SRT_R0_ENG_BW0_0 + CHANNEL_OFFSET * axi +
			ENGINE_OFFSET * engine);

	writel(0, bwr->base +
		REG_BWR_CAM_SRT_W0_ENG_BW0_0 + CHANNEL_OFFSET * axi +
			ENGINE_OFFSET * engine);

	writel(0, bwr->base +
		REG_BWR_CAM_SRT_EMI_ENG_BW0 + ENGINE_OFFSET * engine);

	//HRT bandwidth
	writel(0, bwr->base +
		REG_BWR_CAM_HRT_R0_ENG_BW0_0 + CHANNEL_OFFSET * axi +
			ENGINE_OFFSET * engine);

	writel(0, bwr->base +
		REG_BWR_CAM_HRT_W0_ENG_BW0_0 + CHANNEL_OFFSET * axi +
			ENGINE_OFFSET * engine);

	writel(0, bwr->base +
		REG_BWR_CAM_HRT_EMI_ENG_BW0 + ENGINE_OFFSET * engine);

	if (CAM_DEBUG_ENABLED(MMQOS))
		pr_info("%s: engine:%d, axi:%d set zero\n", __func__, engine, axi);

	mutex_unlock(&bwr->op_lock);

}

static void bwr_set_default(struct mtk_bwr_device *bwr)
{
	//no modules need
}

static void bwr_clr_default(struct mtk_bwr_device *bwr)
{
	//no modules need
}

//unit MB/s
#define BWR_TEST_SRT_R    100
#define BWR_TEST_SRT_W    200
#define BWR_TEST_SRT_EMI  300
#define BWR_TEST_HRT_R    100
#define BWR_TEST_HRT_W    200
#define BWR_TEST_HRT_EMI  300
static __maybe_unused void bwr_set_test(struct mtk_bwr_device *bwr)
{
	bwr_set_chn_bw(bwr, ENGINE_SUB_A, CAM0_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_chn_bw(bwr, ENGINE_SUB_A, CAM2_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_emi_bw(bwr, ENGINE_SUB_A, BWR_TEST_SRT_EMI, BWR_TEST_HRT_EMI, true);

	bwr_set_chn_bw(bwr, ENGINE_SUB_B, CAM1_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_chn_bw(bwr, ENGINE_SUB_B, CAM0_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_emi_bw(bwr, ENGINE_SUB_B, BWR_TEST_SRT_EMI, BWR_TEST_HRT_EMI, true);

	bwr_set_chn_bw(bwr, ENGINE_SUB_C, CAM1_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_chn_bw(bwr, ENGINE_SUB_C, CAM3_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_emi_bw(bwr, ENGINE_SUB_C, BWR_TEST_SRT_EMI, BWR_TEST_HRT_EMI, true);

	mtk_cam_isp8s_bwr_dbg_dump(bwr);
}

static __maybe_unused void bwr_clr_test(struct mtk_bwr_device *bwr)
{
	int i ,j;

	for (i = 0 ; i < ENGINE_NUM; i++)
		for (j = 0 ; j < BWR_AXI_PORT_NUM; j++)
			bwr_zero_bw(bwr, i, j);
}

static int bwr_start(struct mtk_bwr_device *bwr)
{
	int i, j, offset;
	u32 val;

	mutex_lock(&bwr->op_lock);

	//set unit to 64MB/s
	val = readl(bwr->base + REG_BWR_CAM_BW_TYPE);
	SET_FIELD(&val, BWR_CAM_BW_UNIT_SEL, 2);
	writel(val, bwr->base + REG_BWR_CAM_BW_TYPE);

	//occ factor
	writel(BWR_SRT_EMI_OCC_FACTOR, bwr->base + REG_BWR_CAM_SRT_EMI_OCC_FACTOR);
	writel(BWR_HRT_EMI_OCC_FACTOR, bwr->base + REG_BWR_CAM_HRT_EMI_OCC_FACTOR);
	writel(BWR_SRT_BW_OCC_FACTOR, bwr->base + REG_BWR_CAM_SRT_RW_OCC_FACTOR);
	writel(BWR_HRT_BW_OCC_FACTOR, bwr->base + REG_BWR_CAM_HRT_RW_OCC_FACTOR);

	/* todo: jayer need ? */
	writel(0x0, bwr->base + REG_BWR_CAM_MTCMOS_EN_VLD);

	//dvfs freq
	writel(0x1, bwr->base + REG_BWR_CAM_HRT_EMI_DVFS_FREQ);
	writel(0x1, bwr->base + REG_BWR_CAM_SRT_EMI_DVFS_FREQ);
	writel(0x4, bwr->base + REG_BWR_CAM_HRT_RW_DVFS_FREQ);
	writel(0x4, bwr->base + REG_BWR_CAM_SRT_RW_DVFS_FREQ);

	/* emi ratio */
	for (i = 0 ; i < ENGINE_NUM; ++i) {
		offset = i * ENGINE_OFFSET;
		writel(to_ratio_csr(1), bwr->base + REG_BWR_CAM_SRT_EMI_ENG_BW_RAT0 + offset);
		writel(to_ratio_csr(1), bwr->base + REG_BWR_CAM_HRT_EMI_ENG_BW_RAT0 + offset);

		if (debug_bwr_log)
			pr_info("%s emi ratio 0x%x:0x%x 0x%x:0x%x\n", __func__,
				REG_BWR_CAM_SRT_EMI_ENG_BW_RAT0 + offset,
				readl_relaxed(bwr->base + REG_BWR_CAM_SRT_EMI_ENG_BW_RAT0 + offset),
				REG_BWR_CAM_HRT_EMI_ENG_BW_RAT0 + offset,
				readl_relaxed(bwr->base + REG_BWR_CAM_HRT_EMI_ENG_BW_RAT0 + offset));
	}

	/* default ratio */
	for (i = 0 ; i < BWR_AXI_PORT_NUM + 1; ++i) {
		for (j = 0 ; j < ENGINE_NUM; ++j) {
			offset = CHANNEL_OFFSET * i + ENGINE_OFFSET * j;
			writel(to_ratio_csr(1), bwr->base + REG_BWR_CAM_SRT_R0_ENG_BW_RAT0_0 + offset);
			writel(to_ratio_csr(1), bwr->base + REG_BWR_CAM_SRT_W0_ENG_BW_RAT0_0 + offset);
			writel(to_ratio_csr(1), bwr->base + REG_BWR_CAM_HRT_R0_ENG_BW_RAT0_0 + offset);
			writel(to_ratio_csr(1), bwr->base + REG_BWR_CAM_HRT_W0_ENG_BW_RAT0_0 + offset);

			if (debug_bwr_log)
				pr_info("%s chn ratio: 0x%x:0x%x 0x%x:0x%x 0x%x:0x%x 0x%x:0x%x\n", __func__,
					REG_BWR_CAM_SRT_R0_ENG_BW_RAT0_0 + offset,
					readl_relaxed(bwr->base + REG_BWR_CAM_SRT_R0_ENG_BW_RAT0_0 + offset),
					REG_BWR_CAM_SRT_W0_ENG_BW_RAT0_0 + offset,
					readl_relaxed(bwr->base + REG_BWR_CAM_SRT_W0_ENG_BW_RAT0_0 + offset),
					REG_BWR_CAM_HRT_R0_ENG_BW_RAT0_0 + offset,
					readl_relaxed(bwr->base + REG_BWR_CAM_HRT_R0_ENG_BW_RAT0_0 + offset),
					REG_BWR_CAM_HRT_W0_ENG_BW_RAT0_0 + offset,
					readl_relaxed(bwr->base + REG_BWR_CAM_HRT_W0_ENG_BW_RAT0_0 + offset));
		}
	}

	//emi bwr mode
	writel(debug_bwr_mode,
			bwr->base + REG_BWR_CAM_SRT_EMI_BW_QOS_SEL);
	writel(~debug_bwr_mode & BWR_ENGINES,
			bwr->base + REG_BWR_CAM_SRT_EMI_SW_QOS_EN);
	writel(debug_bwr_mode,
			bwr->base + REG_BWR_CAM_HRT_EMI_BW_QOS_SEL);
	writel(~debug_bwr_mode & BWR_ENGINES,
			bwr->base + REG_BWR_CAM_HRT_EMI_SW_QOS_EN);

	//BWR mode
	for (i = 0; i < BWR_AXI_PORT_NUM; ++i) {
		writel(debug_bwr_mode,
			bwr->base + REG_BWR_CAM_SRT_R0_BW_QOS_SEL0 + CHANNEL_OFFSET * i);
		writel(~debug_bwr_mode & BWR_ENGINES,
			bwr->base + REG_BWR_CAM_SRT_R0_SW_QOS_EN0 + CHANNEL_OFFSET * i);

		writel(debug_bwr_mode,
			bwr->base + REG_BWR_CAM_SRT_W0_BW_QOS_SEL0 + CHANNEL_OFFSET * i);
		writel(~debug_bwr_mode & BWR_ENGINES,
			bwr->base + REG_BWR_CAM_SRT_W0_SW_QOS_EN0 + CHANNEL_OFFSET * i);

		writel(debug_bwr_mode,
			bwr->base + REG_BWR_CAM_HRT_R0_BW_QOS_SEL0 + CHANNEL_OFFSET * i);
		writel(~debug_bwr_mode & BWR_ENGINES,
			bwr->base + REG_BWR_CAM_HRT_R0_SW_QOS_EN0 + CHANNEL_OFFSET * i);

		writel(debug_bwr_mode,
			bwr->base + REG_BWR_CAM_HRT_W0_BW_QOS_SEL0 + CHANNEL_OFFSET * i);
		writel(~debug_bwr_mode & BWR_ENGINES,
			bwr->base + REG_BWR_CAM_HRT_W0_SW_QOS_EN0 + CHANNEL_OFFSET * i);

		if (debug_bwr_log) {
			pr_info("%s BWR mode 0x%x:0x%x 0x%x:0x%x 0x%x:0x%x 0x%x:0x%x\n", __func__,
				REG_BWR_CAM_SRT_R0_BW_QOS_SEL0 + CHANNEL_OFFSET * i,
				readl_relaxed(bwr->base + REG_BWR_CAM_SRT_R0_BW_QOS_SEL0 + CHANNEL_OFFSET * i),
				REG_BWR_CAM_SRT_R0_SW_QOS_EN0 + CHANNEL_OFFSET * i,
				readl_relaxed(bwr->base + REG_BWR_CAM_SRT_R0_SW_QOS_EN0 + CHANNEL_OFFSET * i),

				REG_BWR_CAM_SRT_W0_BW_QOS_SEL0 + CHANNEL_OFFSET * i,
				readl_relaxed(bwr->base + REG_BWR_CAM_SRT_W0_BW_QOS_SEL0 + CHANNEL_OFFSET * i),
				REG_BWR_CAM_SRT_W0_SW_QOS_EN0 + CHANNEL_OFFSET * i,
				readl_relaxed(bwr->base + REG_BWR_CAM_SRT_W0_SW_QOS_EN0 + CHANNEL_OFFSET * i));

			pr_info("%s BWR mode 0x%x:0x%x 0x%x:0x%x 0x%x:0x%x 0x%x:0x%x\n", __func__,
				REG_BWR_CAM_HRT_R0_BW_QOS_SEL0 + CHANNEL_OFFSET * i,
				readl_relaxed(bwr->base + REG_BWR_CAM_HRT_R0_BW_QOS_SEL0 + CHANNEL_OFFSET * i),
				REG_BWR_CAM_HRT_R0_SW_QOS_EN0 + CHANNEL_OFFSET * i,
				readl_relaxed(bwr->base + REG_BWR_CAM_HRT_R0_SW_QOS_EN0 + CHANNEL_OFFSET * i),

				REG_BWR_CAM_HRT_W0_BW_QOS_SEL0 + CHANNEL_OFFSET * i,
				readl_relaxed(bwr->base + REG_BWR_CAM_HRT_W0_BW_QOS_SEL0 + CHANNEL_OFFSET * i),
				REG_BWR_CAM_HRT_W0_SW_QOS_EN0 + CHANNEL_OFFSET * i,
				readl_relaxed(bwr->base + REG_BWR_CAM_HRT_W0_SW_QOS_EN0 + CHANNEL_OFFSET * i));
		}
	}

	//100us
	writel(CLK_CYC_PER_US * 100, bwr->base + REG_BWR_CAM_RPT_TIMER);
	//20us
	writel(CLK_CYC_PER_US * 20, bwr->base + REG_BWR_CAM_DBC_CYC);

	writel(FBIT(BWR_CAM_RPT_START), bwr->base + REG_BWR_CAM_RPT_CTRL);
	pr_info("%s rpt_timer/dbc_cyc:0x%x/0x%x\n", __func__,
		readl_relaxed(bwr->base + REG_BWR_CAM_RPT_TIMER),
		readl_relaxed(bwr->base + REG_BWR_CAM_DBC_CYC));

	bwr->started = true;

	mutex_unlock(&bwr->op_lock);

	return 0;
}

/* todo: check all csr is zero ? */
static int bwr_stop(struct mtk_bwr_device *bwr)
{
	mutex_lock(&bwr->op_lock);

	writel(0x0, bwr->base + REG_BWR_CAM_SEND_BW);
	writel(0x1, bwr->base + REG_BWR_CAM_SEND_TRIG);

	bwr->started = false;

	pr_info("%s rpt_state: %d\n", __func__,
		readl(bwr->base + REG_BWR_CAM_RPT_CTRL));

	mutex_unlock(&bwr->op_lock);

	return 0;
}

struct mtk_bwr_device *mtk_cam_isp8s_bwr_get_dev(struct platform_device *pdev)
{
	struct device_node *node;
	struct platform_device *bwr_pdev;
	struct mtk_bwr_device *bwr;

	node = of_parse_phandle(pdev->dev.of_node, "mediatek,cam-bwr", 0);
	if (!node) {
		pr_info("%s :fail to parse mediatek,cam-bwr\n", __func__);
		return NULL;
	}

	bwr_pdev = of_find_device_by_node(node);
	of_node_put(node);
	if (!bwr_pdev) {
		pr_info("%s :no bwr device\n",  __func__);
		return NULL;
	}

	bwr = dev_get_drvdata(&bwr_pdev->dev);
	if (!bwr) {
		pr_info("%s :no bwr drv data\n",  __func__);
		return NULL;
	}

	return bwr;
}
EXPORT_SYMBOL_GPL(mtk_cam_isp8s_bwr_get_dev);

void mtk_cam_isp8s_bwr_enable(struct mtk_bwr_device *bwr)
{
	if (!bwr)
		return;

	if (pm_runtime_get_sync(bwr->dev) < 0)
		pr_info("%s runtime get fail\n", __func__);
}
EXPORT_SYMBOL_GPL(mtk_cam_isp8s_bwr_enable);

void mtk_cam_isp8s_bwr_disable(struct mtk_bwr_device *bwr)
{
	if (!bwr)
		return;

	pm_runtime_put_sync(bwr->dev);
}
EXPORT_SYMBOL_GPL(mtk_cam_isp8s_bwr_disable);

void mtk_cam_isp8s_bwr_set_chn_bw(struct mtk_bwr_device *bwr,
			  enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi,
			  int srt_r_bw, int srt_w_bw, int hrt_r_bw, int hrt_w_bw, bool clear)
{
	if (!bwr)
		return;

	bwr_set_chn_bw(bwr, engine, axi, srt_r_bw, srt_w_bw, hrt_r_bw, hrt_w_bw, clear);
}
EXPORT_SYMBOL_GPL(mtk_cam_isp8s_bwr_set_chn_bw);

void mtk_cam_isp8s_bwr_set_ttl_bw(struct mtk_bwr_device *bwr,
			  enum BWR_ENGINE_TYPE engine, int srt_emi, int hrt_emi, bool clear)
{
	if (!bwr)
		return;

	bwr_set_emi_bw(bwr, engine, srt_emi, hrt_emi, clear);
}
EXPORT_SYMBOL_GPL(mtk_cam_isp8s_bwr_set_ttl_bw);

void mtk_cam_isp8s_bwr_clr_bw(
	struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi)
{
	if (!bwr)
		return;

	bwr_zero_bw(bwr, engine, axi);
}
EXPORT_SYMBOL_GPL(mtk_cam_isp8s_bwr_clr_bw);

/* hw mode trigger for HRT/SRT */
void mtk_cam_isp8s_bwr_trigger(struct mtk_bwr_device *bwr,
	enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi)
{
	if (!bwr)
		return;

	mutex_lock(&bwr->op_lock);
	if (!bwr || !bwr->started) {
		pr_info("%s: engine:%d %s\n", __func__,
			engine, bwr ?	"BWR is disabled" : "null device");
		mutex_unlock(&bwr->op_lock);
		return;
	}

	writel(0x1, bwr->base +
		REG_BWR_CAM_SRT_R0_SW_QOS_TRIG0 + CHANNEL_OFFSET * axi +
		ENGINE_OFFSET * engine);

	writel(0x1, bwr->base +
		REG_BWR_CAM_HRT_R0_SW_QOS_TRIG0 + CHANNEL_OFFSET * axi +
		ENGINE_OFFSET * engine);

	writel(0x1, bwr->base +
		REG_BWR_CAM_SRT_EMI_SW_QOS_TRIG + ENGINE_OFFSET * engine);

	writel(0x1, bwr->base +
		REG_BWR_CAM_HRT_EMI_SW_QOS_TRIG + ENGINE_OFFSET * engine);

	mutex_unlock(&bwr->op_lock);
}

void mtk_cam_isp8s_bwr_dbg_dump(struct mtk_bwr_device *bwr)
{
	int engine = 0, axi = 0;

	if (!bwr)
		return;

	mutex_lock(&bwr->op_lock);
	if (!bwr || !bwr->started) {
		pr_info("%s: %s\n", __func__,
			bwr ?	"BWR is disabled" : "null device");
		mutex_unlock(&bwr->op_lock);
		return;
	}

	for (engine = 0 ; engine < ENGINE_NUM; engine++) {
		pr_info("%s: %s : SRT_EMI/HRT_EMI : 0x%x:0x%x, 0x%x:0x%x\n",
			__func__, str_engine(engine),
			REG_BWR_CAM_SRT_EMI_ENG_BW0 + ENGINE_OFFSET * engine,
			readl(bwr->base + REG_BWR_CAM_SRT_EMI_ENG_BW0 +
				ENGINE_OFFSET * engine),
			REG_BWR_CAM_HRT_EMI_ENG_BW0 + ENGINE_OFFSET * engine,
			readl(bwr->base + REG_BWR_CAM_HRT_EMI_ENG_BW0 +
				ENGINE_OFFSET * engine));

		for (axi = 0 ; axi < BWR_AXI_PORT_NUM; axi++) {
			pr_info("%s: %s %s : SRT_R/SRT_W/HRT_R/HRT_W : 0x%x:0x%x, 0x%x:0x%x, 0x%x:0x%x, 0x%x:0x%x\n",
				__func__, str_engine(engine), str_axi_port(axi),
				REG_BWR_CAM_SRT_R0_ENG_BW0_0 + CHANNEL_OFFSET * axi + ENGINE_OFFSET * engine,
				readl(bwr->base + REG_BWR_CAM_SRT_R0_ENG_BW0_0 +
					CHANNEL_OFFSET * axi + ENGINE_OFFSET * engine),
				REG_BWR_CAM_SRT_W0_ENG_BW0_0 + CHANNEL_OFFSET * axi + ENGINE_OFFSET * engine,
				readl(bwr->base + REG_BWR_CAM_SRT_W0_ENG_BW0_0 +
					CHANNEL_OFFSET * axi + ENGINE_OFFSET * engine),
				REG_BWR_CAM_HRT_R0_ENG_BW0_0 + CHANNEL_OFFSET * axi + ENGINE_OFFSET * engine,
				readl(bwr->base + REG_BWR_CAM_HRT_R0_ENG_BW0_0 +
					CHANNEL_OFFSET * axi + ENGINE_OFFSET * engine),
				REG_BWR_CAM_HRT_W0_ENG_BW0_0 + CHANNEL_OFFSET * axi + ENGINE_OFFSET * engine,
				readl(bwr->base + REG_BWR_CAM_HRT_W0_ENG_BW0_0 +
					CHANNEL_OFFSET * axi + ENGINE_OFFSET * engine));
		}
	}
	mutex_unlock(&bwr->op_lock);
}

static int mtk_bwr_component_bind(struct device *dev, struct device *master,
				  void *data)
{
	struct mtk_bwr_device *drvdata = dev_get_drvdata(dev);
	struct mtk_cam_device *cam_dev = data;

	cam_dev->bwr = drvdata;
	return 0;
}

static void mtk_bwr_component_unbind(struct device *dev, struct device *master,
				     void *data)
{
}

static const struct component_ops mtk_bwr_component_ops = {
	.bind = mtk_bwr_component_bind,
	.unbind = mtk_bwr_component_unbind,
};

static int mtk_bwr_of_probe(struct platform_device *pdev,
					struct mtk_bwr_device *drvdata)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct platform_device *vcore_pdev;
	struct device_node *node;
	struct device_link *link;
	int i, clks;

	dev_info(dev, "bwr probe\n");

	/* base register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "bwr_base");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	drvdata->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(drvdata->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(drvdata->base);
	}

	clks = of_count_phandle_with_args(pdev->dev.of_node, "clocks",
			"#clock-cells");

	drvdata->num_clks = (clks <= 0) ? 0 : clks;
	dev_info(dev, "clk_num:%d\n", drvdata->num_clks);

	if (drvdata->num_clks) {
		drvdata->clks = devm_kcalloc(dev,
					     drvdata->num_clks, sizeof(*drvdata->clks),
					     GFP_KERNEL);
		if (!drvdata->clks)
			return -ENOMEM;
	}

	for (i = 0; i < drvdata->num_clks; i++) {
		drvdata->clks[i] = of_clk_get(pdev->dev.of_node, i);
		if (IS_ERR(drvdata->clks[i])) {
			dev_info(dev, "failed to get clk %d\n", i);
			return -ENODEV;
		}
	}

	node = of_parse_phandle(
				pdev->dev.of_node, "mediatek,camisp-vcore", 0);
	if (!node) {
		dev_info(dev, "failed to get camisp vcore phandle\n");
		return -ENODEV;
	}

	vcore_pdev = of_find_device_by_node(node);
	if (WARN_ON(!vcore_pdev)) {
		of_node_put(node);
		dev_info(dev, "failed to get camisp vcore pdev\n");
		return -ENODEV;
	}
	of_node_put(node);

	link = device_link_add(dev, &vcore_pdev->dev,
					DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
	if (!link)
		dev_info(dev, "unable to link cam vcore\n");

	dev_info(dev, "bwr probe done\n");

	return 0;
}

static int mtk_bwr_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_bwr_device *drvdata;
	int ret;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = dev;
	dev_set_drvdata(dev, drvdata);

	ret = mtk_bwr_of_probe(pdev, drvdata);
	if (ret) {
		dev_info(dev, "mtk_bwr_of_probe failed\n");
		return ret;
	}

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_bwr_component_ops);

	mutex_init(&drvdata->op_lock);
	drvdata->started = false;

	return ret;
}

static void mtk_bwr_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_bwr_device *bwr = dev_get_drvdata(dev);
	int i;

	pm_runtime_disable(dev);

	component_del(dev, &mtk_bwr_component_ops);

	for (i = 0; i < bwr->num_clks; i++)
		clk_put(bwr->clks[i]);
}

static int mtk_bwr_runtime_suspend(struct device *dev)
{
	struct mtk_bwr_device *bwr = dev_get_drvdata(dev);
	int i;

	bwr_clr_default(bwr);
	bwr_stop(bwr);

	for (i = bwr->num_clks - 1; i >= 0; i--)
		clk_disable_unprepare(bwr->clks[i]);

	return 0;
}

static int mtk_bwr_runtime_resume(struct device *dev)
{
	struct mtk_bwr_device *bwr = dev_get_drvdata(dev);
	int i, ret;

	for (i = 0; i < bwr->num_clks; i++) {
		ret = clk_prepare_enable(bwr->clks[i]);
		if (ret) {
			dev_info(dev, "enable failed at clk #%d, ret = %d\n",
				 i, ret);
			i--;
			while (i >= 0)
				clk_disable_unprepare(bwr->clks[i--]);

			return ret;
		}
	}

	bwr_start(bwr);
	bwr_set_default(bwr);

	return 0;
}

static const struct dev_pm_ops mtk_bwr_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_bwr_runtime_suspend, mtk_bwr_runtime_resume, NULL)
};

static const struct of_device_id mtk_cam_bwr_of_ids[] = {
	{.compatible = "mediatek,mt6993-cam-bwr",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_cam_bwr_of_ids);

struct platform_driver mtk_cam_bwr_driver = {
	.probe   = mtk_bwr_probe,
	.remove  = mtk_bwr_remove,
	.driver  = {
		.name  = "mtk-cam bwr",
		.of_match_table = of_match_ptr(mtk_cam_bwr_of_ids),
		.pm     = &mtk_bwr_pm_ops,
	}
};
