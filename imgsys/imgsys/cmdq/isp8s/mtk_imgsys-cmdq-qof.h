/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: Yuan-Jung Kuo <yuan-jung.kuo@mediatek.com>
 *          Nick.wen <nick.wen@mediatek.com>
 *
 */

#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-cmdq.h"

#define QOF_DYNAMIC_DIP_CINE
/**
 * @brief Record every mtcmos status
 *		such as number of users who are
 *		using this mtcmos power.
 */
struct qof_reg_data {
	unsigned int ofset;
	unsigned int val;
};

struct reg_table_unit {
	unsigned int addr;
	unsigned int val;
	unsigned int mask;
	unsigned int field;
};

struct qof_events {
	u16 sw_event_lock;
	u16 hw_event_restore;
};

/**
 * @brief Record every mtcmos status
 *		such as number of users who are
 *		using this mtcmos power.
 */
struct imgsys_cg_data {
	u32 clr_ofs;
	u32 sta_ofs;
};

struct imgsys_mtcmos_data {
	u32 pwr_id;
	void (*qof_restore_done)(struct cmdq_pkt *pkt, u32 pwr_id);
};

static u32 cnt = 1;
void mtk_imgsys_cmdq_qof_init(struct mtk_imgsys_dev *imgsys_dev, struct cmdq_client *imgsys_clt);
void mtk_imgsys_cmdq_qof_release(struct mtk_imgsys_dev *imgsys_dev, struct cmdq_client *imgsys_clt);
void mtk_imgsys_cmdq_get_non_qof_module(u32 *non_qof_modules);
void mtk_imgsys_cmdq_qof_stream_on(struct mtk_imgsys_dev *imgsys_dev);
void mtk_imgsys_cmdq_qof_stream_off(struct mtk_imgsys_dev *imgsys_dev);
int mtk_qof_WPE_EIS_retry_vote_on(void);
int mtk_qof_WPE_EIS_retry_vote_off(void);

void mtk_imgsys_cmdq_qof_add(struct cmdq_pkt *pkt, bool *qof_need_sub, u32 hw_comb,
		struct img_swfrm_info *user_info, unsigned int mode, bool *need_cine, bool sec);
void mtk_imgsys_cmdq_qof_sub(struct cmdq_pkt *pkt, bool *qof_need_sub,
		struct img_swfrm_info *user_info, unsigned int mode, bool *need_cine);
void mtk_imgsys_cmdq_qof_dump(uint32_t hwcomb, bool need_dump_cg);
int smi_isp_dip_get_if_in_use(void *data);
int smi_isp_dip_put(void *data);
int smi_isp_traw_get_if_in_use(void *data);
int smi_isp_traw_put(void *data);
int smi_isp_wpe1_eis_get_if_in_use(void *data);
int smi_isp_wpe1_eis_put(void *data);
int smi_isp_wpe2_tnr_get_if_in_use(void *data);
int smi_isp_wpe2_tnr_put(void *data);
int smi_isp_wpe3_lite_get_if_in_use(void *data);
int smi_isp_wpe3_lite_put(void *data);
