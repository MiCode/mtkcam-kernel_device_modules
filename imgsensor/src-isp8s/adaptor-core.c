// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

#include <asm-generic/errno-base.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include"adaptor-core.h"

#define AOV_AMOUNT_FOR_MCSS 2

#define ADAPTOR_DRV_CORE_LOG_ERR(fmt, arg...)  pr_info("[ERR][%s]"fmt, __func__, ##arg)
#define ADAPTOR_DRV_CORE_LOG_MSG(fmt, arg...)  pr_info("[%s]"fmt, __func__, ##arg)
#define to_ctx(__sd) container_of(__sd, struct adaptor_ctx, sd)

#define INST_OPS(__ctx, __field, __idx, __set, __unset) do {\
	if (__ctx.__field[__idx]) { \
		__ctx.hw_ops[__idx].set = __set; \
		__ctx.hw_ops[__idx].unset = __unset; \
		__ctx.hw_ops[__idx].idx = __idx; \
	}  else {\
		ADAPTOR_DRV_CORE_LOG_ERR("power id %d install failed",__idx); \
	} \
} while (0)

enum {
	MTK_ADAPTOR_CORE_DEFAULT_MODE,
	MTK_ADAPTOR_CORE_VSYNC_MODE = MTK_ADAPTOR_CORE_DEFAULT_MODE,
	MTK_ADAPTOR_CORE_MCSS_MODE,
};

struct adaptor_drv_core_ctx ctx;

static const char * const reg_names[] = {
	ADAPTOR_DRV_CORE_REGULATOR_NAMES
};

static int set_regulator(u32 idx, u32 min_v, u32 max_v)
{
	int ret;

	// re-get reg everytime due to pmic limitation
	if (IS_ERR(ctx.regulator[idx])) {
		ctx.regulator[idx] = NULL;
		ADAPTOR_DRV_CORE_LOG_ERR("no reg %s\n", reg_names[idx]);
		return -EINVAL;
	}

	if (max_v < min_v) {
		ADAPTOR_DRV_CORE_LOG_MSG(
			"max voltage(val->para2)=%u is smaller than min voltage(val->para1)=%u, fall back max=min\n",
			max_v, min_v);
		max_v = min_v;
	}

	ADAPTOR_DRV_CORE_LOG_MSG("+ idx(%u),val_min-max(%u-%u)\n", idx, min_v, max_v);

	ret = regulator_set_voltage(ctx.regulator[idx], min_v, max_v);
	if (ret) {
		ADAPTOR_DRV_CORE_LOG_ERR(
			"regulator_set_voltage(%s),val_min-max(%u-%u),ret(%d)(fail)\n",
			reg_names[idx], min_v, max_v, ret);
	} else {
		ADAPTOR_DRV_CORE_LOG_MSG(
			"regulator_set_voltage(%s),val_min-max(%u-%u),ret(%d)(correct)\n",
			reg_names[idx], min_v, max_v, ret);
	}

	ret = regulator_enable(ctx.regulator[idx]);
	if (ret) {
		ADAPTOR_DRV_CORE_LOG_ERR("regulator_enable(%s),ret(%d)(fail)\n", reg_names[idx], ret);
		return ret;
	}

	ADAPTOR_DRV_CORE_LOG_MSG("- regulator_enable(%s),ret(%d)(correct)\n", reg_names[idx], ret);

	return 0;
}

static int unset_regulator(u32 idx, u32 min_v, u32 max_v)
{
	int ret;
	struct regulator *reg;

	reg = ctx.regulator[idx];

	if (unlikely(reg == NULL)) {
		ADAPTOR_DRV_CORE_LOG_ERR("%s regulator is NULL\n", reg_names[idx]);
		return -EINVAL;
	}

	ADAPTOR_DRV_CORE_LOG_MSG("+ idx(%u),val(%u)\n", idx, min_v);

	ret = regulator_disable(reg);
	if (ret) {
		ADAPTOR_DRV_CORE_LOG_ERR("disable(%s),ret(%d)(fail)\n", reg_names[idx], ret);
		return ret;
	}

	return 0;
}

static int adaptor_drv_core_hw_init(void)
{
	u32 i = 0;

	/* supplies */
	for (i = 0; i < ADAPTOR_CORE_REGULATOR_MAXCNT; i++) {
		ctx.regulator[i] = devm_regulator_get_optional(ctx.dev, reg_names[i]);
		if (IS_ERR(ctx.regulator[i])) {
			ctx.regulator[i] = NULL;
			ADAPTOR_DRV_CORE_LOG_MSG("no reg %s\n", reg_names[i]);
		}
	}

	INST_OPS(ctx, regulator, ADAPTOR_CORE_REGULATOR_POWER_SUPPLY_1, set_regulator, unset_regulator);
	INST_OPS(ctx, regulator, ADAPTOR_CORE_REGULATOR_POWER_SUPPLY_2, set_regulator, unset_regulator);
	INST_OPS(ctx, regulator, ADAPTOR_CORE_REGULATOR_POWER_SUPPLY_3, set_regulator, unset_regulator);
	INST_OPS(ctx, regulator, ADAPTOR_CORE_REGULATOR_MUX_SEL_CTRL, set_regulator, unset_regulator);

	ADAPTOR_DRV_CORE_LOG_MSG("-");

	return 0;
}

static int adaptor_drv_core_power_on_mux(void)
{
	int i, ret = 0;

	for (i = ADAPTOR_CORE_REGULATOR_POWER_SUPPLY_1; i < ADAPTOR_CORE_REGULATOR_MAX_POWER_CNT; i++)
		ret |= ctx.hw_ops[i].set(i, 0, 1800000);

	return ret;
}

