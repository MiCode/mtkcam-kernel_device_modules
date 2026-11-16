/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_CAM_FMON_REGS_H
#define _MTK_CAM_FMON_REGS_H

/* baseaddr 0x3C828800 */

/* module: CAM_FMON_E1A */
#define REG_CAM_FMON_SETTING                          0x0
#define F_CAM_FMON_ELA_BUS_SEL_POS                                   23
#define F_CAM_FMON_ELA_BUS_SEL_WIDTH                                 1
#define F_CAM_FMON_IRQ_CLEAR_POS                                     22
#define F_CAM_FMON_IRQ_CLEAR_WIDTH                                   1
#define F_CAM_FMON_URGENT_CLEAR_POS                                  21
#define F_CAM_FMON_URGENT_CLEAR_WIDTH                                1
#define F_CAM_FMON_URGENT_MODE_POS                                   20
#define F_CAM_FMON_URGENT_MODE_WIDTH                                 1
#define F_CAM_FMON_FIFO_TYPE_3_POS                                   19
#define F_CAM_FMON_FIFO_TYPE_3_WIDTH                                 1
#define F_CAM_FMON_FIFO_TYPE_2_POS                                   18
#define F_CAM_FMON_FIFO_TYPE_2_WIDTH                                 1
#define F_CAM_FMON_FIFO_TYPE_1_POS                                   17
#define F_CAM_FMON_FIFO_TYPE_1_WIDTH                                 1
#define F_CAM_FMON_FIFO_TYPE_0_POS                                   16
#define F_CAM_FMON_FIFO_TYPE_0_WIDTH                                 1
#define F_CAM_FMON_STOP_MASK_3_POS                                   15
#define F_CAM_FMON_STOP_MASK_3_WIDTH                                 1
#define F_CAM_FMON_STOP_MASK_2_POS                                   14
#define F_CAM_FMON_STOP_MASK_2_WIDTH                                 1
#define F_CAM_FMON_STOP_MASK_1_POS                                   13
#define F_CAM_FMON_STOP_MASK_1_WIDTH                                 1
#define F_CAM_FMON_STOP_MASK_0_POS                                   12
#define F_CAM_FMON_STOP_MASK_0_WIDTH                                 1
#define F_CAM_FMON_SRC_SEL_3_POS                                     10
#define F_CAM_FMON_SRC_SEL_3_WIDTH                                   2
#define F_CAM_FMON_SRC_SEL_2_POS                                     8
#define F_CAM_FMON_SRC_SEL_2_WIDTH                                   2
#define F_CAM_FMON_SRC_SEL_1_POS                                     6
#define F_CAM_FMON_SRC_SEL_1_WIDTH                                   2
#define F_CAM_FMON_SRC_SEL_0_POS                                     4
#define F_CAM_FMON_SRC_SEL_0_WIDTH                                   2
#define F_CAM_FMON_FIFO_MON_EN_POS                                   0
#define F_CAM_FMON_FIFO_MON_EN_WIDTH                                 1

#define REG_CAM_FMON_THRESHOLD_0                      0x4
#define F_CAM_FMON_PER_THR_0_POS                                     13
#define F_CAM_FMON_PER_THR_0_WIDTH                                   7
#define F_CAM_FMON_FIFO_THR_0_POS                                    0
#define F_CAM_FMON_FIFO_THR_0_WIDTH                                  13

#define REG_CAM_FMON_THRESHOLD_1                      0x8
#define F_CAM_FMON_PER_THR_1_POS                                     13
#define F_CAM_FMON_PER_THR_1_WIDTH                                   7
#define F_CAM_FMON_FIFO_THR_1_POS                                    0
#define F_CAM_FMON_FIFO_THR_1_WIDTH                                  13

#define REG_CAM_FMON_THRESHOLD_2                      0xC
#define F_CAM_FMON_PER_THR_2_POS                                     13
#define F_CAM_FMON_PER_THR_2_WIDTH                                   7
#define F_CAM_FMON_FIFO_THR_2_POS                                    0
#define F_CAM_FMON_FIFO_THR_2_WIDTH                                  13

#define REG_CAM_FMON_THRESHOLD_3                     0x10
#define F_CAM_FMON_PER_THR_3_POS                                     13
#define F_CAM_FMON_PER_THR_3_WIDTH                                   7
#define F_CAM_FMON_FIFO_THR_3_POS                                    0
#define F_CAM_FMON_FIFO_THR_3_WIDTH                                  13

