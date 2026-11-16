/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_CAM_BWR_REGS_H
#define _MTK_CAM_BWR_REGS_H

/* baseaddr 0x3A022000 */

/* module: BWR_CAM_E1A */
#define REG_BWR_CAM_RPT_CTRL                          0x0
#define F_BWR_CAM_RPT_RST_POS                                        2
#define F_BWR_CAM_RPT_RST_WIDTH                                      1
#define F_BWR_CAM_RPT_END_POS                                        1
#define F_BWR_CAM_RPT_END_WIDTH                                      1
#define F_BWR_CAM_RPT_START_POS                                      0
#define F_BWR_CAM_RPT_START_WIDTH                                    1

#define REG_BWR_CAM_RPT_TIMER                         0x4
#define REG_BWR_CAM_RPT_STATE                         0x8
#define F_BWR_CAM_RPT_STATE_POS                                      0
#define F_BWR_CAM_RPT_STATE_WIDTH                                    3

#define REG_BWR_CAM_MTCMOS_EN_VLD                     0xC
#define F_BWR_CAM_MTCMOS_EN_VLD_POS                                  0
#define F_BWR_CAM_MTCMOS_EN_VLD_WIDTH                                12

#define REG_BWR_CAM_DBC_CYC                          0x10
#define REG_BWR_CAM_DCM_DIS                          0x20
#define F_BWR_CAM_DCM_DIS_CORE_POS                                   1
#define F_BWR_CAM_DCM_DIS_CORE_WIDTH                                 1
#define F_BWR_CAM_DCM_DIS_CTRL_POS                                   0
#define F_BWR_CAM_DCM_DIS_CTRL_WIDTH                                 1

#define REG_BWR_CAM_DCM_ST                           0x24
#define F_BWR_CAM_DCM_ST_CORE_POS                                    1
#define F_BWR_CAM_DCM_ST_CORE_WIDTH                                  1
#define F_BWR_CAM_DCM_ST_CTRL_POS                                    0
#define F_BWR_CAM_DCM_ST_CTRL_WIDTH                                  1

#define REG_BWR_CAM_DBG_SEL                          0x30
#define F_BWR_CAM_DBG_SEL_POS                                        0
#define F_BWR_CAM_DBG_SEL_WIDTH                                      5

#define REG_BWR_CAM_DBG_DATA                         0x34
#define REG_BWR_CAM_BW_TYPE                          0x40
#define F_BWR_CAM_BW_UNIT_SEL_POS                                    4
#define F_BWR_CAM_BW_UNIT_SEL_WIDTH                                  2
#define F_BWR_CAM_DVFS_AVG_PEAK_POS                                  0
#define F_BWR_CAM_DVFS_AVG_PEAK_WIDTH                                1

#define REG_BWR_CAM_SEND_TRIG                        0x50
#define F_BWR_CAM_SEND_TRIG_POS                                      0
#define F_BWR_CAM_SEND_TRIG_WIDTH                                    1

#define REG_BWR_CAM_SEND_BW                          0x54
#define F_BWR_CAM_SEND_BW_POS                                        0
#define F_BWR_CAM_SEND_BW_WIDTH                                      13

#define REG_BWR_CAM_SEND_BW_ZERO                     0x60
#define F_BWR_CAM_SEND_BW_ZERO_POS                                   0
#define F_BWR_CAM_SEND_BW_ZERO_WIDTH                                 1

#define REG_BWR_CAM_SEND_VLD_ST0                     0x64
#define REG_BWR_CAM_SEND_VLD_ST1                     0x68
#define REG_BWR_CAM_SEND_DONE_ST0                    0x6C
#define REG_BWR_CAM_SEND_DONE_ST1                    0x70
#define REG_BWR_CAM_SRT_EMI_OCC_FACTOR               0x80
#define F_BWR_CAM_SRT_EMI_OCC_FACTOR_POS                             0
#define F_BWR_CAM_SRT_EMI_OCC_FACTOR_WIDTH                           10

#define REG_BWR_CAM_SRT_EMI_DVFS_FREQ                0x84
#define F_BWR_CAM_SRT_EMI_DVFS_FREQ_POS                              0
#define F_BWR_CAM_SRT_EMI_DVFS_FREQ_WIDTH                            10

#define REG_BWR_CAM_SRT_COH_OCC_FACTOR               0x88
#define F_BWR_CAM_SRT_COH_OCC_FACTOR_POS                             0
#define F_BWR_CAM_SRT_COH_OCC_FACTOR_WIDTH                           10

#define REG_BWR_CAM_SRT_COH_DVFS_FREQ                0x8C
#define F_BWR_CAM_SRT_COH_DVFS_FREQ_POS                              0
#define F_BWR_CAM_SRT_COH_DVFS_FREQ_WIDTH                            10

#define REG_BWR_CAM_SRT_RW_OCC_FACTOR                0x90
#define F_BWR_CAM_SRT_RW_OCC_FACTOR_POS                              0
#define F_BWR_CAM_SRT_RW_OCC_FACTOR_WIDTH                            10

#define REG_BWR_CAM_SRT_RW_DVFS_FREQ                 0x94
#define F_BWR_CAM_SRT_RW_DVFS_FREQ_POS                               0
#define F_BWR_CAM_SRT_RW_DVFS_FREQ_WIDTH                             10

#define REG_BWR_CAM_HRT_EMI_OCC_FACTOR               0x98
#define F_BWR_CAM_HRT_EMI_OCC_FACTOR_POS                             0
#define F_BWR_CAM_HRT_EMI_OCC_FACTOR_WIDTH                           10

#define REG_BWR_CAM_HRT_EMI_DVFS_FREQ                0x9C
#define F_BWR_CAM_HRT_EMI_DVFS_FREQ_POS                              0
#define F_BWR_CAM_HRT_EMI_DVFS_FREQ_WIDTH                            10

#define REG_BWR_CAM_HRT_COH_OCC_FACTOR               0xA0
#define F_BWR_CAM_HRT_COH_OCC_FACTOR_POS                             0
#define F_BWR_CAM_HRT_COH_OCC_FACTOR_WIDTH                           10

#define REG_BWR_CAM_HRT_COH_DVFS_FREQ                0xA4
#define F_BWR_CAM_HRT_COH_DVFS_FREQ_POS                              0
#define F_BWR_CAM_HRT_COH_DVFS_FREQ_WIDTH                            10

#define REG_BWR_CAM_HRT_RW_OCC_FACTOR                0xA8
#define F_BWR_CAM_HRT_RW_OCC_FACTOR_POS                              0
#define F_BWR_CAM_HRT_RW_OCC_FACTOR_WIDTH                            10

#define REG_BWR_CAM_HRT_RW_DVFS_FREQ                 0xAC
#define F_BWR_CAM_HRT_RW_DVFS_FREQ_POS                               0
#define F_BWR_CAM_HRT_RW_DVFS_FREQ_WIDTH                             10

#define REG_BWR_CAM_PROTOCOL_SET_EN                  0xC0
#define F_BWR_CAM_MANUAL_SET_POS                                     0
#define F_BWR_CAM_MANUAL_SET_WIDTH                                   1

#define REG_BWR_CAM_PROTOCOL0                        0xC4
#define F_BWR_CAM_CH_TTL3_POS                                        31
#define F_BWR_CAM_CH_TTL3_WIDTH                                      1
#define F_BWR_CAM_RD_WD3_POS                                         30
#define F_BWR_CAM_RD_WD3_WIDTH                                       1
#define F_BWR_CAM_SRT_HRT3_POS                                       29
#define F_BWR_CAM_SRT_HRT3_WIDTH                                     1
#define F_BWR_CAM_DRAM_SLB3_POS                                      27
#define F_BWR_CAM_DRAM_SLB3_WIDTH                                    2
#define F_BWR_CAM_AXI3_POS                                           24
#define F_BWR_CAM_AXI3_WIDTH                                         3
#define F_BWR_CAM_CH_TTL2_POS                                        23
#define F_BWR_CAM_CH_TTL2_WIDTH                                      1
#define F_BWR_CAM_RD_WD2_POS                                         22
#define F_BWR_CAM_RD_WD2_WIDTH                                       1
#define F_BWR_CAM_SRT_HRT2_POS                                       21
#define F_BWR_CAM_SRT_HRT2_WIDTH                                     1
#define F_BWR_CAM_DRAM_SLB2_POS                                      19
#define F_BWR_CAM_DRAM_SLB2_WIDTH                                    2
#define F_BWR_CAM_AXI2_POS                                           16
#define F_BWR_CAM_AXI2_WIDTH                                         3
#define F_BWR_CAM_CH_TTL1_POS                                        15
#define F_BWR_CAM_CH_TTL1_WIDTH                                      1
#define F_BWR_CAM_RD_WD1_POS                                         14
#define F_BWR_CAM_RD_WD1_WIDTH                                       1
#define F_BWR_CAM_SRT_HRT1_POS                                       13
#define F_BWR_CAM_SRT_HRT1_WIDTH                                     1
#define F_BWR_CAM_DRAM_SLB1_POS                                      11
#define F_BWR_CAM_DRAM_SLB1_WIDTH                                    2
#define F_BWR_CAM_AXI1_POS                                           8
#define F_BWR_CAM_AXI1_WIDTH                                         3
#define F_BWR_CAM_CH_TTL0_POS                                        7
#define F_BWR_CAM_CH_TTL0_WIDTH                                      1
#define F_BWR_CAM_RD_WD0_POS                                         6
#define F_BWR_CAM_RD_WD0_WIDTH                                       1
#define F_BWR_CAM_SRT_HRT0_POS                                       5
#define F_BWR_CAM_SRT_HRT0_WIDTH                                     1
#define F_BWR_CAM_DRAM_SLB0_POS                                      3
#define F_BWR_CAM_DRAM_SLB0_WIDTH                                    2
#define F_BWR_CAM_AXI0_POS                                           0
#define F_BWR_CAM_AXI0_WIDTH                                         3

#define REG_BWR_CAM_PROTOCOL1                        0xC8
#define F_BWR_CAM_CH_TTL7_POS                                        31
#define F_BWR_CAM_CH_TTL7_WIDTH                                      1
#define F_BWR_CAM_RD_WD7_POS                                         30
#define F_BWR_CAM_RD_WD7_WIDTH                                       1
#define F_BWR_CAM_SRT_HRT7_POS                                       29
#define F_BWR_CAM_SRT_HRT7_WIDTH                                     1
#define F_BWR_CAM_DRAM_SLB7_POS                                      27
#define F_BWR_CAM_DRAM_SLB7_WIDTH                                    2
#define F_BWR_CAM_AXI7_POS                                           24
#define F_BWR_CAM_AXI7_WIDTH                                         3
#define F_BWR_CAM_CH_TTL6_POS                                        23
#define F_BWR_CAM_CH_TTL6_WIDTH                                      1
#define F_BWR_CAM_RD_WD6_POS                                         22
#define F_BWR_CAM_RD_WD6_WIDTH                                       1
#define F_BWR_CAM_SRT_HRT6_POS                                       21
#define F_BWR_CAM_SRT_HRT6_WIDTH                                     1
#define F_BWR_CAM_DRAM_SLB6_POS                                      19
#define F_BWR_CAM_DRAM_SLB6_WIDTH                                    2
#define F_BWR_CAM_AXI6_POS                                           16
#define F_BWR_CAM_AXI6_WIDTH                                         3
#define F_BWR_CAM_CH_TTL5_POS                                        15
#define F_BWR_CAM_CH_TTL5_WIDTH                                      1
#define F_BWR_CAM_RD_WD5_POS                                         14
#define F_BWR_CAM_RD_WD5_WIDTH                                       1
#define F_BWR_CAM_SRT_HRT5_POS                                       13
#define F_BWR_CAM_SRT_HRT5_WIDTH                                     1
#define F_BWR_CAM_DRAM_SLB5_POS                                      11
#define F_BWR_CAM_DRAM_SLB5_WIDTH                                    2
#define F_BWR_CAM_AXI5_POS                                           8
#define F_BWR_CAM_AXI5_WIDTH                                         3
#define F_BWR_CAM_CH_TTL4_POS                                        7
#define F_BWR_CAM_CH_TTL4_WIDTH                                      1
#define F_BWR_CAM_RD_WD4_POS                                         6
#define F_BWR_CAM_RD_WD4_WIDTH                                       1
#define F_BWR_CAM_SRT_HRT4_POS                                       5
#define F_BWR_CAM_SRT_HRT4_WIDTH                                     1
#define F_BWR_CAM_DRAM_SLB4_POS                                      3
#define F_BWR_CAM_DRAM_SLB4_WIDTH                                    2
#define F_BWR_CAM_AXI4_POS                                           0
#define F_BWR_CAM_AXI4_WIDTH                                         3

#define REG_BWR_CAM_PROTOCOL2                        0xCC
#define F_BWR_CAM_CH_TTL11_POS                                       31
#define F_BWR_CAM_CH_TTL11_WIDTH                                     1
#define F_BWR_CAM_RD_WD11_POS                                        30
#define F_BWR_CAM_RD_WD11_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT11_POS                                      29
#define F_BWR_CAM_SRT_HRT11_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB11_POS                                     27
#define F_BWR_CAM_DRAM_SLB11_WIDTH                                   2
#define F_BWR_CAM_AXI11_POS                                          24
#define F_BWR_CAM_AXI11_WIDTH                                        3
#define F_BWR_CAM_CH_TTL10_POS                                       23
#define F_BWR_CAM_CH_TTL10_WIDTH                                     1
#define F_BWR_CAM_RD_WD10_POS                                        22
#define F_BWR_CAM_RD_WD10_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT10_POS                                      21
#define F_BWR_CAM_SRT_HRT10_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB10_POS                                     19
#define F_BWR_CAM_DRAM_SLB10_WIDTH                                   2
#define F_BWR_CAM_AXI10_POS                                          16
#define F_BWR_CAM_AXI10_WIDTH                                        3
#define F_BWR_CAM_CH_TTL9_POS                                        15
#define F_BWR_CAM_CH_TTL9_WIDTH                                      1
#define F_BWR_CAM_RD_WD9_POS                                         14
#define F_BWR_CAM_RD_WD9_WIDTH                                       1
#define F_BWR_CAM_SRT_HRT9_POS                                       13
#define F_BWR_CAM_SRT_HRT9_WIDTH                                     1
#define F_BWR_CAM_DRAM_SLB9_POS                                      11
#define F_BWR_CAM_DRAM_SLB9_WIDTH                                    2
#define F_BWR_CAM_AXI9_POS                                           8
#define F_BWR_CAM_AXI9_WIDTH                                         3
#define F_BWR_CAM_CH_TTL8_POS                                        7
#define F_BWR_CAM_CH_TTL8_WIDTH                                      1
#define F_BWR_CAM_RD_WD8_POS                                         6
#define F_BWR_CAM_RD_WD8_WIDTH                                       1
#define F_BWR_CAM_SRT_HRT8_POS                                       5
#define F_BWR_CAM_SRT_HRT8_WIDTH                                     1
#define F_BWR_CAM_DRAM_SLB8_POS                                      3
#define F_BWR_CAM_DRAM_SLB8_WIDTH                                    2
#define F_BWR_CAM_AXI8_POS                                           0
#define F_BWR_CAM_AXI8_WIDTH                                         3

#define REG_BWR_CAM_PROTOCOL3                        0xD0
#define F_BWR_CAM_CH_TTL15_POS                                       31
#define F_BWR_CAM_CH_TTL15_WIDTH                                     1
#define F_BWR_CAM_RD_WD15_POS                                        30
#define F_BWR_CAM_RD_WD15_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT15_POS                                      29
#define F_BWR_CAM_SRT_HRT15_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB15_POS                                     27
#define F_BWR_CAM_DRAM_SLB15_WIDTH                                   2
#define F_BWR_CAM_AXI15_POS                                          24
#define F_BWR_CAM_AXI15_WIDTH                                        3
#define F_BWR_CAM_CH_TTL14_POS                                       23
#define F_BWR_CAM_CH_TTL14_WIDTH                                     1
#define F_BWR_CAM_RD_WD14_POS                                        22
#define F_BWR_CAM_RD_WD14_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT14_POS                                      21
#define F_BWR_CAM_SRT_HRT14_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB14_POS                                     19
#define F_BWR_CAM_DRAM_SLB14_WIDTH                                   2
#define F_BWR_CAM_AXI14_POS                                          16
#define F_BWR_CAM_AXI14_WIDTH                                        3
#define F_BWR_CAM_CH_TTL13_POS                                       15
#define F_BWR_CAM_CH_TTL13_WIDTH                                     1
#define F_BWR_CAM_RD_WD13_POS                                        14
#define F_BWR_CAM_RD_WD13_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT13_POS                                      13
#define F_BWR_CAM_SRT_HRT13_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB13_POS                                     11
#define F_BWR_CAM_DRAM_SLB13_WIDTH                                   2
#define F_BWR_CAM_AXI13_POS                                          8
#define F_BWR_CAM_AXI13_WIDTH                                        3
#define F_BWR_CAM_CH_TTL12_POS                                       7
#define F_BWR_CAM_CH_TTL12_WIDTH                                     1
#define F_BWR_CAM_RD_WD12_POS                                        6
#define F_BWR_CAM_RD_WD12_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT12_POS                                      5
#define F_BWR_CAM_SRT_HRT12_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB12_POS                                     3
#define F_BWR_CAM_DRAM_SLB12_WIDTH                                   2
#define F_BWR_CAM_AXI12_POS                                          0
#define F_BWR_CAM_AXI12_WIDTH                                        3

