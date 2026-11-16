// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <asm/arch_timer.h>
#include <linux/sched/clock.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <soc/mediatek/emi.h>
#include <linux/delay.h>

#include "mtk_cam.h"
#include "mtk_cam-job_utils.h"
#include "mtk_cam-reg_utils.h"
#include "mtk_cam-fmon.h"
#include "mtk_cam-fmon_regs.h"
#if IS_ENABLED(CONFIG_MTK_MBRAINK_BRIDGE)
#include "mbraink_bridge_hrt.h"
#endif

/* ep default fix yuv a/b/c */
static int fmon_enable = 1;
module_param(fmon_enable, int, 0644);
MODULE_PARM_DESC(fmon_enable, "debug fifo monitor 0, rx:0-3, tx:4-7");

static int dbg_fmon_bypass;
module_param(dbg_fmon_bypass, int, 0644);
MODULE_PARM_DESC(dbg_fmon_bypass, "fifo monitor bypass mode");

static int dbg_fmon_bypass_trigger;
module_param(dbg_fmon_bypass_trigger, int, 0644);
MODULE_PARM_DESC(dbg_fmon_bypass_trigger, "fifo monitor bypass trigger event");

static int dbg_fmon0_mux = -1;
module_param(dbg_fmon0_mux, int, 0644);
MODULE_PARM_DESC(dbg_fmon0_mux, "debug fifo monitor 0, engine: 15-8, tx:4-7, rx:0-3");

static int dbg_fmon1_mux = -1;
module_param(dbg_fmon1_mux, int, 0644);
MODULE_PARM_DESC(dbg_fmon1_mux, "debug fifo monitor 1, engine: 15-8, tx:4-7, rx:0-3");

static int dbg_fmon2_mux = -1;
module_param(dbg_fmon2_mux, int, 0644);
MODULE_PARM_DESC(dbg_fmon2_mux, "debug fifo monitor 2, engine: 15-8, tx:4-7, rx:0-3");

static int dbg_fmon3_mux = -1;
module_param(dbg_fmon3_mux, int, 0644);
MODULE_PARM_DESC(dbg_fmon3_mux, "debug fifo monitor 3, engine: 15-8, tx:4-7, rx:0-3");

static int dbg_threshold_pr = 0x3C;
module_param(dbg_threshold_pr, int, 0644);
MODULE_PARM_DESC(dbg_threshold_pr, "fifo monitor start threshold percentage");

static int dbg_start_threshold = -1;
module_param(dbg_start_threshold, int, 0644);
MODULE_PARM_DESC(dbg_start_threshold, "fifo monitor start threshold ratio");

static int dbg_stop_threshold = -1;
module_param(dbg_stop_threshold, int, 0644);
MODULE_PARM_DESC(dbg_stop_threshold, "fifo monitor stop threshold ratio");

static int dbg_fmon_tx_csr = -1;
module_param(dbg_fmon_tx_csr, int, 0644);
MODULE_PARM_DESC(dbg_fmon_tx_csr, "fifo monitor tx csr");

static int fmon_mbrain_enable = 1;
module_param(fmon_mbrain_enable, int, 0644);

#define FMON_THRS_RATIO_N 60
#define FMON_THRS_RATIO_D 100
#define FMON_STOP_THRS_RATIO_N 100
#define FMON_STOP_THRS_RATIO_D 100
#define FMON_STOP_THRS_MAX     0x1FFF

bool is_fmon_support(void)
{
	return fmon_enable && GET_PLAT_HW(fmon_support) && is_hrt_dbg_enabled();
}

static u32 stop_thres_ratio(u32 size)
{
	if (dbg_stop_threshold != -1)
		return (size * dbg_stop_threshold) / FMON_STOP_THRS_RATIO_D;
	else
		return (size * FMON_STOP_THRS_RATIO_N) / FMON_STOP_THRS_RATIO_D;
}

static u32 thres_ratio(u32 size)
{
	if (dbg_start_threshold != -1)
		return (size * dbg_start_threshold) / FMON_THRS_RATIO_D;
	else
		return (size * FMON_THRS_RATIO_N) / FMON_THRS_RATIO_D;
}

