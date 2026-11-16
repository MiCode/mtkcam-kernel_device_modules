# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

include $(wildcard $(KERNEL_SRC)/$(DEVICE_MODULES_REL_DIR)/Makefile.include)

EXTRA_SYMBOLS += $(wildcard $(OUT_DIR)/../vendor/mediatek/kernel_modules/mtkcam/cam_cal/src_v4l2/*Module.symvers)
EXTRA_SYMBOLS += $(wildcard $(OUT_DIR)/../vendor/mediatek/kernel_modules/mtkcam/imgsensor/src-v4l2/imgsensor-glue/*Module.symvers)
EXTRA_SYMBOLS += $(wildcard $(OUT_DIR)/../vendor/mediatek/kernel_modules/mtkcam/imgsensor/src-isp8/imgsensor-glue/*Module.symvers)
EXTRA_SYMBOLS += $(wildcard $(OUT_DIR)/../vendor/mediatek/kernel_modules/mtkcam/imgsensor/src_spm-isp8/imgsensor-glue/*Module.symvers)
EXTRA_SYMBOLS += $(wildcard $(OUT_DIR)/../vendor/mediatek/kernel_modules/mtkcam/imgsensor/src-isp8s/imgsensor-glue/*Module.symvers)

modules modules_install clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(KBUILD_OPTIONS) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" $(@)
