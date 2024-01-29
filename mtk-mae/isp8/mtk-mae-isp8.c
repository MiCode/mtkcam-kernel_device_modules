// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Ming-Hsuan.Chiang <Ming-Hsuan.Chiang@mediatek.com>
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>

#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include "cmdq-sec.h"
#include "cmdq-sec-iwc-common.h"

#include "mtk-mae-isp8.h"

// MAE_TO_DO
// #ifdef AIE_TF_DUMP_7SP_1
// #include <dt-bindings/memory/mt6989-larb-port.h>
// #endif
// #include "iommu_debug.h"

#define CHECK_BASE_ADDR(ADDR) (ADDR % MAE_BASE_ADDR_ALIGN != 0)

#define MAE_DUMP_REG(REG)								\
		mae_dev_info(mae_dev->dev, "%s [0x%08X %08X]\n",			\
					#REG, (uint32_t)REG,				\
					(uint32_t)readl(mae_dev->mae_base + REG))

#define MAE_CMDQ_WRITE_REG(PKT, MAE_REG_OFFSET, VALUE)			\
	do {								\
		cmdq_pkt_write(PKT, NULL, MAE_BASE + MAE_REG_OFFSET,	\
						VALUE, CMDQ_REG_MASK);	\
	} while(0)

#define DIV_CEIL_POS(X,Y)	((((X)) - (int)((X) / (Y)) * (Y)) > 0 ? (int)(((X) / (Y)) + 1) : (int)((X) / (Y)))
// #define CEIL_POS(X)	((X-(int)(X)) > 0 ? (int)(X+1) : (int)(X))
// #define CEIL_NEG(X)	(int)(X)
// #define CEIL(X)		( ((X) > 0) ? CEIL_POS(X) : CEIL_NEG(X) )

// #define FLOOR_POS(X)		(int)(X)
// #define FLOOR_NEG(X)		((X-(int)(X)) < 0 ? (int)(X-1) : (int)(X))
// #define FLOOR(X)		( ((X) > 0) ? FLOOR_POS(X) : FLOOR_NEG(X) )

#define MIN(X,Y)		(((X) > (Y)) ? (Y) : (X))
#define MAX(X,Y)		(((X) > (Y)) ? (X) : (Y))
#define ABS(X)			(((X) > 0) ? (X) : -(X))
#define ROUND(X,Y)		((((X)/(Y))>=0)?(int)((2*(X))+(Y))/(2*(Y)):(int)((2*(X))-(Y))/(2*(Y)))

/*
 * MAE Debug level:
 * MAE_INFO = 0
 * MAE_DEBUG = 1
 */
int mae_log_level_value;
int mae_fd_post_on = 1;
int mae_trigger_cmdq_timeout;
int fld_debug_1;
int rsz_debug_on;
int mae_dbf_on;
int set_default_value = 1;
int fld_reset_en = 1;
int aiseg_lut_en = 1;
int fac_v1_pat_en;
int cmdq_polling_en = 1;

module_param(mae_log_level_value, int, 0644);
module_param(mae_fd_post_on, int, 0644);
module_param(mae_trigger_cmdq_timeout, int, 0644);
module_param(fld_debug_1, int, 0644);
module_param(rsz_debug_on, int, 0644);
module_param(mae_dbf_on, int, 0644);
module_param(set_default_value, int, 0644);
module_param(fld_reset_en, int, 0644);
module_param(aiseg_lut_en, int, 0644);
module_param(fac_v1_pat_en, int, 0644);
module_param(cmdq_polling_en, int, 0644);

static void mtk_mae_dump_reg(struct mtk_mae_dev *mae_dev);
static void mtk_mae_fld_reset(struct mtk_mae_dev *mae_dev);
static void mtk_mae_aiseg_pat(struct cmdq_pkt *pkt);
static void mtk_mae_fac_v1_pat(struct cmdq_pkt *pkt);
static void mtk_mae_fd_ipn_640_480_pat(struct cmdq_pkt *pkt);
static void mtk_mae_fd_ipn_240_180_pat(struct cmdq_pkt *pkt);
static void mtk_mae_fd_ipn_120_90_pat(struct cmdq_pkt *pkt);
static void mtk_mae_fd_fpn_480_360_pat(struct cmdq_pkt *pkt);
static void mtk_mae_attr_v0_pat(struct cmdq_pkt *pkt);

static void mtk_mae_dump_reg(struct mtk_mae_dev *mae_dev);
static void mtk_mae_fld_reset(struct mtk_mae_dev *mae_dev);

static void mtk_mae_reg_dump_to_buffer(struct mtk_mae_dev *mae_dev);

static void MAECmdqCB(struct cmdq_cb_data data)
{
	struct mtk_mae_dev *mae_dev = (struct mtk_mae_dev *)data.data;
	bool is_hw_hang = (data.err == 0) ? false : true;

	mae_dev->is_hw_hang = is_hw_hang;

	if (is_hw_hang) {
		mae_dev_info(mae_dev->dev, "MAE HW Hang");
		mtk_mae_dump_reg(mae_dev);
	}

	queue_work(mae_dev->frame_done_wq, &mae_dev->req_work.work);
}

static int mtk_mae_fd_core_sel(struct mtk_mae_dev *mae_dev, int w, int h)
{
	if (w > fd_pattern_width[0] || h > fd_pattern_height[0]) {
		mae_dev_info(mae_dev->dev, "[%s] over pyramid size limit, too large w(%d/%d), h(%d/%d)",
					__func__, w, fd_pattern_width[0],
					h, fd_pattern_height[0]);
		return -1;
	} else if (w > fd_pattern_width[1] || h > fd_pattern_height[1]) {
		return 0;
	} else if (w > fd_pattern_width[2] || h > fd_pattern_height[2]) {
		return 1;
	} else if (w > fd_pattern_width[3] || h > fd_pattern_height[3]) {
		return 2;
	} else if (w >= FD_PYRAMID_MIN_WIDTH || h >= FD_PYRAMID_MIN_HEIGHT) {
		return 3;
	} else {
		mae_dev_info(mae_dev->dev, "[%s] over pyramid size limit, too small w(%d/%d), h(%d/%d)",
					__func__, w, FD_PYRAMID_MIN_WIDTH, h, FD_PYRAMID_MIN_HEIGHT);
		return -1;
	}
}

static void mtk_mae_set_default_value(struct mtk_mae_dev *mae_dev, struct cmdq_pkt *pkt)
{
	uint32_t i;

	for (i = 0; i < RSZ_NUM; i++) {
		MAE_CMDQ_WRITE_REG(pkt, REG_0004_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, REG_0008_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0008);
		MAE_CMDQ_WRITE_REG(pkt, REG_000C_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, REG_0010_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0008);
		MAE_CMDQ_WRITE_REG(pkt, REG_001C_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, REG_0020_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0101);
		MAE_CMDQ_WRITE_REG(pkt, REG_0024_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x8000);
		MAE_CMDQ_WRITE_REG(pkt, REG_0028_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0100);

		MAE_CMDQ_WRITE_REG(pkt, REG_002C_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0101);
		MAE_CMDQ_WRITE_REG(pkt, REG_0034_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0003);
		MAE_CMDQ_WRITE_REG(pkt, REG_005C_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, REG_0060_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, REG_0064_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, REG_0068_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, REG_0080_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0780);
		MAE_CMDQ_WRITE_REG(pkt, REG_0084_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0f00);
		MAE_CMDQ_WRITE_REG(pkt, REG_00A0_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x8780);
		MAE_CMDQ_WRITE_REG(pkt, REG_00A4_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x8438);
		MAE_CMDQ_WRITE_REG(pkt, REG_00A8_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0F00);
		MAE_CMDQ_WRITE_REG(pkt, REG_00AC_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0870);
		MAE_CMDQ_WRITE_REG(pkt, REG_00C0_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0090);

		MAE_CMDQ_WRITE_REG(pkt, REG_00C4_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, REG_00C8_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, REG_00CC_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x02C0);
		MAE_CMDQ_WRITE_REG(pkt, REG_00D0_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x02C0);
		MAE_CMDQ_WRITE_REG(pkt, REG_00D4_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, REG_00D8_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x01E0);

		MAE_CMDQ_WRITE_REG(pkt, REG_0104_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0022);
		MAE_CMDQ_WRITE_REG(pkt, REG_0108_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, REG_010C_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, REG_0110_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x02C0);
		MAE_CMDQ_WRITE_REG(pkt, REG_0114_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x02C6);
		MAE_CMDQ_WRITE_REG(pkt, REG_0118_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, REG_011C_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x01E0);
		MAE_CMDQ_WRITE_REG(pkt, REG_0120_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x01E0);
		MAE_CMDQ_WRITE_REG(pkt, REG_0180_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0005);
		MAE_CMDQ_WRITE_REG(pkt, REG_0184_RSZ1 + RSZ_BASE_ADDR_OFFSET * i, 0x0002);

		MAE_CMDQ_WRITE_REG(pkt, MAE_REG_X_OFFSET_0 + COMMON_REG_SIZE * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, MAE_REG_Y_OFFSET_0 + COMMON_REG_SIZE * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, MAE_REG_MMFD_O_SCALE_0 + COMMON_REG_SIZE * i, 0x0AA8);
		MAE_CMDQ_WRITE_REG(pkt, MAE_REG_H_SIZE0 + COMMON_REG_SIZE * i, 0x0280);
		MAE_CMDQ_WRITE_REG(pkt, MAE_REG_V_SIZE0 + COMMON_REG_SIZE * i, 0x01E0);
		MAE_CMDQ_WRITE_REG(pkt, MAE_REG_H_MIN0 + COMMON_REG_SIZE * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, MAE_REG_V_MIN0 + COMMON_REG_SIZE * i, 0x0000);
		MAE_CMDQ_WRITE_REG(pkt, MAE_REG_SCORE_TH0 + COMMON_REG_SIZE * i, 0x012C);
		MAE_CMDQ_WRITE_REG(pkt, MAE_REG_H_MAX0 + COMMON_REG_SIZE * i, 0x01F4);
		MAE_CMDQ_WRITE_REG(pkt, MAE_REG_V_MAX0 + COMMON_REG_SIZE * i, 0x01F4);
	}
}

