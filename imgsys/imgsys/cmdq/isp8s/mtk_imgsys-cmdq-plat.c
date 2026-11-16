// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Daniel Huang <daniel.huang@mediatek.com>
 *
 */

#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <dt-bindings/interconnect/mtk,mmqos.h>
#include <mt-plat/aee.h>
//#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/pm_opp.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/mailbox_controller.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <mtk_imgsys-engine-isp8s.h>
#include "mtk_imgsys-cmdq.h"
#include "mtk_imgsys-cmdq-plat.h"
#include "mtk_imgsys-cmdq-plat_def.h"
#include "mtk_imgsys-cmdq-ext.h"
#include "mtk_imgsys-cmdq-qof.h"
#include "mtk_imgsys-cmdq-dvfs.h"
#include "mtk_imgsys-cmdq-hwqos.h"
#include "mtk_imgsys-cmdq-qos.h"
#include "mtk_imgsys-trace.h"
#include "mtk-interconnect.h"

#if DVFS_QOS_READY
#include "mtk-smi-dbg.h"
#endif
#include "smi.h"

#if IMGSYS_SECURE_ENABLE
#include "cmdq-sec.h"
#include "cmdq-sec-iwc-common.h"
#endif

#define IMGSYS_SEC_THD_IDX_START (IMGSYS_NOR_THD + IMGSYS_PWR_THD + IMGSYS_QOS_THD)

#define WPE_BWLOG_HW_COMB (IMGSYS_HW_FLAG_WPE_TNR | IMGSYS_HW_FLAG_DIP)
#define WPE_BWLOG_HW_COMB_ninA (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_PQDIP_A)
#define WPE_BWLOG_HW_COMB_ninB (IMGSYS_HW_FLAG_WPE_EIS | IMGSYS_HW_FLAG_PQDIP_B)
#define WPE_BWLOG_HW_COMB_ninC (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_TRAW)
#define WPE_BWLOG_HW_COMB_ninD (IMGSYS_HW_FLAG_WPE_LITE | IMGSYS_HW_FLAG_LTR)

#if CMDQ_CB_KTHREAD
static struct kthread_worker imgsys_cmdq_worker;
static struct task_struct *imgsys_cmdq_kworker_task;
#else
static struct workqueue_struct *imgsys_cmdq_wq;
#endif
static u32 is_stream_off;
#if IMGSYS_SECURE_ENABLE
static u32 is_sec_task_create;
static bool is_pwr_sec_mode;
#endif
static struct imgsys_event_history event_hist[IMGSYS_CMDQ_SYNC_POOL_NUM];

#ifdef IMGSYS_CMDQ_CBPARAM_NUM
static struct mtk_imgsys_cb_param g_cb_param[IMGSYS_CMDQ_CBPARAM_NUM];
static u32 g_cb_param_idx;
static struct mutex g_cb_param_lock;
#endif

#ifdef IMGSYS_MAE_WRITE_BACK_SUPPORT
static u32 is_mae_read_cmd;
static dma_addr_t g_pkt_mae_pa;
static u32 *g_pkt_mae_va;
static dma_addr_t mae_pa;
static u32 *mae_va;
static dma_addr_t g_pkt_mae_pa_end;
static u32 *g_pkt_mae_va_end;
#define MAE_SRAM_SIZE (4088)
#define REG_SIZE (4)
static struct mutex cpr_lock;
#endif

#ifdef IMGSYS_WPE_CHECK_FUNC_EN
static u32 is_wpe_read_cmd;
static dma_addr_t g_pkt_wpe_pa;
static u32 *g_pkt_wpe_va;
static dma_addr_t g_pkt_pqdipa_pa;
static u32 *g_pkt_pqdipa_va;
static u32 retry_cnt;
#define WPE_EIS_RETRY_DB_CNT (10)
#endif

static int isc_irq_enabled;

#ifdef IMGSYS_CMDQ_PKT_REUSE
int imgsys_cmdq_pkt_reuse_dis;
module_param(imgsys_cmdq_pkt_reuse_dis, int, 0644);
static dma_addr_t g_pkt_reuse_pa[IMGSYS_NOR_THD];
static u32 *g_pkt_reuse_va[IMGSYS_NOR_THD];
static struct cmdq_pkt *g_pkt_reuse[IMGSYS_NOR_THD];
static u32 is_pkt_created[IMGSYS_NOR_THD];
static u32 cur_cmd_block[IMGSYS_NOR_THD];
static u32 g_reuse_cmd_num[IMGSYS_NOR_THD];
static u32 g_reuse_cmd_num_max[IMGSYS_NOR_THD];
static struct cmdq_reuse g_event_reuse[IMGSYS_NOR_THD][IMGSYS_PKT_EVENT_NUM];
static u32 g_reuse_event_num[IMGSYS_NOR_THD];
static u32 g_reuse_event_num_max[IMGSYS_NOR_THD];
static void *g_reuse_cb_param[IMGSYS_NOR_THD][IMGSYS_PKT_REUSE_CB_NUM];
static u32 g_cb_idx[IMGSYS_NOR_THD];
static u32 cur_cb_idx[IMGSYS_NOR_THD];
static size_t g_pkt_reuse_ofst[IMGSYS_NOR_THD][IMGSYS_PKT_REUSE_POOL_NUM][MAX_FRAME_IN_TASK];
#endif

u32 imgsys_cmdq_is_stream_off(void)
{
	return is_stream_off;
}

void imgsys_cmdq_init_plat8s(struct mtk_imgsys_dev *imgsys_dev, const int nr_imgsys_dev)
{
	struct device *dev = imgsys_dev->dev;
	u32 idx = 0;

	pr_info("%s: +, dev(0x%lx), num(%d)\n", __func__, (unsigned long)dev, nr_imgsys_dev);

	/* Only first user has to do init work queue */
	if (nr_imgsys_dev == 1) {
#if CMDQ_CB_KTHREAD
		kthread_init_worker(&imgsys_cmdq_worker);
		imgsys_cmdq_kworker_task = kthread_run(kthread_worker_fn,
			&imgsys_cmdq_worker, "imgsys-cmdqcb");
		if (IS_ERR(imgsys_cmdq_kworker_task)) {
			dev_info(dev, "%s: failed to start imgsys-cmdqcb kthread worker\n",
				__func__);
			imgsys_cmdq_kworker_task = NULL;
		} else {
			#if IMGSYS_KTHREAD_USE_VIP
			dev_info(dev, "%s: imgsys-cmdqcb kthread worker set to VIP\n", __func__);
			set_user_nice(imgsys_cmdq_kworker_task,-20);
			set_task_priority_based_vip(imgsys_cmdq_kworker_task->pid,1);
			set_task_ls(imgsys_cmdq_kworker_task->pid);
			#else
			#define CAM_RT_PRIORITY 98
			struct sched_param param = {
			.sched_priority = (99 - CAM_RT_PRIORITY) };
			sched_setscheduler(imgsys_cmdq_kworker_task, SCHED_RR, &param);
			#endif
		}
#else
		imgsys_cmdq_wq = alloc_ordered_workqueue("%s",
				__WQ_LEGACY | WQ_MEM_RECLAIM |
				WQ_FREEZABLE | WQ_HIGHPRI,
			"imgsys_cmdq_cb_wq");
		if (!imgsys_cmdq_wq)
			pr_info("%s: Create workquque IMGSYS-CMDQ fail!\n",
				__func__);
#endif
	}

	switch (nr_imgsys_dev) {
	case 1: /* DIP */
		/* request thread by index (in dts) 0 */
		for (idx = 0; idx < IMGSYS_NOR_THD; idx++) {
			imgsys_clt[idx] = cmdq_mbox_create(dev, idx);
			pr_info("%s: cmdq_mbox_create(%d, 0x%lx)\n", __func__, idx, (unsigned long)imgsys_clt[idx]);
		}
		#if IMGSYS_SECURE_ENABLE
		/* request for imgsys secure gce thread */
		for (idx = IMGSYS_SEC_THD_IDX_START; idx < (IMGSYS_SEC_THD_IDX_START + IMGSYS_SEC_THD); idx++) {
			imgsys_sec_clt[idx - IMGSYS_SEC_THD_IDX_START] = cmdq_mbox_create(dev, idx);
			pr_info(
				"%s: cmdq_mbox_create sec_thd(%d, 0x%lx)\n",
				__func__, idx, (unsigned long)imgsys_sec_clt[idx - IMGSYS_SEC_THD_IDX_START]);
		}
		#endif
		/* parse hardware event */
		for (idx = 0; idx < IMGSYS_CMDQ_EVENT_MAX; idx++) {
			of_property_read_u16(dev->of_node,
				imgsys_event[idx].dts_name,
				&imgsys_event[idx].event);
			pr_info("%s: event idx %d is (%s, %d)\n", __func__,
				idx, imgsys_event[idx].dts_name,
				imgsys_event[idx].event);
		}
		break;
	default:
		break;
	}
	#if IMGSYS_SECURE_ENABLE
	is_pwr_sec_mode = false;
	#endif

	mtk_imgsys_cmdq_qof_init(imgsys_dev, imgsys_clt[0]);
	mtk_imgsys_cmdq_hwqos_init(imgsys_dev);

	mutex_init(&imgsys_dev->dvfs_qos_lock);
	mutex_init(&imgsys_dev->power_ctrl_lock);
#ifdef IMGSYS_CMDQ_CBPARAM_NUM
	mutex_init(&g_cb_param_lock);
#endif
	mutex_init(&imgsys_dev->vss_blk_lock);
	mutex_init(&imgsys_dev->sec_task_lock);

#ifdef IMGSYS_WPE_CHECK_FUNC_EN
	retry_cnt = 0;
#endif

}

void imgsys_cmdq_release_plat8s(struct mtk_imgsys_dev *imgsys_dev)
{
	u32 idx = 0;

	pr_info("%s: +\n", __func__);

	/* Destroy cmdq client */
	for (idx = 0; idx < IMGSYS_NOR_THD; idx++) {
		cmdq_mbox_destroy(imgsys_clt[idx]);
		imgsys_clt[idx] = NULL;
	}
	#if IMGSYS_SECURE_ENABLE
	for (idx = 0; idx < IMGSYS_SEC_THD; idx++) {
		cmdq_mbox_destroy(imgsys_sec_clt[idx]);
		imgsys_sec_clt[idx] = NULL;
	}
	#endif

	MTK_IMGSYS_QOF_NEED_RUN(imgsys_dev->qof_ver,
		mtk_imgsys_cmdq_qof_release(imgsys_dev, imgsys_clt[0]);
	);
	MTK_IMGSYS_QOS_ENABLE(imgsys_dev->hwqos_info.hwqos_support,
		mtk_imgsys_cmdq_hwqos_release();
	);

	/* Release work_quque */
#if CMDQ_CB_KTHREAD
	if (imgsys_cmdq_kworker_task)
		kthread_stop(imgsys_cmdq_kworker_task);
#else
	flush_workqueue(imgsys_cmdq_wq);
	destroy_workqueue(imgsys_cmdq_wq);
	imgsys_cmdq_wq = NULL;
#endif
	mutex_destroy(&imgsys_dev->dvfs_qos_lock);
	mutex_destroy(&imgsys_dev->power_ctrl_lock);
#ifdef IMGSYS_CMDQ_CBPARAM_NUM
	mutex_destroy(&g_cb_param_lock);
#endif
	mutex_destroy(&imgsys_dev->vss_blk_lock);
	mutex_destroy(&imgsys_dev->sec_task_lock);
}

void imgsys_cmdq_streamon_plat8s(struct mtk_imgsys_dev *imgsys_dev)
{
	u32 idx = 0;

	dev_info(imgsys_dev->dev,
		"%s: cmdq stream on (%d) quick_pwr(%d)\n",
		__func__, is_stream_off, imgsys_quick_onoff_enable_plat8s());
	is_stream_off = 0;

	cmdq_mbox_enable(imgsys_clt[0]->chan);

	for (idx = IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_START;
		idx <= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_END; idx++)
		cmdq_clear_event(imgsys_clt[0]->chan, imgsys_event[idx].event);

#ifndef CONFIG_FPGA_EARLY_PORTING
	cmdq_mbox_disable(imgsys_clt[0]->chan);
#endif

	memset((void *)event_hist, 0x0,
		sizeof(struct imgsys_event_history)*IMGSYS_CMDQ_SYNC_POOL_NUM);
#if DVFS_QOS_READY
	mtk_imgsys_mmdvfs_reset_plat8s(imgsys_dev);
	MTK_IMGSYS_QOS_ENABLE(!imgsys_dev->hwqos_info.hwqos_support,
		mtk_imgsys_mmqos_reset_plat8s(imgsys_dev);
		mtk_imgsys_mmqos_monitor_plat8s(imgsys_dev, SMI_MONITOR_START_STATE);
	);
#endif

#ifdef IMGSYS_CMDQ_CBPARAM_NUM
	memset((void *)g_cb_param, 0x0,
		sizeof(struct mtk_imgsys_cb_param) * IMGSYS_CMDQ_CBPARAM_NUM);
	g_cb_param_idx = -1;
	if (imgsys_cmdq_dbg_enable_plat8s())
		dev_dbg(imgsys_dev->dev,
			"%s: g_cb_param sz: %d * sizeof mtk_imgsys_cb_param %lu\n",
			__func__, IMGSYS_CMDQ_CBPARAM_NUM, sizeof(struct mtk_imgsys_cb_param));
#endif

#ifdef IMGSYS_MAE_WRITE_BACK_SUPPORT
	g_pkt_mae_va = cmdq_mbox_buf_alloc(imgsys_clt[0], &g_pkt_mae_pa);
	mae_va = g_pkt_mae_va;
	mae_pa = g_pkt_mae_pa;
	g_pkt_mae_pa_end = g_pkt_mae_pa + MAE_SRAM_SIZE;
	g_pkt_mae_va_end = g_pkt_mae_va + MAE_SRAM_SIZE / REG_SIZE;
	mutex_init(&cpr_lock);
#endif
#ifdef IMGSYS_WPE_CHECK_FUNC_EN
	g_pkt_wpe_va = g_pkt_mae_va_end;
	g_pkt_wpe_pa = g_pkt_mae_pa_end;
	g_pkt_pqdipa_va = g_pkt_wpe_va + 1;
	g_pkt_pqdipa_pa = g_pkt_wpe_pa + REG_SIZE;
#endif
#ifdef IMGSYS_CMDQ_PKT_REUSE
	for (idx = IMGSYS_MCNR_THD_START; idx <= IMGSYS_MCNR_THD_END; idx++) {
		g_pkt_reuse[idx] = NULL;
		is_pkt_created[idx] = 0;
		g_pkt_reuse_va[idx] = cmdq_mbox_muti_buf_alloc(
			imgsys_clt[idx], &g_pkt_reuse_pa[idx], IMGSYS_PKT_REUSE_PAGE_NUM);
		g_pkt_reuse[idx] = NULL;
		cur_cmd_block[idx] = 0;
		g_reuse_cmd_num[idx] = 0;
		g_reuse_cmd_num_max[idx] = 0;
		g_reuse_event_num[idx] = 0;
		g_reuse_event_num_max[idx] = 0;
		g_cb_idx[idx] = 0;
		cur_cb_idx[idx] = 0;
	}
#endif
}

void imgsys_cmdq_streamoff_plat8s(struct mtk_imgsys_dev *imgsys_dev)
{
	u32 idx = 0;

	dev_info(imgsys_dev->dev,
		"%s: cmdq stream off (%d) idx(%d) quick_pwr(%d)\n",
		__func__, is_stream_off, idx, imgsys_quick_onoff_enable_plat8s());
	is_stream_off = 1;

	#if CMDQ_STOP_FUNC
	for (idx = 0; idx < IMGSYS_NOR_THD; idx++) {
		cmdq_mbox_stop(imgsys_clt[idx]);
		if (imgsys_cmdq_dbg_enable_plat8s())
			dev_dbg(imgsys_dev->dev,
				"%s: calling cmdq_mbox_stop(%d, 0x%lx)\n",
				__func__, idx, (unsigned long)imgsys_clt[idx]);
	}
	#endif

	#ifdef IMGSYS_MAE_WRITE_BACK_SUPPORT
	cmdq_mbox_buf_free(imgsys_clt[0], g_pkt_mae_va, g_pkt_mae_pa);
	mae_va = NULL;
	mae_pa = 0;
	is_mae_read_cmd = 0;
	#endif
	#ifdef IMGSYS_WPE_CHECK_FUNC_EN
	is_wpe_read_cmd = 0;
	#endif

#ifdef IMGSYS_CMDQ_PKT_REUSE
	for (idx = IMGSYS_MCNR_THD_START; idx <= IMGSYS_MCNR_THD_END; idx++) {
		cmdq_mbox_muti_buf_free(imgsys_clt[idx], g_pkt_reuse_va[idx],
			g_pkt_reuse_pa[idx], IMGSYS_PKT_REUSE_PAGE_NUM);
	}
#endif

	#if IMGSYS_SECURE_ENABLE
	mutex_lock(&(imgsys_dev->sec_task_lock));
	if (is_sec_task_create) {
		for (idx = 0; idx < IMGSYS_SEC_CAM_THD; idx++)
			cmdq_sec_mbox_stop(imgsys_sec_clt[idx]);
		is_sec_task_create = 0;
	}
	mutex_unlock(&(imgsys_dev->sec_task_lock));
	#endif

#ifdef CONFIG_FPGA_EARLY_PORTING
	cmdq_mbox_disable(imgsys_clt[0]->chan);
#endif

	#if DVFS_QOS_READY
	mtk_imgsys_mmdvfs_reset_plat8s(imgsys_dev);
	MTK_IMGSYS_QOS_ENABLE(!imgsys_dev->hwqos_info.hwqos_support,
		mtk_imgsys_mmqos_reset_plat8s(imgsys_dev);
		mtk_imgsys_mmqos_monitor_plat8s(imgsys_dev, SMI_MONITOR_STOP_STATE);
	);
	#endif

	#ifdef MTK_ISC_SUPPORT
	if ((imgsys_dev->isc_irq > 0) && isc_irq_enabled) {
		disable_irq(imgsys_dev->isc_irq);
		isc_irq_enabled = 0;
	}
	#endif
}

static void imgsys_cmdq_cmd_dump_plat8s(struct swfrm_info_t *frm_info, u32 frm_idx, bool isFullDump)
{
	struct GCERecoder *cmd_buf = NULL;
	struct Command *cmd = NULL;
	u32 cmd_num = 0;
	u32 cmd_idx = 0;
	u32 cmd_ofst = 0;

	cmd_buf = (struct GCERecoder *)frm_info->user_info[frm_idx].g_swbuf;
	cmd_num = cmd_buf->curr_length / sizeof(struct Command);
	cmd_ofst = sizeof(struct GCERecoder);

	pr_info(
	"%s: +, req fd/no(%d/%d) frame no(%d) frm(%d/%d), cmd_oft(0x%x/0x%x), cmd_len(%d), num(%d), sz_per_cmd(%lu), frm_blk(%d), hw_comb(0x%x)\n",
		__func__, frm_info->request_fd, frm_info->request_no, frm_info->frame_no,
		frm_idx, frm_info->total_frmnum, cmd_buf->cmd_offset, cmd_ofst,
		cmd_buf->curr_length, cmd_num, sizeof(struct Command), cmd_buf->frame_block,
		frm_info->user_info[frm_idx].hw_comb);

	if (cmd_ofst != cmd_buf->cmd_offset) {
		pr_info("%s: [ERROR]cmd offset is not match (0x%x/0x%x)!\n",
			__func__, cmd_buf->cmd_offset, cmd_ofst);
		return;
	}

	cmd = (struct Command *)((unsigned long)(frm_info->user_info[frm_idx].g_swbuf) +
		(unsigned long)(cmd_buf->cmd_offset));

	for (cmd_idx = 0; cmd_idx < cmd_num; cmd_idx++) {
		switch (cmd[cmd_idx].opcode) {
		case IMGSYS_CMD_READ:
			pr_info(
			"%s: READ with source(0x%08x) target(0x%08x) mask(0x%08x)\n", __func__,
				cmd[cmd_idx].u.source, cmd[cmd_idx].u.target, cmd[cmd_idx].u.mask);
			break;
		case IMGSYS_CMD_WRITE:
			if (imgsys_cmdq_dbg_enable_plat8s() || isFullDump)
				pr_debug(
					"%s: WRITE with addr(0x%08x) value(0x%08x) mask(0x%08x)\n", __func__,
					cmd[cmd_idx].u.address, cmd[cmd_idx].u.value, cmd[cmd_idx].u.mask);
			break;
#ifdef MTK_IOVA_SINK2KERNEL
		case IMGSYS_CMD_WRITE_FD:
			if (imgsys_cmdq_dbg_enable_plat8s() || isFullDump)
				pr_debug(
					"%s: WRITE_FD with addr(0x%08x) msb_ofst(0x%08x) fd(0x%08x) ofst(0x%08x) rshift(%d)\n",
					__func__, cmd[cmd_idx].u.dma_addr,
					cmd[cmd_idx].u.dma_addr_msb_ofst,
					cmd[cmd_idx].u.fd, cmd[cmd_idx].u.ofst,
					cmd[cmd_idx].u.right_shift);
			break;
		case IMGSYS_CMD_WRITE_FD_HW:
			if (imgsys_cmdq_dbg_enable_plat8s() || isFullDump) {
				pr_debug(
				"%s: WRITE_FD_HW with addr(0x%08x) hw_id(%d) fd(0x%08x) ofst(0x%08x) rsv(%d)\n",
				__func__, cmd[cmd_idx].u.dma_addr,
				cmd[cmd_idx].u.dma_addr_msb_ofst,
				cmd[cmd_idx].u.fd, cmd[cmd_idx].u.ofst,
				cmd[cmd_idx].u.right_shift);
			}
			break;
#endif
		case IMGSYS_CMD_POLL:
			pr_info(
			"%s: POLL with addr(0x%08x) value(0x%08x) mask(0x%08x)\n", __func__,
				cmd[cmd_idx].u.address, cmd[cmd_idx].u.value, cmd[cmd_idx].u.mask);
			break;
		case IMGSYS_CMD_WAIT:
			pr_info("%s: WAIT event(%d/%d) action(%d)\n", __func__,
					cmd[cmd_idx].u.event, imgsys_event[cmd[cmd_idx].u.event].event,
					cmd[cmd_idx].u.action);
			break;
		case IMGSYS_CMD_UPDATE:
			pr_info("%s: UPDATE event(%d/%d) action(%d)\n", __func__,
					cmd[cmd_idx].u.event, imgsys_event[cmd[cmd_idx].u.event].event,
					cmd[cmd_idx].u.action);
			break;
		case IMGSYS_CMD_ACQUIRE:
			pr_info("%s: ACQUIRE event(%d/%d) action(%d)\n", __func__,
					cmd[cmd_idx].u.event, imgsys_event[cmd[cmd_idx].u.event].event,
					cmd[cmd_idx].u.action);
			break;
		case IMGSYS_CMD_TIME:
			pr_info("%s: Get cmdq TIME stamp\n", __func__);
		break;
		case IMGSYS_CMD_STOP:
			pr_info("%s: End Of Cmd!\n", __func__);
			break;
		default:
			pr_info("%s: [ERROR]Not Support Cmd(%d)!\n", __func__, cmd[cmd_idx].opcode);
			break;
		}
	}
}

