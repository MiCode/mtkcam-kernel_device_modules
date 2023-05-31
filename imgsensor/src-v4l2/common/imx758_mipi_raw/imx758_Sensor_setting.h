/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 imx758_Sensor_setting.h
 *
 * Project:
 * --------
 * Description:
 * ------------
 *	 CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _IMX758_SENSOR_SETTING_H
#define _IMX758_SENSOR_SETTING_H

#include "kd_camera_typedef.h"

// NOTE:
// for 2 exp setting,  VCID of LE/SE should be 0x00 and 0x02
// which align 3 exp setting LE/NE/SE 0x00,  0x01,  0x02
// to seamless switch,  VC ID of SE should remain the same
// SONY sensor: VCID of 2nd frame at 0x3070; VCID of 3rd frame at 0x3080
// must be two different value

static u16 imx758_init_setting[] = {
	/*External Clock Setting*/
	0x0136,	0x18,
	0x0137,	0x00,
	/*PHY_VIF Setting*/
	0x3204,	0x00,
	/*Register version*/
	0x33F0,	0x02,
	0x33F1,	0x05,
	/*Signaling mode setting*/
	0x0111,	0x02,
	/*Global Setting*/
	0x4BF3,	0x00,
	0x75C3,	0x01,
	0x7602,	0x02,
	0x89A6,	0x06,
	0x89A8,	0x3C,
	0x89A9,	0x70,
	0x89AA,	0x3C,
	/*Image Quality adjustment setting*/
	0x41A8,	0x01,
	0x81F3,	0x67,
	0x9F1B,	0x90,
	0x9F1C,	0x90,
	0x9F1D,	0x34,
	0x9F1E,	0xE8,
	0x9F1F,	0x50,
	0x9F20,	0x06,
	0xA32A,	0x00,
	0xA32B,	0x70,
	0xA337,	0x0B,
	0xA339,	0x59,
	0xA33B,	0x76,
	0xA33D,	0x11,
	0xA33F,	0xAF,
	0xA341,	0xE7,
	0xA343,	0x0F,
	0xA345,	0x69,
	0xA347,	0x89,
	// /*dphy mipi*/
	// 0x080A, 0x00,//TCLK_POST_EX
	// 0x080B, 0xAF,//TCLK_POST_EX
	// 0x080C, 0x00,//THS_PREPARE_EX
	// 0x080D, 0x5F,//THS_PREPARE_EX
	// 0x080E, 0x00,//THS_ZERO_MIN_EX
	// 0x080F, 0xAF,//THS_ZERO_MIN_EX
	// 0x0810, 0x00,//THS_TRAIL_EX
	// 0x0811, 0x5F,//THS_TRAIL_EX
	// 0x0812, 0x00,//TCLK_TRAIL_MIN_EX
	// 0x0813, 0x5F,//TCLK_TRAIL_MIN_EX
	// 0x0814, 0x00,//TCLK_PREPARE_EX
	// 0x0815, 0x5F,//TCLK_PREPARE_EX
	// 0x0816, 0x01,//TCLK_ZERO_MIN_EX
	// 0x0817, 0x9F,//TCLK_ZERO_MIN_EX
	// 0x0818, 0x00,//TLPX_EX
	// 0x0819, 0x4F,//TLPX_EX
	// 0x0824, 0x00,//THS_EXIT_EX
	// 0x0825, 0x9F,//THS_EXIT_EX
	// 0x0826, 0x00,//TCLK_PRE_EX
	// 0x0827, 0x0F,//TCLK_PRE_EX
};

