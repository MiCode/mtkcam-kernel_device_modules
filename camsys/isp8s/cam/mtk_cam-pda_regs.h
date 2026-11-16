/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _MTK_CAM_PDA_REGS_H
#define _MTK_CAM_PDA_REGS_H

/* baseaddr 0X3A660400 */

/* following added manually */
#define REG_PDA_PDA_DEBUG_SEL                 (0x288)
#define REG_PDA_PDA_DEBUG_DATA                (0x2a8)

#define REG_E_PDA_OTF_CFG_0                   (0x400)
#define REG_E_PDA_OTF_CFG_1                   (0x404)
#define REG_E_PDA_OTF_CFG_2                   (0x408)
#define REG_E_PDA_OTF_CFG_3                   (0x40c)
#define REG_E_PDA_OTF_CFG_4                   (0x410)
#define REG_E_PDA_OTF_CFG_5                   (0x414)
#define REG_E_PDA_OTF_CFG_6                   (0x418)

#define REG_E_PDA_OTF_CFG_14                  (0x438)
#define REG_E_PDA_OTF_CFG_15                  (0x43c)
#define REG_E_PDA_OTF_CFG_16                  (0x440)
#define REG_E_PDA_OTF_CFG_17                  (0x444)
#define REG_E_PDA_OTF_CFG_18                  (0x448)
#define REG_E_PDA_OTF_CFG_19                  (0x44c)
#define REG_E_PDA_OTF_CFG_20                  (0x450)
#define REG_E_PDA_OTF_CFG_21                  (0x454)
#define REG_E_PDA_OTF_CFG_22                  (0x458)

#define REG_E_PDA_OTF_CFG_23                  (0x45c)
#define REG_E_PDA_OTF_CFG_24                  (0x460)
#define REG_E_PDA_OTF_CFG_25                  (0x464)
#define REG_E_PDA_OTF_CFG_26                  (0x468)
#define REG_E_PDA_OTF_CFG_27                  (0x46c)
#define REG_E_PDA_OTF_CFG_28                  (0x470)
#define REG_E_PDA_OTF_CFG_29                  (0x474)
#define REG_E_PDA_OTF_CFG_30                  (0x478)
#define REG_E_PDA_OTF_CFG_31                  (0x47c)
#define REG_E_PDA_OTF_CFG_32                  (0x480)
#define REG_E_PDA_OTF_CFG_33                  (0x484)
#define REG_E_PDA_OTF_CFG_34                  (0x488)
#define REG_E_PDA_OTF_CFG_35                  (0x48c)
#define REG_E_PDA_OTF_CFG_36                  (0x490)
#define REG_E_PDA_OTF_CFG_37                  (0x494)
#define REG_E_PDA_OTF_CFG_38                  (0x498)
#define REG_E_PDA_OTF_CFG_39                  (0x49c)

#define REG_E_PDA_OTF_CFG_40                  (0x4a0)
#define REG_E_PDA_OTF_CFG_41                  (0x4a4)
#define REG_E_PDA_OTF_CFG_42                  (0x4a8)
#define REG_E_PDA_OTF_CFG_43                  (0x4ac)

#define REG_E_PDA_OTF_CFG_44                  (0x4b0)
#define REG_E_PDA_OTF_CFG_45                  (0x4b4)
#define REG_E_PDA_OTF_CFG_46                  (0x4b8)
#define REG_E_PDA_OTF_CFG_47                  (0x4bc)

#define REG_E_PDA_OTF_CFG_48                  (0x4c0)
#define REG_E_PDA_OTF_CFG_49                  (0x4c4)
#define REG_E_PDA_OTF_CFG_50                  (0x4c8)
#define REG_E_PDA_OTF_CFG_51                  (0x4cc)

#define REG_E_PDA_OTF_CFG_52                  (0x4d0)
#define REG_E_PDA_OTF_CFG_53                  (0x4d4)
#define REG_E_PDA_OTF_CFG_54                  (0x4d8)
#define REG_E_PDA_OTF_CFG_55                  (0x4dc)

#define REG_E_PDA_OTF_CFG_56                  (0x4e0)
#define REG_E_PDA_OTF_CFG_57                  (0x4e4)
#define REG_E_PDA_OTF_CFG_58                  (0x4e8)
#define REG_E_PDA_OTF_CFG_59                  (0x4ec)

#define REG_E_PDA_OTF_CFG_60                  (0x4f0)
#define REG_E_PDA_OTF_CFG_61                  (0x4f4)
#define REG_E_PDA_OTF_CFG_62                  (0x4f8)
#define REG_E_PDA_OTF_CFG_63                  (0x4fc)