static struct fmon_settings fmon_map
	[FPIPE_INFO_NUM][FPIPE_INFO_NUM][FPIPE_INFO_NUM][FMON_NUM] = {
#ifdef FMON_OTF_SUBA_TEST
	[FPIPE_RAW][FPIPE_NONE][FPIPE_NONE] = {
		{ .tx_mux = 3, .rx_mux = 1, .engine = FMON_RAW_A, .fifo_size = 7424, },
		{ .tx_mux = 0, .rx_mux = 0, .engine = FMON_NONE,  .fifo_size = 0,   },
		{ .tx_mux = 0, .rx_mux = 1, .engine = FMON_YUV_A, .fifo_size = 5888, },
		{ .tx_mux = 0, .rx_mux = 0, .engine = FMON_NONE,  .fifo_size = 0,   },
	},
#endif
	/* pipe-a dc only */
	[FPIPE_SV][FPIPE_NONE][FPIPE_NONE] = {
		{ .tx_mux = 0, .rx_mux = 0, .engine = FMON_CAMSV_0, .fifo_size = 1536, },
		{ .tx_mux = 1, .rx_mux = 0, .engine = FMON_CAMSV_1, .fifo_size = 1536, },
		{ .tx_mux = 4, .rx_mux = 0, .engine = FMON_CAMSV_2, .fifo_size = 1536, },
		{ .tx_mux = 1, .rx_mux = 0, .engine = FMON_CAMSV_3, .fifo_size = 1536, },
	},
	/* pipe-b dc only */
	[FPIPE_NONE][FPIPE_SV][FPIPE_NONE] = {
		{ .tx_mux = 3, .rx_mux = 0, .engine = FMON_CAMSV_0, .fifo_size = 1536, },
		{ .tx_mux = 0, .rx_mux = 0, .engine = FMON_CAMSV_1, .fifo_size = 1536, },
		{ .tx_mux = 3, .rx_mux = 0, .engine = FMON_CAMSV_2, .fifo_size = 1536, },
		{ .tx_mux = 0, .rx_mux = 0, .engine = FMON_CAMSV_3, .fifo_size = 1536, },
	},
	/* pipe-c dc only */
	[FPIPE_NONE][FPIPE_NONE][FPIPE_SV] = {
		{ .tx_mux = 2, .rx_mux = 0, .engine = FMON_CAMSV_0, .fifo_size = 1536, },
		{ .tx_mux = 0, .rx_mux = 0, .engine = FMON_CAMSV_1, .fifo_size = 1536, },
		{ .tx_mux = 0, .rx_mux = 0, .engine = FMON_CAMSV_2, .fifo_size = 1536, },
		{ .tx_mux = 0, .rx_mux = 0, .engine = FMON_CAMSV_3, .fifo_size = 1536, },
	},
	/* pipe-a,b dc */
	[FPIPE_SV][FPIPE_SV][FPIPE_NONE] = {
		{ .tx_mux = 3, .rx_mux = 0, .engine = FMON_CAMSV_0, .fifo_size = 1536, },
		{ .tx_mux = 1, .rx_mux = 0, .engine = FMON_CAMSV_1, .fifo_size = 1536, },
		{ .tx_mux = 3, .rx_mux = 0, .engine = FMON_CAMSV_2, .fifo_size = 1536, },
		{ .tx_mux = 1, .rx_mux = 0, .engine = FMON_CAMSV_3, .fifo_size = 1536, },
	},
	/* pipe-b,c dc */
	[FPIPE_NONE][FPIPE_SV][FPIPE_SV] = {
		{ .tx_mux = 3, .rx_mux = 0, .engine = FMON_CAMSV_0, .fifo_size = 1536, },
		{ .tx_mux = 0, .rx_mux = 0, .engine = FMON_CAMSV_1, .fifo_size = 1536, },
		{ .tx_mux = 3, .rx_mux = 0, .engine = FMON_CAMSV_2, .fifo_size = 1536, },
		{ .tx_mux = 0, .rx_mux = 0, .engine = FMON_CAMSV_3, .fifo_size = 1536, },
	},
	/* pipe-a,c dc */
	[FPIPE_SV][FPIPE_NONE][FPIPE_SV] = {
		{ .tx_mux = 2, .rx_mux = 0, .engine = FMON_CAMSV_0, .fifo_size = 1536, },
		{ .tx_mux = 0, .rx_mux = 0, .engine = FMON_CAMSV_1, .fifo_size = 1536, },
		{ .tx_mux = 4, .rx_mux = 0, .engine = FMON_CAMSV_2, .fifo_size = 1536, },
		{ .tx_mux = 1, .rx_mux = 0, .engine = FMON_CAMSV_3, .fifo_size = 1536, },
	},
	/* pipe-a,b,c dc */
	[FPIPE_SV][FPIPE_SV][FPIPE_SV] = {
		{ .tx_mux = 3, .rx_mux = 0, .engine = FMON_CAMSV_0, .fifo_size = 1536, },
		{ .tx_mux = 0, .rx_mux = 0, .engine = FMON_CAMSV_1, .fifo_size = 1536, },
		{ .tx_mux = 3, .rx_mux = 0, .engine = FMON_CAMSV_2, .fifo_size = 1536, },
		{ .tx_mux = 1, .rx_mux = 0, .engine = FMON_CAMSV_3, .fifo_size = 1536, },
	},
	/* more combination ... */
};

static void fmon_rx_mux(struct mtk_fmon_device *fmon,
			enum FMON_INDEX fmon_idx, enum FMON_RX_MUX rx_mux)
{
	u32 val;

	val = readl(fmon->base + REG_CAM_FMON_SETTING);
	switch(fmon_idx) {
	case FMON_0:
		SET_FIELD(&val, CAM_FMON_SRC_SEL_0, rx_mux);
	break;
	case FMON_1:
		SET_FIELD(&val, CAM_FMON_SRC_SEL_1, rx_mux);
	break;
	case FMON_2:
		SET_FIELD(&val, CAM_FMON_SRC_SEL_2,  rx_mux);
	break;
	case FMON_3:
		SET_FIELD(&val, CAM_FMON_SRC_SEL_3, rx_mux);
	break;
	default:
		pr_info("%s: unsupport indx\n", __func__);
	break;
	}

	writel(val, fmon->base + REG_CAM_FMON_SETTING);
}

static void fmon_tx_mux(struct mtk_fmon_device *fmon,
		enum FMON_ENGINE engine, enum FMON_RX_MUX tx_mux)
{
	void __iomem *base = NULL;

