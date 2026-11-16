/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _MTK_CAM_QOF_REGS_H
#define _MTK_CAM_QOF_REGS_H

/* baseaddr 0x3A0A0000 */

/* module: QOF_CAM_TOP_E1A */
#define REG_QOF_CAM_TOP_QOF_TOP_CTL                   0x0
#define F_QOF_CAM_TOP_OUT_LOCK_3_POS                                 29
#define F_QOF_CAM_TOP_OUT_LOCK_3_WIDTH                               1
#define F_QOF_CAM_TOP_OFF_LOCK_3_POS                                 28
#define F_QOF_CAM_TOP_OFF_LOCK_3_WIDTH                               1
#define F_QOF_CAM_TOP_ON_LOCK_3_POS                                  27
#define F_QOF_CAM_TOP_ON_LOCK_3_WIDTH                                1
#define F_QOF_CAM_TOP_OUT_LOCK_2_POS                                 26
#define F_QOF_CAM_TOP_OUT_LOCK_2_WIDTH                               1
#define F_QOF_CAM_TOP_OFF_LOCK_2_POS                                 25
#define F_QOF_CAM_TOP_OFF_LOCK_2_WIDTH                               1
#define F_QOF_CAM_TOP_ON_LOCK_2_POS                                  24
#define F_QOF_CAM_TOP_ON_LOCK_2_WIDTH                                1
#define F_QOF_CAM_TOP_OUT_LOCK_1_POS                                 23
#define F_QOF_CAM_TOP_OUT_LOCK_1_WIDTH                               1
#define F_QOF_CAM_TOP_OFF_LOCK_1_POS                                 22
#define F_QOF_CAM_TOP_OFF_LOCK_1_WIDTH                               1
#define F_QOF_CAM_TOP_ON_LOCK_1_POS                                  21
#define F_QOF_CAM_TOP_ON_LOCK_1_WIDTH                                1
#define F_QOF_CAM_TOP_DCIF_EXP_SOF_SEL_3_POS                         19
#define F_QOF_CAM_TOP_DCIF_EXP_SOF_SEL_3_WIDTH                       2
#define F_QOF_CAM_TOP_DCIF_EXP_SOF_SEL_2_POS                         17
#define F_QOF_CAM_TOP_DCIF_EXP_SOF_SEL_2_WIDTH                       2
#define F_QOF_CAM_TOP_DCIF_EXP_SOF_SEL_1_POS                         15
#define F_QOF_CAM_TOP_DCIF_EXP_SOF_SEL_1_WIDTH                       2
#define F_QOF_CAM_TOP_OTF_DC_MODE_3_POS                              13
#define F_QOF_CAM_TOP_OTF_DC_MODE_3_WIDTH                            2
#define F_QOF_CAM_TOP_OTF_DC_MODE_2_POS                              11
#define F_QOF_CAM_TOP_OTF_DC_MODE_2_WIDTH                            2
#define F_QOF_CAM_TOP_OTF_DC_MODE_1_POS                              9
#define F_QOF_CAM_TOP_OTF_DC_MODE_1_WIDTH                            2
#define F_QOF_CAM_TOP_ITC_SRC_SEL_3_POS                              8
#define F_QOF_CAM_TOP_ITC_SRC_SEL_3_WIDTH                            1
#define F_QOF_CAM_TOP_ITC_SRC_SEL_2_POS                              7
#define F_QOF_CAM_TOP_ITC_SRC_SEL_2_WIDTH                            1
#define F_QOF_CAM_TOP_ITC_SRC_SEL_1_POS                              6
#define F_QOF_CAM_TOP_ITC_SRC_SEL_1_WIDTH                            1
#define F_QOF_CAM_TOP_QOF_SUBC_EN_POS                                5
#define F_QOF_CAM_TOP_QOF_SUBC_EN_WIDTH                              1
#define F_QOF_CAM_TOP_QOF_SUBB_EN_POS                                4
#define F_QOF_CAM_TOP_QOF_SUBB_EN_WIDTH                              1
#define F_QOF_CAM_TOP_QOF_SUBA_EN_POS                                3
#define F_QOF_CAM_TOP_QOF_SUBA_EN_WIDTH                              1
#define F_QOF_CAM_TOP_SEQUENCE_MODE_POS                              2
#define F_QOF_CAM_TOP_SEQUENCE_MODE_WIDTH                            1

#define REG_QOF_CAM_TOP_ITC_STATUS                    0x4
#define REG_QOF_CAM_TOP_QOF_INT_EN                    0x8
#define F_QOF_CAM_TOP_INT_WCLR_EN_POS                                31
#define F_QOF_CAM_TOP_INT_WCLR_EN_WIDTH                              1
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_EN_3_POS                  11
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_EN_3_WIDTH                1
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_EN_2_POS                  10
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_EN_2_WIDTH                1
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_EN_1_POS                  9
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_EN_1_WIDTH                1
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_EN_3_POS                       8
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_EN_3_WIDTH                     1
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_EN_2_POS                       7
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_EN_2_WIDTH                     1
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_EN_1_POS                       6
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_EN_1_WIDTH                     1
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_EN_3_POS                    5
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_EN_3_WIDTH                  1
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_EN_2_POS                    4
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_EN_2_WIDTH                  1
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_EN_1_POS                    3
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_EN_1_WIDTH                  1
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_EN_3_POS                        2
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_EN_3_WIDTH                      1
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_EN_2_POS                        1
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_EN_2_WIDTH                      1
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_EN_1_POS                        0
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_EN_1_WIDTH                      1

#define REG_QOF_CAM_TOP_QOF_INT_STATUS                0xC
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_ST_3_POS                  11
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_ST_3_WIDTH                1
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_ST_2_POS                  10
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_ST_2_WIDTH                1
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_ST_1_POS                  9
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_ST_1_WIDTH                1
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_ST_3_POS                       8
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_ST_3_WIDTH                     1
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_ST_2_POS                       7
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_ST_2_WIDTH                     1
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_ST_1_POS                       6
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_ST_1_WIDTH                     1
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_ST_3_POS                    5
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_ST_3_WIDTH                  1
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_ST_2_POS                    4
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_ST_2_WIDTH                  1
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_ST_1_POS                    3
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_ST_1_WIDTH                  1
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_ST_3_POS                        2
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_ST_3_WIDTH                      1
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_ST_2_POS                        1
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_ST_2_WIDTH                      1
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_ST_1_POS                        0
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_ST_1_WIDTH                      1

#define REG_QOF_CAM_TOP_QOF_INT_TRIG                 0x10
#define F_QOF_CAM_TOP_QOF_SELF_INT_TRIG_EN_POS                       31
#define F_QOF_CAM_TOP_QOF_SELF_INT_TRIG_EN_WIDTH                     1
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_TRIG_3_POS                    17
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_TRIG_3_WIDTH                  1
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_TRIG_2_POS                    16
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_TRIG_2_WIDTH                  1
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_TRIG_1_POS                    15
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_TRIG_1_WIDTH                  1
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_TRIG_3_POS                       14
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_TRIG_3_WIDTH                     1
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_TRIG_2_POS                       13
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_TRIG_2_WIDTH                     1
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_TRIG_1_POS                       12
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_TRIG_1_WIDTH                     1
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_TRIG_3_POS                11
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_TRIG_3_WIDTH              1
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_TRIG_2_POS                10
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_TRIG_2_WIDTH              1
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_TRIG_1_POS                9
#define F_QOF_CAM_TOP_MTC_PROC_OV_TIME_INT_TRIG_1_WIDTH              1
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_TRIG_3_POS                     8
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_TRIG_3_WIDTH                   1
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_TRIG_2_POS                     7
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_TRIG_2_WIDTH                   1
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_TRIG_1_POS                     6
#define F_QOF_CAM_TOP_CQ_TRIG_DLY_INT_TRIG_1_WIDTH                   1
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_TRIG_3_POS                  5
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_TRIG_3_WIDTH                1
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_TRIG_2_POS                  4
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_TRIG_2_WIDTH                1
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_TRIG_1_POS                  3
#define F_QOF_CAM_TOP_PWR_OFF_ACCESS_INT_TRIG_1_WIDTH                1
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_TRIG_3_POS                      2
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_TRIG_3_WIDTH                    1
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_TRIG_2_POS                      1
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_TRIG_2_WIDTH                    1
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_TRIG_1_POS                      0
#define F_QOF_CAM_TOP_MTC_CYC_OV_INT_TRIG_1_WIDTH                    1