static void mtk_mae_config_dma(struct mtk_mae_dev *mae_dev, int idx)
{
	struct EnqueParam *param =
		(struct EnqueParam*)mae_dev->map_table->param_dmabuf_info[idx].kva;
	uint64_t addr = 0;
	uint64_t config_addr = 0;
	uint64_t coef_addr = 0;
	uint32_t config_offset = 0;
	uint32_t config_rt_offset = 0;
	uint32_t config_size = 0;
	uint32_t coef_offset = 0;
	uint32_t coef_size = 0;
	uint32_t outer_loop = 0;
	uint32_t loop = 0;
	uint32_t i = 0;
	uint32_t wdma_base_addr_reg_offset = 0;
	struct ModelTable *model_table = (struct ModelTable *)mae_dev->map_table->model_table_dmabuf_info.kva;

	mae_dev_dbg(mae_dev->dev, "%s+", __func__);
	if (set_default_value)
		mtk_mae_set_default_value(mae_dev, mae_dev->pkt[idx]);

	switch (param->maeMode) {
	case FD_V0:
	case FD_V1_IPN:
	case FD_V1_FPN:
		outer_loop = param->pyramidNumber;
		break;
	case ATTR_V0:
	case FAC_V1:
		outer_loop = param->attrFaceNumber;
		break;
	case AISEG:
		outer_loop = 1;
		break;
	default:
		mae_dev_info(mae_dev->dev, "[%s] unsupport mode(%d)",
				__func__, param->maeMode);
		return;
	}

	if (outer_loop > MAX_OUTER_LOOP_NUM) {
		mae_dev_info(mae_dev->dev, "[%s] outer_loop num(%d) is over limitation",
					__func__, outer_loop);
		return;
	}

	// config the base address of internal buffer
	addr = mae_dev->map_table->internal_dmabuf_info.pa;
	if (CHECK_BASE_ADDR(addr) || addr == 0) {
		mae_dev_info(mae_dev->dev, "%s(0x%llx) is not %d-aligned",
				"internal buffer", addr ,MAE_BASE_ADDR_ALIGN);
		return;
	}

	mae_dev_dbg(mae_dev->dev, "internal base (0x%llx)", addr);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_INTRN_BASE_0_W, LSB_ADDR(addr));
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_INTRN_BASE_1_W, MSB_ADDR(addr));
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_INTRN_BASE_0_R, LSB_ADDR(addr));
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_INTRN_BASE_1_R, MSB_ADDR(addr));

	for (loop = 0; loop < outer_loop; loop++) {
		if (!param->image[loop].enRoi) {
			param->image[loop].roi.x1 = 0;
			param->image[loop].roi.y1 = 0;
			param->image[loop].roi.x2 = 0;
			param->image[loop].roi.y2 = 0;
		}

		if (!param->image[loop].enPadding) {
			param->image[loop].padding.left = 0;
			param->image[loop].padding.right = 0;
			param->image[loop].padding.up = 0;
			param->image[loop].padding.down = 0;
		}

		// config the base address of input buffer
		addr = mae_dev->map_table->image_dmabuf_info[idx][loop].pa;

		if (param->image[loop].imgWidth % 16 != 0) {
			mae_dev_info(mae_dev->dev, "Loop %d: image width(%d) should be 16-aligned",
					loop, param->image[loop].imgWidth);
			return;
		}

		if (param->image[loop].enRoi)
			addr += (MAX(param->image[loop].roi.y1, 0) / 2 * 2) * param->image[loop].imgWidth +
					(MAX(param->image[loop].roi.x1, 0) / 16) * 16;

		if (CHECK_BASE_ADDR(addr) || addr == 0)
			mae_dev_info(mae_dev->dev, "Loop %d: %s(0x%llx) is not %d-aligned or zero",
					loop, "image0", addr, MAE_BASE_ADDR_ALIGN);

		mae_dev_dbg(mae_dev->dev, "Loop %d: %s(0x%llx)", loop, "image0", addr);

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				MAE_REG_EXTRN_BASE0_00_0_R + loop * BASE_ADDR_REG_SIZE,
				LSB_ADDR(addr));
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				MAE_REG_EXTRN_BASE0_00_1_R + loop * BASE_ADDR_REG_SIZE,
				MSB_ADDR(addr));

		addr = mae_dev->map_table->image_dmabuf_info[idx][loop].pa +
				param->image[loop].imgWidth * param->image[loop].imgHeight;
		if (param->image[loop].enRoi)
			addr += (MAX(param->image[loop].roi.y1, 0) / 2 * 2) * (param->image[loop].imgWidth / 2) +
					(MAX(param->image[loop].roi.x1, 0) / 16) * 16;

		if (CHECK_BASE_ADDR(addr) || addr == 0)
			mae_dev_info(mae_dev->dev, "Loop %d: %s(0x%llx) is not %d-aligned or zero",
										loop, "image1", addr, MAE_BASE_ADDR_ALIGN);

		mae_dev_dbg(mae_dev->dev, "Loop %d: %s(0x%llx)", loop, "image1", addr);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				MAE_REG_EXTRN_BASE1_00_0_R + loop * BASE_ADDR_REG_SIZE,
				LSB_ADDR(addr));
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				MAE_REG_EXTRN_BASE1_00_1_R + loop * BASE_ADDR_REG_SIZE,
				MSB_ADDR(addr));

		// config the base address of output buffer
		if (param->maeMode == AISEG) {
			for (i = 0; i < AISEG_MAP_NUM; i++) {
				addr = mae_dev->map_table->aiseg_output_dmabuf_info[idx][i].pa +
					model_table->aisegOutput[i].offset;
				if (CHECK_BASE_ADDR(addr) || addr == 0)
					mae_dev_info(mae_dev->dev, "Loop %d: %s %d (0x%llx) is not %d-aligned",
						loop, "aiseg output", i, addr, MAE_BASE_ADDR_ALIGN);

				mae_dev_dbg(mae_dev->dev, "Loop %d: %s %d (0x%llx)", loop, "aiseg output", i, addr);
				mae_dev_dbg(mae_dev->dev, "aiseg offset (0x%lx)", model_table->aisegOutput[i].offset);

				wdma_base_addr_reg_offset = BASE_ADDR_REG_SIZE * i;

				MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
					MAE_REG_EXTRN_BASE0_00_0_W + wdma_base_addr_reg_offset,
					LSB_ADDR(addr));

				MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
					MAE_REG_EXTRN_BASE0_00_1_W + wdma_base_addr_reg_offset,
					MSB_ADDR(addr));
			}
		} else {
			addr = mae_dev->map_table->output_dmabuf_info[idx][loop].pa;
			if (CHECK_BASE_ADDR(addr) || addr == 0)
				mae_dev_info(mae_dev->dev, "Loop %d: %s (0x%llx) is not %d-aligned",
					loop, "output", addr, MAE_BASE_ADDR_ALIGN);
			mae_dev_dbg(mae_dev->dev, "Loop %d: %s (0x%llx)", loop, "output", addr);

			switch (param->maeMode) {
			case FD_V0:
				wdma_base_addr_reg_offset = BASE_ADDR_REG_SIZE * FD_V0_WDMA_NUM * loop;

				MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
					MAE_REG_EXTRN_BASE0_00_0_W + wdma_base_addr_reg_offset,
					LSB_ADDR(addr));

				MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
					MAE_REG_EXTRN_BASE0_00_1_W + wdma_base_addr_reg_offset,
					MSB_ADDR(addr));
				break;
			case FD_V1_IPN:
				for (i = 0; i < FD_V1_IPN_WDMA_NUM; i++) {
					wdma_base_addr_reg_offset =
						BASE_ADDR_REG_SIZE * (FD_V1_IPN_WDMA_NUM * loop + i);

					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						MAE_REG_EXTRN_BASE0_00_0_W + wdma_base_addr_reg_offset,
						LSB_ADDR(addr));

					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						MAE_REG_EXTRN_BASE0_00_1_W + wdma_base_addr_reg_offset,
						MSB_ADDR(addr));

					addr += FD_V1_IPN_WDMA_SIZE;
				}
				break;
			case FD_V1_FPN:
				for (i = 0; i < FD_V1_FPN_WDMA_NUM; i++) {
					wdma_base_addr_reg_offset =
						BASE_ADDR_REG_SIZE * (FD_V1_FPN_WDMA_NUM * loop + i);

					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						MAE_REG_EXTRN_BASE0_00_0_W + wdma_base_addr_reg_offset,
						LSB_ADDR(addr));

					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						MAE_REG_EXTRN_BASE0_00_1_W + wdma_base_addr_reg_offset,
						MSB_ADDR(addr));

					addr += FD_V1_FPN_WDMA_SIZE;
				}
				break;
			case ATTR_V0:
				for (i = 0; i < ATTR_V0_WDMA_NUM; i++) {
					wdma_base_addr_reg_offset =
					BASE_ADDR_REG_SIZE * (ATTR_V0_WDMA_NUM * loop + i);

					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						MAE_REG_EXTRN_BASE0_00_0_W + wdma_base_addr_reg_offset,
						LSB_ADDR(addr));

					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						MAE_REG_EXTRN_BASE0_00_1_W + wdma_base_addr_reg_offset,
						MSB_ADDR(addr));

					addr += ATTR_V0_WDMA_SIZE * WDMA_DATA_UNIT;
				}

				if (loop == 0) {
					// follow model footprint document
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_00_W, 1);
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_01_W, 1);
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_02_W, 1);
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_03_W, 1);
				}
				break;
			case FAC_V1:
				for (i = 0; i < FAC_V1_WDMA_NUM; i++) {
					wdma_base_addr_reg_offset =
						BASE_ADDR_REG_SIZE * (FAC_V1_WDMA_NUM * loop + i);

					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						MAE_REG_EXTRN_BASE0_00_0_W + wdma_base_addr_reg_offset,
						LSB_ADDR(addr));

					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						MAE_REG_EXTRN_BASE0_00_1_W + wdma_base_addr_reg_offset,
						MSB_ADDR(addr));

					addr += FAC_V1_WDMA_SIZE * WDMA_DATA_UNIT;
				}

				if (loop == 0) {
					// follow model footprint document
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_00_W, 54);
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_01_W, 2);
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_02_W, 2);
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_03_W, 2);
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_04_W, 2);
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_05_W, 2);
				}
				break;
			default:
				mae_dev_info(mae_dev->dev, "[%s] unsupport mode(%d)",
					__func__, param->maeMode);
				return;
			}
		}

		// calculate the offset and size of binary file
		if (param->maeMode == FD_V0 || param->maeMode == FD_V1_IPN || param->maeMode == FD_V1_FPN) {
			if (param->image[loop].enRoi) {
				mae_dev->core_sel[idx] = mtk_mae_fd_core_sel(mae_dev,
						param->image[loop].resizeWidth,
						param->image[loop].resizeWidth *
						(param->image[loop].roi.y2 - param->image[loop].roi.y1 + 1) /
						(param->image[loop].roi.x2 - param->image[loop].roi.x1 + 1));
			} else {
				mae_dev->core_sel[idx] = mtk_mae_fd_core_sel(mae_dev,
						param->image[loop].resizeWidth,
						param->image[loop].resizeWidth *
						param->image[loop].imgHeight / param->image[loop].imgWidth);
			}

			if (mae_dev->core_sel[idx] < 0)
				return;
		}

		switch (param->maeMode) {
		case FD_V0:
			config_offset = v0_fd_config_offset[mae_dev->core_sel[idx]];
			coef_offset = v0_fd_coef_offset[mae_dev->core_sel[idx]];

			if (param->fdInputDegree == DEGREE_90 ||
				param->fdInputDegree == DEGREE_180) {
				config_rt_offset = fd_v0_config_info[mae_dev->core_sel[idx]].rotate_offset;
				config_size = fd_v0_config_info[mae_dev->core_sel[idx]].rotate_size;
			} else {
				config_rt_offset = 0;
				config_size = fd_v0_config_info[mae_dev->core_sel[idx]].size;
			}

			coef_size = fd_v0_coef_info[mae_dev->core_sel[idx]].size;

			config_addr =
				mae_dev->map_table->config_dmabuf_info[MODEL_TYPE_FD_V0].pa + config_offset;
			coef_addr =
				mae_dev->map_table->coef_dmabuf_info[MODEL_TYPE_FD_V0].pa + coef_offset;
			break;
		case FD_V1_IPN:
			config_offset = v1_fd_ipn_config_offset[mae_dev->core_sel[idx]];
			coef_offset = v1_fd_ipn_coef_offset[mae_dev->core_sel[idx]];

			if (param->fdInputDegree == DEGREE_90 ||
				param->fdInputDegree == DEGREE_180) {
				config_rt_offset = fd_v1_ipn_config_info[mae_dev->core_sel[idx]].rotate_offset;
				config_size = fd_v1_ipn_config_info[mae_dev->core_sel[idx]].rotate_size;
			} else {
				config_rt_offset = 0;
				config_size = fd_v1_ipn_config_info[mae_dev->core_sel[idx]].size;
			}

			coef_size = fd_v1_ipn_coef_info[mae_dev->core_sel[idx]].size;

			config_addr =
				mae_dev->map_table->config_dmabuf_info[MODEL_TYPE_FD_V1_IPN].pa + config_offset;
			coef_addr =
				mae_dev->map_table->coef_dmabuf_info[MODEL_TYPE_FD_V1_IPN].pa + coef_offset;
			break;
		case FD_V1_FPN:
			config_offset = 0;
			coef_offset = 0;

			if (param->fdInputDegree == DEGREE_90 ||
				param->fdInputDegree == DEGREE_180) {
				config_rt_offset = fd_v1_fpn_config_info.rotate_offset;
				config_size = fd_v1_fpn_config_info.rotate_size;
			} else {
				config_rt_offset = 0;
				config_size = fd_v1_fpn_config_info.size;
			}
			coef_size = fd_v1_fpn_coef_info.size;

			config_addr =
				mae_dev->map_table->config_dmabuf_info[MODEL_TYPE_FD_V1_FPN].pa + config_offset;
			coef_addr =
				mae_dev->map_table->coef_dmabuf_info[MODEL_TYPE_FD_V1_FPN].pa + coef_offset;
			break;
		case ATTR_V0:
			config_offset = 0;
			coef_offset = 0;

			if (param->attrInputDegree[loop] == DEGREE_90 ||
				param->attrInputDegree[loop] == DEGREE_180) {
				// MAE_TO_CHECK: rotate_offset is 16B align but offset is not
				config_rt_offset = attr_v0_config_info.rotate_offset;
				config_size = attr_v0_config_info.rotate_size;
			} else {
				config_rt_offset = 0;
				config_size = attr_v0_config_info.size;
			}

			coef_size = attr_v0_coef_info.size;

			config_addr =
				mae_dev->map_table->config_dmabuf_info[MODEL_TYPE_FLD_FAC_V0].pa +
				config_offset;
			coef_addr =
				mae_dev->map_table->coef_dmabuf_info[MODEL_TYPE_FLD_FAC_V0].pa +
				coef_offset;
			break;
		case FAC_V1:
			config_offset = 0;
			coef_offset = 0;

			if (param->attrInputDegree[loop] == DEGREE_90 ||
				param->attrInputDegree[loop] == DEGREE_180) {
				// MAE_TO_CHECK: rotate_offset is 16B align but offset is not
				config_rt_offset = fac_v1_config_info.rotate_offset;
				config_size = fac_v1_config_info.rotate_size;
			} else {
				config_rt_offset = 0;
				config_size = fac_v1_config_info.size;
			}

			coef_size = fac_v1_coef_info.size;

			config_addr =
				mae_dev->map_table->config_dmabuf_info[MODEL_TYPE_FLD_FAC_V1].pa +
				config_offset;
			coef_addr =
				mae_dev->map_table->coef_dmabuf_info[MODEL_TYPE_FLD_FAC_V1].pa +
				coef_offset;
			break;
		case AISEG:
			config_rt_offset = 0;
			config_offset =
				model_table->configTable[MODEL_TYPE_AISEG].offset;
			coef_offset =
				model_table->coefTable[MODEL_TYPE_AISEG].offset;

			config_size =
				model_table->configTable[MODEL_TYPE_AISEG].size >> LSB_ADDR_SHIFT_BITS;
			coef_size =
				model_table->coefTable[MODEL_TYPE_AISEG].size >> LSB_ADDR_SHIFT_BITS;

			config_addr =
				mae_dev->map_table->config_dmabuf_info[MODEL_TYPE_AISEG].pa +
				config_offset ;
			coef_addr =
				mae_dev->map_table->coef_dmabuf_info[MODEL_TYPE_AISEG].pa +
				coef_offset;
			break;
		default:
			mae_dev_info(mae_dev->dev, "[%s] unsupport mode(%d)",
						__func__, param->maeMode);
			break;
		}

		mae_dev_dbg(mae_dev->dev,
			"sel: %d, deg: %d, config_offset: %d (B), config_rt_offset: %d (16B), config_size: %d (16 B), ",
			mae_dev->core_sel[idx], param->fdInputDegree, config_offset, config_rt_offset, config_size);
		mae_dev_dbg(mae_dev->dev,
			"coef_offset: %d (B), coef_size: %d (16 B) , config_addr: 0x%llx , coef_addr: 0x%llx",
			coef_offset, coef_size, config_addr, coef_addr);

		// config the base address of config buffer
		if (CHECK_BASE_ADDR(config_addr) || config_addr == 0)
			mae_dev_info(mae_dev->dev, "Loop %d: %s(0x%llx) is not %d-aligned",
					loop, "config0", config_addr, MAE_BASE_ADDR_ALIGN);

		mae_dev_dbg(mae_dev->dev, "Loop %d: %s(0x%llx)", loop, "config", config_addr);

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				MAE_REG_OUTER_CONFIG_BASE_00_0 + loop * BASE_ADDR_REG_SIZE,
				LSB_ADDR(config_addr + (config_rt_offset << LSB_ADDR_SHIFT_BITS)));
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				MAE_REG_OUTER_CONFIG_BASE_00_1 + loop * BASE_ADDR_REG_SIZE,
				MSB_ADDR(config_addr + (config_rt_offset << LSB_ADDR_SHIFT_BITS)));

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				MAE_REG_OUTER_CONFIG_SIZE_00 + loop * COMMON_REG_SIZE,
				config_size);

		// config the base address of coef buffer
		if (CHECK_BASE_ADDR(coef_addr) || coef_addr == 0)
			mae_dev_info(mae_dev->dev, "Loop %d: %s(0x%llx) is not %d-aligned",
					loop, "coef0", coef_addr, MAE_BASE_ADDR_ALIGN);

		mae_dev_dbg(mae_dev->dev, "Loop %d: %s(0x%llx)", loop, "coef0", coef_addr);

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				MAE_REG_OUTER_COEF_BASE_00_0 + loop * BASE_ADDR_REG_SIZE,
				LSB_ADDR(coef_addr));
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				MAE_REG_OUTER_COEF_BASE_00_1 + loop * BASE_ADDR_REG_SIZE,
				MSB_ADDR(coef_addr));

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				MAE_REG_OUTER_COEF_SIZE_00 + loop * COMMON_REG_SIZE,
				coef_size);
	}

	mae_dev_dbg(mae_dev->dev, "%s-", __func__);
}

// follow DE crop formula
void mtk_mae_crop(struct mtk_mae_dev *mae_dev,
		const struct crop_setting_in *in,
		struct crop_setting_out *out)
{
	int32_t crop_x_size;
	int32_t crop_y_size;
	int32_t even_end_y;
	int32_t even_start_y;
	int32_t reg_outer_src_hsize;
	int32_t reg_outer_src_vsize;
	// int32_t base0_shift_offset;
	// int32_t base0_shift;
	// int32_t base1_shift;
	int32_t crop_right_x;
	int32_t crop_left_x;
	int32_t crop_down_y;
	int32_t crop_up_y;


	mae_dev_dbg(mae_dev->dev, "[%s] s_x(%d), e_x(%d), s_y(%d), e_y(%d), input_h_size(%d), input_v_size(%d)\n",
		__func__, in->start_x, in->end_x, in->start_y, in->end_y,
		in->input_h_size, in->input_v_size);

	crop_x_size = in->end_x - in->start_x;
	crop_y_size = in->end_y - in->start_y;

	if (crop_x_size <= 0 || crop_y_size <= 0 || in->input_h_size <= 0 || in->input_v_size <= 0) {
		mae_dev_info(mae_dev->dev, "[%s] error input height/width(%d/%d), (x1,x2,y1,y2)=(%d,%d,%d,%d)",
				__func__, in->input_v_size, in->input_h_size,
				in->start_x, in->end_x, in->start_y, in->end_y);
		return;
	}

	even_end_y = DIV_CEIL_POS(MIN(in->end_y, in->input_v_size), 2) * 2;
	even_start_y = MAX(in->start_y, 0) / 2 * 2;

	reg_outer_src_hsize = DIV_CEIL_POS(MIN(in->end_x, in->input_h_size), 16) * 16
		- MIN(MAX(in->start_x, 0), in->input_h_size) / 16 * 16;
	reg_outer_src_vsize = even_end_y - even_start_y;

	// base0_shift_offset = MAX(in->start_x, 0) / 16;
	// base0_shift = even_start_y * 40 + base0_shift_offset;
	// base1_shift = even_start_y * 20 + base0_shift_offset;

	crop_right_x = MIN(in->end_x, in->input_h_size)
		- DIV_CEIL_POS(MIN(in->end_x, in->input_h_size), 16) * 16;
	crop_left_x =  MAX(in->start_x, 0)
		- MIN(MAX(in->start_x, 0), in->input_h_size) / 16 * 16;

	crop_down_y = even_end_y - MIN(in->end_y, in->input_v_size);
	crop_up_y = even_start_y - MAX(in->start_y, 0);

	out->reg_pre_crop_h_st = ABS(crop_left_x);
	out->reg_pre_crop_h_length =
		reg_outer_src_hsize - ABS(crop_right_x) - ABS(crop_left_x);
	out->reg_pre_crop_hfde_size = reg_outer_src_hsize;

	out->reg_pre_crop_v_st = ABS(crop_up_y);
	out->reg_pre_crop_v_length =
		reg_outer_src_vsize - ABS(crop_down_y) - ABS(crop_up_y);
	out->reg_pre_crop_vfde_size = reg_outer_src_vsize;

	out->reg_ins_path = 1;
	out->reg_pre_crop_h_crop_en = 1;
	out->reg_pre_crop_v_crop_en = 1;

	mae_dev_dbg(mae_dev->dev, "[%s] reg_pre_crop_h_st(%d), reg_pre_crop_h_length(%d), ",
			__func__,
			out->reg_pre_crop_h_st,
			out->reg_pre_crop_h_length);
	mae_dev_dbg(mae_dev->dev, "reg_pre_crop_hfde_size(%d), reg_pre_crop_v_st(%d), reg_pre_crop_v_length(%d), ",
			out->reg_pre_crop_hfde_size,
			out->reg_pre_crop_v_st,
			out->reg_pre_crop_v_length);
	mae_dev_dbg(mae_dev->dev, "pre_crop_vfde_size(%d), ins_path(%d), pre_crop_h_crop_en(%d), pre_crop_v_crop_en(%d)\n",
			out->reg_pre_crop_vfde_size,
			out->reg_ins_path,
			out->reg_pre_crop_h_crop_en,
			out->reg_pre_crop_v_crop_en);
}

// follow DE crop formula
void mtk_mae_padding(struct mtk_mae_dev *mae_dev,
			const struct padding_setting_in *in,
			struct padding_setting_out *out)
{
	int32_t pad_left_x = -in->left;
	int32_t pad_right_x = -in->right;
	int32_t pad_down_y = -in->down;
	int32_t pad_up_y = -in->up;

