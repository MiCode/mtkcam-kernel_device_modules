/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 MediaTek Inc. */

#ifndef __ADAPTOR_SENTEST_H__
#define __ADAPTOR_SENTEST_H__

#include <linux/mutex.h>

/******************************************************************************/
// adaptor sentest  --- structure define
/******************************************************************************/
#define SENTEST_TSREC_TS_REC_MAX_CNT (4)
#define SENTEST_TSREC_EXP_MAX_CNT    (3)

struct mtk_cam_seninf_ts_exp {
	u64 ts_us[SENTEST_TSREC_TS_REC_MAX_CNT];
};

struct mtk_cam_seninf_sentest_ts {
	struct mtk_cam_seninf_ts_exp exp_recs[SENTEST_TSREC_EXP_MAX_CNT];
	u64 sys_time_ns;
	u64 sentest_tsrec_frame_cnt;
};

struct mtk_cam_sentest_cfg_info {
	bool lbmf_delay_do_ae_en;
	bool power_on_profile_en;
	u32 listen_tsrec_frame_id;
	struct mtk_cam_seninf_sentest_ts ts;
	struct mutex sentest_update_tsrec_mutex;
};

#endif
