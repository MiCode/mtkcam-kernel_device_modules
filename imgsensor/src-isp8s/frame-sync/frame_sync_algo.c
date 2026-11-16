// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifdef FS_UT
#include <string.h>
#include <stdlib.h>         /* Needed by memory allocate */
#else
/* INSTEAD of using stdio.h, you have to use the following include */
#include <linux/module.h>   /* Needed by all modules */
#include <linux/kernel.h>   /* Needed for KERN_INFO */
#include <linux/slab.h>     /* Needed by memory allocate */
#include <linux/string.h>
#endif

#include "frame_sync_trace.h"
#include "frame_sync_algo.h"
#include "frame_monitor.h"
#include "frame_sync_flk.h"
#include "frame_sync_event_exe.h"
#include "sensor_recorder.h"

#if !defined(FS_UT)
#include "frame_sync_console.h"
#endif


/******************************************************************************/
// Log message
/******************************************************************************/
#include "frame_sync_log.h"

#define REDUCE_FS_ALGO_LOG
#define PFX "FrameSyncAlgo"
#define FS_LOG_DBG_DEF_CAT LOG_FS_ALGO
/******************************************************************************/


#ifdef SUPPORT_FS_NEW_METHOD
/******************************************************************************/
// Lock
/******************************************************************************/
#ifndef FS_UT
#include <linux/spinlock.h>
static DEFINE_SPINLOCK(fs_alg_sa_dynamic_para_op_lock);
static DEFINE_SPINLOCK(fs_alg_sa_dynamic_fps_op_lock);
#endif // FS_UT
/******************************************************************************/
#endif // SUPPORT_FS_NEW_METHOD





/******************************************************************************/
// Frame Sync Instance Structure (private structure)
/******************************************************************************/
#ifdef SUPPORT_FS_NEW_METHOD
enum fs_dynamic_fps_status {
	FS_DY_FPS_STABLE = 0,
	FS_DY_FPS_INC,
	FS_DY_FPS_DEC,
	FS_DY_FPS_DEC_MOST,
	FS_DY_FPS_USER_CHG,        /* user update max fps setting */
	FS_DY_FPS_SINGLE_CAM_SKIP, /* currently is single cam case */

	FS_DY_FPS_UNSTABLE         /* NOT classified it, just assign a status */
};


struct fs_dynamic_fps_record_st {
	unsigned int magic_num;
	int req_id;

	unsigned int pure_min_fl_us;
	unsigned int min_fl_us;
	unsigned int target_min_fl_us;
	unsigned int stable_fl_us;
};


/* structure for FrameSync StandAlone (SA) mode using */
struct FrameSyncDynamicPara {
	/* serial number for each dynamic paras */
	unsigned int magic_num;
	unsigned int extra_magic_num; /* NOT from AE CTRL, e.g., bcast re-CTRL FL */

	/* adjust diff info */
	int master_idx;
	unsigned int is_master;
	unsigned int ref_m_idx_magic_num;
	unsigned int ask_for_chg;       // if finally ask FS DRV switch to master
	unsigned int ask_for_bcast_re_ctrl_fl;  /* broadcast to re-ctrl others' FL */
	unsigned int chg_master;        // if request to change to master
	unsigned int subtract_fl;       /* if output FL is calculated by subtraction */
	unsigned int adj_or_not;
	unsigned int need_auto_restore_fl;
	unsigned int is_correction_suitable; /* diff is suitable for correction */
	long long corrected_fl_diff;    /* when FPS inc, M&S's stable FL are diff. */
	long long adj_diff_m;
	long long adj_diff_s;
	long long adj_diff_final;

	/* frame length info */
	/* ==> Frame-Sync view --- multiply by f_cell */
	unsigned int pure_min_fl_lc;    // max((exp+margin),user-min_fl)
	unsigned int pure_min_fl_us;    // max((exp+margin),user-min_fl)
	unsigned int min_fl_us;         // max((exp+margin),user-min_fl)+flk
	unsigned int target_min_fl_us;  // FPS sync result => a frame block size
	unsigned int fps_status;
	unsigned int fps_status_aligned;
	/* ==> sensor view --- output value for sensor */
	unsigned int out_fl_us_min;     /* valid min fl for output (w/ flk) */
	unsigned int out_fl_us_init;    // FPS sync output / async min output
	unsigned int out_fl_us;         // final output
	unsigned int stable_fl_us;

	/* predicted frame length (0:current / 1:next) */
	unsigned int pred_fl_us[2];
	unsigned int pred_next_exp_rd_offset_us[FS_HDR_MAX];
	unsigned int pred_fl_err_chk_bits_m;
	long long pred_fl_err_us_m;

	/* sync target ts bias (for feature that sync to non-1st exp) */
	unsigned int ts_bias_us;
	unsigned int m_last_ts_bias_us;
	int anchor_bias_us;             /* For MW, SOF anchor bias */

	/* N:1 sync */
	unsigned int f_tag;
	unsigned int f_cell;
	unsigned int tag_bias_us;

	/* total dalta (without adding timestamp diff) */
	unsigned int delta;
	unsigned int async_m_delta;

	/* timestamp info */
	enum fs_timestamp_src_type ts_src_type;
	unsigned long long cur_tick;    // current tick at querying data
	unsigned long long last_ts;     // last timestamp at querying data
	unsigned int vsyncs;            // passed vsync counts
	unsigned int ts_offset;         /* XVS vs Vsync timing offset */

	/* fs SA mode cfg */
	struct fs_sa_cfg sa_cfg;

	/* debug variables --- from mtk hdr ae structure */
	unsigned int frame_id;          /* MW pipeline frame id */
	int req_id;                     /* MW job id or ae id */
};


struct FrameSyncStandAloneInst {
	/* support: 0:adaptive switch master */
	unsigned int sa_algo;

	/* serial number for each dynamic paras */
	unsigned int magic_num[SENSOR_MAX_NUM];
	unsigned int extra_magic_num[SENSOR_MAX_NUM];

	/* all sensor shared dynamic paras for FS SA mode */
	struct FrameSyncDynamicPara dynamic_paras[SENSOR_MAX_NUM];

	FS_Atomic_T unstable_fps_bits;
	struct fs_dynamic_fps_record_st dynamic_fps_recs[SENSOR_MAX_NUM];
	struct fs_dynamic_fps_record_st last_dynamic_fps_recs[SENSOR_MAX_NUM];
};
static struct FrameSyncStandAloneInst fs_sa_inst;
#endif // SUPPORT_FS_NEW_METHOD
//----------------------------------------------------------------------------//


#define FS_FL_RECORD_DEPTH (5)
struct fs_fl_rec_st {
	unsigned int ref_magic_num;
	unsigned int target_min_fl_us;
	unsigned int out_fl_us;
};
//----------------------------------------------------------------------------//


struct FrameSyncInst {
	/* register sensor info */
	unsigned int sensor_id;         // imx586 -> 0x0586; s5k3m5sx -> 0x30D5
	unsigned int sensor_idx;        // main1 -> 0; sub1 -> 1;
					// main2 -> 2; sub2 -> 3; main3 -> 4;

//----------------------------------------------------------------------------//

	enum FS_SYNC_TYPE sync_type;
	unsigned int custom_bias_us;    // for sync with diff

//----------------------------------------------------------------------------//

//---------------------------- fs_streaming_st -------------------------------//
	/* know TG when streaming */
	unsigned int tg;                // Not used if ISP7 uses sensor_idx

	unsigned int fl_active_delay;   // SONY/auto_ext:(3, 1); others:(2, 0);
	unsigned int def_min_fl_lc;
	unsigned int max_fl_lc;         // for frame length boundary check

	unsigned int def_shutter_lc;    // default shutter_lc in driver

//----------------------------------------------------------------------------//

	/* IOCTRL CID ANTI_FLICKER */
	unsigned int flicker_en;        // move to perframe_st

//---------------------------- fs_perframe_st --------------------------------//
	/* IOCTRL CID SHUTTER_FRAME_LENGTH */
	unsigned int min_fl_lc;         // dynamic FPS using
	unsigned int shutter_lc;

	/* current/previous multi exposure settings */
	/*    because exp is N+1 delay, so just keep previous settings */
	struct fs_hdr_exp_st hdr_exp;
	struct fs_hdr_exp_st prev_hdr_exp;

	/* on-the-fly sensor mode change */
	unsigned int margin_lc;
	unsigned int lineTimeInNs;      // ~= 10^9 * (linelength/pclk)
	unsigned int readout_time_us;   // current mode read out time.

	/* output frame length */
	unsigned int output_fl_us;      // microsecond for algo using
	unsigned int output_fl_lc;
	unsigned int output_fl_lc_arr[FS_HDR_MAX];
//----------------------------------------------------------------------------//

//---------------------------- private member --------------------------------//
	/* for STG sensor read offset change, effect valid min fl */
	unsigned int readout_min_fl_lc;
	unsigned int prev_readout_min_fl_lc;


	/* for STG sensor using FDOL mode like DOL mode */
	/* when doing STG seamless switch */
	unsigned int extend_fl_lc;
	unsigned int extend_fl_us;


	/* for different fps sync sensor sync together */
	/* e.g. fps: (60 vs 30) */
	/* => frame_cell_size: 2 */
	/* => frame_tag: 0, 1, 0, 1, 0, ... */
	unsigned int n_1_on_off:1;
	unsigned int frame_cell_size;
	unsigned int frame_tag;


	/* predicted frame length, current:0, next:1 */
	/* must be updated when getting new frame record data / vsync data */
	unsigned int predicted_fl_us[2];
	unsigned int predicted_fl_lc[2];
	/* => last ts bias will be updated when receive pre-latch from SenRec */
	FS_Atomic_T ts_bias_us;
	FS_Atomic_T last_ts_bias_us; /* => preivous ts bias */
	FS_Atomic_T anchor_bias_us;  /* For MW, SOF anchor bias */

	unsigned int vsyncs_updated:1;

	/* frame_record_st (record shutter and framelength settings) */
	struct FrameRecord *p_frecs[RECORDER_DEPTH];
	struct predicted_fl_info_st fl_info;

	struct fs_fl_rec_st fl_rec[FS_FL_RECORD_DEPTH];


	/* frame monitor data */
	enum fs_timestamp_src_type ts_src_type;
	unsigned int vsyncs;
	unsigned long long last_vts;
	unsigned long long timestamps[VSYNCS_MAX];
	unsigned long long cur_tick;
	unsigned int vdiff;

	unsigned int is_nonvalid_ts:1;

//----------------------------------------------------------------------------//

	/* debug variables */
	unsigned int sof_cnt;           /* from seninf vsync notify */
	unsigned int frame_id;          /* MW pipeline frame id */
	int req_id;                     /* MW job id or ae id */
};
static struct FrameSyncInst fs_inst[SENSOR_MAX_NUM];


/* fps sync result */
static unsigned int target_min_fl_us;


/* frame monitor data */
static unsigned long long cur_tick;
static unsigned int tick_factor;
/******************************************************************************/





/******************************************************************************/
// utility functions
/******************************************************************************/
void fs_alg_get_out_fl_info(const unsigned int idx,
	unsigned int *p_out_fl_lc,
	unsigned int p_out_fl_lc_arr[], const unsigned int arr_len)
{
	if (unlikely((p_out_fl_lc == NULL) || (p_out_fl_lc_arr == NULL))) {
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u/inf:%u), #%u, (req:%d/f:%u/%u), get nullptr of p_out_fl_lc:%p/p_out_fl_lc_arr:%p, return\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fs_get_reg_sensor_inf_idx(idx),
			fs_sa_inst.dynamic_paras[idx].magic_num,
			fs_inst[idx].req_id,
			fs_inst[idx].frame_id,
			fs_inst[idx].sof_cnt,
			p_out_fl_lc,
			p_out_fl_lc_arr);
		return;
	}

	*p_out_fl_lc = fs_inst[idx].output_fl_lc;
	memcpy(p_out_fl_lc_arr, &fs_inst[idx].output_fl_lc_arr,
		(sizeof(unsigned int) * FS_HDR_MAX));
}


/**
 * return:
 *          "0" -> done.
 *      "non 0" -> errors. (error case will only appear when using CCU.)
 */
static unsigned int g_vsync_timestamp_data(const unsigned int idx_arr[],
	const unsigned int len, const enum fs_timestamp_src_type ts_src_type)
{
	struct vsync_rec vsync_recs = {0};
	unsigned int i = 0, j = 0;

	/* get timestamp data */
	if (unlikely(frm_g_vsync_timestamp_data(idx_arr, len,
			ts_src_type, &vsync_recs)))
		return 1;

	/* keep cur_tick and tick_factor value */
	cur_tick = vsync_recs.cur_tick;
	tick_factor = vsync_recs.tick_factor;
	/* keep vsync and last_vts data */
	for (i = 0; i < len; ++i) {
		const unsigned int idx = idx_arr[i];

		if (fs_inst[idx].tg != vsync_recs.recs[i].id) {
			LOG_PR_WARN(
				"ERROR: [%u].tg:%u not sync to v_recs[%u].tg:%u\n",
				idx,
				fs_inst[idx].tg,
				i,
				vsync_recs.recs[i].id);

			return 1;
		}

		fs_inst[idx].vsyncs = vsync_recs.recs[i].vsyncs;
		fs_inst[idx].last_vts = vsync_recs.recs[i].timestamps[0];
		fs_inst[idx].cur_tick = vsync_recs.cur_tick;

		for (j = 0; j < VSYNCS_MAX; ++j) {
			fs_inst[idx].timestamps[j] =
				vsync_recs.recs[i].timestamps[j];
		}

		frec_notify_update_timestamp_data(idx,
			vsync_recs.tick_factor,
			vsync_recs.recs[i].timestamps, VSYNCS_MAX,
			ts_src_type);

#if !defined(REDUCE_FS_ALGO_LOG)
		LOG_MUST(
			"[%u] ID:%#x(sidx:%u), tg:%u, vsyncs:%u, last_vts:%llu, cur_tick:%llu, ts(type:%u):(%llu/%llu/%llu/%llu), tick_factor:%u\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			fs_inst[idx].tg,
			fs_inst[idx].vsyncs,
			fs_inst[idx].last_vts,
			fs_inst[idx].cur_tick,
			fs_inst[idx].ts_src_type,
			fs_inst[idx].timestamps[0],
			fs_inst[idx].timestamps[1],
			fs_inst[idx].timestamps[2],
			fs_inst[idx].timestamps[3],
			vsync_recs.tick_factor);
#endif
	}

	return 0;
}


static inline unsigned int set_and_chk_margin_lc(const unsigned int idx,
	const unsigned int margin_lc, const char *caller)
{
	if (unlikely(margin_lc == 0)) {
		LOG_MUST(
			"[%s] WARNING: [%u] ID:%#x(sidx:%u), get non valid margin_lc:%u, plz check sensor driver for getting correct value\n",
			caller,
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			margin_lc);
	}
	return margin_lc;
}


static inline unsigned int set_and_chk_fl_active_delay(const unsigned int idx,
	const unsigned int fl_active_delay, const char *caller)
{
	if (unlikely((fl_active_delay < 2) || (fl_active_delay > 3))) {
		LOG_MUST(
			"[%s] ERROR: [%u] ID:%#x(sidx:%u), get non valid frame_time_delay_frame/delay_frame:%u (must be 2 or 3), plz check sensor driver for getting correct value\n",
			caller,
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			fl_active_delay);
	}
	return fl_active_delay;
}


/* return: 1 => mixed together; 0 => same type (e.g., all N+3 or N+2) */
/* static */ int chk_if_fdelay_type_mixed_together(const unsigned int mask)
{
	unsigned int fdelay_3_cnt = 0, fdelay_2_cnt = 0;
	unsigned int i;

	for (i = 0; i < SENSOR_MAX_NUM; ++i) {
		if (((mask >> i) & 1UL) == 0)
			continue;

		if (fs_inst[i].fl_active_delay == 3)
			fdelay_3_cnt++;
		else if (fs_inst[i].fl_active_delay == 2)
			fdelay_2_cnt++;
	}

	return ((fdelay_2_cnt != 0) && (fdelay_3_cnt != 0)) ? 1 : 0;
}


static void g_flk_fl_and_flk_diff(const unsigned int idx,
	unsigned int *p_fl_us, unsigned int *p_flk_diff,
	const unsigned int sync_flk_en)
{
	const unsigned int fl_us_orig = *p_fl_us;
	unsigned int fl_us = *p_fl_us, flk_diff = 0;
	unsigned int i, ret;

	/* check flk EN on itself */
	if (fs_inst[idx].flicker_en) {
		ret = fs_flk_get_anti_flicker_fl(fs_inst[idx].flicker_en,
			fl_us_orig, &fl_us);
		if (unlikely(ret != FLK_ERR_NONE)) {
			LOG_MUST(
				"ERROR: call fs flk get anti flk fl, ret:%u   [flk_en:%u/fl:(%u->%u)]\n",
				ret, fs_inst[idx].flicker_en,
				fl_us_orig, fl_us);
		}
		flk_diff = fl_us - fl_us_orig;
	}

	/* check flk EN on other sensors */
	if (sync_flk_en) {
		for (i = 0; i < SENSOR_MAX_NUM; ++i) {
			const unsigned int flk_en = fs_inst[i].flicker_en;
			unsigned int temp_fl_us = fl_us_orig;

			if (flk_en == 0)
				continue;

			ret = fs_flk_get_anti_flicker_fl(flk_en,
				fl_us_orig, &temp_fl_us);
			if (unlikely(ret != FLK_ERR_NONE)) {
				LOG_MUST(
					"ERROR: call fs flk get anti flk fl, ret:%u   [flk_en:%u/fl:(%u->%u)]\n",
					ret, flk_en, fl_us_orig, temp_fl_us);
			}
			if (temp_fl_us > fl_us) {
				fl_us = temp_fl_us;
				flk_diff = fl_us - fl_us_orig;
			}
		}
	}

	/* copy/sync results */
	if (likely(p_fl_us != NULL))
		*p_fl_us = fl_us;
	if (likely(p_flk_diff != NULL))
		*p_flk_diff = flk_diff;
}


/*static*/ unsigned int chk_if_need_to_sync_flk_en_status(const unsigned int idx)
{
	unsigned int flk_en_fdelay = (0 - 1);
	unsigned int i;

	/* find out min fdelay of all flk_en */
	for (i = 0; i < SENSOR_MAX_NUM; ++i) {
		if (fs_inst[i].flicker_en > 0) {
			/* find out min. fdelay of flicker EN sensors */
			if (fs_inst[i].fl_active_delay < flk_en_fdelay)
				flk_en_fdelay = fs_inst[i].fl_active_delay;
		}
	}

	return (fs_inst[idx].fl_active_delay < flk_en_fdelay) ? 0 : 1;
}


static inline unsigned int check_fs_inst_vsync_data_valid(
	const unsigned int solveIdxs[], const unsigned int len)
{
	unsigned int i = 0;
	unsigned int ret = 1; // valid -> 1 / non-valid -> 0

	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];

		/* valid -> ret = 1 (not be changed) */
		/* non-valid -> ret = 0, and keep it being 0 */
		if (fs_inst[idx].last_vts == 0)
			ret = 0;
	}

	return ret;
}


#if defined(SUPPORT_FS_NEW_METHOD)
/*
 * Be careful: query frame_cell_size behavior must use this API
 *
 * return:
 *     1: when frame_cell_size is 0 or 1
 *     u_int (>1): when frame_cell_size is bigger than 1
 */
static inline unsigned int get_valid_frame_cell_size(unsigned int idx)
{
	return (fs_inst[idx].frame_cell_size > 1)
		? (fs_inst[idx].frame_cell_size) : 1;
}
#endif


static inline void chk_fl_boundary(const unsigned int idx,
	unsigned int *p_fl_lc, const char *caller)
{
	if (unlikely(*p_fl_lc > fs_inst[idx].max_fl_lc)) {
		LOG_MUST(
			"[%s] ERROR: [%u] ID:%#x(sidx:%u), want to set fl:%u(%u), but reaches max_fl:%u(%u)\n",
			caller,
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			convert2TotalTime(
				fs_inst[idx].lineTimeInNs,
				*p_fl_lc),
			*p_fl_lc,
			convert2TotalTime(
				fs_inst[idx].lineTimeInNs,
				fs_inst[idx].max_fl_lc),
			fs_inst[idx].max_fl_lc);

		*p_fl_lc = fs_inst[idx].max_fl_lc;
	}
}


static void chk_fl_restriction(const unsigned int idx, unsigned int *p_fl_lc,
	const char *caller)
{
	/* check extend frame length had been set */
	if (unlikely(fs_inst[idx].extend_fl_lc != 0)) {
		*p_fl_lc += fs_inst[idx].extend_fl_lc;
		LOG_MUST(
			"[%s] NOTICE: [%u] ID:%#x(sidx:%u), set fl to %u(%u) due to receive ext_fl:%u(%u))\n",
			caller,
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			convert2TotalTime(
				fs_inst[idx].lineTimeInNs,
				*p_fl_lc),
			*p_fl_lc,
			fs_inst[idx].extend_fl_us,
			fs_inst[idx].extend_fl_lc);
	}

	/* check frame length register boundary (max) */
	chk_fl_boundary(idx, p_fl_lc, __func__);
}


static void get_valid_fl_lc_info(const unsigned int idx, unsigned int *p_fl_lc,
	unsigned int fl_lc_arr[], const unsigned int arr_len)
{
	/* check frame length register boundary (max) */
	chk_fl_boundary(idx, p_fl_lc, __func__);

	/* ==> e.g., LB-MF sensor */
	if (frec_chk_if_lut_is_used(fs_inst[idx].p_frecs[0]->m_exp_type)) {
		frec_g_valid_min_fl_arr_val_for_lut(
			idx, fs_inst[idx].p_frecs[0],
			*p_fl_lc, fl_lc_arr, arr_len);
	}
}


static void set_fl_us(const unsigned int idx, const unsigned int us)
{
	const unsigned int line_time = fs_inst[idx].lineTimeInNs;
	unsigned int fl_lc = convert2LineCount(line_time, us);
	unsigned int fl_lc_arr[FS_HDR_MAX] = {0};

	get_valid_fl_lc_info(idx, &fl_lc, fl_lc_arr, FS_HDR_MAX);

	/* setup out FL values */
	fs_inst[idx].output_fl_lc = fl_lc;
	fs_inst[idx].output_fl_us = convert2TotalTime(line_time, fl_lc);
	/* ==> for LB-MF sensor */
	memcpy(fs_inst[idx].output_fl_lc_arr, fl_lc_arr,
		(sizeof(unsigned int) * FS_HDR_MAX));

#ifdef FS_UT
	frm_set_sensor_curr_fl_us(idx, fs_inst[idx].output_fl_us);
#endif
}


/*
 * calculate a appropriate min framelength base on shutter with boundary check
 *
 * input: idx, min_fl_lc
 *
 * reference: shutter, margin, max_fl_lc
 *
 * "min_fl_lc":
 *      could be "def_min_fl_lc" ( like sensor driver write_shutter() function )
 *      or "min_fl_lc" ( for frame sync dynamic FPS ).
 */
static unsigned int calc_min_fl_lc(const unsigned int idx,
	const unsigned int min_fl_lc, const enum predicted_fl_label label)
{
	unsigned int output_fl_lc = 0;

	output_fl_lc =
		frec_g_valid_min_fl_lc_for_shutters_by_frame_rec(
			idx, fs_inst[idx].p_frecs[0],
			min_fl_lc, label);

	chk_fl_restriction(idx, &output_fl_lc, __func__);

	return output_fl_lc;
}


static unsigned int calc_vts_sync_bias_lc(const unsigned int idx)
{
	const enum FS_SYNC_TYPE sync_type = fs_inst[idx].sync_type;
	unsigned int exp_bias_lc = 0, total_bias_lc = 0;

	if (sync_type & FS_SYNC_TYPE_LE) {
		exp_bias_lc =
			fs_inst[idx].fl_info.next_exp_rd_offset_lc[FS_HDR_LE];
	}
	if (sync_type & FS_SYNC_TYPE_SE) {
		exp_bias_lc =
			fs_inst[idx].fl_info.next_exp_rd_offset_lc[FS_HDR_SE];
	}

	total_bias_lc = (exp_bias_lc);

#if defined(SYNC_WITH_CUSTOM_DIFF)
	if (unlikely(fs_inst[idx].custom_bias_us != 0)) {
		total_bias_lc += convert2LineCount(
				fs_inst[idx].lineTimeInNs,
				fs_inst[idx].custom_bias_us);

		LOG_MUST(
			"NOTICE: [%u] ID:%#x(sidx:%u), set custom_bias:%u(%u)\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			fs_inst[idx].custom_bias_us,
			convert2LineCount(
				fs_inst[idx].lineTimeInNs,
				fs_inst[idx].custom_bias_us));
	}
#endif

	return total_bias_lc;
}


/*
 * check pf ctrl trigger in critical section of timestamp.
 * next timestamp of one sensor is coming soon.
 * if it is sync, pred_vdiff almost equal to target_min_fl_us.
 *
 * return:
 *     1: trigger in ts critaical section
 *     0: not trigger in ts critical section
 */
static inline unsigned int check_timing_critical_section(
	const long long pred_vdiff, const unsigned int target_min_fl_us)
{
	unsigned int threshold = 0/*, delta = 0*/;

	// threshold = FS_TOLERANCE / 2;
	// delta = threshold / 10;
	// threshold += delta;
	threshold = FS_TOLERANCE;

	if (pred_vdiff > target_min_fl_us)
		return (((pred_vdiff - target_min_fl_us) < threshold) ? 1 : 0);
	else
		return (((target_min_fl_us - pred_vdiff) < threshold) ? 1 : 0);
}


