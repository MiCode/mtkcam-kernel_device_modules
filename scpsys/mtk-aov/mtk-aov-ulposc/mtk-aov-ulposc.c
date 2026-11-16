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
#include <clk-fmeter.h>

#include "mtk-aov-drv.h"
#include "mtk-aov-ulposc.h"
#include "mtk-aov-ulposc-hw.h"
#include "mtk-aov-core.h"
#include "mtk-aov-regs.h"
#include "mtk-aov-log.h"

struct mtk_aov_ulposc_ops *g_ulposc_ops;

static int aov_ulposc_turn_on_off(struct mtk_aov *aov_dev, bool turnon)
{
	int ret = 0;

	if (turnon) {
		ret = aov_core_send_cmd(aov_dev, AOV_SCP_CMD_TURN_ON_ULPOSC, NULL, 0, false);
		if (ret < 0)
			dev_info(aov_dev->dev, "%s: send cmd(%d) fail", __func__, AOV_SCP_CMD_TURN_ON_ULPOSC);
		else
			dev_info(aov_dev->dev, "%s: send cmd(%d) done", __func__, AOV_SCP_CMD_TURN_ON_ULPOSC);
	} else {
		ret = aov_core_send_cmd(aov_dev, AOV_SCP_CMD_TURN_OFF_ULPOSC, NULL, 0, false);
		if (ret < 0)
			dev_info(aov_dev->dev, "%s: send cmd(%d) fail", __func__, AOV_SCP_CMD_TURN_OFF_ULPOSC);
		else
			dev_info(aov_dev->dev, "%s: send cmd(%d) done", __func__, AOV_SCP_CMD_TURN_OFF_ULPOSC);
	}

	return 0;
}

int aov_ulposc_check_cali_result(struct mtk_aov *aov_dev)
{
	struct aov_ulposc_info *info = &(aov_dev->ulposc_info);
	unsigned int current_val = 0;

	aov_ulposc_turn_on_off(aov_dev, 0);
	g_ulposc_ops->_set_config(aov_dev, CALI_CONFIG);
	aov_ulposc_turn_on_off(aov_dev, 1);

	current_val = g_ulposc_ops->_get_fmeter_result(aov_dev, RESULT_FMETER);

	if (current_val < (info->ulposc_cali_result_target * 1000 * (100 + CALI_MIS_RATE)/100) &&
		current_val > (info->ulposc_cali_result_target * 1000 * (100 - CALI_MIS_RATE)/100)) {
		info->ulposc_cali_done = true;
		dev_info(aov_dev->dev,"[%s] pass: current_val: %dMHz/%dkhz, target: %dMHz\n", __func__,
			KHZ_TO_MHZ(current_val), current_val, info->ulposc_cali_result_target);
		return 1;
	}

	dev_info(aov_dev->dev,"[%s] fail: current_val: %dMHz/%dkhz, target: %dMHz\n", __func__,
		KHZ_TO_MHZ(current_val), current_val, info->ulposc_cali_result_target);
	return 0;
}