static u16 imx758_preview_setting[] = {
	/*reg_A*/
	/*2x2BIN 4096_3072_30FPS_PD_SEAMLESS1*/
	/*H: 4096*/
	/*V: 3072*/
	/*MIPI output setting*/
	0x0114, 0x03,

	/*Line Length PCK Setting*/
	0x0342, 0x1E,
	0x0343, 0xF0,

	/*Frame Length Lines Setting*/
	0x0340, 0x0E,
	0x0341, 0x70,

	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x17,
	0x034B, 0xFF,

	/*Mode Setting*/
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x00,
	0x30D8, 0x04,
	0x31D0, 0x41,
	0x31D1, 0x41,

	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x10,
	0x040D, 0x00,
	0x040E, 0x0C,
	0x040F, 0x00,

	/*Output Size Setting*/
	0x034C, 0x10,
	0x034D, 0x00,
	0x034E, 0x0C,
	0x034F, 0x00,

	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x6E,
	0x030B, 0x01,
	0x030D, 0x06,
	0x030E, 0x02,
	0x030F, 0x71,

	/*Other Setting*/
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40B2, 0x00,
	0x40B3, 0xFA,
	0x40B4, 0x01,
	0x40B5, 0x90,
	0x40B6, 0x00,
	0x40B7, 0x64,
	0x40B8, 0x01,
	0x40B9, 0xF4,
	0x40BC, 0x02,
	0x40BD, 0x44,
	0x40BE, 0x00,
	0x40BF, 0x64,
	0x40D0, 0x00,
	0x40D1, 0x00,
	0x40D2, 0x00,
	0x40D3, 0x00,

	/*Integration Setting*/
	0x0202, 0x0E,
	0x0203, 0x30,

	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,

	/*PDAF TYPE2 Setting*/
	0x3093, 0x01,
	0x3478, 0x01,
	0x3479, 0xFC,

	/*PDAF TYPE2 data type Setting*/
	0x3072, 0x00,
	0x3073, 0x30,

	/*init deskew test*/
	0x0830, 0x00,//disable periodic deskew
	0x0832, 0x01,//enable init deskew
};

static u16 imx758_capture_setting[] = {
	/*reg_A*/
	/*2x2BIN 4096_3072_30FPS_PD_SEAMLESS1*/
	/*H: 4096*/
	/*V: 3072*/
	/*MIPI output setting*/
	0x0114, 0x03,

	/*Line Length PCK Setting*/
	0x0342, 0x1E,
	0x0343, 0xF0,

	/*Frame Length Lines Setting*/
	0x0340, 0x0E,
	0x0341, 0x70,

	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x17,
	0x034B, 0xFF,

	/*Mode Setting*/
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x00,
	0x30D8, 0x04,
	0x31D0, 0x41,
	0x31D1, 0x41,

	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x10,
	0x040D, 0x00,
	0x040E, 0x0C,
	0x040F, 0x00,

	/*Output Size Setting*/
	0x034C, 0x10,
	0x034D, 0x00,
	0x034E, 0x0C,
	0x034F, 0x00,

	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x6E,
	0x030B, 0x01,
	0x030D, 0x06,
	0x030E, 0x02,
	0x030F, 0x71,

	/*Other Setting*/
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40B2, 0x00,
	0x40B3, 0xFA,
	0x40B4, 0x01,
	0x40B5, 0x90,
	0x40B6, 0x00,
	0x40B7, 0x64,
	0x40B8, 0x01,
	0x40B9, 0xF4,
	0x40BC, 0x02,
	0x40BD, 0x44,
	0x40BE, 0x00,
	0x40BF, 0x64,
	0x40D0, 0x00,
	0x40D1, 0x00,
	0x40D2, 0x00,
	0x40D3, 0x00,

	/*Integration Setting*/
	0x0202, 0x0E,
	0x0203, 0x30,

	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,

	/*PDAF TYPE2 Setting*/
	0x3093, 0x01,
	0x3478, 0x01,
	0x3479, 0xFC,

	/*PDAF TYPE2 data type Setting*/
	0x3072, 0x00,
	0x3073, 0x30,

	/*init deskew test*/
	0x0830, 0x00,//disable periodic deskew
	0x0832, 0x01,//enable init deskew
};

