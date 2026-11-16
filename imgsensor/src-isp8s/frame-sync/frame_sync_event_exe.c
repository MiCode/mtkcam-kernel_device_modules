// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#include "frame_sync_event_exe_def.h"
#include "frame_sync_event_exe.h"
#include "frame_sync_def.h"
#include "frame_sync_util.h"
#include "frame_sync.h"


/*******************************************************************************
 * for Log message
 ******************************************************************************/
#include "frame_sync_log.h"

#define REDUCE_FS_BC_LOG
#define PFX "FrameSyncEventExe"
#define FS_LOG_DBG_DEF_CAT LOG_FS_EVENT_EXE
/******************************************************************************/


/*******************************************************************************
 * Lock
 ******************************************************************************/
#ifndef FS_UT
#include <linux/spinlock.h>
static DEFINE_SPINLOCK(fs_event_exe_cb_info_op_lock);
#endif
/******************************************************************************/





/*******************************************************************************
 * event-execute static structure / enum
 ******************************************************************************/
struct cb_fsync_ctrl_info {
	unsigned int is_valid;
	void *p_ctx;
	cb_func_event_execute_bcast bcast_func_ptr;
};


/*----------------------------------------------------------------------------*/
/* for broadcasting, general ctrl structure */
/*----------------------------------------------------------------------------*/
struct fs_event_exe_bcast_ctrl {
	unsigned int user_bits; /* to know which idx will use this ctrl */

	/* which one can receive and try to trigger broadcast */
	/* ==> clear when: vsync(pre-latch) / recv bcast */
	unsigned int recv_en_bits;
	/* which one request to send broadcast */
	/* ==> clear when: vsync(pre-latch) */
	unsigned int sent_bits;

	/* check the key if valid for triggering broadcasting */
	unsigned int magic_key;

	/* broadcast requester/caller latest info */
	struct fs_event_exe_bcast_req_info latest_caller_info;

#ifndef FS_UT
	spinlock_t op_spinlock;
#endif
};


/*----------------------------------------------------------------------------*/
/* event-execute mgr */
/*----------------------------------------------------------------------------*/
struct fs_event_execute_mgr {
	/* event-execute mgr */
	FS_Atomic_T validSync_bits;

	/* cb function info (w/ a spinlock) */
	struct cb_fsync_ctrl_info cb_info[SENSOR_MAX_NUM];

	/* broadcast ctrl */
	struct fs_event_exe_bcast_ctrl bcast_re_ctrl_fl;
};
static struct fs_event_execute_mgr fs_event_exe_mgr;
/******************************************************************************/





/*******************************************************************************
 * event-execute static functions
 ******************************************************************************/

/*----------------------------------------------------------------------------*/
/* for debugging dump */
/*----------------------------------------------------------------------------*/
static void fs_event_exe_dbg_dump_bcast_ctrl_info(const unsigned int idx,
	const struct fs_event_exe_bcast_ctrl *p_ctrl,
	const char *caller)
{
	LOG_MUST(
		"[%s] NOTICE: [%u]sidx:%u, recv_en:%#x/sent:%#x, magic_key:%u, info:(idx:%u/recv_en:%#x/#%u/req:%d/f:%u)\n",
		caller,
		idx,
		fs_get_reg_sensor_idx(idx),
		p_ctrl->recv_en_bits,
		p_ctrl->sent_bits,
		p_ctrl->magic_key,
		p_ctrl->latest_caller_info.idx,
		p_ctrl->latest_caller_info.recv_en_bits,
		p_ctrl->latest_caller_info.magic_num,
		p_ctrl->latest_caller_info.req_id,
		p_ctrl->latest_caller_info.frame_id);
}

/*----------------------------------------------------------------------------*/
/* for broadcasting ctrl, general ctrl handling function */
/*----------------------------------------------------------------------------*/
static inline unsigned int chk_status_for_bcast_ctrl(const unsigned int idx,
	const unsigned int magic_key,
	const struct fs_event_exe_bcast_ctrl *p_ctrl)
{
	if (((p_ctrl->recv_en_bits >> idx) & 0x01) == 0)
		return FS_EVENT_EXE_ERROR_TYPE_NOT_EXPECTING_TO_RECV_BCAST;
	if (unlikely(magic_key == FS_EVENT_EXE_INVALID_FRAME_ID))
		return FS_EVENT_EXE_ERROR_TYPE_MAGIC_KEY_INVALID;
	if (unlikely(magic_key != p_ctrl->magic_key))
		return FS_EVENT_EXE_ERROR_TYPE_MAGIC_KEY_NOT_MATCH;
	return 0;
}


static inline void init_bcast_ctrl(struct fs_event_exe_bcast_ctrl *p_ctrl)
{
	/* !!! init all members in the ctrl structure !!! */
	/* ==> all clear */
	memset(p_ctrl, 0, sizeof(*p_ctrl));

	/* ==> init to a specific value */
	p_ctrl->magic_key = (unsigned int)(FS_EVENT_EXE_INVALID_FRAME_ID);

	fs_spin_init(&p_ctrl->op_spinlock);
}


