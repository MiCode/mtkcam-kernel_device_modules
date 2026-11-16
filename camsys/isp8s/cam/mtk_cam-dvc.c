// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include "mtk_cam.h"
#include "mtk_cam-reg_utils.h"
#include "mtk_cam-dvc.h"
#include "mtk_cam-dvc_regs.h"
#include "mtk_cam-debug_option.h"

static int dvc_enable = 1;
module_param(dvc_enable, int, 0644);
MODULE_PARM_DESC(dvc_enable, "debug dvc enable");

static int dvc_dump;
module_param(dvc_dump, int, 0644);
MODULE_PARM_DESC(dvc_dump, "debug dvc enable");

u32 is_dvc_support(void)
{
	return dvc_enable && GET_PLAT_HW(dvc_support);
}

static void __iomem *get_dvc_base(struct mtk_camsys_dvc *dvc, u32 raw_id)
{
	switch(raw_id) {
	case RAW_A:
		return dvc->dvc_a_base;
	case RAW_B:
		return dvc->dvc_b_base;
	case RAW_C:
		return dvc->dvc_c_base;
	default:
		return NULL;
	}
}

#define DVC_DEFAULT_CLK 208
#define DVC_TIME_STAMP 1
static u32 dvc_cnt_rate_khz(void)
{
	return DVC_DEFAULT_CLK * 1000 / ((DVC_TIME_STAMP + 1) * 2);
}

static irqreturn_t mtk_cam_irq_dvc(int irq, void *data)
{
	struct mtk_camsys_dvc *dvc = (struct mtk_camsys_dvc *)data;
	struct device *dev = dvc->dev;
	unsigned int irq_status = 0;

	irq_status = readl_relaxed(dvc->top_base + REG_DVC_CAM_TOP_INT_STATUS);

	dev_info(dev, "dvc INT 0x%x\n", irq_status);

	return IRQ_HANDLED;
}

#define DVC_INIT_OPP 1
void mtk_cam_dvc_top_enable(struct mtk_camsys_dvc *dvc)
{
	u32 val = 0;

	if (!is_dvc_support())
		return;

	/* default opp index */
	val = readl(dvc->top_base + REG_DVC_CAM_TOP_CTL);
	SET_FIELD(&val, DVC_CAM_TOP_INIT_OPP_VAL, DVC_INIT_OPP);
	SET_FIELD(&val, DVC_CAM_TOP_DVC_EN, 1);
	writel(val, dvc->top_base + REG_DVC_CAM_TOP_CTL);

	/* ack timeout = 1 ms */
	writel(dvc_cnt_rate_khz() * 1,
		dvc->top_base + REG_DVC_CAM_TOP_DVFSRC_ACK_TMOUT);

	enable_irq(dvc->irq);

	pr_info("%s: REG_DVC_CAM_TOP_CTL:0x%x REG_DVC_CAM_TOP_DVFSRC_ACK_TMOUT:0x%x REG_DVC_CAM_TOP_OPP_STATUS:0x%x\n",
		__func__,
		readl(dvc->top_base + REG_DVC_CAM_TOP_CTL),
		readl(dvc->top_base + REG_DVC_CAM_TOP_DVFSRC_ACK_TMOUT),
		readl(dvc->top_base + REG_DVC_CAM_TOP_OPP_STATUS));
}

