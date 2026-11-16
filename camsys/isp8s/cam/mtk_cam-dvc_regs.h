/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

/* baseaddr 0x3A0A8000 */

/* module: DVC_CAM_TOP_E1A */
#define REG_DVC_CAM_TOP_SW_RST                        0x0
#define F_DVC_CAM_TOP_SW_RST_POS                                     0
#define F_DVC_CAM_TOP_SW_RST_WIDTH                                   11

#define REG_DVC_CAM_TOP_CTL                           0x4
#define F_DVC_CAM_TOP_RAW_C_DCM_DIS_POS                              27
#define F_DVC_CAM_TOP_RAW_C_DCM_DIS_WIDTH                            1
#define F_DVC_CAM_TOP_RAW_B_DCM_DIS_POS                              26
#define F_DVC_CAM_TOP_RAW_B_DCM_DIS_WIDTH                            1
#define F_DVC_CAM_TOP_RAW_A_DCM_DIS_POS                              25
#define F_DVC_CAM_TOP_RAW_A_DCM_DIS_WIDTH                            1
#define F_DVC_CAM_TOP_TOP_DCM_DIS_POS                                24
#define F_DVC_CAM_TOP_TOP_DCM_DIS_WIDTH                              1
#define F_DVC_CAM_TOP_RAW_C_HW_OPP_EN_POS                            18
#define F_DVC_CAM_TOP_RAW_C_HW_OPP_EN_WIDTH                          1
#define F_DVC_CAM_TOP_RAW_C_SW_OPP_EN_POS                            17
#define F_DVC_CAM_TOP_RAW_C_SW_OPP_EN_WIDTH                          1
#define F_DVC_CAM_TOP_RAW_B_HW_OPP_EN_POS                            16
#define F_DVC_CAM_TOP_RAW_B_HW_OPP_EN_WIDTH                          1
#define F_DVC_CAM_TOP_RAW_B_SW_OPP_EN_POS                            15
#define F_DVC_CAM_TOP_RAW_B_SW_OPP_EN_WIDTH                          1
#define F_DVC_CAM_TOP_RAW_A_HW_OPP_EN_POS                            14
#define F_DVC_CAM_TOP_RAW_A_HW_OPP_EN_WIDTH                          1
#define F_DVC_CAM_TOP_RAW_A_SW_OPP_EN_POS                            13
#define F_DVC_CAM_TOP_RAW_A_SW_OPP_EN_WIDTH                          1
#define F_DVC_CAM_TOP_CCU_OPP_EN_POS                                 12
#define F_DVC_CAM_TOP_CCU_OPP_EN_WIDTH                               1
#define F_DVC_CAM_TOP_CCU_OPP_VAL_POS                                8
#define F_DVC_CAM_TOP_CCU_OPP_VAL_WIDTH                              4
#define F_DVC_CAM_TOP_INIT_OPP_VAL_POS                               4
#define F_DVC_CAM_TOP_INIT_OPP_VAL_WIDTH                             4
#define F_DVC_CAM_TOP_DVC_EN_POS                                     0
#define F_DVC_CAM_TOP_DVC_EN_WIDTH                                   1

#define REG_DVC_CAM_TOP_DVFSRC_ACK_TMOUT              0x8
#define REG_DVC_CAM_TOP_OPP_STATUS                    0xC
#define F_DVC_CAM_TOP_INIT_ACK_POS                                   31
#define F_DVC_CAM_TOP_INIT_ACK_WIDTH                                 1
#define F_DVC_CAM_TOP_MAX_OPP_W_POS                                  16
#define F_DVC_CAM_TOP_MAX_OPP_W_WIDTH                                4
#define F_DVC_CAM_TOP_MAX_OPP_LO_POS                                 12
#define F_DVC_CAM_TOP_MAX_OPP_LO_WIDTH                               4
#define F_DVC_CAM_TOP_MAX_OPP_HI_POS                                 8
#define F_DVC_CAM_TOP_MAX_OPP_HI_WIDTH                               4
#define F_DVC_CAM_TOP_MAX_OPP_POS                                    4
#define F_DVC_CAM_TOP_MAX_OPP_WIDTH                                  4
#define F_DVC_CAM_TOP_CURRENT_OPP_POS                                0
#define F_DVC_CAM_TOP_CURRENT_OPP_WIDTH                              4

