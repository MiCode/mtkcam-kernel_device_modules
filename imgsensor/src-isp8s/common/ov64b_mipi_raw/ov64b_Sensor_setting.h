/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 ov64b_Sensor_setting.h
 *
 * Project:
 * --------
 * Description:
 * ------------
 *	 CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _OV64B_SENSOR_SETTING_H
#define _OV64B_SENSOR_SETTING_H

#include "kd_camera_typedef.h"

// NOTE:
// for 2 exp setting,  VCID of LE/SE should be 0x00 and 0x02
// which align 3 exp setting LE/NE/SE 0x00,  0x01,  0x02
// to seamless switch,  VC ID of SE should remain the same
// SONY sensor: VCID of 2nd frame at 0x3070; VCID of 3rd frame at 0x3080
// must be two different value


// semless switch step 1
u16 isf_addr_data_pair_seamless_switch_step1_ov64b[] = {
	// group 0
	0x3208, 0x00,
	0x3016, 0xf1,
	// new mode settings
};

// semless switch step 2
u16 isf_addr_data_pair_seamless_switch_step2_ov64b[] = {
	0x3046, 0x01,
	0x3016, 0xf0,
	0x3208, 0x10,
	// group 1
	0x3208, 0x01,
	// 0x3016 f1
	// new gain for 2nd frame
};

// semless switch step 3
u16 isf_addr_data_pair_seamless_switch_step3_ov64b[] = {
	// 0x3046, 0x01,
	// 0x3016 f0
	0x3208, 0x11,
	// group 2 - intend to be blank
	0x3208, 0x02,
	0x3208, 0x12,
	// group 3 - intend to be blank
	0x3208, 0x03,
	0x3208, 0x13,
	// switch
	0x368B, 0xEF,
	0x320d, 0x93,
	0x3209, 0x01,
	0x320a, 0x01,
	0x320b, 0x01,
	0x320c, 0x01,
	0x320e, 0xa0,
};

// semless switch step 3 HDR to linear
u16 isf_addr_data_pair_seamless_switch_step3_HDR_ov64b[] = {
	// 0x3046, 0x01,
	// 0x3016 f0
	0x3208, 0x11,
	// group 2 - intend to be blank
	0x3208, 0x02,
	0x3208, 0x12,
	// group 3 - intend to be blank
	0x3208, 0x03,
	0x3208, 0x13,
	// switch
	0x368B, 0xFF,
	0x320d, 0x93,
	0x3209, 0x01,
	0x320a, 0x01,
	0x320b, 0x01,
	0x320c, 0x01,
	0x382a, 0x82,
	0x382e, 0x40,
	0x382f, 0x04,
	0x320e, 0xa0,
};

#endif