#define REG_BWR_CAM_PROTOCOL4                        0xD4
#define F_BWR_CAM_CH_TTL19_POS                                       31
#define F_BWR_CAM_CH_TTL19_WIDTH                                     1
#define F_BWR_CAM_RD_WD19_POS                                        30
#define F_BWR_CAM_RD_WD19_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT19_POS                                      29
#define F_BWR_CAM_SRT_HRT19_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB19_POS                                     27
#define F_BWR_CAM_DRAM_SLB19_WIDTH                                   2
#define F_BWR_CAM_AXI19_POS                                          24
#define F_BWR_CAM_AXI19_WIDTH                                        3
#define F_BWR_CAM_CH_TTL18_POS                                       23
#define F_BWR_CAM_CH_TTL18_WIDTH                                     1
#define F_BWR_CAM_RD_WD18_POS                                        22
#define F_BWR_CAM_RD_WD18_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT18_POS                                      21
#define F_BWR_CAM_SRT_HRT18_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB18_POS                                     19
#define F_BWR_CAM_DRAM_SLB18_WIDTH                                   2
#define F_BWR_CAM_AXI18_POS                                          16
#define F_BWR_CAM_AXI18_WIDTH                                        3
#define F_BWR_CAM_CH_TTL17_POS                                       15
#define F_BWR_CAM_CH_TTL17_WIDTH                                     1
#define F_BWR_CAM_RD_WD17_POS                                        14
#define F_BWR_CAM_RD_WD17_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT17_POS                                      13
#define F_BWR_CAM_SRT_HRT17_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB17_POS                                     11
#define F_BWR_CAM_DRAM_SLB17_WIDTH                                   2
#define F_BWR_CAM_AXI17_POS                                          8
#define F_BWR_CAM_AXI17_WIDTH                                        3
#define F_BWR_CAM_CH_TTL16_POS                                       7
#define F_BWR_CAM_CH_TTL16_WIDTH                                     1
#define F_BWR_CAM_RD_WD16_POS                                        6
#define F_BWR_CAM_RD_WD16_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT16_POS                                      5
#define F_BWR_CAM_SRT_HRT16_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB16_POS                                     3
#define F_BWR_CAM_DRAM_SLB16_WIDTH                                   2
#define F_BWR_CAM_AXI16_POS                                          0
#define F_BWR_CAM_AXI16_WIDTH                                        3

#define REG_BWR_CAM_PROTOCOL5                        0xD8
#define F_BWR_CAM_CH_TTL23_POS                                       31
#define F_BWR_CAM_CH_TTL23_WIDTH                                     1
#define F_BWR_CAM_RD_WD23_POS                                        30
#define F_BWR_CAM_RD_WD23_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT23_POS                                      29
#define F_BWR_CAM_SRT_HRT23_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB23_POS                                     27
#define F_BWR_CAM_DRAM_SLB23_WIDTH                                   2
#define F_BWR_CAM_AXI23_POS                                          24
#define F_BWR_CAM_AXI23_WIDTH                                        3
#define F_BWR_CAM_CH_TTL22_POS                                       23
#define F_BWR_CAM_CH_TTL22_WIDTH                                     1
#define F_BWR_CAM_RD_WD22_POS                                        22
#define F_BWR_CAM_RD_WD22_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT22_POS                                      21
#define F_BWR_CAM_SRT_HRT22_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB22_POS                                     19
#define F_BWR_CAM_DRAM_SLB22_WIDTH                                   2
#define F_BWR_CAM_AXI22_POS                                          16
#define F_BWR_CAM_AXI22_WIDTH                                        3
#define F_BWR_CAM_CH_TTL21_POS                                       15
#define F_BWR_CAM_CH_TTL21_WIDTH                                     1
#define F_BWR_CAM_RD_WD21_POS                                        14
#define F_BWR_CAM_RD_WD21_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT21_POS                                      13
#define F_BWR_CAM_SRT_HRT21_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB21_POS                                     11
#define F_BWR_CAM_DRAM_SLB21_WIDTH                                   2
#define F_BWR_CAM_AXI21_POS                                          8
#define F_BWR_CAM_AXI21_WIDTH                                        3
#define F_BWR_CAM_CH_TTL20_POS                                       7
#define F_BWR_CAM_CH_TTL20_WIDTH                                     1
#define F_BWR_CAM_RD_WD20_POS                                        6
#define F_BWR_CAM_RD_WD20_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT20_POS                                      5
#define F_BWR_CAM_SRT_HRT20_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB20_POS                                     3
#define F_BWR_CAM_DRAM_SLB20_WIDTH                                   2
#define F_BWR_CAM_AXI20_POS                                          0
#define F_BWR_CAM_AXI20_WIDTH                                        3

#define REG_BWR_CAM_PROTOCOL6                        0xDC
#define F_BWR_CAM_CH_TTL27_POS                                       31
#define F_BWR_CAM_CH_TTL27_WIDTH                                     1
#define F_BWR_CAM_RD_WD27_POS                                        30
#define F_BWR_CAM_RD_WD27_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT27_POS                                      29
#define F_BWR_CAM_SRT_HRT27_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB27_POS                                     27
#define F_BWR_CAM_DRAM_SLB27_WIDTH                                   2
#define F_BWR_CAM_AXI27_POS                                          24
#define F_BWR_CAM_AXI27_WIDTH                                        3
#define F_BWR_CAM_CH_TTL26_POS                                       23
#define F_BWR_CAM_CH_TTL26_WIDTH                                     1
#define F_BWR_CAM_RD_WD26_POS                                        22
#define F_BWR_CAM_RD_WD26_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT26_POS                                      21
#define F_BWR_CAM_SRT_HRT26_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB26_POS                                     19
#define F_BWR_CAM_DRAM_SLB26_WIDTH                                   2
#define F_BWR_CAM_AXI26_POS                                          16
#define F_BWR_CAM_AXI26_WIDTH                                        3
#define F_BWR_CAM_CH_TTL25_POS                                       15
#define F_BWR_CAM_CH_TTL25_WIDTH                                     1
#define F_BWR_CAM_RD_WD25_POS                                        14
#define F_BWR_CAM_RD_WD25_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT25_POS                                      13
#define F_BWR_CAM_SRT_HRT25_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB25_POS                                     11
#define F_BWR_CAM_DRAM_SLB25_WIDTH                                   2
#define F_BWR_CAM_AXI25_POS                                          8
#define F_BWR_CAM_AXI25_WIDTH                                        3
#define F_BWR_CAM_CH_TTL24_POS                                       7
#define F_BWR_CAM_CH_TTL24_WIDTH                                     1
#define F_BWR_CAM_RD_WD24_POS                                        6
#define F_BWR_CAM_RD_WD24_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT24_POS                                      5
#define F_BWR_CAM_SRT_HRT24_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB24_POS                                     3
#define F_BWR_CAM_DRAM_SLB24_WIDTH                                   2
#define F_BWR_CAM_AXI24_POS                                          0
#define F_BWR_CAM_AXI24_WIDTH                                        3

#define REG_BWR_CAM_PROTOCOL7                        0xE0
#define F_BWR_CAM_CH_TTL31_POS                                       31
#define F_BWR_CAM_CH_TTL31_WIDTH                                     1
#define F_BWR_CAM_RD_WD31_POS                                        30
#define F_BWR_CAM_RD_WD31_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT31_POS                                      29
#define F_BWR_CAM_SRT_HRT31_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB31_POS                                     27
#define F_BWR_CAM_DRAM_SLB31_WIDTH                                   2
#define F_BWR_CAM_AXI31_POS                                          24
#define F_BWR_CAM_AXI31_WIDTH                                        3
#define F_BWR_CAM_CH_TTL30_POS                                       23
#define F_BWR_CAM_CH_TTL30_WIDTH                                     1
#define F_BWR_CAM_RD_WD30_POS                                        22
#define F_BWR_CAM_RD_WD30_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT30_POS                                      21
#define F_BWR_CAM_SRT_HRT30_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB30_POS                                     19
#define F_BWR_CAM_DRAM_SLB30_WIDTH                                   2
#define F_BWR_CAM_AXI30_POS                                          16
#define F_BWR_CAM_AXI30_WIDTH                                        3
#define F_BWR_CAM_CH_TTL29_POS                                       15
#define F_BWR_CAM_CH_TTL29_WIDTH                                     1
#define F_BWR_CAM_RD_WD29_POS                                        14
#define F_BWR_CAM_RD_WD29_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT29_POS                                      13
#define F_BWR_CAM_SRT_HRT29_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB29_POS                                     11
#define F_BWR_CAM_DRAM_SLB29_WIDTH                                   2
#define F_BWR_CAM_AXI29_POS                                          8
#define F_BWR_CAM_AXI29_WIDTH                                        3
#define F_BWR_CAM_CH_TTL28_POS                                       7
#define F_BWR_CAM_CH_TTL28_WIDTH                                     1
#define F_BWR_CAM_RD_WD28_POS                                        6
#define F_BWR_CAM_RD_WD28_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT28_POS                                      5
#define F_BWR_CAM_SRT_HRT28_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB28_POS                                     3
#define F_BWR_CAM_DRAM_SLB28_WIDTH                                   2
#define F_BWR_CAM_AXI28_POS                                          0
#define F_BWR_CAM_AXI28_WIDTH                                        3

#define REG_BWR_CAM_PROTOCOL8                        0xE4
#define F_BWR_CAM_CH_TTL35_POS                                       31
#define F_BWR_CAM_CH_TTL35_WIDTH                                     1
#define F_BWR_CAM_RD_WD35_POS                                        30
#define F_BWR_CAM_RD_WD35_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT35_POS                                      29
#define F_BWR_CAM_SRT_HRT35_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB35_POS                                     27
#define F_BWR_CAM_DRAM_SLB35_WIDTH                                   2
#define F_BWR_CAM_AXI35_POS                                          24
#define F_BWR_CAM_AXI35_WIDTH                                        3
#define F_BWR_CAM_CH_TTL34_POS                                       23
#define F_BWR_CAM_CH_TTL34_WIDTH                                     1
#define F_BWR_CAM_RD_WD34_POS                                        22
#define F_BWR_CAM_RD_WD34_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT34_POS                                      21
#define F_BWR_CAM_SRT_HRT34_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB34_POS                                     19
#define F_BWR_CAM_DRAM_SLB34_WIDTH                                   2
#define F_BWR_CAM_AXI34_POS                                          16
#define F_BWR_CAM_AXI34_WIDTH                                        3
#define F_BWR_CAM_CH_TTL33_POS                                       15
#define F_BWR_CAM_CH_TTL33_WIDTH                                     1
#define F_BWR_CAM_RD_WD33_POS                                        14
#define F_BWR_CAM_RD_WD33_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT33_POS                                      13
#define F_BWR_CAM_SRT_HRT33_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB33_POS                                     11
#define F_BWR_CAM_DRAM_SLB33_WIDTH                                   2
#define F_BWR_CAM_AXI33_POS                                          8
#define F_BWR_CAM_AXI33_WIDTH                                        3
#define F_BWR_CAM_CH_TTL32_POS                                       7
#define F_BWR_CAM_CH_TTL32_WIDTH                                     1
#define F_BWR_CAM_RD_WD32_POS                                        6
#define F_BWR_CAM_RD_WD32_WIDTH                                      1
#define F_BWR_CAM_SRT_HRT32_POS                                      5
#define F_BWR_CAM_SRT_HRT32_WIDTH                                    1
#define F_BWR_CAM_DRAM_SLB32_POS                                     3
#define F_BWR_CAM_DRAM_SLB32_WIDTH                                   2
#define F_BWR_CAM_AXI32_POS                                          0
#define F_BWR_CAM_AXI32_WIDTH                                        3

#define REG_BWR_CAM_SRT_SLC_OFLD_BW                 0x100
#define F_BWR_CAM_SRT_SLC_OFLD_BW_POS                                0
#define F_BWR_CAM_SRT_SLC_OFLD_BW_WIDTH                              30

#define REG_BWR_CAM_HRT_SLC_OFLD_BW                 0x104
#define F_BWR_CAM_HRT_SLC_OFLD_BW_POS                                0
#define F_BWR_CAM_HRT_SLC_OFLD_BW_WIDTH                              30

#define REG_BWR_CAM_SRT_EMI_BW_QOS_SEL              0x110
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL11_POS                           11
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL11_WIDTH                         1
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL10_POS                           10
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL10_WIDTH                         1
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL9_POS                            9
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL9_WIDTH                          1
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL8_POS                            8
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL8_WIDTH                          1
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL7_POS                            7
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL7_WIDTH                          1
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL6_POS                            6
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL6_WIDTH                          1
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL5_POS                            5
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL5_WIDTH                          1
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL4_POS                            4
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL4_WIDTH                          1
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL3_POS                            3
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL3_WIDTH                          1
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL2_POS                            2
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL2_WIDTH                          1
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL1_POS                            1
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL1_WIDTH                          1
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL0_POS                            0
#define F_BWR_CAM_SRT_EMI_BW_QOS_SEL0_WIDTH                          1

#define REG_BWR_CAM_SRT_EMI_SW_QOS_TRIG             0x114
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG11_POS                          11
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG11_WIDTH                        1
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG10_POS                          10
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG10_WIDTH                        1
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG9_POS                           9
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG9_WIDTH                         1
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG8_POS                           8
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG8_WIDTH                         1
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG7_POS                           7
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG7_WIDTH                         1
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG6_POS                           6
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG6_WIDTH                         1
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG5_POS                           5
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG5_WIDTH                         1
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG4_POS                           4
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG4_WIDTH                         1
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG3_POS                           3
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG3_WIDTH                         1
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG2_POS                           2
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG2_WIDTH                         1
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG1_POS                           1
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG1_WIDTH                         1
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG0_POS                           0
#define F_BWR_CAM_SRT_EMI_SW_QOS_TRIG0_WIDTH                         1

#define REG_BWR_CAM_SRT_EMI_SW_QOS_EN               0x118
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN11_POS                            11
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN11_WIDTH                          1
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN10_POS                            10
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN10_WIDTH                          1
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN9_POS                             9
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN9_WIDTH                           1
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN8_POS                             8
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN8_WIDTH                           1
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN7_POS                             7
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN7_WIDTH                           1
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN6_POS                             6
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN6_WIDTH                           1
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN5_POS                             5
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN5_WIDTH                           1
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN4_POS                             4
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN4_WIDTH                           1
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN3_POS                             3
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN3_WIDTH                           1
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN2_POS                             2
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN2_WIDTH                           1
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN1_POS                             1
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN1_WIDTH                           1
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN0_POS                             0
#define F_BWR_CAM_SRT_EMI_SW_QOS_EN0_WIDTH                           1

#define REG_BWR_CAM_SRT_EMI_ENG_BW0                 0x11C
#define F_BWR_CAM_SRT_EMI_ENG_BW0_POS                                0
#define F_BWR_CAM_SRT_EMI_ENG_BW0_WIDTH                              18

#define REG_BWR_CAM_SRT_EMI_ENG_BW1                 0x120
#define F_BWR_CAM_SRT_EMI_ENG_BW1_POS                                0
#define F_BWR_CAM_SRT_EMI_ENG_BW1_WIDTH                              18

#define REG_BWR_CAM_SRT_EMI_ENG_BW2                 0x124
#define F_BWR_CAM_SRT_EMI_ENG_BW2_POS                                0
#define F_BWR_CAM_SRT_EMI_ENG_BW2_WIDTH                              18

#define REG_BWR_CAM_SRT_EMI_ENG_BW3                 0x128
#define F_BWR_CAM_SRT_EMI_ENG_BW3_POS                                0
#define F_BWR_CAM_SRT_EMI_ENG_BW3_WIDTH                              18

#define REG_BWR_CAM_SRT_EMI_ENG_BW4                 0x12C
#define F_BWR_CAM_SRT_EMI_ENG_BW4_POS                                0
#define F_BWR_CAM_SRT_EMI_ENG_BW4_WIDTH                              18

