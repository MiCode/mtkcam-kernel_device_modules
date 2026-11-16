/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MTK_AOV_ULPOSC_HW_H
#define MTK_AOV_ULPOSC_HW_H

struct mtk_aov_ulposc_ops {
	void (*_init)(struct mtk_aov *aov_dev);
	int (*_set_config)(struct mtk_aov *aov_dev, int type);
	void (*_set_cali_factor1_value)(struct mtk_aov *aov_dev, int value);
	void (*_set_cali_factor2_value)(struct mtk_aov *aov_dev, int value);
	int (*_get_fmeter_result)(struct mtk_aov *aov_dev, int type);
	void (*_set_cali_result)(struct mtk_aov *aov_dev);
};

extern struct mtk_aov_ulposc_ops ulposc_v1;
extern struct mtk_aov_ulposc_ops ulposc_v2;

#endif /* MTK_AOV_ULPOSC_HW_H */