#define REG_E_PDA_OTF_CFG_64                  (0x500)
#define REG_E_PDA_OTF_CFG_65                  (0x504)
#define REG_E_PDA_OTF_CFG_66                  (0x508)
#define REG_E_PDA_OTF_CFG_67                  (0x50c)

#define REG_E_PDA_OTF_CFG_68                  (0x510)
#define REG_E_PDA_OTF_CFG_69                  (0x514)
#define REG_E_PDA_OTF_CFG_70                  (0x518)
#define REG_E_PDA_OTF_CFG_71                  (0x51c)

#define REG_E_PDA_OTF_CFG_72                  (0x520)
#define REG_E_PDA_OTF_CFG_73                  (0x524)
#define REG_E_PDA_OTF_CFG_74                  (0x528)
#define REG_E_PDA_OTF_CFG_75                  (0x52c)

#define REG_E_PDA_OTF_CFG_76                  (0x530)
#define REG_E_PDA_OTF_CFG_77                  (0x534)
#define REG_E_PDA_OTF_CFG_78                  (0x538)
#define REG_E_PDA_OTF_CFG_79                  (0x53c)

#define REG_E_PDA_OTF_CFG_80                  (0x540)
#define REG_E_PDA_OTF_CFG_81                  (0x544)
#define REG_E_PDA_OTF_CFG_82                  (0x548)
#define REG_E_PDA_OTF_CFG_83                  (0x54c)

#define REG_E_PDA_OTF_CFG_84                  (0x550)
#define REG_E_PDA_OTF_CFG_85                  (0x554)
#define REG_E_PDA_OTF_CFG_86                  (0x558)
#define REG_E_PDA_OTF_CFG_87                  (0x55c)

#define REG_E_PDA_OTF_CFG_88                  (0x560)
#define REG_E_PDA_OTF_CFG_89                  (0x564)
#define REG_E_PDA_OTF_CFG_90                  (0x568)
#define REG_E_PDA_OTF_CFG_91                  (0x56c)

#define REG_E_PDA_OTF_CFG_92                  (0x570)
#define REG_E_PDA_OTF_CFG_93                  (0x574)
#define REG_E_PDA_OTF_CFG_94                  (0x578)
#define REG_E_PDA_OTF_CFG_95                  (0x57c)

#define REG_E_PDA_OTF_CFG_96                  (0x580)
#define REG_E_PDA_OTF_CFG_97                  (0x584)
#define REG_E_PDA_OTF_CFG_98                  (0x588)
#define REG_E_PDA_OTF_CFG_99                  (0x58c)

#define REG_E_PDA_OTF_CFG_100                 (0x590)
#define REG_E_PDA_OTF_CFG_101                 (0x594)
#define REG_E_PDA_OTF_CFG_102                 (0x598)
#define REG_E_PDA_OTF_CFG_103                 (0x59c)

#define REG_E_PDA_OTF_CFG_104                 (0x5a0)
#define REG_E_PDA_OTF_CFG_105                 (0x5a4)
#define REG_E_PDA_OTF_CFG_106                 (0x5a8)
#define REG_E_PDA_OTF_CFG_107                 (0x5ac)

#define REG_E_PDA_OTF_CFG_108                 (0x5b0)
#define REG_E_PDA_OTF_CFG_109                 (0x5b4)
#define REG_E_PDA_OTF_CFG_110                 (0x5b8)
#define REG_E_PDA_OTF_CFG_111                 (0x5bc)

#define REG_E_PDA_OTF_CFG_112                 (0x5c0)
#define REG_E_PDA_OTF_CFG_113                 (0x5c4)
#define REG_E_PDA_OTF_CFG_114                 (0x5c8)

#define REG_E_PDA_OTF_PDAI_P1_BASE_ADDR       (0x5cc)
#define REG_E_PDA_OTF_PDATI_P1_BASE_ADDR      (0x5d0)
#define REG_E_PDA_OTF_PDAI_P2_BASE_ADDR       (0x5d4)
#define REG_E_PDA_OTF_PDATI_P2_BASE_ADDR      (0x5d8)

