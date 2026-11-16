/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_BWR_H
#define __MTK_CAM_BWR_H

#define CAMSYS_BWR_SUPPORT

/* do not modify order */
enum BWR_MODE {
	BWR_SW_MODE = 0,
	BWR_HW_MODE,
};

/* do not modify order */
enum BWR_ENGINE_TYPE {
	ENGINE_SUB_A = 0,
	ENGINE_SUB_B,
	ENGINE_SUB_C,
	ENGINE_MRAW, /* no used */
	ENGINE_CAM_MAIN,
	ENGINE_CAMSV_B,
	ENGINE_CAMSV_A,
	ENGINE_CAMSV_C,
	ENGINE_PDA,
	ENGINE_CAMSV_OTHER,
	ENGINE_UISP,
	ENGINE_NUM,
};

/* do not modify order */
enum BWR_AXI_PORT {
	CAM1_PORT = 0,  /* DISP-0 */
	CAM2_PORT,      /* MDP-0  */
	CAM3_PORT,      /* DISP-1 */
	CAM0_PORT,      /* MDP-1  */
	CAM6_PORT,      /* CAM2SYS */
	BWR_AXI_PORT_NUM,
};


struct mtk_bwr_device {
	struct device *dev;
	void __iomem *base;
	unsigned int num_clks;
	struct clk **clks;
	struct mutex op_lock;
	/* protect by op_lock */
	bool started;
};

static inline int get_axi_port(int raw_id, int is_raw)
{
	switch(raw_id) {
	case 0:
		return is_raw ? CAM0_PORT : CAM2_PORT;
	case 1:
		return is_raw ? CAM1_PORT : CAM0_PORT;
	case 2:
		return is_raw ? CAM3_PORT : CAM1_PORT;
	default:
		return 0;
	}
}

/* todo: need camsv check */
static inline int get_sv_axi_port(int sv_id, int port_id)
{
	switch(sv_id) {
	case 0:
		if (port_id == 0)
			return CAM3_PORT;
		else if (port_id == 1)
			return CAM1_PORT;
		else
			return CAM2_PORT;
	case 1:
		if (port_id == 0)
			return CAM0_PORT;
		else if (port_id == 1)
			return CAM3_PORT;
		else
			return CAM2_PORT;
	case 2:
		if (port_id == 0)
			return CAM1_PORT;
		else
			return CAM0_PORT;
	case 3:
		return CAM2_PORT;
	case 4:
		return CAM2_PORT;
	case 5:
		return CAM0_PORT;
	default:
		return 0;
	}
}
static inline int get_sv_axi_port_num(int sv_id)
{
	if (sv_id <= 2)
		return 3;
	else if (sv_id == 3)
		return 2;
	return 1;
}

/* Jayer no used */
static inline int get_mraw_axi_port(int mraw_id)
{
	return 0;
}

static inline int get_bwr_engine(int raw_id)
{
	switch(raw_id) {
	case 0:
		return ENGINE_SUB_A;
	case 1:
		return ENGINE_SUB_B;
	case 2:
		return ENGINE_SUB_C;
	default:
		return 0;
	}
}

static inline int get_sv_bwr_engine(int sv_id)
{
	switch(sv_id) {
	case 0:
		return ENGINE_CAMSV_A;
	case 1:
		return ENGINE_CAMSV_B;
	case 2:
		return ENGINE_CAMSV_C;
	default:
		return ENGINE_CAMSV_OTHER;
	}
}

extern struct platform_driver mtk_cam_bwr_driver;

#ifdef CAMSYS_BWR_SUPPORT

struct mtk_bwr_device *mtk_cam_isp8s_bwr_get_dev(struct platform_device *pdev);

void mtk_cam_isp8s_bwr_enable(struct mtk_bwr_device *bwr);

void mtk_cam_isp8s_bwr_disable(struct mtk_bwr_device *bwr);

/* unit : MB/s */
void mtk_cam_isp8s_bwr_set_chn_bw(
		struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi,
		int srt_r_bw, int srt_w_bw, int hrt_r_bw, int hrt_w_bw, bool clear);

/* unit : MB/s */
void mtk_cam_isp8s_bwr_set_ttl_bw(
		struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine,
		int srt_ttl, int hrt_ttl, bool clear);

void mtk_cam_isp8s_bwr_clr_bw(
	struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi);

void mtk_cam_isp8s_bwr_trigger(
	struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi);

void mtk_cam_isp8s_bwr_dbg_dump(struct mtk_bwr_device *bwr);

#else
static inline struct mtk_bwr_device *mtk_cam_isp8s_bwr_get_dev(struct platform_device *pdev){ return NULL; }
static inline void mtk_cam_isp8s_bwr_enable(struct mtk_bwr_device *bwr){}
static inline void mtk_cam_isp8s_bwr_disable(struct mtk_bwr_device *bwr){}
static inline void mtk_cam_isp8s_bwr_set_chn_bw(
		struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi,
		int srt_r_bw, int srt_w_bw, int hrt_r_bw, int hrt_w_bw, bool clear){}
static inline void mtk_cam_isp8s_bwr_set_ttl_bw(
		struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine,
		int srt_ttl, int hrt_ttl, bool clear){}

static inline void mtk_cam_isp8s_bwr_clr_bw(
	struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi){}

static inline void mtk_cam_isp8s_bwr_trigger(
	struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi){}

static inline void mtk_cam_isp8s_bwr_dbg_dump(struct mtk_bwr_device *bwr){}
#endif

#endif /*__MTK_CAM_BWR_H*/