#if CMDQ_CB_KTHREAD
static void imgsys_cmdq_cb_work_plat8s(struct kthread_work *work)
#else
static void imgsys_cmdq_cb_work_plat8s(struct work_struct *work)
#endif
{
	struct mtk_imgsys_cb_param *cb_param = NULL;
	struct mtk_imgsys_dev *imgsys_dev = NULL;
	u32 hw_comb = 0;
	u32 cb_frm_cnt = 0;
	u64 tsDvfsQosStart = 0, tsDvfsQosEnd = 0;
	int req_fd = 0, req_no = 0, frm_no = 0, ret_sn = 0;
	u32 tsSwEvent = 0, tsHwEvent = 0, tsHw = 0, tsTaskPending = 0;
	u32 tsHwStr = 0, tsHwEnd = 0;
	bool isLastTaskInReq = 0;
	char *wpestr = NULL;
	u32 wpebw_en = imgsys_wpe_bwlog_enable_plat8s();
	char logBuf_temp[MTK_IMGSYS_LOG_LENGTH];
	u32 idx = 0;
	u32 real_frm_idx = 0;

	if (imgsys_cmdq_dbg_enable_plat8s())
		pr_debug("%s: +\n", __func__);
#ifdef IMGSYS_WPE_CHECK_FUNC_EN
	int ret_flush;
	u64 frm_owner;
#endif
	cb_param = container_of(work, struct mtk_imgsys_cb_param, cmdq_cb_work);
	cb_param->cmdqTs.tsCmdqCbWorkStart = ktime_get_boottime_ns()/1000;
	imgsys_dev = cb_param->imgsys_dev;

	if (imgsys_cmdq_dbg_enable_plat8s())
		dev_dbg(imgsys_dev->dev,
			"%s: cb(%p) gid(%d) in block(%d/%d) for frm(%d/%d) lst(%d/%d/%d) task(%d/%d/%d) ofst(%lx/%lx/%lx/%lx/%lx)\n",
			__func__, cb_param, cb_param->group_id,
			cb_param->blk_idx,  cb_param->blk_num,
			cb_param->frm_idx, cb_param->frm_num,
			cb_param->isBlkLast, cb_param->isFrmLast, cb_param->isTaskLast,
			cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
			cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
			cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);

#ifdef IMGSYS_WPE_CHECK_FUNC_EN

	if (!imgsys_cmdq_wpe_retry_enable_plat8s())
		goto wpe_normal_flow;

	if (cb_param->hw_comb != (IMGSYS_HW_FLAG_WPE_EIS|IMGSYS_HW_FLAG_PQDIP_A))
		goto wpe_normal_flow;

	if (cb_param->err == -8011) {

		frm_owner = cb_param->frm_info->frm_owner;

		if (cb_param->is2ndflush == 1) {

			for (idx = 0; idx < cb_param->retry_tbl.event_num; idx++)
				cmdq_set_event(imgsys_clt[0]->chan, cb_param->retry_tbl.events[idx]);

			cmdq_clear_event(imgsys_clt[0]->chan,
				imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_WPE_EIS].event);
			cmdq_clear_event(imgsys_clt[0]->chan,
				imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_PQDIP_A].event);

			cmdq_pkt_refinalize(cb_param->pkt);
			ret_flush = cmdq_pkt_flush_async(cb_param->pkt, imgsys_cmdq_task_cb_plat8s,
									(void *)cb_param);
			if (ret_flush < 0)
				pr_info("%s: failed to cmdq_pkt_flush_async ret(%d) for WPE_EIS!\n",
					__func__, ret_flush);
			else {
				pr_info("%s: cmdq_pkt_flush_async ret(%d) for WPE_EIS run (%d) for user(%s)!\n",
					__func__, ret_flush, cb_param->is2ndflush, (char *)(&frm_owner));
			}

			pr_info("%s: [ERROR] WPE_EIS-PQDIP-A HW timeout with retry wfe(%d) event(%d) user(%s)",
				__func__, cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, (char *)(&frm_owner));

			retry_cnt++;
			if (retry_cnt >= WPE_EIS_RETRY_DB_CNT)
				aee_kernel_exception("CRDISPATCH_KEY:MM_IMG_WPE",
				"DISPATCH:IMGSYS_WPE-PQDIPA_1st, hwcomb:0x%x", cb_param->hw_comb);

		} else {
			pr_info("%s: [ERROR] WPE_EIS-PQDIP-A HW timeout still! wfe(%d) event(%d) user(%s)",
				__func__, cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, (char *)(&frm_owner));

			if (cb_param->user_cmdq_err_cb) {
				struct cmdq_cb_data user_cb_data;

				user_cb_data.err = cb_param->err;
				user_cb_data.data = (void *)cb_param->frm_info;
				cb_param->user_cmdq_err_cb(user_cb_data, cb_param->frm_idx, 1, 0);
			}
			aee_kernel_exception("CRDISPATCH_KEY:MM_IMG_WPE",
			"DISPATCH:IMGSYS_WPE-PQDIPA_2nd, hwcomb:0x%x", cb_param->hw_comb);

			if (cb_param->hw_comb == (IMGSYS_HW_FLAG_WPE_EIS|IMGSYS_HW_FLAG_PQDIP_A)) {
				cmdq_clear_event(imgsys_clt[0]->chan,
					imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_WPE_EIS].event);
				cmdq_clear_event(imgsys_clt[0]->chan,
					imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_PQDIP_A].event);
			}
		}


		if (cb_param->is2ndflush == 1)
			return;
	}

	if (cb_param->err == -8012) {

		frm_owner = cb_param->frm_info->frm_owner;

		if (cb_param->is2ndflush == 1) {

			for (idx = 0; idx < cb_param->retry_tbl.event_num; idx++)
				cmdq_set_event(imgsys_clt[0]->chan, cb_param->retry_tbl.events[idx]);

			cmdq_clear_event(imgsys_clt[0]->chan,
				imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_WPE_EIS].event);
			cmdq_clear_event(imgsys_clt[0]->chan,
				imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_PQDIP_A].event);

			cmdq_pkt_refinalize(cb_param->pkt);
			ret_flush = cmdq_pkt_flush_async(cb_param->pkt, imgsys_cmdq_task_cb_plat8s,
									(void *)cb_param);
			if (ret_flush < 0)
				pr_info("%s: failed to cmdq_pkt_flush_async ret(%d) for PQDIPA!\n",
					__func__, ret_flush);
			else {
				pr_info("%s: cmdq_pkt_flush_async ret(%d) for PQDIPA run(%d) for user(%s)!\n",
					__func__, ret_flush, cb_param->is2ndflush, (char *)(&frm_owner));
			}
		} else {
			pr_info("%s: [ERROR] WPE_EIS-PQDIPA(PQDIPA) timeout still! wfe(%d) event(%d) user(%s)",
				__func__, cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, (char *)(&frm_owner));

			if (cb_param->user_cmdq_err_cb) {
				struct cmdq_cb_data user_cb_data;

				user_cb_data.err = cb_param->err;
				user_cb_data.data = (void *)cb_param->frm_info;
				cb_param->user_cmdq_err_cb(user_cb_data, cb_param->frm_idx, 1, 0);
			}
			aee_kernel_exception("CRDISPATCH_KEY:MM_IMG_PQDIPA",
			"DISPATCH:IMGSYS_WPE-PQDIPA_2nd, hwcomb:0x%x", cb_param->hw_comb);

			cmdq_clear_event(imgsys_clt[0]->chan,
				imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_WPE_EIS].event);
			cmdq_clear_event(imgsys_clt[0]->chan,
				imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_PQDIP_A].event);
		}

		if (cb_param->is2ndflush == 1)
			return;
	}
wpe_normal_flow:

#endif

#if CMDQ_TIMEOUT_KTHREAD
	if ((cb_param->err != 0) && cb_param->user_cmdq_err_cb && (cb_param->err != -8011)
								&& (cb_param->err != -8012)) {
		struct cmdq_cb_data user_cb_data;

		user_cb_data.err = cb_param->err;
		user_cb_data.data = (void *)cb_param->frm_info;
		cb_param->user_cmdq_err_cb(
			user_cb_data, cb_param->fail_subfidx, cb_param->isHWhang,
			cb_param->hangEvent);
	}
#endif

#ifndef CONFIG_FPGA_EARLY_PORTING
	#ifdef IMGSYS_CMDQ_PKT_REUSE
	if (cb_param->isPktReuse == 0)
	#endif
		mtk_imgsys_power_ctrl_plat8s(imgsys_dev, false);
#endif

	if (imgsys_cmdq_ts_enable_plat8s()) {
		for (idx = 0; idx < cb_param->task_cnt; idx++) {
			/* Calculating task timestamp */
			tsSwEvent = cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+1]
					- cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+0];
			tsHwEvent = cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+2]
					- cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+1];
			tsHwStr = cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+2];
			tsHwEnd = cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+3];
			tsHw = cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+3]
				- cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+2];
			tsTaskPending =
			cb_param->taskTs.dma_va[cb_param->taskTs.ofst+cb_param->taskTs.num-1]
			- cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+0];
			CMDQ_TICK_TO_US(tsSwEvent);
			CMDQ_TICK_TO_US(tsHwEvent);
			CMDQ_TICK_TO_US(tsHw);
			CMDQ_TICK_TO_US(tsHwStr);
			CMDQ_TICK_TO_US(tsHwEnd);
			CMDQ_TICK_TO_US(tsTaskPending);
			tsTaskPending =
				(cb_param->cmdqTs.tsCmdqCbStart-cb_param->cmdqTs.tsFlushStart)
				- tsTaskPending;
			if (imgsys_cmdq_dbg_enable_plat8s())
				dev_dbg(imgsys_dev->dev,
					"%s: TSus cb(%p) err(%d) frm(%d/%d/%d) hw_comb(0x%x) ts_num(%d) sw_event(%d) hw_event(%d) hw_real(%d) (%d/%d/%d/%d)\n",
					__func__, cb_param, cb_param->err, cb_param->frm_idx,
					cb_param->frm_num, cb_frm_cnt, hw_comb,
					cb_param->taskTs.num, tsSwEvent, tsHwEvent, tsHw,
					cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+0],
					cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+1],
					cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+2],
					cb_param->taskTs.dma_va[cb_param->taskTs.ofst+4*idx+3]);
			/* if (imgsys_cmdq_ts_dbg_enable_plat8s()) { */
				real_frm_idx = cb_param->frm_idx - (cb_param->task_cnt - 1) + idx;
				hw_comb = cb_param->frm_info->user_info[real_frm_idx].hw_comb;
				memset((char *)logBuf_temp, 0x0, MTK_IMGSYS_LOG_LENGTH);
				logBuf_temp[strlen(logBuf_temp)] = '\0';
				ret_sn = snprintf(logBuf_temp, MTK_IMGSYS_LOG_LENGTH,
					"/[%d/%d/%d/%d]hw_comb(0x%x)ts(%d-%d-%d-%d)hw(%d-%d)",
					real_frm_idx, cb_param->frm_num,
					cb_param->blk_idx, cb_param->blk_num,
					hw_comb, tsTaskPending, tsSwEvent, tsHwEvent,
					tsHw, tsHwStr, tsHwEnd);
				if (ret_sn < 0) {
					pr_info("%s: [ERROR] snprintf fail: %d\n",
						__func__, ret_sn);
				}
				strncat(cb_param->frm_info->hw_ts_log, logBuf_temp,
						strlen(logBuf_temp));
			/* } */
		}
	}

	if (cb_param->err != 0)
		pr_info(
			"%s: [ERROR] cb(%p) req fd/no(%d/%d) frame no(%d) error(%d) gid(%d) clt(0x%lx) hw_comb(0x%x) for frm(%d/%d) blk(%d/%d) lst(%d/%d) earlycb(%d) ofst(0x%lx) task(%d/%d/%d) ofst(%lx/%lx/%lx/%lx/%lx)",
			__func__, cb_param, cb_param->req_fd,
			cb_param->req_no, cb_param->frm_no,
			cb_param->err, cb_param->group_id,
			(unsigned long)cb_param->clt, cb_param->hw_comb,
			cb_param->frm_idx, cb_param->frm_num,
			cb_param->blk_idx, cb_param->blk_num,
			cb_param->isBlkLast, cb_param->isFrmLast,
			cb_param->is_earlycb, cb_param->pkt->err_data.offset,
			cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
			cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
			cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);
	if (is_stream_off == 1)
		pr_info("%s: [ERROR] cb(%p) pipe already streamoff(%d)!\n",
			__func__, cb_param, is_stream_off);

#ifdef IMGSYS_CMDQ_PKT_REUSE
	if (cb_param->isPktReuse == 1)
		goto imgsys_cmdq_pkt_destroy;
#endif
	if (imgsys_cmdq_dbg_enable_plat8s())
		dev_dbg(imgsys_dev->dev,
			"%s: req fd/no(%d/%d) frame no(%d) cb(%p)frm_info(%p) isBlkLast(%d) isFrmLast(%d) isECB(%d) isGPLast(%d) isGPECB(%d) for frm(%d/%d)\n",
			__func__, cb_param->frm_info->request_fd,
			cb_param->frm_info->request_no, cb_param->frm_info->frame_no,
			cb_param, cb_param->frm_info, cb_param->isBlkLast,
			cb_param->isFrmLast, cb_param->is_earlycb,
			cb_param->frm_info->user_info[cb_param->frm_idx].is_lastingroup,
			cb_param->frm_info->user_info[cb_param->frm_idx].is_earlycb,
			cb_param->frm_idx, cb_param->frm_num);

	req_fd = cb_param->frm_info->request_fd;
	req_no = cb_param->frm_info->request_no;
	frm_no = cb_param->frm_info->frame_no;

	cb_param->frm_info->cb_frmcnt++;
	cb_frm_cnt = cb_param->frm_info->cb_frmcnt;

	if (wpebw_en > 0) {
		for (idx = 0; idx < cb_param->task_cnt; idx++) {
			wpestr = NULL;
			real_frm_idx = cb_param->frm_idx - (cb_param->task_cnt - 1) + idx;
			hw_comb = cb_param->frm_info->user_info[real_frm_idx].hw_comb;
			switch (wpebw_en) {
			case 1:
				if ((hw_comb & WPE_BWLOG_HW_COMB) == WPE_BWLOG_HW_COMB)
					wpestr = "tnr";
				break;
			case 2:
				if (((hw_comb & WPE_BWLOG_HW_COMB_ninA) == WPE_BWLOG_HW_COMB_ninA)
				 || ((hw_comb & WPE_BWLOG_HW_COMB_ninB) == WPE_BWLOG_HW_COMB_ninB))
					wpestr = "eis";
				break;
			case 3:
				if (((hw_comb & WPE_BWLOG_HW_COMB_ninC) == WPE_BWLOG_HW_COMB_ninC)
				 || ((hw_comb & WPE_BWLOG_HW_COMB_ninD) == WPE_BWLOG_HW_COMB_ninD))
					wpestr = "lite";
				break;
			}
			if (wpestr) {
				dev_info(imgsys_dev->dev,
					"%s: wpe_bwlog req fd/no(%d/%d)frameNo(%d)cb(%p)err(%d)frm(%d/%d/%d)hw_comb(0x%x)read_num(%d)-%s(%d/%d/%d/%d)\n",
					__func__, cb_param->frm_info->request_fd,
					cb_param->frm_info->request_no,
					cb_param->frm_info->frame_no,
					cb_param, cb_param->err, real_frm_idx,
					cb_param->frm_num, cb_frm_cnt, hw_comb,
					cb_param->taskTs.num,
					wpestr,
					cb_param->taskTs.dma_va[cb_param->taskTs.ofst+0],
					cb_param->taskTs.dma_va[cb_param->taskTs.ofst+1],
					cb_param->taskTs.dma_va[cb_param->taskTs.ofst+2],
					cb_param->taskTs.dma_va[cb_param->taskTs.ofst+3]
					);
			}
		}
	}

	hw_comb = cb_param->frm_info->user_info[cb_param->frm_idx].hw_comb;

	if (imgsys_cmdq_dbg_enable_plat8s())
		dev_dbg(imgsys_dev->dev,
			"%s: req fd/no(%d/%d) frame no(%d) cb(%p)frm_info(%p) isBlkLast(%d) cb_param->frm_num(%d) cb_frm_cnt(%d)\n",
			__func__, cb_param->frm_info->request_fd,
			cb_param->frm_info->request_no, cb_param->frm_info->frame_no,
			cb_param, cb_param->frm_info, cb_param->isBlkLast, cb_param->frm_num,
			cb_frm_cnt);

	if (cb_param->isBlkLast && cb_param->user_cmdq_cb &&
		((cb_param->frm_info->total_taskcnt == cb_frm_cnt) || cb_param->is_earlycb)) {
		struct cmdq_cb_data user_cb_data;

		/* PMQOS API */
		tsDvfsQosStart = ktime_get_boottime_ns()/1000;
		IMGSYS_CMDQ_SYSTRACE_BEGIN(
			"%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d Own:%llx",
			__func__, "dvfs_qos", cb_param->frm_info->frame_no,
			cb_param->frm_info->request_no, cb_param->frm_info->request_fd,
			cb_param->frm_info->frm_owner);
		/* Calling PMQOS API if last frame */
		if (cb_param->frm_info->total_taskcnt == cb_frm_cnt) {
			mutex_lock(&(imgsys_dev->dvfs_qos_lock));
			#if DVFS_QOS_READY
			mtk_imgsys_mmdvfs_mmqos_cal_plat8s(imgsys_dev, cb_param->frm_info, 0);
			mtk_imgsys_mmdvfs_set_plat8s(imgsys_dev, cb_param->frm_info, 0);
			#endif
			mutex_unlock(&(imgsys_dev->dvfs_qos_lock));
			if (imgsys_cmdq_ts_enable_plat8s() || imgsys_wpe_bwlog_enable_plat8s()) {
				IMGSYS_CMDQ_SYSTRACE_BEGIN(
					"%s_%s|%s",
					__func__, "hw_ts_trace", cb_param->frm_info->hw_ts_log);
				cmdq_mbox_buf_free(cb_param->clt,
					cb_param->taskTs.dma_va, cb_param->taskTs.dma_pa);
				if (imgsys_cmdq_ts_dbg_enable_plat8s())
					dev_info(imgsys_dev->dev, "%s: %s",
						__func__, cb_param->frm_info->hw_ts_log);
				vfree(cb_param->frm_info->hw_ts_log);
				IMGSYS_CMDQ_SYSTRACE_END();
			}
			isLastTaskInReq = 1;
		} else
			isLastTaskInReq = 0;
		IMGSYS_CMDQ_SYSTRACE_END();
		tsDvfsQosEnd = ktime_get_boottime_ns()/1000;

		user_cb_data.err = cb_param->err;
		user_cb_data.data = (void *)cb_param->frm_info;
		cb_param->cmdqTs.tsUserCbStart = ktime_get_boottime_ns()/1000;
		IMGSYS_CMDQ_SYSTRACE_BEGIN(
			"%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d Own:%llx",
			__func__, "user_cb", cb_param->frm_info->frame_no,
			cb_param->frm_info->request_no, cb_param->frm_info->request_fd,
			cb_param->frm_info->frm_owner);
		cb_param->user_cmdq_cb(user_cb_data, cb_param->frm_idx, isLastTaskInReq,
			cb_param->batchnum, cb_param->memory_mode);
		IMGSYS_CMDQ_SYSTRACE_END();
		cb_param->cmdqTs.tsUserCbEnd = ktime_get_boottime_ns()/1000;
	}

	IMGSYS_CMDQ_SYSTRACE_BEGIN(
		"%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d fidx:%d hw_comb:0x%x Own:%llx cb:%p thd:%d frm(%d/%d/%d) DvfsSt(%lld) SetCmd(%lld) HW(%lld/%d-%d-%d-%d) Cmdqcb(%lld) WK(%lld) UserCb(%lld) DvfsEnd(%lld)",
		__func__, "wait_pkt", cb_param->frm_info->frame_no,
		cb_param->frm_info->request_no, cb_param->frm_info->request_fd,
		cb_param->frm_info->user_info[cb_param->frm_idx].subfrm_idx, hw_comb,
		cb_param->frm_info->frm_owner, cb_param, cb_param->thd_idx,
		cb_param->frm_idx, cb_param->frm_num, cb_frm_cnt,
		(cb_param->cmdqTs.tsDvfsQosEnd-cb_param->cmdqTs.tsDvfsQosStart),
		(cb_param->cmdqTs.tsFlushStart-cb_param->cmdqTs.tsReqStart),
		(cb_param->cmdqTs.tsCmdqCbStart-cb_param->cmdqTs.tsFlushStart),
		tsTaskPending, tsSwEvent, tsHwEvent, tsHw,
		(cb_param->cmdqTs.tsCmdqCbEnd-cb_param->cmdqTs.tsCmdqCbStart),
		(cb_param->cmdqTs.tsCmdqCbWorkStart-cb_param->cmdqTs.tsCmdqCbEnd),
		(cb_param->cmdqTs.tsUserCbEnd-cb_param->cmdqTs.tsUserCbStart),
		(tsDvfsQosEnd-tsDvfsQosStart));

#ifdef IMGSYS_CMDQ_PKT_REUSE
imgsys_cmdq_pkt_destroy:
	if ((cb_param->is_ctrl_cache <= 0) ||
		((cb_param->is_ctrl_cache == 1) && (cb_param->isPktReuse == 1))) {
		cmdq_pkt_wait_complete(cb_param->pkt);
		cmdq_pkt_destroy_no_wq(cb_param->pkt);
		if ((cb_param->is_ctrl_cache == 1) && (cb_param->isPktReuse == 1)) {
			is_pkt_created[cb_param->thd_idx] = 0;
			g_pkt_reuse[cb_param->thd_idx] = NULL;
			cur_cmd_block[cb_param->thd_idx] = 0;
			g_reuse_cmd_num[cb_param->thd_idx] = 0;
			g_reuse_cmd_num_max[cb_param->thd_idx] = 0;
			g_reuse_event_num[cb_param->thd_idx] = 0;
			g_reuse_event_num_max[cb_param->thd_idx] = 0;
			g_cb_idx[cb_param->thd_idx] = 0;
			cur_cb_idx[cb_param->thd_idx] = 0;
		}
	}
#else
	cmdq_pkt_wait_complete(cb_param->pkt);
	cmdq_pkt_destroy_no_wq(cb_param->pkt);
