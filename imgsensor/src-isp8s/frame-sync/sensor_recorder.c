// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef FS_UT
#include <linux/delay.h>        /* for get ktime */
#endif

#include "sensor_recorder.h"
#include "frame_sync.h"
#include "frame_sync_log.h"
#include "frame_sync_def.h"
#include "frame_sync_trace.h"
/* TODO: check below files, after refactor some files should not include */
#include "frame_sync_algo.h"
#include "frame_sync_util.h"
#include "frame_monitor.h"


/******************************************************************************/
// frame recorder macro/define/enum
/******************************************************************************/
#define REDUCE_SEN_REC_LOG
#define PFX "SensorRecorder"
#define FS_LOG_DBG_DEF_CAT LOG_SEN_REC


#define RING_BACK(x, n) \
	(((x)+(RECORDER_DEPTH-((n)%RECORDER_DEPTH))) & (RECORDER_DEPTH-1))

#define RING_FORWARD(x, n) \
	(((x)+((n)%RECORDER_DEPTH)) & (RECORDER_DEPTH-1))


#if !defined(FS_UT)
#define frec_mutex_lock_init(p)        mutex_init(p)
#define frec_mutex_lock(p)             mutex_lock(p)
#define frec_mutex_unlock(p)           mutex_unlock(p)

#define frec_spin_lock_init(p)         spin_lock_init(p)
#define frec_spin_lock(p)              spin_lock(p)
#define frec_spin_unlock(p)            spin_unlock(p)
#else
#define frec_mutex_lock_init(p)
#define frec_mutex_lock(p)
#define frec_mutex_unlock(p)

#define frec_spin_lock_init(p)
#define frec_spin_lock(p)
#define frec_spin_unlock(p)
#endif


/******************************************************************************/
// frame recorder static structure
/******************************************************************************/
/* some sw properties that used to help explain the sensor mode info */
struct frec_sen_mode_property_info_st {
	/* common properties */
	unsigned int multi_exp_type;	/* STG / LBMF / DCG+VS */
	unsigned int mode_exp_cnt;	/* 2-exp / 3-exp */

	/* special properties */
	unsigned int exp_order;		/* for LBMF type => LE/SE 1st */
	unsigned int dol_type;		/* for STG => FDOL/DOL/NDOL */
};


/* for mode have different mode parameters for each exposure, e.g., DCG+VS */
struct frec_sen_cascade_mode_info_st {
	unsigned int lineTimeInNs[FS_HDR_MAX];
	unsigned int margin_lc[FS_HDR_MAX];
	unsigned int read_margin_lc[FS_HDR_MAX];
	unsigned int cit_loss_lc[FS_HDR_MAX];
};


/* keeping info that according to sensor mode */
struct frec_sen_mode_info_st {
	/* software properties for the sensor mode */
	struct frec_sen_mode_property_info_st prop;

	struct frec_sen_cascade_mode_info_st cas_mode_info;
};


struct FrameRecorder {
	/* is_init: */
	/*     0 => records data need be setup to sensor initial value. */
	/*       => need setup default shutter and fl values */
	unsigned int is_init;

	unsigned int fl_act_delay; // (N+2) => 3; (N+1) => 2;
	unsigned int def_fl_lc;
	struct frec_sen_mode_info_st mode_info;

	/* sensor record */
	FS_Atomic_T depth_idx;
	struct FrameRecord frame_recs[RECORDER_DEPTH];

	/* !!! frame length related info !!! */
	/* predict frame length */
	unsigned int curr_predicted_fl_lc;
	unsigned int curr_predicted_fl_us;
	unsigned int prev_predicted_fl_lc;
	unsigned int prev_predicted_fl_us;
	/* predict read offset for each shutter */
	unsigned int next_predicted_rd_offset_lc[FS_HDR_MAX];
	unsigned int next_predicted_rd_offset_us[FS_HDR_MAX];
	unsigned int curr_predicted_rd_offset_lc[FS_HDR_MAX];
	unsigned int curr_predicted_rd_offset_us[FS_HDR_MAX];
	/* predict EOF offset (in sensor HW order) for each shutter */
	unsigned int next_predicted_eof_offset_us[FS_HDR_MAX];
	unsigned int curr_predicted_eof_offset_us[FS_HDR_MAX];
	/* for identifying which settings have been latched */
	unsigned int mw_req_id_latched;


	/* timestamp info */
	enum fs_timestamp_src_type ts_src_type;
	unsigned int tick_factor;
	SenRec_TS_T ts_exp_0[VSYNCS_MAX];
	/* actual frame length (by timestamp diff.) */
	unsigned int act_fl_us;


	/* info for debug */
	// unsigned int p1_sof_cnt;
	/* recorder push/update system timestamp for debugging */
	unsigned long long sys_ts_recs[RECORDER_DEPTH];

	/* locks */
#if !defined(FS_UT)
	struct mutex frame_recs_update_lock;
	spinlock_t sen_mode_info_update_lock;
#endif
};
static struct FrameRecorder *frm_recorders[SENSOR_MAX_NUM];
/******************************************************************************/


/******************************************************************************/
// Frame Recorder function
/******************************************************************************/
static struct FrameRecorder *frec_g_recorder_ctx(const unsigned int idx,
	const char *caller)
{
	struct FrameRecorder *pfrec = frm_recorders[idx];

	if (unlikely(pfrec == NULL)) {
		LOG_MUST(
			"[%s] ERROR: [%u] ID:%#x(sidx:%u/inf:%u), recs[%u]:%p is nullptr, return\n",
			caller, idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			idx, pfrec);
	}

	return pfrec;
}


/*----------------------------------------------------------------------------*/
// recorder dynamic memory allocate / free functions
/*----------------------------------------------------------------------------*/
static void frec_data_init(const unsigned int idx)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);

	if (unlikely(pfrec == NULL))
		return;

	frec_mutex_lock_init(&pfrec->frame_recs_update_lock);
	frec_spin_lock_init(&pfrec->sen_mode_info_update_lock);
}


void frec_alloc_mem_data(const unsigned int idx, void *dev)
{
	struct FrameRecorder *ptr = NULL;

	if (unlikely(frm_recorders[idx] != NULL)) {
		LOG_MUST(
			"NOTICE: [%u] ID:%#x(sidx:%u/inf:%u), mem already allocated(recs[%u]:%p), return\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			idx, frm_recorders[idx]);
		return;
	}

	ptr = FS_DEV_ZALLOC(dev, sizeof(*ptr));
	if (unlikely(ptr == NULL)) {
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u/inf:%u), mem allocate failed(ptr:%p, recs[%u]:%p), return\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			ptr, idx, frm_recorders[idx]);
		return;
	}

	frm_recorders[idx] = ptr;

	/* init allocated data */
	frec_data_init(idx);

	LOG_INF(
		"[%u] ID:%#x(sidx:%u/inf:%u), mem allocated (ptr:%p, recs[%u]:%p)\n",
		idx,
		fs_get_reg_sensor_id(idx),
		fs_get_reg_sensor_idx(idx),
		fs_get_reg_sensor_inf_idx(idx),
		ptr, idx, frm_recorders[idx]);
}


void frec_free_mem_data(const unsigned int idx, void *dev)
{
	if (unlikely(frm_recorders[idx] == NULL)) {
		LOG_MUST(
			"NOTICE: [%u] ID:%#x(sidx:%u/inf:%u), mem already null(recs[%u]:%p), return\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			idx, frm_recorders[idx]);
		return;
	}

	FS_FREE(frm_recorders[idx]);
	frm_recorders[idx] = NULL;

	LOG_INF(
		"[%u] ID:%#x(sidx:%u/inf:%u), mem freed (recs[%u]:%p)\n",
		idx,
		fs_get_reg_sensor_id(idx),
		fs_get_reg_sensor_idx(idx),
		fs_get_reg_sensor_inf_idx(idx),
		idx, frm_recorders[idx]);
}
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/* utilities functions */
/*----------------------------------------------------------------------------*/
static unsigned int divide_num(const unsigned int idx,
	const unsigned int n, const unsigned int base, const char *caller)
{
	unsigned int val = n;

	/* error handle */
	if (unlikely(base == 0)) {
		LOG_MUST(
			"[%s]: ERROR: [%u] ID:%#x(sidx:%u/inf:%u), divide by zero (plz chk input params), n:%u/base:%u, return:%u\n",
			caller, idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			n, base, n);
		return n;
	}

	do_div(val, base);

	return val;
}


/* Return: @0 => non valid / @1 => valid */
static unsigned int chk_if_sen_fdelay_is_valid(const unsigned int idx,
	const struct FrameRecorder *pfrec, const char *caller)
{
	const unsigned int fdelay = pfrec->fl_act_delay;

	/* error handle, check sensor fl_active_delay value */
	if (unlikely((fdelay < 2) || (fdelay > 3))) {
		LOG_MUST(
			"[%s]: ERROR: [%u] ID:%#x(sidx:%u/inf:%u), sensor driver's frame_time_delay_frame:%u is not valid (MUST be 2 or 3), plz check sensor driver for getting correct value\n",
			caller, idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			fdelay);
		return 0;
	}
	return 1;
}


static inline int chk_exp_order_valid(const unsigned int exp_order)
{
	return (likely(exp_order < EXP_ORDER_MAX)) ? 1 : 0;
}


static inline int chk_exp_cnt_valid(const unsigned int exp_cnt)
{
	return (likely(exp_cnt < (FS_HDR_MAX + 1))) ? 1 : 0;
}


static inline int chk_exp_no_valid(const unsigned int exp_no)
{
	return (likely(exp_no < FS_HDR_MAX)) ? 1 : 0;
}


static inline int g_exp_order_idx_mapping(const unsigned int idx,
	const unsigned int m_exp_order, const unsigned int m_exp_cnt,
	const unsigned int exp_no, const char *caller)
{
	/* error handling */
	if (unlikely((!chk_exp_order_valid(m_exp_order))
			|| (!chk_exp_cnt_valid(m_exp_cnt))
			|| (!chk_exp_no_valid(exp_no)))) {
		LOG_MUST(
			"[%s] ERROR: [%u] ID:%#x(sidx:%u/inf:%u), get invalid para, exp(order:%u/cnt:%u/no:%u) => return exp_idx:%d\n",
			caller,
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			m_exp_order,
			m_exp_cnt,
			exp_no,
			FS_HDR_NONE);

		return FS_HDR_NONE;
	}

	return (exp_order_idx_map[m_exp_order][m_exp_cnt][exp_no]);
}


static int frec_g_mode_last_exp_idx(const unsigned int idx,
	const struct FrameRecorder *pfrec, const unsigned int depth_idx)
{
	unsigned int m_exp_cnt, m_exp_order, last_exp_no;
	int last_exp_idx;

	m_exp_order = pfrec->frame_recs[depth_idx].exp_order;

	m_exp_cnt = pfrec->frame_recs[depth_idx].mode_exp_cnt;
	if (unlikely(m_exp_cnt == 0)) {
		m_exp_cnt = 1;
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u/inf:%u), m_exp_cnt:0 => assign m_exp_cnt:%u\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			m_exp_cnt);
	}
	last_exp_no = (m_exp_cnt - 1);

	last_exp_idx = g_exp_order_idx_mapping(idx, m_exp_order, m_exp_cnt, last_exp_no, __func__);
	if (unlikely(last_exp_idx < 0)) {
		last_exp_idx = 0;
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u/inf:%u), exp_order_idx_map[%u][%u][%u]:(< 0) => assign last_exp_idx:%d\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			m_exp_order,
			m_exp_cnt,
			last_exp_no,
			last_exp_idx);
	}

	return last_exp_idx;
}


/*----------------------------------------------------------------------------*/
/* sensor mode info struct                                                    */
/*----------------------------------------------------------------------------*/
static inline void update_sen_cascade_mode_info(
	struct frec_sen_cascade_mode_info_st *p_dst_st,
	const struct fs_hdr_sen_cascade_mode_info_st *p_src_st)
{
	/* !!! sync all needed info by yourself !!! */
	memcpy(&p_dst_st->lineTimeInNs, &p_src_st->lineTimeInNs,
		sizeof(unsigned int) * FS_HDR_MAX);
	memcpy(&p_dst_st->margin_lc, &p_src_st->margin_lc,
		sizeof(unsigned int) * FS_HDR_MAX);
	memcpy(&p_dst_st->read_margin_lc, &p_src_st->read_margin_lc,
		sizeof(unsigned int) * FS_HDR_MAX);
	memcpy(&p_dst_st->cit_loss_lc, &p_src_st->cit_loss_lc,
		sizeof(unsigned int) * FS_HDR_MAX);
}


static inline void update_sen_mode_info(struct FrameRecorder *pfrec,
	struct frec_sen_mode_info_st *p_mode_info,
	const struct fs_hdr_exp_st *p_hdr_info)
{
	frec_spin_lock(&pfrec->sen_mode_info_update_lock);
	/* => some SW properties */
	pfrec->mode_info.prop.multi_exp_type = p_hdr_info->multi_exp_type;
	pfrec->mode_info.prop.mode_exp_cnt = p_hdr_info->mode_exp_cnt;
	pfrec->mode_info.prop.exp_order = p_hdr_info->exp_order;
	pfrec->mode_info.prop.dol_type = p_hdr_info->dol_type;

	/* => sensor HW mode info */
	update_sen_cascade_mode_info(
		&p_mode_info->cas_mode_info,
		&p_hdr_info->cas_mode_info);
	frec_spin_unlock(&pfrec->sen_mode_info_update_lock);
}


static inline void clr_sen_mode_info(struct FrameRecorder *pfrec,
	struct frec_sen_mode_info_st *p_mode_info)
{
	frec_spin_lock(&pfrec->sen_mode_info_update_lock);
	memset(p_mode_info, 0, sizeof(struct frec_sen_mode_info_st));
	frec_spin_unlock(&pfrec->sen_mode_info_update_lock);
}


static void g_sen_mode_info(const unsigned int idx,
	struct frec_sen_mode_info_st *p_info)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	struct frec_sen_mode_info_st *p_mode_info = NULL;

	/* unexpected case */
	if (unlikely(pfrec == NULL || p_info == NULL))
		return;
	p_mode_info = &pfrec->mode_info;

	frec_spin_lock(&pfrec->sen_mode_info_update_lock);
	*p_info = *p_mode_info;
	frec_spin_unlock(&pfrec->sen_mode_info_update_lock);
}