static int aov_ulposc_cali_process(struct mtk_aov *aov_dev)
{
	struct aov_ulposc_info *info = &(aov_dev->ulposc_info);
	unsigned int target_val = 0, current_val = 0, ret = 0;
	unsigned int min = 0, max = 0, mid  = 0;
	unsigned int diff_by_min = 0, diff_by_max = 0xffff;
	unsigned int factor1_cali_result = 0;

	target_val = info->ulposc_cali_target;

	min = 0x0;
	max = 0x1 << CAL_FACTOR1_BITS;

	do {
		mid = (min + max) / 2;
		if (mid == min) {
			pr_debug("turning_factor1 mid(%u) == min(%u)\n", mid, min);
			break;
		}
		g_ulposc_ops->_set_cali_factor1_value(aov_dev, mid);
		current_val = KHZ_TO_MHZ(g_ulposc_ops->_get_fmeter_result(aov_dev, CALI_FMETER));
		if (current_val > target_val)
			max = mid;
		else
			min = mid;
	} while (min <= max);

	g_ulposc_ops->_set_cali_factor1_value(aov_dev, min);
	current_val = KHZ_TO_MHZ(g_ulposc_ops->_get_fmeter_result(aov_dev, CALI_FMETER));
	diff_by_min = (current_val > target_val) ? (current_val - target_val):(target_val - current_val);
	g_ulposc_ops->_set_cali_factor1_value(aov_dev, max);
	current_val = KHZ_TO_MHZ(g_ulposc_ops->_get_fmeter_result(aov_dev, CALI_FMETER));
	diff_by_max = (current_val > target_val) ? (current_val - target_val):(target_val - current_val);
	factor1_cali_result = (diff_by_min < diff_by_max) ? min : max;
	g_ulposc_ops->_set_cali_factor1_value(aov_dev, factor1_cali_result);

	min = 0x0;
	max = 0x1 << CAL_FACTOR2_BITS;

	do {
		mid = (min + max) / 2;
		if (mid == min) {
			pr_debug("turning_factor2 mid(%u) == min(%u)\n", mid, min);
			break;
		}
		g_ulposc_ops->_set_cali_factor2_value(aov_dev, mid);
		current_val =  KHZ_TO_MHZ(g_ulposc_ops->_get_fmeter_result(aov_dev, CALI_FMETER));
		if (current_val > target_val)
			max = mid;
		else
			min = mid;
	} while (min <= max);

	g_ulposc_ops->_set_cali_factor2_value(aov_dev, min);
	current_val = KHZ_TO_MHZ(g_ulposc_ops->_get_fmeter_result(aov_dev, CALI_FMETER));
	diff_by_min = (current_val > target_val) ? (current_val - target_val):(target_val - current_val);
	g_ulposc_ops->_set_cali_factor2_value(aov_dev, max);
	current_val = KHZ_TO_MHZ(g_ulposc_ops->_get_fmeter_result(aov_dev, CALI_FMETER));
	diff_by_max = (current_val > target_val) ? (current_val - target_val):(target_val - current_val);
	factor1_cali_result = (diff_by_min < diff_by_max) ? min : max;
	g_ulposc_ops->_set_cali_factor2_value(aov_dev, factor1_cali_result);

	g_ulposc_ops->_set_cali_result(aov_dev);

	/* check calibration result */
	ret = aov_ulposc_check_cali_result(aov_dev);
	if (ret)
		dev_info(aov_dev->dev, "%s : pass,\n", __func__);
	else
		dev_info(aov_dev->dev, "%s : fail,\n", __func__);

	return 0;
}

void aov_ulposc_dts_init(struct mtk_aov *aov_dev)
{
	unsigned int base_pa = 0;
	struct aov_ulposc_info *info = &(aov_dev->ulposc_info);

	info->ulposc_support = of_property_read_bool(aov_dev->dev->of_node, "ulposc-support");
	if (!info->ulposc_support) {
		dev_info(aov_dev->dev, "%s: no ulposc support\n", __func__);
		return;
	}

	if (of_property_read_string(aov_dev->dev->of_node, "ver", &(info->ver)))
		info->ver = "v1";

	if (!strcasecmp(info->ver, "v2"))
		g_ulposc_ops = &ulposc_v2;
	else
		g_ulposc_ops = &ulposc_v1;

	g_ulposc_ops->_init(aov_dev);
	info->ulposc_cali_done = false;
	if (!of_property_read_u32(aov_dev->dev->of_node, "vlp-base", &base_pa))
		info->base = ioremap(base_pa, 0x1000);
	else if (!of_property_read_u32(aov_dev->dev->of_node, "base", &base_pa))
		info->base = ioremap(base_pa, 0x1000);
	else
		info->base = NULL;
}

int aov_ulposc_cali(struct mtk_aov *aov_dev)
{
	struct aov_ulposc_info *info = &(aov_dev->ulposc_info);

	if (!info->ulposc_support) {
		dev_info(aov_dev->dev, "%s:no ulposc-support", __func__);
		return 0;
	}

	if (info->ulposc_cali_done) {
		dev_info(aov_dev->dev, "%s:ulposc cali done, no need to cali", __func__);
		return 0;
	}

	g_ulposc_ops->_set_config(aov_dev, INIT_CONFIG);
	aov_ulposc_turn_on_off(aov_dev, 0);
	g_ulposc_ops->_set_config(aov_dev, PRE_CONFIG);
	aov_ulposc_turn_on_off(aov_dev, 1);
	aov_ulposc_cali_process(aov_dev);
	aov_ulposc_turn_on_off(aov_dev, 0);
	return 0;
}