static int adaptor_drv_core_power_off_mux(void)
{
	int i, ret = 0;

	for (i = ADAPTOR_CORE_REGULATOR_MAX_POWER_CNT - 1;
		i >= ADAPTOR_CORE_REGULATOR_POWER_SUPPLY_1; i--)
		ret |= ctx.hw_ops[i].unset(i, 0, 1800000);

	return ret;
}


void adaptor_drv_core_aov_sensor_notify(bool enable)
{
	static int aov_notify_count;
	static int current_mux_status;

	mutex_lock(&ctx.aov_notify_lock);
	(enable) ? aov_notify_count++ : aov_notify_count--;
	mutex_unlock(&ctx.aov_notify_lock);

	ADAPTOR_DRV_CORE_LOG_MSG("enable(%d) aov_notify_count(%d), current_mux_status(%d)\n",
		enable,
		aov_notify_count,
		current_mux_status);

	if (aov_notify_count < 0) {
		ADAPTOR_DRV_CORE_LOG_ERR("notify count error!\n");
		aov_notify_count = 0;
	}

	if (aov_notify_count >= AOV_AMOUNT_FOR_MCSS &&
		current_mux_status != MTK_ADAPTOR_CORE_MCSS_MODE) {
		if (adaptor_drv_core_power_on_mux())
			ADAPTOR_DRV_CORE_LOG_ERR("adaptor_drv_core_power_on_mux return failed\n");

		current_mux_status = MTK_ADAPTOR_CORE_MCSS_MODE;
		ctx.hw_ops[ADAPTOR_CORE_REGULATOR_MUX_SEL_CTRL].set(
			ADAPTOR_CORE_REGULATOR_MUX_SEL_CTRL, 0, 1800000);

		ADAPTOR_DRV_CORE_LOG_MSG("Switch to MCSS mode done");
	}

	if (aov_notify_count < AOV_AMOUNT_FOR_MCSS &&
		current_mux_status != MTK_ADAPTOR_CORE_DEFAULT_MODE) {
		ctx.hw_ops[ADAPTOR_CORE_REGULATOR_MUX_SEL_CTRL].unset(
			ADAPTOR_CORE_REGULATOR_MUX_SEL_CTRL, 0, 1800000);

		current_mux_status = MTK_ADAPTOR_CORE_DEFAULT_MODE;
		if (adaptor_drv_core_power_off_mux())
			ADAPTOR_DRV_CORE_LOG_ERR("adaptor_drv_core_power_on_mux return failed\n");

		ADAPTOR_DRV_CORE_LOG_MSG("Switch to default mode done");
	}
}
EXPORT_SYMBOL(adaptor_drv_core_aov_sensor_notify);

static int adaptor_drv_core_register_char_dev(void)
{
	int ret = 0;
	struct device *dev;

	ret = alloc_chrdev_region(&ctx.dev_no, 0, 1, ADAPTOR_DRV_CORE_DEV_NAME);

	if (ret < 0) {
		ADAPTOR_DRV_CORE_LOG_ERR("alloc_chrdev_region failed %d\n", ret);
		return ret;
	}

	/* Create class register */
	ctx.pclass = class_create(ADAPTOR_DRV_CORE_DEV_NAME);
	if (IS_ERR(ctx.pclass)) {
		ret = PTR_ERR(ctx.pclass);
		ADAPTOR_DRV_CORE_LOG_ERR("Unable to create class, err = %d\n", ret);
		return ret;
	}

	/* Register device driver */
	dev = device_create(ctx.pclass, NULL, ctx.dev_no, NULL, ADAPTOR_DRV_CORE_DEV_NAME);
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		ADAPTOR_DRV_CORE_LOG_ERR("Unable to create class, err = %d\n", ret);
		goto EXIT;
	}

	return ret;

EXIT:
	class_destroy(ctx.pclass);
	return -EFAULT;
}

static int adaptor_drv_core_probe(struct platform_device *pDev)
{
	memset(&ctx, 0, sizeof(struct adaptor_drv_core_ctx));
	ctx.dev = &pDev->dev;

	adaptor_drv_core_hw_init();
	mutex_init(&ctx.aov_notify_lock);

	if (adaptor_drv_core_register_char_dev()) {
		ADAPTOR_DRV_CORE_LOG_ERR("adaptor_drv_core_register_char_dev failed\n");
		return -EFAULT;
	}

	ADAPTOR_DRV_CORE_LOG_MSG("done");

	return 0;
}

static void adaptor_drv_core_remove(struct platform_device *pDev)
{
	device_destroy(ctx.pclass, ctx.dev_no);
	class_destroy(ctx.pclass);
}


static const struct of_device_id adaptor_drv_core_of_device_id[] = {
	{.compatible = "mediatek,adaptor_core"},
	{}
};

static struct platform_driver adaptor_drv_core_platform_driver = {
	.probe = adaptor_drv_core_probe,
	.remove = adaptor_drv_core_remove,
	.driver = {
			.name = ADAPTOR_DRV_CORE_DEV_NAME,
			.owner = THIS_MODULE,
			.of_match_table = adaptor_drv_core_of_device_id,
			}
};

int adaptor_drv_core_init(void)
{
	ADAPTOR_DRV_CORE_LOG_MSG("+");

	if (platform_driver_register(&adaptor_drv_core_platform_driver) < 0) {
		ADAPTOR_DRV_CORE_LOG_ERR("platform_driver_register fail");
		return -ENODEV;
	}

	return 0;
}

void adaptor_drv_core_exit(void)
{
	platform_driver_unregister(&adaptor_drv_core_platform_driver);
}


MODULE_DESCRIPTION("sensor adaptor top driver");
MODULE_AUTHOR("Mediatek");
MODULE_LICENSE("GPL");
