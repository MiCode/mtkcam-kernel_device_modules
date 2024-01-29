// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2024 MediaTek Inc.

#include <linux/iopoll.h>

#include "mtk_cam.h"
#include "mtk_cam-raw.h"
#include "mtk_cam-qof.h"
#include "mtk_cam-qof_regs.h"
#include "mtk_cam-raw_regs.h"
#include "mtk_cam-reg_utils.h"
#include "mtk_cam-bit_mapping.h"
#include "mtk_cam-hsf.h"

/* QOF timer freq = (TM_FREQ / 2 / (QOF_TIMER_FREQ_DIV+1)) */
#define TM_FREQ_KHZ						208000
#define QOF_TIMER_FREQ_DIV				3

// TODO: tune this threshold
#define ON_OFF_TIME_US					300

// NOTE: PWR_OFF_MAX_THRESHOLD_US should be large enough to avoid
// overlay of on_proc and off_proc(generally 10us)
#define PWR_OFF_MAX_THRESHOLD_US			100

#define PWR_STATE_POLLING_TIMEOUT_SHORT_US		50
#define PWR_STATE_POLLING_TIMEOUT_LONG_US		600

//#define WORKAROUND_VOTER_SET_OUTSIDE_OFF_PROC

static u32 qof_readl(struct mtk_raw_device *raw,
					 void __iomem *base, u32 offset);
static u32 qof_readl_relaxed(struct mtk_raw_device *raw,
							 void __iomem *base, u32 offset);
static void qof_writel(struct mtk_raw_device *raw, u32 val,
					   void __iomem *base, u32 offset);
static void qof_writel_relaxed(struct mtk_raw_device *raw, u32 val,
							   void __iomem *base, u32 offset);

struct raw_io_ops qof_io_ops = {
	.readl = qof_readl,
	.readl_relaxed = qof_readl_relaxed,
	.writel = qof_writel,
	.writel_relaxed = qof_writel_relaxed,
};

enum QOF_POWER_STATE {
	PS_REST			 = 1 << 0,
	PS_ON			 = 1 << 1,
	PS_GCE_SAVE		 = 1 << 2,
	PS_OFF_PROC		 = 1 << 3,
	PS_OFF			 = 1 << 4,
	PS_ON_PROC		 = 1 << 5,
	PS_GCE_RESTORE	 = 1 << 6,
	PS_RESERVED		 = 1 << 7,
};

static inline u32 avoid_power_state(struct mtk_raw_device *raw, u32 state)
{
	u32 ret, val;

	if (state & (PS_GCE_SAVE | PS_ON_PROC | PS_GCE_RESTORE)) {
		// NOTE: PS_ON_PROC could take as long as 300us to finish
		// GCE SAVE/RESTORE depends on GCE operation
		ret = readx_poll_timeout(readl, raw->qof_base + REG_QOF_CAM_A_QOF_STATE_DBG_1,
								 val, !(val & state),
								 50 /*us*/, PWR_STATE_POLLING_TIMEOUT_LONG_US);
	} else {
		ret = readx_poll_timeout_atomic(readl, raw->qof_base + REG_QOF_CAM_A_QOF_STATE_DBG_1,
								 val, !(val & state),
								 15 /*us*/, PWR_STATE_POLLING_TIMEOUT_SHORT_US);
	}

	if (ret < 0)
		dev_info(raw->dev, "%s: error: timeout! (val: %u, state: %u)\n",
				 __func__, val, state);

	return ret;
}

