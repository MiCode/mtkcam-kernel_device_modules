/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: Yuan-Jung Kuo <yuan-jung.kuo@mediatek.com>
 *          Nick.wen <nick.wen@mediatek.com>
 *
 */
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/iopoll.h>
#include <linux/of_device.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include "vcp_status.h"
#include "mtk_imgsys-engine-isp8.h"
#include "mtk_imgsys-cmdq.h"
#include "mtk_imgsys-cmdq-plat.h"
#include "mtk_imgsys-cmdq-qof-data.h"
#include "mtk-smi-dbg.h"

/* HW event: restore event for qof */
#define CMDQ_EVENT_IMG_QOF_RESTORE_EVENT_0_DIP			(312)
#define CMDQ_EVENT_IMG_QOF_RESTORE_EVENT_1_TRAW			(313)
#define CMDQ_EVENT_IMG_QOF_RESTORE_EVENT_2_WPE_EIS		(314)
#define CMDQ_EVENT_IMG_QOF_RESTORE_EVENT_3_WPE_TNR		(315)
#define CMDQ_EVENT_IMG_QOF_RESTORE_EVENT_4_LITE			(316)
/* SW event: sw event for qof */
#define CMDQ_SW_EVENT_QOF_DIP							(862)
#define CMDQ_SW_EVENT_QOF_TRAW							(863)
#define CMDQ_SW_EVENT_QOF_WPE_EIS						(864)
#define CMDQ_SW_EVENT_QOF_WPE_TNR						(865)
#define CMDQ_SW_EVENT_QOF_LITE							(866)
/* delay cnt */
#define IMG_HWV_DELAY_CNT								(0xFFFF)
#define IMG_CG_UNGATING_DELAY_CNT						(0xFFFF)
#define IMG_MTCMOS_STABLE_CNT							(0xFFFF)
#define INT_STATUS_POLL_TIMES							(20)
#define INT_STATUS_POLL_US								(50000)
#define POLL_DELAY_US									(1)
#define TIMEOUT_500US									(500)
#define TIMEOUT_1000US									(1000)
#define TIMEOUT_100000US								(100000)
/* others */
#define MOD_BIT_OFST									(3)
#define QOF_SUPPORT_SMI_GCE_CALLBACK					(1)
/* func */
#define IS_MOD_SUPPORT_QOF(m)		(((m < QOF_TOTAL_MODULE)) && ((g_qof_ver & BIT(m)) == BIT(m)))
#define IS_CON_PWR_ON(val)			((val & (BIT(30)|BIT(31))) == (BIT(30)|BIT(31)) ? TRUE:FALSE)
#define QOF_GET_REMAP_ADDR(addr)	(g_maped_rg[MAPED_RG_QOF_REG_BASE] + (addr - QOF_REG_BASE))
#define QOF_LOGI(fmt, args...)		pr_info("[QOF_LOGI]%s:%d " fmt "\n", __func__, __LINE__, ##args)
#define QOF_LOGE(fmt, args...)		pr_err("[QOF_ERROR]%s:%d " fmt "\n", __func__, __LINE__, ##args)
#define write_mask(addr, val, mask)\
	do {\
		unsigned int tmp = readl(addr) & ~(mask);\
		tmp = tmp | ((val) & mask);\
		writel(tmp, addr);\
	} while(0)

atomic_t imgsys_voter_cnt;
static u32 g_qof_ver;
static int g_qof_debug_level;
struct mtk_imgsys_dev *g_imgsys_dev;
static void __iomem *g_maped_rg[MAPED_RG_LIST_NUM] = {0};
static struct qof_events qof_events_isp8[ISP8_PWR_NUM];
struct cmdq_pkt *add_gce_pkt[ISP8_PWR_NUM];
struct cmdq_pkt *sub_gce_pkt[ISP8_PWR_NUM];
struct cmdq_client *qof_smi_client;
static struct cmdq_client *imgsys_pwr_clt[QOF_TOTAL_THREAD] = {NULL};
static struct cmdq_client *smi_cb_pwr_ctl;

enum QOF_DEBUG_MODE {
	QOF_DEBUG_MODE_PERFRAME_DUMP = 1,
};

enum QOF_GCE_THREAD_LIST {
	QOF_GCE_THREAD_DIP = 14,
	QOF_GCE_THREAD_TRAW = 15,
	QOF_GCE_THREAD_WPE1 = 16,
	QOF_GCE_THREAD_WPE23 = 17,
	QOF_GCE_THREAD_SMI_CB = 18,
};

u32 sw_event_lock_list[ISP8_PWR_NUM] = {
	CMDQ_SW_EVENT_QOF_DIP,
	CMDQ_SW_EVENT_QOF_TRAW,
	CMDQ_SW_EVENT_QOF_WPE_EIS,
	CMDQ_SW_EVENT_QOF_WPE_TNR,
	CMDQ_SW_EVENT_QOF_LITE,
};

u32 hw_event_restore_list[ISP8_PWR_NUM] = {
	CMDQ_EVENT_IMG_QOF_RESTORE_EVENT_0_DIP,
	CMDQ_EVENT_IMG_QOF_RESTORE_EVENT_1_TRAW,
	CMDQ_EVENT_IMG_QOF_RESTORE_EVENT_2_WPE_EIS,
	CMDQ_EVENT_IMG_QOF_RESTORE_EVENT_3_WPE_TNR,
	CMDQ_EVENT_IMG_QOF_RESTORE_EVENT_4_LITE,
};

struct cmdq_pwr_buf {
	struct cmdq_pkt *restore_pkt;
};

static struct cmdq_pwr_buf pwr_buf_handle[ISP8_PWR_NUM] = {
	{
		.restore_pkt = NULL,
	},
	{
		.restore_pkt = NULL,
	},
	{
		.restore_pkt = NULL,
	},
	{
		.restore_pkt = NULL,
	},
	{
		.restore_pkt = NULL,
	},
};

/* mtcmos data */
static void qof_poll_status(u32 pwr, bool enable);
static void imgsys_cmdq_qof_set_restore_done(struct cmdq_pkt *pkt, u32 pwr_id);
static void mtk_imgsys_set_dip_larb_golden(struct cmdq_pkt *pkt);
static void mtk_imgsys_set_wpe_eis_larb_golden(struct cmdq_pkt *pkt);
static void mtk_imgsys_set_wpe_tnr_larb_golden(struct cmdq_pkt *pkt);
static void mtk_imgsys_set_wpe_lite_larb_golden(struct cmdq_pkt *pkt);
static void mtk_imgsys_set_traw_larb_golden(struct cmdq_pkt *pkt);
static void imgsys_qof_traw_direct_link_reset(struct cmdq_pkt *pkt);
static void imgsys_cmdq_dip_cg_unating(struct cmdq_pkt *pkt,
		const struct imgsys_cg_data *cg, dma_addr_t qof_work_buf_pa);
static void imgsys_cmdq_traw_cg_unating(struct cmdq_pkt *pkt,
		const struct imgsys_cg_data *cg, dma_addr_t qof_work_buf_pa);
static void imgsys_cmdq_wpe1_cg_unating(struct cmdq_pkt *pkt,
		const struct imgsys_cg_data *cg, dma_addr_t qof_work_buf_pa);
static void imgsys_cmdq_wpe2_cg_unating(struct cmdq_pkt *pkt,
		const struct imgsys_cg_data *cg, dma_addr_t qof_work_buf_pa);
static void imgsys_cmdq_wpe3_cg_unating(struct cmdq_pkt *pkt,
		const struct imgsys_cg_data *cg, dma_addr_t qof_work_buf_pa);