static unsigned int fs_alg_chk_if_need_to_setup_fl_restore_ctrl(
	const unsigned int idx, const unsigned int out_fl_us,
	const struct FrameSyncDynamicPara *p_para)
{
#if !defined(FS_FL_AUTO_RESTORE_DISABLE)
	struct fs_fl_restore_info_st fl_restore_info = {0};
	const unsigned int line_time = fs_inst[idx].lineTimeInNs;
	unsigned int fl_lc = convert2LineCount(line_time, p_para->stable_fl_us);
	unsigned int fl_lc_arr[FS_HDR_MAX] = {0};
	unsigned int diff;

	/* check case */
	diff = (out_fl_us > p_para->stable_fl_us)
		? (out_fl_us - p_para->stable_fl_us)
		: (p_para->stable_fl_us - out_fl_us);
	/* if (p_para->adj_diff_final < FS_FL_AUTO_RESTORE_TH) */
	if (diff < FS_FL_AUTO_RESTORE_TH)
		return 0;
	/* !!! FL auto restore mechanism not support LB-MF mode !!! */
	if (frec_chk_if_lut_is_used(fs_inst[idx].p_frecs[0]->m_exp_type))
		return 0;

	get_valid_fl_lc_info(idx, &fl_lc, fl_lc_arr, FS_HDR_MAX);

	/* setup debug info */
	fl_restore_info.magic_num = p_para->magic_num;
	fl_restore_info.req_id = p_para->req_id;
	fl_restore_info.frame_id = p_para->frame_id;

	/* setup FL info */
	fl_restore_info.restored_fl_lc = fl_lc;
	/* ==> for LB-MF sensor */
	memcpy(fl_restore_info.restored_fl_lc_arr, fl_lc_arr,
		(sizeof(unsigned int) * FS_HDR_MAX));

	/* call to frame sync to setup these info */
	fs_setup_fl_restore_status(idx, &fl_restore_info);

	return 1;
#else
	return 0;
#endif
}


/*
 * be careful:
 *    In each frame this API should only be called at once,
 *    otherwise will cause wrong frame monitor data.
 *
 *    So calling this API at/before next vsync coming maybe a good choise.
 */
void fs_alg_setup_frame_monitor_fmeas_data(unsigned int idx)
{
#if defined(FS_UT)
	LOG_INF(
		"[%u] ID:%#x(sidx:%u), call set frame measurement data for pr(c:%u(%u)/n:%u(%u)), vsyncs:%u\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		fs_inst[idx].predicted_fl_us[0],
		fs_inst[idx].predicted_fl_lc[0],
		fs_inst[idx].predicted_fl_us[1],
		fs_inst[idx].predicted_fl_lc[1],
		fs_inst[idx].vsyncs);
#endif

	/* set frame measurement predicted frame length */
	frm_set_frame_measurement(
		idx, fs_inst[idx].vsyncs,
		fs_inst[idx].predicted_fl_us[0],
		fs_inst[idx].predicted_fl_lc[0],
		fs_inst[idx].predicted_fl_us[1],
		fs_inst[idx].predicted_fl_lc[1]);
}


/**
 * receive frame record data from sensor recorder.
 *
 * fs algo will use these information to predict current and
 *     next framelength when calculating vsync diff.
 */
void fs_alg_set_frame_record_st_data(const unsigned int idx,
	struct FrameRecord *recs_ordered[],
	const struct predicted_fl_info_st *fl_info)
{
	unsigned int i = 0;

	/* 0. update last ts bias us info by 'current' ts bias us */
	FS_ATOMIC_SET(FS_ATOMIC_READ(&fs_inst[idx].ts_bias_us),
		&fs_inst[idx].last_ts_bias_us);

	/* 1. set/update frame recoder data */
	for (i = 0; i < RECORDER_DEPTH; ++i)
		fs_inst[idx].p_frecs[i] = recs_ordered[i];

	memcpy(&fs_inst[idx].fl_info, fl_info, sizeof(fs_inst[idx].fl_info));

	fs_inst[idx].predicted_fl_lc[0] = fl_info->pr_curr_fl_lc;
	fs_inst[idx].predicted_fl_us[0] = fl_info->pr_curr_fl_us;
	fs_inst[idx].predicted_fl_lc[1] = fl_info->pr_next_fl_lc;
	fs_inst[idx].predicted_fl_us[1] = fl_info->pr_next_fl_us;

	/* frec_dump_predicted_fl_info_st(idx, fl_info, __func__); */
}
/******************************************************************************/





/*******************************************************************************
 * basic instance operation functions
 ******************************************************************************/
/* dynamic fps info structure */
static inline void fs_alg_sa_setup_dynamic_fps_info_by_dynamic_para(
	const struct FrameSyncDynamicPara *p_para,
	struct fs_dynamic_fps_record_st *fps_info)
{
	/* !!! below all, manually copy / sync each item !!! */
	fps_info->magic_num = p_para->magic_num;
	fps_info->req_id = p_para->req_id;

	fps_info->pure_min_fl_us = p_para->pure_min_fl_us;
	fps_info->min_fl_us = p_para->min_fl_us;
	fps_info->target_min_fl_us = p_para->target_min_fl_us;
	fps_info->stable_fl_us = p_para->stable_fl_us;
}


/* dynamic fps info structure */
static void fs_alg_sa_reset_dynamic_fps_info(const unsigned int idx)
{
	struct fs_dynamic_fps_record_st *p_clr_st;

	fs_spin_lock(&fs_alg_sa_dynamic_fps_op_lock);

	FS_WRITE_BIT(idx, 0, &fs_sa_inst.unstable_fps_bits);
	p_clr_st = &fs_sa_inst.dynamic_fps_recs[idx];
	memset(p_clr_st, 0, sizeof(*p_clr_st));
	p_clr_st = &fs_sa_inst.last_dynamic_fps_recs[idx];
	memset(p_clr_st, 0, sizeof(*p_clr_st));

	fs_spin_unlock(&fs_alg_sa_dynamic_fps_op_lock);
}


/* dynamic fps info structure */
static void fs_alg_sa_update_dynamic_fps_info(const unsigned int idx,
	const struct fs_dynamic_fps_record_st *fps_info)
{
	fs_spin_lock(&fs_alg_sa_dynamic_fps_op_lock);

	fs_sa_inst.dynamic_fps_recs[idx] = *fps_info;

	fs_spin_unlock(&fs_alg_sa_dynamic_fps_op_lock);
}


/* dynamic fps info structure */
static void fs_alg_sa_update_last_dynamic_fps_info(const unsigned int idx)
{
	fs_spin_lock(&fs_alg_sa_dynamic_fps_op_lock);

	fs_sa_inst.last_dynamic_fps_recs[idx] = fs_sa_inst.dynamic_fps_recs[idx];
	FS_WRITE_BIT(idx, 0, &fs_sa_inst.unstable_fps_bits);

	fs_spin_unlock(&fs_alg_sa_dynamic_fps_op_lock);
}


/* dynamic fps info structure */
static void fs_alg_sa_query_all_dynamic_fps_info(
	struct fs_dynamic_fps_record_st fps_info_arr[],
	struct fs_dynamic_fps_record_st last_fps_info_arr[],
	const unsigned int arr_len, unsigned int *p_unstable_fps_bits)
{
	const unsigned int len =
		(arr_len < SENSOR_MAX_NUM) ? arr_len : SENSOR_MAX_NUM;

	fs_spin_lock(&fs_alg_sa_dynamic_fps_op_lock);

	*p_unstable_fps_bits = FS_ATOMIC_READ(&fs_sa_inst.unstable_fps_bits);
	memcpy(fps_info_arr, fs_sa_inst.dynamic_fps_recs,
		sizeof(struct fs_dynamic_fps_record_st) * len);
	memcpy(last_fps_info_arr, fs_sa_inst.last_dynamic_fps_recs,
		sizeof(struct fs_dynamic_fps_record_st) * len);

	fs_spin_unlock(&fs_alg_sa_dynamic_fps_op_lock);
}


/* dynamic parameters info structure */
static void fs_alg_sa_reset_dynamic_para(const unsigned int idx)
{
	struct FrameSyncDynamicPara *p_clr_st;

	fs_spin_lock(&fs_alg_sa_dynamic_para_op_lock);

	p_clr_st = &fs_sa_inst.dynamic_paras[idx];
	memset(p_clr_st, 0, sizeof(*p_clr_st));

	fs_spin_unlock(&fs_alg_sa_dynamic_para_op_lock);
}


/* dynamic parameters info structure */
static void fs_alg_sa_get_dynamic_para(const unsigned int idx,
	struct FrameSyncDynamicPara *p_para)
{
	fs_spin_lock(&fs_alg_sa_dynamic_para_op_lock);

	*p_para = fs_sa_inst.dynamic_paras[idx];

	fs_spin_unlock(&fs_alg_sa_dynamic_para_op_lock);
}


/* dynamic parameters info structure */
static void fs_alg_sa_update_dynamic_para(const unsigned int idx,
	const struct FrameSyncDynamicPara *p_para)
{
	struct fs_dynamic_fps_record_st fps_info = {0};

	fs_alg_sa_setup_dynamic_fps_info_by_dynamic_para(p_para, &fps_info);

	fs_spin_lock(&fs_alg_sa_dynamic_para_op_lock);

	fs_sa_inst.dynamic_paras[idx] = *p_para;
	fs_alg_sa_update_dynamic_fps_info(idx, &fps_info);

	fs_spin_unlock(&fs_alg_sa_dynamic_para_op_lock);
}


static inline void fs_alg_reset_fs_sa_inst(const unsigned int idx)
{
	fs_sa_inst.magic_num[idx] = 0;
	fs_sa_inst.extra_magic_num[idx] = 0;

	fs_alg_sa_reset_dynamic_para(idx);
	fs_alg_sa_reset_dynamic_fps_info(idx);
}


void fs_alg_reset_fs_inst(const unsigned int idx)
{
	struct FrameSyncInst *p_clr_st = &fs_inst[idx];

	memset(p_clr_st, 0, sizeof(*p_clr_st));

	fs_alg_reset_fs_sa_inst(idx);
}


void fs_alg_reset_vsync_data(const unsigned int idx)
{
	unsigned int i = 0;

	fs_inst[idx].vsyncs = 0;
	fs_inst[idx].last_vts = 0;
	fs_inst[idx].cur_tick = 0;

	for (i = 0; i < VSYNCS_MAX; ++i)
		fs_inst[idx].timestamps[i] = 0;
}
/******************************************************************************/





/*******************************************************************************
 * Dump & Debug function
 ******************************************************************************/
void fs_alg_get_cur_frec_data(unsigned int idx,
	unsigned int *p_fl_lc, unsigned int *p_shut_lc)
{
	if (p_fl_lc != NULL)
		*p_fl_lc = fs_inst[idx].p_frecs[0]->framelength_lc;

	if (p_shut_lc != NULL)
		*p_shut_lc = fs_inst[idx].p_frecs[0]->shutter_lc;
}


void fs_alg_get_fs_inst_ts_data(unsigned int idx,
	unsigned int *p_tg, unsigned long long ts_arr[],
	unsigned long long *p_last_vts, unsigned long long *p_time_after_sof,
	unsigned long long *p_cur_tick, unsigned int *p_vsyncs)
{
	const struct FrameSyncInst *p_inst = NULL;
	unsigned int i = 0;

	p_inst = &fs_inst[idx];

	if (p_tg != NULL)
		*p_tg = p_inst->tg;

	if (p_last_vts != NULL)
		*p_last_vts = p_inst->last_vts;

	if (p_time_after_sof != NULL) {
		*p_time_after_sof = (p_inst->cur_tick != 0)
			? (calc_time_after_sof(
				p_inst->last_vts, p_inst->cur_tick, tick_factor))
			: 0;
	}

	if (p_cur_tick != NULL)
		*p_cur_tick = p_inst->cur_tick;

	if (p_vsyncs != NULL)
		*p_vsyncs = p_inst->vsyncs;

	if (ts_arr != NULL) {
		for (i = 0; i < VSYNCS_MAX; ++i)
			ts_arr[i] = p_inst->timestamps[i];
	}
}


static void fs_alg_sa_tsrec_m_s_msg_connector(
	const unsigned int m_idx, const unsigned int s_idx,
	const unsigned int log_str_len, char *log_buf, int len,
	const char *caller)
{
#if defined(USING_TSREC)
	const struct mtk_cam_seninf_tsrec_timestamp_info
		*p_ts_info_s = frm_get_tsrec_timestamp_info_ptr(s_idx);
	const struct mtk_cam_seninf_tsrec_timestamp_info
		*p_ts_info_m = frm_get_tsrec_timestamp_info_ptr(m_idx);
	const unsigned int exp_id = TSREC_1ST_EXP_ID;

	if (unlikely((p_ts_info_s == NULL) || (p_ts_info_m == NULL))) {
		LOG_MUST(
			"[%s] ERROR: USING_TSREC timestamp, but get p_ts_info_s:%p/p_ts_info_m:%p, skip dump tsrec ts info\n",
			caller, p_ts_info_s, p_ts_info_m);
		return;
	}

	/* print TSREC info */
	FS_SNPRF(log_str_len, log_buf, len,
		", tsrec[(%u/inf:%u,irq(%llu/%llu)(+%llu)):(0:(%llu/%llu/%llu/%llu)/1:(%llu/%llu/%llu/%llu)/2:(%llu/%llu/%llu/%llu)) / (%u/inf:%u,irq(%llu/%llu)(+%llu)):(0:(%llu/%llu/%llu/%llu)/1:(%llu/%llu/%llu/%llu)/2:(%llu/%llu/%llu/%llu))]",
		p_ts_info_s->tsrec_no, p_ts_info_s->seninf_idx,
		p_ts_info_s->irq_sys_time_ns / 1000,
		p_ts_info_s->irq_tsrec_ts_us,
		(convert_tick_2_timestamp(
			(p_ts_info_s->tsrec_curr_tick
				- (p_ts_info_s->exp_recs[exp_id].ts_us[0]
					* p_ts_info_s->tick_factor)),
			p_ts_info_s->tick_factor)),
		p_ts_info_s->exp_recs[0].ts_us[0],
		p_ts_info_s->exp_recs[0].ts_us[1],
		p_ts_info_s->exp_recs[0].ts_us[2],
		p_ts_info_s->exp_recs[0].ts_us[3],
		p_ts_info_s->exp_recs[1].ts_us[0],
		p_ts_info_s->exp_recs[1].ts_us[1],
		p_ts_info_s->exp_recs[1].ts_us[2],
		p_ts_info_s->exp_recs[1].ts_us[3],
		p_ts_info_s->exp_recs[2].ts_us[0],
		p_ts_info_s->exp_recs[2].ts_us[1],
		p_ts_info_s->exp_recs[2].ts_us[2],
		p_ts_info_s->exp_recs[2].ts_us[3],
		p_ts_info_m->tsrec_no, p_ts_info_m->seninf_idx,
		p_ts_info_m->irq_sys_time_ns / 1000,
		p_ts_info_m->irq_tsrec_ts_us,
		(convert_tick_2_timestamp(
			(p_ts_info_m->tsrec_curr_tick
				- (p_ts_info_m->exp_recs[exp_id].ts_us[0]
					* p_ts_info_m->tick_factor)),
			p_ts_info_m->tick_factor)),
		p_ts_info_m->exp_recs[0].ts_us[0],
		p_ts_info_m->exp_recs[0].ts_us[1],
		p_ts_info_m->exp_recs[0].ts_us[2],
		p_ts_info_m->exp_recs[0].ts_us[3],
		p_ts_info_m->exp_recs[1].ts_us[0],
		p_ts_info_m->exp_recs[1].ts_us[1],
		p_ts_info_m->exp_recs[1].ts_us[2],
		p_ts_info_m->exp_recs[1].ts_us[3],
		p_ts_info_m->exp_recs[2].ts_us[0],
		p_ts_info_m->exp_recs[2].ts_us[1],
		p_ts_info_m->exp_recs[2].ts_us[2],
		p_ts_info_m->exp_recs[2].ts_us[3]);
#endif // USING_TSREC
}


void fs_alg_sa_dynamic_fps_info_arr_msg_connector(const unsigned int idx,
	const struct fs_dynamic_fps_record_st fps_info_arr[],
	const struct fs_dynamic_fps_record_st last_fps_info_arr[],
	const unsigned int arr_len, const unsigned int valid_bits,
	const unsigned int log_str_len, char *log_buf, int len,
	const char *caller)
{
	unsigned int i;

	FS_SNPRF(log_str_len, log_buf, len, ", FL((");

	for (i = 0; i < arr_len; ++i) {
		if (((valid_bits >> i) & 1UL) == 0)
			continue;

		FS_SNPRF(log_str_len, log_buf, len,
			"[%u]((#%u/%d)[%u/%u/%u],(#%u/%d)[%u/%u/%u])/",
			i,
			fps_info_arr[i].magic_num,
			fps_info_arr[i].req_id,
			fps_info_arr[i].pure_min_fl_us,
			fps_info_arr[i].min_fl_us,
			fps_info_arr[i].target_min_fl_us,
			last_fps_info_arr[i].magic_num,
			last_fps_info_arr[i].req_id,
			last_fps_info_arr[i].pure_min_fl_us,
			last_fps_info_arr[i].min_fl_us,
			last_fps_info_arr[i].target_min_fl_us);
	}

	FS_SNPRF(log_str_len, log_buf, len, ")");
}


void fs_alg_sa_ts_info_dynamic_msg_connector(const unsigned int idx,
	const unsigned int log_str_len, char *log_buf, int len,
	const char *caller)
{
	const struct FrameSyncDynamicPara *p_para = NULL;
	const struct FrameSyncInst *p_inst = NULL;
	unsigned long long dynamic_after_us = 0, after_us = 0;
	unsigned int act_fl[(VSYNCS_MAX - 1)] = {0};

	p_para = &fs_sa_inst.dynamic_paras[idx];
	p_inst = &fs_inst[idx];

	fs_util_calc_act_fl(p_inst->timestamps, act_fl, VSYNCS_MAX, tick_factor);

	if (p_para->cur_tick != 0) {
		dynamic_after_us = calc_time_after_sof(
			p_para->last_ts, p_para->cur_tick, tick_factor);
	}
	if (p_inst->cur_tick != 0) {
		after_us = calc_time_after_sof(
			p_inst->last_vts, p_inst->cur_tick, tick_factor);
	}

	if (p_inst->ts_src_type == FS_TS_SRC_EINT) {
		/* e.g., MAIN source is TSREC but flow is triggered by EINT */
		FS_SNPRF(log_str_len, log_buf, len,
			", ts((%u)%u,%llu(+%u),%u/%u/%u,%llu/%llu/%llu/%llu)",
			p_para->ts_src_type,
			frm_get_eint_no(idx),
			p_para->last_ts,
			p_para->ts_offset,
			act_fl[0], act_fl[1], act_fl[2],
			p_inst->timestamps[0],
			p_inst->timestamps[1],
			p_inst->timestamps[2],
			p_inst->timestamps[3]);

		fs_util_tsrec_dynamic_msg_connector(idx,
			log_str_len, log_buf, len, __func__);
	} else if (frm_get_ts_src_type() != FS_TS_SRC_TSREC) {
		/* e.g., using CCU */
		FS_SNPRF(log_str_len, log_buf, len,
			", ts(%u,%llu(+%llu),%u/%u/%u,%llu(+%llu)/%llu/%llu/%llu)",
			p_inst->tg,
			p_para->last_ts, dynamic_after_us,
			act_fl[0], act_fl[1], act_fl[2],
			p_inst->timestamps[0], after_us,
			p_inst->timestamps[1],
			p_inst->timestamps[2],
			p_inst->timestamps[3]);
	} else {
		/* e.g., using TSREC */
		FS_SNPRF(log_str_len, log_buf, len,
			", ts(%u,%llu(+%llu),%u/%u/%u)",
			p_inst->tg,
			p_para->last_ts, dynamic_after_us,
			act_fl[0], act_fl[1], act_fl[2]);

		fs_util_tsrec_dynamic_msg_connector(idx,
			log_str_len, log_buf, len, __func__);
	}
}


static void fs_alg_sa_ts_info_m_s_msg_connector(
	const unsigned int m_idx, const unsigned int s_idx,
	const struct FrameSyncDynamicPara *p_para_m,
	struct FrameSyncDynamicPara *p_para_s,
	const unsigned int log_str_len, char *log_buf, int len,
	const char *caller)
{
	const struct FrameSyncInst *p_inst_m = NULL, *p_inst_s = NULL;
	unsigned long long after_us_m = 0, after_us_s = 0;
	unsigned int act_fl_m[(VSYNCS_MAX - 1)] = {0};
	unsigned int act_fl_s[(VSYNCS_MAX - 1)] = {0};
	unsigned int is_trigger_by_eint = 0;

	p_inst_m = &fs_inst[m_idx];
	p_inst_s = &fs_inst[s_idx];

	fs_util_calc_act_fl(
		p_inst_m->timestamps, act_fl_m, VSYNCS_MAX, tick_factor);
	fs_util_calc_act_fl(
		p_inst_s->timestamps, act_fl_s, VSYNCS_MAX, tick_factor);

	if (p_inst_m->cur_tick != 0) {
		after_us_m = calc_time_after_sof(
			p_inst_m->timestamps[0], p_inst_m->cur_tick, tick_factor);
	}
	if (p_inst_s->cur_tick != 0) {
		after_us_s = calc_time_after_sof(
			p_inst_s->timestamps[0], p_inst_s->cur_tick, tick_factor);
	}
	is_trigger_by_eint =
		(p_inst_m->ts_src_type == FS_TS_SRC_EINT
			|| p_inst_s->ts_src_type == FS_TS_SRC_EINT)
		? 1 : 0;

	if (is_trigger_by_eint) {
		/* e.g., MAIN source is TSREC but flow is triggered by EINT */
		FS_SNPRF(log_str_len, log_buf, len,
			", ts((%u)%u,%llu(+%u),%u/%u/%u,%llu/%llu/%llu/%llu, (%u)%u,%llu(+%u),%u/%u/%u,%llu/%llu/%llu/%llu)",
			p_para_s->ts_src_type,
			frm_get_eint_no(s_idx),
			p_para_s->last_ts, /* fs_sa_inst.dynamic_paras[s_idx].last_ts, */
			p_para_s->ts_offset,
			act_fl_s[0], act_fl_s[1], act_fl_s[2],
			p_inst_s->timestamps[0],
			p_inst_s->timestamps[1],
			p_inst_s->timestamps[2],
			p_inst_s->timestamps[3],
			p_para_m->ts_src_type,
			frm_get_eint_no(m_idx),
			p_para_m->last_ts, /* fs_sa_inst.dynamic_paras[m_idx].last_ts, */
			p_para_m->ts_offset,
			act_fl_m[0], act_fl_m[1], act_fl_m[2],
			p_inst_m->timestamps[0],
			p_inst_m->timestamps[1],
			p_inst_m->timestamps[2],
			p_inst_m->timestamps[3]);

		fs_alg_sa_tsrec_m_s_msg_connector(
			m_idx, s_idx, log_str_len, log_buf, len, caller);
	} else if (frm_get_ts_src_type() != FS_TS_SRC_TSREC) {
		/* e.g., using CCU */
		FS_SNPRF(log_str_len, log_buf, len,
			", ts(%u,%llu,%u/%u/%u,%llu(+%llu)/%llu/%llu/%llu, %u,%llu,%u/%u/%u,%llu(+%llu)/%llu/%llu/%llu)",
			p_inst_s->tg,
			p_para_s->last_ts,
			// fs_sa_inst.dynamic_paras[s_idx].last_ts,
			act_fl_s[0], act_fl_s[1], act_fl_s[2],
			p_inst_s->timestamps[0], after_us_s,
			p_inst_s->timestamps[1],
			p_inst_s->timestamps[2],
			p_inst_s->timestamps[3],
			p_inst_m->tg,
			p_para_m->last_ts,
			// fs_sa_inst.dynamic_paras[m_idx].last_ts,
			act_fl_m[0], act_fl_m[1], act_fl_m[2],
			p_inst_m->timestamps[0], after_us_m,
			p_inst_m->timestamps[1],
			p_inst_m->timestamps[2],
			p_inst_m->timestamps[3]);
	} else {
		/* e.g., using TSREC */
		FS_SNPRF(log_str_len, log_buf, len,
			", ts(%u,%llu,%u/%u/%u, %u,%llu,%u/%u/%u)",
			p_inst_s->tg,
			p_para_s->last_ts,
			// fs_sa_inst.dynamic_paras[s_idx].last_ts,
			act_fl_s[0], act_fl_s[1], act_fl_s[2],
			p_inst_m->tg,
			p_para_m->last_ts,
			// fs_sa_inst.dynamic_paras[m_idx].last_ts,
			act_fl_m[0], act_fl_m[1], act_fl_m[2]);

		fs_alg_sa_tsrec_m_s_msg_connector(
			m_idx, s_idx, log_str_len, log_buf, len, caller);
	}
}


static inline void fs_alg_sa_adjust_diff_m_s_general_msg_connector(
	const unsigned int m_idx, const unsigned int s_idx,
	const struct FrameSyncDynamicPara *p_para_m,
	struct FrameSyncDynamicPara *p_para_s,
	const unsigned int log_str_len, char *log_buf, int len,
	const char *caller)
{
	FS_SNPRF(log_str_len, log_buf, len,
		", [((%u:%u)c:%u/n:%u/o:%u/s:%u/e:%u(%u/%u|m_p:%u)/t:%u(%u/%u),%u(%u->%u))/((%u:%u)c:%u/n:%u/o:%u/s:%u/e:%u(%u/%u)/t:%u(%u/%u),%u(%u->%u))],mFL:%u/%u,lineT:%u/%u,roT(%#x):%u/%u,anch:%d/%d",
		fs_inst[s_idx].fl_active_delay,
		p_para_s->delta,
		p_para_s->pred_fl_us[0],
		p_para_s->pred_fl_us[1],
		p_para_s->out_fl_us,
		p_para_s->stable_fl_us,
		p_para_s->ts_bias_us,
		p_para_s->pred_next_exp_rd_offset_us[FS_HDR_LE],
		p_para_s->pred_next_exp_rd_offset_us[FS_HDR_SE],
		p_para_s->m_last_ts_bias_us,
		p_para_s->tag_bias_us,
		p_para_s->f_tag,
		get_valid_frame_cell_size(s_idx),
		p_para_s->target_min_fl_us,
		p_para_s->fps_status,
		p_para_s->fps_status_aligned,
		fs_inst[m_idx].fl_active_delay,
		p_para_m->delta,
		p_para_m->pred_fl_us[0],
		p_para_m->pred_fl_us[1],
		p_para_m->out_fl_us,
		p_para_m->stable_fl_us,
		p_para_m->ts_bias_us,
		p_para_m->pred_next_exp_rd_offset_us[FS_HDR_LE],
		p_para_m->pred_next_exp_rd_offset_us[FS_HDR_SE],
		p_para_m->tag_bias_us,
		p_para_m->f_tag,
		get_valid_frame_cell_size(m_idx),
		p_para_m->target_min_fl_us,
		p_para_m->fps_status,
		p_para_m->fps_status_aligned,
		fs_inst[s_idx].min_fl_lc,
		fs_inst[m_idx].min_fl_lc,
		fs_inst[s_idx].lineTimeInNs,
		fs_inst[m_idx].lineTimeInNs,
		p_para_s->sa_cfg.rout_center_en_bits,
		fs_inst[s_idx].readout_time_us,
		fs_inst[m_idx].readout_time_us,
		p_para_s->anchor_bias_us,
		p_para_m->anchor_bias_us);

	fs_alg_sa_ts_info_m_s_msg_connector(
		m_idx, s_idx, p_para_m, p_para_s,
		log_str_len, log_buf, len, caller);
}


