# SPDX-License-Identifier: GPL-2.0-only

include $(wildcard $(KERNEL_SRC)/$(DEVICE_MODULES_REL_DIR)/Makefile.include)


#EXTRA_SYMBOLS += $(OUT_DIR)/../vendor/mediatek/kernel_modules/mtkcam/imgsensor/Module.symvers
EXTRA_SYMBOLS += $(wildcard $(OUT_DIR)/../vendor/mediatek/kernel_modules/mtkcam/imgsensor/src-v4l2/*Module.symvers)

EXTRA_SYMBOLS += $(wildcard $(OUT_DIR)/../vendor/mediatek/kernel_modules/mtkcam/camsys/rpmsg/*Module.symvers)
EXTRA_SYMBOLS += $(wildcard $(OUT_DIR)/../vendor/mediatek/kernel_modules/mtkcam/camsys/remoteproc/*Module.symvers)
EXTRA_SYMBOLS += $(wildcard $(OUT_DIR)/../vendor/mediatek/kernel_modules/mtkcam/camsys/*Module.symvers)

modules modules_install clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(KBUILD_OPTIONS) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" $(@)

########################################################################
############################# non-DDK part #############################
########################################################################
# # SPDX-License-Identifier: GPL-2.0-only
#
# include $(wildcard $(KERNEL_SRC)/$(DEVICE_MODULES_REL_DIR)/Makefile.include)
#
# EXTRA_SYMBOLS += $(OUT_DIR)/../vendor/mediatek/kernel_modules/mtkcam/imgsensor/Module.symvers
#
# modules modules_install clean:
# 	$(MAKE) -C $(KERNEL_SRC) M=$(M) $(KBUILD_OPTIONS) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" $(@)
