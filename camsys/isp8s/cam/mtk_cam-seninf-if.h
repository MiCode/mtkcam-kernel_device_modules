/* SPDX-License-Identifier: GPL-2.0 */
// Copyright (c) 2019 MediaTek Inc.

#ifndef __MTK_CAM_SENINF_IF_H__
#define __MTK_CAM_SENINF_IF_H__

#include <media/v4l2-ctrls.h>
#include "imgsensor-krn_user.h"

/* ISP8 new API */

enum pix_mode {
	pix_mode_1p = 0,
	pix_mode_2p = 1,
	pix_mode_4p = 2,
	pix_mode_8p = 3,
	pix_mode_16p = 4,
};

/**
 * struct of camtg config setting
 *
 * @pad_id: source pad id of the seninf subdev, to indicate the image
 *          processing engine to be conncted
 * @tag_id: the tag of connected data to be
 */
struct mtk_cam_seninf_camtg_param {
	int pad_id;
	int tag_id;
};

/**
 * set camtg pixel mode
 *
 * @sd: sensor interface's V4L2 subdev
 * @camtg: physical image processing engine's id (e.g. raw's connect outmux id)
 * @pixmode: the pixel mode
 */
int mtk_cam_seninf_set_camtg_pixmode(struct v4l2_subdev *sd, int camtg,
				 int pixmode);

/**
 * get camtg pixel mode
 *
 * @sd: sensor interface's V4L2 subdev
 * @camtg: physical image processing engine's id (e.g. raw's connect outmux id)
 * @pixmode: the returned pixel mode
 */
int mtk_cam_seninf_get_camtg_pixmode(struct v4l2_subdev *sd, int camtg,
				 int *pixmode);

/**
 * enum for isp-raw receiving multi raw set
 */
enum seninf_recv_raw_set {
	MTK_SENINF_RAW_SET1 = 0,
	MTK_SENINF_RAW_SET2,
	MTK_SENINF_RAW_SET3,
	MTK_MAX_SENINF_RAW_SET_NUM,
};

/////

/* @Deprecated */
int mtk_cam_seninf_get_pixelmode(struct v4l2_subdev *sd, int pad_id,
				 int *pixelmode);

/* @Deprecated */
int mtk_cam_seninf_set_pixelmode(struct v4l2_subdev *sd, int pad_id,
				 int pixelmode);

/* @Deprecated */
int mtk_cam_seninf_set_pixelmode_camsv(struct v4l2_subdev *sd, int pad_id,
				 int pixelMode, int camtg);

int mtk_cam_seninf_get_sensor_usage(struct v4l2_subdev *sd);

int mtk_cam_seninf_is_non_comb_ic(struct v4l2_subdev *sd);

//////

int mtk_cam_seninf_get_pixelrate(struct v4l2_subdev *sd, s64 *pixelrate);

int mtk_cam_seninf_calc_pixelrate(struct device *dev, s64 width, s64 height, s64 hblank,
				  s64 vblank, int fps_n, int fps_d, s64 sensor_pixel_rate);

int mtk_cam_seninf_dump(struct v4l2_subdev *sd, u32 seq_id, bool force_check,
			bool assert_when_error);
int mtk_cam_seninf_get_csi_irq_status(struct v4l2_subdev *sd, struct v4l2_ctrl *ctrl);

int mtk_cam_seninf_dump_current_status(struct v4l2_subdev *sd, bool assert_when_error);

int mtk_cam_seninf_set_abort(struct v4l2_subdev *sd);

int mtk_cam_seninf_check_timeout(struct v4l2_subdev *sd, u64 time_waited);
u64 mtk_cam_seninf_get_frame_time(struct v4l2_subdev *sd, u32 seq_id);

int mtk_cam_seninf_get_tag_order(struct v4l2_subdev *sd, __u32 fmt_code, int pad_id);
int mtk_cam_seninf_get_vsync_order(struct v4l2_subdev *sd);

struct mtk_cam_seninf_sentest_param {
	bool is_lbmf;
};

int mtk_cam_seninf_get_sentest_param(struct v4l2_subdev *sd, __u32 fmt_code,
	struct mtk_cam_seninf_sentest_param *param);

/**
 * struct mtk_cam_seninf_mux_setting - mux setting change setting
 * @seninf: sensor interface's V4L2 subdev
 * @source: source pad id of the seninf subdev, to indicate the image
 *          processing engine to be conncted
 * @camtg: physical image processing engine's id (e.g. raw's device id)
 * @pixmode: the pixel mode
 */
struct mtk_cam_seninf_mux_setting {
	struct v4l2_subdev *seninf;
	int source;
	int camtg;
	int enable;
	union {
		int tag_id;
		enum seninf_recv_raw_set raw_set;
	};
	int pixelmode;
};


