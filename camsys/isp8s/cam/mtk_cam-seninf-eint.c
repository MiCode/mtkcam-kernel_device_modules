// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/spinlock.h>
#include <linux/printk.h>
#include <linux/atomic.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/device.h>

#include "mtk_cam-seninf-eint.h"
#include "mtk_cam-seninf-event-handle.h"
#include "mtk_cam-seninf-eint-cb-def.h"
#include "mtk_cam-seninf.h"
#include "imgsensor-user.h"

unsigned int eint_log_ctrl;

#define PFX "SenIfEINT"
#define EINT_INF(format, args...) \
	pr_info(PFX "[%s] " format, __func__, ##args)
#define EINT_DBG(format, args...) \
do { \
	if (unlikely(eint_log_ctrl)) { \
		pr_info(PFX "[%s] " format, __func__, ##args); \
	} \
} while (0)

#define sd_to_ctx(__sd) container_of(__sd, struct seninf_ctx, subdev)

#define EINT_KZALLOC(dev, size)       (devm_kzalloc(dev, (size), GFP_ATOMIC))
#define EINT_KCALLOC(dev, n, size)    (devm_kcalloc(dev, (n), (size), GFP_ATOMIC))
#define EINT_KFREE(dev, p)            (devm_kfree(dev, (p)))
#define EINT_DEVM_KFREE(p)


#define EINT_TICK_FACTOR  (1000)
#define EINT_IRQ_FIFO_LEN (10)

#define TIMER_LATCH_CTRL   (0x0C20)
#define TIMER_LATCH_L(n)   (0x0C24 + n * 8)
#define TIMER_LATCH_H(n)   (0x0C28 + n * 8)

#define EINT_NUM_MAX         (3)
#define EINT_IDX_INVALID     (0x87)
#define EINT_DTS_NUM_INVALID (0x87 + 1) /* dts idx start from 1 (using 1,2,3) */

#define EINT_CON_CMD_ID_BASE        (100000000)
#define EINT_CON_CMD_ID_MOD         (100)
#define EINT_CON_CMD_VAL_BASE       (1)
#define EINT_CON_CMD_VAL_MOD        (100000000)

DEFINE_SPINLOCK(eint_cfg_concurrency_lock);

enum eint_console_cmd_id {
	EINT_CON_FORCE_TO_EN_USING_XVS = 1,
	EINT_CON_FORCE_TO_EN_USING_MASKFRAME = 2,

	/* last cmd id (42) */
	EINT_CON_CMD_LOG_CTRL = 42,
};

struct reg_address_size_pair {
	u32 address;
	u32 size;
};

struct eint_dts_info_pair {
	unsigned int id; /* from dts, the same as gpio_num */
	unsigned int inst; /* index for using which reg base addr */
	unsigned int hw_latch_code; /* from dts, for HW latch config reg */
};

struct eint_work_request {
	struct kthread_work work;
	void *dev_ctx;
	unsigned int tsrec_no;
	struct mtk_cam_seninf_eint_timestamp_info eint_timestamp_info;
};

struct eint_event_st {
	void *msg_buffer;
	unsigned int fifo_size;
	atomic_t is_fifo_overflow;
	struct kfifo msg_fifo;
};

struct eint_irq_info {
	unsigned int index;
	int seq_no;
	int is_streaming;
	unsigned long long tick;
	unsigned long long sys_ts;
	unsigned long long sys_ts_mono;
};

struct eint_n_timestamp_st {
	unsigned int curr_idx;
	unsigned long long ticks[EINT_TS_REC_MAX_CNT];
};

struct eint_dts_info_st {
	unsigned int eint_no; /* Max to 3, options:{1, 2, 3} */
	unsigned int gpio_num; /* from dts */
	unsigned int reg_address; /* from dts */
	unsigned int reg_size; /* from dts */
	unsigned int id; /* from dts, the same as gpio_num */
	unsigned int hw_latch_code; /* from dts, for HW latch config reg */
	void __iomem *base;
};

struct eint_status_st {
	struct eint_n_timestamp_st eint_n_timestamp_st;
	unsigned int is_start;
	unsigned int intr_en;
	atomic_t seq_no;
	atomic_t is_streaming;
	atomic_t is_seamless_switch;
};

struct mtk_seninf_eint_pin {
	struct seninf_ctx *inf_ctx;
	unsigned int tsrec_idx;
	struct kthread_worker *worker;
	unsigned int irq_num;

	struct eint_event_st event_st;
	struct eint_dts_info_st dts_info;
	struct eint_status_st status;

	struct mutex eint_intr_en_lock;
};

struct eint_irq_cb_ctx {
	struct mutex op_lock;

	struct mtk_cam_seninf_eint_irq_cb_info info[SENINF_EINT_IRQ_CB_UID_MAX];
};

struct mtk_seninf_eint {
	struct device *seninf_dev;
	struct seninf_core *core;
	struct reg_address_size_pair *address_size_pairs;
	struct eint_dts_info_pair *eint_dts_info_pairs;
	int address_size_pairs_len;
	int eint_dts_info_pairs_len;
	u32 systimer_left_shift_bits;
	struct mtk_seninf_eint_pin eints[EINT_NUM_MAX];
	struct eint_irq_cb_ctx irq_cb_ctx[EINT_NUM_MAX];
};
static struct mtk_seninf_eint seninf_eint;



/******************************************************************************
 * EINT static/utils functions
 *****************************************************************************/
static void find_seninf_ctx_by_tsrec_no(const unsigned int tsrec_no,
	struct seninf_ctx **seninf_ctx)
{
	int i;

	for(i = 0; i < EINT_NUM_MAX; i++) {
		if(seninf_eint.eints[i].tsrec_idx == tsrec_no) {
			*seninf_ctx = seninf_eint.eints[i].inf_ctx;
			return;
		}
	}
}

static int chk_eint_idx_valid(const unsigned int idx, const char *caller)
{
	if (idx == EINT_IDX_INVALID) {
		EINT_INF(
			"[%s] eint_idx:%u(INVAL:%u(%#x)) => seems NOT has EINT, skip\n",
			caller, idx, EINT_IDX_INVALID, EINT_IDX_INVALID);
		return -EINVAL;
	}
	if (idx >= EINT_NUM_MAX) {
		EINT_INF(
			"[%s] invalid eint_idx:%u(MAX:%u) => skip\n",
			caller, idx, EINT_NUM_MAX);
		return -EINVAL;
	}

	return 0;
}

static unsigned int get_eint_idx_by_irq(int irq)
{
	unsigned int i;

	for(i = 0; i < EINT_NUM_MAX; i++) {
		if(seninf_eint.eints[i].irq_num == irq)
			return i;
	}
	return -1;
}

static inline unsigned int eint_check_info_msgfifo_not_empty(unsigned int index)
{
	return (kfifo_len(&seninf_eint.eints[index].event_st.msg_fifo)
		>= sizeof(struct eint_irq_info));
}

static inline void eint_pop_irq_info_msgfifo(struct eint_irq_info *irq_info, unsigned int index)
{
	unsigned int len;

	len = kfifo_out(
		&seninf_eint.eints[index].event_st.msg_fifo, irq_info, sizeof(struct eint_irq_info));
}

static void push_irq_inifo_into_msgfifo(unsigned int index,
	unsigned long long tick,
	unsigned long long sys_ts,
	unsigned long long sys_ts_mono,
	int seq_no,
	int is_streaming)
{
	int len;
	struct eint_irq_info irq_info;

	irq_info.sys_ts = sys_ts;
	irq_info.sys_ts_mono = sys_ts_mono;
	irq_info.tick = tick;
	irq_info.index = index;
	irq_info.seq_no = seq_no;
	irq_info.is_streaming = is_streaming;

	if (unlikely(kfifo_avail(
			&seninf_eint.eints[index].event_st.msg_fifo) < sizeof(struct eint_irq_info))) {
		atomic_set(&seninf_eint.eints[index].event_st.is_fifo_overflow, 1);
		return;
	}

	len = kfifo_in(&seninf_eint.eints[index].event_st.msg_fifo,
		&irq_info, sizeof(struct eint_irq_info));
}

static inline void reset_eint_info(unsigned int index, u32 en)
{
	if (en) {
		struct kthread_worker *worker = NULL;

		worker = mtk_cam_seninf_tsrec_g_kthread(
			seninf_eint.eints[index].tsrec_idx, __func__);

		if (worker)
			seninf_eint.eints[index].worker = worker;
		else
			EINT_INF("ERROR: get tsrec kthread failed   [index:%u]\n", index);
	}

	seninf_eint.eints[index].status.is_start = en;
	memset(&(seninf_eint.eints[index].status.eint_n_timestamp_st),
		0, sizeof(struct eint_n_timestamp_st));
}

static void config_hw_latch(unsigned int index, u32 en)
{
	/* config HW to latch which specified GPIO */
	unsigned int idx = index;
	u32 val, before, after, code, mask;
	void __iomem *base = seninf_eint.eints[index].dts_info.base;

	if (base == NULL) {
		EINT_INF("ERROR: base is null   [index:%u]\n", index);
		return;
	}

	code = en ? seninf_eint.eints[index].dts_info.hw_latch_code : 0;
	mask = 0xff << (idx * 8);


	spin_lock(&eint_cfg_concurrency_lock);

	before = readl(base + TIMER_LATCH_CTRL);
	val = (before & ~mask) | (code << (idx * 8));
	writel(val, base + TIMER_LATCH_CTRL);
	after = readl(base + TIMER_LATCH_CTRL);

	spin_unlock(&eint_cfg_concurrency_lock);

	EINT_INF(
		"index:%u en:%u (before=0x%X after=0x%X val=0x%X(mask=0x%X code=0x%X))\n",
		index, en, before, after, val, mask, code);
}

static u64 read_eint_reg_tick(unsigned int index)
{
	int i;
	u64 cnt_h, cnt_l, tmp_h, val;
	u32 systimer_left_shift_bits = seninf_eint.systimer_left_shift_bits;
	void __iomem *base =
		seninf_eint.eints[index].dts_info.base;

	for(i = 0; i < 100; i++) {
		cnt_h = readl(base + TIMER_LATCH_H(index) );
		cnt_l = readl(base + TIMER_LATCH_L(index) );
		tmp_h = readl(base + TIMER_LATCH_H(index) );
		if (cnt_h == tmp_h)
			break;
	}
	val = ((cnt_h << 32) & 0xFFFFFFFF00000000) | (cnt_l & 0xFFFFFFFF);
	val = val << systimer_left_shift_bits;

	return val;
}

static void update_ticks_data(unsigned int index, struct eint_irq_info *irq_info)
{
	u64 tick = irq_info->tick;
	unsigned int curr_idx =
		seninf_eint.eints[index].status.eint_n_timestamp_st.curr_idx;

	seninf_eint.eints[index].status.eint_n_timestamp_st.ticks[curr_idx] = tick;

	curr_idx = (curr_idx + 1) % EINT_TS_REC_MAX_CNT;
	seninf_eint.eints[index].status.eint_n_timestamp_st.curr_idx = curr_idx;
}

static void setup_eint_timestamp_info_st(unsigned int index, void *data,
	struct eint_irq_info *irq_info,
	struct mtk_cam_seninf_eint_timestamp_info *eint_timestamp_info)
{
	int i;
	unsigned int curr_idx, tmp_idx;

	eint_timestamp_info->eint_no = index;
	eint_timestamp_info->tsrec_idx = seninf_eint.eints[index].tsrec_idx;
	eint_timestamp_info->tick = irq_info->tick;
	eint_timestamp_info->tick_factor = EINT_TICK_FACTOR;
	eint_timestamp_info->irq_seq_no = irq_info->seq_no;
	eint_timestamp_info->irq_sys_time_ns = irq_info->sys_ts;
	eint_timestamp_info->irq_mono_time_ns = irq_info->sys_ts_mono;

	curr_idx =
		seninf_eint.eints[index].status.eint_n_timestamp_st.curr_idx;

	for(i = 0; i < EINT_TS_REC_MAX_CNT; i++) {
		tmp_idx = (curr_idx + i) % EINT_TS_REC_MAX_CNT;
		/* re-order */
		eint_timestamp_info->ts_us[EINT_TS_REC_MAX_CNT - 1 - i] =
			seninf_eint.eints[index].status.eint_n_timestamp_st.ticks[tmp_idx]
			/ EINT_TICK_FACTOR;
	}
}



/******************************************************************************
 * EINT worke fgunctions (using TSREC worker case)
 *****************************************************************************/
static void eint_work_executor(
	const unsigned int tsrec_no, struct eint_work_request *req)
{
	struct seninf_ctx *seninf_ctx = NULL;
	struct mtk_cam_seninf_eint_timestamp_info *info;

	info = &req->eint_timestamp_info;

	find_seninf_ctx_by_tsrec_no(tsrec_no, &seninf_ctx);

	if (unlikely(seninf_ctx == NULL)) {
		EINT_INF(
			"ERROR: unlikely(seninf_ctx == NULL)\n");
		return;
	}

	if (unlikely(!(seninf_ctx->sensor_sd
			&& seninf_ctx->sensor_sd->ops
			&& seninf_ctx->sensor_sd->ops->core
			&& seninf_ctx->sensor_sd->ops->core->command))) {
		EINT_INF(
			"ERROR: v4l2_subdev_core_ops command function not found\n");
		return;
	}

	/* call v4l2_subdev_core_ops command to sensor adaptor */
	seninf_ctx->sensor_sd->ops->core->command(
		seninf_ctx->sensor_sd, V4L2_CMD_EINT_NOTIFY_VSYNC, &(req->eint_timestamp_info));

	EINT_DBG(
		"eint_no:%u tsrec_idx:%u seq_no:%d ts:%lluus(%llu/%d) sys_ts:(%llu|%llu) [%llu %llu %llu %llu] act_fl:%lluus",
		info->eint_no,
		info->tsrec_idx,
		info->irq_seq_no,
		info->tick / info->tick_factor,
		info->tick,
		info->tick_factor,
		info->irq_sys_time_ns,
		info->irq_mono_time_ns,
		info->ts_us[0],
		info->ts_us[1],
		info->ts_us[2],
		info->ts_us[3],
		info->ts_us[0] - info->ts_us[1]);
}

static inline void eint_work_free(struct eint_work_request *req)
{
	if (unlikely(req == NULL)) {
		EINT_INF(
			"ERROR: work request:%p dynamic alloc mem failed, return\n",
			req);
		return;
	}

	devm_kfree(seninf_eint.seninf_dev, req);
}

static void eint_work_handler(struct kthread_work *work)
{
	struct eint_work_request *req = NULL;

	/* error handle (unexpected case) */
	if (unlikely(work == NULL))
		return;
	/* convert/cast work_struct */
	req = container_of(work, struct eint_work_request, work);
	if (unlikely(req == NULL)) {
		EINT_INF(
			"ERROR: container_of() casting failed, return\n");
		return;
	}

	if (unlikely(req->dev_ctx == NULL)) {
		EINT_INF(
			"ERROR: irq_dev_ctx:%p is nullptr, req:(tsrec_no:%u)\n",
			req->dev_ctx,
			req->tsrec_no);

		goto eint_work_handler_end;
	}

	eint_work_executor(req->tsrec_no, req);

eint_work_handler_end:
	eint_work_free(req);
}

static void eint_work_init_and_queue(
	unsigned int index,
	struct eint_work_request *req)
{
	struct kthread_worker *p_worker = NULL;

	kthread_init_work(&req->work, eint_work_handler);

	p_worker = seninf_eint.eints[index].worker;
	if (p_worker == NULL) {
		EINT_INF("ERROR: g_tsrec_worker_st failed   [index:%u]\n", index);
		return;
	}

	kthread_queue_work(
		p_worker, &req->work);
}

static inline void eint_work_request_info_setup(void *data,
	const struct mtk_cam_seninf_eint_timestamp_info *timestamp_info,
	struct eint_work_request *req)
{
	/* copy input data/info */
	req->dev_ctx = data;
	req->tsrec_no = timestamp_info->tsrec_idx;
	memcpy(&req->eint_timestamp_info, timestamp_info, sizeof(*timestamp_info));
}

void eint_work_setup(int irq, void *data, struct eint_irq_info *irq_info)
{
	struct eint_work_request *req;
	struct mtk_cam_seninf_eint_timestamp_info eint_timestamp_info;
	int index = irq_info->index;

	if (chk_eint_idx_valid(index, __func__))
		return;

	req = devm_kzalloc(seninf_eint.seninf_dev,
		sizeof(struct eint_work_request),
		GFP_ATOMIC);

	if (unlikely(req == NULL)) {
		EINT_INF(
			"ERROR: work request:%p dynamic alloc mem failed, return   [index:%d]\n",
			req,
			index);
		return;
	}

	setup_eint_timestamp_info_st(
		index, data, irq_info, &eint_timestamp_info);

	eint_work_request_info_setup(
		data, &eint_timestamp_info, req);

	eint_work_init_and_queue(index, req);
}


/******************************************************************************
 * EINT IRQ cb --- define / enum / struct / function / etc.
 *****************************************************************************/
static int chk_eint_irq_cb_user_id_valid(
	const enum mtk_cam_seninf_eint_irq_cb_uid id, const char *caller)
{
	if (unlikely(id >= SENINF_EINT_IRQ_CB_UID_MAX)) {
		EINT_INF("WARNING: cb_user_id invalid, id:%d\n", id);
		return 1;
	}
	return 0;
}

static void init_eint_irq_cb_ctx(struct eint_irq_cb_ctx *irq_cb_ctx)
{
	struct mtk_cam_seninf_eint_irq_cb_info *p_cb_info;
	unsigned int i;

	mutex_init(&irq_cb_ctx->op_lock);

	for (i = 0; i < SENINF_EINT_IRQ_CB_UID_MAX; ++i) {
		p_cb_info = &irq_cb_ctx->info[i];
		memset(p_cb_info, 0, sizeof(*p_cb_info));
	}
}

static void eint_irq_cb_info_clear(struct eint_irq_cb_ctx *irq_cb_ctx,
	const enum mtk_cam_seninf_eint_irq_cb_uid id)
{
	struct mtk_cam_seninf_eint_irq_cb_info *p_cb_info;

	mutex_lock(&irq_cb_ctx->op_lock);

	p_cb_info = &irq_cb_ctx->info[id];
	memset(p_cb_info, 0, sizeof(*p_cb_info));

	mutex_unlock(&irq_cb_ctx->op_lock);
}

static void eint_irq_cb_info_update(struct eint_irq_cb_ctx *irq_cb_ctx,
	const enum mtk_cam_seninf_eint_irq_cb_uid id,
	const struct mtk_cam_seninf_eint_irq_cb_info *p_cb_info)
{
	mutex_lock(&irq_cb_ctx->op_lock);

	irq_cb_ctx->info[id] = *p_cb_info;

	mutex_unlock(&irq_cb_ctx->op_lock);
}

static void eint_irq_cb_func_all_execute(struct eint_irq_cb_ctx *irq_cb_ctx,
	const struct mtk_cam_seninf_eint_irq_notify_info *p_irq_info)
{
	struct mtk_cam_seninf_eint_irq_cb_info *p_cb_info;
	unsigned int i;

	mutex_lock(&irq_cb_ctx->op_lock);

	for (i = SENINF_EINT_IRQ_CB_UID_MIN; i < SENINF_EINT_IRQ_CB_UID_MAX; ++i) {
		p_cb_info = &irq_cb_ctx->info[i];

		if (p_cb_info->func_ptr != NULL)
			p_cb_info->func_ptr(p_irq_info, p_cb_info->p_data);
	}

	mutex_unlock(&irq_cb_ctx->op_lock);
}

int mtk_cam_seninf_eint_register_irq_cb(struct v4l2_subdev *sd,
	const enum mtk_cam_seninf_eint_irq_cb_uid id,
	const struct mtk_cam_seninf_eint_irq_cb_info *p_cb_info)
{
	struct seninf_ctx *ctx = sd_to_ctx(sd);
	unsigned int index = ctx->eint_idx;

	/* error case */
	if (unlikely(chk_eint_idx_valid(index, __func__)))
		return SENINF_EINT_CB_ERR_NOT_CONNECT_TO_EINT;
	if (unlikely(chk_eint_irq_cb_user_id_valid(id, __func__)))
		return SENINF_EINT_CB_ERR_IRQ_CB_UID_INVALID;

	eint_irq_cb_info_update(&seninf_eint.irq_cb_ctx[index], id, p_cb_info);

	EINT_DBG(
		"eint_idx:%d, UID:%u, p_cb_info:%p(fptr:%p/data:%p)\n",
		index, id,
		p_cb_info, p_cb_info->func_ptr, p_cb_info->p_data);

	return SENINF_EINT_CB_ERR_NONE;
}

int mtk_cam_seninf_eint_unregister_irq_cb(struct v4l2_subdev *sd,
	const enum mtk_cam_seninf_eint_irq_cb_uid id)
{
	struct seninf_ctx *ctx = sd_to_ctx(sd);
	unsigned int index = ctx->eint_idx;

	/* error case */
	if (unlikely(chk_eint_idx_valid(index, __func__)))
		return SENINF_EINT_CB_ERR_NOT_CONNECT_TO_EINT;
	if (unlikely(chk_eint_irq_cb_user_id_valid(id, __func__)))
		return SENINF_EINT_CB_ERR_IRQ_CB_UID_INVALID;

	eint_irq_cb_info_clear(&seninf_eint.irq_cb_ctx[index], id);

	EINT_DBG(
		"eint_idx:%d, UID:%u\n",
		index, id);

	return SENINF_EINT_CB_ERR_NONE;
}


/******************************************************************************
 * EINT irq functions
 *****************************************************************************/
static int chk_ticks_val_not_zero(struct eint_irq_info *irq_info)
{
	if (irq_info->tick == 0) {
		EINT_INF("NOTICE: ticks value is zero! idx:%u tick:%llu sys_ts:(%llu|%llu)\n",
			irq_info->index,
			irq_info->tick,
			irq_info->sys_ts,
			irq_info->sys_ts_mono);
		return -EINVAL;
	}

	return 0;
}

static int chk_xvs_for_preshutter(unsigned int index, struct eint_irq_info *irq_info)
{
	/* specail case, there are one more xvs before 1SOF */
	int i, flag = 0;
	int seq_no = atomic_read(&seninf_eint.eints[index].status.seq_no);
	int is_seamless_switch =
		atomic_read(&seninf_eint.eints[index].status.is_seamless_switch);

	for(i = 0; i < EINT_TS_REC_MAX_CNT; i++) {
		if (seninf_eint.eints[index].status.eint_n_timestamp_st.ticks[i]) {
			flag = 1;
			break;
		}
	}

	if ((seq_no == 1) && (flag == 0)) {
		EINT_INF(
			"NOTICE: filter 1st XVS (preshutter) before 1 SOF idx:%u ts:%lluus(%llu/%u) seq_no:%d sys_ts:(%llu|%llu)   [index:%u]\n",
			irq_info->index,
			irq_info->tick / EINT_TICK_FACTOR,
			irq_info->tick,
			EINT_TICK_FACTOR,
			seq_no,
			irq_info->sys_ts,
			irq_info->sys_ts_mono,
			index);
		return -EINVAL;
	}


	if (is_seamless_switch) {
		atomic_set(&seninf_eint.eints[index].status.is_seamless_switch, 0);

		EINT_INF(
			"NOTICE: filter 1st XVS (preshutter) for seamless switch idx:%u ts:%lluus(%llu/%u) seq_no:%d sys_ts:(%llu|%llu)   [index:%u]\n",
			irq_info->index,
			irq_info->tick / EINT_TICK_FACTOR,
			irq_info->tick,
			EINT_TICK_FACTOR,
			seq_no,
			irq_info->sys_ts,
			irq_info->sys_ts_mono,
			index);
		return -EINVAL;
	}

	return 0;
}

static int chk_eint_intr_en(unsigned int index, struct eint_irq_info *irq_info)
{
	if (seninf_eint.eints[index].status.intr_en == 0) {
		EINT_INF("WARNING: EINT is not enabled! idx:%u ts:%lluus(%llu/%u) sys_ts:(%llu|%llu)   [index:%u]\n",
			irq_info->index,
			irq_info->tick / EINT_TICK_FACTOR,
			irq_info->tick,
			EINT_TICK_FACTOR,
			irq_info->sys_ts,
			irq_info->sys_ts_mono,
			index);
		return -EINVAL;
	}

	return 0;
}

static int chk_imgsensor_is_streaming(struct eint_irq_info *irq_info)
{
	if (irq_info->is_streaming == 0) {
		EINT_INF(
			"WARNING: get EINT before imgsensor start streaming, idx:%u ts:%lluus(%llu/%u) sys_ts:(%llu|%llu)\n",
			irq_info->index,
			irq_info->tick / EINT_TICK_FACTOR,
			irq_info->tick,
			EINT_TICK_FACTOR,
			irq_info->sys_ts,
			irq_info->sys_ts_mono);
		return -EINVAL;
	}

	return 0;
}

static int chk_eint_irq_info_status(unsigned int index,
	struct eint_irq_info *irq_info)
{
	if (chk_ticks_val_not_zero(irq_info))
		return -EINVAL;

	if(chk_imgsensor_is_streaming(irq_info))
		return -EINVAL;

	if (chk_xvs_for_preshutter(index, irq_info))
		return -EINVAL;

	if (chk_eint_intr_en(index, irq_info))
		return -EINVAL;

	return 0;
}

static inline void update_seq_no(unsigned int index)
{
	atomic_inc(&seninf_eint.eints[index].status.seq_no);
}

static void eint_notify_irq_to_others(struct eint_irq_info *irq_info)
{
	struct mtk_cam_seninf_eint_irq_notify_info info;
	unsigned long long start, end;
	const unsigned long long time_th = 250000; /* 250 us */

	info.inf_ctx = seninf_eint.eints[irq_info->index].inf_ctx;
	info.index = irq_info->index;
	info.sys_ts_ns = irq_info->sys_ts;
	info.mono_ts_ns = irq_info->sys_ts_mono;

	start = ktime_get_boottime_ns();

	/* broadcast info for seninf */
	eint_irq_cb_func_all_execute(
		&seninf_eint.irq_cb_ctx[irq_info->index],
		&info);

	end = ktime_get_boottime_ns();

	if (unlikely((end - start) >= time_th)) {
		EINT_INF(
			"WARNING: seninf took to much time in doing mtk_cam_seninf_eint_irq_notify function, %llu >= th:%llu (%llu ~ %llu)(us)\n",
			(end - start)/1000, time_th/1000,
			start/1000, end/1000);
	}
}

static void eint_irq_handler(int irq, void *data,
	struct eint_irq_info *irq_info)
{
	unsigned int index = irq_info->index;

	if (chk_eint_idx_valid(index, __func__))
		return;

	/* fileter specail case*/
	if (chk_eint_irq_info_status(index, irq_info))
		return;



	update_ticks_data(index, irq_info);

	eint_work_setup(irq, data, irq_info);

	eint_notify_irq_to_others(irq_info);
}

static irqreturn_t mtk_eint_irq(int irq, void *data)
{
	unsigned long long start, start_mono, tick;
	int seq_no, is_streaming;
	unsigned int index = get_eint_idx_by_irq(irq);

	if (chk_eint_idx_valid(index, __func__))
		return IRQ_HANDLED;

	tick = read_eint_reg_tick(index);
	start = ktime_get_boottime_ns();
	start_mono = ktime_get_ns();
	is_streaming =
		atomic_read(&seninf_eint.eints[index].status.is_streaming);

	if (!((tick == 0) || (is_streaming == 0)))
		atomic_inc(&seninf_eint.eints[index].status.seq_no);

	seq_no = atomic_read(&seninf_eint.eints[index].status.seq_no);

	push_irq_inifo_into_msgfifo(index,
		tick, start, start_mono, seq_no, is_streaming);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t mtk_thread_eint_irq(int irq, void *data)
{
	unsigned int index = get_eint_idx_by_irq(irq);
	struct eint_irq_info irq_info;

	if (chk_eint_idx_valid(index, __func__))
		return IRQ_HANDLED;

	/* error handling (check case and print information) */
	if (unlikely(atomic_cmpxchg(&seninf_eint.eints[index].event_st.is_fifo_overflow, 1, 0)))
		EINT_INF("WARNING: irq msg fifo overflow\n");

	while (eint_check_info_msgfifo_not_empty(index)) {
		eint_pop_irq_info_msgfifo(&irq_info, index);

		/* inform interrupt information */
		eint_irq_handler(irq, data, &irq_info);
	}

	return IRQ_HANDLED;
}


/******************************************************************************
 * EINT call back handler entry
 *****************************************************************************/
static int eint_cb_notify_streamon(const unsigned int eint_no,
	void *arg, const char *caller)
{
	int *is_streaming = NULL;

	if (chk_eint_idx_valid(eint_no, __func__))
		return -EINVAL;

	is_streaming = (int *)arg;
	atomic_set(&seninf_eint.eints[eint_no].status.is_streaming, *is_streaming);
	EINT_INF("index:%u is_streaming:%u caller:%s\n",eint_no, *is_streaming, caller);
	return 0;
}

static int eint_cb_notify_seamless_switch(const unsigned int eint_no,
	void *arg, const char *caller)
{
	int *is_seamless_switch = NULL;

	if (chk_eint_idx_valid(eint_no, __func__))
		return -EINVAL;

	is_seamless_switch = (int *)arg;
	atomic_set(&seninf_eint.eints[eint_no].status.is_seamless_switch, *is_seamless_switch);
	EINT_INF("index:%u is_seamless_switch:%u intr_en:%u caller:%s\n",
		eint_no,
		*is_seamless_switch,
		seninf_eint.eints[eint_no].status.intr_en,
		caller);

	return 0;
}

struct eint_cb_cmd_entry {
	unsigned int cmd;
	int (*func)(const unsigned int eint_no, void *arg, const char *caller);
};

static const struct eint_cb_cmd_entry eint_cb_cmd_list[] = {
	/* user get eint information */
	{EINT_CB_CMD_NOTIFY_STREAMON, eint_cb_notify_streamon},
	{EINT_CB_CMD_NOTIFY_SEAMLESS_SWITCH, eint_cb_notify_seamless_switch},
};

int eint_cb_handler(const unsigned int eint_no,
	const unsigned int cmd, void *arg, const char *caller)
{
	int i, is_cmd_found = 0, ret = 0;

	/* !!! dispatch eint cb cmd request !!! */
	for (i = 0; i < ARRAY_SIZE(eint_cb_cmd_list); ++i) {
		if (eint_cb_cmd_list[i].cmd == cmd) {
			is_cmd_found = 1;
			ret = eint_cb_cmd_list[i].func(
				eint_no, arg, caller);
			break;
		}
	}
	if (unlikely(is_cmd_found == 0)) {
		ret = -1;
		EINT_INF("WARNING: EINT callback cmd not found, cmd:%u\n", cmd);
	}

	return ret;
}

static void eint_setup_cb_func_info_of_sensor(struct seninf_ctx *inf_ctx,
	const unsigned int is_start)
{
	unsigned int index = inf_ctx->eint_idx;
	struct mtk_cam_seninf_eint_cb_info cb_info = {0};

	if (chk_eint_idx_valid(index, __func__))
		return;

	/* case check */
	if (unlikely(inf_ctx == NULL))
		return;
	if (unlikely(!(inf_ctx->sensor_sd
			&& inf_ctx->sensor_sd->ops
			&& inf_ctx->sensor_sd->ops->core
			&& inf_ctx->sensor_sd->ops->core->command))) {
		EINT_INF("[%s] cannot get sensor core ops   [index:%u]\n", __func__, index);
		return;
	}

	cb_info.tsrec_idx = seninf_eint.eints[index].tsrec_idx;
	cb_info.eint_no = seninf_eint.eints[index].dts_info.eint_no;
	cb_info.is_start = is_start;
	cb_info.eint_cb_handler = &eint_cb_handler;

	/* call v4l2_subdev_core_ops command to sensor adaptor */
	inf_ctx->sensor_sd->ops->core->command(
		inf_ctx->sensor_sd,
		V4L2_CMD_EINT_SETUP_CB_FUNC_OF_SENSOR,
		&cb_info);

	EINT_INF("tsrec_idx:%u eint_no:%d is_start:%u eint_cb_handler:%p   [index:%u]\n",
		cb_info.tsrec_idx,
		cb_info.eint_no,
		cb_info.is_start,
		cb_info.eint_cb_handler,
		index);
}


/******************************************************************************
 * EINT APIs --- seninf cid
 *****************************************************************************/
static void eint_notify_adaptor_enable_irq(unsigned int index, u32 en)
{
	struct seninf_ctx *seninf_ctx = NULL;
	struct mtk_cam_seninf_eint_irq_en_info info;

	if (chk_eint_idx_valid(index, __func__))
		return;

	seninf_ctx = seninf_eint.eints[index].inf_ctx;
	if (unlikely(seninf_ctx == NULL)) {
		EINT_INF("[%s] seninf_ctx == NULL   [index:%u]\n", __func__, index);
		return;
	}

	if (unlikely(!(seninf_ctx->sensor_sd
			&& seninf_ctx->sensor_sd->ops
			&& seninf_ctx->sensor_sd->ops->core
			&& seninf_ctx->sensor_sd->ops->core->command))) {
		EINT_INF("ERROR: can not get sensor ops or core ops   [index:%u]\n", index);
		return;
	}

	info.eint_no = seninf_eint.eints[index].dts_info.eint_no;
	info.tsrec_idx = seninf_eint.eints[index].tsrec_idx;
	info.flag = en;

	/* call v4l2_subdev_core_ops command to sensor adaptor */
	seninf_ctx->sensor_sd->ops->core->command(
		seninf_ctx->sensor_sd, V4L2_CMD_EINT_NOTIFY_IRQ_EN, &info);

	EINT_INF("eint_no:%u tsrec_idx:%u flag:%u   [index:%u]\n",
		info.eint_no,
		info.tsrec_idx,
		info.flag,
		index);
}


static void eint_irq_line_en(unsigned int index, u32 en)
{
	unsigned int irq;
	int seq_no;

	if (chk_eint_idx_valid(index, __func__))
		return;

	irq = seninf_eint.eints[index].irq_num;

	if(!irq) {
		EINT_INF("ERROR: irq == 0   [index:%u]\n", index);
		return;
	}

	mutex_lock(&seninf_eint.eints[index].eint_intr_en_lock);

	seq_no = atomic_read(&seninf_eint.eints[index].status.seq_no);

	if (en) {
		if (!seninf_eint.eints[index].status.intr_en) {
			enable_irq(irq);
			seninf_eint.eints[index].status.intr_en = 1;
		}
	} else {
		if (seninf_eint.eints[index].status.intr_en) {
			disable_irq(irq);
			seninf_eint.eints[index].status.intr_en = 0;
		}
	}

	atomic_set(&seninf_eint.eints[index].status.seq_no, 0);

	mutex_unlock(&seninf_eint.eints[index].eint_intr_en_lock);

	EINT_INF(
		"index:%u en:%u, seq_no:(before:%d->after:0), irq_no:%u",
		index, en, seq_no, irq);
}

static void eint_irq_ctrl(unsigned int index, u32 en)
{
	unsigned int irq;

	if (chk_eint_idx_valid(index, __func__))
		return;

	irq = seninf_eint.eints[index].irq_num;
	if(!irq) {
		EINT_INF("ERROR: irq == 0   [index:%u]\n", index);
		return;
	}

	if (en) {
		eint_notify_adaptor_enable_irq(index, 1);
		config_hw_latch(index, 1);
		eint_irq_line_en(index, 1);
	} else {
		eint_irq_line_en(index, 0);
		config_hw_latch(index, 0);
		eint_notify_adaptor_enable_irq(index, 0);
	}
}


int mtk_cam_seninf_eint_irq_en(struct seninf_ctx *ctx, u32 en)
{
	unsigned int index = ctx->eint_idx;

	if (chk_eint_idx_valid(index, __func__))
		return -EINVAL;

	eint_irq_ctrl(index, en);

	return 0;
}


/******************************************************************************
 * EINT APIs --- start/reset @ seninf streamon/off
 *****************************************************************************/
int mtk_cam_seninf_eint_start(struct seninf_ctx *ctx)
{
	unsigned int index = ctx->eint_idx;

	if (chk_eint_idx_valid(index, __func__))
		return 0;

	reset_eint_info(index, 1);

	eint_setup_cb_func_info_of_sensor(ctx, 1);

	return 0;
}

int mtk_cam_seninf_eint_reset(struct seninf_ctx *ctx)
{
	unsigned int index = ctx->eint_idx;

	if (chk_eint_idx_valid(index, __func__))
		return 0;

	eint_irq_ctrl(index, 0); /* force disable */

	eint_setup_cb_func_info_of_sensor(ctx, 0);

	reset_eint_info(index, 0);

	return 0;
}


/*---------------------------------------------------------------------------*/
// eint debugging functions
/*---------------------------------------------------------------------------*/
static void eint_notify_sensor_force_using_eint(const unsigned int index, const unsigned int en)
{
	struct seninf_ctx *seninf_ctx = NULL;
	struct mtk_fsync_hw_mcss_init_info info;

	if (chk_eint_idx_valid(index, __func__))
		return;

	seninf_ctx = seninf_eint.eints[index].inf_ctx;
	if (unlikely(seninf_ctx == NULL)) {
		EINT_INF("[%s] seninf_ctx == NULL   [index:%u]\n", __func__, index);
		return;
	}

	if (unlikely(!(seninf_ctx->sensor_sd
			&& seninf_ctx->sensor_sd->ops
			&& seninf_ctx->sensor_sd->ops->core
			&& seninf_ctx->sensor_sd->ops->core->command))) {
		EINT_INF("ERROR: can not get sensor ops or core ops   [index:%u]\n", index);
		return;
	}

	info.enable_mcss = en ? 1 : 0;
	info.is_mcss_master = en ? 1 : 0;

	/* call v4l2_subdev_core_ops command to sensor adaptor */
	seninf_ctx->sensor_sd->ops->core->command(
		seninf_ctx->sensor_sd, V4L2_CMD_EINT_NOTIFY_FORCE_EN_XVS, &info);

	EINT_INF("index:%u en:%u info.enable_mcss:%u info.is_mcss_master:%u\n",
		index,
		en,
		info.enable_mcss,
		info.is_mcss_master);
}

static void eint_notify_sensor_force_using_maskframe(const unsigned int index, const unsigned int num)
{
	struct seninf_ctx *seninf_ctx = NULL;
	struct mtk_fsync_hw_mcss_mask_frm_info info;

	if (chk_eint_idx_valid(index, __func__))
		return;

	seninf_ctx = seninf_eint.eints[index].inf_ctx;
	if (unlikely(seninf_ctx == NULL)) {
		EINT_INF("[%s] seninf_ctx == NULL   [index:%u]\n", __func__, index);
		return;
	}

	if (unlikely(!(seninf_ctx->sensor_sd
			&& seninf_ctx->sensor_sd->ops
			&& seninf_ctx->sensor_sd->ops->core
			&& seninf_ctx->sensor_sd->ops->core->command))) {
		EINT_INF("ERROR: can not get sensor ops or core ops   [index:%u]\n", index);
		return;
	}

	info.mask_frm_num = num;
	info.is_critical = 1;

	/* call v4l2_subdev_core_ops command to sensor adaptor */
	seninf_ctx->sensor_sd->ops->core->command(
		seninf_ctx->sensor_sd, V4L2_CMD_EINT_NOTIFY_FORCE_MASKFRAME, &info);

	EINT_INF("index:%u num:%u info.mask_frm_num:%u info.is_critical:%u\n",
		index,
		num,
		info.mask_frm_num,
		info.is_critical);
}

static void eint_force_en_irq_and_notify_sensor(const unsigned int index, const unsigned int en)
{
	if (chk_eint_idx_valid(index, __func__))
		return;

	eint_notify_sensor_force_using_eint(index, en);
	eint_irq_ctrl(index, en);
}

static void eint_trigger_force_en(const unsigned int val)
{
	int i;
	unsigned int en;

	for(i=0; i<EINT_NUM_MAX; i++) {
		en = (((1UL << i) & val) != 0);
		eint_force_en_irq_and_notify_sensor(i, en);
	}
}

static void eint_trigger_force_en_maskframe(const unsigned int val)
{
	int i;
	unsigned int num;

	for(i=0; i<EINT_NUM_MAX; i++) {
		num = (((1UL << i) & val) != 0);
		eint_notify_sensor_force_using_maskframe(i, num);
	}
}

/*---------------------------------------------------------------------------*/
// eint console framework functions
/*---------------------------------------------------------------------------*/
static inline unsigned int eint_con_mgr_decode_cmd_value(const unsigned int cmd,
	const unsigned int base, const unsigned int mod)
{
	unsigned int ret = 0;

	ret = (base != 0) ? (cmd / base) : 0;
	ret %= mod;

	return ret;
}

static inline enum eint_console_cmd_id eint_con_mgr_g_cmd_id(
	const unsigned int cmd)
{
	return (enum eint_console_cmd_id)eint_con_mgr_decode_cmd_value(cmd,
		EINT_CON_CMD_ID_BASE, EINT_CON_CMD_ID_MOD);
}

static inline void eint_con_mgr_s_cmd_value(const unsigned int cmd,
	unsigned int *p_val)
{
	*p_val = eint_con_mgr_decode_cmd_value(cmd,
		EINT_CON_CMD_VAL_BASE, EINT_CON_CMD_VAL_MOD);
}

static void eint_con_mgr_process_cmd(const unsigned int cmd)
{
	enum eint_console_cmd_id cmd_id = 0;

	cmd_id = eint_con_mgr_g_cmd_id(cmd);
	switch (cmd_id) {
	case EINT_CON_FORCE_TO_EN_USING_XVS:
		{
			unsigned int eint_force_en_ctrl;

			eint_con_mgr_s_cmd_value(cmd, &eint_force_en_ctrl);
			eint_trigger_force_en(eint_force_en_ctrl);
		}
		break;

	case EINT_CON_FORCE_TO_EN_USING_MASKFRAME:
		{
			unsigned int eint_maskframe_ctrl;

			eint_con_mgr_s_cmd_value(cmd, &eint_maskframe_ctrl);
			eint_trigger_force_en_maskframe(eint_maskframe_ctrl);
		}
		break;

	case EINT_CON_CMD_LOG_CTRL:
		eint_con_mgr_s_cmd_value(cmd, &eint_log_ctrl);
		break;
	default:
		break;
	}
}

static ssize_t seninf_eint_console_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int len = 0;

	return len;
}

static ssize_t seninf_eint_console_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int cmd = 0;
	int ret = 0;

	/* convert string to unsigned int */
	ret = kstrtouint(buf, 0, &cmd);
	if (ret != 0) {
		/*
		 *	SHOW(str_buf, len,
		 *		"\n\t[fsync_console]: kstrtoint failed, input:%s, cmd:%u, ret:%d\n",
		 *		buf,
		 *		cmd,
		 *		ret);
		 */
	}

	eint_con_mgr_process_cmd(cmd);

	return count;
}


static DEVICE_ATTR_RW(seninf_eint_console);
/*---------------------------------------------------------------------------*/
// eint sysfs framwwork functions
/*---------------------------------------------------------------------------*/
static void mtk_cam_seninf_eint_create_sysfs_file(struct device *dev)
{
	int ret = 0;

	/* case handling (unexpected case) */
	if (unlikely(dev == NULL)) {
		EINT_INF(
			"ERROR: failed to create sysfs file, dev is NULL, return\n");
		return;
	}

	/* !!! create each sysfs file you want !!! */
	ret = device_create_file(dev, &dev_attr_seninf_eint_console);
	if (ret) {
		EINT_INF(
			"ERROR: call device_create_file() failed, ret:%d\n",
			ret);
	}
}


static void mtk_cam_seninf_eint_remove_sysfs_file(struct device *dev)
{
	/* case handling (unexpected case) */
	if (unlikely(dev == NULL)) {
		EINT_INF(
			"ERROR: *dev is NULL, abort process device_remove_file, return\n");
		return;
	}

	/* !!! remove each sysfs file you created !!! */
	device_remove_file(dev, &dev_attr_seninf_eint_console);
}

/******************************************************************************
 * EINT core init/uninit @ seninf_core_probe/seninf_core_remove
 *****************************************************************************/
static int get_eint_info(struct device *dev)
{
	phandle handle;
	int matrix_number, i, ret;
	u32 systimer_left_shift_bits = 0;
	unsigned int total_pin_number, instance_number;
	struct device_node *eint_np, *node = NULL;

	node = of_find_compatible_node(dev->of_node, NULL, "mediatek,seninf-eint");
	if (unlikely(node == NULL)) {
		EINT_INF(
			"ERROR: can't find DTS compatiable node:'%s'\n",
			"mediatek,seninf-eint");
		return -1;
	}

	ret = of_property_read_u32(node,
		"systimer-leftshift-bits", &systimer_left_shift_bits);
	if (unlikely(ret < 0))
		EINT_INF(
			"ERROR: Failed to read lshift_bits property, ret:%d\n", ret);
	else {
		seninf_eint.systimer_left_shift_bits = systimer_left_shift_bits;
		EINT_INF(
			"read systimer_left_shift_bits property successlly, lshift_bits=%u ret:%d\n",
			seninf_eint.systimer_left_shift_bits, ret);
	}

	ret = of_property_read_u32(node, "mediatek,seninf-eint", &handle);
	if (unlikely(ret < 0)) {
		EINT_INF(
			"ERROR: can't find property:'%s', ret:%d\n",
			"mediatek,seninf-eint", ret);
	}

	eint_np = of_find_node_by_phandle(handle);
	if (unlikely(eint_np == NULL))
		EINT_INF("ERROR: can't find rproc node by phandle\n");


	ret = of_property_read_u32(eint_np, "mediatek,total-pin-number",
				   &total_pin_number);
	if (ret) {
		EINT_INF("cannot read total-pin-number from device node.\n");
		of_node_put(node);
		return -EINVAL;
	}

	ret = of_property_read_u32(eint_np, "mediatek,instance-num",
				   &instance_number);
	if (ret)
		instance_number = 1; /* only 1 instance in legacy chip */

	seninf_eint.address_size_pairs_len = instance_number;
	seninf_eint.address_size_pairs = devm_kzalloc(dev,
		instance_number * sizeof(struct reg_address_size_pair),
		GFP_KERNEL);

	if (!seninf_eint.address_size_pairs) {
		EINT_INF("ERROR: devm_kzalloc fail\n");
		of_node_put(node);
		return -ENOMEM;
	}


	for (i = 0; i < instance_number; i++) {
		const u32 *reg; /* __be32 */
		int len;

		reg = of_get_property(eint_np, "reg", &len);
		if (!reg) {
			EINT_INF("ERROR: Property 'reg' not found\n");
			continue;
		}

		seninf_eint.address_size_pairs[i].address =
			be32_to_cpu(reg[i * sizeof(__be32) + 1]);
		seninf_eint.address_size_pairs[i].size =
			be32_to_cpu(reg[i * sizeof(__be32) + 3]);
	}


	matrix_number = of_property_count_u32_elems(eint_np, "mediatek,pins") / 4;
	if (matrix_number < 0)
		matrix_number = total_pin_number;

	seninf_eint.eint_dts_info_pairs_len = matrix_number;
	seninf_eint.eint_dts_info_pairs = devm_kzalloc(dev,
		matrix_number * sizeof(struct eint_dts_info_pair),
		GFP_KERNEL);

	for (i = 0; i < matrix_number; i++) {
		unsigned int id, inst, idx, support_deb, offset;

		offset = i * 4;
		ret = of_property_read_u32_index(eint_np, "mediatek,pins",
					   offset, &id);
		ret |= of_property_read_u32_index(eint_np, "mediatek,pins",
					   offset+1, &inst);
		ret |= of_property_read_u32_index(eint_np, "mediatek,pins",
					   offset+2, &idx);
		ret |= of_property_read_u32_index(eint_np, "mediatek,pins",
					   offset+3, &support_deb);

		/* Legacy chip which no need to give coordinate list */
		if (ret) {
			id = i;
			inst = 0;
			idx = i;
			support_deb = (i < 32) ? 1 : 0;
		}

		seninf_eint.eint_dts_info_pairs[i].id = id;
		seninf_eint.eint_dts_info_pairs[i].inst = inst;
		seninf_eint.eint_dts_info_pairs[i].hw_latch_code = idx;
	}

	if (node)
		of_node_put(node);

	return 0;
}

int mtk_cam_seninf_eint_core_init(struct device *dev, struct seninf_core *core)
{
	seninf_eint.seninf_dev = dev;
	seninf_eint.core = core;

	mtk_cam_seninf_eint_create_sysfs_file(seninf_eint.seninf_dev);
	get_eint_info(dev);

	return 0;
}

int mtk_cam_seninf_eint_core_uninit(void)
{
	mtk_cam_seninf_eint_remove_sysfs_file(seninf_eint.seninf_dev);
	seninf_eint.seninf_dev = NULL;
	seninf_eint.core = NULL;
	seninf_eint.address_size_pairs = NULL;
	seninf_eint.eint_dts_info_pairs = NULL;
	seninf_eint.address_size_pairs_len = 0;
	seninf_eint.eint_dts_info_pairs_len = 0;

	return 0;
}


/******************************************************************************
 * Functions for EINT init/uninit @ seninf_probe/seninf_remove
 *****************************************************************************/
static int get_eint_num(struct device *dev, unsigned int *eint_no)
{
	int ret = 0;

	ret = of_property_read_u32(dev->of_node, "eint-no", eint_no);
	if (ret) {
		*eint_no = EINT_DTS_NUM_INVALID;
		EINT_INF(
			"NOTICE: cannot read eint-no, ret:%d, assigned EINT_DTS_NUM_INVALID:%u\n",
			ret, *eint_no);
	}

	return ret;
}

static int get_gpio_num(struct device *dev, u32 *gpio_num)
{
	int ret = 0;

	ret = of_property_read_u32(dev->of_node, "gpio-num", gpio_num);
	if (ret)
		EINT_INF("NOTICE: cannot read gpio-num ret:%d\n", ret);

	return ret;
}

static int get_eint_info_by_gpio_num(struct device *dev,
				u32 gpio_num,
				struct mtk_seninf_eint_pin *pin)
{
	int len, i;

	if (seninf_eint.address_size_pairs == NULL) {
		EINT_INF(
			"ERROR: seninf_eint.address_size_pairs == NULL\n");
		return -1;
	}

	if (seninf_eint.eint_dts_info_pairs == NULL) {
		EINT_INF(
			"ERROR: seninf_eint.eint_dts_info_pairs == NULL\n");
		return -1;
	}

	len = seninf_eint.eint_dts_info_pairs_len;

	for (i = 0; i < len; i++) {
		unsigned int id = seninf_eint.eint_dts_info_pairs[i].id;
		unsigned int inst = seninf_eint.eint_dts_info_pairs[i].inst;

		if (gpio_num == id) {
			pin->dts_info.id = id;
			pin->dts_info.hw_latch_code = seninf_eint.eint_dts_info_pairs[i].hw_latch_code;

			if (inst < seninf_eint.address_size_pairs_len) {
				pin->dts_info.reg_address = seninf_eint.address_size_pairs[inst].address;
				pin->dts_info.reg_size = seninf_eint.address_size_pairs[inst].size;
			} else {
				EINT_INF("ERROR: get address_size_pairs with invalid inst");
			}

			break;
		}
	}

	EINT_DBG(
		"parsing mediatek,pins id:%u hw_latch_code:%#x address:%#x size:%#x\n",
		pin->dts_info.id, pin->dts_info.hw_latch_code,
		pin->dts_info.reg_address, pin->dts_info.reg_size);

	return 0;
}

static int get_dts_hw_info(struct platform_device *pdev,
	struct seninf_ctx *ctx,
	struct mtk_seninf_eint_pin *pin)
{
	struct device *dev = &pdev->dev;
	unsigned int eint_no;
	u32 gpio_num = 0;
	int ret, err = 0;

	/* check eint no in SenIF DTS */
	ret = get_eint_num(dev, &eint_no);
	err = (ret != 0) ? (err | 0x1) : err;
	EINT_DBG(
		"get eint num from dts => eint_no:%d, ret:%d(err:%#x)\n",
		eint_no, ret, err);

	if (ret == 0) {
		/* check advanced info - gpio num in SenIF DTS */
		ret = get_gpio_num(dev, &gpio_num);
		err = (ret != 0) ? (err | 0x2) : err;
		EINT_DBG(
			"get gpio num from dts => gpio_num:%u, ret:%d(err:%#x)\n",
			gpio_num, ret, err);

		/* check advanced info - eint pin info in SenIF DTS */
		ret = get_eint_info_by_gpio_num(dev, gpio_num, pin);
		err = (ret != 0) ? (err | 0x4) : err;
		EINT_DBG(
			"get eint info by gpio num from dts => ret:%d(err:%#x)\n",
			ret, err);
	}

	pin->inf_ctx = ctx;
	pin->tsrec_idx = ctx->tsrec_idx;
	pin->dts_info.eint_no = eint_no - 1; /* dts using 1, 2, 3 */
	pin->dts_info.gpio_num = gpio_num;

	/* MUST setup eint info on seninf ctx (all flow will pass this info) */
	/* => if you NOT setup it, it will be '0' and it's a valid value... */
	ctx->eint_idx = pin->dts_info.eint_no;

	EINT_INF(
		"seninf-csi-port-%d, eint_no:(%u|ctx:%u)(INVAL:%u(%#x)), gpio_num:%u, pin_id:%u, hw_latch_code:%#x, address:%#x, size:%#x => err:%#x\n",
		ctx->port,
		pin->dts_info.eint_no,
		ctx->eint_idx,
		EINT_IDX_INVALID, EINT_IDX_INVALID,
		pin->dts_info.gpio_num,
		pin->dts_info.id,
		pin->dts_info.hw_latch_code,
		pin->dts_info.reg_address,
		pin->dts_info.reg_size,
		err);

	return err;
}

static int init_eint_info(struct platform_device *pdev, struct mtk_seninf_eint_pin *pin)
{
	unsigned int index = pin->dts_info.eint_no;

	if (chk_eint_idx_valid(index, __func__))
		return -EINVAL;

	memcpy( seninf_eint.eints + index, pin, sizeof(struct mtk_seninf_eint_pin));
	mutex_init(&seninf_eint.eints[index].eint_intr_en_lock);

	EINT_DBG(
		"eint_no:%u, gpio_num:%u, pin_id:%u, hw_latch_code:%#x, address:%#x, size:%#x   [index:%u]\n",
		seninf_eint.eints[index].dts_info.eint_no,
		seninf_eint.eints[index].dts_info.gpio_num,
		seninf_eint.eints[index].dts_info.id,
		seninf_eint.eints[index].dts_info.hw_latch_code,
		seninf_eint.eints[index].dts_info.reg_address,
		seninf_eint.eints[index].dts_info.reg_size,
		index);

	return 0;
}

static int init_eint_regs_iomem(struct platform_device *pdev, unsigned int index)
{
	int ret = 0;
	void __iomem *reg_base = NULL;

	if (chk_eint_idx_valid(index, __func__))
		return -EINVAL;

	u32 address = seninf_eint.eints[index].dts_info.reg_address;
	u32 size = seninf_eint.eints[index].dts_info.reg_size;

	reg_base = ioremap(address, size);
	if (!reg_base) {
		EINT_INF("ERROR: Failed to map I/O memory   [index:%u]\n", index);
		return -ENOMEM;
	}

	seninf_eint.eints[index].dts_info.base = reg_base;
	EINT_DBG("of_iomap success get reg_base:%p   [index:%u]\n", reg_base, index);

	return ret;
}

static int init_eint_irq(struct platform_device *pdev, unsigned int index)
{
	int irq, ret = 0;
	struct device *dev = &pdev->dev;

	if (chk_eint_idx_valid(index, __func__))
		return -EINVAL;

	if(!dev) {
		EINT_INF("ERROR: dev == NULL   [index:%u]\n", index);
		return -1;
	}

	irq = irq_of_parse_and_map(dev->of_node, 0);
	if (irq <= 0) {
		EINT_INF("ERROR: failed to get irq number, ret:%d   [index:%u]\n", irq, index);
		return -1;
	}

	ret = devm_request_threaded_irq(dev, irq,
		mtk_eint_irq,
		mtk_thread_eint_irq,
		IRQF_NO_AUTOEN,
		"seninf-eint",
		seninf_eint.core);

	if (ret) {
		EINT_INF("ERROR: Request %s failed   [index:%u]\n", "seninf-eint", index);
		return ret;
	}

	seninf_eint.eints[index].irq_num = irq;
	EINT_DBG("registered seninf-eint-irq=%d seninf_eint.eints[%d].irq_num=%u   [index:%u]\n",
		irq, index, seninf_eint.eints[index].irq_num, index);

	return ret;
}

static inline void uninit_eint_regs_iomem(unsigned int index)
{
	void __iomem *base;

	if (chk_eint_idx_valid(index, __func__))
		return;

	base = seninf_eint.eints[index].dts_info.base;
}

static inline void uninit_eint_info(unsigned int index)
{
	if (chk_eint_idx_valid(index, __func__))
		return;

	memset(&(seninf_eint.eints[index]), 0, sizeof(struct mtk_seninf_eint_pin));
}

static void init_eint_event_st(struct platform_device *pdev, unsigned int index)
{
	struct device *dev = &pdev->dev;
	int ret = 0;

	if (chk_eint_idx_valid(index, __func__))
		return;

	/* init/setup fifo size for below dynamic mem alloc using */
	seninf_eint.eints[index].event_st.fifo_size =
		roundup_pow_of_two(EINT_IRQ_FIFO_LEN * sizeof(struct eint_irq_info));

	if (likely(seninf_eint.eints[index].event_st.msg_buffer == NULL)) {
		seninf_eint.eints[index].event_st.msg_buffer =
			devm_kzalloc(dev, seninf_eint.eints[index].event_st.fifo_size, GFP_ATOMIC);

		if (unlikely(seninf_eint.eints[index].event_st.msg_buffer == NULL))
			EINT_INF("ERROR: kfifo_init failed   [index:%u]\n", index);
	}

	atomic_set(&seninf_eint.eints[index].event_st.is_fifo_overflow, 0);

	ret = kfifo_init(
		&seninf_eint.eints[index].event_st.msg_fifo,
		seninf_eint.eints[index].event_st.msg_buffer,
		seninf_eint.eints[index].event_st.fifo_size);

	if (unlikely(ret != 0)) {
		EINT_INF("ERROR: kfifo_init failed\n");
		return;
	}
}

static void uninit_eint_event_st(struct platform_device *pdev, unsigned int index)
{
	if (chk_eint_idx_valid(index, __func__))
		return;

	kfifo_free(&seninf_eint.eints[index].event_st.msg_fifo);

	if (likely(seninf_eint.eints[index].event_st.msg_buffer != NULL))
		seninf_eint.eints[index].event_st.msg_buffer = NULL;

	/* clear struct data */
	memset(&seninf_eint.eints[index].event_st, 0, sizeof(struct eint_event_st));
}


/******************************************************************************
 * EINT init/uninit @ seninf_probe/seninf_remove
 *****************************************************************************/
int mtk_cam_seninf_eint_init(struct platform_device *pdev,
	struct seninf_ctx *ctx)
{
	int ret = 0;
	struct mtk_seninf_eint_pin pin = {0};

	/* init basic info (in seninf ctx) */
	ctx->eint_idx = EINT_IDX_INVALID;   /* will be OVW in get dts hw info */

	ret = get_dts_hw_info(pdev, ctx, &pin);
	if (ret)
		return ret;

	ret = init_eint_info(pdev, &pin);
	if (ret)
		return ret;

	init_eint_irq_cb_ctx(&seninf_eint.irq_cb_ctx[ctx->eint_idx]);

	ret = init_eint_regs_iomem(pdev, ctx->eint_idx);
	if (ret)
		return ret;

	init_eint_event_st(pdev, ctx->eint_idx);

	ret = init_eint_irq(pdev, ctx->eint_idx);
	if (ret)
		return ret;

	return 0;
}

int mtk_cam_seninf_eint_uninit(struct platform_device *pdev,
	struct seninf_ctx *ctx)
{
	unsigned int index = ctx->eint_idx;

	if (chk_eint_idx_valid(index, __func__))
		return -EINVAL;

	eint_irq_ctrl(index, 0);

	uninit_eint_event_st(pdev, index);

	uninit_eint_regs_iomem(index);

	uninit_eint_info(index);

	return 0;
}