#define REG_CAM_FMON_WIND_SET_0                      0x14
#define F_CAM_FMON_WINDOW_LEN_1_POS                                  15
#define F_CAM_FMON_WINDOW_LEN_1_WIDTH                                15
#define F_CAM_FMON_WINDOW_LEN_0_POS                                  0
#define F_CAM_FMON_WINDOW_LEN_0_WIDTH                                15

#define REG_CAM_FMON_WIND_SET_1                      0x18
#define F_CAM_FMON_WINDOW_LEN_3_POS                                  15
#define F_CAM_FMON_WINDOW_LEN_3_WIDTH                                15
#define F_CAM_FMON_WINDOW_LEN_2_POS                                  0
#define F_CAM_FMON_WINDOW_LEN_2_WIDTH                                15

#define REG_CAM_FMON_SETTING_2                       0x1C
#define F_CAM_FMON_STATUS_3_POS                                      29
#define F_CAM_FMON_STATUS_3_WIDTH                                    3
#define F_CAM_FMON_STATUS_2_POS                                      26
#define F_CAM_FMON_STATUS_2_WIDTH                                    3
#define F_CAM_FMON_STATUS_1_POS                                      23
#define F_CAM_FMON_STATUS_1_WIDTH                                    3
#define F_CAM_FMON_STATUS_0_POS                                      20
#define F_CAM_FMON_STATUS_0_WIDTH                                    3
#define F_CAM_FMON_TRIG_MUX_SEL_3_POS                                19
#define F_CAM_FMON_TRIG_MUX_SEL_3_WIDTH                              1
#define F_CAM_FMON_TRIG_MUX_SEL_2_POS                                18
#define F_CAM_FMON_TRIG_MUX_SEL_2_WIDTH                              1
#define F_CAM_FMON_TRIG_MUX_SEL_1_POS                                17
#define F_CAM_FMON_TRIG_MUX_SEL_1_WIDTH                              1
#define F_CAM_FMON_TRIG_MUX_SEL_0_POS                                16
#define F_CAM_FMON_TRIG_MUX_SEL_0_WIDTH                              1
#define F_CAM_FMON_TRIG_SRC_MUX_SEL_3_POS                            15
#define F_CAM_FMON_TRIG_SRC_MUX_SEL_3_WIDTH                          1
#define F_CAM_FMON_TRIG_SRC_MUX_SEL_2_POS                            14
#define F_CAM_FMON_TRIG_SRC_MUX_SEL_2_WIDTH                          1
#define F_CAM_FMON_TRIG_SRC_MUX_SEL_1_POS                            13
#define F_CAM_FMON_TRIG_SRC_MUX_SEL_1_WIDTH                          1
#define F_CAM_FMON_TRIG_SRC_MUX_SEL_0_POS                            12
#define F_CAM_FMON_TRIG_SRC_MUX_SEL_0_WIDTH                          1
#define F_CAM_FMON_URGENT_MASK_3_POS                                 11
#define F_CAM_FMON_URGENT_MASK_3_WIDTH                               1
#define F_CAM_FMON_URGENT_MASK_2_POS                                 10
#define F_CAM_FMON_URGENT_MASK_2_WIDTH                               1
#define F_CAM_FMON_URGENT_MASK_1_POS                                 9
#define F_CAM_FMON_URGENT_MASK_1_WIDTH                               1
#define F_CAM_FMON_URGENT_MASK_0_POS                                 8
#define F_CAM_FMON_URGENT_MASK_0_WIDTH                               1
#define F_CAM_FMON_IRQ_END_PATH_MSK_3_POS                            7
#define F_CAM_FMON_IRQ_END_PATH_MSK_3_WIDTH                          1
#define F_CAM_FMON_IRQ_END_PATH_MSK_2_POS                            6
#define F_CAM_FMON_IRQ_END_PATH_MSK_2_WIDTH                          1
#define F_CAM_FMON_IRQ_END_PATH_MSK_1_POS                            5
#define F_CAM_FMON_IRQ_END_PATH_MSK_1_WIDTH                          1
#define F_CAM_FMON_IRQ_END_PATH_MSK_0_POS                            4
#define F_CAM_FMON_IRQ_END_PATH_MSK_0_WIDTH                          1
#define F_CAM_FMON_IRQ_START_PATH_MSK_3_POS                          3
#define F_CAM_FMON_IRQ_START_PATH_MSK_3_WIDTH                        1
#define F_CAM_FMON_IRQ_START_PATH_MSK_2_POS                          2
#define F_CAM_FMON_IRQ_START_PATH_MSK_2_WIDTH                        1
#define F_CAM_FMON_IRQ_START_PATH_MSK_1_POS                          1
#define F_CAM_FMON_IRQ_START_PATH_MSK_1_WIDTH                        1
#define F_CAM_FMON_IRQ_START_PATH_MSK_0_POS                          0
#define F_CAM_FMON_IRQ_START_PATH_MSK_0_WIDTH                        1