/*----------------------------------------------------------------------------*/
// debug/dump function
/*----------------------------------------------------------------------------*/
static inline unsigned int frec_snprf_sen_cascade_mode_info(
	const struct frec_sen_mode_info_st *p_mode_info,
	unsigned int log_str_len, char *log_buf, unsigned int len)
{
	const unsigned int mode_exp_cnt = p_mode_info->prop.mode_exp_cnt;
	unsigned int i;

	FS_SNPRF(log_str_len, log_buf, len, ",cas(lineT/mar(c,r)/cLOS):(");
	for (i = 0; (i < mode_exp_cnt && i < FS_HDR_MAX); ++i) {
		FS_SNPRF(log_str_len, log_buf, len,
			"[%u](%u/(%u,%u)/%u)|",
			i,
			p_mode_info->cas_mode_info.lineTimeInNs[i],
			p_mode_info->cas_mode_info.margin_lc[i],
			p_mode_info->cas_mode_info.read_margin_lc[i],
			p_mode_info->cas_mode_info.cit_loss_lc[i]);
	}
	FS_SNPRF(log_str_len, log_buf, len, ")");

	return len;
}


static unsigned int frec_snprf_sen_mode_info(
	const struct frec_sen_mode_info_st *p_mode_info,
	unsigned int log_str_len, char *log_buf, unsigned int len)
{
	if (unlikely(p_mode_info == NULL))
		return len;

	FS_SNPRF(log_str_len, log_buf, len,
		"mProp(m:%u(t:%u(%u),o:%u))",
		p_mode_info->prop.mode_exp_cnt,
		p_mode_info->prop.multi_exp_type,
		p_mode_info->prop.dol_type,
		p_mode_info->prop.exp_order);

	if (p_mode_info->prop.multi_exp_type == MULTI_EXP_TYPE_DCG_VSL) {
		len = frec_snprf_sen_cascade_mode_info(
			p_mode_info, log_str_len, log_buf, len);
	}

	return len;
}


void frec_dump_cascade_exp_fl_info(const unsigned int idx,
	const unsigned int *exp_cas_arr, const unsigned int *fl_cas_arr,
	const unsigned int arr_len, const char *caller)
{
	const unsigned int log_str_len = 512;
	unsigned int i;
	char *log_buf = NULL;
	int len = 0, ret;

	ret = alloc_log_buf(log_str_len, &log_buf);
	if (unlikely(ret != 0)) {
		LOG_MUST("ERROR: log_buf allocate memory failed\n");
		return;
	}

	FS_SNPRF(log_str_len, log_buf, len,
		"[%s]: [%u] ID:%#x(sidx:%u/inf:%u), cas_exp:(",
		caller,
		idx,
		fs_get_reg_sensor_id(idx),
		fs_get_reg_sensor_idx(idx),
		fs_get_reg_sensor_inf_idx(idx));

	for (i = 0; i < arr_len; ++i) {
		FS_SNPRF(log_str_len, log_buf, len, "%u", exp_cas_arr[i]);
		if ((i + 1) < arr_len)
			FS_SNPRF(log_str_len, log_buf, len, "/");
	}

	FS_SNPRF(log_str_len, log_buf, len, "), cas_fl:(");

	for (i = 0; i < arr_len; ++i) {
		FS_SNPRF(log_str_len, log_buf, len, "%u", fl_cas_arr[i]);
		if ((i + 1) < arr_len)
			FS_SNPRF(log_str_len, log_buf, len, "/");
	}

	FS_SNPRF(log_str_len, log_buf, len, "), arr_len:%u", arr_len);

	LOG_MUST("%s\n", log_buf);
	FS_FREE(log_buf);
}


void frec_dump_predicted_fl_info_st(const unsigned int idx,
	const struct predicted_fl_info_st *fl_info,
	const char *caller)
{
	LOG_MUST(
		"[%s]: [%u] ID:%#x(sidx:%u/inf:%u), pr_fl(c:%u(%u)/n:%u(%u)/s:%u(%u)), r_offset(c:(%u(%u)/%u(%u)/%u(%u)/%u(%u)/%u(%u), n:(%u(%u)/%u(%u)/%u(%u)/%u(%u)/%u(%u)))\n",
		caller,
		idx,
		fs_get_reg_sensor_id(idx),
		fs_get_reg_sensor_idx(idx),
		fs_get_reg_sensor_inf_idx(idx),
		fl_info->pr_curr_fl_us,
		fl_info->pr_curr_fl_lc,
		fl_info->pr_next_fl_us,
		fl_info->pr_next_fl_lc,
		fl_info->pr_stable_fl_us,
		fl_info->pr_stable_fl_lc,
		fl_info->curr_exp_rd_offset_us[0],
		fl_info->curr_exp_rd_offset_lc[0],
		fl_info->curr_exp_rd_offset_us[1],
		fl_info->curr_exp_rd_offset_lc[1],
		fl_info->curr_exp_rd_offset_us[2],
		fl_info->curr_exp_rd_offset_lc[2],
		fl_info->curr_exp_rd_offset_us[3],
		fl_info->curr_exp_rd_offset_lc[3],
		fl_info->curr_exp_rd_offset_us[4],
		fl_info->curr_exp_rd_offset_lc[4],
		fl_info->next_exp_rd_offset_us[0],
		fl_info->next_exp_rd_offset_lc[0],
		fl_info->next_exp_rd_offset_us[1],
		fl_info->next_exp_rd_offset_lc[1],
		fl_info->next_exp_rd_offset_us[2],
		fl_info->next_exp_rd_offset_lc[2],
		fl_info->next_exp_rd_offset_us[3],
		fl_info->next_exp_rd_offset_lc[3],
		fl_info->next_exp_rd_offset_us[4],
		fl_info->next_exp_rd_offset_lc[4]);
}


void frec_dump_frame_record_info(const struct FrameRecord *p_frame_rec,
	const char *caller)
{
	LOG_MUST(
		"[%s]: req_id:%d, (exp_lc:%u/fl_lc:%u), (a:%u/m:%u(%u/%u,%u), exp:%u/%u/%u/%u/%u, fl:%u/%u/%u/%u/%u), margin_lc:(%u, read:%u), readout_len_lc:%u, min_vblank_lc:%u, line_time:%u, routT(us):%u\n",
		caller,
		p_frame_rec->mw_req_id,
		p_frame_rec->shutter_lc,
		p_frame_rec->framelength_lc,
		p_frame_rec->ae_exp_cnt,
		p_frame_rec->mode_exp_cnt,
		p_frame_rec->m_exp_type,
		p_frame_rec->dol_type,
		p_frame_rec->exp_order,
		p_frame_rec->exp_lc_arr[0],
		p_frame_rec->exp_lc_arr[1],
		p_frame_rec->exp_lc_arr[2],
		p_frame_rec->exp_lc_arr[3],
		p_frame_rec->exp_lc_arr[4],
		p_frame_rec->fl_lc_arr[0],
		p_frame_rec->fl_lc_arr[1],
		p_frame_rec->fl_lc_arr[2],
		p_frame_rec->fl_lc_arr[3],
		p_frame_rec->fl_lc_arr[4],
		p_frame_rec->margin_lc,
		p_frame_rec->read_margin_lc,
		p_frame_rec->readout_len_lc,
		p_frame_rec->min_vblank_lc,
		p_frame_rec->lineTimeInNs,
		p_frame_rec->readout_time_us);
}


void frec_dump_recorder(const unsigned int idx, const char *caller)
{
	const struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	const unsigned int log_str_len = LOG_BUF_STR_LEN;
	struct frec_sen_mode_info_st mode_info = {0};
	unsigned int act_fl_arr[RECORDER_DEPTH-1] = {0};
	unsigned int depth_idx;
	unsigned int i;
	char *log_buf = NULL;
	int len = 0, ret;

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;

	depth_idx = FS_ATOMIC_READ(&pfrec->depth_idx);

	ret = alloc_log_buf(log_str_len, &log_buf);
	if (unlikely(ret != 0)) {
		LOG_MUST("ERROR: log_buf allocate memory failed\n");
		return;
	}

	FS_SNPRF(log_str_len, log_buf, len,
		"[%s]:[%u][sidx:%u] fdelay:%u/defFL:%u/lineT:%u/mar(%u,r:%u)/roL:%u/minVB:%u,",
		caller,
		idx,
		fs_get_reg_sensor_idx(idx),
		pfrec->fl_act_delay,
		pfrec->def_fl_lc,
		pfrec->frame_recs[depth_idx].lineTimeInNs,
		pfrec->frame_recs[depth_idx].margin_lc,
		pfrec->frame_recs[depth_idx].read_margin_lc,
		pfrec->frame_recs[depth_idx].readout_len_lc,
		pfrec->frame_recs[depth_idx].min_vblank_lc);

	g_sen_mode_info(idx, &mode_info);
	len = frec_snprf_sen_mode_info(&mode_info, log_str_len, log_buf, len);

	for (i = 0; i < RECORDER_DEPTH; ++i) {
		/* dump data from newest to old */
		/* newest => copy from previous, at this time sensor driver not get AE ctrl yet */
		const unsigned int idx = RING_BACK(depth_idx, i);

		FS_SNPRF(log_str_len, log_buf, len,
			",([%u](%llu/req:%d):(%u/%u),(a:%u/m:%u(t:%u/%u,o:%u),%u/%u/%u/%u/%u",
			idx,
			pfrec->sys_ts_recs[idx]/1000,
			pfrec->frame_recs[idx].mw_req_id,
			pfrec->frame_recs[idx].shutter_lc,
			pfrec->frame_recs[idx].framelength_lc,
			pfrec->frame_recs[idx].ae_exp_cnt,
			pfrec->frame_recs[idx].mode_exp_cnt,
			pfrec->frame_recs[idx].m_exp_type,
			pfrec->frame_recs[idx].dol_type,
			pfrec->frame_recs[idx].exp_order,
			pfrec->frame_recs[idx].exp_lc_arr[0],
			pfrec->frame_recs[idx].exp_lc_arr[1],
			pfrec->frame_recs[idx].exp_lc_arr[2],
			pfrec->frame_recs[idx].exp_lc_arr[3],
			pfrec->frame_recs[idx].exp_lc_arr[4]);

		if (frec_chk_if_lut_is_used(pfrec->frame_recs[idx].m_exp_type)) {
			FS_SNPRF(log_str_len, log_buf, len,
				",%u/%u/%u/%u/%u",
				pfrec->frame_recs[idx].fl_lc_arr[0],
				pfrec->frame_recs[idx].fl_lc_arr[1],
				pfrec->frame_recs[idx].fl_lc_arr[2],
				pfrec->frame_recs[idx].fl_lc_arr[3],
				pfrec->frame_recs[idx].fl_lc_arr[4]);
		}

		FS_SNPRF(log_str_len, log_buf, len, "))");
	}

	FS_SNPRF(log_str_len, log_buf, len,
		",ofs(r:%u)(R:(%u(%u)/%u(%u)/%u(%u)/%u(%u)/%u(%u))",
		pfrec->mw_req_id_latched,
		pfrec->curr_predicted_rd_offset_us[0],
		pfrec->curr_predicted_rd_offset_lc[0],
		pfrec->curr_predicted_rd_offset_us[1],
		pfrec->curr_predicted_rd_offset_lc[1],
		pfrec->curr_predicted_rd_offset_us[2],
		pfrec->curr_predicted_rd_offset_lc[2],
		pfrec->curr_predicted_rd_offset_us[3],
		pfrec->curr_predicted_rd_offset_lc[3],
		pfrec->curr_predicted_rd_offset_us[4],
		pfrec->curr_predicted_rd_offset_lc[4]);

	FS_SNPRF(log_str_len, log_buf, len,
		"|E:(%u/%u/%u/%u/%u))",
		pfrec->curr_predicted_eof_offset_us[0],
		pfrec->curr_predicted_eof_offset_us[1],
		pfrec->curr_predicted_eof_offset_us[2],
		pfrec->curr_predicted_eof_offset_us[3],
		pfrec->curr_predicted_eof_offset_us[4]);

	FS_SNPRF(log_str_len, log_buf, len,
		",pr(p(%u(%u)/act:%u)/c(%u(%u))",
		pfrec->prev_predicted_fl_us,
		pfrec->prev_predicted_fl_lc,
		pfrec->act_fl_us,
		pfrec->curr_predicted_fl_us,
		pfrec->curr_predicted_fl_lc);

	for (i = 0; i < RECORDER_DEPTH-1; ++i) {
		SenRec_TS_T tick_a, tick_b;

		/* check if tick factor is valid */
		if (unlikely(pfrec->tick_factor == 0))
			break;

		/* update actual frame length by timestamp diff */
		tick_a = pfrec->ts_exp_0[i] * pfrec->tick_factor;
		tick_b = pfrec->ts_exp_0[i+1] * pfrec->tick_factor;

		act_fl_arr[i] = (tick_b != 0)
			? ((tick_a - tick_b) / (pfrec->tick_factor)) : 0;
	}

	if (pfrec->ts_src_type == FS_TS_SRC_EINT) {
		/* e.g., MAIN source is TSREC but flow is triggered by EINT */
		FS_SNPRF(log_str_len, log_buf, len,
#ifdef TS_TICK_64_BITS
			",ts_eint(%u/%u/%u,%llu/%llu/%llu/%llu)",
#else
			",ts_eint(%u/%u/%u,%u/%u/%u/%u)",
#endif
			act_fl_arr[0],
			act_fl_arr[1],
			act_fl_arr[2],
			pfrec->ts_exp_0[0],
			pfrec->ts_exp_0[1],
			pfrec->ts_exp_0[2],
			pfrec->ts_exp_0[3]);

		fs_util_tsrec_dynamic_msg_connector(idx,
			log_str_len, log_buf, len, __func__);
	} else if (frm_get_ts_src_type() != FS_TS_SRC_TSREC) {
		/* e.g., using CCU */
		FS_SNPRF(log_str_len, log_buf, len,
#ifdef TS_TICK_64_BITS
			",ts(%u/%u/%u,%llu/%llu/%llu/%llu)",
#else
			",ts(%u/%u/%u,%u/%u/%u/%u)",
#endif
			act_fl_arr[0],
			act_fl_arr[1],
			act_fl_arr[2],
			pfrec->ts_exp_0[0],
			pfrec->ts_exp_0[1],
			pfrec->ts_exp_0[2],
			pfrec->ts_exp_0[3]);
	} else {
		/* e.g., using TSREC */
		FS_SNPRF(log_str_len, log_buf, len,
			",ts(%u/%u/%u)",
			act_fl_arr[0],
			act_fl_arr[1],
			act_fl_arr[2]);

		fs_util_tsrec_dynamic_msg_connector(idx,
			log_str_len, log_buf, len, __func__);
	}

	LOG_MUST_LOCK("%s\n", log_buf);
	FS_TRACE_PR_LOG_MUST("%s", log_buf);

	FS_FREE(log_buf);
}
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/* user auxiliary functions */
/*----------------------------------------------------------------------------*/
void frec_setup_frame_rec_by_fs_streaming_st(struct FrameRecord *p_frame_rec,
	const struct fs_streaming_st *sensor_info)
{
	memset(p_frame_rec, 0, sizeof(struct FrameRecord));

	/* init/setup sensor frame recorder */
	p_frame_rec->shutter_lc = sensor_info->def_shutter_lc;
	p_frame_rec->framelength_lc = sensor_info->def_fl_lc;

	p_frame_rec->ae_exp_cnt = sensor_info->hdr_exp.ae_exp_cnt;
	memcpy(p_frame_rec->exp_lc_arr, sensor_info->hdr_exp.exp_lc,
		sizeof(unsigned int) * FS_HDR_MAX);
	memcpy(p_frame_rec->fl_lc_arr, sensor_info->hdr_exp.fl_lc,
		sizeof(unsigned int) * FS_HDR_MAX);

	p_frame_rec->margin_lc = sensor_info->margin_lc;
	p_frame_rec->read_margin_lc = sensor_info->hdr_exp.read_margin_lc;
	p_frame_rec->readout_len_lc = sensor_info->hdr_exp.readout_len_lc;
	p_frame_rec->mode_exp_cnt = sensor_info->hdr_exp.mode_exp_cnt;
	p_frame_rec->m_exp_type = sensor_info->hdr_exp.multi_exp_type;
	p_frame_rec->dol_type = sensor_info->hdr_exp.dol_type;
	p_frame_rec->exp_order = sensor_info->hdr_exp.exp_order;
	p_frame_rec->min_vblank_lc = sensor_info->hdr_exp.min_vblank_lc;

	p_frame_rec->lineTimeInNs = sensor_info->lineTimeInNs;
	p_frame_rec->readout_time_us = sensor_info->readout_time_us;

	// p_frame_rec->mw_req_id = // NOT has this info now.

	// frec_dump_frame_record_info(p_frame_rec, __func__);
}


