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

#define MAE_DUMP_REG(REG)										\
	do {														\
		mae_dev_info(mae_dev->dev, "%s [0x%08X %08X]\n",		\
					#REG, (uint32_t)REG,						\
					(uint32_t)readl(mae_dev->mae_base + REG));	\
	} while(0)

#define MAE_CMDQ_DUMP_REG(PKT, REG, SLOT, VA)						\
	do {															\
		cmdq_pkt_mem_move(PKT, NULL, REG, SLOT, CMDQ_THR_SPR_IDX3);	\
		mae_dev_info(mae_dev->dev, "%s [0x%08X %08X]\n",			\
					#REG, (uint32_t)REG, *VA);						\
	} while(0)

#define MAE_CMDQ_WRITE_REG(PKT, MAE_REG_OFFSET, VALUE)			\
	do {														\
		cmdq_pkt_write(PKT, NULL, MAE_BASE + MAE_REG_OFFSET,	\
						VALUE, CMDQ_REG_MASK);					\
	} while(0)

#define DIV_CEIL_POS(X,Y) 	(((X)-(int)(X/Y)*Y) > 0 ? (int)((X/Y)+1) : (int)(X/Y))
// #define CEIL_POS(X) 	((X-(int)(X)) > 0 ? (int)(X+1) : (int)(X))
// #define CEIL_NEG(X) 	(int)(X)
// #define CEIL(X) 		( ((X) > 0) ? CEIL_POS(X) : CEIL_NEG(X) )

// #define FLOOR_POS(X) 	(int)(X)
// #define FLOOR_NEG(X) 	((X-(int)(X)) < 0 ? (int)(X-1) : (int)(X))
// #define FLOOR(X) 		( ((X) > 0) ? FLOOR_POS(X) : FLOOR_NEG(X) )

#define MIN(X,Y)		((X > Y) ? Y : X)
#define MAX(X,Y)		((X > Y) ? X : Y)
#define ABS(X)			((X > 0) ? X : -X)

int mae_fd_post_on = 0;
int mae_cmdq_read = 0;
int mae_trigger_cmdq_timeout = 0;
int fld_debug_1 = 0;
int rsz_debug_on = 0;
int mae_ddren_on = 0;	// 0: on, 1: origin, 2: all off (confirm by DE)

module_param(mae_fd_post_on, int, 0644);
module_param(mae_cmdq_read, int, 0644);
module_param(mae_trigger_cmdq_timeout, int, 0644);
module_param(fld_debug_1, int, 0644);
module_param(rsz_debug_on, int, 0644);
module_param(mae_ddren_on, int, 0644);

static void mtk_mae_dump_reg(struct mtk_mae_dev *mae_dev);

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

static int mtk_mae_fd_pattern_sel(struct mtk_mae_dev *mae_dev, int w, int h)
{
	if (w > fd_pattern_width[0] || h > fd_pattern_height[0]) {
		mae_dev_info(mae_dev->dev, "[%s] over pyramid size limit, too large "
									"w(%d/%d), h(%d/%d)",
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
		mae_dev_info(mae_dev->dev, "[%s] over pyramid size limit, too small "
									"w(%d/%d), h(%d/%d)",
									__func__, w, FD_PYRAMID_MIN_WIDTH,
									h, FD_PYRAMID_MIN_HEIGHT);
		return -1;
	}
}

static void mtk_mae_set_dma_address(struct mtk_mae_dev *mae_dev, int idx)
{
	struct EnqueParam *param =
		(struct EnqueParam*)mae_dev->map_table->param_dmabuf_info[idx].kva;
	uint64_t addr = 0;
	uint64_t config_addr = 0;
	uint64_t coef_addr = 0;
	int pat_sel = 0;
	uint32_t config_offset = 0;
	uint32_t config_rt_offset = 0;
	uint32_t config_size = 0;
	uint32_t coef_offset = 0;
	uint32_t coef_size = 0;
	uint32_t outer_loop = 0;
	uint32_t loop = 0;
	uint32_t i = 0;
	uint32_t wdma_base_addr_reg_offset = 0;
	// DEBUG_ONLY
	pr_info("%s+", __func__);

	switch (param->maeMode) {
		case FD_V0:
		case ATTR_V0:
			outer_loop = param->pyramidNumber;
			break;
		default:
			mae_dev_info(mae_dev->dev, "[%s] unsupport mode(%d)",
						__func__, param->maeMode);
			return;
			break;
	}

	if (outer_loop > MAX_OUTER_LOOP_NUM) {
		mae_dev_info(mae_dev->dev, "[%s] outer_loop num(%d) is over limitation",
									__func__, outer_loop);
		return;
	}

	if (mae_ddren_on == 2) {
		// DBF off
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_WDMA_DBF_OFF, 0x0003);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_01A8_RSZ0, 0X0001);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_POST_DBF_OFF, 0X0001);
	}

	//----------------------- internal base ------------------------------
	addr = mae_dev->map_table->internal_dmabuf_info.pa;
	if (CHECK_BASE_ADDR(addr) || addr == 0)
		mae_dev_info(mae_dev->dev, "%s(0x%llx) is not %d-aligned",
								"internal buffer", addr ,MAE_BASE_ADDR_ALIGN);
	// DEBUG_ONLY
	mae_dev_info(mae_dev->dev, "internal base (0x%llx)", addr);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_INTRN_BASE_0_W, LSB_ADDR(addr));
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_INTRN_BASE_1_W, MSB_ADDR(addr));
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_INTRN_BASE_0_R, LSB_ADDR(addr));
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_INTRN_BASE_1_R, MSB_ADDR(addr));

	for (loop = 0; loop < outer_loop; loop++) {
		//----------------------- image0 p0 base ------------------------------
		addr = mae_dev->map_table->image_dmabuf_info[idx][loop].pa;
		if (CHECK_BASE_ADDR(addr) || addr == 0)
			mae_dev_info(mae_dev->dev, "Loop %d: %s(0x%llx) is not %d-aligned or zero",
										loop, "image0", addr, MAE_BASE_ADDR_ALIGN);

		// DEBUG_ONLY
		mae_dev_dbg(mae_dev->dev, "Loop %d: %s(0x%llx)", loop, "image0", addr);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
							MAE_REG_EXTRN_BASE0_00_0_R + loop * BASE_ADDR_REG_SIZE,
							LSB_ADDR(addr));
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
							MAE_REG_EXTRN_BASE0_00_1_R + loop * BASE_ADDR_REG_SIZE,
							MSB_ADDR(addr));

		//----------------------- image0 p1 base ------------------------------
		addr = mae_dev->map_table->image_dmabuf_info[idx][loop].pa +
				param->image[loop].imgWidth * param->image[loop].imgHeight;
		if (CHECK_BASE_ADDR(addr) || addr == 0)
			mae_dev_info(mae_dev->dev, "Loop %d: %s(0x%llx) is not %d-aligned or zero",
										loop, "image1", addr, MAE_BASE_ADDR_ALIGN);

		// DEBUG_ONLY
		mae_dev_dbg(mae_dev->dev, "Loop %d: %s(0x%llx)", loop, "image1", addr);
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
							MAE_REG_EXTRN_BASE1_00_0_R + loop * BASE_ADDR_REG_SIZE,
							LSB_ADDR(addr));
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
							MAE_REG_EXTRN_BASE1_00_1_R + loop * BASE_ADDR_REG_SIZE,
							MSB_ADDR(addr));


		//----------------------- output base ------------------------------
		// addr = mae_dev->mae_out;
		addr = mae_dev->map_table->output_dmabuf_info[idx][loop].pa;
		if (CHECK_BASE_ADDR(addr) || addr == 0)
			mae_dev_info(mae_dev->dev, "Loop %d: %s (0x%llx) is not %d-aligned",
										loop, "output", addr, MAE_BASE_ADDR_ALIGN);
		mae_dev_dbg(mae_dev->dev, "Loop %d: %s (0x%llx)", loop, "output", addr);

		switch (param->maeMode) {
			case FD_V0:
				wdma_base_addr_reg_offset =
					BASE_ADDR_REG_SIZE * FD_V0_WDMA_NUM * loop;

				MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
							MAE_REG_EXTRN_BASE0_00_0_W + wdma_base_addr_reg_offset,
							LSB_ADDR(addr));

				MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
							MAE_REG_EXTRN_BASE0_00_1_W + wdma_base_addr_reg_offset,
							MSB_ADDR(addr));
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
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_00_W, 1);
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_01_W, 1);
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_02_W, 1);
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_LN_OFFSET_03_W, 1);
				}
				break;
			default:
				mae_dev_info(mae_dev->dev, "[%s] unsupport mode(%d)",
							__func__, param->maeMode);
				return;
				break;
		}

		//------------------- calculate the offset and size of binary file ---------------------
		switch (param->maeMode) {
			case FD_V0:
				pat_sel = mtk_mae_fd_pattern_sel(mae_dev, param->image[loop].resizeWidth,
										param->image[loop].resizeHeight);
				if (pat_sel < 0 || pat_sel >= FD_PATTERN_NUM) {
					mae_dev_info(mae_dev->dev, "Loop %d: invalid pat_sel: %d",
								loop, pat_sel);
					return;
				}

				config_offset = v0_fd_config_offset[pat_sel];
				coef_offset = v0_fd_coef_offset[pat_sel];

				if (param->fdInputDegree == DEGREE_90 ||
					param->fdInputDegree == DEGREE_270) {
					config_rt_offset = fd_v0_config_info[pat_sel].rotate_offset;
					config_size = fd_v0_config_info[pat_sel].rotate_size;
				} else {
					config_rt_offset = 0;
					config_size = fd_v0_config_info[pat_sel].size;
				}

				coef_size = fd_v0_coef_info[pat_sel].size;

				config_addr =
					mae_dev->map_table->config_dmabuf_info[MODEL_TYPE_FD_V0].pa + config_offset;
				coef_addr =
					mae_dev->map_table->coef_dmabuf_info[MODEL_TYPE_FD_V0].pa + coef_offset;
				break;
			case ATTR_V0:
				config_offset = 0;
				coef_offset = 0;

				if (param->fdInputDegree == DEGREE_90 ||
					param->fdInputDegree == DEGREE_270) {
					config_rt_offset = attr_v0_config_info.rotate_offset;
					config_size = attr_v0_config_info.rotate_size;
				} else {
					config_rt_offset = 0;
					config_size = attr_v0_config_info.size;
				}

				coef_size = attr_v0_coef_info.size;

				config_addr =
					mae_dev->map_table->config_dmabuf_info[MODEL_TYPE_FLD_FAC_V0].pa + config_offset;
				coef_addr =
					mae_dev->map_table->coef_dmabuf_info[MODEL_TYPE_FLD_FAC_V0].pa + coef_offset;
				break;
			default:
				mae_dev_info(mae_dev->dev, "[%s] unsupport mode(%d)",
							__func__, param->maeMode);
				break;
		}

		// DEBUG_ONLY
		mae_dev_info(mae_dev->dev, "pat_sel: %d, degree: %d"
									"config_offset: %d (Byte), config_rt_offset: %d (16 Bytes), "
									"config_size: %d (16 Bytes), coef_offset: %d (Byte), "
									"coef_size: %d , "
									"config_addr: 0x%llx , "
									"coef_addr: 0x%llx",
									pat_sel, param->fdInputDegree,
									config_offset, config_rt_offset,
									config_size, coef_offset,
									coef_size, config_addr, coef_addr);

		//----------------------- config0 base ------------------------------
		if (CHECK_BASE_ADDR(config_addr) || config_addr == 0)
			mae_dev_info(mae_dev->dev, "Loop %d: %s(0x%llx) is not %d-aligned",
										loop, "config0", config_addr, MAE_BASE_ADDR_ALIGN);

		mae_dev_dbg(mae_dev->dev, "Loop %d: %s(0x%llx)", loop, "config", config_addr);

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						MAE_REG_OUTER_CONFIG_BASE_00_0 + loop * BASE_ADDR_REG_SIZE,
						LSB_ADDR(config_addr));
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						MAE_REG_OUTER_CONFIG_BASE_00_1 + loop * BASE_ADDR_REG_SIZE,
						MSB_ADDR(config_addr));

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						MAE_REG_OUTER_CONFIG_SIZE_00 + loop * COMMON_REG_SIZE,
						config_size);

		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						MAE_REG_OUTER_CONFIG_OFFSET_00_0 + loop * BASE_ADDR_REG_SIZE,
						REG_RANGE(config_rt_offset, 15, 0));
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						MAE_REG_OUTER_CONFIG_OFFSET_00_1 + loop * BASE_ADDR_REG_SIZE,
						REG_RANGE(config_rt_offset, 31, 16));

		//----------------------- coef0 base ------------------------------
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

	// DEBUG_ONLY
	pr_info("%s+", __func__);
}

