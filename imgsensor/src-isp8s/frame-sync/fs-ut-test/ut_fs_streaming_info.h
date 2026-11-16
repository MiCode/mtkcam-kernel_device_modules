/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _UT_FS_STREAMING_INFO_H
#define _UT_FS_STREAMING_INFO_H

#include "../frame_sync.h"


/* utility marco */
/**
 *  x : input time in us.
 *  y : put sensor lineTimeInNs value.
 */
#define US_TO_LC(x, y) ((x)*1000/(y)+(((x)*1000%(y))?1:0))


struct ut_fs_streaming_sensor_list {
	char *sensor_name;
	unsigned int sensor_idx;
	unsigned int tg;
	struct fs_streaming_st *sensor;
};


/******************************************************************************/
struct fs_streaming_st imx586 = {
	.sensor_idx = 0,
	.sensor_id = 0x0586,
	.tg = 2,
	.fl_active_delay = 3,
	.def_fl_lc = 3068,
	.max_fl_lc = 0xffff,
	.def_shutter_lc = 0x3D0,
	.lineTimeInNs = 10469,
};


struct fs_streaming_st s5k3m5sx = {
	.sensor_idx = 2,
	.sensor_id = 0x30D5,
	.tg = 1,
	.fl_active_delay = 2,
	.def_fl_lc = 3314,
	.max_fl_lc = 0xffff,
	.def_shutter_lc = 0x3D0,
	.lineTimeInNs = 10059,
};


struct fs_streaming_st imx481 = {
	.sensor_idx = 4,
	.sensor_id = 0x0481,
	.tg = 3,
	.fl_active_delay = 3,
	.def_fl_lc = 3776,
	.max_fl_lc = 0xffff,
	.def_shutter_lc = 0x3D0,
	.lineTimeInNs = 8828,
};


struct fs_streaming_st imx766 = {
	.sensor_idx = 0,
	.sensor_id = 0x0766,
	.tg = 2,
	.fl_active_delay = 3,
	.def_fl_lc = 4844,
	.max_fl_lc = 0xffff,
	.def_shutter_lc = 0x3D0,
	.lineTimeInNs = 6879,
};


struct fs_streaming_st imx516 = {
	.sensor_idx = 0,
	.sensor_id = 0x0516,
	.tg = 3,
	.fl_active_delay = 2,
	.def_fl_lc = 2400,
	.max_fl_lc = 0xfffe,
	.def_shutter_lc = 0x029,
	.lineTimeInNs = 1389,
};


struct fs_streaming_st ov64b = {
	.sensor_idx = 0,
	.sensor_id = 0x64,
	.tg = 2,
	.fl_active_delay = 2,
	.def_fl_lc = 3660*2,
	.max_fl_lc = 0xFFFFFF,
	.def_shutter_lc = 0x3D0,
	.lineTimeInNs = 4584,
};


struct fs_streaming_st imx09a = {
	.sensor_idx = 0,
	.sensor_id = 0x0910,
	.tg = 2,
	.fl_active_delay = 3,
	.def_fl_lc = 5802,   /* (2744*5.74 + 4944*3.55)/5.74 */
	.max_fl_lc = 0xffff,
	.def_shutter_lc = 0x3D0,
	.hdr_exp.mode_exp_cnt = 2,
	.hdr_exp.multi_exp_type = 2,   /* MULTI_EXP_TYPE_DCG_VSL */
	.hdr_exp.exp_order = 0,        /* NE first */
	.hdr_exp.ae_exp_cnt = 2,
	.hdr_exp.exp_lc = {
		US_TO_LC(10002, 3550),
		US_TO_LC(2500, 5740),
		0,
		0
	},
	.hdr_exp.readout_len_lc = 2476,
	.hdr_exp.read_margin_lc = 148,
	.hdr_exp.cas_mode_info.lineTimeInNs = {
		5740,
		3550,
		0,
		0,
		0
	},
	.hdr_exp.cas_mode_info.margin_lc = {
		64,
		64,
		0,
		0,
		0
	},
	.hdr_exp.cas_mode_info.read_margin_lc = {
		148,
		124,
		0,
		0,
		0
	},
	.hdr_exp.cas_mode_info.cit_loss_lc = {
		3293,
		0,
		0,
		0,
		0
	},
	.lineTimeInNs = 5740,
};


/******************************************************************************/
struct ut_fs_streaming_sensor_list ut_fs_s_list[] = {
	/* Head */
	{
		.sensor_name = "imx586",
		.sensor = &imx586,
	},

	{
		.sensor_name = "s5k3m5sx",
		.sensor = &s5k3m5sx,
	},

	{
		.sensor_name = "imx481",
		.sensor = &imx481,
	},

	{
		.sensor_name = "imx766",
		.sensor = &imx766,
	},

	{
		.sensor_name = "imx516",
		.sensor = &imx516,
	},

	{
		.sensor_name = "ov64b",
		.sensor = &ov64b,
	},

	{
		.sensor_name = "imx09a",
		.sensor = &imx09a,
	},

	/* End */
	{
		.sensor_name = "NULL",
		.sensor_idx = 0,
		.tg = 255,
		.sensor = NULL,
	},
};

#endif
