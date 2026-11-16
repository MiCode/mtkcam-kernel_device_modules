// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>

#include "mtk-hcp_log.h"

int hcp_dbg_en = HCP_LOG_LVL_INF;
module_param(hcp_dbg_en, int, 0644);