// follow DE crop formula
void mtk_mae_crop(const struct crop_setting_in *in,
					struct crop_setting_out *out)
{
	int32_t crop_x_size;
	int32_t crop_y_size;
	int32_t even_end_y;
	int32_t even_start_y;
	int32_t reg_outer_src_hsize;
	int32_t reg_outer_src_vsize;
	int32_t base0_shift_offset;
	int32_t base0_shift;
	int32_t base1_shift;
	int32_t crop_right_x;
	int32_t crop_left_x;
	int32_t crop_down_y;
	int32_t crop_up_y;

    // DEBUG_ONLY
	pr_info("[%s] start_x(%d), "
			"end_x(%d), "
			"start_y(%d), "
			"end_y(%d), "
			"input_h_size(%d), "
			"input_v_size(%d)\n",
			__func__,
			in->start_x,
			in->end_x,
			in->start_y,
			in->end_y,
			in->input_h_size,
			in->input_v_size);

	crop_x_size = in->end_x - in->start_x;
	crop_y_size = in->end_y - in->start_y;

	if (crop_x_size <= 0 || crop_y_size <= 0 || in->input_h_size <= 0 || in->input_v_size <= 0) {
		pr_info("[%s] error input height/width(%d/%d), (x1,x2,y1,y2)=(%d,%d,%d,%d)",
				__func__, in->input_v_size, in->input_h_size,
				in->start_x, in->end_x, in->start_y, in->end_y);
		return;
	}

	even_end_y = DIV_CEIL_POS(MIN(in->end_y, in->input_v_size), 2) * 2;
    even_start_y = MAX(in->start_y, 0) / 2 * 2;

    reg_outer_src_hsize = DIV_CEIL_POS(MIN(in->end_x, in->input_h_size), 16) * 16
		- MIN(MAX(in->start_x, 0), in->input_h_size) / 16 * 16;
    reg_outer_src_vsize = even_end_y - even_start_y;

    base0_shift_offset = MAX(in->start_x, 0) / 16;
    base0_shift = even_start_y * 40 + base0_shift_offset;
    base1_shift = even_start_y * 20 + base0_shift_offset;

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

