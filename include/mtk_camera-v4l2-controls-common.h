/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAMERA_V4l2_CONTROLS_COMMON_H
#define __MTK_CAMERA_V4l2_CONTROLS_COMMON_H

#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>

/**
 * C A M E R A  C O M M O N  P A R T
 */

/**
 * I D  B A S E
 */

/**
 * The base for the mediatek camsys driver controls
 * We reserve 48 controls for this driver.
 */
#define V4L2_CID_USER_MTK_CAM_BASE		(V4L2_CID_USER_BASE + 0x10d0)

/**
 * The base for the mediatek sensor driver controls
 * We reserve 48 controls for this driver.
 */
#define V4L2_CID_USER_MTK_SENSOR_1ST_BASE	(V4L2_CID_USER_MTK_CAM_BASE + 0x200)
#define V4L2_CID_USER_MTK_SENSOR_BASE		(V4L2_CID_USER_MTK_CAM_BASE + 0x250)

/**
 * The base for the mediatek seninf driver controls
 * We reserve 48 controls for this driver.
 */
#define V4L2_CID_USER_MTK_SENINF_BASE		(V4L2_CID_USER_MTK_CAM_BASE + 0x300)

/**
 * The base for the mediatek sensor driver commands
 * We reserve ?? controls for this driver.
 */
#define V4L2_CMD_USER_MTK_SENSOR_BASE		(V4L2_CID_USER_MTK_CAM_BASE + 0x400)


/* I M A G E  S E N S O R */

#define V4L2_CID_FRAME_SYNC \
	(V4L2_CID_USER_MTK_SENSOR_1ST_BASE + 1)

#define V4L2_CID_FSYNC_ASYNC_MASTER \
	(V4L2_CID_USER_MTK_SENSOR_1ST_BASE + 2)

#define V4L2_CID_MTK_MSTREAM_MODE \
	(V4L2_CID_USER_MTK_SENSOR_1ST_BASE + 3)

#define V4L2_CID_MTK_N_1_MODE \
	(V4L2_CID_USER_MTK_SENSOR_1ST_BASE + 4)





#define V4L2_CID_MTK_TEMPERATURE \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 1)

#define V4L2_CID_MTK_ANTI_FLICKER \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 2)

#define V4L2_CID_MTK_AWB_GAIN \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 3)

#define V4L2_CID_MTK_SHUTTER_GAIN_SYNC \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 4)

#define V4L2_CID_MTK_DUAL_GAIN \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 5)

#define V4L2_CID_MTK_IHDR_SHUTTER_GAIN \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 6)

#define V4L2_CID_MTK_HDR_SHUTTER \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 7)

#define V4L2_CID_MTK_SHUTTER_FRAME_LENGTH \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 8)

#define V4L2_CID_MTK_PDFOCUS_AREA \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 9)

#define V4L2_CID_MTK_HDR_ATR \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 10)

#define V4L2_CID_MTK_HDR_TRI_SHUTTER \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 11)

#define V4L2_CID_MTK_HDR_TRI_GAIN \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 12)

#define V4L2_CID_MTK_MAX_FPS \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 13)

#define V4L2_CID_MTK_STAGGER_AE_CTRL \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 14)

#define V4L2_CID_SEAMLESS_SCENARIOS \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 15)

#define V4L2_CID_MTK_STAGGER_INFO \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 16)

#define V4L2_CID_STAGGER_TARGET_SCENARIO \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 17)

#define V4L2_CID_START_SEAMLESS_SWITCH \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 18)

#define V4L2_CID_MTK_DEBUG_CMD \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 19)

#define V4L2_CID_MAX_EXP_TIME \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 20)

#define V4L2_CID_MTK_SENSOR_PIXEL_RATE \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 21)

#define V4L2_CID_MTK_FRAME_DESC \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 22)

#define V4L2_CID_MTK_SENSOR_STATIC_PARAM \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 23)

#define V4L2_CID_MTK_SENSOR_POWER \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 24)

#define V4L2_CID_MTK_CUST_SENSOR_PIXEL_RATE \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 25)

#define V4L2_CID_MTK_CSI_PARAM \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 26)

#define V4L2_CID_MTK_SENSOR_TEST_PATTERN_DATA \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 27)

#define V4L2_CID_MTK_SENSOR_RESET \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 28)

#define V4L2_CID_MTK_SENSOR_INIT \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 29)