#endif
	cb_param->cmdqTs.tsReqEnd = ktime_get_boottime_ns()/1000;
	IMGSYS_CMDQ_SYSTRACE_END();

	if (imgsys_cmdq_ts_dbg_enable_plat8s() && imgsys_cmdq_dbg_enable_plat8s())
		dev_dbg(imgsys_dev->dev,
			"%s: TSus req fd/no(%d/%d) frame no(%d) thd(%d) cb(%p) err(%d) frm(%d/%d/%d) hw_comb(0x%x) DvfsSt(%lld) Req(%lld) SetCmd(%lld) HW(%lld/%d-%d-%d-%d) Cmdqcb(%lld) WK(%lld) CmdqCbWk(%lld) UserCb(%lld) DvfsEnd(%lld)\n",
			__func__, req_fd, req_no, frm_no, cb_param->thd_idx,
			cb_param, cb_param->err, cb_param->frm_idx,
			cb_param->frm_num, cb_frm_cnt, hw_comb,
			(cb_param->cmdqTs.tsDvfsQosEnd-cb_param->cmdqTs.tsDvfsQosStart),
			(cb_param->cmdqTs.tsReqEnd-cb_param->cmdqTs.tsReqStart),
			(cb_param->cmdqTs.tsFlushStart-cb_param->cmdqTs.tsReqStart),
			(cb_param->cmdqTs.tsCmdqCbStart-cb_param->cmdqTs.tsFlushStart),
			tsTaskPending, tsSwEvent, tsHwEvent, tsHw,
			(cb_param->cmdqTs.tsCmdqCbEnd-cb_param->cmdqTs.tsCmdqCbStart),
			(cb_param->cmdqTs.tsCmdqCbWorkStart-cb_param->cmdqTs.tsCmdqCbEnd),
			(cb_param->cmdqTs.tsReqEnd-cb_param->cmdqTs.tsCmdqCbWorkStart),
			(cb_param->cmdqTs.tsUserCbEnd-cb_param->cmdqTs.tsUserCbStart),
			(tsDvfsQosEnd-tsDvfsQosStart)
			);
#ifdef IMGSYS_CMDQ_CBPARAM_NUM
	if (cb_param->isDynamic)
		vfree(cb_param);
	else
		cb_param->isOccupy = false;
#else
	vfree(cb_param);
#endif
}

void imgsys_cmdq_task_cb_plat8s(struct cmdq_cb_data data)
{
	struct mtk_imgsys_cb_param *cb_param = NULL;
	struct mtk_imgsys_pipe *pipe = NULL;
	struct mtk_imgsys_dev *imgsys_dev = NULL;
	size_t err_ofst;
	u32 idx = 0, err_idx = 0, real_frm_idx = 0;
	u16 event = 0, event_sft = 0;
	u64 event_diff = 0;
	u32 event_val = 0L;
	bool isHWhang = 0;
	bool isQOFhang = 0;
	bool isHwDone = 1;
	bool isGPRtimeout = 0;
#ifdef IMGSYS_MAE_WRITE_BACK_SUPPORT
	u32 *mae_write_back = NULL;
	u32 *mae_read_back = NULL, *addr;
	u32 read_cnt = 0;
	struct mtk_imgsys_hw_info *mae_info = NULL;
#endif
#ifdef IMGSYS_WPE_CHECK_FUNC_EN
	u32 done_reg[2] = {0};
	u64 frm_owner;
#endif
#ifdef IMGSYS_CMDQ_PKT_REUSE
	u32 cookie = 0;
	u32 cb_cnt = 0;
	s32 pkt_err = 0;
	bool isReuseErr = 0;
	struct mtk_imgsys_cb_param *cb_param_pkt = NULL;
#endif

	if (imgsys_cmdq_dbg_enable_plat8s())
		pr_debug("%s: +\n", __func__);

	if (!data.data) {
		pr_info("%s: [ERROR]no callback data\n", __func__);
		return;
	}

	cb_param = (struct mtk_imgsys_cb_param *)data.data;
#ifdef IMGSYS_CMDQ_PKT_REUSE
	if (cb_param->pkt->loop == true) {
		cb_param_pkt = (struct mtk_imgsys_cb_param *)data.data;
		cookie = cb_param->pkt->cookie;
		cb_cnt = cb_param->pkt->cookie_diff;
		pkt_err = data.err;
		if (imgsys_cmdq_dbg_enable_plat8s())
			pr_info(
				"%s: [pkt_reuse] cb(%p) thd_idx(%d) cb_idx(%d/%d) cookie(%u) cb_cnt(%u)",
				__func__, cb_param, cb_param->thd_idx,
				cur_cb_idx[cb_param->thd_idx], g_cb_idx[cb_param->thd_idx], cookie, cb_cnt);
		if (g_reuse_cb_param[cb_param->thd_idx][cur_cb_idx[cb_param->thd_idx]] != NULL) {
			cb_param = g_reuse_cb_param[cb_param->thd_idx][cur_cb_idx[cb_param->thd_idx]];
			g_reuse_cb_param[cb_param->thd_idx][cur_cb_idx[cb_param->thd_idx]] = NULL;
			cur_cb_idx[cb_param->thd_idx]++;
			if (cur_cb_idx[cb_param->thd_idx] == IMGSYS_PKT_REUSE_CB_NUM)
				cur_cb_idx[cb_param->thd_idx] = 0;
			if (pkt_err != 0)
				isReuseErr = 1;
		} else {
			pr_info(
				"%s: [pkt_reuse] No more cb_param is left, run pkt_reuse uninit flow! pkt_cb(%p) error(%d)  gid(%d) for frm(%d/%d) blk(%d/%d) ofst(0x%lx) task(%d/%d/%d) thd_idx(%d) cb_idx(%d/%d) cookie(%u) cb_cnt(%u)",
				__func__, cb_param, data.err, cb_param->group_id,
				cb_param->frm_idx, cb_param->frm_num,
				cb_param->blk_idx, cb_param->blk_num,
				cb_param->pkt->err_data.offset,
				cb_param->task_id, cb_param->task_num, cb_param->task_cnt, cb_param->thd_idx,
				cur_cb_idx[cb_param->thd_idx], g_cb_idx[cb_param->thd_idx], cookie, cb_cnt);
			goto imgsys_cmdq_queue_cb_work;
		}
	}
#endif
	cb_param->err = data.err;
	cb_param->cmdqTs.tsCmdqCbStart = ktime_get_boottime_ns()/1000;
	imgsys_dev = cb_param->imgsys_dev;

	if (imgsys_cmdq_dbg_enable_plat8s())
		pr_debug(
			"%s: Receive cb(%p) with err(%d) for frm(%d/%d)\n",
			__func__, cb_param, data.err, cb_param->frm_idx, cb_param->frm_num);

#ifdef IMGSYS_MAE_WRITE_BACK_SUPPORT
	if ((cb_param->hw_comb == IMGSYS_HW_FLAG_MAE) && cb_param->hw_info.write_back_vaddr
		&& (is_mae_read_cmd == 1) && (is_stream_off == 0)) {
		mae_info = &cb_param->hw_info;
		mae_write_back = mae_info->write_back_vaddr;
		mae_read_back =  mae_info->read_va;
		read_cnt =  mae_info->read_cnt;
		for (idx = 0; idx < read_cnt; idx++ ){
			addr = &mae_read_back[idx];
			if (unlikely(addr >= g_pkt_mae_va_end)) {
				addr = (addr - g_pkt_mae_va_end) + g_pkt_mae_va;
				if (imgsys_cmdq_dbg_enable_plat8s())
					pr_debug("%s: [INFO] cpr va rings back to %p at idx(%d)\n",
										__func__, addr, idx);
			}
			mae_write_back[idx] = *addr;
			if (imgsys_cmdq_dbg_enable_plat8s())
				pr_debug("%s: [INFO] MAE writebacks regs(0x%x/0x%x/0x%x/0x%x)\n",
						__func__, mae_write_back[0], mae_write_back[1],
							mae_write_back[2], mae_write_back[3]);
		}
	}
#endif

#ifdef IMGSYS_WPE_CHECK_FUNC_EN
	if (!imgsys_cmdq_wpe_retry_enable_plat8s())
		goto wpe_normal_flow;

	if ((cb_param->err == 0) && (cb_param->hw_comb == (IMGSYS_HW_FLAG_WPE_EIS|IMGSYS_HW_FLAG_PQDIP_A))
		&& (is_wpe_read_cmd == 1) && (is_stream_off == 0)) {

		done_reg[0] = g_pkt_wpe_va[0];
		done_reg[1] = g_pkt_pqdipa_va[0];

		if (((done_reg[0] & 0x00000003) == 0)) {

			if (imgsys_cmdq_dbg_enable_plat8s())
				pr_info("%s: [ERROR] WPE_EIS-PQDIP_A hang detected! done_reg(0x%x)\n",
				__func__, done_reg[0]);

			frm_owner = cb_param->frm_info->frm_owner;
			if (cb_param->is2ndflush == -1) {
				cb_param->is2ndflush = 1;
				if (imgsys_cmdq_dbg_enable_plat8s())
					pr_info("%s: [WARN] Do reset in GCE buffer and retry for user(%s)\n",
					__func__, (char *)(&frm_owner));
				if (mtk_qof_WPE_EIS_retry_vote_on() < 0)
					pr_err("power on WPE-EIS fail!");
			} else {
				cb_param->is2ndflush = 0;
				if (imgsys_cmdq_dbg_enable_plat8s())
					pr_info("%s: [WARN] Do WPE_LITE retried for user(%s)\n",
						__func__, (char *)(&frm_owner));
			}
			/* mark wpe_eis hang */
			cb_param->err = -8011;

		} else if (((done_reg[1] & 0x00000003) == 0)) {
			frm_owner = cb_param->frm_info->frm_owner;
			if (cb_param->is2ndflush == -1) {
				cb_param->is2ndflush = 1;
				if (imgsys_cmdq_dbg_enable_plat8s())
					pr_info("%s: [WARN] PQDIPA retry for user(%s)\n",
					__func__, (char *)(&frm_owner));
				if (mtk_qof_WPE_EIS_retry_vote_on() < 0)
					pr_err("power on PQDIPA fail!");
			} else {
				cb_param->is2ndflush = 0;
				if (imgsys_cmdq_dbg_enable_plat8s())
					pr_info("%s: [WARN] PQDIPA retried for user(%s)\n",
						__func__, (char *)(&frm_owner));
			}
			/* mark pqdipa hang */
			cb_param->err = -8012;

		} else {
			if (cb_param->is2ndflush == 1) {
				// retry success, need clear voter
				if (mtk_qof_WPE_EIS_retry_vote_off() < 0)
					pr_err("power off WPE-EIS fail!");
			}
		}
	}

	if ((cb_param->err == 0) && (cb_param->hw_comb == (IMGSYS_HW_FLAG_WPE_EIS|IMGSYS_HW_FLAG_PQDIP_A))) {
		cmdq_clear_event(imgsys_clt[0]->chan,
			imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_WPE_EIS].event);
		cmdq_clear_event(imgsys_clt[0]->chan,
			imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_PQDIP_A].event);
	}

wpe_normal_flow:

#endif

	if ((cb_param->err != 0) && (cb_param->err != -8011) && (cb_param->err != -8012)) {
		err_ofst = cb_param->pkt->err_data.offset;
		err_idx = 0;
		for (idx = 0; idx < cb_param->task_cnt; idx++)
			if (err_ofst > cb_param->pkt_ofst[idx])
				err_idx++;
			else
				break;
		if (err_idx >= cb_param->task_cnt) {
			pr_info(
				"%s: [ERROR] can't find task in task list! cb(%p) error(%d)  gid(%d) for frm(%d/%d) blk(%d/%d) ofst(0x%lx) erridx(%d/%d) task(%d/%d/%d) ofst(%lx/%lx/%lx/%lx/%lx)",
				__func__, cb_param, cb_param->err, cb_param->group_id,
				cb_param->frm_idx, cb_param->frm_num,
				cb_param->blk_idx, cb_param->blk_num,
				cb_param->pkt->err_data.offset,
				err_idx, real_frm_idx,
				cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
				cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
				cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);
			err_idx = cb_param->task_cnt - 1;
		}
		real_frm_idx = cb_param->frm_idx - (cb_param->task_cnt - 1) + err_idx;
		pr_info(
			"%s: [ERROR] cb(%p) req fd/no(%d/%d) frame no(%d) error(%d) gid(%d) clt(0x%lx) hw_comb(0x%x) for frm(%d/%d) blk(%d/%d) lst(%d/%d/%d) earlycb(%d) ofst(0x%lx) erridx(%d/%d) task(%d/%d/%d) ofst(%lx/%lx/%lx/%lx/%lx)",
			__func__, cb_param, cb_param->req_fd,
			cb_param->req_no, cb_param->frm_no,
			cb_param->err, cb_param->group_id,
			(unsigned long)cb_param->clt, cb_param->hw_comb,
			cb_param->frm_idx, cb_param->frm_num,
			cb_param->blk_idx, cb_param->blk_num,
			cb_param->isBlkLast, cb_param->isFrmLast, cb_param->isTaskLast,
			cb_param->is_earlycb, cb_param->pkt->err_data.offset,
			err_idx, real_frm_idx,
			cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
			cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
			cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);
		if (is_stream_off == 1)
			pr_info("%s: [ERROR] cb(%p) pipe had been turned off(%d)!\n",
				__func__, cb_param, is_stream_off);
		pipe = cb_param->pipe;
		if (!pipe->streaming) {
			/* is_stream_off = 1; */
			pr_info("%s: [ERROR] cb(%p) pipe already streamoff(%d)\n",
				__func__, cb_param, is_stream_off);
		}

		event = cb_param->pkt->err_data.event;
		if ((event >= IMGSYS_CMDQ_HW_TOKEN_BEGIN) &&
			(event <= IMGSYS_CMDQ_HW_TOKEN_END)) {
			pr_info(
				"%s: [ERROR] HW token timeout! wfe(%d) event(%d) isHW(%d)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang);
		} else if ((event >= IMGSYS_CMDQ_SW_EVENT1_BEGIN) &&
			(event <= IMGSYS_CMDQ_SW_EVENT1_END)) {
			event_sft = event - IMGSYS_CMDQ_SW_EVENT1_BEGIN;
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event1 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_SW_EVENT2_BEGIN) &&
			(event <= IMGSYS_CMDQ_SW_EVENT2_END)) {
			event_sft = event - IMGSYS_CMDQ_SW_EVENT2_BEGIN +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event2 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_SW_EVENT3_BEGIN) &&
			(event <= IMGSYS_CMDQ_SW_EVENT3_END)) {
			event_sft = event - IMGSYS_CMDQ_SW_EVENT3_BEGIN +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event3 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_SW_EVENT4_BEGIN) &&
			(event <= IMGSYS_CMDQ_SW_EVENT4_END)) {
			event_sft = event - IMGSYS_CMDQ_SW_EVENT4_BEGIN +
				(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event4 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_SW_EVENT5_BEGIN) &&
			(event <= IMGSYS_CMDQ_SW_EVENT5_END)) {
			event_sft = event - IMGSYS_CMDQ_SW_EVENT5_BEGIN +
				(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event5 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_SW_EVENT6_BEGIN) &&
			(event <= IMGSYS_CMDQ_SW_EVENT6_END)) {
			event_sft = event - IMGSYS_CMDQ_SW_EVENT6_BEGIN +
				(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event6 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_VSDOF_EVENT_BEGIN) &&
			(event <= IMGSYS_CMDQ_VSDOF_EVENT_END)) {
			event_sft = event - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN +
				(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event vsdof timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT1_BEGIN) &&
			(event <= IMGSYS_CMDQ_AISEG_EVENT1_END)) {
			event_sft = event - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN +
				(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event aiseg timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT2_BEGIN) &&
			(event <= IMGSYS_CMDQ_AISEG_EVENT2_END)) {
			event_sft = event - IMGSYS_CMDQ_AISEG_EVENT2_BEGIN +
				(IMGSYS_CMDQ_AISEG_EVENT1_END - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN + 1) +
				(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event aiseg timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT3_BEGIN) &&
			(event <= IMGSYS_CMDQ_AISEG_EVENT3_END)) {
			event_sft = event - IMGSYS_CMDQ_AISEG_EVENT3_BEGIN +
				(IMGSYS_CMDQ_AISEG_EVENT2_END - IMGSYS_CMDQ_AISEG_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT1_END - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN + 1) +
				(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event vsdof timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT4_BEGIN) &&
			(event <= IMGSYS_CMDQ_AISEG_EVENT4_END)) {
			event_sft = event - IMGSYS_CMDQ_AISEG_EVENT4_BEGIN +
				(IMGSYS_CMDQ_AISEG_EVENT3_END - IMGSYS_CMDQ_AISEG_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT2_END - IMGSYS_CMDQ_AISEG_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT1_END - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN + 1) +
				(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event vsdof timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT5_BEGIN) &&
			(event <= IMGSYS_CMDQ_AISEG_EVENT5_END)) {
			event_sft = event - IMGSYS_CMDQ_AISEG_EVENT5_BEGIN +
				(IMGSYS_CMDQ_AISEG_EVENT4_END - IMGSYS_CMDQ_AISEG_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT3_END - IMGSYS_CMDQ_AISEG_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT2_END - IMGSYS_CMDQ_AISEG_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT1_END - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN + 1) +
				(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event vsdof timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT6_BEGIN) &&
			(event <= IMGSYS_CMDQ_AISEG_EVENT6_END)) {
			event_sft = event - IMGSYS_CMDQ_AISEG_EVENT6_BEGIN +
				(IMGSYS_CMDQ_AISEG_EVENT5_END - IMGSYS_CMDQ_AISEG_EVENT5_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT4_END - IMGSYS_CMDQ_AISEG_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT3_END - IMGSYS_CMDQ_AISEG_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT2_END - IMGSYS_CMDQ_AISEG_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT1_END - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN + 1) +
				(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event vsdof timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT7_BEGIN) &&
			(event <= IMGSYS_CMDQ_AISEG_EVENT7_END)) {
			event_sft = event - IMGSYS_CMDQ_AISEG_EVENT7_BEGIN +
				(IMGSYS_CMDQ_AISEG_EVENT6_END - IMGSYS_CMDQ_AISEG_EVENT6_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT5_END - IMGSYS_CMDQ_AISEG_EVENT5_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT4_END - IMGSYS_CMDQ_AISEG_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT3_END - IMGSYS_CMDQ_AISEG_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT2_END - IMGSYS_CMDQ_AISEG_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT1_END - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN + 1) +
				(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event vsdof timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);
		} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT8_BEGIN) &&
			(event <= IMGSYS_CMDQ_AISEG_EVENT8_END)) {
			event_sft = event - IMGSYS_CMDQ_AISEG_EVENT8_BEGIN +
				(IMGSYS_CMDQ_AISEG_EVENT7_END - IMGSYS_CMDQ_AISEG_EVENT7_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT6_END - IMGSYS_CMDQ_AISEG_EVENT6_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT5_END - IMGSYS_CMDQ_AISEG_EVENT5_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT4_END - IMGSYS_CMDQ_AISEG_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT3_END - IMGSYS_CMDQ_AISEG_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT2_END - IMGSYS_CMDQ_AISEG_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_AISEG_EVENT1_END - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN + 1) +
				(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
				(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
			event_diff = event_hist[event_sft].set.ts >
						event_hist[event_sft].wait.ts ?
						(event_hist[event_sft].set.ts -
						event_hist[event_sft].wait.ts) :
						(event_hist[event_sft].wait.ts -
						event_hist[event_sft].set.ts);
			pr_info(
				"%s: [ERROR] SW event vsdof timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang,
				event_hist[event_sft].st, event_diff,
				event_hist[event_sft].set.req_fd,
				event_hist[event_sft].set.req_no,
				event_hist[event_sft].set.frm_no,
				event_hist[event_sft].set.ts,
				event_hist[event_sft].wait.req_fd,
				event_hist[event_sft].wait.req_no,
				event_hist[event_sft].wait.frm_no,
				event_hist[event_sft].wait.ts);

		} else if ((event >= IMGSYS_CMDQ_QOF_EVENT_BEGIN) &&
			(event <= IMGSYS_CMDQ_QOF_EVENT_END)) {
			dma_addr_t err_pc;

			isQOFhang = 1;
			pr_info(
				"%s: [ERROR] QOF event timeout! wfe(%d) event(%d) isQOF(%d)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isQOFhang);
			err_pc = cmdq_pkt_get_pa_by_offset(cb_param->pkt, cb_param->pkt->err_data.offset);
			cmdq_pkt_dump_buf(cb_param->pkt, err_pc);
#ifdef IMGSYS_CMDQ_PKT_REUSE
		} else if ((event >= IMGSYS_CMDQ_PKT_REUSE_BEGIN) &&
			(event <= IMGSYS_CMDQ_PKT_REUSE_END)) {
			pr_info(
				"%s: [ERROR] PKT_REUSE event timeout! wfe(%d) event(%d)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event);
#endif
		} else if ((event >= IMGSYS_CMDQ_GPR_EVENT_BEGIN) &&
			(event <= IMGSYS_CMDQ_GPR_EVENT_END)) {
			isHWhang = 1;
			isGPRtimeout = 1;
			pr_info(
				"%s: [ERROR] GPR event timeout! wfe(%d) event(%d) isHW(%d)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang);
		} else if ((is_stream_off == 1) && (event == 0)) {
			pr_info(
				"%s: [ERROR] pipe had been turned off(%d)! wfe(%d) event(%d) isHW(%d)",
				__func__, is_stream_off,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, isHWhang);
		} else {
			isHWhang = 1;
			event_val = cmdq_get_event(imgsys_clt[0]->chan, event);
			pr_info(
				"%s: [ERROR] HW event timeout! wfe(%d) event(%d/%d) isHW(%d)",
				__func__,
				cb_param->pkt->err_data.wfe_timeout,
				cb_param->pkt->err_data.event, event_val, isHWhang);
		}

#ifdef IMGSYS_CMDQ_PKT_REUSE
		if (cb_param->pkt->loop && isHWhang)
			imgsys_cmdq_cmd_dump_plat8s(cb_param->frm_info, real_frm_idx, true);
		else
#endif
			imgsys_cmdq_cmd_dump_plat8s(cb_param->frm_info, real_frm_idx, false);

		if (cb_param->user_cmdq_err_cb) {
#if CMDQ_TIMEOUT_KTHREAD
			cb_param->fail_subfidx = real_frm_idx;
			cb_param->isHWhang = isHWhang;
			cb_param->hangEvent = event_sft + IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START;
#else
			struct cmdq_cb_data user_cb_data;

			user_cb_data.err = cb_param->err;
			user_cb_data.data = (void *)cb_param->frm_info;
			cb_param->user_cmdq_err_cb(
				user_cb_data, real_frm_idx, isHWhang,
				event_sft + IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START);
#endif
			if (isHWhang) {
				event_val = cmdq_get_event(imgsys_clt[0]->chan, event);
				pr_info(
					"%s: [ERROR] HW event is (%d/%d)",
					__func__,
					cb_param->pkt->err_data.event, event_val);
			}
		}

		if (isGPRtimeout) {
			if (cb_param->hw_comb &
				(IMGSYS_HW_FLAG_WPE_EIS|IMGSYS_HW_FLAG_WPE_TNR|IMGSYS_HW_FLAG_WPE_LITE|
				IMGSYS_HW_FLAG_WPE_DEPTH)) {
				idx = IMGSYS_MOD_WPE;
				if (imgsys_dev->modules[idx].done_chk) {
					isHwDone = imgsys_dev->modules[idx].done_chk(imgsys_dev,
						cb_param->hw_comb);
					if(!isHwDone) {
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_WPE",
						"DISPATCH:IMGSYS_WPE poll done fail, hwcomb:0x%x",
						cb_param->hw_comb);
					}
				}
			}
			if (isHwDone && (cb_param->hw_comb & (IMGSYS_HW_FLAG_TRAW|IMGSYS_HW_FLAG_LTR))) {
				idx = IMGSYS_MOD_TRAW;
				if (imgsys_dev->modules[idx].done_chk) {
					isHwDone = imgsys_dev->modules[idx].done_chk(imgsys_dev,
						cb_param->hw_comb);
					if(!isHwDone) {
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_TRAW",
						"DISPATCH:IMGSYS_TRAW poll done fail, hwcomb:0x%x",
						cb_param->hw_comb);
					}
				}
			}
			if (isHwDone && (cb_param->hw_comb & IMGSYS_HW_FLAG_DIP)) {
				idx = IMGSYS_MOD_DIP;
				if (imgsys_dev->modules[idx].done_chk) {
					isHwDone = imgsys_dev->modules[idx].done_chk(imgsys_dev,
						cb_param->hw_comb);
					if(!isHwDone) {
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_DIP",
						"DISPATCH:IMGSYS_DIP poll done fail, hwcomb:0x%x",
						cb_param->hw_comb);
				}
				}
			}
			if (isHwDone && (cb_param->hw_comb & (IMGSYS_HW_FLAG_PQDIP_A|IMGSYS_HW_FLAG_PQDIP_B))) {
				idx = IMGSYS_MOD_PQDIP;
				if (imgsys_dev->modules[idx].done_chk) {
					isHwDone = imgsys_dev->modules[idx].done_chk(imgsys_dev,
						cb_param->hw_comb);
					if(!isHwDone) {
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_PQDIP",
						"DISPATCH:IMGSYS_PQDIP poll done fail, hwcomb:0x%x",
						cb_param->hw_comb);
					}
				}
			}
			if (isHwDone && (cb_param->hw_comb & IMGSYS_HW_FLAG_ME)) {
				idx = IMGSYS_MOD_ME;
				if (imgsys_dev->modules[idx].done_chk) {
					isHwDone = imgsys_dev->modules[idx].done_chk(imgsys_dev,
						cb_param->hw_comb);
					if(!isHwDone) {
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_ME",
							"DISPATCH:IMGSYS_ME poll done fail, hwcomb:0x%x",
							cb_param->hw_comb);
					}
				}
			}
			if (isHwDone && (cb_param->hw_comb & IMGSYS_HW_FLAG_MAE)) {
				idx = IMGSYS_MOD_MAE;
				if (imgsys_dev->modules[idx].done_chk) {
					isHwDone = imgsys_dev->modules[idx].done_chk(imgsys_dev,
						cb_param->hw_comb);
					if(!isHwDone) {
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_MAE",
							"DISPATCH:IMGSYS_MAE poll done fail, hwcomb:0x%x",
							cb_param->hw_comb);
					}
				}
			}
			if (isHwDone && (cb_param->hw_comb & IMGSYS_HW_FLAG_DPE)) {
				idx = IMGSYS_MOD_DPE;
				if (imgsys_dev->modules[idx].done_chk) {
					isHwDone = imgsys_dev->modules[idx].done_chk(imgsys_dev,
						cb_param->hw_comb);
					if(!isHwDone) {
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_DPE",
							"DISPATCH:IMGSYS_DPE poll done fail, hwcomb:0x%x",
							cb_param->hw_comb);
					}
				}
			}
			if (isHwDone && (cb_param->hw_comb & IMGSYS_HW_FLAG_DFP)) {
				idx = IMGSYS_MOD_DFP;
				if (imgsys_dev->modules[idx].done_chk) {
					isHwDone = imgsys_dev->modules[idx].done_chk(imgsys_dev,
						cb_param->hw_comb);
					if(!isHwDone) {
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_DFP",
							"DISPATCH:IMGSYS_DFP poll done fail, hwcomb:0x%x",
							cb_param->hw_comb);
					}
				}
			}

			if (cb_param->hw_comb & (IMGSYS_HW_FLAG_ADL_A|IMGSYS_HW_FLAG_ADL_B)) {
				idx = IMGSYS_MOD_ADL;
				if (imgsys_dev->modules[idx].done_chk) {
					isHwDone = imgsys_dev->modules[idx].done_chk(imgsys_dev,
						cb_param->hw_comb);
					if(!isHwDone) {
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_ADL",
						"DISPATCH:IMGSYS_ADL poll done fail, hwcomb:0x%x",
						cb_param->hw_comb);
					}
				}
			}

			/* Polling timeout but all modules are done */
			if (isHwDone) {
				aee_kernel_exception("CRDISPATCH_KEY:IMGSYS",
					"DISPATCH:IMGSYS poll done fail, hwcomb:0x%x",
					cb_param->hw_comb);
			}
		}

		if (isHWhang | isQOFhang) {
			MTK_IMGSYS_QOF_NEED_RUN(imgsys_dev->qof_ver,
				mtk_imgsys_cmdq_qof_dump(cb_param->hw_comb, true);
			);
		}
