// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Author: Yuhsuan.chang <yuhsuan.chang@mediatek.com>
 *
 */

#include "mtk_imgsys-cmdq-hwqos.h"

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/kstrtox.h>
#include <linux/kthread.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>

#include "mtk_imgsys-cmdq-plat.h"
#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-hwqos-reg.h"
#include "mtk_imgsys-hwqos-data.h"
#include "mtk_imgsys-trace.h"

#define IMGSYS_QOS_THD_IDX (IMGSYS_NOR_THD + IMGSYS_PWR_THD)
#define CMDQ_REG_MASK GENMASK(31, 0)

#define SPR1 CMDQ_THR_SPR_IDX1
#define SPR2 CMDQ_THR_SPR_IDX2
#define SPR3 CMDQ_THR_SPR_IDX3

#define CMDQ_GCEM_CPR_IMG_QOS_START 0x8014
/* 5 + 2 x 15 = 35 */
#define CMDQ_GCEM_CPR_IMG_QOS_END 0x803B

#define CPR_IDX (CMDQ_GCEM_CPR_IMG_QOS_START)
#define CPR_ACTIVE_COUNT (CMDQ_GCEM_CPR_IMG_QOS_START + 1)
#define CPR_URATE_FACTOR (CMDQ_GCEM_CPR_IMG_QOS_START + 2)
#define CPR_CORE_SUM_R (CMDQ_GCEM_CPR_IMG_QOS_START + 3)
#define CPR_CORE_SUM_W (CMDQ_GCEM_CPR_IMG_QOS_START + 4)
#define CPR_MMINFRA_FREQ (CMDQ_GCEM_CPR_IMG_QOS_START + 5)
#define CPR_NPS_R (CMDQ_GCEM_CPR_IMG_QOS_START + 6)
#define CPR_NPS_W (CMDQ_GCEM_CPR_IMG_QOS_START + 7)
#define CPR_LINE_BW (CMDQ_GCEM_CPR_IMG_QOS_START + 8)

#define QOS_CPR_STEP (2)
/* CPR count: QOS_CPR_STEP x ARRAY_SIZE(qos_map_data) */
#define CPR_SUM_R (CMDQ_GCEM_CPR_IMG_QOS_START + 9)
#define CPR_SUM_W (CMDQ_GCEM_CPR_IMG_QOS_START + 10)
#define CPR_AVG_R (CPR_SUM_R)
#define CPR_AVG_W (CPR_SUM_W)

#define BWR_POINT_RIGHT_SHIFT (3)
#define BWR_MB_RIGHT_SHIFT (3)

#define IMGSYS_QOS_REPORT_NORMAL (0)
#define IMGSYS_QOS_REPORT_MAX (1)
#define IMGSYS_QOS_INTERVAL_US (976)

#define OSTDL_MAX_VALUE 0x40
#define OSTDL_MIN_VALUE 0x1

#define OSTDBL_MIN_VALUE 16
#define OSTDBL_MAX_VALUE 2047
#define OSTDBL_RIGHT_SHIFT (4)

#define BWL_MIN_BW (35)

#define BWL_UP_BND_RIGHT_SHIFT (2)

/* AVG_BW x 5 / 512 = AVG_BW / 104 */
#define BWL_BUDGET_MULTIPLY (5)
#define BWL_BUDGET_RIGHT_SHIFT (9)
#define BWL_BUDGET_THRESHOLD (GENMASK((BWL_BUDGET_REG_H - BWL_BUDGET_REG_L), 0))
#define BWL_BUDGET_SHIFT_STEP (1)

#define QOS_AVERAGE_SHIFT (7)

/* EMI BW to 64 MB/s */
#define QOS_TTL_RIGHT_SHIFT (2)

#define IMG_QOS_ACTIVE_COUNT_ADDR (BWR_IMG_E1A_BASE + BWR_IMG_SRT_EMI_ENG_BW5_OFT)
#define IMG_QOS_URATE_FACTOR_ADDR (BWR_IMG_E1A_BASE + BWR_IMG_SRT_EMI_ENG_BW_RAT5_OFT)

#define field_get(_mask, _reg) (((_reg) & (_mask)) >> (ffs(_mask) - 1))
#define field_prep(_mask, _val) (((_val) << (ffs(_mask) - 1)) & (_mask))

enum QOS_STATE {
	QOS_STATE_INIT = 0,
	QOS_STATE_NORMAL,
	QOS_STATE_MAX,
	QOS_STATE_UNINIT
};

enum GCE_COND_REVERSE_COND {
	R_CMDQ_NOT_EQUAL = CMDQ_EQUAL,
	R_CMDQ_EQUAL = CMDQ_NOT_EQUAL,
	R_CMDQ_LESS = CMDQ_GREATER_THAN_AND_EQUAL,
	R_CMDQ_GREATER = CMDQ_LESS_THAN_AND_EQUAL,
	R_CMDQ_LESS_EQUAL = CMDQ_GREATER_THAN,
	R_CMDQ_GREATER_EQUAL = CMDQ_LESS_THAN,
};

#define GCE_COND_DECLARE \
	u32 _inst_condi_jump, _inst_jump_end; \
	u64 _jump_pa; \
	u64 *_inst; \
	struct cmdq_pkt *_cond_pkt; \
	u16 _reg_jump

#define GCE_COND_ASSIGN(pkt, addr) do { \
	_cond_pkt = pkt; \
	_reg_jump = addr; \
} while (0)

#define GCE_OP_DECLARE \
	struct cmdq_operand lop, rop

#define GCE_OP_ASSIGN(dst_reg, l_reg, l, logic, r_reg, r) do { \
	lop.reg = l_reg; \
	lop.reg ? (lop.idx = l) : (lop.value = l); \
	rop.reg = r_reg; \
	rop.reg ? (rop.idx = r) : (rop.value = r); \
	cmdq_pkt_logic_command(pkt, logic, \
		dst_reg, &lop, &rop); \
} while (0)

#define GCE_OP_UPDATE(dst_reg, logic, r_reg, r) do { \
	lop.reg = true; \
	lop.value = dst_reg; \
	rop.reg = r_reg; \
	rop.reg ? (rop.idx = r) : (rop.value = r); \
	cmdq_pkt_logic_command(pkt, logic, \
		dst_reg, &lop, &rop); \
} while (0)

#define GCE_IF(l_reg, l, cond, r_reg, r) do { \
	lop.reg = l_reg; \
	lop.reg ? (lop.idx = l) : (lop.value = l); \
	rop.reg = r_reg; \
	rop.reg ? (rop.idx = r) : (rop.value = r); \
	cmdq_pkt_assign_command(_cond_pkt, _reg_jump, 0); \
	_inst_condi_jump = _cond_pkt->cmd_buf_size - 8; \
	cmdq_pkt_cond_jump_abs( \
		_cond_pkt, _reg_jump, &lop, &rop, (enum CMDQ_CONDITION_ENUM) cond); \
	_inst_jump_end = _inst_condi_jump; \
} while (0)

#define GCE_ELSE do { \
	_inst_jump_end = _cond_pkt->cmd_buf_size; \
	cmdq_pkt_jump_addr(_cond_pkt, 0); \
	_inst = cmdq_pkt_get_va_by_offset(_cond_pkt, _inst_condi_jump); \
	_jump_pa = cmdq_pkt_get_pa_by_offset(_cond_pkt, _cond_pkt->cmd_buf_size); \
	*_inst = *_inst & ((u64)0xFFFFFFFF << 32); \
	*_inst = *_inst | CMDQ_REG_SHIFT_ADDR(_jump_pa); \
} while (0)

#define GCE_FI do { \
	_inst = cmdq_pkt_get_va_by_offset(_cond_pkt, _inst_jump_end); \
	_jump_pa = cmdq_pkt_get_pa_by_offset(_cond_pkt, _cond_pkt->cmd_buf_size); \
	*_inst = *_inst & ((u64)0xFFFFFFFF << 32); \
	*_inst = *_inst | CMDQ_REG_SHIFT_ADDR(_jump_pa); \
} while (0)

static struct cmdq_client *g_hwqos_clt;
static struct cmdq_pkt *g_hwqos_pkt;
static enum QOS_STATE g_hwqos_state = QOS_STATE_INIT;
static dma_addr_t g_hwqos_buf_pa;
static u32 *g_hwqos_buf_va;

static bool g_hwqos_support;

/* debug control */
static int g_hwqos_active_thre = 85;
static int g_hwqos_active_multiply = 4;
static int g_hwqos_multiply = 1;
static int g_hwqos_right_shift;
static bool g_hwqos_high_bw;

static int g_hwqos_max_rw_eng_bw = 2400;
static int g_hwqos_max_bw_multiply = 5;
static int g_hwqos_max_bw_right_shift = 2;

static bool g_hwqos_dbg_en;
static bool g_hwqos_sw_en;
static bool imgsys_ftrace_thread_en;