#define V4L2_CID_MTK_SOF_TIMEOUT_VALUE \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 30)

#define V4L2_CID_MTK_SENSOR_RESET_S_STREAM \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 31)

#define V4L2_CID_MTK_SENSOR_RESET_BY_USER \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 32)

#define V4L2_CID_MTK_SENSOR_IDX \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 33)

#define V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SCL_AUX \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 34)

#define V4L2_CID_MTK_AOV_SWITCH_I2C_BUS_SDA_AUX \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 35)

#define V4L2_CID_MTK_AOV_SWITCH_RX_PARAM \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 36)

#define V4L2_CID_MTK_AOV_SWITCH_PM_OPS \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 37)

#define V4L2_CID_MTK_AOV_SWITCH_MCLK_ULPOSC \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 38)

#define V4L2_CID_MTK_DO_NOT_POWER_ON \
	(V4L2_CID_USER_MTK_SENSOR_BASE + 39)

/**
 * enum scl for imgsensor gpio aux function.
 */
enum mtk_cam_sensor_i2c_bus_scl {
	SCL0 = 0,
	SCL1,
	SCL2,
	SCL3,
	SCL4,
	SCL5,
	SCL6,
	SCL7,
	SCL8,
	SCL9,
	SCL10,
	SCL11,
	SCL12,
	SCL13,
	SCL_MAXCNT,
	SCL_ERR = 0xffff,
};

/**
 * enum sda for imgsensor gpio aux function.
 */
enum mtk_cam_sensor_i2c_bus_sda {
	SDA0 = 0,
	SDA1,
	SDA2,
	SDA3,
	SDA4,
	SDA5,
	SDA6,
	SDA7,
	SDA8,
	SDA9,
	SDA10,
	SDA11,
	SDA12,
	SDA13,
	SDA_MAXCNT,
	SDA_ERR = 0xffff,
};

enum stream_mode {
	NORMAL_CAMERA = 0,
	AOV_REAL_SENSOR,
	AOV_TEST_MODEL,
};

struct mtk_seninf_s_stream {
	enum stream_mode stream_mode;
	int enable;
};

enum mtk_cam_seninf_csi_clk_for_param {
	CSI_CLK_52 = 0,
	CSI_CLK_65,
	CSI_CLK_104,
	CSI_CLK_130,
	CSI_CLK_242,
	CSI_CLK_260,
	CSI_CLK_312,
	CSI_CLK_416,
	CSI_CLK_499,
};

enum mtk_cam_sensor_pm_ops {
	AOV_PM_RELAX = 0,
	AOV_PM_STAY_AWAKE,
	AOV_ABNORMAL_FORCE_SENSOR_PWR_OFF,
	AOV_ABNORMAL_FORCE_SENSOR_PWR_ON,
};

struct mtk_seninf_lbmf_info {
	u32 scenario;
	bool is_lbmf;
};


/* imgsensor commands */
#define V4L2_CMD_FSYNC_SYNC_FRAME_START_END \
	(V4L2_CMD_USER_MTK_SENSOR_BASE + 1)

#define V4L2_CMD_GET_SENSOR_MODE_CONFIG_INFO \
	(V4L2_CMD_USER_MTK_SENSOR_BASE + 2)

#define V4L2_CMD_SENSOR_IN_RESET \
	(V4L2_CMD_USER_MTK_SENSOR_BASE + 3)

#define V4L2_CMD_TSREC_NOTIFY_SENSOR_HW_PRE_LATCH \
	(V4L2_CMD_USER_MTK_SENSOR_BASE + 4)

#define V4L2_CMD_TSREC_NOTIFY_VSYNC \
	(V4L2_CMD_USER_MTK_SENSOR_BASE + 5)

#define V4L2_CMD_TSREC_SEND_TIMESTAMP_INFO \
	(V4L2_CMD_USER_MTK_SENSOR_BASE + 6)

#define V4L2_CMD_GET_SEND_SENSOR_MODE_CONFIG_INFO \
	(V4L2_CMD_USER_MTK_SENSOR_BASE + 7)

#define V4L2_CMD_SENSOR_HAS_EBD_PARSER \
	(V4L2_CMD_USER_MTK_SENSOR_BASE + 8)

#define V4L2_CMD_SENSOR_PARSE_EBD \
	(V4L2_CMD_USER_MTK_SENSOR_BASE + 9)