	switch(engine) {
	case FMON_RAW_A:
		base = fmon->raw_a_tx;
	break;
	case FMON_RAW_B:
		base = fmon->raw_b_tx;
	break;
	case FMON_RAW_C:
		base = fmon->raw_c_tx;
	break;
	case FMON_YUV_A:
		base = fmon->yuv_a_tx;
	break;
	case FMON_YUV_B:
		base = fmon->yuv_b_tx;
	break;
	case FMON_YUV_C:
		base = fmon->yuv_c_tx;
	break;
	case FMON_CAMSV_0:
		base = fmon->camsv_tx_0;
	break;
	case FMON_CAMSV_1:
		base = fmon->camsv_tx_1;
	break;
	case FMON_CAMSV_2:
		base = fmon->camsv_tx_2;
	break;
	case FMON_CAMSV_3:
		base = fmon->camsv_tx_3;
	break;
	default:
		if (CAM_DEBUG_ENABLED(FMON))
			pr_info("%s: unsupport engine\n", __func__);
		return;
	break;
	}

	writel(((dbg_fmon_tx_csr != -1) ? 5 : tx_mux) << 1 | 0x3f1 |
		((dbg_fmon_tx_csr != -1) ? dbg_fmon_tx_csr << 10 : 0), base);

	if (CAM_DEBUG_ENABLED(FMON))
		pr_info("%s: engine:%d tx_mux:0x%x\n", __func__, engine, readl(base));
}

static void fmon_threshold(struct mtk_fmon_device *fmon,
			enum FMON_INDEX fmon_idx, int fifo_size, int is_sv)
{
	u32 stop_thr_0, stop_thr_1;
	u32 fifo_thr = 0;

	if (!fifo_size) {
		if (CAM_DEBUG_ENABLED(FMON))
			pr_info("%s: skip for montor-%d\n", __func__, fmon_idx);
		return;
	}

	stop_thr_0 = readl(fmon->base + REG_CAM_FMON_WIN_DELAY_THR_0);
	stop_thr_1 = readl(fmon->base + REG_CAM_FMON_WIN_DELAY_THR_1);

	/* start threshold percentage is 60% */
	switch(fmon_idx) {
	case FMON_0:
		SET_FIELD(&stop_thr_0, CAM_FMON_TRIG_STOP_FIFO_THR_0,
			is_sv ? FMON_STOP_THRS_MAX : stop_thres_ratio(fifo_size));
		writel(stop_thr_0, fmon->base + REG_CAM_FMON_WIN_DELAY_THR_0);

		SET_FIELD(&fifo_thr, CAM_FMON_PER_THR_0, dbg_threshold_pr);
		SET_FIELD(&fifo_thr, CAM_FMON_FIFO_THR_0, thres_ratio(fifo_size));
		writel(fifo_thr, fmon->base + REG_CAM_FMON_THRESHOLD_0);
	break;
	case FMON_1:
		SET_FIELD(&stop_thr_0, CAM_FMON_TRIG_STOP_FIFO_THR_1,
			is_sv ? FMON_STOP_THRS_MAX : stop_thres_ratio(fifo_size));
		writel(stop_thr_0, fmon->base + REG_CAM_FMON_WIN_DELAY_THR_0);

		SET_FIELD(&fifo_thr, CAM_FMON_PER_THR_1, dbg_threshold_pr);
		SET_FIELD(&fifo_thr, CAM_FMON_FIFO_THR_1, thres_ratio(fifo_size));
		writel(fifo_thr, fmon->base + REG_CAM_FMON_THRESHOLD_1);
	break;
	case FMON_2:
		SET_FIELD(&stop_thr_1, CAM_FMON_TRIG_STOP_FIFO_THR_2,
			is_sv ? FMON_STOP_THRS_MAX : stop_thres_ratio(fifo_size));
		writel(stop_thr_1, fmon->base + REG_CAM_FMON_WIN_DELAY_THR_1);

		SET_FIELD(&fifo_thr, CAM_FMON_PER_THR_2, dbg_threshold_pr);
		SET_FIELD(&fifo_thr, CAM_FMON_FIFO_THR_2, thres_ratio(fifo_size));
		writel(fifo_thr, fmon->base + REG_CAM_FMON_THRESHOLD_2);
	break;
	case FMON_3:
		SET_FIELD(&stop_thr_1, CAM_FMON_TRIG_STOP_FIFO_THR_3,
			is_sv ? FMON_STOP_THRS_MAX : stop_thres_ratio(fifo_size));
		writel(stop_thr_1, fmon->base + REG_CAM_FMON_WIN_DELAY_THR_1);

		SET_FIELD(&fifo_thr, CAM_FMON_PER_THR_3, dbg_threshold_pr);
		SET_FIELD(&fifo_thr, CAM_FMON_FIFO_THR_3, thres_ratio(fifo_size));
		writel(fifo_thr, fmon->base + REG_CAM_FMON_THRESHOLD_3);
	break;
	default:
		pr_info("%s: unsupport fmon index\n", __func__);
	break;
	}

	if (CAM_DEBUG_ENABLED(FMON))
		pr_info("%s: montor-%d delay_thr_0/1: 0x%x/0x%x fmon_threshold0/1/2/3: 0x%x/0x%x/0x%x/0x%x\n",
			__func__, fmon_idx,
			readl(fmon->base + REG_CAM_FMON_WIN_DELAY_THR_0),
			readl(fmon->base + REG_CAM_FMON_WIN_DELAY_THR_1),
			readl(fmon->base + REG_CAM_FMON_THRESHOLD_0),
			readl(fmon->base + REG_CAM_FMON_THRESHOLD_1),
			readl(fmon->base + REG_CAM_FMON_THRESHOLD_2),
			readl(fmon->base + REG_CAM_FMON_THRESHOLD_3));
}

