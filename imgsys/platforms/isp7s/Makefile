# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2018 MediaTek Inc.
#
ifneq ($(KERNELRELEASE),)
include Kbuild

else
ccflags-$(CONFIG_MTK_AEE_FEATURE) += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/mtk-hcp/
ccflags-y += -I$(srctree)/drivers/media/platform/mtk-isp/mtk-ipesys-me/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/media/platform/mtk-isp/imgsys/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/mtk-interconnect/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/iommu
endif
# mtk_imgsys_hw-objs := \
# mtk_imgsys-debug.o \
# modules/mtk_imgsys-dip.o \
# modules/mtk_imgsys-traw.o \
# modules/mtk_imgsys-pqdip.o \
# modules/mtk_imgsys-wpe.o \
# modules/mtk_imgsys-me.o
#
# obj-m += mtk_imgsys_hw.o