#ifdef IMGSYS_CMDQ_PKT_REUSE
	} else {
		/* Reset timer if pkt reuse case */
		if ((cb_param->is_ctrl_cache == 1) && (cb_param->isFrmLast == 1))
			cmdq_thread_reset_timer(imgsys_clt[cb_param->thd_idx]->chan);
#endif
	}
	cb_param->cmdqTs.tsCmdqCbEnd = ktime_get_boottime_ns()/1000;

#ifdef IMGSYS_CMDQ_PKT_REUSE
imgsys_cmdq_queue_cb_work:
#endif
#if CMDQ_CB_KTHREAD
	kthread_init_work(&cb_param->cmdq_cb_work, imgsys_cmdq_cb_work_plat8s);
	kthread_queue_work(&imgsys_cmdq_worker, &cb_param->cmdq_cb_work);
#else
	INIT_WORK(&cb_param->cmdq_cb_work, imgsys_cmdq_cb_work_plat8s);
	queue_work(imgsys_cmdq_wq, &cb_param->cmdq_cb_work);
#endif

#ifdef IMGSYS_CMDQ_PKT_REUSE
	/* For timeout case, cmdq will not trigger no more cb. */
	/* So we have to check if there is request left in task list*/
	if (isReuseErr == 1) {
		if (g_reuse_cb_param[cb_param_pkt->thd_idx][cur_cb_idx[cb_param_pkt->thd_idx]] != NULL) {
			cb_param = g_reuse_cb_param[cb_param_pkt->thd_idx][cur_cb_idx[cb_param_pkt->thd_idx]];
			g_reuse_cb_param[cb_param_pkt->thd_idx][cur_cb_idx[cb_param_pkt->thd_idx]] = NULL;
			cur_cb_idx[cb_param_pkt->thd_idx]++;
			if (cur_cb_idx[cb_param_pkt->thd_idx] == IMGSYS_PKT_REUSE_CB_NUM)
				cur_cb_idx[cb_param_pkt->thd_idx] = 0;
			pr_info(
				"%s: [ERROR] More cb_param is left, run pkt_reuse uninit flow! pkt_cb(%p) error(%d)  gid(%d) for frm(%d/%d) blk(%d/%d) ofst(0x%lx) task(%d/%d/%d) thd_idx(%d) cb_idx(%d/%d) cookie(%u) cb_cnt(%u)",
				__func__, cb_param, pkt_err, cb_param->group_id,
				cb_param->frm_idx, cb_param->frm_num,
				cb_param->blk_idx, cb_param->blk_num,
				cb_param->pkt->err_data.offset,
				cb_param->task_id, cb_param->task_num, cb_param->task_cnt, cb_param->thd_idx,
				cur_cb_idx[cb_param->thd_idx], g_cb_idx[cb_param->thd_idx], cookie, cb_cnt);
		} else {
			isReuseErr = 0;
			pr_info(
				"%s: [ERROR] No more cb_param is left, run pkt_reuse uninit flow! pkt_cb(%p) error(%d)  gid(%d) for frm(%d/%d) blk(%d/%d) ofst(0x%lx) task(%d/%d/%d) thd_idx(%d) cb_idx(%d/%d) cookie(%u) cb_cnt(%u)",
				__func__, cb_param_pkt, pkt_err, cb_param_pkt->group_id,
				cb_param_pkt->frm_idx, cb_param_pkt->frm_num,
				cb_param_pkt->blk_idx, cb_param_pkt->blk_num,
				cb_param_pkt->pkt->err_data.offset,
				cb_param_pkt->task_id, cb_param_pkt->task_num, cb_param_pkt->task_cnt,
				cb_param_pkt->thd_idx,
				cur_cb_idx[cb_param_pkt->thd_idx], g_cb_idx[cb_param_pkt->thd_idx], cookie, cb_cnt);
				cb_param = cb_param_pkt;
		}
		goto imgsys_cmdq_queue_cb_work;
	}
#endif
}

int imgsys_cmdq_task_aee_cb_plat8s(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt = NULL;
	struct mtk_imgsys_cb_param *cb_param = NULL;
	struct mtk_imgsys_pipe *pipe = NULL;
	size_t err_ofst;
	u32 idx = 0, err_idx = 0, real_frm_idx = 0;
	u16 event = 0, event_sft = 0;
	u64 event_diff = 0;
	bool isHWhang = 0;
	enum cmdq_aee_type ret = CMDQ_NO_AEE;
	bool isGPRtimeout = 0;

	pkt = (struct cmdq_pkt *)data.data;
	cb_param = pkt->user_priv;

	err_ofst = cb_param->pkt->err_data.offset;
	err_idx = 0;
#ifdef IMGSYS_CMDQ_PKT_REUSE
	if (pkt->loop == true) {
		if (g_reuse_cb_param[cb_param->thd_idx][cur_cb_idx[cb_param->thd_idx]] != NULL) {
			cb_param = g_reuse_cb_param[cb_param->thd_idx][cur_cb_idx[cb_param->thd_idx]];
			pr_info(
				"%s: [ERROR] pkt_cb(%p) real_cb(%p) error(%d)  gid(%d) for frm(%d/%d) blk(%d/%d) ofst(0x%lx) erridx(%d/%d) task(%d/%d/%d) thd_idx(%d) cb_idx(%d/%d)",
				__func__, pkt->user_priv, cb_param, cb_param->err, cb_param->group_id,
				cb_param->frm_idx, cb_param->frm_num,
				cb_param->blk_idx, cb_param->blk_num,
				cb_param->pkt->err_data.offset,
				err_idx, real_frm_idx,
				cb_param->task_id, cb_param->task_num, cb_param->task_cnt, cb_param->thd_idx,
				cur_cb_idx[cb_param->thd_idx], g_cb_idx[cb_param->thd_idx]);
		} else {
			pr_info(
				"%s: [ERROR] No more cb_param is left, run pkt_reuse uninit flow! pkt_cb(%p) error(%d)  gid(%d) for frm(%d/%d) blk(%d/%d) ofst(0x%lx) erridx(%d/%d) task(%d/%d/%d) thd_idx(%d) cb_idx(%d/%d)",
				__func__, cb_param, cb_param->err, cb_param->group_id,
				cb_param->frm_idx, cb_param->frm_num,
				cb_param->blk_idx, cb_param->blk_num,
				cb_param->pkt->err_data.offset,
				err_idx, real_frm_idx,
				cb_param->task_id, cb_param->task_num, cb_param->task_cnt, cb_param->thd_idx,
				cur_cb_idx[cb_param->thd_idx], g_cb_idx[cb_param->thd_idx]);
			return ret;
		}
	}
#endif
	for (idx = 0; idx < cb_param->task_cnt; idx++)
		if (err_ofst > cb_param->pkt_ofst[idx])
			err_idx++;
		else
			break;
	if (err_idx >= cb_param->task_cnt) {
		pr_info(
			"%s: [ERROR] can't find task in task list! cb(%p) error(%d)  gid(%d) for frm(%d/%d) blk(%d/%d) ofst(0x%lx) erridx(%d/%d) task(%d/%d/%d) ofst(%lx/%lx/%lx/%lx/%lx)",
			__func__, cb_param, cb_param->err, cb_param->group_id,
			cb_param->frm_idx, cb_param->frm_num,
			cb_param->blk_idx, cb_param->blk_num,
			cb_param->pkt->err_data.offset,
			err_idx, real_frm_idx,
			cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
			cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
			cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);
		err_idx = cb_param->task_cnt - 1;
	}
	real_frm_idx = cb_param->frm_idx - (cb_param->task_cnt - 1) + err_idx;
	pr_info(
		"%s: [ERROR] cb(%p) req fd/no(%d/%d) frame no(%d) error(%d) gid(%d) clt(0x%lx) hw_comb(0x%x) for frm(%d/%d) blk(%d/%d) lst(%d/%d/%d) earlycb(%d) ofst(0x%lx) erridx(%d/%d) task(%d/%d/%d) ofst(%lx/%lx/%lx/%lx/%lx)",
		__func__, cb_param, cb_param->req_fd,
		cb_param->req_no, cb_param->frm_no,
		cb_param->err, cb_param->group_id,
		(unsigned long)cb_param->clt, cb_param->hw_comb,
		cb_param->frm_idx, cb_param->frm_num,
		cb_param->blk_idx, cb_param->blk_num,
		cb_param->isBlkLast, cb_param->isFrmLast, cb_param->isTaskLast,
		cb_param->is_earlycb, cb_param->pkt->err_data.offset,
		err_idx, real_frm_idx,
		cb_param->task_id, cb_param->task_num, cb_param->task_cnt,
		cb_param->pkt_ofst[0], cb_param->pkt_ofst[1], cb_param->pkt_ofst[2],
		cb_param->pkt_ofst[3], cb_param->pkt_ofst[4]);
	if (is_stream_off == 1)
		pr_info("%s: [ERROR] cb(%p) pipe had been turned off(%d)!\n",
			__func__, cb_param, is_stream_off);
	pipe = cb_param->pipe;
	if (!pipe->streaming) {
		/* is_stream_off = 1; */
		pr_info("%s: [ERROR] cb(%p) pipe already streamoff(%d)\n",
			__func__, cb_param, is_stream_off);
	}

	event = cb_param->pkt->err_data.event;
	if ((event >= IMGSYS_CMDQ_HW_TOKEN_BEGIN) &&
		(event <= IMGSYS_CMDQ_HW_TOKEN_END)) {
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] HW token timeout! wfe(%d) event(%d) isHW(%d)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang);
	} else if ((event >= IMGSYS_CMDQ_SW_EVENT1_BEGIN) &&
		(event <= IMGSYS_CMDQ_SW_EVENT1_END)) {
		event_sft = event - IMGSYS_CMDQ_SW_EVENT1_BEGIN;
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event1 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_SW_EVENT2_BEGIN) &&
		(event <= IMGSYS_CMDQ_SW_EVENT2_END)) {
		event_sft = event - IMGSYS_CMDQ_SW_EVENT2_BEGIN +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event2 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_SW_EVENT3_BEGIN) &&
		(event <= IMGSYS_CMDQ_SW_EVENT3_END)) {
		event_sft = event - IMGSYS_CMDQ_SW_EVENT3_BEGIN +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event3 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_SW_EVENT4_BEGIN) &&
		(event <= IMGSYS_CMDQ_SW_EVENT4_END)) {
		event_sft = event - IMGSYS_CMDQ_SW_EVENT4_BEGIN +
			(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event4 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_SW_EVENT5_BEGIN) &&
		(event <= IMGSYS_CMDQ_SW_EVENT5_END)) {
		event_sft = event - IMGSYS_CMDQ_SW_EVENT5_BEGIN +
			(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event5 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_SW_EVENT6_BEGIN) &&
		(event <= IMGSYS_CMDQ_SW_EVENT6_END)) {
		event_sft = event - IMGSYS_CMDQ_SW_EVENT6_BEGIN +
			(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event6 timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_VSDOF_EVENT_BEGIN) &&
		(event <= IMGSYS_CMDQ_VSDOF_EVENT_END)) {
		event_sft = event - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN +
			(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event vsdof timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT1_BEGIN) &&
		(event <= IMGSYS_CMDQ_AISEG_EVENT1_END)) {
		event_sft = event - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN +
			(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event aiseg timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT2_BEGIN) &&
		(event <= IMGSYS_CMDQ_AISEG_EVENT2_END)) {
		event_sft = event - IMGSYS_CMDQ_AISEG_EVENT2_BEGIN +
			(IMGSYS_CMDQ_AISEG_EVENT1_END - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN + 1) +
			(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event aiseg timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT3_BEGIN) &&
		(event <= IMGSYS_CMDQ_AISEG_EVENT3_END)) {
		event_sft = event - IMGSYS_CMDQ_AISEG_EVENT3_BEGIN +
			(IMGSYS_CMDQ_AISEG_EVENT2_END - IMGSYS_CMDQ_AISEG_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT1_END - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN + 1) +
			(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event vsdof timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT4_BEGIN) &&
		(event <= IMGSYS_CMDQ_AISEG_EVENT4_END)) {
		event_sft = event - IMGSYS_CMDQ_AISEG_EVENT4_BEGIN +
			(IMGSYS_CMDQ_AISEG_EVENT3_END - IMGSYS_CMDQ_AISEG_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT2_END - IMGSYS_CMDQ_AISEG_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT1_END - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN + 1) +
			(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event vsdof timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT5_BEGIN) &&
		(event <= IMGSYS_CMDQ_AISEG_EVENT5_END)) {
		event_sft = event - IMGSYS_CMDQ_AISEG_EVENT5_BEGIN +
			(IMGSYS_CMDQ_AISEG_EVENT4_END - IMGSYS_CMDQ_AISEG_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT3_END - IMGSYS_CMDQ_AISEG_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT2_END - IMGSYS_CMDQ_AISEG_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT1_END - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN + 1) +
			(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event vsdof timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT6_BEGIN) &&
		(event <= IMGSYS_CMDQ_AISEG_EVENT6_END)) {
		event_sft = event - IMGSYS_CMDQ_AISEG_EVENT6_BEGIN +
			(IMGSYS_CMDQ_AISEG_EVENT5_END - IMGSYS_CMDQ_AISEG_EVENT5_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT4_END - IMGSYS_CMDQ_AISEG_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT3_END - IMGSYS_CMDQ_AISEG_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT2_END - IMGSYS_CMDQ_AISEG_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT1_END - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN + 1) +
			(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event vsdof timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT7_BEGIN) &&
		(event <= IMGSYS_CMDQ_AISEG_EVENT7_END)) {
		event_sft = event - IMGSYS_CMDQ_AISEG_EVENT7_BEGIN +
			(IMGSYS_CMDQ_AISEG_EVENT6_END - IMGSYS_CMDQ_AISEG_EVENT6_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT5_END - IMGSYS_CMDQ_AISEG_EVENT5_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT4_END - IMGSYS_CMDQ_AISEG_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT3_END - IMGSYS_CMDQ_AISEG_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT2_END - IMGSYS_CMDQ_AISEG_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT1_END - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN + 1) +
			(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event vsdof timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);
	} else if ((event >= IMGSYS_CMDQ_AISEG_EVENT8_BEGIN) &&
		(event <= IMGSYS_CMDQ_AISEG_EVENT8_END)) {
		event_sft = event - IMGSYS_CMDQ_AISEG_EVENT8_BEGIN +
			(IMGSYS_CMDQ_AISEG_EVENT7_END - IMGSYS_CMDQ_AISEG_EVENT7_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT6_END - IMGSYS_CMDQ_AISEG_EVENT6_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT5_END - IMGSYS_CMDQ_AISEG_EVENT5_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT4_END - IMGSYS_CMDQ_AISEG_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT3_END - IMGSYS_CMDQ_AISEG_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT2_END - IMGSYS_CMDQ_AISEG_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_AISEG_EVENT1_END - IMGSYS_CMDQ_AISEG_EVENT1_BEGIN + 1) +
			(IMGSYS_CMDQ_VSDOF_EVENT_END - IMGSYS_CMDQ_VSDOF_EVENT_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT6_END - IMGSYS_CMDQ_SW_EVENT6_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT5_END - IMGSYS_CMDQ_SW_EVENT5_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT4_END - IMGSYS_CMDQ_SW_EVENT4_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT3_END - IMGSYS_CMDQ_SW_EVENT3_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT2_END - IMGSYS_CMDQ_SW_EVENT2_BEGIN + 1) +
			(IMGSYS_CMDQ_SW_EVENT1_END - IMGSYS_CMDQ_SW_EVENT1_BEGIN + 1);
		event_diff = event_hist[event_sft].set.ts >
					event_hist[event_sft].wait.ts ?
					(event_hist[event_sft].set.ts -
					event_hist[event_sft].wait.ts) :
					(event_hist[event_sft].wait.ts -
					event_hist[event_sft].set.ts);
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] SW event vsdof timeout! wfe(%d) event(%d) isHW(%d); event st(%d)_ts(%lld)_set(%d/%d/%d/%lld)_wait(%d/%d/%d/%lld)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang,
			event_hist[event_sft].st, event_diff,
			event_hist[event_sft].set.req_fd,
			event_hist[event_sft].set.req_no,
			event_hist[event_sft].set.frm_no,
			event_hist[event_sft].set.ts,
			event_hist[event_sft].wait.req_fd,
			event_hist[event_sft].wait.req_no,
			event_hist[event_sft].wait.frm_no,
			event_hist[event_sft].wait.ts);

	} else if ((event >= IMGSYS_CMDQ_QOF_EVENT_BEGIN) &&
		(event <= IMGSYS_CMDQ_QOF_EVENT_END)) {
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] QOF event timeout! wfe(%d) event(%d)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event);
#ifdef IMGSYS_CMDQ_PKT_REUSE
	} else if ((event >= IMGSYS_CMDQ_PKT_REUSE_BEGIN) &&
		(event <= IMGSYS_CMDQ_PKT_REUSE_END)) {
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] PKT_REUSE event timeout! wfe(%d) event(%d)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event);
#endif
	} else if ((event >= IMGSYS_CMDQ_GPR_EVENT_BEGIN) &&
		(event <= IMGSYS_CMDQ_GPR_EVENT_END)) {
		isHWhang = 1;
		isGPRtimeout = 1;
		pkt->timeout_dump_hw_trace = 1;
		ret = CMDQ_NO_AEE_DUMP;
		pr_info(
			"%s: [ERROR] GPR event timeout! wfe(%d) event(%d) isHW(%d)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang);
	} else if ((is_stream_off == 1) && (event == 0)) {
		ret = CMDQ_NO_AEE;
		pr_info(
			"%s: [ERROR] pipe had been turned off(%d)! wfe(%d) event(%d) isHW(%d)",
			__func__, is_stream_off,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang);
	} else {
		isHWhang = 1;
		pkt->timeout_dump_hw_trace = 1;
		ret = CMDQ_AEE_EXCEPTION;
		pr_info(
			"%s: [ERROR] HW event timeout! wfe(%d) event(%d) isHW(%d)",
			__func__,
			cb_param->pkt->err_data.wfe_timeout,
			cb_param->pkt->err_data.event, isHWhang);
	}

	return ret;
}

#ifdef IMGSYS_CMDQ_PKT_REUSE
bool imgsys_cmdq_task_skip_timeout_cb_plat8s(void *data)
{
	struct cmdq_skip_timeout_cb_data *skip_data;
	s32 event;
	bool ret = false;

	skip_data = (struct cmdq_skip_timeout_cb_data *)data;
	event = cmdq_get_inst_event(skip_data->pkt, skip_data->pa_curr);
	if ((skip_data->pkt->loop == true) && (event > 0)){
		if ((event >= IMGSYS_CMDQ_PKT_REUSE_BEGIN) &&
		(event <= IMGSYS_CMDQ_PKT_REUSE_END)) {
			/* Do skip timeout due to pkt reuse event happened */
			pr_info(
			"%s: Do skip timeout due to pkt(%p) reuse event(%d) happened!",
			__func__, skip_data->pkt, event);
			ret = true;
		}
	}
	return ret;
}
#endif