static void fmon_debug_setings(enum FMON_INDEX index, struct fmon_settings *setting)
{
	switch(index) {
	case FMON_0:
		if (dbg_fmon0_mux != -1) {
			setting->engine = (dbg_fmon0_mux & 0xf00) >> 4;
			setting->tx_mux = (dbg_fmon0_mux & 0xf0) >> 4;
			setting->rx_mux = dbg_fmon0_mux & 0xf;
		}
	break;
	case FMON_1:
		if (dbg_fmon1_mux != -1) {
			setting->engine = (dbg_fmon1_mux & 0xf00) >> 4;
			setting->tx_mux = (dbg_fmon1_mux & 0xf0) >> 4;
			setting->rx_mux = dbg_fmon1_mux & 0xf;
		}
	break;
	case FMON_2:
		if (dbg_fmon2_mux != -1) {
			setting->engine = (dbg_fmon2_mux & 0xf00) >> 4;
			setting->tx_mux = (dbg_fmon2_mux & 0xf0) >> 4;
			setting->rx_mux = dbg_fmon2_mux & 0xf;
		}
	break;
	case FMON_3:
		if (dbg_fmon3_mux != -1) {
			setting->engine = (dbg_fmon3_mux & 0xf00) >> 4;
			setting->tx_mux = (dbg_fmon3_mux & 0xf0) >> 4;
			setting->rx_mux = dbg_fmon3_mux & 0xf;
		}
	break;
	default:
		pr_info("%s: unsupport fmon indx\n", __func__);
	break;
	}

	if (CAM_DEBUG_ENABLED(FMON))
		pr_info("%s: monitor/engine/tx_mux/rx_mux:%d/%d/%d/%d\n",
			__func__, index, setting->engine, setting->tx_mux, setting->rx_mux);
}

static void fmon_bind_engine(struct mtk_fmon_device *fmon)
{
	struct fmon_settings setting;
	int i = 0;

	for (i = 0; i < FMON_NUM ; i++) {
		setting = fmon_map[fmon->pipes[0]][fmon->pipes[1]][fmon->pipes[2]][i];
		fmon_debug_setings(i, &setting);
		fmon_rx_mux(fmon, i, setting.rx_mux);
		fmon_tx_mux(fmon, setting.engine, setting.tx_mux);
		fmon_threshold(fmon, i, setting.fifo_size, is_camsv_engine(setting.engine));
	}

	mtk_cam_fmon_dump(fmon);
}

void mtk_cam_fmon_bind(struct mtk_fmon_device *fmon, unsigned int used_raw, bool sv_on)
{
	u32 master_id;

	if (!is_fmon_support())
		return;

	mutex_lock(&fmon->op_lock);
	master_id = get_master_raw_id(used_raw);
	if (master_id >= 3) {
		pr_info("%s: error: invalid master\n", __func__);
		goto OUT;
	}

	/* only monitor camsv */
	fmon->pipes[master_id] = sv_on ? FPIPE_SV : FPIPE_NONE;

	fmon_bind_engine(fmon);

OUT:
	mutex_unlock(&fmon->op_lock);

	if (CAM_DEBUG_ENABLED(FMON))
		pr_info("%s: pipes:%d/%d/%d, used_raw:0x%x\n",
			__func__, fmon->pipes[0], fmon->pipes[1], fmon->pipes[2], used_raw);
}

void mtk_cam_fmon_unbind(struct mtk_fmon_device *fmon, unsigned int used_raw)
{
	u32 master_id;

	if (!is_fmon_support())
		return;

	mutex_lock(&fmon->op_lock);
	master_id = get_master_raw_id(used_raw);
	if (master_id >= 3) {
		pr_info("%s: error: invalid master\n", __func__);
		goto OUT;
	}

	fmon->pipes[master_id] = FPIPE_NONE;
	fmon_bind_engine(fmon);

OUT:
	mutex_unlock(&fmon->op_lock);

	if (CAM_DEBUG_ENABLED(FMON))
		pr_info("%s: pipes:%d/%d/%d, used_raw:0x%x\n",
			__func__, fmon->pipes[0], fmon->pipes[1], fmon->pipes[2], used_raw);
}

void fmon_reset_timer_fn(struct timer_list *timer)
{
	struct mtk_fmon_device *fmon =
		container_of(timer, struct mtk_fmon_device, reset_timer);
	u32 fmon_setting2 = readl(fmon->base + REG_CAM_FMON_SETTING_2);

	writel(fmon_setting2 | 0xF, fmon->base + REG_CAM_FMON_SETTING_2);

	pr_info("%s: fmon_setting_2:0x%x\n", __func__,
					readl(fmon->base + REG_CAM_FMON_SETTING_2));
}

int mtk_cam_fmon_reset_msgfifo(struct mtk_fmon_device *fmon)
{
	atomic_set(&fmon->is_fifo_overflow, 0);
	return kfifo_init(&fmon->msg_fifo, fmon->msg_buffer, fmon->fifo_size);
}