#define REG_QOF_CAM_TOP_QOF_SPARE1_TOP               0x14
#define REG_QOF_CAM_TOP_QOF_SPARE2_TOP               0x18
#define REG_QOF_CAM_TOP_QOF_EVENT_TRIG               0x1C
#define F_QOF_CAM_TOP_QOF_SELF_EVENT_TRIG_EN_POS                     31
#define F_QOF_CAM_TOP_QOF_SELF_EVENT_TRIG_EN_WIDTH                   1
#define F_QOF_CAM_TOP_PWR_ON_DONE_EVENT_TRIG_3_POS                   26
#define F_QOF_CAM_TOP_PWR_ON_DONE_EVENT_TRIG_3_WIDTH                 1
#define F_QOF_CAM_TOP_PWR_ON_DONE_EVENT_TRIG_2_POS                   25
#define F_QOF_CAM_TOP_PWR_ON_DONE_EVENT_TRIG_2_WIDTH                 1
#define F_QOF_CAM_TOP_PWR_ON_DONE_EVENT_TRIG_1_POS                   24
#define F_QOF_CAM_TOP_PWR_ON_DONE_EVENT_TRIG_1_WIDTH                 1
#define F_QOF_CAM_TOP_RESTORE_EVENT_TRIG_3_POS                       23
#define F_QOF_CAM_TOP_RESTORE_EVENT_TRIG_3_WIDTH                     1
#define F_QOF_CAM_TOP_RESTORE_EVENT_TRIG_2_POS                       22
#define F_QOF_CAM_TOP_RESTORE_EVENT_TRIG_2_WIDTH                     1
#define F_QOF_CAM_TOP_RESTORE_EVENT_TRIG_1_POS                       21
#define F_QOF_CAM_TOP_RESTORE_EVENT_TRIG_1_WIDTH                     1
#define F_QOF_CAM_TOP_SAVE_EVENT_TRIG_3_POS                          20
#define F_QOF_CAM_TOP_SAVE_EVENT_TRIG_3_WIDTH                        1
#define F_QOF_CAM_TOP_SAVE_EVENT_TRIG_2_POS                          19
#define F_QOF_CAM_TOP_SAVE_EVENT_TRIG_2_WIDTH                        1
#define F_QOF_CAM_TOP_SAVE_EVENT_TRIG_1_POS                          18
#define F_QOF_CAM_TOP_SAVE_EVENT_TRIG_1_WIDTH                        1
#define F_QOF_CAM_TOP_PWR_OFF_EVENT_TRIG_3_POS                       17
#define F_QOF_CAM_TOP_PWR_OFF_EVENT_TRIG_3_WIDTH                     1
#define F_QOF_CAM_TOP_PWR_OFF_EVENT_TRIG_2_POS                       16
#define F_QOF_CAM_TOP_PWR_OFF_EVENT_TRIG_2_WIDTH                     1
#define F_QOF_CAM_TOP_PWR_OFF_EVENT_TRIG_1_POS                       15
#define F_QOF_CAM_TOP_PWR_OFF_EVENT_TRIG_1_WIDTH                     1
#define F_QOF_CAM_TOP_PWR_ON_EVENT_TRIG_3_POS                        14
#define F_QOF_CAM_TOP_PWR_ON_EVENT_TRIG_3_WIDTH                      1
#define F_QOF_CAM_TOP_PWR_ON_EVENT_TRIG_2_POS                        13
#define F_QOF_CAM_TOP_PWR_ON_EVENT_TRIG_2_WIDTH                      1
#define F_QOF_CAM_TOP_PWR_ON_EVENT_TRIG_1_POS                        12
#define F_QOF_CAM_TOP_PWR_ON_EVENT_TRIG_1_WIDTH                      1
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_12_POS                          11
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_12_WIDTH                        1
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_11_POS                          10
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_11_WIDTH                        1
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_10_POS                          9
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_10_WIDTH                        1
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_9_POS                           8
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_9_WIDTH                         1
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_8_POS                           7
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_8_WIDTH                         1
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_7_POS                           6
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_7_WIDTH                         1
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_6_POS                           5
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_6_WIDTH                         1
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_5_POS                           4
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_5_WIDTH                         1
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_4_POS                           3
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_4_WIDTH                         1
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_3_POS                           2
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_3_WIDTH                         1
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_2_POS                           1
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_2_WIDTH                         1
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_1_POS                           0
#define F_QOF_CAM_TOP_ACK_EVENT_TRIG_1_WIDTH                         1

#define REG_QOF_CAM_TOP_QOF_SW_RST                   0x20
#define F_QOF_CAM_TOP_QOF_SW_RST_POS                                 0
#define F_QOF_CAM_TOP_QOF_SW_RST_WIDTH                               20

#define REG_QOF_CAM_TOP_QOF_VIO_CID_1                0x24
#define F_QOF_CAM_TOP_VIO_CID_1_POS                                  0
#define F_QOF_CAM_TOP_VIO_CID_1_WIDTH                                5

#define REG_QOF_CAM_TOP_QOF_VIO_CID_2                0x28
#define F_QOF_CAM_TOP_VIO_CID_2_POS                                  0
#define F_QOF_CAM_TOP_VIO_CID_2_WIDTH                                5

#define REG_QOF_CAM_TOP_QOF_VIO_CID_3                0x2C
#define F_QOF_CAM_TOP_VIO_CID_3_POS                                  0
#define F_QOF_CAM_TOP_VIO_CID_3_WIDTH                                5

#define REG_QOF_CAM_TOP_QOF_INT_EN_HFRP_1            0x34
#define F_QOF_CAM_TOP_INT_WCLR_EN_HFRP_POS                           31
#define F_QOF_CAM_TOP_INT_WCLR_EN_HFRP_WIDTH                         1
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_EN_1_POS                      1
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_EN_1_WIDTH                    1
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_EN_1_POS                         0
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_EN_1_WIDTH                       1

#define REG_QOF_CAM_TOP_QOF_INT_EN_HFRP_2            0x38
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_EN_2_POS                      1
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_EN_2_WIDTH                    1
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_EN_2_POS                         0
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_EN_2_WIDTH                       1

#define REG_QOF_CAM_TOP_QOF_INT_EN_HFRP_3            0x3C
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_EN_3_POS                      1
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_EN_3_WIDTH                    1
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_EN_3_POS                         0
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_EN_3_WIDTH                       1

#define REG_QOF_CAM_TOP_QOF_INT_STATUS_HFRP_1        0x40
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_ST_1_POS                      1
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_ST_1_WIDTH                    1
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_ST_1_POS                         0
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_ST_1_WIDTH                       1

#define REG_QOF_CAM_TOP_QOF_INT_STATUS_HFRP_2        0x44
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_ST_2_POS                      1
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_ST_2_WIDTH                    1
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_ST_2_POS                         0
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_ST_2_WIDTH                       1

