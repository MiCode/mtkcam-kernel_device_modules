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
#include <linux/delay.h>

#include "mtk-aov-drv.h"
#include "mtk-aov-ulposc.h"
#include "mtk-aov-ulposc-hw.h"
#include "v2/mtk-aov-osc3_ctrl.h"
#include "mtk-aov-core.h"
#include "mtk-aov-regs.h"
#include "mtk-aov-log.h"

#define ULPOSC_CALI_TARGET 260 // unit: MHz
#define ULPOSC_RESULT_TARGET 26 // unit: MHz
#define CALI_DIV_VAL	(512)
#define FM_CNT2FREQ(cnt)	(cnt * 26000 / CALI_DIV_VAL) // unit: kHz
#define FM_FREQ2CNT(freq)	(freq * CALI_DIV_VAL / 26000) // unit: kHz

static unsigned int ulposc_cali_config[ULPOSC_CALI_CONFIG_NUM] = {
	0x0140829d, 0x00080800, 0x19034f90
};
static unsigned int ulposc_cali_result[ULPOSC_CALI_CONFIG_NUM] = {
	0x00000000, 0x00080800, 0x19034f90
};

static void aov_ulposc_init(struct mtk_aov *aov_dev)
{
	struct aov_ulposc_info *info = &(aov_dev->ulposc_info);
	int i = 0;

	info->ulposc_cali_target = ULPOSC_CALI_TARGET;
	info->ulposc_cali_result_target = ULPOSC_RESULT_TARGET;
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
		AOV_WRITE_REG(base, OSC3_CON0, info->ulposc_cali_config_setting[0]);
		AOV_WRITE_REG(base, OSC3_CON1, info->ulposc_cali_config_setting[1]);
		AOV_WRITE_REG(base, OSC3_CON2, info->ulposc_cali_config_setting[2]);
		udelay(10);
		break;
	case CALI_CONFIG:
		AOV_WRITE_REG(base, OSC3_CON0, info->ulposc_cali_result[0]);
		AOV_WRITE_REG(base, OSC3_CON1, info->ulposc_cali_result[1]);
		AOV_WRITE_REG(base, OSC3_CON2, info->ulposc_cali_result[2]);
		dev_info(aov_dev->dev,"[%s] cali_result:0x%x\n", __func__, AOV_READ_REG(base, OSC3_CON0));
		udelay(10);
		break;
	case INIT_CONFIG:
		/* OSC3 Clock Setting Sequence */
		AOV_WRITE_REG(base, OSC3_CLKMON_REG_CON3, 0x00000000);
		AOV_WRITE_REG(base, OSC3_CLKMON_REG_CON0, 0xFFFF1000);
		AOV_WRITE_REG(base, OSC3_CLKMON_REG_CON0, 0xFFFF9000);
		AOV_WRITE_REG(base, OSC3_CLKMON_REG_CON1, 0x01FF0000);
		AOV_WRITE_REG(base, OSC3_CLKMON_REG_CON2, 0x000000E0);
		/* OSC3 Initial Setting Sequence */
		AOV_WRITE_REG(base, OSC3_OSCPON_CON0, 0x93840061);
		AOV_WRITE_REG(base, OSC3_OSCPON_CON0, 0x73840061);
		AOV_WRITE_REG(base, OSC3_MODE_CON0, 0x00000001);
		AOV_WRITE_REG(base, OSC3_MODE_CON0, 0x00000111);
		AOV_WRITE_REG(base, OSC3_OSCPON_CON0, 0xF3840061);
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

	cali = AOV_READ_BITS(base, OSC3_CON0, RG_OSC3_CALI);
	cali &= ~(mask << CAL_FACTOR2_BITS); // clear bits
	cali |= ((value & mask) << CAL_FACTOR2_BITS); // set value
	AOV_BITS(base, OSC3_CON0, RG_OSC3_CALI, cali);
}

static void aov_ulposc_set_cali_factor2_value(struct mtk_aov *aov_dev, int value)
{
	struct aov_ulposc_info *info = &(aov_dev->ulposc_info);
	void *base = info->base;
	unsigned int cali, mask = (0x1 << CAL_FACTOR2_BITS) - 1;

	cali = AOV_READ_BITS(base, OSC3_CON0, RG_OSC3_CALI);
	cali &= ~mask; // clear bits
	cali |= (value & mask); // set value
	AOV_BITS(base, OSC3_CON0, RG_OSC3_CALI, cali);
}

static int aov_ulposc_get_fmeter_result(struct mtk_aov *aov_dev, int type)
{
	struct aov_ulposc_info *info = &(aov_dev->ulposc_info);
	void *base = info->base;
	unsigned int freq = 0;

	switch (type) {
	case CALI_FMETER:
	case RESULT_FMETER:
		AOV_WRITE_REG(base, OSC3_CLKMON_REG_CON0, 0xFFFF9010);
		udelay(45);
		freq = FM_CNT2FREQ(AOV_READ_BITS(base, OSC3_CLKMON_REG_CON1, CAL_CNT));
		AOV_WRITE_REG(base, OSC3_CLKMON_REG_CON0, 0xFFFF9000);
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

	AOV_BITS(base, OSC3_CON0, RG_OSC3_MOD, 0x2);
	info->ulposc_cali_result[0] = AOV_READ_REG(base, OSC3_CON0);

	dev_info(aov_dev->dev, "[%s]:0x%x", __func__, info->ulposc_cali_result[0]);
}

struct mtk_aov_ulposc_ops ulposc_v2 = {
	._init = aov_ulposc_init,
	._set_config = aov_ulposc_set_config,
	._set_cali_factor1_value = aov_ulposc_set_cali_factor1_value,
	._set_cali_factor2_value = aov_ulposc_set_cali_factor2_value,
	._get_fmeter_result = aov_ulposc_get_fmeter_result,
	._set_cali_result = aov_ulposc_set_cali_result,
};