#define REG_DVC_CAM_TOP_INT_EN                       0x10
#define F_DVC_CAM_TOP_INT_WCLR_EN_POS                                31
#define F_DVC_CAM_TOP_INT_WCLR_EN_WIDTH                              1
#define F_DVC_CAM_TOP_REC_2REQ_INT_EN_3_POS                          9
#define F_DVC_CAM_TOP_REC_2REQ_INT_EN_3_WIDTH                        1
#define F_DVC_CAM_TOP_REC_2REQ_INT_EN_2_POS                          8
#define F_DVC_CAM_TOP_REC_2REQ_INT_EN_2_WIDTH                        1
#define F_DVC_CAM_TOP_REC_2REQ_INT_EN_1_POS                          7
#define F_DVC_CAM_TOP_REC_2REQ_INT_EN_1_WIDTH                        1
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_EN_3_POS                  6
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_EN_3_WIDTH                1
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_EN_2_POS                  5
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_EN_2_WIDTH                1
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_EN_1_POS                  4
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_EN_1_WIDTH                1
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_EN_3_POS                 3
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_EN_3_WIDTH               1
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_EN_2_POS                 2
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_EN_2_WIDTH               1
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_EN_1_POS                 1
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_EN_1_WIDTH               1
#define F_DVC_CAM_TOP_DVFSRC_ACK_TMOUT_INT_EN_POS                    0
#define F_DVC_CAM_TOP_DVFSRC_ACK_TMOUT_INT_EN_WIDTH                  1

#define REG_DVC_CAM_TOP_INT_STATUS                   0x14
#define F_DVC_CAM_TOP_REC_2REQ_INT_ST_3_POS                          9
#define F_DVC_CAM_TOP_REC_2REQ_INT_ST_3_WIDTH                        1
#define F_DVC_CAM_TOP_REC_2REQ_INT_ST_2_POS                          8
#define F_DVC_CAM_TOP_REC_2REQ_INT_ST_2_WIDTH                        1
#define F_DVC_CAM_TOP_REC_2REQ_INT_ST_1_POS                          7
#define F_DVC_CAM_TOP_REC_2REQ_INT_ST_1_WIDTH                        1
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_ST_3_POS                  6
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_ST_3_WIDTH                1
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_ST_2_POS                  5
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_ST_2_WIDTH                1
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_ST_1_POS                  4
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_ST_1_WIDTH                1
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_ST_3_POS                 3
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_ST_3_WIDTH               1
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_ST_2_POS                 2
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_ST_2_WIDTH               1
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_ST_1_POS                 1
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_ST_1_WIDTH               1
#define F_DVC_CAM_TOP_DVFSRC_ACK_TMOUT_INT_ST_POS                    0
#define F_DVC_CAM_TOP_DVFSRC_ACK_TMOUT_INT_ST_WIDTH                  1