#define REG_QOF_CAM_TOP_QOF_INT_STATUS_HFRP_3        0x48
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_ST_3_POS                      1
#define F_QOF_CAM_TOP_HFRP_RESTORE_INT_ST_3_WIDTH                    1
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_ST_3_POS                         0
#define F_QOF_CAM_TOP_HFRP_SAVE_INT_ST_3_WIDTH                       1

#define REG_QOF_CAM_TOP_QOF_POWER_CTL                0x4C
#define F_QOF_CAM_TOP_POWER_CTL_MODE_3_POS                           4
#define F_QOF_CAM_TOP_POWER_CTL_MODE_3_WIDTH                         2
#define F_QOF_CAM_TOP_POWER_CTL_MODE_2_POS                           2
#define F_QOF_CAM_TOP_POWER_CTL_MODE_2_WIDTH                         2
#define F_QOF_CAM_TOP_POWER_CTL_MODE_1_POS                           0
#define F_QOF_CAM_TOP_POWER_CTL_MODE_1_WIDTH                         2

#define REG_QOF_CAM_TOP_QOF_HWCCF_SW_CTL             0x50
#define F_QOF_CAM_TOP_HWCCF_SW_VOTE_OFF_3_POS                        9
#define F_QOF_CAM_TOP_HWCCF_SW_VOTE_OFF_3_WIDTH                      1
#define F_QOF_CAM_TOP_HWCCF_SW_VOTE_ON_3_POS                         8
#define F_QOF_CAM_TOP_HWCCF_SW_VOTE_ON_3_WIDTH                       1
#define F_QOF_CAM_TOP_HWCCF_SW_VOTE_OFF_2_POS                        5
#define F_QOF_CAM_TOP_HWCCF_SW_VOTE_OFF_2_WIDTH                      1
#define F_QOF_CAM_TOP_HWCCF_SW_VOTE_ON_2_POS                         4
#define F_QOF_CAM_TOP_HWCCF_SW_VOTE_ON_2_WIDTH                       1
#define F_QOF_CAM_TOP_HWCCF_SW_VOTE_OFF_1_POS                        1
#define F_QOF_CAM_TOP_HWCCF_SW_VOTE_OFF_1_WIDTH                      1
#define F_QOF_CAM_TOP_HWCCF_SW_VOTE_ON_1_POS                         0
#define F_QOF_CAM_TOP_HWCCF_SW_VOTE_ON_1_WIDTH                       1

/* baseaddr 0x3A0B0000 */

/* module: QOF_CAM_A_E1A */
#define REG_QOF_CAM_A_QOF_CTL                         0x0
#define F_QOF_CAM_A_DL_EN_POS                                        30
#define F_QOF_CAM_A_DL_EN_WIDTH                                      1
#define F_QOF_CAM_A_QOF_CQ_EN_POS                                    27
#define F_QOF_CAM_A_QOF_CQ_EN_WIDTH                                  1
#define F_QOF_CAM_A_OFF_SEL_POS                                      26
#define F_QOF_CAM_A_OFF_SEL_WIDTH                                    1
#define F_QOF_CAM_A_ON_SEL_POS                                       25
#define F_QOF_CAM_A_ON_SEL_WIDTH                                     1
#define F_QOF_CAM_A_BW_QOS_SW_POS                                    24
#define F_QOF_CAM_A_BW_QOS_SW_WIDTH                                  1
#define F_QOF_CAM_A_BW_QOS_HW_EN_POS                                 23
#define F_QOF_CAM_A_BW_QOS_HW_EN_WIDTH                               1
#define F_QOF_CAM_A_BW_QOS_SW_CLR_POS                                22
#define F_QOF_CAM_A_BW_QOS_SW_CLR_WIDTH                              1
#define F_QOF_CAM_A_BW_QOS_SW_SET_POS                                21
#define F_QOF_CAM_A_BW_QOS_SW_SET_WIDTH                              1
#define F_QOF_CAM_A_DDREN_SW_POS                                     20
#define F_QOF_CAM_A_DDREN_SW_WIDTH                                   1
#define F_QOF_CAM_A_DDREN_HW_EN_POS                                  19
#define F_QOF_CAM_A_DDREN_HW_EN_WIDTH                                1
#define F_QOF_CAM_A_OPT_MTC_ACT_POS                                  18
#define F_QOF_CAM_A_OPT_MTC_ACT_WIDTH                                1
#define F_QOF_CAM_A_SW_CQ_OFF_POS                                    17
#define F_QOF_CAM_A_SW_CQ_OFF_WIDTH                                  1
#define F_QOF_CAM_A_SW_RAW_OFF_POS                                   16
#define F_QOF_CAM_A_SW_RAW_OFF_WIDTH                                 1
#define F_QOF_CAM_A_CQ_HW_CLR_EN_POS                                 15
#define F_QOF_CAM_A_CQ_HW_CLR_EN_WIDTH                               1
#define F_QOF_CAM_A_CQ_HW_SET_EN_POS                                 14
#define F_QOF_CAM_A_CQ_HW_SET_EN_WIDTH                               1
#define F_QOF_CAM_A_RAW_HW_CLR_EN_POS                                13
#define F_QOF_CAM_A_RAW_HW_CLR_EN_WIDTH                              1
#define F_QOF_CAM_A_RAW_HW_SET_EN_POS                                12
#define F_QOF_CAM_A_RAW_HW_SET_EN_WIDTH                              1
#define F_QOF_CAM_A_OFF_ITC_W_EN_POS                                 11
#define F_QOF_CAM_A_OFF_ITC_W_EN_WIDTH                               1
#define F_QOF_CAM_A_ON_ITC_W_EN_POS                                  10
#define F_QOF_CAM_A_ON_ITC_W_EN_WIDTH                                1
#define F_QOF_CAM_A_RTC_EN_POS                                       8
#define F_QOF_CAM_A_RTC_EN_WIDTH                                     1
#define F_QOF_CAM_A_DDREN_SW_CLR_POS                                 7
#define F_QOF_CAM_A_DDREN_SW_CLR_WIDTH                               1
#define F_QOF_CAM_A_DDREN_SW_SET_POS                                 6
#define F_QOF_CAM_A_DDREN_SW_SET_WIDTH                               1
#define F_QOF_CAM_A_CQ_START_POS                                     5
#define F_QOF_CAM_A_CQ_START_WIDTH                                   1
#define F_QOF_CAM_A_SCP_CLR_POS                                      3
#define F_QOF_CAM_A_SCP_CLR_WIDTH                                    1
#define F_QOF_CAM_A_SCP_SET_POS                                      2
#define F_QOF_CAM_A_SCP_SET_WIDTH                                    1
#define F_QOF_CAM_A_APMCU_CLR_POS                                    1
#define F_QOF_CAM_A_APMCU_CLR_WIDTH                                  1
#define F_QOF_CAM_A_APMCU_SET_POS                                    0
#define F_QOF_CAM_A_APMCU_SET_WIDTH                                  1

#define REG_QOF_CAM_A_QOF_DONE_STATUS                 0x4
#define F_QOF_CAM_A_CQ_ITCW_DONE_POS                                 4
#define F_QOF_CAM_A_CQ_ITCW_DONE_WIDTH                               1
#define F_QOF_CAM_A_CFG_OFF_DONE_POS                                 3
#define F_QOF_CAM_A_CFG_OFF_DONE_WIDTH                               1
#define F_QOF_CAM_A_CFG_ON_DONE_POS                                  2
#define F_QOF_CAM_A_CFG_ON_DONE_WIDTH                                1
#define F_QOF_CAM_A_ITC_DONE_POS                                     1
#define F_QOF_CAM_A_ITC_DONE_WIDTH                                   1
#define F_QOF_CAM_A_RTC_DONE_POS                                     0
#define F_QOF_CAM_A_RTC_DONE_WIDTH                                   1

