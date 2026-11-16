/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef C2PS_REGULATOR_INCLUDE_C2PS_REGULATOR_POLICY_H_
#define C2PS_REGULATOR_INCLUDE_C2PS_REGULATOR_POLICY_H_

#include "c2ps_common.h"
#include <linux/sched/cputime.h>
#include <sched/sched.h>

#define C2PS_UM_RATIO_LOWER_BOUND 7
#define C2PS_UM_RATIO_UPPER_BOUND 14

// critical task policy
void c2ps_regulator_policy_fix_uclamp(struct regulator_req *req);
// void c2ps_regulator_policy_simple(struct regulator_req *req);
void c2ps_regulator_policy_debug_uclamp(struct regulator_req *req);

// background uclamp max policy
void c2ps_regulator_bgpolicy_simple(struct regulator_req *req);
// background um policy
void c2ps_regulator_bgpolicy_um_stable_default(struct regulator_req *req);
void c2ps_regulator_bgpolicy_um_stable(struct regulator_req *req);
void c2ps_regulator_bgpolicy_um_transient(struct regulator_req *req);
void c2ps_regulator_bgpolicy_um_runnable_boost(struct regulator_req *req);
// dynamically adjust um ratio
void c2ps_decide_um_ratio(struct regulator_req *req);

enum c2ps_regulator_mode : int
{
	// Uclamp control
	C2PS_REGULATOR_MODE_FIX = 0,
	C2PS_REGULATOR_MODE_SIMPLE,
	C2PS_REGULATOR_MODE_DEBUG,
	C2PS_REGULATOR_BGMODE_SIMPLE,

	// Util margin control
	C2PS_REGULATOR_BGMODE_UM_STABLE_DEFAULT,
	C2PS_REGULATOR_BGMODE_UM_STABLE,
	C2PS_REGULATOR_BGMODE_UM_TRANSIENT,
	C2PS_REGULATOR_BGMODE_UM_RUNNABLE_BOOST,
};

#endif  // C2PS_REGULATOR_INCLUDE_C2PS_REGULATOR_POLICY_H_