#define REG_DVC_CAM_TOP_INT_TRIG                     0x18
#define F_DVC_CAM_TOP_SELF_INT_TRIG_EN_POS                           31
#define F_DVC_CAM_TOP_SELF_INT_TRIG_EN_WIDTH                         1
#define F_DVC_CAM_TOP_REC_2REQ_INT_TRIG_3_POS                        9
#define F_DVC_CAM_TOP_REC_2REQ_INT_TRIG_3_WIDTH                      1
#define F_DVC_CAM_TOP_REC_2REQ_INT_TRIG_2_POS                        8
#define F_DVC_CAM_TOP_REC_2REQ_INT_TRIG_2_WIDTH                      1
#define F_DVC_CAM_TOP_REC_2REQ_INT_TRIG_1_POS                        7
#define F_DVC_CAM_TOP_REC_2REQ_INT_TRIG_1_WIDTH                      1
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_TRIG_3_POS                6
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_TRIG_3_WIDTH              1
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_TRIG_2_POS                5
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_TRIG_2_WIDTH              1
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_TRIG_1_POS                4
#define F_DVC_CAM_TOP_P1_DONE_HIT_PROT_INT_TRIG_1_WIDTH              1
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_TRIG_3_POS               3
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_TRIG_3_WIDTH             1
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_TRIG_2_POS               2
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_TRIG_2_WIDTH             1
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_TRIG_1_POS               1
#define F_DVC_CAM_TOP_REQ_HI_OVER_VSYNC_INT_TRIG_1_WIDTH             1
#define F_DVC_CAM_TOP_DVFSRC_ACK_TMOUT_INT_TRIG_POS                  0
#define F_DVC_CAM_TOP_DVFSRC_ACK_TMOUT_INT_TRIG_WIDTH                1

#define REG_DVC_CAM_TOP_DUMMY_REG                    0x20
#define REG_DVC_CAM_TOP_DBG_DEBUG_SEL                0x24
#define F_DVC_CAM_TOP_DBG_DEBUG_SEL_POS                              0
#define F_DVC_CAM_TOP_DBG_DEBUG_SEL_WIDTH                            4

#define REG_DVC_CAM_TOP_DBG_DEBUG_DATA               0x28
#define REG_DVC_CAM_TOP_DBG_ACK_TMOUT_CNT            0x2C
#define REG_DVC_CAM_TOP_DVFSRC_PROT_WAIT_TIME        0x30
#define F_DVC_CAM_TOP_DVFSRC_PROT_WAIT_TIME_POS                      0
#define F_DVC_CAM_TOP_DVFSRC_PROT_WAIT_TIME_WIDTH                    16

/* module: DVC_CAM_E1A */
#define REG_DVC_CAM_HW_DVC_CTL                      0x4
#define F_DVC_CAM_HW_DVC_TRI_POS                                   16
#define F_DVC_CAM_HW_DVC_TRI_WIDTH                                 1
#define F_DVC_CAM_HW_DVC_HRT_SRC_POS                               12
#define F_DVC_CAM_HW_DVC_HRT_SRC_WIDTH                             2
#define F_DVC_CAM_HW_DVC_OPP_LOW_POS                               8
#define F_DVC_CAM_HW_DVC_OPP_LOW_WIDTH                             4
#define F_DVC_CAM_HW_DVC_OPP_HIGH_POS                              4
#define F_DVC_CAM_HW_DVC_OPP_HIGH_WIDTH                            4
#define F_DVC_CAM_HW_DVC_MD_POS                                    0
#define F_DVC_CAM_HW_DVC_MD_WIDTH                                  1

#define REG_DVC_CAM_HW_DVC_TIME_STAMP               0x8
#define REG_DVC_CAM_HW_DVC_OPP_HIGH_DLY             0xC
#define REG_DVC_CAM_HW_DVC_MAX_OPP_HIGH_TM         0x10
#define REG_DVC_CAM_SW_VOTER                       0x14
#define F_DVC_CAM_SW_REQ_POS                                       4
#define F_DVC_CAM_SW_REQ_WIDTH                                     1
#define F_DVC_CAM_SW_OPP_VAL_POS                                   0
#define F_DVC_CAM_SW_OPP_VAL_WIDTH                                 4

#define REG_DVC_CAM_SW_VOTER_STATUS                0x18
#define F_DVC_CAM_SW_ACK_POS                                       0
#define F_DVC_CAM_SW_ACK_WIDTH                                     1

#define REG_DVC_CAM_DUMMY_REG                      0x40
#define REG_DVC_CAM_DBG_DEBUG_SEL                  0x44
#define F_DVC_CAM_DBG_DEBUG_SEL_POS                                0
#define F_DVC_CAM_DBG_DEBUG_SEL_WIDTH                              4

#define REG_DVC_CAM_DBG_DEBUG_DATA                 0x48