#define REG_QOF_CAM_A_QOF_RTC_DLY_CNT                 0x8
#define REG_QOF_CAM_A_QOF_VOTER_DBG                   0xC
#define F_QOF_CAM_A_TRG_STATUS_POS                                   20
#define F_QOF_CAM_A_TRG_STATUS_WIDTH                                 2
#define F_QOF_CAM_A_VOTE_CHECK_POS                                   4
#define F_QOF_CAM_A_VOTE_CHECK_WIDTH                                 16
#define F_QOF_CAM_A_VOTE_POS                                         0
#define F_QOF_CAM_A_VOTE_WIDTH                                       4

#define REG_QOF_CAM_A_QOF_TRIG_CNT                   0x10
#define F_QOF_CAM_A_TRG_OFF_CNT_POS                                  16
#define F_QOF_CAM_A_TRG_OFF_CNT_WIDTH                                16
#define F_QOF_CAM_A_TRG_ON_CNT_POS                                   0
#define F_QOF_CAM_A_TRG_ON_CNT_WIDTH                                 16

#define REG_QOF_CAM_A_QOF_TRIG_CNT_CLR               0x14
#define F_QOF_CAM_A_TRG_OFF_CNT_CLR_POS                              1
#define F_QOF_CAM_A_TRG_OFF_CNT_CLR_WIDTH                            1
#define F_QOF_CAM_A_TRG_ON_CNT_CLR_POS                               0
#define F_QOF_CAM_A_TRG_ON_CNT_CLR_WIDTH                             1

#define REG_QOF_CAM_A_QOF_TIME_STAMP                 0x18
#define REG_QOF_CAM_A_QOF_MTC_CYC_MAX                0x1C
#define REG_QOF_CAM_A_QOF_PWR_OFF_MAX                0x20
#define REG_QOF_CAM_A_QOF_CQ_START_MAX               0x24
#define REG_QOF_CAM_A_QOF_GCE_CTL                    0x28
#define F_QOF_CAM_A_GCE_RESTORE_DONE_POS                             3
#define F_QOF_CAM_A_GCE_RESTORE_DONE_WIDTH                           1
#define F_QOF_CAM_A_GCE_RESTORE_EN_POS                               2
#define F_QOF_CAM_A_GCE_RESTORE_EN_WIDTH                             1
#define F_QOF_CAM_A_GCE_SAVE_DONE_POS                                1
#define F_QOF_CAM_A_GCE_SAVE_DONE_WIDTH                              1
#define F_QOF_CAM_A_GCE_SAVE_EN_POS                                  0
#define F_QOF_CAM_A_GCE_SAVE_EN_WIDTH                                1

#define REG_QOF_CAM_A_QOF_VSYNC_PRD                  0x2C
#define REG_QOF_CAM_A_QOF_STATE_DBG                  0x30
#define F_QOF_CAM_A_ACK_DBG_POS                                      21
#define F_QOF_CAM_A_ACK_DBG_WIDTH                                    10
#define F_QOF_CAM_A_HW_SEQ_ST_POS                                    12
#define F_QOF_CAM_A_HW_SEQ_ST_WIDTH                                  9
#define F_QOF_CAM_A_POWER_STATE_POS                                  0
#define F_QOF_CAM_A_POWER_STATE_WIDTH                                8

#define REG_QOF_CAM_A_QOF_RETENTION_CYCLE            0x34
#define F_QOF_CAM_A_RTFF_SAVE_RES_TH_POS                             16
#define F_QOF_CAM_A_RTFF_SAVE_RES_TH_WIDTH                           16
#define F_QOF_CAM_A_RTFF_CLK_DIS_TH_POS                              0
#define F_QOF_CAM_A_RTFF_CLK_DIS_TH_WIDTH                            16

#define REG_QOF_CAM_A_QOF_POWER_ACK_CYCLE            0x38
#define F_QOF_CAM_A_PWR_ACK_2ND_WAIT_TH_POS                          16
#define F_QOF_CAM_A_PWR_ACK_2ND_WAIT_TH_WIDTH                        16
#define F_QOF_CAM_A_PWR_ACK_WAIT_TH_POS                              0
#define F_QOF_CAM_A_PWR_ACK_WAIT_TH_WIDTH                            16

#define REG_QOF_CAM_A_QOF_POWER_ISO_CYCLE            0x3C
#define F_QOF_CAM_A_PWR_ISO_0_TH_POS                                 16
#define F_QOF_CAM_A_PWR_ISO_0_TH_WIDTH                               16
#define F_QOF_CAM_A_PWR_ISO_1_TH_POS                                 0
#define F_QOF_CAM_A_PWR_ISO_1_TH_WIDTH                               16

#define REG_QOF_CAM_A_QOF_POWER_CYCLE                0x40
#define F_QOF_CAM_A_SRAM_WAIT_TH_POS                                 16
#define F_QOF_CAM_A_SRAM_WAIT_TH_WIDTH                               16
#define F_QOF_CAM_A_PWR_CLK_DIS_TH_POS                               0
#define F_QOF_CAM_A_PWR_CLK_DIS_TH_WIDTH                             16

#define REG_QOF_CAM_A_QOF_DDREN_CYC_MAX              0x44
#define REG_QOF_CAM_A_QOF_BWQOS_CYC_MAX              0x48
#define REG_QOF_CAM_A_QOF_TIMEOUT_MAX                0x4C
#define REG_QOF_CAM_A_QOF_DEBOUNCE_CYC               0x50
#define REG_QOF_CAM_A_QOF_MTC_ST_RMS_LSB             0x54
#define REG_QOF_CAM_A_QOF_MTC_ST_RMS_MSB2            0x58
#define F_QOF_CAM_A_MTCMOS_ST_RMS_MSB2_POS                           0
#define F_QOF_CAM_A_MTCMOS_ST_RMS_MSB2_WIDTH                         2

#define REG_QOF_CAM_A_QOF_MTC_ST_RAW_LSB             0x5C
#define REG_QOF_CAM_A_QOF_MTC_ST_RAW_MSB2            0x60
#define F_QOF_CAM_A_MTCMOS_ST_RAW_MSB2_POS                           0
#define F_QOF_CAM_A_MTCMOS_ST_RAW_MSB2_WIDTH                         2

#define REG_QOF_CAM_A_INT1_STATUS_ADDR               0x64
#define F_QOF_CAM_A_INT1_STATUS_ADDR_POS                             0
#define F_QOF_CAM_A_INT1_STATUS_ADDR_WIDTH                           24

#define REG_QOF_CAM_A_INT2_STATUS_ADDR               0x68
#define F_QOF_CAM_A_INT2_STATUS_ADDR_POS                             0
#define F_QOF_CAM_A_INT2_STATUS_ADDR_WIDTH                           24

#define REG_QOF_CAM_A_INT3_STATUS_ADDR               0x6C
#define F_QOF_CAM_A_INT3_STATUS_ADDR_POS                             0
#define F_QOF_CAM_A_INT3_STATUS_ADDR_WIDTH                           24

#define REG_QOF_CAM_A_INT4_STATUS_ADDR               0x70
#define F_QOF_CAM_A_INT4_STATUS_ADDR_POS                             0
#define F_QOF_CAM_A_INT4_STATUS_ADDR_WIDTH                           24

