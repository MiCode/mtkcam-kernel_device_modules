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
	ENGINE_MRAW,
	ENGINE_CAM_MAIN,
	ENGINE_CAMSV_B,
	ENGINE_CAMSV_A,
	ENGINE_DPE,
	ENGINE_PDA,
	ENGINE_CAMSV_OTHER,
	ENGINE_UISP,
	ENGINE_NUM,
};

/* do not modify order */
enum BWR_AXI_PORT {
	DISP_PORT = 0,
	MDP0_PORT,
	MDP1_PORT,
	SYS_PORT,
	NUM_PORT,
};

struct mtk_bwr_device {
	void __iomem *base;
	struct mutex op_lock;
};

static inline int get_axi_port(int raw_id, int is_raw)
{
	switch(raw_id) {
	case 0:
		return is_raw ? DISP_PORT : MDP0_PORT;
	case 1:
		return is_raw ? MDP0_PORT : DISP_PORT;
	case 2:
		return is_raw ? DISP_PORT : MDP0_PORT;
	default:
		return 0;
	}
}
static inline int get_sv_axi_port(int sv_id)
{
	switch(sv_id) {
	case 0:
		return MDP1_PORT;
	case 1:
		return MDP0_PORT;
	default:
		return DISP_PORT;
	}
}
static inline int get_mraw_axi_port(int mraw_id)
{
	switch(mraw_id) {
	case 0:
		return DISP_PORT;
	case 1:
		return MDP0_PORT;
	case 2:
		return DISP_PORT;
	case 3:
		return MDP0_PORT;
	default:
		return 0;
	}
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
	default:
		return ENGINE_CAMSV_OTHER;
	}
}

#ifdef CAMSYS_BWR_SUPPORT

int mtk_cam_bwr_probe(struct device *dev, struct mtk_bwr_device *bwr);

void mtk_cam_bwr_enable(struct mtk_bwr_device *bwr);

void mtk_cam_bwr_disable(struct mtk_bwr_device *bwr);

/* unit : MB/s */
void mtk_cam_bwr_set_chn_bw(
		struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi,
		int srt_r_bw, int srt_w_bw, int hrt_r_bw, int hrt_w_bw, bool clear);

/* unit : MB/s */
void mtk_cam_bwr_set_ttl_bw(
		struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine,
		int srt_ttl, int hrt_ttl, bool clear);

void mtk_cam_bwr_clr_bw(
	struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi);

void mtk_cam_bwr_trigger(
	struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi);

void mtk_cam_bwr_dbg_dump(struct mtk_bwr_device *bwr);

#else
static inline int mtk_cam_bwr_probe(struct device *dev, struct mtk_bwr_device *bwr){ return 0; }
static inline void mtk_cam_bwr_enable(struct mtk_bwr_device *bwr){}
static inline void mtk_cam_bwr_disable(struct mtk_bwr_device *bwr){}
static inline void mtk_cam_bwr_set_chn_bw(
		struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi,
		int srt_r_bw, int srt_w_bw, int hrt_r_bw, int hrt_w_bw, bool clear){}
static inline void mtk_cam_bwr_set_ttl_bw(
		struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine,
		int srt_ttl, int hrt_ttl, bool clear){}

static inline void mtk_cam_bwr_clr_bw(
	struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi){}

static inline void mtk_cam_bwr_trigger(
	struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi){}

static inline void mtk_cam_bwr_dbg_dump(struct mtk_bwr_device *bwr){}
#endif

#endif /*__MTK_CAM_BWR_H*/