static inline void notify_vsync_bcast_ctrl_idx(const unsigned int idx,
	struct fs_event_exe_bcast_ctrl *p_ctrl)
{
	/* !!! reset/clear SOME member to the specific idx !!! */

	fs_spin_lock(&p_ctrl->op_spinlock);

	p_ctrl->sent_bits &= (~(1UL << idx));

	fs_spin_unlock(&p_ctrl->op_spinlock);
}


static void clear_bcast_ctrl_idx(const unsigned int idx,
	struct fs_event_exe_bcast_ctrl *p_ctrl)
{
	/* !!! reset/clear members to the specific idx !!! */

	fs_spin_lock(&p_ctrl->op_spinlock);

	p_ctrl->recv_en_bits &= (~(1UL << idx));
	p_ctrl->sent_bits &= (~(1UL << idx));
	p_ctrl->user_bits &= (~(1UL << idx));

	/* check if do NOT have other user */
	if (FS_POPCOUNT(p_ctrl->user_bits) == 0) {
		p_ctrl->magic_key =
			(unsigned int)(FS_EVENT_EXE_INVALID_FRAME_ID);

		memset(&p_ctrl->latest_caller_info,
			0, sizeof(p_ctrl->latest_caller_info));
	}

	fs_spin_unlock(&p_ctrl->op_spinlock);
}


static void init_bcast_ctrl_idx(const unsigned int idx,
	struct fs_event_exe_bcast_ctrl *p_ctrl)
{
	/* !!! init members to the specific idx !!! */

	fs_spin_lock(&p_ctrl->op_spinlock);

	/* check if do NOT have other user */
	if (FS_POPCOUNT(p_ctrl->user_bits) == 0) {
		p_ctrl->magic_key =
			(unsigned int)(FS_EVENT_EXE_INVALID_FRAME_ID);

		memset(&p_ctrl->latest_caller_info,
			0, sizeof(p_ctrl->latest_caller_info));
	}

	p_ctrl->recv_en_bits &= (~(1UL << idx));
	p_ctrl->sent_bits &= (~(1UL << idx));
	p_ctrl->user_bits |= (1UL << idx);

	fs_spin_unlock(&p_ctrl->op_spinlock);
}


/*----------------------------------------------------------------------------*/
/* for event-execute framework */
/*----------------------------------------------------------------------------*/

/* before using, must use "g fsync ctrl cb info" to get and check cb info situation */
static inline int cb_fsync_ctrl_execute_bcast(
	const struct cb_fsync_ctrl_info *cb_info, const unsigned int event_tag,
	const char *caller)
{
	if (unlikely(cb_info->bcast_func_ptr == NULL)) {
		LOG_MUST(
			"[%s] ERROR: invalid braodcast function ptr %p   [event_tag:%u, info(is_valid:%u)]\n",
			caller,
			cb_info->bcast_func_ptr,
			event_tag,
			cb_info->is_valid);
		return 0;
	}
	return cb_info->bcast_func_ptr(cb_info->p_ctx, NULL, event_tag);
}


/* return: 0 => invalid to use; non-0 => valid to use */
static unsigned int g_fsync_ctrl_cb_info(const unsigned int idx,
	struct cb_fsync_ctrl_info *cb_info)
{
	const struct cb_fsync_ctrl_info *info = &fs_event_exe_mgr.cb_info[idx];
	unsigned int ret;

	fs_spin_lock(&fs_event_exe_cb_info_op_lock);

	ret = info->is_valid;
	if (likely(ret))
		*cb_info = *info;

	fs_spin_unlock(&fs_event_exe_cb_info_op_lock);

	return ret;
}


static inline void event_exe_init_bcast_ctrls(void)
{
	/* init all broadcast ctrl */
	init_bcast_ctrl(&fs_event_exe_mgr.bcast_re_ctrl_fl);
}
/******************************************************************************/





/*******************************************************************************
 * event-execute operation functions
 ******************************************************************************/

/*============================================================================*/
/* for event-execute general framework */
/*============================================================================*/
void fs_event_exe_init(void)
{
	unsigned int i;

	/* init all member */
	FS_ATOMIC_INIT(0, &fs_event_exe_mgr.validSync_bits);

	for (i = 0; i < SENSOR_MAX_NUM; ++i) {
		memset(&fs_event_exe_mgr.cb_info[i],
			0, sizeof(fs_event_exe_mgr.cb_info[i]));
	}

	event_exe_init_bcast_ctrls();
}


void fs_event_exe_cb_info_clear(const unsigned int idx)
{
	struct cb_fsync_ctrl_info *p_info = &fs_event_exe_mgr.cb_info[idx];

	fs_spin_lock(&fs_event_exe_cb_info_op_lock);

	memset(p_info, 0, sizeof(*p_info));

	fs_spin_unlock(&fs_event_exe_cb_info_op_lock);
}


