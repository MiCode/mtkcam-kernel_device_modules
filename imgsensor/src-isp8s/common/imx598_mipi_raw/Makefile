# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

# enable ISF version and ISF dbg
ISF_DRV := 1
ISF_DBG := 1

ifeq ($(ISF_DRV),1)

# ISF driver

imgsensor_isp8s-objs += \
	$(subdrv-rpath)/imx598mipiraw_Sensor.o

ifeq ($(ISF_DBG),1)

# enable ISF compare with legacy s_ctx comparing
subdir-ccflags-y += -D'IMX598_ISF_DBG=1'

imgsensor_isp8s-objs += \
	$(subdrv-rpath)/legacy/imx598mipiraw_Sensor.o

endif

else

# Legacy driver

imgsensor_isp8s-objs += \
	$(subdrv-rpath)/legacy/imx598mipiraw_Sensor.o

endif