#define REG_QOF_CAM_A_INT5_STATUS_ADDR               0x74
#define F_QOF_CAM_A_INT5_STATUS_ADDR_POS                             0
#define F_QOF_CAM_A_INT5_STATUS_ADDR_WIDTH                           24

#define REG_QOF_CAM_A_INT6_STATUS_ADDR               0x78
#define F_QOF_CAM_A_INT6_STATUS_ADDR_POS                             0
#define F_QOF_CAM_A_INT6_STATUS_ADDR_WIDTH                           24

#define REG_QOF_CAM_A_INT7_STATUS_ADDR               0x7C
#define F_QOF_CAM_A_INT7_STATUS_ADDR_POS                             0
#define F_QOF_CAM_A_INT7_STATUS_ADDR_WIDTH                           24

#define REG_QOF_CAM_A_INT8_STATUS_ADDR               0x80
#define F_QOF_CAM_A_INT8_STATUS_ADDR_POS                             0
#define F_QOF_CAM_A_INT8_STATUS_ADDR_WIDTH                           24

#define REG_QOF_CAM_A_INT9_STATUS_ADDR               0x84
#define F_QOF_CAM_A_INT9_STATUS_ADDR_POS                             0
#define F_QOF_CAM_A_INT9_STATUS_ADDR_WIDTH                           24

#define REG_QOF_CAM_A_INT10_STATUS_ADDR              0x88
#define F_QOF_CAM_A_INT10_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT10_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT11_STATUS_ADDR              0x8C
#define F_QOF_CAM_A_INT11_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT11_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT12_STATUS_ADDR              0x90
#define F_QOF_CAM_A_INT12_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT12_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT13_STATUS_ADDR              0x94
#define F_QOF_CAM_A_INT13_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT13_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT14_STATUS_ADDR              0x98
#define F_QOF_CAM_A_INT14_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT14_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT15_STATUS_ADDR              0x9C
#define F_QOF_CAM_A_INT15_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT15_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT16_STATUS_ADDR              0xA0
#define F_QOF_CAM_A_INT16_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT16_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT17_STATUS_ADDR              0xA4
#define F_QOF_CAM_A_INT17_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT17_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT18_STATUS_ADDR              0xA8
#define F_QOF_CAM_A_INT18_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT18_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT19_STATUS_ADDR              0xAC
#define F_QOF_CAM_A_INT19_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT19_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT20_STATUS_ADDR              0xB0
#define F_QOF_CAM_A_INT20_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT20_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT21_STATUS_ADDR              0xB4
#define F_QOF_CAM_A_INT21_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT21_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT22_STATUS_ADDR              0xB8
#define F_QOF_CAM_A_INT22_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT22_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT23_STATUS_ADDR              0xBC
#define F_QOF_CAM_A_INT23_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT23_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT24_STATUS_ADDR              0xC0
#define F_QOF_CAM_A_INT24_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT24_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT25_STATUS_ADDR              0xC4
#define F_QOF_CAM_A_INT25_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT25_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT26_STATUS_ADDR              0xC8
#define F_QOF_CAM_A_INT26_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT26_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT27_STATUS_ADDR              0xCC
#define F_QOF_CAM_A_INT27_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT27_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT28_STATUS_ADDR              0xD0
#define F_QOF_CAM_A_INT28_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT28_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT29_STATUS_ADDR              0xD4
#define F_QOF_CAM_A_INT29_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT29_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT30_STATUS_ADDR              0xD8
#define F_QOF_CAM_A_INT30_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT30_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT31_STATUS_ADDR              0xDC
#define F_QOF_CAM_A_INT31_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT31_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT32_STATUS_ADDR              0xE0
#define F_QOF_CAM_A_INT32_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT32_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT33_STATUS_ADDR              0xE4
#define F_QOF_CAM_A_INT33_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT33_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT34_STATUS_ADDR              0xE8
#define F_QOF_CAM_A_INT34_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT34_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT35_STATUS_ADDR              0xEC
#define F_QOF_CAM_A_INT35_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT35_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT36_STATUS_ADDR              0xF0
#define F_QOF_CAM_A_INT36_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT36_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT37_STATUS_ADDR              0xF4
#define F_QOF_CAM_A_INT37_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT37_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT38_STATUS_ADDR              0xF8
#define F_QOF_CAM_A_INT38_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT38_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT39_STATUS_ADDR              0xFC
#define F_QOF_CAM_A_INT39_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT39_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT40_STATUS_ADDR             0x100
#define F_QOF_CAM_A_INT40_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT40_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT41_STATUS_ADDR             0x104
#define F_QOF_CAM_A_INT41_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT41_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT42_STATUS_ADDR             0x108
#define F_QOF_CAM_A_INT42_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT42_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT43_STATUS_ADDR             0x10C
#define F_QOF_CAM_A_INT43_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT43_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT44_STATUS_ADDR             0x110
#define F_QOF_CAM_A_INT44_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT44_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT45_STATUS_ADDR             0x114
#define F_QOF_CAM_A_INT45_STATUS_ADDR_POS                            0
#define F_QOF_CAM_A_INT45_STATUS_ADDR_WIDTH                          24

