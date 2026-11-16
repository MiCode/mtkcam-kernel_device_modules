// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/iopoll.h>

#include "mtk_cam.h"
#include "mtk_cam-reg_utils.h"
#include "mtk_cam-topctrl_regs.h"
#include "mtk_cam-debug_option.h"

static int cam_remap_qos_en = 1;
module_param(cam_remap_qos_en, int, 0644);
MODULE_PARM_DESC(cam_remap_qos_en, "1 : active 16 level qos");

static int cam_remap_hrt_tbl = 0xCCE0CCE;
module_param(cam_remap_hrt_tbl, int, 0644);
MODULE_PARM_DESC(cam_remap_hrt_tbl, "hrt remap lut");

static int cam_remap_srt_tbl = 0x0990199;
module_param(cam_remap_srt_tbl, int, 0644);
MODULE_PARM_DESC(cam_remap_srt_tbl, "srt remap lut");

static int cam_coh_req_swmode;
module_param(cam_coh_req_swmode, int, 0644);
MODULE_PARM_DESC(cam_coh_req_swmode, "1 : active sw mode");

static int cam_wal20_mode;
module_param(cam_wal20_mode, int, 0644);
MODULE_PARM_DESC(cam_wal20_mode, "-1 : legecy mode, 0: swmode 1: hwmode");

static bool qos_remap_enable(void)
{
	return cam_remap_qos_en && GET_PLAT_HW(qos_remap_support);
}

/* todo : camsv, with on/off, with debug cmd */
void mtk_cam_main_halt(struct mtk_raw_device *raw, int is_srt)
{
	struct mtk_cam_device *cam = raw->cam;
	u32 reg_raw_urgent, reg_yuv_urgent;

	switch (raw->id) {
	case RAW_A:
		reg_raw_urgent = REG_CAM_MAIN_HALT7_EN;
		reg_yuv_urgent = REG_CAM_MAIN_HALT8_EN;
		break;
	case RAW_B:
		reg_raw_urgent = REG_CAM_MAIN_HALT9_EN;
		reg_yuv_urgent = REG_CAM_MAIN_HALT10_EN;
		break;
	case RAW_C:
		reg_raw_urgent = REG_CAM_MAIN_HALT11_EN;
		reg_yuv_urgent = REG_CAM_MAIN_HALT12_EN;
		break;
	default:
		dev_info(raw->dev, "%s: unknown raw id %d\n", __func__, raw->id);
		return;
	}

	if (is_srt) {
		writel_relaxed(0x0, cam->base + reg_raw_urgent);
		writel_relaxed(0x0, cam->base + reg_yuv_urgent);
	} else {
		writel_relaxed(0x400, cam->base + reg_raw_urgent);
		writel_relaxed(0x1, cam->base + reg_yuv_urgent);
	}

	pr_info("%s: id:%d is_srt:%d, 0x%x:0x%x, 0x%x:0x%x\n",
		__func__, raw->id, is_srt,
		reg_raw_urgent, readl(cam->base + reg_raw_urgent),
		reg_yuv_urgent, readl(cam->base + reg_yuv_urgent));
}

void mtk_cam_main_sv_halt(struct mtk_cam_device *cam)
{
	/* camsv default urgent enable */
	writel_relaxed(0x3, cam->base + REG_CAM_MAIN_HALT1_EN);
	writel_relaxed(0x3, cam->base + REG_CAM_MAIN_HALT2_EN);
	writel_relaxed(0x3, cam->base + REG_CAM_MAIN_HALT3_EN);
	writel_relaxed(0x3, cam->base + REG_CAM_MAIN_HALT4_EN);
	writel_relaxed(0x3, cam->base + REG_CAM_MAIN_HALT5_EN);
	writel_relaxed(0x7, cam->base + REG_CAM_MAIN_HALT6_EN);

	if (CAM_DEBUG_ENABLED(V4L2))
		pr_info("%s: 0x%x:0x%x, 0x%x:0x%x, 0x%x:0x%x, 0x%x:0x%x, 0x%x:0x%x, 0x%x:0x%x\n",
			__func__,
			REG_CAM_MAIN_HALT1_EN, readl(cam->base + REG_CAM_MAIN_HALT1_EN),
			REG_CAM_MAIN_HALT2_EN, readl(cam->base + REG_CAM_MAIN_HALT2_EN),
			REG_CAM_MAIN_HALT3_EN, readl(cam->base + REG_CAM_MAIN_HALT3_EN),
			REG_CAM_MAIN_HALT4_EN, readl(cam->base + REG_CAM_MAIN_HALT4_EN),
			REG_CAM_MAIN_HALT5_EN, readl(cam->base + REG_CAM_MAIN_HALT5_EN),
			REG_CAM_MAIN_HALT6_EN, readl(cam->base + REG_CAM_MAIN_HALT6_EN));
}