static void qof_module_vote_add(struct cmdq_pkt *pkt, u32 pwr, u32 user);
static void qof_module_vote_sub(struct cmdq_pkt *pkt, u32 pwr, u32 user);
static struct imgsys_mtcmos_data isp8_module_data[] = {
	[ISP8_PWR_DIP] = {
		.pwr_id = ISP8_PWR_DIP,
		.cg_ungating = imgsys_cmdq_dip_cg_unating,
		.cg_data = &common_cg_data,
		.set_larb_golden = mtk_imgsys_set_dip_larb_golden,
		//.direct_link_reset = mtk_imgsys_dip_direct_link_reset,
		.qof_restore_done = imgsys_cmdq_qof_set_restore_done,
	},
	[ISP8_PWR_TRAW] = {
		.pwr_id = ISP8_PWR_TRAW,
		.cg_ungating = imgsys_cmdq_traw_cg_unating,
		.cg_data = &common_cg_data,
		.set_larb_golden = mtk_imgsys_set_traw_larb_golden,
		.direct_link_reset = imgsys_qof_traw_direct_link_reset,
		.qof_restore_done = imgsys_cmdq_qof_set_restore_done,
	},
	[ISP8_PWR_WPE_1_EIS] = {
		.pwr_id = ISP8_PWR_WPE_1_EIS,
		.cg_ungating = imgsys_cmdq_wpe1_cg_unating,
		.cg_data = &common_cg_data,
		.set_larb_golden = mtk_imgsys_set_wpe_eis_larb_golden,
		//.direct_link_reset = mtk_imgsys_wpe_1_eis_direct_link_reset,
		.qof_restore_done = imgsys_cmdq_qof_set_restore_done,
	},
	[ISP8_PWR_WPE_2_TNR] = {
		.pwr_id = ISP8_PWR_WPE_2_TNR,
		.cg_ungating = imgsys_cmdq_wpe2_cg_unating,
		.cg_data = &common_cg_data,
		.set_larb_golden = mtk_imgsys_set_wpe_tnr_larb_golden,
		//.direct_link_reset = mtk_imgsys_wpe_2_tnr_direct_link_reset,
		.qof_restore_done = imgsys_cmdq_qof_set_restore_done,
	},
	[ISP8_PWR_WPE_3_LITE] = {
		.pwr_id = ISP8_PWR_WPE_3_LITE,
		.cg_ungating = imgsys_cmdq_wpe3_cg_unating,
		.cg_data = &common_cg_data,
		.set_larb_golden = mtk_imgsys_set_wpe_lite_larb_golden,
		//.direct_link_reset = mtk_imgsys_wpe_3_lite_direct_link_reset,
		.qof_restore_done = imgsys_cmdq_qof_set_restore_done,
	},
};

bool is_qof_engine_enabled(int mod)
{
	int bit_mod = mod + MOD_BIT_OFST;
	int addr = qof_reg_table[ISP8_PWR_DIP][QOF_IMG_QOF_ENG_EN].addr;

	return (readl(QOF_GET_REMAP_ADDR(addr)) & BIT(bit_mod)) == BIT(bit_mod);
}

void qof_create_smi_cb_thread(void)
{
#ifdef QOF_SUPPORT_SMI_GCE_CALLBACK
	if (smi_cb_pwr_ctl == NULL) {
		QOF_LOGI("qof smi_cb_pwr_ctl = null\n");
		return;
	} else {
		qof_smi_client = smi_cb_pwr_ctl;
		cmdq_mbox_enable(qof_smi_client->chan);
	}
#endif
}

static void qof_create_smi_const_pkt(void)
{
#ifdef QOF_SUPPORT_SMI_GCE_CALLBACK
	u32 mod = ISP8_PWR_START;

	for (mod = ISP8_PWR_START; mod < ISP8_PWR_NUM; mod++) {
		if (IS_MOD_SUPPORT_QOF(mod)) {
			/* create add const pkt */
			add_gce_pkt[mod] = cmdq_pkt_create(qof_smi_client);
			qof_module_vote_add(add_gce_pkt[mod], mod, QOF_USER_GCE);
			add_gce_pkt[mod]->priority = IMGSYS_PRI_HIGH;
			cmdq_pkt_set_noirq(add_gce_pkt[mod], true);
			cmdq_pkt_finalize(add_gce_pkt[mod]);

			/* create sub const pkt */
			sub_gce_pkt[mod] = cmdq_pkt_create(qof_smi_client);
			qof_module_vote_sub(sub_gce_pkt[mod], mod, QOF_USER_GCE);
			sub_gce_pkt[mod]->priority = IMGSYS_PRI_HIGH;
			cmdq_pkt_set_noirq(sub_gce_pkt[mod], true);
			cmdq_pkt_finalize(sub_gce_pkt[mod]);
			QOF_LOGI("module[%d] create const pkt success.\n", mod);
		} else {
			QOF_LOGI("ignore create const pkt. qof module[%d] not support\n", mod);
		}
	}
#endif
}

static int qof_smi_isp_module_get(void *data, int module)
{
	atomic_inc(&imgsys_voter_cnt);
	pm_runtime_get_noresume(g_imgsys_dev->dev);
	if (is_qof_engine_enabled(module)) {
#ifdef QOF_SUPPORT_SMI_GCE_CALLBACK
		cmdq_pkt_refinalize(add_gce_pkt[module]);
		cmdq_pkt_flush(add_gce_pkt[module]);
		qof_poll_status(module, true);
#else
		qof_module_vote_add(NULL, module, QOF_USER_AP);
#endif
	}
	return 0;
}

static int qof_smi_isp_module_get_if_in_use(void *data, int module)
{
	int ret = 0;
	u32 pm_usr_cnt = atomic_read(&imgsys_voter_cnt);

	if (pm_usr_cnt != 0) {
		atomic_inc(&imgsys_voter_cnt);
		pm_runtime_get_noresume(g_imgsys_dev->dev);
		if (is_qof_engine_enabled(module)) {
#ifdef QOF_SUPPORT_SMI_GCE_CALLBACK
			cmdq_pkt_refinalize(add_gce_pkt[module]);
			cmdq_pkt_flush(add_gce_pkt[module]);
			qof_poll_status(module, true);
#else
			qof_module_vote_add(NULL, module, QOF_USER_AP);
#endif
		}
		ret = 1;
	}
	return ret;
}

static int qof_smi_isp_module_put(void *data, int module)
{
	u32 pm_usr_cnt = atomic_read(&imgsys_voter_cnt);

	if (pm_usr_cnt) {
		if (is_qof_engine_enabled(module)) {
#ifdef QOF_SUPPORT_SMI_GCE_CALLBACK
			cmdq_pkt_refinalize(sub_gce_pkt[module]);
			cmdq_pkt_flush(sub_gce_pkt[module]);
#else
			qof_module_vote_sub(NULL, module, QOF_USER_AP);
#endif
		}
		pm_runtime_put_noidle(g_imgsys_dev->dev);
		atomic_dec(&imgsys_voter_cnt);
	}
	return 0;
}

/* SMI DIP CB */
static int smi_isp_dip_get(void *data)
{
	return qof_smi_isp_module_get(data, ISP8_PWR_DIP);
}

static int smi_isp_dip_get_if_in_use(void *data)
{
	return qof_smi_isp_module_get_if_in_use(data, ISP8_PWR_DIP);
}

static int smi_isp_dip_put(void *data)
{
	return qof_smi_isp_module_put(data, ISP8_PWR_DIP);
}

/* SMI TRAW CB */
static int smi_isp_traw_get(void *data)
{
	return qof_smi_isp_module_get(data, ISP8_PWR_TRAW);
}

static int smi_isp_traw_get_if_in_use(void *data)
{
	return qof_smi_isp_module_get_if_in_use(data, ISP8_PWR_TRAW);
}

static int smi_isp_traw_put(void *data)
{
	return qof_smi_isp_module_put(data, ISP8_PWR_TRAW);
}

/* SMI WPE1 CB */
static int smi_isp_wpe1_eis_get(void *data)
{
	return qof_smi_isp_module_get(data, ISP8_PWR_WPE_1_EIS);
}

static int smi_isp_wpe1_eis_get_if_in_use(void *data)
{
	return qof_smi_isp_module_get_if_in_use(data, ISP8_PWR_WPE_1_EIS);
}

static int smi_isp_wpe1_eis_put(void *data)
{
	return qof_smi_isp_module_put(data, ISP8_PWR_WPE_1_EIS);
}

/* SMI WPE2 CB */
static int smi_isp_wpe2_tnr_get(void *data)
{
	return qof_smi_isp_module_get(data, ISP8_PWR_WPE_2_TNR);
}

static int smi_isp_wpe2_tnr_get_if_in_use(void *data)
{
	return qof_smi_isp_module_get_if_in_use(data, ISP8_PWR_WPE_2_TNR);
}

static int smi_isp_wpe2_tnr_put(void *data)
{
	return qof_smi_isp_module_put(data, ISP8_PWR_WPE_2_TNR);
}

/* SMI WPE3 CB */
static int smi_isp_wpe3_lite_get(void *data)
{
	return qof_smi_isp_module_get(data, ISP8_PWR_WPE_3_LITE);
}

static int smi_isp_wpe3_lite_get_if_in_use(void *data)
{
	return qof_smi_isp_module_get_if_in_use(data, ISP8_PWR_WPE_3_LITE);
}

static int smi_isp_wpe3_lite_put(void *data)
{
	return qof_smi_isp_module_put(data, ISP8_PWR_WPE_3_LITE);
}