module_param(g_hwqos_active_thre, int, 0644);
MODULE_PARM_DESC(g_hwqos_active_thre, "imgsys hwqos active threshold");

module_param(g_hwqos_active_multiply, int, 0644);
MODULE_PARM_DESC(g_hwqos_active_multiply, "imgsys hwqos active multiply");

module_param(g_hwqos_multiply, int, 0644);
MODULE_PARM_DESC(g_hwqos_multiply, "imgsys hwqos multiply");

module_param(g_hwqos_right_shift, int, 0644);
MODULE_PARM_DESC(g_hwqos_right_shift, "imgsys hwqos right shift");

module_param(g_hwqos_high_bw, bool, 0644);
MODULE_PARM_DESC(g_hwqos_high_bw, "imgsys hwqos high bw");

module_param(g_hwqos_max_rw_eng_bw, int, 0644);
MODULE_PARM_DESC(g_hwqos_max_rw_eng_bw, "imgsys hwqos max rw bw");

module_param(g_hwqos_max_bw_multiply, int, 0644);
MODULE_PARM_DESC(g_hwqos_max_bw_multiply, "imgsys hwqos max bw multiply");

module_param(g_hwqos_max_bw_right_shift, int, 0644);
MODULE_PARM_DESC(g_hwqos_max_bw_right_shift, "imgsys hwqos max bw right shift");

module_param(g_hwqos_dbg_en, bool, 0644);
MODULE_PARM_DESC(g_hwqos_dbg_en, "imgsys hwqos log enable");

module_param(g_hwqos_sw_en, bool, 0644);
MODULE_PARM_DESC(g_hwqos_sw_en, "imgsys hwqos mode, sw=1, hw=0 (default)");

module_param(imgsys_ftrace_thread_en, bool, 0644);
MODULE_PARM_DESC(imgsys_ftrace_thread_en, "imgsys ftrace thread enable, 0 (default)");

/* OSTDL enable */
static int g_ostdl_en = 1;

module_param(g_ostdl_en, int, 0644);
MODULE_PARM_DESC(g_ostdl_en, "imgsys ostd limiter en");

static int g_ostdl_mae_min_r = 5;

module_param(g_ostdl_mae_min_r, int, 0644);
MODULE_PARM_DESC(g_ostdl_mae_min_r, "imgsys mae read ostd limiter");

static int g_ostdl_mae_min_w = 2;

module_param(g_ostdl_mae_min_w, int, 0644);
MODULE_PARM_DESC(g_ostdl_mae_min_w, "imgsys mae write ostd limiter");

/* AXI monitor limiter */
static bool g_axi_limiter_en;

module_param(g_axi_limiter_en, bool, 0644);
MODULE_PARM_DESC(g_axi_limiter_en, "imgsys axi limiter en");

/* ostdbl */
static int g_ostdbl_factor_multiply_r = 1;
static int g_ostdbl_factor_right_shift_r;

module_param(g_ostdbl_factor_multiply_r, int, 0644);
MODULE_PARM_DESC(g_ostdbl_factor_multiply_r, "imgsys ostdbl read multiply");

module_param(g_ostdbl_factor_right_shift_r, int, 0644);
MODULE_PARM_DESC(g_ostdbl_factor_right_shift_r, "imgsys ostdbl read right shift");

static int g_ostdbl_factor_multiply_w = 1;
static int g_ostdbl_factor_right_shift_w;

module_param(g_ostdbl_factor_multiply_w, int, 0644);
MODULE_PARM_DESC(g_ostdbl_factor_multiply_w, "imgsys ostdbl write multiply");

module_param(g_ostdbl_factor_right_shift_w, int, 0644);
MODULE_PARM_DESC(g_ostdbl_factor_right_shift_w, "imgsys ostdbl write right shift");

/* bwl */
static int g_bwl_margin_multiply_r = 3;
static int g_bwl_margin_right_shift_r;

module_param(g_bwl_margin_multiply_r, int, 0644);
MODULE_PARM_DESC(g_bwl_margin_multiply_r, "imgsys bwl read multiply");

module_param(g_bwl_margin_right_shift_r, int, 0644);
MODULE_PARM_DESC(g_bwl_margin_right_shift_r, "imgsys bwl read right shift");

static int g_bwl_margin_multiply_w = 3;
static int g_bwl_margin_right_shift_w;

module_param(g_bwl_margin_multiply_w, int, 0644);
MODULE_PARM_DESC(g_bwl_margin_multiply_w, "imgsys bwl write multiply");

module_param(g_bwl_margin_right_shift_w, int, 0644);
MODULE_PARM_DESC(g_bwl_margin_right_shift_w, "imgsys bwl write right shift");

static int g_bwl_threshold_us = 3;

module_param(g_bwl_threshold_us, int, 0644);
MODULE_PARM_DESC(g_bwl_threshold_us, "imgsys bwl threshold");

struct limiter_param {
	const uint16_t cpr_avg_idx;
	const uint16_t cpr_nps_idx;
	const uint8_t ostdbl_reg_l;
	const uint32_t ostdbl_reg_mask;
	int ostdbl_multiply;
	int ostdbl_right_shift;
	int bwl_multiply;
	int bwl_right_shift;
};