static u16 imx758_normal_video_setting[] = {
	/*reg_B*/
	/*2x2BIN 4K_ 4096_2304_30FPS_PD*/
	/*H: 4096*/
	/*V: 2304*/
	/*MIPI output setting*/
	0x0114, 0x03,

	/*Line Length PCK Setting*/
	0x0342, 0x1E,
	0x0343, 0xF0,

	/*Frame Length Lines Setting*/
	0x0340, 0x0E,
	0x0341, 0x70,

	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x03,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x14,
	0x034B, 0xFF,

	/*Mode Setting*/
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x00,
	0x30D8, 0x04,
	0x31D0, 0x41,
	0x31D1, 0x41,

	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x10,
	0x040D, 0x00,
	0x040E, 0x09,
	0x040F, 0x00,

	/*Output Size Setting*/
	0x034C, 0x10,
	0x034D, 0x00,
	0x034E, 0x09,
	0x034F, 0x00,

	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x6E,
	0x030B, 0x01,
	0x030D, 0x06,
	0x030E, 0x02,
	0x030F, 0x71,

	/*Other Setting*/
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40B2, 0x00,
	0x40B3, 0xFA,
	0x40B4, 0x01,
	0x40B5, 0x90,
	0x40B6, 0x00,
	0x40B7, 0x64,
	0x40B8, 0x01,
	0x40B9, 0xF4,
	0x40BC, 0x02,
	0x40BD, 0x44,
	0x40BE, 0x00,
	0x40BF, 0x64,
	0x40D0, 0x00,
	0x40D1, 0x00,
	0x40D2, 0x00,
	0x40D3, 0x00,

	/*Integration Setting*/
	0x0202, 0x0E,
	0x0203, 0x30,

	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,

	/*PDAF TYPE2 Setting*/
	0x3093, 0x01,
	0x3478, 0x01,
	0x3479, 0xFC,

	/*PDAF TYPE2 data type Setting*/
	0x3072, 0x00,
	0x3073, 0x30,

	/*init deskew test*/
	0x0830, 0x00,//disable periodic deskew
	0x0832, 0x01,//enable init deskew
};

static u16 imx758_hs_video_setting[] = {
	/*reg_C*/
	/*2x2BIN 4K_ 4096_2304_60FPS*/
	/*H: 4096*/
	/*V: 2304*/
	/*MIPI output setting*/
	0x0114, 0x03,

	/*Line Length PCK Setting*/
	0x0342, 0x13,
	0x0343, 0xAC,

	/*Frame Length Lines Setting*/
	0x0340, 0x09,
	0x0341, 0x96,

	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x03,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x14,
	0x034B, 0xFF,

	/*Mode Setting*/
	0x0900, 0x01,
	0x0901, 0x22,
	0x0902, 0x00,
	0x30D8, 0x00,
	0x31D0, 0x41,
	0x31D1, 0x41,

	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x10,
	0x040D, 0x00,
	0x040E, 0x09,
	0x040F, 0x00,

	/*Output Size Setting*/
	0x034C, 0x10,
	0x034D, 0x00,
	0x034E, 0x09,
	0x034F, 0x00,

	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x35,
	0x030B, 0x01,
	0x030D, 0x06,
	0x030E, 0x02,
	0x030F, 0x71,

	/*Other Setting*/
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40B2, 0x00,
	0x40B3, 0x00,
	0x40B4, 0x01,
	0x40B5, 0x7C,
	0x40B6, 0x01,
	0x40B7, 0x7C,
	0x40B8, 0x01,
	0x40B9, 0xF4,
	0x40BC, 0x01,
	0x40BD, 0x36,
	0x40BE, 0x01,
	0x40BF, 0x36,
	0x40D0, 0x00,
	0x40D1, 0x00,
	0x40D2, 0x00,
	0x40D3, 0x00,

	/*Integration Setting*/
	0x0202, 0x09,
	0x0203, 0x56,

	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,

	/*PDAF TYPE2 Setting*/
	0x3093, 0x00,
	0x3478, 0x01,
	0x3479, 0xFC,

	/*PDAF TYPE2 data type Setting*/
	0x3072, 0x00,
	0x3073, 0x30,

	/*init deskew test*/
	0x0830, 0x00,//disable periodic deskew
	0x0832, 0x01,//enable init deskew
};