	mae_dev_dbg(mae_dev->dev, "[%s] l(%d), r(%d), d(%d), u(%d), crop_output_h_size(%d), crop_output_v_size(%d)\n",
		__func__,
		in->left, in->right, in->down, in->up,
		in->crop_output_h_size,
		in->crop_output_v_size);

	if (in->crop_output_h_size <= 0 || in->crop_output_v_size <= 0) {
		mae_dev_info(mae_dev->dev, "[%s] error input height/width(%d/%d)",
			__func__, in->crop_output_v_size, in->crop_output_h_size);
		return;
	}

	out->reg_post_ins_blk_hpre = ABS(pad_left_x);
	out->reg_post_ins_h_length = DIV_CEIL_POS(in->crop_output_h_size, 4) * 4;
	out->reg_post_ins_hfde_size =
		DIV_CEIL_POS((out->reg_post_ins_h_length + ABS(pad_right_x) + ABS(pad_left_x)), 4) * 4;

	out->reg_post_ins_blk_vpre = ABS(pad_up_y);
	out->reg_post_ins_v_length = in->crop_output_v_size;
	out->reg_post_ins_vfde_size = in->crop_output_v_size + ABS(pad_up_y) + ABS(pad_down_y);

	out->reg_h_size = out->reg_post_ins_h_length + ABS(pad_right_x) + ABS(pad_left_x);
	out->reg_v_size = out->reg_post_ins_vfde_size;
	out->reg_post_ins_hv_insert_en = 1;

	if (out->reg_post_ins_blk_vpre > 0)
		if (out->reg_post_ins_blk_hpre < 4 &&
		(out->reg_post_ins_hfde_size - out->reg_post_ins_h_length - out->reg_post_ins_blk_hpre) < 4) {
			out->reg_post_ins_hfde_size = out->reg_post_ins_hfde_size + 4;
			out->reg_h_size = out->reg_post_ins_h_length + ABS(pad_right_x) + 4;
		}

	mae_dev_dbg(mae_dev->dev, "[%s] reg_post_ins_blk_hpre(%d), reg_post_ins_h_length(%d), reg_post_ins_hfde_size(%d), ",
			__func__,
			out->reg_post_ins_blk_hpre,
			out->reg_post_ins_h_length,
			out->reg_post_ins_hfde_size);
	mae_dev_dbg(mae_dev->dev, "reg_post_ins_blk_vpre(%d), reg_post_ins_v_length(%d), reg_post_ins_vfde_size(%d), ",
			out->reg_post_ins_blk_vpre,
			out->reg_post_ins_v_length,
			out->reg_post_ins_vfde_size);
	mae_dev_dbg(mae_dev->dev, "reg_h_size(%d), reg_v_size(%d), reg_post_ins_hv_insert_en(%d)\n",
			out->reg_h_size,
			out->reg_v_size,
			out->reg_post_ins_hv_insert_en);
}

// follow DE resize formula
static void mtk_mae_resize(struct mtk_mae_dev *mae_dev,
		const struct rsz_setting_in *in,
		struct rsz_setting_out *out)
{
	int32_t inputHeight_used = in->rsz_input_v_size;
	int32_t inputWidth_used = in->rsz_input_h_size;

	mae_dev_dbg(mae_dev->dev, "[%s] rsz_input_h_size(%d), "
			"rsz_input_v_size(%d), "
			"rsz_output_h_size(%d), "
			"rsz_output_v_size(%d), "
			"rsz_input_ch(%d)\n",
			__func__,
			in->rsz_input_h_size,
			in->rsz_input_v_size,
			in->rsz_output_h_size,
			in->rsz_output_v_size,
			in->rsz_input_ch);

	if (inputHeight_used <= 0 || inputWidth_used <= 0) {
		mae_dev_info(mae_dev->dev, "[%s] input height/width(%d/%d) should not be negative",
				__func__, inputHeight_used, inputWidth_used);
		return;
	}

	out->reg_1p_path_en = 1;
	out->reg_h_size_usr_md = 1;
	out->reg_v_size_usr_md = 1;
	out->reg_scale_ve_en = 1;
	out->reg_scale_ho_en = 1;
	out->reg_rsz_u2s = 1;

	if (in->rsz_input_h_size < in->rsz_output_h_size)
		out->reg_order = 1;
	else
	 	out->reg_order = 0;

	if (in->rsz_input_h_size > in->rsz_output_h_size &&
		in->rsz_input_v_size > in->rsz_output_v_size) {
		// cb mode
		out->reg_rsz_mode_ho = 1;
		out->reg_rsz_mode_ve = 1;
		out->reg_cb_factor_ho = (int32_t)((in->rsz_output_h_size) << 20) / inputWidth_used;
		out->reg_cb_factor_ve = (int32_t)((in->rsz_output_v_size) << 20) / inputHeight_used;

		if ((out->reg_cb_factor_ho - ((out->reg_cb_factor_ho >> 8) << 8)) > 0)
			out->reg_cb_factor_ho = out->reg_cb_factor_ho + 1;

		if ((out->reg_cb_factor_ve - ((out->reg_cb_factor_ve >> 8) << 8)) > 0)
			out->reg_cb_factor_ve = out->reg_cb_factor_ve + 1;
		// confirm with DE
		out->reg_mode_c_ve = 1;
		out->reg_mode_c_ho = 1;
	} else {
		// bilinear mode
		out->reg_scale_factor_ve = (int32_t)((((inputHeight_used - 1) << 20) +
			((in->rsz_output_v_size - 1) >> 1)) / (in->rsz_output_v_size - 1));
		out->reg_v_shift_mode_en = 0;
		out->reg_ini_factor_ve = 0;

		out->reg_scale_factor_ho = (int32_t)((((inputWidth_used - 1) << 20) +
			((in->rsz_output_h_size - 1) >> 1)) / (in->rsz_output_h_size - 1));
		out->reg_h_shift_mode_en = 0;
		out->reg_ini_factor_ho = 0;

		out->reg_mode_c_ve = 1;
		out->reg_mode_c_ho = 1;
	}

	mae_dev_dbg(mae_dev->dev, "[%s] reg_scale_factor_ve(%d), "
			"reg_v_shift_mode_en(%d), "
			"reg_ini_factor_ve(%d), "
			"reg_scale_factor_ho(%d), "
			"reg_h_shift_mode_en(%d), "
			"reg_ini_factor_ho(%d), "
			"reg_1p_path_en(%d), "
			"reg_order(%d), "
			"reg_h_size_usr_md(%d), "
			"reg_v_size_usr_md(%d), "
			"reg_scale_ve_en(%d), "
			"reg_scale_ho_en(%d), "
			"reg_mode_c_ve(%d), "
			"reg_mode_c_ho(%d), "
			"reg_rsz_mode_ho(%d), "
			"reg_rsz_mode_ve(%d), "
			"reg_cb_factor_ho(%d), "
			"reg_cb_factor_ve(%d), "
			"reg_rsz_u2s(%d)\n",
			__func__,
			out->reg_scale_factor_ve,
			out->reg_v_shift_mode_en,
			out->reg_ini_factor_ve,
			out->reg_scale_factor_ho,
			out->reg_h_shift_mode_en,
			out->reg_ini_factor_ho,
			out->reg_1p_path_en,
			out->reg_order,
			out->reg_h_size_usr_md,
			out->reg_v_size_usr_md,
			out->reg_scale_ve_en,
			out->reg_scale_ho_en,
			out->reg_mode_c_ve,
			out->reg_mode_c_ho,
			out->reg_rsz_mode_ho,
			out->reg_rsz_mode_ve,
			out->reg_cb_factor_ho,
			out->reg_cb_factor_ve,
			out->reg_rsz_u2s);
}