void frec_setup_frame_rec_by_fs_perframe_st(struct FrameRecord *p_frame_rec,
	const struct fs_perframe_st *pf_ctrl)
{
	memset(p_frame_rec, 0, sizeof(struct FrameRecord));

	/* init/setup sensor frame recorder */
	p_frame_rec->shutter_lc = pf_ctrl->shutter_lc;
	p_frame_rec->framelength_lc = pf_ctrl->out_fl_lc;

	p_frame_rec->ae_exp_cnt = pf_ctrl->hdr_exp.ae_exp_cnt;
	memcpy(p_frame_rec->exp_lc_arr, pf_ctrl->hdr_exp.exp_lc,
		sizeof(unsigned int) * FS_HDR_MAX);
	memcpy(p_frame_rec->fl_lc_arr, pf_ctrl->hdr_exp.fl_lc,
		sizeof(unsigned int) * FS_HDR_MAX);

	p_frame_rec->margin_lc = pf_ctrl->margin_lc;
	p_frame_rec->read_margin_lc = pf_ctrl->hdr_exp.read_margin_lc;
	p_frame_rec->readout_len_lc = pf_ctrl->hdr_exp.readout_len_lc;
	p_frame_rec->mode_exp_cnt = pf_ctrl->hdr_exp.mode_exp_cnt;
	p_frame_rec->dol_type = pf_ctrl->hdr_exp.dol_type;
	p_frame_rec->m_exp_type = pf_ctrl->hdr_exp.multi_exp_type;
	p_frame_rec->exp_order = pf_ctrl->hdr_exp.exp_order;
	p_frame_rec->min_vblank_lc = pf_ctrl->hdr_exp.min_vblank_lc;

	p_frame_rec->lineTimeInNs = pf_ctrl->lineTimeInNs;
	p_frame_rec->readout_time_us = pf_ctrl->readout_time_us;

	p_frame_rec->mw_req_id = pf_ctrl->req_id;

	// frec_dump_frame_record_info(p_frame_rec, __func__);
}


void frec_setup_seamless_rec_by_fs_seamless_st(
	struct frec_seamless_st *p_seamless_rec,
	const struct fs_seamless_st *p_seamless_info)
{
	memset(p_seamless_rec, 0, sizeof(*p_seamless_rec));

	memcpy(&p_seamless_rec->prop, &p_seamless_info->prop,
		sizeof(p_seamless_rec->prop));

	/* setup frame record data */
	frec_setup_frame_rec_by_fs_perframe_st(
		&p_seamless_rec->frame_rec, &p_seamless_info->seamless_pf_ctrl);

	// frec_dump_frame_record_info(&p_seamless_rec->frame_rec, __func__);
}


void frec_query_pred_info(const unsigned int idx,
	struct fs_pred_info_st *p_pred_info)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	unsigned int i, arr_exp_cnt = 0;

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;
	if (unlikely(p_pred_info == NULL)) {
		LOG_MUST(
			"ERROR: [%u][sidx:%u] p_pred_info from user is nullptr, return\n",
			idx, fs_get_reg_sensor_idx(idx));
		return;
	}

	frec_mutex_lock(&pfrec->frame_recs_update_lock);

	p_pred_info->req_id = pfrec->mw_req_id_latched;
	p_pred_info->curr_fl_us = pfrec->curr_predicted_fl_us;
	for (i = 0; i < FS_HDR_MAX; ++i) {
		p_pred_info->curr_eof_offset_us[i] =
			pfrec->curr_predicted_eof_offset_us[i];

		if (p_pred_info->curr_eof_offset_us[i] != 0)
			arr_exp_cnt++;
	}
	p_pred_info->mode_exp_cnt = arr_exp_cnt;

	frec_mutex_unlock(&pfrec->frame_recs_update_lock);
}


/*----------------------------------------------------------------------------*/
// NDOL / LB-MF utilities functions
/*----------------------------------------------------------------------------*/
static inline unsigned int g_lut_id_by_cascade_ref_idx(
	const unsigned int ref_idx, const unsigned int mode_exp_cnt)
{
	return (mode_exp_cnt != 0)
		? ((ref_idx % mode_exp_cnt) % FS_HDR_MAX)
		: (ref_idx % FS_HDR_MAX);
}


static inline unsigned int g_margin_lc_by_lut_id(const unsigned int idx,
	const unsigned int ref_idx,
	const struct frec_sen_mode_info_st *mode_info,
	const struct FrameRecord *curr_rec)
{
	const unsigned int multi_exp_type = mode_info->prop.multi_exp_type;
	unsigned int ret = 0;

	switch (multi_exp_type) {
	case MULTI_EXP_TYPE_DCG_VSL:
	{
		const unsigned int lut_idx = g_lut_id_by_cascade_ref_idx(
			ref_idx, curr_rec->mode_exp_cnt);

		ret = mode_info->cas_mode_info.margin_lc[lut_idx];
	}
		break;
	case MULTI_EXP_TYPE_LBMF:
	default:
		ret = divide_num(idx,
			curr_rec->margin_lc, curr_rec->mode_exp_cnt, __func__);
		break;
	}

	return ret;
}


static inline unsigned int g_map_cit_lc_by_lut_id(const unsigned int idx,
	const unsigned int fll_ref_idx, const unsigned int exp_ref_idx,
	const unsigned int exp_lc,
	const struct frec_sen_mode_info_st *mode_info,
	const struct FrameRecord *curr_rec)
{
	const unsigned int multi_exp_type = mode_info->prop.multi_exp_type;
	unsigned int ret = 0;

	switch (multi_exp_type) {
	case MULTI_EXP_TYPE_DCG_VSL:
	{
		const unsigned int cit_lut_idx = g_lut_id_by_cascade_ref_idx(
			exp_ref_idx, curr_rec->mode_exp_cnt);
		const unsigned int fll_lut_idx = g_lut_id_by_cascade_ref_idx(
			fll_ref_idx, curr_rec->mode_exp_cnt);
		const unsigned int cit_tline_ns =
			mode_info->cas_mode_info.lineTimeInNs[cit_lut_idx];
		const unsigned int fll_tline_ns =
			mode_info->cas_mode_info.lineTimeInNs[fll_lut_idx];

		/* ret = ((exp_lc * cit_tline_ns) / fll_tline_ns); */
		ret = FS_CEIL_U((exp_lc * cit_tline_ns), fll_tline_ns);
	}
		break;
	case MULTI_EXP_TYPE_LBMF:
	default:
		ret = exp_lc;
		break;
	}

	return ret;
}


static inline unsigned int g_based_fll_by_lut_id(const unsigned int idx,
	const unsigned int ref_idx,
	const struct frec_sen_mode_info_st *mode_info,
	const struct FrameRecord *curr_rec)
{
	const unsigned int multi_exp_type = mode_info->prop.multi_exp_type;
	unsigned int ret = 0;

	switch (multi_exp_type) {
	case MULTI_EXP_TYPE_DCG_VSL:
	{
		const unsigned int lut_idx = g_lut_id_by_cascade_ref_idx(
			ref_idx, curr_rec->mode_exp_cnt);

		ret = curr_rec->readout_len_lc +
			mode_info->cas_mode_info.read_margin_lc[lut_idx];
	}
		break;
	case MULTI_EXP_TYPE_LBMF:
	default:
		ret = curr_rec->readout_len_lc + curr_rec->read_margin_lc;
		break;
	}

	return ret;
}


static inline unsigned int g_cit_loss_by_lut_id(const unsigned int idx,
	const unsigned int ref_idx,
	const struct frec_sen_mode_info_st *mode_info,
	const struct FrameRecord *curr_rec)
{
	const unsigned int multi_exp_type = mode_info->prop.multi_exp_type;
	unsigned int ret = 0;

	switch (multi_exp_type) {
	case MULTI_EXP_TYPE_DCG_VSL:
	{
		const unsigned int lut_idx = g_lut_id_by_cascade_ref_idx(
			ref_idx, curr_rec->mode_exp_cnt);

		ret = mode_info->cas_mode_info.cit_loss_lc[lut_idx];
	}
		break;
	case MULTI_EXP_TYPE_LBMF:
	default:
		ret = 0;
		break;
	}

	return ret;
}


/**
 * function for supporting LBMF/AEB and DCG+VS/L.
 * (MUST be used with g_fll_tgt_req() & g_curr_total_fl_by_lut_id() function)
 *
 * => LBMF/AEB: using line count domain value.
 * => DCG+VS/L: auto switch to time domain value from line count domain.
 *              (used equiv line time that on the driver ctx)
 */
static inline unsigned int g_equiv_min_fl_lc(const unsigned int idx,
	const unsigned int equiv_min_fl,
	const struct frec_sen_mode_info_st *mode_info,
	const struct FrameRecord *curr_rec)
{
	const unsigned int multi_exp_type = mode_info->prop.multi_exp_type;
	unsigned int result = 0;

	switch (multi_exp_type) {
	case MULTI_EXP_TYPE_DCG_VSL:
		/* used equiv line time that on the driver ctx */
		result = convert2LineCount(curr_rec->lineTimeInNs, equiv_min_fl);
		break;
	case MULTI_EXP_TYPE_LBMF:
	default:
		result = equiv_min_fl;
		break;
	}

	return result;
}


/**
 * function for supporting LBMF/AEB and DCG+VS/L.
 * (MUST be used with g_equiv_min_fl_lc() & g_fll_tgt_req() function)
 *
 * => LBMF/AEB: using line count domain value.
 * => DCG+VS/L: auto switch to time domain value from line count domain.
 *
 * @curr_total_fl: start from 0
 */
static inline unsigned int g_curr_total_fl_by_lut_id(const unsigned int idx,
	const unsigned int fl_lc, const unsigned int curr_total_fl,
	const unsigned int ref_idx,
	const struct frec_sen_mode_info_st *mode_info,
	const struct FrameRecord *curr_rec)
{
	const unsigned int multi_exp_type = mode_info->prop.multi_exp_type;
	unsigned int result = 0;

	switch (multi_exp_type) {
	case MULTI_EXP_TYPE_DCG_VSL:
	{
		const unsigned int lut_idx = g_lut_id_by_cascade_ref_idx(
			ref_idx, curr_rec->mode_exp_cnt);

		result = curr_total_fl + convert2TotalTime(
			mode_info->cas_mode_info.lineTimeInNs[lut_idx], fl_lc);
	}
		break;
	case MULTI_EXP_TYPE_LBMF:
	default:
		result = curr_total_fl + fl_lc;
		break;
	}

	return result;
}


/**
 * function for supporting LBMF/AEB and DCG+VS/L.
 * (MUST be used with g_equiv_min_fl_lc() & g_curr_total_fl_by_lut_id() function)
 *
 * => LBMF/AEB: using line count domain value.
 * => DCG+VS/L: auto switch to time domain value from line count domain.
 */
static inline unsigned int g_fll_tgt_req(const unsigned int idx,
	const unsigned int total_min_fl_lc, const unsigned int curr_tatal_fl,
	const unsigned int ref_idx,
	const struct frec_sen_mode_info_st *mode_info,
	const struct FrameRecord *curr_rec)
{
	const unsigned int multi_exp_type = mode_info->prop.multi_exp_type;
	unsigned int result = 0;

	switch (multi_exp_type) {
	case MULTI_EXP_TYPE_DCG_VSL:
	{
		const unsigned int lut_idx = g_lut_id_by_cascade_ref_idx(
			ref_idx, curr_rec->mode_exp_cnt);
		const unsigned int total_min_fl_us = convert2TotalTime(
			curr_rec->lineTimeInNs, total_min_fl_lc);

		/* unit/domain: time us */
		result = (total_min_fl_us > curr_tatal_fl)
			? (total_min_fl_us - curr_tatal_fl) : 0;
		/* unit/domain: line count */
		result = convert2LineCount(
			mode_info->cas_mode_info.lineTimeInNs[lut_idx], result);
	}
		break;
	case MULTI_EXP_TYPE_LBMF:
	default:
		result = (total_min_fl_lc > curr_tatal_fl)
			? (total_min_fl_lc - curr_tatal_fl) : 0;
		break;
	}

	return result;
}


static unsigned int calc_lut_fll_required_by_exp(const unsigned int idx,
	const unsigned int fll_ref_idx, const unsigned int exp_ref_idx,
	const unsigned int exp_lc,
	const struct frec_sen_mode_info_st *p_mode_info,
	const struct FrameRecord *curr_rec,
	const char *caller)
{
	const unsigned int margin_lc =
		g_margin_lc_by_lut_id(idx, fll_ref_idx, p_mode_info, curr_rec);
	const unsigned int based_min_fl_lc =
		g_based_fll_by_lut_id(idx, fll_ref_idx, p_mode_info, curr_rec);
	const unsigned int cit_loss_lc =
		g_cit_loss_by_lut_id(idx, fll_ref_idx, p_mode_info, curr_rec);
	const unsigned int exp_lc_mapped_for_lut =
		g_map_cit_lc_by_lut_id(idx, fll_ref_idx, exp_ref_idx, exp_lc,
			p_mode_info, curr_rec);
	unsigned int exp_req_fll, result;

	exp_req_fll = (exp_lc_mapped_for_lut + margin_lc + cit_loss_lc);
	result = (exp_req_fll > based_min_fl_lc) ? exp_req_fll : based_min_fl_lc;

#ifndef REDUCE_SEN_REC_LOG
	LOG_MUST(
		"[%s][%u][sidx:%u] result:%u(exp_req_fll:%u/exp_lc:(%u->%u)/ref_idx(fll:%u/exp:%u)/(margin:%u/based_min_fl_lc:%u/loss:%u))\n",
		caller,
		idx,
		fs_get_reg_sensor_idx(idx),
		result,
		exp_req_fll,
		exp_lc,
		exp_lc_mapped_for_lut,
		fll_ref_idx,
		exp_ref_idx,
		margin_lc,
		based_min_fl_lc,
		cit_loss_lc);
#endif

	return result;
}