int imgsys_cmdq_sendtask_plat8s(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				void (*cmdq_cb)(struct cmdq_cb_data data,
					uint32_t subfidx, bool isLastTaskInReq,
					uint32_t batchnum, uint32_t memory_mode),
				void (*cmdq_err_cb)(struct cmdq_cb_data data,
					uint32_t fail_subfidx, bool isHWhang, uint32_t hangEvent),
				u64 (*imgsys_get_iova)(struct dma_buf *dma_buf, s32 ionFd,
					struct mtk_imgsys_dev *imgsys_dev,
					struct mtk_imgsys_dev_buffer *dev_buf),
				u64 (*imgsys_get_kva)(struct dma_buf *dma_buf, s32 ionFd,
					struct mtk_imgsys_dev *imgsys_dev,
					struct mtk_imgsys_dev_buffer *dev_buf),
				int (*is_singledev_mode)(struct mtk_imgsys_request *req))
{
	struct cmdq_client *clt = NULL;
	struct cmdq_pkt *pkt = NULL;
	struct GCERecoder *cmd_buf = NULL;
	struct Command *cmd = NULL;
	struct mtk_imgsys_cb_param *cb_param = NULL;
	struct mtk_imgsys_dvfs *dvfs_info = NULL;
	dma_addr_t pkt_ts_pa = 0;
	u32 *pkt_ts_va = NULL;
	u32 pkt_ts_num = 0;
	u32 pkt_ts_ofst = 0;
	u32 cmd_num = 0;
	u32 cmd_idx = 0;
	u32 blk_idx = 0; /* For Vss block cnt */
	u32 blk_num = 0;
	u32 thd_idx = 0;
	u32 hw_comb = 0;
	int ret = 0, ret_flush = 0, ret_sn = 0;
	u64 tsReqStart = 0;
	u64 tsDvfsQosStart = 0, tsDvfsQosEnd = 0;
	u32 frm_num = 0;
	int frm_idx = 0;
	u32 cmd_ofst = 0;
	bool isPack = 0;
	u32 task_idx = 0;
	u32 task_id = 0;
	u32 task_num = 0;
	u32 task_cnt = 0;
	bool qof_need_sub[ISP8S_PWR_NUM] = {0};
	bool is_qof_sec_mode = false;
	size_t pkt_ofst[MAX_FRAME_IN_TASK] = {0};
	char logBuf_temp[MTK_IMGSYS_LOG_LENGTH];
	u64 tsflushStart = 0, tsFlushEnd = 0;
	bool isTimeShared = 0;
	u32 log_sz = 0;
	u32 cb_param_cnt = 0;
	bool need_dip_cine = false;
#ifdef IMGSYS_MAE_WRITE_BACK_SUPPORT
	struct mtk_imgsys_hw_info mae_info = {0};
#endif
#ifdef IMGSYS_CMDQ_PKT_REUSE
	u32 reuse_cmd_num = 0;
	u32 reuse_event_num = 0;
	u32 reuse_task_idx = 0;
#endif
	struct retry_event_table retry_tbl = {0};

	dvfs_info = &imgsys_dev->dvfs_info;
	/* PMQOS API */
	tsDvfsQosStart = ktime_get_boottime_ns()/1000;
	IMGSYS_CMDQ_SYSTRACE_BEGIN("%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d Own:%llx",
		__func__, "dvfs_qos", frm_info->frame_no, frm_info->request_no,
		frm_info->request_fd, frm_info->frm_owner);
	mutex_lock(&(imgsys_dev->dvfs_qos_lock));
	#if DVFS_QOS_READY
	mtk_imgsys_mmdvfs_mmqos_cal_plat8s(imgsys_dev, frm_info, 1);
	mtk_imgsys_mmdvfs_set_plat8s(imgsys_dev, frm_info, 1);
	MTK_IMGSYS_QOS_ENABLE(!imgsys_dev->hwqos_info.hwqos_support,
		mtk_imgsys_mmqos_set_by_scen_plat8s(imgsys_dev, frm_info, 1);
	);
	#endif
	mutex_unlock(&(imgsys_dev->dvfs_qos_lock));
	IMGSYS_CMDQ_SYSTRACE_END();
	tsDvfsQosEnd = ktime_get_boottime_ns()/1000;

	/* is_stream_off = 0; */
	frm_num = frm_info->total_frmnum;
	frm_info->cb_frmcnt = 0;
	frm_info->total_taskcnt = 0;
	cmd_ofst = sizeof(struct GCERecoder);

	#if IMGSYS_SECURE_ENABLE
	mutex_lock(&(imgsys_dev->sec_task_lock));
	if (frm_info->is_secReq && (is_sec_task_create == 0)) {
		is_pwr_sec_mode = true;

		/* start sec work */
		imgsys_cmdq_sec_sendtask_plat8s(imgsys_dev);
		is_sec_task_create = 1;
		pr_info(
			"%s: create imgsys secure task is_secReq(%d)\n",
			__func__, frm_info->is_secReq);
	}
	is_qof_sec_mode = is_pwr_sec_mode;
	mutex_unlock(&(imgsys_dev->sec_task_lock));
	#endif

	/* Allocate cmdq buffer for task timestamp */
	if (imgsys_cmdq_ts_enable_plat8s() || imgsys_wpe_bwlog_enable_plat8s()) {
		pkt_ts_va = cmdq_mbox_buf_alloc(imgsys_clt[0], &pkt_ts_pa);
		/* if (imgsys_cmdq_ts_dbg_enable_plat8s()) { */
			log_sz = ((frm_num / 10) + 1) * 4;
			frm_info->hw_ts_log = vzalloc(sizeof(char)*MTK_IMGSYS_LOG_LENGTH*log_sz);
			if (frm_info->hw_ts_log == NULL)
				return -ENOMEM;
			memset((char *)frm_info->hw_ts_log, 0x0, MTK_IMGSYS_LOG_LENGTH*log_sz);
			frm_info->hw_ts_log[strlen(frm_info->hw_ts_log)] = '\0';
			memset((char *)logBuf_temp, 0x0, MTK_IMGSYS_LOG_LENGTH);
			logBuf_temp[strlen(logBuf_temp)] = '\0';
			ret_sn = snprintf(logBuf_temp, MTK_IMGSYS_LOG_LENGTH,
				"Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d Own:%s gid(%d) fps/bch(%d/%d) dvfs_v/f(%d/%ld) freq(%ld/%ld/%ld/%ld)",
				frm_info->frame_no, frm_info->request_no,
				frm_info->request_fd, (char *)(&(frm_info->frm_owner)),
				frm_info->group_id, frm_info->fps, frm_info->batchnum,
				dvfs_info->cur_volt, dvfs_info->cur_freq,
				dvfs_info->freq, dvfs_info->pixel_size[0],
				dvfs_info->pixel_size[1], dvfs_info->pixel_size[2]);
			if (ret_sn < 0)
				pr_info("%s: [ERROR] snprintf fail: %d\n", __func__, ret_sn);
			strncat(frm_info->hw_ts_log, logBuf_temp, strlen(logBuf_temp));
		/* } */
	}

	for (frm_idx = 0; frm_idx < frm_num; frm_idx++) {
		cmd_buf = (struct GCERecoder *)frm_info->user_info[frm_idx].g_swbuf;

		if ((cmd_buf->header_code != 0x5A5A5A5A) ||
				(cmd_buf->check_pre != 0x55AA55AA) ||
				(cmd_buf->check_post != 0xAA55AA55) ||
				(cmd_buf->footer_code != 0xA5A5A5A5)) {
			pr_info("%s: Incorrect guard word: %08x/%08x/%08x/%08x", __func__,
				cmd_buf->header_code, cmd_buf->check_pre, cmd_buf->check_post,
				cmd_buf->footer_code);
			return -1;
		}

		cmd_num = cmd_buf->curr_length / sizeof(struct Command);
		cmd = (struct Command *)((unsigned long)(frm_info->user_info[frm_idx].g_swbuf) +
			(unsigned long)(cmd_buf->cmd_offset));
		blk_num = cmd_buf->frame_block;
		hw_comb = frm_info->user_info[frm_idx].hw_comb;
		isTimeShared = frm_info->user_info[frm_idx].is_time_shared;

		if (isPack == 0) {
			if (frm_info->group_id == -1) {
				/* Choose cmdq_client base on hw scenario */
				for (thd_idx = 0; thd_idx < IMGSYS_NOR_THD; thd_idx++) {
					if (hw_comb & 0x1) {
						clt = imgsys_clt[thd_idx];
					if (imgsys_cmdq_dbg_enable_plat8s())
						pr_debug(
						"%s: chosen mbox thread (%d, 0x%lx) for frm(%d/%d)\n",
						__func__, thd_idx, (unsigned long)clt, frm_idx, frm_num);
						break;
					}
					hw_comb = hw_comb>>1;
				}
				/* This segment can be removed since user had set dependency */
				if (frm_info->user_info[frm_idx].hw_comb & IMGSYS_HW_FLAG_DIP) {
					thd_idx = 4;
					clt = imgsys_clt[thd_idx];
				}
			} else {
				if (frm_info->group_id < IMGSYS_NOR_THD) {
					thd_idx = frm_info->group_id;
					clt = imgsys_clt[thd_idx];
				} else {
					pr_info(
						"%s: [ERROR] group_id(%d) is over max hw num(%d),hw_comb(0x%x) for frm(%d/%d)!\n",
						__func__, frm_info->group_id, IMGSYS_NOR_THD,
						frm_info->user_info[frm_idx].hw_comb,
						frm_idx, frm_num);
					return -1;
				}
			}

			/* This is work around for low latency flow.		*/
			/* If we change to request base,			*/
			/* we don't have to take this condition into account.	*/
			if (frm_info->sync_id != -1) {
				thd_idx = 0;
				clt = imgsys_clt[thd_idx];
			}

			if (clt == NULL) {
				pr_info("%s: [ERROR] No HW Found (0x%x) for frm(%d/%d)!\n",
					__func__, frm_info->user_info[frm_idx].hw_comb,
					frm_idx, frm_num);
				return -1;
			}
		}

#ifdef IMGSYS_CMDQ_PKT_REUSE
		/* Bypass pkt reuse flow for smvr and secure camera */
		if ((frm_info->batchnum != 0) || (frm_info->is_secReq != 0) ||
			(thd_idx < IMGSYS_MCNR_THD_START) || (thd_idx > IMGSYS_MCNR_THD_END) ||
			(imgsys_cmdq_pkt_reuse_disable_plat8s()))
			frm_info->is_ctrl_cache = 0;

		/* This segment deal with scenario change from isCtrlCache=1 to isCtrlCache=0. */
		/* We need to wait previous pkt done and reset all reuse parameter. */
		if ((frm_info->is_ctrl_cache <= 0) &&
			(is_pkt_created[thd_idx] == IMGSYS_PKT_REUSE_POOL_NUM)) {
			/* Check all callback is done */
			while ((g_cb_idx[thd_idx] != cur_cb_idx[thd_idx]) &&
				(g_reuse_cb_param[thd_idx][cur_cb_idx[thd_idx]] != NULL)) {
				dev_dbg(imgsys_dev->dev,
					"%s: wait for pkt_reuse job done, thd_idx(%d), cb_idx(%d/%d)\n",
					__func__, thd_idx, cur_cb_idx[thd_idx], g_cb_idx[thd_idx]);
				usleep_range(1000, 1050);
			}
			cmdq_mbox_stop(imgsys_clt[thd_idx]);
			is_pkt_created[thd_idx] = 0;
			g_pkt_reuse[thd_idx] = NULL;
			cur_cmd_block[thd_idx] = 0;
			g_reuse_cmd_num[thd_idx] = 0;
			g_reuse_cmd_num_max[thd_idx] = 0;
			g_reuse_event_num[thd_idx] = 0;
			g_reuse_event_num_max[thd_idx] = 0;
			g_cb_idx[thd_idx] = 0;
			cur_cb_idx[thd_idx] = 0;
			for (int pkt_idx = 0; pkt_idx < IMGSYS_PKT_REUSE_POOL_NUM; pkt_idx++) {
				cmdq_clear_event(imgsys_clt[0]->chan,
					imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_PKT_REUSE_POOL_0].event +
					(thd_idx * IMGSYS_PKT_REUSE_POOL_NUM) + pkt_idx);
			}
		} else if ((frm_idx == 0) && (g_reuse_cb_param[thd_idx][cur_cb_idx[thd_idx]] == NULL) &&
			((frm_info->is_ctrl_cache == 1) && (is_pkt_created[thd_idx] == IMGSYS_PKT_REUSE_POOL_NUM))) {
			/* Reset pkt_reuse timer due to new task coming w/o remaining task in task list */
			if (imgsys_cmdq_dbg_enable_plat8s())
				dev_dbg(imgsys_dev->dev,
					"%s: reset pkt_reuse timer, thd_idx(%d), cb_idx(%d/%d)\n",
					__func__, thd_idx, cur_cb_idx[thd_idx], g_cb_idx[thd_idx]);
			cmdq_thread_reset_timer(imgsys_clt[thd_idx]->chan);
		}
#endif

#ifdef IMGSYS_CMDQ_PKT_REUSE
		if (imgsys_cmdq_dbg_enable_plat8s() ||
			((frm_info->is_ctrl_cache == 1) &&
			(is_pkt_created[thd_idx] == 0)))
#else
		if (imgsys_cmdq_dbg_enable_plat8s())
#endif
			dev_dbg(imgsys_dev->dev,
				"%s: req fd/no(%d/%d) frame no(%d) frm(%d/%d) cmd_oft(0x%x/0x%x), cmd_len(%d), num(%d), sz_per_cmd(%lu), frm_blk(%d), hw_comb(0x%x), sync_id(%d), gce_thd(%d), gce_clt(0x%lx), ctrl_cache(%d)\n",
				__func__, frm_info->request_fd, frm_info->request_no, frm_info->frame_no,
				frm_idx, frm_num, cmd_buf->cmd_offset, cmd_ofst, cmd_buf->curr_length,
				cmd_num, sizeof(struct Command), cmd_buf->frame_block,
				frm_info->user_info[frm_idx].hw_comb, frm_info->sync_id, thd_idx, (unsigned long)clt,
				frm_info->is_ctrl_cache);

		cmd_idx = 0;
		if (isTimeShared)
			mutex_lock(&(imgsys_dev->vss_blk_lock));
		for (blk_idx = 0; blk_idx < blk_num; blk_idx++) {
			tsReqStart = ktime_get_boottime_ns()/1000;
			if (isPack == 0) {
				/* create pkt and hook clt as pkt's private data */
#ifdef IMGSYS_CMDQ_PKT_REUSE
				if ((frm_info->is_ctrl_cache <= 0) ||
					(((frm_info->is_ctrl_cache == 1) &&
					(is_pkt_created[thd_idx] == 0) &&
					(frm_idx == 0))))
					pkt = cmdq_pkt_create(clt);
				else
					pkt = g_pkt_reuse[thd_idx];
#else
				pkt = cmdq_pkt_create(clt);
#endif
				if (pkt == NULL) {
					pr_info(
						"%s: [ERROR] cmdq_pkt_create fail in block(%d)!\n",
						__func__, blk_idx);
					if (isTimeShared)
						mutex_unlock(&(imgsys_dev->vss_blk_lock));
					return -1;
				}
#ifdef IMGSYS_CMDQ_PKT_REUSE
				/* Add wait event for block cmd */
				if ((frm_info->is_ctrl_cache == 1) &&
					(is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM) &&
					(frm_idx == 0)) {
					cmdq_pkt_wfe(pkt,
						imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_PKT_REUSE_POOL_0].event +
						(thd_idx * IMGSYS_PKT_REUSE_POOL_NUM) + is_pkt_created[thd_idx]);
					cmdq_pkt_jump(pkt, CMDQ_JUMP_PASS);
				}
#endif
				if (imgsys_cmdq_dbg_enable_plat8s())
					pr_debug(
						"%s: cmdq_pkt_create success(0x%lx) in block(%d) for frm(%d/%d)\n",
						__func__, (unsigned long)pkt, blk_idx, frm_idx, frm_num);
				/* Reset pkt timestamp num */
				pkt_ts_num = 0;
			}

			if (is_qof_sec_mode == false) {
#ifdef IMGSYS_CMDQ_PKT_REUSE
				if ((frm_info->is_ctrl_cache <= 0) ||
					(((frm_info->is_ctrl_cache == 1) &&
					(is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM))))
#endif
					MTK_IMGSYS_QOF_NEED_RUN(imgsys_dev->qof_ver,
						mtk_imgsys_cmdq_qof_add(pkt, qof_need_sub,
							frm_info->user_info[frm_idx].hw_comb,
							&frm_info->user_info[frm_idx],
							frm_info->memory_mode,
							&need_dip_cine, is_qof_sec_mode);
					);
			} else {
				//enable all mtcmos
				MTK_IMGSYS_QOF_NEED_RUN(imgsys_dev->qof_ver,
					mtk_imgsys_cmdq_qof_add(pkt, qof_need_sub,
					pwr_group[ISP8S_PWR_DIP] | pwr_group[ISP8S_PWR_TRAW] |
					pwr_group[ISP8S_PWR_WPE_1_EIS] | pwr_group[ISP8S_PWR_WPE_2_TNR] |
					pwr_group[ISP8S_PWR_WPE_3_LITE],
					&frm_info->user_info[frm_idx],
					frm_info->memory_mode,
					&need_dip_cine, is_qof_sec_mode);
				);
			}

			IMGSYS_CMDQ_SYSTRACE_BEGIN(
				"%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d fidx:%d hw_comb:0x%x Own:%llx frm(%d/%d) blk(%d)",
				__func__, "cmd_parser", frm_info->frame_no, frm_info->request_no,
				frm_info->request_fd, frm_info->user_info[frm_idx].subfrm_idx,
				frm_info->user_info[frm_idx].hw_comb, frm_info->frm_owner,
				frm_idx, frm_num, blk_idx);

			if (imgsys_cmdq_dbg_enable_plat8s())
				pr_debug("%s, is_secFrm = %d.",
					__func__, frm_info->user_info[frm_idx].is_secFrm);
#ifdef IMGSYS_CMDQ_PKT_REUSE
			if ((frm_info->is_ctrl_cache <= 0) ||
				(((frm_info->is_ctrl_cache == 1) &&
				(is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM))))
#endif
				MTK_IMGSYS_QOS_ENABLE(imgsys_dev->hwqos_info.hwqos_support,
					mtk_imgsys_cmdq_hwqos_report(
						pkt, &imgsys_dev->hwqos_info,
						&frm_info->fps, &frm_info->user_info[frm_idx].boost);
				);
			ret = imgsys_cmdq_parser_plat8s(imgsys_dev, frm_info, pkt,
				&cmd[cmd_idx], hw_comb, frm_info->user_info[frm_idx].sw_ridx,
				(pkt_ts_pa + 4 * pkt_ts_ofst), &pkt_ts_num, thd_idx,
				imgsys_get_iova, imgsys_get_kva, is_singledev_mode, &mae_info, &retry_tbl);
			if (ret < 0) {
				pr_info(
					"%s: [ERROR] parsing idx(%d) with cmd(%d) in block(%d) for frm(%d/%d) fail\n",
					__func__, cmd_idx, cmd[cmd_idx].opcode,
					blk_idx, frm_idx, frm_num);
#ifdef IMGSYS_CMDQ_PKT_REUSE
				if ((frm_info->is_ctrl_cache <= 0) ||
				(((frm_info->is_ctrl_cache == 1) &&
				(is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM))))
					cmdq_pkt_destroy_no_wq(pkt);
				else
					cmdq_mbox_stop(imgsys_clt[thd_idx]);
#else
				cmdq_pkt_destroy_no_wq(pkt);
#endif
				if (isTimeShared)
					mutex_unlock(&(imgsys_dev->vss_blk_lock));
				goto sendtask_done;
			}
			cmd_idx += ret;

			IMGSYS_CMDQ_SYSTRACE_END();

			/* Check for packing gce task */
			pkt_ofst[task_cnt] = pkt->cmd_buf_size - CMDQ_INST_SIZE;
#ifdef IMGSYS_CMDQ_PKT_REUSE
			if ((frm_info->is_ctrl_cache == 1) &&
				(is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM)) {
				g_pkt_reuse_ofst[thd_idx][is_pkt_created[thd_idx]][frm_idx] =
					pkt_ofst[task_cnt];
				if (imgsys_cmdq_dbg_enable_plat8s()) {
					dev_dbg(imgsys_dev->dev,
						"%s: req fd/no(%d/%d) frame no(%d) frm(%d/%d) g_pkt_reuse_ofst[%d][%d][%d] = 0x%lx\n",
						__func__,
						frm_info->request_fd, frm_info->request_no, frm_info->frame_no,
						frm_idx, frm_num, thd_idx, is_pkt_created[thd_idx], frm_idx,
						g_pkt_reuse_ofst[thd_idx][is_pkt_created[thd_idx]][frm_idx]);
				}
			}
#endif
			task_cnt++;
			if ((frm_info->user_info[frm_idx].is_time_shared)
				|| (hw_comb == (IMGSYS_HW_FLAG_PQDIP_A|IMGSYS_HW_FLAG_WPE_EIS))
				|| (frm_info->user_info[frm_idx].is_secFrm)
				|| (frm_info->user_info[frm_idx].is_earlycb)
				|| ((frm_idx + 1) == frm_num)) {
#ifndef CONFIG_FPGA_EARLY_PORTING
#ifdef IMGSYS_CMDQ_PKT_REUSE
				if ((frm_info->is_ctrl_cache <= 0) ||
					((frm_info->is_ctrl_cache == 1) && (is_pkt_created[thd_idx] == 0)) ||
					((frm_info->is_ctrl_cache == 1) && (is_pkt_created[thd_idx] ==
					IMGSYS_PKT_REUSE_POOL_NUM)))
#endif
					mtk_imgsys_power_ctrl_plat8s(imgsys_dev, true);
#endif
				/* Prepare cb param */
#ifdef IMGSYS_CMDQ_CBPARAM_NUM
				mutex_lock(&g_cb_param_lock);
				for (cb_param_cnt = 0; cb_param_cnt < IMGSYS_CMDQ_CBPARAM_NUM;
					cb_param_cnt++) {
					g_cb_param_idx = (g_cb_param_idx+1)%IMGSYS_CMDQ_CBPARAM_NUM;
					if (g_cb_param_idx >= IMGSYS_CMDQ_CBPARAM_NUM) {
						dev_info(imgsys_dev->dev,
							"%s: force set g_cb_param_idx(%d) to 0! in block(%d) for frm(%d/%d)\n",
							__func__, g_cb_param_idx, blk_idx,
							frm_idx, frm_num);
						g_cb_param_idx = 0;
					}
					cb_param = &g_cb_param[g_cb_param_idx];
				if (cb_param->isOccupy) {
					if (imgsys_cmdq_dbg_enable_plat8s())
						dev_info(imgsys_dev->dev,
							"%s: g_cb_param[%d] is occypied!!! in block(%d) for frm(%d/%d)\n",
							__func__, g_cb_param_idx, blk_idx,
							frm_idx, frm_num);
						continue;
						//mutex_unlock(&g_cb_param_lock);
						//if (isTimeShared)
							//mutex_unlock(&(imgsys_dev->vss_blk_lock));
						//return -1;
				} else {
					cb_param->isOccupy = true;
					break;
				}
				}
				/* Fail to get available cb_param from pool */
				if (cb_param_cnt == IMGSYS_CMDQ_CBPARAM_NUM) {
					dev_info(imgsys_dev->dev,
						"%s: all g_cb_param with cnt(%d) is occypied!!! in block(%d) for frm(%d/%d)\n",
						__func__, cb_param_cnt, blk_idx,
						frm_idx, frm_num);
					cb_param =
						vzalloc(sizeof(struct mtk_imgsys_cb_param));
					if (cb_param == NULL) {
						cmdq_pkt_destroy_no_wq(pkt);
					dev_info(imgsys_dev->dev,
						"%s: cb_param is NULL! in block(%d) for frm(%d/%d)!\n",
						__func__, blk_idx, frm_idx, frm_num);
					mutex_unlock(&g_cb_param_lock);
#ifndef CONFIG_FPGA_EARLY_PORTING
					mtk_imgsys_power_ctrl_plat8s(imgsys_dev, false);
#endif
					if (isTimeShared)
						mutex_unlock(&(imgsys_dev->vss_blk_lock));
					return -1;
					}
					cb_param->isDynamic = true;
				}
				mutex_unlock(&g_cb_param_lock);
				if (imgsys_cmdq_dbg_enable_plat8s())
					dev_dbg(imgsys_dev->dev,
						"%s: set cb_param to g_cb_param[%d] in block(%d) for frm(%d/%d)\n",
						__func__, g_cb_param_idx, blk_idx,
						frm_idx, frm_num);
#else
				cb_param =
					vzalloc(sizeof(struct mtk_imgsys_cb_param));
				if (cb_param == NULL) {
					cmdq_pkt_destroy_no_wq(pkt);
					dev_info(imgsys_dev->dev,
						"%s: cb_param is NULL! in block(%d) for frm(%d/%d)!\n",
						__func__, blk_idx, frm_idx, frm_num);
					if (isTimeShared)
						mutex_unlock(&(imgsys_dev->vss_blk_lock));
					return -1;
				}
#endif
				if (imgsys_cmdq_dbg_enable_plat8s())
					dev_dbg(imgsys_dev->dev,
						"%s: req fd/no(%d/%d) frame no(%d) thd_idx(%d) cb_param kzalloc success cb(%p) in block(%d) for frm(%d/%d)!\n",
						__func__,
						frm_info->request_fd, frm_info->request_no, frm_info->frame_no,
						thd_idx, cb_param, blk_idx, frm_idx, frm_num);

				task_num++;
#ifdef IMGSYS_MAE_WRITE_BACK_SUPPORT
				cb_param->hw_info = mae_info;
#endif
				cb_param->pkt = pkt;
				cb_param->frm_info = frm_info;
				cb_param->pipe = (struct mtk_imgsys_pipe *)frm_info->pipe;
				cb_param->req_fd = frm_info->request_fd;
				cb_param->req_no = frm_info->request_no;
				cb_param->frm_no = frm_info->frame_no;
				cb_param->hw_comb = hw_comb;
				cb_param->frm_idx = frm_idx;
				cb_param->frm_num = frm_num;
				cb_param->user_cmdq_cb = cmdq_cb;
				cb_param->user_cmdq_err_cb = cmdq_err_cb;
				if ((blk_idx + 1) == blk_num)
					cb_param->isBlkLast = 1;
				else
					cb_param->isBlkLast = 0;
				if ((frm_idx + 1) == frm_num)
					cb_param->isFrmLast = 1;
				else
					cb_param->isFrmLast = 0;
				cb_param->blk_idx = blk_idx;
				cb_param->blk_num = blk_num;
				cb_param->is_earlycb = frm_info->user_info[frm_idx].is_earlycb;
				cb_param->group_id = frm_info->group_id;
				cb_param->cmdqTs.tsReqStart = tsReqStart;
				cb_param->cmdqTs.tsDvfsQosStart = tsDvfsQosStart;
				cb_param->cmdqTs.tsDvfsQosEnd = tsDvfsQosEnd;
				cb_param->imgsys_dev = imgsys_dev;
				cb_param->thd_idx = thd_idx;
				cb_param->clt = clt;
				cb_param->task_cnt = task_cnt;
				for (task_idx = 0; task_idx < task_cnt; task_idx++) {
#ifdef IMGSYS_CMDQ_PKT_REUSE
					if ((frm_info->is_ctrl_cache <= 0) ||
						(((frm_info->is_ctrl_cache == 1) &&
						(is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM))))
						cb_param->pkt_ofst[task_idx] = pkt_ofst[task_idx];
					else {
						reuse_task_idx = frm_idx-task_cnt+1+task_idx;
						cb_param->pkt_ofst[task_idx] =
						g_pkt_reuse_ofst[thd_idx][cur_cmd_block[thd_idx]][reuse_task_idx];
					if (imgsys_cmdq_dbg_enable_plat8s())
						dev_dbg(imgsys_dev->dev,
							"%s: req fd/no(%d/%d) frame no(%d) frm(%d/%d) task_cnt(%d) cb_param->pkt_ofst[%d] = 0x%lx; g_pkt_reuse_ofst[%d][%d][%d] = 0x%lx\n",
							__func__,
							frm_info->request_fd, frm_info->request_no, frm_info->frame_no,
							frm_idx, frm_num, task_cnt, task_idx,
							cb_param->pkt_ofst[task_idx], thd_idx,
							cur_cmd_block[thd_idx], reuse_task_idx,
							g_pkt_reuse_ofst[thd_idx][cur_cmd_block[thd_idx]][
							reuse_task_idx]);
					}
#else
					cb_param->pkt_ofst[task_idx] = pkt_ofst[task_idx];
#endif
				}
				task_cnt = 0;
				cb_param->task_id = task_id;
				task_id++;
				if ((cb_param->isBlkLast) && (cb_param->isFrmLast)) {
					cb_param->isTaskLast = 1;
					cb_param->task_num = task_num;
					frm_info->total_taskcnt = task_num;
				} else {
					cb_param->isTaskLast = 0;
					cb_param->task_num = 0;
				}
				cb_param->batchnum = frm_info->batchnum;
				cb_param->memory_mode = frm_info->memory_mode;
				cb_param->is2ndflush = -1;
				cb_param->retry_tbl = retry_tbl;
				cb_param->is_ctrl_cache = frm_info->is_ctrl_cache;
				cb_param->isPktReuse = 0;

				if (imgsys_cmdq_dbg_enable_plat8s())
					dev_dbg(imgsys_dev->dev,
						"%s: cb(%p) gid(%d) in block(%d/%d) for frm(%d/%d) lst(%d/%d/%d) task(%d/%d/%d) ofst(%lx/%lx/%lx/%lx/%lx)\n",
						__func__, cb_param, cb_param->group_id,
						cb_param->blk_idx,  cb_param->blk_num,
						cb_param->frm_idx, cb_param->frm_num,
						cb_param->isBlkLast, cb_param->isFrmLast,
						cb_param->isTaskLast, cb_param->task_id,
						cb_param->task_num, cb_param->task_cnt,
						cb_param->pkt_ofst[0], cb_param->pkt_ofst[1],
						cb_param->pkt_ofst[2], cb_param->pkt_ofst[3],
						cb_param->pkt_ofst[4]);

				if (imgsys_cmdq_ts_enable_plat8s()
					|| imgsys_wpe_bwlog_enable_plat8s()) {
					cb_param->taskTs.dma_pa = pkt_ts_pa;
					cb_param->taskTs.dma_va = pkt_ts_va;
					cb_param->taskTs.num = pkt_ts_num;
					cb_param->taskTs.ofst = pkt_ts_ofst;
					pkt_ts_ofst += pkt_ts_num;
				}

				/* flush synchronized, block API */
				cb_param->cmdqTs.tsFlushStart = ktime_get_boottime_ns()/1000;
				tsflushStart = cb_param->cmdqTs.tsFlushStart;
#ifdef IMGSYS_CMDQ_PKT_REUSE
				if ((frm_info->is_ctrl_cache <= 0) ||
					(((frm_info->is_ctrl_cache == 1) &&
					(is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM)))) {
					pkt->aee_cb = imgsys_cmdq_task_aee_cb_plat8s;
					pkt->user_priv = (void *)cb_param;
				}
#else
				pkt->aee_cb = imgsys_cmdq_task_aee_cb_plat8s;
				pkt->user_priv = (void *)cb_param;
#endif

				IMGSYS_CMDQ_SYSTRACE_BEGIN(
					"%s_%s|Imgsys MWFrame:#%d MWReq:#%d ReqFd:%d fidx:%d hw_comb:0x%x Own:%llx cb(%p) frm(%d/%d) blk(%d/%d)",
					__func__, "pkt_flush", frm_info->frame_no,
					frm_info->request_no, frm_info->request_fd,
					frm_info->user_info[frm_idx].subfrm_idx,
					frm_info->user_info[frm_idx].hw_comb,
					frm_info->frm_owner, cb_param, frm_idx, frm_num,
					blk_idx, blk_num);

#ifdef IMGSYS_CMDQ_PKT_REUSE
				if ((frm_info->is_ctrl_cache <= 0) ||
					(((frm_info->is_ctrl_cache == 1) &&
					(is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM))))
#endif
					MTK_IMGSYS_QOF_NEED_RUN(imgsys_dev->qof_ver,
						mtk_imgsys_cmdq_qof_sub(pkt, qof_need_sub,
							&frm_info->user_info[frm_idx],
							frm_info->memory_mode, &need_dip_cine);
					);

#ifdef IMGSYS_CMDQ_PKT_REUSE
				if ((frm_info->is_ctrl_cache <= 0) ||
					((frm_info->is_ctrl_cache == 1) &&
					(is_pkt_created[thd_idx] == (IMGSYS_PKT_REUSE_POOL_NUM - 1)) &&
					(cb_param->isFrmLast == 1))) {
					if (frm_info->is_ctrl_cache == 1) {
						//cmdq_pkt_eoc(pkt, true);
						is_pkt_created[thd_idx] = IMGSYS_PKT_REUSE_POOL_NUM;
						g_reuse_cmd_num_max[thd_idx] = g_reuse_cmd_num[thd_idx];
						g_reuse_cmd_num[thd_idx] = reuse_cmd_num;
						g_reuse_event_num_max[thd_idx] = g_reuse_event_num[thd_idx];
						g_reuse_event_num[thd_idx] = reuse_event_num;
						pkt->loop = true;
						pkt->loop_cb_times_by_cookie = true;
						pkt->skip_add_cookie = false;
						pkt->skip_timeout_cb = imgsys_cmdq_task_skip_timeout_cb_plat8s;
						pr_info(
						"%s: cmdq_pkt_finalize_loop frame_last pkt(0x%lx) is_pkt(%d) thd_idx(%d) reuse_cmd(%d) reuse_cmd_max(%d) reuse_event(%d) reuse_event_max(%d)\n",
							__func__, (unsigned long)pkt, is_pkt_created[thd_idx], thd_idx,
							reuse_cmd_num, g_reuse_cmd_num_max[thd_idx],
							reuse_event_num, g_reuse_event_num_max[thd_idx]);
						cmdq_pkt_finalize_loop(pkt);
						/* Remove user_cb for pkt_reuse removal */
						cb_param->user_cmdq_cb = NULL;
						cb_param->user_cmdq_err_cb = NULL;
						cb_param->isPktReuse = 1;
					/* Add cmdq thread list check to avoid irq missing */
					while (cmdq_thread_check_list_empty(imgsys_clt[thd_idx]->chan) == false) {
						dev_dbg(imgsys_dev->dev,
							"%s: waiting for thd_idx(%d) empty\n",
							__func__, thd_idx);
						usleep_range(1000, 1050);
					}
					} else
						pkt->skip_add_cookie = true;

					ret_flush = cmdq_pkt_flush_async(pkt, imgsys_cmdq_task_cb_plat8s,
								(void *)cb_param);
					IMGSYS_CMDQ_SYSTRACE_END();

					tsFlushEnd = ktime_get_boottime_ns()/1000;
				if (ret_flush < 0)
					pr_info(
					"%s: cmdq_pkt_flush_async ret(%d) for frm(%d/%d) ts(%lld)!\n",
					__func__, ret_flush, frm_idx, frm_num,
					tsFlushEnd - tsflushStart);
				else {
					if (imgsys_cmdq_dbg_enable_plat8s())
						pr_debug(
						"%s: cmdq_pkt_flush_async success(%d), blk(%d), frm(%d/%d), ts(%lld)!\n",
						__func__, ret_flush, blk_idx, frm_idx, frm_num,
						tsFlushEnd - tsflushStart);
				}
				} else if ((frm_info->is_ctrl_cache == 1) &&
					(is_pkt_created[thd_idx] <= (IMGSYS_PKT_REUSE_POOL_NUM - 1))) {
					if (is_pkt_created[thd_idx] == 0) {
						g_pkt_reuse[thd_idx] = pkt;
						g_reuse_cb_param[thd_idx][g_cb_idx[thd_idx]] = (void *)cb_param;
						g_cb_idx[thd_idx]++;
						g_cb_idx[thd_idx] = (g_cb_idx[thd_idx] == IMGSYS_PKT_REUSE_CB_NUM) ?
							0 : g_cb_idx[thd_idx];
						//if (g_cb_idx[thd_idx] == IMGSYS_PKT_REUSE_CB_NUM)
							//g_cb_idx[thd_idx] = 0;
					/* For last frame, not just early callback */
					if (cb_param->isFrmLast == 1) {
						reuse_cmd_num = g_reuse_cmd_num[thd_idx];
						reuse_event_num = g_reuse_event_num[thd_idx];
						cmdq_set_event(imgsys_clt[0]->chan,
						imgsys_event[
						IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_PKT_REUSE_POOL_0].event +
						(thd_idx * IMGSYS_PKT_REUSE_POOL_NUM) + cur_cmd_block[thd_idx]);
						cur_cmd_block[thd_idx]++;
					}
					} else {
						/* Release cb_param due to not really using */
						cb_param->isOccupy = false;
					}
					cmdq_pkt_eoc(pkt, true);
					if (cb_param->isFrmLast == 1) {
						pr_info(
						"%s: cmdq_pkt_eoc frame_last pkt(%d)\n",
							__func__, is_pkt_created[thd_idx]);
						is_pkt_created[thd_idx]++;
						// Reset frame idx to repeat cmd
						task_num = 0; // Reset task_num
						frm_idx = -1; // Reset frame_idx
					}
				} else {
					/* pkt reuse flow for block cmd */
					if (g_reuse_cmd_num[thd_idx] == g_reuse_cmd_num_max[thd_idx])
						g_reuse_cmd_num[thd_idx] = 0;
					if (g_reuse_event_num[thd_idx] == g_reuse_event_num_max[thd_idx])
						g_reuse_event_num[thd_idx] = 0;

					g_reuse_cb_param[thd_idx][g_cb_idx[thd_idx]] = (void *)cb_param;
					g_cb_idx[thd_idx]++;
					g_cb_idx[thd_idx] = (g_cb_idx[thd_idx] == IMGSYS_PKT_REUSE_CB_NUM) ?
						0 : g_cb_idx[thd_idx];
					//if (g_cb_idx[thd_idx] == IMGSYS_PKT_REUSE_CB_NUM)
						//g_cb_idx[thd_idx] = 0;
					/* For last frame, not just early callback */
					if (cb_param->isFrmLast == 1) {
						cmdq_set_event(imgsys_clt[0]->chan,
						imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_PKT_REUSE_POOL_0].event +
						(thd_idx * IMGSYS_PKT_REUSE_POOL_NUM) + cur_cmd_block[thd_idx]);
						cur_cmd_block[thd_idx]++;
						cur_cmd_block[thd_idx] =
							(cur_cmd_block[thd_idx] == IMGSYS_PKT_REUSE_POOL_NUM) ?
							0 : cur_cmd_block[thd_idx];
						//if (cur_cmd_block[thd_idx] == IMGSYS_PKT_REUSE_POOL_NUM)
							//cur_cmd_block[thd_idx] = 0;
					}
				}
#else
				ret_flush = cmdq_pkt_flush_async(pkt, imgsys_cmdq_task_cb_plat8s,
					(void *)cb_param);
				IMGSYS_CMDQ_SYSTRACE_END();

				tsFlushEnd = ktime_get_boottime_ns()/1000;
				if (ret_flush < 0)
					pr_info(
					"%s: cmdq_pkt_flush_async ret(%d) for frm(%d/%d) ts(%lld)!\n",
					__func__, ret_flush, frm_idx, frm_num,
					tsFlushEnd - tsflushStart);
				else {
					if (imgsys_cmdq_dbg_enable_plat8s())
						pr_debug(
						"%s: cmdq_pkt_flush_async success(%d), blk(%d), frm(%d/%d), ts(%lld)!\n",
						__func__, ret_flush, blk_idx, frm_idx, frm_num,
						tsFlushEnd - tsflushStart);
				}
#endif
				isPack = 0;
			} else {
				isPack = 1;
			}
		}
		if (isTimeShared)
			mutex_unlock(&(imgsys_dev->vss_blk_lock));
	}

