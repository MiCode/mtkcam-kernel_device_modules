# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2018 MediaTek Inc.
#
# export MTK_IMGSYS_VER := $(subst ",,$(CONFIG_MTK_IMGSYS_VER))

include $(wildcard $(KERNEL_SRC)/$(DEVICE_MODULES_REL_DIR)/Makefile.include)

EXTRA_SYMBOLS += $(OUT_DIR)/../vendor/mediatek/kernel_modules/mtkcam/mtk-hcp/Module.symvers
EXTRA_SYMBOLS += $(OUT_DIR)/../vendor/mediatek/kernel_modules/mtkcam/mtk-ipesys-me/Module.symvers

modules modules_install clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(KBUILD_OPTIONS) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" $(@)