#define REG_CAM_FMON_SETTING_3                       0x20
#define F_CAM_FMON_STOP_DEP_PTH_MSK_3_POS                            20
#define F_CAM_FMON_STOP_DEP_PTH_MSK_3_WIDTH                          1
#define F_CAM_FMON_STOP_DEP_PTH_MSK_2_POS                            19
#define F_CAM_FMON_STOP_DEP_PTH_MSK_2_WIDTH                          1
#define F_CAM_FMON_STOP_DEP_PTH_MSK_1_POS                            18
#define F_CAM_FMON_STOP_DEP_PTH_MSK_1_WIDTH                          1
#define F_CAM_FMON_STOP_DEP_PTH_MSK_0_POS                            17
#define F_CAM_FMON_STOP_DEP_PTH_MSK_0_WIDTH                          1
#define F_CAM_FMON_TRIG_SRC_SEL_3_POS                                16
#define F_CAM_FMON_TRIG_SRC_SEL_3_WIDTH                              1
#define F_CAM_FMON_TRIG_SRC_SEL_2_POS                                15
#define F_CAM_FMON_TRIG_SRC_SEL_2_WIDTH                              1
#define F_CAM_FMON_TRIG_SRC_SEL_1_POS                                14
#define F_CAM_FMON_TRIG_SRC_SEL_1_WIDTH                              1
#define F_CAM_FMON_TRIG_SRC_SEL_0_POS                                13
#define F_CAM_FMON_TRIG_SRC_SEL_0_WIDTH                              1
#define F_CAM_FMON_FMON_O_TRIG_MSK_3_POS                             12
#define F_CAM_FMON_FMON_O_TRIG_MSK_3_WIDTH                           1
#define F_CAM_FMON_FMON_O_TRIG_MSK_2_POS                             11
#define F_CAM_FMON_FMON_O_TRIG_MSK_2_WIDTH                           1
#define F_CAM_FMON_FMON_O_TRIG_MSK_1_POS                             10
#define F_CAM_FMON_FMON_O_TRIG_MSK_1_WIDTH                           1
#define F_CAM_FMON_FMON_O_TRIG_MSK_0_POS                             9
#define F_CAM_FMON_FMON_O_TRIG_MSK_0_WIDTH                           1
#define F_CAM_FMON_FMON_MODE_POS                                     8
#define F_CAM_FMON_FMON_MODE_WIDTH                                   1
#define F_CAM_FMON_CTI_STOP_PATH_MSK_3_POS                           7
#define F_CAM_FMON_CTI_STOP_PATH_MSK_3_WIDTH                         1
#define F_CAM_FMON_CTI_STOP_PATH_MSK_2_POS                           6
#define F_CAM_FMON_CTI_STOP_PATH_MSK_2_WIDTH                         1
#define F_CAM_FMON_CTI_STOP_PATH_MSK_1_POS                           5
#define F_CAM_FMON_CTI_STOP_PATH_MSK_1_WIDTH                         1
#define F_CAM_FMON_CTI_STOP_PATH_MSK_0_POS                           4
#define F_CAM_FMON_CTI_STOP_PATH_MSK_0_WIDTH                         1
#define F_CAM_FMON_CTI_START_PATH_MSK_3_POS                          3
#define F_CAM_FMON_CTI_START_PATH_MSK_3_WIDTH                        1
#define F_CAM_FMON_CTI_START_PATH_MSK_2_POS                          2
#define F_CAM_FMON_CTI_START_PATH_MSK_2_WIDTH                        1
#define F_CAM_FMON_CTI_START_PATH_MSK_1_POS                          1
#define F_CAM_FMON_CTI_START_PATH_MSK_1_WIDTH                        1
#define F_CAM_FMON_CTI_START_PATH_MSK_0_POS                          0
#define F_CAM_FMON_CTI_START_PATH_MSK_0_WIDTH                        1