sendtask_done:
	return ret;
}

static inline void imgsys_get_hw_shift_iova(u8 hw_id, u8 rsv, u64 cur_iova_addr, u64 *shift_iova_addr)
{
	if (hw_id == IMGSYS_HW_MAE) {
		switch (rsv) {
		case 0:
			*shift_iova_addr = (cur_iova_addr>>4);
			break;
		case 1:
			*shift_iova_addr = (cur_iova_addr>>20);
			break;
		default:
			*shift_iova_addr = cur_iova_addr;
			break;
		}
	}
}

static inline void imgsys_get_hw_iova_mask(u8 hw_id, u8 rsv, u32 *iova_mask)
{
	if (hw_id == IMGSYS_HW_MAE) {
		switch (rsv) {
		case 0:
		case 1:
			*iova_mask = 0xFFFF;
			break;
		default:
			*iova_mask = 0xFFFFFFFF;
			break;
		}
	}
}

int imgsys_cmdq_parser_plat8s(struct mtk_imgsys_dev *imgsys_dev,
					struct swfrm_info_t *frm_info, struct cmdq_pkt *pkt,
					struct Command *cmd, u32 hw_comb, int sw_ridx,
					dma_addr_t dma_pa, uint32_t *num, u32 thd_idx,
					u64 (*imgsys_get_iova)(struct dma_buf *dma_buf, s32 ionFd,
						struct mtk_imgsys_dev *imgsys_dev,
						struct mtk_imgsys_dev_buffer *dev_buf),
					u64 (*imgsys_get_kva)(struct dma_buf *dma_buf, s32 ionFd,
						struct mtk_imgsys_dev *imgsys_dev,
						struct mtk_imgsys_dev_buffer *dev_buf),
					int (*is_singledev_mode)(struct mtk_imgsys_request *req),
					struct mtk_imgsys_hw_info *hw_info,
					struct retry_event_table *retry_tbl)
{
	bool stop = 0;
	int count = 0;
	int req_fd = 0, req_no = 0, frm_no = 0;
	u32 event = 0;
#ifdef MTK_IOVA_SINK2KERNEL
	u64 iova_addr = 0, cur_iova_addr = 0;
	u64 vaddr = 0;
	struct mtk_imgsys_req_fd_info *fd_info = NULL;
	struct dma_buf *dbuf = NULL;
	struct mtk_imgsys_request *req = NULL;
	struct mtk_imgsys_dev_buffer *dev_b = 0;
	bool iova_dbg = false;
	u16 pre_fd = 0;
	u64 shift_iova_addr = 0;
	u32 iova_mask = 0;
#endif
#ifdef IMGSYS_MAE_WRITE_BACK_SUPPORT
	u32 first_read = 1;
	struct Command *nxt_cmd;
#endif
	int is_ctrl_cache;
#ifdef IMGSYS_WPE_CHECK_FUNC_EN
	unsigned int base;
#endif

	req_fd = frm_info->request_fd;
	req_no = frm_info->request_no;
	frm_no = frm_info->frame_no;
	is_ctrl_cache = frm_info->is_ctrl_cache;
	retry_tbl->event_num = 0;

	if (imgsys_cmdq_dbg_enable_plat8s())
		pr_debug("%s: +, cmd(%d)\n", __func__, cmd->opcode);