void mtk_cam_dvc_top_disable(struct mtk_camsys_dvc *dvc)
{
	u32 val = 0;
	int ret, ack;

	if (!is_dvc_support())
		return;

	val = readl(dvc->top_base + REG_DVC_CAM_TOP_CTL);
	SET_FIELD(&val, DVC_CAM_TOP_RAW_A_HW_OPP_EN, 0);
	/* for reset clk level */
	SET_FIELD(&val, DVC_CAM_TOP_RAW_A_SW_OPP_EN, 1);
	SET_FIELD(&val, DVC_CAM_TOP_RAW_B_HW_OPP_EN, 0);
	SET_FIELD(&val, DVC_CAM_TOP_RAW_B_SW_OPP_EN, 0);
	SET_FIELD(&val, DVC_CAM_TOP_RAW_C_HW_OPP_EN, 0);
	SET_FIELD(&val, DVC_CAM_TOP_RAW_C_SW_OPP_EN, 0);
	SET_FIELD(&val, DVC_CAM_TOP_INIT_OPP_VAL, 0);
	writel(val, dvc->top_base + REG_DVC_CAM_TOP_CTL);

	val = 0;
	SET_FIELD(&val, DVC_CAM_SW_REQ, 1);
	SET_FIELD(&val, DVC_CAM_SW_OPP_VAL, 0);
	writel(val, dvc->dvc_a_base + REG_DVC_CAM_SW_VOTER);

	/* wait dvfssrc protocal idle */
	ret = readx_poll_timeout(readl, dvc->top_base + REG_DVC_CAM_TOP_DBG_DEBUG_DATA,
				 ack,
				 (ack & 0xf) == 0, 10 /* delay, us */,
				 1000 /* timeout, us */);
	if (ret < 0)
		pr_info("%s: error: timeout!, (ack 0x%x)\n", __func__, ack);

	SET_FIELD(&val, DVC_CAM_TOP_DVC_EN, 0);
	writel(val, dvc->top_base + REG_DVC_CAM_TOP_CTL);

	disable_irq(dvc->irq);

	pr_info("%s: top_opp_stat:0x%x, top_dbg_data:0x%x\n",
		__func__,
		readl(dvc->top_base + REG_DVC_CAM_TOP_OPP_STATUS),
		readl(dvc->top_base + REG_DVC_CAM_TOP_DBG_DEBUG_DATA));
}

static void mtk_cam_dvc_top_hwmode(struct mtk_camsys_dvc *dvc,
		u32 raw_id, u32 is_hwmode, u32 is_init)
{
	u32 val = 0;

	mutex_lock(&dvc->op_lock);
	dvc->hwmode[raw_id] = is_hwmode;
	val = readl(dvc->top_base + REG_DVC_CAM_TOP_CTL);
	switch(raw_id) {
	case RAW_A:
		SET_FIELD(&val, DVC_CAM_TOP_RAW_A_HW_OPP_EN, is_init ? 1 : 0);
		break;
	case RAW_B:
		SET_FIELD(&val, DVC_CAM_TOP_RAW_B_HW_OPP_EN, is_init ? 1 : 0);
		break;
	case RAW_C:
		SET_FIELD(&val, DVC_CAM_TOP_RAW_C_HW_OPP_EN, is_init ? 1 : 0);
		break;
	default:
		break;
	}
	writel(val, dvc->top_base + REG_DVC_CAM_TOP_CTL);
	mutex_unlock(&dvc->op_lock);
}