#define REG_E_PDA_OTF_PDAI_STRIDE             (0x5dc)
#define REG_E_PDA_OTF_PDAI_P1_CON0            (0x5E0)
#define REG_E_PDA_OTF_PDAI_P1_CON1            (0x5E4)
#define REG_E_PDA_OTF_PDAI_P1_CON2            (0x5E8)
#define REG_E_PDA_OTF_PDAI_P1_CON3            (0x5Ec)
#define REG_E_PDA_OTF_PDAI_P1_CON4            (0x5F0)
#define REG_E_PDA_OTF_PDATI_P1_CON0           (0x5F4)
#define REG_E_PDA_OTF_PDATI_P1_CON1           (0x5F8)
#define REG_E_PDA_OTF_PDATI_P1_CON2           (0x5Fc)
#define REG_E_PDA_OTF_PDATI_P1_CON3           (0x600)
#define REG_E_PDA_OTF_PDATI_P1_CON4           (0x604)
#define REG_E_PDA_OTF_PDAI_P2_CON0            (0x608)
#define REG_E_PDA_OTF_PDAI_P2_CON1            (0x60c)
#define REG_E_PDA_OTF_PDAI_P2_CON2            (0x610)
#define REG_E_PDA_OTF_PDAI_P2_CON3            (0x614)
#define REG_E_PDA_OTF_PDAI_P2_CON4            (0x618)
#define REG_E_PDA_OTF_PDATI_P2_CON0           (0x61c)
#define REG_E_PDA_OTF_PDATI_P2_CON1           (0x620)
#define REG_E_PDA_OTF_PDATI_P2_CON2           (0x624)
#define REG_E_PDA_OTF_PDATI_P2_CON3           (0x628)
#define REG_E_PDA_OTF_PDATI_P2_CON4           (0x62c)

#define REG_E_PDA_OTF_PDAO_P1_BASE_ADDR      (0x630)

#define REG_E_PDA_OTF_PDAO_P1_XSIZE           (0x634)
#define REG_E_PDA_OTF_PDAO_P1_CON0            (0x638)
#define REG_E_PDA_OTF_PDAO_P1_CON1            (0x63c)
#define REG_E_PDA_OTF_PDAO_P1_CON2            (0x640)
#define REG_E_PDA_OTF_PDAO_P1_CON3            (0x644)
#define REG_E_PDA_OTF_PDAO_P1_CON4            (0x648)
#define REG_E_PDA_OTF_PDA_DMA_EN              (0x64c)
#define REG_E_PDA_OTF_PDA_DMA_RST             (0x650)
#define REG_E_PDA_OTF_PDA_DMA_TOP             (0x654)
#define REG_E_PDA_OTF_PDA_SECURE              (0x658)
#define REG_E_PDA_OTF_PDA_DCM_DIS             (0x65c)
#define REG_E_PDA_OTF_PDAI_P1_ERR_STAT        (0x660)
#define REG_E_PDA_OTF_PDATI_P1_ERR_STAT       (0x664)
#define REG_E_PDA_OTF_PDAI_P2_ERR_STAT        (0x668)
#define REG_E_PDA_OTF_PDATI_P2_ERR_STAT       (0x66c)
#define REG_E_PDA_OTF_PDAO_P1_ERR_STAT        (0x670)
#define REG_E_PDA_OTF_PDA_ERR_STAT_EN         (0x674)

/* ERROR Status */
#define REG_E_PDA_OTF_PDA_ERR_STAT            (0x678)
#define PDA_OTF_PDA_DONE_ST                     BIT(0)
#define PDA_OTF_PDA_HANG_ST                     BIT(1)
#define PDA_OTF_PDA_ERR_STAT_RESERVED2          BIT(2)
#define PDA_OTF_PDA_ERR_STAT_RESERVED3          BIT(3)

#define REG_E_PDA_OTF_PDA_TOP_CTL             (0x67c)
#define REG_E_PDA_OTF_PDA_IRQ_TRIG            (0x680)

#define REG_E_PDA_OTF_PDAI_P1_BASE_ADDR_MSB   (0x684)
#define REG_E_PDA_OTF_PDATI_P1_BASE_ADDR_MSB  (0x688)
#define REG_E_PDA_OTF_PDAI_P2_BASE_ADDR_MSB   (0x68c)
#define REG_E_PDA_OTF_PDATI_P2_BASE_ADDR_MSB  (0x690)
#define REG_E_PDA_OTF_PDAO_P1_BASE_ADDR_MSB   (0x694)

#define REG_E_PDA_OTF_PDA_P1_AXSLC            (0x6a0)
#define REG_E_PDA_OTF_PDA_P2_AXSLC            (0x6a4)
#define REG_E_PDA_OTF_PDAO_P1_AXSLC           (0x6a8)

#define REG_E_PDA_OTF_PACK_MODE               (0x6ac)
#define REG_E_PDA_OTF_DILATION_CFG            (0x6b0)

#define REG_E_PDA_OTF_PDA_SECURE_1            (0x6b4)
#define REG_E_PDA_OTF_DCIF_CTL                (0x6b8)

#define REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA0    (0x6c8)
#define REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA1    (0x6cc)
#define REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA2    (0x6d0)
#define REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA3    (0x6d4)
#define REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA4    (0x6d8)
#define REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA5    (0x6dc)
#define REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA6    (0x6e0)
#define REG_E_PDA_OTF_PDA_DCIF_DEBUG_DATA7    (0x6e4)

#endif	/* _MTK_CAM_PDA_REGS_H */