static void frec_get_cascade_exp_fl_settings(const unsigned int idx,
	const struct FrameRecord *recs_arr[], const unsigned int recs_len,
	unsigned int *exp_cas_arr, unsigned int *fl_cas_arr,
	const unsigned int arr_len)
{
	unsigned int total_exp_cnt = 0, accumulated_exp_cnt = 0;
	unsigned int i, j;

	memset(exp_cas_arr, 0, sizeof(unsigned int) * arr_len);
	memset(fl_cas_arr, 0, sizeof(unsigned int) * arr_len);

	/* check if the arr len for using is sufficient */
	for (i = 0; i < recs_len; ++i) {
		const struct FrameRecord *rec = recs_arr[i];

		total_exp_cnt += rec->mode_exp_cnt;
	}
	/* error case check */
	if (unlikely(arr_len < total_exp_cnt)) {
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u/inf:%u), cascade array size is too short (%u < total_mode_exp_cnt:%u), return\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			arr_len,
			total_exp_cnt);
		return;
	}

	for (i = 0; i < recs_len; ++i) {
		const struct FrameRecord *rec = recs_arr[i];
		const unsigned int mode_exp_cnt = rec->mode_exp_cnt;
		const unsigned int order = rec->exp_order;

		/* copy each LUT CIT/FLL settings */
		for (j = 0; j < mode_exp_cnt; ++j) {
			const unsigned int ii = j + accumulated_exp_cnt;
			const int exp_idx =
				g_exp_order_idx_mapping(idx, order, mode_exp_cnt, j, __func__);

			if (unlikely(exp_idx < 0)) {
				LOG_MUST(
					"ERROR: [%u] ID:%#x(sidx:%u/inf:%u), exp_order_idx_map[%u][%u][%u]:%d(< 0), wants to write to idx:%u\n",
					idx,
					fs_get_reg_sensor_id(idx),
					fs_get_reg_sensor_idx(idx),
					fs_get_reg_sensor_inf_idx(idx),
					order,
					mode_exp_cnt,
					j,
					exp_idx,
					ii);
				return;
			}

			exp_cas_arr[ii] = rec->exp_lc_arr[exp_idx];

#ifdef FL_ARR_IN_LUT_ORDER
			fl_cas_arr[ii] = rec->fl_lc_arr[j];
#else
			fl_cas_arr[ii] = rec->fl_lc_arr[exp_idx];
#endif
		}

		accumulated_exp_cnt += mode_exp_cnt;
	}
}


static void frec_calc_target_fl_lc_arr_val_fdelay_2(const unsigned int idx,
	const struct FrameRecord *curr_rec, const struct FrameRecord *prev_rec,
	const unsigned int target_fl_lc,
	unsigned int fl_lc_arr[], const unsigned int arr_len)
{
	const struct FrameRecord *recs[2] = {prev_rec, curr_rec};
	const unsigned int mode_exp_cnt = curr_rec->mode_exp_cnt;
	struct frec_sen_mode_info_st mode_info = {0};
	unsigned int exp_cas[2*FS_HDR_MAX] = {0}, fl_cas[2*FS_HDR_MAX] = {0};
	unsigned int prev_total_fl = 0, lut_fll_0;
	unsigned int i;

	g_sen_mode_info(idx, &mode_info);
	frec_get_cascade_exp_fl_settings(
		idx, recs, 2, exp_cas, fl_cas, (2*FS_HDR_MAX));

	/* !!! mode related info in defferent frame should be the same !!! */
	/* first, setup basic min fl for shutter of current frec */
	/* => prepare info that will use on below flow */
	/* => setup each fl arr value for each LUT block */
	for (i = 0; (i < mode_exp_cnt && i < FS_HDR_MAX); ++i) {
		const unsigned int ref_idx = mode_exp_cnt + i; /* calc. ref idx */
		const unsigned int exp_lc = exp_cas[ref_idx]; /* get cit value by the ref idx */
		/* basic required fll for each lut/turn */
		const unsigned int req_fl_lc = calc_lut_fll_required_by_exp(idx,
			i, ref_idx, exp_lc, &mode_info, curr_rec, __func__);

		fl_lc_arr[i] = req_fl_lc;
	}

	/* then, keep or let FL of N+1 frame match to min FL or target FL */
	/* => setup each fl arr value for each LUT block */
	for (i = 1; (i < mode_exp_cnt && i < FS_HDR_MAX); ++i) {
		/* get cit & fll value by the ref idx */
		const unsigned int exp_lc = exp_cas[i];
		const unsigned int fl_lc = fl_cas[i];
		/* basic required fll for each lut/turn */
		const unsigned int req_fl_lc = calc_lut_fll_required_by_exp(idx,
			i, i, exp_lc, &mode_info, curr_rec, __func__);
		/* basic output for each turn */
		unsigned int min_fl_lc = 0;

		/*
		 * for preventing any issue of wrong fl values in LUT from drv,
		 * check correct FL value again manually.
		 */
		min_fl_lc = (fl_lc > req_fl_lc) ? fl_lc : req_fl_lc;

		prev_total_fl = g_curr_total_fl_by_lut_id(idx,
			min_fl_lc, prev_total_fl,
			i, &mode_info, curr_rec);
	}
	/* => modify the 1st LUT-FLL / [0] value (match to N+1 type) */
	lut_fll_0 = g_fll_tgt_req(idx,
		target_fl_lc, prev_total_fl, i, &mode_info, curr_rec);
	lut_fll_0 = (lut_fll_0 != 0) ? lut_fll_0 : fl_lc_arr[0];
	fl_lc_arr[0] = (lut_fll_0 > fl_lc_arr[0]) ? lut_fll_0 : fl_lc_arr[0];

	LOG_INF(
		"[%u][sidx:%u] tar_fl_lc:%u, fl_lc_arr:(%u/%u/%u/%u/%u), p:((a:%u/m:%u(%u,%u),%u/%u/%u/%u/%u,%u/%u/%u/%u/%u),mar(%u/r:%u),roL:%u)/c:((a:%u/m:%u(%u,%u),%u/%u/%u/%u/%u),mar(%u/r:%u),roL:%u), cas(lineT(%u/%u/%u/%u/%u),mar(%u/%u/%u/%u/%u,r(%u/%u/%u/%u/%u)))\n",
		idx,
		fs_get_reg_sensor_idx(idx),
		target_fl_lc,
		fl_lc_arr[0],
		fl_lc_arr[1],
		fl_lc_arr[2],
		fl_lc_arr[3],
		fl_lc_arr[4],
		prev_rec->ae_exp_cnt, prev_rec->mode_exp_cnt,
		prev_rec->m_exp_type, prev_rec->exp_order,
		prev_rec->exp_lc_arr[0],
		prev_rec->exp_lc_arr[1],
		prev_rec->exp_lc_arr[2],
		prev_rec->exp_lc_arr[3],
		prev_rec->exp_lc_arr[4],
		prev_rec->fl_lc_arr[0],
		prev_rec->fl_lc_arr[1],
		prev_rec->fl_lc_arr[2],
		prev_rec->fl_lc_arr[3],
		prev_rec->fl_lc_arr[4],
		prev_rec->margin_lc, prev_rec->read_margin_lc,
		prev_rec->readout_len_lc,
		curr_rec->ae_exp_cnt, curr_rec->mode_exp_cnt,
		curr_rec->m_exp_type, curr_rec->exp_order,
		curr_rec->exp_lc_arr[0],
		curr_rec->exp_lc_arr[1],
		curr_rec->exp_lc_arr[2],
		curr_rec->exp_lc_arr[3],
		curr_rec->exp_lc_arr[4],
		curr_rec->margin_lc, curr_rec->read_margin_lc,
		curr_rec->readout_len_lc,
		mode_info.cas_mode_info.lineTimeInNs[0],
		mode_info.cas_mode_info.lineTimeInNs[1],
		mode_info.cas_mode_info.lineTimeInNs[2],
		mode_info.cas_mode_info.lineTimeInNs[3],
		mode_info.cas_mode_info.lineTimeInNs[4],
		mode_info.cas_mode_info.margin_lc[0],
		mode_info.cas_mode_info.margin_lc[1],
		mode_info.cas_mode_info.margin_lc[2],
		mode_info.cas_mode_info.margin_lc[3],
		mode_info.cas_mode_info.margin_lc[4],
		mode_info.cas_mode_info.read_margin_lc[0],
		mode_info.cas_mode_info.read_margin_lc[1],
		mode_info.cas_mode_info.read_margin_lc[2],
		mode_info.cas_mode_info.read_margin_lc[3],
		mode_info.cas_mode_info.read_margin_lc[4]);
}


static void frec_calc_target_fl_lc_arr_val_fdelay_3(const unsigned int idx,
	const struct FrameRecord *curr_rec,
	const unsigned int target_fl_lc,
	unsigned int fl_lc_arr[], const unsigned int arr_len)
{
	const struct FrameRecord *recs[2] = {curr_rec, curr_rec};
	const unsigned int curr_mode_exp_cnt = curr_rec->mode_exp_cnt;
	struct frec_sen_mode_info_st mode_info = {0};
	unsigned int exp_cas[2*FS_HDR_MAX] = {0}, fl_cas[2*FS_HDR_MAX] = {0};
	unsigned int equiv_min_fl = 0;
	unsigned int i;

	g_sen_mode_info(idx, &mode_info);
	frec_get_cascade_exp_fl_settings(
		idx, recs, 2, exp_cas, fl_cas, (2*FS_HDR_MAX));

	for (i = 0; (i < curr_mode_exp_cnt && i < FS_HDR_MAX); ++i) {
		const unsigned int ref_idx = i + 1; /* calc. ref idx */
		const unsigned int exp_lc = exp_cas[ref_idx]; /* get cit value by the ref idx */
		/* basic required fll for each lut/turn */
		const unsigned int req_fl_lc = calc_lut_fll_required_by_exp(idx,
			i, ref_idx, exp_lc, &mode_info, curr_rec, __func__);

		fl_lc_arr[i] = req_fl_lc;

		/* keep total fps match to target fps */
		if (ref_idx == curr_mode_exp_cnt) {
			const unsigned int temp_min_fl_lc =
				g_fll_tgt_req(idx,
					target_fl_lc, equiv_min_fl,
					i, &mode_info, curr_rec);

			fl_lc_arr[i] = (temp_min_fl_lc > fl_lc_arr[i])
				? temp_min_fl_lc : fl_lc_arr[i];
		}
		equiv_min_fl = g_curr_total_fl_by_lut_id(idx,
			fl_lc_arr[i], equiv_min_fl,
			i, &mode_info, curr_rec);
	}

	LOG_INF(
		"[%u][sidx:%u] tar_fl_lc:%u, fl_lc_arr:(%u/%u/%u/%u/%u), c:((a:%u/m:%u(%u,%u),%u/%u/%u/%u/%u),mar(%u/r:%u),roL:%u,lineT:%u), cas(lineT(%u/%u/%u/%u/%u),mar(%u/%u/%u/%u/%u,r(%u/%u/%u/%u/%u)))\n",
		idx,
		fs_get_reg_sensor_idx(idx),
		target_fl_lc,
		fl_lc_arr[0],
		fl_lc_arr[1],
		fl_lc_arr[2],
		fl_lc_arr[3],
		fl_lc_arr[4],
		curr_rec->ae_exp_cnt, curr_rec->mode_exp_cnt,
		curr_rec->m_exp_type, curr_rec->exp_order,
		curr_rec->exp_lc_arr[0],
		curr_rec->exp_lc_arr[1],
		curr_rec->exp_lc_arr[2],
		curr_rec->exp_lc_arr[3],
		curr_rec->exp_lc_arr[4],
		curr_rec->margin_lc, curr_rec->read_margin_lc,
		curr_rec->readout_len_lc,
		curr_rec->lineTimeInNs,
		mode_info.cas_mode_info.lineTimeInNs[0],
		mode_info.cas_mode_info.lineTimeInNs[1],
		mode_info.cas_mode_info.lineTimeInNs[2],
		mode_info.cas_mode_info.lineTimeInNs[3],
		mode_info.cas_mode_info.lineTimeInNs[4],
		mode_info.cas_mode_info.margin_lc[0],
		mode_info.cas_mode_info.margin_lc[1],
		mode_info.cas_mode_info.margin_lc[2],
		mode_info.cas_mode_info.margin_lc[3],
		mode_info.cas_mode_info.margin_lc[4],
		mode_info.cas_mode_info.read_margin_lc[0],
		mode_info.cas_mode_info.read_margin_lc[1],
		mode_info.cas_mode_info.read_margin_lc[2],
		mode_info.cas_mode_info.read_margin_lc[3],
		mode_info.cas_mode_info.read_margin_lc[4]);
}