#define REG_BWR_CAM_SRT_EMI_ENG_BW5                 0x130
#define F_BWR_CAM_SRT_EMI_ENG_BW5_POS                                0
#define F_BWR_CAM_SRT_EMI_ENG_BW5_WIDTH                              18

#define REG_BWR_CAM_SRT_EMI_ENG_BW6                 0x134
#define F_BWR_CAM_SRT_EMI_ENG_BW6_POS                                0
#define F_BWR_CAM_SRT_EMI_ENG_BW6_WIDTH                              18

#define REG_BWR_CAM_SRT_EMI_ENG_BW7                 0x138
#define F_BWR_CAM_SRT_EMI_ENG_BW7_POS                                0
#define F_BWR_CAM_SRT_EMI_ENG_BW7_WIDTH                              18

#define REG_BWR_CAM_SRT_EMI_ENG_BW8                 0x13C
#define F_BWR_CAM_SRT_EMI_ENG_BW8_POS                                0
#define F_BWR_CAM_SRT_EMI_ENG_BW8_WIDTH                              18

#define REG_BWR_CAM_SRT_EMI_ENG_BW9                 0x140
#define F_BWR_CAM_SRT_EMI_ENG_BW9_POS                                0
#define F_BWR_CAM_SRT_EMI_ENG_BW9_WIDTH                              18

#define REG_BWR_CAM_SRT_EMI_ENG_BW10                0x144
#define F_BWR_CAM_SRT_EMI_ENG_BW10_POS                               0
#define F_BWR_CAM_SRT_EMI_ENG_BW10_WIDTH                             18

#define REG_BWR_CAM_SRT_EMI_ENG_BW11                0x148
#define F_BWR_CAM_SRT_EMI_ENG_BW11_POS                               0
#define F_BWR_CAM_SRT_EMI_ENG_BW11_WIDTH                             18

#define REG_BWR_CAM_SRT_EMI_ENG_BW_RAT0             0x14C
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT0_POS                            0
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT0_WIDTH                          12

#define REG_BWR_CAM_SRT_EMI_ENG_BW_RAT1             0x150
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT1_POS                            0
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT1_WIDTH                          12

#define REG_BWR_CAM_SRT_EMI_ENG_BW_RAT2             0x154
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT2_POS                            0
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT2_WIDTH                          12

#define REG_BWR_CAM_SRT_EMI_ENG_BW_RAT3             0x158
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT3_POS                            0
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT3_WIDTH                          12

#define REG_BWR_CAM_SRT_EMI_ENG_BW_RAT4             0x15C
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT4_POS                            0
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT4_WIDTH                          12

#define REG_BWR_CAM_SRT_EMI_ENG_BW_RAT5             0x160
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT5_POS                            0
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT5_WIDTH                          12

#define REG_BWR_CAM_SRT_EMI_ENG_BW_RAT6             0x164
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT6_POS                            0
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT6_WIDTH                          12

#define REG_BWR_CAM_SRT_EMI_ENG_BW_RAT7             0x168
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT7_POS                            0
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT7_WIDTH                          12

#define REG_BWR_CAM_SRT_EMI_ENG_BW_RAT8             0x16C
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT8_POS                            0
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT8_WIDTH                          12

#define REG_BWR_CAM_SRT_EMI_ENG_BW_RAT9             0x170
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT9_POS                            0
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT9_WIDTH                          12

#define REG_BWR_CAM_SRT_EMI_ENG_BW_RAT10            0x174
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT10_POS                           0
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT10_WIDTH                         12

#define REG_BWR_CAM_SRT_EMI_ENG_BW_RAT11            0x178
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT11_POS                           0
#define F_BWR_CAM_SRT_EMI_ENG_BW_RAT11_WIDTH                         12

#define REG_BWR_CAM_SRT_COH_BW_QOS_SEL              0x190
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL11_POS                           11
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL11_WIDTH                         1
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL10_POS                           10
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL10_WIDTH                         1
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL9_POS                            9
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL9_WIDTH                          1
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL8_POS                            8
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL8_WIDTH                          1
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL7_POS                            7
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL7_WIDTH                          1
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL6_POS                            6
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL6_WIDTH                          1
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL5_POS                            5
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL5_WIDTH                          1
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL4_POS                            4
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL4_WIDTH                          1
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL3_POS                            3
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL3_WIDTH                          1
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL2_POS                            2
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL2_WIDTH                          1
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL1_POS                            1
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL1_WIDTH                          1
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL0_POS                            0
#define F_BWR_CAM_SRT_COH_BW_QOS_SEL0_WIDTH                          1

#define REG_BWR_CAM_SRT_COH_SW_QOS_TRIG             0x194
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG11_POS                          11
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG11_WIDTH                        1
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG10_POS                          10
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG10_WIDTH                        1
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG9_POS                           9
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG9_WIDTH                         1
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG8_POS                           8
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG8_WIDTH                         1
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG7_POS                           7
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG7_WIDTH                         1
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG6_POS                           6
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG6_WIDTH                         1
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG5_POS                           5
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG5_WIDTH                         1
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG4_POS                           4
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG4_WIDTH                         1
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG3_POS                           3
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG3_WIDTH                         1
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG2_POS                           2
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG2_WIDTH                         1
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG1_POS                           1
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG1_WIDTH                         1
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG0_POS                           0
#define F_BWR_CAM_SRT_COH_SW_QOS_TRIG0_WIDTH                         1

#define REG_BWR_CAM_SRT_COH_SW_QOS_EN               0x198
#define F_BWR_CAM_SRT_COH_SW_QOS_EN11_POS                            11
#define F_BWR_CAM_SRT_COH_SW_QOS_EN11_WIDTH                          1
#define F_BWR_CAM_SRT_COH_SW_QOS_EN10_POS                            10
#define F_BWR_CAM_SRT_COH_SW_QOS_EN10_WIDTH                          1
#define F_BWR_CAM_SRT_COH_SW_QOS_EN9_POS                             9
#define F_BWR_CAM_SRT_COH_SW_QOS_EN9_WIDTH                           1
#define F_BWR_CAM_SRT_COH_SW_QOS_EN8_POS                             8
#define F_BWR_CAM_SRT_COH_SW_QOS_EN8_WIDTH                           1
#define F_BWR_CAM_SRT_COH_SW_QOS_EN7_POS                             7
#define F_BWR_CAM_SRT_COH_SW_QOS_EN7_WIDTH                           1
#define F_BWR_CAM_SRT_COH_SW_QOS_EN6_POS                             6
#define F_BWR_CAM_SRT_COH_SW_QOS_EN6_WIDTH                           1
#define F_BWR_CAM_SRT_COH_SW_QOS_EN5_POS                             5
#define F_BWR_CAM_SRT_COH_SW_QOS_EN5_WIDTH                           1
#define F_BWR_CAM_SRT_COH_SW_QOS_EN4_POS                             4
#define F_BWR_CAM_SRT_COH_SW_QOS_EN4_WIDTH                           1
#define F_BWR_CAM_SRT_COH_SW_QOS_EN3_POS                             3
#define F_BWR_CAM_SRT_COH_SW_QOS_EN3_WIDTH                           1
#define F_BWR_CAM_SRT_COH_SW_QOS_EN2_POS                             2
#define F_BWR_CAM_SRT_COH_SW_QOS_EN2_WIDTH                           1
#define F_BWR_CAM_SRT_COH_SW_QOS_EN1_POS                             1
#define F_BWR_CAM_SRT_COH_SW_QOS_EN1_WIDTH                           1
#define F_BWR_CAM_SRT_COH_SW_QOS_EN0_POS                             0
#define F_BWR_CAM_SRT_COH_SW_QOS_EN0_WIDTH                           1

#define REG_BWR_CAM_SRT_COH_ENG_BW0                 0x19C
#define F_BWR_CAM_SRT_COH_ENG_BW0_POS                                0
#define F_BWR_CAM_SRT_COH_ENG_BW0_WIDTH                              18

#define REG_BWR_CAM_SRT_COH_ENG_BW1                 0x1A0
#define F_BWR_CAM_SRT_COH_ENG_BW1_POS                                0
#define F_BWR_CAM_SRT_COH_ENG_BW1_WIDTH                              18

#define REG_BWR_CAM_SRT_COH_ENG_BW2                 0x1A4
#define F_BWR_CAM_SRT_COH_ENG_BW2_POS                                0
#define F_BWR_CAM_SRT_COH_ENG_BW2_WIDTH                              18

#define REG_BWR_CAM_SRT_COH_ENG_BW3                 0x1A8
#define F_BWR_CAM_SRT_COH_ENG_BW3_POS                                0
#define F_BWR_CAM_SRT_COH_ENG_BW3_WIDTH                              18

#define REG_BWR_CAM_SRT_COH_ENG_BW4                 0x1AC
#define F_BWR_CAM_SRT_COH_ENG_BW4_POS                                0
#define F_BWR_CAM_SRT_COH_ENG_BW4_WIDTH                              18

#define REG_BWR_CAM_SRT_COH_ENG_BW5                 0x1B0
#define F_BWR_CAM_SRT_COH_ENG_BW5_POS                                0
#define F_BWR_CAM_SRT_COH_ENG_BW5_WIDTH                              18

#define REG_BWR_CAM_SRT_COH_ENG_BW6                 0x1B4
#define F_BWR_CAM_SRT_COH_ENG_BW6_POS                                0
#define F_BWR_CAM_SRT_COH_ENG_BW6_WIDTH                              18

#define REG_BWR_CAM_SRT_COH_ENG_BW7                 0x1B8
#define F_BWR_CAM_SRT_COH_ENG_BW7_POS                                0
#define F_BWR_CAM_SRT_COH_ENG_BW7_WIDTH                              18

#define REG_BWR_CAM_SRT_COH_ENG_BW8                 0x1BC
#define F_BWR_CAM_SRT_COH_ENG_BW8_POS                                0
#define F_BWR_CAM_SRT_COH_ENG_BW8_WIDTH                              18

#define REG_BWR_CAM_SRT_COH_ENG_BW9                 0x1C0
#define F_BWR_CAM_SRT_COH_ENG_BW9_POS                                0
#define F_BWR_CAM_SRT_COH_ENG_BW9_WIDTH                              18

#define REG_BWR_CAM_SRT_COH_ENG_BW10                0x1C4
#define F_BWR_CAM_SRT_COH_ENG_BW10_POS                               0
#define F_BWR_CAM_SRT_COH_ENG_BW10_WIDTH                             18

#define REG_BWR_CAM_SRT_COH_ENG_BW11                0x1C8
#define F_BWR_CAM_SRT_COH_ENG_BW11_POS                               0
#define F_BWR_CAM_SRT_COH_ENG_BW11_WIDTH                             18

#define REG_BWR_CAM_SRT_COH_ENG_BW_RAT0             0x1CC
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT0_POS                            0
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT0_WIDTH                          12

#define REG_BWR_CAM_SRT_COH_ENG_BW_RAT1             0x1D0
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT1_POS                            0
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT1_WIDTH                          12

#define REG_BWR_CAM_SRT_COH_ENG_BW_RAT2             0x1D4
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT2_POS                            0
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT2_WIDTH                          12

#define REG_BWR_CAM_SRT_COH_ENG_BW_RAT3             0x1D8
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT3_POS                            0
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT3_WIDTH                          12

#define REG_BWR_CAM_SRT_COH_ENG_BW_RAT4             0x1DC
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT4_POS                            0
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT4_WIDTH                          12

#define REG_BWR_CAM_SRT_COH_ENG_BW_RAT5             0x1E0
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT5_POS                            0
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT5_WIDTH                          12

#define REG_BWR_CAM_SRT_COH_ENG_BW_RAT6             0x1E4
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT6_POS                            0
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT6_WIDTH                          12

#define REG_BWR_CAM_SRT_COH_ENG_BW_RAT7             0x1E8
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT7_POS                            0
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT7_WIDTH                          12

#define REG_BWR_CAM_SRT_COH_ENG_BW_RAT8             0x1EC
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT8_POS                            0
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT8_WIDTH                          12

#define REG_BWR_CAM_SRT_COH_ENG_BW_RAT9             0x1F0
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT9_POS                            0
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT9_WIDTH                          12

#define REG_BWR_CAM_SRT_COH_ENG_BW_RAT10            0x1F4
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT10_POS                           0
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT10_WIDTH                         12

#define REG_BWR_CAM_SRT_COH_ENG_BW_RAT11            0x1F8
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT11_POS                           0
#define F_BWR_CAM_SRT_COH_ENG_BW_RAT11_WIDTH                         12