// TODO: qof_reset and qof_enable(false) merge?
int qof_reset(struct mtk_raw_device *dev)
{
	struct mtk_cam_device *cam = dev->cam;
	u32 val;

	val = readl_relaxed(cam->qoftop_base + REG_QOF_CAM_TOP_QOF_TOP_CTL);

	switch (dev->id) {
	case RAW_A:
		SET_FIELD(&val, QOF_CAM_TOP_OFF_LOCK_1, 0);
		SET_FIELD(&val, QOF_CAM_TOP_ON_LOCK_1, 0);
		SET_FIELD(&val, QOF_CAM_TOP_OUT_LOCK_1, 0);
		SET_FIELD(&val, QOF_CAM_TOP_OTF_DC_MODE_1, 0);
		SET_FIELD(&val, QOF_CAM_TOP_DCIF_EXP_SOF_SEL_1, 0);
		break;
	case RAW_B:
		SET_FIELD(&val, QOF_CAM_TOP_OFF_LOCK_2, 0);
		SET_FIELD(&val, QOF_CAM_TOP_ON_LOCK_2, 0);
		SET_FIELD(&val, QOF_CAM_TOP_OUT_LOCK_2, 0);
		SET_FIELD(&val, QOF_CAM_TOP_OTF_DC_MODE_2, 0);
		SET_FIELD(&val, QOF_CAM_TOP_DCIF_EXP_SOF_SEL_2, 0);
		break;
	case RAW_C:
		SET_FIELD(&val, QOF_CAM_TOP_OFF_LOCK_3, 0);
		SET_FIELD(&val, QOF_CAM_TOP_ON_LOCK_3, 0);
		SET_FIELD(&val, QOF_CAM_TOP_OUT_LOCK_3, 0);
		SET_FIELD(&val, QOF_CAM_TOP_OTF_DC_MODE_3, 0);
		SET_FIELD(&val, QOF_CAM_TOP_DCIF_EXP_SOF_SEL_3, 0);
		break;
	default:
		pr_info("%s: raw id %d not found", __func__, dev->id);
		return -1;
	}

	writel(val, cam->qoftop_base + REG_QOF_CAM_TOP_QOF_TOP_CTL);

	writel_relaxed(0, dev->qof_base + REG_QOF_CAM_A_QOF_TIME_STAMP_1);
	writel_relaxed(0, dev->qof_base + REG_QOF_CAM_A_QOF_MTC_CYC_MAX_1);
	writel_relaxed(0, dev->qof_base + REG_QOF_CAM_A_QOF_PWR_OFF_MAX_1);

	val = readl_relaxed(dev->qof_base + REG_QOF_CAM_A_QOF_CTL_1);
	SET_FIELD(&val, QOF_CAM_A_ON_SEL_1, 0);
	SET_FIELD(&val, QOF_CAM_A_OFF_SEL_1, 0);
	SET_FIELD(&val, QOF_CAM_A_SW_RAW_OFF_1, 1);
	SET_FIELD(&val, QOF_CAM_A_SW_CQ_OFF_1, 1);
	writel(val, dev->qof_base + REG_QOF_CAM_A_QOF_CTL_1);

	qof_setup_ctrl(dev, 0);

	dev->io_ops = &basic_io_ops;

	return 0;
}

void qof_setup_ctrl(struct mtk_raw_device *raw, int on)
{
	u32 val;

	val = readl(raw->qof_base + REG_QOF_CAM_A_QOF_CTL_1);
	SET_FIELD(&val, QOF_CAM_A_QOF_CQ_EN_1, on);
	SET_FIELD(&val, QOF_CAM_A_HW_SEQ_EN_1, on);

	SET_FIELD(&val, QOF_CAM_A_CQ_HW_CLR_EN_1, on);
	SET_FIELD(&val, QOF_CAM_A_CQ_HW_SET_EN_1, on);
	SET_FIELD(&val, QOF_CAM_A_RAW_HW_CLR_EN_1, on);
	SET_FIELD(&val, QOF_CAM_A_RAW_HW_SET_EN_1, on);

	SET_FIELD(&val, QOF_CAM_A_RTC_EN_1, on);

	writel(val, raw->qof_base + REG_QOF_CAM_A_QOF_CTL_1);
}

void qof_sof_src_sel(struct mtk_raw_device *dev, bool with_dcif,
					 bool with_tg, int sv_last_tag)
{
	struct mtk_cam_device *cam = dev->cam;
	u32 otf_dc_mode = 0;
	u32 exp_sof_sel = 0;
	u32 val;

	if (with_tg) {
		if (with_dcif)
			otf_dc_mode = 1;
	} else {
		otf_dc_mode = 2;
		exp_sof_sel = sv_last_tag;
	}

	val = readl_relaxed(cam->qoftop_base + REG_QOF_CAM_TOP_QOF_TOP_CTL);

	switch (dev->id) {
	case RAW_A:
		SET_FIELD(&val, QOF_CAM_TOP_OTF_DC_MODE_1, otf_dc_mode);
		SET_FIELD(&val, QOF_CAM_TOP_DCIF_EXP_SOF_SEL_1, exp_sof_sel);
		break;
	case RAW_B:
		SET_FIELD(&val, QOF_CAM_TOP_OTF_DC_MODE_2, otf_dc_mode);
		SET_FIELD(&val, QOF_CAM_TOP_DCIF_EXP_SOF_SEL_2, exp_sof_sel);
		break;
	case RAW_C:
		SET_FIELD(&val, QOF_CAM_TOP_OTF_DC_MODE_3, otf_dc_mode);
		SET_FIELD(&val, QOF_CAM_TOP_DCIF_EXP_SOF_SEL_3, exp_sof_sel);
		break;
	default:
		pr_info("%s: raw id %d not found", __func__, dev->id);
		return;
	}

	writel(val, cam->qoftop_base + REG_QOF_CAM_TOP_QOF_TOP_CTL);

	if (CAM_DEBUG_ENABLED(QOF))
		pr_info("qof: %s: TOP_CTL 0x%08x", __func__, val);
}

