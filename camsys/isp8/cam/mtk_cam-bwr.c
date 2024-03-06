// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/vmalloc.h>
#include <linux/suspend.h>


#include "mtk_cam-bwr.h"
#include "mtk_cam-bwr_regs.h"
#include "mtk_cam-debug_option.h"

static int debug_bwr_mode = BWR_SW_MODE;
module_param(debug_bwr_mode, int, 0644);
MODULE_PARM_DESC(debug_bwr_mode, "0: sw mode, 1 : hw mode");

static int debug_bwr_test;
module_param(debug_bwr_test, int, 0644);
MODULE_PARM_DESC(debug_bwr_test, "debug bwr test");

static int debug_bwr_dump;
module_param(debug_bwr_dump, int, 0644);
MODULE_PARM_DESC(debug_bwr_dump, "debug bwr dump");

#define CHANNEL_OFFSET   0x80
#define ENGINE_OFFSET    0x4
#define NUM_BW_CHANNEL   18

#define CLK_CYC_PER_US 26 // default 26MHz

#define BWR_BW_DECIMAL_POS       3
#define BWR_BW_DECIMAL_WIDTH     15
#define BWR_RAT_DECIMAL_POS      7

#define BWR_SRT_TTL_OCC_FACTOR       170  //1.33
#define BWR_HRT_TTL_OCC_FACTOR       149  //1.17
#define BWR_SRT_BW_OCC_FACTOR        181  //1.42
#define BWR_HRT_BW_OCC_FACTOR        181  //1.42

//unit : MB/s
#define BWR_DEFAULT_DPE_SRT_R        537
#define BWR_DEFAULT_DPE_SRT_W        234
#define BWR_DEFAULT_PDA_SRT_R        258
#define BWR_DEFAULT_PDA_SRT_W        39
#define BWR_DEFAULT_UISP_SRT_R       3
#define BWR_DEFAULT_UISP_SRT_W       231
#define BWR_DEFAULT_UISP_HRT_R       1
#define BWR_DEFAULT_UISP_HRT_W       7

/* E1 workaround */
#define BWR_CAM_PROTOCOL0		0x3100020
#define BWR_CAM_PROTOCOL1		0x4120213
#define BWR_CAM_PROTOCOL2		0x18082814
#define BWR_CAM_PROTOCOL3		0x1a0a1b0b
#define BWR_CAM_PROTOCOL4		0x1c0c

static inline u32 to_bw_csr(int MBs)
{
	return (MBs > 0 && MBs < (1 << BWR_BW_DECIMAL_WIDTH)) ?
			MBs << BWR_BW_DECIMAL_POS : 0;
}

static inline u32 to_ratio_csr(int MBs)
{
	return MBs << BWR_RAT_DECIMAL_POS;
}

static inline u32 to_bw_val(int MBs)
{
	return MBs >> BWR_BW_DECIMAL_POS;
}

static inline u32 to_ratio_val(int MBs)
{
	return MBs >> BWR_RAT_DECIMAL_POS;
}

static const char *value_to_str(const char * const str_arr[], size_t size,
				int value)
{
	const char *str;

	if (WARN_ON((unsigned int)value >= size))
		return "(not-found)";

	str = str_arr[value];
	return str ? str : "null";
}

const char *str_engine(int event)
{
	static const char * const str[] = {
		[ENGINE_SUB_A] = "sub_a",
		[ENGINE_SUB_B] = "sub_b",
		[ENGINE_SUB_C] = "sub_c",
		[ENGINE_MRAW] = "mraw",
		[ENGINE_CAM_MAIN] = "cam_main",
		[ENGINE_CAMSV_B] = "camsv_b",
		[ENGINE_CAMSV_A] = "camsv_a",
		[ENGINE_DPE] = "dpe",
		[ENGINE_PDA] = "pda",
		[ENGINE_CAMSV_OTHER] = "camsv_other",
		[ENGINE_UISP] = "uisp",
	};

	return value_to_str(str, ARRAY_SIZE(str), event);
}

