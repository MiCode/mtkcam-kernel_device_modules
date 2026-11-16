/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef __MTK_CAM_SENINF_CONTROL_8_H__
#define __MTK_CAM_SENINF_CONTROL_8_H__

#include <linux/videodev2.h>
#include "mtk_cam-aov-data-isp8s.h"

#define MAX_OUTMUX_DEBUG_RESULT 15

enum seninf_sentest_ctrl_id {
	/* GET CTRL */
	SENINF_SENTEST_G_CTRL_ID_MIN,
	SENINF_SENTEST_G_DEBUG_RESULT = SENINF_SENTEST_G_CTRL_ID_MIN,
	SENINF_SENTEST_G_SEAMLESS_STATUS,
	SENINF_SENTEST_G_SENSOR_METER_INFO_BY_LINE,
	SENINF_SENTEST_G_SEAMLESS_LAST_FRAME_INFO,
	SENINF_SENTEST_G_CTRL_ID_MAX,

	/* SET CTRL */
	SENINF_SENTEST_S_CTRL_ID_MIN,
	SENINF_SENTEST_S_MAX_ISP_EN = SENINF_SENTEST_S_CTRL_ID_MIN,
	SENINF_SENTEST_S_SINGLE_STREAM_RAW,
	SENINF_SENTEST_S_SEAMLESS_UT_EN,
	SENINF_SENTEST_S_SEAMLESS_UT_CONFIG,
	SENINF_SENTEST_S_TSREC_VC_DT_MANUAL_CONFIG,
	SENINF_SENTEST_S_ACTIVE_FRAME_MEASURE_EN,
	SENINF_SENTEST_S_VSYNC_NOTIFY_TO_IMGSENSOR_EN,
	SENINF_SENTEST_S_CTRL_ID_MAX,
};

enum CSIMAC_MEASURE {
	CSIMAC_MEASURE_0,
	CSIMAC_MEASURE_1,
	CSIMAC_MEASURE_MAX_NUM
};

struct mipi_measure_meter {
	__u32 measure_line;
	__u32 measure_HV_cnt;
	__u32 measure_HB_cnt;
};

struct mtk_cam_seninf_sentest_active_info {
	bool enable;
	__u32 fps;
};

struct mtk_cam_seninf_meter_info {
	struct mipi_measure_meter probes[CSIMAC_MEASURE_MAX_NUM];
	__u32 valid_measure_line;
	__u32 target_vc;
	__u32 target_dt;
	__u32 measure_VV_cnt;
	__u32 measure_VB_cnt;
};

struct mtk_seninf_sentest_ctrl {
	enum seninf_sentest_ctrl_id ctrl_id;
	void *param_ptr;
};

enum seninf_aov_sentest_ctrl_id {
	/* GET CTRL */
	SENINF_AOV_SENTEST_G_CTRL_ID_MIN,
	SENINF_AOV_SENTEST_G_APMCU_UT_RESULT = SENINF_AOV_SENTEST_G_CTRL_ID_MIN,
	SENINF_AOV_SENTEST_G_SCP_UT_RESULT,
	SENINF_AOV_SENTEST_G_APMCU_SCP_UT_RESULT,
	SENINF_AOV_SENTEST_G_CTRL_ID_MAX,

	/* SET CTRL */
	SENINF_AOV_SENTEST_S_CTRL_ID_MIN,
	SENINF_AOV_SENTEST_S_APMCU_UT = SENINF_AOV_SENTEST_S_CTRL_ID_MIN,
	SENINF_AOV_SENTEST_S_SCP_UT,
	SENINF_AOV_SENTEST_S_APMCU_SCP_UT,
	SENINF_AOV_SENTEST_S_CTRL_ID_MAX,
};

struct mtk_seninf_aov_sentest_ctrl {
	enum seninf_aov_sentest_ctrl_id ctrl_id;
	void *param_ptr;
};

struct outmux_debug_result {
	u32 vc_feature;
	u32 tag_id;
	u32 vc;
	u32 dt;
	u32 exp_size_h;
	u32 exp_size_v;
	u32 rec_size_h;
	u32 rec_size_v;
	u32 outmux_id;
	u32 done_irq_status;
	u32 oversize_irq_status;
	u32 incomplete_frame_status;
	u32 ref_vsync_irq_status;
};

struct mtk_seninf_debug_result {
	__u8 valid_result_cnt;
	__u8 csi_port;
	__u8 seninfAsyncIdx;
	__u8 data_lanes;
	__u8 packet_status_err;
	__u32 csi_mac_irq_status;
	__u32 seninf_async_irq;
	__u64 mipi_rate;
	bool is_cphy;
	struct outmux_debug_result outmux_result[MAX_OUTMUX_DEBUG_RESULT];
};

struct mtk_seninf_aov_streaming_info {
	__u32 sensor_idx;
	__u32 enable;
	__u32 aov_mclk_flag;
	__u32 aov_pm_runtime_flag;
	__u32 sensor_on;
	struct mtk_seninf_aov_param aov_param;
};

struct mtk_seninf_aov_status_check {
	__u32 sensor_idx;
	__u32 aov_csi_port;
	__u32 frame_cnt;
	__u32 mclk_reg_val;
	__u32 height;
	__u32 width;
	__u32 ctx_is_null;
};

struct seninf_fakesensor_tm_param {
	u32 clr_value;
	u32 width_tm;
	u32 tm_height;
	u32 vs_diff;
	u32 width_tm_bit;
	u8 c_clk_div_cnt;
	u16 c_dummy_pxl;
	u16 c_dum_vsync;
	u8 tm_core_fmt;
};

/* SET */

#define VIDIOC_MTK_S_SENINF_SENTEST_CTRL \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 50, struct mtk_seninf_sentest_ctrl)

#define VIDIOC_MTK_S_SENINF_AOV_SENTEST_CTRL \
	_IOWR('M', BASE_VIDIOC_PRIVATE + 51, struct mtk_seninf_aov_sentest_ctrl)

#endif  // __MTK_CAM_SENINF_CONTROL_8_H__