void qof_setup_hw_timer(struct mtk_raw_device *dev, u32 interval_us)
{
	u32 timer_freq_khz =
		(TM_FREQ_KHZ / 2 / (QOF_TIMER_FREQ_DIV + 1));
	u32 mtcmos_cycle = (interval_us - ON_OFF_TIME_US) * timer_freq_khz / 1000;
	u32 pwr_off_max = (interval_us - ON_OFF_TIME_US - PWR_OFF_MAX_THRESHOLD_US)
		* timer_freq_khz / 1000;

	writel_relaxed(QOF_TIMER_FREQ_DIV,
				   dev->qof_base + REG_QOF_CAM_A_QOF_TIME_STAMP_1);
	writel_relaxed(mtcmos_cycle,
				   dev->qof_base + REG_QOF_CAM_A_QOF_MTC_CYC_MAX_1);
	writel_relaxed(pwr_off_max,
				   dev->qof_base + REG_QOF_CAM_A_QOF_PWR_OFF_MAX_1);
	// TODO: QOF_CAM_A_TIMEOUT_MAX_1

	qof_dump_hw_timer(dev);

	if (CAM_DEBUG_ENABLED(QOF)) {
		pr_info("qof: %s: freq(khz)/frm_tm(us)/mtcmos/max_pwr_off: %u/%u/%u/%u",
				__func__,
				timer_freq_khz, interval_us,
				mtcmos_cycle, pwr_off_max);
	}
}

int qof_enable(struct mtk_raw_device *raw, bool enable)
{
	int en = enable ? 1 : 0;
	struct mtk_cam_device *cam = raw->cam;
	u32 val;

	val = readl(cam->qoftop_base + REG_QOF_CAM_TOP_QOF_TOP_CTL);

	switch (raw->id) {
	case RAW_A:
		SET_FIELD(&val, QOF_CAM_TOP_QOF_SUBA_EN, en);
		SET_FIELD(&val, QOF_CAM_TOP_ITC_SRC_SEL_1, en);
		break;
	case RAW_B:
		SET_FIELD(&val, QOF_CAM_TOP_QOF_SUBB_EN, en);
		SET_FIELD(&val, QOF_CAM_TOP_ITC_SRC_SEL_2, en);
		break;
	case RAW_C:
		SET_FIELD(&val, QOF_CAM_TOP_QOF_SUBC_EN, en);
		SET_FIELD(&val, QOF_CAM_TOP_ITC_SRC_SEL_3, en);
		break;
	default:
		return -1;
	}

	qof_setup_ctrl(raw, en);
	raw->io_ops = (enable) ? &qof_io_ops : &basic_io_ops;

	SET_FIELD(&val, QOF_CAM_TOP_SEQUENCE_MODE, QOF_SEQ_MODE_RTC_THEN_ITC);
	writel(val, cam->qoftop_base + REG_QOF_CAM_TOP_QOF_TOP_CTL);
	writel(0xfff, cam->qoftop_base + REG_QOF_CAM_TOP_QOF_INT_EN);

	pr_info("%s: %s", __func__, (enable) ? "enable" : "disable");

	return 0;
}

// TODO: optimize
bool qof_is_enabled(struct mtk_raw_device *dev)
{
	bool enabled = false;
	struct mtk_cam_device *cam = dev->cam;
	u32 val;

	val = readl_relaxed(cam->qoftop_base + REG_QOF_CAM_TOP_QOF_TOP_CTL);

	switch (dev->id) {
	case RAW_A:
		enabled = !!(val & FBIT(QOF_CAM_TOP_QOF_SUBA_EN));
		break;
	case RAW_B:
		enabled = !!(val & FBIT(QOF_CAM_TOP_QOF_SUBB_EN));
		break;
	case RAW_C:
		enabled = !!(val & FBIT(QOF_CAM_TOP_QOF_SUBC_EN));
		break;
	default:
		break;
	}

	return enabled;
}

