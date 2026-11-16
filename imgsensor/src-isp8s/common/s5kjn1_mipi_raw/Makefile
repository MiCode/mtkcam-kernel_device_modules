# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

# enable ISF version and ISF dbg
ISF_DRV := 1
ISF_DBG := 0

ifeq ($(ISF_DRV),1)

# ISF driver

imgsensor_isp8s-objs += \
	$(subdrv-rpath)/s5kjn1mipiraw_Sensor.o \

ifeq ($(ISF_DBG),1)

# enable ISF compare with legacy s_ctx comparing
subdir-ccflags-y += -D'S5KJN1_ISF_DBG=1'

imgsensor_isp8s-objs += \
	$(subdrv-rpath)/legacy/s5kjn1mipiraw_Sensor.o

endif

else

# Legacy driver

imgsensor_isp8s-objs += \
	$(subdrv-rpath)/legacy/s5kjn1mipiraw_Sensor.o

endif