static bool mtk_mae_config_rsz(struct mtk_mae_dev *mae_dev,
				struct EnqueParam *param,
				struct cmdq_pkt *pkt,
				uint32_t rsz_offset,
				int idx)
{
	struct crop_setting_in crop_in;
	struct crop_setting_out crop_out = {0};
	struct padding_setting_in padding_in;
	struct padding_setting_out padding_out = {0};
	struct rsz_setting_in rsz_in;
	struct rsz_setting_out rsz_out = {0};

	// crop
	if (param->image[0].enRoi) {
		crop_in.start_x = param->image[0].roi.x1;
		crop_in.end_x = param->image[0].roi.x2;
		crop_in.start_y = param->image[0].roi.y1;
		crop_in.end_y = param->image[0].roi.y2;
	} else {
		crop_in.start_x = 0;
		crop_in.end_x = param->image[0].imgWidth;
		crop_in.start_y = 0;
		crop_in.end_y = param->image[0].imgHeight;
	}
	crop_in.input_h_size = param->image[0].imgWidth;
	crop_in.input_v_size = param->image[0].imgHeight;

	mtk_mae_crop(mae_dev, &crop_in, &crop_out);

	// padding
	if (param->maeMode == FD_V0 || param->maeMode == FD_V1_IPN) {
		padding_in.left = 0;
		padding_in.up = 0;

		if (param->image[0].enRoi) {
			padding_in.crop_output_h_size = param->image[0].roi.x2 - param->image[0].roi.x1 + 1;
			padding_in.crop_output_v_size = param->image[0].roi.y2 - param->image[0].roi.y1 + 1;
		} else {
			padding_in.crop_output_h_size = param->image[0].imgWidth;
			padding_in.crop_output_v_size = param->image[0].imgHeight;
	}

		padding_in.right =
			(fd_pattern_width[mae_dev->core_sel[idx]] * padding_in.crop_output_h_size) /
			(param->image[0].resizeWidth)
			- padding_in.crop_output_h_size;
		padding_in.down =
			(fd_pattern_height[mae_dev->core_sel[idx]] * padding_in.crop_output_h_size) /
			(param->image[0].resizeWidth)
			- padding_in.crop_output_v_size;

		if (padding_in.right < 0 || padding_in.down < 0) {
			mae_dev_info(mae_dev->dev, "can not padding negative value r(%d) d(%d)",
				padding_in.right, padding_in.down);
			return false; // RETURN_ERROR
		}
	} else {
		padding_in.left = param->image[0].padding.left;
		padding_in.right = param->image[0].padding.right;
		padding_in.down = param->image[0].padding.down;
		padding_in.up = param->image[0].padding.up;
		if (param->image[0].enRoi) {
			padding_in.crop_output_h_size = param->image[0].roi.x2 - param->image[0].roi.x1 + 1;
			padding_in.crop_output_v_size = param->image[0].roi.y2 - param->image[0].roi.y1 + 1;
		} else {
			padding_in.crop_output_h_size = param->image[0].imgWidth;
			padding_in.crop_output_v_size = param->image[0].imgHeight;
		}
	}

	mtk_mae_padding(mae_dev, &padding_in, &padding_out);

	rsz_in.rsz_input_h_size =
		padding_in.crop_output_h_size + padding_in.left + padding_in.right;
	rsz_in.rsz_input_v_size =
		padding_in.crop_output_v_size + padding_in.down + padding_in.up;

	switch (param->maeMode) {
	case FD_V0:
	case FD_V1_IPN:
		rsz_in.rsz_output_h_size = fd_pattern_width[mae_dev->core_sel[idx]];
		rsz_in.rsz_output_v_size = fd_pattern_height[mae_dev->core_sel[idx]];
		break;
	case FD_V1_FPN:
		rsz_in.rsz_output_h_size = FPN_PYRAMID_WIDTH;
		rsz_in.rsz_output_v_size = FPN_PYRAMID_HEIGHT;
		break;
	case ATTR_V0:
	case FAC_V1:
		rsz_in.rsz_output_h_size = param->image[0].imgWidth;
		rsz_in.rsz_output_v_size = param->image[0].imgHeight;
		break;
	default:
		break;
	}
	rsz_in.rsz_input_ch = 8;
	mtk_mae_resize(mae_dev, &rsz_in, &rsz_out);

	//
	MAE_CMDQ_WRITE_REG(pkt, REG_00C8_RSZ1 + rsz_offset,
			REG_RANGE(crop_out.reg_pre_crop_h_st, 13, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_00CC_RSZ1 + rsz_offset,
			REG_RANGE(crop_out.reg_pre_crop_h_length, 13, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_00D0_RSZ1 + rsz_offset,
			REG_RANGE(crop_out.reg_pre_crop_hfde_size, 13, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_00D4_RSZ1 + rsz_offset,
			REG_RANGE(crop_out.reg_pre_crop_v_st, 13, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_00D8_RSZ1 + rsz_offset,
			REG_RANGE(crop_out.reg_pre_crop_v_length, 13, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_00C0_RSZ1 + rsz_offset,
			0x80 +
			(REG_RANGE(crop_out.reg_pre_crop_v_crop_en, 0, 0) << 1) +
			REG_RANGE(crop_out.reg_pre_crop_h_crop_en, 0, 0));

	//
	MAE_CMDQ_WRITE_REG(pkt,
			REG_010C_RSZ1 + rsz_offset,
			REG_RANGE(padding_out.reg_post_ins_blk_hpre, 13, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_0110_RSZ1 + rsz_offset,
			REG_RANGE(padding_out.reg_post_ins_h_length, 13, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_0114_RSZ1 + rsz_offset,
			REG_RANGE(padding_out.reg_post_ins_hfde_size, 13, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_0118_RSZ1 + rsz_offset,
			REG_RANGE(padding_out.reg_post_ins_blk_vpre, 13, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_011C_RSZ1 + rsz_offset,
			REG_RANGE(padding_out.reg_post_ins_v_length, 13, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_0120_RSZ1 + rsz_offset,
			REG_RANGE(padding_out.reg_post_ins_vfde_size, 13, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_00A0_RSZ1 + rsz_offset,
			(1 << 15) + REG_RANGE(padding_out.reg_h_size, 13, 0));

	// reg_v_size_usr_md_1[15] = 1
	MAE_CMDQ_WRITE_REG(pkt,
			REG_00A4_RSZ1 + rsz_offset,
			(1 << 15) + REG_RANGE(padding_out.reg_v_size, 13, 0));

	// reg_post_ins_boundary_md_1[0] = 0, reg_post_ins_de_start_trig_md_1 = 0
	MAE_CMDQ_WRITE_REG(pkt,
			REG_0104_RSZ1 + rsz_offset,
			REG_RANGE(padding_out.reg_post_ins_hv_insert_en, 0, 0));

	//
	MAE_CMDQ_WRITE_REG(pkt,
			REG_0004_RSZ1 + rsz_offset,
			REG_RANGE(rsz_out.reg_ini_factor_ho, 15, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_0008_RSZ1 + rsz_offset,
			REG_RANGE(rsz_out.reg_ini_factor_ho, 27, 16));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_000C_RSZ1 + rsz_offset,
			REG_RANGE(rsz_out.reg_ini_factor_ve, 15, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_0010_RSZ1 + rsz_offset,
			REG_RANGE(rsz_out.reg_ini_factor_ve, 27, 16));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_001C_RSZ1 + rsz_offset,
			REG_RANGE(rsz_out.reg_scale_factor_ho, 15, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_0020_RSZ1 + rsz_offset,
			(REG_RANGE(rsz_out.reg_scale_factor_ho, 27, 24) << 12) +
			(REG_RANGE(rsz_out.reg_h_shift_mode_en, 0, 0) << 9) +
			(REG_RANGE(rsz_out.reg_scale_ho_en, 0, 0) << 8) +
			(REG_RANGE(rsz_out.reg_scale_factor_ho, 23, 16)));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_0024_RSZ1 + rsz_offset,
			REG_RANGE(rsz_out.reg_scale_factor_ve, 15, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_0028_RSZ1 + rsz_offset,
			(REG_RANGE(rsz_out.reg_scale_factor_ve, 27, 24) << 12) +
			(REG_RANGE(rsz_out.reg_v_shift_mode_en, 0, 0) << 9) +
			(REG_RANGE(rsz_out.reg_scale_ve_en, 0, 0) << 8) +
			(REG_RANGE(rsz_out.reg_scale_factor_ve, 23, 16)));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_002C_RSZ1 + rsz_offset,
			(REG_RANGE(rsz_out.reg_mode_c_ve, 1, 0) << 8) +
			(REG_RANGE(rsz_out.reg_mode_c_ho, 1, 0)));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_0034_RSZ1 + rsz_offset,
			(REG_RANGE(rsz_out.reg_rsz_u2s, 0, 0) << 1) + 0);

	MAE_CMDQ_WRITE_REG(pkt,
			REG_005C_RSZ1 + rsz_offset,
			REG_RANGE(rsz_out.reg_cb_factor_ho, 15, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_0060_RSZ1 + rsz_offset,
			REG_RANGE(rsz_out.reg_cb_factor_ho, 19, 16));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_0064_RSZ1 + rsz_offset,
			REG_RANGE(rsz_out.reg_cb_factor_ve, 15, 0));

	MAE_CMDQ_WRITE_REG(pkt,
			REG_0068_RSZ1 + rsz_offset,
			REG_RANGE(rsz_out.reg_cb_factor_ve, 19, 16));

	if (!param->image[0].enPadding) {
		// size_usr_md[15] = 1
		MAE_CMDQ_WRITE_REG(pkt,
				REG_00A0_RSZ1 + rsz_offset,
				(1 << 15) +
				rsz_in.rsz_input_h_size);

		// reg_v_size_usr_md_1[15] = 1
		MAE_CMDQ_WRITE_REG(pkt,
				REG_00A4_RSZ1 + rsz_offset,
				(1 << 15) +
				rsz_in.rsz_input_v_size);
	}

	MAE_CMDQ_WRITE_REG(pkt,
			REG_00A8_RSZ1 + rsz_offset,
			rsz_in.rsz_output_h_size);

	MAE_CMDQ_WRITE_REG(pkt,
			REG_00AC_RSZ1 + rsz_offset,
			rsz_in.rsz_output_v_size);

	if (!param->image[0].enRoi)
		MAE_CMDQ_WRITE_REG(pkt,
				REG_00D0_RSZ1 + rsz_offset,
				rsz_in.rsz_input_h_size);

	MAE_CMDQ_WRITE_REG(pkt,
			REG_0180_RSZ1 + rsz_offset,
			(REG_RANGE(rsz_out.reg_order, 0, 0) << 2) +
			(REG_RANGE(rsz_out.reg_rsz_mode_ho, 0, 0) << 4) +
			(REG_RANGE(rsz_out.reg_rsz_mode_ve, 0, 0) << 8));

	return true;
}

static void mtk_mae_fd_post(struct mtk_mae_dev *mae_dev,
			struct EnqueParam *param,
			struct cmdq_pkt *pkt,
			int core_sel,
			MAE_MODE mode)
{
	uint32_t core_offset;
	struct EnqueImage *image = &param->image[0];

	switch (core_sel) {
	case 0:
	case 1:
		core_offset = 0;
		break;
	case 2:
		core_offset = 1;
		break;
	case 3:
		core_offset = 2;
		break;
	default:
		break;
	}

	if (mae_fd_post_on == 0) {
		// for ut
		switch (mode) {
		case FD_V0:
			if (core_sel == 0) {
				cmdq_pkt_write(pkt, NULL,
					MAE_BASE + MAE_REG_MMFD_O_SCALE_0, 0x00000AA8,
					CMDQ_REG_MASK);
				cmdq_pkt_write(pkt, NULL, MAE_BASE + MAE_REG_H_SIZE0,
						0x280, CMDQ_REG_MASK);
				cmdq_pkt_write(pkt, NULL, MAE_BASE + MAE_REG_V_SIZE0,
						0x1E0, CMDQ_REG_MASK);
			} else if (core_sel == 1) {
				cmdq_pkt_write(pkt, NULL,
					MAE_BASE + MAE_REG_MMFD_O_SCALE_0, 0x000002AA,
					CMDQ_REG_MASK);
				cmdq_pkt_write(pkt, NULL, MAE_BASE + MAE_REG_H_SIZE0,
						0x280, CMDQ_REG_MASK);
				cmdq_pkt_write(pkt, NULL, MAE_BASE + MAE_REG_V_SIZE0,
						0x1E0, CMDQ_REG_MASK);
			} else if (core_sel == 2) {
				cmdq_pkt_write(pkt, NULL,
					MAE_BASE + MAE_REG_MMFD_O_SCALE_1, 0x000002AA,
					CMDQ_REG_MASK);
				cmdq_pkt_write(pkt, NULL, MAE_BASE + MAE_REG_H_SIZE1,
						0x280, CMDQ_REG_MASK);
				cmdq_pkt_write(pkt, NULL, MAE_BASE + MAE_REG_V_SIZE1,
						0x1E0, CMDQ_REG_MASK);
			} else if (core_sel == 3) {
				cmdq_pkt_write(pkt, NULL,
					MAE_BASE + MAE_REG_MMFD_O_SCALE_2, 0x00000AA8,
					CMDQ_REG_MASK);
				cmdq_pkt_write(pkt, NULL, MAE_BASE + MAE_REG_H_SIZE2,
						0x280, CMDQ_REG_MASK);
				cmdq_pkt_write(pkt, NULL, MAE_BASE + MAE_REG_V_SIZE2,
						0x1E0, CMDQ_REG_MASK);
			}

			break;
		case FD_V1_IPN:
			if (core_sel == 0) {
				mtk_mae_fd_ipn_640_480_pat(pkt);
			} else if (core_sel == 1) {
				cmdq_pkt_write(pkt, NULL,
					MAE_BASE + MAE_REG_MMFD_O_SCALE_0, 0x0200,
					CMDQ_REG_MASK);
				cmdq_pkt_write(pkt, NULL, MAE_BASE + MAE_REG_H_SIZE0,
						0x01E0, CMDQ_REG_MASK);
				cmdq_pkt_write(pkt, NULL, MAE_BASE + MAE_REG_V_SIZE0,
						0x0168, CMDQ_REG_MASK);
				cmdq_pkt_write(pkt, NULL, MAE_BASE + MAE_REG_SCORE_TH0,
						0x000A, CMDQ_REG_MASK);
				cmdq_pkt_write(pkt, NULL, MAE_BASE + MAE_REG_H_MAX0,
						0x01F4, CMDQ_REG_MASK);
				cmdq_pkt_write(pkt, NULL, MAE_BASE + MAE_REG_V_MAX0,
						0x01F4, CMDQ_REG_MASK);
			} else if (core_sel == 2) {
				mtk_mae_fd_ipn_240_180_pat(pkt);
			} else if (core_sel == 3) {
				mtk_mae_fd_ipn_120_90_pat(pkt);
			}

			break;
		case FD_V1_FPN:
			// mtk_mae_fd_fpn_480_360_pat(pkt);
			break;
		case ATTR_V0:
			mtk_mae_attr_v0_pat(pkt);
			cmdq_pkt_write(pkt, NULL,
				MAE_BASE + MAE_REG_MMFD_O_SCALE_0, 0x00000AA8,
				CMDQ_REG_MASK);
			break;
		case FAC_V1:
			cmdq_pkt_write(pkt, NULL,
				MAE_BASE + MAE_REG_MMFD_O_SCALE_0, 0x00000AA8,
				CMDQ_REG_MASK);
			break;
		default:
			break;
		}
	} else {
		// formula from algo
		MAE_CMDQ_WRITE_REG(pkt, MAE_REG_X_OFFSET_0 + COMMON_REG_SIZE * core_offset,
			(image->enRoi) ? (image->roi.x1) : 0);
		mae_dev_dbg(mae_dev->dev, "[%s] 0x%x = 0x%x",
			__func__, MAE_BASE + MAE_REG_X_OFFSET_0 + COMMON_REG_SIZE * core_offset,
			(image->enRoi) ? (image->roi.x1) : 0);

		MAE_CMDQ_WRITE_REG(pkt, MAE_REG_Y_OFFSET_0 + COMMON_REG_SIZE * core_offset,
			(image->enRoi) ? (image->roi.y1) : 0);
		mae_dev_dbg(mae_dev->dev, "[%s] 0x%x = 0x%x",
			__func__, MAE_BASE + MAE_REG_Y_OFFSET_0 + COMMON_REG_SIZE * core_offset,
			(image->enRoi) ? (image->roi.y1) : 0);

		MAE_CMDQ_WRITE_REG(pkt, MAE_REG_MMFD_O_SCALE_0 + COMMON_REG_SIZE * core_offset,
			(image->enRoi) ?
			ROUND(((image->roi.x2 - image->roi.x1 + 1) << 9), image->resizeWidth) :
			ROUND((image->imgWidth << 9), image->resizeWidth));
		mae_dev_dbg(mae_dev->dev, "[%s] 0x%x = 0x%x",
			__func__, MAE_BASE + MAE_REG_MMFD_O_SCALE_0 + COMMON_REG_SIZE * core_offset,
			(image->enRoi) ?
			ROUND(((image->roi.x2 - image->roi.x1 + 1) << 9), image->resizeWidth) :
			ROUND((image->imgWidth << 9), image->resizeWidth));

		MAE_CMDQ_WRITE_REG(pkt, MAE_REG_H_SIZE0 + COMMON_REG_SIZE * core_offset,
			(uint32_t)image->imgWidth);
		mae_dev_dbg(mae_dev->dev, "[%s] 0x%x = 0x%x",
			__func__, MAE_BASE + MAE_REG_H_SIZE0 + COMMON_REG_SIZE * core_offset,
			(uint32_t)image->imgWidth);

		MAE_CMDQ_WRITE_REG(pkt, MAE_REG_V_SIZE0 + COMMON_REG_SIZE * core_offset,
			(uint32_t)image->imgHeight);
		mae_dev_dbg(mae_dev->dev, "[%s] 0x%x = 0x%x",
			__func__, MAE_BASE + MAE_REG_V_SIZE0 + COMMON_REG_SIZE * core_offset,
			(uint32_t)image->imgHeight);

		if (mode == FD_V1_IPN || mode == FD_V1_FPN) {
			MAE_CMDQ_WRITE_REG(pkt, MAE_REG_H_MIN0 + COMMON_REG_SIZE * core_offset, 0x0);
			MAE_CMDQ_WRITE_REG(pkt, MAE_REG_V_MIN0 + COMMON_REG_SIZE * core_offset, 0x0);
			MAE_CMDQ_WRITE_REG(pkt, MAE_REG_H_MAX0 + COMMON_REG_SIZE * core_offset,
					(uint32_t)image->imgWidth);
			MAE_CMDQ_WRITE_REG(pkt, MAE_REG_V_MAX0 + COMMON_REG_SIZE * core_offset,
					(uint32_t)image->imgHeight);
			MAE_CMDQ_WRITE_REG(pkt, MAE_REG_SCORE_TH0 + COMMON_REG_SIZE * core_offset, 0x11);
		}
	}
}

static void mtk_mae_config_aiseg_post(struct mtk_mae_dev *mae_dev,
				struct EnqueParam *param,
				struct cmdq_pkt *pkt)
{
	uint32_t i, j;
	uint32_t reg_addr;

	for (i = 0; i < AISEG_POP_GROUP_SIZE; i++) {
		reg_addr = REG_01A0_RSZ1 + i * RSZ_BASE_ADDR_OFFSET;

		for (j = 0; j < SEMANTIC_MERGE_NUM / 2; j++) {
			MAE_CMDQ_WRITE_REG(pkt, reg_addr,
				(param->semanticMerge[i][2*j+1] << 8) + param->semanticMerge[i][2*j]);
			reg_addr += COMMON_REG_SIZE;
		}

		for (j = 0; j < PERSON_MERGE_NUM / 2; j++) {
			MAE_CMDQ_WRITE_REG(pkt, reg_addr,
				(param->personMerge[i][2*j+1] << 8) + param->personMerge[i][2*j]);
			reg_addr += COMMON_REG_SIZE;
		}

		for (j = 0; j < MERGE_CONFIDENCE_NUM / 2; j++) {
			MAE_CMDQ_WRITE_REG(pkt, reg_addr,
				(param->mergeConfidence[i][2*j+1] << 8) + param->mergeConfidence[i][2*j]);
			reg_addr += COMMON_REG_SIZE;
		}
	}
}

static void mtk_mae_config_hw(struct mtk_mae_dev *mae_dev, int idx)
{
	struct EnqueParam *param =
		(struct EnqueParam*)mae_dev->map_table->param_dmabuf_info[idx].kva;
	uint32_t rsz_offset = 0;

	mae_dev_dbg(mae_dev->dev, "%s+", __func__);

	// ddren set should be 100ns earlier than sw trig
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_TRIG_RST_CTRL, 0x0100);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_TRIG_RST_CTRL, 0x0000);

	mae_dev_dbg(mae_dev->dev, "adb: mae_fd_post_on(%d)\n", mae_fd_post_on);
	mae_dev_dbg(mae_dev->dev, "adb: mae_trigger_cmdq_timeout(%d)\n", mae_trigger_cmdq_timeout);
	mae_dev_dbg(mae_dev->dev, "adb: fld_debug_1(%d)\n", fld_debug_1);
	mae_dev_dbg(mae_dev->dev, "adb: rsz_debug_on(%d)\n", rsz_debug_on);
	mae_dev_dbg(mae_dev->dev, "adb: mae_dbf_on(%d)\n", mae_dbf_on);
	mae_dev_dbg(mae_dev->dev, "adb: set_default_value(%d)\n", set_default_value);
	mae_dev_dbg(mae_dev->dev, "adb: fld_reset_en(%d)\n", fld_reset_en);
	mae_dev_dbg(mae_dev->dev, "adb: aiseg_lut_en(%d)\n", aiseg_lut_en);
	mae_dev_dbg(mae_dev->dev, "adb: fac_v1_pat_en(%d)\n", fac_v1_pat_en);
	mae_dev_dbg(mae_dev->dev, "adb: cmdq_polling_en(%d)\n", cmdq_polling_en);

	if (param->image[0].srcImgFmt == NV12 &&
		param->image[0].imgHeight % 2 != 0) {
		mae_dev_info(mae_dev->dev, "imgHeight(%d) should be 2 pixel aligned in NV12",
						param->image[0].imgHeight);
		return;
	}

	if (param->image[0].imgWidth % 16 != 0) {
		mae_dev_info(mae_dev->dev, "imgWidth(%d) should be 16 pixel aligned",
						param->image[0].imgWidth);
		return;
	}

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_00_R, param->image[0].imgWidth);

	if (param->image[0].enRoi) {
		// reg_outer_src_hsize
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_OUTER_SRC_HSIZE_00,
			DIV_CEIL_POS(MIN(param->image[0].roi.x2, param->image[0].imgWidth), 16) * 16 -
			MIN(MAX(param->image[0].roi.x1, 0), param->image[0].imgWidth) / 16 * 16);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_OUTER_SRC_VSIZE_00,
			param->image[0].roi.y2 - param->image[0].roi.y1);
	} else {
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_OUTER_SRC_HSIZE_00, param->image[0].imgWidth);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_OUTER_SRC_VSIZE_00, param->image[0].imgHeight);
	}

	// MAE_TO_DO: multiple models
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_SYS_SHADOW_CTRL, 0x0000);	// [6] 0: mae done event

	switch (param->maeMode) {
	case FD_V0:
	case FD_V1_IPN:
		if (param->image[0].enRoi) {
			mae_dev->core_sel[idx] = mtk_mae_fd_core_sel(mae_dev,
					param->image[0].resizeWidth,
					param->image[0].resizeWidth *
					(param->image[0].roi.y2 - param->image[0].roi.y1 + 1) /
					(param->image[0].roi.x2 - param->image[0].roi.x1 + 1));
		} else {
			mae_dev->core_sel[idx] = mtk_mae_fd_core_sel(mae_dev,
					param->image[0].resizeWidth,
					param->image[0].resizeWidth *
					param->image[0].imgHeight / param->image[0].imgWidth);
		}

		if (mae_dev->core_sel[idx] < 0)
			return;

		switch (mae_dev->core_sel[idx]) {
		case 0:
		case 1:
			rsz_offset = 0;
			break;
		case 2:
			rsz_offset = 1 * RSZ_BASE_ADDR_OFFSET;
			break;
		case 3:
			rsz_offset = 2 * RSZ_BASE_ADDR_OFFSET;
			break;
		default:
			break;
		}

		if (param->image[0].srcImgFmt == NV12) {
			MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_MEM_CONFIG, 0x000C);
			MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_RESERVE, 0x0007);
		} else {
			mae_dev_info(mae_dev->dev, "wrong img fmt(%d) for fd mode(%d)\n",
				param->image[0].srcImgFmt, param->maeMode);
			return;
		}

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_COEF_ROTATE, param->fdInputDegree);

		mtk_mae_fd_post(mae_dev, param, mae_dev->pkt[idx], mae_dev->core_sel[idx], param->maeMode);

		mtk_mae_config_rsz(mae_dev, param, mae_dev->pkt[idx], rsz_offset, idx);

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0184_RSZ1 + rsz_offset,
				0xb);

		break;
	case FD_V1_FPN:
		rsz_offset = 0;

		if (param->image[0].srcImgFmt == NV12) {
			MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_MEM_CONFIG, 0x000C);
			MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_RESERVE, 0x0007);
		} else {
			mae_dev_info(mae_dev->dev, "wrong img fmt(%d) for fd mode(%d)\n",
				param->image[0].srcImgFmt, param->maeMode);
			return;
		}

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_COEF_ROTATE, param->fdInputDegree);

		mtk_mae_fd_post(mae_dev, param, mae_dev->pkt[idx], 0, param->maeMode);

		mtk_mae_config_rsz(mae_dev, param, mae_dev->pkt[idx], rsz_offset, idx);

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0184_RSZ1 + rsz_offset,
				0xb);

		if (mae_fd_post_on == 0)
			mtk_mae_fd_fpn_480_360_pat(mae_dev->pkt[idx]);
		break;
	case ATTR_V0:
	case FAC_V1:
		if (param->image[0].srcImgFmt == NV12) {
			MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_MEM_CONFIG, 0x000C);
			MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_RESERVE, 0x0007);
		} else {
			mae_dev_info(mae_dev->dev,
				"wrong img fmt(%d) for attr_v0\n", param->image[0].srcImgFmt);
			return;
		}

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_COEF_ROTATE, param->attrInputDegree[idx]);

		mtk_mae_fd_post(mae_dev, param, mae_dev->pkt[idx], 0, param->maeMode);

		// force rsz_offset
		rsz_offset = 0;

		mtk_mae_config_rsz(mae_dev, param, mae_dev->pkt[idx], rsz_offset, idx);

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
			REG_0184_RSZ1 + rsz_offset,
				0xb);

		if (fac_v1_pat_en && param->maeMode == FAC_V1)
			mtk_mae_fac_v1_pat(mae_dev->pkt[idx]);

		break;
	case AISEG:
		if (param->image[0].srcImgFmt == YUYV) {
			MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_MEM_CONFIG, 0x0005);
			MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_RESERVE, 0x0006);
		} else {
			mae_dev_info(mae_dev->dev,
				"wrong img fmt(%d) for attr_v0\n", param->image[0].srcImgFmt);
			return;
		}

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_00_W, 0x000c);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_01_W, 0x0008);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_02_W, 0x0004);

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_03_W, 0x0008);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_04_W, 0x0008);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_05_W, 0x0008);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_06_W, 0x0008);

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_07_W, 0x0008);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_08_W, 0x0008);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_09_W, 0x0008);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_10_W, 0x0008);

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_COEF_ROTATE, param->aisegInputDegree);

		mtk_mae_config_aiseg_post(mae_dev, param, mae_dev->pkt[idx]);

		if (aiseg_lut_en)
			mtk_mae_aiseg_pat(mae_dev->pkt[idx]);

		break;
	default:
		break;
	}

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_0004_MAE_RDMA_5, 0x6221);
	if (mae_dbf_on == 1)
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_IRQ_DDREN_CMDQ_CTRL, 0x2200);
	else
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_IRQ_DDREN_CMDQ_CTRL, 0x2202);

	if (mae_trigger_cmdq_timeout == 0)
		// sw trigger
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_TRIG_RST_CTRL, 0x8000);

	// ddren clear
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_TRIG_RST_CTRL, 0x0100);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_TRIG_RST_CTRL, 0x0000);

	if (cmdq_polling_en)
		cmdq_pkt_poll_sleep(mae_dev->pkt[idx], MAE_IRQ_STATUS_VALUE,
			MAE_BASE + MAE_IRQ_CTRL1, MAE_IRQ_MASK);
	else
		cmdq_pkt_wfe(mae_dev->pkt[idx], mae_dev->mae_event_id);

	cmdq_pkt_flush_async(mae_dev->pkt[idx], MAECmdqCB, (void *)mae_dev);

	// DEBUG_ONLY
	mae_dev_dbg(mae_dev->dev, "%s-", __func__);
}