#define REG_QOF_CAM_A_INT1_EN_APMCU                 0x150
#define REG_QOF_CAM_A_INT1_STATUS_APMCU             0x154
#define REG_QOF_CAM_A_INT1_EN_CCU                   0x158
#define REG_QOF_CAM_A_INT1_STATUS_CCU               0x15C
#define REG_QOF_CAM_A_INT2_EN_APMCU                 0x160
#define REG_QOF_CAM_A_INT2_STATUS_APMCU             0x164
#define REG_QOF_CAM_A_INT2_EN_CCU                   0x168
#define REG_QOF_CAM_A_INT2_STATUS_CCU               0x16C
#define REG_QOF_CAM_A_INT3_EN_APMCU                 0x170
#define REG_QOF_CAM_A_INT3_STATUS_APMCU             0x174
#define REG_QOF_CAM_A_INT3_EN_CCU                   0x178
#define REG_QOF_CAM_A_INT3_STATUS_CCU               0x17C
#define REG_QOF_CAM_A_INT4_EN_APMCU                 0x180
#define REG_QOF_CAM_A_INT4_STATUS_APMCU             0x184
#define REG_QOF_CAM_A_INT4_EN_CCU                   0x188
#define REG_QOF_CAM_A_INT4_STATUS_CCU               0x18C
#define REG_QOF_CAM_A_INT5_EN_APMCU                 0x190
#define REG_QOF_CAM_A_INT5_STATUS_APMCU             0x194
#define REG_QOF_CAM_A_INT5_EN_CCU                   0x198
#define REG_QOF_CAM_A_INT5_STATUS_CCU               0x19C
#define REG_QOF_CAM_A_INT6_EN_APMCU                 0x1A0
#define REG_QOF_CAM_A_INT6_STATUS_APMCU             0x1A4
#define REG_QOF_CAM_A_INT6_EN_CCU                   0x1A8
#define REG_QOF_CAM_A_INT6_STATUS_CCU               0x1AC
#define REG_QOF_CAM_A_INT7_EN_APMCU                 0x1B0
#define REG_QOF_CAM_A_INT7_STATUS_APMCU             0x1B4
#define REG_QOF_CAM_A_INT7_EN_CCU                   0x1B8
#define REG_QOF_CAM_A_INT7_STATUS_CCU               0x1BC
#define REG_QOF_CAM_A_INT8_EN_APMCU                 0x1C0
#define REG_QOF_CAM_A_INT8_STATUS_APMCU             0x1C4
#define REG_QOF_CAM_A_INT8_EN_CCU                   0x1C8
#define REG_QOF_CAM_A_INT8_STATUS_CCU               0x1CC
#define REG_QOF_CAM_A_INT9_EN_APMCU                 0x1D0
#define REG_QOF_CAM_A_INT9_STATUS_APMCU             0x1D4
#define REG_QOF_CAM_A_INT9_EN_CCU                   0x1D8
#define REG_QOF_CAM_A_INT9_STATUS_CCU               0x1DC
#define REG_QOF_CAM_A_INT10_EN_APMCU                0x1E0
#define REG_QOF_CAM_A_INT10_STATUS_APMCU            0x1E4
#define REG_QOF_CAM_A_INT10_EN_CCU                  0x1E8
#define REG_QOF_CAM_A_INT10_STATUS_CCU              0x1EC
#define REG_QOF_CAM_A_INT11_EN_APMCU                0x1F0
#define REG_QOF_CAM_A_INT11_STATUS_APMCU            0x1F4
#define REG_QOF_CAM_A_INT11_EN_CCU                  0x1F8
#define REG_QOF_CAM_A_INT11_STATUS_CCU              0x1FC
#define REG_QOF_CAM_A_INT12_EN_APMCU                0x200
#define REG_QOF_CAM_A_INT12_STATUS_APMCU            0x204
#define REG_QOF_CAM_A_INT12_EN_CCU                  0x208
#define REG_QOF_CAM_A_INT12_STATUS_CCU              0x20C
#define REG_QOF_CAM_A_INT13_EN_APMCU                0x210
#define REG_QOF_CAM_A_INT13_STATUS_APMCU            0x214
#define REG_QOF_CAM_A_INT13_EN_CCU                  0x218
#define REG_QOF_CAM_A_INT13_STATUS_CCU              0x21C
#define REG_QOF_CAM_A_INT14_EN_APMCU                0x220
#define REG_QOF_CAM_A_INT14_STATUS_APMCU            0x224
#define REG_QOF_CAM_A_INT14_EN_CCU                  0x228
#define REG_QOF_CAM_A_INT14_STATUS_CCU              0x22C
#define REG_QOF_CAM_A_INT15_EN_APMCU                0x230
#define REG_QOF_CAM_A_INT15_STATUS_APMCU            0x234
#define REG_QOF_CAM_A_INT15_EN_CCU                  0x238
#define REG_QOF_CAM_A_INT15_STATUS_CCU              0x23C
#define REG_QOF_CAM_A_INT16_EN_APMCU                0x240
#define REG_QOF_CAM_A_INT16_STATUS_APMCU            0x244
#define REG_QOF_CAM_A_INT16_EN_CCU                  0x248
#define REG_QOF_CAM_A_INT16_STATUS_CCU              0x24C
#define REG_QOF_CAM_A_INT17_EN_APMCU                0x250
#define REG_QOF_CAM_A_INT17_STATUS_APMCU            0x254
#define REG_QOF_CAM_A_INT17_EN_CCU                  0x258
#define REG_QOF_CAM_A_INT17_STATUS_CCU              0x25C
#define REG_QOF_CAM_A_INT18_EN_APMCU                0x260
#define REG_QOF_CAM_A_INT18_STATUS_APMCU            0x264
#define REG_QOF_CAM_A_INT18_EN_CCU                  0x268
#define REG_QOF_CAM_A_INT18_STATUS_CCU              0x26C
#define REG_QOF_CAM_A_INT19_EN_APMCU                0x270
#define REG_QOF_CAM_A_INT19_STATUS_APMCU            0x274
#define REG_QOF_CAM_A_INT19_EN_CCU                  0x278
#define REG_QOF_CAM_A_INT19_STATUS_CCU              0x27C
#define REG_QOF_CAM_A_INT20_EN_APMCU                0x280
#define REG_QOF_CAM_A_INT20_STATUS_APMCU            0x284
#define REG_QOF_CAM_A_INT20_EN_CCU                  0x288
#define REG_QOF_CAM_A_INT20_STATUS_CCU              0x28C
#define REG_QOF_CAM_A_INT21_EN_APMCU                0x290
#define REG_QOF_CAM_A_INT21_STATUS_APMCU            0x294
#define REG_QOF_CAM_A_INT21_EN_CCU                  0x298
#define REG_QOF_CAM_A_INT21_STATUS_CCU              0x29C
#define REG_QOF_CAM_A_INT22_EN_APMCU                0x2A0
#define REG_QOF_CAM_A_INT22_STATUS_APMCU            0x2A4
#define REG_QOF_CAM_A_INT22_EN_CCU                  0x2A8
#define REG_QOF_CAM_A_INT22_STATUS_CCU              0x2AC
#define REG_QOF_CAM_A_INT23_EN_APMCU                0x2B0
#define REG_QOF_CAM_A_INT23_STATUS_APMCU            0x2B4
#define REG_QOF_CAM_A_INT23_EN_CCU                  0x2B8
#define REG_QOF_CAM_A_INT23_STATUS_CCU              0x2BC
#define REG_QOF_CAM_A_INT24_EN_APMCU                0x2C0
#define REG_QOF_CAM_A_INT24_STATUS_APMCU            0x2C4
#define REG_QOF_CAM_A_INT24_EN_CCU                  0x2C8
#define REG_QOF_CAM_A_INT24_STATUS_CCU              0x2CC
#define REG_QOF_CAM_A_INT25_EN_APMCU                0x2D0
#define REG_QOF_CAM_A_INT25_STATUS_APMCU            0x2D4
#define REG_QOF_CAM_A_INT25_EN_CCU                  0x2D8
#define REG_QOF_CAM_A_INT25_STATUS_CCU              0x2DC
#define REG_QOF_CAM_A_INT26_EN_APMCU                0x2E0
#define REG_QOF_CAM_A_INT26_STATUS_APMCU            0x2E4
#define REG_QOF_CAM_A_INT26_EN_CCU                  0x2E8
#define REG_QOF_CAM_A_INT26_STATUS_CCU              0x2EC
#define REG_QOF_CAM_A_INT27_EN_APMCU                0x2F0
#define REG_QOF_CAM_A_INT27_STATUS_APMCU            0x2F4
#define REG_QOF_CAM_A_INT27_EN_CCU                  0x2F8
#define REG_QOF_CAM_A_INT27_STATUS_CCU              0x2FC
#define REG_QOF_CAM_A_INT28_EN_APMCU                0x300
#define REG_QOF_CAM_A_INT28_STATUS_APMCU            0x304
#define REG_QOF_CAM_A_INT28_EN_CCU                  0x308
#define REG_QOF_CAM_A_INT28_STATUS_CCU              0x30C
#define REG_QOF_CAM_A_INT29_EN_APMCU                0x310
#define REG_QOF_CAM_A_INT29_STATUS_APMCU            0x314
#define REG_QOF_CAM_A_INT29_EN_CCU                  0x318
#define REG_QOF_CAM_A_INT29_STATUS_CCU              0x31C
#define REG_QOF_CAM_A_INT30_EN_APMCU                0x320
#define REG_QOF_CAM_A_INT30_STATUS_APMCU            0x324
#define REG_QOF_CAM_A_INT30_EN_CCU                  0x328
#define REG_QOF_CAM_A_INT30_STATUS_CCU              0x32C
#define REG_QOF_CAM_A_INT31_EN_APMCU                0x330
#define REG_QOF_CAM_A_INT31_STATUS_APMCU            0x334
#define REG_QOF_CAM_A_INT31_EN_CCU                  0x338
#define REG_QOF_CAM_A_INT31_STATUS_CCU              0x33C
#define REG_QOF_CAM_A_INT32_EN_APMCU                0x340
#define REG_QOF_CAM_A_INT32_STATUS_APMCU            0x344
#define REG_QOF_CAM_A_INT32_EN_CCU                  0x348
#define REG_QOF_CAM_A_INT32_STATUS_CCU              0x34C
#define REG_QOF_CAM_A_INT33_EN_APMCU                0x350
#define REG_QOF_CAM_A_INT33_STATUS_APMCU            0x354
#define REG_QOF_CAM_A_INT33_EN_CCU                  0x358
#define REG_QOF_CAM_A_INT33_STATUS_CCU              0x35C
#define REG_QOF_CAM_A_INT34_EN_APMCU                0x360
#define REG_QOF_CAM_A_INT34_STATUS_APMCU            0x364
#define REG_QOF_CAM_A_INT34_EN_CCU                  0x368
#define REG_QOF_CAM_A_INT34_STATUS_CCU              0x36C
#define REG_QOF_CAM_A_INT35_EN_APMCU                0x370
#define REG_QOF_CAM_A_INT35_STATUS_APMCU            0x374
#define REG_QOF_CAM_A_INT35_EN_CCU                  0x378
#define REG_QOF_CAM_A_INT35_STATUS_CCU              0x37C
#define REG_QOF_CAM_A_INT36_EN_APMCU                0x380
#define REG_QOF_CAM_A_INT36_STATUS_APMCU            0x384
#define REG_QOF_CAM_A_INT36_EN_CCU                  0x388
#define REG_QOF_CAM_A_INT36_STATUS_CCU              0x38C
#define REG_QOF_CAM_A_INT37_EN_APMCU                0x390
#define REG_QOF_CAM_A_INT37_STATUS_APMCU            0x394
#define REG_QOF_CAM_A_INT37_EN_CCU                  0x398
#define REG_QOF_CAM_A_INT37_STATUS_CCU              0x39C
#define REG_QOF_CAM_A_INT38_EN_APMCU                0x3A0
#define REG_QOF_CAM_A_INT38_STATUS_APMCU            0x3A4
#define REG_QOF_CAM_A_INT38_EN_CCU                  0x3A8
#define REG_QOF_CAM_A_INT38_STATUS_CCU              0x3AC
#define REG_QOF_CAM_A_INT39_EN_APMCU                0x3B0
#define REG_QOF_CAM_A_INT39_STATUS_APMCU            0x3B4
#define REG_QOF_CAM_A_INT39_EN_CCU                  0x3B8
#define REG_QOF_CAM_A_INT39_STATUS_CCU              0x3BC
#define REG_QOF_CAM_A_INT40_EN_APMCU                0x3C0
#define REG_QOF_CAM_A_INT40_STATUS_APMCU            0x3C4
#define REG_QOF_CAM_A_INT40_EN_CCU                  0x3C8
#define REG_QOF_CAM_A_INT40_STATUS_CCU              0x3CC
#define REG_QOF_CAM_A_INT41_EN_APMCU                0x3D0
#define REG_QOF_CAM_A_INT41_STATUS_APMCU            0x3D4
#define REG_QOF_CAM_A_INT41_EN_CCU                  0x3D8
#define REG_QOF_CAM_A_INT41_STATUS_CCU              0x3DC
#define REG_QOF_CAM_A_INT42_EN_APMCU                0x3E0
#define REG_QOF_CAM_A_INT42_STATUS_APMCU            0x3E4
#define REG_QOF_CAM_A_INT42_EN_CCU                  0x3E8
#define REG_QOF_CAM_A_INT42_STATUS_CCU              0x3EC
#define REG_QOF_CAM_A_INT43_EN_APMCU                0x3F0
#define REG_QOF_CAM_A_INT43_STATUS_APMCU            0x3F4
#define REG_QOF_CAM_A_INT43_EN_CCU                  0x3F8
#define REG_QOF_CAM_A_INT43_STATUS_CCU              0x3FC
#define REG_QOF_CAM_A_INT44_EN_APMCU                0x400
#define REG_QOF_CAM_A_INT44_STATUS_APMCU            0x404
#define REG_QOF_CAM_A_INT44_EN_CCU                  0x408
#define REG_QOF_CAM_A_INT44_STATUS_CCU              0x40C
#define REG_QOF_CAM_A_INT45_EN_APMCU                0x410
#define REG_QOF_CAM_A_INT45_STATUS_APMCU            0x414
#define REG_QOF_CAM_A_INT45_EN_CCU                  0x418
#define REG_QOF_CAM_A_INT45_STATUS_CCU              0x41C
#define REG_QOF_CAM_A_INT_WCLR_EN                   0x420
#define F_QOF_CAM_A_INT_WCLR_EN_POS                                  0
#define F_QOF_CAM_A_INT_WCLR_EN_WIDTH                                1