#define V4L2_CMD_GET_SENSOR_EBD_INFO_BY_SCENARIO \
	(V4L2_CMD_USER_MTK_SENSOR_BASE + 10)

#define V4L2_CMD_TSREC_SETUP_CB_FUNC_OF_SENSOR \
	(V4L2_CMD_USER_MTK_SENSOR_BASE + 11)

#define V4L2_CMD_SENSOR_GET_LBMF_TYPE_BY_SCENARIO \
	(V4L2_CMD_USER_MTK_SENSOR_BASE + 12)

/**
 * TSREC - notify vsync structure
 *         V4L2_CMD_TSREC_NOTIFY_VSYNC
 */
struct mtk_cam_seninf_tsrec_vsync_info {
	/* source info */
	__u32 tsrec_no;
	__u32 seninf_idx;

	__u64 irq_sys_time_ns; // ktime_get_boottime_ns()
	__u64 irq_tsrec_ts_us;
};


/**
 * TSREC - timestamp info structure / define
 *         V4L2_CMD_TSREC_SEND_TIMESTAMP_INFO
 */
#define TSREC_TS_REC_MAX_CNT (4)
#define TSREC_EXP_MAX_CNT    (3)

struct mtk_cam_seninf_tsrec_timestamp_exp {
	__u64 ts_us[TSREC_TS_REC_MAX_CNT];
};

struct mtk_cam_seninf_tsrec_timestamp_info {
	/* source info */
	__u32 tsrec_no;
	__u32 seninf_idx;

	/* basic info */
	__u32 tick_factor; // MHz

	/* interrupt pre-latch exp no */
	__u32 irq_pre_latch_exp_no;
	/* record when receive a interrupt (top-half) */
	__u64 irq_sys_time_ns; // ktime_get_boottime_ns()
	__u64 irq_tsrec_ts_us;

	/* current tick when query/send tsrec timestamp info */
	__u64 tsrec_curr_tick;
	struct mtk_cam_seninf_tsrec_timestamp_exp exp_recs[TSREC_EXP_MAX_CNT];
};


/**
 * TSREC - callback info
 */
enum tsrec_cb_cmd {
	/* user get tsrec information */
	TSREC_CB_CMD_READ_CURR_TS,
	TSREC_CB_CMD_READ_TS_INFO,
};

enum tsrec_cb_ctrl_error_type {
	TSREC_CB_CTRL_ERR_NONE = 0,
	TSREC_CB_CTRL_ERR_INVALID,
	TSREC_CB_CTRL_ERR_NOT_CONNECTED_TO_TSREC,
	TSREC_CB_CTRL_ERR_CB_FUNC_PTR_NULL,
	TSREC_CB_CTRL_ERR_CMD_NOT_FOUND,
	TSREC_CB_CTRL_ERR_CMD_ARG_PTR_NULL,
	TSREC_CB_CTRL_ERR_CMD_IN_SENINF_SUSPEND,
};

/* call back function prototype, see mtk_cam-seninf-tsrec.c */
typedef int (*tsrec_cb_handler_func_ptr)(const unsigned int seninf_idx,
	const unsigned int tsrec_no, const unsigned int cmd, void *arg,
	const char *caller);

struct mtk_cam_seninf_tsrec_cb_info {
	/* check this sensor is => 1: with TSREC; 0: NOT with TSREC */
	__u32 is_connected_to_tsrec;

	/* !!! below data are valid ONLY when "is_connected_to_tsrec != 0" !!! */
	__u32 seninf_idx;
	__u32 tsrec_no;
	tsrec_cb_handler_func_ptr tsrec_cb_handler;
};


/* S E N I N F */
#define V4L2_CID_MTK_SENINF_S_STREAM \
		(V4L2_CID_USER_MTK_SENINF_BASE + 1)

#define V4L2_CID_FSYNC_MAP_ID \
	(V4L2_CID_USER_MTK_SENINF_BASE + 2)

#define V4L2_CID_VSYNC_NOTIFY \
	(V4L2_CID_USER_MTK_SENINF_BASE + 3)

#define V4L2_CID_TEST_PATTERN_FOR_AOV_PARAM \
	(V4L2_CID_USER_MTK_SENINF_BASE + 4)

#define V4L2_CID_FSYNC_LISTEN_TARGET \
	(V4L2_CID_USER_MTK_SENINF_BASE + 5)