const char *str_axi_port(int event)
{
	static const char * const str[] = {
		[DISP_PORT] = "disp",
		[MDP0_PORT] = "mdp0",
		[MDP1_PORT] = "mdp1",
		[SYS_PORT] = "sys",
	};

	return value_to_str(str, ARRAY_SIZE(str), event);
}

static void bwr_set_chn_bw(struct mtk_bwr_device *bwr,
			  enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi,
			  int srt_r_bw, int srt_w_bw, int hrt_r_bw, int hrt_w_bw, bool clr)
{
	int bw, offset;

	mutex_lock(&bwr->op_lock);

	offset = CHANNEL_OFFSET * axi + ENGINE_OFFSET * engine;

	//SRT bandwidth
	bw = clr ? srt_r_bw :
			to_bw_val(readl(bwr->base + REG_BWR_CAM_SRT_R0_ENG_BW0_0 + offset)) + srt_r_bw;
	writel(to_bw_csr(bw),bwr->base + REG_BWR_CAM_SRT_R0_ENG_BW0_0 + offset);

	bw = clr ? srt_w_bw :
			to_bw_val(readl(bwr->base + REG_BWR_CAM_SRT_W0_ENG_BW0_0 + offset)) + srt_w_bw;
	writel(to_bw_csr(bw), bwr->base + REG_BWR_CAM_SRT_W0_ENG_BW0_0 + offset);

	//HRT bandwidth
	bw = clr ? hrt_r_bw :
			to_bw_val(readl(bwr->base + REG_BWR_CAM_HRT_R0_ENG_BW0_0 + offset)) + hrt_r_bw;
	writel(to_bw_csr(bw), bwr->base + REG_BWR_CAM_HRT_R0_ENG_BW0_0 + offset);

	bw = clr ? hrt_w_bw :
			to_bw_val(readl(bwr->base + REG_BWR_CAM_HRT_W0_ENG_BW0_0 + offset)) + hrt_w_bw;
	writel(to_bw_csr(bw), bwr->base + REG_BWR_CAM_HRT_W0_ENG_BW0_0 + offset);
	if (CAM_DEBUG_ENABLED(MMQOS))
		pr_info("%s: engine:%d, axi:%d SRT(r/w): %d/%d HRT(r/w): %d/%d, clear: %d\n",
		__func__, engine, axi, srt_r_bw, srt_w_bw, hrt_r_bw, hrt_w_bw, clr);

	mutex_unlock(&bwr->op_lock);
}

static void bwr_set_ttl_bw(struct mtk_bwr_device *bwr,
		enum BWR_ENGINE_TYPE engine, int srt_ttl, int hrt_ttl, bool clr)
{
	int bw, offset;

	mutex_lock(&bwr->op_lock);

	offset = ENGINE_OFFSET * engine;

	//SRT bandwidth
	bw = clr ? srt_ttl :
			to_bw_val(readl(bwr->base + REG_BWR_CAM_SRT_TTL_ENG_BW0 + offset)) + srt_ttl;
	writel(to_bw_csr(bw),bwr->base + REG_BWR_CAM_SRT_TTL_ENG_BW0 + offset);

	bw = clr ? hrt_ttl :
			to_bw_val(readl(bwr->base + REG_BWR_CAM_HRT_TTL_ENG_BW0 + offset)) + hrt_ttl;
	writel(to_bw_csr(bw), bwr->base + REG_BWR_CAM_HRT_TTL_ENG_BW0 + offset);
	if (CAM_DEBUG_ENABLED(MMQOS))
		pr_info("%s: engine:%d, SRT_TTL/HRT_TTL: %d/%d, clear: %d\n",
				__func__, engine, srt_ttl, hrt_ttl, clr);

	mutex_unlock(&bwr->op_lock);
}

