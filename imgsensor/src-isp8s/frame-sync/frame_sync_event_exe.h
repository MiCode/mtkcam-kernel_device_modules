/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_EVENT_EXE_H
#define _FRAME_SYNC_EVENT_EXE_H


/*******************************************************************************
 * event-execute operation functions
 ******************************************************************************/

/*============================================================================*/
/* for event-execute general framework */
/*============================================================================*/
void fs_event_exe_init(void);

void fs_event_exe_cb_info_clear(const unsigned int idx);
void fs_event_exe_cb_info_init(const unsigned int idx, void *p_ctx,
	const cb_func_event_execute_bcast bcast_func_ptr);

/* to broadcasting ctrls */
void fs_event_exe_bcast_ctrls_notify_vsync_idx(const unsigned int idx);
void fs_event_exe_bcast_ctrls_clear_idx(const unsigned int idx);
void fs_event_exe_bcast_ctrls_init_idx(const unsigned int idx);
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
	struct fs_event_exe_bcast_req_info *info);

/* return: 0 => no error; non-0 => some error */
unsigned int fs_event_exe_bcast_request_re_ctrl_fl(const unsigned int idx,
	const unsigned int magic_key,
	const struct fs_event_exe_bcast_req_info *info);
/*----------------------------------------------------------------------------*/

/*============================================================================*/

/******************************************************************************/
#endif