	    // DEBUG_ONLY
	pr_info("[%s] reg_pre_crop_h_st(%d), "
			"reg_pre_crop_h_length(%d), "
			"reg_pre_crop_hfde_size(%d), "
			"reg_pre_crop_v_st(%d), "
			"reg_pre_crop_v_length(%d), "
			"reg_pre_crop_vfde_size(%d), "
			"reg_ins_path(%d), "
			"reg_pre_crop_h_crop_en(%d), "
			"reg_pre_crop_v_crop_en(%d)\n",
			__func__,
			out->reg_pre_crop_h_st,
			out->reg_pre_crop_h_length,
			out->reg_pre_crop_hfde_size,
			out->reg_pre_crop_v_st,
			out->reg_pre_crop_v_length,
			out->reg_pre_crop_vfde_size,
			out->reg_ins_path,
			out->reg_pre_crop_h_crop_en,
			out->reg_pre_crop_v_crop_en);
}

static void mtk_mae_config_crop(struct mtk_mae_dev *mae_dev,
								struct EnqueParam *param,
								int idx,
								uint32_t rsz_num)
{
	struct crop_setting_in crop_in;
	struct crop_setting_out crop_out = {0};
	uint32_t rsz_offset = 0;

	pr_info("crop setting on\n");

	// resize setting on
	crop_in.start_x = param->image[0].roi.x1;
	crop_in.end_x = param->image[0].roi.x2;
	crop_in.start_y = param->image[0].roi.y1;
	crop_in.end_y = param->image[0].roi.y2;
	crop_in.input_h_size = param->image[0].imgWidth;
	crop_in.input_v_size = param->image[0].imgHeight;
	mtk_mae_crop(&crop_in, &crop_out);

	switch (rsz_num) {
		case 1:
			rsz_offset = 0;
			break;
		case 2:
			rsz_offset = 0x200;
			break;
		case 3:
			rsz_offset = 0x400;
			break;
	}

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_00C8_RSZ1 + rsz_offset,
				REG_RANGE(crop_out.reg_pre_crop_h_st, 13, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_00C8_RSZ1 + rsz_offset,
		REG_RANGE(crop_out.reg_pre_crop_h_st, 13, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_00CC_RSZ1 + rsz_offset,
				REG_RANGE(crop_out.reg_pre_crop_h_length, 13, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_00CC_RSZ1 + rsz_offset,
		REG_RANGE(crop_out.reg_pre_crop_h_length, 13, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_00D0_RSZ1 + rsz_offset,
				REG_RANGE(crop_out.reg_pre_crop_hfde_size, 13, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_00D0_RSZ1 + rsz_offset,
		REG_RANGE(crop_out.reg_pre_crop_hfde_size, 13, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_00D4_RSZ1 + rsz_offset,
				REG_RANGE(crop_out.reg_pre_crop_v_st, 13, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_00D4_RSZ1 + rsz_offset,
		REG_RANGE(crop_out.reg_pre_crop_v_st, 13, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_00D8_RSZ1 + rsz_offset,
				REG_RANGE(crop_out.reg_pre_crop_v_length, 13, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_00D8_RSZ1 + rsz_offset,
		REG_RANGE(crop_out.reg_pre_crop_v_length, 13, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_00C0_RSZ1 + rsz_offset,
				0x80 +
				(REG_RANGE(crop_out.reg_pre_crop_v_crop_en, 0, 0) << 1) +
				REG_RANGE(crop_out.reg_pre_crop_h_crop_en, 0, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
				__func__, MAE_BASE + REG_00C0_RSZ1 + rsz_offset,
				0x80 +
				(REG_RANGE(crop_out.reg_pre_crop_v_crop_en, 0, 0) << 1) +
				REG_RANGE(crop_out.reg_pre_crop_h_crop_en, 0, 0));
}

// follow DE crop formula
void mtk_mae_padding(const struct padding_setting_in *in,
					struct padding_setting_out *out)
{
	int32_t pad_left_x = -in->left;
	int32_t pad_right_x = -in->right;
	int32_t pad_down_y = -in->down;
	int32_t pad_up_y = -in->up;

    // DEBUG_ONLY
	pr_info("[%s] left(%d), "
			"right(%d), "
			"down(%d), "
			"up(%d), "
			"crop_output_h_size(%d), "
			"crop_output_v_size(%d)\n",
			__func__,
			in->left,
			in->right,
			in->down,
			in->up,
			in->crop_output_h_size,
			in->crop_output_v_size);

	if (in->crop_output_h_size <= 0 || in->crop_output_v_size <= 0) {
		pr_info("[%s] error input height/width(%d/%d)",
				__func__, in->crop_output_v_size, in->crop_output_h_size);
		return;
	}

    out->reg_post_ins_blk_hpre = ABS(pad_left_x);
    out->reg_post_ins_h_length = DIV_CEIL_POS(in->crop_output_h_size, 4) * 4;
    out->reg_post_ins_hfde_size = DIV_CEIL_POS((out->reg_post_ins_h_length + ABS(pad_right_x) + ABS(pad_left_x)), 4) * 4;

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

    // DEBUG_ONLY
	pr_info("[%s] reg_post_ins_blk_hpre(%d), "
			"reg_post_ins_h_length(%d), "
			"reg_post_ins_hfde_size(%d), "
			"reg_post_ins_blk_vpre(%d), "
			"reg_post_ins_v_length(%d), "
			"reg_post_ins_vfde_size(%d), "
			"reg_h_size(%d), "
			"reg_v_size(%d), "
			"reg_post_ins_hv_insert_en(%d)\n",
			__func__,
			out->reg_post_ins_blk_hpre,
			out->reg_post_ins_h_length,
			out->reg_post_ins_hfde_size,
			out->reg_post_ins_blk_vpre,
			out->reg_post_ins_v_length,
			out->reg_post_ins_vfde_size,
			out->reg_h_size,
			out->reg_v_size,
			out->reg_post_ins_hv_insert_en);
}

static void mtk_mae_config_padding(struct mtk_mae_dev *mae_dev,
								struct EnqueParam *param,
								int idx,
								uint32_t rsz_num)
{
	struct padding_setting_in padding_in;
	struct padding_setting_out padding_out = {0};
	uint32_t rsz_offset = 0;

	pr_info("padding on\n");

	padding_in.left = param->image[0].padding.left;
	padding_in.right = param->image[0].padding.right;
	padding_in.down = param->image[0].padding.down;
	padding_in.up = param->image[0].padding.up;
	if (param->image[0].enRoi) {
		padding_in.crop_output_h_size = param->image[0].roi.x2 - param->image[0].roi.x1;
		padding_in.crop_output_v_size = param->image[0].roi.y2 - param->image[0].roi.y1;
	} else {
		padding_in.crop_output_h_size = param->image[0].imgWidth;
		padding_in.crop_output_v_size = param->image[0].imgHeight;
	}
	mtk_mae_padding(&padding_in, &padding_out);

	switch (rsz_num) {
		case 1:
			rsz_offset = 0;
			break;
		case 2:
			rsz_offset = 0x200;
			break;
		case 3:
			rsz_offset = 0x400;
			break;
	}

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_010C_RSZ1 + rsz_offset,
				REG_RANGE(padding_out.reg_post_ins_blk_hpre, 13, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_010C_RSZ1 + rsz_offset,
		REG_RANGE(padding_out.reg_post_ins_blk_hpre, 13, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0110_RSZ1 + rsz_offset,
				REG_RANGE(padding_out.reg_post_ins_h_length, 13, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0110_RSZ1 + rsz_offset,
		REG_RANGE(padding_out.reg_post_ins_h_length, 13, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0114_RSZ1 + rsz_offset,
				REG_RANGE(padding_out.reg_post_ins_hfde_size, 13, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0114_RSZ1 + rsz_offset,
		REG_RANGE(padding_out.reg_post_ins_hfde_size, 13, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0118_RSZ1 + rsz_offset,
				REG_RANGE(padding_out.reg_post_ins_blk_vpre, 13, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0118_RSZ1 + rsz_offset,
		REG_RANGE(padding_out.reg_post_ins_blk_vpre, 13, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_011C_RSZ1 + rsz_offset,
				REG_RANGE(padding_out.reg_post_ins_v_length, 13, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_011C_RSZ1 + rsz_offset,
		REG_RANGE(padding_out.reg_post_ins_v_length, 13, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0120_RSZ1 + rsz_offset,
				REG_RANGE(padding_out.reg_post_ins_vfde_size, 13, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0120_RSZ1 + rsz_offset,
		REG_RANGE(padding_out.reg_post_ins_vfde_size, 13, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_00A0_RSZ1 + rsz_offset,
				(1 << 15) + REG_RANGE(padding_out.reg_h_size, 13, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_00A0_RSZ1 + rsz_offset,
		(1 << 15) + REG_RANGE(padding_out.reg_h_size, 13, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_00A4_RSZ1 + rsz_offset,
				(1 << 15) + REG_RANGE(padding_out.reg_v_size, 13, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_00A4_RSZ1 + rsz_offset,
		(1 << 15) + REG_RANGE(padding_out.reg_v_size, 13, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0104_RSZ1 + rsz_offset,
				REG_RANGE(padding_out.reg_post_ins_hv_insert_en, 0, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0104_RSZ1 + rsz_offset,
		REG_RANGE(padding_out.reg_post_ins_hv_insert_en, 0, 0));
}

// follow DE resize formula
void mtk_mae_resize(const struct rsz_setting_in *in, struct rsz_setting_out *out)
{
	int32_t inputHeight_used = in->rsz_input_v_size;
	int32_t inputWidth_used = in->rsz_input_h_size;

    // DEBUG_ONLY
	pr_info("[%s] rsz_input_h_size(%d), "
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
		pr_info("[%s] input height/width(%d/%d) should not be negative",
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
		out->reg_rsz_mode_ho = 1;
		out->reg_rsz_mode_ve = 1;
		out->reg_cb_factor_ho = (int32_t)((in->rsz_output_h_size) << 20) / inputWidth_used;
		out->reg_cb_factor_ve = (int32_t)((in->rsz_output_v_size) << 20) / inputHeight_used;

		if ((out->reg_cb_factor_ho - ((out->reg_cb_factor_ho >> 8) << 8)) > 0)
			out->reg_cb_factor_ho = out->reg_cb_factor_ho + 1;

		if ((out->reg_cb_factor_ve - ((out->reg_cb_factor_ve >> 8) << 8)) > 0)
			out->reg_cb_factor_ve = out->reg_cb_factor_ve + 1;
	} else {
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

    // DEBUG_ONLY
	pr_info("[%s] reg_scale_factor_ve(%d), "
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

static void mtk_mae_config_rsz(struct mtk_mae_dev *mae_dev,
								struct EnqueParam *param,
								int idx,
								uint32_t rsz_num)
{
	struct rsz_setting_in rsz_in;
	struct rsz_setting_out rsz_out = {0};
	uint32_t rsz_offset = 0;
	uint32_t rsz_input_h_size = param->image[0].imgWidth;
	uint32_t rsz_input_v_size = param->image[0].imgHeight;

	pr_info("resize setting on\n");

	// resize setting on
	if (param->image[0].enRoi) {
		rsz_input_h_size = param->image[0].roi.x2 - param->image[0].roi.x1;
		rsz_input_v_size = param->image[0].roi.y2 - param->image[0].roi.y1;
	}

	if (param->image[0].enPadding) {
		rsz_input_h_size += param->image[0].padding.right + param->image[0].padding.left;
		rsz_input_v_size += param->image[0].padding.up + param->image[0].padding.down;
	}

	rsz_in.rsz_input_h_size = rsz_input_h_size;
	rsz_in.rsz_input_v_size = rsz_input_v_size;
	rsz_in.rsz_output_h_size = param->image[0].resizeWidth;
	rsz_in.rsz_output_v_size = param->image[0].resizeHeight;
	rsz_in.rsz_input_ch = 8;
	mtk_mae_resize(&rsz_in, &rsz_out);

	switch (rsz_num) {
		case 1:
			rsz_offset = 0;
			break;
		case 2:
			rsz_offset = 0x200;
			break;
		case 3:
			rsz_offset = 0x400;
			break;
	}

	if (mae_ddren_on == 1)
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_01A8_RSZ0, 0X0001);

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0004_RSZ1 + rsz_offset,
				REG_RANGE(rsz_out.reg_ini_factor_ho, 15, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0004_RSZ1 + rsz_offset,
		REG_RANGE(rsz_out.reg_ini_factor_ho, 15, 0));


	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0008_RSZ1 + rsz_offset,
				REG_RANGE(rsz_out.reg_ini_factor_ho, 27, 16));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0008_RSZ1 + rsz_offset,
		REG_RANGE(rsz_out.reg_ini_factor_ho, 27, 16));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_000C_RSZ1 + rsz_offset,
				REG_RANGE(rsz_out.reg_ini_factor_ve, 15, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_000C_RSZ1 + rsz_offset,
		REG_RANGE(rsz_out.reg_ini_factor_ve, 15, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0010_RSZ1 + rsz_offset,
				REG_RANGE(rsz_out.reg_ini_factor_ve, 27, 16));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0010_RSZ1 + rsz_offset,
		REG_RANGE(rsz_out.reg_ini_factor_ve, 27, 16));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_001C_RSZ1 + rsz_offset,
				REG_RANGE(rsz_out.reg_scale_factor_ho, 15, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_001C_RSZ1 + rsz_offset,
		REG_RANGE(rsz_out.reg_scale_factor_ho, 15, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0020_RSZ1 + rsz_offset,
				(REG_RANGE(rsz_out.reg_scale_factor_ho, 27, 24) << 12) +
				(REG_RANGE(rsz_out.reg_h_shift_mode_en, 0, 0) << 9) +
				(REG_RANGE(rsz_out.reg_scale_ho_en, 0, 0) << 8) +
				(REG_RANGE(rsz_out.reg_scale_factor_ho, 23, 16)));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0020_RSZ1 + rsz_offset, (REG_RANGE(rsz_out.reg_scale_factor_ho, 27, 24) << 12) +
				(REG_RANGE(rsz_out.reg_h_shift_mode_en, 0, 0) << 9) +
				(REG_RANGE(rsz_out.reg_scale_ho_en, 0, 0) << 8) +
				(REG_RANGE(rsz_out.reg_scale_factor_ho, 23, 16)));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0024_RSZ1 + rsz_offset,
				REG_RANGE(rsz_out.reg_scale_factor_ve, 15, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0024_RSZ1 + rsz_offset, REG_RANGE(rsz_out.reg_scale_factor_ve, 15, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0028_RSZ1 + rsz_offset,
				(REG_RANGE(rsz_out.reg_scale_factor_ve, 27, 24) << 12) +
				(REG_RANGE(rsz_out.reg_v_shift_mode_en, 0, 0) << 9) +
				(REG_RANGE(rsz_out.reg_scale_ve_en, 0, 0) << 8) +
				(REG_RANGE(rsz_out.reg_scale_factor_ve, 23, 16)));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0028_RSZ1 + rsz_offset,
				(REG_RANGE(rsz_out.reg_scale_factor_ve, 27, 24) << 12) +
				(REG_RANGE(rsz_out.reg_v_shift_mode_en, 0, 0) << 9) +
				(REG_RANGE(rsz_out.reg_scale_ve_en, 0, 0) << 8) +
				(REG_RANGE(rsz_out.reg_scale_factor_ve, 23, 16)));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_002C_RSZ1 + rsz_offset,
				(REG_RANGE(rsz_out.reg_mode_c_ve, 1, 0) << 8) +
				(REG_RANGE(rsz_out.reg_mode_c_ho, 1, 0)));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_002C_RSZ1 + rsz_offset,
				(REG_RANGE(rsz_out.reg_mode_c_ve, 1, 0) << 8) +
				(REG_RANGE(rsz_out.reg_mode_c_ho, 1, 0)));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0034_RSZ1 + rsz_offset,
				(REG_RANGE(rsz_out.reg_rsz_u2s, 0, 0) << 1) + 0); // DE comment: RGB is already unsigned int
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0034_RSZ1 + rsz_offset,
			((uint32_t)(REG_RANGE(rsz_out.reg_rsz_u2s, 0, 0) << 1) + 1));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_005C_RSZ1 + rsz_offset,
				REG_RANGE(rsz_out.reg_cb_factor_ho, 15, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_005C_RSZ1 + rsz_offset,
		REG_RANGE(rsz_out.reg_cb_factor_ho, 15, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0060_RSZ1 + rsz_offset,
				REG_RANGE(rsz_out.reg_cb_factor_ho, 19, 16));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0060_RSZ1 + rsz_offset,
		REG_RANGE(rsz_out.reg_cb_factor_ho, 19, 16));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0064_RSZ1 + rsz_offset,
				REG_RANGE(rsz_out.reg_cb_factor_ve, 15, 0));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0064_RSZ1 + rsz_offset,
		REG_RANGE(rsz_out.reg_cb_factor_ve, 15, 0));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0068_RSZ1 + rsz_offset,
				REG_RANGE(rsz_out.reg_cb_factor_ve, 19, 16));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0068_RSZ1 + rsz_offset,
		REG_RANGE(rsz_out.reg_cb_factor_ve, 19, 16));

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_00A0_RSZ1 + rsz_offset,
				(REG_RANGE(rsz_out.reg_h_size_usr_md, 0, 0) << 15) +
				rsz_in.rsz_input_h_size);
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_00A0_RSZ1 + rsz_offset,
		(REG_RANGE(rsz_out.reg_h_size_usr_md, 0, 0) << 15) +
		rsz_in.rsz_input_h_size);

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_00A4_RSZ1 + rsz_offset,
				(REG_RANGE(rsz_out.reg_v_size_usr_md, 0, 0) << 15) +
				rsz_in.rsz_input_v_size);
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_00A4_RSZ1 + rsz_offset,
		(REG_RANGE(rsz_out.reg_v_size_usr_md, 0, 0) << 15) +
		rsz_in.rsz_input_v_size);

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_00A8_RSZ1 + rsz_offset,
				rsz_in.rsz_output_h_size);
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_00A8_RSZ1 + rsz_offset, rsz_in.rsz_output_h_size);

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_00AC_RSZ1 + rsz_offset,
				rsz_in.rsz_output_v_size);
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_00AC_RSZ1 + rsz_offset, rsz_in.rsz_output_v_size);

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_00D0_RSZ1 + rsz_offset,
				rsz_in.rsz_input_h_size);
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_00D0_RSZ1 + rsz_offset, rsz_in.rsz_input_h_size);

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0180_RSZ1 + rsz_offset,
				(REG_RANGE(rsz_out.reg_order, 0, 0) << 2) +
				(REG_RANGE(rsz_out.reg_rsz_mode_ho, 0, 0) << 4) +
				(REG_RANGE(rsz_out.reg_rsz_mode_ve, 0, 0) << 8));
	mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
		__func__, MAE_BASE + REG_0180_RSZ1 + rsz_offset,
				(uint32_t)((REG_RANGE(rsz_out.reg_order, 0, 0) << 2) +
				(REG_RANGE(rsz_out.reg_rsz_mode_ho, 0, 0) << 4) +
				(REG_RANGE(rsz_out.reg_rsz_mode_ve, 0, 0) << 8)));
}

static void mtk_mae_config_hw(struct mtk_mae_dev *mae_dev, int idx)
{
	struct EnqueParam *param =
		(struct EnqueParam*)mae_dev->map_table->param_dmabuf_info[idx].kva;
	int pat_sel;

	// DEBUG_ONLY
	pr_info("%s+", __func__);

	// ddren set should be 100ns earlier than sw trig
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_TRIG_RST_CTRL, 0x0100);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_TRIG_RST_CTRL, 0x0000);

	// DEBUG_ONLY
	pr_info("adb: mae_fd_post_on(%d)\n", mae_fd_post_on);
	pr_info("adb: mae_cmdq_read(%d)\n", mae_cmdq_read);
	pr_info("adb: mae_trigger_cmdq_timeout(%d)\n", mae_trigger_cmdq_timeout);
	pr_info("adb: fld_debug_1(%d)\n", fld_debug_1);
	pr_info("adb: rsz_debug_on(%d)\n", rsz_debug_on);
	pr_info("adb: mae_ddren_on(%d)\n", mae_ddren_on);

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
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_SYS_SHADOW_CTRL, 0x0000);

	switch (param->maeMode) {
		case FD_V0:
			// resize default
			pat_sel = mtk_mae_fd_pattern_sel(mae_dev, param->image[0].resizeWidth,
				param->image[0].resizeHeight);
			pr_info("pat sel (%d)\n", pat_sel);

			if (param->image[0].srcImgFmt == NV12) {
				if (mae_ddren_on == 2)
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_MEM_CONFIG, 0x000C + (0XF << 12)); // reg[12]: dbf_off
				else if (mae_ddren_on == 1)
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_MEM_CONFIG, 0x000C + (0X1 << 12)); // reg[12]: dbf_off
				else
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_MEM_CONFIG, 0x000C); // reg[12]: dbf_off
				MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_RESERVE, 0x0007);
			} else {
				pr_info("wrong img fmt(%d) for fd_v0\n", param->image[0].srcImgFmt);
				return;
			}

			MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_COEF_ROTATE, param->fdInputDegree);

			// FD POST: formula from algo
			if (mae_fd_post_on == 0) {
				if (pat_sel == 0) {
					cmdq_pkt_write(mae_dev->pkt[idx], NULL,
						MAE_BASE + MAE_REG_MMFD_O_SCALE_0, 0x00000AA8,
						CMDQ_REG_MASK);
					mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
						__func__, MAE_BASE + MAE_REG_MMFD_O_SCALE_0,
						0x00000AA8);
					cmdq_pkt_write(mae_dev->pkt[idx], NULL, MAE_BASE + MAE_REG_H_SIZE0,
									0x280, CMDQ_REG_MASK);
					mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
						__func__, MAE_BASE + MAE_REG_H_SIZE0, 0x280);

					cmdq_pkt_write(mae_dev->pkt[idx], NULL, MAE_BASE + MAE_REG_V_SIZE0,
									0x1E0, CMDQ_REG_MASK);
					mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
						__func__, MAE_BASE + MAE_REG_V_SIZE0, 0x1E0);
				} else if (pat_sel == 1) {
					cmdq_pkt_write(mae_dev->pkt[idx], NULL,
						MAE_BASE + MAE_REG_MMFD_O_SCALE_0, 0x000002AA,
						CMDQ_REG_MASK);
					mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
						__func__, MAE_BASE + MAE_REG_MMFD_O_SCALE_0,
						0x000002AA);

					cmdq_pkt_write(mae_dev->pkt[idx], NULL, MAE_BASE + MAE_REG_H_SIZE0,
									0x280, CMDQ_REG_MASK);
					mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
						__func__, MAE_BASE + MAE_REG_H_SIZE0, 0x280);

					cmdq_pkt_write(mae_dev->pkt[idx], NULL, MAE_BASE + MAE_REG_V_SIZE0,
									0x1E0, CMDQ_REG_MASK);
					mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
						__func__, MAE_BASE + MAE_REG_V_SIZE0, 0x1E0);
				} else if (pat_sel == 2) {
					cmdq_pkt_write(mae_dev->pkt[idx], NULL,
						MAE_BASE + MAE_REG_MMFD_O_SCALE_1, 0x000002AA,
						CMDQ_REG_MASK);
					mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
						__func__, MAE_BASE + MAE_REG_MMFD_O_SCALE_1,
						0x000002AA);

					cmdq_pkt_write(mae_dev->pkt[idx], NULL, MAE_BASE + MAE_REG_H_SIZE1,
									0x280, CMDQ_REG_MASK);
					mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
						__func__, MAE_BASE + MAE_REG_H_SIZE1, 0x280);

					cmdq_pkt_write(mae_dev->pkt[idx], NULL, MAE_BASE + MAE_REG_V_SIZE1,
									0x1E0, CMDQ_REG_MASK);
					mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
						__func__, MAE_BASE + MAE_REG_V_SIZE1, 0x1E0);

				} else if (pat_sel == 3) {
					cmdq_pkt_write(mae_dev->pkt[idx], NULL,
						MAE_BASE + MAE_REG_MMFD_O_SCALE_2, 0x00000AA8,
						CMDQ_REG_MASK);
					mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
						__func__, MAE_BASE + MAE_REG_MMFD_O_SCALE_2,
						0x00000AA8);

					cmdq_pkt_write(mae_dev->pkt[idx], NULL, MAE_BASE + MAE_REG_H_SIZE2,
									0x280, CMDQ_REG_MASK);
					mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
						__func__, MAE_BASE + MAE_REG_H_SIZE2, 0x280);

					cmdq_pkt_write(mae_dev->pkt[idx], NULL, MAE_BASE + MAE_REG_V_SIZE2,
									0x1E0, CMDQ_REG_MASK);
					mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
						__func__, MAE_BASE + MAE_REG_V_SIZE2, 0x1E0);
				}
			} else {
				MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_MMFD_O_SCALE_0,
					(uint32_t)((param->image[0].imgWidth << 9) / param->image[0].resizeWidth)); // 0x000002AA
				mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
					__func__, MAE_BASE + MAE_REG_MMFD_O_SCALE_0,
					(uint32_t)((param->image[0].imgWidth << 9) / param->image[0].resizeWidth));

				MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_H_SIZE0,
					(uint32_t)param->image[0].imgWidth);
				mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
					__func__, MAE_BASE + MAE_REG_H_SIZE0,
							(uint32_t)param->image[0].imgWidth);

				MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_V_SIZE0,
					(uint32_t)param->image[0].imgHeight);
				mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
					__func__, MAE_BASE + MAE_REG_V_SIZE0,
							(uint32_t)param->image[0].imgHeight);

				// MM 16 only
				// cmdq_pkt_write(mae_dev->pkt[idx], NULL, MAE_BASE + MAE_REG_SCORE_TH0,
				// 				0x0000012C, CMDQ_REG_MASK);
				// cmdq_pkt_write(mae_dev->pkt[idx], NULL, MAE_BASE + MAE_REG_H_MAX0,
				// 				0x000001F4, CMDQ_REG_MASK);
				// cmdq_pkt_write(mae_dev->pkt[idx], NULL, MAE_BASE + MAE_REG_V_MAX0,
				// 				0x000001F4, CMDQ_REG_MASK);
			}


			// always resize to pyramid size
			switch (pat_sel) {
				case 0:
				case 1:
					if (param->image[0].enRoi)
						mtk_mae_config_crop(mae_dev, param, idx, 1);

					if (param->image[0].enPadding)
						mtk_mae_config_padding(mae_dev, param, idx, 1);

					mtk_mae_config_rsz(mae_dev, param, idx, 1);

					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						REG_0184_RSZ1,
						(param->image[0].enRoi << 3) + 0x3);
					mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
						__func__, MAE_BASE + REG_0184_RSZ1,
						(param->image[0].enRoi << 3) + 0x3);
					break;
				case 2:
					if (param->image[0].enRoi)
						mtk_mae_config_crop(mae_dev, param, idx, 2);

					if (param->image[0].enPadding)
						mtk_mae_config_padding(mae_dev, param, idx, 2);

					mtk_mae_config_rsz(mae_dev, param, idx, 2);

					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						REG_0184_RSZ1 + 0x200,
						(param->image[0].enRoi << 3) + 0x3);
					mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
						__func__, MAE_BASE + REG_0184_RSZ1 + 0x200,
						(param->image[0].enRoi << 3) + 0x3);
					break;
				case 3:
					if (param->image[0].enRoi)
						mtk_mae_config_crop(mae_dev, param, idx, 3);

					if (param->image[0].enPadding)
						mtk_mae_config_padding(mae_dev, param, idx, 3);

					mtk_mae_config_rsz(mae_dev, param, idx, 3);

					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
						REG_0184_RSZ1 + 0x400,
						(param->image[0].enRoi << 3) + 0x3);
					mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
						__func__, MAE_BASE + REG_0184_RSZ1 + 0x400,
						(param->image[0].enRoi << 3) + 0x3);
					break;
			}

			break;
		case ATTR_V0:
			if (param->image[0].srcImgFmt == NV12) {
				if (mae_ddren_on == 2)
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_MEM_CONFIG, 0x000C + (0XF << 12)); // reg[12]: dbf_off
				else if (mae_ddren_on == 1)
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_MEM_CONFIG, 0x000C + (0X1 << 12)); // reg[12]: dbf_off
				else
					MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_EXTRN_MEM_CONFIG, 0x000C); // reg[12]: dbf_off
                MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_RESERVE, 0x0007);
			} else {
				pr_info("wrong img fmt(%d) for attr_v0\n", param->image[0].srcImgFmt);
                return;
			}

            MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_COEF_ROTATE, param->fdInputDegree);

            if (mae_fd_post_on == 0) {
                cmdq_pkt_write(mae_dev->pkt[idx], NULL,
                    MAE_BASE + MAE_REG_MMFD_O_SCALE_0, 0x00000AA8,
                    CMDQ_REG_MASK);
                mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
                    __func__, MAE_BASE + MAE_REG_MMFD_O_SCALE_0,
                    0x00000AA8);
                // cmdq_pkt_write(mae_dev->pkt[idx], NULL, MAE_BASE + MAE_REG_H_SIZE1,
                //                 0x280, CMDQ_REG_MASK);
                // mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
                //     __func__, MAE_BASE + MAE_REG_H_SIZE1, 0x280);

                // cmdq_pkt_write(mae_dev->pkt[idx], NULL, MAE_BASE + MAE_REG_V_SIZE1,
                //                 0x1E0, CMDQ_REG_MASK);
                // mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
                //     __func__, MAE_BASE + MAE_REG_V_SIZE1, 0x1E0);
            } else {
                // MAE_TO_DO: should adjust core number
				MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_MMFD_O_SCALE_0,
					(uint32_t)((param->image[0].imgWidth << 9) / param->image[0].resizeWidth)); // 0x000002AA
				mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
					__func__, MAE_BASE + MAE_REG_MMFD_O_SCALE_0,
					(uint32_t)((param->image[0].imgWidth << 9) / param->image[0].resizeWidth));

				MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_H_SIZE0,
					(uint32_t)param->image[0].imgWidth);
				mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
					__func__, MAE_BASE + MAE_REG_H_SIZE0,
							(uint32_t)param->image[0].imgWidth);

				MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_V_SIZE0,
					(uint32_t)param->image[0].imgHeight);
				mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
					__func__, MAE_BASE + MAE_REG_V_SIZE0,
							(uint32_t)param->image[0].imgHeight);
            }

			if (param->image[0].enRoi)
				mtk_mae_config_crop(mae_dev, param, idx, 1);

			if (param->image[0].enPadding)
				mtk_mae_config_padding(mae_dev, param, idx, 1);

			// always resize to pyramid size
			mtk_mae_config_rsz(mae_dev, param, idx, 1);

			MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx],
				REG_0184_RSZ1,
				(param->image[0].enRoi << 3) + 0x3);
			mae_dev_info(mae_dev->dev, "[%s] 0x%x = 0x%x",
				__func__, MAE_BASE + REG_0184_RSZ1,
				(param->image[0].enRoi << 3) + 0x3);
			break;
		default:
			break;
	}

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_REG_0004_MAE_RDMA_5, 0x6221);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_IRQ_DDREN_CMDQ_CTRL, 0x2200);

	if (mae_trigger_cmdq_timeout == 0)
		// sw trigger
		MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_TRIG_RST_CTRL, 0x8000);

	// ddren clear
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_TRIG_RST_CTRL, 0x0100);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_TRIG_RST_CTRL, 0x0000);
	cmdq_pkt_wfe(mae_dev->pkt[idx], mae_dev->mae_event_id);

	cmdq_pkt_flush_async(mae_dev->pkt[idx], MAECmdqCB, (void *)mae_dev);