static int push_msgfifo(struct mtk_fmon_device *fmon,
			struct mtk_fmon_irq_info *info)
{
	int len;

	if (unlikely(kfifo_avail(&fmon->msg_fifo) < sizeof(*info))) {
		atomic_set(&fmon->is_fifo_overflow, 1);
		return -1;
	}

	len = kfifo_in(&fmon->msg_fifo, info, sizeof(*info));
	WARN_ON(len != sizeof(*info));

	return 0;
}

/* once by cam-main pwr on */
#define FMON_TRIG_START_MASK 0x00000F0F
#define FMON_TRIG_STOP_MASK  0x000000F0
void mtk_cam_fmon_enable(struct mtk_fmon_device *fmon)
{
	u32 val = 0;

	if (!is_fmon_support())
		return;

	mutex_lock(&fmon->op_lock);

	atomic_set(&fmon->fmon_triggered, 0);
	mtk_cam_fmon_reset_msgfifo(fmon);

	if (dbg_fmon_bypass) {
		writel(0x1e000, fmon->base + REG_CAM_FMON_SETTING_3);
		writel(0x0, fmon->base + REG_CAM_FMON_SETTING);
		pr_info("%s: bypass mode fmon_setting0/3: 0x%x/0x%x\n", __func__,
			readl(fmon->base + REG_CAM_FMON_SETTING),
			readl(fmon->base + REG_CAM_FMON_SETTING_3));
		mutex_unlock(&fmon->op_lock);
		return;
	}

	val = readl(fmon->base + REG_CAM_FMON_IRQ_TRIG);
	SET_FIELD(&val, CAM_FMON_CSR_IRQ_CLR_MODE, 0);
	writel(val, fmon->base + REG_CAM_FMON_IRQ_TRIG);

	/* clear fmon#0~3 irq/urgent */
	val = readl(fmon->base + REG_CAM_FMON_SETTING);
	SET_FIELD(&val, CAM_FMON_IRQ_CLEAR, 1);
	SET_FIELD(&val, CAM_FMON_URGENT_CLEAR, 1);
	SET_FIELD(&val, CAM_FMON_STOP_MASK_0, 1);
	SET_FIELD(&val, CAM_FMON_STOP_MASK_1, 1);
	SET_FIELD(&val, CAM_FMON_STOP_MASK_2, 1);
	SET_FIELD(&val, CAM_FMON_STOP_MASK_3, 1);
	writel(val, fmon->base + REG_CAM_FMON_SETTING);

	val = readl(fmon->base + REG_CAM_FMON_SETTING);
	/* todo: set fifo type 0:read or 1:write */
	SET_FIELD(&val, CAM_FMON_FIFO_TYPE_0, 1);
	SET_FIELD(&val, CAM_FMON_FIFO_TYPE_1, 1);
	SET_FIELD(&val, CAM_FMON_FIFO_TYPE_2, 1);
	SET_FIELD(&val, CAM_FMON_FIFO_TYPE_3, 1);
	writel(val, fmon->base + REG_CAM_FMON_SETTING);
	if (dbg_fmon_bypass_trigger) {
		SET_FIELD(&val, CAM_FMON_CTI_STOP_PATH_MSK_0, 0);
		SET_FIELD(&val, CAM_FMON_CTI_STOP_PATH_MSK_1, 0);
		SET_FIELD(&val, CAM_FMON_CTI_STOP_PATH_MSK_2, 0);
		SET_FIELD(&val, CAM_FMON_CTI_STOP_PATH_MSK_3, 0);
		SET_FIELD(&val, CAM_FMON_CTI_START_PATH_MSK_0, 0);
		SET_FIELD(&val, CAM_FMON_CTI_START_PATH_MSK_1, 0);
		SET_FIELD(&val, CAM_FMON_CTI_START_PATH_MSK_2, 0);
		SET_FIELD(&val, CAM_FMON_CTI_START_PATH_MSK_3, 0);
	}
	val = readl(fmon->base + REG_CAM_FMON_SETTING_3);

	SET_FIELD(&val, CAM_FMON_FMON_MODE, 1);
	writel(val, fmon->base + REG_CAM_FMON_SETTING_3);

	//ela 0x3c824000 + 0x908 bit-4 = 1
	val = readl(fmon->ela_ctrl);
	writel(val | BIT(4), fmon->ela_ctrl);

	/* 26M * 256 = 9.85us */
	val = readl(fmon->base + REG_CAM_FMON_WIND_SET_0);
	SET_FIELD(&val, CAM_FMON_WINDOW_LEN_0, 0x100);
	writel(val, fmon->base + REG_CAM_FMON_WIND_SET_0);
	SET_FIELD(&val, CAM_FMON_WINDOW_LEN_1, 0x100);
	writel(val, fmon->base + REG_CAM_FMON_WIND_SET_0);
	SET_FIELD(&val, CAM_FMON_WINDOW_LEN_2, 0x100);
	writel(val, fmon->base + REG_CAM_FMON_WIND_SET_1);
	SET_FIELD(&val, CAM_FMON_WINDOW_LEN_3, 0x100);
	writel(val, fmon->base + REG_CAM_FMON_WIND_SET_1);

	//initialize start threshold
	writel(0xFFFFFFFF, fmon->base + REG_CAM_FMON_THRESHOLD_0);
	writel(0xFFFFFFFF, fmon->base + REG_CAM_FMON_THRESHOLD_1);
	writel(0xFFFFFFFF, fmon->base + REG_CAM_FMON_THRESHOLD_2);
	writel(0xFFFFFFFF, fmon->base + REG_CAM_FMON_THRESHOLD_3);
	//initialize stop threshold
	writel(0xFFFFFFFF, fmon->base + REG_CAM_FMON_WIN_DELAY_THR_0);
	writel(0xFFFFFFFF, fmon->base + REG_CAM_FMON_WIN_DELAY_THR_1);

	/* enable fifo monitor */
	val = readl(fmon->base + REG_CAM_FMON_SETTING_2);
	writel(val | FMON_TRIG_START_MASK | FMON_TRIG_STOP_MASK, fmon->base + REG_CAM_FMON_SETTING_2);
	val = readl(fmon->base + REG_CAM_FMON_SETTING);
	SET_FIELD(&val, CAM_FMON_FIFO_MON_EN, 1);
	SET_FIELD(&val, CAM_FMON_ELA_BUS_SEL, 1);
	writel(val, fmon->base + REG_CAM_FMON_SETTING);

	mutex_unlock(&fmon->op_lock);

	timer_setup(&fmon->reset_timer, fmon_reset_timer_fn, 0);

	if (CAM_DEBUG_ENABLED(FMON))
		pr_info("%s: irq_trig:0x%x fmon_wind_set_0/1:0x%x/0x%x fmon_setting0/2/3:0x%x/0x%x/0x%x ela_ctrl:0x%x\n",
			__func__,
			readl(fmon->base + REG_CAM_FMON_IRQ_TRIG),
			readl(fmon->base + REG_CAM_FMON_WIND_SET_0),
			readl(fmon->base + REG_CAM_FMON_WIND_SET_1),
			readl(fmon->base + REG_CAM_FMON_SETTING),
			readl(fmon->base + REG_CAM_FMON_SETTING_2),
			readl(fmon->base + REG_CAM_FMON_SETTING_3),
			readl(fmon->ela_ctrl));

	enable_irq(fmon->irq);
}

