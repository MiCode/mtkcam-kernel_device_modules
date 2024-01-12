// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Daniel Huang <daniel.huang@mediatek.com>
 *
 */
#include "mtk_imgsys_cmdq_token.h"
/* TODO: include frm_sync_token header */
#define HEADER_NOT_READY (1)
#ifdef HEADER_NOT_READY
struct imgsys_control_meta_token_kernel {
	uint32_t mSyncTokenNotifyList[16];
	uint32_t mSyncTokenWaitList[16];
	uint32_t mSyncTokenWaitNoClearList[16];
};

struct dpe_in_data {
	uint64_t frm_owner;
	uint64_t imgstm_inst;
	uint32_t req_fd;
	uint32_t req_no;
	uint32_t frm_no;
	int sw_ridx;
	struct imgsys_control_meta_token_kernel token_info;
};

struct dpe_out_data {
 	uint32_t *event_id;
};

int handler_frame_token_sync_DPE(struct dpe_in_data *in_data, struct dpe_out_data *out_data)
{
	return 0;
}

#endif

/* TODO */
unsigned int imgsys_cmdq_is_vsdof_event(uint32_t event)
{
	unsigned int ret = 0;


	return ret;
}

unsigned int imgsys_cmdq_try_vsdof_wfe(struct token_data *data, struct cmdq_pkt *pkt,
										uint32_t event)
{
	unsigned int ret = 1;
	struct dpe_in_data in = {0};
	struct dpe_out_data out = {0};


	if (imgsys_cmdq_is_vsdof_event(event)) {
		in.frm_owner = data->frm_owner;
		in.req_fd = data->req_fd;
		in.req_no = data->req_no;
		in.sw_ridx = data->sw_ridx;

		in.token_info.mSyncTokenWaitList[0] = event;
		handler_frame_token_sync_DPE(&in, &out);
		cmdq_pkt_wfe(pkt, *out.event_id);
		ret = 0;
	}

	return ret;
}

unsigned int imgsys_cmdq_try_vsdof_wfe_no_clear(struct token_data *data, struct cmdq_pkt *pkt,
										uint32_t event)
{
	unsigned int ret = 1;
	struct dpe_in_data in = {0};
	struct dpe_out_data out = {0};

	if (imgsys_cmdq_is_vsdof_event(event)) {
		in.frm_owner = data->frm_owner;
		in.req_fd = data->req_fd;
		in.req_no = data->req_no;
		in.sw_ridx = data->sw_ridx;

		in.token_info.mSyncTokenWaitNoClearList[0] = event;

		handler_frame_token_sync_DPE(&in, &out);
		cmdq_pkt_wait_no_clear(pkt, *out.event_id);
		ret = 0;
	}

	return ret;
}

unsigned int imgsys_cmdq_try_vsdof_set_event(struct token_data *data, struct cmdq_pkt *pkt,
										uint32_t event)

{
	unsigned int ret = 1;
	struct dpe_in_data in = {0};
	struct dpe_out_data out = {0};

	if (imgsys_cmdq_is_vsdof_event(event)) {

		in.frm_owner = data->frm_owner;
		in.req_fd = data->req_fd;
		in.req_no = data->req_no;
		in.sw_ridx = data->sw_ridx;
		in.token_info.mSyncTokenNotifyList[0] = event;

		handler_frame_token_sync_DPE(&in, &out);
		cmdq_pkt_set_event(pkt, *out.event_id);
		ret = 0;
	}

	return ret;
}

unsigned int imgsys_cmdq_try_vsdof_clear_event(struct token_data *data, struct cmdq_pkt *pkt,
										uint32_t event)

{
	unsigned int ret = 1;
	struct dpe_in_data in = {0};
	struct dpe_out_data out = {0};

	if (imgsys_cmdq_is_vsdof_event(event)) {

		in.frm_owner = data->frm_owner;
		in.req_fd = data->req_fd;
		in.req_no = data->req_no;
		in.sw_ridx = data->sw_ridx;
		in.token_info.mSyncTokenNotifyList[0] = event;

		handler_frame_token_sync_DPE(&in, &out);
		cmdq_pkt_set_event(pkt, *out.event_id);
		ret = 0;
	}

	return ret;
}


unsigned int imgsys_cmdq_release_token_vsdof(int sw_ridx)
{
/* TODO */
	unsigned int ret = 0;
//	sw_ridx;
//	release_frame_token_dpe();
	return ret;
}