int qof_setup_twin(struct mtk_raw_device *raw, bool is_master)
{
	int ret = 0;
	u32 val = 0;
	u32 voter_sel = (is_master) ? 0 : 1; //0: from raw; 1: from master

#ifdef QOF_CCU_READY
	ret = mtk_cam_hsf_qof_config(raw, is_master, is_master, !is_master);
#endif

	if (ret) {
		pr_info("ERROR: fail to setup QOF lock");
		return ret;
	}

	val = readl(raw->qof_base + REG_QOF_CAM_A_QOF_CTL_1);
	SET_FIELD(&val, QOF_CAM_A_ON_SEL_1, voter_sel);
	SET_FIELD(&val, QOF_CAM_A_OFF_SEL_1, voter_sel);
	writel(val, raw->qof_base + REG_QOF_CAM_A_QOF_CTL_1);

	if (CAM_DEBUG_ENABLED(QOF))
		pr_info("qof: %s: qof_ctrl val 0x%x", __func__, val);

	return ret;
}

void qof_set_cq_start_max(struct mtk_raw_device *dev, u32 start_max)
{
	writel(start_max, dev->qof_base + REG_QOF_CAM_A_QOF_CQ_START_MAX_1);
}

static int qof_mtcmos_raw_voter(struct mtk_raw_device *raw, bool enable)
{
	u32 qof_ctrl_write = 0;
	u32 pwr_state_wait = 0;
	u32 state_dbg = 0;
	u32 val = 0;
	int ret = 0;

	mutex_lock(&raw->apmcu_voter_lock);

	if (enable)
		raw->apmcu_voter_cnt++;
	else
		raw->apmcu_voter_cnt--;

	if (CAM_DEBUG_ENABLED(QOF))
		pr_info("[%s] voter cnt is %d", __func__, raw->apmcu_voter_cnt);

	if (raw->apmcu_voter_cnt == 1) {
		qof_ctrl_write = FBIT(QOF_CAM_A_APMCU_SET_1);
		SET_FIELD(&pwr_state_wait, QOF_CAM_A_POWER_STATE_1, PS_ON);
	} else if (raw->apmcu_voter_cnt == 0) {
		qof_ctrl_write = FBIT(QOF_CAM_A_APMCU_CLR_1);
	} else if (raw->apmcu_voter_cnt < 0) {
		pr_info("[%s] ERROR: voter cnt negative %d", __func__,
				raw->apmcu_voter_cnt);
		goto UNLOCK;
	} else
		goto UNLOCK;

	val = readl(raw->qof_base + REG_QOF_CAM_A_QOF_CTL_1);

#ifdef WORKAROUND_VOTER_SET_OUTSIDE_OFF_PROC
	if (qof_ctrl_write | FBIT(QOF_CAM_A_APMCU_SET_1)) {
		// NOTE: workaround for HW issue
		avoid_power_state(raw, PS_OFF_PROC);
	}
#endif

	writel(val | qof_ctrl_write, raw->qof_base + REG_QOF_CAM_A_QOF_CTL_1);

	if (pwr_state_wait) {
		ret = readx_poll_timeout(readl, raw->qof_base + REG_QOF_CAM_A_QOF_STATE_DBG_1,
					 state_dbg, state_dbg & pwr_state_wait,
					 50 /* delay, us */, PWR_STATE_POLLING_TIMEOUT_LONG_US);

		if (ret < 0)
			pr_info("[%s] ERROR: pwr_state polling timeout", __func__);
	}

UNLOCK:
	mutex_unlock(&raw->apmcu_voter_lock);
	return ret;
}

// TODO: synchronization
// will be run on threaded irq/done workqueue/flow workqueue
int qof_mtcmos_voter(struct mtk_cam_ctx *ctx, bool enable)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_engines *eng = &cam->engines;
	int raw_num = eng->num_raw_devices;
	unsigned long submask;
	struct mtk_raw_device *raw;
	int i;

	submask = bit_map_subset_of(MAP_HW_RAW, ctx->used_engine);
	for (i = 0; i < raw_num && submask; i++, submask >>= 1) {
		if (!(submask & 0x1))
			continue;
		raw = dev_get_drvdata(eng->raw_devs[i]);

		if (qof_is_enabled(raw))
			qof_mtcmos_raw_voter(raw, enable);
	}

	return 0;
}