static void bwr_zero_bw(struct mtk_bwr_device *bwr,
			  enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi)
{
	mutex_lock(&bwr->op_lock);

	//SRT bandwidth
	writel(0, bwr->base +
		REG_BWR_CAM_SRT_R0_ENG_BW0_0 + CHANNEL_OFFSET * axi +
			ENGINE_OFFSET * engine);

	writel(0, bwr->base +
		REG_BWR_CAM_SRT_W0_ENG_BW0_0 + CHANNEL_OFFSET * axi +
			ENGINE_OFFSET * engine);

	writel(0, bwr->base +
		REG_BWR_CAM_SRT_TTL_ENG_BW0 + ENGINE_OFFSET * engine);

	//HRT bandwidth
	writel(0, bwr->base +
		REG_BWR_CAM_HRT_R0_ENG_BW0_0 + CHANNEL_OFFSET * axi +
			ENGINE_OFFSET * engine);

	writel(0, bwr->base +
		REG_BWR_CAM_HRT_W0_ENG_BW0_0 + CHANNEL_OFFSET * axi +
			ENGINE_OFFSET * engine);

	writel(0, bwr->base +
		REG_BWR_CAM_HRT_TTL_ENG_BW0 + ENGINE_OFFSET * engine);
	if (CAM_DEBUG_ENABLED(MMQOS))
		pr_info("%s: engine:%d, axi:%d set zero\n", __func__, engine, axi);

	mutex_unlock(&bwr->op_lock);

}

static void bwr_set_default(struct mtk_bwr_device *bwr)
{
	/* DPE */
	bwr_set_chn_bw(bwr, ENGINE_DPE, DISP_PORT,
		BWR_DEFAULT_DPE_SRT_R, BWR_DEFAULT_DPE_SRT_W, 0, 0, true);

	bwr_set_ttl_bw(bwr, ENGINE_DPE,
		BWR_DEFAULT_DPE_SRT_R + BWR_DEFAULT_DPE_SRT_W, 0, true);

	/* PDA */
	bwr_set_chn_bw(bwr, ENGINE_PDA, DISP_PORT,
		BWR_DEFAULT_PDA_SRT_R, BWR_DEFAULT_PDA_SRT_W, 0, 0, true);

	bwr_set_ttl_bw(bwr, ENGINE_DPE,
		BWR_DEFAULT_PDA_SRT_R + BWR_DEFAULT_PDA_SRT_W, 0, true);

	/* USIP */
	bwr_set_chn_bw(bwr, ENGINE_UISP, MDP0_PORT,
		BWR_DEFAULT_UISP_SRT_R, BWR_DEFAULT_UISP_SRT_W,
		BWR_DEFAULT_UISP_HRT_R, BWR_DEFAULT_UISP_HRT_W, true);

	bwr_set_ttl_bw(bwr, ENGINE_UISP,
		BWR_DEFAULT_UISP_SRT_R + BWR_DEFAULT_UISP_SRT_W,
		BWR_DEFAULT_UISP_HRT_R + BWR_DEFAULT_UISP_HRT_W, true);
}

static void bwr_clr_default(struct mtk_bwr_device *bwr)
{
	/* DPE */
	bwr_zero_bw(bwr, ENGINE_DPE, DISP_PORT);

	/* PDA */
	bwr_zero_bw(bwr, ENGINE_PDA, DISP_PORT);

	/* USIP */
	bwr_zero_bw(bwr, ENGINE_UISP, MDP0_PORT);
}