static u16 imx758_slim_video_setting[] = {
	/*reg_F*/
	/*FULL_CROP_4096_3072_24FPS_PD_SEAMLESS1*/
	/*H: 4096*/
	/*V: 3072*/
	/*MIPI output setting*/
	0x0114, 0x03,

	/*Line Length PCK Setting*/
	0x0342, 0x22,
	0x0343, 0x68,

	/*Frame Length Lines Setting*/
	0x0340, 0x10,
	0x0341, 0x3B,

	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x06,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x11,
	0x034B, 0xFF,

	/*Mode Setting*/
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x00,
	0x30D8, 0x00,
	0x31D0, 0x01,
	0x31D1, 0x01,

	/*Digital Crop & Scaling*/
	0x0408, 0x08,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x10,
	0x040D, 0x00,
	0x040E, 0x0C,
	0x040F, 0x00,

	/*Output Size Setting*/
	0x034C, 0x10,
	0x034D, 0x00,
	0x034E, 0x0C,
	0x034F, 0x00,

	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x6E,
	0x030B, 0x01,
	0x030D, 0x06,
	0x030E, 0x02,
	0x030F, 0x71,

	/*Other Setting*/
	0x32D5, 0x01,
	0x32D6, 0x00,//0x01
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40B2, 0x08,
	0x40B3, 0x98,
	0x40B4, 0x00,
	0x40B5, 0xF0,
	0x40B6, 0x00,
	0x40B7, 0xF0,
	0x40B8, 0x0D,
	0x40B9, 0xAC,
	0x40BC, 0x00,
	0x40BD, 0xFA,
	0x40BE, 0x00,
	0x40BF, 0xFA,
	0x40D0, 0x00,
	0x40D1, 0x00,
	0x40D2, 0x00,
	0x40D3, 0x00,

	/*Integration Setting*/
	0x0202, 0x0F,
	0x0203, 0xFB,

	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,

	/*PDAF TYPE2 Setting*/
	0x3093, 0x01,
	0x3478, 0x01,
	0x3479, 0x00,

	/*PDAF TYPE2 data type Setting*/
	0x3072, 0x00,
	0x3073, 0x30,

	/*init deskew test*/
	0x0830, 0x00,//disable periodic deskew
	0x0832, 0x01,//enable init deskew
};

static u16 imx758_custom1_setting[] = {
	/*reg_G*/
	/*FULL_CROP_RAW_4096_3072_24FPS_SEAMLESS1*/
	/*H: 4096*/
	/*V: 3072*/
	/*MIPI output setting*/
	0x0114, 0x03,

	/*Line Length PCK Setting*/
	0x0342, 0x22,
	0x0343, 0x68,

	/*Frame Length Lines Setting*/
	0x0340, 0x10,
	0x0341, 0x3B,

	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x06,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x11,
	0x034B, 0xFF,

	/*Mode Setting*/
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x00,
	0x30D8, 0x00,
	0x31D0, 0x01,
	0x31D1, 0x01,

	/*Digital Crop & Scaling*/
	0x0408, 0x08,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x10,
	0x040D, 0x00,
	0x040E, 0x0C,
	0x040F, 0x00,

	/*Output Size Setting*/
	0x034C, 0x10,
	0x034D, 0x00,
	0x034E, 0x0C,
	0x034F, 0x00,

	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x6E,
	0x030B, 0x01,
	0x030D, 0x06,
	0x030E, 0x02,
	0x030F, 0x71,

	/*Other Setting*/
	0x32D5, 0x00,
	0x32D6, 0x00,//0x01
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40B2, 0x08,
	0x40B3, 0x98,
	0x40B4, 0x00,
	0x40B5, 0xF0,
	0x40B6, 0x00,
	0x40B7, 0xF0,
	0x40B8, 0x0D,
	0x40B9, 0xAC,
	0x40BC, 0x00,
	0x40BD, 0xFA,
	0x40BE, 0x00,
	0x40BF, 0xFA,
	0x40D0, 0x00,
	0x40D1, 0x00,
	0x40D2, 0x00,
	0x40D3, 0x00,

	/*Integration Setting*/
	0x0202, 0x0F,
	0x0203, 0xFB,

	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,

	/*PDAF TYPE2 Setting*/
	0x3093, 0x01,
	0x3478, 0x01,
	0x3479, 0x00,

	/*PDAF TYPE2 data type Setting*/
	0x3072, 0x00,
	0x3073, 0x30,

	/*init deskew test*/
	0x0830, 0x00,//disable periodic deskew
	0x0832, 0x01,//enable init deskew
};