void frec_g_valid_min_fl_arr_val_for_lut(const unsigned int idx,
	const struct FrameRecord *curr_rec,
	const unsigned int target_fl_lc,
	unsigned int fl_lc_arr[], const unsigned int arr_len)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;
	if (unlikely(!chk_if_sen_fdelay_is_valid(idx, pfrec, __func__)))
		return;
	if (unlikely(fl_lc_arr == NULL))
		return;
	/* ONLY for mode that using LUT */
	if (unlikely((curr_rec->mode_exp_cnt <= 1)
			|| (!frec_chk_if_lut_is_used(curr_rec->m_exp_type)))) {
		LOG_MUST(
			"WARNING: [%u] ID:%#x(sidx:%u/inf:%u), sensor curr_mode_exp_cnt:%u, multi_exp_type:%u(STG:%u/LBMF:%u/DCGVSL:%u) is not for LUT gen, return\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			curr_rec->mode_exp_cnt,
			curr_rec->m_exp_type,
			MULTI_EXP_TYPE_STG,
			MULTI_EXP_TYPE_LBMF,
			MULTI_EXP_TYPE_DCG_VSL);
		return;
	}

	memset(fl_lc_arr, 0, sizeof(unsigned int) * arr_len);

	switch (pfrec->fl_act_delay) {
	case 3:
	{
		/* N+2 type, e.g., general SONY sensor */
		frec_calc_target_fl_lc_arr_val_fdelay_3(idx,
			curr_rec, target_fl_lc, fl_lc_arr, arr_len);
	}
		break;
	case 2:
	{
		/* N+1 type, e.g., non-SONY sensor */
		const struct FrameRecord *prev_rec = NULL;
		const unsigned int curr_idx = FS_ATOMIC_READ(&pfrec->depth_idx);
		const unsigned int target_idx = RING_BACK(curr_idx, 1);

		prev_rec = &pfrec->frame_recs[target_idx];
		frec_calc_target_fl_lc_arr_val_fdelay_2(idx,
			curr_rec, prev_rec, target_fl_lc, fl_lc_arr, arr_len);
	}
		break;
	}
}
/*----------------------------------------------------------------------------*/
/* NDOL / LB-MF utilities functions                                           */
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
// NDOL / LB-MF functions
/*----------------------------------------------------------------------------*/
static void frec_calc_lut_read_offset_by_fdelay(const unsigned int idx,
	const struct FrameRecord *curr_rec, const unsigned int fdelay,
	unsigned int *p_next_pr_rd_offset_lc,
	unsigned int *p_next_pr_rd_offset_us)
{
	const struct FrameRecord *recs[1] = {curr_rec};
	const unsigned int order = curr_rec->exp_order;
	const unsigned int curr_mode_exp_cnt = curr_rec->mode_exp_cnt;
	struct frec_sen_mode_info_st mode_info = {0};
	unsigned int exp_cas[FS_HDR_MAX] = {0}, fl_cas[FS_HDR_MAX] = {0};
	unsigned int bias = 0;
	unsigned int i;

	memset(p_next_pr_rd_offset_lc, 0, sizeof(unsigned int) * FS_HDR_MAX);
	memset(p_next_pr_rd_offset_us, 0, sizeof(unsigned int) * FS_HDR_MAX);

	g_sen_mode_info(idx, &mode_info);
	frec_get_cascade_exp_fl_settings(
		idx, recs, 1, exp_cas, fl_cas, (FS_HDR_MAX));

	for (i = 0; (i < (curr_mode_exp_cnt - 1) && i < (FS_HDR_MAX - 1)); ++i) {
		/* calc. ref idx */
		const unsigned int exp_ref_idx = (i + 1);
		const unsigned int fl_ref_idx = (fdelay == 2) ? (i + 1) : i;
		/* get cit & fll value by the ref idx */
		const unsigned int exp_lc = exp_cas[exp_ref_idx];
		const unsigned int fl_lc = fl_cas[fl_ref_idx];
		/* basic required fll for each lut/turn */
		const unsigned int req_fl_lc = calc_lut_fll_required_by_exp(idx,
			fl_ref_idx, exp_ref_idx, exp_lc,
			&mode_info, curr_rec, __func__);
		/* info for output result */
		const int exp_id = g_exp_order_idx_mapping(idx,
			order, curr_mode_exp_cnt, exp_ref_idx, __func__);
		unsigned int min_fl_lc = 0;

		if (unlikely(exp_id < 0))
			return;

		min_fl_lc = (fl_lc > req_fl_lc) ? fl_lc : req_fl_lc;

		bias = g_curr_total_fl_by_lut_id(idx,
			min_fl_lc, bias, fl_ref_idx, &mode_info, curr_rec);

		p_next_pr_rd_offset_lc[exp_id] =
			g_equiv_min_fl_lc(idx, bias, &mode_info, curr_rec);
		p_next_pr_rd_offset_us[exp_id] =
			convert2TotalTime(
				curr_rec->lineTimeInNs,
				p_next_pr_rd_offset_lc[exp_id]);

#ifndef REDUCE_SEN_REC_LOG
		LOG_MUST(
			"[%u][sidx:%u] fdelay:%u, lineT:%u, cnt:%u, i:%u(ref_idx:(exp:%u/fl:%u)), exp_lc:%u, (reqFL:%u,FL:%u) => r_offset[exp_id:%d]:%u(%u)\n",
			idx,
			fs_get_reg_sensor_idx(idx),
			fdelay, curr_rec->lineTimeInNs, curr_mode_exp_cnt,
			i, exp_ref_idx, fl_ref_idx,
			exp_lc, fl_lc,
			exp_id,
			p_next_pr_rd_offset_us[exp_id],
			p_next_pr_rd_offset_lc[exp_id]);
#endif
	}
}


static unsigned int frec_calc_lut_valid_min_fl_lc_for_shutters_by_fdelay(
	const unsigned int idx,
	const struct FrameRecord *curr_rec, const struct FrameRecord *prev_rec,
	const unsigned int fdelay, const unsigned int target_min_fl_lc)
{
	const struct FrameRecord *recs[2] = {prev_rec, curr_rec};
	const unsigned int mode_exp_cnt = curr_rec->mode_exp_cnt;
	const unsigned int total_min_fl_lc = (target_min_fl_lc > 0) ? target_min_fl_lc : 0;
	struct frec_sen_mode_info_st mode_info = {0};
	unsigned int exp_cas[2*FS_HDR_MAX] = {0}, fl_cas[2*FS_HDR_MAX] = {0};
	unsigned int equiv_min_fl = 0;
	unsigned int i;
	unsigned int log_en = _FS_LOG_ENABLED(LOG_SEN_REC_CALC_LBMF_VALID_MIN_FL_DUMP);
	char *log_buf = NULL;
	int len = 0, ret;

	g_sen_mode_info(idx, &mode_info);

	if (unlikely(log_en)) {
		/* prepare for log print */
		ret = alloc_log_buf(LOG_BUF_STR_LEN, &log_buf);
		if (unlikely(ret != 0)) {
			LOG_MUST("ERROR: log_buf allocate memory failed\n");
			/* for disable snprint msg */
			log_en = 0;
		}
	}
	if (unlikely(log_en)) {
		FS_SNPRF(LOG_BUF_STR_LEN, log_buf, len,
			"[%u][sidx:%u] tar_minFL:%u(%u):(tar:%u/p:%u/c:%u),fdelay:%u,cnt:%u,lineT:%u,ro(L:%u,mar:%u),",
			idx,
			fs_get_reg_sensor_idx(idx),
			convert2TotalTime(
				curr_rec->lineTimeInNs, total_min_fl_lc),
			total_min_fl_lc,
			target_min_fl_lc,
			prev_rec->framelength_lc, curr_rec->framelength_lc,
			fdelay, mode_exp_cnt,
			curr_rec->lineTimeInNs,
			curr_rec->readout_len_lc, curr_rec->read_margin_lc);

		len = frec_snprf_sen_mode_info(&mode_info, LOG_BUF_STR_LEN, log_buf, len);
	}

	frec_get_cascade_exp_fl_settings(
		idx, recs, 2, exp_cas, fl_cas, (2*FS_HDR_MAX));

	/* check each CIT/FLL settings in LUT */
	for (i = 0; (i < mode_exp_cnt && i < FS_HDR_MAX); ++i) {
		/* calc. ref idx */
		const unsigned int exp_ref_idx = (i + 1);
		const unsigned int fl_ref_idx = (fdelay == 2) ? (i + 1) : i;
		/* get cit & fll value by the ref idx */
		const unsigned int exp_lc = exp_cas[exp_ref_idx];
		const unsigned int fl_lc = fl_cas[fl_ref_idx];
		/* basic required fll for each lut/turn */
		const unsigned int req_fl_lc = calc_lut_fll_required_by_exp(idx,
			fl_ref_idx, exp_ref_idx, exp_lc,
			&mode_info, curr_rec, __func__);
		unsigned int min_fl_lc = 0;

		/* max(based and valid min fl, s+m) */
		min_fl_lc = (fl_lc > req_fl_lc) ? fl_lc : req_fl_lc;

		/* keep total fps match to max fps */
		if (exp_ref_idx == mode_exp_cnt) {
			const unsigned int temp_min_fl_lc =
				g_fll_tgt_req(idx,
					total_min_fl_lc, equiv_min_fl,
					fl_ref_idx, &mode_info, curr_rec);

			min_fl_lc = (temp_min_fl_lc > min_fl_lc)
				? temp_min_fl_lc : min_fl_lc;
		}
		equiv_min_fl = g_curr_total_fl_by_lut_id(idx,
			min_fl_lc, equiv_min_fl,
			fl_ref_idx, &mode_info, curr_rec);

		if (unlikely(log_en)) {
			FS_SNPRF(LOG_BUF_STR_LEN, log_buf, len,
				", i:%u(ref_idx:(exp:%u/fl:%u),exp_lc:%u,equiv_minFL:%u(minFL:%u(reqFL:%u,FL:%u)))",
				i, exp_ref_idx, fl_ref_idx, exp_lc,
				equiv_min_fl, min_fl_lc, req_fl_lc, fl_lc);
		}
	}

	if (unlikely(log_en)) {
		LOG_INF("%s\n", log_buf);
		if (likely(log_buf != NULL))
			FS_FREE(log_buf);
	}

	return g_equiv_min_fl_lc(idx, equiv_min_fl, &mode_info, curr_rec);
}


static void frec_calc_lut_read_offset(const unsigned int idx,
	const struct FrameRecord *curr_rec,
	unsigned int *p_next_pr_rd_offset_lc,
	unsigned int *p_next_pr_rd_offset_us)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;
	if (unlikely(!chk_if_sen_fdelay_is_valid(idx, pfrec, __func__)))
		return;

	frec_calc_lut_read_offset_by_fdelay(idx, curr_rec, pfrec->fl_act_delay,
		p_next_pr_rd_offset_lc, p_next_pr_rd_offset_us);
}


static unsigned int frec_calc_lut_valid_min_fl_lc_for_shutters(
	const unsigned int idx,
	const struct FrameRecord *curr_rec, const struct FrameRecord *prev_rec,
	const unsigned int target_min_fl_lc)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	unsigned int equiv_min_fl_lc = 0;

	/* error handle */
	if (unlikely(pfrec == NULL))
		return curr_rec->framelength_lc;
	if (unlikely(!chk_if_sen_fdelay_is_valid(idx, pfrec, __func__)))
		return curr_rec->framelength_lc;

	equiv_min_fl_lc = frec_calc_lut_valid_min_fl_lc_for_shutters_by_fdelay(
		idx, curr_rec, prev_rec, pfrec->fl_act_delay, target_min_fl_lc);

	return equiv_min_fl_lc;
}
/*----------------------------------------------------------------------------*/
// !!! END !!! ---  NDOL / LB-MF functions
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/* FDOL, DOL / Stagger functions                                              */
/*----------------------------------------------------------------------------*/
static inline unsigned int is_dol_stg_type(const struct FrameRecord *curr_rec)
{
	return (curr_rec->m_exp_type == MULTI_EXP_TYPE_STG
			&& curr_rec->dol_type == STAGGER_DOL_TYPE_DOL)
		? 1 : 0;
}


static void frec_calc_stg_read_offset(const unsigned int idx,
	const struct FrameRecord *curr_rec,
	unsigned int *p_next_pr_rd_offset_lc,
	unsigned int *p_next_pr_rd_offset_us)
{
	const unsigned int curr_mode_exp_cnt = curr_rec->mode_exp_cnt;
	const unsigned int margin_lc_per_exp = (curr_mode_exp_cnt != 0)
		? (curr_rec->margin_lc / curr_mode_exp_cnt)
		: curr_rec->margin_lc;
	unsigned int bias = 0;
	unsigned int i;

	memset(p_next_pr_rd_offset_lc, 0, sizeof(unsigned int) * FS_HDR_MAX);
	memset(p_next_pr_rd_offset_us, 0, sizeof(unsigned int) * FS_HDR_MAX);

	for (i = 1; i < curr_mode_exp_cnt; ++i) {
		int hdr_idx = hdr_exp_idx_map[curr_mode_exp_cnt][i];

		if (unlikely(hdr_idx < 0)) {
			LOG_MUST(
				"ERROR: [%u] ID:%#x(sidx:%u/inf:%u), hdr_exp_idx_map[%u][%u] = %d\n",
				idx,
				fs_get_reg_sensor_id(idx),
				fs_get_reg_sensor_idx(idx),
				fs_get_reg_sensor_inf_idx(idx),
				curr_mode_exp_cnt,
				i,
				hdr_idx);
			return;
		}

		bias += margin_lc_per_exp + curr_rec->exp_lc_arr[hdr_idx];
		p_next_pr_rd_offset_lc[hdr_idx] = bias;
		p_next_pr_rd_offset_us[hdr_idx] =
			convert2TotalTime(
				curr_rec->lineTimeInNs,
				p_next_pr_rd_offset_lc[hdr_idx]);

#ifndef REDUCE_SEN_REC_LOG
		LOG_MUST(
			"[%u] ID:%#x(sidx:%u/inf:%u), i:%u/cnt:%u/hdr_idx:%d, exp_lc:%u, margin_per_exp:%u => r_offset:%u(%u)\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			i, curr_mode_exp_cnt, hdr_idx,
			curr_rec->exp_lc_arr[hdr_idx],
			margin_lc_per_exp,
			p_next_pr_rd_offset_us[hdr_idx],
			p_next_pr_rd_offset_lc[hdr_idx]);
#endif
	}

#ifndef REDUCE_SEN_REC_LOG
	LOG_MUST(
		"[%u] ID:%#x(sidx:%u/inf:%u), mode_exp_cnt:%u, r_offset(%u(%u)/%u(%u)/%u(%u)/%u(%u))\n",
		idx,
		fs_get_reg_sensor_id(idx),
		fs_get_reg_sensor_idx(idx),
		fs_get_reg_sensor_inf_idx(idx),
		curr_mode_exp_cnt,
		p_next_pr_rd_offset_us[1],
		p_next_pr_rd_offset_lc[1],
		p_next_pr_rd_offset_us[2],
		p_next_pr_rd_offset_lc[2],
		p_next_pr_rd_offset_us[3],
		p_next_pr_rd_offset_lc[3],
		p_next_pr_rd_offset_us[4],
		p_next_pr_rd_offset_lc[4]);
#endif
}


static unsigned int frec_chk_stg_fl_rule_1(const unsigned int idx,
	const struct FrameRecord *curr_rec, const struct FrameRecord *prev_rec)
{
	const unsigned int prev_mode_exp_cnt = prev_rec->mode_exp_cnt;
	unsigned int shutter_margin_lc;
	unsigned int i;

	/* check for auto-extended rule */
	shutter_margin_lc = curr_rec->exp_lc_arr[0];
	for (i = 1; i < prev_mode_exp_cnt; ++i) {
		int hdr_idx = hdr_exp_idx_map[prev_mode_exp_cnt][i];

		if (unlikely(hdr_idx < 0)) {
			LOG_MUST(
				"ERROR: [%u] ID:%#x(sidx:%u/inf:%u), hdr_exp_idx_map[%u][%u] = %d\n",
				idx,
				fs_get_reg_sensor_id(idx),
				fs_get_reg_sensor_idx(idx),
				fs_get_reg_sensor_inf_idx(idx),
				prev_mode_exp_cnt,
				i,
				hdr_idx);
			return 0;
		}

		shutter_margin_lc += prev_rec->exp_lc_arr[hdr_idx];
	}
	shutter_margin_lc += prev_rec->margin_lc;

	return shutter_margin_lc;
}