static struct smi_user_pwr_ctrl smi_isp_dip_pwr_cb = {
	 .name = "qof_isp_dip",
	 .data = NULL,
	 .smi_user_id =  MTK_SMI_IMG_DIP,
	 .smi_user_get = smi_isp_dip_get,
	 .smi_user_get_if_in_use = smi_isp_dip_get_if_in_use,
	 .smi_user_put = smi_isp_dip_put,
};

static struct smi_user_pwr_ctrl smi_isp_traw_pwr_cb = {
	 .name = "qof_isp_traw",
	 .data = NULL,
	 .smi_user_id =  MTK_SMI_IMG_TRAW,
	 .smi_user_get = smi_isp_traw_get,
	 .smi_user_get_if_in_use = smi_isp_traw_get_if_in_use,
	 .smi_user_put = smi_isp_traw_put,
};

static struct smi_user_pwr_ctrl smi_isp_wpe1_eis_pwr_cb = {
	 .name = "qof_isp_wpe1_eis",
	 .data = NULL,
	 .smi_user_id =  MTK_SMI_IMG_WPE_EIS,
	 .smi_user_get = smi_isp_wpe1_eis_get,
	 .smi_user_get_if_in_use = smi_isp_wpe1_eis_get_if_in_use,
	 .smi_user_put = smi_isp_wpe1_eis_put,
};

static struct smi_user_pwr_ctrl smi_isp_wpe2_tnr_pwr_cb = {
	 .name = "qof_isp_wpe2_tnr",
	 .data = NULL,
	 .smi_user_id =  MTK_SMI_IMG_WPE_TNR,
	 .smi_user_get = smi_isp_wpe2_tnr_get,
	 .smi_user_get_if_in_use = smi_isp_wpe2_tnr_get_if_in_use,
	 .smi_user_put = smi_isp_wpe2_tnr_put,
};

static struct smi_user_pwr_ctrl smi_isp_wpe3_lite_pwr_cb = {
	 .name = "qof_isp_wpe3_lite",
	 .data = NULL,
	 .smi_user_id =  MTK_SMI_IMG_WPE_LITE,
	 .smi_user_get = smi_isp_wpe3_lite_get,
	 .smi_user_get_if_in_use = smi_isp_wpe3_lite_get_if_in_use,
	 .smi_user_put = smi_isp_wpe3_lite_put,
};

static void imgsys_cmdq_qof_set_restore_done(struct cmdq_pkt *pkt, u32 pwr_id)
{
	unsigned int addr;
	unsigned int val;
	unsigned int mask;
	struct qof_events *qof_event;

	if (pkt == NULL || pwr_id >= ISP8_PWR_NUM) {
		QOF_LOGI("qof param wrong!\n");
		return;
	}

	addr = qof_reg_table[pwr_id][QOF_IMG_GCE_RESTORE_DONE].addr;
	val = qof_reg_table[pwr_id][QOF_IMG_GCE_RESTORE_DONE].val;
	mask = qof_reg_table[pwr_id][QOF_IMG_GCE_RESTORE_DONE].mask;
	qof_event = &qof_events_isp8[pwr_id];

	// set restore done
	cmdq_pkt_write(pkt, NULL, addr /* address*/ , val /* val */ , mask /* mask */ );

	// put down hw event
	cmdq_pkt_clear_event(pkt, qof_event->hw_event_restore);
}

static void imgsys_cmdq_qof_set_larb_golden(struct qof_larb_info larb_info, struct cmdq_pkt *pkt)
{
	unsigned int reg_ba, ofset, bound, i;
	if (larb_info.larb_reg_list == NULL) {
		QOF_LOGE("Param is null !\n");
		return;
	}

	reg_ba = larb_info.reg_ba;
	bound = larb_info.reg_list_size;

	for (i = 0; i < bound; i++) {
		ofset = reg_ba + larb_info.larb_reg_list[i].ofset;
		cmdq_pkt_write(pkt, NULL, ofset /*address*/,
				larb_info.larb_reg_list[i].val, 0xffffffff);
	}
}

static void imgsys_qof_traw_direct_link_reset(struct cmdq_pkt *pkt)
{
	cmdq_pkt_write(pkt, NULL,
			(IMG_CG_IMGSYS_MAIN + 0x260) /*address*/, 0x82,
			0xffffffff);
	cmdq_pkt_write(pkt, NULL,
			(IMG_CG_IMGSYS_MAIN + 0x260) /*address*/, 0x0,
			0xffffffff);
}

static void mtk_imgsys_set_dip_larb_golden(struct cmdq_pkt *pkt)
{
	if (pkt == NULL) {
		QOF_LOGE("Param is null !\n");
		return;
	}

	/* larb 10 */
	imgsys_cmdq_qof_set_larb_golden(qof_larb10_info, pkt);

	/* larb 15 */
	imgsys_cmdq_qof_set_larb_golden(qof_larb15_info, pkt);

	/* larb 38 */
	imgsys_cmdq_qof_set_larb_golden(qof_larb38_info, pkt);

	/* larb 39 */
	imgsys_cmdq_qof_set_larb_golden(qof_larb39_info, pkt);
}

static void mtk_imgsys_set_wpe_eis_larb_golden(struct cmdq_pkt *pkt)
{
	if (pkt == NULL) {
		QOF_LOGE("Param is null !\n");
		return;
	}

	/* larb 11 */
	imgsys_cmdq_qof_set_larb_golden(qof_larb11_info, pkt);
}

static void mtk_imgsys_set_wpe_tnr_larb_golden(struct cmdq_pkt *pkt)
{
	if (pkt == NULL) {
		QOF_LOGE("Param is null !\n");
		return;
	}

	/* larb 22 */
	imgsys_cmdq_qof_set_larb_golden(qof_larb22_info, pkt);
}

static void mtk_imgsys_set_wpe_lite_larb_golden(struct cmdq_pkt *pkt)
{
	if (pkt == NULL) {
		QOF_LOGE("Param is null !\n");
		return;
	}

	/* larb 23 */
	imgsys_cmdq_qof_set_larb_golden(qof_larb23_info, pkt);
}

static void mtk_imgsys_set_traw_larb_golden(struct cmdq_pkt *pkt)
{
	if (pkt == NULL) {
		QOF_LOGE("Param is null !\n");
		return;
	}

	/* larb 28 */
	imgsys_cmdq_qof_set_larb_golden(qof_larb28_info, pkt);

	/* larb 40 */
	imgsys_cmdq_qof_set_larb_golden(qof_larb40_info, pkt);
}

static void imgsys_cmdq_modules_cg_ungating(struct cmdq_pkt *pkt,
		unsigned int addr, const struct imgsys_cg_data *cg, unsigned int val, dma_addr_t qof_work_buf_pa)
{
	unsigned int reg;
	unsigned int clr_ofs = cg->clr_ofs;
	unsigned int sta_ofs = cg->sta_ofs;

	/* Clock un-gating */
	reg = addr + clr_ofs;
	cmdq_pkt_write(pkt, NULL, (reg) /* address*/ ,
			val /* val */, val);

	/* Wait clk un-gating */
	reg = addr + sta_ofs;

	cmdq_pkt_poll_timeout(pkt, 0/*poll val*/, SUBSYS_NO_SUPPORT, (reg)/*addr*/,
			val /*mask*/, IMG_CG_UNGATING_DELAY_CNT /*delay cnt*/,
			CMDQ_GPR_R13 /*GPR*/);
}

static void imgsys_cmdq_dip_cg_unating(struct cmdq_pkt *pkt,
		const struct imgsys_cg_data *cg, dma_addr_t qof_work_buf_pa)
{
	unsigned int val;

	/* DIP_NR1_DIP1*/
	val = BIT(0)|BIT(1);
	imgsys_cmdq_modules_cg_ungating(pkt, IMG_CG_DIP_NR1_DIP1, cg, val, qof_work_buf_pa);

	/* DIP_NR2_DIP1 */
	val = BIT(0)|BIT(1)|BIT(2);
	imgsys_cmdq_modules_cg_ungating(pkt, IMG_CG_DIP_NR2_DIP1, cg, val, qof_work_buf_pa);

	/* DIP_TOP_DIP1 */
	val = BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8);
	imgsys_cmdq_modules_cg_ungating(pkt, IMG_CG_DIP_TOP_DIP1, cg, val, qof_work_buf_pa);
}

static void imgsys_cmdq_wpe1_cg_unating(struct cmdq_pkt *pkt,
		const struct imgsys_cg_data *cg, dma_addr_t qof_work_buf_pa)
{
	unsigned int val;

	/* WPE1_DIP1 */
	val = BIT(0)|BIT(1)|BIT(2);
	imgsys_cmdq_modules_cg_ungating(pkt, IMG_CG_WPE1_DIP1, cg, val, qof_work_buf_pa);
}

