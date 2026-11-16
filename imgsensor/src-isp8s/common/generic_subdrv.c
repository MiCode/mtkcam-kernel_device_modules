// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2024 MediaTek Inc.

#include "adaptor-subdrv-ctrl.h"

static struct subdrv_ops ops = {
	.get_id = common_get_imgsensor_id,
	.init_ctx = common_init_ctx,
	.open = common_open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = common_close,
	.get_frame_desc = common_get_frame_desc,
	.get_temp = common_get_temp,
	.get_csi_param = common_get_csi_param,
	.update_sof_cnt = common_update_sof_cnt,
	.parse_ebd_line = common_parse_ebd_line,
	//.set_ctrl_locker = common_set_ctrl_locker,
};

const struct subdrv_entry generic_subdrv_entry = {
	.ops = &ops,
};