void fs_event_exe_cb_info_init(const unsigned int idx, void *p_ctx,
	const cb_func_event_execute_bcast bcast_func_ptr)
{
	struct cb_fsync_ctrl_info *p_info = &fs_event_exe_mgr.cb_info[idx];

	fs_spin_lock(&fs_event_exe_cb_info_op_lock);

	p_info->p_ctx = p_ctx;
	p_info->bcast_func_ptr = bcast_func_ptr;
	p_info->is_valid = 1;

	fs_spin_unlock(&fs_event_exe_cb_info_op_lock);
}


void fs_event_exe_bcast_ctrls_notify_vsync_idx(const unsigned int idx)
{
	/* broadcast ctrls that want to be notified */
	notify_vsync_bcast_ctrl_idx(idx, &fs_event_exe_mgr.bcast_re_ctrl_fl);
}


void fs_event_exe_bcast_ctrls_clear_idx(const unsigned int idx)
{
	/* broadcast ctrls that want to be clear */
	clear_bcast_ctrl_idx(idx, &fs_event_exe_mgr.bcast_re_ctrl_fl);
}


void fs_event_exe_bcast_ctrls_init_idx(const unsigned int idx)
{
	/* broadcast ctrls that want to be init */
	init_bcast_ctrl_idx(idx, &fs_event_exe_mgr.bcast_re_ctrl_fl);
}
/*============================================================================*/


/*============================================================================*/
/* for broadcasting ctrls */
/*============================================================================*/

/*----------------------------------------------------------------------------*/
/* for broadcasting ctrls: re-CTRL FL */
/*----------------------------------------------------------------------------*/
/* return: 0 => no error; non-0 => some error */
unsigned int fs_event_exe_bcast_chk_for_re_ctrl_fl(const unsigned int idx,
	const unsigned int magic_key,
	struct fs_event_exe_bcast_req_info *info)
{
	struct fs_event_exe_bcast_ctrl *p_ctrl =
		&fs_event_exe_mgr.bcast_re_ctrl_fl;
	unsigned int ret = 0;

	fs_spin_lock(&p_ctrl->op_spinlock);

	if (likely(info != NULL)) {
		memcpy(info, &p_ctrl->latest_caller_info, sizeof(*info));
		info->curr_recv_en_bits = p_ctrl->recv_en_bits;
		info->curr_sent_bits = p_ctrl->sent_bits;
	}

	/* NOT expecting to receive broadcasting ctrl */
	ret = chk_status_for_bcast_ctrl(idx, magic_key, p_ctrl);
	if (unlikely(ret != 0))
		goto end_fs_event_exe_bcast_chk_valid_for_re_ctrl_fl;

	/* !!!---> valid for doing <---!!! */
	p_ctrl->recv_en_bits &= (~(1UL << idx));

end_fs_event_exe_bcast_chk_valid_for_re_ctrl_fl:

	fs_spin_unlock(&p_ctrl->op_spinlock);

	return ret;
}


/* return: 0 => no error; non-0 => some error */
unsigned int fs_event_exe_bcast_request_re_ctrl_fl(const unsigned int idx,
	const unsigned int magic_key,
	const struct fs_event_exe_bcast_req_info *info)
{
	const unsigned int event_tag = FSYNC_CTRL_EVENT_BCAST_RE_CTRL_FL;
	struct cb_fsync_ctrl_info cb_info = {0};
	struct fs_event_exe_bcast_ctrl *p_ctrl =
		&fs_event_exe_mgr.bcast_re_ctrl_fl;
	unsigned int is_valid = 1;

	if (unlikely(g_fsync_ctrl_cb_info(idx, &cb_info) == 0))
		return FS_EVENT_EXE_ERROR_TYPE_INVALID_CB_INFO;

	fs_spin_lock(&p_ctrl->op_spinlock);

	/* for preventing re-send broadcasting event */
	if (unlikely((p_ctrl->sent_bits >> idx) & 0x01)) {
		is_valid = 0;
		LOG_MUST(
			"NOTICE: [%u]sidx:%u, already sent event before, sent_bits:%#x/recv_en:%#x   [req_info(idx:%u/recv_en:%#x/#%u/req:%d/f:%u)]\n",
			idx,
			fs_get_reg_sensor_idx(idx),
			p_ctrl->sent_bits,
			p_ctrl->recv_en_bits,
			info->idx,
			info->recv_en_bits,
			info->magic_num,
			info->req_id,
			info->frame_id);
		fs_event_exe_dbg_dump_bcast_ctrl_info(idx, p_ctrl, __func__);
		goto end_fs_event_exe_bcast_request_re_ctrl_fl;
	}

	/* update ctrl info */
	p_ctrl->magic_key = magic_key;
	p_ctrl->recv_en_bits = info->recv_en_bits;
	p_ctrl->sent_bits |= (1UL << idx);
	memcpy(&p_ctrl->latest_caller_info, info, sizeof(*info));

end_fs_event_exe_bcast_request_re_ctrl_fl:

	fs_spin_unlock(&p_ctrl->op_spinlock);

	if (likely(is_valid))
		cb_fsync_ctrl_execute_bcast(&cb_info, event_tag, __func__);

	return 0;
}
/*----------------------------------------------------------------------------*/

/*============================================================================*/

/******************************************************************************/