#define REG_BWR_CAM_SRT_R0_BW_QOS_SEL0              0x210
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_SRT_R0_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_SRT_R0_SW_QOS_TRIG0             0x214
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_SRT_R0_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_SRT_R0_SW_QOS_EN0               0x218
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_SRT_R0_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_SRT_R0_ENG_BW0_0                0x21C
#define F_BWR_CAM_SRT_R0_ENG_BW0_0_POS                               0
#define F_BWR_CAM_SRT_R0_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_SRT_R0_ENG_BW0_1                0x220
#define F_BWR_CAM_SRT_R0_ENG_BW0_1_POS                               0
#define F_BWR_CAM_SRT_R0_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_SRT_R0_ENG_BW0_2                0x224
#define F_BWR_CAM_SRT_R0_ENG_BW0_2_POS                               0
#define F_BWR_CAM_SRT_R0_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_SRT_R0_ENG_BW0_3                0x228
#define F_BWR_CAM_SRT_R0_ENG_BW0_3_POS                               0
#define F_BWR_CAM_SRT_R0_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_SRT_R0_ENG_BW0_4                0x22C
#define F_BWR_CAM_SRT_R0_ENG_BW0_4_POS                               0
#define F_BWR_CAM_SRT_R0_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_SRT_R0_ENG_BW0_5                0x230
#define F_BWR_CAM_SRT_R0_ENG_BW0_5_POS                               0
#define F_BWR_CAM_SRT_R0_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_SRT_R0_ENG_BW0_6                0x234
#define F_BWR_CAM_SRT_R0_ENG_BW0_6_POS                               0
#define F_BWR_CAM_SRT_R0_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_SRT_R0_ENG_BW0_7                0x238
#define F_BWR_CAM_SRT_R0_ENG_BW0_7_POS                               0
#define F_BWR_CAM_SRT_R0_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_SRT_R0_ENG_BW0_8                0x23C
#define F_BWR_CAM_SRT_R0_ENG_BW0_8_POS                               0
#define F_BWR_CAM_SRT_R0_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_SRT_R0_ENG_BW0_9                0x240
#define F_BWR_CAM_SRT_R0_ENG_BW0_9_POS                               0
#define F_BWR_CAM_SRT_R0_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_SRT_R0_ENG_BW0_10               0x244
#define F_BWR_CAM_SRT_R0_ENG_BW0_10_POS                              0
#define F_BWR_CAM_SRT_R0_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_SRT_R0_ENG_BW0_11               0x248
#define F_BWR_CAM_SRT_R0_ENG_BW0_11_POS                              0
#define F_BWR_CAM_SRT_R0_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_SRT_R0_ENG_BW_RAT0_0            0x24C
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_SRT_R0_ENG_BW_RAT0_1            0x250
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_SRT_R0_ENG_BW_RAT0_2            0x254
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_SRT_R0_ENG_BW_RAT0_3            0x258
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_SRT_R0_ENG_BW_RAT0_4            0x25C
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_SRT_R0_ENG_BW_RAT0_5            0x260
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_SRT_R0_ENG_BW_RAT0_6            0x264
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_SRT_R0_ENG_BW_RAT0_7            0x268
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_SRT_R0_ENG_BW_RAT0_8            0x26C
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_SRT_R0_ENG_BW_RAT0_9            0x270
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_SRT_R0_ENG_BW_RAT0_10           0x274
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_SRT_R0_ENG_BW_RAT0_11           0x278
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_SRT_R0_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_SRT_R1_BW_QOS_SEL0              0x310
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_SRT_R1_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_SRT_R1_SW_QOS_TRIG0             0x314
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_SRT_R1_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_SRT_R1_SW_QOS_EN0               0x318
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_SRT_R1_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_SRT_R1_ENG_BW0_0                0x31C
#define F_BWR_CAM_SRT_R1_ENG_BW0_0_POS                               0
#define F_BWR_CAM_SRT_R1_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_SRT_R1_ENG_BW0_1                0x320
#define F_BWR_CAM_SRT_R1_ENG_BW0_1_POS                               0
#define F_BWR_CAM_SRT_R1_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_SRT_R1_ENG_BW0_2                0x324
#define F_BWR_CAM_SRT_R1_ENG_BW0_2_POS                               0
#define F_BWR_CAM_SRT_R1_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_SRT_R1_ENG_BW0_3                0x328
#define F_BWR_CAM_SRT_R1_ENG_BW0_3_POS                               0
#define F_BWR_CAM_SRT_R1_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_SRT_R1_ENG_BW0_4                0x32C
#define F_BWR_CAM_SRT_R1_ENG_BW0_4_POS                               0
#define F_BWR_CAM_SRT_R1_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_SRT_R1_ENG_BW0_5                0x330
#define F_BWR_CAM_SRT_R1_ENG_BW0_5_POS                               0
#define F_BWR_CAM_SRT_R1_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_SRT_R1_ENG_BW0_6                0x334
#define F_BWR_CAM_SRT_R1_ENG_BW0_6_POS                               0
#define F_BWR_CAM_SRT_R1_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_SRT_R1_ENG_BW0_7                0x338
#define F_BWR_CAM_SRT_R1_ENG_BW0_7_POS                               0
#define F_BWR_CAM_SRT_R1_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_SRT_R1_ENG_BW0_8                0x33C
#define F_BWR_CAM_SRT_R1_ENG_BW0_8_POS                               0
#define F_BWR_CAM_SRT_R1_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_SRT_R1_ENG_BW0_9                0x340
#define F_BWR_CAM_SRT_R1_ENG_BW0_9_POS                               0
#define F_BWR_CAM_SRT_R1_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_SRT_R1_ENG_BW0_10               0x344
#define F_BWR_CAM_SRT_R1_ENG_BW0_10_POS                              0
#define F_BWR_CAM_SRT_R1_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_SRT_R1_ENG_BW0_11               0x348
#define F_BWR_CAM_SRT_R1_ENG_BW0_11_POS                              0
#define F_BWR_CAM_SRT_R1_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_SRT_R1_ENG_BW_RAT0_0            0x34C
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_SRT_R1_ENG_BW_RAT0_1            0x350
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_SRT_R1_ENG_BW_RAT0_2            0x354
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_SRT_R1_ENG_BW_RAT0_3            0x358
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_SRT_R1_ENG_BW_RAT0_4            0x35C
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_SRT_R1_ENG_BW_RAT0_5            0x360
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_SRT_R1_ENG_BW_RAT0_6            0x364
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_SRT_R1_ENG_BW_RAT0_7            0x368
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_SRT_R1_ENG_BW_RAT0_8            0x36C
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_SRT_R1_ENG_BW_RAT0_9            0x370
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_SRT_R1_ENG_BW_RAT0_10           0x374
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_SRT_R1_ENG_BW_RAT0_11           0x378
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_SRT_R1_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_SRT_R2_BW_QOS_SEL0              0x410
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_SRT_R2_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_SRT_R2_SW_QOS_TRIG0             0x414
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_SRT_R2_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_SRT_R2_SW_QOS_EN0               0x418
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_SRT_R2_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_SRT_R2_ENG_BW0_0                0x41C
#define F_BWR_CAM_SRT_R2_ENG_BW0_0_POS                               0
#define F_BWR_CAM_SRT_R2_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_SRT_R2_ENG_BW0_1                0x420
#define F_BWR_CAM_SRT_R2_ENG_BW0_1_POS                               0
#define F_BWR_CAM_SRT_R2_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_SRT_R2_ENG_BW0_2                0x424
#define F_BWR_CAM_SRT_R2_ENG_BW0_2_POS                               0
#define F_BWR_CAM_SRT_R2_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_SRT_R2_ENG_BW0_3                0x428
#define F_BWR_CAM_SRT_R2_ENG_BW0_3_POS                               0
#define F_BWR_CAM_SRT_R2_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_SRT_R2_ENG_BW0_4                0x42C
#define F_BWR_CAM_SRT_R2_ENG_BW0_4_POS                               0
#define F_BWR_CAM_SRT_R2_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_SRT_R2_ENG_BW0_5                0x430
#define F_BWR_CAM_SRT_R2_ENG_BW0_5_POS                               0
#define F_BWR_CAM_SRT_R2_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_SRT_R2_ENG_BW0_6                0x434
#define F_BWR_CAM_SRT_R2_ENG_BW0_6_POS                               0
#define F_BWR_CAM_SRT_R2_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_SRT_R2_ENG_BW0_7                0x438
#define F_BWR_CAM_SRT_R2_ENG_BW0_7_POS                               0
#define F_BWR_CAM_SRT_R2_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_SRT_R2_ENG_BW0_8                0x43C
#define F_BWR_CAM_SRT_R2_ENG_BW0_8_POS                               0
#define F_BWR_CAM_SRT_R2_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_SRT_R2_ENG_BW0_9                0x440
#define F_BWR_CAM_SRT_R2_ENG_BW0_9_POS                               0
#define F_BWR_CAM_SRT_R2_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_SRT_R2_ENG_BW0_10               0x444
#define F_BWR_CAM_SRT_R2_ENG_BW0_10_POS                              0
#define F_BWR_CAM_SRT_R2_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_SRT_R2_ENG_BW0_11               0x448
#define F_BWR_CAM_SRT_R2_ENG_BW0_11_POS                              0
#define F_BWR_CAM_SRT_R2_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_SRT_R2_ENG_BW_RAT0_0            0x44C
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_SRT_R2_ENG_BW_RAT0_1            0x450
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_SRT_R2_ENG_BW_RAT0_2            0x454
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_SRT_R2_ENG_BW_RAT0_3            0x458
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_SRT_R2_ENG_BW_RAT0_4            0x45C
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_SRT_R2_ENG_BW_RAT0_5            0x460
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_SRT_R2_ENG_BW_RAT0_6            0x464
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_SRT_R2_ENG_BW_RAT0_7            0x468
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_SRT_R2_ENG_BW_RAT0_8            0x46C
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_SRT_R2_ENG_BW_RAT0_9            0x470
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_SRT_R2_ENG_BW_RAT0_10           0x474
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_SRT_R2_ENG_BW_RAT0_11           0x478
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_SRT_R2_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_SRT_R3_BW_QOS_SEL0              0x510
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_SRT_R3_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_SRT_R3_SW_QOS_TRIG0             0x514
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_SRT_R3_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_SRT_R3_SW_QOS_EN0               0x518
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_SRT_R3_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_SRT_R3_ENG_BW0_0                0x51C
#define F_BWR_CAM_SRT_R3_ENG_BW0_0_POS                               0
#define F_BWR_CAM_SRT_R3_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_SRT_R3_ENG_BW0_1                0x520
#define F_BWR_CAM_SRT_R3_ENG_BW0_1_POS                               0
#define F_BWR_CAM_SRT_R3_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_SRT_R3_ENG_BW0_2                0x524
#define F_BWR_CAM_SRT_R3_ENG_BW0_2_POS                               0
#define F_BWR_CAM_SRT_R3_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_SRT_R3_ENG_BW0_3                0x528
#define F_BWR_CAM_SRT_R3_ENG_BW0_3_POS                               0
#define F_BWR_CAM_SRT_R3_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_SRT_R3_ENG_BW0_4                0x52C
#define F_BWR_CAM_SRT_R3_ENG_BW0_4_POS                               0
#define F_BWR_CAM_SRT_R3_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_SRT_R3_ENG_BW0_5                0x530
#define F_BWR_CAM_SRT_R3_ENG_BW0_5_POS                               0
#define F_BWR_CAM_SRT_R3_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_SRT_R3_ENG_BW0_6                0x534
#define F_BWR_CAM_SRT_R3_ENG_BW0_6_POS                               0
#define F_BWR_CAM_SRT_R3_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_SRT_R3_ENG_BW0_7                0x538
#define F_BWR_CAM_SRT_R3_ENG_BW0_7_POS                               0
#define F_BWR_CAM_SRT_R3_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_SRT_R3_ENG_BW0_8                0x53C
#define F_BWR_CAM_SRT_R3_ENG_BW0_8_POS                               0
#define F_BWR_CAM_SRT_R3_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_SRT_R3_ENG_BW0_9                0x540
#define F_BWR_CAM_SRT_R3_ENG_BW0_9_POS                               0
#define F_BWR_CAM_SRT_R3_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_SRT_R3_ENG_BW0_10               0x544
#define F_BWR_CAM_SRT_R3_ENG_BW0_10_POS                              0
#define F_BWR_CAM_SRT_R3_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_SRT_R3_ENG_BW0_11               0x548
#define F_BWR_CAM_SRT_R3_ENG_BW0_11_POS                              0
#define F_BWR_CAM_SRT_R3_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_SRT_R3_ENG_BW_RAT0_0            0x54C
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_SRT_R3_ENG_BW_RAT0_1            0x550
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_SRT_R3_ENG_BW_RAT0_2            0x554
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_SRT_R3_ENG_BW_RAT0_3            0x558
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_SRT_R3_ENG_BW_RAT0_4            0x55C
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_SRT_R3_ENG_BW_RAT0_5            0x560
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_SRT_R3_ENG_BW_RAT0_6            0x564
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_SRT_R3_ENG_BW_RAT0_7            0x568
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_SRT_R3_ENG_BW_RAT0_8            0x56C
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_SRT_R3_ENG_BW_RAT0_9            0x570
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_SRT_R3_ENG_BW_RAT0_10           0x574
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_SRT_R3_ENG_BW_RAT0_11           0x578
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_SRT_R3_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_SRT_R4_BW_QOS_SEL0              0x610
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_SRT_R4_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_SRT_R4_SW_QOS_TRIG0             0x614
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_SRT_R4_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_SRT_R4_SW_QOS_EN0               0x618
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_SRT_R4_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_SRT_R4_ENG_BW0_0                0x61C
#define F_BWR_CAM_SRT_R4_ENG_BW0_0_POS                               0
#define F_BWR_CAM_SRT_R4_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_SRT_R4_ENG_BW0_1                0x620
#define F_BWR_CAM_SRT_R4_ENG_BW0_1_POS                               0
#define F_BWR_CAM_SRT_R4_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_SRT_R4_ENG_BW0_2                0x624
#define F_BWR_CAM_SRT_R4_ENG_BW0_2_POS                               0
#define F_BWR_CAM_SRT_R4_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_SRT_R4_ENG_BW0_3                0x628
#define F_BWR_CAM_SRT_R4_ENG_BW0_3_POS                               0
#define F_BWR_CAM_SRT_R4_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_SRT_R4_ENG_BW0_4                0x62C
#define F_BWR_CAM_SRT_R4_ENG_BW0_4_POS                               0
#define F_BWR_CAM_SRT_R4_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_SRT_R4_ENG_BW0_5                0x630
#define F_BWR_CAM_SRT_R4_ENG_BW0_5_POS                               0
#define F_BWR_CAM_SRT_R4_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_SRT_R4_ENG_BW0_6                0x634
#define F_BWR_CAM_SRT_R4_ENG_BW0_6_POS                               0
#define F_BWR_CAM_SRT_R4_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_SRT_R4_ENG_BW0_7                0x638
#define F_BWR_CAM_SRT_R4_ENG_BW0_7_POS                               0
#define F_BWR_CAM_SRT_R4_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_SRT_R4_ENG_BW0_8                0x63C
#define F_BWR_CAM_SRT_R4_ENG_BW0_8_POS                               0
#define F_BWR_CAM_SRT_R4_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_SRT_R4_ENG_BW0_9                0x640
#define F_BWR_CAM_SRT_R4_ENG_BW0_9_POS                               0
#define F_BWR_CAM_SRT_R4_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_SRT_R4_ENG_BW0_10               0x644
#define F_BWR_CAM_SRT_R4_ENG_BW0_10_POS                              0
#define F_BWR_CAM_SRT_R4_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_SRT_R4_ENG_BW0_11               0x648
#define F_BWR_CAM_SRT_R4_ENG_BW0_11_POS                              0
#define F_BWR_CAM_SRT_R4_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_SRT_R4_ENG_BW_RAT0_0            0x64C
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_SRT_R4_ENG_BW_RAT0_1            0x650
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_SRT_R4_ENG_BW_RAT0_2            0x654
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_SRT_R4_ENG_BW_RAT0_3            0x658
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_SRT_R4_ENG_BW_RAT0_4            0x65C
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_SRT_R4_ENG_BW_RAT0_5            0x660
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_SRT_R4_ENG_BW_RAT0_6            0x664
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_SRT_R4_ENG_BW_RAT0_7            0x668
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_SRT_R4_ENG_BW_RAT0_8            0x66C
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_SRT_R4_ENG_BW_RAT0_9            0x670
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_SRT_R4_ENG_BW_RAT0_10           0x674
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_SRT_R4_ENG_BW_RAT0_11           0x678
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_SRT_R4_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_SRT_W0_BW_QOS_SEL0              0x910
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_SRT_W0_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_SRT_W0_SW_QOS_TRIG0             0x914
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_SRT_W0_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_SRT_W0_SW_QOS_EN0               0x918
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_SRT_W0_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_SRT_W0_ENG_BW0_0                0x91C
#define F_BWR_CAM_SRT_W0_ENG_BW0_0_POS                               0
#define F_BWR_CAM_SRT_W0_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_SRT_W0_ENG_BW0_1                0x920
#define F_BWR_CAM_SRT_W0_ENG_BW0_1_POS                               0
#define F_BWR_CAM_SRT_W0_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_SRT_W0_ENG_BW0_2                0x924
#define F_BWR_CAM_SRT_W0_ENG_BW0_2_POS                               0
#define F_BWR_CAM_SRT_W0_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_SRT_W0_ENG_BW0_3                0x928
#define F_BWR_CAM_SRT_W0_ENG_BW0_3_POS                               0
#define F_BWR_CAM_SRT_W0_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_SRT_W0_ENG_BW0_4                0x92C
#define F_BWR_CAM_SRT_W0_ENG_BW0_4_POS                               0
#define F_BWR_CAM_SRT_W0_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_SRT_W0_ENG_BW0_5                0x930
#define F_BWR_CAM_SRT_W0_ENG_BW0_5_POS                               0
#define F_BWR_CAM_SRT_W0_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_SRT_W0_ENG_BW0_6                0x934
#define F_BWR_CAM_SRT_W0_ENG_BW0_6_POS                               0
#define F_BWR_CAM_SRT_W0_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_SRT_W0_ENG_BW0_7                0x938
#define F_BWR_CAM_SRT_W0_ENG_BW0_7_POS                               0
#define F_BWR_CAM_SRT_W0_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_SRT_W0_ENG_BW0_8                0x93C
#define F_BWR_CAM_SRT_W0_ENG_BW0_8_POS                               0
#define F_BWR_CAM_SRT_W0_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_SRT_W0_ENG_BW0_9                0x940
#define F_BWR_CAM_SRT_W0_ENG_BW0_9_POS                               0
#define F_BWR_CAM_SRT_W0_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_SRT_W0_ENG_BW0_10               0x944
#define F_BWR_CAM_SRT_W0_ENG_BW0_10_POS                              0
#define F_BWR_CAM_SRT_W0_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_SRT_W0_ENG_BW0_11               0x948
#define F_BWR_CAM_SRT_W0_ENG_BW0_11_POS                              0
#define F_BWR_CAM_SRT_W0_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_SRT_W0_ENG_BW_RAT0_0            0x94C
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_SRT_W0_ENG_BW_RAT0_1            0x950
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_SRT_W0_ENG_BW_RAT0_2            0x954
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_SRT_W0_ENG_BW_RAT0_3            0x958
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_SRT_W0_ENG_BW_RAT0_4            0x95C
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_SRT_W0_ENG_BW_RAT0_5            0x960
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_SRT_W0_ENG_BW_RAT0_6            0x964
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_SRT_W0_ENG_BW_RAT0_7            0x968
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_SRT_W0_ENG_BW_RAT0_8            0x96C
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_SRT_W0_ENG_BW_RAT0_9            0x970
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_SRT_W0_ENG_BW_RAT0_10           0x974
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_SRT_W0_ENG_BW_RAT0_11           0x978
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_SRT_W0_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_SRT_W1_BW_QOS_SEL0              0xA10
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_SRT_W1_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_SRT_W1_SW_QOS_TRIG0             0xA14
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_SRT_W1_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_SRT_W1_SW_QOS_EN0               0xA18
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_SRT_W1_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_SRT_W1_ENG_BW0_0                0xA1C
#define F_BWR_CAM_SRT_W1_ENG_BW0_0_POS                               0
#define F_BWR_CAM_SRT_W1_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_SRT_W1_ENG_BW0_1                0xA20
#define F_BWR_CAM_SRT_W1_ENG_BW0_1_POS                               0
#define F_BWR_CAM_SRT_W1_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_SRT_W1_ENG_BW0_2                0xA24
#define F_BWR_CAM_SRT_W1_ENG_BW0_2_POS                               0
#define F_BWR_CAM_SRT_W1_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_SRT_W1_ENG_BW0_3                0xA28
#define F_BWR_CAM_SRT_W1_ENG_BW0_3_POS                               0
#define F_BWR_CAM_SRT_W1_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_SRT_W1_ENG_BW0_4                0xA2C
#define F_BWR_CAM_SRT_W1_ENG_BW0_4_POS                               0
#define F_BWR_CAM_SRT_W1_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_SRT_W1_ENG_BW0_5                0xA30
#define F_BWR_CAM_SRT_W1_ENG_BW0_5_POS                               0
#define F_BWR_CAM_SRT_W1_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_SRT_W1_ENG_BW0_6                0xA34
#define F_BWR_CAM_SRT_W1_ENG_BW0_6_POS                               0
#define F_BWR_CAM_SRT_W1_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_SRT_W1_ENG_BW0_7                0xA38
#define F_BWR_CAM_SRT_W1_ENG_BW0_7_POS                               0
#define F_BWR_CAM_SRT_W1_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_SRT_W1_ENG_BW0_8                0xA3C
#define F_BWR_CAM_SRT_W1_ENG_BW0_8_POS                               0
#define F_BWR_CAM_SRT_W1_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_SRT_W1_ENG_BW0_9                0xA40
#define F_BWR_CAM_SRT_W1_ENG_BW0_9_POS                               0
#define F_BWR_CAM_SRT_W1_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_SRT_W1_ENG_BW0_10               0xA44
#define F_BWR_CAM_SRT_W1_ENG_BW0_10_POS                              0
#define F_BWR_CAM_SRT_W1_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_SRT_W1_ENG_BW0_11               0xA48
#define F_BWR_CAM_SRT_W1_ENG_BW0_11_POS                              0
#define F_BWR_CAM_SRT_W1_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_SRT_W1_ENG_BW_RAT0_0            0xA4C
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_SRT_W1_ENG_BW_RAT0_1            0xA50
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_SRT_W1_ENG_BW_RAT0_2            0xA54
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_SRT_W1_ENG_BW_RAT0_3            0xA58
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_SRT_W1_ENG_BW_RAT0_4            0xA5C
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_SRT_W1_ENG_BW_RAT0_5            0xA60
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_SRT_W1_ENG_BW_RAT0_6            0xA64
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_SRT_W1_ENG_BW_RAT0_7            0xA68
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_SRT_W1_ENG_BW_RAT0_8            0xA6C
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_SRT_W1_ENG_BW_RAT0_9            0xA70
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_SRT_W1_ENG_BW_RAT0_10           0xA74
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_SRT_W1_ENG_BW_RAT0_11           0xA78
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_SRT_W1_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_SRT_W2_BW_QOS_SEL0              0xB10
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_SRT_W2_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_SRT_W2_SW_QOS_TRIG0             0xB14
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_SRT_W2_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_SRT_W2_SW_QOS_EN0               0xB18
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_SRT_W2_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_SRT_W2_ENG_BW0_0                0xB1C
#define F_BWR_CAM_SRT_W2_ENG_BW0_0_POS                               0
#define F_BWR_CAM_SRT_W2_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_SRT_W2_ENG_BW0_1                0xB20
#define F_BWR_CAM_SRT_W2_ENG_BW0_1_POS                               0
#define F_BWR_CAM_SRT_W2_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_SRT_W2_ENG_BW0_2                0xB24
#define F_BWR_CAM_SRT_W2_ENG_BW0_2_POS                               0
#define F_BWR_CAM_SRT_W2_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_SRT_W2_ENG_BW0_3                0xB28
#define F_BWR_CAM_SRT_W2_ENG_BW0_3_POS                               0
#define F_BWR_CAM_SRT_W2_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_SRT_W2_ENG_BW0_4                0xB2C
#define F_BWR_CAM_SRT_W2_ENG_BW0_4_POS                               0
#define F_BWR_CAM_SRT_W2_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_SRT_W2_ENG_BW0_5                0xB30
#define F_BWR_CAM_SRT_W2_ENG_BW0_5_POS                               0
#define F_BWR_CAM_SRT_W2_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_SRT_W2_ENG_BW0_6                0xB34
#define F_BWR_CAM_SRT_W2_ENG_BW0_6_POS                               0
#define F_BWR_CAM_SRT_W2_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_SRT_W2_ENG_BW0_7                0xB38
#define F_BWR_CAM_SRT_W2_ENG_BW0_7_POS                               0
#define F_BWR_CAM_SRT_W2_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_SRT_W2_ENG_BW0_8                0xB3C
#define F_BWR_CAM_SRT_W2_ENG_BW0_8_POS                               0
#define F_BWR_CAM_SRT_W2_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_SRT_W2_ENG_BW0_9                0xB40
#define F_BWR_CAM_SRT_W2_ENG_BW0_9_POS                               0
#define F_BWR_CAM_SRT_W2_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_SRT_W2_ENG_BW0_10               0xB44
#define F_BWR_CAM_SRT_W2_ENG_BW0_10_POS                              0
#define F_BWR_CAM_SRT_W2_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_SRT_W2_ENG_BW0_11               0xB48
#define F_BWR_CAM_SRT_W2_ENG_BW0_11_POS                              0
#define F_BWR_CAM_SRT_W2_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_SRT_W2_ENG_BW_RAT0_0            0xB4C
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_SRT_W2_ENG_BW_RAT0_1            0xB50
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_SRT_W2_ENG_BW_RAT0_2            0xB54
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_SRT_W2_ENG_BW_RAT0_3            0xB58
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_SRT_W2_ENG_BW_RAT0_4            0xB5C
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_SRT_W2_ENG_BW_RAT0_5            0xB60
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_SRT_W2_ENG_BW_RAT0_6            0xB64
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_SRT_W2_ENG_BW_RAT0_7            0xB68
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_SRT_W2_ENG_BW_RAT0_8            0xB6C
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_SRT_W2_ENG_BW_RAT0_9            0xB70
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_SRT_W2_ENG_BW_RAT0_10           0xB74
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_SRT_W2_ENG_BW_RAT0_11           0xB78
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_SRT_W2_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_SRT_W3_BW_QOS_SEL0              0xC10
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_SRT_W3_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_SRT_W3_SW_QOS_TRIG0             0xC14
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_SRT_W3_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_SRT_W3_SW_QOS_EN0               0xC18
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_SRT_W3_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_SRT_W3_ENG_BW0_0                0xC1C
#define F_BWR_CAM_SRT_W3_ENG_BW0_0_POS                               0
#define F_BWR_CAM_SRT_W3_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_SRT_W3_ENG_BW0_1                0xC20
#define F_BWR_CAM_SRT_W3_ENG_BW0_1_POS                               0
#define F_BWR_CAM_SRT_W3_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_SRT_W3_ENG_BW0_2                0xC24
#define F_BWR_CAM_SRT_W3_ENG_BW0_2_POS                               0
#define F_BWR_CAM_SRT_W3_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_SRT_W3_ENG_BW0_3                0xC28
#define F_BWR_CAM_SRT_W3_ENG_BW0_3_POS                               0
#define F_BWR_CAM_SRT_W3_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_SRT_W3_ENG_BW0_4                0xC2C
#define F_BWR_CAM_SRT_W3_ENG_BW0_4_POS                               0
#define F_BWR_CAM_SRT_W3_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_SRT_W3_ENG_BW0_5                0xC30
#define F_BWR_CAM_SRT_W3_ENG_BW0_5_POS                               0
#define F_BWR_CAM_SRT_W3_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_SRT_W3_ENG_BW0_6                0xC34
#define F_BWR_CAM_SRT_W3_ENG_BW0_6_POS                               0
#define F_BWR_CAM_SRT_W3_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_SRT_W3_ENG_BW0_7                0xC38
#define F_BWR_CAM_SRT_W3_ENG_BW0_7_POS                               0
#define F_BWR_CAM_SRT_W3_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_SRT_W3_ENG_BW0_8                0xC3C
#define F_BWR_CAM_SRT_W3_ENG_BW0_8_POS                               0
#define F_BWR_CAM_SRT_W3_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_SRT_W3_ENG_BW0_9                0xC40
#define F_BWR_CAM_SRT_W3_ENG_BW0_9_POS                               0
#define F_BWR_CAM_SRT_W3_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_SRT_W3_ENG_BW0_10               0xC44
#define F_BWR_CAM_SRT_W3_ENG_BW0_10_POS                              0
#define F_BWR_CAM_SRT_W3_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_SRT_W3_ENG_BW0_11               0xC48
#define F_BWR_CAM_SRT_W3_ENG_BW0_11_POS                              0
#define F_BWR_CAM_SRT_W3_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_SRT_W3_ENG_BW_RAT0_0            0xC4C
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_SRT_W3_ENG_BW_RAT0_1            0xC50
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_SRT_W3_ENG_BW_RAT0_2            0xC54
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_SRT_W3_ENG_BW_RAT0_3            0xC58
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_SRT_W3_ENG_BW_RAT0_4            0xC5C
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_SRT_W3_ENG_BW_RAT0_5            0xC60
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_SRT_W3_ENG_BW_RAT0_6            0xC64
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_SRT_W3_ENG_BW_RAT0_7            0xC68
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_SRT_W3_ENG_BW_RAT0_8            0xC6C
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_SRT_W3_ENG_BW_RAT0_9            0xC70
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_SRT_W3_ENG_BW_RAT0_10           0xC74
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_SRT_W3_ENG_BW_RAT0_11           0xC78
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_SRT_W3_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_SRT_W4_BW_QOS_SEL0              0xD10
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_SRT_W4_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_SRT_W4_SW_QOS_TRIG0             0xD14
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_SRT_W4_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_SRT_W4_SW_QOS_EN0               0xD18
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_SRT_W4_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_SRT_W4_ENG_BW0_0                0xD1C
#define F_BWR_CAM_SRT_W4_ENG_BW0_0_POS                               0
#define F_BWR_CAM_SRT_W4_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_SRT_W4_ENG_BW0_1                0xD20
#define F_BWR_CAM_SRT_W4_ENG_BW0_1_POS                               0
#define F_BWR_CAM_SRT_W4_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_SRT_W4_ENG_BW0_2                0xD24
#define F_BWR_CAM_SRT_W4_ENG_BW0_2_POS                               0
#define F_BWR_CAM_SRT_W4_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_SRT_W4_ENG_BW0_3                0xD28
#define F_BWR_CAM_SRT_W4_ENG_BW0_3_POS                               0
#define F_BWR_CAM_SRT_W4_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_SRT_W4_ENG_BW0_4                0xD2C
#define F_BWR_CAM_SRT_W4_ENG_BW0_4_POS                               0
#define F_BWR_CAM_SRT_W4_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_SRT_W4_ENG_BW0_5                0xD30
#define F_BWR_CAM_SRT_W4_ENG_BW0_5_POS                               0
#define F_BWR_CAM_SRT_W4_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_SRT_W4_ENG_BW0_6                0xD34
#define F_BWR_CAM_SRT_W4_ENG_BW0_6_POS                               0
#define F_BWR_CAM_SRT_W4_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_SRT_W4_ENG_BW0_7                0xD38
#define F_BWR_CAM_SRT_W4_ENG_BW0_7_POS                               0
#define F_BWR_CAM_SRT_W4_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_SRT_W4_ENG_BW0_8                0xD3C
#define F_BWR_CAM_SRT_W4_ENG_BW0_8_POS                               0
#define F_BWR_CAM_SRT_W4_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_SRT_W4_ENG_BW0_9                0xD40
#define F_BWR_CAM_SRT_W4_ENG_BW0_9_POS                               0
#define F_BWR_CAM_SRT_W4_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_SRT_W4_ENG_BW0_10               0xD44
#define F_BWR_CAM_SRT_W4_ENG_BW0_10_POS                              0
#define F_BWR_CAM_SRT_W4_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_SRT_W4_ENG_BW0_11               0xD48
#define F_BWR_CAM_SRT_W4_ENG_BW0_11_POS                              0
#define F_BWR_CAM_SRT_W4_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_SRT_W4_ENG_BW_RAT0_0            0xD4C
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_SRT_W4_ENG_BW_RAT0_1            0xD50
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_SRT_W4_ENG_BW_RAT0_2            0xD54
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_SRT_W4_ENG_BW_RAT0_3            0xD58
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_SRT_W4_ENG_BW_RAT0_4            0xD5C
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_SRT_W4_ENG_BW_RAT0_5            0xD60
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_SRT_W4_ENG_BW_RAT0_6            0xD64
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_SRT_W4_ENG_BW_RAT0_7            0xD68
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_SRT_W4_ENG_BW_RAT0_8            0xD6C
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_SRT_W4_ENG_BW_RAT0_9            0xD70
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_SRT_W4_ENG_BW_RAT0_10           0xD74
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_SRT_W4_ENG_BW_RAT0_11           0xD78
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_SRT_W4_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_HRT_EMI_BW_QOS_SEL             0x1010
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL11_POS                           11
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL11_WIDTH                         1
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL10_POS                           10
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL10_WIDTH                         1
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL9_POS                            9
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL9_WIDTH                          1
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL8_POS                            8
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL8_WIDTH                          1
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL7_POS                            7
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL7_WIDTH                          1
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL6_POS                            6
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL6_WIDTH                          1
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL5_POS                            5
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL5_WIDTH                          1
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL4_POS                            4
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL4_WIDTH                          1
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL3_POS                            3
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL3_WIDTH                          1
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL2_POS                            2
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL2_WIDTH                          1
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL1_POS                            1
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL1_WIDTH                          1
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL0_POS                            0
#define F_BWR_CAM_HRT_EMI_BW_QOS_SEL0_WIDTH                          1