/* once by cam-main pwr on */
void mtk_cam_fmon_disable(struct mtk_fmon_device *fmon)
{
	u32 val, mminfra_cti_st, apinfra_cti_st, cti_set;

	if (!is_fmon_support())
		return;

	mutex_lock(&fmon->op_lock);
	val = readl(fmon->base + REG_CAM_FMON_SETTING);
	SET_FIELD(&val, CAM_FMON_FIFO_MON_EN, 0);
	SET_FIELD(&val, CAM_FMON_ELA_BUS_SEL, 0);
	writel(val, fmon->base + REG_CAM_FMON_SETTING);
	writel(val & (~BIT(4)), fmon->ela_ctrl);

	//reset to default
	writel(0x1e1eff, fmon->base + REG_CAM_FMON_SETTING_3);
	writel(0x80f000, fmon->base + REG_CAM_FMON_SETTING);

	memset(&fmon->pipes, 0, sizeof(fmon->pipes));

	mutex_unlock(&fmon->op_lock);

#if (KERNEL_VERSION(6, 15, 0) > LINUX_VERSION_CODE)
	del_timer_sync(&fmon->reset_timer);
#else
	timer_delete_sync(&fmon->reset_timer);
#endif
	disable_irq(fmon->irq);

	/* cti force stop if start is trigged */
	if (atomic_read(&fmon->fmon_triggered))
		writel(1, fmon->cti_set);

	udelay(50);
	mminfra_cti_st = readl(fmon->mminfra_cti_st);
	apinfra_cti_st = readl(fmon->apinfra_cti_st);
	cti_set = readl(fmon->cti_set);
	writel(1, fmon->cti_clear);

	pr_info("%s: fmon_setting:0x%x, trigged:%d cti_set:0x%x/0x%x mm_cti_st:0x%x/0x%x ap_cti_st:0x%x/0x%x\n",
		__func__, readl(fmon->base + REG_CAM_FMON_SETTING),
		atomic_read(&fmon->fmon_triggered),
		cti_set, readl(fmon->cti_set),
		mminfra_cti_st, readl(fmon->mminfra_cti_st),
		apinfra_cti_st, readl(fmon->apinfra_cti_st));
}

void mtk_cam_fmon_dump(struct mtk_fmon_device *fmon)
{
	if (!is_fmon_support())
		return;

	pr_info("%s: pipes:%d/%d/%d setting/2/3:0x%x/0x%x/0x%x delay_thr_0/1: 0x%x/0x%x fmon_threshold0/1/2/3: 0x%x/0x%x/0x%x/0x%x\n",
		__func__,
		fmon->pipes[0], fmon->pipes[1], fmon->pipes[2],
		readl(fmon->base + REG_CAM_FMON_SETTING),
		readl(fmon->base + REG_CAM_FMON_SETTING_2),
		readl(fmon->base + REG_CAM_FMON_SETTING_3),
		readl(fmon->base + REG_CAM_FMON_WIN_DELAY_THR_0),
		readl(fmon->base + REG_CAM_FMON_WIN_DELAY_THR_1),
		readl(fmon->base + REG_CAM_FMON_THRESHOLD_0),
		readl(fmon->base + REG_CAM_FMON_THRESHOLD_1),
		readl(fmon->base + REG_CAM_FMON_THRESHOLD_2),
		readl(fmon->base + REG_CAM_FMON_THRESHOLD_3));
}

