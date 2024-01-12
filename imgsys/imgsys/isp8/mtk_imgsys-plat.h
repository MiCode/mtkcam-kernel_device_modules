/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Christopher Chen <christopher.chen@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_PLAT_H_
#define _MTK_IMGSYS_PLAT_H_

#include <linux/clk.h>

struct clk_bulk_data imgsys_isp8_clks_mt6991[] = {
	{
		.id = "IMGSYS_CG_IMG_TRAW0",
	},
	{
		.id = "IMGSYS_CG_IMG_TRAW1",
	},
	{
		.id = "IMGSYS_CG_IMG_DIP0",
	},
	{
		.id = "IMGSYS_CG_IMG_WPE0",
	},
	{
		.id = "IMGSYS_CG_IMG_WPE1",
	},
	{
		.id = "IMGSYS_CG_IMG_WPE2",
	},
	{
		.id = "IMGSYS_CG_IMG_AVS",
	},
	{
		.id = "IMGSYS_CG_IMG_IPS",
	},
	{
		.id = "IMGSYS_CG_SUB_COMMON0",
	},
	{
		.id = "IMGSYS_CG_SUB_COMMON1",
	},
	{
		.id = "IMGSYS_CG_SUB_COMMON2",
	},
	{
		.id = "IMGSYS_CG_SUB_COMMON3",
	},
	{
		.id = "IMGSYS_CG_SUB_COMMON4",
	},
	{
		.id = "IMGSYS_CG_GALS_RX_DIP0",
	},
	{
		.id = "IMGSYS_CG_GALS_RX_DIP1",
	},
	{
		.id = "IMGSYS_CG_GALS_RX_TRAW0",
	},
	{
		.id = "IMGSYS_CG_GALS_RX_WPE0",
	},
	{
		.id = "IMGSYS_CG_GALS_RX_WPE1",
	},
	{
		.id = "IMGSYS_CG_GALS_RX_WPE2",
	},
	{
		.id = "IMGSYS_CG_GALS_RX_IPE0",
	},
	{
		.id = "IMGSYS_CG_GALS_TX_IPE0",
	},
	{
		.id = "IMGSYS_CG_GALS_RX_IPE1",
	},
	{
		.id = "IMGSYS_CG_GALS_TX_IPE1",
	},
	{
		.id = "IMGSYS_CG_IMG_GALS",
	},
	{
		.id = "IMGSYS_CG_IMG_IPE"
	},
	{
		.id = "ME_CG"
	},
	{
		.id = "MMG_CG"
	}
};

#define MTK_IMGSYS_CLK_NUM_MT6991	ARRAY_SIZE(imgsys_isp8_clks_mt6991)

#endif /* _MTK_IMGSYS_PLAT_H_ */