#define __LIMITER_PARAM(rw)  \
	{.cpr_avg_idx  = CPR_CORE_SUM_ ## rw, \
	.cpr_nps_idx = CPR_NPS_ ## rw,\
	.ostdbl_reg_l   = OSTDBL_ ## rw ## _REG_L,  \
	.ostdbl_reg_mask    = OSTDBL_ ## rw ## _REG_MASK}
#define _LIMITER_PARAM(rw) __LIMITER_PARAM(rw)
#define LIMITER_PARAM(rw) _LIMITER_PARAM(rw)

static struct limiter_param limiter_param_r = LIMITER_PARAM(R);
static struct limiter_param limiter_param_w = LIMITER_PARAM(W);

#define IDX true
#define VAL false

#define __SET_LIMITER_PARAM(rw)  \
	limiter_param_ ## rw.ostdbl_multiply  = g_ostdbl_factor_multiply_ ## rw, \
	limiter_param_ ## rw.ostdbl_right_shift = g_ostdbl_factor_right_shift_ ## rw,\
	limiter_param_ ## rw.bwl_multiply   = g_bwl_margin_multiply_ ## rw,  \
	limiter_param_ ## rw.bwl_right_shift    = g_bwl_margin_right_shift_ ## rw
#define _SET_LIMITER_PARAM(rw) __SET_LIMITER_PARAM(rw)
#define SET_LIMITER_PARAM(rw) _SET_LIMITER_PARAM(rw)

/** Dump api
 * @brief List Dump api
 */

static void imgsys_hwqos_dbg_reg_read(u32 pa)
{
	u32 value = 0x0;
	void __iomem *va;

	va = ioremap(pa, 0x4);
	if (va) {
		value = readl((void *)va);
		ftrace_imgsys_hwqos_dbg_reg_read(pa, value);
		iounmap(va);
	} else {
		pr_info("[%s] addr:0x%08X ioremap fail!\n", __func__, pa);
	}
}

static void imgsys_reg_read_base_list(const uint32_t *reg_base_list,
					const uint32_t reg_base_list_size,
					const uint32_t offset)
{
	uint32_t i;

	for (i = 0; i < reg_base_list_size; i++)
		imgsys_hwqos_dbg_reg_read(reg_base_list[i] + offset);
}

static void imgsys_reg_read(const uint32_t *base_list,
				const uint32_t base_list_size,
				const struct reg *reg_list,
				const uint32_t reg_list_size)
{
	uint32_t i, j;

	for (j = 0; j < base_list_size; j++) {
		for (i = 0; i < reg_list_size; i++) {
			imgsys_hwqos_dbg_reg_read(
				base_list[j] + reg_list[i].offset);
		}
	}
}

static void imgsys_hwqos_dbg_reg_dump(uint8_t level)
{
	uint32_t i;

	if (level >= 3) {
		/* BLS config */
		imgsys_reg_read(bls_base_array, ARRAY_SIZE(bls_base_array),
					bls_init_data, ARRAY_SIZE(bls_init_data));
		imgsys_reg_read_base_list(bls_base_array, ARRAY_SIZE(bls_base_array),
					BLS_IMG_CTRL_OFT);
		/* BWR config */
		imgsys_reg_read(bwr_base_array, ARRAY_SIZE(bwr_base_array),
				bwr_init_data, ARRAY_SIZE(bwr_init_data));
		for (i = 0; i < ARRAY_SIZE(bwr_ctrl_data); i++) {
			imgsys_hwqos_dbg_reg_read(
				BWR_IMG_E1A_BASE + bwr_ctrl_data[i].qos_sel_offset);
			imgsys_hwqos_dbg_reg_read(
				BWR_IMG_E1A_BASE + bwr_ctrl_data[i].qos_en_offset);
		}
		for (i = 0; i < ARRAY_SIZE(qos_map_data); i++) {
			imgsys_hwqos_dbg_reg_read(
				BWR_IMG_E1A_BASE + qos_map_data[i].bwr_r_rat_offset);
			imgsys_hwqos_dbg_reg_read(
				BWR_IMG_E1A_BASE + qos_map_data[i].bwr_w_rat_offset);
		}
		imgsys_hwqos_dbg_reg_read(
			BWR_IMG_E1A_BASE + BWR_IMG_SRT_EMI_ENG_BW_RAT0_OFT);
	}

	if (level >= 1) {
		/* BLS & BWR BW */
		for (i = 0; i < ARRAY_SIZE(qos_map_data); i++) {
			if (level >= 2) {
				imgsys_hwqos_dbg_reg_read(
					qos_map_data[i].bls_base + BLS_IMG_LEN_SUM_R_OFT);
				imgsys_hwqos_dbg_reg_read(
					qos_map_data[i].bls_base + BLS_IMG_LEN_SUM_W_OFT);
			}
			imgsys_hwqos_dbg_reg_read(
				BWR_IMG_E1A_BASE + qos_map_data[i].bwr_r_offset);
			imgsys_hwqos_dbg_reg_read(
				BWR_IMG_E1A_BASE + qos_map_data[i].bwr_w_offset);
		}
		if (level >= 2) {
			imgsys_hwqos_dbg_reg_read(
				BLS_IMG_E1A_BASE + BLS_IMG_LEN_SUM_R_OFT);
			imgsys_hwqos_dbg_reg_read(
				BLS_IMG_E1A_BASE + BLS_IMG_LEN_SUM_W_OFT);
		}
		for (i = 0; i < ARRAY_SIZE(bwr_ttl_bw_array); i++) {
			imgsys_hwqos_dbg_reg_read(
				bwr_ttl_bw_array[i].addr);
		}
	}

	/* BWR status */
	imgsys_hwqos_dbg_reg_read(
			BWR_IMG_E1A_BASE + BWR_IMG_RPT_STATE_OFT);
	imgsys_hwqos_dbg_reg_read(
			BWR_IMG_E1A_BASE + BWR_IMG_SEND_BW_ZERO_OFT);
	imgsys_hwqos_dbg_reg_read(
			BWR_IMG_E1A_BASE + BWR_IMG_SEND_DONE_ST0_OFT);
	imgsys_hwqos_dbg_reg_read(
			BWR_IMG_E1A_BASE + BWR_IMG_SEND_DONE_ST1_OFT);
}

static int imgsys_hwqos_dbg_thread(void *data)
{
	unsigned int i = 0;
	u32 reg = 0;
	u32 value;
	char buf[128];
	int ret;
	struct va_map {
		void __iomem *va;
		u32 value;
	};
	struct bw_item {
		struct va_map bwr_r;
		struct va_map bwr_w;
		struct va_map bls_r;
		struct va_map bls_w;
	};
	struct limiter_item {
		struct va_map ostdl;
	};
	struct bw_item bw_data[ARRAY_SIZE(qos_map_data)] = {0};
	struct va_map img_hw_bw[ARRAY_SIZE(img_hw_bw_reg_array)] = {0};
	struct va_map bwr_ttl_bw[ARRAY_SIZE(bwr_ttl_bw_array)] = {0};
	struct limiter_item img_limiter[ARRAY_SIZE(limiter_data)] = {0};
	struct va_map qos_active_count = {0};
	struct va_map qos_urate_factor = {0};

	/* map pa to va */
	for (i = 0; i < ARRAY_SIZE(bw_data); i++) {
		bw_data[i].bwr_r.va = ioremap(BWR_IMG_E1A_BASE + qos_map_data[i].bwr_r_offset, 0x4);
		bw_data[i].bwr_w.va = ioremap(BWR_IMG_E1A_BASE + qos_map_data[i].bwr_w_offset, 0x4);
		bw_data[i].bls_r.va = ioremap(qos_map_data[i].bls_base + BLS_IMG_LEN_SUM_R_OFT, 0x4);
		bw_data[i].bls_w.va = ioremap(qos_map_data[i].bls_base + BLS_IMG_LEN_SUM_W_OFT, 0x4);
	}
	for (i = 0; i < ARRAY_SIZE(img_hw_bw); i++)
		img_hw_bw[i].va = ioremap(img_hw_bw_reg_array[i].addr, 0x4);
	for (i = 0; i < ARRAY_SIZE(bwr_ttl_bw); i++)
		bwr_ttl_bw[i].va = ioremap(bwr_ttl_bw_array[i].addr, 0x4);
	for (i = 0; i < ARRAY_SIZE(img_limiter); i++)
		img_limiter[i].ostdl.va = ioremap(limiter_data[i].ostdl, 0x4);

	qos_active_count.va = ioremap(IMG_QOS_ACTIVE_COUNT_ADDR, 0x4);
	qos_urate_factor.va = ioremap(IMG_QOS_URATE_FACTOR_ADDR, 0x4);

	/* loop to trace qos */
	while (!kthread_should_stop()) {
		for (i = 0; i < ARRAY_SIZE(qos_map_data); i++) {
			ret = snprintf(buf, sizeof(buf),
				"core%d_sc%d_eng%d_larb%d",
				qos_map_data[i].core,
				qos_map_data[i].sub_common,
				qos_map_data[i].engine,
				qos_map_data[i].larb);
			if (ret < 0) {
				pr_err("snprintf failed\n");
				continue;
			}

			/* ratio: BLS_UNIT / (IMGSYS_QOS_INTERVAL_US * 10^-6) / (1 << 20) */
			value = readl((void *)bw_data[i].bls_r.va);
			MTK_IMGSYS_QOS_ENABLE(value != bw_data[i].bls_r.value,
				ftrace_imgsys_hwqos_bls("%s_%s=%u",
					"r", buf,
					(u32) (value / 64));
				bw_data[i].bls_r.value = value;
			);
			value = readl((void *)bw_data[i].bls_w.va);
			MTK_IMGSYS_QOS_ENABLE(value != bw_data[i].bls_w.value,
				ftrace_imgsys_hwqos_bls("%s_%s=%u",
					"w", buf,
					(u32) (value / 64));
				bw_data[i].bls_w.value = value;
			);
			if (reg == qos_map_data[i].bwr_r_offset) {
				/* skip current BWR value since same as previous */
				continue;
			}
			reg = qos_map_data[i].bwr_r_offset;
			/* ratio: 1 / BIT(BWR_BW_POINT) * BW_RAT * BUS_URATE */
			value = readl((void *)bw_data[i].bwr_r.va);
			MTK_IMGSYS_QOS_ENABLE(value != bw_data[i].bwr_r.value,
				ftrace_imgsys_hwqos_bwr("%s_%s=%u",
					"r", buf,
					(u32) (value * 91 / 512));
				bw_data[i].bwr_r.value = value;
			);
			value = readl((void *)bw_data[i].bwr_w.va);
			MTK_IMGSYS_QOS_ENABLE(value != bw_data[i].bwr_w.value,
				ftrace_imgsys_hwqos_bwr("%s_%s=%u",
					"w", buf,
					(u32) (value * 91 / 512));
				bw_data[i].bwr_w.value = value;
			);
		}

		/* Read/Write AXI BW */
		for (i = 0; i < ARRAY_SIZE(img_hw_bw) - 1; i++) {
			value = field_get(img_hw_bw_reg_array[i].mask, readl((void *)img_hw_bw[i].va));
			MTK_IMGSYS_QOS_ENABLE(value != img_hw_bw[i].value,
				ftrace_imgsys_hwqos_bwr("%s%u=%u",
					"img_hw_bw", i, value * 16);
				img_hw_bw[i].value = value;
			);
		}
		/* Total BW */
		value = field_get(img_hw_bw_reg_array[i].mask, readl((void *)img_hw_bw[i].va));
		MTK_IMGSYS_QOS_ENABLE(value != img_hw_bw[i].value,
			ftrace_imgsys_hwqos_bwr("%s%u=%u",
				"img_hw_bw", i, value * 64);
			img_hw_bw[i].value = value;
		);
		/* BWR ttl BW */
		for (i = 0; i < ARRAY_SIZE(bwr_ttl_bw); i++) {
			value = field_get(bwr_ttl_bw_array[i].mask, readl((void *)bwr_ttl_bw[i].va));
			MTK_IMGSYS_QOS_ENABLE(value != bwr_ttl_bw[i].value,
				ftrace_imgsys_hwqos_bwr("%s%u=%u",
				"bwr_ttl_bw", i, (value * 5 / 4) << QOS_TTL_RIGHT_SHIFT);
				bwr_ttl_bw[i].value = value;
			);
		}

		value = readl((void *)qos_active_count.va);
		MTK_IMGSYS_QOS_ENABLE(value != qos_active_count.value,
			ftrace_imgsys_hwqos_policy("%s=%u",
			"active_count", value);
			qos_active_count.value = value;
		);

		value = readl((void *)qos_urate_factor.va);
		MTK_IMGSYS_QOS_ENABLE(value != qos_urate_factor.value,
			ftrace_imgsys_hwqos_policy("%s=%u",
			"urate_factor", value);
			qos_urate_factor.value = value;
		);

		for (i = 0; i < ARRAY_SIZE(img_limiter); i++) {
			value = readl((void *)img_limiter[i].ostdl.va);
			MTK_IMGSYS_QOS_ENABLE(value != img_limiter[i].ostdl.value,
				ftrace_imgsys_hwqos_ostdl("%s%u=%u",
					"ostdl_r_img_comm", i, field_get(limiter_data[i].r.ostdl_reg_mask, value));
				ftrace_imgsys_hwqos_ostdl("%s%u=%u",
					"ostdl_w_img_comm", i, field_get(limiter_data[i].w.ostdl_reg_mask, value));
				img_limiter[i].ostdl.value = value;
			);
		}

		usleep_range(1000, 1050);
	}

	/* unmap va */
	for (i = 0; i < ARRAY_SIZE(bw_data); i++) {
		iounmap(bw_data[i].bwr_r.va);
		iounmap(bw_data[i].bwr_w.va);
		iounmap(bw_data[i].bls_r.va);
		iounmap(bw_data[i].bls_w.va);
	}

	for (i = 0; i < ARRAY_SIZE(img_hw_bw); i++)
		iounmap(img_hw_bw[i].va);
	for (i = 0; i < ARRAY_SIZE(bwr_ttl_bw); i++)
		iounmap(bwr_ttl_bw[i].va);
	for (i = 0; i < ARRAY_SIZE(img_limiter); i++)
		iounmap(img_limiter[i].ostdl.va);

	iounmap(qos_active_count.va);
	iounmap(qos_urate_factor.va);

	return 0;
}

static void imgsys_hwqos_set_dbg_thread(bool enable)
{
	static struct task_struct *kthr;
	static bool dbg_en;

	if (imgsys_ftrace_thread_en && enable && !dbg_en) {
		kthr = kthread_run(imgsys_hwqos_dbg_thread,
				NULL, "imgsys-dbg");
		if (IS_ERR(kthr))
			pr_info("[%s] create kthread imgsys-dbg failed\n", __func__);
		else
			dbg_en = true;
	} else if (dbg_en) {
		kthread_stop(kthr);
		dbg_en = false;
	}
}

/** Local api
 * @brief List Local api
 */

static void imgsys_reg_write(struct cmdq_pkt *pkt,
				const uint32_t *base_list,
				const uint32_t base_list_size,
				const struct reg *reg_list,
				const uint32_t reg_list_size)
{
	uint32_t i, j;

	for (j = 0; j < base_list_size; j++) {
		for (i = 0; i < reg_list_size; i++) {
			cmdq_pkt_write(pkt, NULL,
				base_list[j] + reg_list[i].offset,
				reg_list[i].value, CMDQ_REG_MASK);
		}
	}
}

static void imgsys_reg_write_base_list(struct cmdq_pkt *pkt,
					const uint32_t *reg_base_list,
					const uint32_t reg_base_list_size,
					const uint32_t offset,
					const uint32_t value)
{
	uint32_t i;

	for (i = 0; i < reg_base_list_size; i++) {
		cmdq_pkt_write(pkt, NULL, reg_base_list[i] + offset,
				value, CMDQ_REG_MASK);
	}
}

static void imgsys_qos_set_report_mode(struct cmdq_pkt *pkt)
{
	uint32_t i, sel_reg_value, en_reg_value;

	for (i = 0; i < ARRAY_SIZE(bwr_ctrl_data); i++) {
		if (g_hwqos_sw_en) {
			sel_reg_value = 0;
			en_reg_value = bwr_ctrl_data[i].eng_en_value;
		} else /* hw mode */ {
			sel_reg_value = bwr_ctrl_data[i].eng_en_value;
			en_reg_value = 0;
		}
		cmdq_pkt_write(pkt, NULL,
			BWR_IMG_E1A_BASE + bwr_ctrl_data[i].qos_sel_offset,
			sel_reg_value, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL,
			BWR_IMG_E1A_BASE + bwr_ctrl_data[i].qos_en_offset,
			en_reg_value, CMDQ_REG_MASK);
	}
}

static void imgsys_qos_to_MBps(struct cmdq_pkt *pkt,
				const uint32_t bls_addr,
				const uint16_t cpr_sum_idx)
{
	GCE_OP_DECLARE;

	cmdq_pkt_read(pkt, NULL, bls_addr, SPR2);
	GCE_OP_UPDATE(SPR2, CMDQ_LOGIC_RIGHT_SHIFT,
		VAL, BWR_POINT_RIGHT_SHIFT);

	/* BW = (BW * CPR_URATE_FACTOR) >> g_hwqos_right_shift */
	GCE_OP_UPDATE(SPR2, CMDQ_LOGIC_MULTIPLY,
		IDX, CPR_URATE_FACTOR);
	GCE_OP_UPDATE(SPR2, CMDQ_LOGIC_RIGHT_SHIFT,
		VAL, g_hwqos_right_shift);
	GCE_OP_ASSIGN(cpr_sum_idx,
		IDX, SPR2, CMDQ_LOGIC_ADD,
		IDX, cpr_sum_idx);
}

static void imgsys_qos_set_limiter_en(struct cmdq_pkt *pkt,
	const uint8_t enable)
{
	uint32_t i;
	uint32_t ostdl_val;
	struct qos_limiter *limiter;
	struct qos_limiter_rw *limiter_r, *limiter_w;

	for (i = 0; i < ARRAY_SIZE(limiter_data); i++) {
		limiter = &limiter_data[i];
		limiter_r = &limiter->r;
		limiter_w = &limiter->w;
		if (enable) {
			ostdl_val = field_prep(limiter_r->ostdl_reg_mask, OSTDL_MIN_VALUE) |
					field_prep(limiter_w->ostdl_reg_mask, OSTDL_MIN_VALUE);
			cmdq_pkt_write(pkt, NULL,
				limiter->ostdl,
				ostdl_val,
				limiter_r->ostdl_reg_mask | limiter_w->ostdl_reg_mask);
			cmdq_pkt_write(pkt, NULL,
				limiter->ostdl_en,
				FIELD_PREP(OSTDL_EN_REG_MASK, g_ostdl_en),
				OSTDL_EN_REG_MASK);
		} else {
			cmdq_pkt_write(pkt, NULL,
				limiter->ostdl_en,
				FIELD_PREP(OSTDL_EN_REG_MASK, 0),
				OSTDL_EN_REG_MASK);
		}
	}
}

static void imgsys_qos_init_counter(struct cmdq_pkt *pkt, const bool clear_avg)
{
	uint32_t i;

	GCE_OP_DECLARE;
	GCE_COND_DECLARE;

	cmdq_pkt_assign_command(pkt, CPR_IDX, 0);
	for (i = 0; i < ARRAY_SIZE(qos_map_data); i++) {
		cmdq_pkt_assign_command(pkt, CPR_SUM_R + i * QOS_CPR_STEP, 0);
		cmdq_pkt_assign_command(pkt, CPR_SUM_W + i * QOS_CPR_STEP, 0);
	}
	if (clear_avg) {
		/* active count = 100% = 128 */
		cmdq_pkt_assign_command(pkt, CPR_ACTIVE_COUNT,
			(1 << QOS_AVERAGE_SHIFT));
	}

	cmdq_pkt_write_reg_addr(pkt,
		IMG_QOS_ACTIVE_COUNT_ADDR, CPR_ACTIVE_COUNT, CMDQ_REG_MASK);

	/* case:  active rate > 4/10 */
	GCE_COND_ASSIGN(pkt, SPR1);
	GCE_IF(IDX, CPR_ACTIVE_COUNT, R_CMDQ_GREATER_EQUAL,
		VAL, g_hwqos_active_thre);
	{
		/* x 1 / 1 */
		cmdq_pkt_assign_command(pkt, CPR_URATE_FACTOR, g_hwqos_multiply);
	}
	GCE_ELSE;
	{
		/* x default(4) / 1 */
		cmdq_pkt_assign_command(pkt, CPR_URATE_FACTOR, g_hwqos_active_multiply);
	}
	GCE_FI;
	cmdq_pkt_write_reg_addr(pkt,
		IMG_QOS_URATE_FACTOR_ADDR, CPR_URATE_FACTOR, CMDQ_REG_MASK);
	cmdq_pkt_assign_command(pkt, CPR_ACTIVE_COUNT, 0);
}

static void imgsys_qos_sum_to_MBps(struct cmdq_pkt *pkt)
{
	GCE_OP_DECLARE;
	GCE_COND_DECLARE;

	cmdq_pkt_read(pkt, NULL, BLS_IMG_E1A_BASE + BLS_IMG_LEN_SUM_R_OFT, SPR2);
	cmdq_pkt_read(pkt, NULL, BLS_IMG_E1A_BASE + BLS_IMG_LEN_SUM_W_OFT, SPR3);
	GCE_OP_UPDATE(SPR2, CMDQ_LOGIC_ADD,
		IDX, SPR3);
	GCE_OP_UPDATE(SPR2, CMDQ_LOGIC_RIGHT_SHIFT,
		VAL, (BWR_POINT_RIGHT_SHIFT + BWR_MB_RIGHT_SHIFT));

	/* case:  bw != 0 */
	GCE_COND_ASSIGN(pkt, SPR1);
	GCE_IF(IDX, SPR2, R_CMDQ_NOT_EQUAL,
		VAL, 0);
	{
		GCE_OP_UPDATE(CPR_ACTIVE_COUNT, CMDQ_LOGIC_ADD,
			VAL, 1);
	}
	GCE_FI;
}

static void imgsys_qos_calculate_avg_bw(struct cmdq_pkt *pkt,
	const uint16_t cpr_sum_idx,
	const uint16_t cpr_avg_idx)
{
	GCE_OP_DECLARE;

	GCE_OP_ASSIGN(cpr_avg_idx,
		IDX, cpr_sum_idx, CMDQ_LOGIC_RIGHT_SHIFT,
		VAL, QOS_AVERAGE_SHIFT);
}

static void imgsys_qos_set_ttl_eng_bw(struct cmdq_pkt *pkt,
	const bool is_avg, const uint32_t bwr_bw,
	const uint32_t bwr_ttl_offset, const uint32_t count, ...)
{
	uint32_t i, qos_map_index;
	va_list args;

	GCE_OP_DECLARE;
	va_start(args, count);
	if (is_avg) {
		cmdq_pkt_assign_command(pkt, SPR2, 0);
		for (i = 0; i < count; i++) {
			qos_map_index = va_arg(args, uint32_t);
			GCE_OP_UPDATE(SPR2, CMDQ_LOGIC_ADD,
				IDX, (CPR_AVG_R + qos_map_index * QOS_CPR_STEP));
			GCE_OP_UPDATE(SPR2, CMDQ_LOGIC_ADD,
				IDX, (CPR_AVG_W + qos_map_index * QOS_CPR_STEP));
		}
		GCE_OP_UPDATE(SPR2, CMDQ_LOGIC_RIGHT_SHIFT,
			VAL, QOS_TTL_RIGHT_SHIFT);
		cmdq_pkt_write_reg_addr(pkt,
			BWR_IMG_E1A_BASE + bwr_ttl_offset,
			SPR2, CMDQ_REG_MASK);
	} else {
		cmdq_pkt_write(pkt, NULL,
			BWR_IMG_E1A_BASE + bwr_ttl_offset,
			((bwr_bw >> g_hwqos_max_bw_right_shift) *
				g_hwqos_max_bw_multiply * count) >> QOS_TTL_RIGHT_SHIFT,
			CMDQ_REG_MASK);
	}
	va_end(args);
}

static void imgsys_qos_set_chn_avg_eng_bw(struct cmdq_pkt *pkt,
	const uint32_t bwr_index, const uint32_t count, ...)
{
	uint32_t i, qos_map_index;
	va_list args;

	GCE_OP_DECLARE;
	va_start(args, count);
	/* read */
	cmdq_pkt_assign_command(pkt, SPR2, 0);
	/* write */
	cmdq_pkt_assign_command(pkt, SPR3, 0);
	for (i = 0; i < count; i++) {
		qos_map_index = va_arg(args, uint32_t);
		GCE_OP_UPDATE(SPR2, CMDQ_LOGIC_ADD,
			IDX, (CPR_AVG_R + qos_map_index * QOS_CPR_STEP));
		GCE_OP_UPDATE(SPR3, CMDQ_LOGIC_ADD,
			IDX, (CPR_AVG_W + qos_map_index * QOS_CPR_STEP));
	}
	cmdq_pkt_write_reg_addr(pkt,
		BWR_IMG_E1A_BASE + qos_map_data[bwr_index].bwr_r_offset,
		SPR2, CMDQ_REG_MASK);
	cmdq_pkt_write_reg_addr(pkt,
		BWR_IMG_E1A_BASE + qos_map_data[bwr_index].bwr_w_offset,
		SPR3, CMDQ_REG_MASK);
	va_end(args);
}

static void imgsys_qos_set_fix_comm_ostdl(struct cmdq_pkt *pkt,
	const uint32_t bw,
	const struct qos_limiter *limiter)
{
	uint32_t r_val, w_val;
	const struct qos_limiter_rw *limiter_r = &limiter->r;
	const struct qos_limiter_rw *limiter_w = &limiter->w;

	r_val = bw >> limiter_r->ostdl_right_shift;
	r_val = clamp_t(u32, r_val, OSTDL_MIN_VALUE, OSTDL_MAX_VALUE);
	w_val = bw >> limiter_w->ostdl_right_shift;
	w_val = clamp_t(u32, w_val, OSTDL_MIN_VALUE, OSTDL_MAX_VALUE);

	cmdq_pkt_write(pkt, NULL,
		limiter->ostdl,
		field_prep(limiter_r->ostdl_reg_mask, r_val) |
		field_prep(limiter_w->ostdl_reg_mask, w_val),
		limiter_r->ostdl_reg_mask | limiter_w->ostdl_reg_mask);

}

static void imgsys_qos_set_comm_ostdl(struct cmdq_pkt *pkt,
	const struct qos_limiter *limiter,
	const struct qos_limiter_rw *limiter_rw,
	const struct limiter_param *qos_param)
{
	GCE_OP_DECLARE;
	GCE_COND_DECLARE;
	GCE_COND_ASSIGN(pkt, SPR1);
	GCE_OP_ASSIGN(SPR2,
		IDX, qos_param->cpr_avg_idx, CMDQ_LOGIC_RIGHT_SHIFT,
		VAL, limiter_rw->ostdl_right_shift);
	GCE_OP_UPDATE(SPR2, CMDQ_LOGIC_ADD,
		VAL, 1);

	/* case: ostdl > OSTDL_MAX_VALUE */
	GCE_IF(IDX, SPR2, R_CMDQ_GREATER,
		VAL, OSTDL_MAX_VALUE);
	{
		cmdq_pkt_assign_command(pkt, SPR2, OSTDL_MAX_VALUE);
	}
	GCE_FI;

	GCE_OP_UPDATE(SPR2, CMDQ_LOGIC_LEFT_SHIFT,
		VAL, limiter_rw->ostdl_reg_l);
	cmdq_pkt_write_reg_addr(pkt, limiter->ostdl,
		SPR2, limiter_rw->ostdl_reg_mask);
}

static void imgsys_qos_calculate_limiter(struct cmdq_pkt *pkt,
	const bool is_avg,
	const uint32_t bw,
	const uint8_t limiter_idx,
	const uint32_t count, ...)
{
	uint32_t i, qos_map_index;
	struct qos_limiter *limiter = &limiter_data[limiter_idx];
	struct qos_limiter_rw *limiter_r = &limiter->r;
	struct qos_limiter_rw *limiter_w = &limiter->w;
	va_list args;

	GCE_OP_DECLARE;
	va_start(args, count);
	if (is_avg) {
		cmdq_pkt_assign_command(pkt, CPR_CORE_SUM_R, 0);
		cmdq_pkt_assign_command(pkt, CPR_CORE_SUM_W, 0);
		for (i = 0; i < count; i++) {
			qos_map_index = va_arg(args, uint32_t);
			GCE_OP_UPDATE(CPR_CORE_SUM_R, CMDQ_LOGIC_ADD,
				IDX, (CPR_AVG_R + qos_map_index * QOS_CPR_STEP));
			GCE_OP_UPDATE(CPR_CORE_SUM_W, CMDQ_LOGIC_ADD,
				IDX, CPR_AVG_W + qos_map_index * QOS_CPR_STEP);
		}
		/* Change Unit to MB/s */
		GCE_OP_UPDATE(CPR_CORE_SUM_R, CMDQ_LOGIC_RIGHT_SHIFT,
			VAL, BWR_MB_RIGHT_SHIFT);
		GCE_OP_UPDATE(CPR_CORE_SUM_W, CMDQ_LOGIC_RIGHT_SHIFT,
			VAL, BWR_MB_RIGHT_SHIFT);
		/* ostdl */
		imgsys_qos_set_comm_ostdl(pkt, limiter, limiter_r, &limiter_param_r);
		imgsys_qos_set_comm_ostdl(pkt, limiter, limiter_w, &limiter_param_w);
	} else {
		/* use single channel BW to calculate */
		/* ostdl */
		imgsys_qos_set_fix_comm_ostdl(pkt, bw, limiter);
	}
	va_end(args);
}

static void imgsys_qos_set_bw_ratio(struct cmdq_pkt *pkt)
{
	uint32_t i, reg = 0;
	uint32_t bwr_ttl_bw_rat_offset_array[] = {
		BWR_IMG_SRT_EMI_ENG_BW_RAT0_OFT,
		BWR_IMG_SRT_EMI_ENG_BW_RAT1_OFT,
		BWR_IMG_SRT_EMI_ENG_BW_RAT2_OFT,
		BWR_IMG_SRT_EMI_ENG_BW_RAT3_OFT,
		BWR_IMG_SRT_EMI_ENG_BW_RAT4_OFT,
		BWR_IMG_SRT_EMI_ENG_BW_RAT6_OFT,
		BWR_IMG_SRT_EMI_ENG_BW_RAT7_OFT,
		BWR_IMG_SRT_EMI_ENG_BW_RAT8_OFT,
		BWR_IMG_SRT_EMI_ENG_BW_RAT9_OFT,
	};

	/* set bwr read & write reg */
	for (i = 0; i < ARRAY_SIZE(qos_map_data); i++) {
		if (reg == qos_map_data[i].bwr_r_offset) {
			/* skip current BWR value since same as previous */
			continue;
		}
		reg = qos_map_data[i].bwr_r_offset;
		cmdq_pkt_write(pkt, NULL,
				BWR_IMG_E1A_BASE + qos_map_data[i].bwr_r_rat_offset,
				BWR_IMG_SRT_ENG_BW_RAT, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL,
				BWR_IMG_E1A_BASE + qos_map_data[i].bwr_w_rat_offset,
				BWR_IMG_SRT_ENG_BW_RAT, CMDQ_REG_MASK);
	}
	for (i = 0; i < ARRAY_SIZE(bwr_ttl_bw_rat_offset_array); i++) {
		cmdq_pkt_write(pkt, NULL,
				BWR_IMG_E1A_BASE + bwr_ttl_bw_rat_offset_array[i],
				BWR_IMG_SRT_ENG_BW_RAT, CMDQ_REG_MASK);
	}
}

static void imgsys_qos_set_mae_floor_bw(struct cmdq_pkt *pkt)
{
	uint16_t cpr_idx;
	uint32_t mae_floor_bw;

	GCE_OP_DECLARE;
	GCE_COND_DECLARE;
	GCE_COND_ASSIGN(pkt, SPR1);

	cpr_idx = CPR_AVG_R + 8 * QOS_CPR_STEP;
	mae_floor_bw = clamp_t(u32, (g_ostdl_mae_min_r - 1), OSTDL_MIN_VALUE, OSTDL_MAX_VALUE)
			<< (OSTDL_R2_RIGHT_SHIFT + BWR_MB_RIGHT_SHIFT);
	GCE_IF(IDX, cpr_idx, R_CMDQ_LESS,
		VAL, mae_floor_bw);
	{
		cmdq_pkt_assign_command(pkt, cpr_idx, mae_floor_bw);
	}
	GCE_FI;

	cpr_idx = CPR_AVG_W + 8 * QOS_CPR_STEP;
	mae_floor_bw = clamp_t(u32, (g_ostdl_mae_min_w - 1), OSTDL_MIN_VALUE, OSTDL_MAX_VALUE)
			<< (OSTDL_W2_RIGHT_SHIFT + BWR_MB_RIGHT_SHIFT);
	GCE_IF(IDX, cpr_idx, R_CMDQ_LESS,
		VAL, mae_floor_bw);
	{
		cmdq_pkt_assign_command(pkt, cpr_idx, mae_floor_bw);
	}
	GCE_FI;
}

static void imgsys_qos_set_chn_avg_bw(struct cmdq_pkt *pkt)
{
	imgsys_qos_set_chn_avg_eng_bw(pkt, 0, 1 /* count */, 0);
	imgsys_qos_set_chn_avg_eng_bw(pkt, 1, 1 /* count */, 1);
	imgsys_qos_set_chn_avg_eng_bw(pkt, 2, 1 /* count */, 2);
	imgsys_qos_set_chn_avg_eng_bw(pkt, 3, 1 /* count */, 3);
	imgsys_qos_set_chn_avg_eng_bw(pkt, 4, 2 /* count */, 4, 5);
	imgsys_qos_set_chn_avg_eng_bw(pkt, 6, 1 /* count */, 6);
	imgsys_qos_set_chn_avg_eng_bw(pkt, 7, 1 /* count */, 7);
	imgsys_qos_set_chn_avg_eng_bw(pkt, 8, 1 /* count */, 8);
	imgsys_qos_set_chn_avg_eng_bw(pkt, 9, 2 /* count */, 9, 10);
	imgsys_qos_set_chn_avg_eng_bw(pkt, 11, 1 /* count */, 11);
}

static void imgsys_qos_set_ttl_bw(struct cmdq_pkt *pkt,
	const bool is_avg, const uint32_t bwr_bw)
{
	imgsys_qos_set_ttl_eng_bw(pkt, is_avg, bwr_bw,
		BWR_IMG_SRT_EMI_ENG_BW0_OFT, 1 /* count */, 1);
	imgsys_qos_set_ttl_eng_bw(pkt, is_avg, bwr_bw,
		BWR_IMG_SRT_EMI_ENG_BW1_OFT, 1 /* count */, 2);
	imgsys_qos_set_ttl_eng_bw(pkt, is_avg, bwr_bw,
		BWR_IMG_SRT_EMI_ENG_BW2_OFT, 1 /* count */, 6);
	imgsys_qos_set_ttl_eng_bw(pkt, is_avg, bwr_bw,
		BWR_IMG_SRT_EMI_ENG_BW3_OFT, 2 /* count */, 9, 10);
	imgsys_qos_set_ttl_eng_bw(pkt, is_avg, bwr_bw,
		BWR_IMG_SRT_EMI_ENG_BW4_OFT, 4 /* count */, 0, 3, 4, 5);
	imgsys_qos_set_ttl_eng_bw(pkt, is_avg, bwr_bw,
		BWR_IMG_SRT_EMI_ENG_BW6_OFT, 1 /* count */, 7);
	imgsys_qos_set_ttl_eng_bw(pkt, is_avg, bwr_bw,
		BWR_IMG_SRT_EMI_ENG_BW7_OFT, 1 /* count */, 8);
	if (is_avg) {
		/* set DFP when avg BW */
		imgsys_qos_set_ttl_eng_bw(pkt, is_avg, bwr_bw,
			BWR_IMG_SRT_EMI_ENG_BW9_OFT, 1 /* count */, 11);
	}
}

static void imgsys_get_mminfra_freq(struct cmdq_pkt *pkt)
{
	uint32_t i;

	GCE_OP_DECLARE;
	GCE_COND_DECLARE;
	cmdq_pkt_assign_command(pkt, CPR_MMINFRA_FREQ, mminfra_gear_data[0].freq);
	cmdq_pkt_read(pkt, NULL, MMINFRA_CLK_REG, SPR2);

	GCE_OP_UPDATE(SPR2, CMDQ_LOGIC_RIGHT_SHIFT,
		VAL, MMINFRA_CLK_REG_L);

	GCE_OP_UPDATE(SPR2, CMDQ_LOGIC_AND,
		VAL, (GENMASK((MMINFRA_CLK_REG_H - MMINFRA_CLK_REG_L), 0)));

	GCE_COND_ASSIGN(pkt, SPR1);

	for (i = 0; i < ARRAY_SIZE(mminfra_gear_data); i++) {
		GCE_IF(IDX, SPR2, R_CMDQ_EQUAL,
			VAL, mminfra_gear_data[i].level);
		{
			cmdq_pkt_assign_command(pkt,
				CPR_MMINFRA_FREQ, mminfra_gear_data[i].freq);
		}
		GCE_FI;
	}

	for (i = 0; i < ARRAY_SIZE(ostdbl_lut_data); i++) {
		GCE_IF(IDX, CPR_MMINFRA_FREQ, R_CMDQ_GREATER_EQUAL,
			VAL, ostdbl_lut_data[i].min_freq);
		{
			cmdq_pkt_assign_command(pkt,
				CPR_NPS_R, ostdbl_lut_data[i].nps_r);
			cmdq_pkt_assign_command(pkt,
				CPR_NPS_W, ostdbl_lut_data[i].nps_w);
		}
		GCE_FI;
	}

	/* LINE_BW = MMINFRA_FREQ x 16 */
	GCE_OP_ASSIGN(CPR_LINE_BW,
		IDX, CPR_MMINFRA_FREQ, CMDQ_LOGIC_MULTIPLY,
		VAL, 16);
}

static void imgsys_qos_set_limiter(struct cmdq_pkt *pkt,
	const bool is_avg, const uint32_t bw)
{
	if (is_avg)
		imgsys_get_mminfra_freq(pkt);

	imgsys_qos_calculate_limiter(pkt,
		is_avg, bw,
		0, /* limiter idx */
		4  /* count */, 0, 1, 4, 5);
	imgsys_qos_calculate_limiter(pkt,
		is_avg, bw,
		2, /* limiter idx */
		4  /* count */, 8, 9, 10, 11);
	imgsys_qos_calculate_limiter(pkt,
		is_avg, bw,
		1, /* limiter idx */
		4  /* count */, 2, 3, 6, 7);
}

static void imgsys_qos_set_fix_bw(struct cmdq_pkt *pkt,
	const uint32_t bwr_bw)
{
	uint32_t i, bw, reg = 0;
	uint32_t current_bwr_bw = bwr_bw;

	/* set bwr bw and ostdl */
	bw = bwr_bw >> BWR_BW_POINT;
	for (i = 0; i < ARRAY_SIZE(qos_map_data); i++) {
		if (reg == qos_map_data[i].bwr_r_offset) {
			/* accumulate previous bwr_bw for sharing reg */
			current_bwr_bw += bwr_bw;
		} else {
			/* reset to initial bwr_bw */
			current_bwr_bw = bwr_bw;
		}
		reg = qos_map_data[i].bwr_r_offset;
		/* skip DFP for high BW */
		if (current_bwr_bw != 0 &&
			qos_map_data[i].engine == BWR_DWPE__DFP__DVS) {
			continue;
		}
		cmdq_pkt_write(pkt, NULL,
				BWR_IMG_E1A_BASE + qos_map_data[i].bwr_r_offset,
				current_bwr_bw, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL,
				BWR_IMG_E1A_BASE + qos_map_data[i].bwr_w_offset,
				current_bwr_bw, CMDQ_REG_MASK);
	}
	imgsys_qos_set_limiter(pkt, /* is_avg */ false, bw);
	if (bwr_bw == 0) {
		for (i = 0; i < ARRAY_SIZE(bwr_ttl_bw_array); i++) {
			cmdq_pkt_write(pkt, NULL,
					bwr_ttl_bw_array[i].addr,
					0, CMDQ_REG_MASK);
		}
	} else {
		imgsys_qos_set_ttl_bw(pkt, /* is_avg */ false, bwr_bw);
	}
}

static void imgsys_qos_set_bw(struct cmdq_pkt *pkt)
{
	uint32_t i;

	GCE_OP_DECLARE;
	GCE_COND_DECLARE;

	for (i = 0; i < ARRAY_SIZE(qos_map_data); i++) {
		imgsys_qos_to_MBps(pkt,
			qos_map_data[i].bls_base + BLS_IMG_LEN_SUM_R_OFT,
			CPR_SUM_R + i * QOS_CPR_STEP);
		imgsys_qos_to_MBps(pkt,
			qos_map_data[i].bls_base + BLS_IMG_LEN_SUM_W_OFT,
			CPR_SUM_W + i * QOS_CPR_STEP);
	}

	GCE_OP_UPDATE(CPR_IDX, CMDQ_LOGIC_ADD,
		VAL, 1);

	GCE_COND_ASSIGN(pkt, SPR1);
	/* 128 ms */
	/* case: CPR_IDX == 128 */
	GCE_IF(IDX, CPR_IDX, R_CMDQ_GREATER_EQUAL,
		VAL, (1 << QOS_AVERAGE_SHIFT));
	{
		for (i = 0; i < ARRAY_SIZE(qos_map_data); i++) {
			/* calculate avg read/write BW */
			imgsys_qos_calculate_avg_bw(pkt,
				CPR_SUM_R + i * QOS_CPR_STEP,
				CPR_AVG_R + i * QOS_CPR_STEP);
			imgsys_qos_calculate_avg_bw(pkt,
				CPR_SUM_W + i * QOS_CPR_STEP,
				CPR_AVG_W + i * QOS_CPR_STEP);
		}
		imgsys_qos_set_mae_floor_bw(pkt);
		imgsys_qos_set_chn_avg_bw(pkt);
		imgsys_qos_set_ttl_bw(pkt, /* is_avg */ true, 0);
		imgsys_qos_set_limiter(pkt, /* is_avg */ true, 0);
		imgsys_qos_init_counter(pkt, false);
	}
	GCE_FI;
	imgsys_qos_sum_to_MBps(pkt);
}

static void imgsys_qos_config_bwr(struct cmdq_pkt *pkt,
	const enum BWR_CTRL bwr_ctrl)
{
	switch (bwr_ctrl) {
	case BWR_START:
		imgsys_reg_write(pkt,
			bwr_base_array, ARRAY_SIZE(bwr_base_array),
			bwr_init_data, ARRAY_SIZE(bwr_init_data));
		imgsys_qos_set_bw_ratio(pkt);
		imgsys_qos_set_report_mode(pkt);
		cmdq_pkt_write(pkt, NULL, BWR_IMG_E1A_BASE + BWR_IMG_MTCMOS_EN_VLD_OFT,
			BWR_IMG_MTCMOS_EN_VLD_EN, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, BWR_IMG_E1A_BASE + BWR_IMG_RPT_CTRL_OFT,
			BIT(BWR_IMG_RPT_START), CMDQ_REG_MASK);
		break;
	case BWR_STOP:
		/* Report 0 */
		imgsys_qos_set_fix_bw(pkt, 0);
		/* Wait for 150 us (BWR_IMG_RPT_TIMER is 100 us) */
		cmdq_pkt_sleep(pkt, CMDQ_US_TO_TICK(150), 0 /*don't care*/);
		cmdq_pkt_poll_sleep(pkt, 0x1,
			BWR_IMG_E1A_BASE + BWR_IMG_SEND_BW_ZERO_OFT, CMDQ_REG_MASK);
		cmdq_pkt_poll_sleep(pkt, BIT(BWR_IMG_RPT_WAIT),
			BWR_IMG_E1A_BASE + BWR_IMG_RPT_STATE_OFT, CMDQ_REG_MASK);

		/* Terminate flow */
		cmdq_pkt_write(pkt, NULL, BWR_IMG_E1A_BASE + BWR_IMG_RPT_CTRL_OFT,
			BIT(BWR_IMG_RPT_END), CMDQ_REG_MASK);
		cmdq_pkt_poll_sleep(pkt, BIT(BWR_IMG_RPT_WAIT),
			BWR_IMG_E1A_BASE + BWR_IMG_RPT_STATE_OFT, CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, BWR_IMG_E1A_BASE + BWR_IMG_RPT_CTRL_OFT,
			BIT(BWR_IMG_RPT_RST), CMDQ_REG_MASK);
		cmdq_pkt_write(pkt, NULL, BWR_IMG_E1A_BASE + BWR_IMG_MTCMOS_EN_VLD_OFT,
			0, CMDQ_REG_MASK);
		break;
	}
}

static void imgsys_qos_config_bls(struct cmdq_pkt *pkt,
	const enum BLS_CTRL bls_ctrl)
{
	switch (bls_ctrl) {
	case BLS_INIT:
		imgsys_reg_write_base_list(pkt,
			bls_base_array, ARRAY_SIZE(bls_base_array),
			BLS_IMG_CTRL_OFT, 0x1);
		imgsys_reg_write(pkt,
			bls_base_array, ARRAY_SIZE(bls_base_array),
			bls_init_data, ARRAY_SIZE(bls_init_data));
		break;
	case BLS_TRIG:
		imgsys_reg_write_base_list(pkt,
			bls_base_array, ARRAY_SIZE(bls_base_array),
			BLS_IMG_SW_TRIG_START_OFT, 0x1);
		break;
	case BLS_STOP:
		imgsys_reg_write_base_list(pkt,
			bls_base_array, ARRAY_SIZE(bls_base_array),
			BLS_IMG_CTRL_OFT, 0x0);
		break;
	}
}

static void imgsys_qos_report_switch(struct cmdq_pkt *pkt,
	const enum QOS_STATE qos_state)
{
	switch (qos_state) {
	case QOS_STATE_MAX:
		imgsys_qos_set_fix_bw(pkt,
			FLOAT2FIX(g_hwqos_max_rw_eng_bw, BWR_BW_POINT));
		cmdq_pkt_sleep(pkt, CMDQ_US_TO_TICK(IMGSYS_QOS_INTERVAL_US), 0 /*don't care*/);
		imgsys_qos_config_bls(pkt, BLS_TRIG);
		imgsys_qos_init_counter(pkt, false);
		break;
	case QOS_STATE_NORMAL:
		cmdq_pkt_sleep(pkt, CMDQ_US_TO_TICK(IMGSYS_QOS_INTERVAL_US), 0 /*don't care*/);
		imgsys_qos_config_bls(pkt, BLS_TRIG);
		imgsys_qos_set_bw(pkt);
		break;
	default:
		pr_err("[%s] enum QOS_STATE(%d) is not supported\n",
			__func__, qos_state);
		break;
	}
}

static void imgsys_qos_loop(const struct mtk_imgsys_hwqos *hwqos_info)
{
	GCE_OP_DECLARE;
	GCE_COND_DECLARE;

	g_hwqos_pkt = cmdq_pkt_create(g_hwqos_clt);
	if (!g_hwqos_pkt) {
		pr_err("[%s] [ERROR] cmdq_pkt_create fail\n", __func__);
		return;
	}
	g_hwqos_pkt->no_irq = true;
	/* check if hwqos_buf is IMGSYS_QOS_REPORT_MAX */
	GCE_COND_ASSIGN(g_hwqos_pkt, SPR1);
	cmdq_pkt_acquire_event(g_hwqos_pkt, hwqos_info->hwqos_sync_token);
	cmdq_pkt_read(g_hwqos_pkt, NULL, g_hwqos_buf_pa, SPR2);
	GCE_IF(IDX, SPR2, R_CMDQ_EQUAL,
		VAL, IMGSYS_QOS_REPORT_MAX);
	/* case: hwqos_buf == IMGSYS_QOS_REPORT_MAX */
	cmdq_pkt_write(g_hwqos_pkt, NULL, g_hwqos_buf_pa,
			IMGSYS_QOS_REPORT_NORMAL, CMDQ_REG_MASK);
	cmdq_pkt_clear_event(g_hwqos_pkt, hwqos_info->hwqos_sync_token);
	imgsys_qos_report_switch(g_hwqos_pkt, QOS_STATE_MAX);
	GCE_ELSE;
	/* case: hwqos_buf == IMGSYS_QOS_REPORT_NORMAL */
	cmdq_pkt_clear_event(g_hwqos_pkt, hwqos_info->hwqos_sync_token);
	imgsys_qos_report_switch(g_hwqos_pkt, QOS_STATE_NORMAL);
	GCE_FI;
	cmdq_pkt_finalize_loop(g_hwqos_pkt);
	cmdq_pkt_flush_async(g_hwqos_pkt, NULL, NULL);
}

/** Global api
 * @brief List Global api
 */

void mtk_imgsys_cmdq_hwqos_init(struct mtk_imgsys_dev *imgsys_dev)
{
	struct device *dev = imgsys_dev->dev;
	struct mtk_imgsys_hwqos *hwqos_info = &imgsys_dev->hwqos_info;

	hwqos_info->hwqos_support = of_property_read_bool(
		imgsys_dev->dev->of_node, "mediatek,imgsys-hwqos-support");
	g_hwqos_support = hwqos_info->hwqos_support;
	if (!hwqos_info->hwqos_support)
		return;

	of_property_read_u16(imgsys_dev->dev->of_node,
				"hwqos-sync-token",
				&hwqos_info->hwqos_sync_token);

	g_hwqos_clt = cmdq_mbox_create(dev, IMGSYS_QOS_THD_IDX);

	pr_info("[%s] sync_token(%d), thd_idx(%d), clt(0x%lx)\n", __func__,
		hwqos_info->hwqos_sync_token,
		IMGSYS_QOS_THD_IDX,
		(unsigned long) g_hwqos_clt);
}

void mtk_imgsys_cmdq_hwqos_release(void)
{
	if (!g_hwqos_clt) {
		pr_err("[%s] g_hwqos_clt is NULL\n", __func__);
		return;
	}
	cmdq_mbox_destroy(g_hwqos_clt);
	g_hwqos_clt = NULL;
}

void mtk_imgsys_cmdq_hwqos_streamon(const struct mtk_imgsys_hwqos *hwqos_info)
{
	struct cmdq_pkt *pkt;

	g_hwqos_state = QOS_STATE_INIT;
	if (g_hwqos_pkt) {
		pr_info("[%s] g_hwqos_pkt is already exists, skip\n", __func__);
		return;
	}

	SET_LIMITER_PARAM(r);
	SET_LIMITER_PARAM(w);

	cmdq_mbox_enable(g_hwqos_clt->chan);
	cmdq_clear_event(g_hwqos_clt->chan, hwqos_info->hwqos_sync_token);

	if (!g_hwqos_buf_va || !g_hwqos_buf_pa)
		g_hwqos_buf_va = cmdq_mbox_buf_alloc(g_hwqos_clt, &g_hwqos_buf_pa);

	if (!g_hwqos_buf_va || !g_hwqos_buf_pa) {
		pr_err("[%s] [ERROR] cmdq_mbox_buf_alloc fail\n", __func__);
		g_hwqos_buf_va = NULL;
		g_hwqos_buf_pa = 0;
		return;
	}

	*g_hwqos_buf_va = IMGSYS_QOS_REPORT_NORMAL;
	pkt = cmdq_pkt_create(g_hwqos_clt);
	if (!pkt) {
		pr_err("[%s] [ERROR] cmdq_pkt_create fail\n", __func__);
		return;
	}
	imgsys_qos_config_bls(pkt, BLS_INIT);
	imgsys_qos_config_bwr(pkt, BWR_START);
	imgsys_qos_set_limiter_en(pkt, 1);
	imgsys_qos_init_counter(pkt, true);
	cmdq_pkt_flush(pkt);
	cmdq_pkt_destroy(pkt);
	MTK_IMGSYS_QOS_ENABLE(g_hwqos_dbg_en,
		imgsys_hwqos_dbg_reg_dump(3);
	);
	imgsys_qos_loop(hwqos_info);
	imgsys_hwqos_set_dbg_thread(true);
}

void mtk_imgsys_cmdq_hwqos_streamoff(void)
{
	struct cmdq_pkt *pkt;

	g_hwqos_state = QOS_STATE_UNINIT;
	if (g_hwqos_pkt) {
		cmdq_mbox_stop(g_hwqos_clt);
		cmdq_pkt_destroy(g_hwqos_pkt);
		g_hwqos_pkt = NULL;
	}

	if (g_hwqos_buf_va && g_hwqos_buf_pa) {
		cmdq_mbox_buf_free(g_hwqos_clt, g_hwqos_buf_va, g_hwqos_buf_pa);
		g_hwqos_buf_va = NULL;
		g_hwqos_buf_pa = 0;
	}

	pkt = cmdq_pkt_create(g_hwqos_clt);
	if (pkt) {
		imgsys_qos_config_bwr(pkt, BWR_STOP);
		imgsys_qos_config_bls(pkt, BLS_STOP);
		imgsys_qos_set_limiter_en(pkt, 0);
		cmdq_pkt_flush(pkt);
		cmdq_pkt_destroy(pkt);
	} else {
		pr_err("[%s] [ERROR] cmdq_pkt_create fail\n", __func__);
	}
	imgsys_hwqos_set_dbg_thread(false);
	cmdq_mbox_disable(g_hwqos_clt->chan);
	MTK_IMGSYS_QOS_ENABLE(g_hwqos_dbg_en,
		imgsys_hwqos_dbg_reg_dump(3);
	);
}

void mtk_imgsys_cmdq_hwqos_report(
	struct cmdq_pkt *pkt,
	const struct mtk_imgsys_hwqos *hwqos_info,
	const int *fps,
	const uint8_t *boost)
{
	if ((*fps == 0) || (*boost == 0xFF) ||
		(g_hwqos_state == QOS_STATE_INIT) || (g_hwqos_high_bw)) {
		if (g_hwqos_buf_pa && g_hwqos_buf_va) {
			cmdq_pkt_acquire_event(pkt, hwqos_info->hwqos_sync_token);
			cmdq_pkt_write(pkt, NULL, g_hwqos_buf_pa,
					IMGSYS_QOS_REPORT_MAX, CMDQ_REG_MASK);
			cmdq_pkt_clear_event(pkt, hwqos_info->hwqos_sync_token);
		}
		if (g_hwqos_state == QOS_STATE_INIT)
			g_hwqos_state = QOS_STATE_MAX;
	}
}

static int imgsys_hwqos_set_status(const char *val, const struct kernel_param *kp)
{
	u32 hwqos_log_level;

	if (kstrtou32(val, 0, &hwqos_log_level)) {
		pr_err("[%s] param fail\n", __func__);
		return 0;
	}
	if (!g_hwqos_support) {
		pr_err("[%s] hwqos is not supported\n", __func__);
		return 0;
	}

	if ((g_hwqos_state == QOS_STATE_INIT) || (g_hwqos_state == QOS_STATE_UNINIT)) {
		pr_err("[%s] hwqos is not ready\n", __func__);
		return 0;
	}
	imgsys_hwqos_dbg_reg_dump(hwqos_log_level);
	return 0;
}

static const struct kernel_param_ops imgsys_hwqos_status_ops = {
	.set = imgsys_hwqos_set_status,
};

module_param_cb(imgsys_hwqos_status, &imgsys_hwqos_status_ops, NULL, 0644);
MODULE_PARM_DESC(imgsys_hwqos_status, "imgsys hwqos status");