	do {
		switch (cmd->opcode) {
		case IMGSYS_CMD_READ:
			if (imgsys_cmdq_dbg_enable_plat8s())
				pr_debug(
					"%s: READ with source(0x%08x) target(0x%08x) mask(0x%08x)\n",
					__func__, cmd->u.source, cmd->u.target, cmd->u.mask);
			if (imgsys_wpe_bwlog_enable_plat8s() && (hw_comb != 0x800)) {
				cmdq_pkt_mem_move(pkt, NULL, (dma_addr_t)cmd->u.source,
					dma_pa + (4*(*num)), CMDQ_THR_SPR_IDX3);
				(*num)++;
#ifdef IMGSYS_WPE_CHECK_FUNC_EN
			} else if (hw_comb == (IMGSYS_HW_FLAG_WPE_EIS|IMGSYS_HW_FLAG_PQDIP_A)) {
				base = cmd->u.source >> 16;
				switch (base) {
				case 0x3424:
					cmdq_pkt_mem_move(pkt, NULL, (dma_addr_t)cmd->u.source,
						g_pkt_wpe_pa , CMDQ_THR_SPR_IDX2);
					break;
				case 0x3425:
					cmdq_pkt_mem_move(pkt, NULL, (dma_addr_t)cmd->u.source,
						g_pkt_pqdipa_pa , CMDQ_THR_SPR_IDX2);
					break;
				}
				is_wpe_read_cmd = 1;
#endif
			} else
				pr_info(
					"%s: [ERROR]Not enable imgsys read cmd!!\n",
					__func__);
			break;
#ifdef IMGSYS_MAE_WRITE_BACK_SUPPORT
		case IMGSYS_CMD_READ_FD:
			iova_dbg = (imgsys_iova_dbg_port_plat8s() == cmd->u.dma_addr);
			if (imgsys_iova_dbg_enable_plat8s() || iova_dbg) {
				pr_info(
					"%s: READ_FD with req_fd/no(%d/%d) frame_no(%d) addr(0x%08lx) msb_ofst(0x%08x) fd(0x%08x) ofst(0x%08x) rshift(%d)\n",
					__func__, req_fd, req_no, frm_no,
				(unsigned long)cmd->u.dma_addr, cmd->u.dma_addr_msb_ofst,
				cmd->u.fd, cmd->u.ofst, cmd->u.right_shift);
			}
			if (cmd->u.fd <= 0) {
				pr_info("%s: [ERROR] READ_FD with FD(%d)! req_fd/no(%d/%d) frame_no(%d)\n",
					__func__, cmd->u.fd, req_fd, req_no, frm_no);
				return -1;
			}

			if (hw_comb != IMGSYS_HW_FLAG_MAE) {
				pr_info("%s: [ERROR]Not enable imgsys(%d) read cmd!!\n", __func__, hw_comb);
				break;
			}
			if (first_read) {
				mutex_lock(&cpr_lock);
				hw_info->read_va = mae_va;

				#ifndef MTK_IOVA_NOTCHECK
				dbuf = dma_buf_get(cmd->u.fd);
				#endif
				fd_info = &imgsys_dev->req_fd_cache.info_array[req_fd];
				req = (struct mtk_imgsys_request *) fd_info->req_addr_va;
				dev_b = req->buf_map[is_singledev_mode(req)];
				vaddr = imgsys_get_kva(dbuf, cmd->u.fd, imgsys_dev, dev_b);
				if (vaddr <= 0) {
					aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_MAE",
						"DISPATCH:IMGSYS_MAE map kva fail, cmd->u.dma_addrddr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
					break;
				}

				hw_info->write_back_vaddr = (u32 *)(vaddr + cmd->u.ofst);
				first_read = 0;
				if (imgsys_cmdq_dbg_enable_plat8s())
					pr_debug("%s: MAE need to write-back to 0x%p",__func__,
									hw_info->write_back_vaddr);
			}
			cmdq_pkt_mem_move(pkt, NULL, (dma_addr_t)cmd->u.dma_addr,
				mae_pa, CMDQ_THR_SPR_IDX2);
			hw_info->read_cnt++;

			is_mae_read_cmd = 1;

			mae_pa = mae_pa + 4;
			mae_va = mae_va + 1;
			if (mae_pa >= g_pkt_mae_pa_end) {
				mae_pa = g_pkt_mae_pa;
				mae_va = g_pkt_mae_va;
				if (imgsys_cmdq_dbg_enable_plat8s())
					pr_debug("%s: mae gce sram rings back\n", __func__);
			}
			nxt_cmd = cmd + 1;
			if (nxt_cmd->opcode != IMGSYS_CMD_READ_FD)
				mutex_unlock(&cpr_lock);
			break;
#endif

		case IMGSYS_CMD_WRITE:
			if (imgsys_cmdq_dbg_enable_plat8s())
				pr_debug(
					"%s: WRITE with addr(0x%08x) value(0x%08x) mask(0x%08x)\n",
					__func__, cmd->u.address, cmd->u.value, cmd->u.mask);

			if(imgsy_isc_xfd_dbg_enable_plat8s() && (cmd->u.address == 0x34060000))
				pr_info(
				"%s: WRITE with addr(0x%08x) value(0x%08x) mask(0x%08x)\n",
				__func__, cmd->u.address, cmd->u.value, cmd->u.mask);
#ifdef IMGSYS_CMDQ_PKT_REUSE
			if (is_ctrl_cache <= 0)
				cmdq_pkt_write(pkt, NULL, (dma_addr_t)cmd->u.address,
					cmd->u.value, cmd->u.mask);
			else {
				if (is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM)
					cmdq_pkt_mem_move_mask(pkt, NULL,
						g_pkt_reuse_pa[thd_idx] + (4*(g_reuse_cmd_num[thd_idx])),
						(dma_addr_t)cmd->u.address, CMDQ_THR_SPR_IDX2, cmd->u.mask);
				g_pkt_reuse_va[thd_idx][g_reuse_cmd_num[thd_idx]] = cmd->u.value;
				g_reuse_cmd_num[thd_idx]++;
			}
#else
			cmdq_pkt_write(pkt, NULL, (dma_addr_t)cmd->u.address,
				cmd->u.value, cmd->u.mask);
#endif
			break;
#ifdef MTK_IOVA_SINK2KERNEL
		case IMGSYS_CMD_WRITE_FD:
			iova_dbg = (imgsys_iova_dbg_port_plat8s() == cmd->u.dma_addr);
			if (imgsys_iova_dbg_enable_plat8s() || iova_dbg) {
				pr_info(
					"%s: WRITE_FD with req_fd/no(%d/%d) frame_no(%d) addr(0x%08lx) msb_ofst(0x%08x) fd(0x%08x) ofst(0x%08x) rshift(%d)\n",
					__func__, req_fd, req_no, frm_no,
				(unsigned long)cmd->u.dma_addr, cmd->u.dma_addr_msb_ofst,
				cmd->u.fd, cmd->u.ofst, cmd->u.right_shift);
			}
			if (cmd->u.fd <= 0) {
				pr_info("%s: [ERROR] WRITE_FD with FD(%d)! req_fd/no(%d/%d) frame_no(%d)\n",
					__func__, cmd->u.fd, req_fd, req_no, frm_no);
				return -1;
			}
			//
			if (cmd->u.fd != pre_fd) {
				#ifndef MTK_IOVA_NOTCHECK
				dbuf = dma_buf_get(cmd->u.fd);
				#endif
				fd_info = &imgsys_dev->req_fd_cache.info_array[req_fd];
				req = (struct mtk_imgsys_request *) fd_info->req_addr_va;
				dev_b = req->buf_map[is_singledev_mode(req)];
				iova_addr = imgsys_get_iova(dbuf, cmd->u.fd, imgsys_dev, dev_b);
				pre_fd = cmd->u.fd;

				if (iova_addr <= 0) {
					u32 dma_addr_msb = (cmd->u.dma_addr >> 16);

					pr_info(
						"%s: [ERROR] WRITE_FD map iova fail (%llu)! with req_fd/no(%d/%d) frame_no(%d) fd(%d) addr(0x%08lx)\n",
						__func__, iova_addr, req_fd, req_no, frm_no, cmd->u.fd,
						(unsigned long)cmd->u.dma_addr);

					switch (dma_addr_msb) {
					case 0x3419:
					case 0x341a:
					case 0x341b:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_DIP",
							"DISPATCH:IMGSYS_DIP map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3476:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_TRAW",
							"DISPATCH:IMGSYS_TRAW map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3405:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_LTRAW",
							"DISPATCH:IMGSYS_LTRAW map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3424:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_WPE_EIS",
							"DISPATCH:IMGSYS_WPE_EIS map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3453:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_OMC_TNR",
							"DISPATCH:IMGSYS_OMC_TNR map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3464:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_WPE_LITE",
							"DISPATCH:IMGSYS_WPE_LITE map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3465:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_OMC_LITE",
							"DISPATCH:IMGSYS_OMC_LITE map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3469:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_WPE_DEPTH",
							"DISPATCH:IMGSYS_WPE_DEPTH map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3425:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_PQDIP_A",
							"DISPATCH:IMGSYS_PQDIP_A map iova fail, addr:0x%08llx",
							 (unsigned long)cmd->u.dma_addr);
						break;
					case 0x3454:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_PQDIP_B",
							"DISPATCH:IMGSYS_PQDIP_B map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3455:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_ME",
							"DISPATCH:IMGSYS_ME map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3432:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_MAE",
							"DISPATCH:IMGSYS_MAE map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3467:
					case 0x3468:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_DFP",
							"DISPATCH:IMGSYS_DFP map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3a00:
					case 0x3a7a:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_DPE",
							"DISPATCH:IMGSYS_DPE map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3401:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_ADL",
							"DISPATCH:IMGSYS_ADL map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3456:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_MMG",
							"DISPATCH:IMGSYS_MMG map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					default:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS",
							"DISPATCH:IMGSYS map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					}

					return -1;
				}
			} else {
				if (imgsys_cmdq_dbg_enable_plat8s()) {
					pr_info(
						"%s: Current fd(0x%08x) is the same with previous fd(0x%08x) with iova(0x%08llx), bypass map iova operation\n",
						__func__, cmd->u.fd, pre_fd, iova_addr);
				}
			}
			cur_iova_addr = iova_addr + cmd->u.ofst;
			//
			if (imgsys_iova_dbg_enable_plat8s() || iova_dbg) {
				pr_info(
					"%s: WRITE_FD with req_fd/no(%d/%d) frame_no(%d) addr(0x%08lx) value(0x%08llx)\n",
					__func__, req_fd, req_no, frm_no,
					(unsigned long)cmd->u.dma_addr,
					(cur_iova_addr >> cmd->u.right_shift));
			}
#ifdef IMGSYS_CMDQ_PKT_REUSE
			if (is_ctrl_cache <= 0)
				cmdq_pkt_write(pkt, NULL, cmd->u.dma_addr,
					(cur_iova_addr >> cmd->u.right_shift), 0xFFFFFFFF);
			else {
				if (is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM)
					cmdq_pkt_mem_move(pkt, NULL,
						g_pkt_reuse_pa[thd_idx] + (4*(g_reuse_cmd_num[thd_idx])),
						(dma_addr_t)cmd->u.dma_addr, CMDQ_THR_SPR_IDX2);
				g_pkt_reuse_va[thd_idx][g_reuse_cmd_num[thd_idx]] =
					(cur_iova_addr >> cmd->u.right_shift);
				g_reuse_cmd_num[thd_idx]++;
			}
#else
			cmdq_pkt_write(pkt, NULL, cmd->u.dma_addr,
				(cur_iova_addr >> cmd->u.right_shift), 0xFFFFFFFF);
#endif
			if (cmd->u.dma_addr_msb_ofst) {
				if (imgsys_iova_dbg_enable_plat8s() || iova_dbg) {
					pr_info(
						"%s: WRITE_FD with req_fd/no(%d/%d) frame_no(%d) addr(0x%08lx) value(0x%08llx)\n",
						__func__, req_fd, req_no, frm_no,
						(unsigned long)cmd->u.dma_addr,
						(cur_iova_addr>>32));
				}
#ifdef IMGSYS_CMDQ_PKT_REUSE
				if (is_ctrl_cache <= 0)
					cmdq_pkt_write(pkt, NULL,
						(cmd->u.dma_addr + cmd->u.dma_addr_msb_ofst),
						(cur_iova_addr>>32), 0xFFFFFFFF);
				else {
					if (is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM)
						cmdq_pkt_mem_move(pkt, NULL,
							g_pkt_reuse_pa[thd_idx] + (4*(g_reuse_cmd_num[thd_idx])),
							(dma_addr_t)(cmd->u.dma_addr + cmd->u.dma_addr_msb_ofst),
							CMDQ_THR_SPR_IDX2);
					g_pkt_reuse_va[thd_idx][g_reuse_cmd_num[thd_idx]] =
						cur_iova_addr>>32;
					g_reuse_cmd_num[thd_idx]++;
				}
#else
				cmdq_pkt_write(pkt, NULL,
					(cmd->u.dma_addr + cmd->u.dma_addr_msb_ofst),
					(cur_iova_addr>>32), 0xFFFFFFFF);
#endif
			}

			break;
		case IMGSYS_CMD_WRITE_FD_HW:
			iova_dbg = (imgsys_iova_dbg_port_plat8s() == cmd->u.dma_addr);
			if (imgsys_iova_dbg_enable_plat8s() || iova_dbg) {
				pr_info(
					"%s: WRITE_FD_HW with req_fd/no(%d/%d) frame_no(%d) addr(0x%08lx) hw_id(%d) fd(0x%08x) ofst(0x%08x) rsv(%d)\n",
					__func__, req_fd, req_no, frm_no,
				(unsigned long)cmd->u.dma_addr, cmd->u.dma_addr_msb_ofst,
				cmd->u.fd, cmd->u.ofst, cmd->u.right_shift);
			}
			if (cmd->u.fd <= 0) {
				pr_info("%s: [ERROR] WRITE_FD_HW with FD(%d)! req_fd/no(%d/%d) frame_no(%d)\n",
					__func__, cmd->u.fd, req_fd, req_no, frm_no);
				return -1;
			}
			//
			if (cmd->u.fd != pre_fd) {
				#ifndef MTK_IOVA_NOTCHECK
				dbuf = dma_buf_get(cmd->u.fd);
				#endif
				fd_info = &imgsys_dev->req_fd_cache.info_array[req_fd];
				req = (struct mtk_imgsys_request *) fd_info->req_addr_va;
				dev_b = req->buf_map[is_singledev_mode(req)];
				iova_addr = imgsys_get_iova(dbuf, cmd->u.fd, imgsys_dev, dev_b);
				pre_fd = cmd->u.fd;

				if (iova_addr <= 0) {
					u32 dma_addr_msb = (cmd->u.dma_addr >> 16);

					pr_info(
						"%s: [ERROR] WRITE_FD_HW map iova fail (%llu)! with req_fd/no(%d/%d) frame_no(%d) fd(%d) addr(0x%08lx)\n",
						__func__, iova_addr, req_fd, req_no, frm_no, cmd->u.fd,
						(unsigned long)cmd->u.dma_addr);

					switch (dma_addr_msb) {
					case 0x3419:
					case 0x341a:
					case 0x341b:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_DIP",
							"DISPATCH:IMGSYS_DIP map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3476:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_TRAW",
							"DISPATCH:IMGSYS_TRAW map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3405:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_LTRAW",
							"DISPATCH:IMGSYS_LTRAW map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3424:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_WPE_EIS",
							"DISPATCH:IMGSYS_WPE_EIS map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3453:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_OMC_TNR",
							"DISPATCH:IMGSYS_OMC_TNR map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3464:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_WPE_LITE",
							"DISPATCH:IMGSYS_WPE_LITE map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3465:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_OMC_LITE",
							"DISPATCH:IMGSYS_OMC_LITE map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3469:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_WPE_DEPTH",
							"DISPATCH:IMGSYS_WPE_DEPTH map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3425:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_PQDIP_A",
							"DISPATCH:IMGSYS_PQDIP_A map iova fail, addr:0x%08llx",
							 (unsigned long)cmd->u.dma_addr);
						break;
					case 0x3454:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_PQDIP_B",
							"DISPATCH:IMGSYS_PQDIP_B map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3455:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_ME",
							"DISPATCH:IMGSYS_ME map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3432:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_MAE",
							"DISPATCH:IMGSYS_MAE map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3467:
					case 0x3468:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_DFP",
							"DISPATCH:IMGSYS_DFP map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3a00:
					case 0x3a7a:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_DPE",
							"DISPATCH:IMGSYS_DPE map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3401:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_ADL",
							"DISPATCH:IMGSYS_ADL map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					case 0x3456:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS_MMG",
							"DISPATCH:IMGSYS_MMG map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					default:
						aee_kernel_exception("CRDISPATCH_KEY:IMGSYS",
							"DISPATCH:IMGSYS map iova fail, addr:0x%08llx",
							(unsigned long)cmd->u.dma_addr);
						break;
					}

					return -1;
				}
			} else {
				if (imgsys_cmdq_dbg_enable_plat8s()) {
					pr_info(
						"%s: Current fd(0x%08x) is the same with previous fd(0x%08x) with iova(0x%08llx), bypass map iova operation\n",
						__func__, cmd->u.fd, pre_fd, iova_addr);
				}
			}

			if (imgsys_iova_dbg_enable_plat8s() || iova_dbg) {
				pr_info(
					"%s: WRITE_FD_HW with req_fd/no(%d/%d) frame_no(%d) addr(0x%08lx) fd(0x%08x) iova(0x%08llx)\n",
					__func__, req_fd, req_no, frm_no,
					(unsigned long)cmd->u.dma_addr, cmd->u.fd, iova_addr);
			}

			cur_iova_addr = iova_addr + cmd->u.ofst;
			//
			/* call module api to get mask and shifted iova */
			imgsys_get_hw_shift_iova(cmd->u.dma_addr_msb_ofst, cmd->u.right_shift,
									cur_iova_addr, &shift_iova_addr);
			imgsys_get_hw_iova_mask(cmd->u.dma_addr_msb_ofst, cmd->u.right_shift,
									&iova_mask);

			if (imgsys_iova_dbg_enable_plat8s() || iova_dbg) {
				pr_info(
					"%s: WRITE_FD_HW with req_fd/no(%d/%d) frame_no(%d) addr(0x%08lx) value(0x%08llx) mask(0x%08lx)\n",
					__func__, req_fd, req_no, frm_no,
					(unsigned long)cmd->u.dma_addr,
					shift_iova_addr, (unsigned long)iova_mask);
			}
			cmdq_pkt_write(pkt, NULL, cmd->u.dma_addr,
				shift_iova_addr, iova_mask);
			break;
#endif
		case IMGSYS_CMD_POLL:
			/* cmdq_pkt_poll(pkt, NULL, cmd->u.value, cmd->u.address, */
			/* cmd->u.mask, CMDQ_GPR_R15); */
		{
			u32 addr_msb = (cmd->u.address >> 16);
			u32 gpr_idx = 0;

			switch (addr_msb) {
			case 0x3419:
			case 0x341a:
			case 0x341b:
				/* DIP */
				gpr_idx = 0;
				break;
			case 0x3476:
				/* TRAW */
				gpr_idx = 1;
				break;
			case 0x3405:
				/* LTRAW */
				gpr_idx = 2;
				break;
			case 0x3424:
				/* WPE_EIS */
				gpr_idx = 3;
				break;
			case 0x3453:
				/* OMC_TNR */
				gpr_idx = 4;
				break;
			case 0x3464:
				/* WPE_LITE */
				gpr_idx = 5;
				break;
			case 0x3425:
				/* PQDIP_A */
				gpr_idx = 6;
				break;
			case 0x3454:
				/* PQDIP_B */
				gpr_idx = 7;
				break;
			case 0x3455:
			case 0x3456:
				/* ME */
				gpr_idx = 8;
				break;
			case 0x3465:
				/* OMC_LITE */
				gpr_idx = 9;
				break;
			case 0x3432:
				/* MAE */
				gpr_idx = 11;
				break;
			case 0x3467:
			case 0x3468:
				/* DFP */
				gpr_idx = 12;
				break;
			default:
				gpr_idx = thd_idx;
				break;
			}
			if (imgsys_cmdq_dbg_enable_plat8s()) {
				pr_debug(
					"%s: POLL with addr(0x%08x) value(0x%08x) mask(0x%08x) addr_msb(0x%08x) thd(%d/%d)\n",
					__func__, cmd->u.address, cmd->u.value, cmd->u.mask, addr_msb, gpr_idx,
					thd_idx);
			}
#ifdef IMGSYS_CMDQ_PKT_REUSE
			if ((is_ctrl_cache <= 0) ||
				((is_ctrl_cache == 0) && (is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM)))
#endif
			#ifdef IMGSYS_WPE_CHECK_FUNC_EN
			{
				if (((gpr_idx == 3) || (gpr_idx == 6)) &&
						(hw_comb == (IMGSYS_HW_FLAG_WPE_EIS|IMGSYS_HW_FLAG_PQDIP_A)))

					cmdq_pkt_poll_timeout(pkt, cmd->u.value, SUBSYS_NO_SUPPORT,
						cmd->u.address, cmd->u.mask, IMGSYS_POLL_TIME_1000MS,
						CMDQ_GPR_R03+gpr_idx);
				else
					cmdq_pkt_poll_timeout(pkt, cmd->u.value, SUBSYS_NO_SUPPORT,
						cmd->u.address, cmd->u.mask, IMGSYS_POLL_TIME_INFINI,
						CMDQ_GPR_R03+gpr_idx);
			}
			#else

				cmdq_pkt_poll_timeout(pkt, cmd->u.value, SUBSYS_NO_SUPPORT,
					cmd->u.address, cmd->u.mask, IMGSYS_POLL_TIME_INFINI,
					CMDQ_GPR_R03+gpr_idx);
			#endif
		}
			break;
		case IMGSYS_CMD_WAIT:
			if (imgsys_cmdq_dbg_enable_plat8s())
				pr_debug(
					"%s: WAIT event(%d/%d) action(%d)\n",
					__func__, cmd->u.event, imgsys_event[cmd->u.event].event,
					cmd->u.action);
			if (cmd->u.action == 1) {
#ifdef IMGSYS_CMDQ_PKT_REUSE
				if (is_ctrl_cache <= 0) {
					cmdq_pkt_wfe(pkt, imgsys_event[cmd->u.event].event);
#ifdef IMGSYS_WPE_CHECK_FUNC_EN
					if (hw_comb != (IMGSYS_HW_FLAG_WPE_EIS|IMGSYS_HW_FLAG_PQDIP_A))
						goto bypass_set_event;
					if ((cmd->u.event > IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_100) ||
						(cmd->u.event < IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_1))
						goto bypass_set_event;
					if (retry_tbl->event_num == RETRY_EVENT_NUM) {
						pr_info("%s: out-of-sw-event.", __func__);
						goto bypass_set_event;
					}
					retry_tbl->events[retry_tbl->event_num] = imgsys_event[cmd->u.event].event;
					retry_tbl->event_num++;
#endif
				} else {
					if (is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM)
						cmdq_pkt_wfe_reuse(pkt, imgsys_event[cmd->u.event].event,
							&g_event_reuse[thd_idx][g_reuse_event_num[thd_idx]]);
					else {
						g_event_reuse[thd_idx][g_reuse_event_num[thd_idx]].val =
							imgsys_event[cmd->u.event].event;
						cmdq_pkt_reuse_buf_va(pkt,
							&g_event_reuse[thd_idx][g_reuse_event_num[thd_idx]], 1);
					}
					g_reuse_event_num[thd_idx]++;
				}
bypass_set_event:
#else
				cmdq_pkt_wfe(pkt, imgsys_event[cmd->u.event].event);
#ifdef IMGSYS_WPE_CHECK_FUNC_EN
				if (hw_comb != (IMGSYS_HW_FLAG_WPE_EIS|IMGSYS_HW_FLAG_PQDIP_A))
					goto bypass_set_event;
				if ((cmd->u.event > IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_AISEG_POOL_100) ||
					(cmd->u.event < IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_1))
					goto bypass_set_event;
				if (retry_tbl->event_num == RETRY_EVENT_NUM) {
					pr_info("%s: out-of-sw-event.", __func__);
					goto bypass_set_event;
				}
				retry_tbl->events[retry_tbl->event_num] = imgsys_event[cmd->u.event].event;
				retry_tbl->event_num++;
bypass_set_event:
#endif

#endif

				if ((cmd->u.event >= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START) &&
					(cmd->u.event <= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_END)) {
					event = cmd->u.event -
						IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START;
					event_hist[event].st++;
					event_hist[event].wait.req_fd = req_fd;
					event_hist[event].wait.req_no = req_no;
					event_hist[event].wait.frm_no = frm_no;
					event_hist[event].wait.ts = ktime_get_boottime_ns()/1000;
					event_hist[event].wait.frm_info = frm_info;
					event_hist[event].wait.pkt = pkt;
				}
			} else if (cmd->u.action == 0) {
#ifdef IMGSYS_CMDQ_PKT_REUSE
				if (is_ctrl_cache <= 0)
					cmdq_pkt_wait_no_clear(pkt, imgsys_event[cmd->u.event].event);
				else {
					if (is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM)
						cmdq_pkt_wait_no_clear_reuse(pkt, imgsys_event[cmd->u.event].event,
							&g_event_reuse[thd_idx][g_reuse_event_num[thd_idx]]);
					else {
						g_event_reuse[thd_idx][g_reuse_event_num[thd_idx]].val =
							imgsys_event[cmd->u.event].event;
						cmdq_pkt_reuse_buf_va(pkt,
							&g_event_reuse[thd_idx][g_reuse_event_num[thd_idx]], 1);
					}
					g_reuse_event_num[thd_idx]++;
				}
#else
				cmdq_pkt_wait_no_clear(pkt, imgsys_event[cmd->u.event].event);
#endif
			} else
				pr_info("%s: [ERROR]Not Support wait action(%d)!\n",
					__func__, cmd->u.action);
			break;
		case IMGSYS_CMD_UPDATE:
			if (imgsys_cmdq_dbg_enable_plat8s()) {
				pr_debug(
					"%s: UPDATE event(%d/%d) action(%d)\n",
					__func__, cmd->u.event, imgsys_event[cmd->u.event].event,
					cmd->u.action);
			}
			if (cmd->u.action == 1) {
#ifdef IMGSYS_CMDQ_PKT_REUSE
				if (is_ctrl_cache <= 0)
					cmdq_pkt_set_event(pkt, imgsys_event[cmd->u.event].event);
				else {
					if (is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM)
						cmdq_pkt_set_event_reuse(pkt, imgsys_event[cmd->u.event].event,
							&g_event_reuse[thd_idx][g_reuse_event_num[thd_idx]]);
					else {
						g_event_reuse[thd_idx][g_reuse_event_num[thd_idx]].val =
							imgsys_event[cmd->u.event].event;
						cmdq_pkt_reuse_buf_va(pkt,
							&g_event_reuse[thd_idx][g_reuse_event_num[thd_idx]], 1);
					}
					g_reuse_event_num[thd_idx]++;
				}
#else
				cmdq_pkt_set_event(pkt, imgsys_event[cmd->u.event].event);
#endif
				if ((cmd->u.event >= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START) &&
					(cmd->u.event <= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_END)) {
					event = cmd->u.event -
						IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START;
					event_hist[event].st--;
					event_hist[event].set.req_fd = req_fd;
					event_hist[event].set.req_no = req_no;
					event_hist[event].set.frm_no = frm_no;
					event_hist[event].set.ts = ktime_get_boottime_ns()/1000;
					event_hist[event].set.frm_info = frm_info;
					event_hist[event].set.pkt = pkt;
				}
			} else if (cmd->u.action == 0) {
#ifdef IMGSYS_CMDQ_PKT_REUSE
				if (is_ctrl_cache <= 0)
					cmdq_pkt_clear_event(pkt, imgsys_event[cmd->u.event].event);
				else {
					if (is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM)
						cmdq_pkt_clear_event_reuse(pkt, imgsys_event[cmd->u.event].event,
							&g_event_reuse[thd_idx][g_reuse_event_num[thd_idx]]);
					else {
						g_event_reuse[thd_idx][g_reuse_event_num[thd_idx]].val =
							imgsys_event[cmd->u.event].event;
						cmdq_pkt_reuse_buf_va(pkt,
							&g_event_reuse[thd_idx][g_reuse_event_num[thd_idx]], 1);
					}
					g_reuse_event_num[thd_idx]++;
				}
#else
				cmdq_pkt_clear_event(pkt, imgsys_event[cmd->u.event].event);
#endif
			} else
				pr_info("%s: [ERROR]Not Support update action(%d)!\n",
					__func__, cmd->u.action);
			break;
		case IMGSYS_CMD_ACQUIRE:
			if (imgsys_cmdq_dbg_enable_plat8s())
				pr_debug(
					"%s: ACQUIRE event(%d/%d) action(%d)\n", __func__,
					cmd->u.event, imgsys_event[cmd->u.event].event, cmd->u.action);
#ifdef IMGSYS_CMDQ_PKT_REUSE
			if (is_ctrl_cache <= 0)
				cmdq_pkt_acquire_event(pkt, imgsys_event[cmd->u.event].event);
			else {
				if (is_pkt_created[thd_idx] < IMGSYS_PKT_REUSE_POOL_NUM)
					cmdq_pkt_acquire_event_reuse(pkt, imgsys_event[cmd->u.event].event,
						&g_event_reuse[thd_idx][g_reuse_event_num[thd_idx]]);
				else {
					g_event_reuse[thd_idx][g_reuse_event_num[thd_idx]].val =
						imgsys_event[cmd->u.event].event;
					cmdq_pkt_reuse_buf_va(pkt,
						&g_event_reuse[thd_idx][g_reuse_event_num[thd_idx]], 1);
				}
				g_reuse_event_num[thd_idx]++;
			}
#else
			cmdq_pkt_acquire_event(pkt, imgsys_event[cmd->u.event].event);
#endif
			break;
		case IMGSYS_CMD_TIME:
			if (imgsys_cmdq_dbg_enable_plat8s())
				pr_debug(
					"%s: TIME with addr(0x%08lx) num(0x%08x)\n",
					__func__, (unsigned long)dma_pa, *num);
			if (imgsys_cmdq_ts_enable_plat8s()) {
				cmdq_pkt_write_indriect(pkt, NULL, dma_pa + (4*(*num)),
					CMDQ_TPR_ID, ~0);
				(*num)++;
			} else
				pr_info(
					"%s: [ERROR]Not enable imgsys cmdq ts function!!\n",
					__func__);
			break;
		case IMGSYS_CMD_STOP:
			if (imgsys_cmdq_dbg_enable_plat8s())
				pr_debug("%s: End Of Cmd!\n", __func__);
			stop = 1;
			break;
		default:
			pr_info("%s: [ERROR]Not Support Cmd(%d)!\n", __func__, cmd->opcode);
			return -1;
		}
		cmd++;
		count++;
	} while (stop == 0);

	return count;
}