static void imgsys_cmdq_wpe2_cg_unating(struct cmdq_pkt *pkt,
		const struct imgsys_cg_data *cg, dma_addr_t qof_work_buf_pa)
{
	unsigned int val;

	/* WPE2_DIP1 */
	val = BIT(0)|BIT(1)|BIT(2);
	imgsys_cmdq_modules_cg_ungating(pkt, IMG_CG_WPE2_DIP1, cg, val, qof_work_buf_pa);
}

static void imgsys_cmdq_wpe3_cg_unating(struct cmdq_pkt *pkt,
		const struct imgsys_cg_data *cg, dma_addr_t qof_work_buf_pa)
{
	unsigned int val;

	/* WPE3_DIP1 */
	val = BIT(0)|BIT(1)|BIT(2);
	imgsys_cmdq_modules_cg_ungating(pkt, IMG_CG_WPE3_DIP1, cg, val, qof_work_buf_pa);
}

static void imgsys_cmdq_traw_cg_unating(struct cmdq_pkt *pkt,
		const struct imgsys_cg_data *cg, dma_addr_t qof_work_buf_pa)
{
	unsigned int val;

	/* 1 &traw_dip1_clk	CLK_TRAW_DIP1_TRAW */
	val = BIT(0)|BIT(1)|BIT(2)|BIT(3);
	imgsys_cmdq_modules_cg_ungating(pkt, IMG_CG_TRAW_DIP1, cg, val, qof_work_buf_pa);

	/* 2 TRAW CAP */
	val = BIT(0);
	imgsys_cmdq_modules_cg_ungating(pkt, IMG_CG_TRAW_CAP_DIP1, cg, val, qof_work_buf_pa);
}

void mtk_imgsys_qof_print_hw_info(u32 mod)
{
	struct qof_events *event;

	if (IS_MOD_SUPPORT_QOF(mod) == false)
		return;

	event = &qof_events_isp8[mod];

	QOF_LOGI("qof_hw_info: ver[%u], mod[%d], rg(0x%x): 0x%x; rg(0x%x): 0x%x; rg(0x%x): 0x%x; rg(0x%x): 0x%x\n",
		g_qof_ver,
		mod,
		qof_reg_table[mod][QOF_IMG_EVENT_CNT_ADD].addr,
		readl(QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_EVENT_CNT_ADD].addr)),
		qof_reg_table[mod][QOF_IMG_ITC_SRC_SEL].addr,
		readl(QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_ITC_SRC_SEL].addr)),
		qof_reg_table[mod][QOF_IMG_PWR_ACK_2ND_WAIT_TH].addr,
		readl(QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_PWR_ACK_2ND_WAIT_TH].addr)),
		qof_reg_table[mod][QOF_IMG_POWER_STATE].addr,
		readl(QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_POWER_STATE].addr)));

	QOF_LOGI("qof_hw_info: rg(0x%x): 0x%x; rg(0x%x): 0x%x; rg(0x%x): 0x%x; rg(0x%x): 0x%x; rg(0x%x): 0x%x\n",
		qof_reg_table[mod][QOF_IMG_GCE_SAVE_DONE].addr,
		readl(QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_GCE_SAVE_DONE].addr)),
		qof_reg_table[mod][QOF_IMG_QOF_EVENT_CNT].addr,
		readl(QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_QOF_EVENT_CNT].addr)),
		qof_reg_table[mod][QOF_IMG_QOF_VOTER_DBG].addr,
		readl(QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_QOF_VOTER_DBG].addr)),
		qof_reg_table[mod][QOF_IMG_QOF_DONE_STATUS].addr,
		readl(QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_QOF_DONE_STATUS].addr)),
		qof_reg_table[mod][QOF_IMG_ITC_STATUS].addr,
		readl(QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_ITC_STATUS].addr)));

	QOF_LOGI("qof_hw_info: rg(0x%x): 0x%x; rg(0x%x): 0x%x; rg(0x%x): 0x%x;\n",
		qof_reg_table[mod][QOF_IMG_QOF_STATE_DBG].addr,
		readl(QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_QOF_STATE_DBG].addr)),
		qof_reg_table[mod][QOF_IMG_QOF_MTC_ST_LSB].addr,
		readl(QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_QOF_MTC_ST_LSB].addr)),
		qof_reg_table[mod][QOF_IMG_QOF_MTC_ST_MSB2].addr,
		readl(QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_QOF_MTC_ST_MSB2].addr)));
}

static void mtk_qof_print_mtcmos_status(void)
{
	QOF_LOGI("MTCMOS:cnt[%u]MAIN[0x%x]VCORE[0x%x]DIP[0x%x]TRAW[0x%x]WPE1[0x%x]WPE2[0x%x]WPE3[0x%x]\n",
		atomic_read(&imgsys_voter_cnt),
		(readl(g_maped_rg[MAPED_RG_ISP_MAIN_PWR_CON])),
		(readl(g_maped_rg[MAPED_RG_ISP_VCORE_PWR_CON])),
		(readl(g_maped_rg[MAPED_RG_ISP_DIP_PWR_CON])),
		(readl(g_maped_rg[MAPED_RG_ISP_TRAW_PWR_CON])),
		(readl(g_maped_rg[MAPED_RG_ISP_WPE_EIS_PWR_CON])),
		(readl(g_maped_rg[MAPED_RG_ISP_WPE_TNR_PWR_CON])),
		(readl(g_maped_rg[MAPED_RG_ISP_WPE_LITE_PWR_CON])));
}

static void imgsys_cmdq_init_qof_events(void)
{
	struct qof_events *event;
	u32 usr_id = 0;

	for(usr_id = ISP8_PWR_START; usr_id < ISP8_PWR_NUM; usr_id++) {
		event = &qof_events_isp8[usr_id];
		event->sw_event_lock = sw_event_lock_list[usr_id];
		event->hw_event_restore = hw_event_restore_list[usr_id];
	}
}

static void imgsys_cmdq_qof_init_pwr_thread(struct mtk_imgsys_dev *imgsys_dev)
{
	struct device *dev = imgsys_dev->dev;
	u32 idx = 0, thd_idx = 0;

	for (thd_idx = IMGSYS_NOR_THD; thd_idx < IMGSYS_NOR_THD + IMGSYS_PWR_THD; thd_idx++) {
		idx = thd_idx - IMGSYS_NOR_THD;
#ifdef QOF_SUPPORT_SMI_GCE_CALLBACK
		if (thd_idx != QOF_GCE_THREAD_SMI_CB) {
			imgsys_pwr_clt[thd_idx - IMGSYS_NOR_THD] = cmdq_mbox_create(dev, thd_idx);
			QOF_LOGI(
				"%s: cmdq_mbox_create pwr_thd(%d, 0x%lx)\n",
				__func__, thd_idx, (unsigned long)imgsys_pwr_clt[thd_idx - IMGSYS_NOR_THD]);
		} else {
			thd_idx = QOF_GCE_THREAD_SMI_CB;
			smi_cb_pwr_ctl = cmdq_mbox_create(dev, thd_idx);
			QOF_LOGI(
				"%s: cmdq_mbox_create smi_cb_pwr_thd(%d, 0x%lx)\n",
				__func__, thd_idx, (unsigned long)smi_cb_pwr_ctl);
		}
#else
		if (idx < QOF_TOTAL_THREAD) {
			imgsys_pwr_clt[thd_idx - IMGSYS_NOR_THD] = cmdq_mbox_create(dev, thd_idx);
			QOF_LOGI(
				"%s: cmdq_mbox_create pwr_thd(%d, 0x%lx)\n",
				__func__, thd_idx, (unsigned long)imgsys_pwr_clt[thd_idx - IMGSYS_NOR_THD]);
		} else {
			imgsys_pwr_clt[thd_idx - IMGSYS_NOR_THD] = NULL;
			QOF_LOGI("qof [%s] thread indexs are not match !
				[thd_id: %d][index: %d][total pwr thd num: %d/%d]\n",
				__func__, thd_idx, idx, QOF_TOTAL_THREAD, IMGSYS_PWR_THD);
		}
#endif
	}
}