static int qof_reset_mtcmos_raw_voter(struct mtk_raw_device *dev)
{
	int ret = 0;
	u32 val;

	mutex_lock(&dev->apmcu_voter_lock);

	if (dev->apmcu_voter_cnt > 1)
		pr_info("WARNING: QOF apmcu voter is %d (>1)",
				dev->apmcu_voter_cnt);

	val = readl(dev->qof_base + REG_QOF_CAM_A_QOF_CTL_1);
	writel(val | FBIT(QOF_CAM_A_APMCU_CLR_1),
		   dev->qof_base + REG_QOF_CAM_A_QOF_CTL_1);

	dev->apmcu_voter_cnt = 0;

	mutex_unlock(&dev->apmcu_voter_lock);
	return ret;
}

int qof_reset_mtcmos_voter(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_engines *eng = &cam->engines;
	int raw_num = eng->num_raw_devices;
	unsigned long submask;
	struct mtk_raw_device *raw;
	int i;

	submask = bit_map_subset_of(MAP_HW_RAW, ctx->used_engine);
	for (i = 0; i < raw_num && submask; i++, submask >>= 1) {
		if (!(submask & 0x1))
			continue;

		raw = dev_get_drvdata(eng->raw_devs[i]);
		qof_reset_mtcmos_raw_voter(raw);
	}

	return 0;
}

static inline int read_replace_addr(const struct mtk_raw_device *raw,
										void __iomem **base, u32 *offset)
{
	u32 _off = *offset;

	void *_base;
	u32 _offset;

	// TODO: use define instead of debug opt?
	if (CAM_DEBUG_ENABLED(QOF_ADDR)) {
		_base = *base;
		_offset = *offset;
	}

#define CHANGE_TO(b) {*offset = b; break;}
	if (*base == raw->base) {
		switch (_off) {
		// interrupt
		case REG_CAMCTL_INT_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT1_STATUS_1)
		case REG_CAMCTL_INT2_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT2_STATUS_1)
		case REG_CAMCTL_INT3_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT3_STATUS_1)
		//case REG_CAMCTL_INT4_STATUS:
			//CHANGE_TO(REG_QOF_CAM_A_INT4_STATUS_1)
		case REG_CAMCTL_INT5_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT5_STATUS_1)
		case REG_CAMCTL_INT6_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT6_STATUS_1)
		case REG_CAMCTL_INT7_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT7_STATUS_1)
		case REG_CAMCTL_INT8_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT8_STATUS_1)
		case REG_CAMCTL_INT17_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT9_STATUS_1)
		case REG_CAMCTL_INT18_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT10_STATUS_1)
		case REG_CAMCTL_INT19_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT11_STATUS_1)
		case REG_CAMCTL_INT20_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT12_STATUS_1)
		case REG_CAMCTL_INT21_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT13_STATUS_1)
		case REG_CAMCTL_INT25_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT14_STATUS_1)
		case REG_CAMCTL_INT26_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT15_STATUS_1)
		case REG_CAMCTL_TFMR_INT_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT16_STATUS_1)
		case REG_CAMCTL_TFMR_INT2_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT17_STATUS_1)
		case REG_CAMCTL_TFMR_INT3_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT18_STATUS_1)
		//case REG_CAMCTL_TFMR_INT4_STATUS:
			//CHANGE_TO(REG_QOF_CAM_A_INT19_STATUS_1)
		case REG_CAMCTL_TFMR_INT5_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT20_STATUS_1)
		case REG_CAMCTL_TFMR_INT6_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT21_STATUS_1)
		case REG_CAMCTL_TFMR_INT7_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT22_STATUS_1)
		case REG_CAMCTL_TFMR_INT8_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT23_STATUS_1)
		//CQ addr
		case REG_CAMCQ_CQ_THR0_BASEADDR:
			CHANGE_TO(REG_QOF_CAM_A_QOF_CQ1_DATA_1)
		case REG_CAMCQ_CQ_THR0_BASEADDR_MSB:
			CHANGE_TO(REG_QOF_CAM_A_QOF_CQ2_DATA_1)
		case REG_CAMCQ_CQ_THR0_DESC_SIZE:
			CHANGE_TO(REG_QOF_CAM_A_QOF_CQ3_DATA_1)
		case REG_CAMCQ_CQ_SUB_THR0_BASEADDR_2:
			CHANGE_TO(REG_QOF_CAM_A_QOF_CQ4_DATA_1)
		case REG_CAMCQ_CQ_SUB_THR0_BASEADDR_2_MSB:
			CHANGE_TO(REG_QOF_CAM_A_QOF_CQ5_DATA_1)
		case REG_CAMCQ_CQ_SUB_THR0_DESC_SIZE_2:
			CHANGE_TO(REG_QOF_CAM_A_QOF_CQ6_DATA_1)
		default:
			return 0;
		}
	} else if (*base == raw->yuv_base) {
		switch (_off) {
		case REG_CAMCTL2_INT_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT24_STATUS_1)
		case REG_CAMCTL2_INT2_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT25_STATUS_1)
		//case REG_CAMCTL2_INT3_STATUS:
			//CHANGE_TO(REG_QOF_CAM_A_INT26_STATUS_1)
		case REG_CAMCTL2_INT5_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT27_STATUS_1)
		case REG_CAMCTL2_INT8_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT28_STATUS_1)
		case REG_CAMCTL2_INT17_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT29_STATUS_1)
		case REG_CAMCTL2_INT25_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT30_STATUS_1)
		case REG_CAMCTL2_TFMR_INT_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT31_STATUS_1)
		case REG_CAMCTL2_TFMR_INT2_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT32_STATUS_1)
		//case REG_CAMCTL2_INT18_STATUS:
			//CHANGE_TO(REG_QOF_CAM_A_INT33_STATUS_1)
		case REG_CAMCTL2_TFMR_INT5_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT34_STATUS_1)
		case REG_CAMCTL2_TFMR_INT8_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT35_STATUS_1)
		default:
			return 0;
		}
	} else if (*base == raw->rms_base) {
		switch (_off) {
		case REG_CAMCTL3_INT_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT36_STATUS_1)
		case REG_CAMCTL3_INT8_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT37_STATUS_1)
		case REG_CAMCTL3_TFMR_INT_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT38_STATUS_1)
		case REG_CAMCTL3_TFMR_INT8_STATUS:
			CHANGE_TO(REG_QOF_CAM_A_INT39_STATUS_1)
		default:
			return 0;
		}
	} else
		return 0;