#define DVC_OPP_HIGH_DLY_BEFORE_MS    1
#define DVC_MAX_OPP_HIGH_TM_BEFORE_MS 2
void mtk_cam_dvc_init(struct mtk_camsys_dvc *dvc,
			u32 raw_id, u32 dvc_hwmode, u32 is_srt, u32 opp, u32 frm_time_ms)
{
	void __iomem *base = get_dvc_base(dvc, raw_id);
	u32 val = 0;
	int ret, ack;

	if (!is_dvc_support())
		return;

	if (!base)
		return;

	mtk_cam_dvc_top_hwmode(dvc, raw_id, dvc_hwmode, 1);
	writel(DVC_TIME_STAMP, base + REG_DVC_CAM_HW_DVC_TIME_STAMP);

	SET_FIELD(&val, DVC_CAM_HW_DVC_MD, is_srt ? 1 : 0);
	SET_FIELD(&val, DVC_CAM_HW_DVC_HRT_SRC, is_srt ? 1 : 0);
	writel((frm_time_ms - DVC_OPP_HIGH_DLY_BEFORE_MS) * dvc_cnt_rate_khz(),
		base + REG_DVC_CAM_HW_DVC_OPP_HIGH_DLY);
	writel((frm_time_ms - DVC_MAX_OPP_HIGH_TM_BEFORE_MS) * dvc_cnt_rate_khz(),
		base + REG_DVC_CAM_HW_DVC_MAX_OPP_HIGH_TM);

	SET_FIELD(&val, DVC_CAM_HW_DVC_OPP_HIGH, opp);
	SET_FIELD(&val, DVC_CAM_HW_DVC_OPP_LOW, dvc_hwmode ? 1 : opp);
	SET_FIELD(&val, DVC_CAM_HW_DVC_TRI, frm_time_ms ? 1 : 0);
	writel(val, base + REG_DVC_CAM_HW_DVC_CTL);

	/* wait dvfssrc protocal idle */
	ret = readx_poll_timeout(readl, dvc->top_base + REG_DVC_CAM_TOP_DBG_DEBUG_DATA,
				 ack,
				 (ack & 0xf) == 0, 10 /* delay, us */,
				 1000 /* timeout, us */);
	if (ret < 0)
		pr_info("%s: error: timeout!, (ack 0x%x)\n", __func__, ack);

	val = readl(dvc->top_base + REG_DVC_CAM_TOP_OPP_STATUS) & 0xf;
	if (val < opp)
		pr_info("%s: error: current < target (%d, %d)", __func__, val, opp);

	pr_info("%s: raw_id:%d dvc_hwmode:%d opp:%d, frm_time_ms:%d, time_stamp:0x%x, dvc_ctl:0x%x opp_high_dly:0x%x, opp_high_dly:0x%x\n",
		__func__, raw_id, dvc_hwmode, opp, frm_time_ms,
		readl(base + REG_DVC_CAM_HW_DVC_TIME_STAMP),
		readl(base + REG_DVC_CAM_HW_DVC_CTL),
		readl(base + REG_DVC_CAM_HW_DVC_OPP_HIGH_DLY),
		readl(base + REG_DVC_CAM_HW_DVC_MAX_OPP_HIGH_TM));
}

void mtk_cam_dvc_unint(struct mtk_camsys_dvc *dvc, u32 raw_id)
{
	void __iomem *base = get_dvc_base(dvc, raw_id);

	if (!is_dvc_support())
		return;

	if (!base)
		return;

	writel(0, base + REG_DVC_CAM_HW_DVC_OPP_HIGH_DLY);
	writel(0, base + REG_DVC_CAM_HW_DVC_MAX_OPP_HIGH_TM);
	writel(0, base + REG_DVC_CAM_HW_DVC_CTL);
	mtk_cam_dvc_top_hwmode(dvc, raw_id, 0, 0);

	pr_info("%s: raw_id:%d time_stamp:0x%x, dvc_ctl:0x%x dbg_data:0x%x, top_opp_stat:0x%x, top_dbg_data:0x%x\n",
		__func__, raw_id,
		readl(base + REG_DVC_CAM_HW_DVC_TIME_STAMP),
		readl(base + REG_DVC_CAM_HW_DVC_CTL),
		readl(base + REG_DVC_CAM_DBG_DEBUG_DATA),
		readl(dvc->top_base + REG_DVC_CAM_TOP_OPP_STATUS),
		readl(dvc->top_base + REG_DVC_CAM_TOP_DBG_DEBUG_DATA));
}

#define MAX_DVFS_OPP_IDX       4
#define BOOST_DVFS_OPP         2
int mtk_cam_dvc_vote(struct mtk_camsys_dvc *dvc, u32 raw_id, u32 opp, bool boost)
{
	void __iomem *base = get_dvc_base(dvc, raw_id);
	u32 boost_opp = boost ? min(opp + BOOST_DVFS_OPP, MAX_DVFS_OPP_IDX) : opp;
	u32 hw_mode = dvc->hwmode[raw_id];
	u32 val = 0;

	if (!is_dvc_support())
		return 0;

	if (!base)
		return 0;

	val = readl(base + REG_DVC_CAM_HW_DVC_CTL);
	SET_FIELD(&val, DVC_CAM_HW_DVC_OPP_HIGH, boost_opp);
	SET_FIELD(&val, DVC_CAM_HW_DVC_OPP_LOW, (hw_mode && !boost) ? 1 : boost_opp);
	writel(val, base + REG_DVC_CAM_HW_DVC_CTL);

	pr_info("%s: raw_id:%d opp:%d, boost_opp:%d, boost:%d, hw_mode:%d, dvc_ctl:0x%x dbg_data:0x%x top_dbg_data:0x%x\n",
		__func__, raw_id, opp, boost_opp, boost, hw_mode,
		readl(base + REG_DVC_CAM_HW_DVC_CTL),
		readl(base + REG_DVC_CAM_DBG_DEBUG_DATA),
		readl(dvc->top_base + REG_DVC_CAM_TOP_DBG_DEBUG_DATA));

	return 0;
}