// if (fd or attr)
	// write MAE_SYS_SHADOW_CTRL
	// write MAE_REG_EXTRN_MEM_CONFIG 0x000C // [0:8]extrn_mem_config
	// write MAE_REG_RESERVE 0x0007 // [0]cup_420_en [1]csc_enable [2:3]reg_422to444_md
	// write MAE_COEF_ROTATE
// else if (aiseg)
	// write MAE_SYS_SHADOW_CTRL
	// write MAE_REG_RESERVE 0x0006 // [1]csc_enable [2:3]reg_422to444_md
	// write MAE_COEF_ROTATE

// else if (old fld)
	// write MAE_SYS_SHADOW_CTRL
// endif

	// write MAE_REG_0004_MAE_RDMA_5 0x6221


	// write MAE_IRQ_DDREN_CMDQ_CTRL 0x2200 // [13]ddren_en [9]AOV irq mask


	// ddren should set 100ns earlier than sw trig
	// write MAE_TRIG_RST_CTRL 0x0100 // [8]ddren_set
	// write MAE_TRIG_RST_CTRL 0x0000 // [8]ddren_set

	// write MAE_TRIG_RST_CTRL 0x8000 // [15]sw trigger

	// write MAE_TRIG_RST_CTRL 0x0200 // [9]ddren_clr
	// write MAE_TRIG_RST_CTRL 0x0000 // [8]ddren_clr

	// DEBUG_ONLY
	pr_info("%s+", __func__);
}