#define REG_CAM_FMON_EXT_GALS_STATUS                 0x24
#define REG_CAM_FMON_EXT_GALS_CTRL                   0x28
#define REG_CAM_FMON_FT_PAT_STATUS_0                 0x2C
#define F_CAM_FMON_FT_TEST_STATUS_1_POS                              16
#define F_CAM_FMON_FT_TEST_STATUS_1_WIDTH                            16
#define F_CAM_FMON_FT_TEST_STATUS_0_POS                              0
#define F_CAM_FMON_FT_TEST_STATUS_0_WIDTH                            16

#define REG_CAM_FMON_FT_PAT_STATUS_1                 0x30
#define F_CAM_FMON_FT_TEST_STATUS_3_POS                              16
#define F_CAM_FMON_FT_TEST_STATUS_3_WIDTH                            16
#define F_CAM_FMON_FT_TEST_STATUS_2_POS                              0
#define F_CAM_FMON_FT_TEST_STATUS_2_WIDTH                            16

#define REG_CAM_FMON_FT_PAT_STATUS_2                 0x34
#define F_CAM_FMON_FT_TEST_STATUS_5_POS                              16
#define F_CAM_FMON_FT_TEST_STATUS_5_WIDTH                            16
#define F_CAM_FMON_FT_TEST_STATUS_4_POS                              0
#define F_CAM_FMON_FT_TEST_STATUS_4_WIDTH                            16

#define REG_CAM_FMON_FT_PAT_STATUS_3                 0x38
#define F_CAM_FMON_FT_TEST_STATUS_7_POS                              16
#define F_CAM_FMON_FT_TEST_STATUS_7_WIDTH                            16
#define F_CAM_FMON_FT_TEST_STATUS_6_POS                              0
#define F_CAM_FMON_FT_TEST_STATUS_6_WIDTH                            16

#define REG_CAM_FMON_FT_PAT_STATUS_4                 0x3C
#define F_CAM_FMON_FT_TEST_STATUS_9_POS                              16
#define F_CAM_FMON_FT_TEST_STATUS_9_WIDTH                            16
#define F_CAM_FMON_FT_TEST_STATUS_8_POS                              0
#define F_CAM_FMON_FT_TEST_STATUS_8_WIDTH                            16

#define REG_CAM_FMON_IRQ_TRIG                        0x40
#define F_CAM_FMON_CSR_IRQ_POL_SEL_OUT_POS                           10
#define F_CAM_FMON_CSR_IRQ_POL_SEL_OUT_WIDTH                         4
#define F_CAM_FMON_CSR_IRQ_POL_SEL_INTERNAL_POS                      6
#define F_CAM_FMON_CSR_IRQ_POL_SEL_INTERNAL_WIDTH                    4
#define F_CAM_FMON_CSR_IRQ_CLR_MODE_POS                              5
#define F_CAM_FMON_CSR_IRQ_CLR_MODE_WIDTH                            1
#define F_CAM_FMON_CSR_IRQ_TRIG_3_POS                                4
#define F_CAM_FMON_CSR_IRQ_TRIG_3_WIDTH                              1
#define F_CAM_FMON_CSR_IRQ_TRIG_2_POS                                3
#define F_CAM_FMON_CSR_IRQ_TRIG_2_WIDTH                              1
#define F_CAM_FMON_CSR_IRQ_TRIG_1_POS                                2
#define F_CAM_FMON_CSR_IRQ_TRIG_1_WIDTH                              1
#define F_CAM_FMON_CSR_IRQ_TRIG_0_POS                                1
#define F_CAM_FMON_CSR_IRQ_TRIG_0_WIDTH                              1
#define F_CAM_FMON_CSR_VERIF_EN_POS                                  0
#define F_CAM_FMON_CSR_VERIF_EN_WIDTH                                1

#define REG_CAM_FMON_DELAY_EXT                       0x44
#define F_CAM_FMON_TRIG_WIN_EXT_CNT_3_POS                            21
#define F_CAM_FMON_TRIG_WIN_EXT_CNT_3_WIDTH                          7
#define F_CAM_FMON_TRIG_WIN_EXT_CNT_2_POS                            14
#define F_CAM_FMON_TRIG_WIN_EXT_CNT_2_WIDTH                          7
#define F_CAM_FMON_TRIG_WIN_EXT_CNT_1_POS                            7
#define F_CAM_FMON_TRIG_WIN_EXT_CNT_1_WIDTH                          7
#define F_CAM_FMON_TRIG_WIN_EXT_CNT_0_POS                            0
#define F_CAM_FMON_TRIG_WIN_EXT_CNT_0_WIDTH                          7

