/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2020 MediaTek Inc. */

#ifndef __IMGSENSOR_KRN_USER_H__
#define __IMGSENSOR_KRN_USER_H__

#include "kd_imgsensor_define_v4l2.h"
#include "imgsensor-user.h"

/*
 * This file defineds struct/type/enum only for common kernel users
 * and no sync to user space kernel header
 */

enum {
	IMGSENSOR_LUT_NA = 0,
	IMGSENSOR_LUT_A = IMGSENSOR_LUT_NA,
	IMGSENSOR_LUT_B,
	IMGSENSOR_LUT_C,
	IMGSENSOR_LUT_D,
	IMGSENSOR_LUT_E,
	IMGSENSOR_LUT_MAXCNT,
};

struct struct_lut_info {
	u32 readout_length;
	u32 read_margin;
	u32 framelength_step;
	u32 min_vblanking_line;
	u32 cit_loss;
	u32 linetime_in_ns;
};
struct struct_exp_info {
	u8 lut_idx;

	u32 coarse_integ_step;
	u32 exposure_margin;
	u32 ae_binning_ratio;
	int fine_integ_line;
	u32 dig_gain_min;
	u32 dig_gain_max;
	u32 dig_gain_step;
	u32 ana_gain_min;
	u32 ana_gain_max;
	u32 ana_gain_step;
	u64 shutter_min;
	u64 shutter_max;
};

struct struct_dcg_vsl_info {
	u8 lut_cnt;
	u8 exp_cnt;
	struct struct_lut_info lut_info[IMGSENSOR_LUT_MAXCNT];
	struct struct_exp_info exp_info[IMGSENSOR_EXPOSURE_CNT];
};

/* V4L2_CMD_G_DCG_VSL_LINETIME_INFO used */
struct struct_cmd_sensor_dcg_vsl_info {
	u32 scenario_id;
	u32 hdr_mode;
	struct struct_dcg_vsl_info info;
};

#endif
