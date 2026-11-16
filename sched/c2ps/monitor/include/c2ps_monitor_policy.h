/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef C2PS_REGULATOR_INCLUDE_C2PS_MONITOR_POLICY_H_
#define C2PS_REGULATOR_INCLUDE_C2PS_MONITOR_POLICY_H_

#include "c2ps_common.h"

#define MAX_PERFMONITOR_THREAD_NUM 4

// rescue policy
enum c2ps_rescue_mode : int
{
	C2PS_RESCUE_DISABLE,
	C2PS_RESCUE_AGGRESIVE,
	C2PS_RESCUE_CONVERGE,
	C2PS_RESCUE_NORMAL,
};

struct reg_check_cache_data {
	unsigned long start_timing;
	int rescue_count;
	int curr_uclamp;
	int last_high_uclamp;
	int last_low_uclamp;
	int num_thread;
	int threads[MAX_PERFMONITOR_THREAD_NUM];
};

struct reg_check_input_data {
	u32 target_id;
	u32 serial_no;
	int start_in_ms;
	enum c2ps_rescue_mode rescue_mode;
	struct reg_check_cache_data *cache_data;
};


// reg_check_action
void reg_check_action_aggresive_set_uclamp(struct reg_check_input_data input_data);
void reg_check_action_aggresive_unset_uclamp(struct reg_check_input_data input_data);
void reg_check_action_converge_set_uclamp(struct reg_check_input_data input_data);
void reg_check_action_converge_unset_uclamp(struct reg_check_input_data input_data);
void reg_check_action_normal_set_uclamp(struct reg_check_input_data input_data);
void reg_check_action_normal_unset_uclamp(struct reg_check_input_data input_data);


#endif  // C2PS_REGULATOR_INCLUDE_C2PS_MONITOR_POLICY_H_