static unsigned int frec_chk_stg_fl_rule_2(const unsigned int idx,
	const struct FrameRecord *curr_rec, const struct FrameRecord *prev_rec)
{
	const unsigned int curr_mode_exp_cnt = curr_rec->mode_exp_cnt;
	const unsigned int readout_len_lc = curr_rec->readout_len_lc;
	const unsigned int read_margin_lc = curr_rec->read_margin_lc;
	unsigned int readout_fl_lc = 0, readout_min_fl_lc = 0;
	unsigned int i;
	int read_offset_diff = 0;

	/* error case highlight */
	if (unlikely((readout_len_lc == 0) || (read_margin_lc == 0))) {
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u/inf:%u), sensor driver's readout_length:%u, read_margin:%u is/are not valid, plz check sensor driver for getting correct value\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			readout_len_lc,
			read_margin_lc);
		return 0;
	}

	/* check trigger auto-extended for preventing readout overlap */
	for (i = 1; i < curr_mode_exp_cnt; ++i) {
		int hdr_idx = hdr_exp_idx_map[curr_mode_exp_cnt][i];

		if (unlikely(hdr_idx < 0)) {
			LOG_MUST(
				"ERROR: [%u] ID:%#x(sidx:%u/inf:%u), hdr_exp_idx_map[%u][%u] = %d\n",
				idx,
				fs_get_reg_sensor_id(idx),
				fs_get_reg_sensor_idx(idx),
				fs_get_reg_sensor_inf_idx(idx),
				curr_mode_exp_cnt,
				i,
				hdr_idx);
			return 0;
		}

		read_offset_diff +=
			prev_rec->exp_lc_arr[hdr_idx] -
			curr_rec->exp_lc_arr[hdr_idx];

		readout_fl_lc = (read_offset_diff > 0)
			? (readout_len_lc + read_margin_lc + read_offset_diff)
			: (readout_len_lc + read_margin_lc);

		if (readout_min_fl_lc < readout_fl_lc)
			readout_min_fl_lc = readout_fl_lc;
	}

	return readout_min_fl_lc;
}


static unsigned int frec_chk_dol_stg_fl_rule(const unsigned int idx,
	const struct FrameRecord *curr_rec, const struct FrameRecord *prev_rec)
{
	const unsigned int prev_mode_exp_cnt = prev_rec->mode_exp_cnt;
	const unsigned int margin_lc_per_exp = (prev_mode_exp_cnt != 0)
		? (prev_rec->margin_lc / prev_mode_exp_cnt)
		: prev_rec->margin_lc;
	unsigned int last_exp_read_offset = 0, min_fl_lc_for_dol = 0;
	unsigned int i;

	/* chech stagger type (is it DOL) */
	if (is_dol_stg_type(curr_rec) == 0)
		return 0;

	/* calculate read offset by values in frame records */
	for (i = 1; i < prev_mode_exp_cnt; ++i) {
		const int hdr_idx = g_exp_order_idx_mapping(idx,
			prev_rec->exp_order, prev_mode_exp_cnt, i, __func__);

		if (unlikely(hdr_idx < 0))
			return 0;
		last_exp_read_offset +=
			(margin_lc_per_exp + prev_rec->exp_lc_arr[hdr_idx]);
	}

	/* for DOL rule */
	min_fl_lc_for_dol = last_exp_read_offset +
		(prev_rec->readout_len_lc + prev_rec->min_vblank_lc);

	LOG_INF_CAT(LOG_SEN_REC,
		"[%u] ID:%#x(sidx:%u/inf:%u), dol_min_fl_lc:%u(offset:%u + rout_L:%u + min_vb:%u))\n",
		idx,
		fs_get_reg_sensor_id(idx),
		fs_get_reg_sensor_idx(idx),
		fs_get_reg_sensor_inf_idx(idx),
		min_fl_lc_for_dol,
		last_exp_read_offset,
		prev_rec->readout_len_lc,
		prev_rec->min_vblank_lc);

	return min_fl_lc_for_dol;
}


static unsigned int frec_chk_stg_fl_sw_rule_1(const unsigned int idx,
	const struct FrameRecord *curr_rec)
{
	const unsigned int curr_mode_exp_cnt = curr_rec->mode_exp_cnt;
	unsigned int shutter_margin_lc = 0;
	unsigned int i;

	/* check for preventing w/o auto-extended sensor exp not enough */
	for (i = 0; i < curr_mode_exp_cnt; ++i) {
		int hdr_idx = hdr_exp_idx_map[curr_mode_exp_cnt][i];

		if (unlikely(hdr_idx < 0)) {
			LOG_MUST(
				"ERROR: [%u] ID:%#x(sidx:%u/inf:%u), hdr_exp_idx_map[%u][%u] = %d\n",
				idx,
				fs_get_reg_sensor_id(idx),
				fs_get_reg_sensor_idx(idx),
				fs_get_reg_sensor_inf_idx(idx),
				curr_mode_exp_cnt,
				i,
				hdr_idx);
			return 0;
		}

		shutter_margin_lc += curr_rec->exp_lc_arr[hdr_idx];
	}
	shutter_margin_lc += curr_rec->margin_lc;

	return shutter_margin_lc;
}


static unsigned int frec_calc_stg_valid_min_fl_lc_for_shutters(
	const unsigned int idx,
	const struct FrameRecord *curr_rec, const struct FrameRecord *prev_rec)
{
	unsigned int result_1, result_2, result_3;
	unsigned int min_fl_lc = 0;

	/* ONLY when stagger/HDR mode ===> mode exp cnt > 1 */
	if (unlikely(curr_rec->mode_exp_cnt <= 1))
		return 0;

	/* only take HW needed min frame length for shutters into account */
	result_1 = frec_chk_stg_fl_rule_1(idx, curr_rec, prev_rec);
	result_2 = frec_chk_stg_fl_rule_2(idx, curr_rec, prev_rec);
	result_3 = frec_chk_dol_stg_fl_rule(idx, curr_rec,prev_rec);

	/**
	 * Coverity scan...
	 * The condition 'min_fl_lc > result_1' cannot be true.
	 * => the value of 'min_fl_lc' is assigned/initialized to zero.
	 */
	/* min_fl_lc = (min_fl_lc > result_1) ? min_fl_lc : result_1; */
	min_fl_lc = result_1;
	min_fl_lc = (min_fl_lc > result_2) ? min_fl_lc : result_2;
	min_fl_lc = (min_fl_lc > result_3) ? min_fl_lc : result_3;

	return min_fl_lc;
}
/*----------------------------------------------------------------------------*/
// !!! END !!! ---  FDOL / Stagger functions
/*----------------------------------------------------------------------------*/


static void refresh_next_predict_eof_offset(const unsigned int idx,
	struct FrameRecorder *pfrec,
	const struct FrameRecord *p_frame_rec)
{
	const unsigned int m_exp_cnt = p_frame_rec->mode_exp_cnt;
	const unsigned int m_exp_order = p_frame_rec->exp_order;
	unsigned int i;

	memset(pfrec->next_predicted_eof_offset_us, 0,
		sizeof(unsigned int) * FS_HDR_MAX);

	for (i = 0; (i < m_exp_cnt && i < FS_HDR_MAX); ++i) {
		const int exp_idx = g_exp_order_idx_mapping(
			idx, m_exp_order, m_exp_cnt, i, __func__);
		unsigned int rout_us = 0;

		if (unlikely(exp_idx < 0))
			break;

		switch (p_frame_rec->m_exp_type) {
		case MULTI_EXP_TYPE_DCG_VSL:
		{
			const unsigned int lut_idx =
				g_lut_id_by_cascade_ref_idx(i, m_exp_cnt);
			struct frec_sen_mode_info_st mode_info = {0};
			unsigned int lineTimeInNs;

			g_sen_mode_info(idx, &mode_info);
			lineTimeInNs =
				mode_info.cas_mode_info.lineTimeInNs[lut_idx];

			rout_us = convert2TotalTime(
				lineTimeInNs,
				p_frame_rec->readout_len_lc);
		}
			break;
		case MULTI_EXP_TYPE_LBMF:
		case MULTI_EXP_TYPE_STG:
		default:
			rout_us = p_frame_rec->readout_time_us;
			break;
		}

		pfrec->next_predicted_eof_offset_us[i] =
			pfrec->next_predicted_rd_offset_us[exp_idx] + rout_us;
	}
}


static unsigned int frec_calc_valid_min_fl_lc_for_shutters(
	const unsigned int idx, const unsigned int fdelay,
	const struct FrameRecord *curr_rec, const struct FrameRecord *prev_rec,
	const unsigned int target_min_fl_lc)
{
	const unsigned int m_exp_type = curr_rec->m_exp_type;
	unsigned int min_fl_lc = 0;

	/* multi-exp / HDR sensor => mode exp cnt > 1 */
	if (curr_rec->mode_exp_cnt > 1) {
		switch (m_exp_type) {
		case MULTI_EXP_TYPE_DCG_VSL:
		case MULTI_EXP_TYPE_LBMF:
			min_fl_lc = frec_calc_lut_valid_min_fl_lc_for_shutters(
				idx, curr_rec, prev_rec, target_min_fl_lc);
			break;
		case MULTI_EXP_TYPE_STG:
		default:
		{
			/* ONLY when stagger/HDR ===> mode exp cnt > 1 */
			min_fl_lc = frec_calc_stg_valid_min_fl_lc_for_shutters(
				idx, curr_rec, prev_rec);

			/* check N+1 type for protection not per-frame ctrl */
			if (fdelay == 2) {
				const unsigned int temp_min_fl_lc =
					frec_chk_stg_fl_sw_rule_1(idx, curr_rec);

				min_fl_lc = (min_fl_lc > temp_min_fl_lc)
					? min_fl_lc : temp_min_fl_lc;
			}
		}
			break;
		}
	} else {
		/* 1-exp / normal sensor => mode exp cnt = 1 */
		min_fl_lc = curr_rec->shutter_lc + curr_rec->margin_lc;
	}

	return min_fl_lc;
}


static void frec_predict_shutters_read_offset_by_curr_rec(const unsigned int idx,
	const struct FrameRecord *curr_rec,
	unsigned int *p_next_pr_rd_offset_lc,
	unsigned int *p_next_pr_rd_offset_us)
{
	const unsigned int m_exp_type = curr_rec->m_exp_type;
	const unsigned int curr_mode_exp_cnt = curr_rec->mode_exp_cnt;

	/* not multi-exp sensor mode */
	if (curr_mode_exp_cnt <= 1) {
		memset(p_next_pr_rd_offset_lc, 0,
			sizeof(unsigned int) * FS_HDR_MAX);
		memset(p_next_pr_rd_offset_us, 0,
			sizeof(unsigned int) * FS_HDR_MAX);
		return;
	}

	switch (m_exp_type) {
	case MULTI_EXP_TYPE_DCG_VSL:
	case MULTI_EXP_TYPE_LBMF:
		frec_calc_lut_read_offset(idx, curr_rec,
			p_next_pr_rd_offset_lc,
			p_next_pr_rd_offset_us);
		break;
	case MULTI_EXP_TYPE_STG:
	default:
		frec_calc_stg_read_offset(idx, curr_rec,
			p_next_pr_rd_offset_lc,
			p_next_pr_rd_offset_us);
		break;
	}
}


static unsigned int frec_predict_fl_lc_by_curr_rec(const unsigned int idx,
	const struct FrameRecord *curr_rec, const enum predicted_fl_label label)
{
	const struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	const struct FrameRecord *prev_rec = NULL;
	unsigned int min_fl_lc, next_fl_lc;

	/* error handle */
	if (unlikely(pfrec == NULL))
		return curr_rec->framelength_lc;
	if (unlikely(!chk_if_sen_fdelay_is_valid(idx, pfrec, __func__)))
		return curr_rec->framelength_lc;

	/* check case get corresponding frame record pointer */
	switch (label) {
	case PREDICT_NEXT_FL:
	{
		const unsigned int curr_idx = FS_ATOMIC_READ(&pfrec->depth_idx);
		const unsigned int target_idx = RING_BACK(curr_idx, 1);

		prev_rec = &pfrec->frame_recs[target_idx];
	}
		break;
	case PREDICT_STABLE_FL:
	default:
		prev_rec = curr_rec;
		break;
	}

	min_fl_lc = frec_calc_valid_min_fl_lc_for_shutters(
		idx, pfrec->fl_act_delay, curr_rec, prev_rec, 0);

	switch (pfrec->fl_act_delay) {
	case 3:
		/* N+2 type, e.g., general SONY sensor */
		next_fl_lc = (prev_rec->framelength_lc > min_fl_lc)
			? prev_rec->framelength_lc : min_fl_lc;
		break;
	case 2:
		/* N+1 type, e.g., non-SONY sensor */
		if (frec_chk_if_lut_is_used(curr_rec->m_exp_type))
			next_fl_lc = min_fl_lc;
		else
			next_fl_lc = (min_fl_lc > curr_rec->framelength_lc)
				? min_fl_lc : curr_rec->framelength_lc;
		break;
	}

	return next_fl_lc;
}


void frec_get_predicted_frame_length_info(const unsigned int idx,
	const struct FrameRecord *curr_rec,
	struct predicted_fl_info_st *fl_info,
	const char *caller)
{
	const struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;
	/* case handle */
	if (unlikely((curr_rec == NULL) || (fl_info == NULL))) {
		LOG_MUST(
			"[%s]: ERROR: [%u] ID:%#x(sidx:%u/inf:%u), get nullptr of curr_rec:%p or fl_info:%p, return\n",
			caller, idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			curr_rec, fl_info);
		return;
	}

	/* !!! copy already calculated info !!! */
	/* => copy current predicted frame length info */
	fl_info->pr_curr_fl_lc = pfrec->curr_predicted_fl_lc;
	fl_info->pr_curr_fl_us = pfrec->curr_predicted_fl_us;

	/* => copy current read offset info and calculate next read offset info */
	memcpy(fl_info->curr_exp_rd_offset_lc,
		pfrec->curr_predicted_rd_offset_lc,
		sizeof(unsigned int) * FS_HDR_MAX);
	memcpy(fl_info->curr_exp_rd_offset_us,
		pfrec->curr_predicted_rd_offset_us,
		sizeof(unsigned int) * FS_HDR_MAX);


	/* !!! calculate & copy new info by current sensor frame record !!! */
	/* => calc. & copy next predicted frame length info */
	fl_info->pr_next_fl_lc =
		frec_predict_fl_lc_by_curr_rec(
			idx, curr_rec, PREDICT_NEXT_FL);
	fl_info->pr_next_fl_us =
		convert2TotalTime(
			curr_rec->lineTimeInNs,
			fl_info->pr_next_fl_lc);

	/* => calc. & copy stable predicted frame length info */
	fl_info->pr_stable_fl_lc = frec_predict_fl_lc_by_curr_rec(
		idx, curr_rec, PREDICT_STABLE_FL);
	fl_info->pr_stable_fl_us =
		convert2TotalTime(
			curr_rec->lineTimeInNs,
			fl_info->pr_stable_fl_lc);

	/* => calc. & copy next read offset info */
	frec_predict_shutters_read_offset_by_curr_rec(idx, curr_rec,
		fl_info->next_exp_rd_offset_lc,
		fl_info->next_exp_rd_offset_us);

#if defined(TRACE_FS_FREC_LOG)
	frec_dump_predicted_fl_info_st(idx, fl_info, caller);
#endif
}