void mtk_cam_main_dbg_dump(struct mtk_cam_device *cam)
{
	pr_info("%s: halt_en:0x%x/0x%x/0x%x/0x%x, 0x%x/0x%x/0x%x/0x%x, 0x%x/0x%x/0x%x/0x%x\n",
		__func__,
		readl(cam->base + REG_CAM_MAIN_HALT1_EN), readl(cam->base + REG_CAM_MAIN_HALT2_EN),
		readl(cam->base + REG_CAM_MAIN_HALT3_EN), readl(cam->base + REG_CAM_MAIN_HALT4_EN),
		readl(cam->base + REG_CAM_MAIN_HALT5_EN), readl(cam->base + REG_CAM_MAIN_HALT6_EN),
		readl(cam->base + REG_CAM_MAIN_HALT7_EN), readl(cam->base + REG_CAM_MAIN_HALT8_EN),
		readl(cam->base + REG_CAM_MAIN_HALT9_EN), readl(cam->base + REG_CAM_MAIN_HALT10_EN),
		readl(cam->base + REG_CAM_MAIN_HALT11_EN), readl(cam->base + REG_CAM_MAIN_HALT12_EN));
}

/* todo : with on/off, with debug cmd */
void mtk_cam_vcore_qos_remap(struct mtk_raw_device *raw, int is_srt)
{
	struct mtk_cam_device *cam = raw->cam;
	u32 val = 0;

	if (!qos_remap_enable())
		return;

	val = is_srt ? cam_remap_srt_tbl : cam_remap_hrt_tbl;
	/* raw/yuv */
	switch(raw->id) {
	case RAW_A:
		/* raw-a lut */
		SET_FIELD(&val, CAM_VCORE_sub_comm0_0_armmqos_en, 1);
		SET_FIELD(&val, CAM_VCORE_sub_comm0_0_awmmqos_en, 1);
		writel(val, cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM0_0);
		/* yuv-a lut */
		SET_FIELD(&val, CAM_VCORE_sub_comm2_0_armmqos_en, 1);
		SET_FIELD(&val, CAM_VCORE_sub_comm2_0_awmmqos_en, 1);
		writel(val, cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM2_0);

		if (CAM_DEBUG_ENABLED(MMQOS))
			pr_info("%s: raw-a remap: 0x%x/0x%x\n", __func__,
				readl(cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM0_0),
				readl(cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM2_0));
	break;
	case RAW_B:
		/* raw-b lut */
		SET_FIELD(&val, CAM_VCORE_sub_comm1_0_armmqos_en, 1);
		SET_FIELD(&val, CAM_VCORE_sub_comm1_0_awmmqos_en, 1);
		writel(val, cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM1_0);

		/* yuv-b lut */
		SET_FIELD(&val, CAM_VCORE_sub_comm0_1_armmqos_en, 1);
		SET_FIELD(&val, CAM_VCORE_sub_comm0_1_awmmqos_en, 1);
		writel(val, cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM0_1);

		if (CAM_DEBUG_ENABLED(MMQOS))
			pr_info("%s: raw-b remap: 0x%x/0x%x\n", __func__,
				readl(cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM1_0),
				readl(cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM0_1));
	break;
	case RAW_C:
		/* raw-c lut */
		SET_FIELD(&val, CAM_VCORE_sub_comm3_0_armmqos_en, 1);
		SET_FIELD(&val, CAM_VCORE_sub_comm3_0_awmmqos_en, 1);
		writel(val, cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM3_0);

		/* yuv-c lut */
		SET_FIELD(&val, CAM_VCORE_sub_comm1_1_armmqos_en, 1);
		SET_FIELD(&val, CAM_VCORE_sub_comm1_1_awmmqos_en, 1);
		writel(val, cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM1_1);

		if (CAM_DEBUG_ENABLED(MMQOS))
			pr_info("%s: raw-c remap: 0x%x/0x%x\n", __func__,
				readl(cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM3_0),
				readl(cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM1_1));
	break;
	default:
		pr_info("%s: unsupport id:%d\n", __func__, raw->id);
	break;
	}
}

void mtk_cam_vcore_sv_qos_remap(struct mtk_cam_device *cam)
{
	u32 val = 0;

	if (!qos_remap_enable())
		return;

	val = cam_remap_hrt_tbl | 0x10001000;
	writel(val, cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM0_2);
	writel(val, cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM1_2);
	writel(val, cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM3_1);
	writel(val, cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM2_1);
	writel(val, cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM2_2);
	writel(val, cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM0_3);

	if (CAM_DEBUG_ENABLED(V4L2))
		pr_info("%s: camsv remap: 0x%x/0x%x/0x%x/0x%x/0x%x/0x%x\n", __func__,
			readl(cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM0_2),
			readl(cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM1_2),
			readl(cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM3_1),
			readl(cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM2_1),
			readl(cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM2_2),
			readl(cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM0_3));
}

void mtk_cam_vcore_ccu_qos_remap(struct mtk_cam_device *cam)
{
	u32 val = 0;

	if (!qos_remap_enable())
		return;

	val = cam_remap_srt_tbl;
	SET_FIELD(&val, CAM_VCORE_sub_comm1_3_armmqos_en, 1);
	SET_FIELD(&val, CAM_VCORE_sub_comm1_3_awmmqos_en, 1);
	writel(val, cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM1_3);

	if (CAM_DEBUG_ENABLED(V4L2))
		pr_info("%s: ccu remap: 0x%x\n", __func__,
			readl(cam->vcore_base + REG_CAM_VCORE_MMQOS_CTRL_SUB_COMM1_3));
}

