/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_DATA_H_
#define _MTK_IMGSYS_DATA_H_

#include "mtk_imgsys-of.h"
/* For ISP 7 */
#include "mtk_imgsys-modops.h"
#include "mtk_imgsys-plat.h"
#include "mtk_imgsys_v4l2_vnode.h"
#include "mtk_imgsys-debug.h"
#include "mtk_imgsys-port.h"

/* imgsys_data is a keyword, please don't rename */
const struct cust_data imgsys_data_mt6993[] = {
	[0] = {
	.clks = imgsys_isp8s_clks_mt6993,
	.clk_num = MTK_IMGSYS_CLK_NUM_MT6993,
	.module_pipes = module_pipe_isp8s,
	.mod_num = ARRAY_SIZE(module_pipe_isp8s),
	.pipe_settings = pipe_settings_isp8s,
	.pipe_num = ARRAY_SIZE(pipe_settings_isp8s),
	.imgsys_modules = imgsys_isp8s_modules,
	.imgsys_modules_num = MTK_IMGSYS_MODULE_NUM,
	.dump = imgsys_debug_dump_routine,
#ifdef IMGSYS_TF_DUMP_8S_L
	.imgsys_ports = imgsys_dma_port_mt6993,
	.imgsys_ports_num = ARRAY_SIZE(imgsys_dma_port_mt6993),
#else
	.imgsys_ports_num = 0,
#endif
	},
};
#endif /* _MTK_IMGSYS_DATA_H_ */

