// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 MediaTek Inc.
 *
 * Author: ChenHung Yang <chenhung.yang@mediatek.com>
 */

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/io.h>
#include <clk-fmeter.h>

#include "mtk-aov-drv.h"
#include "mtk-aov-ulposc.h"
#include "mtk-aov-ulposc-hw.h"
#include "v1/mtk-aov-vlp_osc.h"
#include "mtk-aov-core.h"
#include "mtk-aov-regs.h"
#include "mtk-aov-log.h"

#define ULPOSC_CALI_TARGET 260 // unit: MHz
#define ULPOSC_RESULT_TARGET 26 // unit: MHz
#define ULPOSC_CALI_FMETER_ID 50 // AD_OSC3_SYNC_CK
#define ULPOSC_RESULT_FMETER_ID 63 // AD_OSC3_CK

static unsigned int ulposc_cali_config[ULPOSC_CALI_CONFIG_NUM] = {
	0x0d233340, 0x10000800, 0x0000484a
};
static unsigned int ulposc_cali_result[ULPOSC_CALI_CONFIG_NUM] = {
	0x00000000, 0x42000800, 0x0000484a
};

static void aov_ulposc_init(struct mtk_aov *aov_dev)
{
	struct aov_ulposc_info *info = &(aov_dev->ulposc_info);
	int i = 0;

	info->ulposc_cali_target = ULPOSC_CALI_TARGET;
	info->ulposc_cali_result_target = ULPOSC_RESULT_TARGET;
	info->ulposc_cali_fmeter_id = ULPOSC_CALI_FMETER_ID;
	info->ulposc_result_fmeter_id = ULPOSC_RESULT_FMETER_ID;
	for (i = 0; i < ULPOSC_CALI_CONFIG_NUM; i++) {
		info->ulposc_cali_config_setting[i] = ulposc_cali_config[i];
		info->ulposc_cali_result[i] = ulposc_cali_result[i];
	}
}

static int aov_ulposc_set_config(struct mtk_aov *aov_dev, int type)
{
	struct aov_ulposc_info *info = &(aov_dev->ulposc_info);
	void *base = info->base;

	switch (type) {
	case PRE_CONFIG:
		AOV_WRITE_REG(base, VLP_ULPOSC3_CON0, info->ulposc_cali_config_setting[0]);
		AOV_WRITE_REG(base, VLP_ULPOSC3_CON1, info->ulposc_cali_config_setting[1]);
		AOV_WRITE_REG(base, VLP_ULPOSC3_CON2, info->ulposc_cali_config_setting[2]);
		break;
	case CALI_CONFIG:
		AOV_WRITE_REG(base, VLP_ULPOSC3_CON0, info->ulposc_cali_result[0]);
		AOV_WRITE_REG(base, VLP_ULPOSC3_CON1, info->ulposc_cali_result[1]);
		AOV_WRITE_REG(base, VLP_ULPOSC3_CON2, info->ulposc_cali_result[2]);
		dev_info(aov_dev->dev,"[%s] cali_result:0x%x\n", __func__, AOV_READ_REG(base, VLP_ULPOSC3_CON0));
		break;
	default:
		break;
	};

	return 0;
}

static void aov_ulposc_set_cali_factor1_value(struct mtk_aov *aov_dev, int value)
{
	struct aov_ulposc_info *info = &(aov_dev->ulposc_info);
	void *base = info->base;
	unsigned int cali, mask = (0x1 << CAL_FACTOR1_BITS) - 1;

	cali = AOV_READ_BITS(base, VLP_ULPOSC3_CON0, RG_OSC3_CALI);
	cali &= ~(mask << CAL_FACTOR2_BITS); // clear bits
	cali |= ((value & mask) << CAL_FACTOR2_BITS); // set value
	AOV_BITS(base, VLP_ULPOSC3_CON0, RG_OSC3_CALI, cali);
}

static void aov_ulposc_set_cali_factor2_value(struct mtk_aov *aov_dev, int value)
{
	struct aov_ulposc_info *info = &(aov_dev->ulposc_info);
	void *base = info->base;
	unsigned int cali, mask = (0x1 << CAL_FACTOR2_BITS) - 1;

	cali = AOV_READ_BITS(base, VLP_ULPOSC3_CON0, RG_OSC3_CALI);
	cali &= ~mask; // clear bits
	cali |= (value & mask); // set value
	AOV_BITS(base, VLP_ULPOSC3_CON0, RG_OSC3_CALI, cali);
}

static int aov_ulposc_get_fmeter_result(struct mtk_aov *aov_dev, int type)
{
	struct aov_ulposc_info *info = &(aov_dev->ulposc_info);
	unsigned int freq = 0;

	switch (type) {
	case CALI_FMETER:
		freq = mt_get_fmeter_freq(info->ulposc_cali_fmeter_id, VLPCK);
		if (freq == 0) {
			/* freq is not expected to be 0 */
			pr_notice("[%s]: freq %d, pls check CCF configs\n", __func__, freq);
		}
		break;
	case RESULT_FMETER:
		freq = mt_get_fmeter_freq(info->ulposc_result_fmeter_id, VLPCK);
		if (freq == 0) {
			/* freq is not expected to be 0 */
			pr_notice("[%s]: freq %d, pls check CCF configs\n", __func__, freq);
		}
		break;
	default:
		dev_info(aov_dev->dev, "%s no support fmeter type:%d\n", __func__, type);
		break;
	};

	return freq;
}

static void aov_ulposc_set_cali_result(struct mtk_aov *aov_dev)
{
	struct aov_ulposc_info *info = &(aov_dev->ulposc_info);
	void *base = info->base;

	info->ulposc_cali_result[0] = AOV_READ_REG(base, VLP_ULPOSC3_CON0);

	dev_info(aov_dev->dev, "[%s]:0x%x", __func__, info->ulposc_cali_result[0]);
}

struct mtk_aov_ulposc_ops ulposc_v1 = {
	._init = aov_ulposc_init,
	._set_config = aov_ulposc_set_config,
	._set_cali_factor1_value = aov_ulposc_set_cali_factor1_value,
	._set_cali_factor2_value = aov_ulposc_set_cali_factor2_value,
	._get_fmeter_result = aov_ulposc_get_fmeter_result,
	._set_cali_result = aov_ulposc_set_cali_result,
};