struct isc_init_info {
	int isc_cookie;
	struct mtk_imgsys_dev *imgsys_dev;
	struct kthread_work work;
	struct cmdq_pkt *pkt;
} isc_init_info[2];

void imgsys_cmdq_isc_task_cbbh_plat8s(struct kthread_work *work)
{
	struct cmdq_pkt *pkt;
	struct isc_init_info *isc;

	isc = container_of(work, struct isc_init_info, work);
	pkt = isc->pkt;

	#if IMGSYS_SECURE_ENABLE
	if (unlikely(!imgsys_sec_clt[IMGSYS_SEC_ISC]))
		pr_info("%s: null isc sec thread!\n", __func__);
	else
		cmdq_sec_mbox_stop(imgsys_sec_clt[IMGSYS_SEC_ISC]);
	#endif
	cmdq_pkt_wait_complete(pkt);
	cmdq_pkt_destroy_no_wq(pkt);

	if (isc->isc_cookie == 3)
		vfree(isc);
	else
		isc->isc_cookie = 0;
}

void imgsys_cmdq_isc_task_cb_plat8s(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt = (struct cmdq_pkt *)data.data;
	struct isc_init_info *isc;
	int cookie;


	isc = (struct isc_init_info *)pkt->user_priv;
	cookie = isc->isc_cookie;

	if (cookie == 2 || cookie == 3) {
		complete(&isc->imgsys_dev->isc_init);
		if (isc_irq_enabled == 1) {

			pr_info("%s: isc irq already enabled(%d)\n", __func__, isc_irq_enabled);

		} else if ((isc->imgsys_dev->isc_irq > 0) && (!data.err)) {
			enable_irq(isc->imgsys_dev->isc_irq);
			isc_irq_enabled = 1;
		}

		if (data.err == 0) {
			kthread_queue_work(&imgsys_cmdq_worker, &isc->work);
		} else
			imgsys_cmdq_isc_task_cbbh_plat8s(&isc->work);
	}

	if (cookie == 1) {
		cmdq_pkt_destroy(pkt);
		isc->isc_cookie = 0;
	}
}

int imgsys_cmdq_sec_isc_init_plat8s(struct mtk_imgsys_dev *imgsys_dev)
{
	struct cmdq_client *clt_sec = NULL, *clt = NULL;
	#if IMGSYS_SECURE_ENABLE
	struct cmdq_pkt *pkt_sec = NULL, *pkt = NULL;
	#endif
	int ret = 0;
	struct isc_init_info *isc = NULL;

	isc = vzalloc(sizeof(struct isc_init_info));
	if (!isc) {
		isc = &isc_init_info[1];
		isc->isc_cookie = 2;
	} else
		isc->isc_cookie = 3;

	clt_sec = imgsys_sec_clt[IMGSYS_SEC_ISC];
	#if IMGSYS_SECURE_ENABLE
	pkt_sec = cmdq_pkt_create(clt_sec);
	isc_init_info[0].imgsys_dev = imgsys_dev;
	isc_init_info[0].isc_cookie = 1;
	pkt_sec->user_priv = (void *)&isc_init_info[0];

	cmdq_sec_pkt_set_data(pkt_sec, 0, 0, CMDQ_SEC_DEBUG, CMDQ_METAEX_TZMP);
	cmdq_sec_pkt_set_mtee(pkt_sec, true);
	cmdq_pkt_finalize_loop(pkt_sec);
	cmdq_pkt_flush_threaded(pkt_sec, imgsys_cmdq_isc_task_cb_plat8s, (void *)pkt_sec);
	#endif

	clt = imgsys_clt[0];
	#if IMGSYS_SECURE_ENABLE
	pkt = cmdq_pkt_create(clt);
	isc->imgsys_dev = imgsys_dev;
	isc->pkt = pkt;
	kthread_init_work(&isc->work, imgsys_cmdq_isc_task_cbbh_plat8s);
	pkt->user_priv = (void *)isc;
	cmdq_pkt_set_event(pkt, imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_TZMP_ISC_WAIT].event);
	cmdq_pkt_wfe(pkt, imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_TZMP_ISC_SET].event);
	ret = cmdq_pkt_flush_async(pkt, imgsys_cmdq_isc_task_cb_plat8s, (void *)pkt);

	if (ret < 0)
		pr_info("%s: cmdq_pkt_flush_async ret(%d)\n", __func__, ret);
	#else
		pr_info("%s: IMGSYS_SECURE_ENABLE not set\n", __func__);
	#endif

	return ret;
}



void imgsys_cmdq_sec_task_cb_plat8s(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt_sec = (struct cmdq_pkt *)data.data;

	cmdq_pkt_destroy(pkt_sec);
}

int imgsys_cmdq_sec_sendtask_plat8s(struct mtk_imgsys_dev *imgsys_dev)
{
	struct cmdq_client *clt_sec = NULL;
	#if IMGSYS_SECURE_ENABLE
	struct cmdq_pkt *pkt_sec = NULL;
	#endif
	int ret = 0;
	int i = 0;

	for (i = 0; i < IMGSYS_SEC_CAM_THD; i++) {
		clt_sec = imgsys_sec_clt[i];
	#if IMGSYS_SECURE_ENABLE
		pkt_sec = cmdq_pkt_create(clt_sec);
		cmdq_sec_pkt_set_data(pkt_sec, 0, 0, CMDQ_SEC_DEBUG, CMDQ_METAEX_TZMP);
		cmdq_sec_pkt_set_mtee(pkt_sec, true);
		cmdq_pkt_finalize_loop(pkt_sec);
		cmdq_pkt_flush_threaded(pkt_sec, imgsys_cmdq_sec_task_cb_plat8s, (void *)pkt_sec);
	#endif
	}
	return ret;
}

void imgsys_cmdq_sec_cmd_plat8s(struct cmdq_pkt *pkt)
{
	cmdq_pkt_set_event(pkt, imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_TZMP_ISP_WAIT].event);
	cmdq_pkt_wfe(pkt, imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_TZMP_ISP_SET].event);
}

void imgsys_cmdq_sec_cmd_fdvt_plat8s(struct cmdq_pkt *pkt)
{
	cmdq_pkt_set_event(pkt, imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_TZMP_FDVT_WAIT].event);
	cmdq_pkt_wfe(pkt, imgsys_event[IMGSYS_CMDQ_SYNC_TOKEN_TZMP_FDVT_SET].event);
}


void imgsys_cmdq_setevent_plat8s(u64 u_id)
{
	u32 event_id = 0L, event_val = 0L;

	event_id = IMGSYS_CMDQ_SYNC_TOKEN_CAMSYS_POOL_1 + (u_id % 10);
	event_val = cmdq_get_event(imgsys_clt[0]->chan, imgsys_event[event_id].event);

	if (event_val == 0) {
		cmdq_set_event(imgsys_clt[0]->chan, imgsys_event[event_id].event);
		if (imgsys_cmdq_dbg_enable_plat8s())
			pr_debug(
				"%s: SetEvent success with (u_id/event_id/event_val)=(%llu/%d/%d)!\n",
				__func__, u_id, event_id, event_val);
	} else {
		pr_info(
			"%s: [ERROR]SetEvent fail with (u_id/event_id/event_val)=(%llu/%d/%d)!\n",
			__func__, u_id, event_id, event_val);
	}
}

void imgsys_cmdq_clearevent_plat8s(int event_id)
{
	u32 event = 0;

	if ((event_id >= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START) &&
		(event_id <= IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_END)) {
		cmdq_mbox_enable(imgsys_clt[0]->chan);
		cmdq_clear_event(imgsys_clt[0]->chan, imgsys_event[event_id].event);
		if (imgsys_cmdq_dbg_enable_plat8s())
			pr_debug("%s: cmdq_clear_event with (%d/%d)!\n",
				__func__, event_id, imgsys_event[event_id].event);
		event = event_id - IMGSYS_CMDQ_SYNC_TOKEN_IMGSYS_POOL_START;
		memset((void *)&event_hist[event], 0x0,
			sizeof(struct imgsys_event_history));
		cmdq_mbox_disable(imgsys_clt[0]->chan);
	} else {
		pr_info("%s: [ERROR]unexpected event_id=(%d)!\n",
			__func__, event_id);
	}
}

#if DVFS_QOS_READY
void mtk_imgsys_power_ctrl_plat8s(struct mtk_imgsys_dev *imgsys_dev, bool isPowerOn)
{
	struct mtk_imgsys_dvfs *dvfs_info = &imgsys_dev->dvfs_info;
	u32 user_cnt = 0;
	int i;
	u32 img_main_modules = 0xFFFF;
	int pm_ret = 0;

	if (isPowerOn) {
		user_cnt = atomic_inc_return(&imgsys_dev->imgsys_user_cnt);
		if (user_cnt == 1) {
			if (!imgsys_quick_onoff_enable_plat8s())
				dev_info(dvfs_info->dev,
					"[%s] isPowerOn(%d) user(%d)\n",
					__func__, isPowerOn, user_cnt);

			MTK_IMGSYS_QOF_NEED_RUN(imgsys_dev->qof_ver,
				mtk_imgsys_cmdq_get_non_qof_module(&img_main_modules);
				dev_info(dvfs_info->dev, "[%s] support module = 0x%x\n", __func__, img_main_modules);
			);

			mutex_lock(&(imgsys_dev->power_ctrl_lock));

			cmdq_mbox_enable(imgsys_clt[0]->chan);

			imgsys_dev->sw_pm_flow_cnt |= PRE_PWR_ON_2;
			pm_ret = mtk_imgsys_resume(imgsys_dev);
			if (pm_ret < 0) {
				dev_err(imgsys_dev->dev,
					"%s: [ERROR] mtk_imgsys_resume FAIL: %d\n", __func__, pm_ret);
				return;
			}
			pm_ret = pm_runtime_get_sync(imgsys_dev->dev);
			if (pm_ret < 0) {
				dev_err(imgsys_dev->dev,
					"%s: [ERROR] PM_RUNTIME_GET_SYNC FAIL: %d\n", __func__, pm_ret);
				return;
			}
			imgsys_dev->sw_pm_flow_cnt |= PRE_PWR_ON_3;

			/*set default value for hw module*/
			mtk_imgsys_mod_get(imgsys_dev);

			for (i = 0; i < imgsys_dev->modules_num; i++)
				if ((BIT(i) & img_main_modules) && imgsys_dev->modules[i].set)
					imgsys_dev->modules[i].set(imgsys_dev);

#ifdef MTK_ISC_SUPPORT
			imgsys_cmdq_sec_isc_init_plat8s(imgsys_dev);
#endif

			MTK_IMGSYS_QOF_NEED_RUN(imgsys_dev->qof_ver,
				mtk_imgsys_cmdq_qof_stream_on(imgsys_dev);
			);

			MTK_IMGSYS_QOS_ENABLE(imgsys_dev->hwqos_info.hwqos_support,
				mtk_imgsys_cmdq_hwqos_streamon(&imgsys_dev->hwqos_info);
			);

			mutex_unlock(&(imgsys_dev->power_ctrl_lock));
		}
	} else {
		user_cnt = atomic_dec_return(&imgsys_dev->imgsys_user_cnt);
		if (user_cnt == 0) {
			if (!imgsys_quick_onoff_enable_plat8s())
				dev_info(dvfs_info->dev,
					"[%s] isPowerOn(%d) user(%d)\n",
					__func__, isPowerOn, user_cnt);

			#if IMGSYS_SECURE_ENABLE
			mutex_lock(&(imgsys_dev->sec_task_lock));
			is_pwr_sec_mode = false;
			mutex_unlock(&(imgsys_dev->sec_task_lock));
			#endif

			mutex_lock(&(imgsys_dev->power_ctrl_lock));

			MTK_IMGSYS_QOS_ENABLE(imgsys_dev->hwqos_info.hwqos_support,
				mtk_imgsys_cmdq_hwqos_streamoff();
			);

			MTK_IMGSYS_QOF_NEED_RUN(imgsys_dev->qof_ver,
				mtk_imgsys_cmdq_qof_stream_off(imgsys_dev);
			);

			mtk_imgsys_mod_put(imgsys_dev);

			imgsys_dev->sw_pm_flow_cnt |= PRE_PWR_OFF_2;
			pm_ret = mtk_imgsys_suspend(imgsys_dev);
			if (pm_ret < 0) {
				dev_err(imgsys_dev->dev,
					"%s: [ERROR] mtk_imgsys_suspend FAIL: %d\n", __func__, pm_ret);
				return;
			}
			pm_ret = pm_runtime_put_sync(imgsys_dev->dev);
			if (pm_ret < 0) {
				dev_err(imgsys_dev->dev,
					"%s: [ERROR] PM_RUNTIME_PUT_SYNC FAIL: %d\n", __func__, pm_ret);
				return;
			}
			imgsys_dev->sw_pm_flow_cnt |= PRE_PWR_OFF_3;
			//pm_runtime_mark_last_busy(imgsys_dev->dev);
			//pm_runtime_put_autosuspend(imgsys_dev->dev);

			cmdq_mbox_disable(imgsys_clt[0]->chan);

			mutex_unlock(&(imgsys_dev->power_ctrl_lock));
		}
	}
}

void mtk_imgsys_main_power_ctrl_plat8s(struct mtk_imgsys_dev *imgsys_dev, bool isPowerOn)
{
	int i;
	const u32 img_main_modules = BIT(IMGSYS_MOD_ADL) |
			BIT(IMGSYS_MOD_ME) |
			BIT(IMGSYS_MOD_IMGMAIN);

	if (isPowerOn) {
		for (i = 0; i < imgsys_dev->modules_num; i++) {
			if ((BIT(i) & img_main_modules) && imgsys_dev->modules[i].set)
				imgsys_dev->modules[i].set(imgsys_dev);
		}
	}
}

#endif

bool imgsys_cmdq_ts_enable_plat8s(void)
{
	return imgsys_cmdq_ts_en;
}

u32 imgsys_wpe_bwlog_enable_plat8s(void)
{
	return imgsys_wpe_bwlog_en;
}

bool imgsys_cmdq_ts_dbg_enable_plat8s(void)
{
	return imgsys_cmdq_ts_dbg_en;
}

bool imgsys_cmdq_dbg_enable_plat8s(void)
{
	return imgsys_cmdq_dbg_en;
}


bool imgsys_dvfs_dbg_enable_plat8s(void)
{
	return imgsys_dvfs_dbg_en;
}

bool imgsys_qos_dbg_enable_plat8s(void)
{
	return imgsys_qos_dbg_en;
}

bool imgsys_quick_onoff_enable_plat8s(void)
{
	return imgsys_quick_onoff_en;
}

bool imgsys_fence_dbg_enable_plat8s(void)
{
	return imgsys_fence_dbg_en;
}

bool imgsys_fine_grain_dvfs_enable_plat8s(void)
{
	return imgsys_fine_grain_dvfs_en;
}

bool imgsy_isc_xfd_dbg_enable_plat8s(void)
{
	return imgsys_isc_xfd_dbg_en;
}

bool imgsys_iova_dbg_enable_plat8s(void)
{
	return imgsys_iova_dbg_en;
}

bool imgsys_cmdq_wpe_retry_enable_plat8s(void)
{
	return !wpe_retry_en;
}


u32 imgsys_iova_dbg_port_plat8s(void)
{
	return imgsys_iova_dbg_port_en;
}

#ifdef IMGSYS_CMDQ_PKT_REUSE
bool imgsys_cmdq_pkt_reuse_disable_plat8s(void)
{
	return imgsys_cmdq_pkt_reuse_dis;
}
#endif

struct imgsys_cmdq_cust_data imgsys_cmdq_data_8s = {
	.cmdq_init = imgsys_cmdq_init_plat8s,
	.cmdq_release = imgsys_cmdq_release_plat8s,
	.cmdq_streamon = imgsys_cmdq_streamon_plat8s,
	.cmdq_streamoff = imgsys_cmdq_streamoff_plat8s,
	.cmdq_sendtask = imgsys_cmdq_sendtask_plat8s,
	.cmdq_sec_sendtask = imgsys_cmdq_sec_sendtask_plat8s,
	.cmdq_sec_cmd = imgsys_cmdq_sec_cmd_plat8s,
	.cmdq_clearevent = imgsys_cmdq_clearevent_plat8s,
#if DVFS_QOS_READY
	.mmdvfs_init = mtk_imgsys_mmdvfs_init_plat8s,
	.mmdvfs_uninit = mtk_imgsys_mmdvfs_uninit_plat8s,
	.mmdvfs_set = mtk_imgsys_mmdvfs_set_plat8s,
	.mmqos_init = mtk_imgsys_mmqos_init_plat8s,
	.mmqos_uninit = mtk_imgsys_mmqos_init_plat8s,
	.mmqos_set_by_scen = mtk_imgsys_mmqos_set_by_scen_plat8s,
	.mmqos_reset = mtk_imgsys_mmqos_reset_plat8s,
	.mmdvfs_mmqos_cal = mtk_imgsys_mmdvfs_mmqos_cal_plat8s,
	.mmqos_bw_cal = mtk_imgsys_mmqos_bw_cal_plat8s,
	.mmqos_ts_cal = mtk_imgsys_mmqos_ts_cal_plat8s,
	.power_ctrl = mtk_imgsys_power_ctrl_plat8s,
	.main_power_ctrl = mtk_imgsys_main_power_ctrl_plat8s,
	.mmqos_monitor = mtk_imgsys_mmqos_monitor_plat8s,
#endif
	.cmdq_ts_en = imgsys_cmdq_ts_enable_plat8s,
	.wpe_bwlog_en = imgsys_wpe_bwlog_enable_plat8s,
	.cmdq_ts_dbg_en = imgsys_cmdq_ts_dbg_enable_plat8s,
	.dvfs_dbg_en = imgsys_dvfs_dbg_enable_plat8s,
	.quick_onoff_en = imgsys_quick_onoff_enable_plat8s,
};
#if (KERNEL_VERSION(6, 13, 0) > LINUX_VERSION_CODE)
MODULE_IMPORT_NS(DMA_BUF);
#else
MODULE_IMPORT_NS("DMA_BUF");
#endif