static irqreturn_t mtk_irq_fmon(int irq, void *data)
{
	struct mtk_cam_device *drvdata = (struct mtk_cam_device *)data;
	struct mtk_fmon_device *fmon = &drvdata->fmon;
	struct mtk_fmon_irq_info irq_info;
	bool wake_thread = 0;
	u32 fmon_setting2 = 0;
	u32 val = 0;

	fmon_setting2 = readl_relaxed(fmon->base + REG_CAM_FMON_SETTING_2);

	irq_info.irq_type = 0;
	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.irq_status = fmon_setting2;

	/* fifo > 40% urgent start */
	if (READ_FIELD(fmon_setting2, CAM_FMON_STATUS_0) & BIT(1) ||
		READ_FIELD(fmon_setting2, CAM_FMON_STATUS_1) & BIT(1) ||
		READ_FIELD(fmon_setting2, CAM_FMON_STATUS_2) & BIT(1) ||
		READ_FIELD(fmon_setting2, CAM_FMON_STATUS_3) & BIT(1)) {
		writel(fmon_setting2 & ~(0xF00), fmon->base + REG_CAM_FMON_SETTING_2);
		irq_info.irq_type |= FMON_SIG_URGENT;
		atomic_set(&fmon->fmon_triggered, 1);
	}

	/* fifo > 60% */
	if (READ_FIELD(fmon_setting2, CAM_FMON_STATUS_0) & BIT(0) ||
		READ_FIELD(fmon_setting2, CAM_FMON_STATUS_1) & BIT(0) ||
		READ_FIELD(fmon_setting2, CAM_FMON_STATUS_2) & BIT(0) ||
		READ_FIELD(fmon_setting2, CAM_FMON_STATUS_3) & BIT(0)) {
		writel(fmon_setting2 & ~(0xF), fmon->base + REG_CAM_FMON_SETTING_2);
		irq_info.irq_type |= FMON_SIG_START;
		atomic_set(&fmon->fmon_triggered, 1);
	}

	/* fifo > 90% */
	if (READ_FIELD(fmon_setting2, CAM_FMON_STATUS_0) & BIT(2) ||
		READ_FIELD(fmon_setting2, CAM_FMON_STATUS_1) & BIT(2) ||
		READ_FIELD(fmon_setting2, CAM_FMON_STATUS_2) & BIT(2) ||
		READ_FIELD(fmon_setting2, CAM_FMON_STATUS_3) & BIT(2)) {
		writel(fmon_setting2 & ~(0xFFF), fmon->base + REG_CAM_FMON_SETTING_2);
		/* force trigger stop instuction */
		writel(1, fmon->cti_set);

		/* stop monitor */
		val = readl(fmon->base + REG_CAM_FMON_SETTING);
		SET_FIELD(&val, CAM_FMON_FIFO_MON_EN, 0);
		SET_FIELD(&val, CAM_FMON_ELA_BUS_SEL, 0);
		writel(val, fmon->base + REG_CAM_FMON_SETTING);
		irq_info.irq_type |= FMON_SIG_STOP;
	}

	val = readl(fmon->base + REG_CAM_FMON_SETTING);
	SET_FIELD(&val, CAM_FMON_IRQ_CLEAR, 1);
	SET_FIELD(&val, CAM_FMON_URGENT_CLEAR, 1);
	writel(val, fmon->base + REG_CAM_FMON_SETTING);

	if (push_msgfifo(fmon, &irq_info) == 0)
		wake_thread = 1;

	return wake_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}

#define FMON_RECOVER_TIMER 10000
static irqreturn_t mtk_thread_irq_fmon(int irq, void *data)
{
	struct mtk_cam_device *cam = (struct mtk_cam_device *)data;
	struct mtk_fmon_device *fmon = &cam->fmon;
	struct mtk_fmon_irq_info irq_info;
	u64 systimer_cnt = arch_timer_read_counter();
	u64 sched_clock_value = sched_clock();

	if (unlikely(atomic_cmpxchg(&fmon->is_fifo_overflow, 1, 0)))
		pr_info("msg fifo overflow\n");

	while (kfifo_len(&fmon->msg_fifo) >= sizeof(irq_info)) {
		int len = kfifo_out(&fmon->msg_fifo, &irq_info, sizeof(irq_info));

		WARN_ON(len != sizeof(irq_info));

		pr_info("FMON INT: setting2:0x%x, top_funnel:0x%x, mm_funnel:0x%x, systimer:%llu ns, ktime: %llu ns\n",
			irq_info.irq_status, readl(fmon->trace_top_funnel), readl(fmon->mminfra_funnel),
			systimer_cnt, sched_clock_value);

		if (irq_info.irq_type & FMON_SIG_START) {
			if (fmon_mbrain_enable) {
#if IS_ENABLED(CONFIG_MTK_MBRAINK_BRIDGE)
				mtk_mbrain2isp_hrt_cb(FMON_THRS_RATIO_N);
				/* trigger timer to recovery settings */
				mod_timer(&fmon->reset_timer, jiffies + msecs_to_jiffies(FMON_RECOVER_TIMER));
#endif
			}
		} else if (irq_info.irq_type & FMON_SIG_STOP) {
			if (fmon_mbrain_enable) {
#if IS_ENABLED(CONFIG_MTK_MBRAINK_BRIDGE)
				mtk_mbrain2isp_hrt_cb(FMON_STOP_THRS_RATIO_N);
#endif
			}
		}
	}

	return IRQ_HANDLED;
}