static void mtk_mae_reg_dump_to_buffer(struct mtk_mae_dev *mae_dev)
{
	uint8_t *debug_buffer = (uint8_t *)mae_dev->map_table->debug_dmabuf_info[0].kva;
	uint32_t i;

	mae_dev_info(mae_dev->dev, "%s +\n", __func__);

	// 21 line
	for (i = 0; i < FDVT_LEN; i += 0x10) {
		snprintf(debug_buffer, DEBUG_BUFFER_LINE_LEN,
				"\n[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				MAE_BASE + FDVT_START + i,
				(uint32_t)readl(mae_dev->mae_base + FDVT_START + i),
				(uint32_t)readl(mae_dev->mae_base + FDVT_START + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + FDVT_START + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + FDVT_START + i + 0xc));
		mae_dev_info(mae_dev->dev,
				"[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x\n",
				MAE_BASE + FDVT_START + i,
				(uint32_t)readl(mae_dev->mae_base + FDVT_START + i),
				(uint32_t)readl(mae_dev->mae_base + FDVT_START + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + FDVT_START + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + FDVT_START + i + 0xc));
		debug_buffer += DEBUG_BUFFER_LINE_LEN;
	}

	// 48 line
	for (i = 0; i < MAE_CTRL_CENTER_LEN; i += 0x10) {
		snprintf(debug_buffer, DEBUG_BUFFER_LINE_LEN,
				"\n[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				MAE_BASE + MAE_CTRL_CENTER_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + MAE_CTRL_CENTER_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + MAE_CTRL_CENTER_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + MAE_CTRL_CENTER_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + MAE_CTRL_CENTER_BASE + i + 0xc));
		mae_dev_info(mae_dev->dev,
				"[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x\n",
				MAE_BASE + MAE_CTRL_CENTER_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + MAE_CTRL_CENTER_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + MAE_CTRL_CENTER_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + MAE_CTRL_CENTER_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + MAE_CTRL_CENTER_BASE + i + 0xc));
		debug_buffer += DEBUG_BUFFER_LINE_LEN;
	}

	// 31 line
	for (i = 0; i < MAE_RSZ0_LEN; i += 0x10) {
		snprintf(debug_buffer, DEBUG_BUFFER_LINE_LEN,
				"\n[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				MAE_BASE + MAE_RSZ0_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + MAE_RSZ0_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + MAE_RSZ0_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + MAE_RSZ0_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + MAE_RSZ0_BASE + i + 0xc));
		mae_dev_info(mae_dev->dev,
				"[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x\n",
				MAE_BASE + MAE_RSZ0_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + MAE_RSZ0_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + MAE_RSZ0_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + MAE_RSZ0_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + MAE_RSZ0_BASE + i + 0xc));
		debug_buffer += DEBUG_BUFFER_LINE_LEN;
	}

	// 24 line
	for (i = 0; i < MAE_CMP_LEN; i += 0x10) {
		snprintf(debug_buffer, DEBUG_BUFFER_LINE_LEN,
				"\n[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				MAE_BASE + MAE_CMP_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + MAE_CMP_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + MAE_CMP_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + MAE_CMP_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + MAE_CMP_BASE + i + 0xc));
		mae_dev_info(mae_dev->dev,
				"[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x\n",
				MAE_BASE + MAE_CMP_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + MAE_CMP_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + MAE_CMP_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + MAE_CMP_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + MAE_CMP_BASE + i + 0xc));
		debug_buffer += DEBUG_BUFFER_LINE_LEN;
	}

	// 31 line
	for (i = 0; i < RSZ1_BASE_LEN; i += 0x10) {
		snprintf(debug_buffer, DEBUG_BUFFER_LINE_LEN,
				"\n[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				MAE_BASE + RSZ1_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + RSZ1_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + RSZ1_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + RSZ1_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + RSZ1_BASE + i + 0xc));
		mae_dev_info(mae_dev->dev,
				"[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x\n",
				MAE_BASE + RSZ1_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + RSZ1_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + RSZ1_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + RSZ1_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + RSZ1_BASE + i + 0xc));
		debug_buffer += DEBUG_BUFFER_LINE_LEN;
	}

	// 31 line
	for (i = 0; i < RSZ2_BASE_LEN; i += 0x10) {
		snprintf(debug_buffer, DEBUG_BUFFER_LINE_LEN,
				"\n[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				MAE_BASE + RSZ2_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + RSZ2_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + RSZ2_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + RSZ2_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + RSZ2_BASE + i + 0xc));
		mae_dev_info(mae_dev->dev,
				"[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x\n",
				MAE_BASE + RSZ2_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + RSZ2_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + RSZ2_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + RSZ2_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + RSZ2_BASE + i + 0xc));
		debug_buffer += DEBUG_BUFFER_LINE_LEN;
	}

	// 31 line
	for (i = 0; i < RSZ3_BASE_LEN; i += 0x10) {
		snprintf(debug_buffer, DEBUG_BUFFER_LINE_LEN,
				"\n[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				MAE_BASE + RSZ3_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + RSZ3_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + RSZ3_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + RSZ3_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + RSZ3_BASE + i + 0xc));
		mae_dev_info(mae_dev->dev,
				"[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x\n",
				MAE_BASE + RSZ3_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + RSZ3_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + RSZ3_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + RSZ3_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + RSZ3_BASE + i + 0xc));
		debug_buffer += DEBUG_BUFFER_LINE_LEN;
	}

	// 1 line
	snprintf(debug_buffer, DEBUG_BUFFER_LINE_LEN,
			"\n[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
			MAE_BASE + MAE_MAISR_APB_BASE,
			(uint32_t)readl(mae_dev->mae_base + MAE_MAISR_APB_BASE),
			(uint32_t)readl(mae_dev->mae_base + MAE_MAISR_APB_BASE + 0x4),
			(uint32_t)readl(mae_dev->mae_base + MAE_MAISR_APB_BASE + 0x8),
			(uint32_t)readl(mae_dev->mae_base + MAE_MAISR_APB_BASE + 0xc));
	mae_dev_info(mae_dev->dev,
			"[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x\n",
			MAE_BASE + MAE_MAISR_APB_BASE,
			(uint32_t)readl(mae_dev->mae_base + MAE_MAISR_APB_BASE),
			(uint32_t)readl(mae_dev->mae_base + MAE_MAISR_APB_BASE + 0x4),
			(uint32_t)readl(mae_dev->mae_base + MAE_MAISR_APB_BASE + 0x8),
			(uint32_t)readl(mae_dev->mae_base + MAE_MAISR_APB_BASE + 0xc));
	debug_buffer += DEBUG_BUFFER_LINE_LEN;

	// 17 line
	for (i = 0; i < MMFD_POST_LEN; i += 0x10) {
		snprintf(debug_buffer, DEBUG_BUFFER_LINE_LEN,
				"\n[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				MAE_BASE + MMFD_POST_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + MMFD_POST_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + MMFD_POST_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + MMFD_POST_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + MMFD_POST_BASE + i + 0xc));
		mae_dev_info(mae_dev->dev,
				"[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x\n",
				MAE_BASE + MMFD_POST_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + MMFD_POST_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + MMFD_POST_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + MMFD_POST_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + MMFD_POST_BASE + i + 0xc));
		debug_buffer += DEBUG_BUFFER_LINE_LEN;
	}

	// 22 line
	for (i = 0; i < MAE_DRV_W_LEN; i += 0x10) {
		snprintf(debug_buffer, DEBUG_BUFFER_LINE_LEN,
				"\n[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				MAE_BASE + MAE_DRV_W_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_W_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_W_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_W_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_W_BASE + i + 0xc));
		mae_dev_info(mae_dev->dev,
				"[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x\n",
				MAE_BASE + MAE_DRV_W_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_W_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_W_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_W_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_W_BASE + i + 0xc));
		debug_buffer += DEBUG_BUFFER_LINE_LEN;
	}

	// 31 line
	for (i = 0; i < MAE_DRV_R_LEN; i += 0x10) {
		snprintf(debug_buffer, DEBUG_BUFFER_LINE_LEN,
				"\n[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
				MAE_BASE + MAE_DRV_R_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_R_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_R_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_R_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_R_BASE + i + 0xc));
		mae_dev_info(mae_dev->dev,
				"[0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x\n",
				MAE_BASE + MAE_DRV_R_BASE + i,
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_R_BASE + i),
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_R_BASE + i + 0x4),
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_R_BASE + i + 0x8),
				(uint32_t)readl(mae_dev->mae_base + MAE_DRV_R_BASE + i + 0xc));
		debug_buffer += DEBUG_BUFFER_LINE_LEN;
	}
}