unsigned int frec_g_valid_min_fl_lc_for_shutters_by_frame_rec(
	const unsigned int idx, const struct FrameRecord *curr_rec,
	const unsigned int min_fl_lc, const enum predicted_fl_label label)
{
	const struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	const struct FrameRecord *frame_rec = NULL;
	unsigned int s_m_min_lc = 0;
	unsigned int result;

	/* error handle */
	if (unlikely(pfrec == NULL))
		return min_fl_lc;
	if (unlikely(!chk_if_sen_fdelay_is_valid(idx, pfrec, __func__)))
		return min_fl_lc;

	switch (label) {
	case PREDICT_NEXT_FL:
	{
		const unsigned int curr_idx = FS_ATOMIC_READ(&pfrec->depth_idx);
		const unsigned int target_idx = RING_BACK(curr_idx, 1);

		frame_rec = &pfrec->frame_recs[target_idx];
	}
		break;
	case PREDICT_STABLE_FL:
	default:
		frame_rec = curr_rec;
		break;
	}

	/* check min frame length that shutter needed */
	s_m_min_lc = frec_calc_valid_min_fl_lc_for_shutters(
		idx, pfrec->fl_act_delay, curr_rec, frame_rec, min_fl_lc);

	result = (s_m_min_lc > min_fl_lc) ? s_m_min_lc : min_fl_lc;

	LOG_INF(
		"[%u] ID:%#x(sidx:%u/inf:%u), result:%u (fdelay:%u/min_fl(s+m):%u/min_fl(maxFPS):%u), predict:%u(next:%u/stable:%u)\n",
		idx,
		fs_get_reg_sensor_id(idx),
		fs_get_reg_sensor_idx(idx),
		fs_get_reg_sensor_inf_idx(idx),
		result,
		pfrec->fl_act_delay,
		s_m_min_lc,
		min_fl_lc,
		label,
		PREDICT_NEXT_FL,
		PREDICT_STABLE_FL);

	return result;
}


static unsigned int g_seamless_1st_exp_line_time(
	const struct frec_sen_mode_info_st *p_mode_info,
	const struct FrameRecord *frame_rec, const unsigned int fl_act_delay)
{
	const unsigned int multi_exp_type = p_mode_info->prop.multi_exp_type;
	unsigned int ret = 0;

	switch (multi_exp_type) {
	case MULTI_EXP_TYPE_DCG_VSL:
		/* lineT is different in LUT_A, LUT_B, ... */
		if (fl_act_delay == 3) {
			/* => for CIT N+1 active but FLL N+2 active */
			/* const unsigned int mode_exp_cnt = frame_rec->mode_exp_cnt; */
			/* const unsigned int ref_idx = g_lut_id_by_cascade_ref_idx( */
			/*	mode_exp_cnt-1, mode_exp_cnt); */

			/* ret = p_mode_info->cas_mode_info.lineTimeInNs[ref_idx]; */
			ret = p_mode_info->cas_mode_info.lineTimeInNs[0];
		} else {
			/* => for CIT, FLL all N+1 active */
			ret = p_mode_info->cas_mode_info.lineTimeInNs[0];
		}
		break;
	default:
		ret = frame_rec->lineTimeInNs;
		break;
	}

	return ret;
}


static unsigned int frec_calc_seamless_frame_length(const unsigned int idx,
	const struct FrameRecorder *pfrec,
	const struct frec_seamless_st *p_seamless_rec,
	const unsigned int fl_act_delay)
{
	const struct FrameRecord *frame_rec = &p_seamless_rec->frame_rec;
	const struct fs_seamless_property_st *ss_prop = &p_seamless_rec->prop;
	const unsigned int orig_fl_us = pfrec->curr_predicted_fl_us;
	const int first_exp_idx = g_exp_order_idx_mapping(idx,
		frame_rec->exp_order, frame_rec->mode_exp_cnt, 0, __func__);
	const unsigned int log_str_len = 512;
	struct frec_sen_mode_info_st mode_info = {0};
	unsigned int fl_us_composition[3] = {0};
	unsigned int curr_exp_read_offset, ctrl_delta_to_sof_us;
	unsigned int depth_idx, seamless_shutter_lc = 0, new_mode_line_time_ns = 0;
	unsigned int result = 0;
	int orig_last_exp_idx;
	int len = 0, ret;
	char *log_buf = NULL;

	g_sen_mode_info(idx, &mode_info);

	/* Part-1: calculate end of readout time us */
	depth_idx = FS_ATOMIC_READ(&pfrec->depth_idx);
	orig_last_exp_idx = frec_g_mode_last_exp_idx(idx, pfrec, depth_idx);
	curr_exp_read_offset =
		pfrec->curr_predicted_rd_offset_us[orig_last_exp_idx];
	ctrl_delta_to_sof_us = (ss_prop->ctrl_receive_time_us < orig_fl_us)
		? ss_prop->ctrl_receive_time_us : 0;
	if ((curr_exp_read_offset + ss_prop->orig_readout_time_us)
			< ctrl_delta_to_sof_us)
		fl_us_composition[0] = ctrl_delta_to_sof_us;
	else {
		fl_us_composition[0] =
			curr_exp_read_offset + ss_prop->orig_readout_time_us;
	}

	/* Part-3: calculate seamless (new) mode re-shutter time us */
	if (unlikely(first_exp_idx < 0)) {
		seamless_shutter_lc = 0;
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u/inf:%u), exp_order_idx_map[%u][%u][0]:%d(< 0) => assign seamless_shutter_lc:%u\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			frame_rec->exp_order,
			frame_rec->mode_exp_cnt,
			first_exp_idx,
			seamless_shutter_lc);
	} else {
		/* check normal or hdr situation (normal: shutter_lc / hdr: hdr_exp) */
		seamless_shutter_lc = (p_seamless_rec->frame_rec.ae_exp_cnt > 1)
			? (p_seamless_rec->frame_rec.exp_lc_arr[first_exp_idx])
			: (p_seamless_rec->frame_rec.shutter_lc);
	}
	new_mode_line_time_ns =
		g_seamless_1st_exp_line_time(&mode_info, frame_rec, fl_act_delay);
	fl_us_composition[2] =
		convert2TotalTime(new_mode_line_time_ns, seamless_shutter_lc);

	/* Part-2: by seamless switch type, has different behavior => need differnt process */
	switch (ss_prop->type_id) {
	case FREC_SEAMLESS_SWITCH_CUT_VB_INIT_SHUT:
		if (ss_prop->prsh_length_lc > 0) {
			if (unlikely(ss_prop->prsh_length_lc < seamless_shutter_lc)) {
				/* !!! unexpected case !!! */
				/* dump more info */
				frec_dump_frame_record_info(
					&p_seamless_rec->frame_rec, __func__);

				LOG_MUST(
					"ERROR: [%u] ID:%#x(sidx:%u/inf:%u), get prsh_length_lc:%u but seamless_shutter_lc(NE):%u, prsh_length_lc value may be wrong\n",
					idx,
					fs_get_reg_sensor_id(idx),
					fs_get_reg_sensor_idx(idx),
					fs_get_reg_sensor_inf_idx(idx),
					ss_prop->prsh_length_lc,
					seamless_shutter_lc);

				fl_us_composition[1] =
					ss_prop->hw_re_init_time_us;
			} else {
				fl_us_composition[1] =
					ss_prop->hw_re_init_time_us +
					convert2TotalTime(
						new_mode_line_time_ns,
						ss_prop->prsh_length_lc
							- seamless_shutter_lc);
			}
		} else {
			fl_us_composition[1] =
				ss_prop->hw_re_init_time_us;
		}
		break;
	case FREC_SEAMLESS_SWITCH_ORIG_VB_INIT_SHUT:
		fl_us_composition[1] =
			orig_fl_us + ss_prop->hw_re_init_time_us;
		break;
	case FREC_SEAMLESS_SWITCH_ORIG_VB_ORIG_IMG:
		fl_us_composition[1] = orig_fl_us;
		break;
	default:
		break;
	}

	/* calculate the frame length of seamless switch frame */
	result =
		fl_us_composition[0] +
		fl_us_composition[1] +
		fl_us_composition[2];

	/* for print info */
	ret = alloc_log_buf(log_str_len, &log_buf);
	if (unlikely(ret != 0)) {
		LOG_MUST(
			"ERROR: [%u] log_buf allocate memory failed, only return result:%u\n",
			idx, result);
		return result;
	}

	FS_SNPRF(log_str_len, log_buf, len,
		"NOTICE: [%u][sidx:%u] seamless_fl_us:%u(%u(%u/%u)/%u/%u(%u)), new_mode_lineT:%u(%u/%u/%u/%u/%u), fdelay:%u, pr(c(%u)), r_offset[%u]:%u, type_id:%u, orig_readout_t:%u, hw_re_init_t:%u, prsh_length_lc:%u, (exp_lc:%u, (a:%u/m:%u(%u,%u), exp:%u/%u/%u/%u/%u)",
		idx,
		fs_get_reg_sensor_idx(idx),
		result,
		fl_us_composition[0],
		curr_exp_read_offset + ss_prop->orig_readout_time_us,
		ss_prop->ctrl_receive_time_us,
		fl_us_composition[1],
		fl_us_composition[2],
		seamless_shutter_lc,
		new_mode_line_time_ns,
		mode_info.cas_mode_info.lineTimeInNs[0],
		mode_info.cas_mode_info.lineTimeInNs[1],
		mode_info.cas_mode_info.lineTimeInNs[2],
		mode_info.cas_mode_info.lineTimeInNs[3],
		mode_info.cas_mode_info.lineTimeInNs[4],
		fl_act_delay,
		orig_fl_us,
		orig_last_exp_idx,
		curr_exp_read_offset,
		ss_prop->type_id,
		ss_prop->orig_readout_time_us,
		ss_prop->hw_re_init_time_us,
		ss_prop->prsh_length_lc,
		p_seamless_rec->frame_rec.shutter_lc,
		p_seamless_rec->frame_rec.ae_exp_cnt,
		p_seamless_rec->frame_rec.mode_exp_cnt,
		p_seamless_rec->frame_rec.m_exp_type,
		p_seamless_rec->frame_rec.exp_order,
		p_seamless_rec->frame_rec.exp_lc_arr[0],
		p_seamless_rec->frame_rec.exp_lc_arr[1],
		p_seamless_rec->frame_rec.exp_lc_arr[2],
		p_seamless_rec->frame_rec.exp_lc_arr[3],
		p_seamless_rec->frame_rec.exp_lc_arr[4]);
	LOG_MUST("%s\n", log_buf);
	FS_TRACE_PR_LOG_MUST("%s", log_buf);

	FS_FREE(log_buf);

	return result;
}
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
// sensor recorder framework functions
/*----------------------------------------------------------------------------*/
void frec_update_sen_mode_info(const unsigned int idx,
	const struct fs_hdr_exp_st *p_hdr_info)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	struct frec_sen_mode_info_st *p_mode_info = NULL;

	/* unexpected case */
	if (unlikely(pfrec == NULL))
		return;
	p_mode_info = &pfrec->mode_info;

	/* call static inline function to help */
	update_sen_mode_info(pfrec, p_mode_info, p_hdr_info);
}


void frec_clr_sen_mode_info(const unsigned int idx)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	struct frec_sen_mode_info_st *p_mode_info = NULL;

	/* unexpected case */
	if (unlikely(pfrec == NULL))
		return;
	p_mode_info = &pfrec->mode_info;

	/* call static inline function to help */
	clr_sen_mode_info(pfrec, p_mode_info);
}


void frec_chk_fl_pr_match_act(const unsigned int idx)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	const unsigned int diff_th = FL_ACT_CHK_TH;
	unsigned int diff;

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;

	diff = (pfrec->prev_predicted_fl_us > pfrec->act_fl_us)
		? (pfrec->prev_predicted_fl_us - pfrec->act_fl_us)
		: (pfrec->act_fl_us - pfrec->prev_predicted_fl_us);

	if (unlikely((pfrec->act_fl_us != 0) && (diff > diff_th))) {
		const unsigned int log_str_len = LOG_BUF_STR_LEN;
		char *log_buf = NULL;
		int len = 0, ret;

		ret = alloc_log_buf(log_str_len, &log_buf);
		if (unlikely(ret != 0)) {
			LOG_MUST("ERROR: log_buf allocate memory failed\n");
			return;
		}

		FS_SNPRF(log_str_len, log_buf, len,
			"WARNING: [%u] ID:%#x(sidx:%u/inf:%u), frame length (fdelay:%u): pr(p)(%u(%u)/act:%u) seems not match, diff:%u(%u), plz check manually",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			pfrec->fl_act_delay,
			pfrec->prev_predicted_fl_us,
			pfrec->prev_predicted_fl_lc,
			pfrec->act_fl_us,
			diff,
			diff_th);

		LOG_MUST_LOCK("%s\n", log_buf);
		FS_TRACE_PR_LOG_MUST("%s", log_buf);

		FS_FREE(log_buf);

		frec_dump_recorder(idx, __func__);
	} else {
		if (unlikely(_FS_LOG_ENABLED(LOG_FS_PF)))
			frec_dump_recorder(idx, __func__);
	}
}


/*
 * Notify fs algo and frame monitor the data in the frame recorder have been
 * updated
 *
 * This function should be call after having any frame recorder operation
 *
 *
 * description:
 *     fs algo will use these information to predict current and
 *     next framelength when calculating vsync diff.
 */
static void frec_notify_setting_frame_record_st_data(const unsigned int idx)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	struct FrameRecord *recs_ordered[RECORDER_DEPTH];
	struct predicted_fl_info_st fl_info = {0};
	unsigned int depth_idx;
	unsigned int i;

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;

	if (unlikely(!pfrec->is_init)) {
		LOG_INF(
			"NOTICE: [%u] frec has not been initialized(setup def val, fdelay), is_init:%u, return\n",
			idx,
			pfrec->is_init);
		return;
	}

	depth_idx = FS_ATOMIC_READ(&pfrec->depth_idx);

	/* prepare frame settings in the recorder */
	/*    => 0:newest, 1:second, 2:third */
	for (i = 0; i < RECORDER_DEPTH; ++i) {
		recs_ordered[i] = &pfrec->frame_recs[depth_idx];

		/* ring back depth_idx */
		depth_idx = RING_BACK(depth_idx, 1);
	}
	frec_get_predicted_frame_length_info(
		idx, recs_ordered[0], &fl_info, __func__);

	/* call fs alg set to update frame record data */
	fs_alg_set_frame_record_st_data(idx, recs_ordered, &fl_info);
}