#undef CHANGE_TO

	*base = raw->qof_base;

	if (CAM_DEBUG_ENABLED(QOF_ADDR))
		pr_info("%s: {%p, 0x%08x} -> {%p, 0x%08x}",
				__func__,
				_base, _offset,
				*base, *offset);

	return 1;
}

static inline int read_replace_rtc_addr(const struct mtk_raw_device *raw,
										void __iomem **base, u32 *offset)
{
	u32 _off = *offset;

	void *_base;
	u32 _offset;

	if (CAM_DEBUG_ENABLED(QOF_ADDR)) {
		_base = *base;
		_offset = *offset;
	}

#define CHANGE_TO(to) {*offset = to; break;}
	if (*base == raw->base) {
		// TODO: check REG_TG_INTER_ST update behavior
		switch (_off) {
		case REG_FHG_FHG_SPARE_1:
			CHANGE_TO(REG_QOF_CAM_A_TRANS1_DATA_1)
		case REG_TG_INTER_ST:
			CHANGE_TO(REG_QOF_CAM_A_TRANS3_DATA_1)
		default:
			return 0;
		}
	} else if (*base == raw->base_inner) {
		switch (_off) {
		case REG_FHG_FHG_SPARE_1:
			CHANGE_TO(REG_QOF_CAM_A_TRANS2_DATA_1)
		case REG_CAMCTL_MOD5_EN:
			CHANGE_TO(REG_QOF_CAM_A_TRANS4_DATA_1)
		default:
			return 0;
		}
	} else
		return 0;
#undef CHANGE_TO

	*base = raw->qof_base;

	if (CAM_DEBUG_ENABLED(QOF_ADDR))
		pr_info("%s: {%p, 0x%08x} -> {%p, 0x%08x}",
				__func__,
				_base, _offset,
				*base, *offset);

	return 1;
}