#define REG_BWR_CAM_HRT_EMI_SW_QOS_TRIG            0x1014
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG11_POS                          11
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG11_WIDTH                        1
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG10_POS                          10
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG10_WIDTH                        1
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG9_POS                           9
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG9_WIDTH                         1
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG8_POS                           8
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG8_WIDTH                         1
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG7_POS                           7
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG7_WIDTH                         1
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG6_POS                           6
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG6_WIDTH                         1
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG5_POS                           5
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG5_WIDTH                         1
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG4_POS                           4
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG4_WIDTH                         1
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG3_POS                           3
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG3_WIDTH                         1
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG2_POS                           2
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG2_WIDTH                         1
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG1_POS                           1
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG1_WIDTH                         1
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG0_POS                           0
#define F_BWR_CAM_HRT_EMI_SW_QOS_TRIG0_WIDTH                         1

#define REG_BWR_CAM_HRT_EMI_SW_QOS_EN              0x1018
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN11_POS                            11
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN11_WIDTH                          1
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN10_POS                            10
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN10_WIDTH                          1
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN9_POS                             9
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN9_WIDTH                           1
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN8_POS                             8
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN8_WIDTH                           1
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN7_POS                             7
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN7_WIDTH                           1
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN6_POS                             6
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN6_WIDTH                           1
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN5_POS                             5
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN5_WIDTH                           1
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN4_POS                             4
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN4_WIDTH                           1
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN3_POS                             3
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN3_WIDTH                           1
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN2_POS                             2
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN2_WIDTH                           1
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN1_POS                             1
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN1_WIDTH                           1
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN0_POS                             0
#define F_BWR_CAM_HRT_EMI_SW_QOS_EN0_WIDTH                           1

#define REG_BWR_CAM_HRT_EMI_ENG_BW0                0x101C
#define F_BWR_CAM_HRT_EMI_ENG_BW0_POS                                0
#define F_BWR_CAM_HRT_EMI_ENG_BW0_WIDTH                              18

#define REG_BWR_CAM_HRT_EMI_ENG_BW1                0x1020
#define F_BWR_CAM_HRT_EMI_ENG_BW1_POS                                0
#define F_BWR_CAM_HRT_EMI_ENG_BW1_WIDTH                              18

#define REG_BWR_CAM_HRT_EMI_ENG_BW2                0x1024
#define F_BWR_CAM_HRT_EMI_ENG_BW2_POS                                0
#define F_BWR_CAM_HRT_EMI_ENG_BW2_WIDTH                              18

#define REG_BWR_CAM_HRT_EMI_ENG_BW3                0x1028
#define F_BWR_CAM_HRT_EMI_ENG_BW3_POS                                0
#define F_BWR_CAM_HRT_EMI_ENG_BW3_WIDTH                              18

#define REG_BWR_CAM_HRT_EMI_ENG_BW4                0x102C
#define F_BWR_CAM_HRT_EMI_ENG_BW4_POS                                0
#define F_BWR_CAM_HRT_EMI_ENG_BW4_WIDTH                              18

#define REG_BWR_CAM_HRT_EMI_ENG_BW5                0x1030
#define F_BWR_CAM_HRT_EMI_ENG_BW5_POS                                0
#define F_BWR_CAM_HRT_EMI_ENG_BW5_WIDTH                              18

#define REG_BWR_CAM_HRT_EMI_ENG_BW6                0x1034
#define F_BWR_CAM_HRT_EMI_ENG_BW6_POS                                0
#define F_BWR_CAM_HRT_EMI_ENG_BW6_WIDTH                              18

