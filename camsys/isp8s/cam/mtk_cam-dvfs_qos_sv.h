/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_CAM_DVFS_QOS_SV_H
#define __MTK_CAM_DVFS_QOS_SV_H

/* type0: one smi out / type1: two smi out / type2: three smi out */
enum SMI_SV_MERGE_PORT_ID {
	SMI_PORT_SV_CQI = 0,
	SMI_PORT_SV_WDMA_0,
	SMI_PORT_SV_STG_0,
	SMI_PORT_SV_TYPE0_NUM,
	SMI_PORT_SV_WDMA_1 = SMI_PORT_SV_TYPE0_NUM,
	SMI_PORT_SV_STG_1,
	SMI_PORT_SV_TYPE1_NUM,
	SMI_PORT_SV_WDMA_2 = SMI_PORT_SV_TYPE1_NUM,
	SMI_PORT_SV_STG_2,
	SMI_PORT_SV_TYPE2_NUM,
	SMI_PORT_SV_NUM = SMI_PORT_SV_TYPE2_NUM,
};

#endif /* __MTK_CAM_DVFS_QOS_SV_H */
