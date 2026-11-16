/* SPDX-License-Identifier: GPL-2.0 */
// Copyright (c) 2019 MediaTek Inc.

#ifndef __MTK_AOV_REGS_H__
#define __MTK_AOV_REGS_H__

#include <linux/regmap.h>

#define DRV_Reg32(addr) readl(addr)
#define DRV_WriteReg32(addr, val) writel(val, addr)
#define DRV_SetReg32(addr, val) DRV_WriteReg32(addr, DRV_Reg32(addr) | (val))
#define DRV_ClrReg32(addr, val) DRV_WriteReg32(addr, DRV_Reg32(addr) & ~(val))

#define DRV_WriteReg32_Mask(addr, val, msk, shift) \
		DRV_WriteReg32((addr), ((DRV_Reg32(addr) & (~(msk))) | ((val << shift) & (msk))))

#define AOV_BITS(base, reg, field, val) do { \
	u32 __iomem *__p = base + reg; \
	u32 __v = readl(__p); \
	__v &= ~field##_MASK; \
	__v |= (((val) << field##_SHIFT) & field##_MASK); \
	writel(__v, __p); \
} while (0)

#define AOV_READ_BITS(base, reg, field) \
({ \
	u32 __iomem *__p = base + reg; \
	u32 __v = readl(__p); \
	__v &= field##_MASK; \
	__v >>= field##_SHIFT; \
	__v; \
})

#define AOV_READ_REG(base, reg) \
({ \
	u32 __iomem *__p = base + reg; \
	u32 __v = readl(__p); \
	__v; \
})

#define AOV_WRITE_REG(base, reg, val) do { \
	u32 __iomem *__p = base + reg; \
	writel(val, __p); \
} while (0)

#endif //__MTK_AOV_REGS_H__