#define REG_QOF_CAM_A_TRANS1_ADDR                   0x460
#define F_QOF_CAM_A_TRANS1_ADDR_POS                                  0
#define F_QOF_CAM_A_TRANS1_ADDR_WIDTH                                24

#define REG_QOF_CAM_A_TRANS2_ADDR                   0x464
#define F_QOF_CAM_A_TRANS2_ADDR_POS                                  0
#define F_QOF_CAM_A_TRANS2_ADDR_WIDTH                                24

#define REG_QOF_CAM_A_TRANS3_ADDR                   0x468
#define F_QOF_CAM_A_TRANS3_ADDR_POS                                  0
#define F_QOF_CAM_A_TRANS3_ADDR_WIDTH                                24

#define REG_QOF_CAM_A_TRANS4_ADDR                   0x46C
#define F_QOF_CAM_A_TRANS4_ADDR_POS                                  0
#define F_QOF_CAM_A_TRANS4_ADDR_WIDTH                                24

#define REG_QOF_CAM_A_TRANS5_ADDR                   0x470
#define F_QOF_CAM_A_TRANS5_ADDR_POS                                  0
#define F_QOF_CAM_A_TRANS5_ADDR_WIDTH                                24

#define REG_QOF_CAM_A_TRANS6_ADDR                   0x474
#define F_QOF_CAM_A_TRANS6_ADDR_POS                                  0
#define F_QOF_CAM_A_TRANS6_ADDR_WIDTH                                24

#define REG_QOF_CAM_A_TRANS7_ADDR                   0x478
#define F_QOF_CAM_A_TRANS7_ADDR_POS                                  0
#define F_QOF_CAM_A_TRANS7_ADDR_WIDTH                                24

#define REG_QOF_CAM_A_TRANS8_ADDR                   0x47C
#define F_QOF_CAM_A_TRANS8_ADDR_POS                                  0
#define F_QOF_CAM_A_TRANS8_ADDR_WIDTH                                24

#define REG_QOF_CAM_A_TRANS9_ADDR                   0x480
#define F_QOF_CAM_A_TRANS9_ADDR_POS                                  0
#define F_QOF_CAM_A_TRANS9_ADDR_WIDTH                                24

#define REG_QOF_CAM_A_TRANS10_ADDR                  0x484
#define F_QOF_CAM_A_TRANS10_ADDR_POS                                 0
#define F_QOF_CAM_A_TRANS10_ADDR_WIDTH                               24

#define REG_QOF_CAM_A_TRANS11_ADDR                  0x488
#define F_QOF_CAM_A_TRANS11_ADDR_POS                                 0
#define F_QOF_CAM_A_TRANS11_ADDR_WIDTH                               24

#define REG_QOF_CAM_A_TRANS12_ADDR                  0x48C
#define F_QOF_CAM_A_TRANS12_ADDR_POS                                 0
#define F_QOF_CAM_A_TRANS12_ADDR_WIDTH                               24