/**
 * struct mtk_cam_seninf_rdy_mask_en - mux setting change setting rdy mask setting
 * @rdy_sw_en: sw ready enable bit for whole ready mask group
 * @rdy_cq_en: cq ready enable bit for whole ready mask group
 * @rdy_grp_en: setting for putting all out mux in ready grp control enable
 *
 * This structure is adapt for controlling out mux ready mask function on or off
 * when changing the out mux
 */
struct mtk_cam_seninf_rdy_mask_en {
	bool rdy_sw_en;
	bool rdy_cq_en;
	bool rdy_grp_en;
};

/**
 * typedef mtk_cam_seninf_mux_change_done_fnc - mux change fininshed callback
 *
 * @private: Private data passed from mtk_cam_seninf_streaming_mux_change.
 *           In general, it is the request object indicate the mux changes.
 *
 * Returns true if the mux changes are all applied.
 */
typedef bool (*mtk_cam_seninf_mux_change_done_fn)(void *private);

/**
 * struct mtk_cam_seninf_mux_param - mux setting change parameters
 * @settings: per mux settings
 * @num: number of params
 * @fnc: callback function when seninf driver finihsed all the mux changes.
 * @private: Private data of the caller. The private data will be return from
 *           seninf driver through mtk_cam_seninf_mux_change_done_fnc callback.
 */
struct mtk_cam_seninf_mux_param {
	struct mtk_cam_seninf_mux_setting *settings;
	struct mtk_cam_seninf_rdy_mask_en rdy_mask_en;
	mtk_cam_seninf_mux_change_done_fn func;
	int num;
	void *private;
};


/**
 * struct mtk_cam_seninf_mux_setup - changing the out mux connection
 * when setting up out mux
 * @sd: V4L2 subdev of seninf
 * @param: a new connection from sensor interface to image processing engine
 *
 * To be called when camsys driver need to change the connection from sensor
 * interface to image processing engine during streaming. It is a asynchronized
 * call, the sensor interface driver will call back func to notify the caller.
 *
 * Returns true if the mux changes will be applied.
 */
bool mtk_cam_seninf_mux_setup(struct v4l2_subdev *sd,
			struct mtk_cam_seninf_mux_param *param);


/**
 * struct mtk_cam_seninf_set_mux_sw_rdy - changing the sw ready bit status
 * @sd: V4L2 subdev of seninf
 * @camtg: a camtg id for the ready mask group that user
 *         intend to control its sw ready bit
 * @sw_rdy_status: the sw ready bit status to apply on the specified camtg group
 * @is_defer_by_tsrec: whether the sw ready bit status is defer by tsrec
 *
 * To be called when camsys driver need to change the connection from sensor
 * interface to image processing engine. It is a asynchronized
 * call, the sensor interface driver will call back func to notify the caller.
 *
 * Returns true if the mux changes will be applied.
 */
bool mtk_cam_seninf_set_mux_sw_rdy(struct v4l2_subdev *sd,
			u8 camtg, bool sw_rdy_status, bool is_defered_by_tsrec);

/**
 * struct mtk_cam_seninf_set_mux_cq_en - changing cq ready function enable control
 * @sd: V4L2 subdev of seninf
 * @camtg: a camtg id for the ready mask group that user
 *         intend to control its sw ready bit
 * @cq_rdy_en: the cq ready enable to apply on the specified camtg group
 *
 * To be called when camsys driver need to change the connection from sensor
 * interface to image processing engine. It is a asynchronized
 * call, the sensor interface driver will call back func to notify the caller.
 *
 * Returns true if the mux changes will be applied.
 */
bool mtk_cam_seninf_set_mux_cq_en(struct v4l2_subdev *sd,
			u8 camtg, bool cq_rdy_en);


/**
 * struct mtk_cam_seninf_force_disable_out_mux - reset the unsed out mux directly
 * @sd: V4L2 subdev of seninf
 *
 * This function is adopt for disable the unsed outmux when the scenario such as
 * seamless switch / raw switch for such some out mux may be not to be used
 */
bool mtk_cam_seninf_force_disable_out_mux(struct v4l2_subdev *sd);


struct mtk_seninf_sof_notify_param {
	struct v4l2_subdev *sd;
	unsigned int sof_cnt;
	u64 sof_ts;
};

void
mtk_cam_seninf_sof_notify(struct mtk_seninf_sof_notify_param *param);

/**
 * struct mtk_seninf_pad_data_info - data information outputed by pad
 */
struct mtk_seninf_pad_data_info {
	u8 feature;
	u16 exp_hsize;
	u16 exp_vsize;
	u32 mbus_code;
};

/**
 * Get data info by seninf pad
 *
 * @param sd v4l2_subdev
 * @param pad The pad id
 * @param result The result
 * @return 0 if success, and negative number if error occur
 */
int mtk_cam_seninf_get_pad_data_info(struct v4l2_subdev *sd,
				unsigned int pad,
				struct mtk_seninf_pad_data_info *result);

