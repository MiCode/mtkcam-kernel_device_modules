/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CAM_TOP_CTRL_H
#define __MTK_CAM_TOP_CTRL_H

/* cam-main: urgent top enable */
void mtk_cam_main_halt(struct mtk_raw_device *raw, int is_srt);
void mtk_cam_main_sv_halt(struct mtk_cam_device *cam);
void mtk_cam_main_dbg_dump(struct mtk_cam_device *cam);

/* cam-vcore : 16 level qos */
void mtk_cam_vcore_qos_remap(struct mtk_raw_device *raw, int is_srt);
void mtk_cam_vcore_sv_qos_remap(struct mtk_cam_device *cam);
void mtk_cam_vcore_ccu_qos_remap(struct mtk_cam_device *cam);

/* cam-vcore: ddren/coh_req/wla2.0 */
void mtk_cam_vcore_ddren(struct mtk_cam_device *cam, int on_off);
void mtk_cam_vcore_coh_req(struct mtk_cam_device *cam);
void mtk_cam_vcore_wla20(struct mtk_cam_device *cam, int on_off);

void mtk_cam_vcore_wla20_dbg_dump(struct mtk_cam_device *cam);

#endif /*__MTK_CAM_TOP_CTRL_H*/