static void mtk_mae_dump_reg(struct mtk_mae_dev *mae_dev)
{
	struct EnqueParam *param =
		(struct EnqueParam *)mae_dev->map_table->param_dmabuf_info[0].kva;
	uint32_t i, j;
	uint32_t reg_addr;

	mae_dev_info(mae_dev->dev, "%s +\n", __func__);

	mae_dev_info(mae_dev->dev, "Dump user setting\n");
	mae_dev_info(mae_dev->dev, "user(%d) Max W/H(%d/%d) Sec(%d) FD/FAC Model SEL(%d/%d)\n",
		param->user, param->imgMaxWidth, param->imgMaxHeight ,param->isSecure,
		param->FDModelSel, param->FACModelSel);

	mae_dev_info(mae_dev->dev, "py num(%d) rot(%d) mode(%d) reqNum(%d)\n",
		param->pyramidNumber, param->fdInputDegree, param->maeMode ,param->requestNum);

	mae_dev_info(mae_dev->dev, "attr py num(%d)\n", param->attrFaceNumber);
	for (i = 0; i < param->attrFaceNumber; i++)
		mae_dev_info(mae_dev->dev, "attr rot(%d)\n", param->attrInputDegree[i]);

	if (param->maeMode == FLD_V0) {
		mae_dev_info(mae_dev->dev, "fmt(%d) fldFaceNum(%d) img W/H(%d/%d)\n",
			param->image[0].srcImgFmt, param->fldConfig.fldFaceNum,
			param->image[0].imgWidth, param->image[0].imgHeight);
		for (i = 0; i < MAX_FLD_V0_FACE_NUM; i++) {
			mae_dev_info(mae_dev->dev, "roi(%d,%d->%d,%d), rip(%d), rop(%d)\n",
				param->fldConfig.fldSetting[i].roi.x1, param->fldConfig.fldSetting[i].roi.y1,
				param->fldConfig.fldSetting[i].roi.x2, param->fldConfig.fldSetting[i].roi.y2,
				param->fldConfig.fldSetting[i].rip, param->fldConfig.fldSetting[i].rop);
		}
	} else {
		for (i = 0; i < param->pyramidNumber; i++) {
			mae_dev_info(mae_dev->dev, "fmt(%d), img W/H(%d/%d), roi(%d)(%d,%d->%d,%d), rsz W/H(%d/%d)\n",
				param->image[i].srcImgFmt, param->image[i].imgWidth, param->image[i].imgHeight,
				param->image[i].enRoi, param->image[i].roi.x1, param->image[i].roi.y1,
				param->image[i].roi.x2, param->image[i].roi.y2, param->image[i].resizeWidth,
				param->image[i].resizeHeight);
			mae_dev_info(mae_dev->dev, "pad(%d) (l,r,d,u)=(%d,%d->%d,%d)", param->image[i].enPadding,
				param->image[i].padding.left, param->image[i].padding.right,
				param->image[i].padding.down, param->image[i].padding.up);
		}
	}

	mtk_mae_reg_dump_to_buffer(mae_dev);

	mae_dev_info(mae_dev->dev, "Dump reg\n");
	if (param->maeMode == FLD_V0) {
		MAE_DUMP_REG(FLD_IMG_BASE_ADDR);
		MAE_DUMP_REG(FLD_MS_BASE_ADDR);
		MAE_DUMP_REG(FLD_FP_BASE_ADDR);
		MAE_DUMP_REG(FLD_TR_BASE_ADDR);
		MAE_DUMP_REG(FLD_SH_BASE_ADDR);
		MAE_DUMP_REG(FLD_CV_BASE_ADDR);
		MAE_DUMP_REG(FLD_BS_BASE_ADDR);
		MAE_DUMP_REG(FLD_PP_BASE_ADDR);
		MAE_DUMP_REG(FLD_FP_FORT_OFST);
		MAE_DUMP_REG(FLD_TR_FORT_OFST);
		MAE_DUMP_REG(FLD_SH_FORT_OFST);
		MAE_DUMP_REG(FLD_CV_FORT_OFST);

		MAE_DUMP_REG(FLD_FACE_0_INFO_0);
		MAE_DUMP_REG(FLD_FACE_0_INFO_1);
		MAE_DUMP_REG(FLD_FACE_1_INFO_0);
		MAE_DUMP_REG(FLD_FACE_1_INFO_1);
		MAE_DUMP_REG(FLD_FACE_2_INFO_0);
		MAE_DUMP_REG(FLD_FACE_2_INFO_1);
		MAE_DUMP_REG(FLD_FACE_3_INFO_0);
		MAE_DUMP_REG(FLD_FACE_3_INFO_1);
		MAE_DUMP_REG(FLD_FACE_4_INFO_0);
		MAE_DUMP_REG(FLD_FACE_4_INFO_1);
		MAE_DUMP_REG(FLD_FACE_5_INFO_0);
		MAE_DUMP_REG(FLD_FACE_5_INFO_1);
		MAE_DUMP_REG(FLD_FACE_6_INFO_0);
		MAE_DUMP_REG(FLD_FACE_6_INFO_1);
		MAE_DUMP_REG(FLD_FACE_7_INFO_0);
		MAE_DUMP_REG(FLD_FACE_7_INFO_1);
		MAE_DUMP_REG(FLD_FACE_8_INFO_0);
		MAE_DUMP_REG(FLD_FACE_8_INFO_1);
		MAE_DUMP_REG(FLD_FACE_9_INFO_0);
		MAE_DUMP_REG(FLD_FACE_9_INFO_1);
		MAE_DUMP_REG(FLD_FACE_10_INFO_0);
		MAE_DUMP_REG(FLD_FACE_10_INFO_1);
		MAE_DUMP_REG(FLD_FACE_11_INFO_0);
		MAE_DUMP_REG(FLD_FACE_11_INFO_1);
		MAE_DUMP_REG(FLD_FACE_12_INFO_0);
		MAE_DUMP_REG(FLD_FACE_12_INFO_1);
		MAE_DUMP_REG(FLD_FACE_13_INFO_0);
		MAE_DUMP_REG(FLD_FACE_13_INFO_1);
		MAE_DUMP_REG(FLD_FACE_14_INFO_0);
		MAE_DUMP_REG(FLD_FACE_14_INFO_1);

		MAE_DUMP_REG(FLD_NUM_CONFIG_0);
		MAE_DUMP_REG(FLD_FACE_NUM);

		MAE_DUMP_REG(FLD_PCA_MEAN_SCALE_0);
		MAE_DUMP_REG(FLD_PCA_MEAN_SCALE_1);
		MAE_DUMP_REG(FLD_PCA_MEAN_SCALE_2);
		MAE_DUMP_REG(FLD_PCA_MEAN_SCALE_3);
		MAE_DUMP_REG(FLD_PCA_MEAN_SCALE_4);
		MAE_DUMP_REG(FLD_PCA_MEAN_SCALE_5);
		MAE_DUMP_REG(FLD_PCA_MEAN_SCALE_6);
		MAE_DUMP_REG(FLD_PCA_VEC_0);
		MAE_DUMP_REG(FLD_PCA_VEC_1);
		MAE_DUMP_REG(FLD_PCA_VEC_2);
		MAE_DUMP_REG(FLD_PCA_VEC_3);
		MAE_DUMP_REG(FLD_PCA_VEC_4);
		MAE_DUMP_REG(FLD_PCA_VEC_5);
		MAE_DUMP_REG(FLD_PCA_VEC_6);
		MAE_DUMP_REG(FLD_CV_BIAS_FR_0);
		MAE_DUMP_REG(FLD_CV_BIAS_PF_0);
		MAE_DUMP_REG(FLD_CV_RANGE_FR_0);
		MAE_DUMP_REG(FLD_CV_RANGE_FR_1);
		MAE_DUMP_REG(FLD_CV_RANGE_PF_0);
		MAE_DUMP_REG(FLD_CV_RANGE_PF_1);
		MAE_DUMP_REG(FLD_PP_COEF);
		MAE_DUMP_REG(FLD_SRC_SIZE);
		MAE_DUMP_REG(FLD_CMDQ_SRC_SIZE);
		MAE_DUMP_REG(FLD_SRC_PITCH);
		MAE_DUMP_REG(FLD_BS_CONFIG0);
		MAE_DUMP_REG(FLD_BS_CONFIG1);
		MAE_DUMP_REG(FLD_BS_CONFIG2);
	} else {
		mae_dev_info(mae_dev->dev, "Dump MAE_RDMA_5\n");
		MAE_DUMP_REG(MAE_REG_0004_MAE_RDMA_5);

		mae_dev_info(mae_dev->dev, "Dump RSZ1_BASE\n");
		for (i = 0; i < RSZ_NUM; i++) {
			MAE_DUMP_REG(REG_0004_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0008_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_000C_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0010_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_001C_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0020_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0024_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0028_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_002C_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0034_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_005C_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0060_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0064_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0068_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0080_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0084_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_00A0_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_00A4_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_00A8_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_00AC_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_00C0_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_00C4_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_00C8_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_00CC_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_00D0_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_00D4_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_00D8_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0104_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0108_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_010C_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0110_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0114_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0118_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_011C_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0120_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0180_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
			MAE_DUMP_REG(REG_0184_RSZ1 + i * RSZ_BASE_ADDR_OFFSET);
		}

		MAE_DUMP_REG(MAE_COEF_ROTATE);

		mae_dev_info(mae_dev->dev, "Dump MMFD_POST\n");
		MAE_DUMP_REG(MAE_REG_X_OFFSET_0);
		MAE_DUMP_REG(MAE_REG_X_OFFSET_1);
		MAE_DUMP_REG(MAE_REG_X_OFFSET_2);
		MAE_DUMP_REG(MAE_REG_Y_OFFSET_0);
		MAE_DUMP_REG(MAE_REG_Y_OFFSET_1);
		MAE_DUMP_REG(MAE_REG_Y_OFFSET_2);

		MAE_DUMP_REG(MAE_REG_MMFD_O_SCALE_0);
		MAE_DUMP_REG(MAE_REG_MMFD_O_SCALE_1);
		MAE_DUMP_REG(MAE_REG_MMFD_O_SCALE_2);

		MAE_DUMP_REG(MAE_REG_H_SIZE0);
		MAE_DUMP_REG(MAE_REG_H_SIZE1);
		MAE_DUMP_REG(MAE_REG_H_SIZE2);
		MAE_DUMP_REG(MAE_REG_V_SIZE0);
		MAE_DUMP_REG(MAE_REG_V_SIZE1);
		MAE_DUMP_REG(MAE_REG_V_SIZE2);

		MAE_DUMP_REG(MAE_REG_H_MIN0);
		MAE_DUMP_REG(MAE_REG_H_MIN1);
		MAE_DUMP_REG(MAE_REG_H_MIN2);
		MAE_DUMP_REG(MAE_REG_V_MIN0);
		MAE_DUMP_REG(MAE_REG_V_MIN1);
		MAE_DUMP_REG(MAE_REG_V_MIN2);

		MAE_DUMP_REG(MAE_REG_SCORE_TH0);
		MAE_DUMP_REG(MAE_REG_SCORE_TH1);
		MAE_DUMP_REG(MAE_REG_SCORE_TH2);

		MAE_DUMP_REG(MAE_REG_H_MAX0);
		MAE_DUMP_REG(MAE_REG_H_MAX1);
		MAE_DUMP_REG(MAE_REG_H_MAX2);
		MAE_DUMP_REG(MAE_REG_V_MAX0);
		MAE_DUMP_REG(MAE_REG_V_MAX1);
		MAE_DUMP_REG(MAE_REG_V_MAX2);

		mae_dev_info(mae_dev->dev, "Dump MAE_DRV\n");
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_00_0_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_00_1_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_01_0_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_01_1_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_02_0_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_02_1_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_03_0_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_03_1_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_04_0_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_04_1_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_05_0_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_05_1_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_06_0_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_06_1_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_07_0_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_07_1_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_08_0_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_08_1_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_09_0_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_09_1_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_10_0_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_10_1_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_11_0_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_11_1_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_12_0_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_12_1_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_13_0_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_13_1_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_14_0_W);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_14_1_W);

		MAE_DUMP_REG(MAE_REG_INTRN_BASE_0_W);
		MAE_DUMP_REG(MAE_REG_INTRN_BASE_1_W);

		mae_dev_info(mae_dev->dev, "Dump MAE_DRV_R\n");
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_00_0_R);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_00_1_R);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_01_0_R);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_01_1_R);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_02_0_R);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE0_02_1_R);

		MAE_DUMP_REG(MAE_REG_INTRN_BASE_0_R);
		MAE_DUMP_REG(MAE_REG_INTRN_BASE_1_R);

		MAE_DUMP_REG(MAE_REG_EXTRN_BASE1_00_0_R);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE1_00_1_R);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE1_01_0_R);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE1_01_1_R);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE1_02_0_R);
		MAE_DUMP_REG(MAE_REG_EXTRN_BASE1_02_1_R);

		MAE_DUMP_REG(MAE_REG_EXTRN_LN_OFFSET_00_R);
		MAE_DUMP_REG(MAE_REG_EXTRN_LN_OFFSET_01_R);
		MAE_DUMP_REG(MAE_REG_EXTRN_LN_OFFSET_02_R);

		MAE_DUMP_REG(MAE_REG_EXTRN_MEM_CONFIG);

		MAE_DUMP_REG(MAE_REG_OUTER_SRC_HSIZE_00);
		MAE_DUMP_REG(MAE_REG_OUTER_SRC_VSIZE_00);
		MAE_DUMP_REG(MAE_REG_OUTER_SRC_HSIZE_01);
		MAE_DUMP_REG(MAE_REG_OUTER_SRC_VSIZE_01);
		MAE_DUMP_REG(MAE_REG_OUTER_SRC_HSIZE_02);
		MAE_DUMP_REG(MAE_REG_OUTER_SRC_VSIZE_02);

		MAE_DUMP_REG(MAE_REG_OUTER_CONFIG_BASE_00_0);
		MAE_DUMP_REG(MAE_REG_OUTER_CONFIG_BASE_00_1);
		MAE_DUMP_REG(MAE_REG_OUTER_CONFIG_BASE_01_0);
		MAE_DUMP_REG(MAE_REG_OUTER_CONFIG_BASE_01_1);
		MAE_DUMP_REG(MAE_REG_OUTER_CONFIG_BASE_02_0);
		MAE_DUMP_REG(MAE_REG_OUTER_CONFIG_BASE_02_1);

		MAE_DUMP_REG(MAE_REG_OUTER_CONFIG_OFFSET_00_0);
		MAE_DUMP_REG(MAE_REG_OUTER_CONFIG_OFFSET_00_1);
		MAE_DUMP_REG(MAE_REG_OUTER_CONFIG_OFFSET_01_0);
		MAE_DUMP_REG(MAE_REG_OUTER_CONFIG_OFFSET_01_1);
		MAE_DUMP_REG(MAE_REG_OUTER_CONFIG_OFFSET_02_0);
		MAE_DUMP_REG(MAE_REG_OUTER_CONFIG_OFFSET_02_1);

		MAE_DUMP_REG(MAE_REG_OUTER_CONFIG_SIZE_00);
		MAE_DUMP_REG(MAE_REG_OUTER_CONFIG_SIZE_01);
		MAE_DUMP_REG(MAE_REG_OUTER_CONFIG_SIZE_02);

		MAE_DUMP_REG(MAE_REG_OUTER_COEF_BASE_00_0);
		MAE_DUMP_REG(MAE_REG_OUTER_COEF_BASE_00_1);
		MAE_DUMP_REG(MAE_REG_OUTER_COEF_BASE_01_0);
		MAE_DUMP_REG(MAE_REG_OUTER_COEF_BASE_01_1);
		MAE_DUMP_REG(MAE_REG_OUTER_COEF_BASE_02_0);
		MAE_DUMP_REG(MAE_REG_OUTER_COEF_BASE_02_1);

		MAE_DUMP_REG(MAE_REG_OUTER_COEF_OFFSET_00_0);
		MAE_DUMP_REG(MAE_REG_OUTER_COEF_OFFSET_00_1);
		MAE_DUMP_REG(MAE_REG_OUTER_COEF_OFFSET_01_0);
		MAE_DUMP_REG(MAE_REG_OUTER_COEF_OFFSET_01_1);
		MAE_DUMP_REG(MAE_REG_OUTER_COEF_OFFSET_02_0);
		MAE_DUMP_REG(MAE_REG_OUTER_COEF_OFFSET_02_1);

		MAE_DUMP_REG(MAE_REG_OUTER_COEF_SIZE_00);
		MAE_DUMP_REG(MAE_REG_OUTER_COEF_SIZE_01);
		MAE_DUMP_REG(MAE_REG_OUTER_COEF_SIZE_02);

		MAE_DUMP_REG(MAE_REG_RESERVE);

		MAE_DUMP_REG(MAE_IRQ_CTRL1);

	}

	if (param->maeMode == AISEG) {
		mae_dev_info(mae_dev->dev, "Dump AISEG POST\n");

		for (i = 0; i < AISEG_POP_GROUP_SIZE; i++) {
			reg_addr = REG_01A0_RSZ1 + i * RSZ_BASE_ADDR_OFFSET;
			for (j = 0; j < SEMANTIC_MERGE_NUM / 2; j++) {
				MAE_DUMP_REG(reg_addr);
				reg_addr += COMMON_REG_SIZE;
			}

			for (j = 0; j < PERSON_MERGE_NUM / 2; j++) {
				MAE_DUMP_REG(reg_addr);
				reg_addr += COMMON_REG_SIZE;
			}

			for (j = 0; j < AISEG_POP_GROUP_SIZE / 2; j++) {
				MAE_DUMP_REG(reg_addr);
				reg_addr += COMMON_REG_SIZE;
			}
		}
	}

	mae_dev_info(mae_dev->dev, "%s -\n", __func__);
}

static void mtk_mae_irq_handle(struct mtk_mae_dev *mae_dev)
{
	writel(0x1, mae_dev->mae_base + MAE_IRQ_CTRL0);
	writel(0x0, mae_dev->mae_base + MAE_IRQ_CTRL0);
}

static void mtk_mae_config_fld_v0(struct mtk_mae_dev *mae_dev, int idx)
{
	struct EnqueParam *param =
		(struct EnqueParam*)mae_dev->map_table->param_dmabuf_info[idx].kva;
	uint64_t addr = 0;
	uint8_t i;

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FDVT_ENABLE, 0x4000000);	// [26] ddren set for v0 fld

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_SYS_SHADOW_CTRL, 0x00001040);	// [6] 1: fld done event

	// follow fld pattern setting which is necessary
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], 0x0600, 0x00000010);

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FDVT_DMA_CTL, 0x00011111);

	// fill fld base address
	addr = mae_dev->map_table->image_dmabuf_info[idx][0].pa;
	if (CHECK_BASE_ADDR(addr) || addr == 0)
		mae_dev_info(mae_dev->dev, "%s(0x%llx) is not %d-aligned or zero",
									"fld input image", addr, MAE_BASE_ADDR_ALIGN);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_IMG_BASE_ADDR, addr >> 4);
	mae_dev_dbg(mae_dev->dev, "%s(0x%llx)", "fld input image", addr);

	addr = mae_dev->map_table->coef_dmabuf_info[MODEL_TYPE_FLD_FAC_V0].pa +
			round_up(V0_ATTR_128_128_COEF_SIZE, MAE_BASE_ADDR_ALIGN);
	if (CHECK_BASE_ADDR(addr) || addr == 0)
		mae_dev_info(mae_dev->dev, "%s(0x%llx) is not %d-aligned or zero",
									"FLD_BS_BASE_ADDR", addr, MAE_BASE_ADDR_ALIGN);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_BS_BASE_ADDR, addr >> 4);	// 0x418

	addr += round_up(fdvt_fld_blink_weight_forest14_size, MAE_BASE_ADDR_ALIGN);
	if (CHECK_BASE_ADDR(addr) || addr == 0)
		mae_dev_info(mae_dev->dev, "%s(0x%llx) is not %d-aligned or zero",
									"FLD_FP_BASE_ADDR", addr, MAE_BASE_ADDR_ALIGN);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_FP_BASE_ADDR, addr >> 4);	// 0x408

	addr += round_up(fdvt_fld_fp_forest00_om45_size, MAE_BASE_ADDR_ALIGN) * 15;
	if (CHECK_BASE_ADDR(addr) || addr == 0)
		mae_dev_info(mae_dev->dev, "%s(0x%llx) is not %d-aligned or zero",
									"FLD_SH_BASE_ADDR", addr, MAE_BASE_ADDR_ALIGN);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_SH_BASE_ADDR, addr >> 4);	// 0x410

	addr += round_up(fdvt_fld_leafnode_forest00_size, MAE_BASE_ADDR_ALIGN) * 15;
	if (CHECK_BASE_ADDR(addr) || addr == 0)
		mae_dev_info(mae_dev->dev, "%s(0x%llx) is not %d-aligned or zero",
									"FLD_CV_BASE_ADDR", addr, MAE_BASE_ADDR_ALIGN);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_CV_BASE_ADDR, addr >> 4);	// 0x414

	addr += round_up(fdvt_fld_tree_forest00_cv_weight_size, MAE_BASE_ADDR_ALIGN) * 15;
	if (CHECK_BASE_ADDR(addr) || addr == 0)
		mae_dev_info(mae_dev->dev, "%s(0x%llx) is not %d-aligned or zero",
									"FLD_MS_BASE_ADDR", addr, MAE_BASE_ADDR_ALIGN);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_MS_BASE_ADDR, addr >> 4);	// 0x404

	addr += round_up(fdvt_fld_tree_forest00_init_shape_size, MAE_BASE_ADDR_ALIGN);
	if (CHECK_BASE_ADDR(addr) || addr == 0)
		mae_dev_info(mae_dev->dev, "%s(0x%llx) is not %d-aligned or zero",
									"FLD_TR_BASE_ADDR", addr, MAE_BASE_ADDR_ALIGN);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_TR_BASE_ADDR, addr >> 4);	// 0x40C

	addr = mae_dev->map_table->output_dmabuf_info[idx][0].pa;
	if (CHECK_BASE_ADDR(addr) || addr == 0)
		mae_dev_info(mae_dev->dev, "%s(0x%llx) is not %d-aligned or zero",
									"fld output image", addr, MAE_BASE_ADDR_ALIGN);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PP_BASE_ADDR, addr >> 4);
	mae_dev_dbg(mae_dev->dev, "%s(0x%llx)", "fld output image", addr);

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_FP_FORT_OFST,
		round_up(fdvt_fld_fp_forest00_om45_size, MAE_BASE_ADDR_ALIGN) >> 4);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_TR_FORT_OFST,
		round_up(fdvt_fld_tree_forest00_tree_node_size, MAE_BASE_ADDR_ALIGN) >> 4);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_SH_FORT_OFST,
		round_up(fdvt_fld_leafnode_forest00_size, MAE_BASE_ADDR_ALIGN) >> 4);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_CV_FORT_OFST,
		round_up(fdvt_fld_tree_forest00_cv_weight_size, MAE_BASE_ADDR_ALIGN) >> 4);


	for (i = 0; i < param->fldConfig.fldFaceNum; i++) {
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], fld_face_info_idx_0[i],
			(param->fldConfig.fldSetting[i].roi.y1 << 12) |
			param->fldConfig.fldSetting[i].roi.x1);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], fld_face_info_idx_1[i],
			(param->fldConfig.fldSetting[i].rop << 28) |
			(param->fldConfig.fldSetting[i].rip << 24) |
			(param->fldConfig.fldSetting[i].roi.y2 << 12) |
			param->fldConfig.fldSetting[i].roi.x2);
	}

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_FACE_NUM,
		(FLD_V0_POINT << 8) | param->fldConfig.fldFaceNum);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_SRC_SIZE,
		(param->image[0].imgHeight << 16) | param->image[0].imgWidth);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_SRC_PITCH,
		param->image[0].imgWidth);

	// MAE_TO_DO: maybe write once
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_NUM_CONFIG_0, 0x00b0c80f);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PCA_MEAN_SCALE_0, 0x6C004800);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PCA_MEAN_SCALE_1, 0x6c007c00);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PCA_MEAN_SCALE_2, 0x6c00b800);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PCA_MEAN_SCALE_3, 0x6c00ec00);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PCA_MEAN_SCALE_4, 0xb0009800);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PCA_MEAN_SCALE_5, 0xdc006800);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PCA_MEAN_SCALE_6, 0xdc00cc00);

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PCA_VEC_0, 0x00fdefd3);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PCA_VEC_1, 0x00fef095);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PCA_VEC_2, 0x00011095);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PCA_VEC_3, 0x00022fd3);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PCA_VEC_4, 0x000003e6);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PCA_VEC_5, 0x0000dfe9);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PCA_VEC_6, 0x00ff3fe9);

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_CV_BIAS_FR_0, 0x00000008);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_CV_BIAS_PF_0, 0x00000003);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_CV_RANGE_FR_0, 0x0000b835);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_CV_RANGE_FR_1, 0xFFFF5cba);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_CV_RANGE_PF_0, 0x00005ed5);
	if (fld_debug_1)
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_CV_RANGE_PF_1, 0xFFFF910d);
	else
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_CV_RANGE_PF_1, 0xFFFF310d);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_PP_COEF, 0xe8242184);

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_BS_CONFIG0, 0x00000001);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_BS_CONFIG1, 0x0000031e);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_BS_CONFIG2, 0xfffffcae);

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_IRQ_DDREN_CMDQ_CTRL, 0x2200);

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FDVT_INT_EN, 0x00000011);

	// sw trigger
	if (mae_trigger_cmdq_timeout == 0) {
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_TRIG_RST_CTRL, 0x8000);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FDVT_START, 0x00000011);  // [4] AIE_MODE, [0] START

	}

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FDVT_ENABLE, 0x8000000);	// [27] ddren clr for v0 fld

	if (fld_reset_en)
		mtk_mae_fld_reset(mae_dev);

	cmdq_pkt_wfe(mae_dev->pkt[idx], mae_dev->mae_event_id);
	cmdq_pkt_flush_async(mae_dev->pkt[idx], MAECmdqCB, (void *)mae_dev);
}

