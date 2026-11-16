/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_FMON_H
#define __MTK_CAM_FMON_H

#include <linux/timer.h>
#include <linux/jiffies.h>

enum FMON_INDEX {
	FMON_0 = 0,
	FMON_1,
	FMON_2,
	FMON_3,
	FMON_NUM,
};

enum FMON_RX_MUX {
	FMON_RX_0 = 0,
	FMON_RX_1,
	FMON_RX_2,
	FMON_RX_NUM,
};

enum FMON_TX_MUX {
	FMON_TX_0 = 0,
	FMON_TX_1,
	FMON_TX_2,
	FMON_TX_3,
	FMON_TX_4,
	FMON_TX_NUM,
};

enum FMON_PIPE_INFO {
	FPIPE_NONE = 0,
	FPIPE_RAW,
	FPIPE_SV,
	FPIPE_INFO_NUM,
};

enum FMON_ENGINE {
	FMON_NONE = 0,
	FMON_RAW_A,
	FMON_YUV_A,
	FMON_RAW_B,
	FMON_YUV_B,
	FMON_RAW_C,
	FMON_YUV_C,
	FMON_CAMSV_0,
	FMON_CAMSV_1,
	FMON_CAMSV_2,
	FMON_CAMSV_3,
	FMON_ENG_NUM,
};

enum FMON_SIGNAL {
	FMON_SIG_URGENT = 1 << 0,
	FMON_SIG_START  = 1 << 1,
	FMON_SIG_STOP   = 1 << 2,
};

struct fmon_settings {
	u32 tx_mux;
	u32 rx_mux;
	u32 engine;
	u32 fifo_size;
};

struct mtk_fmon_irq_info {
	int irq_type;
	u64 ts_ns;
	u32 irq_status;
};

struct mtk_fmon_device {
	void __iomem *base;
	int irq;
	int		fifo_size;
	void		*msg_buffer;
	struct kfifo	msg_fifo;
	atomic_t	is_fifo_overflow;

	/* remap raw/yuv/camsv tx mux control */
	void __iomem *raw_a_tx;
	void __iomem *raw_b_tx;
	void __iomem *raw_c_tx;
	void __iomem *yuv_a_tx;
	void __iomem *yuv_b_tx;
	void __iomem *yuv_c_tx;
	void __iomem *camsv_tx_0;
	void __iomem *camsv_tx_1;
	void __iomem *camsv_tx_2;
	void __iomem *camsv_tx_3;

	/* cti/ela */
	void __iomem *ela_ctrl;
	void __iomem *cti_set;
	void __iomem *cti_clear;
	/* debug */
	void __iomem *mminfra_funnel;
	void __iomem *trace_top_funnel;
	void __iomem *mminfra_cti_st;
	void __iomem *apinfra_cti_st;

	struct mutex op_lock;
	enum FMON_PIPE_INFO pipes[3];

	atomic_t fmon_triggered;

	struct timer_list reset_timer;
};

static inline int is_camsv_engine(enum FMON_ENGINE engine)
{
	return (engine >= FMON_CAMSV_0 && engine < FMON_ENG_NUM);
}

extern struct platform_driver mtk_cam_fmon_driver;
extern int mtk_hrt_issue_flag_set(bool is_hrt_issue);

bool is_fmon_support(void);
void mtk_cam_fmon_enable(struct mtk_fmon_device *fmon);
void mtk_cam_fmon_disable(struct mtk_fmon_device *fmon);

void mtk_cam_fmon_bind(struct mtk_fmon_device *fmon, unsigned int used_raw, bool sv_on);
void mtk_cam_fmon_unbind(struct mtk_fmon_device *fmon, unsigned int used_raw);

void mtk_cam_fmon_dump(struct mtk_fmon_device *fmon);

int mtk_cam_fmon_probe(struct platform_device *pdev, struct mtk_cam_device *cam);

#endif /*__MTK_CAM_FMON_H*/