static inline void fs_alg_dump_streaming_data(unsigned int idx)
{
	LOG_MUST(
		"[%u] ID:%#x(sidx:%u/inf:%u), tg:%u, fl_delay:%u, fl_lc(def/min/max):%u/%u/%u, def_shut_lc:%u, lineTime:%u, hdr_exp: c(%u/%u/%u/%u/%u, %u/%u), prev(%u/%u/%u/%u/%u, %u/%u), cnt:(mode/ae)\n",
		idx,
		fs_get_reg_sensor_id(idx),
		fs_get_reg_sensor_idx(idx),
		fs_get_reg_sensor_inf_idx(idx),
		fs_inst[idx].tg,
		fs_inst[idx].fl_active_delay,
		fs_inst[idx].def_min_fl_lc,
		fs_inst[idx].min_fl_lc,
		fs_inst[idx].max_fl_lc,
		fs_inst[idx].def_shutter_lc,
		fs_inst[idx].lineTimeInNs,
		fs_inst[idx].hdr_exp.exp_lc[0],
		fs_inst[idx].hdr_exp.exp_lc[1],
		fs_inst[idx].hdr_exp.exp_lc[2],
		fs_inst[idx].hdr_exp.exp_lc[3],
		fs_inst[idx].hdr_exp.exp_lc[4],
		fs_inst[idx].hdr_exp.mode_exp_cnt,
		fs_inst[idx].hdr_exp.ae_exp_cnt,
		fs_inst[idx].prev_hdr_exp.exp_lc[0],
		fs_inst[idx].prev_hdr_exp.exp_lc[1],
		fs_inst[idx].prev_hdr_exp.exp_lc[2],
		fs_inst[idx].prev_hdr_exp.exp_lc[3],
		fs_inst[idx].prev_hdr_exp.exp_lc[4],
		fs_inst[idx].prev_hdr_exp.mode_exp_cnt,
		fs_inst[idx].prev_hdr_exp.ae_exp_cnt);
}


static inline void fs_alg_dump_perframe_data(unsigned int idx)
{
	LOG_INF(
		"[%u] ID:%#x(sidx:%u/inf:%u), flk_en:%u, min_fl:%u(%u), shutter:%u(%u), margin:%u(%u), lineTime(ns):%u, hdr_exp: c(%u(%u)/%u(%u)/%u(%u)/%u(%u)/%u(%u), %u/%u), prev(%u(%u)/%u(%u)/%u(%u)/%u(%u)/%u(%u), %u/%u)\n",
		idx,
		fs_get_reg_sensor_id(idx),
		fs_get_reg_sensor_idx(idx),
		fs_get_reg_sensor_inf_idx(idx),
		fs_inst[idx].flicker_en,
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].min_fl_lc),
		fs_inst[idx].min_fl_lc,
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].shutter_lc),
		fs_inst[idx].shutter_lc,
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].margin_lc),
		fs_inst[idx].margin_lc,
		fs_inst[idx].lineTimeInNs,
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].hdr_exp.exp_lc[0]),
		fs_inst[idx].hdr_exp.exp_lc[0],
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].hdr_exp.exp_lc[1]),
		fs_inst[idx].hdr_exp.exp_lc[1],
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].hdr_exp.exp_lc[2]),
		fs_inst[idx].hdr_exp.exp_lc[2],
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].hdr_exp.exp_lc[3]),
		fs_inst[idx].hdr_exp.exp_lc[3],
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].hdr_exp.exp_lc[4]),
		fs_inst[idx].hdr_exp.exp_lc[4],
		fs_inst[idx].hdr_exp.mode_exp_cnt,
		fs_inst[idx].hdr_exp.ae_exp_cnt,
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].prev_hdr_exp.exp_lc[0]),
		fs_inst[idx].prev_hdr_exp.exp_lc[0],
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].prev_hdr_exp.exp_lc[1]),
		fs_inst[idx].prev_hdr_exp.exp_lc[1],
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].prev_hdr_exp.exp_lc[2]),
		fs_inst[idx].prev_hdr_exp.exp_lc[2],
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].prev_hdr_exp.exp_lc[3]),
		fs_inst[idx].prev_hdr_exp.exp_lc[3],
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].prev_hdr_exp.exp_lc[4]),
		fs_inst[idx].prev_hdr_exp.exp_lc[4],
		fs_inst[idx].prev_hdr_exp.mode_exp_cnt,
		fs_inst[idx].prev_hdr_exp.ae_exp_cnt);
}


/* for debug using, dump all data in instance */
void fs_alg_dump_fs_inst_data(const unsigned int idx)
{
	LOG_MUST(
		"[%u] ID:%#x(sidx:%u/inf:%u), (req:%d/f:%u/%u), tg:%u, fdelay:%u, fl_lc(def/min/max/out):%u/%u/%u/%u(%u), pred_fl(c:%u(%u)/n:%u(%u)), shut_lc:%u(def:%u), margin_lc:%u, flk_en:%u, lineTime:%u, readout(us):%u, f_cell:%u, f_tag:%u, n_1:%u, hdr_exp(c(%u/%u/%u/%u/%u, %u/%u, %u/%u), prev(%u/%u/%u/%u/%u, %u/%u, %u/%u), cnt:(mode/ae), read(len/margin)), anchor:%d, ts(%llu/%llu/%llu/%llu, %llu/+(%llu)/%u)\n",
		idx,
		fs_get_reg_sensor_id(idx),
		fs_get_reg_sensor_idx(idx),
		fs_get_reg_sensor_inf_idx(idx),
		fs_inst[idx].req_id,
		fs_inst[idx].frame_id,
		fs_inst[idx].sof_cnt,
		fs_inst[idx].tg,
		fs_inst[idx].fl_active_delay,
		fs_inst[idx].def_min_fl_lc,
		fs_inst[idx].min_fl_lc,
		fs_inst[idx].max_fl_lc,
		fs_inst[idx].output_fl_lc,
		fs_inst[idx].output_fl_us,
		fs_inst[idx].predicted_fl_us[0],
		fs_inst[idx].predicted_fl_lc[0],
		fs_inst[idx].predicted_fl_us[1],
		fs_inst[idx].predicted_fl_lc[1],
		fs_inst[idx].shutter_lc,
		fs_inst[idx].def_shutter_lc,
		fs_inst[idx].margin_lc,
		fs_inst[idx].flicker_en,
		fs_inst[idx].lineTimeInNs,
		fs_inst[idx].readout_time_us,
		fs_inst[idx].frame_tag,
		fs_inst[idx].frame_cell_size,
		fs_inst[idx].n_1_on_off,
		fs_inst[idx].hdr_exp.exp_lc[0],
		fs_inst[idx].hdr_exp.exp_lc[1],
		fs_inst[idx].hdr_exp.exp_lc[2],
		fs_inst[idx].hdr_exp.exp_lc[3],
		fs_inst[idx].hdr_exp.exp_lc[4],
		fs_inst[idx].hdr_exp.mode_exp_cnt,
		fs_inst[idx].hdr_exp.ae_exp_cnt,
		fs_inst[idx].hdr_exp.readout_len_lc,
		fs_inst[idx].hdr_exp.read_margin_lc,
		fs_inst[idx].prev_hdr_exp.exp_lc[0],
		fs_inst[idx].prev_hdr_exp.exp_lc[1],
		fs_inst[idx].prev_hdr_exp.exp_lc[2],
		fs_inst[idx].prev_hdr_exp.exp_lc[3],
		fs_inst[idx].prev_hdr_exp.exp_lc[4],
		fs_inst[idx].prev_hdr_exp.mode_exp_cnt,
		fs_inst[idx].prev_hdr_exp.ae_exp_cnt,
		fs_inst[idx].prev_hdr_exp.readout_len_lc,
		fs_inst[idx].prev_hdr_exp.read_margin_lc,
		FS_ATOMIC_READ(&fs_inst[idx].anchor_bias_us),
		fs_inst[idx].timestamps[0],
		fs_inst[idx].timestamps[1],
		fs_inst[idx].timestamps[2],
		fs_inst[idx].timestamps[3],
		fs_inst[idx].last_vts,
		fs_inst[idx].cur_tick,
		fs_inst[idx].vsyncs);
}


/* for debug using, dump all data in all instance */
void fs_alg_dump_all_fs_inst_data(void)
{
	unsigned int i = 0;

	for (i = 0; i < SENSOR_MAX_NUM; ++i)
		fs_alg_dump_fs_inst_data(i);
}


#ifdef SUPPORT_FS_NEW_METHOD
void fs_alg_sa_dump_dynamic_para(const unsigned int idx)
{
	const unsigned int log_str_len = LOG_BUF_STR_LEN;
	struct FrameSyncDynamicPara para = {0};
	char *log_buf = NULL;
	int len = 0, ret;

	ret = alloc_log_buf(log_str_len, &log_buf);
	if (unlikely(ret != 0)) {
		LOG_MUST("ERROR: log_buf allocate memory failed\n");
		return;
	}

	/* get data that want to print out */
	fs_alg_sa_get_dynamic_para(idx, &para);

	FS_SNPRF(log_str_len, log_buf, len,
		"[%u] ID:%#x(sidx:%u), #%u(%u), req:%d/f:%u, out_fl:%u(%u) +%lld(%u), flk(%u), ref([%d](#%u)), adj_diff(M:%u/corr:%lld(%u))(%lld(v:%u/chg:%u/sub:%u(min:%u)/ask_chg:%u)/%lld,+%lld(%#x),unstable:%u/%u), ((%u:%u)c:%u/n:%u/o:%u/s:%u/e:%u(%u/%u|m_p:%u)/t:%u(%u/%u),%u),lineT:%u,routT:%u,anchor:%d",
		idx,
		fs_get_reg_sensor_id(idx),
		fs_get_reg_sensor_idx(idx),
		para.magic_num,
		para.extra_magic_num,
		para.req_id,
		para.frame_id,
		para.out_fl_us,
		convert2LineCount(
			fs_inst[idx].lineTimeInNs,
			para.out_fl_us),
		para.adj_diff_final,
		para.need_auto_restore_fl,
		fs_inst[idx].flicker_en,
		para.master_idx,
		para.ref_m_idx_magic_num,
		para.is_master,
		para.corrected_fl_diff,
		para.is_correction_suitable,
		para.adj_diff_s,
		para.adj_or_not,
		para.chg_master,
		para.subtract_fl,
		para.out_fl_us_min,
		para.ask_for_chg,
		para.adj_diff_m,
		para.pred_fl_err_us_m,
		para.pred_fl_err_chk_bits_m,
		para.fps_status,
		para.fps_status_aligned,
		fs_inst[idx].fl_active_delay,
		para.delta,
		para.pred_fl_us[0],
		para.pred_fl_us[1],
		para.out_fl_us_init,
		para.stable_fl_us,
		para.ts_bias_us,
		para.pred_next_exp_rd_offset_us[FS_HDR_LE],
		para.pred_next_exp_rd_offset_us[FS_HDR_SE],
		para.m_last_ts_bias_us,
		para.tag_bias_us,
		para.f_tag,
		para.f_cell,
		para.target_min_fl_us,
		fs_inst[idx].lineTimeInNs,
		fs_inst[idx].readout_time_us,
		para.anchor_bias_us);

	/* print per-frame config info */
	FS_SNPRF(log_str_len, log_buf, len,
		", cfg(idx(%u/m:%d)/aS(m:%d/s:%#x)/vS:%#x/routC:%#x/Evt(%u)(bcastT:%u))",
		para.sa_cfg.idx,
		para.sa_cfg.m_idx,
		para.sa_cfg.async_m_idx,
		para.sa_cfg.async_s_bits,
		para.sa_cfg.valid_sync_bits,
		para.sa_cfg.rout_center_en_bits,
		para.sa_cfg.extra_event.is_valid,
		para.sa_cfg.extra_event.bcast_event_type);

	/* print timestamp related info */
	fs_alg_sa_ts_info_dynamic_msg_connector(idx,
		log_str_len, log_buf, len, __func__);

	LOG_MUST_LOCK("%s\n", log_buf);
	FS_TRACE_PR_LOG_MUST("%s", log_buf);

	FS_FREE(log_buf);
}
#endif // SUPPORT_FS_NEW_METHOD
/******************************************************************************/





/*******************************************************************************
 * frame length record structure's functions
 ******************************************************************************/
static void fs_alg_init_fl_rec_st(const unsigned int idx)
{
	const unsigned int f_cell = get_valid_frame_cell_size(idx);
	const unsigned int f_tag = fs_inst[idx].frame_tag;

	/* for m-stream case (only tag 0 need reset) */
	if (f_cell == 2 && f_tag != 0)
		return;

	memset(&fs_inst[idx].fl_rec, 0, sizeof(fs_inst[idx].fl_rec));
}


static void fs_alg_update_fl_rec_st_fl_info(const unsigned int idx,
	const unsigned int ref_magic_num,
	const unsigned int target_min_fl_us, const unsigned int out_fl_us)
{
	const unsigned int f_cell = get_valid_frame_cell_size(idx);
	const unsigned int f_tag = fs_inst[idx].frame_tag;

	if (f_tag >= FS_FL_RECORD_DEPTH) {
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u), f_tag:%u >= FS_FL_RECORD_DEPTH:%u, f_cell:%u, array index overflow, return   [idx:%u, target_min_fl_us:%u, out_fl_us:%u]\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			f_tag,
			FS_FL_RECORD_DEPTH,
			f_cell,
			idx,
			target_min_fl_us,
			out_fl_us);
		return;
	}

	/* update debug info */
	fs_inst[idx].fl_rec[f_tag].ref_magic_num = ref_magic_num;

	/* update/setup frame length info */
	/* target_min_fl_us is equal to (fl_us * f_cell) */
	fs_inst[idx].fl_rec[f_tag].target_min_fl_us =
		(f_cell != 0) ? (target_min_fl_us/f_cell) : target_min_fl_us;
	fs_inst[idx].fl_rec[f_tag].out_fl_us = out_fl_us;

	LOG_INF(
		"[%u] ID:%#x(sidx:%u), fl_rec(0:(#%u, %u/%u), 1:(#%u, %u/%u), 2:(#%u, %u/%u), 3:(#%u, %u/%u), 4:(#%u, %u/%u), (target_min_fl_us/out_fl_us)), f_tag:%u/f_cell:%u\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		fs_inst[idx].fl_rec[0].ref_magic_num,
		fs_inst[idx].fl_rec[0].target_min_fl_us,
		fs_inst[idx].fl_rec[0].out_fl_us,
		fs_inst[idx].fl_rec[1].ref_magic_num,
		fs_inst[idx].fl_rec[1].target_min_fl_us,
		fs_inst[idx].fl_rec[1].out_fl_us,
		fs_inst[idx].fl_rec[2].ref_magic_num,
		fs_inst[idx].fl_rec[2].target_min_fl_us,
		fs_inst[idx].fl_rec[2].out_fl_us,
		fs_inst[idx].fl_rec[3].ref_magic_num,
		fs_inst[idx].fl_rec[3].target_min_fl_us,
		fs_inst[idx].fl_rec[3].out_fl_us,
		fs_inst[idx].fl_rec[4].ref_magic_num,
		fs_inst[idx].fl_rec[4].target_min_fl_us,
		fs_inst[idx].fl_rec[4].out_fl_us,
		f_tag,
		f_cell);
}


void fs_alg_get_fl_rec_st_info(const unsigned int idx,
	unsigned int *p_target_min_fl_us, unsigned int *p_out_fl_us)
{
	const unsigned int f_cell = get_valid_frame_cell_size(idx);
	const unsigned int f_tag = fs_inst[idx].frame_tag;
	unsigned int i = 0;

	/* error handle (unexpected case) */
	if (p_target_min_fl_us == NULL || p_out_fl_us == NULL) {
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u), get non-valid p_target_min_fl_us:%p or p_out_fl_us:%p, return\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			p_target_min_fl_us,
			p_out_fl_us);
		return;
	}

	/* clear data */
	*p_target_min_fl_us = 0;
	*p_out_fl_us = 0;

	if (f_tag >= FS_FL_RECORD_DEPTH) {
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u), f_tag:%u >= FS_FL_RECORD_DEPTH:%u, f_cell:%u, array index overflow, return\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			f_tag,
			FS_FL_RECORD_DEPTH,
			f_cell);
		return;
	}

	/* copy data */
	for (i = 0; i < f_cell; ++i) {
		*p_target_min_fl_us += fs_inst[idx].fl_rec[i].target_min_fl_us;
		*p_out_fl_us += fs_inst[idx].fl_rec[i].out_fl_us;
	}

	LOG_INF_CAT_LOCK(LOG_FS_USER_QUERY_INFO,
		"[%u] ID:%#x(sidx:%u), target_min_fl_us:%u/out_fl_us:%u, fl_rec(0:(#%u, %u/%u), 1:(#%u, %u/%u), 2:(#%u, %u/%u), 3:(#%u, %u/%u), 4:(#%u, %u/%u), (target_min_fl_us/out_fl_us)), f_cell:%u\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		*p_target_min_fl_us,
		*p_out_fl_us,
		fs_inst[idx].fl_rec[0].ref_magic_num,
		fs_inst[idx].fl_rec[0].target_min_fl_us,
		fs_inst[idx].fl_rec[0].out_fl_us,
		fs_inst[idx].fl_rec[1].ref_magic_num,
		fs_inst[idx].fl_rec[1].target_min_fl_us,
		fs_inst[idx].fl_rec[1].out_fl_us,
		fs_inst[idx].fl_rec[2].ref_magic_num,
		fs_inst[idx].fl_rec[2].target_min_fl_us,
		fs_inst[idx].fl_rec[2].out_fl_us,
		fs_inst[idx].fl_rec[3].ref_magic_num,
		fs_inst[idx].fl_rec[3].target_min_fl_us,
		fs_inst[idx].fl_rec[3].out_fl_us,
		fs_inst[idx].fl_rec[4].ref_magic_num,
		fs_inst[idx].fl_rec[4].target_min_fl_us,
		fs_inst[idx].fl_rec[4].out_fl_us,
		f_cell);
}
/******************************************************************************/





/*******************************************************************************
 * For MW get anchor bias (to 1st-SOF) functions
 ******************************************************************************/
void fs_alg_get_latest_anchor_info(const unsigned int idx,
	long long *p_anchor_bias_ns)
{
	if (unlikely(p_anchor_bias_ns == NULL)) {
		LOG_MUST(
			"ERROR: [%u][sidx:%u], #%u, (req:%d/f:%u/%u), p_anchor_bias_ns:%p is nullptr, return\n",
			idx,
			fs_get_reg_sensor_idx(idx),
			fs_sa_inst.dynamic_paras[idx].magic_num,
			fs_inst[idx].req_id,
			fs_inst[idx].frame_id,
			fs_inst[idx].sof_cnt,
			p_anchor_bias_ns);
		return;
	}

	*p_anchor_bias_ns = (long long)
		(1000 * FS_ATOMIC_READ(&fs_inst[idx].anchor_bias_us));
}
/******************************************************************************/





/*******************************************************************************
 * fs algo static functions
 ******************************************************************************/
static unsigned int fs_alg_get_hdr_equivalent_exp_lc(const unsigned int idx)
{
	unsigned int mode_exp_cnt_1 = fs_inst[idx].hdr_exp.mode_exp_cnt;
	unsigned int mode_exp_cnt_2 = fs_inst[idx].prev_hdr_exp.mode_exp_cnt;
	unsigned int result_1 = 0, result_2 = 0;
	unsigned int exp_lc = 0;
	unsigned int i = 0;

	/* calc. method 1. */
	for (i = 0; i < mode_exp_cnt_1; ++i) {
		int hdr_idx = hdr_exp_idx_map[mode_exp_cnt_1][i];

		if (hdr_idx < 0) {
			LOG_INF(
				"ERROR: [%u] ID:%#x(sidx:%u), hdr_exp_idx_map[%u][%u] = %d\n",
				idx,
				fs_inst[idx].sensor_id,
				fs_inst[idx].sensor_idx,
				mode_exp_cnt_1,
				i,
				hdr_idx);

			return 0;
		}

		result_1 += fs_inst[idx].hdr_exp.exp_lc[hdr_idx];
	}

	/* calc. method 2. */
	result_2 += fs_inst[idx].hdr_exp.exp_lc[0];
	for (i = 1; i < mode_exp_cnt_2; ++i) {
		int hdr_idx = hdr_exp_idx_map[mode_exp_cnt_2][i];

		if (hdr_idx < 0) {
			LOG_INF(
				"ERROR: [%u] ID:%#x(sidx:%u), hdr_exp_idx_map[%u][%u] = %d\n",
				idx,
				fs_inst[idx].sensor_id,
				fs_inst[idx].sensor_idx,
				mode_exp_cnt_2,
				i,
				hdr_idx);

			return 0;
		}

		result_2 += fs_inst[idx].prev_hdr_exp.exp_lc[hdr_idx];
	}

	exp_lc = (result_1 > result_2) ? result_1 : result_2;


#ifndef REDUCE_FS_ALGO_LOG
	LOG_INF("[%u] ID:%#x(sidx:%u), equiv_exp_lc:%u(%u/%u)\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		exp_lc,
		result_1,
		result_2);
#endif

	return exp_lc;
}


static void fs_alg_update_hdr_exp_readout_fl_lc(const unsigned int idx)
{
	struct fs_hdr_exp_st *p_curr_hdr = &fs_inst[idx].hdr_exp;
	struct fs_hdr_exp_st *p_prev_hdr = &fs_inst[idx].prev_hdr_exp;
	unsigned int readout_fl_lc = 0, readout_min_fl_lc = 0;
	unsigned int mode_exp_cnt = fs_inst[idx].hdr_exp.mode_exp_cnt;
	unsigned int readout_len_lc = fs_inst[idx].hdr_exp.readout_len_lc;
	unsigned int read_margin_lc = fs_inst[idx].hdr_exp.read_margin_lc;
	unsigned int i = 1;
	int read_offset_diff = 0;

	if ((mode_exp_cnt > 1) && (readout_len_lc == 0)) {
		/* multi exp mode but with readout length equal to zero */
		fs_inst[idx].readout_min_fl_lc = 0;

		LOG_INF(
			"WARNING: [%u] ID:%#x(sidx:%u), readout_len_lc:%d (mode_exp_cnt:%u) FL calc. may have error\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			readout_len_lc,
			mode_exp_cnt);

		return;
	}

	/* calc. each exp readout offset change, except LE */
	for (i = 1; i < mode_exp_cnt; ++i) {
		int hdr_idx = hdr_exp_idx_map[mode_exp_cnt][i];

		if (hdr_idx < 0) {
			LOG_INF(
				"ERROR: [%u] ID:%#x(sidx:%u), hdr_exp_idx_map[%u][%u] = %d\n",
				idx,
				fs_inst[idx].sensor_id,
				fs_inst[idx].sensor_idx,
				mode_exp_cnt,
				i,
				hdr_idx);

			return;
		}

		read_offset_diff +=
			p_prev_hdr->exp_lc[hdr_idx] -
			p_curr_hdr->exp_lc[hdr_idx];

		readout_fl_lc = (read_offset_diff > 0)
			? (readout_len_lc + read_margin_lc + read_offset_diff)
			: (readout_len_lc + read_margin_lc);

		if (readout_min_fl_lc < readout_fl_lc)
			readout_min_fl_lc = readout_fl_lc;
	}

	fs_inst[idx].readout_min_fl_lc = readout_min_fl_lc;
}