void qof_setup_rtc(struct mtk_raw_device *dev)
{
	struct mtk_cam_device *cam = dev->cam;

	u32 base = (u32)((u64)dev->base_reg_addr - (u64)cam->base_reg_addr);
	u32 base_inner = (u32)((u64)dev->base_inner_reg_addr - (u64)cam->base_reg_addr);

	dev_info(dev->dev,
			 "qof: %s: base_reg_addr 0x%x base_inner_reg_addr 0x%x",
			 __func__, base, base_inner);

	writel_relaxed(base + REG_FHG_FHG_SPARE_1,
				   dev->qof_base + REG_QOF_CAM_A_TRANS1_ADDR_1);
	writel_relaxed(base_inner + REG_FHG_FHG_SPARE_1,
				   dev->qof_base + REG_QOF_CAM_A_TRANS2_ADDR_1);
	writel_relaxed(base + REG_TG_INTER_ST,
				   dev->qof_base + REG_QOF_CAM_A_TRANS3_ADDR_1);
	writel_relaxed(base_inner + REG_CAMCTL_MOD5_EN,
				   dev->qof_base + REG_QOF_CAM_A_TRANS4_ADDR_1);
}

static inline int write_replace_cq_baseaddr(const struct mtk_raw_device *raw,
										void __iomem **base, u32 *offset)
{
	u32 _off = *offset;

	void *_base;
	u32 _offset;

	if (CAM_DEBUG_ENABLED(QOF_ADDR)) {
		_base = *base;
		_offset = *offset;
	}

#define CHANGE_TO(to) {*offset = to; break;}

	if (*base == raw->base) {
		switch (_off) {
		case REG_CAMCQ_CQ_THR0_BASEADDR:
			CHANGE_TO(REG_QOF_CAM_A_QOF_CQ1_DATA_1)
		case REG_CAMCQ_CQ_THR0_BASEADDR_MSB:
			CHANGE_TO(REG_QOF_CAM_A_QOF_CQ2_DATA_1)
		case REG_CAMCQ_CQ_THR0_DESC_SIZE:
			CHANGE_TO(REG_QOF_CAM_A_QOF_CQ3_DATA_1)
		case REG_CAMCQ_CQ_SUB_THR0_BASEADDR_2:
			CHANGE_TO(REG_QOF_CAM_A_QOF_CQ4_DATA_1)
		case REG_CAMCQ_CQ_SUB_THR0_BASEADDR_2_MSB:
			CHANGE_TO(REG_QOF_CAM_A_QOF_CQ5_DATA_1)
		case REG_CAMCQ_CQ_SUB_THR0_DESC_SIZE_2:
			CHANGE_TO(REG_QOF_CAM_A_QOF_CQ6_DATA_1)
		default:
			return 0;
		}
	} else
		return 0;

#undef CHANGE_TO

	*base = raw->qof_base;

	if (CAM_DEBUG_ENABLED(QOF_ADDR))
		pr_info("%s: {%p, 0x%08x} -> {%p, 0x%08x}",
				__func__,
				_base, _offset,
				*base, *offset);

	return 1;
}

static inline int write_replace_trigger_cq(const struct mtk_raw_device *raw,
										   void __iomem **base, u32 *offset,
										   u32 *val)
{
	if (*base == raw->base && *offset == REG_CAMCTL_START) {
		void *_base;
		u32 _offset;

		if (CAM_DEBUG_ENABLED(QOF_ADDR)) {
			_base = *base;
			_offset = *offset;
		}

		// TODO: "QOF_CTRL" reg synchonization
		*base = raw->qof_base;
		*offset = REG_QOF_CAM_A_QOF_CTL_1;

		*val = readl_relaxed(raw->qof_base + REG_QOF_CAM_A_QOF_CTL_1);
		SET_FIELD(val, QOF_CAM_A_CQ_START_1, 1);

		if (CAM_DEBUG_ENABLED(QOF_ADDR))
			pr_info("%s: {%p, 0x%08x} -> {%p, 0x%08x}",
					__func__,
					_base, _offset,
					*base, *offset);

		return 1;
	}

	return 0;
}

static u32 qof_readl(struct mtk_raw_device *raw,
					 void __iomem *base, u32 offset)
{
	int ret =
		read_replace_addr(raw, &base, &offset) ||
		read_replace_rtc_addr(raw, &base, &offset);

	(void)ret;

	if (CAM_DEBUG_ENABLED(QOF_ADDR))
		pr_info("%s: {%p, 0x%08x}", __func__, base, offset);

	return readl(base + offset);
}

static u32 qof_readl_relaxed(struct mtk_raw_device *raw,
							 void __iomem *base, u32 offset)
{
	int ret =
		read_replace_addr(raw, &base, &offset) ||
		read_replace_rtc_addr(raw, &base, &offset);

	(void)ret;

	if (CAM_DEBUG_ENABLED(QOF_ADDR))
		pr_info("%s: {%p, 0x%08x}", __func__, base, offset);

	return readl_relaxed(base + offset);
}

