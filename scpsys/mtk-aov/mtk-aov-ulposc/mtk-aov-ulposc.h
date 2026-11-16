/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef MTK_AOV_ULPOSC_H
#define MTK_AOV_ULPOSC_H

#include <linux/regmap.h>

#define KHZ_TO_MHZ(freq)	(freq / 1000)
#define CALI_MIS_RATE	(2)
#define ULPOSC_CALI_CONFIG_NUM 3 // CON0/CON1/CON2
#define CAL_FACTOR1_BITS	(2)
#define CAL_FACTOR2_BITS	(7)

enum ULPOSC_CONFIG_TYPE {
	PRE_CONFIG = 0,
	CALI_CONFIG = 1,
	INIT_CONFIG = 2,
	ULPOSC_CONFIG_MAX,
};

enum ULPOSC_FMETER_TYPE {
	CALI_FMETER = 0,
	RESULT_FMETER = 1,
	ULPOSC_FMETER_MAX,
};

struct aov_ulposc_info {
	bool ulposc_support;
	bool ulposc_cali_done;
	unsigned int ulposc_cali_config_setting[ULPOSC_CALI_CONFIG_NUM];
	unsigned int ulposc_cali_result[ULPOSC_CALI_CONFIG_NUM];
	unsigned int ulposc_cali_target;
	unsigned int ulposc_cali_result_target;
	unsigned int ulposc_cali_fmeter_id;
	unsigned int ulposc_result_fmeter_id;
	void __iomem *base;
	const char *ver;
};

int aov_ulposc_cali(struct mtk_aov *aov_dev);
void aov_ulposc_dts_init(struct mtk_aov *aov_dev);
int aov_ulposc_check_cali_result(struct mtk_aov *aov_dev);
#endif /* MTK_AOV_ULPOSC_H */