static void fs_alg_set_hdr_exp_st_data(const unsigned int idx,
	unsigned int *shutter_lc, struct fs_hdr_exp_st *p_hdr_exp)
{
	unsigned int valid_exp_idx[FS_HDR_MAX] = {0};
	unsigned int i = 0;

	/* boundary ckeck */
	if (p_hdr_exp->ae_exp_cnt == 0)
		return;
	if (p_hdr_exp->ae_exp_cnt > FS_HDR_MAX ||
		p_hdr_exp->mode_exp_cnt > FS_HDR_MAX) {

		LOG_INF(
			"ERROR: [%u] ID:%#x(sidx:%u), hdr_exp: cnt:(mode:%u/ae:%u)), set to max:%u\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			p_hdr_exp->mode_exp_cnt,
			p_hdr_exp->ae_exp_cnt,
			FS_HDR_MAX);

		if (p_hdr_exp->mode_exp_cnt > FS_HDR_MAX)
			p_hdr_exp->mode_exp_cnt = FS_HDR_MAX;

		if (p_hdr_exp->ae_exp_cnt > FS_HDR_MAX)
			p_hdr_exp->ae_exp_cnt = FS_HDR_MAX;
	}

	/* sensor is NOT at STG mode */
	if (p_hdr_exp->mode_exp_cnt == 0) {
		/* NOT STG mode => get first EXP and overwrite shutter_lc data */
		fs_inst[idx].shutter_lc = p_hdr_exp->exp_lc[0];
		*shutter_lc = fs_inst[idx].shutter_lc;

		fs_inst[idx].prev_hdr_exp = fs_inst[idx].hdr_exp;
		memset(&fs_inst[idx].hdr_exp, 0, sizeof(fs_inst[idx].hdr_exp));


		/* NOT STG mode and ae_exp_cnt == 1 => fine, return */
		if (p_hdr_exp->ae_exp_cnt == 1)
			return;

		LOG_INF(
			"WARNING: [%u] ID:%#x(sidx:%u), Not HDR mode, set shutter:%u(%u) (hdr_exp: ctrl(%u/%u/%u/%u/%u, %u/%u) cnt:(mode/ae))\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			convert2TotalTime(
				fs_inst[idx].lineTimeInNs,
				*shutter_lc),
			fs_inst[idx].shutter_lc,
			p_hdr_exp->exp_lc[0],
			p_hdr_exp->exp_lc[1],
			p_hdr_exp->exp_lc[2],
			p_hdr_exp->exp_lc[3],
			p_hdr_exp->exp_lc[4],
			p_hdr_exp->mode_exp_cnt,
			p_hdr_exp->ae_exp_cnt);

		return;
	}

	/* for sensor is at STG mode */
	/* 1.  update from new -> old: p_hdr_exp -> .hdr_exp -> .prev_hdr_exp */
	fs_inst[idx].prev_hdr_exp = fs_inst[idx].hdr_exp;

	/* 1.1 update hdr_exp struct data one by one */
	fs_inst[idx].hdr_exp.mode_exp_cnt = p_hdr_exp->mode_exp_cnt;
	fs_inst[idx].hdr_exp.multi_exp_type = p_hdr_exp->multi_exp_type;
	fs_inst[idx].hdr_exp.ae_exp_cnt = p_hdr_exp->ae_exp_cnt;
	fs_inst[idx].hdr_exp.readout_len_lc = p_hdr_exp->readout_len_lc;
	fs_inst[idx].hdr_exp.read_margin_lc = p_hdr_exp->read_margin_lc;

	/* 1.2 update hdr_exp.exp_lc array value */
	for (i = 0; i < p_hdr_exp->ae_exp_cnt; ++i) {
		int hdr_idx = hdr_exp_idx_map[p_hdr_exp->ae_exp_cnt][i];

		if (hdr_idx < 0) {
			LOG_INF(
				"ERROR: [%u] ID:%#x(sidx:%u), hdr_exp_idx_map[%u] = %d\n",
				idx,
				fs_inst[idx].sensor_id,
				fs_inst[idx].sensor_idx,
				i,
				hdr_idx);

			return;
		}

		fs_inst[idx].hdr_exp.exp_lc[hdr_idx] =
						p_hdr_exp->exp_lc[hdr_idx];

#ifndef REDUCE_FS_ALGO_LOG
		LOG_INF("[%u] ID:%#x(sidx:%u), exp_lc[%u] = %u / %u, i = %u\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			hdr_idx,
			fs_inst[idx].hdr_exp.exp_lc[idx],
			p_hdr_exp->exp_lc[idx],
			i);
#endif
	}

	/* 2. clear non exp value in non valid idx */
	/* 2.1 generate valid_exp_idx array for clear data using */
	for (i = 0; i < p_hdr_exp->mode_exp_cnt; ++i)
		valid_exp_idx[hdr_exp_idx_map[p_hdr_exp->mode_exp_cnt][i]] = 1;

#ifndef REDUCE_FS_ALGO_LOG
	LOG_INF("[%u] ID:%#x(sidx:%u), valid_idx:%u/%u/%u/%u/%u\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		valid_exp_idx[0],
		valid_exp_idx[1],
		valid_exp_idx[2],
		valid_exp_idx[3],
		valid_exp_idx[4]);
#endif

	/* 2.2 clear the data in non valid idx */
	for (i = 0 ; i < FS_HDR_MAX; ++i) {
		if (valid_exp_idx[i] == 0) /* 0 => non valid */
			fs_inst[idx].hdr_exp.exp_lc[i] = 0;
	}

	/* 3. calc. equivalent exp lc */
	/*    and overwrite shutter_lc data */
	fs_inst[idx].shutter_lc = fs_alg_get_hdr_equivalent_exp_lc(idx);
	*shutter_lc = fs_inst[idx].shutter_lc;

	/* 4. update read offset change (update readout_min_fl_lc) */
	fs_alg_update_hdr_exp_readout_fl_lc(idx);


	LOG_INF(
		"[%u] ID:%#x(sidx:%u), hdr_exp: c(%u/%u/%u/%u/%u, %u/%u, %u/%u), p(%u/%u/%u/%u/%u, %u/%u, %u/%u), ctrl(%u/%u/%u/%u/%u, %u/%u, %u/%u) cnt:(mode/ae) read:(len/margin), readout_min_fl:%u(%u), shutter:%u(%u) (equiv)\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		fs_inst[idx].hdr_exp.exp_lc[0],
		fs_inst[idx].hdr_exp.exp_lc[1],
		fs_inst[idx].hdr_exp.exp_lc[2],
		fs_inst[idx].hdr_exp.exp_lc[3],
		fs_inst[idx].hdr_exp.exp_lc[4],
		fs_inst[idx].hdr_exp.mode_exp_cnt,
		fs_inst[idx].hdr_exp.ae_exp_cnt,
		fs_inst[idx].hdr_exp.readout_len_lc,
		fs_inst[idx].hdr_exp.read_margin_lc,
		fs_inst[idx].prev_hdr_exp.exp_lc[0],
		fs_inst[idx].prev_hdr_exp.exp_lc[1],
		fs_inst[idx].prev_hdr_exp.exp_lc[2],
		fs_inst[idx].prev_hdr_exp.exp_lc[3],
		fs_inst[idx].prev_hdr_exp.exp_lc[4],
		fs_inst[idx].prev_hdr_exp.mode_exp_cnt,
		fs_inst[idx].prev_hdr_exp.ae_exp_cnt,
		fs_inst[idx].prev_hdr_exp.readout_len_lc,
		fs_inst[idx].prev_hdr_exp.read_margin_lc,
		p_hdr_exp->exp_lc[0],
		p_hdr_exp->exp_lc[1],
		p_hdr_exp->exp_lc[2],
		p_hdr_exp->exp_lc[3],
		p_hdr_exp->exp_lc[4],
		p_hdr_exp->mode_exp_cnt,
		p_hdr_exp->ae_exp_cnt,
		p_hdr_exp->readout_len_lc,
		p_hdr_exp->read_margin_lc,
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			fs_inst[idx].readout_min_fl_lc),
		fs_inst[idx].readout_min_fl_lc,
		convert2TotalTime(
			fs_inst[idx].lineTimeInNs,
			*shutter_lc),
		*shutter_lc);
}


#ifdef SUPPORT_FS_NEW_METHOD
static inline void fs_alg_setup_basic_out_fl(const unsigned int idx,
	unsigned int *p_out_fl_us,
	const struct FrameSyncDynamicPara *p_para)
{
	if (unlikely((p_out_fl_us == NULL) || (*p_out_fl_us == 0)))
		return;
	if (fs_inst[idx].fl_active_delay != 2)
		return;

	/**
	 * FL is N+1 type, check extra shutter & FL rules when output FL
	 * e.g., stagger rules ==> may be need for a larger FL.
	 */
	*p_out_fl_us = (p_para->out_fl_us_min > *p_out_fl_us)
		? p_para->out_fl_us_min : *p_out_fl_us;
}


static unsigned int fs_alg_sa_calc_f_tag_diff(const unsigned int idx,
	const unsigned int stable_fl_us, const unsigned int f_tag)
{
	const unsigned int f_cell = get_valid_frame_cell_size(idx);
	const unsigned int fdelay = fs_inst[idx].fl_active_delay;
	unsigned int need_add_f_cnt;
	unsigned int f_tag_diff;

	/* case check --- not N:1 case ==> return 0 */
	if (f_cell < 2)
		return 0;

	/* Take f_cell and fdelay into account to calculate how many */
	/* remaining frames that FL can be controlled in a f_cell. */
	/* Due to FL will be extended and than be restored */
	/* , so actual cnt must be ring back of 1 */
	need_add_f_cnt = (f_cell-(fdelay-1)) + ((f_cell-f_tag)) % f_cell;
	need_add_f_cnt = FS_RING_BACK(need_add_f_cnt, f_cell, 1);

	f_tag_diff = stable_fl_us * need_add_f_cnt;

	return f_tag_diff;
}


/*
 * special API return equivelent prdicted frame length
 * according to fdelay, target (current/next), f_cell size.
 *
 * be careful:
 *     for sensor that N+1 FL activate,
 *     this API only calculate to current predicted frame length.
 *
 * input:
 *     pred_fl_us: predicted frame length
 *     stable_fl_us: if not change register setting, FL is this value for all frame
 *     fdelay: frame length activate delay
 *     target: current/next predicted => 0/1
 *     f_cell: for algorithm, treat f_cell frame as one frame to give predicted FL
 *
 * output:
 *     u_int: according to input return corresponding predicted frame length
 */
static unsigned int fs_alg_sa_calc_target_pred_fl_us(
	unsigned int pred_fl_us[], const unsigned int stable_fl_us,
	const unsigned int fdelay, const unsigned int target,
	unsigned int f_cell)
{
	unsigned int i = 0, cnt = 0, val = 0;
	unsigned int pred_fl = 0;


	/* fdelay must only be 2 or 3 */
	if (!((fdelay == 2) || (fdelay == 3))) {
		LOG_MUST(
			"ERROR: frame_time_delay_frame:%u is not valid (must be 2 or 3), plz check sensor driver for getting correct value\n",
			fdelay
		);
	}

	/* N+1 FL activate, only calculate to current predicted frame length */
	if ((fdelay == 2) && (target == 1))
		return 0;

	/* for the logic of this function, min f_cell is 1, */
	/* through at normal Frame-Sync case f_cell is 0 */
	if (f_cell < 1)
		f_cell = 1;


	i = (target * f_cell);
	cnt = (target * f_cell) + f_cell;

	if ((target == 0) || (target == 1)) {
		/* calculate curr/next predicted frame length */
		for (; i < cnt; ++i) {
			val = (i < 2) ? pred_fl_us[i] : stable_fl_us;

			pred_fl += val;
		}

	} else {
		LOG_INF(
			"ERROR: request to calculate invalid target:%u (0:curr/1:next/unknown)\n",
			target
		);

		return 0;
	}


	return pred_fl;
}


/**
 * Calculate the anchor bias by the newest sensor ctrl.
 * Anchor bias/offset will change by sync type that user assign.
 *
 * So, need to take below item into account.
 * 1. vsync target bias, e.g, 1st-exp(LE) / last-exp(SE).
 * 2. readout center bias.
 * 3. etc.
 */
static int fs_alg_calc_anchor_bias_us(const unsigned int idx,
	const struct FrameSyncDynamicPara *p_para)
{
	const unsigned int vsync_bias_us = p_para->ts_bias_us;
	const unsigned int ro_cent_bias_us = (fs_inst[idx].readout_time_us / 2);
	const int rout_center_en = p_para->sa_cfg.rout_center_en_bits;
	int result = 0;

	result = (rout_center_en)
		? (vsync_bias_us + ro_cent_bias_us)
		: (vsync_bias_us);

	/* update anchor bias us in fs inst for dumping fs inst data to check */
	FS_ATOMIC_SET(result, &fs_inst[idx].anchor_bias_us);

	return result;
}


static inline unsigned int fs_alg_sa_update_ts_bias_us(const unsigned int idx,
	struct FrameSyncDynamicPara *p_para)
{
	const unsigned int line_time = fs_inst[idx].lineTimeInNs;
	unsigned int ts_bias_lc, ts_bias_us;

	/* get timestamp bias info */
	ts_bias_lc = calc_vts_sync_bias_lc(idx);
	ts_bias_us = convert2TotalTime(line_time, ts_bias_lc);

	/* update ts bias us in fs inst */
	FS_ATOMIC_SET(ts_bias_us, &fs_inst[idx].ts_bias_us);

	return ts_bias_us;
}


static void fs_alg_sa_update_pred_fl_and_ts_bias(const unsigned int idx,
	struct FrameSyncDynamicPara *p_para)
{
	unsigned int i;

	/* calculate and get predicted frame length */
	p_para->pred_fl_us[0] =
		fs_inst[idx].predicted_fl_us[0];
	p_para->pred_fl_us[1] =
		fs_inst[idx].predicted_fl_us[1];
	memcpy(&p_para->pred_next_exp_rd_offset_us,
		&fs_inst[idx].fl_info.next_exp_rd_offset_us,
		sizeof(fs_inst[idx].fl_info.next_exp_rd_offset_us));

	/* calculate and get timestamp bias */
	p_para->ts_bias_us = fs_alg_sa_update_ts_bias_us(idx, p_para);


#if defined(FS_UT)
	/* update frame monitor current predicted framelength data */
	frm_update_next_vts_bias_us(idx, p_para->ts_bias_us);
#endif


	/* for N:1 FrameSync case, calculate and get tag bias */
	p_para->tag_bias_us = fs_alg_sa_calc_f_tag_diff(idx,
		p_para->stable_fl_us, fs_inst[idx].frame_tag);


	/* calculate predicted total delta (without timestamp diff) */
	p_para->delta = (p_para->ts_bias_us + p_para->tag_bias_us);
	for (i = 0; i < 2; ++i) {
		p_para->delta +=
			fs_alg_sa_calc_target_pred_fl_us(
				p_para->pred_fl_us, p_para->stable_fl_us,
				fs_inst[idx].fl_active_delay, i, 1);
	}


	/* update anchor bias for MW using */
	p_para->anchor_bias_us = fs_alg_calc_anchor_bias_us(idx, p_para);
}


static void fs_alg_sa_update_fl_us(const unsigned int idx,
	const unsigned int us, struct FrameSyncDynamicPara *p_para)
{
	set_fl_us(idx, us);

	p_para->out_fl_us = us;

	/* update predicted frame length and ts_bias */
	fs_alg_sa_update_pred_fl_and_ts_bias(idx, p_para);

	/* update frame length info to user */
	fs_alg_update_fl_rec_st_fl_info(idx,
		p_para->magic_num,
		p_para->target_min_fl_us, us);
}


static inline void fs_alg_sa_update_target_stable_fl_info(const unsigned int idx,
	struct FrameSyncDynamicPara *p_para, const unsigned int fl_us)
{
	const unsigned int f_cell = get_valid_frame_cell_size(idx);

	p_para->target_min_fl_us = fl_us;
	p_para->stable_fl_us = (fl_us / f_cell);
	p_para->out_fl_us_init = (fl_us / f_cell);
}


static void fs_alg_sa_setup_basic_fl_info(const unsigned int idx,
	struct FrameSyncDynamicPara *p_para,
	const unsigned int sync_flk_en, unsigned int *p_flk_diff)
{
	/* fall back to sensor def min FL if user not called set max fps */
	const unsigned int min_fl_lc = (fs_inst[idx].min_fl_lc != 0)
		? fs_inst[idx].min_fl_lc : fs_inst[idx].def_min_fl_lc;
	const unsigned int line_time = fs_inst[idx].lineTimeInNs;
	const unsigned int fdelay = fs_inst[idx].fl_active_delay;
	const unsigned int f_cell = get_valid_frame_cell_size(idx);
	unsigned int fl_lc, fl_us, next_fl_lc, next_fl_us;
	unsigned int flk_diff, next_flk_diff;

	/* setup basic information */
	p_para->f_tag = fs_inst[idx].frame_tag;
	p_para->f_cell = f_cell;

	/* !!! setup basic FL information !!! */
	/* ==> find min fl that this sensor support (for fps alignment) */
	/*     and check anti-flicker FL */
	fl_lc = calc_min_fl_lc(idx, min_fl_lc, PREDICT_STABLE_FL);
	fl_us = convert2TotalTime(line_time, fl_lc);
	p_para->pure_min_fl_lc = (fl_lc * f_cell);
	p_para->pure_min_fl_us = (fl_us * f_cell);

	g_flk_fl_and_flk_diff(idx, &fl_us, &flk_diff, sync_flk_en);
	if (p_flk_diff != NULL)
		*p_flk_diff = flk_diff;

	p_para->min_fl_us = (fl_us * f_cell);

	/* ==> setup/update target & stable FL information */
	fs_alg_sa_update_target_stable_fl_info(idx, p_para, (fl_us * f_cell));

	/* ==> setup/update valid min out FL information */
	if (fdelay == 3) {
		/* FL N+2 type => valid min output FL = min FL */
		p_para->out_fl_us_min = fl_us;
	} else if (fdelay == 2) {
		/* FL N+1 type => check extra shutter & FL rules */
		next_fl_lc = calc_min_fl_lc(idx, min_fl_lc, PREDICT_NEXT_FL);
		next_fl_us = convert2TotalTime(line_time, next_fl_lc);

		g_flk_fl_and_flk_diff(idx, &next_fl_us, &next_flk_diff, sync_flk_en);

		p_para->out_fl_us_min = next_fl_us;
	}
}


static void fs_alg_sa_init_dynamic_para_info(const unsigned int idx,
	struct FrameSyncDynamicPara *p_para)
{
	/* copy debug info */
	p_para->req_id = fs_inst[idx].req_id;
	p_para->frame_id = fs_inst[idx].frame_id;

	/* setup basic frame length info */
	fs_alg_sa_setup_basic_fl_info(idx, p_para, 0, NULL);

	/* update predicted frame length and ts_bias */
	fs_alg_sa_update_pred_fl_and_ts_bias(idx, p_para);

	/* init frame length record st data (if needed) */
	fs_alg_init_fl_rec_st(idx);
}


static void fs_alg_sa_update_seamless_dynamic_para(const unsigned int idx,
	struct fs_seamless_st *p_seamless_info,
	struct FrameSyncDynamicPara *p_para)
{
	unsigned int i = 0;

	/* error handling (unexpected case) */
	if (unlikely(p_seamless_info == NULL)) {
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u), get p_seamless_info:%p, return\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			p_seamless_info);
		return;
	}

	/* using seamless ctrl to update pf ctrl */
	fs_inst[idx].hdr_exp = p_seamless_info->seamless_pf_ctrl.hdr_exp;
	fs_alg_set_perframe_st_data(idx, &p_seamless_info->seamless_pf_ctrl);


	/* !!! setup dynamic parameters !!! */
	/* calculate and get timestamp bias */
	p_para->ts_bias_us = fs_alg_sa_update_ts_bias_us(idx, p_para);

	/* setup frame length info */
	p_para->pred_fl_us[0] = fs_inst[idx].predicted_fl_us[0];
	p_para->pred_fl_us[1] = fs_inst[idx].predicted_fl_us[1];

	/* overwrite frame measurement predicted frame length for debugging */
	frm_set_frame_measurement(idx, 0,
		fs_inst[idx].predicted_fl_us[0],
		fs_inst[idx].predicted_fl_lc[0],
		fs_inst[idx].predicted_fl_us[1],
		fs_inst[idx].predicted_fl_lc[1]);


#if defined(FS_UT)
	/* update frame monitor current predicted framelength data */
	frm_update_next_vts_bias_us(idx, p_para->ts_bias_us);
#endif


	/* for N:1 FrameSync case, calculate and get tag bias */
	p_para->tag_bias_us = fs_alg_sa_calc_f_tag_diff(idx,
		p_para->stable_fl_us, fs_inst[idx].frame_tag);


	/* sync seamless frame length RG value */
	set_fl_us(idx, p_para->stable_fl_us);


	/* calculate predicted total delta (without timestamp diff) */
	p_para->delta = (p_para->ts_bias_us + p_para->tag_bias_us);
	for (i = 0; i < 2; ++i) {
		p_para->delta +=
			fs_alg_sa_calc_target_pred_fl_us(
				p_para->pred_fl_us, p_para->stable_fl_us,
				fs_inst[idx].fl_active_delay, i, 1);
	}

	/* update anchor bias for MW using */
	p_para->anchor_bias_us = fs_alg_calc_anchor_bias_us(idx, p_para);

	/* finally update result */
	fs_alg_sa_update_dynamic_para(idx, p_para);


#if !defined(REDUCE_FS_ALGO_LOG)
	LOG_INF(
		"[%u] ID:%#x(sidx:%u), #%u, stable_fl_us:%u, pred_fl(c:%u(%u), n:%u(%u))(%u), bias(exp:%u/tag:%u), delta:%u(fdelay:%u), anchor_bias:%u\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		p_para->magic_num,
		p_para->stable_fl_us,
		p_para->pred_fl_us[0],
		fs_inst[idx].predicted_fl_lc[0],
		p_para->pred_fl_us[1],
		fs_inst[idx].predicted_fl_lc[1],
		fs_inst[idx].lineTimeInNs,
		p_para->ts_bias_us,
		p_para->tag_bias_us,
		p_para->delta,
		fs_inst[idx].fl_active_delay,
		p_para->anchor_bias_us);
#endif


	fs_alg_dump_fs_inst_data(idx);
}


static inline void fs_alg_sa_get_last_vts_info(const unsigned int idx,
	struct FrameSyncDynamicPara *p_para)
{
	/* write back newest last_ts and cur_tick data */
	p_para->ts_src_type = fs_inst[idx].ts_src_type;
	p_para->last_ts = fs_inst[idx].last_vts;
	p_para->cur_tick = fs_inst[idx].cur_tick;
	p_para->vsyncs = fs_inst[idx].vsyncs;
}


static void fs_alg_sa_init_new_ctrl(const struct fs_sa_cfg *p_sa_cfg,
	struct FrameSyncDynamicPara *p_para)
{
	const unsigned int idx = p_sa_cfg->idx;
	const int m_idx = p_sa_cfg->m_idx;

	/* copy SA cfg info */
	p_para->sa_cfg = *p_sa_cfg;

	/* generate new ctrl serial number */
	p_para->magic_num = ++fs_sa_inst.magic_num[idx];
	p_para->master_idx = m_idx;

	/* check flow is NOT trigged by AE shutter */
	if (unlikely(p_sa_cfg->extra_event.is_valid)) {
		p_para->extra_magic_num =
			++fs_sa_inst.extra_magic_num[idx];
	}

	/* !!! timestamp data is setup when fs_alg_sa_get_last_vts_info !!! */

	fs_alg_sa_init_dynamic_para_info(idx, p_para);
}


static void fs_alg_sa_pre_set_dynamic_paras(const unsigned int idx,
	struct FrameSyncDynamicPara *p_para)
{
	const unsigned int f_cell = get_valid_frame_cell_size(idx);
	unsigned int fl_us = convert2TotalTime(
		fs_inst[idx].lineTimeInNs,
		fs_inst[idx].p_frecs[0]->framelength_lc);

	/* setup basic information */
	p_para->f_tag = fs_inst[idx].frame_tag;
	p_para->f_cell = f_cell;

	/* init FL info */
	p_para->pure_min_fl_us = (fl_us * f_cell);
	p_para->min_fl_us = (fl_us * f_cell);
	p_para->target_min_fl_us = (fl_us * f_cell);
	p_para->stable_fl_us = fl_us;
	p_para->out_fl_us_min = fl_us;
	p_para->out_fl_us_init = fl_us;

	fs_alg_sa_update_fl_us(idx, fl_us, p_para);
	fs_alg_sa_get_last_vts_info(idx, p_para);

	LOG_MUST(
		"NOTICE: #%u, fl:(p_min:%u/min:%u/target_min:%u/o:(%u/init:%u/min:%u)/s:%u), frec(0:%u/%u), %u, pr_fl(c:%u(%u)/n:%u(%u)), ts_bias(exp:%u/tag:%u(%u/%u)), delta:%u(fdelay:%u), tg:%u, ts(%llu/%llu/%u)\n",
		p_para->magic_num,
		p_para->pure_min_fl_us,
		p_para->min_fl_us,
		p_para->target_min_fl_us,
		p_para->out_fl_us,
		p_para->out_fl_us_min,
		p_para->out_fl_us_init,
		p_para->stable_fl_us,
		fs_inst[idx].p_frecs[0]->framelength_lc,
		fs_inst[idx].p_frecs[0]->shutter_lc,
		fs_inst[idx].lineTimeInNs,
		p_para->pred_fl_us[0],
		convert2LineCount(
			fs_inst[idx].lineTimeInNs,
			p_para->pred_fl_us[0]),
		p_para->pred_fl_us[1],
		convert2LineCount(
			fs_inst[idx].lineTimeInNs,
			p_para->pred_fl_us[1]),
		p_para->ts_bias_us,
		p_para->tag_bias_us,
		p_para->f_tag,
		p_para->f_cell,
		p_para->delta,
		fs_inst[idx].fl_active_delay,
		fs_inst[idx].tg,
		p_para->last_ts,
		p_para->cur_tick,
		p_para->vsyncs);
}


/*
 * return:
 *     0: check passed / non-0: non-valid data is detected
 *     bit 1: last timestamp is zero
 *     bit 2: sensor frame_time_delay_frame value is non-valid
 */
static unsigned int fs_alg_sa_dynamic_paras_checker(
	const unsigned int s_idx, const unsigned int m_idx,
	const struct FrameSyncDynamicPara *p_para_s,
	struct FrameSyncDynamicPara *p_para_m)
{
	const unsigned int fdelay_s = fs_inst[s_idx].fl_active_delay;
	const unsigned int fdelay_m = fs_inst[m_idx].fl_active_delay;
	unsigned int query_ts_idx[2] = {s_idx, m_idx};
	unsigned int ret = 0;

