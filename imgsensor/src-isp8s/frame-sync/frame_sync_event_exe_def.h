/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_EVENT_EXE_DEF_H
#define _FRAME_SYNC_EVENT_EXE_DEF_H


/*******************************************************************************
 * evnet-execute define/struct/enum used in Frame-Sync and adaptor-fsync-ctrls.
 ******************************************************************************/
#define FS_EVENT_EXE_INVALID_FRAME_ID (-1)


/* for broadcasting */
typedef int (*cb_func_event_execute_bcast)(void *p_ctx, void *p_data,
	const unsigned int event_tag);

enum fsync_ctrl_event_bcast_id {
	FSYNC_CTRL_EVENT_BCAST_NONE = 0,
	FSYNC_CTRL_EVENT_BCAST_RE_CTRL_FL,   /* 1, trigger FL re-calc, re-ctrl */
};


/*******************************************************************************
 * frame-sync internal used define / enum / macro
 ******************************************************************************/
/* for event-execute error type */
enum fs_event_exe_error_type {
	FS_EVENT_EXE_ERROR_TYPE_NONE = 0, /* no error */
	FS_EVENT_EXE_ERROR_TYPE_INVALID_CB_INFO,
	FS_EVENT_EXE_ERROR_TYPE_NOT_EXPECTING_TO_RECV_BCAST,
	FS_EVENT_EXE_ERROR_TYPE_MAGIC_KEY_INVALID,
	FS_EVENT_EXE_ERROR_TYPE_MAGIC_KEY_NOT_MATCH,
};


/* for broadcasting */
struct fs_event_exe_bcast_req_info {
	unsigned int idx; /* request from which frame-sync idx */
	unsigned int recv_en_bits; /* which one can be triggered */

	/* frame info */
	unsigned int magic_num;    /* fs set shutter trigger, magic num */
	unsigned int frame_id;     /* MW pipeline frame_id */
	int req_id;                /* MW req_id */

	/* for debugging */
	unsigned int curr_recv_en_bits;
	unsigned int curr_sent_bits;
};
/******************************************************************************/


#endif
