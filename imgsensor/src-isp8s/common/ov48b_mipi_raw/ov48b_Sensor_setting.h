/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 ov48b2q_Sensor_setting.h
 *
 * Project:
 * --------
 * Description:
 * ------------
 *	 CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _OV48B2Q_SENSOR_SETTING_H
#define _OV48B2Q_SENSOR_SETTING_H

#define USER_DEFINE_TRIO 3

u16 isf_addr_data_pair_seamless_switch_step1_ov48b2q[] = {
	//group 0
	0x3208, 0x00,
	0x3016, 0xf1,
	//new mode settings
};
u16 isf_addr_data_pair_seamless_switch_step2_ov48b2q[] = {
	0x3016, 0xf0,
	0x3208, 0x10,
	//group 1
	0x3208, 0x01,
	//0x3016, 0xf1,
	//new gain for 2nd frame
};
u16 isf_addr_data_pair_seamless_switch_step3_ov48b2q[] = {
	//0x3016, 0xf0,
	0x3208, 0x11,
	//group 2 - intend to be blank
	0x3208, 0x02,
	0x3208, 0x12,
	//group 3 - intend to be blank
	0x3208, 0x03,
	0x3208, 0x13,
	//switch
	0x3689, 0x27,
	0x320d, 0x07,
	0x3209, 0x01,
	0x320a, 0x01,
	0x320b, 0x01,
	0x320c, 0x01,
	0x320e, 0xa0,
};
#endif