static void qof_cmdq_set_module_rst(struct mtk_imgsys_dev *imgsys_dev,
		struct cmdq_pkt *pkt,
		const struct imgsys_mtcmos_data *pwr)
{

	if (imgsys_dev == NULL || pkt == NULL || pwr == NULL) {
		QOF_LOGE("param is null (%d/%d/%d)\n",
			(imgsys_dev == NULL), (pkt == NULL), (pwr == NULL));
		return;
	}

	QOF_LOGI("imgsys_cmdq_restore_locked pwr->pwr_id = %d\n",pwr->pwr_id);
	switch (pwr->pwr_id) {
	case ISP8_PWR_DIP:
		if (imgsys_dev->modules[IMGSYS_MOD_DIP].cmdq_set)
			imgsys_dev->modules[IMGSYS_MOD_DIP].cmdq_set(imgsys_dev, pkt, REG_MAP_E_DIP);
		else
			QOF_LOGE("cmdq_set function pointer is null\n");
		break;
	case ISP8_PWR_TRAW:
		if (imgsys_dev->modules[IMGSYS_MOD_TRAW].cmdq_set)
			imgsys_dev->modules[IMGSYS_MOD_TRAW].cmdq_set(imgsys_dev, pkt, REG_MAP_E_TRAW);
		else
			QOF_LOGE("cmdq_set function pointer is null\n");
		break;
	case ISP8_PWR_WPE_1_EIS:
		if (imgsys_dev->modules[IMGSYS_MOD_WPE].cmdq_set)
			imgsys_dev->modules[IMGSYS_MOD_WPE].cmdq_set(imgsys_dev, pkt, REG_MAP_E_WPE_EIS);
		else
			QOF_LOGE("cmdq_set function pointer is null\n");
		if (imgsys_dev->modules[IMGSYS_MOD_PQDIP].cmdq_set)
			imgsys_dev->modules[IMGSYS_MOD_PQDIP].cmdq_set(imgsys_dev, pkt, REG_MAP_E_PQDIP_A);
		else
			QOF_LOGE("cmdq_set function pointer is null\n");
		break;
	case ISP8_PWR_WPE_2_TNR:
		if (imgsys_dev->modules[IMGSYS_MOD_OMC].cmdq_set)
			imgsys_dev->modules[IMGSYS_MOD_OMC].cmdq_set(imgsys_dev, pkt, REG_MAP_E_OMC_TNR);
		else
			QOF_LOGE("cmdq_set function pointer is null\n");
		if (imgsys_dev->modules[IMGSYS_MOD_PQDIP].cmdq_set)
			imgsys_dev->modules[IMGSYS_MOD_PQDIP].cmdq_set(imgsys_dev, pkt, REG_MAP_E_PQDIP_B);
		else
			QOF_LOGE("cmdq_set function pointer is null\n");
		break;
	case ISP8_PWR_WPE_3_LITE:
		if (imgsys_dev->modules[IMGSYS_MOD_WPE].cmdq_set)
			imgsys_dev->modules[IMGSYS_MOD_WPE].cmdq_set(imgsys_dev, pkt, REG_MAP_E_WPE_LITE);
		else
			QOF_LOGE("cmdq_set function pointer is null\n");
		if (imgsys_dev->modules[IMGSYS_MOD_OMC].cmdq_set)
			imgsys_dev->modules[IMGSYS_MOD_OMC].cmdq_set(imgsys_dev, pkt, REG_MAP_E_OMC_LITE);
		else
			QOF_LOGE("cmdq_set function pointer is null\n");
		break;
	default:
		QOF_LOGE("case invalid[%u]\n", pwr->pwr_id);
		break;
	}
}

static void imgsys_cmdq_restore_locked(struct mtk_imgsys_dev *imgsys_dev,
		struct cmdq_pkt *pkt,
		const struct imgsys_mtcmos_data *pwr)
{
	if (imgsys_dev == NULL || pkt == NULL || pwr == NULL) {
		QOF_LOGE("param is null (%d/%d/%d)\n",
			(imgsys_dev == NULL), (pkt == NULL), (pwr == NULL));
		return;
	}

	/* ungate CG */
	if (pwr->cg_ungating)
		pwr->cg_ungating(pkt, pwr->cg_data, imgsys_dev->qof_work_buf_pa);

	/* reset larb golden */
	if (pwr->set_larb_golden)
		pwr->set_larb_golden(pkt);

	/* set module rst */
	qof_cmdq_set_module_rst(imgsys_dev, pkt, pwr);

	/* reset module after power on */
	if (pwr->direct_link_reset)
		pwr->direct_link_reset(pkt);

	/* register qof */
	if (pwr->qof_restore_done)
		pwr->qof_restore_done(pkt, pwr->pwr_id);
}

static void qof_start_pwr_restore_task(struct mtk_imgsys_dev *imgsys_dev,
		struct cmdq_client *client, u32 pwr_id,
		struct qof_events *event)
{
	struct cmdq_pkt *restore_pkt;

	if (pwr_id > IMG_GCE_THREAD_PWR_END) {
		QOF_LOGI("%s:qof mtcmos id is invalid %u\n", __func__, pwr_id);
		return;
	}

	restore_pkt = pwr_buf_handle[pwr_id].restore_pkt;

	/* Power on kernel thread */
	cmdq_mbox_enable(client->chan);

	if (!restore_pkt) {
		restore_pkt = pwr_buf_handle[pwr_id].restore_pkt = cmdq_pkt_create(client);
		if (!restore_pkt) {
			QOF_LOGI("%s:create cmdq package fail\n", __func__);
			return;
		}
	}

	/* Program start */
	/* Wait for restore hw event */
	cmdq_pkt_wfe(restore_pkt, event->hw_event_restore);

	imgsys_cmdq_restore_locked(imgsys_dev, restore_pkt,
			 &isp8_module_data[pwr_id]);

	/* Program end */
	restore_pkt->priority = IMGSYS_PRI_HIGH;
	cmdq_pkt_finalize_loop(restore_pkt);
	cmdq_pkt_flush_async(restore_pkt, NULL, (void *)restore_pkt);
}

void mtk_imgsys_cmdq_qof_run_gce_loop(struct mtk_imgsys_dev *imgsys_dev)
{
	struct qof_events *qof_event;
	u32 pwr = ISP8_PWR_START, th_id = IMG_GCE_THREAD_PWR_START;

	if (!imgsys_dev) {
		cmdq_err("qof imgsys_dev is NULL");
		return;
	}

	for (; (pwr < ISP8_PWR_NUM) && (th_id <= IMG_GCE_THREAD_PWR_END); pwr++, th_id++) {
		qof_event = &qof_events_isp8[pwr];
		/* Initial power restore thread */
#ifdef QOF_SUPPORT_SMI_GCE_CALLBACK
		if(imgsys_pwr_clt[th_id] && (th_id != IMG_GCE_THREAD_WPE_3_LITE))
			qof_start_pwr_restore_task(imgsys_dev, imgsys_pwr_clt[th_id], pwr, qof_event);
		else
			QOF_LOGE("qof imgsys_pwr_clt[%u] = null thid=%d\n", pwr, th_id);
#else
		if(imgsys_pwr_clt[th_id])
			qof_start_pwr_restore_task(imgsys_dev, imgsys_pwr_clt[th_id], pwr, qof_event);
		else
			QOF_LOGE("qof imgsys_pwr_clt[%u] = null thid=%d\n", pwr, th_id);
#endif
	}
}