#define REG_CAM_FMON_WIN_DELAY_CNT                   0x48
#define F_CAM_FMON_TRIG_STOP_WIN_DLY_CNT_3_POS                       21
#define F_CAM_FMON_TRIG_STOP_WIN_DLY_CNT_3_WIDTH                     7
#define F_CAM_FMON_TRIG_STOP_WIN_DLY_CNT_2_POS                       14
#define F_CAM_FMON_TRIG_STOP_WIN_DLY_CNT_2_WIDTH                     7
#define F_CAM_FMON_TRIG_STOP_WIN_DLY_CNT_1_POS                       7
#define F_CAM_FMON_TRIG_STOP_WIN_DLY_CNT_1_WIDTH                     7
#define F_CAM_FMON_TRIG_STOP_WIN_DLY_CNT_0_POS                       0
#define F_CAM_FMON_TRIG_STOP_WIN_DLY_CNT_0_WIDTH                     7

#define REG_CAM_FMON_WIN_DELAY_THR_0                 0x4C
#define F_CAM_FMON_TRIG_STOP_FIFO_THR_1_POS                          13
#define F_CAM_FMON_TRIG_STOP_FIFO_THR_1_WIDTH                        13
#define F_CAM_FMON_TRIG_STOP_FIFO_THR_0_POS                          0
#define F_CAM_FMON_TRIG_STOP_FIFO_THR_0_WIDTH                        13

#define REG_CAM_FMON_WIN_DELAY_THR_1                 0x50
#define F_CAM_FMON_TRIG_STOP_FIFO_THR_3_POS                          13
#define F_CAM_FMON_TRIG_STOP_FIFO_THR_3_WIDTH                        13
#define F_CAM_FMON_TRIG_STOP_FIFO_THR_2_POS                          0
#define F_CAM_FMON_TRIG_STOP_FIFO_THR_2_WIDTH                        13

#define REG_CAM_FMON_DBG_STATUS                      0x54
#define F_CAM_FMON_CTI_O_STOP_STATUS_POS                             24
#define F_CAM_FMON_CTI_O_STOP_STATUS_WIDTH                           2
#define F_CAM_FMON_CTI_IN_STOP_STATUS_POS                            22
#define F_CAM_FMON_CTI_IN_STOP_STATUS_WIDTH                          2
#define F_CAM_FMON_O_ELA_TRIG_STATUS_POS                             18
#define F_CAM_FMON_O_ELA_TRIG_STATUS_WIDTH                           4
#define F_CAM_FMON_CTI_O_START_STATUS_POS                            16
#define F_CAM_FMON_CTI_O_START_STATUS_WIDTH                          2
#define F_CAM_FMON_CTI_IN_START_STATUS_POS                           14
#define F_CAM_FMON_CTI_IN_START_STATUS_WIDTH                         2
#define F_CAM_FMON_CTI_CHANL_O_ACK_STATUS_POS                        11
#define F_CAM_FMON_CTI_CHANL_O_ACK_STATUS_WIDTH                      3
#define F_CAM_FMON_CTI_CHANL_IN_STATUS_POS                           8
#define F_CAM_FMON_CTI_CHANL_IN_STATUS_WIDTH                         3
#define F_CAM_FMON_AXI_TRIG_STATUS_3_POS                             6
#define F_CAM_FMON_AXI_TRIG_STATUS_3_WIDTH                           2
#define F_CAM_FMON_AXI_TRIG_STATUS_2_POS                             4
#define F_CAM_FMON_AXI_TRIG_STATUS_2_WIDTH                           2
#define F_CAM_FMON_AXI_TRIG_STATUS_1_POS                             2
#define F_CAM_FMON_AXI_TRIG_STATUS_1_WIDTH                           2
#define F_CAM_FMON_AXI_TRIG_STATUS_0_POS                             0
#define F_CAM_FMON_AXI_TRIG_STATUS_0_WIDTH                           2

#define REG_CAM_FMON_EXT_GALS_STATUS_1               0x58
#define F_CAM_FMON_C_GALS_STATUS_2_POS                               0
#define F_CAM_FMON_C_GALS_STATUS_2_WIDTH                             3


#endif	/* _MTK_CAM_FMON_REGS_H */
