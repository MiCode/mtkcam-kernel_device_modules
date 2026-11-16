/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __CSIRX_MAC_SW_PPI_0_C_HEADER_H__
#define __CSIRX_MAC_SW_PPI_0_C_HEADER_H__

#define csirx_mac_csi2_pg_ctrl 0x7100
#define RG_CSI2_PG_CTRL_EN_SHIFT 0
#define RG_CSI2_PG_CTRL_EN_MASK (0x1 << 0)
#define RG_CSI2_PG_CDPHY_SEL_SHIFT 1
#define RG_CSI2_PG_CDPHY_SEL_MASK (0x1 << 1)
#define RG_CSI2_PG_SYNC_LANE0_DLY_SHIFT 8
#define RG_CSI2_PG_SYNC_LANE0_DLY_MASK (0x3 << 8)
#define RG_CSI2_PG_SYNC_LANE1_DLY_SHIFT 10
#define RG_CSI2_PG_SYNC_LANE1_DLY_MASK (0x3 << 10)
#define RG_CSI2_PG_SYNC_LANE2_DLY_SHIFT 12
#define RG_CSI2_PG_SYNC_LANE2_DLY_MASK (0x3 << 12)
#define RG_CSI2_PG_SYNC_LANE3_DLY_SHIFT 14
#define RG_CSI2_PG_SYNC_LANE3_DLY_MASK (0x3 << 14)
#define RG_CSI2_PG_SYNC_INIT_0_SHIFT 16
#define RG_CSI2_PG_SYNC_INIT_0_MASK (0x1 << 16)
#define RG_CSI2_PG_SYNC_DET_0_SHIFT 17
#define RG_CSI2_PG_SYNC_DET_0_MASK (0x1 << 17)
#define RG_CSI2_PG_SYNC_DATA_VALID_0_SHIFT 18
#define RG_CSI2_PG_SYNC_DATA_VALID_0_MASK (0x1 << 18)
#define RG_CSI2_PG_SYNC_INIT_1_SHIFT 20
#define RG_CSI2_PG_SYNC_INIT_1_MASK (0x1 << 20)
#define RG_CSI2_PG_SYNC_DET_1_SHIFT 21
#define RG_CSI2_PG_SYNC_DET_1_MASK (0x1 << 21)
#define RG_CSI2_PG_SYNC_DATA_VALID_1_SHIFT 22
#define RG_CSI2_PG_SYNC_DATA_VALID_1_MASK (0x1 << 22)
#define RG_CSI2_PG_SYNC_INIT_2_SHIFT 24
#define RG_CSI2_PG_SYNC_INIT_2_MASK (0x1 << 24)
#define RG_CSI2_PG_SYNC_DET_2_SHIFT 25
#define RG_CSI2_PG_SYNC_DET_2_MASK (0x1 << 25)
#define RG_CSI2_PG_SYNC_DATA_VALID_2_SHIFT 26
#define RG_CSI2_PG_SYNC_DATA_VALID_2_MASK (0x1 << 26)
#define RG_CSI2_PG_SYNC_INIT_3_SHIFT 28
#define RG_CSI2_PG_SYNC_INIT_3_MASK (0x1 << 28)
#define RG_CSI2_PG_SYNC_DET_3_SHIFT 29
#define RG_CSI2_PG_SYNC_DET_3_MASK (0x1 << 29)
#define RG_CSI2_PG_SYNC_DATA_VALID_3_SHIFT 30
#define RG_CSI2_PG_SYNC_DATA_VALID_3_MASK (0x1 << 30)

#define csirx_mac_csi2_pg_cphy_ctrl 0x7104
#define RG_CSI2_PG_CPHY_TYPE_0_SHIFT 0
#define RG_CSI2_PG_CPHY_TYPE_0_MASK (0x7 << 0)
#define RG_CSI2_PG_CPHY_HS_0_SHIFT 4
#define RG_CSI2_PG_CPHY_HS_0_MASK (0xf << 4)
#define RG_CSI2_PG_CPHY_TYPE_1_SHIFT 8
#define RG_CSI2_PG_CPHY_TYPE_1_MASK (0x7 << 8)
#define RG_CSI2_PG_CPHY_HS_1_SHIFT 12
#define RG_CSI2_PG_CPHY_HS_1_MASK (0xf << 12)
#define RG_CSI2_PG_CPHY_TYPE_2_SHIFT 16
#define RG_CSI2_PG_CPHY_TYPE_2_MASK (0x7 << 16)
#define RG_CSI2_PG_CPHY_HS_2_SHIFT 20
#define RG_CSI2_PG_CPHY_HS_2_MASK (0xf << 20)

#define csirx_mac_csi2_pg_data_low_0 0x7108
#define RG_CSI2_PG_DATA_0_L_SHIFT 0
#define RG_CSI2_PG_DATA_0_L_MASK (0xffffffff << 0)

#define csirx_mac_csi2_pg_data_high_0 0x710c
#define RG_CSI2_PG_DATA_0_H_SHIFT 0
#define RG_CSI2_PG_DATA_0_H_MASK (0xffffffff << 0)

#define csirx_mac_csi2_pg_data_low_1 0x7110
#define RG_CSI2_PG_DATA_1_L_SHIFT 0
#define RG_CSI2_PG_DATA_1_L_MASK (0xffffffff << 0)