void mtk_imgsys_cmdq_qof_init(struct mtk_imgsys_dev *imgsys_dev, struct cmdq_client *imgsys_clt)
{
	int ver = 0;
	int rg_idx = 0;

	QOF_LOGI("[%s] mtk_imgsys_cmdq_qof_init  start\n", __func__);
	if (of_property_read_u32_index(imgsys_dev->dev->of_node,
		"mediatek,imgsys-qof-ver", 0, &ver) == 0)
		QOF_LOGI("[%s] qof version = %u\n", __func__, ver);

	/* init global val */
	imgsys_dev->qof_ver = ver;
	g_qof_ver = ver;
	g_imgsys_dev = imgsys_dev;
	g_qof_debug_level = 0;
	qof_smi_client = NULL;
	smi_cb_pwr_ctl = NULL;

	atomic_set(&imgsys_voter_cnt, 0);

	if (imgsys_dev->qof_ver == MTK_IMGSYS_QOF_FUNCTION_OFF)
		return;

	QOF_LOGI("[%s] mtk_imgsys_cmdq_qof_init  qof version = %u\n", __func__, ver);

	imgsys_cmdq_qof_init_pwr_thread(imgsys_dev);

	imgsys_cmdq_init_qof_events();

	mtk_imgsys_cmdq_qof_run_gce_loop(imgsys_dev);

	qof_create_smi_cb_thread();

	qof_create_smi_const_pkt();

	/* smi cb register */
	if (IS_MOD_SUPPORT_QOF(ISP8_PWR_DIP))
		mtk_smi_dbg_register_pwr_ctrl_cb(&smi_isp_dip_pwr_cb);
	if (IS_MOD_SUPPORT_QOF(ISP8_PWR_TRAW))
		mtk_smi_dbg_register_pwr_ctrl_cb(&smi_isp_traw_pwr_cb);
	if (IS_MOD_SUPPORT_QOF(ISP8_PWR_WPE_1_EIS))
		mtk_smi_dbg_register_pwr_ctrl_cb(&smi_isp_wpe1_eis_pwr_cb);
	if (IS_MOD_SUPPORT_QOF(ISP8_PWR_WPE_2_TNR))
		mtk_smi_dbg_register_pwr_ctrl_cb(&smi_isp_wpe2_tnr_pwr_cb);
	if (IS_MOD_SUPPORT_QOF(ISP8_PWR_WPE_3_LITE))
		mtk_smi_dbg_register_pwr_ctrl_cb(&smi_isp_wpe3_lite_pwr_cb);

	/* ioremap rg */
	g_maped_rg[MAPED_RG_ISP_TRAW_PWR_CON]		= ioremap(ISP_TRAW_PWR_CON, 4);
	g_maped_rg[MAPED_RG_ISP_DIP_PWR_CON]		= ioremap(ISP_DIP_PWR_CON, 4);
	g_maped_rg[MAPED_RG_ISP_MAIN_PWR_CON]		= ioremap(ISP_MAIN_PWR_CON, 4);
	g_maped_rg[MAPED_RG_ISP_VCORE_PWR_CON]		= ioremap(ISP_VCORE_PWR_CON, 4);
	g_maped_rg[MAPED_RG_ISP_WPE_EIS_PWR_CON]	= ioremap(ISP_WPE_EIS_PWR_CON, 4);
	g_maped_rg[MAPED_RG_ISP_WPE_TNR_PWR_CON]	= ioremap(ISP_WPE_TNR_PWR_CON, 4);
	g_maped_rg[MAPED_RG_ISP_WPE_LITE_PWR_CON]	= ioremap(ISP_WPE_LITE_PWR_CON, 4);
	g_maped_rg[MAPED_RG_IMG_CG_IMGSYS_MAIN]		= ioremap(IMG_CG_IMGSYS_MAIN, 4);
	g_maped_rg[MAPED_RG_IMG_CG_DIP_NR1_DIP1]	= ioremap(IMG_CG_DIP_NR1_DIP1, 4);
	g_maped_rg[MAPED_RG_IMG_CG_DIP_NR2_DIP1]	= ioremap(IMG_CG_DIP_NR2_DIP1, 4);
	g_maped_rg[MAPED_RG_IMG_CG_DIP_TOP_DIP1]	= ioremap(IMG_CG_DIP_TOP_DIP1, 4);
	g_maped_rg[MAPED_RG_IMG_CG_TRAW_CAP_DIP1]	= ioremap(IMG_CG_TRAW_CAP_DIP1, 4);
	g_maped_rg[MAPED_RG_IMG_CG_TRAW_DIP1]		= ioremap(IMG_CG_TRAW_DIP1, 4);
	g_maped_rg[MAPED_RG_IMG_CG_WPE1_DIP1]		= ioremap(IMG_CG_WPE1_DIP1, 4);
	g_maped_rg[MAPED_RG_IMG_CG_WPE2_DIP1]		= ioremap(IMG_CG_WPE2_DIP1, 4);
	g_maped_rg[MAPED_RG_IMG_CG_WPE3_DIP1]		= ioremap(IMG_CG_WPE3_DIP1, 4);
	g_maped_rg[MAPED_RG_IMG_LARB10_BASE]		= ioremap(IMG_LARB10_BASE, 4);
	g_maped_rg[MAPED_RG_IMG_LARB11_BASE]		= ioremap(IMG_LARB11_BASE, 4);
	g_maped_rg[MAPED_RG_IMG_LARB15_BASE]		= ioremap(IMG_LARB15_BASE, 4);
	g_maped_rg[MAPED_RG_IMG_LARB22_BASE]		= ioremap(IMG_LARB22_BASE, 4);
	g_maped_rg[MAPED_RG_IMG_LARB23_BASE]		= ioremap(IMG_LARB23_BASE, 4);
	g_maped_rg[MAPED_RG_IMG_LARB38_BASE]		= ioremap(IMG_LARB38_BASE, 4);
	g_maped_rg[MAPED_RG_IMG_LARB39_BASE]		= ioremap(IMG_LARB39_BASE, 4);
	g_maped_rg[MAPED_RG_IMG_LARB28_BASE]		= ioremap(IMG_LARB28_BASE, 4);
	g_maped_rg[MAPED_RG_IMG_LARB40_BASE]		= ioremap(IMG_LARB40_BASE, 4);
	g_maped_rg[MAPED_RG_QOF_REG_BASE]			= ioremap(QOF_REG_BASE, 4);
	g_maped_rg[MAPED_RG_MMPC_REG_BASE]			= ioremap(MMPC_REG_BASE, 4);

	for (rg_idx = MAPED_RG_LIST_START; rg_idx < MAPED_RG_LIST_NUM; rg_idx++) {
		if (!g_maped_rg[rg_idx]) {
			QOF_LOGI("qof %s Unable to ioremap %d registers !\n",
				__func__, rg_idx);
		}
	}
}

void mtk_imgsys_cmdq_qof_release(struct mtk_imgsys_dev *imgsys_dev, struct cmdq_client *imgsys_clt)
{
	/* release resource */
	int idx = 0;
	int rg_idx = 0;

	if (!imgsys_clt) {
		cmdq_err("cl is NULL");
		dump_stack();
		return;
	}
	QOF_LOGI("release resource +\n");

	for (idx = IMG_GCE_THREAD_PWR_START; idx < QOF_TOTAL_THREAD; idx++) {
		if (IS_MOD_SUPPORT_QOF(idx)) {
			if (add_gce_pkt[idx])
				cmdq_pkt_destroy(add_gce_pkt[idx]);
			if (sub_gce_pkt[idx])
				cmdq_pkt_destroy(sub_gce_pkt[idx]);
		}
	}

	for (idx = IMG_GCE_THREAD_PWR_START; idx < QOF_TOTAL_THREAD; idx++) {
		cmdq_mbox_destroy(imgsys_pwr_clt[idx]);
		imgsys_pwr_clt[idx] = NULL;
	}

	for (rg_idx = MAPED_RG_LIST_START; rg_idx < MAPED_RG_LIST_NUM; rg_idx++) {
		if (g_maped_rg[rg_idx])
			iounmap(g_maped_rg[rg_idx]);
	}
	if(smi_cb_pwr_ctl)
		cmdq_mbox_destroy(smi_cb_pwr_ctl);
	QOF_LOGI("release resource -\n");
}

static bool qof_check_pwr_state(const u32 mod, u32 check_mode)
{
	mtk_qof_print_mtcmos_status();
	return true;
	if (check_mode == CHECK_MTCMOS) {
		switch(mod) {
		case QOF_SUPPORT_DIP:
			return IS_CON_PWR_ON(readl(g_maped_rg[MAPED_RG_ISP_DIP_PWR_CON]));
		case QOF_SUPPORT_TRAW:
			return IS_CON_PWR_ON(readl(g_maped_rg[MAPED_RG_ISP_TRAW_PWR_CON]));
		case QOF_SUPPORT_WPE_EIS:
			return IS_CON_PWR_ON(readl(g_maped_rg[MAPED_RG_ISP_WPE_EIS_PWR_CON]));
		case QOF_SUPPORT_WPE_TNR:
			return IS_CON_PWR_ON(readl(g_maped_rg[MAPED_RG_ISP_WPE_TNR_PWR_CON]));
		case QOF_SUPPORT_WPE_LITE:
			return IS_CON_PWR_ON(readl(g_maped_rg[MAPED_RG_ISP_WPE_LITE_PWR_CON]));
		default:
			QOF_LOGI("qof [%s] module id wrong(%u) !\n", __func__, mod);
		}
	} else if (check_mode == CHECK_QOF) {
		return (readl(ioremap(qof_reg_table[mod][QOF_IMG_QOF_STATE_DBG].addr, 4)) & BIT(1)) == BIT(1);
	}

	return false;
}