	/* check if last timestamp equal to zero */
	if (unlikely(check_fs_inst_vsync_data_valid(query_ts_idx, 2) == 0)) {
		ret |= (1U << 1);
		LOG_MUST(
			"NOTICE: [%u] ID:%#x(sidx:%u), #%u/#%u(m_idx:%u), before first vsync, latest timestamp is/are ZERO (s:%llu/m:%llu), fs_inst(s(%llu/%llu/%llu/%llu), m(%llu/%llu/%llu/%llu)), p_para_ts(s:%llu/m:%llu) => out_fl:%u(%u), ret:%#x\n",
			s_idx,
			fs_inst[s_idx].sensor_id,
			fs_inst[s_idx].sensor_idx,
			p_para_s->magic_num,
			p_para_m->magic_num,
			m_idx,
			fs_inst[s_idx].last_vts,
			fs_inst[m_idx].last_vts,
			fs_inst[s_idx].timestamps[0],
			fs_inst[s_idx].timestamps[1],
			fs_inst[s_idx].timestamps[2],
			fs_inst[s_idx].timestamps[3],
			fs_inst[m_idx].timestamps[0],
			fs_inst[m_idx].timestamps[1],
			fs_inst[m_idx].timestamps[2],
			fs_inst[m_idx].timestamps[3],
			p_para_s->last_ts,
			p_para_m->last_ts,
			fs_inst[s_idx].output_fl_us,
			convert2LineCount(
				fs_inst[s_idx].lineTimeInNs,
				fs_inst[s_idx].output_fl_us),
			ret);

		/* for first req ctrl, slave get ctrl faster than master, */
		/* so gen a pre set dynamic para data for slave adjust diff */
		if ((p_para_m->last_ts == 0) && (fs_inst[m_idx].last_vts != 0))
			fs_alg_sa_pre_set_dynamic_paras(m_idx, p_para_m);
	}

	/* check sensor fl_active_delay value */
	/* in this time predicted frame length are equal to zero */
	if (unlikely((fdelay_s < 2 || fdelay_s > 3)
			|| (fdelay_m < 2 || fdelay_m > 3))) {
		ret |= (1U << 2);
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u), #%u/#%u(m_idx:%u), frame_time_delay_frame is/are not valid (must be 2 or 3), s:%u/m:%u => out_fl:%u(%u), ret:%#x\n",
			s_idx,
			fs_inst[s_idx].sensor_id,
			fs_inst[s_idx].sensor_idx,
			p_para_s->magic_num,
			p_para_m->magic_num,
			m_idx,
			fs_inst[s_idx].fl_active_delay,
			fs_inst[m_idx].fl_active_delay,
			fs_inst[s_idx].output_fl_us,
			convert2LineCount(
				fs_inst[s_idx].lineTimeInNs,
				fs_inst[s_idx].output_fl_us),
			ret);
	}

	return ret;
}


/* return => 0: check passed / non-0: non-valid data is detected */
static inline unsigned int fs_alg_sa_dynamic_params_preparer(
	const unsigned int s_idx, const unsigned int m_idx,
	const struct FrameSyncDynamicPara *p_para_s,
	struct FrameSyncDynamicPara *p_para_m)
{
	/* !!! Check master & slave information !!! */
	/* ==> get master dynamic para data */
	fs_alg_sa_get_dynamic_para(m_idx, p_para_m);
	/* ==> check all needed info is valid */
	if (unlikely(fs_alg_sa_dynamic_paras_checker(
			s_idx, m_idx, p_para_s, p_para_m)))
		return 1;
	return 0;
}


/**
 * return:
 *      1 => possible, @(*p_subtracting_diff) => the entire diff.
 *      2 => possible, @(*p_subtracting_diff) => the extra blanking.
 *      0 => impossible.
 * only when the case that possible to keep sync by subtracting out FL
 * the function will return 1; otherwise do not subtract out FL.
 */
static int chk_if_subtracting_out_fl_to_keep_sync_is_possible(
	const struct FrameSyncDynamicPara *p_para,
	const unsigned int out_fl_us, const long long adj_diff,
	long long *p_subtracting_diff)
{
	const unsigned int threshold = FS_TOLERANCE;
	long long extra_blanking;
	int ret;

	if (out_fl_us <= p_para->out_fl_us_min)
		return 0;

	/**
	 * handle with error caused by numerical conversion.
	 * ==> e.g., line time / us / multiple sensors.
	 *
	 * out fl min : 33355 us
	 * out fl : 35337 us (FPS aligned)
	 * --> extra blanking : 1982 us <=> adj diff : 1983 us
	 *
	 * in fact, can do subtract at this point, after subtration...
	 * --> adj diff : 1983 - 1982 = 1 us <-- is in the threshold.
	 */
	extra_blanking = out_fl_us - p_para->out_fl_us_min;

	ret = 0; /* init value */
	if (extra_blanking >= adj_diff) {
		*p_subtracting_diff = adj_diff;
		ret = 1;
	} else { /* extra_blanking < adj_diff */
		if ((adj_diff - extra_blanking) <= threshold) {
			/**
			 * e.g.,
			 * out fl : 33326 / out fl min : 33323 / adj diff : 494
			 * --> can only subtract 3 (33326 - 33323)
			 */
			*p_subtracting_diff = extra_blanking;
			ret = 2;
		}
	}

	return ret;
}


static inline int chk_pr_fl_error_is_in_range(const long long pr_act_fl_diff,
	const unsigned int threshold, const unsigned int max_th)
{
	const long long val = (pr_act_fl_diff >= 0)
		? pr_act_fl_diff : (0 - pr_act_fl_diff);

	return (val >= threshold && val < max_th) ? 1 : 0;
}


static void fs_alg_sa_calc_pr_fl_error(
	const unsigned int m_idx, const unsigned int idx,
	const struct FrameSyncDynamicPara *p_para_m,
	struct FrameSyncDynamicPara *p_para_s)
{
	const struct FrameSyncInst *p_fs_inst_m = &fs_inst[m_idx];
	const unsigned int TH_US = 500, MAX_TH = p_para_m->stable_fl_us;
	const unsigned int CHK_TO = 0;		// 0:current / 1:next
	long long act_diff[VSYNCS_MAX-1] = {0};
	long long pr_act_fl_diff[VSYNCS_MAX-1] = {0};
	unsigned int i, run = 0, ret = 0;

	/* init/setup default value */
	p_para_s->pred_fl_err_us_m = 0;

	/* case to skip */
	if (unlikely(p_para_m->last_ts == 0))
		return;

	/* find out which ts data is match to that in dynamic para */
	/* => if current dynamic para is newest */
	if (p_fs_inst_m->timestamps[0] == p_para_m->last_ts)
		return;
	for (i = 1; (i < VSYNCS_MAX && (run <= CHK_TO && run < (VSYNCS_MAX-1))); ++i) {
		if (p_fs_inst_m->timestamps[i] != p_para_m->last_ts)
			continue;
		/* !!! timestamp data are match !!! */

		/* calc. ts diff from this ts data */
		act_diff[run] =
			(p_fs_inst_m->timestamps[i-1] - p_fs_inst_m->timestamps[i]);
		pr_act_fl_diff[run] =
			(act_diff[run] - p_para_m->pred_fl_us[run]);

		/* check if wants to take this error into account */
		if (unlikely(chk_pr_fl_error_is_in_range(
				pr_act_fl_diff[run], TH_US, MAX_TH))) {
			p_para_s->pred_fl_err_us_m += pr_act_fl_diff[run];
			ret |= (1UL << (i+16));
		}
		ret |= (1UL << i);

		/* !!! procedure are all done !!! */
		run++;		// 0:current / 1:next
	}

	if (unlikely(ret >> 16)) {
		LOG_MUST(
			"NOTICE: [%u] ID:%#x(sidx:%u), pred_fl_err:%lld(chk_to:%u(0:c/1:n)), #%u/#%u(m_idx:%u) m_idx's:(pr_act_fl_diff:(%lld/%lld/%lld), act_diff:(%lld/%lld/%lld), pr_fl:(%u/%u), ts:(%llu/%llu/%llu/%llu, last:%llu)), th:(%u~%u), ret:%#x\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			p_para_s->pred_fl_err_us_m, CHK_TO,
			p_para_s->magic_num,
			p_para_m->magic_num,
			m_idx,
			pr_act_fl_diff[0], pr_act_fl_diff[1], pr_act_fl_diff[2],
			act_diff[0], act_diff[1], act_diff[2],
			p_para_m->pred_fl_us[0], p_para_m->pred_fl_us[1],
			p_fs_inst_m->timestamps[0], p_fs_inst_m->timestamps[1],
			p_fs_inst_m->timestamps[2], p_fs_inst_m->timestamps[3],
			p_para_m->last_ts, TH_US, MAX_TH, ret);
	}

	p_para_s->pred_fl_err_chk_bits_m = ret;
}


/*
 * input:
 *     p_para_m: a pointer to dynamic para structure of master sensor
 *     p_para_s: a pointer to dynamic para structure of slave sensor
 *
 * output:
 *     p_ts_diff_m: a pointer for ts diff of master sensor
 *     p_ts_diff_s: a pointer for ts diff of slave sensor
 *
 * be careful:
 *     Before do any operation, timestamp should be converted to clock count
 *     Tick is uint_32_t, so for correct calculation
 *         all data type should also uint32_t.
 */
static void fs_alg_sa_calc_m_s_ts_diff(
	const struct FrameSyncDynamicPara *p_para_m,
	struct FrameSyncDynamicPara *p_para_s,
	long long *p_ts_diff_m, long long *p_ts_diff_s)
{
	fs_timestamp_t ts_diff_m = 0, ts_diff_s = 0;

	/* unexpected case */
	if (unlikely(tick_factor == 0)) {
		LOG_INF(
			"ERROR: tick_factor:%u, all ts calculation will be force to zero\n",
			tick_factor);
		goto end_fs_alg_sa_calc_m_s_ts_diff;
	}

	/* !!! all operation must be handle in register/clock domain !!! */
	/* => prepare lastest timestamp data */
	ts_diff_m = p_para_m->last_ts * tick_factor;
	ts_diff_s = p_para_s->last_ts * tick_factor;

	if ((p_para_m->cur_tick != 0) && (p_para_s->cur_tick != 0)) {
		fs_timestamp_t cur_tick;

		/* !!! find newest timestamp by cur tick (newest) !!! */
		if (check_tick_b_after_a(p_para_m->cur_tick, p_para_s->cur_tick)) {
			/* case - master is before slave */
			cur_tick = p_para_s->cur_tick;
		} else {
			/* case - master is after slave */
			cur_tick = p_para_m->cur_tick;
		}

		/* normalization/shift (oldest ts => 0) */
		if ((cur_tick - ts_diff_m) < (cur_tick - ts_diff_s)) {
			ts_diff_m -= ts_diff_s;
			ts_diff_s = 0;
			ts_diff_m /= tick_factor;
		} else {
			ts_diff_s -= ts_diff_m;
			ts_diff_m = 0;
			ts_diff_s /= tick_factor;
		}
	} else {
		/* !!! find newest timestamp direct by timestamp !!! */
		/* find newest ts and normalization/shift (oldest ts => 0) */
		if (check_tick_b_after_a(ts_diff_s, ts_diff_m)) {
			ts_diff_m -= ts_diff_s;
			ts_diff_s = 0;
			ts_diff_m /= tick_factor;
		} else {
			ts_diff_s -= ts_diff_m;
			ts_diff_m = 0;
			ts_diff_s /= tick_factor;
		}
	}

	/* check if ts data is from EINT (XVS) */
	if (p_para_s->ts_src_type == FS_TS_SRC_EINT) {
		p_para_s->ts_offset =
			frm_g_ts_offset_between_eint_and_tsrec(p_para_s->sa_cfg.idx);

		/* add the XVS <-> Vsync offset to diff result directly */
		ts_diff_s += (fs_timestamp_t)p_para_s->ts_offset;
	}

end_fs_alg_sa_calc_m_s_ts_diff:
	/* sync result out */
	*p_ts_diff_m = (long long)ts_diff_m;
	*p_ts_diff_s = (long long)ts_diff_s;
}


static long long fs_alg_sa_calc_adjust_diff_master(
	const unsigned int m_idx, const unsigned int s_idx,
	long long *p_adjust_diff_s,
	const struct FrameSyncDynamicPara *p_para_m,
	const struct FrameSyncDynamicPara *p_para_s)
{
	const unsigned int f_cell_m = get_valid_frame_cell_size(m_idx);
	const long long adjust_diff_s_orig = *p_adjust_diff_s;
	const long long stable_fl_us_m =
		(long long)(p_para_m->stable_fl_us) * f_cell_m;
	long long adjust_diff_m;

	adjust_diff_m = stable_fl_us_m - adjust_diff_s_orig;

	if (unlikely(adjust_diff_m < 0)) {
		long long adjust_diff_m_new;

		*p_adjust_diff_s =
			calc_mod_64(adjust_diff_s_orig, stable_fl_us_m);
		adjust_diff_m_new = stable_fl_us_m - *p_adjust_diff_s;

		LOG_MUST(
			"NOTICE: [%u] ID:%#x(sidx:%u), #%u/#%u(m_idx:%u), detect negative adjust_diff(s:%lld/m:%lld), because stable_fl_us(s:%u/m:%u) is different => recalculate adjust_diff(s:%lld/m:%lld)\n",
			s_idx,
			fs_inst[s_idx].sensor_id,
			fs_inst[s_idx].sensor_idx,
			p_para_s->magic_num,
			p_para_m->magic_num,
			m_idx,
			adjust_diff_s_orig,
			adjust_diff_m,
			p_para_s->stable_fl_us,
			p_para_m->stable_fl_us,
			*p_adjust_diff_s,
			adjust_diff_m_new);

		adjust_diff_m = adjust_diff_m_new;
	}

	return adjust_diff_m;
}


static long long fs_alg_sa_calc_adjust_diff_slave(
	const unsigned int m_idx, const unsigned int s_idx,
	const long long ts_diff_m, const long long ts_diff_s,
	const struct FrameSyncDynamicPara *p_para_m,
	struct FrameSyncDynamicPara *p_para_s)
{
	const unsigned int f_cell_m = get_valid_frame_cell_size(m_idx);
	/* const unsigned int f_cell_s = get_valid_frame_cell_size(s_idx); */
	const long long m_stable_fl_us = (long long)p_para_m->stable_fl_us * f_cell_m;
	/* const long long s_stable_fl_us = (long long)p_para_s->stable_fl_us * f_cell_s; */
	const int rout_center_en = p_para_s->sa_cfg.rout_center_en_bits;
	long long adjust_diff_s = 0;

	/* unexpected case */
	if (unlikely((!p_para_m->pure_min_fl_us) || (!p_para_m->stable_fl_us))) {
		LOG_MUST(
			"NOTICE: [%u] ID:%#x(sidx:%u), #%u/#%u(m_idx:%u), detect master pure_min_fl_us:%u/stable_fl_us:%u, for preventing calculation error, abort processing ==> adjust_diff:0\n",
			s_idx,
			fs_inst[s_idx].sensor_id,
			fs_inst[s_idx].sensor_idx,
			p_para_s->magic_num,
			p_para_m->magic_num,
			m_idx,
			p_para_m->pure_min_fl_us,
			p_para_m->stable_fl_us);
		return 0;
	}


	/* !!! Calculate adjust diff !!! */
	adjust_diff_s =
		(ts_diff_m + p_para_m->delta + p_para_m->out_fl_us) -
		(ts_diff_s + p_para_s->delta + p_para_s->out_fl_us);
	if (rout_center_en) {
		adjust_diff_s +=
			((long long)(fs_inst[m_idx].readout_time_us) -
			fs_inst[s_idx].readout_time_us) / 2;
	}
	if (p_para_s->pred_fl_err_chk_bits_m)
		adjust_diff_s += p_para_s->pred_fl_err_us_m;

	/* ==> check situation (N+2/N+1 mixed), modify adjust diff */
	// if ((fs_inst[s_idx].fl_active_delay != fs_inst[m_idx].fl_active_delay)
	//	&& (adjust_diff_s > 0)) {
	//	/* if there are the pair, N+2 pred_fl will bigger than N+1 sensor */
	//	adjust_diff_s -= s_stable_fl_us;
	// }
	/* if ((fdelay_m != fdelay_s) && (adjust_diff_s > m_stable_fl_us)) */
	/*	adjust_diff_s -= p_para_m->out_fl_us; */
	if (adjust_diff_s > m_stable_fl_us) {
		/**
		 * back to previous sync point.
		 * e.g.,
		 *       last ts bias: 15076 / outFL: 42615 / ts bias: 7577
		 *       t:(s:0/m:5217)
		 *       m:(2:57515)(c:49938/o:42615/s:33355/e:7577) => 105347
		 *       s:(2:35115)(c:35115/o:33355/s:33355/e:0)    =>  68470
		 *       ==> 36877 > 33355
		 */
		const unsigned int last_ts_bias_us =
			FS_ATOMIC_READ(&fs_inst[m_idx].last_ts_bias_us);

		p_para_s->m_last_ts_bias_us = last_ts_bias_us;
		adjust_diff_s = adjust_diff_s
			- p_para_m->ts_bias_us - p_para_m->out_fl_us
			+ last_ts_bias_us;
	}


	/* !!! Normalize adjust diff !!! */
	/* ==> checking N:1, high fps slave's adjust diff should be normalize */
	/* if (adjust_diff_s > s_stable_fl_us) */
	/*	adjust_diff_s = calc_mod_64(adjust_diff_s, s_stable_fl_us); */
	if (adjust_diff_s > m_stable_fl_us)
		adjust_diff_s = calc_mod_64(adjust_diff_s, m_stable_fl_us);
	/* ==> calculate suitable adjust_diff_s */
	if (adjust_diff_s < 0) {
		/* calculate mod */
		adjust_diff_s = calc_mod_64(adjust_diff_s, m_stable_fl_us);
	}

	return adjust_diff_s;
}


static void fs_alg_sa_calc_async_m_delta(
	unsigned int m_idx, unsigned int s_idx,
	struct FrameSyncDynamicPara *p_para_m,
	struct FrameSyncDynamicPara *p_para_s)
{
	unsigned int ts_bias_us = 0;
	unsigned int i;

	/* calculate async mode master's vts sync bias */
	if (fs_inst[s_idx].sync_type & FS_SYNC_TYPE_LE)
		ts_bias_us = p_para_m->pred_next_exp_rd_offset_us[FS_HDR_LE];
	if (fs_inst[s_idx].sync_type & FS_SYNC_TYPE_SE)
		ts_bias_us = p_para_m->pred_next_exp_rd_offset_us[FS_HDR_SE];

	/* calculate predicted total delta (without timestamp diff) */
	p_para_s->async_m_delta = (ts_bias_us + p_para_m->tag_bias_us);
	for (i = 0; i < 2; ++i) {
		p_para_s->async_m_delta +=
			fs_alg_sa_calc_target_pred_fl_us(
				p_para_m->pred_fl_us, p_para_m->stable_fl_us,
				fs_inst[m_idx].fl_active_delay, i, 1);
	}

	LOG_PF_INF(
		"async_m_delta:%u, p_para_m:(delta:%u, pr_fl:(%u/%u), stable:%u, rd_offset:(LE:%u/SE:%u), fdelay:%u)\n",
		p_para_s->async_m_delta,
		p_para_m->delta,
		p_para_m->pred_fl_us[0],
		p_para_m->pred_fl_us[1],
		p_para_m->stable_fl_us,
		p_para_m->pred_next_exp_rd_offset_us[FS_HDR_LE],
		p_para_m->pred_next_exp_rd_offset_us[FS_HDR_SE],
		fs_inst[m_idx].fl_active_delay);
}


static long long fs_alg_sa_calc_adjust_diff_async(
	unsigned int m_idx, unsigned int s_idx,
	long long ts_diff_m, long long ts_diff_s,
	struct FrameSyncDynamicPara *p_para_m,
	struct FrameSyncDynamicPara *p_para_s)
{
	const unsigned int f_cell_m = get_valid_frame_cell_size(m_idx);
	const unsigned int f_cell_s = get_valid_frame_cell_size(s_idx);
	const long long m_pure_min_fl_us = (long long)p_para_m->pure_min_fl_us * f_cell_m;
	const long long m_stable_fl_us = (long long)p_para_m->stable_fl_us * f_cell_s;
	const int rout_center_en = p_para_s->sa_cfg.rout_center_en_bits;
	long long quotient = 0;
	long long adjust_diff_s = 0;

	/* unexpected case */
	if (unlikely((!p_para_m->pure_min_fl_us) || (!p_para_m->stable_fl_us))) {
		LOG_MUST(
			"NOTICE: [%u] ID:%#x(sidx:%u), #%u/#%u(m_idx:%u), detect master pure_min_fl_us:%u/stable_fl_us:%u, for preventing calculation error, abort processing ==> adjust_diff:0\n",
			s_idx,
			fs_inst[s_idx].sensor_id,
			fs_inst[s_idx].sensor_idx,
			p_para_s->magic_num,
			p_para_m->magic_num,
			m_idx,
			p_para_m->pure_min_fl_us,
			p_para_m->stable_fl_us);
		return 0;
	}

	/* prepare async master's delta info */
	fs_alg_sa_calc_async_m_delta(m_idx, s_idx, p_para_m, p_para_s);

	/* !!! Calculate adjust diff !!! */
	adjust_diff_s =
		(ts_diff_m + p_para_s->async_m_delta + p_para_m->out_fl_us) -
		(ts_diff_s + p_para_s->delta + p_para_s->out_fl_us);
	if (rout_center_en) {
		adjust_diff_s +=
			((long long)(fs_inst[m_idx].readout_time_us) -
			fs_inst[s_idx].readout_time_us) / 2;
	}
	if (p_para_s->pred_fl_err_chk_bits_m != 0)
		adjust_diff_s += p_para_s->pred_fl_err_us_m;

	/* ==> check situation (N+2/N+1 mixed), modify adjust diff */
	// if ((fs_inst[s_idx].fl_active_delay != fs_inst[m_idx].fl_active_delay)
	//	&& (adjust_diff_s > 0)) {
	//	/* if there are the pair, N+2 pred_fl will bigger than N+1 sensor */
	//	adjust_diff_s -= s_stable_fl_us;
	// }


	/* !!! Normalize adjust diff !!! */
	/* ==> checking N:1, high fps slave's adjust diff will be too big */
	if (adjust_diff_s > m_stable_fl_us)
		adjust_diff_s = calc_mod_64(adjust_diff_s, m_stable_fl_us);
	/* ==> calculate suitable adjust_diff_s */
	if (adjust_diff_s < 0) {
		/* calculate quotient (for predict flicker diff) */
		const long long q = FS_FLOOR(adjust_diff_s, m_pure_min_fl_us);

		quotient = (q > 0) ? (q) : (-q);

		/* calculate mod */
		adjust_diff_s = calc_mod_64(adjust_diff_s, m_pure_min_fl_us);
	}


	/* for low/high FS, calculate complement flicker diff */
	if (fs_inst[m_idx].flicker_en) {
		const unsigned int flk_diff =
			p_para_m->stable_fl_us - p_para_m->pure_min_fl_us;
		unsigned int complement_flk_diff =
			(flk_diff * quotient * 3 / 10);

		if (likely(complement_flk_diff < p_para_m->pure_min_fl_us)) {
			LOG_INF(
				"NOTICE: [%u] ID:%#x(sidx:%u), #%u/#%u(m_idx:%u), predict flk operation, complement flk_diff:%u (original adjust_diff:%lld, flk_diff:%u, flk_ratio:3/10, quotient:%lld, m_pure_min_fl_us:%u)\n",
				s_idx,
				fs_inst[s_idx].sensor_id,
				fs_inst[s_idx].sensor_idx,
				p_para_s->magic_num,
				p_para_m->magic_num,
				m_idx,
				complement_flk_diff,
				adjust_diff_s,
				flk_diff,
				quotient,
				p_para_m->pure_min_fl_us);
		} else {
			LOG_MUST(
				"ERROR: [%u] ID:%#x(sidx:%u), #%u/#%u(m_idx:%u), predict flk operation, complement flk_diff:%u unreasonable => not apply (original adjust_diff:%lld, flk_diff:%u, flk_ratio:3/10, quotient:%lld, m_pure_min_fl_us:%u)\n",
				s_idx,
				fs_inst[s_idx].sensor_id,
				fs_inst[s_idx].sensor_idx,
				p_para_s->magic_num,
				p_para_m->magic_num,
				m_idx,
				complement_flk_diff,
				adjust_diff_s,
				flk_diff,
				quotient,
				p_para_m->pure_min_fl_us);

			complement_flk_diff = 0;
		}

		adjust_diff_s += (long long)(complement_flk_diff);
	}

	return adjust_diff_s;
}


#if !defined(FORCE_ADJUST_SMALLER_DIFF)
static inline unsigned int fs_alg_sa_calc_sync_delay(
	const unsigned int idx, const long long adjust_diff,
	const struct FrameSyncDynamicPara *p_para)
{
	unsigned int f_cell = get_valid_frame_cell_size(idx);

	return (fs_inst[idx].fl_active_delay == 3)
		? (p_para->pred_fl_us[1] + (p_para->stable_fl_us * f_cell
			+ adjust_diff))
		: ((p_para->stable_fl_us * f_cell
			+ adjust_diff));
}
#endif


static long long fs_alg_sa_adjust_slave_diff_resolver(
	const unsigned int m_idx, const unsigned int s_idx,
	const struct FrameSyncDynamicPara *p_para_m,
	struct FrameSyncDynamicPara *p_para_s)
{
#if !defined(FORCE_ADJUST_SMALLER_DIFF)
	unsigned int sync_delay_m = 0, sync_delay_s = 0;
#endif
	const unsigned int log_str_len = LOG_BUF_STR_LEN;
	const unsigned int f_cell_m = get_valid_frame_cell_size(m_idx);
	// const unsigned int f_cell_s = get_valid_frame_cell_size(s_idx);
	unsigned int request_switch_master = 0, adjust_or_not = 1;
	unsigned int flk_diff_final, out_fl_us_final;
	long long adjust_diff_m, adjust_diff_s;
	long long ts_diff_m = 0, ts_diff_s = 0;
	char *log_buf = NULL;
	int len = 0, ret;


	/* !!! Calculate slave adjust diff !!! */
	/* ==> calculate/get current receive timestamp diff */
	fs_alg_sa_calc_m_s_ts_diff(p_para_m, p_para_s,
		&ts_diff_m, &ts_diff_s);
	fs_alg_sa_calc_pr_fl_error(m_idx, s_idx, p_para_m, p_para_s);