//unit MB/s
#define BWR_TEST_SRT_R    100
#define BWR_TEST_SRT_W    200
#define BWR_TEST_SRT_TTL  300
#define BWR_TEST_HRT_R    100
#define BWR_TEST_HRT_W    200
#define BWR_TEST_HRT_TTL  300
static void bwr_set_test(struct mtk_bwr_device *bwr)
{
	bwr_set_chn_bw(bwr, ENGINE_SUB_A, DISP_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_chn_bw(bwr, ENGINE_SUB_A, MDP0_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_chn_bw(bwr, ENGINE_SUB_B, DISP_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_chn_bw(bwr, ENGINE_SUB_B, MDP0_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_chn_bw(bwr, ENGINE_SUB_C, DISP_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_chn_bw(bwr, ENGINE_SUB_C, MDP0_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_chn_bw(bwr, ENGINE_MRAW, DISP_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_chn_bw(bwr, ENGINE_MRAW, MDP0_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_chn_bw(bwr, ENGINE_CAMSV_B, MDP0_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_chn_bw(bwr, ENGINE_CAMSV_B, MDP1_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_chn_bw(bwr, ENGINE_CAMSV_A, DISP_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_chn_bw(bwr, ENGINE_CAMSV_A, MDP1_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);
	bwr_set_chn_bw(bwr, ENGINE_DPE, DISP_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, 0, 0, true);
	bwr_set_chn_bw(bwr, ENGINE_PDA, DISP_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, 0, 0, true);
	bwr_set_chn_bw(bwr, ENGINE_UISP, MDP0_PORT,
			BWR_TEST_SRT_R, BWR_TEST_SRT_W, BWR_TEST_HRT_R, BWR_TEST_HRT_W, true);

	bwr_set_ttl_bw(bwr, ENGINE_SUB_A, BWR_TEST_SRT_TTL, BWR_TEST_HRT_TTL, true);
	bwr_set_ttl_bw(bwr, ENGINE_SUB_B, BWR_TEST_SRT_TTL, BWR_TEST_HRT_TTL, true);
	bwr_set_ttl_bw(bwr, ENGINE_SUB_C, BWR_TEST_SRT_TTL, BWR_TEST_HRT_TTL, true);
	bwr_set_ttl_bw(bwr, ENGINE_MRAW, BWR_TEST_SRT_TTL, BWR_TEST_HRT_TTL, true);
	bwr_set_ttl_bw(bwr, ENGINE_CAMSV_B, BWR_TEST_SRT_TTL, BWR_TEST_HRT_TTL, true);
	bwr_set_ttl_bw(bwr, ENGINE_CAMSV_A, BWR_TEST_SRT_TTL, BWR_TEST_HRT_TTL, true);
	bwr_set_ttl_bw(bwr, ENGINE_DPE, BWR_TEST_SRT_TTL, 0, true);
	bwr_set_ttl_bw(bwr, ENGINE_PDA, BWR_TEST_SRT_TTL, 0, true);
	bwr_set_ttl_bw(bwr, ENGINE_PDA, BWR_TEST_SRT_TTL, BWR_TEST_HRT_TTL, true);

	mtk_cam_bwr_dbg_dump(bwr);
}

static void bwr_clr_test(struct mtk_bwr_device *bwr)
{
	int i ,j;

	for (i = 0 ; i < ENGINE_NUM; i++)
		for (j = 0 ; j < NUM_PORT; j++)
			bwr_zero_bw(bwr, i, j);
}

static int bwr_start(struct mtk_bwr_device *bwr)
{
	int i, j, offset;

	mutex_lock(&bwr->op_lock);

	//set unit to MB/s
	writel(0x0, bwr->base + REG_BWR_CAM_BW_TYPE);

	//occ factor
	writel(BWR_SRT_TTL_OCC_FACTOR, bwr->base + REG_BWR_CAM_SRT_TTL_OCC_FACTOR);
	writel(BWR_HRT_TTL_OCC_FACTOR, bwr->base + REG_BWR_CAM_HRT_TTL_OCC_FACTOR);
	writel(BWR_SRT_BW_OCC_FACTOR, bwr->base + REG_BWR_CAM_SRT_RW_OCC_FACTOR);
	writel(BWR_HRT_BW_OCC_FACTOR, bwr->base + REG_BWR_CAM_HRT_RW_OCC_FACTOR);

	//dvfs freq
	writel(0x1, bwr->base + REG_BWR_CAM_HRT_TTL_DVFS_FREQ);
	writel(0x1, bwr->base + REG_BWR_CAM_SRT_TTL_DVFS_FREQ);
	writel(0x4, bwr->base + REG_BWR_CAM_HRT_RW_DVFS_FREQ);
	writel(0x4, bwr->base + REG_BWR_CAM_SRT_RW_DVFS_FREQ);

	//default ratio
	for (i = 0 ; i < NUM_BW_CHANNEL; ++i) {
		for (j = 0 ; j < ENGINE_NUM; ++j) {
			offset = CHANNEL_OFFSET * i + ENGINE_OFFSET * j;
			writel(to_ratio_csr(1), bwr->base + REG_BWR_CAM_SRT_TTL_ENG_BW_RAT0 + offset);
		}
	}

	//cam protocol
	writel(BWR_CAM_PROTOCOL0, bwr->base + REG_BWR_CAM_PROTOCOL0);
	writel(BWR_CAM_PROTOCOL1, bwr->base + REG_BWR_CAM_PROTOCOL1);
	writel(BWR_CAM_PROTOCOL2, bwr->base + REG_BWR_CAM_PROTOCOL2);
	writel(BWR_CAM_PROTOCOL3, bwr->base + REG_BWR_CAM_PROTOCOL3);
	writel(BWR_CAM_PROTOCOL4, bwr->base + REG_BWR_CAM_PROTOCOL4);
	writel(0x1, bwr->base + REG_BWR_CAM_PROTOCOL_SET_EN);

	//100us
	writel(CLK_CYC_PER_US * 100, bwr->base + REG_BWR_CAM_RPT_TIMER);
	//20us
	writel(CLK_CYC_PER_US * 20, bwr->base + REG_BWR_CAM_DBC_CYC);

	//BWR mode
	for (i = 0; i < NUM_BW_CHANNEL ; ++i) {
		writel(debug_bwr_mode == BWR_HW_MODE ? 0x7ff : 0x0,
			bwr->base + REG_BWR_CAM_SRT_TTL_BW_QOS_SEL + CHANNEL_OFFSET * i);
		writel(debug_bwr_mode == BWR_SW_MODE ? 0x7ff : 0x0,
			bwr->base + REG_BWR_CAM_SRT_TTL_SW_QOS_EN + CHANNEL_OFFSET * i);
	}

	writel(FBIT(BWR_CAM_RPT_START), bwr->base + REG_BWR_CAM_RPT_CTRL);
	pr_info("%s rpt_timer/dbc_cyc:0x%x/0x%x\n", __func__,
		readl_relaxed(bwr->base + REG_BWR_CAM_RPT_TIMER),
		readl_relaxed(bwr->base + REG_BWR_CAM_DBC_CYC));

	mutex_unlock(&bwr->op_lock);

	return 0;
}

/* todo: check all csr is zero ? */
static int bwr_stop(struct mtk_bwr_device *bwr)
{
	int rpt_state;

	mutex_lock(&bwr->op_lock);

	if (readx_poll_timeout(readl, bwr->base + REG_BWR_CAM_SEND_BW_ZERO,
			rpt_state, rpt_state & 0x1,
				50 /* delay, us */,
				500 /* timeout, us */) < 0)
		pr_info("%s: send bw zero timeout!(%d)\n",
			 __func__, rpt_state);

	writel(FBIT(BWR_CAM_RPT_END), bwr->base + REG_BWR_CAM_RPT_CTRL);

	if (readx_poll_timeout(readl, bwr->base + REG_BWR_CAM_RPT_STATE,
			rpt_state, rpt_state & 0x2,
			50 /* delay, us */,
			500 /* timeout, us */) < 0)
		pr_info("%s: wait state timeout!(%d)\n",
			 __func__, rpt_state);

	writel(FBIT(BWR_CAM_RPT_RST), bwr->base + REG_BWR_CAM_RPT_CTRL);

	pr_info("%s rpt_state: %d\n", __func__, rpt_state);

	mutex_unlock(&bwr->op_lock);

	return 0;
}

/* notice: enable once by cam main */
void mtk_cam_bwr_enable(struct mtk_bwr_device *bwr)
{
	bwr_start(bwr);
	bwr_set_default(bwr);

	if (debug_bwr_test)
		bwr_set_test(bwr);
}

/* notice: disable once by cam main */
void mtk_cam_bwr_disable(struct mtk_bwr_device *bwr)
{
	if (debug_bwr_test)
		bwr_clr_test(bwr);

	bwr_clr_default(bwr);
	bwr_stop(bwr);
}

void mtk_cam_bwr_set_chn_bw(struct mtk_bwr_device *bwr,
			  enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi,
			  int srt_r_bw, int srt_w_bw, int hrt_r_bw, int hrt_w_bw, bool clear)
{
	if (debug_bwr_test)
		return;

	bwr_set_chn_bw(bwr, engine, axi, srt_r_bw, srt_w_bw, hrt_r_bw, hrt_w_bw, clear);
}

void mtk_cam_bwr_set_ttl_bw(struct mtk_bwr_device *bwr,
			  enum BWR_ENGINE_TYPE engine, int srt_ttl, int hrt_ttl, bool clear)
{
	if (debug_bwr_test)
		return;

	bwr_set_ttl_bw(bwr, engine, srt_ttl, hrt_ttl, clear);
}

void mtk_cam_bwr_clr_bw(
	struct mtk_bwr_device *bwr, enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi)
{
	if (debug_bwr_test)
		return;

	bwr_zero_bw(bwr, engine, axi);
}

/* hw mode trigger for HRT/SRT */
void mtk_cam_bwr_trigger(struct mtk_bwr_device *bwr,
	enum BWR_ENGINE_TYPE engine, enum BWR_AXI_PORT axi)
{
	mutex_lock(&bwr->op_lock);

	writel(0x1, bwr->base +
		REG_BWR_CAM_SRT_R0_SW_QOS_TRIG0 + CHANNEL_OFFSET * axi +
		ENGINE_OFFSET * engine);

	writel(0x1, bwr->base +
		REG_BWR_CAM_HRT_R0_SW_QOS_TRIG0 + CHANNEL_OFFSET * axi +
		ENGINE_OFFSET * engine);

	writel(0x1, bwr->base +
		REG_BWR_CAM_SRT_TTL_SW_QOS_TRIG + ENGINE_OFFSET * engine);

	writel(0x1, bwr->base +
		REG_BWR_CAM_HRT_TTL_SW_QOS_TRIG + ENGINE_OFFSET * engine);

	mutex_unlock(&bwr->op_lock);
}

void mtk_cam_bwr_dbg_dump(struct mtk_bwr_device *bwr)
{
	int engine, axi;

	if (!debug_bwr_dump)
		return;

	for (engine = 0 ; engine < ENGINE_NUM; engine++) { //11
		pr_info("%s : SRT_TLL/HRT_TLL 0x%08x, 0x%08x\n",
			str_engine(engine),
			to_bw_val(readl(bwr->base + REG_BWR_CAM_SRT_TTL_ENG_BW0 +
				ENGINE_OFFSET * engine)),
			to_bw_val(readl(bwr->base + REG_BWR_CAM_HRT_TTL_ENG_BW0 +
				ENGINE_OFFSET * engine)));

		for (axi = 0 ; axi < NUM_PORT; axi++) { //44
			pr_info("%s %s : SRT_R/SRT_W/HRT_R/HRT_W : 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
				str_engine(engine), str_axi_port(axi),
				to_bw_val(readl(bwr->base + REG_BWR_CAM_SRT_R0_ENG_BW0_0 +
					CHANNEL_OFFSET * axi + ENGINE_OFFSET * engine)),
				to_bw_val(readl(bwr->base + REG_BWR_CAM_SRT_W0_ENG_BW0_0 +
					CHANNEL_OFFSET * axi + ENGINE_OFFSET * engine)),
				to_bw_val(readl(bwr->base + REG_BWR_CAM_HRT_R0_ENG_BW0_0 +
					CHANNEL_OFFSET * axi + ENGINE_OFFSET * engine)),
				to_bw_val(readl(bwr->base + REG_BWR_CAM_HRT_W0_ENG_BW0_0 +
					CHANNEL_OFFSET * axi + ENGINE_OFFSET * engine)));
		}
	}
}

int mtk_cam_bwr_probe(struct device *dev, struct mtk_bwr_device *bwr)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *res;

	/* base register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "bwr_base");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	bwr->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(bwr->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(bwr->base);
	}

	mutex_init(&bwr->op_lock);

	return 0;
}