static void qof_set_engine_on(const u32 mod)
{
	void __iomem *io_addr;
	int tmp;

	if(IS_MOD_SUPPORT_QOF(mod) == false)
		return;

	QOF_LOGI("engine on mod[%d]+\n", mod);

	/* Add voter */
	io_addr = QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_APMCU_SET].addr);
	write_mask(io_addr,
		qof_reg_table[mod][QOF_IMG_APMCU_SET].val,
		qof_reg_table[mod][QOF_IMG_APMCU_SET].mask);
	/* Init setting */
	io_addr = QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_HW_CLR_EN].addr);
	write_mask(io_addr,
		qof_reg_table[mod][QOF_IMG_HW_CLR_EN].val,
		qof_reg_table[mod][QOF_IMG_HW_CLR_EN].mask);
	io_addr = QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_HW_SET_EN].addr);
	write_mask(io_addr,
		qof_reg_table[mod][QOF_IMG_HW_SET_EN].val,
		qof_reg_table[mod][QOF_IMG_HW_SET_EN].mask);
	io_addr = QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_HW_SEQ_EN].addr);
	write_mask(io_addr,
		qof_reg_table[mod][QOF_IMG_HW_SEQ_EN].val,
		qof_reg_table[mod][QOF_IMG_HW_SEQ_EN].mask);
	io_addr = QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_GCE_RESTORE_EN].addr);
	write_mask(io_addr,
		qof_reg_table[mod][QOF_IMG_GCE_RESTORE_EN].val,
		qof_reg_table[mod][QOF_IMG_GCE_RESTORE_EN].mask);
	io_addr = QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_PWR_ACK_2ND_WAIT_TH].addr);
	write_mask(io_addr,
		qof_reg_table[mod][QOF_IMG_PWR_ACK_2ND_WAIT_TH].val,
		qof_reg_table[mod][QOF_IMG_PWR_ACK_2ND_WAIT_TH].mask);
	io_addr = QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_PWR_ACK_WAIT_TH].addr);
	write_mask(io_addr,
		qof_reg_table[mod][QOF_IMG_PWR_ACK_WAIT_TH].val,
		qof_reg_table[mod][QOF_IMG_PWR_ACK_WAIT_TH].mask);
	/* qof engine enable */
	io_addr = QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_QOF_ENG_EN].addr);
	write_mask(io_addr,
		qof_reg_table[mod][QOF_IMG_QOF_ENG_EN].val,
		qof_reg_table[mod][QOF_IMG_QOF_ENG_EN].mask);

	io_addr =  QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_QOF_STATE_DBG].addr);
	if (readl_poll_timeout_atomic
		(io_addr, tmp, (tmp & BIT(1)) == BIT(1), POLL_DELAY_US, TIMEOUT_1000US) < 0) {
		QOF_LOGE("mod[%d] waiting for qof state pwr on timeout, disable support\n",mod);
		mtk_imgsys_cmdq_qof_dump(0, true);
		g_qof_ver &= (~BIT(mod));
		return;
	}

	/* qof APMCU Vote sub */
	io_addr = QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_APMCU_CLR].addr);
	write_mask(io_addr,
		qof_reg_table[mod][QOF_IMG_APMCU_CLR].val,
		qof_reg_table[mod][QOF_IMG_APMCU_CLR].mask);

	io_addr =  QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_QOF_STATE_DBG].addr);
	if (readl_poll_timeout_atomic
		(io_addr, tmp, (tmp & BIT(4)) == BIT(4), POLL_DELAY_US, TIMEOUT_1000US) < 0) {
		QOF_LOGE("mod[%d] waiting for qof state pwr off timeout, disable support\n",mod);
		mtk_imgsys_cmdq_qof_dump(0, true);
		g_qof_ver &= (~BIT(mod));
		// engine off
		return;
	}

	QOF_LOGI("engine on mod[%d]-\n", mod);
}

static void qof_set_engine_off(const u32 mod)
{
	void __iomem *io_addr;

	if (IS_MOD_SUPPORT_QOF(mod) == false)
		return;

	QOF_LOGI("engine off mod[%d]+\n", mod);
	mtk_imgsys_cmdq_qof_dump(0, true);

	io_addr =  QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_APMCU_SET].addr);
	write_mask(io_addr,
		qof_reg_table[mod][QOF_IMG_APMCU_SET].val,
		qof_reg_table[mod][QOF_IMG_APMCU_SET].mask);

	if (qof_check_pwr_state(mod, CHECK_QOF) == false) {
		QOF_LOGE("qof mtk_imgsys_cmdq_qof_engine_off fail cus mtcmos is off\n");
		mtk_imgsys_cmdq_qof_dump(0, true);
		return;
	}

	// disable hw_seq
	io_addr =  QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_HW_SEQ_EN].addr);
	write_mask(io_addr,
		0,
		qof_reg_table[mod][QOF_IMG_HW_SEQ_EN].mask);

	//Set engine enable = 0
	io_addr =  QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_QOF_ENG_EN].addr);
	write_mask(io_addr,
		0,
		qof_reg_table[mod][QOF_IMG_QOF_ENG_EN].mask);
	io_addr =  QOF_GET_REMAP_ADDR(qof_reg_table[mod][QOF_IMG_APMCU_CLR].addr);
	write_mask(io_addr,
		qof_reg_table[mod][QOF_IMG_APMCU_CLR].val,
		qof_reg_table[mod][QOF_IMG_APMCU_CLR].mask);
	mtk_imgsys_qof_print_hw_info(mod);
	QOF_LOGI("engine off mod[%d]-\n", mod);
}

void mtk_imgsys_cmdq_qof_engine_on(struct mtk_imgsys_dev *imgsys_dev)
{
	u32 mod = 0;

	pm_runtime_get_sync(g_imgsys_dev->dev);
	for (mod = ISP8_PWR_DIP; mod < ISP8_PWR_NUM; mod++) {
		if ((IS_MOD_SUPPORT_QOF(mod)) &&
			qof_check_pwr_state(mod, CHECK_MTCMOS)) {
			qof_set_engine_on(mod);
		} else {
			QOF_LOGI("module[%u] not support QOF, ver[%u]\n", mod, g_qof_ver);
		}
	}
	atomic_inc(&imgsys_voter_cnt);
	mtk_imgsys_cmdq_qof_dump(0, true);
}

void mtk_imgsys_cmdq_qof_engine_off(struct mtk_imgsys_dev *imgsys_dev)
{
	u32 qof_module = QOF_SUPPORT_START;

	atomic_dec(&imgsys_voter_cnt);

	for (; qof_module < QOF_TOTAL_MODULE; qof_module++) {
		/* engine off */
		if (qof_check_pwr_state(qof_module, CHECK_QOF)) {
			QOF_LOGI("qof[%u] engine off+\n", qof_module);
			qof_set_engine_off(qof_module);
			QOF_LOGI("qof[%u] engine off-\n", qof_module);
		}
	}
	pm_runtime_put_sync(g_imgsys_dev->dev);
}

static void qof_poll_status(u32 pwr, bool enable)
{
	void __iomem *io_addr;
	u32 tmp;
	u32 val = (enable?BIT(1):BIT(4));

	io_addr = QOF_GET_REMAP_ADDR(qof_reg_table[pwr][QOF_IMG_POWER_STATE].addr);
	if (readl_poll_timeout_atomic
		(io_addr, tmp, (tmp & val) == val, POLL_DELAY_US, TIMEOUT_1000US) < 0) {
		QOF_LOGE("mod[%d] waiting for qof state ap add done timeout, disable support\n",pwr);
		mtk_imgsys_cmdq_qof_dump(0, true);
		return;
	}
}

