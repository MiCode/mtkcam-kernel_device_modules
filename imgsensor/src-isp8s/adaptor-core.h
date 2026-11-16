/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 MediaTek Inc. */

#ifndef __ADAPTOR_DRV_CORE_H__
#define __ADAPTOR_DRV_CORE_H__

#include <linux/regulator/consumer.h>
#include <linux/mutex.h>

#define ADAPTOR_DRV_CORE_DEV_NAME "adaptor_drv_core"

#define ADAPTOR_DRV_CORE_REGULATOR_NAMES \
	"power1", \
	"power2", \
	"power3", \
	"mux-sel-ctrl", \

enum {
	ADAPTOR_CORE_REGULATOR_POWER_SUPPLY_1 = 0,
	ADAPTOR_CORE_REGULATOR_POWER_SUPPLY_2,
	ADAPTOR_CORE_REGULATOR_POWER_SUPPLY_3,
	ADAPTOR_CORE_REGULATOR_MAX_POWER_CNT,
	ADAPTOR_CORE_REGULATOR_MUX_SEL_CTRL = ADAPTOR_CORE_REGULATOR_MAX_POWER_CNT,
	ADAPTOR_CORE_REGULATOR_MAXCNT,
};

struct adaptor_drv_core_hw_ops {
	int (*set)(u32 idx, u32 min_v, u32 max_v);
	int (*unset)(u32 idx, u32 min_v, u32 max_v);
	u8 idx;
};

struct adaptor_drv_core_ctx {
	dev_t dev_no;
	struct cdev *pchar_dev;
	struct class *pclass;
	struct device *dev;
	struct regulator *regulator[ADAPTOR_CORE_REGULATOR_MAXCNT];
	struct adaptor_drv_core_hw_ops hw_ops[ADAPTOR_CORE_REGULATOR_MAXCNT];

	struct mutex aov_notify_lock;
};

void adaptor_drv_core_aov_sensor_notify(bool enable);
int adaptor_drv_core_init(void);
void adaptor_drv_core_exit(void);

#endif //__ADAPTOR_DRV_CORE_H__