static void qof_writel(struct mtk_raw_device *raw, u32 val,
					   void __iomem *base, u32 offset)
{
	int ret = write_replace_cq_baseaddr(raw, &base, &offset);
	int ret_trigger_cq = 0;

	if (!ret)
		ret_trigger_cq = write_replace_trigger_cq(raw, &base, &offset, &val);

#ifdef WORKAROUND_VOTER_SET_OUTSIDE_OFF_PROC
	if (ret_trigger_cq) {
		// NOTE: workaround for HW issue
		avoid_power_state(raw, PS_OFF_PROC);
	}
#endif

	writel(val, base + offset);
}

static void qof_writel_relaxed(struct mtk_raw_device *raw, u32 val,
							   void __iomem *base, u32 offset)
{
	int ret =
		write_replace_cq_baseaddr(raw, &base, &offset) ||
		write_replace_trigger_cq(raw, &base, &offset, &val);
	(void)ret;

	writel_relaxed(val, base + offset);
}

void qof_dump_trigger_cnt(struct mtk_raw_device *raw)
{
	if (CAM_DEBUG_ENABLED(QOF)) {
		unsigned int qof_trig_cnt =
			readl_relaxed(raw->qof_base + REG_QOF_CAM_A_QOF_TRIG_CNT_1);

		pr_info("qof: on_cnf: %d off_cnt: %d",
				qof_trig_cnt & 0xFF, (qof_trig_cnt >> 16) & 0xFF);
	}
}

void qof_dump_voter(struct mtk_raw_device *raw)
{
	if (CAM_DEBUG_ENABLED(QOF)) {
		unsigned int voter_dbg =
			readl_relaxed(raw->qof_base + REG_QOF_CAM_A_QOF_VOTER_DBG_1);
		unsigned int voter_history =
			READ_FIELD(voter_dbg, QOF_CAM_A_VOTE_CHECK_1);

		pr_info("qof: voter overall: 0x%x (on=0x2, off=0x1)",
			READ_FIELD(voter_dbg, QOF_CAM_A_TRG_STATUS_1));
		pr_info("qof: cur status: 0x%x (CQ/RAW/SCP/APMCU=0x8/0x4/0x2/0x1)",
			READ_FIELD(voter_dbg, QOF_CAM_A_VOTE_1));
		pr_info("qof: history: 0x%x 0x%x 0x%x 0x%x",
			voter_history & 0xf,
			voter_history >> 4 & 0xf,
			voter_history >> 8 & 0xf,
			voter_history >> 12 & 0xf);
	}
}

void qof_dump_power_state(struct mtk_raw_device *raw)
{
	if (CAM_DEBUG_ENABLED(QOF)) {
		unsigned int state_dbg =
			readl_relaxed(raw->qof_base + REG_QOF_CAM_A_QOF_STATE_DBG_1);
		unsigned int mtcmos_state_raw_lsb =
			readl_relaxed(raw->qof_base + REG_QOF_CAM_A_QOF_MTC_ST_RAW_LSB_1);
		unsigned int mtcmos_state_raw_msb =
			readl_relaxed(raw->qof_base + REG_QOF_CAM_A_QOF_MTC_ST_RAW_MSB2_1);

		pr_info("qof: state dbg: 0x%08x", state_dbg);
		pr_info("qof: mtcmos status msb/lsb: 0x%08x %08x",
			mtcmos_state_raw_msb, mtcmos_state_raw_lsb);
	}
}

void qof_dump_hw_timer(struct mtk_raw_device *raw)
{
	if (CAM_DEBUG_ENABLED(QOF)) {
		pr_info("qof: %s: TIME_STAMP_1 %u MTC_CYC_MAX %u PWR_OFF_MAX %u",
				__func__,
				readl_relaxed(raw->qof_base + REG_QOF_CAM_A_QOF_TIME_STAMP_1),
				readl_relaxed(raw->qof_base + REG_QOF_CAM_A_QOF_MTC_CYC_MAX_1),
				readl_relaxed(raw->qof_base + REG_QOF_CAM_A_QOF_PWR_OFF_MAX_1));
		pr_info("qof: %s: VSYNC_PRD_1 %u",
				__func__,
				readl_relaxed(raw->qof_base + REG_QOF_CAM_A_QOF_VSYNC_PRD_1));
	}
}