#define REG_BWR_CAM_HRT_EMI_ENG_BW7                0x1038
#define F_BWR_CAM_HRT_EMI_ENG_BW7_POS                                0
#define F_BWR_CAM_HRT_EMI_ENG_BW7_WIDTH                              18

#define REG_BWR_CAM_HRT_EMI_ENG_BW8                0x103C
#define F_BWR_CAM_HRT_EMI_ENG_BW8_POS                                0
#define F_BWR_CAM_HRT_EMI_ENG_BW8_WIDTH                              18

#define REG_BWR_CAM_HRT_EMI_ENG_BW9                0x1040
#define F_BWR_CAM_HRT_EMI_ENG_BW9_POS                                0
#define F_BWR_CAM_HRT_EMI_ENG_BW9_WIDTH                              18

#define REG_BWR_CAM_HRT_EMI_ENG_BW10               0x1044
#define F_BWR_CAM_HRT_EMI_ENG_BW10_POS                               0
#define F_BWR_CAM_HRT_EMI_ENG_BW10_WIDTH                             18

#define REG_BWR_CAM_HRT_EMI_ENG_BW11               0x1048
#define F_BWR_CAM_HRT_EMI_ENG_BW11_POS                               0
#define F_BWR_CAM_HRT_EMI_ENG_BW11_WIDTH                             18

#define REG_BWR_CAM_HRT_EMI_ENG_BW_RAT0            0x104C
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT0_POS                            0
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT0_WIDTH                          12

#define REG_BWR_CAM_HRT_EMI_ENG_BW_RAT1            0x1050
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT1_POS                            0
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT1_WIDTH                          12

#define REG_BWR_CAM_HRT_EMI_ENG_BW_RAT2            0x1054
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT2_POS                            0
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT2_WIDTH                          12

#define REG_BWR_CAM_HRT_EMI_ENG_BW_RAT3            0x1058
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT3_POS                            0
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT3_WIDTH                          12

#define REG_BWR_CAM_HRT_EMI_ENG_BW_RAT4            0x105C
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT4_POS                            0
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT4_WIDTH                          12

#define REG_BWR_CAM_HRT_EMI_ENG_BW_RAT5            0x1060
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT5_POS                            0
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT5_WIDTH                          12

#define REG_BWR_CAM_HRT_EMI_ENG_BW_RAT6            0x1064
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT6_POS                            0
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT6_WIDTH                          12

#define REG_BWR_CAM_HRT_EMI_ENG_BW_RAT7            0x1068
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT7_POS                            0
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT7_WIDTH                          12

#define REG_BWR_CAM_HRT_EMI_ENG_BW_RAT8            0x106C
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT8_POS                            0
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT8_WIDTH                          12

#define REG_BWR_CAM_HRT_EMI_ENG_BW_RAT9            0x1070
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT9_POS                            0
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT9_WIDTH                          12

#define REG_BWR_CAM_HRT_EMI_ENG_BW_RAT10           0x1074
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT10_POS                           0
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT10_WIDTH                         12

#define REG_BWR_CAM_HRT_EMI_ENG_BW_RAT11           0x1078
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT11_POS                           0
#define F_BWR_CAM_HRT_EMI_ENG_BW_RAT11_WIDTH                         12

#define REG_BWR_CAM_HRT_COH_BW_QOS_SEL             0x1090
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL11_POS                           11
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL11_WIDTH                         1
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL10_POS                           10
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL10_WIDTH                         1
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL9_POS                            9
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL9_WIDTH                          1
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL8_POS                            8
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL8_WIDTH                          1
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL7_POS                            7
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL7_WIDTH                          1
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL6_POS                            6
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL6_WIDTH                          1
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL5_POS                            5
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL5_WIDTH                          1
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL4_POS                            4
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL4_WIDTH                          1
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL3_POS                            3
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL3_WIDTH                          1
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL2_POS                            2
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL2_WIDTH                          1
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL1_POS                            1
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL1_WIDTH                          1
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL0_POS                            0
#define F_BWR_CAM_HRT_COH_BW_QOS_SEL0_WIDTH                          1

#define REG_BWR_CAM_HRT_COH_SW_QOS_TRIG            0x1094
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG11_POS                          11
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG11_WIDTH                        1
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG10_POS                          10
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG10_WIDTH                        1
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG9_POS                           9
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG9_WIDTH                         1
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG8_POS                           8
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG8_WIDTH                         1
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG7_POS                           7
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG7_WIDTH                         1
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG6_POS                           6
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG6_WIDTH                         1
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG5_POS                           5
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG5_WIDTH                         1
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG4_POS                           4
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG4_WIDTH                         1
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG3_POS                           3
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG3_WIDTH                         1
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG2_POS                           2
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG2_WIDTH                         1
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG1_POS                           1
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG1_WIDTH                         1
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG0_POS                           0
#define F_BWR_CAM_HRT_COH_SW_QOS_TRIG0_WIDTH                         1

#define REG_BWR_CAM_HRT_COH_SW_QOS_EN              0x1098
#define F_BWR_CAM_HRT_COH_SW_QOS_EN11_POS                            11
#define F_BWR_CAM_HRT_COH_SW_QOS_EN11_WIDTH                          1
#define F_BWR_CAM_HRT_COH_SW_QOS_EN10_POS                            10
#define F_BWR_CAM_HRT_COH_SW_QOS_EN10_WIDTH                          1
#define F_BWR_CAM_HRT_COH_SW_QOS_EN9_POS                             9
#define F_BWR_CAM_HRT_COH_SW_QOS_EN9_WIDTH                           1
#define F_BWR_CAM_HRT_COH_SW_QOS_EN8_POS                             8
#define F_BWR_CAM_HRT_COH_SW_QOS_EN8_WIDTH                           1
#define F_BWR_CAM_HRT_COH_SW_QOS_EN7_POS                             7
#define F_BWR_CAM_HRT_COH_SW_QOS_EN7_WIDTH                           1
#define F_BWR_CAM_HRT_COH_SW_QOS_EN6_POS                             6
#define F_BWR_CAM_HRT_COH_SW_QOS_EN6_WIDTH                           1
#define F_BWR_CAM_HRT_COH_SW_QOS_EN5_POS                             5
#define F_BWR_CAM_HRT_COH_SW_QOS_EN5_WIDTH                           1
#define F_BWR_CAM_HRT_COH_SW_QOS_EN4_POS                             4
#define F_BWR_CAM_HRT_COH_SW_QOS_EN4_WIDTH                           1
#define F_BWR_CAM_HRT_COH_SW_QOS_EN3_POS                             3
#define F_BWR_CAM_HRT_COH_SW_QOS_EN3_WIDTH                           1
#define F_BWR_CAM_HRT_COH_SW_QOS_EN2_POS                             2
#define F_BWR_CAM_HRT_COH_SW_QOS_EN2_WIDTH                           1
#define F_BWR_CAM_HRT_COH_SW_QOS_EN1_POS                             1
#define F_BWR_CAM_HRT_COH_SW_QOS_EN1_WIDTH                           1
#define F_BWR_CAM_HRT_COH_SW_QOS_EN0_POS                             0
#define F_BWR_CAM_HRT_COH_SW_QOS_EN0_WIDTH                           1

#define REG_BWR_CAM_HRT_COH_ENG_BW0                0x109C
#define F_BWR_CAM_HRT_COH_ENG_BW0_POS                                0
#define F_BWR_CAM_HRT_COH_ENG_BW0_WIDTH                              18

#define REG_BWR_CAM_HRT_COH_ENG_BW1                0x10A0
#define F_BWR_CAM_HRT_COH_ENG_BW1_POS                                0
#define F_BWR_CAM_HRT_COH_ENG_BW1_WIDTH                              18

#define REG_BWR_CAM_HRT_COH_ENG_BW2                0x10A4
#define F_BWR_CAM_HRT_COH_ENG_BW2_POS                                0
#define F_BWR_CAM_HRT_COH_ENG_BW2_WIDTH                              18

#define REG_BWR_CAM_HRT_COH_ENG_BW3                0x10A8
#define F_BWR_CAM_HRT_COH_ENG_BW3_POS                                0
#define F_BWR_CAM_HRT_COH_ENG_BW3_WIDTH                              18

#define REG_BWR_CAM_HRT_COH_ENG_BW4                0x10AC
#define F_BWR_CAM_HRT_COH_ENG_BW4_POS                                0
#define F_BWR_CAM_HRT_COH_ENG_BW4_WIDTH                              18

#define REG_BWR_CAM_HRT_COH_ENG_BW5                0x10B0
#define F_BWR_CAM_HRT_COH_ENG_BW5_POS                                0
#define F_BWR_CAM_HRT_COH_ENG_BW5_WIDTH                              18

#define REG_BWR_CAM_HRT_COH_ENG_BW6                0x10B4
#define F_BWR_CAM_HRT_COH_ENG_BW6_POS                                0
#define F_BWR_CAM_HRT_COH_ENG_BW6_WIDTH                              18

#define REG_BWR_CAM_HRT_COH_ENG_BW7                0x10B8
#define F_BWR_CAM_HRT_COH_ENG_BW7_POS                                0
#define F_BWR_CAM_HRT_COH_ENG_BW7_WIDTH                              18

#define REG_BWR_CAM_HRT_COH_ENG_BW8                0x10BC
#define F_BWR_CAM_HRT_COH_ENG_BW8_POS                                0
#define F_BWR_CAM_HRT_COH_ENG_BW8_WIDTH                              18

#define REG_BWR_CAM_HRT_COH_ENG_BW9                0x10C0
#define F_BWR_CAM_HRT_COH_ENG_BW9_POS                                0
#define F_BWR_CAM_HRT_COH_ENG_BW9_WIDTH                              18

#define REG_BWR_CAM_HRT_COH_ENG_BW10               0x10C4
#define F_BWR_CAM_HRT_COH_ENG_BW10_POS                               0
#define F_BWR_CAM_HRT_COH_ENG_BW10_WIDTH                             18

#define REG_BWR_CAM_HRT_COH_ENG_BW11               0x10C8
#define F_BWR_CAM_HRT_COH_ENG_BW11_POS                               0
#define F_BWR_CAM_HRT_COH_ENG_BW11_WIDTH                             18

#define REG_BWR_CAM_HRT_COH_ENG_BW_RAT0            0x10CC
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT0_POS                            0
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT0_WIDTH                          12

#define REG_BWR_CAM_HRT_COH_ENG_BW_RAT1            0x10D0
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT1_POS                            0
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT1_WIDTH                          12

#define REG_BWR_CAM_HRT_COH_ENG_BW_RAT2            0x10D4
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT2_POS                            0
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT2_WIDTH                          12

#define REG_BWR_CAM_HRT_COH_ENG_BW_RAT3            0x10D8
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT3_POS                            0
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT3_WIDTH                          12

#define REG_BWR_CAM_HRT_COH_ENG_BW_RAT4            0x10DC
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT4_POS                            0
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT4_WIDTH                          12

#define REG_BWR_CAM_HRT_COH_ENG_BW_RAT5            0x10E0
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT5_POS                            0
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT5_WIDTH                          12

#define REG_BWR_CAM_HRT_COH_ENG_BW_RAT6            0x10E4
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT6_POS                            0
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT6_WIDTH                          12

#define REG_BWR_CAM_HRT_COH_ENG_BW_RAT7            0x10E8
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT7_POS                            0
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT7_WIDTH                          12

#define REG_BWR_CAM_HRT_COH_ENG_BW_RAT8            0x10EC
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT8_POS                            0
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT8_WIDTH                          12

#define REG_BWR_CAM_HRT_COH_ENG_BW_RAT9            0x10F0
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT9_POS                            0
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT9_WIDTH                          12

#define REG_BWR_CAM_HRT_COH_ENG_BW_RAT10           0x10F4
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT10_POS                           0
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT10_WIDTH                         12

#define REG_BWR_CAM_HRT_COH_ENG_BW_RAT11           0x10F8
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT11_POS                           0
#define F_BWR_CAM_HRT_COH_ENG_BW_RAT11_WIDTH                         12