int mtk_cam_fmon_probe(struct platform_device *pdev, struct mtk_cam_device *cam)
{
	struct mtk_fmon_device *fmon = &cam->fmon;
	struct resource *res;
	struct device *dev = &pdev->dev;
	int ret = 0;

	dev_info(dev, "fmon probe\n");

	/* base register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "fmon_base");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	fmon->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(fmon->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(fmon->base);
	}

	/* ela/cti */
	fmon->ela_ctrl = ioremap(0x3c824000 + 0x908, 0x4);
	if (IS_ERR(fmon->ela_ctrl))
		dev_err(dev, "%s: failed to map ela_ctrl\n", __func__);

	fmon->cti_set = ioremap(0x3c82c014, 0x4);
	if (IS_ERR(fmon->cti_set))
		dev_err(dev, "%s: failed to map cti_set\n", __func__);

	fmon->cti_clear = ioremap(0x3c82c018, 0x4);
	if (IS_ERR(fmon->cti_clear))
		dev_err(dev, "%s: failed to map cti_clear\n", __func__);

	fmon->mminfra_funnel = ioremap(0x30a2f000, 0x4);
	if (IS_ERR(fmon->mminfra_funnel))
		dev_err(dev, "%s: failed to map mminfra_funnel\n", __func__);

	fmon->trace_top_funnel = ioremap(0x0d070000, 0x4);
	if (IS_ERR(fmon->trace_top_funnel))
		dev_err(dev, "%s: failed to map trace_top_funnel\n", __func__);

	fmon->mminfra_cti_st = ioremap(0x30a2b138, 0x4);
	if (IS_ERR(fmon->mminfra_cti_st))
		dev_err(dev, "%s: failed to map mminfra_cti_st\n", __func__);

	fmon->apinfra_cti_st = ioremap(0xd032138, 0x4);
	if (IS_ERR(fmon->apinfra_cti_st))
		dev_err(dev, "%s: failed to map apinfra_cti_st\n", __func__);

	/* raw/yuv */
	fmon->raw_a_tx = ioremap(0x3a7d0900, 0x4);
	if (IS_ERR(fmon->raw_a_tx))
		dev_err(dev, "%s: failed to map raw_a_tx\n", __func__);

	fmon->raw_b_tx = ioremap(0x3a9d0900, 0x4);
	if (IS_ERR(fmon->raw_b_tx))
		dev_err(dev, "%s: failed to map raw_b_tx\n", __func__);

	fmon->raw_c_tx = ioremap(0x3abd0900, 0x4);
	if (IS_ERR(fmon->raw_c_tx))
		dev_err(dev, "%s: failed to map raw_c_tx\n", __func__);

	fmon->yuv_a_tx = ioremap(0x3a810900, 0x4);
	if (IS_ERR(fmon->yuv_a_tx))
		dev_err(dev, "%s: failed to map yuv_a_tx\n", __func__);

	fmon->yuv_b_tx = ioremap(0x3aa10900, 0x4);
	if (IS_ERR(fmon->yuv_b_tx))
		dev_err(dev, "%s: failed to map yuv_b_tx\n", __func__);

	fmon->yuv_c_tx = ioremap(0x3ac10900, 0x4);
	if (IS_ERR(fmon->yuv_c_tx))
		dev_err(dev, "%s: failed to map yuv_c_tx\n", __func__);

	fmon->camsv_tx_0 = ioremap(0x3a680000 + 0x900, 0x4);
	if (IS_ERR(fmon->camsv_tx_0))
		dev_err(dev, "%s: failed to map camsv_tx_0\n", __func__);

	fmon->camsv_tx_1 = ioremap(0x3a680000 + 0x904, 0x4);
	if (IS_ERR(fmon->camsv_tx_1))
		dev_err(dev, "%s: failed to map camsv_tx_1\n", __func__);

	fmon->camsv_tx_2 = ioremap(0x3a680000 + 0x908, 0x4);
	if (IS_ERR(fmon->camsv_tx_2))
		dev_err(dev, "%s: failed to map camsv_tx_2\n", __func__);

	fmon->camsv_tx_3 = ioremap(0x3a680000 + 0x90C, 0x4);
	if (IS_ERR(fmon->camsv_tx_3))
		dev_err(dev, "%s: failed to map camsv_tx_3\n", __func__);

	fmon->irq = platform_get_irq_byname(pdev, "fmon");
	if (fmon->irq < 0)
		dev_err(dev, "%s: failed to get fmon irq\n", __func__);

	ret = devm_request_threaded_irq(dev, fmon->irq,
					mtk_irq_fmon,
					mtk_thread_irq_fmon,
					IRQF_NO_AUTOEN, dev_name(dev), cam);
	if (ret)
		dev_info(dev, "failed to request irq=%d\n", fmon->irq);

	dev_info(dev, "registered irq=%d\n", fmon->irq);

	mutex_init(&fmon->op_lock);

	memset(&fmon->pipes, 0, sizeof(fmon->pipes));

	fmon->fifo_size =
		roundup_pow_of_two(8 * sizeof(struct mtk_fmon_irq_info));

	fmon->msg_buffer = devm_kzalloc(dev, fmon->fifo_size, GFP_KERNEL);
	if (!fmon->msg_buffer)
		ret = -ENOMEM;

	return ret;
}