static u16 imx758_custom2_setting[] = {
	/*reg_H*/
	/*FULL_8192_6144_15FPS_PD_SEAMLESS1*/
	/*H: 8192*/
	/*V: 6144*/
	/*MIPI output setting*/
	0x0114, 0x03,

	/*Line Length PCK Setting*/
	0x0342, 0x22,
	0x0343, 0x68,

	/*Frame Length Lines Setting*/
	0x0340, 0x19,
	0x0341, 0xF8,

	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x17,
	0x034B, 0xFF,

	/*Mode Setting*/
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x00,
	0x30D8, 0x00,
	0x31D0, 0x01,
	0x31D1, 0x01,

	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x20,
	0x040D, 0x00,
	0x040E, 0x18,
	0x040F, 0x00,

	/*Output Size Setting*/
	0x034C, 0x20,
	0x034D, 0x00,
	0x034E, 0x18,
	0x034F, 0x00,

	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x6E,
	0x030B, 0x01,
	0x030D, 0x06,
	0x030E, 0x02,
	0x030F, 0x71,

	/*Other Setting*/
	0x32D5, 0x01,
	0x32D6, 0x00,//0x01
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40B2, 0x08,
	0x40B3, 0x98,
	0x40B4, 0x00,
	0x40B5, 0xF0,
	0x40B6, 0x00,
	0x40B7, 0xF0,
	0x40B8, 0x0D,
	0x40B9, 0xAC,
	0x40BC, 0x00,
	0x40BD, 0xFA,
	0x40BE, 0x00,
	0x40BF, 0xFA,
	0x40D0, 0x00,
	0x40D1, 0x00,
	0x40D2, 0x00,
	0x40D3, 0x00,

	/*Integration Setting*/
	0x0202, 0x19,
	0x0203, 0xB8,

	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,

	/*PDAF TYPE2 Setting*/
	0x3093, 0x01,
	0x3478, 0x01,
	0x3479, 0xFC,

	/*PDAF TYPE2 data type Setting*/
	0x3072, 0x00,
	0x3073, 0x30,

	/*init deskew test*/
	0x0830, 0x00,//disable periodic deskew
	0x0832, 0x01,//enable init deskew
};
static u16 imx758_custom3_setting[] = {
	/*reg_I*/
	/*FULL_RAW_8192_6144_15FPS_PD_SEAMLESS1*/
	/*H: 8192*/
	/*V: 6144*/
	/*MIPI output setting*/
	0x0114, 0x03,

	/*Line Length PCK Setting*/
	0x0342, 0x22,
	0x0343, 0x68,

	/*Frame Length Lines Setting*/
	0x0340, 0x19,
	0x0341, 0xF8,

	/*ROI Setting*/
	0x0344, 0x00,
	0x0345, 0x00,
	0x0346, 0x00,
	0x0347, 0x00,
	0x0348, 0x1F,
	0x0349, 0xFF,
	0x034A, 0x17,
	0x034B, 0xFF,

	/*Mode Setting*/
	0x0900, 0x00,
	0x0901, 0x11,
	0x0902, 0x00,
	0x30D8, 0x00,
	0x31D0, 0x01,
	0x31D1, 0x01,

	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x0409, 0x00,
	0x040A, 0x00,
	0x040B, 0x00,
	0x040C, 0x20,
	0x040D, 0x00,
	0x040E, 0x18,
	0x040F, 0x00,

	/*Output Size Setting*/
	0x034C, 0x20,
	0x034D, 0x00,
	0x034E, 0x18,
	0x034F, 0x00,

	/*Clock Setting*/
	0x0301, 0x05,
	0x0303, 0x02,
	0x0305, 0x04,
	0x0306, 0x01,
	0x0307, 0x6E,
	0x030B, 0x01,
	0x030D, 0x06,
	0x030E, 0x02,
	0x030F, 0x71,

	/*Other Setting*/
	0x32D5, 0x00,
	0x32D6, 0x00,//0x01
	0x40A0, 0x00,
	0x40A1, 0x00,
	0x40A4, 0x00,
	0x40A5, 0x00,
	0x40B2, 0x08,
	0x40B3, 0x98,
	0x40B4, 0x00,
	0x40B5, 0xF0,
	0x40B6, 0x00,
	0x40B7, 0xF0,
	0x40B8, 0x0D,
	0x40B9, 0xAC,
	0x40BC, 0x00,
	0x40BD, 0xFA,
	0x40BE, 0x00,
	0x40BF, 0xFA,
	0x40D0, 0x00,
	0x40D1, 0x00,
	0x40D2, 0x00,
	0x40D3, 0x00,

	/*Integration Setting*/
	0x0202, 0x19,
	0x0203, 0xB8,

	/*Gain Setting*/
	0x0204, 0x00,
	0x0205, 0x00,
	0x020E, 0x01,
	0x020F, 0x00,

	/*PDAF TYPE2 Setting*/
	0x3093, 0x01,
	0x3478, 0x01,
	0x3479, 0xFC,

	/*PDAF TYPE2 data type Setting*/
	0x3072, 0x00,
	0x3073, 0x30,

	/*init deskew test*/
	0x0830, 0x00,//disable periodic deskew
	0x0832, 0x01,//enable init deskew
};