static void mtk_mae_dump_reg(struct mtk_mae_dev *mae_dev)
{
	dma_addr_t slot;
	uint32_t *va;

	if (mae_cmdq_read) {
		va = (uint32_t*)cmdq_mbox_buf_alloc(mae_dev->mae_clt, &slot);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_FACE_NUM0, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_FACE_NUM1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_FACE_NUM2, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_0004_MAE_RDMA_5, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0004_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0008_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_000C_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0010_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_001C_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0020_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0024_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0028_RSZ1, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_002C_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0034_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_005C_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0060_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0064_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0068_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0080_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0084_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_00A0_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_00A4_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_00A8_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_00AC_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_00C0_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_00C4_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_00C8_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_00CC_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_00D0_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_00D4_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_00D8_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0104_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0108_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_010C_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0110_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0114_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0118_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_011C_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0120_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0180_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_0184_RSZ1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], REG_01A8_RSZ1, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_COEF_ROTATE, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_X_OFFSET_0, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_X_OFFSET_1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_X_OFFSET_2, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_Y_OFFSET_0, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_Y_OFFSET_1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_Y_OFFSET_2, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_MMFD_O_SCALE_0, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_MMFD_O_SCALE_1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_MMFD_O_SCALE_2, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_H_SIZE0, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_H_SIZE1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_H_SIZE2, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_V_SIZE0, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_V_SIZE1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_V_SIZE2, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_EXTRN_BASE0_00_0_W, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_EXTRN_BASE0_00_1_W, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_INTRN_BASE_0_W, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_INTRN_BASE_1_W, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_EXTRN_BASE0_00_0_R, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_EXTRN_BASE0_00_1_R, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_INTRN_BASE_0_R, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_INTRN_BASE_1_R, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_EXTRN_BASE1_00_0_R, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_EXTRN_BASE1_00_1_R, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_EXTRN_LN_OFFSET_00_R, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_EXTRN_MEM_CONFIG, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_OUTER_SRC_HSIZE_00, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_OUTER_SRC_VSIZE_00, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_OUTER_CONFIG_BASE_00_0, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_OUTER_CONFIG_BASE_00_1, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_OUTER_CONFIG_OFFSET_00_0, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_OUTER_CONFIG_OFFSET_00_1, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_OUTER_CONFIG_SIZE_00, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_OUTER_COEF_BASE_00_0, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_OUTER_COEF_BASE_00_1, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_OUTER_COEF_OFFSET_00_0, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_OUTER_COEF_OFFSET_00_1, slot, va);

		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_OUTER_COEF_OFFSET_00_1, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_OUTER_COEF_SIZE_00, slot, va);
		MAE_CMDQ_DUMP_REG(mae_dev->pkt[0], MAE_REG_RESERVE, slot, va);

		cmdq_mbox_buf_free(mae_dev->mae_clt, va, slot);
	} else {
		mae_dev_info(mae_dev->dev, "%s +\n", __func__);

		mae_dev_info(mae_dev->dev, "Dump MAE_RDMA_5\n");
		MAE_DUMP_REG(MAE_REG_0004_MAE_RDMA_5);

		mae_dev_info(mae_dev->dev, "Dump RSZ1_BASE\n");
		MAE_DUMP_REG(REG_0004_RSZ1);
		MAE_DUMP_REG(REG_0008_RSZ1);
		MAE_DUMP_REG(REG_000C_RSZ1);
		MAE_DUMP_REG(REG_0010_RSZ1);
		MAE_DUMP_REG(REG_001C_RSZ1);
		MAE_DUMP_REG(REG_0020_RSZ1);
		MAE_DUMP_REG(REG_0024_RSZ1);
		MAE_DUMP_REG(REG_0028_RSZ1);
		MAE_DUMP_REG(REG_002C_RSZ1);
		MAE_DUMP_REG(REG_0034_RSZ1);
		MAE_DUMP_REG(REG_005C_RSZ1);
		MAE_DUMP_REG(REG_0060_RSZ1);
		MAE_DUMP_REG(REG_0064_RSZ1);
		MAE_DUMP_REG(REG_0068_RSZ1);
		MAE_DUMP_REG(REG_0080_RSZ1);
		MAE_DUMP_REG(REG_0084_RSZ1);
		MAE_DUMP_REG(REG_00A0_RSZ1);
		MAE_DUMP_REG(REG_00A4_RSZ1);
		MAE_DUMP_REG(REG_00A8_RSZ1);
		MAE_DUMP_REG(REG_00AC_RSZ1);
		MAE_DUMP_REG(REG_00C0_RSZ1);
		MAE_DUMP_REG(REG_00C4_RSZ1);
		MAE_DUMP_REG(REG_00C8_RSZ1);
		MAE_DUMP_REG(REG_00CC_RSZ1);
		MAE_DUMP_REG(REG_00D0_RSZ1);
		MAE_DUMP_REG(REG_00D4_RSZ1);
		MAE_DUMP_REG(REG_00D8_RSZ1);
		MAE_DUMP_REG(REG_0104_RSZ1);
		MAE_DUMP_REG(REG_0108_RSZ1);
		MAE_DUMP_REG(REG_010C_RSZ1);
		MAE_DUMP_REG(REG_0110_RSZ1);
		MAE_DUMP_REG(REG_0114_RSZ1);
		MAE_DUMP_REG(REG_0118_RSZ1);
		MAE_DUMP_REG(REG_011C_RSZ1);
		MAE_DUMP_REG(REG_0120_RSZ1);
		MAE_DUMP_REG(REG_0180_RSZ1);
		MAE_DUMP_REG(REG_0184_RSZ1);
		MAE_DUMP_REG(REG_01A8_RSZ1);

		mae_dev_info(mae_dev->dev, "Dump MAISR_APB_BASE\n");
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
	uint32_t *debug_dump;
	uint8_t i;

	mae_dev_info(mae_dev->dev, "%s +\n", __func__);


	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FDVT_ENABLE, 0x4000000);	// [26] ddren set for v0 fld

	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_SYS_SHADOW_CTRL, 0x00001000);

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
	debug_dump = (uint32_t*)mae_dev->map_table->coef_dmabuf_info[MODEL_TYPE_FLD_FAC_V0].kva +
		round_up(V0_ATTR_128_128_COEF_SIZE, MAE_BASE_ADDR_ALIGN) / 4;
	mae_dev_dbg(mae_dev->dev, "%s(0x%llx) (0x%8x_%8x )(0x%8x_%8x)",
				"FLD_BS_BASE_ADDR", addr, *debug_dump, *(debug_dump+1), *(debug_dump+2), *(debug_dump+3));

	addr += round_up(fdvt_fld_blink_weight_forest14_size, MAE_BASE_ADDR_ALIGN);
	if (CHECK_BASE_ADDR(addr) || addr == 0)
		mae_dev_info(mae_dev->dev, "%s(0x%llx) is not %d-aligned or zero",
									"FLD_FP_BASE_ADDR", addr, MAE_BASE_ADDR_ALIGN);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_FP_BASE_ADDR, addr >> 4);	// 0x408
	debug_dump +=
		round_up(fdvt_fld_blink_weight_forest14_size, MAE_BASE_ADDR_ALIGN) / 4;
	mae_dev_dbg(mae_dev->dev, "%s(0x%llx) (0x%8x_%8x )(0x%8x_%8x)",
				"FLD_FP_BASE_ADDR", addr, *debug_dump, *(debug_dump+1), *(debug_dump+2), *(debug_dump+3));

	addr += round_up(fdvt_fld_fp_forest00_om45_size, MAE_BASE_ADDR_ALIGN) * 15;
	if (CHECK_BASE_ADDR(addr) || addr == 0)
		mae_dev_info(mae_dev->dev, "%s(0x%llx) is not %d-aligned or zero",
									"FLD_SH_BASE_ADDR", addr, MAE_BASE_ADDR_ALIGN);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_SH_BASE_ADDR, addr >> 4);	// 0x410
	debug_dump +=
		(round_up(fdvt_fld_fp_forest00_om45_size, MAE_BASE_ADDR_ALIGN) * 15) / 4;
	mae_dev_dbg(mae_dev->dev, "%s(0x%llx) (0x%8x_%8x )(0x%8x_%8x)",
				"FLD_SH_BASE_ADDR", addr, *debug_dump, *(debug_dump+1), *(debug_dump+2), *(debug_dump+3));

	addr += round_up(fdvt_fld_leafnode_forest00_size, MAE_BASE_ADDR_ALIGN) * 15;
	if (CHECK_BASE_ADDR(addr) || addr == 0)
		mae_dev_info(mae_dev->dev, "%s(0x%llx) is not %d-aligned or zero",
									"FLD_CV_BASE_ADDR", addr, MAE_BASE_ADDR_ALIGN);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_CV_BASE_ADDR, addr >> 4);	// 0x414
	debug_dump +=
		(round_up(fdvt_fld_leafnode_forest00_size, MAE_BASE_ADDR_ALIGN) * 15) / 4;
	mae_dev_dbg(mae_dev->dev, "%s(0x%llx) (0x%8x_%8x )(0x%8x_%8x)",
				"FLD_CV_BASE_ADDR", addr, *debug_dump, *(debug_dump+1), *(debug_dump+2), *(debug_dump+3));

	addr += round_up(fdvt_fld_tree_forest00_cv_weight_size, MAE_BASE_ADDR_ALIGN) * 15;
	if (CHECK_BASE_ADDR(addr) || addr == 0)
		mae_dev_info(mae_dev->dev, "%s(0x%llx) is not %d-aligned or zero",
									"FLD_MS_BASE_ADDR", addr, MAE_BASE_ADDR_ALIGN);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_MS_BASE_ADDR, addr >> 4);	// 0x404
	debug_dump +=
		(round_up(fdvt_fld_tree_forest00_cv_weight_size, MAE_BASE_ADDR_ALIGN) * 15) / 4;
	mae_dev_dbg(mae_dev->dev, "%s(0x%llx) (0x%8x_%8x )(0x%8x_%8x)",
				"FLD_MS_BASE_ADDR", addr, *debug_dump, *(debug_dump+1), *(debug_dump+2), *(debug_dump+3));

	addr += round_up(fdvt_fld_tree_forest00_init_shape_size, MAE_BASE_ADDR_ALIGN);
	if (CHECK_BASE_ADDR(addr) || addr == 0)
		mae_dev_info(mae_dev->dev, "%s(0x%llx) is not %d-aligned or zero",
									"FLD_TR_BASE_ADDR", addr, MAE_BASE_ADDR_ALIGN);
	MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], FLD_TR_BASE_ADDR, addr >> 4);	// 0x40C
	debug_dump +=
		round_up(fdvt_fld_tree_forest00_init_shape_size, MAE_BASE_ADDR_ALIGN) / 4;
	mae_dev_dbg(mae_dev->dev, "%s(0x%llx) (0x%8x_%8x )(0x%8x_%8x)",
				"FLD_TR_BASE_ADDR", addr, *debug_dump, *(debug_dump+1), *(debug_dump+2), *(debug_dump+3));

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
	cmdq_pkt_wfe(mae_dev->pkt[idx], mae_dev->mae_event_id);
	cmdq_pkt_flush_async(mae_dev->pkt[idx], MAECmdqCB, (void *)mae_dev);
	// MAE_CMDQ_WRITE_REG(mae_dev->pkt[idx], MAE_CLK_CTRL, 0x0000);  // reg_clk_sw_md_en[6:4], reg_clk_en[2:0]

	mae_dev_info(mae_dev->dev, "%s -\n", __func__);
}

const struct mtk_mae_drv_ops mae_ops_isp8 = {
	// .reset = aie_reset,
	// .alloc_buf = aie_alloc_aie_buf,
	// .init = aie_init,
	// .uninit = aie_uninit,
	.set_dma_address = mtk_mae_set_dma_address,
	.config_hw = mtk_mae_config_hw,
	.config_fld = mtk_mae_config_fld_v0,
	// .get_fd_result = aie_get_fd_result,
	// .get_attr_result = aie_get_attr_result,
	// .get_fld_result = aie_get_fld_result,
	.irq_handle = mtk_mae_irq_handle,
	// .config_fld_buf_reg = aie_config_fld_buf_reg,
	.dump_reg = mtk_mae_dump_reg,
	// .dump_cg_reg = aie_dump_cg_reg,
	// .enable_ddren = aie_enable_ddren_7sp_1,
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


// AOV case
// {
	// write MAE_IRQ_DDREN_CMDQ_CTRL 0x2110 // [13]ddren_en [8]cam irq mask [4]cmdq mask
// }

// MAE_TO_DO: FRAME DONE CLEAR IRQ

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