/* todo: must ? */
void mtk_cam_vcore_ddren(struct mtk_cam_device *cam, int on_off)
{
	int ret, ack = 0;

	writel(on_off ? 0x1fd : 0x0, cam->vcore_base + REG_CAM_VCORE_DDREN_EN);

	if (on_off) {
		ret = readx_poll_timeout(readl, cam->vcore_base + REG_CAM_VCORE_DDREN_ACK,
					 ack,
					 ack & 0x1fc,
					 1 /* delay, us */,
					 2000 /* timeout, us */);
		if (ret < 0) {
			dev_info(cam->dev, "%s: error: timeout!, (ack 0x%x)\n",
				__func__, ack);
			return;
		}
	}

	if (CAM_DEBUG_ENABLED(V4L2))
		dev_info(cam->dev, "%s: ddren:0x%x, ack:0x%x", __func__,
				 readl_relaxed(cam->vcore_base + REG_CAM_VCORE_DDREN_EN),
				 readl_relaxed(cam->vcore_base + REG_CAM_VCORE_DDREN_ACK));
}

void mtk_cam_vcore_coh_req(struct mtk_cam_device *cam)
{
	u32 val = 0;

	if (cam_coh_req_swmode) {
		SET_FIELD(&val, CAM_VCORE_COH_REQ_TCU_SW_MODE, 1);
		SET_FIELD(&val, CAM_VCORE_COH_REQ_TBU_SW_MODE, 1);
		SET_FIELD(&val, CAM_VCORE_COH_REQ_TCU_SW_MODE_SEL, 1);
		SET_FIELD(&val, CAM_VCORE_COH_REQ_TBU_SW_MODE_SEL, 1);
		writel(val, cam->vcore_base + REG_CAM_VCORE_COH_REQ_CTRL_0);
	}

	if (CAM_DEBUG_ENABLED(V4L2))
		pr_info("%s: 0x%x\n", __func__,
			readl(cam->vcore_base + REG_CAM_VCORE_COH_REQ_CTRL_0));
}

#define WLA2P0_DEBOUNCE 0x80006000
void mtk_cam_vcore_wla20(struct mtk_cam_device *cam, int on_off)
{
	u32 val = 0;

	if (on_off) {
		/* set debounce to 15us */
		writel(WLA2P0_DEBOUNCE, cam->vcore_base + REG_CAM_VCORE_WLA2P0_DEBOUNCE);

		val = readl(cam->vcore_base + REG_CAM_VCORE_WLA2P0_CTRL_0);
		SET_FIELD(&val, CAM_VCORE_WLA2P0_LEGACY_MODE, (cam_wal20_mode < 0) ? 1 : 0);
		SET_FIELD(&val, CAM_VCORE_WLA2P0_SW_DDREN_VOTE, (cam_wal20_mode == 0) ? 0 : 1);
		SET_FIELD(&val, CAM_VCORE_WLA2P0_SW_DDREN_VOTE_MUX_EN, (cam_wal20_mode == 0) ? 1 : 0);
		writel(val, cam->vcore_base + REG_CAM_VCORE_WLA2P0_CTRL_0);
	} else {
		/* set to hw default */
		writel(0x7d0000, cam->vcore_base + REG_CAM_VCORE_WLA2P0_CTRL_0);
	}

	if (CAM_DEBUG_ENABLED(V4L2))
		pr_info("%s: on:%d ctrl_0: 0x%x wla20_deb: 0x%x\n", __func__, on_off,
			readl(cam->vcore_base + REG_CAM_VCORE_WLA2P0_CTRL_0),
			readl(cam->vcore_base + REG_CAM_VCORE_WLA2P0_DEBOUNCE));
}

void mtk_cam_vcore_wla20_dbg_dump(struct mtk_cam_device *cam)
{
	pr_info("%s: wla20 status0~4:0x%x/0x%x/0x%x/0x%x/0x%x dbg status0~2:0x%x/0x%x/0x%x\n",
		__func__,
		readl(cam->vcore_base + REG_CAM_VCORE_WLA2P0_STATUS_0),
		readl(cam->vcore_base + REG_CAM_VCORE_WLA2P0_STATUS_1),
		readl(cam->vcore_base + REG_CAM_VCORE_WLA2P0_STATUS_2),
		readl(cam->vcore_base + REG_CAM_VCORE_WLA2P0_STATUS_3),
		readl(cam->vcore_base + REG_CAM_VCORE_WLA2P0_STATUS_4),
		readl(cam->vcore_base + REG_CAM_VCORE_WLA2P0_STATUS_DBG_0),
		readl(cam->vcore_base + REG_CAM_VCORE_WLA2P0_STATUS_DBG_1),
		readl(cam->vcore_base + REG_CAM_VCORE_WLA2P0_STATUS_DBG_2));
}