static void qof_module_vote_add(struct cmdq_pkt *pkt, u32 pwr, u32 user)
{
	struct qof_events *qof_event;
	void __iomem *io_addr;
	u32 tmp;

	qof_event = &qof_events_isp8[pwr];
	if (user == QOF_USER_AP) {
		//add voter
		io_addr = QOF_GET_REMAP_ADDR(qof_reg_table[pwr][QOF_IMG_EVENT_CNT_ADD].addr);
		write_mask(io_addr,
			qof_reg_table[pwr][QOF_IMG_EVENT_CNT_ADD].val,
			qof_reg_table[pwr][QOF_IMG_EVENT_CNT_ADD].mask);

		io_addr = QOF_GET_REMAP_ADDR(qof_reg_table[pwr][QOF_IMG_POWER_STATE].addr);
		if (readl_poll_timeout_atomic
			(io_addr, tmp, (tmp & BIT(1)) == BIT(1), POLL_DELAY_US, TIMEOUT_1000US) < 0) {
			QOF_LOGE("mod[%d] waiting for qof state ap add done timeout, disable support\n",pwr);
			mtk_imgsys_cmdq_qof_dump(0, true);
			return;
		}
		mtk_imgsys_cmdq_qof_dump(0, true);
	} else {
		if (pkt == NULL) {
			QOF_LOGE("parameter wrong !\n");
			return;
		}
		/* Enter critical section */
		cmdq_pkt_acquire_event(pkt, qof_event->sw_event_lock);
		cmdq_pkt_write(pkt, NULL, (qof_reg_table[pwr][QOF_IMG_EVENT_CNT_ADD].addr) /*address*/,
			qof_reg_table[pwr][QOF_IMG_EVENT_CNT_ADD].val,
			qof_reg_table[pwr][QOF_IMG_EVENT_CNT_ADD].mask);
		cmdq_pkt_poll_timeout(pkt, BIT(1)/*poll val*/, SUBSYS_NO_SUPPORT,
			(qof_reg_table[pwr][QOF_IMG_QOF_STATE_DBG].addr)/*addr*/,
			BIT(1) /*mask*/,
			IMG_MTCMOS_STABLE_CNT /*delay cnt 1 for 12us*/,
			CMDQ_GPR_R13 /*GPR*/);
		/* End of critical section */
		cmdq_pkt_clear_event(pkt, qof_event->sw_event_lock);
	}
}

static void qof_module_vote_sub(struct cmdq_pkt *pkt, u32 pwr, u32 user)
{
	struct qof_events *qof_event;
	void __iomem *io_addr;

	if(IS_MOD_SUPPORT_QOF(pwr) == false)
		return;

	if (user == QOF_USER_AP) {
		// sub voter
		io_addr = QOF_GET_REMAP_ADDR(qof_reg_table[pwr][QOF_IMG_EVENT_CNT_SUB].addr);
		write_mask(io_addr,
			qof_reg_table[pwr][QOF_IMG_EVENT_CNT_SUB].val,
			qof_reg_table[pwr][QOF_IMG_EVENT_CNT_SUB].mask);
		mtk_imgsys_cmdq_qof_dump(0, true);
	} else {
		if (pkt == NULL) {
			QOF_LOGE("parameter wrong !\n");
			return;
		}
		qof_event = &qof_events_isp8[pwr];

		/* Enter critical section */
		cmdq_pkt_acquire_event(pkt, qof_event->sw_event_lock);

		cmdq_pkt_write(pkt, NULL, (qof_reg_table[pwr][QOF_IMG_EVENT_CNT_SUB].addr) /*address*/,
			qof_reg_table[pwr][QOF_IMG_EVENT_CNT_SUB].val,
			qof_reg_table[pwr][QOF_IMG_EVENT_CNT_SUB].mask);

		cmdq_pkt_poll_timeout(pkt, 0/*poll val*/, SUBSYS_NO_SUPPORT,
			(qof_reg_table[pwr][QOF_IMG_QOF_STATE_DBG].addr)/*addr*/,
			BIT(3) /*mask*/, IMG_MTCMOS_STABLE_CNT /*delay cnt 1 for 12us*/,
			CMDQ_GPR_R13 /*GPR*/);

		/* End of critical section */
		cmdq_pkt_clear_event(pkt, qof_event->sw_event_lock);
	}
}

void mtk_imgsys_cmdq_qof_add(struct cmdq_pkt *pkt, bool *qof_need_sub, u32 hw_comb)
{
	u32 pwr = 0;

	if(g_qof_debug_level == QOF_DEBUG_MODE_PERFRAME_DUMP)
		mtk_imgsys_cmdq_qof_dump(0, false);

	for (pwr = ISP8_PWR_START; pwr < ISP8_PWR_NUM; pwr++) {
		if ((IS_MOD_SUPPORT_QOF(pwr)) &&
			(qof_need_sub[pwr] == false) &&
			(hw_comb & pwr_group[pwr])) {
			qof_module_vote_add(pkt, pwr, QOF_USER_GCE);
			qof_need_sub[pwr] = true;
		}
	}
}

void mtk_imgsys_cmdq_qof_sub(struct cmdq_pkt *pkt, bool *qof_need_sub)
{
	u32 pwr = 0;

	for (pwr = ISP8_PWR_DIP; pwr < ISP8_PWR_NUM; pwr++) {
		if (qof_need_sub[pwr] == true) {
			qof_module_vote_sub(pkt, pwr, QOF_USER_GCE);
			qof_need_sub[pwr] = false;
		}
	}
}

void mtk_imgsys_cmdq_qof_dump(uint32_t hwcomb, bool force_dump)
{
	int mod = 0;
	void __iomem *addr;

	if (force_dump == false)
		return;
	for (mod = ISP8_PWR_START; mod < ISP8_PWR_NUM; mod++)
		mtk_imgsys_qof_print_hw_info(mod);

	mtk_qof_print_mtcmos_status();

	addr = QOF_GET_REMAP_ADDR(qof_reg_table[ISP8_PWR_DIP][QOF_IMG_QOF_STATE_DBG].addr);
	if ((readl(addr) & BIT(1)) == BIT(1)) {
		QOF_LOGI(" CG Status: IMG_MAIN[0x%x], NR1[0x%x], NR2[0x%x], DIP_TOP[0x%x]\n",
		(readl(g_maped_rg[MAPED_RG_IMG_CG_IMGSYS_MAIN])),
		(readl(g_maped_rg[MAPED_RG_IMG_CG_DIP_NR1_DIP1])),
		(readl(g_maped_rg[MAPED_RG_IMG_CG_DIP_NR2_DIP1])),
		(readl(g_maped_rg[MAPED_RG_IMG_CG_DIP_TOP_DIP1])));
	} else
		QOF_LOGI("qof mtcmos dip is off. sta=0x%x\n", readl(addr));
	addr = QOF_GET_REMAP_ADDR(qof_reg_table[ISP8_PWR_TRAW][QOF_IMG_QOF_STATE_DBG].addr);
	if ((readl(addr) & BIT(1)) == BIT(1)) {
		QOF_LOGI(" CG Status: TRAW_CAP[0x%x], TRAW_DIP1[0x%x]\n",
		(readl(g_maped_rg[MAPED_RG_IMG_CG_TRAW_CAP_DIP1])),
		(readl(g_maped_rg[MAPED_RG_IMG_CG_TRAW_DIP1])));
	} else
		QOF_LOGI("qof mtcmos traw is off. sta=0x%x\n",  readl(addr));
	addr = QOF_GET_REMAP_ADDR(qof_reg_table[ISP8_PWR_WPE_1_EIS][QOF_IMG_QOF_STATE_DBG].addr);
	if ((readl(addr) & BIT(1)) == BIT(1)) {
		QOF_LOGI(" CG Status: WPE1[0x%x]\n",
		(readl(g_maped_rg[MAPED_RG_IMG_CG_WPE1_DIP1])));
	} else
		QOF_LOGI("qof mtcmos wpe1 is off. sta=0x%x\n", readl(addr));
	addr = QOF_GET_REMAP_ADDR(qof_reg_table[ISP8_PWR_WPE_2_TNR][QOF_IMG_QOF_STATE_DBG].addr);
	if ((readl(addr) & BIT(1)) == BIT(1)) {
		QOF_LOGI(" CG Status: WPE2[0x%x]\n",
		(readl(g_maped_rg[MAPED_RG_IMG_CG_WPE2_DIP1])));
	} else
		QOF_LOGI("qof mtcmos wpe2 is off. sta=0x%x\n", readl(addr));
	addr = QOF_GET_REMAP_ADDR(qof_reg_table[ISP8_PWR_WPE_3_LITE][QOF_IMG_QOF_STATE_DBG].addr);
	if ((readl(addr) & BIT(1)) == BIT(1)) {
		QOF_LOGI(" CG Status: WPE3[0x%x]\n",
		(readl(g_maped_rg[MAPED_RG_IMG_CG_WPE2_DIP1])));
	} else
		QOF_LOGI("qof mtcmos wpe3 is off. sta=0x%x\n", readl(addr));
}

int mtk_imgsys_qof_ctrl(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = sscanf(val, "%u %u", &g_qof_debug_level, &g_qof_ver);
	QOF_LOGI("g_qof_debug_level[%u], force ver:g_qof_ver[%u]\n",
		g_qof_debug_level,
		g_qof_ver);

	return 0;
}

static const struct kernel_param_ops qof_ctrl_ops = {
	.set = mtk_imgsys_qof_ctrl,
};

module_param_cb(imgsys_qof_ctrl, &qof_ctrl_ops, NULL, 0644);
MODULE_PARM_DESC(imgsys_qof_ctrl, "imgsys_qof_ctrl");