#define REG_BWR_CAM_HRT_R0_BW_QOS_SEL0             0x1110
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_HRT_R0_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_HRT_R0_SW_QOS_TRIG0            0x1114
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_HRT_R0_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_HRT_R0_SW_QOS_EN0              0x1118
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_HRT_R0_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_HRT_R0_ENG_BW0_0               0x111C
#define F_BWR_CAM_HRT_R0_ENG_BW0_0_POS                               0
#define F_BWR_CAM_HRT_R0_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_HRT_R0_ENG_BW0_1               0x1120
#define F_BWR_CAM_HRT_R0_ENG_BW0_1_POS                               0
#define F_BWR_CAM_HRT_R0_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_HRT_R0_ENG_BW0_2               0x1124
#define F_BWR_CAM_HRT_R0_ENG_BW0_2_POS                               0
#define F_BWR_CAM_HRT_R0_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_HRT_R0_ENG_BW0_3               0x1128
#define F_BWR_CAM_HRT_R0_ENG_BW0_3_POS                               0
#define F_BWR_CAM_HRT_R0_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_HRT_R0_ENG_BW0_4               0x112C
#define F_BWR_CAM_HRT_R0_ENG_BW0_4_POS                               0
#define F_BWR_CAM_HRT_R0_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_HRT_R0_ENG_BW0_5               0x1130
#define F_BWR_CAM_HRT_R0_ENG_BW0_5_POS                               0
#define F_BWR_CAM_HRT_R0_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_HRT_R0_ENG_BW0_6               0x1134
#define F_BWR_CAM_HRT_R0_ENG_BW0_6_POS                               0
#define F_BWR_CAM_HRT_R0_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_HRT_R0_ENG_BW0_7               0x1138
#define F_BWR_CAM_HRT_R0_ENG_BW0_7_POS                               0
#define F_BWR_CAM_HRT_R0_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_HRT_R0_ENG_BW0_8               0x113C
#define F_BWR_CAM_HRT_R0_ENG_BW0_8_POS                               0
#define F_BWR_CAM_HRT_R0_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_HRT_R0_ENG_BW0_9               0x1140
#define F_BWR_CAM_HRT_R0_ENG_BW0_9_POS                               0
#define F_BWR_CAM_HRT_R0_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_HRT_R0_ENG_BW0_10              0x1144
#define F_BWR_CAM_HRT_R0_ENG_BW0_10_POS                              0
#define F_BWR_CAM_HRT_R0_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_HRT_R0_ENG_BW0_11              0x1148
#define F_BWR_CAM_HRT_R0_ENG_BW0_11_POS                              0
#define F_BWR_CAM_HRT_R0_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_HRT_R0_ENG_BW_RAT0_0           0x114C
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_HRT_R0_ENG_BW_RAT0_1           0x1150
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_HRT_R0_ENG_BW_RAT0_2           0x1154
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_HRT_R0_ENG_BW_RAT0_3           0x1158
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_HRT_R0_ENG_BW_RAT0_4           0x115C
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_HRT_R0_ENG_BW_RAT0_5           0x1160
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_HRT_R0_ENG_BW_RAT0_6           0x1164
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_HRT_R0_ENG_BW_RAT0_7           0x1168
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_HRT_R0_ENG_BW_RAT0_8           0x116C
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_HRT_R0_ENG_BW_RAT0_9           0x1170
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_HRT_R0_ENG_BW_RAT0_10          0x1174
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_HRT_R0_ENG_BW_RAT0_11          0x1178
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_HRT_R0_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_HRT_R1_BW_QOS_SEL0             0x1210
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_HRT_R1_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_HRT_R1_SW_QOS_TRIG0            0x1214
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_HRT_R1_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_HRT_R1_SW_QOS_EN0              0x1218
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_HRT_R1_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_HRT_R1_ENG_BW0_0               0x121C
#define F_BWR_CAM_HRT_R1_ENG_BW0_0_POS                               0
#define F_BWR_CAM_HRT_R1_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_HRT_R1_ENG_BW0_1               0x1220
#define F_BWR_CAM_HRT_R1_ENG_BW0_1_POS                               0
#define F_BWR_CAM_HRT_R1_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_HRT_R1_ENG_BW0_2               0x1224
#define F_BWR_CAM_HRT_R1_ENG_BW0_2_POS                               0
#define F_BWR_CAM_HRT_R1_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_HRT_R1_ENG_BW0_3               0x1228
#define F_BWR_CAM_HRT_R1_ENG_BW0_3_POS                               0
#define F_BWR_CAM_HRT_R1_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_HRT_R1_ENG_BW0_4               0x122C
#define F_BWR_CAM_HRT_R1_ENG_BW0_4_POS                               0
#define F_BWR_CAM_HRT_R1_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_HRT_R1_ENG_BW0_5               0x1230
#define F_BWR_CAM_HRT_R1_ENG_BW0_5_POS                               0
#define F_BWR_CAM_HRT_R1_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_HRT_R1_ENG_BW0_6               0x1234
#define F_BWR_CAM_HRT_R1_ENG_BW0_6_POS                               0
#define F_BWR_CAM_HRT_R1_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_HRT_R1_ENG_BW0_7               0x1238
#define F_BWR_CAM_HRT_R1_ENG_BW0_7_POS                               0
#define F_BWR_CAM_HRT_R1_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_HRT_R1_ENG_BW0_8               0x123C
#define F_BWR_CAM_HRT_R1_ENG_BW0_8_POS                               0
#define F_BWR_CAM_HRT_R1_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_HRT_R1_ENG_BW0_9               0x1240
#define F_BWR_CAM_HRT_R1_ENG_BW0_9_POS                               0
#define F_BWR_CAM_HRT_R1_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_HRT_R1_ENG_BW0_10              0x1244
#define F_BWR_CAM_HRT_R1_ENG_BW0_10_POS                              0
#define F_BWR_CAM_HRT_R1_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_HRT_R1_ENG_BW0_11              0x1248
#define F_BWR_CAM_HRT_R1_ENG_BW0_11_POS                              0
#define F_BWR_CAM_HRT_R1_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_HRT_R1_ENG_BW_RAT0_0           0x124C
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_HRT_R1_ENG_BW_RAT0_1           0x1250
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_HRT_R1_ENG_BW_RAT0_2           0x1254
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_HRT_R1_ENG_BW_RAT0_3           0x1258
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_HRT_R1_ENG_BW_RAT0_4           0x125C
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_HRT_R1_ENG_BW_RAT0_5           0x1260
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_HRT_R1_ENG_BW_RAT0_6           0x1264
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_HRT_R1_ENG_BW_RAT0_7           0x1268
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_HRT_R1_ENG_BW_RAT0_8           0x126C
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_HRT_R1_ENG_BW_RAT0_9           0x1270
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_HRT_R1_ENG_BW_RAT0_10          0x1274
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_HRT_R1_ENG_BW_RAT0_11          0x1278
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_HRT_R1_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_HRT_R2_BW_QOS_SEL0             0x1310
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_HRT_R2_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_HRT_R2_SW_QOS_TRIG0            0x1314
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_HRT_R2_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_HRT_R2_SW_QOS_EN0              0x1318
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_HRT_R2_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_HRT_R2_ENG_BW0_0               0x131C
#define F_BWR_CAM_HRT_R2_ENG_BW0_0_POS                               0
#define F_BWR_CAM_HRT_R2_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_HRT_R2_ENG_BW0_1               0x1320
#define F_BWR_CAM_HRT_R2_ENG_BW0_1_POS                               0
#define F_BWR_CAM_HRT_R2_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_HRT_R2_ENG_BW0_2               0x1324
#define F_BWR_CAM_HRT_R2_ENG_BW0_2_POS                               0
#define F_BWR_CAM_HRT_R2_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_HRT_R2_ENG_BW0_3               0x1328
#define F_BWR_CAM_HRT_R2_ENG_BW0_3_POS                               0
#define F_BWR_CAM_HRT_R2_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_HRT_R2_ENG_BW0_4               0x132C
#define F_BWR_CAM_HRT_R2_ENG_BW0_4_POS                               0
#define F_BWR_CAM_HRT_R2_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_HRT_R2_ENG_BW0_5               0x1330
#define F_BWR_CAM_HRT_R2_ENG_BW0_5_POS                               0
#define F_BWR_CAM_HRT_R2_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_HRT_R2_ENG_BW0_6               0x1334
#define F_BWR_CAM_HRT_R2_ENG_BW0_6_POS                               0
#define F_BWR_CAM_HRT_R2_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_HRT_R2_ENG_BW0_7               0x1338
#define F_BWR_CAM_HRT_R2_ENG_BW0_7_POS                               0
#define F_BWR_CAM_HRT_R2_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_HRT_R2_ENG_BW0_8               0x133C
#define F_BWR_CAM_HRT_R2_ENG_BW0_8_POS                               0
#define F_BWR_CAM_HRT_R2_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_HRT_R2_ENG_BW0_9               0x1340
#define F_BWR_CAM_HRT_R2_ENG_BW0_9_POS                               0
#define F_BWR_CAM_HRT_R2_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_HRT_R2_ENG_BW0_10              0x1344
#define F_BWR_CAM_HRT_R2_ENG_BW0_10_POS                              0
#define F_BWR_CAM_HRT_R2_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_HRT_R2_ENG_BW0_11              0x1348
#define F_BWR_CAM_HRT_R2_ENG_BW0_11_POS                              0
#define F_BWR_CAM_HRT_R2_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_HRT_R2_ENG_BW_RAT0_0           0x134C
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_HRT_R2_ENG_BW_RAT0_1           0x1350
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_HRT_R2_ENG_BW_RAT0_2           0x1354
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_HRT_R2_ENG_BW_RAT0_3           0x1358
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_HRT_R2_ENG_BW_RAT0_4           0x135C
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_HRT_R2_ENG_BW_RAT0_5           0x1360
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_HRT_R2_ENG_BW_RAT0_6           0x1364
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_HRT_R2_ENG_BW_RAT0_7           0x1368
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_HRT_R2_ENG_BW_RAT0_8           0x136C
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_HRT_R2_ENG_BW_RAT0_9           0x1370
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_HRT_R2_ENG_BW_RAT0_10          0x1374
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_HRT_R2_ENG_BW_RAT0_11          0x1378
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_HRT_R2_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_HRT_R3_BW_QOS_SEL0             0x1410
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_HRT_R3_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_HRT_R3_SW_QOS_TRIG0            0x1414
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_HRT_R3_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_HRT_R3_SW_QOS_EN0              0x1418
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_HRT_R3_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_HRT_R3_ENG_BW0_0               0x141C
#define F_BWR_CAM_HRT_R3_ENG_BW0_0_POS                               0
#define F_BWR_CAM_HRT_R3_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_HRT_R3_ENG_BW0_1               0x1420
#define F_BWR_CAM_HRT_R3_ENG_BW0_1_POS                               0
#define F_BWR_CAM_HRT_R3_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_HRT_R3_ENG_BW0_2               0x1424
#define F_BWR_CAM_HRT_R3_ENG_BW0_2_POS                               0
#define F_BWR_CAM_HRT_R3_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_HRT_R3_ENG_BW0_3               0x1428
#define F_BWR_CAM_HRT_R3_ENG_BW0_3_POS                               0
#define F_BWR_CAM_HRT_R3_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_HRT_R3_ENG_BW0_4               0x142C
#define F_BWR_CAM_HRT_R3_ENG_BW0_4_POS                               0
#define F_BWR_CAM_HRT_R3_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_HRT_R3_ENG_BW0_5               0x1430
#define F_BWR_CAM_HRT_R3_ENG_BW0_5_POS                               0
#define F_BWR_CAM_HRT_R3_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_HRT_R3_ENG_BW0_6               0x1434
#define F_BWR_CAM_HRT_R3_ENG_BW0_6_POS                               0
#define F_BWR_CAM_HRT_R3_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_HRT_R3_ENG_BW0_7               0x1438
#define F_BWR_CAM_HRT_R3_ENG_BW0_7_POS                               0
#define F_BWR_CAM_HRT_R3_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_HRT_R3_ENG_BW0_8               0x143C
#define F_BWR_CAM_HRT_R3_ENG_BW0_8_POS                               0
#define F_BWR_CAM_HRT_R3_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_HRT_R3_ENG_BW0_9               0x1440
#define F_BWR_CAM_HRT_R3_ENG_BW0_9_POS                               0
#define F_BWR_CAM_HRT_R3_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_HRT_R3_ENG_BW0_10              0x1444
#define F_BWR_CAM_HRT_R3_ENG_BW0_10_POS                              0
#define F_BWR_CAM_HRT_R3_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_HRT_R3_ENG_BW0_11              0x1448
#define F_BWR_CAM_HRT_R3_ENG_BW0_11_POS                              0
#define F_BWR_CAM_HRT_R3_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_HRT_R3_ENG_BW_RAT0_0           0x144C
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_HRT_R3_ENG_BW_RAT0_1           0x1450
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_HRT_R3_ENG_BW_RAT0_2           0x1454
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_HRT_R3_ENG_BW_RAT0_3           0x1458
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_HRT_R3_ENG_BW_RAT0_4           0x145C
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_HRT_R3_ENG_BW_RAT0_5           0x1460
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_HRT_R3_ENG_BW_RAT0_6           0x1464
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_HRT_R3_ENG_BW_RAT0_7           0x1468
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_HRT_R3_ENG_BW_RAT0_8           0x146C
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_HRT_R3_ENG_BW_RAT0_9           0x1470
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_HRT_R3_ENG_BW_RAT0_10          0x1474
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_HRT_R3_ENG_BW_RAT0_11          0x1478
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_HRT_R3_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_HRT_R4_BW_QOS_SEL0             0x1510
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_HRT_R4_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_HRT_R4_SW_QOS_TRIG0            0x1514
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_HRT_R4_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_HRT_R4_SW_QOS_EN0              0x1518
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_HRT_R4_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_HRT_R4_ENG_BW0_0               0x151C
#define F_BWR_CAM_HRT_R4_ENG_BW0_0_POS                               0
#define F_BWR_CAM_HRT_R4_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_HRT_R4_ENG_BW0_1               0x1520
#define F_BWR_CAM_HRT_R4_ENG_BW0_1_POS                               0
#define F_BWR_CAM_HRT_R4_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_HRT_R4_ENG_BW0_2               0x1524
#define F_BWR_CAM_HRT_R4_ENG_BW0_2_POS                               0
#define F_BWR_CAM_HRT_R4_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_HRT_R4_ENG_BW0_3               0x1528
#define F_BWR_CAM_HRT_R4_ENG_BW0_3_POS                               0
#define F_BWR_CAM_HRT_R4_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_HRT_R4_ENG_BW0_4               0x152C
#define F_BWR_CAM_HRT_R4_ENG_BW0_4_POS                               0
#define F_BWR_CAM_HRT_R4_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_HRT_R4_ENG_BW0_5               0x1530
#define F_BWR_CAM_HRT_R4_ENG_BW0_5_POS                               0
#define F_BWR_CAM_HRT_R4_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_HRT_R4_ENG_BW0_6               0x1534
#define F_BWR_CAM_HRT_R4_ENG_BW0_6_POS                               0
#define F_BWR_CAM_HRT_R4_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_HRT_R4_ENG_BW0_7               0x1538
#define F_BWR_CAM_HRT_R4_ENG_BW0_7_POS                               0
#define F_BWR_CAM_HRT_R4_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_HRT_R4_ENG_BW0_8               0x153C
#define F_BWR_CAM_HRT_R4_ENG_BW0_8_POS                               0
#define F_BWR_CAM_HRT_R4_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_HRT_R4_ENG_BW0_9               0x1540
#define F_BWR_CAM_HRT_R4_ENG_BW0_9_POS                               0
#define F_BWR_CAM_HRT_R4_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_HRT_R4_ENG_BW0_10              0x1544
#define F_BWR_CAM_HRT_R4_ENG_BW0_10_POS                              0
#define F_BWR_CAM_HRT_R4_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_HRT_R4_ENG_BW0_11              0x1548
#define F_BWR_CAM_HRT_R4_ENG_BW0_11_POS                              0
#define F_BWR_CAM_HRT_R4_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_HRT_R4_ENG_BW_RAT0_0           0x154C
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_HRT_R4_ENG_BW_RAT0_1           0x1550
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_HRT_R4_ENG_BW_RAT0_2           0x1554
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_HRT_R4_ENG_BW_RAT0_3           0x1558
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_HRT_R4_ENG_BW_RAT0_4           0x155C
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_HRT_R4_ENG_BW_RAT0_5           0x1560
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_HRT_R4_ENG_BW_RAT0_6           0x1564
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_HRT_R4_ENG_BW_RAT0_7           0x1568
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_HRT_R4_ENG_BW_RAT0_8           0x156C
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_HRT_R4_ENG_BW_RAT0_9           0x1570
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_HRT_R4_ENG_BW_RAT0_10          0x1574
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_HRT_R4_ENG_BW_RAT0_11          0x1578
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_HRT_R4_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_HRT_W0_BW_QOS_SEL0             0x1810
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_HRT_W0_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_HRT_W0_SW_QOS_TRIG0            0x1814
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_HRT_W0_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_HRT_W0_SW_QOS_EN0              0x1818
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_HRT_W0_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_HRT_W0_ENG_BW0_0               0x181C
#define F_BWR_CAM_HRT_W0_ENG_BW0_0_POS                               0
#define F_BWR_CAM_HRT_W0_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_HRT_W0_ENG_BW0_1               0x1820
#define F_BWR_CAM_HRT_W0_ENG_BW0_1_POS                               0
#define F_BWR_CAM_HRT_W0_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_HRT_W0_ENG_BW0_2               0x1824
#define F_BWR_CAM_HRT_W0_ENG_BW0_2_POS                               0
#define F_BWR_CAM_HRT_W0_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_HRT_W0_ENG_BW0_3               0x1828
#define F_BWR_CAM_HRT_W0_ENG_BW0_3_POS                               0
#define F_BWR_CAM_HRT_W0_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_HRT_W0_ENG_BW0_4               0x182C
#define F_BWR_CAM_HRT_W0_ENG_BW0_4_POS                               0
#define F_BWR_CAM_HRT_W0_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_HRT_W0_ENG_BW0_5               0x1830
#define F_BWR_CAM_HRT_W0_ENG_BW0_5_POS                               0
#define F_BWR_CAM_HRT_W0_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_HRT_W0_ENG_BW0_6               0x1834
#define F_BWR_CAM_HRT_W0_ENG_BW0_6_POS                               0
#define F_BWR_CAM_HRT_W0_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_HRT_W0_ENG_BW0_7               0x1838
#define F_BWR_CAM_HRT_W0_ENG_BW0_7_POS                               0
#define F_BWR_CAM_HRT_W0_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_HRT_W0_ENG_BW0_8               0x183C
#define F_BWR_CAM_HRT_W0_ENG_BW0_8_POS                               0
#define F_BWR_CAM_HRT_W0_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_HRT_W0_ENG_BW0_9               0x1840
#define F_BWR_CAM_HRT_W0_ENG_BW0_9_POS                               0
#define F_BWR_CAM_HRT_W0_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_HRT_W0_ENG_BW0_10              0x1844
#define F_BWR_CAM_HRT_W0_ENG_BW0_10_POS                              0
#define F_BWR_CAM_HRT_W0_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_HRT_W0_ENG_BW0_11              0x1848
#define F_BWR_CAM_HRT_W0_ENG_BW0_11_POS                              0
#define F_BWR_CAM_HRT_W0_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_HRT_W0_ENG_BW_RAT0_0           0x184C
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_HRT_W0_ENG_BW_RAT0_1           0x1850
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_HRT_W0_ENG_BW_RAT0_2           0x1854
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_HRT_W0_ENG_BW_RAT0_3           0x1858
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_HRT_W0_ENG_BW_RAT0_4           0x185C
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_HRT_W0_ENG_BW_RAT0_5           0x1860
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_HRT_W0_ENG_BW_RAT0_6           0x1864
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_HRT_W0_ENG_BW_RAT0_7           0x1868
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_HRT_W0_ENG_BW_RAT0_8           0x186C
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_HRT_W0_ENG_BW_RAT0_9           0x1870
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_HRT_W0_ENG_BW_RAT0_10          0x1874
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_HRT_W0_ENG_BW_RAT0_11          0x1878
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_HRT_W0_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_HRT_W1_BW_QOS_SEL0             0x1910
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_HRT_W1_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_HRT_W1_SW_QOS_TRIG0            0x1914
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_HRT_W1_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_HRT_W1_SW_QOS_EN0              0x1918
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_HRT_W1_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_HRT_W1_ENG_BW0_0               0x191C
#define F_BWR_CAM_HRT_W1_ENG_BW0_0_POS                               0
#define F_BWR_CAM_HRT_W1_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_HRT_W1_ENG_BW0_1               0x1920
#define F_BWR_CAM_HRT_W1_ENG_BW0_1_POS                               0
#define F_BWR_CAM_HRT_W1_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_HRT_W1_ENG_BW0_2               0x1924
#define F_BWR_CAM_HRT_W1_ENG_BW0_2_POS                               0
#define F_BWR_CAM_HRT_W1_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_HRT_W1_ENG_BW0_3               0x1928
#define F_BWR_CAM_HRT_W1_ENG_BW0_3_POS                               0
#define F_BWR_CAM_HRT_W1_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_HRT_W1_ENG_BW0_4               0x192C
#define F_BWR_CAM_HRT_W1_ENG_BW0_4_POS                               0
#define F_BWR_CAM_HRT_W1_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_HRT_W1_ENG_BW0_5               0x1930
#define F_BWR_CAM_HRT_W1_ENG_BW0_5_POS                               0
#define F_BWR_CAM_HRT_W1_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_HRT_W1_ENG_BW0_6               0x1934
#define F_BWR_CAM_HRT_W1_ENG_BW0_6_POS                               0
#define F_BWR_CAM_HRT_W1_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_HRT_W1_ENG_BW0_7               0x1938
#define F_BWR_CAM_HRT_W1_ENG_BW0_7_POS                               0
#define F_BWR_CAM_HRT_W1_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_HRT_W1_ENG_BW0_8               0x193C
#define F_BWR_CAM_HRT_W1_ENG_BW0_8_POS                               0
#define F_BWR_CAM_HRT_W1_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_HRT_W1_ENG_BW0_9               0x1940
#define F_BWR_CAM_HRT_W1_ENG_BW0_9_POS                               0
#define F_BWR_CAM_HRT_W1_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_HRT_W1_ENG_BW0_10              0x1944
#define F_BWR_CAM_HRT_W1_ENG_BW0_10_POS                              0
#define F_BWR_CAM_HRT_W1_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_HRT_W1_ENG_BW0_11              0x1948
#define F_BWR_CAM_HRT_W1_ENG_BW0_11_POS                              0
#define F_BWR_CAM_HRT_W1_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_HRT_W1_ENG_BW_RAT0_0           0x194C
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_HRT_W1_ENG_BW_RAT0_1           0x1950
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_HRT_W1_ENG_BW_RAT0_2           0x1954
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_HRT_W1_ENG_BW_RAT0_3           0x1958
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_HRT_W1_ENG_BW_RAT0_4           0x195C
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_HRT_W1_ENG_BW_RAT0_5           0x1960
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_HRT_W1_ENG_BW_RAT0_6           0x1964
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_HRT_W1_ENG_BW_RAT0_7           0x1968
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_HRT_W1_ENG_BW_RAT0_8           0x196C
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_HRT_W1_ENG_BW_RAT0_9           0x1970
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_HRT_W1_ENG_BW_RAT0_10          0x1974
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_HRT_W1_ENG_BW_RAT0_11          0x1978
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_HRT_W1_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_HRT_W2_BW_QOS_SEL0             0x1A10
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_HRT_W2_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_HRT_W2_SW_QOS_TRIG0            0x1A14
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_HRT_W2_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_HRT_W2_SW_QOS_EN0              0x1A18
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_HRT_W2_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_HRT_W2_ENG_BW0_0               0x1A1C
#define F_BWR_CAM_HRT_W2_ENG_BW0_0_POS                               0
#define F_BWR_CAM_HRT_W2_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_HRT_W2_ENG_BW0_1               0x1A20
#define F_BWR_CAM_HRT_W2_ENG_BW0_1_POS                               0
#define F_BWR_CAM_HRT_W2_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_HRT_W2_ENG_BW0_2               0x1A24
#define F_BWR_CAM_HRT_W2_ENG_BW0_2_POS                               0
#define F_BWR_CAM_HRT_W2_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_HRT_W2_ENG_BW0_3               0x1A28
#define F_BWR_CAM_HRT_W2_ENG_BW0_3_POS                               0
#define F_BWR_CAM_HRT_W2_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_HRT_W2_ENG_BW0_4               0x1A2C
#define F_BWR_CAM_HRT_W2_ENG_BW0_4_POS                               0
#define F_BWR_CAM_HRT_W2_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_HRT_W2_ENG_BW0_5               0x1A30
#define F_BWR_CAM_HRT_W2_ENG_BW0_5_POS                               0
#define F_BWR_CAM_HRT_W2_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_HRT_W2_ENG_BW0_6               0x1A34
#define F_BWR_CAM_HRT_W2_ENG_BW0_6_POS                               0
#define F_BWR_CAM_HRT_W2_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_HRT_W2_ENG_BW0_7               0x1A38
#define F_BWR_CAM_HRT_W2_ENG_BW0_7_POS                               0
#define F_BWR_CAM_HRT_W2_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_HRT_W2_ENG_BW0_8               0x1A3C
#define F_BWR_CAM_HRT_W2_ENG_BW0_8_POS                               0
#define F_BWR_CAM_HRT_W2_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_HRT_W2_ENG_BW0_9               0x1A40
#define F_BWR_CAM_HRT_W2_ENG_BW0_9_POS                               0
#define F_BWR_CAM_HRT_W2_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_HRT_W2_ENG_BW0_10              0x1A44
#define F_BWR_CAM_HRT_W2_ENG_BW0_10_POS                              0
#define F_BWR_CAM_HRT_W2_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_HRT_W2_ENG_BW0_11              0x1A48
#define F_BWR_CAM_HRT_W2_ENG_BW0_11_POS                              0
#define F_BWR_CAM_HRT_W2_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_HRT_W2_ENG_BW_RAT0_0           0x1A4C
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_HRT_W2_ENG_BW_RAT0_1           0x1A50
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_HRT_W2_ENG_BW_RAT0_2           0x1A54
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_HRT_W2_ENG_BW_RAT0_3           0x1A58
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_HRT_W2_ENG_BW_RAT0_4           0x1A5C
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_HRT_W2_ENG_BW_RAT0_5           0x1A60
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_HRT_W2_ENG_BW_RAT0_6           0x1A64
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_HRT_W2_ENG_BW_RAT0_7           0x1A68
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_HRT_W2_ENG_BW_RAT0_8           0x1A6C
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_HRT_W2_ENG_BW_RAT0_9           0x1A70
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_HRT_W2_ENG_BW_RAT0_10          0x1A74
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_HRT_W2_ENG_BW_RAT0_11          0x1A78
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_HRT_W2_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_HRT_W3_BW_QOS_SEL0             0x1B10
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_HRT_W3_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_HRT_W3_SW_QOS_TRIG0            0x1B14
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_HRT_W3_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_HRT_W3_SW_QOS_EN0              0x1B18
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_HRT_W3_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_HRT_W3_ENG_BW0_0               0x1B1C
#define F_BWR_CAM_HRT_W3_ENG_BW0_0_POS                               0
#define F_BWR_CAM_HRT_W3_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_HRT_W3_ENG_BW0_1               0x1B20
#define F_BWR_CAM_HRT_W3_ENG_BW0_1_POS                               0
#define F_BWR_CAM_HRT_W3_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_HRT_W3_ENG_BW0_2               0x1B24
#define F_BWR_CAM_HRT_W3_ENG_BW0_2_POS                               0
#define F_BWR_CAM_HRT_W3_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_HRT_W3_ENG_BW0_3               0x1B28
#define F_BWR_CAM_HRT_W3_ENG_BW0_3_POS                               0
#define F_BWR_CAM_HRT_W3_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_HRT_W3_ENG_BW0_4               0x1B2C
#define F_BWR_CAM_HRT_W3_ENG_BW0_4_POS                               0
#define F_BWR_CAM_HRT_W3_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_HRT_W3_ENG_BW0_5               0x1B30
#define F_BWR_CAM_HRT_W3_ENG_BW0_5_POS                               0
#define F_BWR_CAM_HRT_W3_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_HRT_W3_ENG_BW0_6               0x1B34
#define F_BWR_CAM_HRT_W3_ENG_BW0_6_POS                               0
#define F_BWR_CAM_HRT_W3_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_HRT_W3_ENG_BW0_7               0x1B38
#define F_BWR_CAM_HRT_W3_ENG_BW0_7_POS                               0
#define F_BWR_CAM_HRT_W3_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_HRT_W3_ENG_BW0_8               0x1B3C
#define F_BWR_CAM_HRT_W3_ENG_BW0_8_POS                               0
#define F_BWR_CAM_HRT_W3_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_HRT_W3_ENG_BW0_9               0x1B40
#define F_BWR_CAM_HRT_W3_ENG_BW0_9_POS                               0
#define F_BWR_CAM_HRT_W3_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_HRT_W3_ENG_BW0_10              0x1B44
#define F_BWR_CAM_HRT_W3_ENG_BW0_10_POS                              0
#define F_BWR_CAM_HRT_W3_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_HRT_W3_ENG_BW0_11              0x1B48
#define F_BWR_CAM_HRT_W3_ENG_BW0_11_POS                              0
#define F_BWR_CAM_HRT_W3_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_HRT_W3_ENG_BW_RAT0_0           0x1B4C
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_HRT_W3_ENG_BW_RAT0_1           0x1B50
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_HRT_W3_ENG_BW_RAT0_2           0x1B54
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_HRT_W3_ENG_BW_RAT0_3           0x1B58
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_HRT_W3_ENG_BW_RAT0_4           0x1B5C
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_HRT_W3_ENG_BW_RAT0_5           0x1B60
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_HRT_W3_ENG_BW_RAT0_6           0x1B64
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_HRT_W3_ENG_BW_RAT0_7           0x1B68
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_HRT_W3_ENG_BW_RAT0_8           0x1B6C
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_HRT_W3_ENG_BW_RAT0_9           0x1B70
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_HRT_W3_ENG_BW_RAT0_10          0x1B74
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_HRT_W3_ENG_BW_RAT0_11          0x1B78
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_HRT_W3_ENG_BW_RAT0_11_WIDTH                        12