	/* ==> calculate master/slave adjust_diff */
	adjust_diff_s =
		fs_alg_sa_calc_adjust_diff_slave(
			m_idx, s_idx, ts_diff_m, ts_diff_s,
			p_para_m, p_para_s);
	adjust_diff_m =
		fs_alg_sa_calc_adjust_diff_master(
			m_idx, s_idx, &adjust_diff_s,
			p_para_m, p_para_s);

#if !defined(FORCE_ADJUST_SMALLER_DIFF)
	/* ==> calculate master/slave sync_delay */
	sync_delay_m =
		fs_alg_sa_calc_sync_delay(m_idx, adjust_diff_m, p_para_m);

	sync_delay_s =
		fs_alg_sa_calc_sync_delay(s_idx, adjust_diff_s, p_para_s);

	/* ==> check situation for changing master and adjusting this diff or not */
	if ((adjust_diff_s > FS_TOLERANCE) && (adjust_diff_m > 0)
			&& (sync_delay_m < sync_delay_s))
		request_switch_master = 1;
#else
	/* ==> check situation for changing master and adjusting this diff or not */
	if ((adjust_diff_s > FS_TOLERANCE) && (adjust_diff_m > 0)
			&& (adjust_diff_m < adjust_diff_s))
		request_switch_master = 1;
#endif
	if ((adjust_diff_s > FS_TOLERANCE) && (adjust_diff_m > 0)
			&& (adjust_diff_m < FS_TOLERANCE))
		request_switch_master = 1;

	/* ==> check case for adjust_diff */
	if (check_timing_critical_section(
		adjust_diff_s, (p_para_m->stable_fl_us * f_cell_m))) {
#ifndef REDUCE_FS_ALGO_LOG
		LOG_MUST(
			"NOTICE: [%u] ID:%#x(sidx:%u), #%u/#%u(m_idx:%u), req_id:%d/%d, chk timing critical section, set adjust_vsync_diff_s:0 (s(adjust_diff_s:%lld), m(s:%u, f_cell:%u)),\n",
			s_idx,
			fs_get_reg_sensor_id(s_idx),
			fs_get_reg_sensor_idx(s_idx),
			p_para_s->magic_num,
			p_para_m->magic_num,
			m_idx,
			p_para_s->req_id,
			p_para_m->req_id,
			adjust_diff_s,
			p_para_m->stable_fl_us,
			f_cell_m);
#endif
		adjust_or_not = 0;
	}


	/* !!! Update slave status for dynamic_para st & adjust diff final !!! */
	p_para_s->ref_m_idx_magic_num = p_para_m->magic_num;
	p_para_s->adj_diff_m = adjust_diff_m;
	p_para_s->adj_diff_s = adjust_diff_s;
	p_para_s->adj_or_not = adjust_or_not;
	p_para_s->chg_master = request_switch_master;
	p_para_s->ask_for_chg = request_switch_master || (!adjust_or_not);
	p_para_s->adj_diff_final = (request_switch_master || (!adjust_or_not))
		? 0 : adjust_diff_s;


	/* !!! (For LOG) Calculate final slave adjust diff !!! */
	out_fl_us_final = fs_inst[s_idx].output_fl_us + p_para_s->adj_diff_final;
	p_para_s->need_auto_restore_fl =
		fs_alg_chk_if_need_to_setup_fl_restore_ctrl(
			s_idx, out_fl_us_final, p_para_s);
	g_flk_fl_and_flk_diff(s_idx, &out_fl_us_final, &flk_diff_final, 0);


	/* !!! for log info !!! */
	ret = alloc_log_buf(log_str_len, &log_buf);
	if (unlikely(ret != 0)) {
		LOG_MUST("ERROR: log_buf allocate memory failed\n");
		return p_para_s->adj_diff_final;
	}

	FS_SNPRF(log_str_len, log_buf, len,
		"[%u] ID:%#x(sidx:%u), out_fl:%u(%u) +%lld(%u), flk(%u):+%u, s/m, #%u/#%u([%u]), req_id(%d/%d), adj_diff(%lld(%u/%u/%u)/%lld), t(%lld/%lld(+%lld(%#x)))",
		s_idx,
		fs_get_reg_sensor_id(s_idx),
		fs_get_reg_sensor_idx(s_idx),
		out_fl_us_final,
		convert2LineCount(fs_inst[s_idx].lineTimeInNs, out_fl_us_final),
		p_para_s->adj_diff_final,
		p_para_s->need_auto_restore_fl,
		fs_inst[s_idx].flicker_en,
		flk_diff_final,
		p_para_s->magic_num,
		p_para_m->magic_num,
		m_idx,
		p_para_s->req_id,
		p_para_m->req_id,
		p_para_s->adj_diff_s,
		p_para_s->adj_or_not,
		p_para_s->chg_master,
		p_para_s->ask_for_chg,
		p_para_s->adj_diff_m,
		ts_diff_s,
		ts_diff_m,
		p_para_s->pred_fl_err_us_m,
		p_para_s->pred_fl_err_chk_bits_m);

#if !defined(FORCE_ADJUST_SMALLER_DIFF)
	FS_SNPRF(log_str_len, log_buf, len,
		", sync_delay(%d/%d)",
		sync_delay_s,
		sync_delay_m);
#endif

	fs_alg_sa_adjust_diff_m_s_general_msg_connector(
		m_idx, s_idx, p_para_m, p_para_s,
		log_str_len, log_buf, len, __func__);

	LOG_MUST_LOCK("%s\n", log_buf);
	FS_TRACE_PR_LOG_MUST("%s", log_buf);

	FS_FREE(log_buf);

	return p_para_s->adj_diff_final;
}
#endif
/******************************************************************************/





/*******************************************************************************
 * fs algo operation functions (set information data)
 ******************************************************************************/
void fs_alg_set_n_1_on_off_flag(const unsigned int idx, const unsigned int flag)
{
	fs_inst[idx].n_1_on_off = flag;
}


void fs_alg_set_frame_cell_size(const unsigned int idx, const unsigned int size)
{
	fs_inst[idx].frame_cell_size = size;
}


void fs_alg_set_frame_tag(const unsigned int idx, const unsigned int count)
{
	fs_inst[idx].frame_tag = count;
}


void fs_alg_update_tg(const unsigned int idx, const unsigned int tg)
{
	fs_inst[idx].tg = tg;
}


void fs_alg_set_sync_type(const unsigned int idx, const unsigned int type)
{
	fs_inst[idx].sync_type = type;

#if defined(SYNC_WITH_CUSTOM_DIFF)
	if (fs_inst[idx].sensor_idx == CUSTOM_DIFF_SENSOR_IDX) {
		fs_inst[idx].custom_bias_us = diff_us;

		LOG_MUST(
			"NOTICE: [%u] ID:%#x(sidx:%u), set sync with diff:%u (us)\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			fs_inst[idx].custom_bias_us);
	}
#endif
}


void fs_alg_set_anti_flicker(const unsigned int idx, const unsigned int flag)
{
	fs_inst[idx].flicker_en = flag;
}


void fs_alg_update_min_fl_lc(const unsigned int idx,
	const unsigned int min_fl_lc)
{
	if (fs_inst[idx].min_fl_lc != min_fl_lc) {
		/* min_fl_lc was changed after set shutter, so update it */
		fs_inst[idx].min_fl_lc = min_fl_lc;


#if !defined(REDUCE_FS_ALGO_LOG)
		LOG_INF("[%u] ID:%#x(sidx:%u), updated min_fl:%u(%u)\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			convert2TotalTime(
				fs_inst[idx].lineTimeInNs,
				fs_inst[idx].min_fl_lc),
			fs_inst[idx].min_fl_lc);
#endif
	}
}


void fs_alg_set_debug_info_sof_cnt(const unsigned int idx,
	const unsigned int sof_cnt)
{
	fs_inst[idx].sof_cnt = sof_cnt;
}


void fs_alg_set_streaming_st_data(const unsigned int idx,
	struct fs_streaming_st (*pData))
{
	fs_inst[idx].sensor_id = pData->sensor_id;
	fs_inst[idx].sensor_idx = pData->sensor_idx;
	fs_inst[idx].tg = pData->tg;
	fs_inst[idx].fl_active_delay = set_and_chk_fl_active_delay(idx,
		pData->fl_active_delay, __func__);
	fs_inst[idx].def_min_fl_lc = pData->def_fl_lc;
	fs_inst[idx].max_fl_lc = pData->max_fl_lc;
	fs_inst[idx].def_shutter_lc = pData->def_shutter_lc;
	fs_inst[idx].margin_lc = set_and_chk_margin_lc(idx,
		pData->margin_lc, __func__);

	fs_inst[idx].lineTimeInNs = pData->lineTimeInNs;


	/* for first run, assume the hdr exp not be changed */
	fs_inst[idx].hdr_exp = pData->hdr_exp;

	/* hdr exp settings, overwrite shutter_lc value (equivalent shutter) */
	fs_alg_set_hdr_exp_st_data(idx, &pData->def_shutter_lc, &pData->hdr_exp);

	/* init frame length record st data (if needed) */
	fs_alg_init_fl_rec_st(idx);

	fs_alg_dump_streaming_data(idx);
}


void fs_alg_set_perframe_st_data(const unsigned int idx,
	struct fs_perframe_st (*pData))
{
	/* fs_inst[idx].sensor_id = pData->sensor_id; */
	/* fs_inst[idx].sensor_idx = pData->sensor_idx; */
	fs_inst[idx].min_fl_lc = pData->min_fl_lc;
	fs_inst[idx].shutter_lc = pData->shutter_lc;
	fs_inst[idx].margin_lc = set_and_chk_margin_lc(idx,
		pData->margin_lc, __func__);
	fs_inst[idx].flicker_en = pData->flicker_en;
	fs_inst[idx].lineTimeInNs = pData->lineTimeInNs;
	fs_inst[idx].readout_time_us = pData->readout_time_us;

	fs_inst[idx].prev_readout_min_fl_lc = fs_inst[idx].readout_min_fl_lc;
	fs_inst[idx].readout_min_fl_lc = 0;

	fs_inst[idx].req_id = pData->req_id;
	fs_inst[idx].frame_id = pData->frame_id;

	/* hdr exp settings, overwrite shutter_lc value (equivalent shutter) */
	fs_alg_set_hdr_exp_st_data(idx, &pData->shutter_lc, &pData->hdr_exp);

#ifndef REDUCE_FS_ALGO_LOG
	fs_alg_dump_perframe_data(idx);
#endif
}


void fs_alg_set_preset_perframe_streaming_st_data(const unsigned int idx,
	struct fs_streaming_st *p_stream_data,
	struct fs_perframe_st *p_pf_ctrl_data)
{
	/* from streaming st */
	fs_inst[idx].sensor_id = p_stream_data->sensor_id;
	fs_inst[idx].sensor_idx = p_stream_data->sensor_idx;
	fs_inst[idx].tg = p_stream_data->tg;
	fs_inst[idx].fl_active_delay = set_and_chk_fl_active_delay(idx,
		p_stream_data->fl_active_delay, __func__);
	fs_inst[idx].def_min_fl_lc = p_stream_data->def_fl_lc;
	fs_inst[idx].max_fl_lc = p_stream_data->max_fl_lc;
	fs_inst[idx].def_shutter_lc = p_stream_data->def_shutter_lc;

	/* from perframe st */
	fs_inst[idx].min_fl_lc = p_pf_ctrl_data->min_fl_lc;
	fs_inst[idx].shutter_lc = p_pf_ctrl_data->shutter_lc;
	fs_inst[idx].margin_lc = set_and_chk_margin_lc(idx,
		p_pf_ctrl_data->margin_lc, __func__);
	fs_inst[idx].flicker_en = p_pf_ctrl_data->flicker_en;
	fs_inst[idx].lineTimeInNs = p_pf_ctrl_data->lineTimeInNs;
	fs_inst[idx].readout_time_us = p_pf_ctrl_data->readout_time_us;

	fs_inst[idx].prev_readout_min_fl_lc = fs_inst[idx].readout_min_fl_lc;
	fs_inst[idx].readout_min_fl_lc = 0;

	fs_inst[idx].req_id = p_pf_ctrl_data->req_id;
	fs_inst[idx].frame_id = p_pf_ctrl_data->frame_id;

	/* for first run, assume the hdr exp not be changed */
	p_stream_data->hdr_exp = p_pf_ctrl_data->hdr_exp;
	fs_inst[idx].hdr_exp = p_pf_ctrl_data->hdr_exp;

	/* hdr exp settings, overwrite shutter_lc value (equivalent shutter) */
	fs_alg_set_hdr_exp_st_data(idx,
		&p_stream_data->def_shutter_lc, &p_stream_data->hdr_exp);

	fs_alg_dump_fs_inst_data(idx);
}


void fs_alg_set_extend_framelength(const unsigned int idx,
	const unsigned int ext_fl_lc, const unsigned int ext_fl_us)
{
	if (ext_fl_lc == 0 && ext_fl_us == 0) {
		/* clear/exit extend framelength stage */
		fs_inst[idx].extend_fl_lc = ext_fl_lc;
		fs_inst[idx].extend_fl_us = ext_fl_us;

	} else if (ext_fl_lc != 0 && ext_fl_us == 0) {
		fs_inst[idx].extend_fl_lc = ext_fl_lc;
		fs_inst[idx].extend_fl_us =
			convert2TotalTime(
				fs_inst[idx].lineTimeInNs,
				fs_inst[idx].extend_fl_lc);

	} else if (ext_fl_lc == 0 && ext_fl_us != 0) {
		fs_inst[idx].extend_fl_us = ext_fl_us;
		fs_inst[idx].extend_fl_lc =
			convert2LineCount(
				fs_inst[idx].lineTimeInNs,
				fs_inst[idx].extend_fl_us);

	} else { /* both have non zero value */
		unsigned int tmp_ext_fl_lc = 0;

		tmp_ext_fl_lc =
			convert2LineCount(
				fs_inst[idx].lineTimeInNs,
				ext_fl_us);

		fs_inst[idx].extend_fl_lc = (ext_fl_lc > tmp_ext_fl_lc)
			? ext_fl_lc : tmp_ext_fl_lc;

		fs_inst[idx].extend_fl_us =
			convert2TotalTime(
				fs_inst[idx].lineTimeInNs,
				fs_inst[idx].extend_fl_lc);

		LOG_INF(
			"WARNING: [%u] ID:%#x(sidx:%u), both set value, ext_fl_lc:%u, ext_fl_us:%u\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			ext_fl_lc,
			ext_fl_us);
	}

	LOG_MUST(
		"NOTICE: [%u] ID:%#x(sidx:%u), detect driver receive extend FL ctrl  [%u(%u)]\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		fs_inst[idx].extend_fl_us,
		fs_inst[idx].extend_fl_lc);
}


void fs_alg_seamless_switch(const unsigned int idx,
	struct fs_seamless_st *p_seamless_info,
	const struct fs_sa_cfg *p_sa_cfg)
{
	struct FrameSyncDynamicPara para = {0};

	/* error handling (unexpected case) */
	if (unlikely((p_seamless_info == NULL) || (p_sa_cfg == NULL))) {
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u), get p_seamless_info:%p or p_sa_cfg:%p, return\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			p_seamless_info,
			p_sa_cfg);
		return;
	}

	/* update info that may be changed through seamless switch */
	fs_inst[idx].fl_active_delay = p_seamless_info->fl_active_delay;

	/* prepare new dynamic para */
	fs_alg_sa_init_new_ctrl(p_sa_cfg, &para);

	/* get Vsync data by Frame Monitor */
	fs_alg_sa_get_last_vts_info(idx, &para);

	/* X. update dynamic para for sharing to other sensor */
	fs_alg_sa_update_seamless_dynamic_para(idx, p_seamless_info, &para);

	/* fs_alg_sa_dump_dynamic_para(idx); */
}


void fs_alg_sa_notify_setup_all_frame_info(const unsigned int idx)
{
	// fs_alg_sa_dump_dynamic_para(idx);
	fs_alg_setup_frame_monitor_fmeas_data(idx);
}


void fs_alg_sa_notify_vsync(const unsigned int idx)
{
	fs_alg_sa_update_last_dynamic_fps_info(idx);
}


void fs_alg_sa_notify_get_ts_info(const unsigned int idx,
	const enum fs_timestamp_src_type ts_src_type)
{
	const unsigned int query_ts_idx[1] = {idx};

	fs_inst[idx].ts_src_type = ts_src_type;

	/* get timestamp info and calibrate frame recorder data */
	fs_inst[idx].is_nonvalid_ts =
		g_vsync_timestamp_data(query_ts_idx, 1, ts_src_type);
}


void fs_alg_sa_update_dynamic_infos(const unsigned int idx,
	const unsigned int is_pf_ctrl)
{
	struct FrameSyncDynamicPara para = {0};

	fs_alg_sa_init_dynamic_para_info(idx, &para);

	if (is_pf_ctrl) {
		/* get Vsync data by Frame Monitor */
		fs_alg_sa_get_last_vts_info(idx, &para);
	}

	/* update dynamic para for sharing to other sensor */
	fs_alg_sa_update_dynamic_para(idx, &para);
}
/******************************************************************************/





/*******************************************************************************
 * Frame Sync Algorithm function
 ******************************************************************************/
static void do_fps_sync(unsigned int solveIdxs[], unsigned int len)
{
	unsigned int i = 0;
	unsigned int target_fl_us = 0;

	int ret = 0;
	char *log_buf = NULL;

	log_buf = FS_CALLOC(LOG_BUF_STR_LEN, sizeof(char));
	if (log_buf == NULL) {
		LOG_PR_ERR("ERROR: log_buf allocate memory failed\n");

		return;
	}

	log_buf[0] = '\0';


	/* 1. calc each fl_us for each sensor, take max fl_us as target_fl_us */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];
		unsigned int fl_us = 0, fl_lc = 0;

		fl_lc = calc_min_fl_lc(idx,
				fs_inst[idx].min_fl_lc, PREDICT_STABLE_FL);
		fl_us = convert2TotalTime(fs_inst[idx].lineTimeInNs, fl_lc);


		ret = snprintf(log_buf + strlen(log_buf),
			LOG_BUF_STR_LEN - strlen(log_buf),
			"[%u] ID:%#x(sidx:%u), fl:%u(%u) (%u/%u/%u/%u/%u, %u); ",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			fl_us,
			fl_lc,
			fs_inst[idx].shutter_lc,
			fs_inst[idx].margin_lc,
			fs_inst[idx].min_fl_lc,
			fs_inst[idx].readout_min_fl_lc,
			fs_inst[idx].p_frecs[0]->framelength_lc,
			fs_inst[idx].lineTimeInNs);

		if (ret < 0)
			LOG_INF("ERROR: LOG encoding error, ret:%d\n", ret);


		if (fl_us > target_fl_us)
			target_fl_us = fl_us;
	}


	/* 2. use target_fl_us as output_fl_us for each sensor */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];

		/* set to min framelength */
		/* all framelength operation must use this API */
		set_fl_us(idx, target_fl_us);
	}

	target_min_fl_us = target_fl_us;


	ret = snprintf(log_buf + strlen(log_buf),
		LOG_BUF_STR_LEN - strlen(log_buf),
		"FL sync to %u (us)",
		target_fl_us);

	if (ret < 0)
		LOG_INF("ERROR: LOG encoding error, ret:%d\n", ret);


	LOG_INF("%s\n", log_buf);
	FS_FREE(log_buf);
}


static void adjust_vsync_diff(unsigned int solveIdxs[], unsigned int len)
{
	unsigned int i = 0;

	unsigned int min_vtick_diff = (0 - 1);    // 0xffffffff
	unsigned int target_vts = 0;
	unsigned int pf_ctrl_timing_issue[SENSOR_MAX_NUM] = {0};

	unsigned int anti_flicker_en = 0;
	unsigned int flicker_vdiff[SENSOR_MAX_NUM] = {0};
	unsigned int max_flicker_vdiff = 0;


	/* for snprintf and memory allocate */
	int ret = 0;
	char *log_buf[SENSOR_MAX_NUM] = {NULL};

	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];

#ifdef FS_UT
		log_buf[idx] = calloc(LOG_BUF_STR_LEN, sizeof(char));
#else
		log_buf[idx] = kcalloc(LOG_BUF_STR_LEN, sizeof(char),
					GFP_KERNEL);
#endif // FS_UT

		if (log_buf[idx] == NULL) {
			LOG_PR_ERR(
				"ERROR: [%u] log_buf allocate memory failed\n",
				idx);

			/* return, and free alloc memory */
			goto free_alloc_mem;
		} else
			log_buf[idx][0] = '\0';
	}


	/* 0. check vsync timestamp (preventing last vts is "0") */
	if (check_fs_inst_vsync_data_valid(solveIdxs, len) == 0) {
		LOG_PR_WARN(
			"ERROR: Incorrect vsync timestamp detected, not adjust vsync diff\n"
			);

		/* return, and free alloc memory */
		goto free_alloc_mem;
	}


	/* 1. calculate vsync diff for all */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];


		fs_inst[idx].vdiff =
			fs_inst[idx].last_vts * tick_factor - cur_tick;


		if (fs_inst[idx].vdiff < min_vtick_diff)
			min_vtick_diff = fs_inst[idx].vdiff;
	}
	// LOG_INF("min vtick diff:%u\n", min_vtick_diff);
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];


		fs_inst[idx].vdiff -= min_vtick_diff;


		/* prevent for floating point exception */
		if (tick_factor != 0)
			fs_inst[idx].vdiff /= tick_factor;

		// fs_inst[idx].predicted_vts = fs_inst[idx].vdiff;


		ret = snprintf(log_buf[idx] + strlen(log_buf[idx]),
			LOG_BUF_STR_LEN - strlen(log_buf[idx]),
			"[%u] ID:%#x(sidx:%u), tg:%u, vsyncs:%u, vdiff:%u, ",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			fs_inst[idx].tg,
			fs_inst[idx].vsyncs,
			fs_inst[idx].vdiff);

		if (ret < 0)
			LOG_INF("ERROR: LOG encoding error, ret:%d\n", ret);
	}


	/* 2. predict current and next vsync timestamp */
	/*    and calculate vsync timestamp bias */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];
		unsigned int *predicted_fl_us = fs_inst[idx].predicted_fl_us;
		unsigned int vts_bias = 0, vts_bias_us = 0;

		fs_inst[idx].vdiff += predicted_fl_us[0] + predicted_fl_us[1];

		vts_bias = calc_vts_sync_bias_lc(idx);
		vts_bias_us =
			convert2TotalTime(fs_inst[idx].lineTimeInNs, vts_bias);

		fs_inst[idx].vdiff += vts_bias_us;


		ret = snprintf(log_buf[idx] + strlen(log_buf[idx]),
			LOG_BUF_STR_LEN - strlen(log_buf[idx]),
			"pred_fl(c:%u(%u), n:%u(%u)), fdelay:%u, bias(%u(%u), opt:%u), ",
			predicted_fl_us[0],
			fs_inst[idx].predicted_fl_lc[0],
			predicted_fl_us[1],
			fs_inst[idx].predicted_fl_lc[1],
			fs_inst[idx].fl_active_delay,
			vts_bias_us,
			vts_bias,
			fs_inst[idx].sync_type);

		if (ret < 0)
			LOG_INF("ERROR: LOG encoding error, ret:%d\n", ret);


#ifdef FS_UT
		/* update frame monitor current predicted framelength data */
		frm_update_predicted_curr_fl_us(idx, predicted_fl_us[0]);
		frm_update_next_vts_bias_us(idx, vts_bias_us);
#endif // FS_UT
	}


	/* 3. calculate diff of predicted_vts */
	/* 3.1 find target timestamp to sync */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];

		if (fs_inst[idx].vdiff > target_vts)
			target_vts = fs_inst[idx].vdiff;
	}
	/* 3.2 extend frame length to align target timestamp */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];
		unsigned int pred_vdiff = 0;
		unsigned int fl_lc = 0;


		/* detect pf ctrl timing error */
		pred_vdiff = target_vts - fs_inst[idx].vdiff;
		if (check_timing_critical_section(
				pred_vdiff, target_min_fl_us)) {

			/* pf ctrl trigger in critiacal section */
			/* maybe the coming soon timestamp is in sync */
			/* => Do not adjust vdiff, only use fps sync result */

			pf_ctrl_timing_issue[idx] = 1UL << 1;
			pred_vdiff = 0;

#ifndef REDUCE_FS_ALGO_LOG
			LOG_INF(
				"WARNING: [%u] ID:%#x(sidx:%u), in timing_cs, coming Vsync is in sync, set pred_vdiff:%u\n",
				idx,
				fs_inst[idx].sensor_id,
				fs_inst[idx].sensor_idx,
				pred_vdiff);
#endif // REDUCE_FS_ALGO_LOG
		} else {
			if (pred_vdiff >= target_min_fl_us) {
				/* pf ctrl timing error */
				/* prevent framelength getting bigger */
				/* => Do not adjust vdiff, only use fps sync result */

				pf_ctrl_timing_issue[idx] = 1;
				pred_vdiff = 0;
			}
		}


		/* add diff and set framelength */
		/* all framelength operation must use this API */
		fl_lc = fs_inst[idx].output_fl_us + pred_vdiff;

		set_fl_us(idx, fl_lc);


		ret = snprintf(log_buf[idx] + strlen(log_buf[idx]),
			LOG_BUF_STR_LEN - strlen(log_buf[idx]),
			"pred_vdiff:%u(t_issue:%u), flk_en:%u, out_fl:%u(%u)",
			target_vts - fs_inst[idx].vdiff,
			pf_ctrl_timing_issue[idx],
			fs_inst[idx].flicker_en,
			fs_inst[idx].output_fl_us,
			fs_inst[idx].output_fl_lc);

		if (ret < 0)
			LOG_INF("ERROR: LOG encoding error, ret:%d\n", ret);
	}


	/* TODO: add check perframe ctrl timing error */


	/* 4. anti-flicker */

	/* 4.1 check anti-flicker enable, */
	/*     and find out max flicker vdiff simultaneously */
	for (i = 0; i < len; ++i) {
		const unsigned int idx = solveIdxs[i];
		const unsigned int fl_us_orig = fs_inst[idx].output_fl_us;
		unsigned int fl_us_result = fl_us_orig, ret;

		/* check anti-flicker enable */
		if (fs_inst[idx].flicker_en == 0)
			continue;

		/* for anti-flicker enable case */
		anti_flicker_en |= fs_inst[idx].flicker_en;


		/* calculate anti-flicker vdiff */
		/*      flk vdiff = 0 => not flk fl */
		/*      flk vdiff > 0 => flk fl, adjust fl */
		ret = fs_flk_get_anti_flicker_fl(
			fs_inst[idx].flicker_en, fl_us_orig, &fl_us_result);
		if (unlikely(ret != FLK_ERR_NONE)) {
			LOG_MUST(
				"ERROR: call fs flk get anti flk fl, ret:%u   [flk_en:%u/fl:(%u->%u)]\n",
				ret, fs_inst[idx].flicker_en,
				fl_us_orig, fl_us_result);
		}
		flicker_vdiff[idx] = fl_us_result - fl_us_orig;
		if (flicker_vdiff[idx] == 0)
			continue;


		/* get maximum flicker vdiff */
		if (flicker_vdiff[idx] > max_flicker_vdiff)
			max_flicker_vdiff = flicker_vdiff[idx];
	}


	if (anti_flicker_en == 0) {
		for (i = 0; i < len; ++i) {
			unsigned int idx = solveIdxs[i];

			LOG_INF("%s\n", log_buf[idx]);
		}

		/* return, and free alloc memory */
		goto free_alloc_mem;
	}


	/* 4.2 add max anti-flicker vdiff to (all) sony sensor */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];
		unsigned int fl_lc = 0;


		/* add max flk vdiff and set framelength */
		/* all framelength operation must use this API */
		fl_lc = fs_inst[idx].output_fl_us + max_flicker_vdiff;
		set_fl_us(idx, fl_lc);


		ret = snprintf(log_buf[idx] + strlen(log_buf[idx]),
			LOG_BUF_STR_LEN - strlen(log_buf[idx]),
			", +flk:%u, 10xFPS:%u, out_fl:%u(%u)",
			max_flicker_vdiff,
			(unsigned int)(10000000/fs_inst[idx].output_fl_us),
			fs_inst[idx].output_fl_us,
			fs_inst[idx].output_fl_lc);

		if (ret < 0)
			LOG_INF("ERROR: LOG encoding error, ret:%d\n", ret);
	}


	for (i = 0; i < len; ++i)
		LOG_INF("%s\n", log_buf[solveIdxs[i]]);