#define REG_QOF_CAM_A_TRANS13_ADDR                  0x490
#define F_QOF_CAM_A_TRANS13_ADDR_POS                                 0
#define F_QOF_CAM_A_TRANS13_ADDR_WIDTH                               24

#define REG_QOF_CAM_A_TRANS14_ADDR                  0x494
#define F_QOF_CAM_A_TRANS14_ADDR_POS                                 0
#define F_QOF_CAM_A_TRANS14_ADDR_WIDTH                               24

#define REG_QOF_CAM_A_TRANS15_ADDR                  0x498
#define F_QOF_CAM_A_TRANS15_ADDR_POS                                 0
#define F_QOF_CAM_A_TRANS15_ADDR_WIDTH                               24

#define REG_QOF_CAM_A_TRANS16_ADDR                  0x49C
#define F_QOF_CAM_A_TRANS16_ADDR_POS                                 0
#define F_QOF_CAM_A_TRANS16_ADDR_WIDTH                               24

#define REG_QOF_CAM_A_TRANS1_DATA                   0x4A0
#define REG_QOF_CAM_A_TRANS2_DATA                   0x4A4
#define REG_QOF_CAM_A_TRANS3_DATA                   0x4A8
#define REG_QOF_CAM_A_TRANS4_DATA                   0x4AC
#define REG_QOF_CAM_A_TRANS5_DATA                   0x4B0
#define REG_QOF_CAM_A_TRANS6_DATA                   0x4B4
#define REG_QOF_CAM_A_TRANS7_DATA                   0x4B8
#define REG_QOF_CAM_A_TRANS8_DATA                   0x4BC
#define REG_QOF_CAM_A_TRANS9_DATA                   0x4C0
#define REG_QOF_CAM_A_TRANS10_DATA                  0x4C4
#define REG_QOF_CAM_A_TRANS11_DATA                  0x4C8
#define REG_QOF_CAM_A_TRANS12_DATA                  0x4CC
#define REG_QOF_CAM_A_TRANS13_DATA                  0x4D0
#define REG_QOF_CAM_A_TRANS14_DATA                  0x4D4
#define REG_QOF_CAM_A_TRANS15_DATA                  0x4D8
#define REG_QOF_CAM_A_TRANS16_DATA                  0x4DC
#define REG_QOF_CAM_A_QOF_SPARE1                    0x4E0
#define REG_QOF_CAM_A_QOF_SPARE2                    0x4E4
#define REG_QOF_CAM_A_QOF_SPARE3                    0x4E8
#define REG_QOF_CAM_A_QOF_SPARE4                    0x4EC
#define REG_QOF_CAM_A_QOF_CQ1_ADDR                  0x4F0
#define F_QOF_CAM_A_QOF_CQ1_ADDR_POS                                 0
#define F_QOF_CAM_A_QOF_CQ1_ADDR_WIDTH                               24

#define REG_QOF_CAM_A_QOF_CQ2_ADDR                  0x4F4
#define F_QOF_CAM_A_QOF_CQ2_ADDR_POS                                 0
#define F_QOF_CAM_A_QOF_CQ2_ADDR_WIDTH                               24

#define REG_QOF_CAM_A_QOF_CQ3_ADDR                  0x4F8
#define F_QOF_CAM_A_QOF_CQ3_ADDR_POS                                 0
#define F_QOF_CAM_A_QOF_CQ3_ADDR_WIDTH                               24

#define REG_QOF_CAM_A_QOF_CQ4_ADDR                  0x4FC
#define F_QOF_CAM_A_QOF_CQ4_ADDR_POS                                 0
#define F_QOF_CAM_A_QOF_CQ4_ADDR_WIDTH                               24

#define REG_QOF_CAM_A_QOF_CQ5_ADDR                  0x500
#define F_QOF_CAM_A_QOF_CQ5_ADDR_POS                                 0
#define F_QOF_CAM_A_QOF_CQ5_ADDR_WIDTH                               24

#define REG_QOF_CAM_A_QOF_CQ6_ADDR                  0x504
#define F_QOF_CAM_A_QOF_CQ6_ADDR_POS                                 0
#define F_QOF_CAM_A_QOF_CQ6_ADDR_WIDTH                               24

#define REG_QOF_CAM_A_QOF_CQ1_DATA                  0x508
#define REG_QOF_CAM_A_QOF_CQ2_DATA                  0x50C
#define REG_QOF_CAM_A_QOF_CQ3_DATA                  0x510
#define REG_QOF_CAM_A_QOF_CQ4_DATA                  0x514
#define REG_QOF_CAM_A_QOF_CQ5_DATA                  0x518
#define REG_QOF_CAM_A_QOF_CQ6_DATA                  0x51C
#define REG_QOF_CAM_A_QOF_CID_CHK_INT_STATUSX       0x530
#define F_QOF_CAM_A_QOF_CID_CHK_INT_STX_POS                          0
#define F_QOF_CAM_A_QOF_CID_CHK_INT_STX_WIDTH                        1

#define REG_QOF_CAM_A_QOF_CID_CHK_INT_WCLR_EN       0x534
#define F_QOF_CAM_A_QOF_CID_CHK_INT_WCLR_EN_POS                      0
#define F_QOF_CAM_A_QOF_CID_CHK_INT_WCLR_EN_WIDTH                    1

#define REG_QOF_CAM_A_QOF_COH_CTL                   0x538
#define F_QOF_CAM_A_COH_SW_POS                                       3
#define F_QOF_CAM_A_COH_SW_WIDTH                                     1
#define F_QOF_CAM_A_COH_HW_EN_POS                                    2
#define F_QOF_CAM_A_COH_HW_EN_WIDTH                                  1
#define F_QOF_CAM_A_COH_SW_CLR_POS                                   1
#define F_QOF_CAM_A_COH_SW_CLR_WIDTH                                 1
#define F_QOF_CAM_A_COH_SW_SET_POS                                   0
#define F_QOF_CAM_A_COH_SW_SET_WIDTH                                 1

#define REG_QOF_CAM_A_QOF_COH_CYC_MAX               0x53C

/* following added manually */
#define QOF_SEQ_MODE_RTC_THEN_ITC                   0
#define F_QOF_CAM_TOP_QOF_SW_RST_RAWA_SW_RST_POS    6
#define F_QOF_CAM_TOP_QOF_SW_RST_RAWA_SW_RST_WIDTH    1
#define F_QOF_CAM_TOP_QOF_SW_RST_RAWB_SW_RST_POS    7
#define F_QOF_CAM_TOP_QOF_SW_RST_RAWB_SW_RST_WIDTH    1
#define F_QOF_CAM_TOP_QOF_SW_RST_RAWC_SW_RST_POS    8
#define F_QOF_CAM_TOP_QOF_SW_RST_RAWC_SW_RST_WIDTH    1

#define REG_CAM_VCORE_PM_RAWA                   0x44
#define REG_CAM_VCORE_PM_RAWB                   0x48
#define REG_CAM_VCORE_PM_RAWC                   0x4C
#define REG_CAM_VCORE_PM_RMSA                   0x50
#define REG_CAM_VCORE_PM_RMSB                   0x54
#define REG_CAM_VCORE_PM_RMSC                   0x58

#define REG_HWCCF_BASE                          0x31C00000
#define REG_HWCCF_LINK_SET_ADDR                 0x3FB0
#define REG_HWCCF_LINK_CLR_ADDR                 0x3FB4
#define REG_HWCCF_LINK_STA_ADDR                 0x3FB8

#endif	/* _MTK_CAM_QOF_REGS_H */
