// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2024 MediaTek Inc.

#include "mtk_cam-job_state_impl.h"

static struct state_transition STATE_TRANS(virt_isp, S_ISP_NOT_SET)[] = {
	{
		S_ISP_COMPOSING, CAMSYS_EVENT_ENQUE,
		guard_next_compose, ACTION_COMPOSE_CQ
	},
};

static struct state_transition STATE_TRANS(virt_isp, S_ISP_COMPOSING)[] = {
	{
		S_ISP_COMPOSED, CAMSYS_EVENT_ACK,
		NULL, 0
	},
};

static struct state_transition STATE_TRANS(virt_isp, S_ISP_COMPOSED)[] = {
	{
		S_ISP_DONE, CAMSYS_EVENT_IRQ_FRAME_DONE,
		NULL, 0
	},
};

static struct transitions_entry virt_isp_isp_entries[NR_S_ISP_STATE] = {
	ADD_TRANS_ENTRY(virt_isp, S_ISP_NOT_SET),
	ADD_TRANS_ENTRY(virt_isp, S_ISP_COMPOSING),
	ADD_TRANS_ENTRY(virt_isp, S_ISP_COMPOSED),
};

struct state_table virt_isp_isp_tbl = {
	.entries = virt_isp_isp_entries,
	.size = ARRAY_SIZE(virt_isp_isp_entries),
};

static struct state_transition STATE_TRANS(virt_isp_sensor, S_SENSOR_NOT_SET)[] = {
	{
		S_SENSOR_LATCHED, CAMSYS_EVENT_ENQUE,
		NULL, ACTION_APPLY_SENSOR
	},
};

static struct transitions_entry virt_isp_sensor_entries[NR_S_SENSOR_STATE] = {
	ADD_TRANS_ENTRY(virt_isp_sensor, S_SENSOR_NOT_SET),
};

struct state_table virt_isp_sensor_tbl = {
	.entries = virt_isp_sensor_entries,
	.size = ARRAY_SIZE(virt_isp_sensor_entries),
};

static bool state_acc_allow(struct state_accessor *s)
{
	(void)s;
	return true;
}

static int applicable_allow(struct mtk_cam_job_state *s)
{
	(void)s;
	return 1;
}

static const struct state_accessor_ops _acc_ops = {
	.prev_allow_apply_sensor = state_acc_allow,
	.prev_allow_apply_isp = state_acc_allow,
	.is_next_sensor_applied = state_acc_allow,
	.cur_sensor_state = sf_cur_isp_state,
	.cur_isp_state = sf_cur_isp_state,
};

static int virt_isp_send_event(struct mtk_cam_job_state *s,
			    struct transition_param *p)
{
	struct state_accessor s_acc;

	s_acc.head = p->head;
	s_acc.s = s;
	s_acc.seq_no = s->seq_no;
	s_acc.ops = &_acc_ops;
	p->s_params = &s->s_params;
	p->cq_trigger_thres = s->cq_trigger_thres_ns;
	p->reference_sof_ns = s->reference_sof_ns;

	// TODO: SENSOR table
	loop_each_transition(&virt_isp_sensor_tbl, &s_acc, SENSOR_STATE, p);
	loop_each_transition(&virt_isp_isp_tbl, &s_acc, ISP_STATE, p);

	return 0;
}

static const struct mtk_cam_job_state_ops virt_isp_state_ops = {
	.send_event = virt_isp_send_event,
	.is_next_sensor_applicable = applicable_allow,
	.is_next_isp_applicable = applicable_allow,
	.is_sensor_applied = applicable_allow,
};

int mtk_cam_job_state_init_virt_isp(struct mtk_cam_job_state *s,
				 const struct mtk_cam_job_state_cb *cb,
				 int with_sensor_ctrl)
{
	s->ops = &virt_isp_state_ops;

	mtk_cam_job_state_set(s, SENSOR_STATE,
			      with_sensor_ctrl ?
			      S_SENSOR_NOT_SET : S_SENSOR_NONE);
	mtk_cam_job_state_set(s, ISP_STATE, S_ISP_NOT_SET);

	atomic_set(&s->todo_action, 0);
	s->cb = cb;
	s->apply_by_fsm = 1;
	s->compose_by_fsm = 1;
	s->bypass_by_aewa = 0;

	return 0;
}