#define csirx_mac_csi2_pg_data_high_1 0x7114
#define RG_CSI2_PG_DATA_1_H_SHIFT 0
#define RG_CSI2_PG_DATA_1_H_MASK (0xffffffff << 0)

#define csirx_mac_csi2_pg_data_low_2 0x7118
#define RG_CSI2_PG_DATA_2_L_SHIFT 0
#define RG_CSI2_PG_DATA_2_L_MASK (0xffffffff << 0)

#define csirx_mac_csi2_pg_data_high_2 0x711c
#define RG_CSI2_PG_DATA_2_H_SHIFT 0
#define RG_CSI2_PG_DATA_2_H_MASK (0xffffffff << 0)

#define csirx_mac_csi2_pg_data_low_3 0x7120
#define RG_CSI2_PG_DATA_3_L_SHIFT 0
#define RG_CSI2_PG_DATA_3_L_MASK (0xffffffff << 0)

#define csirx_mac_csi2_pg_data_high_3 0x7124
#define RG_CSI2_PG_DATA_3_H_SHIFT 0
#define RG_CSI2_PG_DATA_3_H_MASK (0xffffffff << 0)

#define CSIRX_MAC_CSI2_HW_PG_CTRL_SEL 0x7330
#define RG_HW_PPI_PG_EN_SHIFT 0
#define RG_HW_PPI_PG_EN_MASK (0x1 << 0)
#define RG_PG_CDPHY_SEL_SHIFT 1
#define RG_PG_CDPHY_SEL_MASK (0x1 << 1)
#define RG_PG_SPLIT_EN_SHIFT 2
#define RG_PG_SPLIT_EN_MASK (0x1 << 2)
#define RG_STREAM_ON_SHIFT 3
#define RG_STREAM_ON_MASK (0x1 << 3)
#define RG_PG_LN0_EN_SHIFT 4
#define RG_PG_LN0_EN_MASK (0x1 << 4)
#define RG_PG_LN1_EN_SHIFT 5
#define RG_PG_LN1_EN_MASK (0x1 << 5)
#define RG_PG_LN2_EN_SHIFT 6
#define RG_PG_LN2_EN_MASK (0x1 << 6)
#define RG_PG_LN3_EN_SHIFT 7
#define RG_PG_LN3_EN_MASK (0x1 << 7)
#define RG_PG_CPHY_SYNC_TYPE_SHIFT 8
#define RG_PG_CPHY_SYNC_TYPE_MASK (0x7 << 8)
#define RG_PG_DLY_SEL0_SHIFT 12
#define RG_PG_DLY_SEL0_MASK (0x3 << 12)
#define RG_PG_DLY_SEL1_SHIFT 14
#define RG_PG_DLY_SEL1_MASK (0x3 << 14)
#define RG_PG_DLY_SEL2_SHIFT 16
#define RG_PG_DLY_SEL2_MASK (0x3 << 16)
#define RG_PG_DLY_SEL3_SHIFT 18
#define RG_PG_DLY_SEL3_MASK (0x3 << 18)
#define RG_NO_VLD_CYCLE_SHIFT 20
#define RG_NO_VLD_CYCLE_MASK (0xf << 20)
#define RG_CONTI_VLD_SHIFT 24
#define RG_CONTI_VLD_MASK (0x1 << 24)

#define CSIRX_MAC_CSI2_HW_PG_SIZE1 0x7334
#define RG_PG_VSIZE_SHIFT 0
#define RG_PG_VSIZE_MASK (0xffff << 0)
#define RG_PG_FSIZE_SHIFT 16
#define RG_PG_FSIZE_MASK (0xffff << 16)

#define CSIRX_MAC_CSI2_HW_PG_SIZE2 0x7338
#define RG_PG_HBSIZE_SHIFT 0
#define RG_PG_HBSIZE_MASK (0xffff << 0)
#define RG_PG_VBSIZE_SHIFT 16
#define RG_PG_VBSIZE_MASK (0xffff << 16)

#define CSIRX_MAC_CSI2_HW_PG_FCNT_RCV 0x733c
#define RO_HW_PG_RCV_FSIZE_SHIFT 0
#define RO_HW_PG_RCV_FSIZE_MASK (0xffff << 0)

#define CSIRX_MAC_CSI2_HW_PG_HVCNT_RCV 0x7340
#define RO_HW_PG_RCV_HSIZE_SHIFT 0
#define RO_HW_PG_RCV_HSIZE_MASK (0xffff << 0)
#define RO_HW_PG_RCV_VSIZE_SHIFT 16
#define RO_HW_PG_RCV_VSIZE_MASK (0xffff << 16)

#define CSIRX_MAC_CSI2_HW_PG_HV_B_CNT_RCV 0x7344
#define RO_HW_PG_RCV_HB_SIZE_SHIFT 0
#define RO_HW_PG_RCV_HB_SIZE_MASK (0xffff << 0)
#define RO_HW_PG_RCV_VB_SIZE_SHIFT 16
#define RO_HW_PG_RCV_VB_SIZE_MASK (0xffff << 16)

#define CSIRX_MAC_CSI2_HW_PG_PH 0x7348
#define RG_PG_DT_SHIFT 0
#define RG_PG_DT_MASK (0x3f << 0)
#define RG_PG_VC_SHIFT 8
#define RG_PG_VC_MASK (0x1f << 8)
#define RG_PG_WC_SHIFT 16
#define RG_PG_WC_MASK (0xffff << 16)

#endif