static void mtk_mae_get_fd_v0_result(struct mtk_mae_dev *mae_dev, int idx)
{
	struct EnqueParam *param =
		(struct EnqueParam *)mae_dev->map_table->param_dmabuf_info[idx].kva;
	uint32_t i;

	for (i = 0; i < param->pyramidNumber; i++) {
		switch (mae_dev->core_sel[idx]) {
		case 0:
		case 1:
			param->faceNum[i][0] =
				(uint32_t)readl(mae_dev->mae_base + MAE_REG_FACE_NUM0);
			mae_dev_dbg(mae_dev->dev, "%s + face_num(%d)\n", __func__, param->faceNum[i][0]);
			break;
		case 2:
			param->faceNum[i][0] =
				(uint32_t)readl(mae_dev->mae_base + MAE_REG_FACE_NUM1);
			mae_dev_dbg(mae_dev->dev, "%s + face_num(%d)\n", __func__, param->faceNum[i][0]);
			break;
		case 3:
			param->faceNum[i][0] =
				(uint32_t)readl(mae_dev->mae_base + MAE_REG_FACE_NUM2);
			mae_dev_dbg(mae_dev->dev, "%s + face_num(%d)\n", __func__, param->faceNum[i][0]);
			break;
		default:
			mae_dev_info(mae_dev->dev, "[%s] unsupport core_sel(%d)",
						__func__, mae_dev->core_sel[idx]);
			break;
		}
	}
}

static void mtk_mae_get_fd_v1_result(struct mtk_mae_dev *mae_dev, int idx)
{
	struct EnqueParam *param =
		(struct EnqueParam *)mae_dev->map_table->param_dmabuf_info[idx].kva;
	uint32_t i, j;
	uint32_t reg_base = 0;

	for (i = 0; i < param->pyramidNumber; i++) {
		switch (mae_dev->core_sel[idx]) {
		case 0:
		case 1:
			reg_base = MAE_REG_FACE_NUM0;
			break;
		case 2:
			reg_base = MAE_REG_FACE_NUM1;
			break;
		case 3:
			reg_base = MAE_REG_FACE_NUM2;
			break;
		default:
			mae_dev_info(mae_dev->dev, "[%s] unsupport core_sel(%d)",
						__func__, mae_dev->core_sel[idx]);
			reg_base = MAE_REG_FACE_NUM0;
			break;
		}

		for (j = 0; j < FD_V1_IPN_WDMA_NUM; j++)
			param->faceNum[i][j] =
					(uint32_t)readl(mae_dev->mae_base + reg_base + j * FACE_NUM_REG_OFFSET);
	}
}

static void mtk_mae_fld_reset(struct mtk_mae_dev *mae_dev)
{
	uint32_t value;

	value = (uint32_t)readl(mae_dev->mae_base + MAE_TRIG_RST_CTRL);
	writel(value | (0x1 << 12), mae_dev->mae_base + MAE_TRIG_RST_CTRL);

	value = (uint32_t)readl(mae_dev->mae_base + MAE_TRIG_RST_CTRL);
	writel(value & ~(0x1 << 12), mae_dev->mae_base + MAE_TRIG_RST_CTRL);

	value = (uint32_t)readl(mae_dev->mae_base + MAE_TRIG_RST_CTRL);
	writel(value | (0x1 << 13), mae_dev->mae_base + MAE_TRIG_RST_CTRL);

	value = (uint32_t)readl(mae_dev->mae_base + MAE_TRIG_RST_CTRL);
	writel(value & ~(0x1 << 13), mae_dev->mae_base + MAE_TRIG_RST_CTRL);
}

#if MAE_CMDQ_SEC_READY
static void mtk_mae_sec_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_mae_dev *mae_dev = (struct mtk_mae_dev *)data.data;

	mae_dev_info(mae_dev->dev, "MAE SEC CMDQ CB\n");
}

static void mtk_mae_sec_pkt_cb(struct cmdq_cb_data data)
{
	struct mtk_mae_dev *mae_dev = (struct mtk_mae_dev *)data.data;

	cmdq_pkt_destroy(mae_dev->sec_pkt);
	mae_dev->sec_pkt = NULL;
}

static void mtk_mae_secure_cmdq_init(struct mtk_mae_dev *mae_dev)
{
	mae_dev->sec_pkt = cmdq_pkt_create(mae_dev->mae_secure_clt);
	cmdq_sec_pkt_set_data(mae_dev->sec_pkt, 0, 0, CMDQ_SEC_DEBUG, CMDQ_METAEX_TZMP);
	cmdq_sec_pkt_set_mtee(mae_dev->sec_pkt, true);
	cmdq_pkt_finalize_loop(mae_dev->sec_pkt);
	cmdq_pkt_flush_threaded(mae_dev->sec_pkt, mtk_mae_sec_pkt_cb, (void *)mae_dev);
}

static void mtk_mae_enable_secure_domain(struct mtk_mae_dev *mae_dev)
{
	struct cmdq_pkt *pkt = cmdq_pkt_create(mae_dev->mae_clt);

	cmdq_pkt_set_event(pkt, mae_dev->mae_sec_wait);
	cmdq_pkt_wfe(pkt, mae_dev->mae_sec_set);
	cmdq_pkt_flush_async(pkt, mtk_mae_sec_cmdq_cb, (void *)mae_dev);
	cmdq_pkt_wait_complete(pkt);
	cmdq_pkt_destroy(pkt);
}

static void mtk_mae_disable_secure_domain(struct mtk_mae_dev *mae_dev)
{
	struct cmdq_pkt *pkt = cmdq_pkt_create(mae_dev->mae_clt);

	cmdq_pkt_set_event(pkt, mae_dev->mae_sec_wait);
	cmdq_pkt_wfe(pkt, mae_dev->mae_sec_set);
	cmdq_pkt_flush_async(pkt, mtk_mae_sec_cmdq_cb, (void *)mae_dev);
	cmdq_pkt_wait_complete(pkt);
	cmdq_pkt_destroy(pkt);
}
#endif

const struct mtk_mae_drv_ops mae_ops_isp8 = {
	// .reset = mtk_mae_reset,
	// .alloc_buf = aie_alloc_aie_buf,
	// .init = aie_init,
	// .uninit = aie_uninit,0
	.set_dma_address = mtk_mae_config_dma,
	.config_hw = mtk_mae_config_hw,
	.config_fld = mtk_mae_config_fld_v0,
	.get_fd_v0_result = mtk_mae_get_fd_v0_result,
	.get_fd_v1_result = mtk_mae_get_fd_v1_result,
	// .get_attr_result = aie_get_attr_result,
	// .get_fld_result = aie_get_fld_result,
	.irq_handle = mtk_mae_irq_handle,
	// .config_fld_buf_reg = aie_config_fld_buf_reg,
	.dump_reg = mtk_mae_dump_reg,
	// .dump_cg_reg = aie_dump_cg_reg,
	// .enable_ddren = aie_enable_ddren_7sp_1,
#if MAE_CMDQ_SEC_READY
	.secure_init = mtk_mae_secure_cmdq_init,
	.secure_enable = mtk_mae_enable_secure_domain,
	.secure_disable = mtk_mae_disable_secure_domain,
#endif
};

int mtk_mae_isp8_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev ,"%s +", __func__);

	mtk_mae_register_drv_ops(&mae_ops_isp8);

	dev_info(&pdev->dev ,"%s -", __func__);

	return 0;
}


int mtk_mae_isp8_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev ,"%s +-", __func__);
	return 0;
}

static void mtk_mae_fac_v1_pat(struct cmdq_pkt *pkt)
{
	MAE_CMDQ_WRITE_REG(pkt, 0x7010, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7014, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7018, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x701c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7020, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7024, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7028, 0x00000aa8);
	MAE_CMDQ_WRITE_REG(pkt, 0x702c, 0x00000aa8);
	MAE_CMDQ_WRITE_REG(pkt, 0x7030, 0x00000aa8);
	MAE_CMDQ_WRITE_REG(pkt, 0x7080, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7084, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7088, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x708c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7090, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7094, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7098, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x709c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a0, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70ac, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b0, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7100, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7104, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7108, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x710c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7110, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7114, 0x00000000);

	MAE_CMDQ_WRITE_REG(pkt, 0x6004, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6008, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x600c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6010, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x601c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6020, 0x00000100);
	MAE_CMDQ_WRITE_REG(pkt, 0x6024, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6028, 0x00000100);
	MAE_CMDQ_WRITE_REG(pkt, 0x602c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6034, 0x00000002);
	MAE_CMDQ_WRITE_REG(pkt, 0x605c, 0x0000cccd);
	MAE_CMDQ_WRITE_REG(pkt, 0x6060, 0x00000002);
	MAE_CMDQ_WRITE_REG(pkt, 0x6064, 0x0000bbbc);
	MAE_CMDQ_WRITE_REG(pkt, 0x6068, 0x00000003);
	MAE_CMDQ_WRITE_REG(pkt, 0x60a0, 0x00008280);
	MAE_CMDQ_WRITE_REG(pkt, 0x60a4, 0x000081e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x60a8, 0x00000070);
	MAE_CMDQ_WRITE_REG(pkt, 0x60ac, 0x00000070);
	MAE_CMDQ_WRITE_REG(pkt, 0x60c0, 0x00000080);
	MAE_CMDQ_WRITE_REG(pkt, 0x60c4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x60c8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x60cc, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x60d0, 0x00000280);
	MAE_CMDQ_WRITE_REG(pkt, 0x60d4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x60d8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6104, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6108, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x610c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6110, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6114, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6118, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x611c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6120, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6180, 0x00000110);
	MAE_CMDQ_WRITE_REG(pkt, 0x6184, 0x0000000b);

}