free_alloc_mem:
	for (i = 0; i < len; ++i) {
		if (log_buf[solveIdxs[i]] == NULL)
			continue;
#ifdef FS_UT
		free(log_buf[solveIdxs[i]]);
#else
		kfree(log_buf[solveIdxs[i]]);
#endif // FS_UT
	}
}


/* return "0" -> done; "non 0" -> error ? */
unsigned int fs_alg_solve_frame_length(
	unsigned int solveIdxs[],
	unsigned int framelength_lc[], unsigned int len)
{
	unsigned int i = 0;


#ifndef REDUCE_FS_ALGO_LOG
	LOG_INF("%u sensors do frame sync\n", len);
#endif // REDUCE_FS_ALGO_LOG


	/* boundary checking for how many sensor sync */
	// if (len > SENSOR_MAX_NUM)
	if (len > TG_MAX_NUM)
		return 1;


#ifndef REDUCE_FS_ALGO_LOG
	/* dump information */
	for (i = 0; i < len; ++i)
		dump_fs_inst_data(solveIdxs[i]);

	// dump_all_fs_inst_data();
#endif // REDUCE_FS_ALGO_LOG


	/* 1. get Vsync data by Frame Monitor */
	if (g_vsync_timestamp_data(solveIdxs, len, FS_TS_SRC_CCU)) {
		LOG_PR_WARN("Get Vsync data ERROR\n");
		return 1;
	}


	/* 2. FPS Sync */
	do_fps_sync(solveIdxs, len);


	/* 3. adjust Vsync diff */
	adjust_vsync_diff(solveIdxs, len);


	/* 4. copy framelength_lc results */
	for (i = 0; i < len; ++i) {
		unsigned int idx = solveIdxs[i];

		framelength_lc[idx] = fs_inst[idx].output_fl_lc;
	}


	return 0;
}


#ifdef SUPPORT_FS_NEW_METHOD
static unsigned int do_fps_sync_sa_proc_checker(const unsigned int idx,
	struct FrameSyncDynamicPara *p_para,
	const struct fs_dynamic_fps_record_st *fps_info)
{
	unsigned int ret = 0;

	/* use pure min FL because it does NOT take flicker effects into account */
	if (likely(p_para->pure_min_fl_us == fps_info->pure_min_fl_us))
		return 0;

	FS_WRITE_BIT(idx, 1, &fs_sa_inst.unstable_fps_bits);
	p_para->fps_status = FS_DY_FPS_UNSTABLE;
	p_para->fps_status_aligned = FS_DY_FPS_UNSTABLE;
	ret = 1;

	LOG_MUST_LOCK(
		"WARNING: [%u] ID:%#x(sidx:%u), detect pure min FL for shutter NOT stable, %#x, (#%u/req_id:%d)[%u/%u/%u] => (#%u/req_id:%d)[%u/%u/%u], target min FL:%u, ts:%llu, ret:%u\n",
		idx,
		fs_get_reg_sensor_id(idx),
		fs_get_reg_sensor_idx(idx),
		FS_ATOMIC_READ(&fs_sa_inst.unstable_fps_bits),
		fps_info->magic_num,
		fps_info->req_id,
		fps_info->pure_min_fl_us,
		fps_info->min_fl_us,
		fps_info->target_min_fl_us,
		p_para->magic_num,
		p_para->req_id,
		p_para->pure_min_fl_us,
		p_para->min_fl_us,
		p_para->target_min_fl_us,
		p_para->target_min_fl_us,
		p_para->last_ts,
		ret);

	return ret;
}


/* static */ unsigned int do_fps_sync_sa(const struct fs_sa_cfg *p_sa_cfg,
	struct FrameSyncDynamicPara *p_para, const unsigned int sync_flk_en)
{
	struct fs_dynamic_fps_record_st fps_info_arr[SENSOR_MAX_NUM] = {0};
	struct fs_dynamic_fps_record_st last_fps_info_arr[SENSOR_MAX_NUM] = {0};
	const unsigned int valid_bits =
		(p_sa_cfg->valid_sync_bits ^ p_sa_cfg->async_s_bits);
	const unsigned int idx = p_sa_cfg->idx;
	unsigned int max_target_min_fl_us = 0, max_pure_min_fl_us = 0;
	unsigned int unstable_fps_bits;
	unsigned int skip_adjust_vsync_diff;
	unsigned int fps_sync_fl_result, flk_diff;
	unsigned int i;

	fs_alg_sa_query_all_dynamic_fps_info(fps_info_arr, last_fps_info_arr,
		SENSOR_MAX_NUM, &unstable_fps_bits);

	skip_adjust_vsync_diff =
		do_fps_sync_sa_proc_checker(idx, p_para, &fps_info_arr[idx]);
	if (skip_adjust_vsync_diff)
		return skip_adjust_vsync_diff;

	/* find maximum value of "min FL" & "target min FL" */
	for (i = 0; i < SENSOR_MAX_NUM; ++i) {
		const unsigned int is_unstable = (unstable_fps_bits >> i) & 1UL;

		if (((valid_bits >> i) & 1UL) == 0)
			continue;

		if (is_unstable) {
			if (fps_info_arr[i].pure_min_fl_us > max_pure_min_fl_us)
				max_pure_min_fl_us = fps_info_arr[i].pure_min_fl_us;
			if (fps_info_arr[i].target_min_fl_us
					> max_target_min_fl_us) {
				max_target_min_fl_us =
					fps_info_arr[i].target_min_fl_us;
			}
		} else {
			if (last_fps_info_arr[i].pure_min_fl_us > max_pure_min_fl_us)
				max_pure_min_fl_us = last_fps_info_arr[i].pure_min_fl_us;
			if (last_fps_info_arr[i].target_min_fl_us
					> max_target_min_fl_us) {
				max_target_min_fl_us =
					last_fps_info_arr[i].target_min_fl_us;
			}
		}
	}

	/* check "target min FL" can be retract to maximum "min FL" or not */
	fps_sync_fl_result = (max_pure_min_fl_us < max_target_min_fl_us)
		? max_pure_min_fl_us : max_target_min_fl_us;
	g_flk_fl_and_flk_diff(idx, &fps_sync_fl_result, &flk_diff, sync_flk_en);

	fs_alg_sa_update_target_stable_fl_info(idx, p_para, fps_sync_fl_result);


	/* !!! for log info !!! */
	if (unlikely(_FS_LOG_ENABLED(LOG_FS_ALGO_FPS_INFO))) {
		const unsigned int log_str_len = 1024;
		char *log_buf = NULL;
		int len = 0, ret;

		ret = alloc_log_buf(log_str_len, &log_buf);
		if (unlikely(ret != 0)) {
			LOG_MUST("ERROR: log_buf allocate memory failed\n");
			goto end_do_fps_sync_sa;
		}

		FS_SNPRF(log_str_len, log_buf, len,
			"[%u] ID:%#x(sidx:%u), #%u, m_idx:%u, target FL sync to %u(%u)(+%u), fl:(%u(%u)/%u/%u/%u,%u), flk_en:[%u/%u/%u/%u/%u], valid:%#x(%#x/%#x), unstable:%#x",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			p_para->magic_num,
			p_sa_cfg->m_idx,
			fps_sync_fl_result,
			convert2LineCount(
				fs_inst[idx].lineTimeInNs,
				fps_sync_fl_result),
			flk_diff,
			p_para->pure_min_fl_us,
			p_para->pure_min_fl_lc,
			p_para->min_fl_us,
			p_para->target_min_fl_us,
			p_para->stable_fl_us,
			fs_inst[idx].lineTimeInNs,
			fs_inst[0].flicker_en,
			fs_inst[1].flicker_en,
			fs_inst[2].flicker_en,
			fs_inst[3].flicker_en,
			fs_inst[4].flicker_en,
			valid_bits,
			p_sa_cfg->valid_sync_bits,
			p_sa_cfg->async_s_bits,
			unstable_fps_bits);

		fs_alg_sa_dynamic_fps_info_arr_msg_connector(idx,
			fps_info_arr, last_fps_info_arr, SENSOR_MAX_NUM, valid_bits,
			log_str_len, log_buf, len, __func__);

		LOG_MUST_LOCK("%s\n", log_buf);
		FS_TRACE_PR_LOG_MUST("%s", log_buf);
		FS_FREE(log_buf);
	}
end_do_fps_sync_sa:

	return 0;
}


/* static */ unsigned int fps_sync_sa_handler(const struct fs_sa_cfg *p_sa_cfg,
	struct FrameSyncDynamicPara *p_para)
{
	const unsigned int idx = p_sa_cfg->idx;
	const unsigned int sync_flk_en = 1; // chk_if_need_to_sync_flk_en_status(idx);
	unsigned int flk_diff, out_fl_us;
	unsigned int do_skip;

	fs_alg_sa_setup_basic_fl_info(idx, p_para, sync_flk_en, &flk_diff);
	do_skip = do_fps_sync_sa(p_sa_cfg, p_para, sync_flk_en);

	out_fl_us = ((fs_inst[idx].fl_active_delay == 3))
		? p_para->stable_fl_us
		: (p_para->pure_min_fl_us/p_para->f_cell);
	fs_alg_setup_basic_out_fl(idx, &out_fl_us, p_para);

	fs_alg_sa_update_fl_us(idx, out_fl_us, p_para);

	return do_skip;
}