/* seamless_switch */
#define PHASE_PIX_OUT_EN            0x30B4
#define LINE_LEN_UPPER              0x0342
#define LINE_LEN_LOWER              0x0343
#define FRAME_LEN_UPPER             0x0340
#define FRAME_LEN_LOWER             0x0341
#define DOL_EN                      0x33D0
#define DOL_MODE                    0x33D1
#define BINNING_TYPE                0x0901
#define BINNING_PRIORITY_H          0x3200
#define BINNING_PRIORITY_V          0x3201
#define X_ADD_STA_UPPER             0x0344
#define X_ADD_STA_LOWER             0x0345
#define Y_ADD_STA_UPPER             0x0346
#define Y_ADD_STA_LOWER             0x0347
#define X_ADD_END_UPPER             0x0348
#define X_ADD_END_LOWER             0x0349
#define Y_ADD_END_UPPER             0x034A
#define Y_ADD_END_LOWER             0x034B
#define DIG_CROP_X_OFFSET_UPPER     0x0408
#define DIG_CROP_X_OFFSET_LOWER     0x0409
#define DIG_CROP_Y_OFFSET_UPPER     0x040A
#define DIG_CROP_Y_OFFSET_LOWER     0x040B
#define DIG_CROP_IMAGE_WIDTH_UPPER  0x040C
#define DIG_CROP_IMAGE_WIDTH_LOWER  0x040D
#define DIG_CROP_IMAGE_HEIGHT_UPPER 0x040E
#define DIG_CROP_IMAGE_HEIGHT_LOWER 0x040F
#define X_OUT_SIZE_UPPER            0x034C
#define X_OUT_SIZE_LOWER            0x034D
#define Y_OUT_SIZE_UPPER            0x034E
#define Y_OUT_SIZE_LOWER            0x034F