/**
 * Get embedded line info by mtk_mbus_code (with sensor mode info)
 *
 * @param sd v4l2_subdev
 * @param scenario_mbus_code The mbus code with mtk sensor mode
 * @param result The result
 * @return 0 if success, and negative number if error occur
 */
int mtk_cam_seninf_get_ebd_info_by_scenario(struct v4l2_subdev *sd,
				u32 scenario_mbus_code,
				struct mtk_seninf_pad_data_info *result);

/**
 * struct mtk_seninf_active_line_info - active line information
 */
struct mtk_seninf_active_line_info {
	u32 active_line_num;
	u64 avg_linetime_in_ns;
};

/**
 * Get active line info
 *
 * @param sd v4l2_subdev
 * @param result The result
 * @return 0 if success, and negative number if error occur
 */
int mtk_cam_seninf_get_active_line_info(struct v4l2_subdev *sd,
				unsigned int mbus_code,
				struct mtk_seninf_active_line_info *result);


/**
 * struct mtk_seninf_sensor_linetime_list - DCG+VS lut linetime list
 * @hdr_mode: HDR_RAW_DCG_RAW_VS: ap merge moded,
 *            HDR_RAW_DCG_COMPOSE_VS: sensor merged mode,
 *            others (non-DCG_VS)
 * @linetime_cnt: array count of lut_read_linetimes_in_ns
 * @lut_read_linetimes_in_ns: linetime list in ns
 */
struct mtk_seninf_sensor_linetime_list {
	unsigned int hdr_mode;
	unsigned int linetime_cnt;
	unsigned int lut_read_linetimes_in_ns[IMGSENSOR_LUT_MAXCNT];
};

/**
 * Get linetime_list by lut order (DCG VS used)
 *
 * @param sd v4l2_subdev
 * @param mbus_code Info of queried sensor mode id
 * @param result The result
 * @return 0 if success, and negative number if error occur
 */
int mtk_seninf_g_read_linetime_list(struct v4l2_subdev *sd,
				unsigned int mbus_code, struct mtk_seninf_sensor_linetime_list *result);

void
mtk_cam_seninf_set_secure(struct v4l2_subdev *sd, int enable, u64 SecInfo_addr);

bool is_fsync_listening_on_pd(struct v4l2_subdev *sd);

/**
 * Has embedded data line parser implemented
 *
 * @param sd v4l2_subdev
 * @return 1 if parser implemented
 */
bool has_embedded_parser(struct v4l2_subdev *sd);

/**
 * Callback function for parsing embedded data line
 *
 * @param sd v4l2_subdev
 * @param req_id Request id
 * @param req_fd_desc Request fd description
 * @param buf Buffer of received data
 * @param buf_sz Size of buffer of received data
 * @param stride the stride of buffer
 * @param scenario_mbus_code The mbus code with mtk sensor mode
 */
void mtk_cam_seninf_parse_ebd_line(struct v4l2_subdev *sd,
				unsigned int req_id,
				char *req_fd_desc,
				char *buf, u32 buf_sz,
				u32 stride, u32 scenario_mbus_code);
/**
 * do sensor prolong
 *
 * @param sd v4l2_subdev
 * @param action prolong type
 */
void notify_sensor_set_fl_prolong(struct v4l2_subdev *sd,
	unsigned int action);

/**
 * start or stop seninf streaming
 *
 * @param sd v4l2_subdev
 * @param enable start or stop
 */
int seninf_s_stream(struct v4l2_subdev *sd, int enable);



/**
 * struct for mtk_cam_seninf_frame_event_notify
 *
 * @param sensor_sequence sensor sequence number from middleware event
 * @param sensor_sync_id  sensor sync id number from middleware event
 * @param fs_anchor_ns  sensor anchor offset info in ns as unit
 */
struct mtk_seninf_frame_event_info {
	u32 sensor_sequence;
	u32 sensor_sync_id;
	long long fs_anchor_ns;
};
/**
 * Notify event frame sync info to seninf
 *
 * @param sd v4l2_subdev
 * @param info mtk_seninf_pad_data_info
 */
void mtk_cam_seninf_frame_event_notify(struct v4l2_subdev *sd,
	struct mtk_seninf_frame_event_info *info);

/**
 * Temp API for HP9 signal interference used,
 * sof delay only enable when HP9 signal interference case
 *
 * @param sd v4l2_subdev
 */
bool mtk_cam_seninf_is_sof_delay_enabled(struct v4l2_subdev *sd);

/**
 * Notify seninf to start test model for stress test
 *
 * @param sd v4l2_subdev
 * @param mode test model mode
 */
void mtk_cam_seninf_start_test_model_for_camsv(struct v4l2_subdev *sd, u32 mode);

/**
 * Notify seninf to stoptest model for stress test
 *
 * @param sd v4l2_subdev
 */
void mtk_cam_seninf_stop_test_model_for_camsv(struct v4l2_subdev *sd);

#endif