static unsigned int dynamic_fps_set_out_fl_us(const unsigned int idx,
	const struct fs_sa_cfg *p_sa_cfg,
	const struct FrameSyncDynamicPara *p_para,
	const unsigned int last_fps_sync_result)
{
	/* const unsigned int valid_bits = */
	/*	(p_sa_cfg->valid_sync_bits ^ p_sa_cfg->async_s_bits); */
	const unsigned int fdelay = fs_inst[idx].fl_active_delay;
	unsigned int out_fl_us;

	/* NOT mixed fdelay type => set out FL to stable FL */
	/* if (chk_if_fdelay_type_mixed_together(valid_bits) == 0) */
		/* return p_para->stable_fl_us; */

	/* Mixed fdelay type => ASSIGN/SETUP out FL by scenario/condition */
	if (fdelay == 3) {
		out_fl_us = p_para->stable_fl_us;
	} else if (fdelay == 2) {
		switch (p_para->fps_status_aligned) {
		case FS_DY_FPS_STABLE:
		case FS_DY_FPS_DEC:
		case FS_DY_FPS_DEC_MOST:
			/**
			 * in these case, on N+2 sensor FL will be auto-extended,
			 * so, N+1 sensor can just align to the stable FL.
			 */
			out_fl_us = p_para->stable_fl_us;
			break;
		case FS_DY_FPS_INC:
			/**
			 * in the case, on N+2 sensor FL will keep on previous setting,
			 * so, N+1 sensor should set the output FL value as same as
			 * the last stable FL result to match N+2 sensor.
			 */
			/* out_fl_us = p_fps_info->stable_fl_us; */
			/* last_fps_sync_result == 0 => NOT do fps align, e.g., single cam */
			out_fl_us = (last_fps_sync_result != 0)
				? last_fps_sync_result : p_para->stable_fl_us;
			break;
		/* case FS_DY_FPS_USER_CHG: */
			/* TBD */
			/* break; */
		case FS_DY_FPS_SINGLE_CAM_SKIP:
		default:
			out_fl_us = p_para->stable_fl_us;
			break;
		}
	} else {
		/* error handle */
		out_fl_us = p_para->stable_fl_us;
		LOG_MUST(
			"ERROR: [%u] ID:%#x(sidx:%u), detect non valid 'frame_time_delay_frame/delay_frame':%u (must be 2 or 3), plz check sensor driver for giving fsync the correct value\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			fs_inst[idx].fl_active_delay);
	}

	return out_fl_us;
}


static enum fs_dynamic_fps_status dynamic_fps_check(const unsigned int idx,
	const unsigned int fl_us, const unsigned int fps_info_fl_us)
{
	enum fs_dynamic_fps_status fps_status = FS_DY_FPS_STABLE;

	/* use pure min FL because it does NOT take flicker effects into account */
	if (likely(fl_us == fps_info_fl_us))
		return FS_DY_FPS_STABLE;

	FS_WRITE_BIT(idx, 1, &fs_sa_inst.unstable_fps_bits);
	if (fl_us > fps_info_fl_us) {
		/* FL is extended => FPS decrease */
		fps_status = FS_DY_FPS_DEC;
	} else if (fl_us < fps_info_fl_us) {
		/* FL is retracted => FPS increase */
		fps_status = FS_DY_FPS_INC;
	}
	return fps_status;
}


/**
 * return:
 *          0: valid for doing fps sync flow
 *      non-0: should SKIP fps sync flow
 *
 * filter out situations that require additional processing.
 * e.g., single cam running / usr chg fps
 */
static unsigned int dynamic_fps_scen_pre_check(const unsigned int idx,
	struct FrameSyncDynamicPara *p_para, const unsigned int valid_bits)
{
	/* case checking */
	if (FS_POPCOUNT(valid_bits) == 1) {
		/**
		 * if only single cam is streaming => skip below flow for
		 * keeping the sensor FPS responsive when flicker on/off.
		 */
		p_para->fps_status = FS_DY_FPS_SINGLE_CAM_SKIP;
		p_para->fps_status_aligned = FS_DY_FPS_SINGLE_CAM_SKIP;
		return FS_DY_FPS_SINGLE_CAM_SKIP;
	}

	return 0;
}


static unsigned int do_fps_sync_sa_v2(const struct fs_sa_cfg *p_sa_cfg,
	struct FrameSyncDynamicPara *p_para,
	struct fs_dynamic_fps_record_st fps_info_arr[],
	struct fs_dynamic_fps_record_st last_fps_info_arr[],
	const unsigned int sync_flk_en)
{
	const unsigned int idx = p_sa_cfg->idx;
	const unsigned int valid_bits =
		(p_sa_cfg->valid_sync_bits ^ p_sa_cfg->async_s_bits);
	unsigned int max_pure_min_fl_us = 0, last_max_min_fl_us = 0;
	unsigned int fps_sync_fl_result, last_fps_sync_fl_result = 0;
	unsigned int flk_diff, out_fl_us, max_cnt = 0;
	unsigned int i, need_to_skip;

	/* TODO: add method for handling user change max FPS */

	/* !!! pre-check current situation for skipping fps align flow !!! */
	need_to_skip = dynamic_fps_scen_pre_check(idx, p_para, valid_bits);
	if (!need_to_skip) {
		/* !!! check the dynamic fps status (ae ctrl) !!! */
		p_para->fps_status =
			dynamic_fps_check(idx,
				p_para->pure_min_fl_us,
				last_fps_info_arr[idx].pure_min_fl_us);

		/* !!! find maximum value of "min FL" & "target min FL" !!! */
		for (i = 0; i < SENSOR_MAX_NUM; ++i) {
			if (((valid_bits >> i) & 1UL) == 0)
				continue;

			/* => for current fps alignment */
			if (fps_info_arr[i].pure_min_fl_us > max_pure_min_fl_us) {
				max_pure_min_fl_us = fps_info_arr[i].pure_min_fl_us;
				max_cnt = 1;
			} else if (fps_info_arr[i].pure_min_fl_us == max_pure_min_fl_us) {
				/* increase the counter (checking FPS DEC MOST needed) */
				max_cnt++;
			}
			/* => for last/prev fps alignment */
			if (last_fps_info_arr[i].pure_min_fl_us > last_max_min_fl_us) {
				last_max_min_fl_us =
					last_fps_info_arr[i].pure_min_fl_us;
			}
		}
		fps_sync_fl_result = max_pure_min_fl_us;
		last_fps_sync_fl_result = last_max_min_fl_us;

		/* update the fps alignment result to "stable FL" & "target FL" */
		g_flk_fl_and_flk_diff(idx,
			&fps_sync_fl_result, &flk_diff, sync_flk_en);
		fs_alg_sa_update_target_stable_fl_info(idx,
			p_para, fps_sync_fl_result);

		/* !!! After fps aligned, check the dynamic fps status again !!! */
		p_para->fps_status_aligned =
			dynamic_fps_check(idx,
				fps_sync_fl_result,
				last_fps_info_arr[idx].target_min_fl_us);
		if (p_para->fps_status_aligned == FS_DY_FPS_DEC) {
			if ((p_para->pure_min_fl_us == fps_sync_fl_result)
					&& (max_cnt == 1))
				p_para->fps_status_aligned = FS_DY_FPS_DEC_MOST;
		}
	}

	/* setup the output fl info */
	out_fl_us =
		dynamic_fps_set_out_fl_us(idx,
			p_sa_cfg, p_para, last_fps_sync_fl_result);
	fs_alg_setup_basic_out_fl(idx, &out_fl_us, p_para);
	fs_alg_sa_update_fl_us(idx, out_fl_us, p_para);


	/* !!! for log info !!! */
	if (unlikely(_FS_LOG_ENABLED(LOG_FS_ALGO_FPS_INFO) && (!need_to_skip))) {
		const unsigned int log_str_len = 1024;
		char *log_buf = NULL;
		int len = 0, ret;

		ret = alloc_log_buf(log_str_len, &log_buf);
		if (unlikely(ret != 0)) {
			LOG_MUST("ERROR: log_buf allocate memory failed\n");
			goto end_do_fps_sync_sa;
		}

		FS_SNPRF(log_str_len, log_buf, len,
			"[%u] ID:%#x(sidx:%u), #%u, out_fl:%u(%u), status:(%u=>%u) => fps sync:(%u(%u)(+%u)/prev:%u(%u)), fl:(pure:%u(%u)/min:%u/tar:%u/stable:%u,%u), flk_en:[%u/%u/%u/%u/%u], valid:%#x(%#x/%#x), unstable:%#x, ts:%llu",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			p_para->magic_num,
			fs_inst[idx].output_fl_us,
			convert2LineCount(
				fs_inst[idx].lineTimeInNs,
				fs_inst[idx].output_fl_us),
			p_para->fps_status,
			p_para->fps_status_aligned,
			fps_sync_fl_result,
			convert2LineCount(
				fs_inst[idx].lineTimeInNs,
				fps_sync_fl_result),
			flk_diff,
			last_fps_sync_fl_result,
			convert2LineCount(
				fs_inst[idx].lineTimeInNs,
				last_fps_sync_fl_result),
			p_para->pure_min_fl_us,
			p_para->pure_min_fl_lc,
			p_para->min_fl_us,
			p_para->target_min_fl_us,
			p_para->stable_fl_us,
			fs_inst[idx].lineTimeInNs,
			fs_inst[0].flicker_en,
			fs_inst[1].flicker_en,
			fs_inst[2].flicker_en,
			fs_inst[3].flicker_en,
			fs_inst[4].flicker_en,
			valid_bits,
			p_sa_cfg->valid_sync_bits,
			p_sa_cfg->async_s_bits,
			FS_ATOMIC_READ(&fs_sa_inst.unstable_fps_bits),
			p_para->last_ts);

		fs_alg_sa_dynamic_fps_info_arr_msg_connector(idx,
			fps_info_arr, last_fps_info_arr, SENSOR_MAX_NUM, valid_bits,
			log_str_len, log_buf, len, __func__);

		LOG_MUST_LOCK("%s\n", log_buf);
		FS_TRACE_PR_LOG_MUST("%s", log_buf);
		FS_FREE(log_buf);
	}

end_do_fps_sync_sa:

	return p_para->fps_status_aligned;
}


static unsigned int fps_sync_sa_handler_v2(const struct fs_sa_cfg *p_sa_cfg,
	struct FrameSyncDynamicPara *p_para)
{
	struct fs_dynamic_fps_record_st fps_info_arr[SENSOR_MAX_NUM] = {0};
	struct fs_dynamic_fps_record_st last_fps_info_arr[SENSOR_MAX_NUM] = {0};
	/* const unsigned int sync_flk_en = chk_if_need_to_sync_flk_en_status(idx); */
	const unsigned int sync_flk_en = 1;
	const unsigned int idx = p_sa_cfg->idx;
	unsigned int unstable_fps_bits, flk_diff, do_skip;

	/* init/setup basic FL info */
	fs_alg_sa_setup_basic_fl_info(idx, p_para, sync_flk_en, &flk_diff);

	/* query all sensors' fps info */
	fs_alg_sa_query_all_dynamic_fps_info(fps_info_arr, last_fps_info_arr,
		SENSOR_MAX_NUM, &unstable_fps_bits);
	/* , then update/overwrite its own data */
	fs_alg_sa_setup_dynamic_fps_info_by_dynamic_para(p_para, &fps_info_arr[idx]);

	/* run fps/FL alignment procedure then setup outFL */
	do_skip = do_fps_sync_sa_v2(p_sa_cfg, p_para,
			fps_info_arr, last_fps_info_arr, sync_flk_en);

	return do_skip;
}


/* static */ unsigned int adjust_vsync_diff_sa(
	const unsigned int idx, const unsigned int m_idx,
	struct FrameSyncDynamicPara *p_para,
	struct FrameSyncDynamicPara *p_para_m)
{
	long long adjust_diff = 0;
	unsigned int out_fl_us = 0, flk_diff;

	/* master only do fps sync */
	if (idx == m_idx) {
		/* TODO: if want/need any extra operation on master, do here */
		return 0;
	}

	/* calculate/get suitable slave adjust diff */
	adjust_diff = fs_alg_sa_adjust_slave_diff_resolver(
		m_idx, idx, p_para_m, p_para);

	if (adjust_diff == 0) {
		fs_sa_request_switch_master(idx);

#if !defined(REDUCE_FS_ALGO_LOG)
		LOG_MUST(
			"[%u] ID:%#x(sidx:%u), #%u/#%u(m_idx:%u), request switch to master sensor, out_fl:%u(%u)\n",
			idx,
			fs_inst[idx].sensor_id,
			fs_inst[idx].sensor_idx,
			p_para->magic_num,
			p_para_m->magic_num,
			m_idx,
			fs_inst[idx].output_fl_us,
			convert2LineCount(
				fs_inst[idx].lineTimeInNs,
				fs_inst[idx].output_fl_us));
#endif

		return 0;
	}


	out_fl_us = fs_inst[idx].output_fl_us + adjust_diff;
	g_flk_fl_and_flk_diff(idx, &out_fl_us, &flk_diff, 0);


#if !defined(REDUCE_FS_ALGO_LOG)
	LOG_MUST(
		"[%u] ID:%#x(sidx:%u), #%u/#%u(m_idx:%u), out_fl:%u(%u), flk_en:%u(+%u)\n",
		idx,
		fs_inst[idx].sensor_id,
		fs_inst[idx].sensor_idx,
		p_para->magic_num,
		p_para_m->magic_num,
		m_idx,
		out_fl_us,
		convert2LineCount(
			fs_inst[idx].lineTimeInNs,
			out_fl_us),
		fs_inst[idx].flicker_en,
		flk_diff);
#endif


	fs_alg_sa_update_fl_us(idx, out_fl_us, p_para);
	return 0;
}


/* return: 0 => calculation completed as expected; others => error. */
/* static */ unsigned int adjust_vsync_diff_sa_v2(
	const unsigned int s_idx, const unsigned int m_idx,
	struct FrameSyncDynamicPara *p_para_s,
	struct FrameSyncDynamicPara *p_para_m)
{
	const unsigned int log_str_len = LOG_BUF_STR_LEN;
	const unsigned int is_master = (s_idx == m_idx) ? 1 : 0;
	/* const unsigned int out_fl_us_temp = fs_inst[s_idx].output_fl_us; */
	const unsigned int out_fl_us_temp = p_para_s->out_fl_us;
	long long adjust_diff_m, adjust_diff_s, final_adjust_diff = 0;
	long long ts_diff_m = 0, ts_diff_s = 0;
	/* situation checking variables for adjust diff */
	unsigned int need_chg_master = 0, need_fl_subtract = 0;
	unsigned int adjust_or_not = 1;
	unsigned int flk_diff_final, out_fl_us_final;
	char *log_buf = NULL;
	int len = 0, ret;

	/* !!! flow check (ONLY using in FPS decrease, stable cases) !!! */
	if (unlikely((p_para_s->fps_status_aligned != FS_DY_FPS_DEC)
			&& (p_para_s->fps_status_aligned != FS_DY_FPS_STABLE)))
		return 1;

	/* !!! Calculate slave adjust diff !!! */
	/* ==> calculate/get current receive timestamp diff */
	fs_alg_sa_calc_m_s_ts_diff(p_para_m, p_para_s,
		&ts_diff_m, &ts_diff_s);
	fs_alg_sa_calc_pr_fl_error(m_idx, s_idx, p_para_m, p_para_s);

	/* ==> calculate master/slave adjust_diff */
	adjust_diff_s =
		fs_alg_sa_calc_adjust_diff_slave(
			m_idx, s_idx, ts_diff_m, ts_diff_s,
			p_para_m, p_para_s);
	adjust_diff_m =
		fs_alg_sa_calc_adjust_diff_master(
			m_idx, s_idx, &adjust_diff_s,
			p_para_m, p_para_s);

	if (is_master) {
		/* request to switch as master, if supported, do broadcast */
		need_chg_master = 1;

		adjust_or_not = 1;
		final_adjust_diff = 0;
	} else {
		/* check situation for changing master or not */
		if ((adjust_diff_s > FS_TOLERANCE)
				&& (adjust_diff_m > 0)
				&& (adjust_diff_m < adjust_diff_s)) {
			long long subtracting_diff = 0;

			/* check if FL subtraction is possible to keep sync */
			if (chk_if_subtracting_out_fl_to_keep_sync_is_possible(
					p_para_s, out_fl_us_temp,
					adjust_diff_m, &subtracting_diff)) {
				/* by FL subtraction */
				need_fl_subtract = 1;

				adjust_or_not = 1;
				final_adjust_diff = (0 - subtracting_diff);
			} else {
				/* by switching to master */
				need_chg_master = 1;

				adjust_or_not = 1;
				final_adjust_diff = 0;
			}
		} else if (check_timing_critical_section(adjust_diff_s,
				(p_para_m->stable_fl_us * p_para_m->f_cell))) {
			/* maybe in the timing critical section */
			adjust_or_not = 0;
			final_adjust_diff = 0;
		} else {
			/* common case, e.g., adjust diff S < adjust diff M */
			adjust_or_not = 1;
			final_adjust_diff = adjust_diff_s;
		}
	}

	/* !!! Update slave status for dynamic_para st & adjust diff final !!! */
	p_para_s->is_master = is_master;
	p_para_s->ref_m_idx_magic_num = p_para_m->magic_num;
	p_para_s->ask_for_chg = need_chg_master || (!adjust_or_not);
	p_para_s->chg_master = need_chg_master;
	p_para_s->subtract_fl = need_fl_subtract;
	p_para_s->adj_or_not = adjust_or_not;
	p_para_s->is_correction_suitable = 0;
	p_para_s->corrected_fl_diff = 0;
	p_para_s->adj_diff_m = adjust_diff_m;
	p_para_s->adj_diff_s = adjust_diff_s;
	p_para_s->adj_diff_final = final_adjust_diff;

	p_para_s->ask_for_bcast_re_ctrl_fl =
		((p_para_s->chg_master)
			&& (adjust_diff_m >= FS_BCAST_RE_CTRL_FL_DIFF_TH))
		? 1 : 0;

	/* !!! Calculate final slave FL !!! */
	out_fl_us_final = out_fl_us_temp + p_para_s->adj_diff_final;
	p_para_s->need_auto_restore_fl =
		fs_alg_chk_if_need_to_setup_fl_restore_ctrl(
			s_idx, out_fl_us_final, p_para_s);
	g_flk_fl_and_flk_diff(s_idx, &out_fl_us_final, &flk_diff_final, 0);


	/* !!! for log info !!! */
	ret = alloc_log_buf(log_str_len, &log_buf);
	if (unlikely(ret != 0)) {
		LOG_MUST("ERROR: log_buf allocate memory failed\n");
		goto end_adjust_vsync_diff_sa_v2;
	}

	FS_SNPRF(log_str_len, log_buf, len,
		"[%u][sidx:%u] out:%u(%u) +%lld(%u),flk(%u):+%u, s/m, #%u(%u)/#%u(%u)[%u],r(%d/%d)/f(%u/%u),{M:%u/%lld(%u)}(%lld(%u/chg:%u/-:%u(%u)/%u(%u))/%lld), t(%lld/%lld(+%lld(%#x)))",
		s_idx,
		fs_get_reg_sensor_idx(s_idx),
		out_fl_us_final,
		convert2LineCount(fs_inst[s_idx].lineTimeInNs, out_fl_us_final),
		p_para_s->adj_diff_final,
		p_para_s->need_auto_restore_fl,
		fs_inst[s_idx].flicker_en,
		flk_diff_final,
		p_para_s->magic_num,
		p_para_s->extra_magic_num,
		p_para_m->magic_num,
		p_para_m->extra_magic_num,
		m_idx,
		p_para_s->req_id,
		p_para_m->req_id,
		p_para_s->frame_id,
		p_para_m->frame_id,
		p_para_s->is_master,
		p_para_s->corrected_fl_diff,
		p_para_s->is_correction_suitable,
		p_para_s->adj_diff_s,
		p_para_s->adj_or_not,
		p_para_s->chg_master,
		p_para_s->subtract_fl,
		p_para_s->out_fl_us_min,
		p_para_s->ask_for_chg,
		p_para_s->ask_for_bcast_re_ctrl_fl,
		p_para_s->adj_diff_m,
		ts_diff_s,
		ts_diff_m,
		p_para_s->pred_fl_err_us_m,
		p_para_s->pred_fl_err_chk_bits_m);

	fs_alg_sa_adjust_diff_m_s_general_msg_connector(
		m_idx, s_idx, p_para_m, p_para_s,
		log_str_len, log_buf, len, __func__);

	LOG_MUST_LOCK("%s\n", log_buf);
	FS_TRACE_PR_LOG_MUST("%s", log_buf);
	FS_FREE(log_buf);

end_adjust_vsync_diff_sa_v2:
	/* setup final FL result avoid influence log info */
	fs_alg_sa_update_fl_us(s_idx, out_fl_us_final, p_para_s);

	return 0;
}


/* return: 0 => calculation completed as expected; others => error. */
/* static */ unsigned int adjust_vsync_diff_holder_sa(
	const unsigned int s_idx, const unsigned int m_idx,
	struct FrameSyncDynamicPara *p_para_s,
	struct FrameSyncDynamicPara *p_para_m)
{
	const unsigned int log_str_len = LOG_BUF_STR_LEN;
	const unsigned int is_master = (s_idx == m_idx) ? 1 : 0;
	/* const unsigned int out_fl_us_temp = fs_inst[s_idx].output_fl_us; */
	const unsigned int out_fl_us_temp = p_para_s->out_fl_us;
	long long corrected_fl_diff = 0;
	long long corrected_adj_diff_m, corrected_adj_diff_s;
	long long adjust_diff_m, adjust_diff_s, final_adjust_diff = 0;
	long long ts_diff_m = 0, ts_diff_s = 0;
	/* situation checking variables for adjust diff */
	unsigned int need_chg_master = 0, need_fl_subtract = 0;
	unsigned int is_correction_suitable = 0;
	unsigned int adjust_or_not = 1;
	unsigned int flk_diff_final, out_fl_us_final;
	char *log_buf = NULL;
	int len = 0, ret;

	/* !!! flow check (ONLY using in FPS increase, stable cases) !!! */
	if (unlikely((p_para_s->fps_status_aligned != FS_DY_FPS_INC)
			&& (p_para_s->fps_status_aligned != FS_DY_FPS_STABLE)))
		return 1;

	/* !!! Calculate slave adjust diff !!! */
	/* ==> calculate/get current receive timestamp diff */
	fs_alg_sa_calc_m_s_ts_diff(p_para_m, p_para_s,
		&ts_diff_m, &ts_diff_s);
	fs_alg_sa_calc_pr_fl_error(m_idx, s_idx, p_para_m, p_para_s);

	/* ==> calculate master/slave adjust_diff */
	adjust_diff_s =
		fs_alg_sa_calc_adjust_diff_slave(
			m_idx, s_idx, ts_diff_m, ts_diff_s,
			p_para_m, p_para_s);
	adjust_diff_m =
		fs_alg_sa_calc_adjust_diff_master(
			m_idx, s_idx, &adjust_diff_s,
			p_para_m, p_para_s);

	if (is_master) {
		/* force apply the diff (itself is the master) */
		adjust_or_not = 1;
		/* since itself is the master, choose a smaller one */
		final_adjust_diff = (adjust_diff_s <= adjust_diff_m)
			? adjust_diff_s : adjust_diff_m;
	} else {
		corrected_fl_diff =
			(p_para_m->stable_fl_us > out_fl_us_temp)
			? ((long long)p_para_m->stable_fl_us - out_fl_us_temp)
			: 0;
		/* check if case is suitable doing diff correction */
		if ((corrected_fl_diff != 0)
				&& (adjust_diff_s > corrected_fl_diff)) {
			is_correction_suitable = 1;

			corrected_adj_diff_s = adjust_diff_s - corrected_fl_diff;
			corrected_adj_diff_m = adjust_diff_m + corrected_fl_diff;
		} else {
			is_correction_suitable = 0;

			corrected_adj_diff_s = adjust_diff_s;
			corrected_adj_diff_m = adjust_diff_m;
		}

		/* check situation for changing master or not */
		if ((corrected_adj_diff_s > FS_TOLERANCE)
				&& (corrected_adj_diff_m > 0)
				&& (corrected_adj_diff_m < corrected_adj_diff_s)) {
			long long subtracting_diff = 0;

			/* check if FL subtraction is possible to keep sync */
			if (chk_if_subtracting_out_fl_to_keep_sync_is_possible(
					p_para_s, out_fl_us_temp,
					adjust_diff_m, &subtracting_diff)) {
				/* by FL subtraction */
				need_fl_subtract = 1;

				adjust_or_not = 1;
				final_adjust_diff = (0 - subtracting_diff);
			} else {
				/* by switching to master */
				need_chg_master = 1;

				adjust_or_not = 1;
				final_adjust_diff = 0;
			}
		} else if (check_timing_critical_section(adjust_diff_s,
				(p_para_m->stable_fl_us * p_para_m->f_cell))) {
			/* maybe in the timing critical section */
			adjust_or_not = 0;
			final_adjust_diff = 0;
		} else {
			/* common case, e.g., adjust diff S < adjust diff M */
			adjust_or_not = 1;
			final_adjust_diff = adjust_diff_s;
		}
	}

	/* !!! Update slave status for dynamic_para st & adjust diff final !!! */
	p_para_s->is_master = is_master;
	p_para_s->ref_m_idx_magic_num = p_para_m->magic_num;
	p_para_s->ask_for_chg = 1; /* => MUST ask chg master after calculation */
	p_para_s->chg_master = need_chg_master;
	p_para_s->subtract_fl = need_fl_subtract;
	p_para_s->adj_or_not = adjust_or_not;
	p_para_s->is_correction_suitable = is_correction_suitable;
	p_para_s->corrected_fl_diff = corrected_fl_diff;
	p_para_s->adj_diff_m = adjust_diff_m;
	p_para_s->adj_diff_s = adjust_diff_s;
	p_para_s->adj_diff_final = final_adjust_diff;

	p_para_s->ask_for_bcast_re_ctrl_fl =
		((p_para_s->chg_master)
			&& (corrected_adj_diff_m >= FS_BCAST_RE_CTRL_FL_DIFF_TH))
		? 1 : 0;

	/* !!! Calculate final slave FL !!! */
	out_fl_us_final = out_fl_us_temp + p_para_s->adj_diff_final;
	p_para_s->need_auto_restore_fl =
		fs_alg_chk_if_need_to_setup_fl_restore_ctrl(
			s_idx, out_fl_us_final, p_para_s);
	g_flk_fl_and_flk_diff(s_idx, &out_fl_us_final, &flk_diff_final, 0);


	/* !!! for log info !!! */
	ret = alloc_log_buf(log_str_len, &log_buf);
	if (unlikely(ret != 0)) {
		LOG_MUST("ERROR: log_buf allocate memory failed\n");
		goto end_adjust_vsync_diff_sa_holder;
	}

	FS_SNPRF(log_str_len, log_buf, len,
		"[%u][sidx:%u] out:%u(%u) +%lld(%u),flk(%u):+%u, s/m, #%u(%u)/#%u(%u)[%u],r(%d/%d)/f(%u/%u),{M:%u/%lld(%u)}(%lld(%u/chg:%u/-:%u(%u)/%u(%u))/%lld), t(%lld/%lld(+%lld(%#x)))",
		s_idx,
		fs_get_reg_sensor_idx(s_idx),
		out_fl_us_final,
		convert2LineCount(fs_inst[s_idx].lineTimeInNs, out_fl_us_final),
		p_para_s->adj_diff_final,
		p_para_s->need_auto_restore_fl,
		fs_inst[s_idx].flicker_en,
		flk_diff_final,
		p_para_s->magic_num,
		p_para_s->extra_magic_num,
		p_para_m->magic_num,
		p_para_m->extra_magic_num,
		m_idx,
		p_para_s->req_id,
		p_para_m->req_id,
		p_para_s->frame_id,
		p_para_m->frame_id,
		p_para_s->is_master,
		p_para_s->corrected_fl_diff,
		p_para_s->is_correction_suitable,
		p_para_s->adj_diff_s,
		p_para_s->adj_or_not,
		p_para_s->chg_master,
		p_para_s->subtract_fl,
		p_para_s->out_fl_us_min,
		p_para_s->ask_for_chg,
		p_para_s->ask_for_bcast_re_ctrl_fl,
		p_para_s->adj_diff_m,
		ts_diff_s,
		ts_diff_m,
		p_para_s->pred_fl_err_us_m,
		p_para_s->pred_fl_err_chk_bits_m);

	fs_alg_sa_adjust_diff_m_s_general_msg_connector(
		m_idx, s_idx, p_para_m, p_para_s,
		log_str_len, log_buf, len, __func__);

	LOG_MUST_LOCK("%s\n", log_buf);
	FS_TRACE_PR_LOG_MUST("%s", log_buf);
	FS_FREE(log_buf);

end_adjust_vsync_diff_sa_holder:
	/* setup final FL result avoid influence log info */
	fs_alg_sa_update_fl_us(s_idx, out_fl_us_final, p_para_s);

	return 0;
}


/* return: 0 => calculation completed as expected; others => error. */
static unsigned int adjust_async_vsync_diff_sa(
	const unsigned int idx, const unsigned int m_idx,
	struct FrameSyncDynamicPara *p_para)
{
	struct FrameSyncDynamicPara m_para = {0};
	struct FrameSyncDynamicPara *p_para_m = &m_para;
	const unsigned int log_str_len = LOG_BUF_STR_LEN;
	const unsigned int f_cell_m = get_valid_frame_cell_size(m_idx);
	const unsigned int f_cell = get_valid_frame_cell_size(idx);
	unsigned int flk_diff, flk_diff_final, out_fl_us_final;
	unsigned int adjust_or_not = 1;
	long long ts_diff_m = 0, ts_diff_s = 0, adjust_diff;
	char *log_buf = NULL;
	int len = 0, ret;

	fs_alg_sa_setup_basic_fl_info(idx, p_para, 0, &flk_diff);

	out_fl_us_final = (p_para->min_fl_us / f_cell);
	fs_alg_setup_basic_out_fl(idx, &out_fl_us_final, p_para);

	fs_alg_sa_update_fl_us(idx, out_fl_us_final, p_para);

	if (idx == m_idx)
		return 0;

	/* !!! Check master & slave information !!! */
	if (unlikely(fs_alg_sa_dynamic_params_preparer(idx, m_idx, p_para, p_para_m)))
		return 1;


	/* !!! Calculate slave adjust diff !!! */
	/* ==> calculate/get current receive timestamp diff */
	fs_alg_sa_calc_m_s_ts_diff(p_para_m, p_para,
		&ts_diff_m, &ts_diff_s);
	fs_alg_sa_calc_pr_fl_error(m_idx, idx, p_para_m, p_para);

	/* ==> calculate master/slave adjust_diff */
	adjust_diff =
		fs_alg_sa_calc_adjust_diff_async(
			m_idx, idx, ts_diff_m, ts_diff_s,
			p_para_m, p_para);

	/* ==> check case for adjust_diff */
	if (check_timing_critical_section(
		adjust_diff, (p_para_m->stable_fl_us * f_cell_m))) {
#ifndef REDUCE_FS_ALGO_LOG
		LOG_MUST(
			"NOTICE: [%u] ID:%#x(sidx:%u), #%u/#%u(m_idx:%u), req_id:%d/%d, chk timing critical section, set adjust_vsync_diff:0 (s(adjust_diff:%lld), m(s:%u, f_cell:%u)),\n",
			idx,
			fs_get_reg_sensor_id(idx),
			fs_get_reg_sensor_idx(idx),
			p_para->magic_num,
			p_para_m->magic_num,
			m_idx,
			p_para->req_id,
			p_para_m->req_id,
			adjust_diff,
			p_para_m->stable_fl_us,
			f_cell_m);
#endif
		adjust_or_not = 0;
	}


	/* !!! Update slave status for dynamic_para st & adjust diff final !!! */
	p_para->master_idx = m_idx; // async mode => overwrite to async master
	p_para->ref_m_idx_magic_num = p_para_m->magic_num;
	p_para->adj_diff_m = 0;
	p_para->adj_diff_s = adjust_diff;
	p_para->adj_or_not = adjust_or_not;
	p_para->chg_master = 0;
	p_para->ask_for_chg = 0;
	p_para->adj_diff_final = (!adjust_or_not) ? 0 : adjust_diff;


	/* !!! Calculate final slave FL !!! */
	out_fl_us_final = fs_inst[idx].output_fl_us + p_para->adj_diff_final;
	p_para->need_auto_restore_fl =
		fs_alg_chk_if_need_to_setup_fl_restore_ctrl(
			idx, out_fl_us_final, p_para);
	g_flk_fl_and_flk_diff(idx, &out_fl_us_final, &flk_diff_final, 0);


	/* !!! for log info !!! */
	ret = alloc_log_buf(log_str_len, &log_buf);
	if (unlikely(ret != 0)) {
		LOG_MUST("ERROR: log_buf allocate memory failed\n");
		goto end_adjust_async_vsync_diff_sa;
	}

	FS_SNPRF(log_str_len, log_buf, len,
		"[%u] ID:%#x(sidx:%u), out_fl:%u(%u) +%lld(%u), flk(%u):(+%u/+%u), s/m, #%u/#%u([%u]), req_id(%d/%d), adj_diff(%lld(%u)), async_m_delta:%u, t:(%lld/%lld(+%lld(%#x)))",
		idx,
		fs_get_reg_sensor_id(idx),
		fs_get_reg_sensor_idx(idx),
		out_fl_us_final,
		convert2LineCount(fs_inst[idx].lineTimeInNs, out_fl_us_final),
		p_para->adj_diff_final,
		p_para->need_auto_restore_fl,
		fs_inst[idx].flicker_en,
		flk_diff,
		flk_diff_final,
		p_para->magic_num,
		p_para_m->magic_num,
		m_idx,
		p_para->req_id,
		p_para_m->req_id,
		adjust_diff,
		adjust_or_not,
		p_para->async_m_delta,
		ts_diff_s,
		ts_diff_m,
		p_para->pred_fl_err_us_m,
		p_para->pred_fl_err_chk_bits_m);

	fs_alg_sa_adjust_diff_m_s_general_msg_connector(
		m_idx, idx, p_para_m, p_para,
		log_str_len, log_buf, len, __func__);

	LOG_MUST_LOCK("%s\n", log_buf);
	FS_TRACE_PR_LOG_MUST("%s", log_buf);

	FS_FREE(log_buf);

end_adjust_async_vsync_diff_sa:
	/* setup final FL result avoid influence log info */
	fs_alg_sa_update_fl_us(idx, out_fl_us_final, p_para);
	return 0;
}


/* return => 0: without error / non-0: error is detected */
static unsigned int vsync_diff_sa_handler(const struct fs_sa_cfg *p_sa_cfg,
	struct FrameSyncDynamicPara *p_para,
	const unsigned int skip_adjust_vsync_diff)
{
	const unsigned int m_idx = p_sa_cfg->m_idx;
	const unsigned int idx = p_sa_cfg->idx;
	struct FrameSyncDynamicPara m_para = {0};
	struct FrameSyncDynamicPara *p_para_m = &m_para;
	unsigned int ret = 0;

	/* !!! Check master & slave information !!! */
	if (unlikely(fs_alg_sa_dynamic_params_preparer(idx, m_idx, p_para, p_para_m)))
		return 1;

#ifndef FS_VSYNC_DIFF_SOLVER_V2
	/* original */
	if (likely(!skip_adjust_vsync_diff))
		ret = adjust_vsync_diff_sa(idx, m_idx, p_para, p_para_m);
#else
	/* new support */
	switch (p_para->fps_status_aligned) {
	case FS_DY_FPS_STABLE:
		if (idx != m_idx)
			ret = adjust_vsync_diff_sa_v2(
					idx, m_idx, p_para, p_para_m);
		else {
			if (p_para->ts_bias_us <= p_para_m->ts_bias_us) {
				/* => sync point is advanced, e.g., shutter time reduce */
				ret = adjust_vsync_diff_holder_sa(
					idx, m_idx, p_para, p_para_m);
			} else {
				/* => sync point is move back, e.g., shutter time larger */
				ret = adjust_vsync_diff_sa_v2(
					idx, m_idx, p_para, p_para_m);
			}
		}
		break;
	case FS_DY_FPS_INC:
		ret = adjust_vsync_diff_holder_sa(idx, m_idx, p_para, p_para_m);
		/* TODO: add function to update other sensors' stable FL (in sync mode) */
		break;
	case FS_DY_FPS_DEC:
		ret = adjust_vsync_diff_sa_v2(idx, m_idx, p_para, p_para_m);
		break;
	case FS_DY_FPS_DEC_MOST:
		/* MUST change master to itself, then if supported, do broadcast */
		p_para->ask_for_chg = 1;
		p_para->ask_for_bcast_re_ctrl_fl = 1;
		break;
	case FS_DY_FPS_USER_CHG:
		/* MUST change master to itself, then if supported, do broadcast */
		p_para->ask_for_chg = 1;
		/* TODO: add some dynamic log for checking data */
		break;
	case FS_DY_FPS_SINGLE_CAM_SKIP:
		/* in single cam situation ==> NO NEED to do anything */
		break;
	}

	/* check if ask need to change/switch master */
	if (p_para->ask_for_chg) {
		fs_sa_request_switch_master(idx);
		/* checking for if needed to broadcast */
		if (unlikely(p_para->ask_for_bcast_re_ctrl_fl)) {
			const unsigned int valid_mask =
				(p_sa_cfg->valid_sync_bits ^ p_sa_cfg->async_s_bits);
			struct fs_event_exe_bcast_req_info info = {0};

			/* setup caller info */
			info.idx = idx;
			info.recv_en_bits = (valid_mask ^ (1UL << idx));
			info.magic_num = p_para->magic_num;
			info.req_id = p_para->req_id;
			info.frame_id = p_para->frame_id;

			fs_request_bcast_for_re_ctrl_fl(
				idx, p_para->frame_id, &info);
		}
	}
#endif

	return ret;
}


/*
 * Every sensor will call into this function
 *
 * return: (0/1) for (no error/some error happened)
 *
 * input:
 *     struct fs_sa_cfg
 *         idx: standalone instance idx
 *         m_idx: master instance idx
 *         valid_sync_bits: all valid for doing frame-sync instance idxs
 *         sa_method: 0 => adaptive switch master; 1 => fix master
 *
 * output:
 *     *fl_lc: pointer for output frame length
 */
unsigned int fs_alg_solve_frame_length_sa(
	const struct fs_sa_cfg *p_sa_cfg, unsigned int *fl_lc)
{
	const unsigned int idx = p_sa_cfg->idx;
	struct FrameSyncDynamicPara para = {0};
	unsigned int ret = 0;

	/* prepare new dynamic para */
	fs_alg_sa_init_new_ctrl(p_sa_cfg, &para);

	/* check ts info status */
	if (unlikely(fs_inst[idx].is_nonvalid_ts)) {
		/* for set shutter with frame length API, */
		/*     give a min FL for sensor driver auto judgment */
		*fl_lc = fs_inst[idx].min_fl_lc;
		LOG_INF(
			"ERROR: [%u][sidx:%u], #%u(%u), req:%d/f:%u, ts data is non-valid, skip FL ctrl => assign fl to minFL:%u\n",
			idx,
			fs_get_reg_sensor_idx(idx),
			para.magic_num,
			para.extra_magic_num,
			para.req_id,
			para.frame_id,
			*fl_lc);
		return ret;
	}

	/* copy/get last timestamp info */
	fs_alg_sa_get_last_vts_info(idx, &para);

	/* check this idx is normal sync or async slave idx */
	if (!((p_sa_cfg->async_s_bits >> idx) & 0x01)) {
		unsigned int skip_adjust_vsync_diff;

		skip_adjust_vsync_diff =
			fps_sync_sa_handler_v2(p_sa_cfg, &para);
		ret = vsync_diff_sa_handler(p_sa_cfg, &para,
			skip_adjust_vsync_diff);

	} else {
		ret = adjust_async_vsync_diff_sa(
			idx, p_sa_cfg->async_m_idx, &para);
	}

	/* copy fl result out */
	*fl_lc = fs_inst[idx].output_fl_lc;

	/* update dynamic para for sharing to other sensor */
	fs_alg_sa_update_dynamic_para(idx, &para);

	return ret;
}
#endif // SUPPORT_FS_NEW_METHOD


/******************************************************************************/
