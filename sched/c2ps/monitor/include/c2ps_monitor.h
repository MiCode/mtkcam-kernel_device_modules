/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef _C2PS_MONITOR_C2PS_MONITOR_H
#define _C2PS_MONITOR_C2PS_MONITOR_H

#include "c2ps_common.h"
#include "c2ps_monitor_policy.h"

struct reg_check_task {
	u32 id;
	u32 serial_no;
	unsigned long start_timing;
	bool active;
	void (*procfunc)(struct reg_check_input_data);
	struct reg_check_input_data input_data;
	struct reg_check_cache_data cache_data;
	struct hlist_node hlist;
};

struct c2ps_reg_check_proxy_task {
	struct work_struct m_work;
};

void monitor_module_init(void);
int monitor_task_start(int pid, int task_id);
int monitor_task_end(int pid, int task_id);
int monitor_vsync(u64 ts);
int monitor_camfps(int camfps);
void monitor_system_info(void);
int monitor_task_scene_change(int task_id, int scene_mode);
int monitor_anchor(
	int anc_id, u32 order, bool register_fixed, u32 anc_type, u32 anc_order,
	u64 cur_ts, u32 latency_spec, u32 jitter_spec);
void c2ps_monitor_init(void);
void c2ps_monitor_uninit(void);
struct reg_check_task *c2ps_find_reg_check_task_by_id(u32 id);
int register_reg_check_task(struct reg_check_task *tsk);
void clear_reg_check_task_list(void);
int set_reg_check_task(struct reg_check_input_data input_data, void (*procfunc)(struct reg_check_input_data));
void notify_rescue_task(struct reg_check_input_data input_data, bool is_start);

#endif