// video stagger seamless switch (1exp-2exp)
static u16 imx758_seamless_normal_video[] = {
	PHASE_PIX_OUT_EN, 0x01,
	FRAME_LEN_UPPER, 0x12,
	FRAME_LEN_LOWER, 0xBA,
	DOL_EN, 0x00,
};
static u16 imx758_seamless_custom4[] = {
	PHASE_PIX_OUT_EN, 0x03,
	FRAME_LEN_UPPER, 0x09,
	FRAME_LEN_LOWER, 0x5C,
	DOL_EN, 0x01,
};

// stagger seamless switch (1exp-2exp-3exp)
// normal seamless switch
static u16 imx758_seamless_preview[] = {
	/*reg_A*/
	/*2x2BIN 4096_3072_30FPS_PD_SEAMLESS1*/
	/*H: 4096*/
	/*V: 3072*/
	/*Line Length PCK Setting*/
	0x0342, 0x1E,
	0x0343, 0xF0,
	/*Frame Length Lines Setting*/
	0x0340, 0x0E,
	0x0341, 0x70,
	/*ROI Setting*/
	0x0346, 0x00,
	0x034A, 0x17,
	/*Mode Setting*/
	0x0900, 0x01,
	0x0901, 0x22,
	0x30D8, 0x04,
	0x31D0, 0x41,
	0x31D1, 0x41,
	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x040C, 0x10,
	0x040E, 0x0C,
	/*Output Size Setting*/
	0x034C, 0x10,
	0x034E, 0x0C,
	/*Other Setting*/
	0x32D5, 0x00,
	0x32D6, 0x00,
	0x40B2, 0x00,
	0x40B3, 0xFA,
	0x40B4, 0x01,
	0x40B5, 0x90,
	0x40B7, 0x64,
	0x40B8, 0x01,
	0x40B9, 0xF4,
	0x40BC, 0x02,
	0x40BD, 0x44,
	0x40BF, 0x64,
	/*Integration Setting*/
	0x0202, 0x0E,
	0x0203, 0x30,
	/*PDAF TYPE2 Setting*/
	0x3479, 0xFC,
};

static u16 imx758_seamless_slim_video[] = {
	/*reg_F*/
	/*FULL_CROP_4096_3072_24FPS_PD_SEAMLESS1*/
	/*H: 4096*/
	/*V: 3072*/
	/*Line Length PCK Setting*/
	0x0342, 0x22,
	0x0343, 0x68,
	/*Frame Length Lines Setting*/
	0x0340, 0x10,
	0x0341, 0x3B,
	/*ROI Setting*/
	0x0346, 0x06,
	0x034A, 0x11,
	/*Mode Setting*/
	0x0900, 0x00,
	0x0901, 0x11,
	0x30D8, 0x00,
	0x31D0, 0x01,
	0x31D1, 0x01,
	/*Digital Crop & Scaling*/
	0x0408, 0x08,
	0x040C, 0x10,
	0x040E, 0x0C,
	/*Output Size Setting*/
	0x034C, 0x10,
	0x034E, 0x0C,
	/*Other Setting*/
	0x32D5, 0x01,
	0x32D6, 0x00,//0x01
	0x40B2, 0x08,
	0x40B3, 0x98,
	0x40B4, 0x00,
	0x40B5, 0xF0,
	0x40B7, 0xF0,
	0x40B8, 0x0D,
	0x40B9, 0xAC,
	0x40BC, 0x00,
	0x40BD, 0xFA,
	0x40BF, 0xFA,
	/*Integration Setting*/
	0x0202, 0x0F,
	0x0203, 0xFB,
	/*PDAF TYPE2 Setting*/
	0x3479, 0x00,
};