#define REG_BWR_CAM_HRT_W4_BW_QOS_SEL0             0x1C10
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_11_POS                          11
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_11_WIDTH                        1
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_10_POS                          10
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_10_WIDTH                        1
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_9_POS                           9
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_9_WIDTH                         1
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_8_POS                           8
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_8_WIDTH                         1
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_7_POS                           7
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_7_WIDTH                         1
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_6_POS                           6
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_6_WIDTH                         1
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_5_POS                           5
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_5_WIDTH                         1
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_4_POS                           4
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_4_WIDTH                         1
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_3_POS                           3
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_3_WIDTH                         1
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_2_POS                           2
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_2_WIDTH                         1
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_1_POS                           1
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_1_WIDTH                         1
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_0_POS                           0
#define F_BWR_CAM_HRT_W4_BW_QOS_SEL0_0_WIDTH                         1

#define REG_BWR_CAM_HRT_W4_SW_QOS_TRIG0            0x1C14
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_11_POS                         11
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_11_WIDTH                       1
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_10_POS                         10
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_10_WIDTH                       1
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_9_POS                          9
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_9_WIDTH                        1
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_8_POS                          8
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_8_WIDTH                        1
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_7_POS                          7
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_7_WIDTH                        1
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_6_POS                          6
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_6_WIDTH                        1
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_5_POS                          5
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_5_WIDTH                        1
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_4_POS                          4
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_4_WIDTH                        1
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_3_POS                          3
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_3_WIDTH                        1
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_2_POS                          2
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_2_WIDTH                        1
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_1_POS                          1
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_1_WIDTH                        1
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_0_POS                          0
#define F_BWR_CAM_HRT_W4_SW_QOS_TRIG0_0_WIDTH                        1

#define REG_BWR_CAM_HRT_W4_SW_QOS_EN0              0x1C18
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_11_POS                           11
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_11_WIDTH                         1
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_10_POS                           10
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_10_WIDTH                         1
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_9_POS                            9
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_9_WIDTH                          1
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_8_POS                            8
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_8_WIDTH                          1
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_7_POS                            7
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_7_WIDTH                          1
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_6_POS                            6
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_6_WIDTH                          1
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_5_POS                            5
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_5_WIDTH                          1
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_4_POS                            4
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_4_WIDTH                          1
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_3_POS                            3
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_3_WIDTH                          1
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_2_POS                            2
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_2_WIDTH                          1
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_1_POS                            1
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_1_WIDTH                          1
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_0_POS                            0
#define F_BWR_CAM_HRT_W4_SW_QOS_EN0_0_WIDTH                          1

#define REG_BWR_CAM_HRT_W4_ENG_BW0_0               0x1C1C
#define F_BWR_CAM_HRT_W4_ENG_BW0_0_POS                               0
#define F_BWR_CAM_HRT_W4_ENG_BW0_0_WIDTH                             18

#define REG_BWR_CAM_HRT_W4_ENG_BW0_1               0x1C20
#define F_BWR_CAM_HRT_W4_ENG_BW0_1_POS                               0
#define F_BWR_CAM_HRT_W4_ENG_BW0_1_WIDTH                             18

#define REG_BWR_CAM_HRT_W4_ENG_BW0_2               0x1C24
#define F_BWR_CAM_HRT_W4_ENG_BW0_2_POS                               0
#define F_BWR_CAM_HRT_W4_ENG_BW0_2_WIDTH                             18

#define REG_BWR_CAM_HRT_W4_ENG_BW0_3               0x1C28
#define F_BWR_CAM_HRT_W4_ENG_BW0_3_POS                               0
#define F_BWR_CAM_HRT_W4_ENG_BW0_3_WIDTH                             18

#define REG_BWR_CAM_HRT_W4_ENG_BW0_4               0x1C2C
#define F_BWR_CAM_HRT_W4_ENG_BW0_4_POS                               0
#define F_BWR_CAM_HRT_W4_ENG_BW0_4_WIDTH                             18

#define REG_BWR_CAM_HRT_W4_ENG_BW0_5               0x1C30
#define F_BWR_CAM_HRT_W4_ENG_BW0_5_POS                               0
#define F_BWR_CAM_HRT_W4_ENG_BW0_5_WIDTH                             18

#define REG_BWR_CAM_HRT_W4_ENG_BW0_6               0x1C34
#define F_BWR_CAM_HRT_W4_ENG_BW0_6_POS                               0
#define F_BWR_CAM_HRT_W4_ENG_BW0_6_WIDTH                             18

#define REG_BWR_CAM_HRT_W4_ENG_BW0_7               0x1C38
#define F_BWR_CAM_HRT_W4_ENG_BW0_7_POS                               0
#define F_BWR_CAM_HRT_W4_ENG_BW0_7_WIDTH                             18

#define REG_BWR_CAM_HRT_W4_ENG_BW0_8               0x1C3C
#define F_BWR_CAM_HRT_W4_ENG_BW0_8_POS                               0
#define F_BWR_CAM_HRT_W4_ENG_BW0_8_WIDTH                             18

#define REG_BWR_CAM_HRT_W4_ENG_BW0_9               0x1C40
#define F_BWR_CAM_HRT_W4_ENG_BW0_9_POS                               0
#define F_BWR_CAM_HRT_W4_ENG_BW0_9_WIDTH                             18

#define REG_BWR_CAM_HRT_W4_ENG_BW0_10              0x1C44
#define F_BWR_CAM_HRT_W4_ENG_BW0_10_POS                              0
#define F_BWR_CAM_HRT_W4_ENG_BW0_10_WIDTH                            18

#define REG_BWR_CAM_HRT_W4_ENG_BW0_11              0x1C48
#define F_BWR_CAM_HRT_W4_ENG_BW0_11_POS                              0
#define F_BWR_CAM_HRT_W4_ENG_BW0_11_WIDTH                            18

#define REG_BWR_CAM_HRT_W4_ENG_BW_RAT0_0           0x1C4C
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_0_POS                           0
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_0_WIDTH                         12

#define REG_BWR_CAM_HRT_W4_ENG_BW_RAT0_1           0x1C50
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_1_POS                           0
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_1_WIDTH                         12

#define REG_BWR_CAM_HRT_W4_ENG_BW_RAT0_2           0x1C54
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_2_POS                           0
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_2_WIDTH                         12

#define REG_BWR_CAM_HRT_W4_ENG_BW_RAT0_3           0x1C58
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_3_POS                           0
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_3_WIDTH                         12

#define REG_BWR_CAM_HRT_W4_ENG_BW_RAT0_4           0x1C5C
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_4_POS                           0
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_4_WIDTH                         12

#define REG_BWR_CAM_HRT_W4_ENG_BW_RAT0_5           0x1C60
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_5_POS                           0
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_5_WIDTH                         12

#define REG_BWR_CAM_HRT_W4_ENG_BW_RAT0_6           0x1C64
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_6_POS                           0
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_6_WIDTH                         12

#define REG_BWR_CAM_HRT_W4_ENG_BW_RAT0_7           0x1C68
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_7_POS                           0
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_7_WIDTH                         12

#define REG_BWR_CAM_HRT_W4_ENG_BW_RAT0_8           0x1C6C
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_8_POS                           0
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_8_WIDTH                         12

#define REG_BWR_CAM_HRT_W4_ENG_BW_RAT0_9           0x1C70
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_9_POS                           0
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_9_WIDTH                         12

#define REG_BWR_CAM_HRT_W4_ENG_BW_RAT0_10          0x1C74
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_10_POS                          0
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_10_WIDTH                        12

#define REG_BWR_CAM_HRT_W4_ENG_BW_RAT0_11          0x1C78
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_11_POS                          0
#define F_BWR_CAM_HRT_W4_ENG_BW_RAT0_11_WIDTH                        12

#endif	/* _MTK_CAM_BWR_REGS_H */