int mtk_cam_dvc_dbg_dump(struct mtk_camsys_dvc *dvc, u32 raw_id)
{
	void __iomem *base = get_dvc_base(dvc, raw_id);

	if (!dvc_dump)
		return 0;

	mdelay(1);
	pr_info("%s: raw_id:%d time_stamp:0x%x, sw_vote:0x%x, dvc_ctl:0x%x opp_high_dly:0x%x, max_opp_high_dly:0x%x dbg_data:0x%x\n",
		__func__, raw_id,
		readl(base + REG_DVC_CAM_HW_DVC_TIME_STAMP),
		readl(base + REG_DVC_CAM_SW_VOTER),
		readl(base + REG_DVC_CAM_HW_DVC_CTL),
		readl(base + REG_DVC_CAM_HW_DVC_OPP_HIGH_DLY),
		readl(base + REG_DVC_CAM_HW_DVC_MAX_OPP_HIGH_TM),
		readl(base + REG_DVC_CAM_DBG_DEBUG_DATA));
	pr_info("%s: top_ctrl:0x%x top_opp_stat:0x%x top_dbg_data:0x%x\n",
		__func__,
		readl(dvc->top_base + REG_DVC_CAM_TOP_CTL),
		readl(dvc->top_base + REG_DVC_CAM_TOP_OPP_STATUS),
		readl(dvc->top_base + REG_DVC_CAM_TOP_DBG_DEBUG_DATA));

	return 0;
}

int mtk_cam_dvc_probe(struct platform_device *pdev, struct mtk_camsys_dvc *dvc)
{
	struct device *dev = &pdev->dev;
	int ret;

	dev_info(dev, "camsys dvc probe\n");

	/* base register */
	dvc->top_base = devm_platform_ioremap_resource_byname(pdev, "dvc_top");
	if (IS_ERR(dvc->top_base)) {
		dev_err(dev, "%s: failed to map adlwr_base\n", __func__);
		dvc->top_base = NULL;
	}

	dvc->dvc_a_base = devm_platform_ioremap_resource_byname(pdev, "dvc_a");
	if (IS_ERR(dvc->dvc_a_base)) {
		dev_err(dev, "%s: failed to map adlwr_base\n", __func__);
		dvc->dvc_a_base = NULL;
	}

	dvc->dvc_b_base = devm_platform_ioremap_resource_byname(pdev, "dvc_b");
	if (IS_ERR(dvc->dvc_b_base)) {
		dev_err(dev, "%s: failed to map adlwr_base\n", __func__);
		dvc->dvc_b_base = NULL;
	}

	dvc->dvc_c_base = devm_platform_ioremap_resource_byname(pdev, "dvc_c");
	if (IS_ERR(dvc->dvc_c_base)) {
		dev_err(dev, "%s: failed to map adlwr_base\n", __func__);
		dvc->dvc_c_base = NULL;
	}

	dvc->irq = platform_get_irq_byname(pdev, "dvc");
	if (dvc->irq < 0) {
		dev_info(dev, "%s: failed to get irq\n", __func__);
		return -ENODEV;
	}

	ret = devm_request_irq(dev, dvc->irq,
			mtk_cam_irq_dvc, IRQF_NO_AUTOEN, dev_name(dev), dvc);
	if (ret) {
		dev_info(dev, "%s: failed to request irq=%d\n", __func__, dvc->irq);
		return ret;
	}
	dev_info(dev, "%s: registered irq=%d\n", __func__, dvc->irq);

	mutex_init(&dvc->op_lock);

	return ret;
}