static u16 imx758_seamless_custom1[] = {
	/*reg_G*/
	/*FULL_CROP_RAW_4096_3072_24FPS_SEAMLESS1*/
	/*H: 4096*/
	/*V: 3072*/
	/*Line Length PCK Setting*/
	0x0342, 0x22,
	0x0343, 0x68,
	/*Frame Length Lines Setting*/
	0x0340, 0x10,
	0x0341, 0x3B,
	/*ROI Setting*/
	0x0346, 0x06,
	0x034A, 0x11,
	/*Mode Setting*/
	0x0900, 0x00,
	0x0901, 0x11,
	0x30D8, 0x00,
	0x31D0, 0x01,
	0x31D1, 0x01,
	/*Digital Crop & Scaling*/
	0x0408, 0x08,
	0x040C, 0x10,
	0x040E, 0x0C,
	/*Output Size Setting*/
	0x034C, 0x10,
	0x034E, 0x0C,
	/*Other Setting*/
	0x32D5, 0x00,
	0x32D6, 0x00,//0x01
	0x40B2, 0x08,
	0x40B3, 0x98,
	0x40B4, 0x00,
	0x40B5, 0xF0,
	0x40B7, 0xF0,
	0x40B8, 0x0D,
	0x40B9, 0xAC,
	0x40BC, 0x00,
	0x40BD, 0xFA,
	0x40BF, 0xFA,
	/*Integration Setting*/
	0x0202, 0x0F,
	0x0203, 0xFB,
	/*PDAF TYPE2 Setting*/
	0x3479, 0x00,
};

static u16 imx758_seamless_custom2[] = {
	/*reg_H*/
	/*FULL_8192_6144_15FPS_PD_SEAMLESS1*/
	/*H: 8192*/
	/*V: 6144*/
	/*Line Length PCK Setting*/
	0x0342, 0x22,
	0x0343, 0x68,
	/*Frame Length Lines Setting*/
	0x0340, 0x19,
	0x0341, 0xF8,
	/*ROI Setting*/
	0x0346, 0x00,
	0x034A, 0x17,
	/*Mode Setting*/
	0x0900, 0x00,
	0x0901, 0x11,
	0x30D8, 0x00,
	0x31D0, 0x01,
	0x31D1, 0x01,
	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x040C, 0x20,
	0x040E, 0x18,
	/*Output Size Setting*/
	0x034C, 0x20,
	0x034E, 0x18,
	/*Other Setting*/
	0x32D5, 0x01,
	0x32D6, 0x00,//0x01
	0x40B2, 0x08,
	0x40B3, 0x98,
	0x40B4, 0x00,
	0x40B5, 0xF0,
	0x40B7, 0xF0,
	0x40B8, 0x0D,
	0x40B9, 0xAC,
	0x40BC, 0x00,
	0x40BD, 0xFA,
	0x40BF, 0xFA,
	/*Integration Setting*/
	0x0202, 0x19,
	0x0203, 0xB8,
	/*PDAF TYPE2 Setting*/
	0x3479, 0xFC,
};

static u16 imx758_seamless_custom3[] = {
	/*Line Length PCK Setting*/
	0x0342, 0x22,
	0x0343, 0x68,
	/*Frame Length Lines Setting*/
	0x0340, 0x19,
	0x0341, 0xF8,
	/*ROI Setting*/
	0x0346, 0x00,
	0x034A, 0x17,
	/*Mode Setting*/
	0x0900, 0x00,
	0x0901, 0x11,
	0x30D8, 0x00,
	0x31D0, 0x01,
	0x31D1, 0x01,
	/*Digital Crop & Scaling*/
	0x0408, 0x00,
	0x040C, 0x20,
	0x040E, 0x18,
	/*Output Size Setting*/
	0x034C, 0x20,
	0x034E, 0x18,
	/*Other Setting*/
	0x32D5, 0x00,
	0x32D6, 0x00,//0x01
	0x40B2, 0x08,
	0x40B3, 0x98,
	0x40B4, 0x00,
	0x40B5, 0xF0,
	0x40B7, 0xF0,
	0x40B8, 0x0D,
	0x40B9, 0xAC,
	0x40BC, 0x00,
	0x40BD, 0xFA,
	0x40BF, 0xFA,
	/*Integration Setting*/
	0x0202, 0x19,
	0x0203, 0xB8,
	/*PDAF TYPE2 Setting*/
	0x3479, 0xFC,
};
#endif