static void mtk_mae_aiseg_pat(struct cmdq_pkt *pkt)
{
	// POST
	MAE_CMDQ_WRITE_REG(pkt, 0x61a0, 0x00000100);
	MAE_CMDQ_WRITE_REG(pkt, 0x61a4, 0x00000302);
	MAE_CMDQ_WRITE_REG(pkt, 0x61a8, 0x00000504);
	MAE_CMDQ_WRITE_REG(pkt, 0x61ac, 0x00000706);
	MAE_CMDQ_WRITE_REG(pkt, 0x61b0, 0x00000908);
	MAE_CMDQ_WRITE_REG(pkt, 0x61b4, 0x00000b0a);
	MAE_CMDQ_WRITE_REG(pkt, 0x61b8, 0x00000d0c);
	MAE_CMDQ_WRITE_REG(pkt, 0x61bc, 0x00000f0e);
	MAE_CMDQ_WRITE_REG(pkt, 0x61c0, 0x00000100);
	MAE_CMDQ_WRITE_REG(pkt, 0x61c4, 0x00000302);
	MAE_CMDQ_WRITE_REG(pkt, 0x61c8, 0x00000504);
	MAE_CMDQ_WRITE_REG(pkt, 0x61cc, 0x00000706);
	MAE_CMDQ_WRITE_REG(pkt, 0x61d0, 0x00001111);
	MAE_CMDQ_WRITE_REG(pkt, 0x61d4, 0x00001111);
	MAE_CMDQ_WRITE_REG(pkt, 0x61d8, 0x00001111);
	MAE_CMDQ_WRITE_REG(pkt, 0x61dc, 0x00001111);
	MAE_CMDQ_WRITE_REG(pkt, 0x61e0, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x61e4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x61e8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x61ec, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x61f0, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x61f4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x61f8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x61fc, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x63a0, 0x00000100);
	MAE_CMDQ_WRITE_REG(pkt, 0x63a4, 0x00000002);
	MAE_CMDQ_WRITE_REG(pkt, 0x63a8, 0x00000504);
	MAE_CMDQ_WRITE_REG(pkt, 0x63ac, 0x00000706);
	MAE_CMDQ_WRITE_REG(pkt, 0x63b0, 0x00000908);
	MAE_CMDQ_WRITE_REG(pkt, 0x63b4, 0x00000b0a);
	MAE_CMDQ_WRITE_REG(pkt, 0x63b8, 0x00000d0c);
	MAE_CMDQ_WRITE_REG(pkt, 0x63bc, 0x00000f0e);
	MAE_CMDQ_WRITE_REG(pkt, 0x63c0, 0x00000300);
	MAE_CMDQ_WRITE_REG(pkt, 0x63c4, 0x00000c03);
	MAE_CMDQ_WRITE_REG(pkt, 0x63c8, 0x00000303);
	MAE_CMDQ_WRITE_REG(pkt, 0x63cc, 0x00000303);
	MAE_CMDQ_WRITE_REG(pkt, 0x63d0, 0x00001111);
	MAE_CMDQ_WRITE_REG(pkt, 0x63d4, 0x00001111);
	MAE_CMDQ_WRITE_REG(pkt, 0x63d8, 0x00001111);
	MAE_CMDQ_WRITE_REG(pkt, 0x63dc, 0x00001111);
	MAE_CMDQ_WRITE_REG(pkt, 0x63e0, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x63e4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x63e8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x63ec, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x63f0, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x63f4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x63f8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x63fc, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x65a0, 0x00000100);
	MAE_CMDQ_WRITE_REG(pkt, 0x65a4, 0x00000302);
	MAE_CMDQ_WRITE_REG(pkt, 0x65a8, 0x00000504);
	MAE_CMDQ_WRITE_REG(pkt, 0x65ac, 0x00000706);
	MAE_CMDQ_WRITE_REG(pkt, 0x65b0, 0x00000908);
	MAE_CMDQ_WRITE_REG(pkt, 0x65b4, 0x00000b0a);
	MAE_CMDQ_WRITE_REG(pkt, 0x65b8, 0x00000d0c);
	MAE_CMDQ_WRITE_REG(pkt, 0x65bc, 0x00000f0e);
	MAE_CMDQ_WRITE_REG(pkt, 0x65c0, 0x00000a00);
	MAE_CMDQ_WRITE_REG(pkt, 0x65c4, 0x00000c0b);
	MAE_CMDQ_WRITE_REG(pkt, 0x65c8, 0x00000e0d);
	MAE_CMDQ_WRITE_REG(pkt, 0x65cc, 0x0000100f);
	MAE_CMDQ_WRITE_REG(pkt, 0x65d0, 0x00001111);
	MAE_CMDQ_WRITE_REG(pkt, 0x65d4, 0x00001111);
	MAE_CMDQ_WRITE_REG(pkt, 0x65d8, 0x00001111);
	MAE_CMDQ_WRITE_REG(pkt, 0x65dc, 0x00001111);
	MAE_CMDQ_WRITE_REG(pkt, 0x65e0, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x65e4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x65e8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x65ec, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x65f0, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x65f4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x65f8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x65fc, 0x00000000);

	// RSZ 1
	MAE_CMDQ_WRITE_REG(pkt, 0x6004, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6008, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x600c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6010, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x601c, 0x0000e9d4);
	MAE_CMDQ_WRITE_REG(pkt, 0x6020, 0x00000104);
	MAE_CMDQ_WRITE_REG(pkt, 0x6024, 0x00009184);
	MAE_CMDQ_WRITE_REG(pkt, 0x6028, 0x00000106);
	MAE_CMDQ_WRITE_REG(pkt, 0x602c, 0x00000101);
	MAE_CMDQ_WRITE_REG(pkt, 0x6034, 0x00000001);
	MAE_CMDQ_WRITE_REG(pkt, 0x605c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6060, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6064, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6068, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x60a0, 0x00008028);
	MAE_CMDQ_WRITE_REG(pkt, 0x60a4, 0x00008028);
	MAE_CMDQ_WRITE_REG(pkt, 0x60a8, 0x00000080);
	MAE_CMDQ_WRITE_REG(pkt, 0x60ac, 0x00000060);
	MAE_CMDQ_WRITE_REG(pkt, 0x60c0, 0x00000080);
	MAE_CMDQ_WRITE_REG(pkt, 0x60c4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x60c8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x60cc, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x60d0, 0x00000028);
	MAE_CMDQ_WRITE_REG(pkt, 0x60d4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x60d8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6104, 0x00000002);
	MAE_CMDQ_WRITE_REG(pkt, 0x6108, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x610c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6110, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6114, 0x00000017);
	MAE_CMDQ_WRITE_REG(pkt, 0x6118, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x611c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6120, 0x00000017);
	MAE_CMDQ_WRITE_REG(pkt, 0x6180, 0x00000004);
	MAE_CMDQ_WRITE_REG(pkt, 0x6184, 0x00000007);

	// RSZ 2
	MAE_CMDQ_WRITE_REG(pkt, 0x6204, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6208, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x620c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6210, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x621c, 0x0000e9d4);
	MAE_CMDQ_WRITE_REG(pkt, 0x6220, 0x00000104);
	MAE_CMDQ_WRITE_REG(pkt, 0x6224, 0x0000e9d4);
	MAE_CMDQ_WRITE_REG(pkt, 0x6228, 0x00000104);
	MAE_CMDQ_WRITE_REG(pkt, 0x622c, 0x00000101);
	MAE_CMDQ_WRITE_REG(pkt, 0x6234, 0x00000001);
	MAE_CMDQ_WRITE_REG(pkt, 0x625c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6260, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6264, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6268, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x62a0, 0x00008028);
	MAE_CMDQ_WRITE_REG(pkt, 0x62a4, 0x00008028);
	MAE_CMDQ_WRITE_REG(pkt, 0x62a8, 0x00000080);
	MAE_CMDQ_WRITE_REG(pkt, 0x62ac, 0x00000080);
	MAE_CMDQ_WRITE_REG(pkt, 0x62c0, 0x00000080);
	MAE_CMDQ_WRITE_REG(pkt, 0x62c4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x62c8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x62cc, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x62d0, 0x00000028);
	MAE_CMDQ_WRITE_REG(pkt, 0x62d4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x62d8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6304, 0x00000002);
	MAE_CMDQ_WRITE_REG(pkt, 0x6308, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x630c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6310, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6314, 0x00000017);
	MAE_CMDQ_WRITE_REG(pkt, 0x6318, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x631c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6320, 0x00000017);
	MAE_CMDQ_WRITE_REG(pkt, 0x6380, 0x00000004);
	MAE_CMDQ_WRITE_REG(pkt, 0x6384, 0x00000007);

	// RSZ 3
	MAE_CMDQ_WRITE_REG(pkt, 0x6404, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6408, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x640c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6410, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x641c, 0x0000e9d4);
	MAE_CMDQ_WRITE_REG(pkt, 0x6420, 0x00000104);
	MAE_CMDQ_WRITE_REG(pkt, 0x6424, 0x0000e9d4);
	MAE_CMDQ_WRITE_REG(pkt, 0x6428, 0x00000104);
	MAE_CMDQ_WRITE_REG(pkt, 0x642c, 0x00000101);
	MAE_CMDQ_WRITE_REG(pkt, 0x6434, 0x00000001);
	MAE_CMDQ_WRITE_REG(pkt, 0x645c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6460, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6464, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6468, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x64a0, 0x00008028);
	MAE_CMDQ_WRITE_REG(pkt, 0x64a4, 0x00008028);
	MAE_CMDQ_WRITE_REG(pkt, 0x64a8, 0x00000080);
	MAE_CMDQ_WRITE_REG(pkt, 0x64ac, 0x00000080);
	MAE_CMDQ_WRITE_REG(pkt, 0x64c0, 0x00000080);
	MAE_CMDQ_WRITE_REG(pkt, 0x64c4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x64c8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x64cc, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x64d0, 0x00000028);
	MAE_CMDQ_WRITE_REG(pkt, 0x64d4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x64d8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6504, 0x00000002);
	MAE_CMDQ_WRITE_REG(pkt, 0x6508, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x650c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6510, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6514, 0x00000017);
	MAE_CMDQ_WRITE_REG(pkt, 0x6518, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x651c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6520, 0x00000017);
	MAE_CMDQ_WRITE_REG(pkt, 0x6580, 0x00000004);
	MAE_CMDQ_WRITE_REG(pkt, 0x6584, 0x00000007);
}

static void mtk_mae_fd_ipn_640_480_pat(struct cmdq_pkt *pkt)
{
	// FD POST
	MAE_CMDQ_WRITE_REG(pkt, 0x7010, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7014, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7018, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x701c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7020, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7024, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7028, 0x00000200);
	MAE_CMDQ_WRITE_REG(pkt, 0x702c, 0x00000200);
	MAE_CMDQ_WRITE_REG(pkt, 0x7030, 0x00000200);
	MAE_CMDQ_WRITE_REG(pkt, 0x7080, 0x00000280);
	MAE_CMDQ_WRITE_REG(pkt, 0x7084, 0x00000280);
	MAE_CMDQ_WRITE_REG(pkt, 0x7088, 0x00000280);
	MAE_CMDQ_WRITE_REG(pkt, 0x708c, 0x000001e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x7090, 0x000001e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x7094, 0x000001e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x7098, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x709c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a0, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70ac, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b0, 0x0000000a);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b4, 0x0000000a);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b8, 0x0000000a);
	MAE_CMDQ_WRITE_REG(pkt, 0x7100, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7104, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7108, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x710c, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7110, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7114, 0x000001f4);
}

static void mtk_mae_attr_v0_pat(struct cmdq_pkt *pkt)
{
	// FD POST
	MAE_CMDQ_WRITE_REG(pkt, 0x7010, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7014, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7018, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x701c, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7020, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7024, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7028, 0x0aa8);
	MAE_CMDQ_WRITE_REG(pkt, 0x702c, 0x0aa8);
	MAE_CMDQ_WRITE_REG(pkt, 0x7030, 0x0aa8);
	MAE_CMDQ_WRITE_REG(pkt, 0x7080, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7084, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7088, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x708c, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7090, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7094, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7098, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x709c, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a0, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a4, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a8, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70ac, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b0, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b4, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b8, 0x0000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7100, 0x0000);

}

static void mtk_mae_fd_ipn_240_180_pat(struct cmdq_pkt *pkt)
{
	// FD POST
	MAE_CMDQ_WRITE_REG(pkt, 0x7010, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7014, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7018, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x701c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7020, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7024, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7028, 0x00000555);
	MAE_CMDQ_WRITE_REG(pkt, 0x702c, 0x00000555);
	MAE_CMDQ_WRITE_REG(pkt, 0x7030, 0x00000555);
	MAE_CMDQ_WRITE_REG(pkt, 0x7080, 0x00000280);
	MAE_CMDQ_WRITE_REG(pkt, 0x7084, 0x00000280);
	MAE_CMDQ_WRITE_REG(pkt, 0x7088, 0x00000280);
	MAE_CMDQ_WRITE_REG(pkt, 0x708c, 0x000001e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x7090, 0x000001e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x7094, 0x000001e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x7098, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x709c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a0, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70ac, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b0, 0x0000000a);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b4, 0x0000000a);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b8, 0x0000000a);
	MAE_CMDQ_WRITE_REG(pkt, 0x7100, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7104, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7108, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x710c, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7110, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7114, 0x000001f4);

	// RSZ 2
	MAE_CMDQ_WRITE_REG(pkt, 0x6204, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6208, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x620c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6210, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x621c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6220, 0x00000100);
	MAE_CMDQ_WRITE_REG(pkt, 0x6224, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6228, 0x00000100);
	MAE_CMDQ_WRITE_REG(pkt, 0x622c, 0x00000101);
	MAE_CMDQ_WRITE_REG(pkt, 0x6234, 0x00000002);
	MAE_CMDQ_WRITE_REG(pkt, 0x625c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6260, 0x00000006);
	MAE_CMDQ_WRITE_REG(pkt, 0x6264, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6268, 0x00000006);
	MAE_CMDQ_WRITE_REG(pkt, 0x62a0, 0x00008280);
	MAE_CMDQ_WRITE_REG(pkt, 0x62a4, 0x000081e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x62a8, 0x000000f0);
	MAE_CMDQ_WRITE_REG(pkt, 0x62ac, 0x000000b4);
	MAE_CMDQ_WRITE_REG(pkt, 0x62c0, 0x00000080);
	MAE_CMDQ_WRITE_REG(pkt, 0x62c4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x62c8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x62cc, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x62d0, 0x00000280);
	MAE_CMDQ_WRITE_REG(pkt, 0x62d4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x62d8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6304, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6308, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x630c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6310, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6314, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6318, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x631c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6320, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6380, 0x00000110);
	MAE_CMDQ_WRITE_REG(pkt, 0x6384, 0x00000003);
}

static void mtk_mae_fd_ipn_120_90_pat(struct cmdq_pkt *pkt)
{
	// FD POST
	MAE_CMDQ_WRITE_REG(pkt, 0x7010, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7014, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7018, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x701c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7020, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7024, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7028, 0x00000aab);
	MAE_CMDQ_WRITE_REG(pkt, 0x702c, 0x00000aab);
	MAE_CMDQ_WRITE_REG(pkt, 0x7030, 0x00000aab);
	MAE_CMDQ_WRITE_REG(pkt, 0x7080, 0x00000280);
	MAE_CMDQ_WRITE_REG(pkt, 0x7084, 0x00000280);
	MAE_CMDQ_WRITE_REG(pkt, 0x7088, 0x00000280);
	MAE_CMDQ_WRITE_REG(pkt, 0x708c, 0x000001e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x7090, 0x000001e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x7094, 0x000001e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x7098, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x709c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a0, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70ac, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b0, 0x0000000a);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b4, 0x0000000a);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b8, 0x0000000a);
	MAE_CMDQ_WRITE_REG(pkt, 0x7100, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7104, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7108, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x710c, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7110, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7114, 0x000001f4);

	// RSZ 3
	MAE_CMDQ_WRITE_REG(pkt, 0x6404, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6408, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x640c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6410, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x641c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6420, 0x00000100);
	MAE_CMDQ_WRITE_REG(pkt, 0x6424, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6428, 0x00000100);
	MAE_CMDQ_WRITE_REG(pkt, 0x642c, 0x00000101);
	MAE_CMDQ_WRITE_REG(pkt, 0x6434, 0x00000002);
	MAE_CMDQ_WRITE_REG(pkt, 0x645c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6460, 0x00000003);
	MAE_CMDQ_WRITE_REG(pkt, 0x6464, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6468, 0x00000003);
	MAE_CMDQ_WRITE_REG(pkt, 0x64a0, 0x00008280);
	MAE_CMDQ_WRITE_REG(pkt, 0x64a4, 0x000081e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x64a8, 0x00000078);
	MAE_CMDQ_WRITE_REG(pkt, 0x64ac, 0x0000005a);
	MAE_CMDQ_WRITE_REG(pkt, 0x64c0, 0x00000080);
	MAE_CMDQ_WRITE_REG(pkt, 0x64c4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x64c8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x64cc, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x64d0, 0x00000280);
	MAE_CMDQ_WRITE_REG(pkt, 0x64d4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x64d8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6504, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6508, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x650c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6510, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6514, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6518, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x651c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6520, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6580, 0x00000110);
	MAE_CMDQ_WRITE_REG(pkt, 0x6584, 0x00000003);
}

static void mtk_mae_fd_fpn_480_360_pat(struct cmdq_pkt *pkt)
{
	// FD POST
	MAE_CMDQ_WRITE_REG(pkt, 0x7010, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7014, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7018, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x701c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7020, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7024, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x7028, 0x00000200);
	MAE_CMDQ_WRITE_REG(pkt, 0x702c, 0x00000200);
	MAE_CMDQ_WRITE_REG(pkt, 0x7030, 0x00000200);
	MAE_CMDQ_WRITE_REG(pkt, 0x7080, 0x000001e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x7084, 0x000001e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x7088, 0x000001e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x708c, 0x00000168);
	MAE_CMDQ_WRITE_REG(pkt, 0x7090, 0x00000168);
	MAE_CMDQ_WRITE_REG(pkt, 0x7094, 0x00000168);
	MAE_CMDQ_WRITE_REG(pkt, 0x7098, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x709c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a0, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70a8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70ac, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b0, 0x00000099);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b4, 0x00000099);
	MAE_CMDQ_WRITE_REG(pkt, 0x70b8, 0x00000099);
	MAE_CMDQ_WRITE_REG(pkt, 0x7100, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7104, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7108, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x710c, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7110, 0x000001f4);
	MAE_CMDQ_WRITE_REG(pkt, 0x7114, 0x000001f4);

	// RSZ 1
	MAE_CMDQ_WRITE_REG(pkt, 0x6004, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6008, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x600c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6010, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x601c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6020, 0x00000100);
	MAE_CMDQ_WRITE_REG(pkt, 0x6024, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6028, 0x00000100);
	MAE_CMDQ_WRITE_REG(pkt, 0x602c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6034, 0x00000002);
	MAE_CMDQ_WRITE_REG(pkt, 0x605c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6060, 0x0000000c);
	MAE_CMDQ_WRITE_REG(pkt, 0x6064, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6068, 0x0000000c);
	MAE_CMDQ_WRITE_REG(pkt, 0x60a0, 0x00008280);
	MAE_CMDQ_WRITE_REG(pkt, 0x60a4, 0x000081e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x60a8, 0x000001e0);
	MAE_CMDQ_WRITE_REG(pkt, 0x60ac, 0x00000168);
	MAE_CMDQ_WRITE_REG(pkt, 0x60c0, 0x00000080);
	MAE_CMDQ_WRITE_REG(pkt, 0x60c4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x60c8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x60cc, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x60d0, 0x00000280);
	MAE_CMDQ_WRITE_REG(pkt, 0x60d4, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x60d8, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6104, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6108, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x610c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6110, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6114, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6118, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x611c, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6120, 0x00000000);
	MAE_CMDQ_WRITE_REG(pkt, 0x6180, 0x00000110);
	MAE_CMDQ_WRITE_REG(pkt, 0x6184, 0x0000000b);
}

static const struct of_device_id of_match_mtk_mae_isp8_drv[] = {
	{
		.compatible = "mediatek,mtk-mae-plat",
	}, {
		/* sentinel */
	}
};

static struct platform_driver mtk_mae_isp8_drv = {
	.probe = mtk_mae_isp8_probe,
	.remove = mtk_mae_isp8_remove,
	.driver = {
		.name = "mtk-mae-plat",
		.of_match_table = of_match_mtk_mae_isp8_drv,
	},
};

module_platform_driver(mtk_mae_isp8_drv);
MODULE_AUTHOR("Ming-Hsuan Chaing <ming-hsuan.chiang@mediatek.com>");
MODULE_LICENSE("GPL v2");