#define V4L2_CID_REAL_SENSOR_FOR_AOV_PARAM \
	(V4L2_CID_USER_MTK_SENINF_BASE + 6)

#define V4L2_CID_UPDATE_SOF_CNT \
	(V4L2_CID_USER_MTK_SENINF_BASE + 7)

/* C A M S Y S */

#define V4L2_MBUS_FRAMEFMT_PAD_ENABLE  BIT(1)

#define MEDIA_BUS_FMT_MTISP_SBGGR10_1X10		0x8001
#define MEDIA_BUS_FMT_MTISP_SBGGR12_1X12		0x8002
#define MEDIA_BUS_FMT_MTISP_SBGGR14_1X14		0x8003
#define MEDIA_BUS_FMT_MTISP_SGBRG10_1X10		0x8004
#define MEDIA_BUS_FMT_MTISP_SGBRG12_1X12		0x8005
#define MEDIA_BUS_FMT_MTISP_SGBRG14_1X14		0x8006
#define MEDIA_BUS_FMT_MTISP_SGRBG10_1X10		0x8007
#define MEDIA_BUS_FMT_MTISP_SGRBG12_1X12		0x8008
#define MEDIA_BUS_FMT_MTISP_SGRBG14_1X14		0x8009
#define MEDIA_BUS_FMT_MTISP_SRGGB10_1X10		0x800a
#define MEDIA_BUS_FMT_MTISP_SRGGB12_1X12		0x800b
#define MEDIA_BUS_FMT_MTISP_SRGGB14_1X14		0x800c
#define MEDIA_BUS_FMT_MTISP_BAYER8_UFBC			0x800d
#define MEDIA_BUS_FMT_MTISP_BAYER10_UFBC		0x800e
#define MEDIA_BUS_FMT_MTISP_BAYER12_UFBC		0x8010
#define MEDIA_BUS_FMT_MTISP_BAYER14_UFBC		0x8011
#define MEDIA_BUS_FMT_MTISP_BAYER16_UFBC		0x8012
#define MEDIA_BUS_FMT_MTISP_NV12			0x8013
#define MEDIA_BUS_FMT_MTISP_NV21			0x8014
#define MEDIA_BUS_FMT_MTISP_NV12_10			0x8015
#define MEDIA_BUS_FMT_MTISP_NV21_10			0x8016
#define MEDIA_BUS_FMT_MTISP_NV12_10P			0x8017
#define MEDIA_BUS_FMT_MTISP_NV21_10P			0x8018
#define MEDIA_BUS_FMT_MTISP_NV12_12			0x8019
#define MEDIA_BUS_FMT_MTISP_NV21_12			0x801a
#define MEDIA_BUS_FMT_MTISP_NV12_12P			0x801b
#define MEDIA_BUS_FMT_MTISP_NV21_12P			0x801c
#define MEDIA_BUS_FMT_MTISP_YUV420			0x801d
#define MEDIA_BUS_FMT_MTISP_NV12_UFBC			0x801e
#define MEDIA_BUS_FMT_MTISP_NV21_UFBC			0x8020
#define MEDIA_BUS_FMT_MTISP_NV12_10_UFBC		0x8021
#define MEDIA_BUS_FMT_MTISP_NV21_10_UFBC		0x8022
#define MEDIA_BUS_FMT_MTISP_NV12_12_UFBC		0x8023
#define MEDIA_BUS_FMT_MTISP_NV21_12_UFBC		0x8024
#define MEDIA_BUS_FMT_MTISP_NV16			0x8025
#define MEDIA_BUS_FMT_MTISP_NV61			0x8026
#define MEDIA_BUS_FMT_MTISP_NV16_10			0x8027
#define MEDIA_BUS_FMT_MTISP_NV61_10			0x8028
#define MEDIA_BUS_FMT_MTISP_NV16_10P			0x8029
#define MEDIA_BUS_FMT_MTISP_NV61_10P			0x802a

#define MEDIA_BUS_FMT_MTISP_SBGGR22_1X22		0x8100
#define MEDIA_BUS_FMT_MTISP_SGBRG22_1X22		0x8101
#define MEDIA_BUS_FMT_MTISP_SGRBG22_1X22		0x8102
#define MEDIA_BUS_FMT_MTISP_SRGGB22_1X22		0x8103


/* I M G S Y S */

#endif /* __MTK_CAMERA_V4l2_CONTROLS_COMMON_H */