static void frec_init_recorder_fl_related_info(const unsigned int idx,
	const struct FrameRecord *p_frame_rec)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	struct FrameRecord *curr_rec = NULL;
	struct predicted_fl_info_st fl_info = {0};

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;

	curr_rec = &pfrec->frame_recs[0];

	/* init predicted frame length */
	pfrec->prev_predicted_fl_lc = 0;
	pfrec->prev_predicted_fl_us = 0;

	/* trigger calculate newest predicted fl info */
	frec_get_predicted_frame_length_info(
		idx, curr_rec, &fl_info, __func__);

	/* copy newest curr predicted fl */
	pfrec->curr_predicted_fl_lc = fl_info.pr_next_fl_lc;
	pfrec->curr_predicted_fl_us = fl_info.pr_next_fl_us;

	/* copy result of newest next read offset info */
	memcpy(pfrec->next_predicted_rd_offset_lc,
		&fl_info.next_exp_rd_offset_lc,
		sizeof(unsigned int) * (FS_HDR_MAX));
	memcpy(pfrec->next_predicted_rd_offset_us,
		&fl_info.next_exp_rd_offset_us,
		sizeof(unsigned int) * (FS_HDR_MAX));

	/* init read offset value of each shutters */
	memcpy(pfrec->curr_predicted_rd_offset_lc,
		pfrec->next_predicted_rd_offset_lc,
		sizeof(unsigned int) * (FS_HDR_MAX));
	memcpy(pfrec->curr_predicted_rd_offset_us,
		pfrec->next_predicted_rd_offset_us,
		sizeof(unsigned int) * (FS_HDR_MAX));

	/* init predict EOF offset */
	refresh_next_predict_eof_offset(idx, pfrec, curr_rec);
	memcpy(pfrec->curr_predicted_eof_offset_us,
		pfrec->next_predicted_eof_offset_us,
		sizeof(unsigned int) * (FS_HDR_MAX));

	/* init debugging info for identifying settings */
	pfrec->mw_req_id_latched = p_frame_rec->mw_req_id;
}


static void frec_init_recorder_seamless_fl_related_info(const unsigned int idx,
	const unsigned int seamless_fl_us,
	const struct FrameRecord *p_frame_rec)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	struct FrameRecord *curr_rec = NULL;
	struct predicted_fl_info_st fl_info = {0};

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;

	curr_rec = &pfrec->frame_recs[0];

	/* setup predicted frame length (seamless frame) */
	pfrec->curr_predicted_fl_lc =
		convert2LineCount(
			p_frame_rec->lineTimeInNs,
			seamless_fl_us);
	pfrec->curr_predicted_fl_us = seamless_fl_us;

	/* trigger calculate newest predicted fl info */
	frec_get_predicted_frame_length_info(
		idx, curr_rec, &fl_info, __func__);

	/* copy result of newest next read offset info */
	memcpy(pfrec->next_predicted_rd_offset_lc,
		&fl_info.next_exp_rd_offset_lc,
		sizeof(unsigned int) * (FS_HDR_MAX));
	memcpy(pfrec->next_predicted_rd_offset_us,
		&fl_info.next_exp_rd_offset_us,
		sizeof(unsigned int) * (FS_HDR_MAX));

	/* refresh next predict EOF offset */
	refresh_next_predict_eof_offset(idx, pfrec, p_frame_rec);
}


static inline void frec_refresh_predicted_info(const unsigned int idx,
	struct FrameRecorder *pfrec,
	const struct predicted_fl_info_st *p_fl_info,
	const struct FrameRecord *p_frame_rec)
{
	/* refresh predicted FL */
	pfrec->prev_predicted_fl_lc = pfrec->curr_predicted_fl_lc;
	pfrec->prev_predicted_fl_us = pfrec->curr_predicted_fl_us;

	pfrec->curr_predicted_fl_lc = p_fl_info->pr_next_fl_lc;
	pfrec->curr_predicted_fl_us = p_fl_info->pr_next_fl_us;

	/* refresh read offset info */
	memcpy(pfrec->curr_predicted_rd_offset_lc,
		pfrec->next_predicted_rd_offset_lc,
		sizeof(unsigned int) * (FS_HDR_MAX));
	memcpy(pfrec->curr_predicted_rd_offset_us,
		pfrec->next_predicted_rd_offset_us,
		sizeof(unsigned int) * (FS_HDR_MAX));

	memcpy(pfrec->next_predicted_rd_offset_lc,
		&p_fl_info->next_exp_rd_offset_lc,
		sizeof(unsigned int) * (FS_HDR_MAX));
	memcpy(pfrec->next_predicted_rd_offset_us,
		&p_fl_info->next_exp_rd_offset_us,
		sizeof(unsigned int) * (FS_HDR_MAX));

	/* refresh predict EOF offset (curr. part will be updated) */
	memcpy(pfrec->curr_predicted_eof_offset_us,
		pfrec->next_predicted_eof_offset_us,
		sizeof(unsigned int) * (FS_HDR_MAX));
	refresh_next_predict_eof_offset(idx, pfrec, p_frame_rec);

	/* refresh debugging info for identifying settings */
	pfrec->mw_req_id_latched = p_frame_rec->mw_req_id;
}


void frec_update_fl_info(const unsigned int idx, const unsigned int fl_lc,
	const unsigned int fl_lc_arr[], const unsigned int arr_len)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	unsigned int depth_idx;

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;
	if (unlikely(fl_lc == 0)) {
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u/inf:%u) get: fl_lc:%u, do NOT update frec data, return\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			fl_lc);

		frec_dump_recorder(idx, __func__);
		return;
	}

	frec_mutex_lock(&pfrec->frame_recs_update_lock);

	depth_idx = FS_ATOMIC_READ(&pfrec->depth_idx);

	/* update values */
	pfrec->frame_recs[depth_idx].framelength_lc = fl_lc;
	memcpy(pfrec->frame_recs[depth_idx].fl_lc_arr, fl_lc_arr,
		sizeof(unsigned int) * arr_len);

#if defined(TRACE_FS_FREC_LOG)
	frec_dump_frame_record_info(&pfrec->frame_recs[depth_idx], __func__);
#endif

	/* set the results to fs algo and frame monitor */
	frec_notify_setting_frame_record_st_data(idx);

	frec_mutex_unlock(&pfrec->frame_recs_update_lock);
}


void frec_update_record(const unsigned int idx,
	const struct FrameRecord *p_frame_rec)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	unsigned int curr_depth_idx;

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;

	frec_mutex_lock(&pfrec->frame_recs_update_lock);

	curr_depth_idx = FS_ATOMIC_READ(&pfrec->depth_idx);

	/* setup/update record data */
	memcpy(&pfrec->frame_recs[curr_depth_idx], p_frame_rec,
		sizeof(pfrec->frame_recs[curr_depth_idx]));

#if defined(TRACE_FS_FREC_LOG)
	frec_dump_recorder(idx, __func__);
#endif

	/* set the results to fs algo and frame monitor */
	frec_notify_setting_frame_record_st_data(idx);

	frec_mutex_unlock(&pfrec->frame_recs_update_lock);
}


void frec_push_record(const unsigned int idx)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	struct FrameRecord *curr_rec = NULL;
	struct predicted_fl_info_st fl_info = {0};
	unsigned long long sys_ts = 0;
	unsigned int curr_depth_idx, next_depth_idx;

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;
	if (unlikely(pfrec->is_init == 0)) {
		LOG_MUST(
			"WARNING: [%u] ID:%#x(sidx:%u/inf:%u) MUST initialize sensor recorder first\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx));
	}

	frec_mutex_lock(&pfrec->frame_recs_update_lock);

	/* !!! prepare information !!! */
	sys_ts = ktime_get_boottime_ns();
	curr_depth_idx = FS_ATOMIC_READ(&pfrec->depth_idx);
	curr_rec = &pfrec->frame_recs[curr_depth_idx];

	/* !!! first, calc. then update predicted FL related info !!! */
	/* => trigger calculate newest predicted fl info */
	frec_get_predicted_frame_length_info(
		idx, curr_rec, &fl_info, __func__);
	frec_refresh_predicted_info(idx, pfrec, &fl_info, curr_rec);

	/* !!! ring forward(update) the recorder !!! */
	/* => depth idx ring forward */
	next_depth_idx = RING_FORWARD(curr_depth_idx, 1);
	FS_ATOMIC_SET(next_depth_idx, &pfrec->depth_idx);

	/* => copy latest sensor record to newest depth idx */
	memcpy(&pfrec->frame_recs[next_depth_idx],
		&pfrec->frame_recs[curr_depth_idx],
		sizeof(pfrec->frame_recs[next_depth_idx]));

	/* !!! update system timestamp for debugging !!! */
	pfrec->sys_ts_recs[next_depth_idx] = sys_ts;

	/* !!! set the results to fs algo and frame monitor !!! */
	frec_notify_setting_frame_record_st_data(idx);

	frec_mutex_unlock(&pfrec->frame_recs_update_lock);
}


void frec_reset_records(const unsigned int idx)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	unsigned int i;

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;

	frec_mutex_lock(&pfrec->frame_recs_update_lock);

	for (i = 0; i < RECORDER_DEPTH; ++i) {
		memset(&pfrec->frame_recs[i], 0, sizeof(pfrec->frame_recs[i]));

		pfrec->sys_ts_recs[i] = 0;
	}

	FS_ATOMIC_SET(0, &pfrec->depth_idx);
	pfrec->is_init = 0;

#if defined(TRACE_FS_FREC_LOG)
	frec_dump_recorder(idx, __func__);
#endif

	frec_mutex_unlock(&pfrec->frame_recs_update_lock);
}


void frec_setup_def_records(const unsigned int idx, const unsigned int def_fl_lc,
	const struct FrameRecord *p_frame_rec)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	unsigned int i;

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;
	if (unlikely(pfrec->is_init)) {
		LOG_MUST(
			"WARNING: [%u] frec had been initialized(setup def/init value), is_init:%u, return\n",
			idx,
			pfrec->is_init);

		frec_dump_recorder(idx, __func__);
		return;
	}

	frec_mutex_lock(&pfrec->frame_recs_update_lock);

	/* init all frec value to default shutter and framelength */
	pfrec->is_init = 1;
	FS_ATOMIC_SET(0, &pfrec->depth_idx);
	for (i = 0; i < RECORDER_DEPTH; ++i) {
		memcpy(&pfrec->frame_recs[i], p_frame_rec,
			sizeof(pfrec->frame_recs[i]));

		pfrec->sys_ts_recs[i] = 0;
	}
	pfrec->def_fl_lc = def_fl_lc;
	frec_init_recorder_fl_related_info(idx, p_frame_rec);

	/* set the results to fs algo and frame monitor */
	frec_notify_setting_frame_record_st_data(idx);

	frec_mutex_unlock(&pfrec->frame_recs_update_lock);

	frec_dump_recorder(idx, __func__);
}


void frec_init_recorder(const unsigned int idx,
	const struct FrameRecord *p_frame_rec,
	const unsigned int def_fl_lc, const unsigned int fl_act_delay)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;

	/* setup basic sensor info */
	pfrec->fl_act_delay = fl_act_delay;

	/* error handle, check sensor fl_active_delay value */
	if (unlikely((fl_act_delay < 2) || (fl_act_delay > 3))) {
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u/inf:%u), sensor driver's frame_time_delay_frame:%u is not valid (MUST be 2 or 3), plz check sensor driver for getting correct value\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			fl_act_delay);
	}

	frec_setup_def_records(idx, def_fl_lc, p_frame_rec);
}


void frec_seamless_switch(const unsigned int idx,
	const unsigned int def_fl_lc, const unsigned int fl_act_delay,
	const struct frec_seamless_st *p_seamless_rec)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	unsigned int seamless_fl_us;
	unsigned int i;

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;

	if (_FS_LOG_ENABLED(LOG_SEN_REC_SEAMLESS_DUMP))
		frec_dump_recorder(idx, __func__);

	frec_mutex_lock(&pfrec->frame_recs_update_lock);

	seamless_fl_us = frec_calc_seamless_frame_length(
		idx, pfrec, p_seamless_rec, fl_act_delay);

	FS_ATOMIC_SET(0, &pfrec->depth_idx);
	for (i = 0; i < RECORDER_DEPTH; ++i) {
		memcpy(&pfrec->frame_recs[i], &p_seamless_rec->frame_rec,
			sizeof(pfrec->frame_recs[i]));

		pfrec->sys_ts_recs[i] = 0;
	}
	pfrec->fl_act_delay = fl_act_delay;
	pfrec->def_fl_lc = def_fl_lc;
	frec_init_recorder_seamless_fl_related_info(
		idx, seamless_fl_us, &p_seamless_rec->frame_rec);

	/* set the results to fs algo and frame monitor */
	frec_notify_setting_frame_record_st_data(idx);

	frec_mutex_unlock(&pfrec->frame_recs_update_lock);

	if (_FS_LOG_ENABLED(LOG_SEN_REC_SEAMLESS_DUMP))
		frec_dump_recorder(idx, __func__);
}


// void frec_notify_sensor_pre_latch(const unsigned int idx)
void frec_notify_vsync(const unsigned int idx)
{
	/* !!! do each thing that needed at the timing sensor HW pre-latch !!! */
	frec_push_record(idx);
}


void frec_notify_update_timestamp_data(const unsigned int idx,
	const unsigned int tick_factor,
	const SenRec_TS_T ts_us[], const unsigned int arr_len,
	const enum fs_timestamp_src_type ts_src_type)
{
	struct FrameRecorder *pfrec = frec_g_recorder_ctx(idx, __func__);
	SenRec_TS_T tick_a, tick_b;

	/* error handle */
	if (unlikely(pfrec == NULL))
		return;

	/* copy timestamp source type */
	pfrec->ts_src_type = ts_src_type;

	/* copy/update newest timestamp data */
	memcpy(pfrec->ts_exp_0, ts_us, sizeof(SenRec_TS_T) * arr_len);
	pfrec->tick_factor = tick_factor;

	/* check if this is first timestamp */
	if (unlikely(pfrec->ts_exp_0[1] == 0)) {
		pfrec->act_fl_us = 0;
		return;
	}

	/* update actual frame length by timestamp diff */
	tick_a = pfrec->ts_exp_0[0] * pfrec->tick_factor;
	tick_b = pfrec->ts_exp_0[1] * pfrec->tick_factor;
	pfrec->act_fl_us = divide_num(idx,
		(tick_a - tick_b), pfrec->tick_factor, __func__);
}
/******************************************************************************/

