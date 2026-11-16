// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/vmalloc.h>

#include <soc/mediatek/smi.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include <soc/mediatek/emi.h>

#include "mtk_cam.h"
#include "mtk_cam-sv-regs.h"
#include "mtk_cam-sv.h"
#include "mtk_cam-fmt_utils.h"
#include "mtk_cam-trace.h"
#include "mtk_cam-hsf.h"
#include "mtk_cam-fmon.h"
#include "mtk_cam-ctrl.h"

#include "mmqos-mtk.h"
#include "iommu_debug.h"
#include "mtk-mmdvfs-debug.h"
#include "mtk-smi-dbg.h"

// place below all other include
#include "mtk_cam-virt-isp.h"

#define CAMSV_TS_CNT 0x2

#define MTK_CAMSV_STOP_HW_TIMEOUT			(33 * USEC_PER_MSEC)
#define CAMSV_DEBUG 0
#define FRAME_TIME 33000000

#define STG_TAG_LENA_MAX_NUM 6
int stg_tag_len_shift[STG_TAG_LENA_MAX_NUM] = {0x0, 0x24, 0x4c, 0x74, 0x9c, 0xc4};

static int debug_cam_sv;
module_param(debug_cam_sv, int, 0644);

static int urgent_high = -1;
module_param(urgent_high, int, 0644);
static int urgent_low = -1;
module_param(urgent_low, int, 0644);
static int ultra_high = -1;
module_param(ultra_high, int, 0644);
static int ultra_low = -1;
module_param(ultra_low, int, 0644);
static int pultra_high = -1;
module_param(pultra_high, int, 0644);
static int pultra_low = -1;
module_param(pultra_low, int, 0644);

static int camsv_fifo_detect;
module_param(camsv_fifo_detect, int, 0644);

static int debug_ddren_camsv_sw_mode = 1;
module_param(debug_ddren_camsv_sw_mode, int, 0644);
MODULE_PARM_DESC(debug_ddren_camsv_sw_mode, "debug: 1 : active camsv sw mode");

static int disable_camsv_df_mode = 1;
module_param(disable_camsv_df_mode, int, 0644);
MODULE_PARM_DESC(disable_camsv_df_mode, "disable camsv df mode");

static unsigned int camsv_stress_test_mode;
module_param(camsv_stress_test_mode, int, 0644);
MODULE_PARM_DESC(camsv_stress_test_mode, "camsv_stress_test_mode");

static int camsv_debug_dump_once;
module_param(camsv_debug_dump_once, int, 0644);
MODULE_PARM_DESC(camsv_debug_dump_once, "camsv debug dump once");

static int serror_dbg_chk;
module_param(serror_dbg_chk, int, 0644);
MODULE_PARM_DESC(serror_dbg_chk, "camsv debug serror apb");

#undef dev_dbg
#define dev_dbg(dev, fmt, arg...)		\
	do {					\
		if (debug_cam_sv >= 1)	\
			dev_info(dev, fmt,	\
				## arg);	\
	} while (0)

static const struct of_device_id mtk_camsv_of_ids[] = {
	{.compatible = "mediatek,camsv",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_camsv_of_ids);

static const struct mtk_camsv_tag_param sv_tag_param_normal[2] = {
	{
		.tag_idx = SVTAG_2,
		.seninf_padidx = PAD_SRC_RAW0,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = false,
	},
	{
		.tag_idx = SVTAG_3,
		.seninf_padidx = PAD_SRC_RAW_W0,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = true,
	},
};

static const struct mtk_camsv_tag_param sv_tag_param_2exp_stagger[4] = {
	{
		.tag_idx = SVTAG_0,
		.seninf_padidx = PAD_SRC_RAW0,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = false,
	},
	{
		.tag_idx = SVTAG_2,
		.seninf_padidx = PAD_SRC_RAW1,
		.tag_order = MTKCAM_IPI_ORDER_LAST_TAG,
		.is_w = false,
	},
	{
		.tag_idx = SVTAG_1,
		.seninf_padidx = PAD_SRC_RAW_W0,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = true,
	},
	{
		.tag_idx = SVTAG_3,
		.seninf_padidx = PAD_SRC_RAW_W1,
		.tag_order = MTKCAM_IPI_ORDER_LAST_TAG,
		.is_w = true,
	},
};


static const struct mtk_camsv_tag_param sv_tag_param_2exp_dcg_vs[4] = {
	{
		.tag_idx = SVTAG_2,
		.seninf_padidx = PAD_SRC_RAW0,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = false,
	},
	{
		.tag_idx = SVTAG_0,
		.seninf_padidx = PAD_SRC_RAW1,
		.tag_order = MTKCAM_IPI_ORDER_LAST_TAG,
		.is_w = false,
	},
	{
		.tag_idx = SVTAG_1,
		.seninf_padidx = PAD_SRC_RAW_W0,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = true,
	},
	{
		.tag_idx = SVTAG_3,
		.seninf_padidx = PAD_SRC_RAW_W1,
		.tag_order = MTKCAM_IPI_ORDER_LAST_TAG,
		.is_w = true,
	},
};

static const struct mtk_camsv_tag_param sv_tag_param_3exp_stagger[3] = {
	{
		.tag_idx = SVTAG_0,
		.seninf_padidx = PAD_SRC_RAW0,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = false,
	},
	{
		.tag_idx = SVTAG_1,
		.seninf_padidx = PAD_SRC_RAW1,
		.tag_order = MTKCAM_IPI_ORDER_NORMAL_TAG,
		.is_w = false,
	},
	{
		.tag_idx = SVTAG_2,
		.seninf_padidx = PAD_SRC_RAW2,
		.tag_order = MTKCAM_IPI_ORDER_LAST_TAG,
		.is_w = false,
	},
};

static const struct mtk_camsv_tag_param sv_tag_param_3exp_dcg_vs[3] = {
	{
		.tag_idx = SVTAG_0,
		.seninf_padidx = PAD_SRC_RAW0,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = false,
	},
	{
		.tag_idx = SVTAG_2,
		.seninf_padidx = PAD_SRC_RAW1,
		.tag_order = MTKCAM_IPI_ORDER_NORMAL_TAG,
		.is_w = false,
	},
	{
		.tag_idx = SVTAG_1,
		.seninf_padidx = PAD_SRC_RAW2,
		.tag_order = MTKCAM_IPI_ORDER_LAST_TAG,
		.is_w = false,
	},
};

static const struct mtk_camsv_tag_param sv_tag_param_display_ic[3] = {
	{
		.tag_idx = SVTAG_0,
		.seninf_padidx = PAD_SRC_RAW0,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = false,
	},
	{
		.tag_idx = SVTAG_2,
		.seninf_padidx = PAD_SRC_GENERAL0,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = false,
	},
	{
		.tag_idx = SVTAG_1,
		.seninf_padidx = PAD_SRC_RAW1,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = false,
	},
};

static const struct mtk_camsv_tag_param sv_tag_param_non_comb_ic[4] = {
	{
		.tag_idx = SVTAG_0,
		.seninf_padidx = PAD_SRC_RAW0,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = false,
	},
	{
		.tag_idx = SVTAG_1,
		.seninf_padidx = PAD_SRC_RAW1,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = false,
	},
	{
		.tag_idx = SVTAG_2,
		.seninf_padidx = PAD_SRC_RAW2,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = false,
	},
	{
		.tag_idx = SVTAG_3,
		.seninf_padidx = PAD_SRC_RAW3,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = false,
	},
};
static const struct mtk_camsv_tag_param sv_tag_param_extisp[1] = {
	{
		.tag_idx = SVTAG_3,
		.seninf_padidx = PAD_SRC_GENERAL0,
		.tag_order = MTKCAM_IPI_ORDER_FIRST_TAG,
		.is_w = false,
	},
};

static int sv_process_fsm(struct mtk_camsv_device *sv_dev,
			  struct mtk_camsys_irq_info *irq_info,
			  int *recovered_done)
{
	struct engine_fsm *fsm = &sv_dev->fsm;
	unsigned int i, done_type, sof_type, drop_type, done_tags = 0;
	int recovered = 0;

	sof_type = irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_START);
	if (sof_type) {
		/* when first tag comes: 1. reset drop tags 2. update used_tags */
		if (irq_info->sof_tags & sv_dev->first_tag) {
			sv_dev->drop_tags = 0;
			sv_dev->used_tags = 0;
			for (i = 0; i < MAX_SV_HW_GROUPS; i++)
				sv_dev->used_tags |= sv_dev->active_group_info[i];
		}
	}

	drop_type = irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_DROP);
	if (drop_type) {
		irq_info->irq_type &= ~drop_type;
		sv_dev->drop_tags |= irq_info->done_tags;
	}

	done_type = irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_DONE);
	if (done_type) {
		irq_info->irq_type &= ~done_type;
		done_tags = irq_info->done_tags;
	}

	if (drop_type || done_type) {
		int cookie_done;
		int ret;

		sv_dev->handled_tags |= done_tags;
		sv_dev->handled_tags |= sv_dev->drop_tags;
		sv_dev->handled_tags &= sv_dev->used_tags;

		if (sv_dev->handled_tags == sv_dev->used_tags) {
			ret = engine_fsm_hw_done(fsm, &cookie_done);
			if (ret > 0) {
				irq_info->irq_type |= BIT(CAMSYS_IRQ_FRAME_DONE);
				irq_info->cookie_done = cookie_done;
				sv_dev->handled_tags = 0;
			} else {
				/* handle for fake p1 done */
				dev_dbg(sv_dev->dev, "warn: fake done in/out: 0x%x 0x%x\n",
							 irq_info->frame_idx_inner,
							 irq_info->frame_idx);
				irq_info->cookie_done = 0;
				sv_dev->handled_tags = 0;
			}
		} else {
			dev_dbg(sv_dev->dev, "%s: not all group done yet: in/out: 0x%x 0x%x tag used/handled: 0x%x 0x%x\n",
				__func__,
				irq_info->frame_idx_inner,
				irq_info->frame_idx,
				sv_dev->used_tags,
				sv_dev->handled_tags);
			irq_info->cookie_done = 0;
		}
	}

	sof_type = irq_info->irq_type & BIT(CAMSYS_IRQ_FRAME_START);
	if (sof_type) {
		irq_info->irq_type &= ~sof_type;

		/* when first tag comes: update irq_type */
		if (irq_info->sof_tags & sv_dev->first_tag)
			irq_info->irq_type |= BIT(CAMSYS_IRQ_FRAME_START_DCIF_MAIN);

		/* when last tag comes: 1. update irq_type 2. update fsm */
		if (irq_info->sof_tags & sv_dev->last_tag) {
			irq_info->irq_type |= BIT(CAMSYS_IRQ_FRAME_START);
			for (i = 0; i < MAX_SV_HW_GROUPS; i++) {
				if (sv_dev->last_tag & sv_dev->active_group_info[i]) {

					recovered =
						engine_fsm_sof(&sv_dev->fsm,
							       irq_info->frame_idx_inner,
							       irq_info->frame_idx,
							       irq_info->fbc_empty,
							       recovered_done);
					break;
				}
			}
		}
	}

	if (recovered)
		dev_info(sv_dev->dev, "recovered done 0x%x in/out: 0x%x 0x%x\n",
			 *recovered_done,
			 irq_info->frame_idx_inner,
			 irq_info->frame_idx);

	return recovered;
}

int mtk_camsv_translation_fault_callback(int port, dma_addr_t mva, void *data)
{
	int index;
	struct mtk_camsv_device *sv_dev = (struct mtk_camsv_device *)data;
	unsigned int first_tag, tag_idx;
	unsigned int frame_idx_inner;

	first_tag = readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FIRST_TAG);

	if (first_tag == 0) {
		dev_info(sv_dev->dev, "abnormal tf callback first_tag=0\n");
		return 0;
	}

	tag_idx = ffs(first_tag) - 1;
	frame_idx_inner = readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FH_SPARE_TAG_1 +
			tag_idx * CAMSVCENTRAL_FH_SPARE_SHIFT);
	dev_info(sv_dev->dev, "tg_sen_mode:0x%x tg_vf_con:0x%x tg_path_cfg:0x%x cq desc_size:0x%x cq addr0x%x_%x",
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_SEN_MODE),
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_VF_CON),
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_PATH_CFG),
		readl_relaxed(sv_dev->base_scq_inner + REG_CAMSVCQ_CQ_SUB_THR0_DESC_SIZE_2),
		readl_relaxed(sv_dev->base_scq_inner + REG_CAMSVCQ_CQ_SUB_THR0_BASEADDR_2_MSB),
		readl_relaxed(sv_dev->base_scq_inner + REG_CAMSVCQ_CQ_SUB_THR0_BASEADDR_2));

	for (index = 0; index < CAMSV_MAX_TAGS; index++) {
		dev_info(sv_dev->dev, "tag:%d seq_no:%d_%d tg_grab_pxl:0x%x tg_grab_lin:0x%x fmt:0x%x imgo_fbc0: 0x%x imgo_fbc1: 0x%x",
		index,
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FH_SPARE_TAG_1 +
			index * CAMSVCENTRAL_FH_SPARE_SHIFT),
		readl_relaxed(sv_dev->base + REG_CAMSVCENTRAL_FH_SPARE_TAG_1 +
			index * CAMSVCENTRAL_FH_SPARE_SHIFT),
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_GRAB_PXL_TAG1 +
			index * CAMSVCENTRAL_GRAB_PXL_TAG_SHIFT),
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_GRAB_LIN_TAG1 +
			index * CAMSVCENTRAL_GRAB_LIN_TAG_SHIFT),
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FORMAT_TAG1 +
			index * CAMSVCENTRAL_FORMAT_TAG_SHIFT),
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FBC0_TAG1 +
			index * CAMSVCENTRAL_FBC0_TAG_SHIFT),
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FBC1_TAG1 +
			index * CAMSVCENTRAL_FBC1_TAG_SHIFT));
	}

	for (index = 0; index < CAMSV_MAX_TAGS; index++) {
		dev_info(sv_dev->dev, "tag:%d imgo_stride_img_a:0x%x imgo_addr_img_a:0x%x_%x",
			index,
			readl_relaxed(sv_dev->base_dma_inner +
				REG_CAMSVDMATOP_WDMA_BASIC_IMG1_A +
				index * CAMSVDMATOP_WDMA_BASIC_IMG_SHIFT),
			readl_relaxed(sv_dev->base_dma_inner +
				REG_CAMSVDMATOP_WDMA_BASE_ADDR_IMG1_A +
				index * CAMSVDMATOP_WDMA_BASE_ADDR_IMG_SHIFT),
			readl_relaxed(sv_dev->base_dma_inner +
				REG_CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG1_A +
				index * CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG_SHIFT));
		if (index >= SVTAG_0 && index <= SVTAG_5)
			dev_info(sv_dev->dev, "tag:%d imgo_stride_img_b:0x%x imgo_addr_img_b:0x%x_%x stride_len_a:0x%x addr_len_a:0x%x_%x stride_len_b:0x%x addr_len_b:0x%x_%x",
				index,
				readl_relaxed(sv_dev->base_dma_inner +
					REG_CAMSVDMATOP_WDMA_BASIC_IMG1_B +
					index * CAMSVDMATOP_WDMA_BASIC_IMG_SHIFT),
				readl_relaxed(sv_dev->base_dma_inner +
					REG_CAMSVDMATOP_WDMA_BASE_ADDR_IMG1_B +
					index * CAMSVDMATOP_WDMA_BASE_ADDR_IMG_SHIFT),
				readl_relaxed(sv_dev->base_dma_inner +
					REG_CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG1_B +
					index * CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG_SHIFT),
				readl_relaxed(sv_dev->base_dma_inner +
					REG_CAMSVDMATOP_WDMA_BASIC_LEN1_A +
					index * CAMSVDMATOP_WDMA_BASIC_IMG_SHIFT),
				readl_relaxed(sv_dev->base_dma_inner +
					REG_CAMSVDMATOP_WDMA_BASE_ADDR_LEN1_A +
					index * CAMSVDMATOP_WDMA_BASE_ADDR_IMG_SHIFT),
				readl_relaxed(sv_dev->base_dma_inner +
					REG_CAMSVDMATOP_WDMA_BASE_ADDR_MSB_LEN1_A +
					index * CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG_SHIFT),
				readl_relaxed(sv_dev->base_dma_inner +
					REG_CAMSVDMATOP_WDMA_BASIC_LEN1_B +
					index * CAMSVDMATOP_WDMA_BASIC_IMG_SHIFT),
				readl_relaxed(sv_dev->base_dma_inner +
					REG_CAMSVDMATOP_WDMA_BASE_ADDR_LEN1_B +
					index * CAMSVDMATOP_WDMA_BASE_ADDR_IMG_SHIFT),
				readl_relaxed(sv_dev->base_dma_inner +
					REG_CAMSVDMATOP_WDMA_BASE_ADDR_MSB_LEN1_B +
					index * CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG_SHIFT));
	}
	mtk_cam_ctrl_dump_request(sv_dev->cam, CAMSYS_ENGINE_CAMSV, sv_dev->id,
		frame_idx_inner, MSG_M4U_TF);
	return 0;
}

void mtk_cam_sv_set_queue_mode(struct mtk_camsv_device *sv_dev, bool enable)
{
	if (enable)
		atomic_set(&sv_dev->is_queue_mode, 1);
	else
		atomic_set(&sv_dev->is_queue_mode, 0);
}

void mtk_cam_sv_backup(struct mtk_camsv_device *sv_dev)
{
	struct mtk_camsv_backup_setting *s = &sv_dev->backup_setting;
	int i;

	s->done_status_en = CAMSV_READ_REG(sv_dev->base +
					   REG_CAMSVCENTRAL_DONE_STATUS_EN);
	s->err_status_en = CAMSV_READ_REG(sv_dev->base +
					  REG_CAMSVCENTRAL_ERR_STATUS_EN);
	s->sof_status_en = CAMSV_READ_REG(sv_dev->base +
					  REG_CAMSVCENTRAL_SOF_STATUS_EN);
	s->channel_status_en = CAMSV_READ_REG(sv_dev->base +
					  REG_CAMSVCENTRAL_CHANNEL_STATUS_EN);
	s->common_status_en = CAMSV_READ_REG(sv_dev->base +
					  REG_CAMSVCENTRAL_COMMON_STATUS_EN);

	for (i = SVTAG_START; i < SVTAG_END; i++) {
		s->grab_pxl[i] = CAMSV_READ_REG(sv_dev->base_inner +
				       REG_CAMSVCENTRAL_GRAB_PXL_TAG1 +
				       CAMSVCENTRAL_GRAB_PXL_TAG_SHIFT * i);
		s->grab_lin[i] = CAMSV_READ_REG(sv_dev->base_inner +
				       REG_CAMSVCENTRAL_GRAB_LIN_TAG1 +
				       CAMSVCENTRAL_GRAB_LIN_TAG_SHIFT * i);
		s->fbc0[i] = CAMSV_READ_REG(sv_dev->base +
				       REG_CAMSVCENTRAL_FBC0_TAG1 +
				       CAMSVCENTRAL_FBC0_TAG_SHIFT * i);
		s->exp0[i] = CAMSV_READ_REG(sv_dev->base +
				       REG_CAMSVCENTRAL_INT_EXP0_TAG1 +
				       CAMSVCENTRAL_INT_EXP0_OFFSET * i);
		s->exp1[i] = CAMSV_READ_REG(sv_dev->base +
				       REG_CAMSVCENTRAL_INT_EXP1_TAG1 +
				       CAMSVCENTRAL_INT_EXP1_OFFSET * i);
	}

	s->dma_en_img = CAMSV_READ_REG(sv_dev->base +
				       REG_CAMSVCENTRAL_DMA_EN_IMG);
	s->dma_en_len = CAMSV_READ_REG(sv_dev->base +
				       REG_CAMSVCENTRAL_DMA_EN_LEN);

	s->dcif_set = CAMSV_READ_REG(sv_dev->base +
				     REG_CAMSVCENTRAL_DCIF_SET);
	s->dcif_sel = CAMSV_READ_REG(sv_dev->base +
				     REG_CAMSVCENTRAL_DCIF_SEL);
}

void mtk_cam_sv_restore(struct mtk_camsv_device *sv_dev)
{
	struct mtk_camsv_backup_setting *s = &sv_dev->backup_setting;
	int i;

	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_DONE_STATUS_EN,
			s->done_status_en);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_ERR_STATUS_EN,
			s->err_status_en);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_SOF_STATUS_EN,
			s->sof_status_en);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_CHANNEL_STATUS_EN,
			s->channel_status_en);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_COMMON_STATUS_EN,
			s->common_status_en);

	for (i = SVTAG_START; i < SVTAG_END; i++) {
		CAMSV_WRITE_REG(sv_dev->base_inner + REG_CAMSVCENTRAL_GRAB_PXL_TAG1 +
			CAMSVCENTRAL_GRAB_PXL_TAG_SHIFT * i, s->grab_pxl[i]);
		CAMSV_WRITE_REG(sv_dev->base_inner + REG_CAMSVCENTRAL_GRAB_LIN_TAG1 +
			CAMSVCENTRAL_GRAB_LIN_TAG_SHIFT * i, s->grab_lin[i]);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_FBC0_TAG1 +
			CAMSVCENTRAL_FBC0_TAG_SHIFT * i, s->fbc0[i]);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_INT_EXP0_TAG1 +
			CAMSVCENTRAL_INT_EXP0_OFFSET * i, s->exp0[i]);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_INT_EXP1_TAG1 +
			CAMSVCENTRAL_INT_EXP1_OFFSET * i, s->exp1[i]);
	}

	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_DMA_EN_IMG, s->dma_en_img);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_DMA_EN_LEN, s->dma_en_len);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_DCIF_SET, s->dcif_set);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_DCIF_SEL, s->dcif_sel);
}


void mtk_cam_sv_exp_setup(struct mtk_camsv_device *sv_dev, int exp0, int exp1)
{
	unsigned int first_tag, tag_idx, grab_pxl, grab_lin;
	unsigned int x, y, w, h, value;

	/* skip unlock raw done sel for first frame */
	sv_dev->is_skip_raw_unlock_done = true;

	first_tag =
		CAMSV_READ_REG(sv_dev->base + REG_CAMSVCENTRAL_FIRST_TAG);
	if (first_tag)
		tag_idx = ffs(first_tag) - 1;
	else {
		dev_info(sv_dev->dev, "%s camsv_id:%d - first_tag shall not be zero\n",
			__func__, sv_dev->id);
		goto EXIT;
	}

	grab_pxl =
		CAMSV_READ_REG(sv_dev->base +
			REG_CAMSVCENTRAL_GRAB_PXL_TAG1 +
			(tag_idx * CAMSVCENTRAL_GRAB_PXL_TAG_SHIFT));
	grab_lin =
		CAMSV_READ_REG(sv_dev->base +
			REG_CAMSVCENTRAL_GRAB_LIN_TAG1 +
			(tag_idx * CAMSVCENTRAL_GRAB_LIN_TAG_SHIFT));
	x = grab_pxl & 0xFFFF;
	y = grab_lin & 0xFFFF;
	w = (grab_pxl >> 16) - x;
	h = (grab_lin >> 16) - y;

	if (w + x <= 16) {
		dev_info(sv_dev->dev, "%s camsv_id:%d - width too small:%d_%d\n",
			__func__, sv_dev->id, x, w);
		goto EXIT;
	}

	/* setup exp0 */
	value = (ALIGN(w + x - 16, 16) << 16) | (exp0 + y);
	CAMSV_WRITE_REG(sv_dev->base +
		REG_CAMSVCENTRAL_INT_EXP0_TAG1 +
		(CAMSVCENTRAL_INT_EXP0_OFFSET * tag_idx),
		value);
	CAMSV_WRITE_REG(sv_dev->base_inner +
		REG_CAMSVCENTRAL_INT_EXP0_TAG1 +
		(CAMSVCENTRAL_INT_EXP0_OFFSET * tag_idx),
		value);


	/* setup exp1 */
	value = (ALIGN(w + x - 16, 16) << 16) | (exp1 + y);
	CAMSV_WRITE_REG(sv_dev->base +
		REG_CAMSVCENTRAL_INT_EXP1_TAG1 +
		(CAMSVCENTRAL_INT_EXP1_OFFSET * tag_idx),
		value);
	CAMSV_WRITE_REG(sv_dev->base_inner +
		REG_CAMSVCENTRAL_INT_EXP1_TAG1 +
		(CAMSVCENTRAL_INT_EXP1_OFFSET * tag_idx),
		value);

	/* enable common interrupt */
	value = 3 << (CAMSVCENTRAL_DBG_INT_BIT_START +
		CAMSVCENTRAL_DBG_INT_BIT_OFFSET * tag_idx);
	CAMSV_WRITE_REG(sv_dev->base +
		REG_CAMSVCENTRAL_COMMON_STATUS_EN,
		value);
	CAMSV_WRITE_REG(sv_dev->base_inner +
		REG_CAMSVCENTRAL_COMMON_STATUS_EN,
		value);

	dev_info(sv_dev->dev, "%s camsv_id:%d - tag_idx:%d exp0:0x%x exp1:0x%x en:0x%x\n",
		__func__, sv_dev->id, tag_idx,
		CAMSV_READ_REG(sv_dev->base_inner +
			REG_CAMSVCENTRAL_INT_EXP0_TAG1 +
			(CAMSVCENTRAL_INT_EXP0_OFFSET * tag_idx)),
		CAMSV_READ_REG(sv_dev->base_inner +
			REG_CAMSVCENTRAL_INT_EXP1_TAG1 +
			(CAMSVCENTRAL_INT_EXP1_OFFSET * tag_idx)),
		CAMSV_READ_REG(sv_dev->base_inner +
			REG_CAMSVCENTRAL_COMMON_STATUS_EN));
EXIT:
	return;
}

int mtk_cam_sv_reset_msgfifo(struct mtk_camsv_device *sv_dev)
{
	atomic_set(&sv_dev->is_fifo_overflow, 0);
	return kfifo_init(&sv_dev->msg_fifo, sv_dev->msg_buffer, sv_dev->fifo_size);
}

static int push_msgfifo(struct mtk_camsv_device *sv_dev,
			struct mtk_camsys_irq_info *info)
{
	int len;
	unsigned long flags;

	spin_lock_irqsave(&sv_dev->msg_lock, flags);
	if (unlikely(kfifo_avail(&sv_dev->msg_fifo) < sizeof(*info))) {
		atomic_set(&sv_dev->is_fifo_overflow, 1);
		spin_unlock_irqrestore(&sv_dev->msg_lock, flags);
		return -1;
	}
	info->ts_after_push_msgfifo_sof = ktime_get_boottime_ns();
	len = kfifo_in(&sv_dev->msg_fifo, info, sizeof(*info));

	spin_unlock_irqrestore(&sv_dev->msg_lock, flags);
	WARN_ON(len != sizeof(*info));
	return 0;
}

static int pop_msgfifo(struct mtk_camsv_device *sv_dev,
			struct mtk_camsys_irq_info *info)
{
	int len;
	unsigned long flags;

	spin_lock_irqsave(&sv_dev->msg_lock, flags);
	if (kfifo_len(&sv_dev->msg_fifo) >= sizeof(*info)) {
		len = kfifo_out(&sv_dev->msg_fifo, info, sizeof(*info));
		spin_unlock_irqrestore(&sv_dev->msg_lock, flags);
		WARN_ON(len != sizeof(*info));
		return 1;
	}

	spin_unlock_irqrestore(&sv_dev->msg_lock, flags);
	return 0;
}

void sv_reset_by_camsys_top(struct mtk_camsv_device *sv_dev)
{
	int cq_dma_sw_ctl;
	int ret;
	unsigned int vcore_gal;

	dev_info(sv_dev->dev, "%s camsv_id:%d\n", __func__, sv_dev->id);

	writel(0, sv_dev->base_scq + REG_CAMSVCQTOP_SW_RST_CTL);
	writel(1, sv_dev->base_scq + REG_CAMSVCQTOP_SW_RST_CTL);
	wmb(); /* make sure committed */

	ret = readx_poll_timeout(readl, sv_dev->base_scq + REG_CAMSVCQTOP_SW_RST_CTL,
				cq_dma_sw_ctl,
				cq_dma_sw_ctl & 0x2,
				1 /* delay, us */,
				100000 /* timeout, us */);
	if (ret < 0) {
		dev_info(sv_dev->dev, "%s: timeout\n", __func__);

		dev_info(sv_dev->dev,
			 "tg_sen_mode: 0x%x, cq_dma_sw_ctl:0x%x\n",
			 readl(sv_dev->base + REG_CAMSVCENTRAL_SEN_MODE),
			 readl(sv_dev->base_scq + REG_CAMSVCQTOP_SW_RST_CTL));
		mtk_smi_dbg_hang_detect("camsys-camsv");
		goto RESET_FAILURE;
	}
	/* debug only, rm soon */
	writel(0, sv_dev->base_scq + REG_CAMSVCQTOP_SW_RST_CTL);
	if (serror_dbg_chk) {
		// vcore
		vcore_gal = readl(sv_dev->cam->vcore_base + 0x10);
		pr_info("vcore gal read: 0x%x", readl(sv_dev->cam->vcore_base + 0x10));
		vcore_gal &= ~(0x2000);
		vcore_gal |= (1 << 13);
		writel(vcore_gal, sv_dev->cam->vcore_base + 0x10);
		pr_info("vcore gal write: 0x%x", readl(sv_dev->cam->vcore_base + 0x10));
		// main
		writel(0xDEADBEEF, sv_dev->cam->base + 0x10);
		pr_info("main sram_del: 0x%x", readl(sv_dev->cam->base + 0x10));
		// mraw
		writel(0xDEADBEEF, sv_dev->top + 0x10);
		pr_info("mraw sram_del: 0x%x", readl(sv_dev->top + 0x10));
	}

	writel(0, sv_dev->top + REG_CAM_MAIN_SW_RST_1);
	if (serror_dbg_chk)
		pr_info("reset_dbg_step 1");
	writel(3 << ((sv_dev->id) * 2 + 4), sv_dev->top + REG_CAM_MAIN_SW_RST_1);
	if (serror_dbg_chk)
		pr_info("reset_dbg_step 2");
	writel(0, sv_dev->top + REG_CAM_MAIN_SW_RST_1);
	if (serror_dbg_chk)
		pr_info("reset_dbg_step 3");
	wmb(); /* make sure committed */

RESET_FAILURE:
	return;
}

void sv_top_reset_by_camsys_top(struct mtk_camsv_device *sv_dev)
{
	if (CAM_DEBUG_ENABLED(RAW_INT))
		dev_info(sv_dev->dev, "%s camsv_id:%d\n", __func__, sv_dev->id);

	writel(0, sv_dev->top + REG_CAM_MAIN_SW_RST_1);
	writel(3 << 2, sv_dev->top + REG_CAM_MAIN_SW_RST_1);
	writel(0, sv_dev->top + REG_CAM_MAIN_SW_RST_1);
	wmb(); /* make sure committed */
}

void sv_reset(struct mtk_camsv_device *sv_dev)
{
	int dma_sw_ctl, cq_dma_sw_ctl, stg_sw_ctl;
	int ret;

	dev_dbg(sv_dev->dev, "%s camsv_id:%d\n", __func__, sv_dev->id);

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_SEN_MODE,
		CAMSVCENTRAL_SEN_MODE, CAM_SUB_EN, 0);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_SEN_MODE,
		CAMSVCENTRAL_SEN_MODE, SOF_SUB_EN, 0);
	CAMSV_WRITE_BITS(sv_dev->base_inner + REG_CAMSVCENTRAL_SEN_MODE,
		CAMSVCENTRAL_SEN_MODE, CAM_SUB_EN, 0);
	CAMSV_WRITE_BITS(sv_dev->base_inner + REG_CAMSVCENTRAL_SEN_MODE,
		CAMSVCENTRAL_SEN_MODE, SOF_SUB_EN, 0);

	CAMSV_WRITE_BITS(sv_dev->base_scq_inner + REG_CAMSVCQ_CQ_EN,
		CAMSVCQ_CQ_EN, CAMSVCQ_SCQ_SUBSAMPLE_EN, 0);

	writel(0, sv_dev->base_dma + REG_CAMSVDMATOP_SW_RST_CTL);
	writel(0, sv_dev->base_scq + REG_CAMSVCQTOP_SW_RST_CTL);
	writel(0, sv_dev->base + REG_CAMSVCENTRAL_STG_RST);
	writel(1, sv_dev->base_dma + REG_CAMSVDMATOP_SW_RST_CTL);
	writel(1, sv_dev->base + REG_CAMSVCENTRAL_STG_RST);
	writel(1, sv_dev->base_scq + REG_CAMSVCQTOP_SW_RST_CTL);
	wmb(); /* make sure committed */

	ret = readx_poll_timeout(readl, sv_dev->base_dma + REG_CAMSVDMATOP_SW_RST_CTL,
			dma_sw_ctl,
			dma_sw_ctl & 0x2,
			1 /* delay, us */,
			100000 /* timeout, us */);
	if (ret < 0) {
		unsigned int debug_sel = 0, dma_core, dbg_port;

		dev_info(sv_dev->dev,
			 "%s: camsv dma timeout tg_sen_mode: 0x%x, dma_sw_ctl:0x%x camsv_dcm_status:0x%x img_en0x%x len_en0x%x",
			 __func__,
			 readl(sv_dev->base + REG_CAMSVCENTRAL_SEN_MODE),
			 readl(sv_dev->base_dma + REG_CAMSVDMATOP_SW_RST_CTL),
			 readl(sv_dev->base + REG_CAMSVCENTRAL_DCM_DIS_STATUS),
			 readl(sv_dev->base + REG_CAMSVCENTRAL_DMA_EN_IMG),
			 readl(sv_dev->base + REG_CAMSVCENTRAL_DMA_EN_LEN));
		for (dma_core = 0; dma_core < MAX_DMA_CORE; dma_core++) {
			debug_sel = 0;
			debug_sel |= (1 << 7);
			debug_sel |= (dma_core << 4);
			for (int sel = 9; sel < 16; sel++) {
				debug_sel &= ~(0xf);
				debug_sel |= sel;
				writel_relaxed(debug_sel, sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_SEL);
				dbg_port = readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_PORT);
				dev_info(sv_dev->dev, "[core%d] dbg_sel:0x%x => dbg_port = 0x%x\n",
					dma_core, debug_sel, dbg_port);
			}
		}
		writel_relaxed((0x2 << 8), sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_SEL);
		dev_info(sv_dev->dev, "dbg_sel:0x%x => dbg_port7 = 0x%x dbg_port8 = 0x%x\n",
			readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_SEL),
			readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DBG_PORT7),
			readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DBG_PORT8));


		writel_relaxed((0x4 << 8), sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_SEL);
		dev_info(sv_dev->dev, "dbg_sel:0x%x => dbg_port7 = 0x%x dbg_port8 = 0x%x\n",
			readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_SEL),
			readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DBG_PORT7),
			readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DBG_PORT8));

		mtk_smi_dbg_hang_detect("camsys-camsv");
		goto RESET_FAILURE;
	}
	ret = readx_poll_timeout(readl, sv_dev->base + REG_CAMSVCENTRAL_STG_RST,
			stg_sw_ctl,
			stg_sw_ctl & STG_SOFT_RST_STAT,
			1 /* delay, us */,
			100000 /* timeout, us */);
	if (ret < 0) {
		dev_info(sv_dev->dev, "%s: timeout\n", __func__);
		goto RESET_FAILURE;
	}
	/* reset cq dma */
	ret = readx_poll_timeout(readl, sv_dev->base_scq + REG_CAMSVCQTOP_SW_RST_CTL,
		cq_dma_sw_ctl,
		cq_dma_sw_ctl & 0x2,
		1 /* delay, us */,
		100000 /* timeout, us */);
	if (ret < 0) {
		dev_info(sv_dev->dev,
			 "%s: cq dma timeout tg_sen_mode: 0x%x, cq_dma_sw_ctl:0x%x cam_main_gals_dbg_status 0x%x, cam_main_ppc_prot_rdy_0 0x%x, cam_main_ppc_prot_rdy_1 0x%x\n",
			 __func__,
			 readl(sv_dev->base + REG_CAMSVCENTRAL_SEN_MODE),
			 readl(sv_dev->base_scq + REG_CAMSVCQTOP_SW_RST_CTL),
			 readl(sv_dev->cam->base + 0x414),
			 readl(sv_dev->cam->base + 0x588),
			 readl(sv_dev->cam->base + 0x58c));

		CAMSV_WRITE_BITS(sv_dev->base_scq + REG_CAMSVCQTOP_DEBUG_SET,
			CAMSVCQTOP_DEBUG_SET, CAMSVCQTOP_DEBUG_TOP_SEL, 0);
		CAMSV_WRITE_BITS(sv_dev->base_scq_inner + REG_CAMSVCQ_CQ_EN,
			CAMSVCQ_CQ_EN, CAMSVCQ_CQ_DBG_MAIN_SUB_SEL, 1);
		CAMSV_WRITE_BITS(sv_dev->base_scq_inner + REG_CAMSVCQ_CQ_EN,
			CAMSVCQ_CQ_EN, CAMSVCQ_CQ_DBG_SEL, 1);
		CAMSV_WRITE_BITS(sv_dev->base_scq + REG_CAMSVCQTOP_DEBUG_SET,
			CAMSVCQTOP_DEBUG_SET, CAMSVCQTOP_DEBUG_SEL, 0x0);
		dev_info(sv_dev->dev, "cqd0 checksum 0x%x",
			readl(sv_dev->base_scq + REG_CAMSVCQTOP_DEBUG));
		CAMSV_WRITE_BITS(sv_dev->base_scq + REG_CAMSVCQTOP_DEBUG_SET,
			CAMSVCQTOP_DEBUG_SET, CAMSVCQTOP_DEBUG_SEL, 0x4);
		dev_info(sv_dev->dev, "cqd1 checksum 0x%x",
			readl(sv_dev->base_scq + REG_CAMSVCQTOP_DEBUG));
		CAMSV_WRITE_BITS(sv_dev->base_scq + REG_CAMSVCQTOP_DEBUG_SET,
			CAMSVCQTOP_DEBUG_SET, CAMSVCQTOP_DEBUG_SEL, 0x8);
		dev_info(sv_dev->dev, "cqa0 checksum 0x%x",
			readl(sv_dev->base_scq + REG_CAMSVCQTOP_DEBUG));
		CAMSV_WRITE_BITS(sv_dev->base_scq + REG_CAMSVCQTOP_DEBUG_SET,
			CAMSVCQTOP_DEBUG_SET, CAMSVCQTOP_DEBUG_SEL, 0xc);
		dev_info(sv_dev->dev, "cqa1 checksum 0x%x",
			readl(sv_dev->base_scq + REG_CAMSVCQTOP_DEBUG));
		CAMSV_WRITE_BITS(sv_dev->base_scq_inner + REG_CAMSVCQ_CQ_EN,
			CAMSVCQ_CQ_EN, CAMSVCQ_CQ_DBG_SEL, 0);
		CAMSV_WRITE_BITS(sv_dev->base_scq + REG_CAMSVCQTOP_DEBUG_SET,
			CAMSVCQTOP_DEBUG_SET, CAMSVCQTOP_DEBUG_SEL, 0x1);
		dev_info(sv_dev->dev, "thr_state 0x%x",
			readl(sv_dev->base_scq + REG_CAMSVCQTOP_DEBUG));
		writel(0xDEADBEEF, sv_dev->base + REG_CAMSVCENTRAL_CAMSV_SPARE0);
		dev_info(sv_dev->dev, "camsv spare reigister WR test write 0xDEADBEEF, read 0x%x",
			readl(sv_dev->base + REG_CAMSVCENTRAL_CAMSV_SPARE0));
		writel(0x3000100, sv_dev->base_scq + REG_CAMSVCQDMATOP_DMA_DBG_SEL);
		writel(1, sv_dev->base_scq + REG_CAMSVCQ_SCQ_MISC);
		writel(1, sv_dev->base_scq + REG_CAMSVCQ_SCQ_SUB_MISC);

		dev_info(sv_dev->dev, "cqi_e1 state checksum 0x%x dbg_sel0x%x",
			readl(sv_dev->base_scq + REG_CAMSVCQDMATOP_DMA_DBG_PORT),
			readl(sv_dev->base_scq + REG_CAMSVCQDMATOP_DMA_DBG_SEL));
		writel(0x3000500, sv_dev->base_scq + REG_CAMSVCQDMATOP_DMA_DBG_SEL);
		dev_info(sv_dev->dev, "cqi_e1 smi debug data 0x%x dbg_sel0x%x",
			readl(sv_dev->base_scq + REG_CAMSVCQDMATOP_DMA_DBG_PORT),
			readl(sv_dev->base_scq + REG_CAMSVCQDMATOP_DMA_DBG_SEL));
		writel(0x3000101, sv_dev->base_scq + REG_CAMSVCQDMATOP_DMA_DBG_SEL);
		dev_info(sv_dev->dev, "cqi_e2 state checksum 0x%x dbg_sel0x%x",
			readl(sv_dev->base_scq + REG_CAMSVCQDMATOP_DMA_DBG_PORT),
			readl(sv_dev->base_scq + REG_CAMSVCQDMATOP_DMA_DBG_SEL));
		writel(0x3000501, sv_dev->base_scq + REG_CAMSVCQDMATOP_DMA_DBG_SEL);
		dev_info(sv_dev->dev, "cqi_e2 smi debug data 0x%x dbg_sel0x%x",
			readl(sv_dev->base_scq + REG_CAMSVCQDMATOP_DMA_DBG_PORT),
			readl(sv_dev->base_scq + REG_CAMSVCQDMATOP_DMA_DBG_SEL));
		writel(0, sv_dev->base_scq + REG_CAMSVCQ_SCQ_MISC);
		writel(0, sv_dev->base_scq + REG_CAMSVCQ_SCQ_SUB_MISC);
		mtk_smi_dbg_hang_detect("camsys-camsv");
		goto RESET_FAILURE;
	}


	/* reset cq */
	writel(0x11, sv_dev->base_scq + REG_CAMSVCQTOP_SW_RST_CTL);

	/* disable cq dcm dis*/
	writel(0x0, sv_dev->base_scq + REG_CAMSVCQTOP_DCM_DIS);
	wmb(); /* make sure committed */

	/* enable dma dcm after dma is idle */
	writel(0, sv_dev->base + REG_CAMSVCENTRAL_DCM_DIS);
	writel(1, sv_dev->base + REG_CAMSVCENTRAL_SW_CTL);
	writel(0, sv_dev->base_dma + REG_CAMSVDMATOP_SW_RST_CTL);
	writel(0, sv_dev->base + REG_CAMSVCENTRAL_STG_RST);
	writel(0x10, sv_dev->base_scq + REG_CAMSVCQTOP_SW_RST_CTL);
	writel(0, sv_dev->base_scq + REG_CAMSVCQTOP_SW_RST_CTL);
	writel(0, sv_dev->base + REG_CAMSVCENTRAL_SW_CTL);
	wmb(); /* make sure committed */



RESET_FAILURE:
	return;
}

int mtk_cam_sv_fifo_monitor_config(struct mtk_camsv_device *sv_dev,
	bool is_on, unsigned int fifo_core1, unsigned int fifo_core2,
	unsigned int fifo_core3)
{
	if (!is_fmon_support())
		return 0;

	CAMSV_WRITE_BITS(sv_dev->base_dma + REG_CAMSVDMATOP_DMA_INT_FIFO_EN,
		CAMSVDMATOP_DMA_INT_FIFO_EN, FIFO_CORE1_2_EN,
		(is_on && fifo_core1) ? 1 : 0);
	CAMSV_WRITE_BITS(sv_dev->base_dma + REG_CAMSVDMATOP_DMA_INT_FIFO_EN,
		CAMSVDMATOP_DMA_INT_FIFO_EN, FIFO_CORE2_2_EN,
		(is_on && fifo_core2) ? 1 : 0);
	CAMSV_WRITE_BITS(sv_dev->base_dma + REG_CAMSVDMATOP_DMA_INT_FIFO_EN,
		CAMSVDMATOP_DMA_INT_FIFO_EN ,FIFO_CORE3_2_EN,
		(is_on && fifo_core3) ? 1 : 0);

	CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_FIFO_INT_CORE1_THD,
		1530 << 16);
	CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_FIFO_INT_CORE2_THD,
		1530 << 16);
	CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_FIFO_INT_CORE3_THD,
		1530 << 16);

	if (CAM_DEBUG_ENABLED(RAW_INT))
		pr_info("%s int_en:0x%x fifo_core:0x%x_%x_%x fifo_core_thd:0x%x_%x_%x\n",
			__func__,
			CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_DMA_INT_FIFO_EN),
			fifo_core1, fifo_core2, fifo_core3,
			CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_FIFO_INT_CORE1_THD),
			CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_FIFO_INT_CORE2_THD),
			CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_FIFO_INT_CORE3_THD));

	return 0;
}

int mtk_cam_sv_dmao_common_config(struct mtk_camsv_device *sv_dev,
	unsigned int fifo_img_p1, unsigned int fifo_img_p2,
	unsigned int fifo_img_p3)
{
	int ret = 0;
	struct sv_dma_th_setting th_setting;
	struct sv_dma_bw_setting bw_setting;
	memset(&th_setting, 0, sizeof(struct sv_dma_th_setting));
	memset(&bw_setting, 0, sizeof(struct sv_dma_bw_setting));

	bw_setting.urgent_high = urgent_high;
	bw_setting.urgent_low = urgent_low;
	bw_setting.ultra_high = ultra_high;
	bw_setting.ultra_low = ultra_low;
	bw_setting.pultra_high =  pultra_high;
	bw_setting.pultra_low = pultra_low;

	CALL_PLAT_V4L2(
		get_sv_dma_th_setting, sv_dev->id, fifo_img_p1, fifo_img_p2,
		fifo_img_p3, &th_setting, &bw_setting);

	switch (sv_dev->id) {
	case CAMSV_0:
	case CAMSV_1:
	case CAMSV_2:
		/* wdma 1 */
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_IMG_1,
			th_setting.urgent_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_IMG_1,
			th_setting.ultra_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_IMG_1,
			th_setting.pultra_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_IMG_1,
			th_setting.dvfs_th);

		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_LEN_1,
			th_setting.urgent_len1_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_LEN_1,
			th_setting.ultra_len1_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_LEN_1,
			th_setting.pultra_len1_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_LEN_1,
			th_setting.dvfs_len1_th);

		/* wdma 2 */
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_IMG_2,
			th_setting.urgent_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_IMG_2,
			th_setting.ultra_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_IMG_2,
			th_setting.pultra_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_IMG_2,
			th_setting.dvfs_th);

		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_LEN_2,
			th_setting.urgent_len1_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_LEN_2,
			th_setting.ultra_len1_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_LEN_2,
			th_setting.pultra_len1_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_LEN_2,
			th_setting.dvfs_len1_th);

		/* wdma 3 */
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_IMG_3,
			th_setting.urgent_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_IMG_3,
			th_setting.ultra_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_IMG_3,
			th_setting.pultra_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_IMG_3,
			th_setting.dvfs_th);

		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_LEN_3,
			th_setting.urgent_len1_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_LEN_3,
			th_setting.ultra_len1_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_LEN_3,
			th_setting.pultra_len1_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_LEN_3,
			th_setting.dvfs_len1_th);

		break;

	case CAMSV_3:
		/* wdma 1 */
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_IMG_1,
			th_setting.urgent_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_IMG_1,
			th_setting.ultra_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_IMG_1,
			th_setting.pultra_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_IMG_1,
			th_setting.dvfs_th);

		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_LEN_1,
			th_setting.urgent_len1_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_LEN_1,
			th_setting.ultra_len1_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_LEN_1,
			th_setting.pultra_len1_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_LEN_1,
			th_setting.dvfs_len1_th);

		/* wdma 2 */
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_IMG_2,
			th_setting.urgent_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_IMG_2,
			th_setting.ultra_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_IMG_2,
			th_setting.pultra_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_IMG_2,
			th_setting.dvfs_th);

		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_LEN_2,
			th_setting.urgent_len1_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_LEN_2,
			th_setting.ultra_len1_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_LEN_2,
			th_setting.pultra_len1_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_LEN_2,
			th_setting.dvfs_len1_th);

		break;
	case CAMSV_4:
	case CAMSV_5:
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_IMG_1,
			th_setting.urgent_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_IMG_1,
			th_setting.ultra_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_IMG_1,
			th_setting.pultra_th);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_IMG_1,
			th_setting.dvfs_th);

		break;
	}

	/* cqi */
	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQI_E1_ORIRDMA_CON0,
		th_setting.cq1_fifo_size);
	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQI_E1_ORIRDMA_CON1,
		th_setting.cq1_pultra_th);
	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQI_E1_ORIRDMA_CON2,
		th_setting.cq1_ultra_th);
	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQI_E1_ORIRDMA_CON3,
		th_setting.cq1_urgent_th);
	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQI_E1_ORIRDMA_CON4,
		th_setting.cq1_dvfs_th);

	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQI_E2_ORIRDMA_CON0,
		th_setting.cq2_fifo_size);
	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQI_E2_ORIRDMA_CON1,
		th_setting.cq2_pultra_th);
	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQI_E2_ORIRDMA_CON2,
		th_setting.cq2_ultra_th);
	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQI_E2_ORIRDMA_CON3,
		th_setting.cq2_urgent_th);
	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQI_E2_ORIRDMA_CON4,
		th_setting.cq2_dvfs_th);

	dev_dbg(sv_dev->dev, "img:0x%x_0x%x_0x%x_0x%x img2:0x%x_0x%x_0x%x_0x%x img3:0x%x_0x%x_0x%x_0x%x len:0x%x_0x%x_0x%x_0x%x len2:0x%x_0x%x_0x%x_0x%x len3:0x%x_0x%x_0x%x_0x%x\n",
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_IMG_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_IMG_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_IMG_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_IMG_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_IMG_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_IMG_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_IMG_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_IMG_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_IMG_3),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_IMG_3),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_IMG_3),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_IMG_3),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_LEN_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_LEN_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_LEN_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_LEN_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_LEN_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_LEN_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_LEN_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_LEN_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_LEN_3),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_LEN_3),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_LEN_3),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_LEN_3));

	return ret;
}

int mtk_cam_sv_toggle_tg_db(struct mtk_camsv_device *sv_dev)
{
	int val, val2;

	val = CAMSV_READ_REG(sv_dev->base_inner + REG_CAMSVCENTRAL_PATH_CFG);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_PATH_CFG,
		CAMSVCENTRAL_PATH_CFG, SYNC_VF_EN_DB_LOAD_DIS, 1);

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_PATH_CFG,
		CAMSVCENTRAL_PATH_CFG, SYNC_VF_EN_DB_LOAD_DIS, 0);
	val2 = CAMSV_READ_REG(sv_dev->base_inner + REG_CAMSVCENTRAL_PATH_CFG);
	dev_info(sv_dev->dev, "%s 0x%x->0x%x\n", __func__, val, val2);
	return 0;
}

int mtk_cam_sv_toggle_db(struct mtk_camsv_device *sv_dev)
{
	int val, val2;

	val = CAMSV_READ_REG(sv_dev->base_inner + REG_CAMSVCENTRAL_MODULE_DB);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_MODULE_DB,
		CAMSVCENTRAL_MODULE_DB, CAM_DB_EN, 0);

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_MODULE_DB,
		CAMSVCENTRAL_MODULE_DB, CAM_DB_EN, 1);
	val2 = CAMSV_READ_REG(sv_dev->base_inner + REG_CAMSVCENTRAL_MODULE_DB);
	dev_info(sv_dev->dev, "%s 0x%x->0x%x\n", __func__, val, val2);

	return 0;
}

int mtk_cam_sv_central_common_enable(struct mtk_camsv_device *sv_dev)
{
	int ret = 0, i;

	for (i = 0; i < MAX_SV_HW_GROUPS; i++) {
		sv_dev->active_group_info[i] =
			CAMSV_READ_REG(sv_dev->base + REG_CAMSVCENTRAL_GROUP_TAG0 +
				CAMSVCENTRAL_GROUP_TAG_SHIFT * i);
		sv_dev->used_tags |= sv_dev->active_group_info[i];
	}

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_SEN_MODE,
		CAMSVCENTRAL_SEN_MODE, CMOS_EN, 1);
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_SEN_MODE,
		CAMSVCENTRAL_SEN_MODE, FLUSH_ALL_SRC_DIS, 1);
	if (atomic_read(&sv_dev->is_sub_en))
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_SEN_MODE,
			CAMSVCENTRAL_SEN_MODE, CAM_SUB_EN, 1);

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_VF_CON,
		CAMSVCENTRAL_VF_CON, VFDATA_EN, 1);

	dev_info(sv_dev->dev, "%s sen_mode:0x%x vf_con:0x%x\n",
		__func__,
		CAMSV_READ_REG(sv_dev->base + REG_CAMSVCENTRAL_SEN_MODE),
		CAMSV_READ_REG(sv_dev->base + REG_CAMSVCENTRAL_VF_CON));

	return ret;
}

int mtk_cam_sv_central_common_disable(struct mtk_camsv_device *sv_dev)
{
	int ret = 0, i;

	/* disable dma dcm before do dma reset */
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_DCM_DIS, 1);

	/* turn off interrupt */
	/* channel status must be disabled before sof status disabled */
	/* due to channel status cleared in sof irq handler */
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_CHANNEL_STATUS_EN, 0);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_COMMON_STATUS_EN, 0);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_DONE_STATUS_EN, 0);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_ERR_STATUS_EN, 0);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_SOF_STATUS_EN, 0);

	/* avoid camsv tag data leakage */
	for (i = SVTAG_START; i < SVTAG_END; i++) {
		CAMSV_WRITE_REG(sv_dev->base_inner + REG_CAMSVCENTRAL_GRAB_PXL_TAG1 +
			CAMSVCENTRAL_GRAB_PXL_TAG_SHIFT * i, 0);
		CAMSV_WRITE_REG(sv_dev->base_inner + REG_CAMSVCENTRAL_GRAB_LIN_TAG1 +
			CAMSVCENTRAL_GRAB_LIN_TAG_SHIFT * i, 0);

		CAMSV_WRITE_REG(sv_dev->base_inner + REG_CAMSVCENTRAL_INT_EXP0_TAG1 +
			(CAMSVCENTRAL_INT_EXP0_OFFSET * i), 0);
		CAMSV_WRITE_REG(sv_dev->base_inner + REG_CAMSVCENTRAL_INT_EXP1_TAG1 +
			(CAMSVCENTRAL_INT_EXP1_OFFSET * i), 0);
	}

	/* bypass tg_mode function before vf off */
	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_SEN_MODE,
		CAMSVCENTRAL_SEN_MODE, TG_MODE_OFF, 1);

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_VF_CON,
		CAMSVCENTRAL_VF_CON, VFDATA_EN, 0);

	mtk_cam_sv_toggle_tg_db(sv_dev);

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_SEN_MODE,
		CAMSVCENTRAL_SEN_MODE, CMOS_EN, 0);

	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_DMA_EN_IMG, 0);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_DMA_EN_LEN, 0);
	sv_reset(sv_dev);

	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_DCIF_SET, 0);
	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_DCIF_SEL, 0);

	CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_SLC_CTRL, 0);

	for (i = SVTAG_START; i < SVTAG_END; i++) {
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_WDMA_SLC_CTRL_IMG1A +
			CAMSVDMATOP_WDMA_SLC_TAG_SHIFT * i, 0);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_WDMA_SLC_CTRL_IMG1B +
			CAMSVDMATOP_WDMA_SLC_TAG_SHIFT * i, 0);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_WDMA_SLC_CTRL_IMG1C +
			CAMSVDMATOP_WDMA_SLC_TAG_SHIFT * i, 0);

		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_WDMA_SPECIAL_EN_IMG1A +
			CAMSVDMATOP_WDMA_SPECIAL_IMG_SHIFT * i, 0);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_WDMA_SPECIAL_EN_IMG1B +
			CAMSVDMATOP_WDMA_SPECIAL_IMG_SHIFT * i, 0);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_WDMA_SPECIAL_EN_IMG1C +
			CAMSVDMATOP_WDMA_SPECIAL_IMG_SHIFT * i, 0);

		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_WDMA_AXSLC_SIZE_IMG1A +
			CAMSVDMATOP_WDMA_SLC_TAG_SHIFT * i, 0);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_WDMA_AXSLC_SIZE_IMG1B +
			CAMSVDMATOP_WDMA_SLC_TAG_SHIFT * i, 0);
		CAMSV_WRITE_REG(sv_dev->base_dma + REG_CAMSVDMATOP_WDMA_AXSLC_SIZE_IMG1C +
			CAMSVDMATOP_WDMA_SLC_TAG_SHIFT * i, 0);
	}

	mtk_cam_sv_toggle_db(sv_dev);

	return ret;
}

int mtk_cam_sv_fbc_disable(struct mtk_camsv_device *sv_dev, unsigned int tag_idx)
{
	int ret = 0;

	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_FBC0_TAG1 +
		CAMSVCENTRAL_FBC0_TAG_SHIFT * tag_idx, 0);

	return ret;
}

int mtk_cam_sv_dev_pertag_write_rcnt(struct mtk_camsv_device *sv_dev,
	unsigned int tag_idx)
{
	int ret = 0;

	CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_FBC0_TAG1 +
		CAMSVCENTRAL_FBC0_TAG_SHIFT * tag_idx,
		CAMSVCENTRAL_FBC0_TAG1, RCNT_INC_TAG1, 1);

	return ret;
}

void mtk_cam_sv_vf_reset(struct mtk_camsv_device *sv_dev)
{
	if (CAMSV_READ_BITS(sv_dev->base + REG_CAMSVCENTRAL_VF_CON,
			CAMSVCENTRAL_VF_CON, VFDATA_EN)) {
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_VF_CON,
			CAMSVCENTRAL_VF_CON, VFDATA_EN, 0);
		mtk_cam_sv_toggle_tg_db(sv_dev);
		dev_info(sv_dev->dev, "preisp sv_vf_reset vf_en off");
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_VF_CON,
			CAMSVCENTRAL_VF_CON, VFDATA_EN, 1);
		mtk_cam_sv_toggle_tg_db(sv_dev);
		dev_info(sv_dev->dev, "preisp sv_vf_reset vf_en on");
	}
	dev_info(sv_dev->dev, "preisp sv_vf_reset");
}

int mtk_cam_sv_is_zero_fbc_cnt(struct mtk_camsv_device *sv_dev,
	unsigned int enable_tags)
{
	int i;

	for (i = SVTAG_START; i < SVTAG_END; i++) {
		if (!(enable_tags & (1 << i)))
			continue;
		if (CAMSV_READ_BITS(sv_dev->base_inner +
				REG_CAMSVCENTRAL_FBC1_TAG1 + CAMSVCENTRAL_FBC1_TAG_SHIFT * i,
				CAMSVCENTRAL_FBC1_TAG1, FBC_CNT_TAG1))
			return 0;
	}

	return 1;
}

void mtk_cam_sv_check_fbc_cnt(struct mtk_camsv_device *sv_dev,
	unsigned int tag_idx)
{
	unsigned int fbc_cnt = 0;

	fbc_cnt = CAMSV_READ_BITS(sv_dev->base +
		REG_CAMSVCENTRAL_FBC1_TAG1 + CAMSVCENTRAL_FBC1_TAG_SHIFT * tag_idx,
		CAMSVCENTRAL_FBC1_TAG1, FBC_CNT_TAG1);

	while (fbc_cnt < 2) {
		mtk_cam_sv_dev_pertag_write_rcnt(sv_dev, tag_idx);
		fbc_cnt++;
	}
}

void apply_camsv_cq(struct mtk_camsv_device *sv_dev,
	dma_addr_t cq_addr, unsigned int cq_size,
	unsigned int cq_offset, int initial)
{
#define CQ_VADDR_MASK 0xFFFFFFFF
	u32 cq_addr_lsb = (cq_addr + cq_offset) & CQ_VADDR_MASK;
	u32 cq_addr_msb = ((cq_addr + cq_offset) >> 32);

	if (cq_size == 0)
		return;

	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQ_CQ_SUB_THR0_DESC_SIZE_2,
		cq_size);
	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQ_CQ_SUB_THR0_BASEADDR_2_MSB,
		cq_addr_msb);
	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQ_CQ_SUB_THR0_BASEADDR_2,
		cq_addr_lsb);
	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQTOP_THR_START, 1);
	wmb(); /* TBC */

	if (initial) {
		/* enable stagger mode for multiple vsync(s) */
		CAMSV_WRITE_BITS(sv_dev->base_scq + REG_CAMSVCQ_CQ_EN,
			CAMSVCQ_CQ_EN, CAMSVCQ_SCQ_STAGGER_MODE, 1);
		dev_info(sv_dev->dev, "apply 1st camsv scq: addr_msb:0x%x addr_lsb:0x%x size:%d cq_en:0x%x\n",
			cq_addr_msb, cq_addr_lsb, cq_size,
			CAMSV_READ_REG(sv_dev->base_scq + REG_CAMSVCQ_CQ_EN));
	} else
		dev_dbg(sv_dev->dev, "apply camsv scq: addr_msb:0x%x addr_lsb:0x%x size:%d\n",
			cq_addr_msb, cq_addr_lsb, cq_size);
}

bool mtk_cam_is_display_ic(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsv_pipeline *sv_pipe;

	if (!ctx->num_sv_subdevs)
		return false;

	sv_pipe = &ctx->cam->pipelines.camsv[ctx->sv_subdev_idx[0]];

	return (sv_pipe->feature_pending & DISPLAY_IC) ? true : false;
}

bool mtk_cam_is_non_comb_ic(struct mtk_cam_ctx *ctx)
{
	if (!ctx->num_sv_subdevs)
		return false;

	if (ctx->seninf)
		return mtk_cam_seninf_is_non_comb_ic(ctx->seninf) != 0;
	return false;
}

void mtk_cam_update_sensor_resource(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsv_device *sv_dev;
	struct v4l2_subdev_frame_interval fi;
	struct v4l2_ctrl *ctrl;
	struct v4l2_subdev_format sd_fmt;
	unsigned long vblank = 0;

	if (ctx->hw_sv == NULL)
		return;
	sv_dev = dev_get_drvdata(ctx->hw_sv);

	memset(&sv_dev->sensor_res, 0, sizeof(struct mtk_cam_resource_sensor_v2));
	memset(&fi, 0, sizeof(fi));
	memset(&sd_fmt, 0, sizeof(sd_fmt));

	if (ctx->sensor) {
		fi.pad = 0;
		fi.reserved[0] = V4L2_SUBDEV_FORMAT_ACTIVE;
#if (KERNEL_VERSION(6, 7, 0) < LINUX_VERSION_CODE)
		fi.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		v4l2_subdev_call_state_active(ctx->sensor, pad, get_frame_interval, &fi);
#else
		v4l2_subdev_call(ctx->sensor, video, g_frame_interval, &fi);
#endif

		ctrl = v4l2_ctrl_find(ctx->sensor->ctrl_handler, V4L2_CID_VBLANK);
		if (!ctrl)
			dev_info(ctx->cam->dev, "[%s] ctrl is NULL\n", __func__);
		else
			vblank = v4l2_ctrl_g_ctrl(ctrl);

		sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		sd_fmt.pad = PAD_SRC_RAW0;
		v4l2_subdev_call(ctx->seninf, pad, get_fmt, NULL, &sd_fmt);

		/* update sensor resource */
		sv_dev->sensor_res.width = sd_fmt.format.width;
		sv_dev->sensor_res.height = sd_fmt.format.height;
		sv_dev->sensor_res.code = sd_fmt.format.code;
		sv_dev->sensor_res.interval.numerator = fi.interval.numerator;
		sv_dev->sensor_res.interval.denominator = fi.interval.denominator;
		sv_dev->sensor_res.vblank = vblank;
	}
}

struct mtk_cam_seninf_sentest_param *
	mtk_cam_get_sentest_param(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_v4l2_pipelines *ppls = &cam->pipelines;
	struct mtk_camsv_pipeline *sv_pipe = NULL;
	struct mtk_camsv_pad_config *pad = NULL;
	unsigned long submask;
	int i;

	submask = bit_map_subset_of(MAP_SUBDEV_CAMSV, ctx->used_pipe);
	for (i = 0; i < ppls->num_camsv && submask; i++, submask >>= 1) {
		if (!(submask & 0x1))
			continue;

		sv_pipe = &ppls->camsv[i];
		pad = &sv_pipe->pad_cfg[MTK_CAMSV_SINK];

		break;
	}

	if (sv_pipe && pad) {
		if (atomic_read(&sv_pipe->is_sentest_param_updated))
			return &sv_pipe->sentest_param;

		memset(&sv_pipe->sentest_param, 0,
			sizeof(struct mtk_cam_seninf_sentest_param));

		mtk_cam_seninf_get_sentest_param(ctx->seninf,
			pad->mbus_fmt.code,
			&sv_pipe->sentest_param);
		atomic_set(&sv_pipe->is_sentest_param_updated, 1);

		pr_info("%s: mbus_code:0x%x is_lbmf:%d\n",
			__func__,
			pad->mbus_fmt.code,
			sv_pipe->sentest_param.is_lbmf);

		return &sv_pipe->sentest_param;
	}

	pr_info("%s: sv pipe not found(used_pipe:0x%lx)\n",
		__func__, ctx->used_pipe);

	return NULL;
}

int mtk_cam_sv_golden_set(struct mtk_camsv_device *sv_dev, bool is_golden_set)
{
	int ret = 0;
#ifdef MMINFRA_PLUS_ONE
	mtk_mmdvfs_camsv_dc_enable(sv_dev->id, is_golden_set);
	dev_info(sv_dev->dev, "%s: is_golden_set:%d",
		__func__, (is_golden_set) ? 1 : 0);
#endif
	return ret;
}

void sv_sram_req(struct mtk_camsv_device *sv_dev,
		enum SV_DF_REQ action, unsigned int core_idx, unsigned int num)
{
	unsigned int mask = 0x1F;

	if (!num)
		return;

	num = num / DF_UNIT;

	/* two's complement */
	if (action == SV_DF_REQ_DEQ)
		num = ((~num & mask) + 1) & mask;

	pr_info("%s: req_num:0x%x\n", __func__, num);

	writel((num << 4) | 0x1,
		sv_dev->base + REG_CAMSVCENTRAL_SHARE_SRAM_PORT1 +
		(core_idx * 0x4));
}

int mtk_cam_sv_df_config(struct mtk_camsv_device *sv_dev)
{
	int ret = 0;

	if (disable_camsv_df_mode || sv_dev->id >= MAX_SV_DF_HW_NUM)
		goto EXIT;

	/* set sram free timeout cycles */
	writel(0x2000000, sv_dev->base + REG_CAMSVCENTRAL_SHARE_SRAM_TIME_OUT);

	/* enable auto mode */
	writel(0x1C0, sv_dev->base_dma + REG_CAMSVDMATOP_CTL);

EXIT:
	return ret;
}

int mtk_cam_sv_run_df_reset(struct mtk_camsv_device *sv_dev, bool is_on)
{
	struct mtk_cam_device *cam_dev = sv_dev->cam;
	unsigned int fifo_core1 = 0, fifo_core2 = 0, fifo_core3 = 0;
	int share_sram_ctl;
	int ret = 0;

	if (disable_camsv_df_mode || sv_dev->id >= MAX_SV_DF_HW_NUM) {
		/* fifo monitor */
		CALL_PLAT_V4L2(
			get_sv_fifo_core_setting, sv_dev->id,
			&fifo_core1, &fifo_core2, &fifo_core3);
		mtk_cam_sv_fifo_monitor_config(sv_dev, is_on,
			fifo_core1, fifo_core2, fifo_core3);
		goto EXIT;
	}

	writel(1, sv_dev->base + REG_CAMSVCENTRAL_SHARE_SRAM_CTL);
	ret = readx_poll_timeout(readl, sv_dev->base + REG_CAMSVCENTRAL_SHARE_SRAM_CTL,
				 share_sram_ctl,
				 share_sram_ctl & 0x10,
				 1 /* delay, us */,
				 100000 /* timeout, us */);
	if (ret < 0)
		dev_info(sv_dev->dev, "%s: share sram reset timeout\n", __func__);

	writel(0, sv_dev->base + REG_CAMSVCENTRAL_SHARE_SRAM_CTL);

	/* disable auto mode */
	writel(0, sv_dev->base_dma + REG_CAMSVDMATOP_CTL);

	mtk_cam_sv_df_reset(&cam_dev->sv_df_mgr, sv_dev->id);

	/* fifo monitor */
	fifo_core1 =
		mtk_cam_sv_df_get_runtime_fifo_size(&cam_dev->sv_df_mgr,
			sv_dev->id, 0);
	fifo_core2 =
		mtk_cam_sv_df_get_runtime_fifo_size(&cam_dev->sv_df_mgr,
			sv_dev->id, 1);
	fifo_core3 =
		mtk_cam_sv_df_get_runtime_fifo_size(&cam_dev->sv_df_mgr,
			sv_dev->id, 2);
	mtk_cam_sv_fifo_monitor_config(sv_dev, is_on,
		fifo_core1, fifo_core2, fifo_core3);

EXIT:
	return ret;
}

int mtk_cam_sv_run_df_actions(struct mtk_camsv_device *sv_dev)
{
	int ret = 0, i;
	struct mtk_cam_device *cam_dev;

	if (sv_dev == NULL)
		goto EXIT;

	cam_dev = sv_dev->cam;

	if (disable_camsv_df_mode || sv_dev->id >= MAX_SV_DF_HW_NUM)
		goto EXIT;

	if (mtk_cam_sv_df_next_action(&cam_dev->sv_df_mgr,
		sv_dev->id, &sv_dev->sv_df_action)) {
		for (i = 0; i < MAX_DMA_CORE; i++) {
			sv_sram_req(sv_dev, sv_dev->sv_df_action.actions[i].action,
				i, sv_dev->sv_df_action.actions[i].req_num);
		}
	}

EXIT:
	return ret;
}

int mtk_cam_sv_run_df_action_ack(struct mtk_camsv_device *sv_dev,
		unsigned int top_status)
{
	struct mtk_cam_device *cam_dev = sv_dev->cam;
	unsigned int fifo_core1, fifo_core2, fifo_core3;
	int ret = 0, i;

	if (disable_camsv_df_mode || sv_dev->id >= MAX_SV_DF_HW_NUM)
		goto EXIT;

	pr_info("%s: top_status:0x%x\n", __func__, top_status);

	for (i = 0; i < MAX_DMA_CORE; i++) {
		if (top_status & 0x1 << (i * 4)) {
			sv_dev->sv_df_action.actions[i].action = SV_DF_REQ_NONE;
			sv_dev->sv_df_action.actions[i].req_num = 0;
		} else if (top_status & (0x6 << (i * 4))) {
			sv_sram_req(sv_dev, sv_dev->sv_df_action.actions[i].action,
				i, sv_dev->sv_df_action.actions[i].req_num);
			dev_info(sv_dev->dev, "%s failed sram request(core_idx:%d/action:%d/req_num:%d)\n",
				__func__, i,
				sv_dev->sv_df_action.actions[i].action,
				sv_dev->sv_df_action.actions[i].req_num);
		} else if (top_status & 0x8 << (i * 4)) {
			dev_info(sv_dev->dev, "%s double sram request(core_idx:%d/action:%d/req_num:%d)\n",
				__func__, i,
				sv_dev->sv_df_action.actions[i].action,
				sv_dev->sv_df_action.actions[i].req_num);
		}
	}

	if (mtk_cam_sv_check_df_action_done(&sv_dev->sv_df_action)) {
		mtk_cam_sv_df_action_ack(&cam_dev->sv_df_mgr, sv_dev->id);

		/* fifo monitor */
		fifo_core1 =
			mtk_cam_sv_df_get_runtime_fifo_size(&cam_dev->sv_df_mgr,
				sv_dev->id, 0);
		fifo_core2 =
			mtk_cam_sv_df_get_runtime_fifo_size(&cam_dev->sv_df_mgr,
				sv_dev->id, 1);
		fifo_core3 =
			mtk_cam_sv_df_get_runtime_fifo_size(&cam_dev->sv_df_mgr,
				sv_dev->id, 2);
		mtk_cam_sv_fifo_monitor_config(sv_dev, true,
			fifo_core1, fifo_core2, fifo_core3);
	}

EXIT:
	return ret;
}

int mtk_cam_sv_run_df_bw_update(struct mtk_camsv_device *sv_dev,
		unsigned long long bw)
{
	struct mtk_cam_device *cam_dev = sv_dev->cam;
	int ret = 0;

	if (disable_camsv_df_mode || sv_dev->id >= MAX_SV_DF_HW_NUM)
		goto EXIT;

	mtk_cam_sv_df_update_bw(&cam_dev->sv_df_mgr, sv_dev->id, bw);

EXIT:
	return ret;
}

int mtk_cam_get_sv_tag_index(struct mtk_camsv_tag_info *arr_tag,
	unsigned int pipe_id)
{
	int i;

	for (i = SVTAG_START; i < SVTAG_END; i++) {
		struct mtk_camsv_tag_info *tag_info = &arr_tag[i];

		if (tag_info->sv_pipe && (tag_info->sv_pipe->id == pipe_id))
			return i;
		if (tag_info->mraw_pipe && (tag_info->mraw_pipe->id == pipe_id))
			return i;
	}

	pr_info("[%s] tag is not found by pipe_id(%d)", __func__, pipe_id);
	return SVTAG_UNKNOWN;
}

unsigned int mtk_cam_get_seninf_pad_index(struct mtk_camsv_tag_info *arr_tag,
	unsigned int pipe_id)
{
	int i;

	for (i = SVTAG_START; i < SVTAG_END; i++) {
		struct mtk_camsv_tag_info *tag_info = &arr_tag[i];

		if (tag_info->sv_pipe && (tag_info->sv_pipe->id == pipe_id))
			return tag_info->sv_pipe->seninf_padidx;

		if (tag_info->mraw_pipe && (tag_info->mraw_pipe->id == pipe_id))
			return tag_info->mraw_pipe->seninf_padidx;
	}

	pr_info("[%s] seninf pad is not found by pipe_id(%d)", __func__, pipe_id);
	return 0;
}

int mtk_camsv_get_stress_mode(struct mtk_camsv_device *slave_sv_dev,
	unsigned int stress_mode)
{
	if (stress_mode >= 1 && stress_mode <= 3)
		return stress_mode;
	else if (stress_mode >= 4 && stress_mode <= 6)
		return stress_mode - 3;
	else if (stress_mode >= 7 && stress_mode <= 9)
		return stress_mode - 6;

	return 0;
}

int mtk_cam_slave_sv_report_bw(struct mtk_cam_ctx *ctx,
	struct mtk_camsv_device *slave_sv_dev)
{
#define SLAVE_SV_PORT_NUM 2
	unsigned int hrt_bw_stress_table[3] = {469, 682, 938};
	unsigned int srt_bw_stress_table[3] = {451, 653, 904};
	int i;
	unsigned int stress_mode = 0;

	if (slave_sv_dev->stress_test_mode == 0)
		return 0;

	stress_mode = slave_sv_dev->stress_test_mode - 1;
	for (i = 1; i <= SLAVE_SV_PORT_NUM; i++) {
		mtk_icc_set_bw(slave_sv_dev->qos.cam_path[i].path,
			srt_bw_stress_table[stress_mode] * 1024 / SLAVE_SV_PORT_NUM,
			hrt_bw_stress_table[stress_mode] * 1024 / SLAVE_SV_PORT_NUM);
		mtk_cam_isp8s_bwr_set_chn_bw(ctx->cam->bwr,
			get_sv_bwr_engine(slave_sv_dev->id), get_sv_axi_port(slave_sv_dev->id, i),
			0, srt_bw_stress_table[stress_mode] / SLAVE_SV_PORT_NUM, 0,
			hrt_bw_stress_table[stress_mode]/ SLAVE_SV_PORT_NUM, false);
	}

	mtk_cam_isp8s_bwr_set_ttl_bw(ctx->cam->bwr,
		get_sv_bwr_engine(slave_sv_dev->id), srt_bw_stress_table[stress_mode], hrt_bw_stress_table[stress_mode],
		false);
	return 0;
}

int mtk_cam_slave_sv_dev_config(struct mtk_cam_ctx *ctx,
	struct mtk_camsv_device *slave_sv_dev)
{
	int ret;
	struct mtk_cam_pool_buffer *buf;
	unsigned int imgo_lsb, imgo_msb, imgo_stride;

	if (CAMSV_READ_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_VF_CON) & 0x1) {
		pr_info("%s already config bypass\n", __func__);
		return 0;
	}

	CAMSV_WRITE_BITS(slave_sv_dev->base + REG_CAMSVCENTRAL_SEN_MODE,
		CAMSVCENTRAL_SEN_MODE, CMOS_EN, 1);
	CAMSV_WRITE_BITS(slave_sv_dev->base + REG_CAMSVCENTRAL_ERR_CTL,
		CAMSVCENTRAL_ERR_CTL, CAMSVCENTRAL_GRAB_ERR_EN, 1);
	CAMSV_WRITE_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_DMA_EN_IMG, 0x1);
	CAMSV_WRITE_BITS(slave_sv_dev->base + REG_CAMSVCENTRAL_DONE_STATUS_EN,
		CAMSVCENTRAL_DONE_STATUS_EN, SW_PASS1_DONE_0_ST_EN, 1);
	CAMSV_WRITE_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_ERR_STATUS_EN, 0x701);
	CAMSV_WRITE_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_SOF_STATUS_EN, 0x1 << 2);
	CAMSV_WRITE_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_FIRST_TAG, 0x1);
	CAMSV_WRITE_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_LAST_TAG, 0x1);
	CAMSV_WRITE_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_GROUP_TAG0 + CAMSVCENTRAL_GROUP_TAG_SHIFT * 0, 0x1);
	CAMSV_WRITE_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_GROUP_TAG0 + CAMSVCENTRAL_GROUP_TAG_SHIFT * 1, 0x0);
	CAMSV_WRITE_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_GROUP_TAG0 + CAMSVCENTRAL_GROUP_TAG_SHIFT * 2, 0x0);
	CAMSV_WRITE_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_GROUP_TAG0 + CAMSVCENTRAL_GROUP_TAG_SHIFT * 3, 0x0);

	CAMSV_WRITE_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_GRAB_PXL_TAG1, 0x1000 << 16);
	CAMSV_WRITE_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_GRAB_LIN_TAG1, 0xc00 << 16);

	ret = mtk_cam_buffer_pool_fetch(
			&ctx->camsv_stress_pool, &ctx->camsv_stress_buf);
	if (ret)
		pr_info("[%s] fail to fetch\n", __func__);

	buf = &ctx->camsv_stress_buf;
	imgo_lsb = buf->daddr & 0xffffffff;
	imgo_msb = buf->daddr >> 32;
	imgo_stride = 0x1400 << 16 | 0x10;
	/* wdma basic / base address / format*/
	CAMSV_WRITE_REG(slave_sv_dev->base_dma + REG_CAMSVDMATOP_WDMA_BASIC_IMG1_A, imgo_stride);
	CAMSV_WRITE_REG(slave_sv_dev->base_dma + REG_CAMSVDMATOP_WDMA_BASE_ADDR_IMG1_A, imgo_lsb);
	CAMSV_WRITE_REG(slave_sv_dev->base_dma + REG_CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG1_A, imgo_msb);
	CAMSV_WRITE_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_FORMAT_TAG1, 0x1);
	mtk_cam_sv_dmao_common_config(slave_sv_dev, 0, 0, 0);
	mtk_cam_sv_ddren_qos_coh_config(slave_sv_dev, 0);

	pr_info("%s sen_mod0x%x dma_en0x%x err_en0x%x sof_en0x%x first_tag0x%x last_tag0x%x group0x%x grab0x%x_%x dma_basic0x%x addr0x%x_%x fmt 0x%x\n",
		__func__,
		CAMSV_READ_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_SEN_MODE),
		CAMSV_READ_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_DMA_EN_IMG),
		CAMSV_READ_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_ERR_STATUS_EN),
		CAMSV_READ_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_SOF_STATUS_EN),
		CAMSV_READ_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_FIRST_TAG),
		CAMSV_READ_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_LAST_TAG),
		CAMSV_READ_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_GROUP_TAG0),
		CAMSV_READ_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_GRAB_PXL_TAG1),
		CAMSV_READ_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_GRAB_LIN_TAG1),
		CAMSV_READ_REG(slave_sv_dev->base_dma + REG_CAMSVDMATOP_WDMA_BASIC_IMG1_A),
		CAMSV_READ_REG(slave_sv_dev->base_dma + REG_CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG1_A),
		CAMSV_READ_REG(slave_sv_dev->base_dma + REG_CAMSVDMATOP_WDMA_BASE_ADDR_IMG1_A),
		CAMSV_READ_REG(slave_sv_dev->base + REG_CAMSVCENTRAL_FORMAT_TAG1));

	mtk_cam_sv_toggle_db(slave_sv_dev);
	mtk_cam_slave_sv_report_bw(ctx, slave_sv_dev);
	return 0;
}

int mtk_cam_sv_dev_config(struct mtk_cam_ctx *ctx,
	struct mtk_camsv_device *sv_dev,
	unsigned int sub_ratio, int frm_time_us)
{
	engine_fsm_reset(&sv_dev->fsm, sv_dev->dev);
	sv_dev->cq_ref = NULL;

	sv_dev->first_tag = 0;
	sv_dev->last_tag = 0;

	sv_dev->handled_tags = 0;
	sv_dev->used_tags = 0;
	sv_dev->drop_tags = 0;

	sv_dev->streaming_tag_cnt = 0;
	sv_dev->sof_count = 0;
	sv_dev->tg_cnt = 0;
	sv_dev->ois_updated_seq = 0;
	sv_dev->camsv_error_count = 0;

	atomic_set(&sv_dev->is_otf, 0);
	atomic_set(&sv_dev->is_seamless, 0);
	atomic_set(&sv_dev->is_fifo_full, 0);
	atomic_set(&sv_dev->is_sub_en, 0);

	mtk_cam_sv_df_config(sv_dev);
	mtk_cam_sv_dmao_common_config(sv_dev, 0, 0, 0);
	mtk_cam_sv_cq_config(sv_dev, sub_ratio);
	mtk_cam_sv_ddren_qos_coh_config(sv_dev, frm_time_us);
	mtk_cam_sv_fifo_dbg_port_config(sv_dev);

	if (atomic_read(&sv_dev->is_slave_on))
		mtk_cam_slave_sv_dev_config(ctx, sv_dev->slave_sv_dev);

	if (CAM_DEBUG_ENABLED(RAW_INT))
		dev_info(sv_dev->dev, "[%s] sub_ratio:%d set seamless check\n", __func__, sub_ratio);

	return 0;
}

void mtk_cam_sv_fill_pdp_tag_info(struct mtk_camsv_tag_info *arr_tag,
	struct mtkcam_ipi_config_param *ipi_config,
	struct mtk_camsv_tag_param *tag_param, unsigned int hw_scen,
	unsigned int pixelmode, unsigned int sub_ratio,
	unsigned int mbus_width, unsigned int mbus_height,
	unsigned int mbus_code, unsigned int is_unpack_msb,
	struct mtk_mraw_pipeline *pipeline)
{
	struct mtk_camsv_tag_info *tag_info = &arr_tag[tag_param->tag_idx];
	struct mtkcam_ipi_input_param *cfg_in_param =
		&ipi_config->sv_input[tag_param->tag_idx].input;

	tag_info->mraw_pipe = pipeline;
	tag_info->seninf_padidx = tag_param->seninf_padidx;
	tag_info->hw_scen = hw_scen;
	tag_info->tag_order = tag_param->tag_order;
	tag_info->pixel_mode = pixelmode;
	tag_info->is_meta_tag  = true;

	ipi_config->sv_input[tag_param->tag_idx].is_unpack_msb =
		is_unpack_msb;

	cfg_in_param->pixel_mode = pixelmode;
	cfg_in_param->data_pattern = 0x0;
	cfg_in_param->in_crop.p.x = 0x0;
	cfg_in_param->in_crop.p.y = 0x0;
	cfg_in_param->in_crop.s.w = mbus_width;
	cfg_in_param->in_crop.s.h = mbus_height;
	cfg_in_param->fmt = sensor_mbus_to_ipi_fmt(mbus_code);
	cfg_in_param->raw_pixel_id = sensor_mbus_to_ipi_pixel_id(mbus_code);
	cfg_in_param->subsample = sub_ratio;
}

void mtk_cam_sv_fill_tag_info(struct mtk_camsv_tag_info *arr_tag,
	struct mtkcam_ipi_config_param *ipi_config,
	struct mtk_camsv_tag_param *tag_param, unsigned int hw_scen,
	unsigned int pixelmode, unsigned int sub_ratio,
	unsigned int mbus_width, unsigned int mbus_height,
	unsigned int mbus_code, unsigned int is_unpack_msb,
	struct mtk_camsv_pipeline *pipeline)
{
	struct mtk_camsv_tag_info *tag_info = &arr_tag[tag_param->tag_idx];
	struct mtkcam_ipi_input_param *cfg_in_param =
		&ipi_config->sv_input[tag_param->tag_idx].input;

	tag_info->sv_pipe = pipeline;
	tag_info->seninf_padidx = tag_param->seninf_padidx;
	tag_info->hw_scen = hw_scen;
	tag_info->tag_order = tag_param->tag_order;
	tag_info->pixel_mode = pixelmode;
	tag_info->is_meta_tag = false;

	/* msb swap(nv21_10) */
	ipi_config->sv_input[tag_param->tag_idx].is_unpack_msb =
		is_unpack_msb;

	cfg_in_param->pixel_mode = pixelmode;
	cfg_in_param->data_pattern = 0x0;
	cfg_in_param->in_crop.p.x = 0x0;
	cfg_in_param->in_crop.p.y = 0x0;
	cfg_in_param->in_crop.s.w = mbus_width;
	cfg_in_param->in_crop.s.h = mbus_height;
	cfg_in_param->fmt = sensor_mbus_to_ipi_fmt(mbus_code);
	cfg_in_param->raw_pixel_id = sensor_mbus_to_ipi_pixel_id(mbus_code);
	cfg_in_param->subsample = sub_ratio;
	// handle yuv special case
	if (cfg_in_param->fmt == MTKCAM_IPI_IMG_FMT_YUYV||
		cfg_in_param->fmt == MTKCAM_IPI_IMG_FMT_UYVY||
		cfg_in_param->fmt == MTKCAM_IPI_IMG_FMT_VYUY||
		cfg_in_param->fmt == MTKCAM_IPI_IMG_FMT_YVYU) {
		cfg_in_param->in_crop.s.w = mbus_width * 2;
	}
	// handle rgb888 special case
	if (cfg_in_param->fmt == MTKCAM_IPI_IMG_FMT_RGB888)
		cfg_in_param->in_crop.s.w = mbus_width * 3;
}
int mtk_cam_get_sv_meta_tag(int meta_tag_num, int pad_id)
{
	switch (meta_tag_num) {
	case 1:
		if (pad_id == PAD_SRC_PDAF0)
			return SVTAG_5;
		else
			return SVTAG_4;
	case 2:
		if (pad_id == PAD_SRC_PDAF1)
			return SVTAG_4;
		else
			return SVTAG_5;
	case 3:
		if (pad_id == PAD_SRC_PDAF1)
			return SVTAG_4;
		else if (pad_id == PAD_SRC_PDAF2)
			return SVTAG_6;
		else
			return SVTAG_5;
	case 4:
		if (pad_id == PAD_SRC_PDAF1)
			return SVTAG_4;
		else if (pad_id == PAD_SRC_PDAF3)
			return SVTAG_5;
		else if (pad_id == PAD_SRC_PDAF2)
			return SVTAG_6;
		else
			return SVTAG_7;
	default:
		break;
	}
	pr_info("%s error. not support pad id %d meta tag num %d\n",
		__func__, pad_id, meta_tag_num);
	return 0;
}
int mtk_cam_sv_get_tag_param(struct mtk_camsv_tag_param *arr_tag_param,
	unsigned int hw_scen, unsigned int exp_no, unsigned int req_amount,
	bool is_dcg_with_vs, bool is_fusion)
{
	int ret = 0;

	if (hw_scen == (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_STAGGER)) ||
		hw_scen == (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC_STAGGER)) ||
		hw_scen == (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_OFFLINE_STAGGER))) {
		if (exp_no == 2 && is_fusion)
			memcpy(arr_tag_param, sv_tag_param_2exp_stagger,
				sizeof(struct mtk_camsv_tag_param) * req_amount);
		else if (exp_no == 2 && is_dcg_with_vs && !is_fusion)
			memcpy(arr_tag_param, sv_tag_param_2exp_dcg_vs,
				sizeof(struct mtk_camsv_tag_param) * req_amount);
		else if (exp_no == 3 && is_dcg_with_vs)
			memcpy(arr_tag_param, sv_tag_param_3exp_dcg_vs,
				sizeof(struct mtk_camsv_tag_param) * req_amount);
		else if (exp_no == 3)
			memcpy(arr_tag_param, sv_tag_param_3exp_stagger,
				sizeof(struct mtk_camsv_tag_param) * req_amount);
		else
			memcpy(arr_tag_param, sv_tag_param_normal,
				sizeof(struct mtk_camsv_tag_param) * req_amount);
	} else if (hw_scen == (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_ON_THE_FLY)) ||
		hw_scen == (1 << HWPATH_ID(MTKCAM_IPI_HW_PATH_DC))) {
		memcpy(arr_tag_param, sv_tag_param_normal,
			sizeof(struct mtk_camsv_tag_param) * req_amount);
	} else if (hw_scen == (1 << MTKCAM_SV_SPECIAL_SCENARIO_DISPLAY_IC)) {
		memcpy(arr_tag_param, sv_tag_param_display_ic,
			sizeof(struct mtk_camsv_tag_param) * req_amount);
	} else if (hw_scen == (1 << MTKCAM_SV_SPECIAL_SCENARIO_NON_COMB_IC)) {
		memcpy(arr_tag_param, sv_tag_param_non_comb_ic,
			sizeof(struct mtk_camsv_tag_param) * req_amount);
	} else if (hw_scen == (1 << MTKCAM_SV_SPECIAL_SCENARIO_EXT_ISP)) {
		memcpy(arr_tag_param, sv_tag_param_extisp,
			sizeof(struct mtk_camsv_tag_param) * req_amount);
	} else {
		pr_info("failed to get tag param(hw_scen:0x%x/exp_no:%d)",
			hw_scen, exp_no);
		ret = 1;
	}

	return ret;
}

int mtk_cam_sv_cq_config(struct mtk_camsv_device *sv_dev, unsigned int sub_ratio)
{
	/* cq en */
	CAMSV_WRITE_BITS(sv_dev->base_scq + REG_CAMSVCQ_CQ_EN,
		CAMSVCQ_CQ_EN, CAMSVCQ_CQ_DB_EN, 1);
	CAMSV_WRITE_BITS(sv_dev->base_scq + REG_CAMSVCQ_CQ_EN,
		CAMSVCQ_CQ_EN, CAMSVCQ_SCQ_STAGGER_MODE, 0);
	CAMSV_WRITE_BITS(sv_dev->base_scq + REG_CAMSVCQ_CQ_EN,
		CAMSVCQ_CQ_EN, CAMSVCQ_SCQ_SUBSAMPLE_EN, (sub_ratio) ? 1 : 0);

	/* cq sub en */
	CAMSV_WRITE_BITS(sv_dev->base_scq + REG_CAMSVCQ_CQ_SUB_EN,
		CAMSVCQ_CQ_SUB_EN, CAMSVCQ_CQ_SUB_DB_EN, 1);

	/* cq dma dcm dis enable */
	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQTOP_DCM_DIS, 0x7);

	/* scq start period */
	CAMSV_WRITE_REG(sv_dev->base_scq  + REG_CAMSVCQ_SCQ_START_PERIOD,
		0xFFFFFFFF);

	/* cq sub thr0 ctl */
	CAMSV_WRITE_BITS(sv_dev->base_scq + REG_CAMSVCQ_CQ_SUB_THR0_CTL,
		CAMSVCQ_CQ_SUB_THR0_CTL, CAMSVCQ_CQ_SUB_THR0_MODE, 1);
	CAMSV_WRITE_BITS(sv_dev->base_scq + REG_CAMSVCQ_CQ_SUB_THR0_CTL,
		CAMSVCQ_CQ_SUB_THR0_CTL, CAMSVCQ_CQ_SUB_THR0_EN, 1);

	/* cq int en */
	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQTOP_INT_0_EN,
		ERR_ST_MASK_CQ_ERR | CAMSVCQTOP_SCQ_SUB_THR_DONE);

	wmb(); /* TBC */

	dev_dbg(sv_dev->dev, "[%s] cq_en:0x%x_%x start_period:0x%x cq_sub_thr0_ctl:0x%x cq_int_en:0x%x cq_dcm0x%x\n",
		__func__,
		CAMSV_READ_REG(sv_dev->base_scq + REG_CAMSVCQ_CQ_EN),
		CAMSV_READ_REG(sv_dev->base_scq + REG_CAMSVCQ_CQ_SUB_EN),
		CAMSV_READ_REG(sv_dev->base_scq + REG_CAMSVCQ_SCQ_START_PERIOD),
		CAMSV_READ_REG(sv_dev->base_scq + REG_CAMSVCQ_CQ_SUB_THR0_CTL),
		CAMSV_READ_REG(sv_dev->base_scq + REG_CAMSVCQTOP_INT_0_EN),
		CAMSV_READ_REG(sv_dev->base_scq + REG_CAMSVCQTOP_DCM_DIS));

	return 0;
}

#define HW_TIMER_INC_PERIOD   0x2
#define DDR_GEN_BEFORE_US     10
#define QOS_GEN_BEFORE_US     100
int mtk_cam_sv_ddren_qos_coh_config(struct mtk_camsv_device *sv_dev, int frm_time_us)
{
	int ddr_gen_pulse, qos_gen_pulse, coh_gen_pulse;

	if (CAM_DEBUG_ENABLED(MMQOS))
		pr_info("%s frm_time_us %d\n", __func__, frm_time_us);

	if (frm_time_us == -1)
		return 0;

	coh_gen_pulse = ddr_gen_pulse = (frm_time_us - DDR_GEN_BEFORE_US) * SCQ_DEFAULT_CLK_RATE /
		(2 * (HW_TIMER_INC_PERIOD + 1)) - 1;

	qos_gen_pulse = (frm_time_us - QOS_GEN_BEFORE_US) * SCQ_DEFAULT_CLK_RATE /
		(2 * (HW_TIMER_INC_PERIOD + 1)) - 1;

	if (debug_ddren_camsv_sw_mode || (ddr_gen_pulse < 0 || qos_gen_pulse < 0)) {
		/* sw ddr en */
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_DDR_CFG,
			CAMSVCENTRAL_DDR_CFG, DDR_SET, 1);

		/* cq en */
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_DDR_CFG,
			CAMSVCENTRAL_DDR_CFG, DDR_OR_CQ_EN, 1);

		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_COH_CFG,
			CAMSVCENTRAL_COH_REQ_CFG, COH_REQ_SET, 1);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_COH_CFG,
			CAMSVCENTRAL_COH_REQ_CFG, COH_REQ_OR_CQ_EN, 1);

		/* sw bw_qos en */
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_BW_QOS_CFG,
			CAMSVCENTRAL_BW_QOS_CFG, BW_QOS_SET, 1);

		/* cq en */
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_BW_QOS_CFG,
			CAMSVCENTRAL_BW_QOS_CFG, BW_QOS_OR_CQ_EN, 1);

		if (ddr_gen_pulse < 0 || qos_gen_pulse < 0)
			pr_info("%s: framelength too small, so use sw mode\n", __func__);
	} else {
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_DDR_CFG,
			CAMSVCENTRAL_DDR_CFG, DDR_MODE_SEL, 0);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_DDR_CFG,
			CAMSVCENTRAL_DDR_CFG, DDR_TIMER_EN, 1);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_DDR_THRESHOLD,
			ddr_gen_pulse);

		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_COH_CFG,
			CAMSVCENTRAL_COH_REQ_CFG, COH_REQ_MODE_SEL, 0);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_COH_CFG,
			CAMSVCENTRAL_COH_REQ_CFG, COH_REQ_TIMER_EN, 1);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_COH_THRESHOLD,
			coh_gen_pulse);

		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_BW_QOS_CFG,
			CAMSVCENTRAL_BW_QOS_CFG, BW_QOS_MODE_SEL, 0);
		CAMSV_WRITE_BITS(sv_dev->base + REG_CAMSVCENTRAL_BW_QOS_CFG,
			CAMSVCENTRAL_BW_QOS_CFG, BW_QOS_TIMER_EN, 1);
		CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_BW_QOS_THRESHOLD,
			qos_gen_pulse);
	}

	return 0;
}

void mtk_cam_sv_update_start_period(
	struct mtk_camsv_device *sv_dev, int scq_ms)
{
	u32 scq_cnt_rate, start_period;

	CAMSV_WRITE_REG(sv_dev->base + REG_CAMSVCENTRAL_TIME_STAMP_CTL, 0x101);
	CAMSV_WRITE_REG(sv_dev->base +
		REG_CAMSVCENTRAL_TIME_STAMP_INC_CNT, CAMSV_TS_CNT);

	/* scq count rate(khz) */
	scq_cnt_rate = SCQ_DEFAULT_CLK_RATE * 1000 / ((CAMSV_TS_CNT + 1) * 2);
	start_period = (scq_ms == -1) ? 0xFFFFFFFF : scq_ms * scq_cnt_rate;

	/* scq start period */
	CAMSV_WRITE_REG(sv_dev->base_scq + REG_CAMSVCQ_SCQ_START_PERIOD,
		start_period);

	dev_info(sv_dev->dev, "[%s] start_period:0x%x ts_cnt:%d, scq_ms:%d\n",
		__func__,
		CAMSV_READ_REG(sv_dev->base_scq + REG_CAMSVCQ_SCQ_START_PERIOD),
		CAMSV_TS_CNT, scq_ms);
}

int mtk_cam_sv_cq_disable(struct mtk_camsv_device *sv_dev)
{
	CAMSV_WRITE_BITS(sv_dev->base_scq + REG_CAMSVCQ_CQ_SUB_THR0_CTL,
		CAMSVCQ_CQ_SUB_THR0_CTL, CAMSVCQ_CQ_SUB_THR0_EN, 0);

	return 0;
}

int mtk_cam_sv_dev_pertag_stream_on(
	struct mtk_camsv_device *sv_dev,
	unsigned int tag_idx,
	bool on)
{
	int ret = 0;

	if (on) {
		if (sv_dev->streaming_tag_cnt == sv_dev->used_tag_cnt)
			goto EXIT;

		sv_dev->streaming_tag_cnt++;
		if (sv_dev->streaming_tag_cnt == sv_dev->used_tag_cnt) {
			ret |= mtk_cam_sv_central_common_enable(sv_dev);
			if (atomic_read(&sv_dev->is_slave_on)) {
				mtk_cam_sv_central_common_enable(sv_dev->slave_sv_dev);
				mtk_cam_seninf_start_test_model_for_camsv(sv_dev->seninf,
					sv_dev->slave_sv_dev->stress_test_mode);
			}
		}
	} else {
		if (sv_dev->streaming_tag_cnt == 0)
			goto EXIT;
		if (sv_dev->streaming_tag_cnt == sv_dev->used_tag_cnt) {
			ret |= mtk_cam_sv_cq_disable(sv_dev);
			ret |= mtk_cam_sv_central_common_disable(sv_dev);
			if (atomic_read(&sv_dev->is_slave_on)) {
				mtk_cam_sv_central_common_disable(sv_dev->slave_sv_dev);
				/* todo: call seninf stream off */
				mtk_cam_seninf_stop_test_model_for_camsv(sv_dev->seninf);
			}
		}

		ret |= mtk_cam_sv_fbc_disable(sv_dev, tag_idx);
		sv_dev->streaming_tag_cnt--;
	}

EXIT:
	return ret;
}

int mtk_cam_sv_dev_stream_on(struct mtk_camsv_device *sv_dev, bool on,
	unsigned int enabled_tags, unsigned int used_tag_cnt)
{
	int ret = 0, i;

	if (on) {
		/* keep enabled tag info. for stream off use */
		sv_dev->enabled_tags = enabled_tags;
		sv_dev->used_tag_cnt = used_tag_cnt;
	}

	for (i = SVTAG_START; i < SVTAG_END; i++) {
		if (sv_dev->enabled_tags & (1 << i))
			mtk_cam_sv_dev_pertag_stream_on(sv_dev, i, on);
	}

	if (CAM_DEBUG_ENABLED(RAW_INT))
		dev_info(sv_dev->dev,
			"camsv %d %s en(%d) streaming_tag_cnt:%d\n",
			sv_dev->id, __func__, (on) ? 1 : 0, sv_dev->streaming_tag_cnt);

	return ret;
}

static unsigned int mtk_cam_sv_pdp_powi(unsigned int x, unsigned int n)
{
	unsigned int rst = 1;
	unsigned int m = n;

	while (m--)
		rst *= x;

	return rst;
}

static unsigned int mtk_cam_sv_pdp_xsize_cal(unsigned int length)
{
	return length * 16 / 8;
}

static unsigned int mtk_cam_sv_pdp_dbg_xsize_cal(unsigned int length, unsigned int imgo_fmt)
{
	switch (imgo_fmt) {
	case MTKCAM_IPI_IMG_FMT_BAYER8:
		return length * 8 / 8;
	case MTKCAM_IPI_IMG_FMT_BAYER10:
		return length * 10 / 8;
	case MTKCAM_IPI_IMG_FMT_BAYER12:
		return length * 12 / 8;
	case MTKCAM_IPI_IMG_FMT_BAYER14:
		return length * 14 / 8;
	default:
		break;
	}

	return length * 16 / 8;
}

static unsigned int mtk_cam_sv_pdp_xsize_cal_cpio(unsigned int length)
{
	return (length + 7) / 8;
}

static void mtk_cam_sv_set_pdp_dense_fmt(
	struct mtk_cam_device *cam, unsigned int *tg_width_temp,
	unsigned int *tg_height_temp,
	struct mraw_stats_cfg_param *param, unsigned int dmao_id)
{
	if (dmao_id == imgo_m1) {
		if (param->mbn_pow > 4) {
			dev_info(cam->dev, "%s:Invalid mbn_pow: %d",
				__func__, param->mbn_pow);
			return;
		}
		switch (param->mbn_dir) {
		case MBN_POW_VERTICAL:
			*tg_height_temp /= mtk_cam_sv_pdp_powi(2, param->mbn_pow);
			break;
		case MBN_POW_HORIZONTAL:
			*tg_width_temp /= mtk_cam_sv_pdp_powi(2, param->mbn_pow);
			break;
		default:
			dev_info(cam->dev, "%s:MBN's dir %d %s fail",
				__func__, param->mbn_dir, "unknown idx");
			return;
		}
		// divided for 2 path from MBN
		*tg_width_temp /= 2;
	} else if (dmao_id == cpio_m1) {
		if (param->cpi_pow > 4) {
			dev_info(cam->dev, "Invalid cpi_pow: %d", param->cpi_pow);
			return;
		}
		switch (param->cpi_dir) {
		case CPI_POW_VERTICAL:
			*tg_height_temp /= mtk_cam_sv_pdp_powi(2, param->cpi_pow);
			break;
		case CPI_POW_HORIZONTAL:
			*tg_width_temp /= mtk_cam_sv_pdp_powi(2, param->cpi_pow);
			break;
		default:
			dev_info(cam->dev, "%s:CPI's dir %d %s fail",
				__func__, param->cpi_dir, "unknown idx");
			return;
		}
	}
}

static void mtk_cam_sv_set_pdp_concatenation_fmt(
	struct mtk_cam_device *cam, unsigned int *tg_width_temp,
	unsigned int *tg_height_temp,
	struct mraw_stats_cfg_param *param, unsigned int dmao_id)
{
	if (dmao_id == imgo_m1) {
		if (param->mbn_spar_pow > 4) {
			dev_info(cam->dev, "%s:Invalid mbn_spar_pow: %d",
				__func__, param->mbn_spar_pow);
			return;
		}

		// vertical binning
		*tg_height_temp /= mtk_cam_sv_pdp_powi(2, param->mbn_spar_pow);

		// divided for 2 path from MBN
		*tg_width_temp /= 2;
	} else if (dmao_id == cpio_m1) {
		if (param->cpi_spar_pow < 1 || param->cpi_spar_pow > 6) {
			dev_info(cam->dev, "%s:Invalid cpi_spar_pow: %d",
				__func__, param->cpi_spar_pow);
			return;
		}

		// vertical binning
		*tg_height_temp /= mtk_cam_sv_pdp_powi(2, param->cpi_spar_pow);
	}
}

static void mtk_cam_sv_set_pdp_interleving_fmt(
	unsigned int *tg_width_temp,
	unsigned int *tg_height_temp, unsigned int dmao_id)
{
	if (dmao_id == imgo_m1) {
		// divided for 2 path from MBN
		*tg_height_temp /= 2;
	} else if (dmao_id == cpio_m1) {
		// concatenated
		*tg_width_temp *= 2;
		*tg_height_temp /= 2;
	}
}

void mtk_cam_sv_get_pdp_mqe_size(struct mtk_cam_device *cam, unsigned int pipe_id,
	unsigned int *width, unsigned int *height)
{
	struct mtk_mraw_pipeline *pipe =
		&cam->pipelines.mraw[pipe_id - MTKCAM_SUBDEV_MRAW_START];
	struct mraw_stats_cfg_param *param = &pipe->res_config.stats_cfg_param;

	*width = param->crop_width;
	*height = param->crop_height;

	if (param->lm_en && param->lm_mode_ctrl) {
		*width = param->crop_width * (2 * param->lm_mode_ctrl);
		*height = param->crop_height / (2 * param->lm_mode_ctrl);
	}

	if (param->mqe_en) {
		switch (param->mqe_mode) {
		case UL_MODE:
		case UR_MODE:
		case DL_MODE:
		case DR_MODE:
			*width /= 2;
			*height /= 2;
			break;
		case PD_L_MODE:
		case PD_R_MODE:
		case PD_M_MODE:
		case PD_B01_MODE:
			*width /= 2;
			break;
		case PD_B02_MODE:
			*width /= 2;
			break;
		default:
			dev_info(cam->dev, "%s:MQE-Mode %d %s fail\n",
				__func__, param->mqe_mode, "unknown idx");
			return;
		}
	}
}

void mtk_cam_sv_get_pdp_mbn_size(struct mtk_cam_device *cam, unsigned int pipe_id,
	unsigned int *width, unsigned int *height)
{
	struct mtk_mraw_pipeline *pipe =
		&cam->pipelines.mraw[pipe_id - MTKCAM_SUBDEV_MRAW_START];
	struct mraw_stats_cfg_param *param = &pipe->res_config.stats_cfg_param;

	mtk_cam_sv_get_pdp_mqe_size(cam, pipe_id, width, height);
	switch (param->mbn_dir) {
	case MBN_POW_VERTICAL:
	case MBN_POW_HORIZONTAL:
		mtk_cam_sv_set_pdp_dense_fmt(
			cam, width, height, param, imgo_m1);
		break;
	case MBN_POW_SPARSE_CONCATENATION:
		mtk_cam_sv_set_pdp_concatenation_fmt(
			cam, width, height, param, imgo_m1);
		break;
	case MBN_POW_SPARSE_INTERLEVING:
		mtk_cam_sv_set_pdp_interleving_fmt(width, height, imgo_m1);
		break;
	default:
		dev_info(cam->dev, "%s:MBN's dir %d %s fail", __func__, param->mbn_dir, "unknown idx");
		return;
	}
}

void mtk_cam_sv_get_pdp_cpi_size(struct mtk_cam_device *cam, unsigned int pipe_id,
	unsigned int *width, unsigned int *height)
{
	struct mtk_mraw_pipeline *pipe =
		&cam->pipelines.mraw[pipe_id - MTKCAM_SUBDEV_MRAW_START];
	struct mraw_stats_cfg_param *param = &pipe->res_config.stats_cfg_param;

	mtk_cam_sv_get_pdp_mqe_size(cam, pipe_id, width, height);

	switch (param->cpi_dir) {
	case CPI_POW_VERTICAL:
	case CPI_POW_HORIZONTAL:
		mtk_cam_sv_set_pdp_dense_fmt(
			cam, width, height, param, cpio_m1);
		break;
	case CPI_POW_SPARSE_CONCATENATION:
		mtk_cam_sv_set_pdp_concatenation_fmt(
			cam, width, height, param, cpio_m1);
		break;
	case CPI_POW_SPARSE_INTERLEVING:
		mtk_cam_sv_set_pdp_interleving_fmt(width, height, cpio_m1);
		break;
	default:
		dev_info(cam->dev, "%s:CPI's dir %d %s fail", __func__, param->cpi_dir, "unknown idx");
		return;
	}
}

void mtk_cam_sv_get_pdp_dbg_size(struct mtk_cam_device *cam, unsigned int pipe_id,
	unsigned int *width, unsigned int *height)
{
	struct mtk_mraw_pipeline *pipe =
		&cam->pipelines.mraw[pipe_id - MTKCAM_SUBDEV_MRAW_START];
	struct mraw_stats_cfg_param *param = &pipe->res_config.stats_cfg_param;

	switch (param->img_sel) {
	case 0:  // CRP output
		*width = param->crop_width;
		*height = param->crop_height;
		break;
	case 1:  // MQE output
	case 2:  // PLSC output
	case 3:  // SGG output
		mtk_cam_sv_get_pdp_mqe_size(cam, pipe_id, width, height);
		break;
	default:
		dev_info(cam->dev, "%s:DBG's img_sel %d %s fail",
			__func__, param->img_sel, "unknown idx");
		return;
	}
}

static void mtk_cam_sv_set_pdp_dmao_info(
	struct mtk_cam_device *cam, struct mtk_cam_buffer *buf, unsigned int pipe_id,
	struct dma_info *info, unsigned int imgo_fmt, bool pdp_en)
{
	unsigned int width_mbn = 0, height_mbn = 0, height_dbg = 0, width_dbg = 0;
	unsigned int width_cpi = 0, height_cpi = 0;
	int i;
	struct mtk_mraw_pipeline *pipe =
		&cam->pipelines.mraw[pipe_id - MTKCAM_SUBDEV_MRAW_START];

	struct mraw_stats_cfg_param *param = &pipe->res_config.stats_cfg_param;

	if (!pdp_en) {
		info[imgo_m1].width = param->crop_width;
		info[imgo_m1].height = param->crop_height;
		info[imgo_m1].stride = param->crop_width * 2;
	} else {
		if (param->dbg_en) {
			mtk_cam_sv_get_pdp_dbg_size(cam, pipe_id, &width_dbg, &height_dbg);
			mtk_cam_sv_get_pdp_cpi_size(cam, pipe_id, &width_cpi, &height_cpi);
			mtk_cam_sv_get_pdp_mbn_size(cam, pipe_id, &width_mbn, &height_mbn);
		} else {
			mtk_cam_sv_get_pdp_mbn_size(cam, pipe_id, &width_mbn, &height_mbn);
			mtk_cam_sv_get_pdp_cpi_size(cam, pipe_id, &width_cpi, &height_cpi);
		}

		/* IMGO */
		if (param->dbg_en) {
			info[imgo_m1].width = mtk_cam_sv_pdp_dbg_xsize_cal(width_dbg, imgo_fmt);
			info[imgo_m1].height = height_dbg;
			info[imgo_m1].xsize = mtk_cam_sv_pdp_dbg_xsize_cal(width_dbg, imgo_fmt);
			info[imgo_m1].stride = info[imgo_m1].xsize;
		} else {
			info[imgo_m1].width = mtk_cam_sv_pdp_xsize_cal(width_mbn);
			info[imgo_m1].height = height_mbn;
			info[imgo_m1].xsize = mtk_cam_sv_pdp_xsize_cal(width_mbn);
			info[imgo_m1].stride = info[imgo_m1].xsize;
		}

		/* IMGBO */
		info[imgbo_m1].width = mtk_cam_sv_pdp_xsize_cal(width_mbn);
		info[imgbo_m1].height = height_mbn;
		info[imgbo_m1].xsize = mtk_cam_sv_pdp_xsize_cal(width_mbn);
		info[imgbo_m1].stride = info[imgbo_m1].xsize;

		/* CPIO_1 */
		info[cpio_m1].width = ALIGN(mtk_cam_sv_pdp_xsize_cal_cpio(width_cpi)/2, 16);
		info[cpio_m1].height = height_cpi;
		info[cpio_m1].xsize = ALIGN(mtk_cam_sv_pdp_xsize_cal_cpio(width_cpi)/2, 16);
		info[cpio_m1].stride = info[cpio_m1].xsize;

		/* CPIO_2 */
		info[cpio_m2].width = ALIGN(mtk_cam_sv_pdp_xsize_cal_cpio(width_cpi)/2, 16);
		info[cpio_m2].height = height_cpi;
		info[cpio_m2].xsize = ALIGN(mtk_cam_sv_pdp_xsize_cal_cpio(width_cpi)/2, 16);
		info[cpio_m2].stride = info[cpio_m1].xsize;

	}
	if (atomic_read(&pipe->res_config.is_fmt_change) == 1) {
		for (i = 0; i < pdp_support_dmao_num; i++)
			pipe->res_config.mraw_dma_size[i] = info[i].stride * info[i].height;

		dev_info(cam->dev, "%s imgo_size:%d imgbo_size:%d cpio1_size:%d cpio2_size:%d\n", __func__,
			pipe->res_config.mraw_dma_size[imgo_m1],
			pipe->res_config.mraw_dma_size[imgbo_m1],
			pipe->res_config.mraw_dma_size[cpio_m1],
			pipe->res_config.mraw_dma_size[cpio_m2]);
			atomic_set(&pipe->res_config.is_fmt_change, 0);
	}

	for (i = 0; i < pdp_support_dmao_num; i++) {
		dev_dbg(cam->dev, "pdp_en %d dma_id:%d, w:%d s:%d xsize:%d stride:%d fmt:%d\n",
			pdp_en, i, info[i].width, info[i].height, info[i].xsize, info[i].stride, imgo_fmt);
	}
}

static void mtk_cam_sv_set_pda_frame_param_dmao(
	struct mtk_cam_ctx *ctx,
	struct mtk_cam_job *job,
	struct mtkcam_ipi_frame_param *fp,
	struct dma_info *info, int pipe_id,
	dma_addr_t buf_daddr,
	bool pda_en)
{
	struct mtkcam_ipi_img_output *out;
	struct mtk_camsv_device *sv_dev;
	struct mtk_mraw_pipeline *pipe =
		&ctx->cam->pipelines.mraw[pipe_id - MTKCAM_SUBDEV_MRAW_START];
	int tag_idx;
	unsigned long offset;

	if (!pda_en)
		return;

	offset =
		(((buf_daddr + GET_PLAT_V4L2(meta_pda_ext_size) + 15) >> 4) << 4) -
		buf_daddr;

	sv_dev = dev_get_drvdata(ctx->hw_sv);
	tag_idx = mtk_cam_get_sv_tag_index(job->tag_info, pipe_id);
	if (tag_idx == SVTAG_UNKNOWN) {
		dev_info(ctx->cam->dev, "%s: unknown tag idx", __func__);
		return;
	}
	fp->camsv_param[tag_idx].pda_enable = pda_en;
	fp->camsv_param[tag_idx].pda_idx = pipe->res_config.stats_cfg_param.pda_idx;

	info[pdao_m1].width = pipe->res_config.stats_cfg_param.pda_width;
	info[pdao_m1].height = pipe->res_config.stats_cfg_param.pda_height;
	info[pdao_m1].stride = pipe->res_config.stats_cfg_param.pda_stride;

	tag_idx = mtk_cam_get_sv_tag_index(job->tag_info, pipe_id);
	if (tag_idx == SVTAG_UNKNOWN) {
		dev_info(ctx->cam->dev, "%s: unknown tag idx", __func__);
		return;
	}
	out = &fp->camsv_param[tag_idx].camsv_img_outputs[pdao_m1];
	out->uid.id = MTKCAM_IPI_MRAW_PDA_OUT;
	out->uid.pipe_id = pipe_id;
	out->buf[0][0].iova = buf_daddr + offset;
	out->buf[0][0].size =
		info[pdao_m1].stride *
		info[pdao_m1].height;

	dev_dbg(ctx->cam->dev, "%s:dmao_id:%d iova:0x%llx stride:0x%x height:0x%x\n size:%d offset:%lu",
		__func__, pdao_m1, out->buf[0][0].iova,
		info[pdao_m1].stride , info[pdao_m1].height, out->buf[0][0].size, offset);
}

static void mtk_cam_sv_set_pdp_frame_param_dmao(
	struct mtk_cam_ctx *ctx,
	struct mtk_cam_job *job,
	struct mtkcam_ipi_frame_param *fp,
	struct dma_info *info, int pipe_id,
	dma_addr_t buf_daddr,
	unsigned int imgo_fmt,
	bool pdp_en)
{
	struct mtkcam_ipi_img_output *out;
	struct mtk_camsv_device *sv_dev;
	struct mtk_mraw_pipeline *pipe =
		&ctx->cam->pipelines.mraw[pipe_id - MTKCAM_SUBDEV_MRAW_START];
	unsigned int dmao_num = 0;
	int tag_idx;
	unsigned long offset;
	int i;

	sv_dev = dev_get_drvdata(ctx->hw_sv);
	tag_idx = mtk_cam_get_sv_tag_index(job->tag_info, pipe_id);
	if (tag_idx == SVTAG_UNKNOWN) {
		dev_info(ctx->cam->dev, "%s: unknown tag idx", __func__);
		return;
	}
	fp->camsv_param[tag_idx].dev_id = sv_dev->id + MTKCAM_SUBDEV_CAMSV_START;
	fp->camsv_param[tag_idx].tag_id = tag_idx;
	fp->camsv_param[tag_idx].pdp_enable = pdp_en;


	offset =
		(((buf_daddr + GET_PLAT_V4L2(meta_mraw_ext_size) + 15) >> 4) << 4) -
		buf_daddr;

	dmao_num = pdp_en ? pdp_support_dmao_num : pdp_not_support_dmao_num;

	for (i = 0; i < dmao_num; i++) {
		out = &fp->camsv_param[tag_idx].camsv_img_outputs[i];

		out->uid.id = MTKCAM_IPI_MRAW_META_STATS_0;
		out->uid.pipe_id = pipe_id;

		out->fmt.format = imgo_fmt;
		out->fmt.stride[0] = info[i].stride;
		out->fmt.s.w = info[i].width;
		out->fmt.s.h = info[i].height;

		out->crop.p.x = 0;
		out->crop.p.y = 0;
		out->crop.s.w = info[i].width;
		out->crop.s.h = info[i].height;

		out->buf[0][0].iova = buf_daddr + offset;
		out->buf[0][0].size =
			out->fmt.stride[0] * out->fmt.s.h;

		offset = offset + (((pipe->res_config.mraw_dma_size[i] + 15) >> 4) << 4);


		dev_dbg(ctx->cam->dev, "%s:dmao_id:%d iova:0x%llx stride:0x%x height:0x%x size:%d imgo fmt:%d offset:%lu\n",
			__func__, i, out->buf[0][0].iova,
			out->fmt.stride[0], out->fmt.s.h,
			pipe->res_config.mraw_dma_size[i], imgo_fmt, offset);
	}
}

int mtk_cam_sv_check_pda_status(struct mtk_cam_ctrl *ctrl, struct mtk_cam_job *job)
{
	spin_lock(&ctrl->info_lock);

	if (ctrl->r_info.sv_p1_done_ts_ns > ctrl->r_info.pda_p1_done_ts_ns &&
		(ctrl->r_info.sv_p1_done_ts_ns - ctrl->r_info.pda_p1_done_ts_ns) < FRAME_TIME)
		job->pda_status = true;
	else
		job->pda_status = false;

	spin_unlock(&ctrl->info_lock);

	return 0;
}

void mtk_cam_sv_set_pda_status(void *vaddr, bool pda_support)
{
	if (pda_support)
		CALL_PLAT_V4L2(
			set_pda_status, vaddr, pda_support);
}

static void mtk_cam_sv_set_meta_stats_info(
	void *pdp_header, void *pda_header, struct dma_info *info,
	bool pdp_support, bool pda_support, int tag_idx)
{
	if (pdp_header)
		CALL_PLAT_V4L2(
			set_mraw_meta_stats_info, MTKCAM_IPI_MRAW_META_STATS_0, pdp_header, info,
			pdp_support, pda_support);
	if (pda_header && tag_idx == SVTAG_4)
		CALL_PLAT_V4L2(
			set_mraw_meta_stats_info, MTKCAM_IPI_MRAW_PDA_OUT, pda_header, info,
			pdp_support, pda_support);
}

int mtk_cam_sv_cal_cfg_info(struct mtk_cam_ctx *ctx, struct mtk_cam_buffer *buf,
	struct mtk_cam_job *job, unsigned int pipe_id, struct mtkcam_ipi_frame_param *fp,
	unsigned int imgo_fmt)
{
	struct dma_info info[pda_support_dmao_num];
	struct mtk_mraw_pipeline *pipe =
		&ctx->cam->pipelines.mraw[pipe_id - MTKCAM_SUBDEV_MRAW_START];
	bool pdp_support, pda_support;
	int tag_idx;

	if (ctx->hw_sv == NULL)
		return 0;

	memset(&info, 0, sizeof(info));

	tag_idx = mtk_cam_get_sv_tag_index(job->tag_info, pipe_id);
	if (tag_idx == SVTAG_UNKNOWN) {
		dev_info(ctx->cam->dev, "%s: unknown tag idx", __func__);
		return 0;
	}
	if (job->tag_info[tag_idx].is_pdp_enable)
		pdp_support = true;
	else
		pdp_support = false;

	if (job->tag_info[tag_idx].is_pda_enable)
		pda_support = true;
	else
		pda_support = false;

	mtk_cam_sv_set_pdp_dmao_info(ctx->cam, buf, pipe_id, info, imgo_fmt,
		pdp_support);
	mtk_cam_sv_set_pdp_frame_param_dmao(ctx, job, fp,
		info, pipe_id,
		pipe->res_config.daddr[MTKCAM_IPI_MRAW_META_STATS_0
			- MTKCAM_IPI_MRAW_ID_START], imgo_fmt, pdp_support);
	mtk_cam_sv_set_pda_frame_param_dmao(ctx, job, fp,
		info, pipe_id,
		pipe->res_config.daddr[MTKCAM_IPI_MRAW_PDA_OUT
				- MTKCAM_IPI_MRAW_ID_START], pda_support);
	mtk_cam_sv_set_meta_stats_info(
		pipe->res_config.vaddr[MTKCAM_IPI_MRAW_META_STATS_0
			- MTKCAM_IPI_MRAW_ID_START],
		pipe->res_config.vaddr[MTKCAM_IPI_MRAW_PDA_OUT
			- MTKCAM_IPI_MRAW_ID_START],
		info, pdp_support, pda_support, tag_idx);

	return 0;
}

void mtk_cam_sv_copy_user_input_param(struct mtk_cam_ctx *ctx, struct mtk_cam_job *job,
	void *vaddr, struct mtk_mraw_pipeline *mraw_pipe)
{
	struct mraw_stats_cfg_param *param =
		&mraw_pipe->res_config.stats_cfg_param;
	struct mtk_camsv_device *sv_dev;
	int tag_idx;

	CALL_PLAT_V4L2(
		get_mraw_stats_cfg_param, vaddr, param);

	if (mraw_pipe->res_config.tg_crop.s.w < param->crop_width ||
		mraw_pipe->res_config.tg_crop.s.h < param->crop_height)
		dev_info(ctx->cam->dev, "%s tg size smaller than crop size", __func__);
	dev_dbg(ctx->cam->dev, "%s: pda (idx:%d stride:%d width:%d height:%d) enable:(%d,%d,%d,%d,%d,%d,%d) crop:(%d,%d) mqe:%d mbn:0x%x_%x_%x_%x_%x_%x_%x_%x cpi:0x%x_%x_%x_%x_%x_%x_%x_%x sel:0x%x_%x lm_ctl:%d\n",
		__func__,
		param->pda_idx,
		param->pda_stride,
		param->pda_width,
		param->pda_height,
		param->pda_dc_en,
		param->pdp_en,
		param->mqe_en,
		param->mobc_en,
		param->plsc_en,
		param->lm_en,
		param->dbg_en,
		param->crop_width,
		param->crop_height,
		param->mqe_mode,
		param->mbn_hei,
		param->mbn_pow,
		param->mbn_dir,
		param->mbn_spar_hei,
		param->mbn_spar_pow,
		param->mbn_spar_fac,
		param->mbn_spar_con1,
		param->mbn_spar_con0,
		param->cpi_th,
		param->cpi_pow,
		param->cpi_dir,
		param->cpi_spar_hei,
		param->cpi_spar_pow,
		param->cpi_spar_fac,
		param->cpi_spar_con1,
		param->cpi_spar_con0,
		param->img_sel,
		param->imgo_sel,
		param->lm_mode_ctrl);

	tag_idx = mtk_cam_get_sv_tag_index(job->tag_info, mraw_pipe->id);
	if (tag_idx == SVTAG_UNKNOWN) {
		dev_info(ctx->cam->dev, "%s: unknown tag idx", __func__);
		return;
	}
	sv_dev = dev_get_drvdata(ctx->hw_sv);

	if (param->pdp_en)
		job->tag_info[tag_idx].is_pdp_enable = true;
	else
		job->tag_info[tag_idx].is_pdp_enable = false;

	if (param->pda_dc_en)
		job->tag_info[tag_idx].is_pda_enable = true;
	else
		job->tag_info[tag_idx].is_pda_enable = false;

	if (job->sub_ratio > 1)
		job->tag_info[tag_idx].is_pda_enable = false;

	if ((sv_dev->id == 2 && tag_idx == SVTAG_5) || tag_idx >= SVTAG_6) {
		job->tag_info[tag_idx].is_pdp_enable = false;
		job->tag_info[tag_idx].is_pda_enable = false;
	}

}

void mtk_cam_sv_fifo_dbg_port_config(struct mtk_camsv_device *sv_dev)
{
	sv_dev->fifo_dbg_cnt = 0;
	memset(sv_dev->fifo_dbg, 0, RECORD_FRAME_NUM * sizeof(unsigned int));
	writel_relaxed(0x3, sv_dev->base_dma + REG_CAMSVDMATOP_DBG_CLR);
	writel_relaxed(0x0, sv_dev->base_dma + REG_CAMSVDMATOP_DBG_CLR);
}

void mtk_cam_sv_fifo_dump(struct mtk_camsv_device *sv_dev)
{
	unsigned int dbg_port;
	unsigned int dbg_port1, dbg_port6;
	unsigned int debug_sel = 0;

	for (int dma_core = 0; dma_core < MAX_DMA_CORE; dma_core++) {
		debug_sel = 0;
		debug_sel |= (1 << 7); // real time val
		debug_sel |= (dma_core << 4);
		for (int sel = 7; sel < 9; sel++) {
			debug_sel &= ~(0xf);
			debug_sel |= sel;
			writel_relaxed(debug_sel, sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_SEL);
			dbg_port = readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_PORT);
			dev_info(sv_dev->dev, "[core%d] dbg_sel:0x%x => dbg_port = 0x%x\n",
				dma_core, debug_sel, dbg_port);
		}

		dbg_port1 = readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DBG_PORT1);
		dev_info(sv_dev->dev,
			"[core%d] urg_dur:0x%x\n", dma_core, dbg_port1);

		for (int dbg_type = 0; dbg_type < 2; dbg_type++) {
			debug_sel &= ~(0x40);
			debug_sel |= dbg_type;
			writel_relaxed(debug_sel, sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_SEL);
			dbg_port6 = readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DBG_PORT6);
			dev_info(sv_dev->dev,
				"[core%d] dbg_sel:0x%x => dbg_dspch:0x%x\n",
				dma_core, debug_sel, dbg_port6);
		}

		for (int i = 0; i < RECORD_FRAME_NUM; i++) {
			dev_info(sv_dev->dev,
			"[core%d] max_fifo %x\n", dma_core, sv_dev->fifo_dbg[dma_core][i]);
		}
	}

	for (int dma_core = 0; dma_core < MAX_DMA_CORE; dma_core++) {
		debug_sel = 0;
		debug_sel |= (1 << 7);  // real time val
		debug_sel |= (dma_core << 4);
		writel_relaxed(debug_sel, sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_SEL);
		dev_info(sv_dev->dev,
			"[core%d] max_fifo %x\n", dma_core,
			readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DBG_PORT5));
	}
	dev_info(sv_dev->dev, "img:0x%x_0x%x_0x%x_0x%x img2:0x%x_0x%x_0x%x_0x%x img3:0x%x_0x%x_0x%x_0x%x len:0x%x_0x%x_0x%x_0x%x len2:0x%x_0x%x_0x%x_0x%x len3:0x%x_0x%x_0x%x_0x%x\n",
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_IMG_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_IMG_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_IMG_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_IMG_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_IMG_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_IMG_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_IMG_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_IMG_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_IMG_3),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_IMG_3),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_IMG_3),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_IMG_3),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_LEN_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_LEN_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_LEN_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_LEN_1),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_LEN_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_LEN_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_LEN_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_LEN_2),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON3_LEN_3),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON2_LEN_3),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON1_LEN_3),
		CAMSV_READ_REG(sv_dev->base_dma + REG_CAMSVDMATOP_CON4_LEN_3));
}

void mtk_cam_sv_stg_dump(struct mtk_camsv_device *sv_dev)
{
	unsigned int tag_idx;
	unsigned int stg_trig0, stg_trig1, stg_trig2, stg_trig3;
	unsigned int ctl0, ctl1, ctl2, ctl3, ctl4, ctl5, ctl6, ctl7, ctl8, img_trig;
	unsigned int len0, len1, len2, len3, len4, len5;
	// central
	stg_trig0 = readl_relaxed(sv_dev->base + REG_CAMSVCENTRAL_STG_TRIG0);
	stg_trig1 = readl_relaxed(sv_dev->base + REG_CAMSVCENTRAL_STG_TRIG1);
	stg_trig2 = readl_relaxed(sv_dev->base + REG_CAMSVCENTRAL_STG_TRIG2);
	stg_trig3 = readl_relaxed(sv_dev->base + REG_CAMSVCENTRAL_STG_TRIG3);
	dev_info(sv_dev->dev, "[stg_camsv-%d] trig:%x_%x_%x_%x",
		sv_dev->id,	stg_trig0, stg_trig1, stg_trig2, stg_trig3);

	for (tag_idx = SVTAG_START; tag_idx < SVTAG_END; tag_idx++) {
		if (sv_dev->enabled_tags & (1 << tag_idx)) {
			img_trig = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1A_TRIG +
				CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
			ctl0 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1A_CTL0 +
				CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
			ctl1 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1A_CTL1 +
				CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
			ctl2 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1A_CTL2 +
				CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
			ctl3 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1A_CTL3 +
				CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
			ctl4 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1A_CTL4 +
				CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
			ctl5 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1A_CTL5 +
				CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
			ctl6 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1A_CTL6 +
				CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
			ctl7 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1A_CTL7 +
				CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
			ctl8 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1A_CTL8 +
				CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
			dev_info(sv_dev->dev, "[stg_camsv-%d] tag %d, img_trig:%x, A_ctl:%x_%x_%x_%x_%x_%x_%x_%x_%x",
				sv_dev->id, tag_idx,
				img_trig, ctl0, ctl1, ctl2, ctl3, ctl4, ctl5, ctl6, ctl7, ctl8);
			if (tag_idx < SVTAG_5) {
				ctl0 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1B_CTL0 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl1 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1B_CTL1 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl2 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1B_CTL2 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl3 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1B_CTL3 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl4 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1B_CTL4 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl5 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1B_CTL5 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl6 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1B_CTL6 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl7 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1B_CTL7 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl8 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1B_CTL8 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				dev_info(sv_dev->dev, "[stg_camsv-%d] tag %d, B_ctl:%x_%x_%x_%x_%x_%x_%x_%x_%x",
					sv_dev->id, tag_idx,
					ctl0, ctl1, ctl2, ctl3, ctl4, ctl5, ctl6, ctl7, ctl8);
			}
			if (tag_idx < SVTAG_2) {
				ctl0 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1C_CTL0 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl1 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1C_CTL1 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl2 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1C_CTL2 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl3 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1C_CTL3 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl4 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1C_CTL4 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl5 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1C_CTL5 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl6 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1C_CTL6 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl7 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1C_CTL7 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				ctl8 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_IMG_1C_CTL8 +
					CAMSVSTG_TAG_IMG_SHIFT * tag_idx);
				dev_info(sv_dev->dev, "[stg_camsv-%d] tag %d, C_ctl:%x_%x_%x_%x_%x_%x_%x_%x_%x",
					sv_dev->id, tag_idx,
					ctl0, ctl1, ctl2, ctl3, ctl4, ctl5, ctl6, ctl7, ctl8);
			}
			if (tag_idx < SVTAG_5) {
				len0 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1A_CTL0 +
					stg_tag_len_shift[tag_idx]);
				len1 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1A_CTL1 +
					stg_tag_len_shift[tag_idx]);
				len2 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1A_CTL2 +
					stg_tag_len_shift[tag_idx]);
				len3 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1A_CTL3 +
					stg_tag_len_shift[tag_idx]);
				len4 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1A_CTL4 +
					stg_tag_len_shift[tag_idx]);
				len5 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1A_CTL5 +
					stg_tag_len_shift[tag_idx]);
				dev_info(sv_dev->dev, "[stg_camsv-%d] tag %d, A_len_ctl:%x_%x_%x_%x_%x_%x",
					sv_dev->id, tag_idx,
					len0, len1, len2, len3, len4, len5);
			}
			if (tag_idx < SVTAG_5) {
				len0 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1B_CTL0 +
					CAMSVSTG_TAG_LEN_SHIFT * tag_idx);
				len1 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1B_CTL1 +
					CAMSVSTG_TAG_LEN_SHIFT * tag_idx);
				len2 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1B_CTL2 +
					CAMSVSTG_TAG_LEN_SHIFT * tag_idx);
				len3 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1B_CTL3 +
					CAMSVSTG_TAG_LEN_SHIFT * tag_idx);
				len4 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1B_CTL4 +
					CAMSVSTG_TAG_LEN_SHIFT * tag_idx);
				len5 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1B_CTL5 +
					CAMSVSTG_TAG_LEN_SHIFT * tag_idx);
				dev_info(sv_dev->dev, "[stg_camsv-%d] tag %d, B_len_ctl:%x_%x_%x_%x_%x_%x",
					sv_dev->id, tag_idx,
					len0, len1, len2, len3, len4, len5);
			}
			if (tag_idx < SVTAG_2) {
				len0 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1C_CTL0 +
					CAMSVSTG_TAG_LEN_SHIFT * tag_idx);
				len1 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1C_CTL1 +
					CAMSVSTG_TAG_LEN_SHIFT * tag_idx);
				len2 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1C_CTL2 +
					CAMSVSTG_TAG_LEN_SHIFT * tag_idx);
				len3 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1C_CTL3 +
					CAMSVSTG_TAG_LEN_SHIFT * tag_idx);
				len4 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1C_CTL4 +
					CAMSVSTG_TAG_LEN_SHIFT * tag_idx);
				len5 = readl_relaxed(sv_dev->base_stg + REG_CAMSVSTG_LEN_1C_CTL5 +
					CAMSVSTG_TAG_LEN_SHIFT * tag_idx);
				dev_info(sv_dev->dev, "[stg_camsv-%d] tag %d, C_len_ctl:%x_%x_%x_%x_%x_%x",
					sv_dev->id, tag_idx,
					len0, len1, len2, len3, len4, len5);
			}
		}
	}
}

int mtk_cam_sv_debug_dump(struct mtk_camsv_device *sv_dev, unsigned int dump_tags)
{
	unsigned int i;
	unsigned int tg_sen_mode, tg_vf_con, tg_path_cfg, sof_delay_en;
	unsigned int seq_no_inner, seq_no_outer;
	unsigned int tag_fmt, tag_cfg;
	unsigned int tag_fbc_status, tag_addr, tag_addr_msb;
	unsigned int frm_size, frm_size_r, grab_pix, grab_lin, sof_delay_period;
	unsigned int dcif_set, dcif_sel;
	unsigned int first_tag, last_tag, group_info;
	unsigned int imgo_stride, len_stride, len_addr, len_addr_msb;
	int need_smi_dump = false;

	dump_tags = (dump_tags) ? dump_tags : BIT(CAMSV_MAX_TAGS) - 1;

	/* check tg setting */
	tg_sen_mode = readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_SEN_MODE);
	tg_vf_con = readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_VF_CON);
	tg_path_cfg = readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_PATH_CFG);
	sof_delay_en = readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_SOF_DELAY_EN);
	dev_info(sv_dev->dev,
		"tg_sen_mode:0x%x tg_vf_con:0x%x tg_path_cfg:0x%x sof_delay_en:0x%x\n",
		tg_sen_mode, tg_vf_con, tg_path_cfg, sof_delay_en);

	/* check frame setting and status for each tag */
	for (i = 0; i < CAMSV_MAX_TAGS; i++) {
		if (!(dump_tags & (1 << i)))
			continue;

		writel_relaxed((i << 22) + (i << 27),
			sv_dev->base_inner + REG_CAMSVCENTRAL_TAG_R_SEL);

		seq_no_inner =
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FH_SPARE_TAG_1 +
			CAMSVCENTRAL_FH_SPARE_SHIFT * i);
		seq_no_outer =
		readl_relaxed(sv_dev->base + REG_CAMSVCENTRAL_FH_SPARE_TAG_1 +
			CAMSVCENTRAL_FH_SPARE_SHIFT * i);
		tag_fmt =
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FORMAT_TAG1 +
			CAMSVCENTRAL_FORMAT_TAG_SHIFT * i);
		tag_cfg =
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_CONFIG_TAG1 +
			CAMSVCENTRAL_CONFIG_TAG_SHIFT * i);
		tag_fbc_status =
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FBC1_TAG1 +
			CAMSVCENTRAL_FBC1_TAG_SHIFT * i);
		tag_addr_msb =
			readl_relaxed(sv_dev->base_dma_inner +
			REG_CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG1_A +
			CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG_SHIFT * i);
		tag_addr =
			readl_relaxed(sv_dev->base_dma_inner + REG_CAMSVDMATOP_WDMA_BASE_ADDR_IMG1_A
			+ i * CAMSVDMATOP_WDMA_BASE_ADDR_IMG_SHIFT);
		len_addr =
			readl_relaxed(sv_dev->base_dma_inner +
				REG_CAMSVDMATOP_WDMA_BASE_ADDR_LEN1_A +
				i * CAMSVDMATOP_WDMA_BASE_ADDR_IMG_SHIFT),
		len_addr_msb =
			readl_relaxed(sv_dev->base_dma_inner +
				REG_CAMSVDMATOP_WDMA_BASE_ADDR_MSB_LEN1_A +
				i * CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG_SHIFT),
		frm_size =
			readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FRMSIZE_ST);
		frm_size_r =
			readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FRMSIZE_ST_R);
		grab_pix =
			readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_GRAB_PXL_TAG1 +
			CAMSVCENTRAL_GRAB_PXL_TAG_SHIFT * i);
		grab_lin =
			readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_GRAB_LIN_TAG1 +
			CAMSVCENTRAL_GRAB_LIN_TAG_SHIFT * i);
		sof_delay_period =
			readl_relaxed(sv_dev->base_inner +
				REG_CAMSVCENTRAL_SOF_DELAY_PERIOD_TAG1 +
				CAMSVCENTRAL_SOF_DELAY_PERIOD_TAG_SHIFT * i);
		imgo_stride =
			readl_relaxed(sv_dev->base_dma_inner +
				REG_CAMSVDMATOP_WDMA_BASIC_IMG1_A +
				CAMSVDMATOP_WDMA_BASIC_IMG_SHIFT * i),
		len_stride =
			readl_relaxed(sv_dev->base_dma_inner +
				REG_CAMSVDMATOP_WDMA_BASIC_LEN1_A +
				CAMSVDMATOP_WDMA_BASIC_IMG_SHIFT * i),

		dev_info(sv_dev->dev, "tag_idx:%d seq_no:%d_%d fmt:0x%x cfg:0x%x fbc_status:0x%x imgo_addr:0x%x_%x imgo stride 0x%x len_addr:0x%x_%x len stride 0x%x frm_size:0x%x frm_size_r:0x%x grab_pix:0x%x grab_lin:0x%x sof_delay_period:0x%x",
			i, seq_no_inner, seq_no_outer, tag_fmt, tag_cfg, tag_fbc_status,
			tag_addr_msb, tag_addr, imgo_stride,
			len_addr_msb, len_addr, len_stride,
			frm_size, frm_size_r, grab_pix, grab_lin, sof_delay_period);
	}

	/* check dcif setting */
	dcif_set = readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_DCIF_SET);
	dcif_sel = readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_DCIF_SEL);
	dev_info(sv_dev->dev, "dcif_set:0x%x dcif_sel:0x%x\n",
		dcif_set, dcif_sel);

	/* check tag/group setting */
	first_tag = readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FIRST_TAG);
	last_tag = readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_LAST_TAG);
	for (i = 0; i < MAX_SV_HW_GROUPS; i++) {
		group_info = readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_GROUP_TAG0 +
			CAMSVCENTRAL_GROUP_TAG_SHIFT * i);
		dev_info_ratelimited(sv_dev->dev, "group[%d]:0x%x\n", i, group_info);
	}
	dev_info_ratelimited(sv_dev->dev, "first_tag:0x%x last_tag:0x%x\n",
		first_tag, last_tag);

	mtk_cam_sv_df_debug_dump(&sv_dev->cam->sv_df_mgr, "error");

	if (atomic_read(&sv_dev->is_fifo_full)) {
		need_smi_dump = true;
		atomic_set(&sv_dev->is_fifo_full, 0);
	}

	return need_smi_dump;
}

void camsv_handle_cq_err(
	struct mtk_camsv_device *sv_dev,
	struct mtk_camsys_irq_info *data)
{
	struct mtk_cam_ctx *ctx;
	unsigned int ctx_id;
	int err_status = data->e.err_status2;
	int frame_idx_inner = data->frame_idx_inner;

	ctx_id = ctx_from_fh_cookie(frame_idx_inner);
	ctx = &sv_dev->cam->ctxs[ctx_id];

	/* dump error status */
	dev_info(sv_dev->dev, "cq error_status:0x%x\n", err_status);

	/* dump seninf debug data */
	if (ctx && ctx->seninf)
		//mtk_cam_seninf_dump_current_status(ctx->seninf, false);

	/* dump camsv debug data */
	mtk_cam_sv_debug_dump(sv_dev, 0);

	mtk_smi_dbg_hang_detect("camsys-camsv");
}
void mtk_cam_sv_fifo_full_dbg_dump(struct mtk_camsv_device *sv_dev)
{
	int i;
	unsigned int debug_sel = 0;

	pr_info("%s vcore cg0 cg1:0x%x_0x%x / main cg0 cg1:0x%x_0x%x / mraw cg:0x%x",
		__func__,
		readl(sv_dev->cam->vcore_base),
		readl(sv_dev->cam->vcore_base + 0x10),
		readl(sv_dev->cam->base + 0x00),
		readl(sv_dev->cam->base + 0x4c),
		readl(sv_dev->top));

	for (i = 0; i <= 0x1a; i++) {
		unsigned int base_val = (1 << 16) | (1 << 10);

		base_val |= i;
		writel(base_val, sv_dev->cam->vcore_base + 0x300);
		pr_info("vcore write value:0x300:0x%x 0x304:0x%x",
			base_val, readl(sv_dev->cam->vcore_base + 0x304));
	}

	for (i = 0; i <= 0x1b; i++) {
		unsigned int base_val = (1 << 16) | (1 << 10) | 0x1b;

		base_val |= (i << 5);
		writel(base_val, sv_dev->cam->vcore_base + 0x300);
		pr_info("vcore write value:0x300:0x%x 0x304:0x%x",
			base_val, readl(sv_dev->cam->vcore_base + 0x304));
	}

	for (i = 0x101; i <= 0x10d; i++) {
		writel(i, sv_dev->top + 0x234);
		pr_info("mraw macro dbg 0x234:0x%x 0x238:0x%x", i, readl(sv_dev->top + 0x238));
	}

	for (i = 0; i < 5 ;i++) {
		debug_sel = (1 << 7 | 1 << 16);
		debug_sel++;
		writel_relaxed(debug_sel, sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_SEL);
		dev_info(sv_dev->dev, "dbg_sel:0x%x => dbg_port = 0x%x\n",
				debug_sel, readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_PORT));
		debug_sel++;
		writel_relaxed(debug_sel, sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_SEL);
		dev_info(sv_dev->dev, "dbg_sel:0x%x => dbg_port = 0x%x\n",
				debug_sel, readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_PORT));
		debug_sel++;
		writel_relaxed(debug_sel, sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_SEL);
		dev_info(sv_dev->dev, "dbg_sel:0x%x => dbg_port = 0x%x\n",
				debug_sel, readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_PORT));
	}
	if (sv_dev->id == 0) {
		mtk_smi_dbg_dump_single(true, 14, "camsys");
		mtk_smi_dbg_dump_single(true, 19, "camsys");
		mtk_smi_dbg_dump_single(true, 29, "camsys");
		mtk_smi_dbg_dump_single(false, 29, "camsys");
		mtk_smi_dbg_dump_single(false, 32, "camsys");
		mtk_smi_dbg_dump_single(false, 33, "camsys");
	} else if (sv_dev->id == 1) {
		mtk_smi_dbg_dump_single(true, 13, "camsys");
		mtk_smi_dbg_dump_single(true, 19, "camsys");
		mtk_smi_dbg_dump_single(true, 25, "camsys");
		mtk_smi_dbg_dump_single(false, 30, "camsys");
		mtk_smi_dbg_dump_single(false, 32, "camsys");
		mtk_smi_dbg_dump_single(false, 33, "camsys");
	} else if (sv_dev->id == 2) {
		mtk_smi_dbg_dump_single(true, 13, "camsys");
		mtk_smi_dbg_dump_single(true, 14, "camsys");
		mtk_smi_dbg_dump_single(true, 26, "camsys");
		mtk_smi_dbg_dump_single(false, 29, "camsys");
		mtk_smi_dbg_dump_single(false, 30, "camsys");
	}

}
void camsv_handle_err(
	struct mtk_camsv_device *sv_dev,
	struct mtk_camsys_irq_info *data)
{
	struct mtk_cam_ctx *ctx;
	unsigned int ctx_id;
	int err_status = data->e.err_status;
	int frame_idx_inner = data->frame_idx_inner;

	ctx_id = ctx_from_fh_cookie(frame_idx_inner);
	ctx = &sv_dev->cam->ctxs[ctx_id];

	/* dump error status */
	dev_info(sv_dev->dev, "error_status:0x%x\n", err_status);

	if (sv_dev->id == 3 && atomic_read(&sv_dev->is_sv_stress_test)) {
		/* stress camsv error do nothing*/
		return;
	}

	/* dump seninf debug data */
	if (ctx && ctx->seninf) {
		sv_dev->camsv_error_count += 1;
		if (!ctx->is_seninf_error_trigger) {
			if (sv_dev->camsv_error_count >= 2)
				ctx->is_seninf_error_trigger =
					mtk_cam_seninf_dump_current_status(ctx->seninf, true);
			else
				mtk_cam_seninf_dump_current_status(ctx->seninf, false);
		}
	}

	/* dump camsv debug data */
	mtk_cam_sv_debug_dump(sv_dev, data->err_tags);

	/* check dma fifo status */
	if (!(data->err_tags) && (err_status & CAMSVCENTRAL_DMA_SRAM_FULL_ST)) {
		if (camsv_fifo_detect)
			mtk_cam_sv_execute_fifo_dump(sv_dev, data->ts_ns);

		dev_info(sv_dev->dev, "camsv dma data congestion\n");

		mtk_cam_sv_fifo_full_dbg_dump(sv_dev);
#if !IS_ENABLED(CONFIG_MTK_EMI_LEGACY)
		mtk_emiisu_record_off();
#endif
		mmqos_stop_record();
		mmdvfs_stop_record();
		atomic_set(&sv_dev->is_fifo_full, 1);

		if (DISABLE_RECOVER_FLOW) {
			mtk_cam_isp8s_bwr_dbg_dump(sv_dev->cam->bwr);
			mtk_cam_sv_fifo_dump(sv_dev);
			mtk_cam_sv_stg_dump(sv_dev);

			mmdvfs_debug_status_dump(NULL);
			mmqos_hrt_dump();
			mtk_hrt_issue_flag_set(true);

			if (atomic_read(&sv_dev->is_seamless))
				mtk_cam_ctrl_dump_request(sv_dev->cam, CAMSYS_ENGINE_CAMSV, sv_dev->id,
					frame_idx_inner, MSG_CAMSV_SEAMLESS_ERROR);
			else
				mtk_cam_ctrl_dump_request(sv_dev->cam, CAMSYS_ENGINE_CAMSV, sv_dev->id,
					frame_idx_inner, MSG_CAMSV_ERROR);
		}

		mtk_cam_ctrl_notify_hw_hang(sv_dev->cam,
			CAMSYS_ENGINE_CAMSV, sv_dev->id, frame_idx_inner, 0);
	}

	/* recover incomplete frame */
	if (atomic_read(&sv_dev->is_fifo_full) == 0 && sv_dev->camsv_error_count == 1)
		mtk_cam_ctrl_notify_hw_hang(sv_dev->cam,
			CAMSYS_ENGINE_CAMSV, sv_dev->id, frame_idx_inner, 1);
}

bool is_all_tag_setting_to_inner(struct mtk_camsv_device *sv_dev,
	unsigned int frm_seq_no_inner)
{
	unsigned int i, j, grp_seq_no_inner;

	for (i = 0; i < MAX_SV_HW_GROUPS; i++) {
		if (!sv_dev->active_group_info[i])
			continue;
		for (j = 0; j < CAMSV_MAX_TAGS; j++) {
			if (sv_dev->active_group_info[i] & BIT(j)) {
				grp_seq_no_inner =
					readl_relaxed(sv_dev->base_inner +
						REG_CAMSVCENTRAL_FH_SPARE_TAG_1 +
						CAMSVCENTRAL_FH_SPARE_SHIFT * j);
				if (grp_seq_no_inner != frm_seq_no_inner)
					return false;
				break;
			}
		}
	}

	return true;
}

static irqreturn_t mtk_irq_camsv_hybrid(int irq, void *data)
{
	struct mtk_camsv_device *sv_dev = (struct mtk_camsv_device *)data;
	struct mtk_camsys_irq_info irq_info;
	unsigned int frm_seq_no, frm_seq_no_inner;
	unsigned int i, err_status, done_status, cq_status;
	unsigned int first_tag, addr_frm_seq_no = REG_CAMSVCENTRAL_FH_SPARE_TAG_1;
	bool wake_thread = false;

	memset(&irq_info, 0, sizeof(irq_info));

	first_tag =
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FIRST_TAG);
	for (i = 0; i < CAMSV_MAX_TAGS; i++) {
		if (first_tag & (1 << i)) {
			addr_frm_seq_no = REG_CAMSVCENTRAL_FH_SPARE_TAG_1 +
				CAMSVCENTRAL_FH_SPARE_SHIFT * i;
			break;
		}
	}

	frm_seq_no =
		readl_relaxed(sv_dev->base + addr_frm_seq_no);
	frm_seq_no_inner =
		readl_relaxed(sv_dev->base_inner + addr_frm_seq_no);
	err_status =
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_ERR_STATUS);
	done_status	=
		readl_relaxed(sv_dev->base + REG_CAMSVCENTRAL_DONE_STATUS);
	cq_status =
		readl_relaxed(sv_dev->base_scq + REG_CAMSVCQTOP_INT_0_STATUS);

	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.frame_idx = frm_seq_no;
	irq_info.frame_idx_inner = frm_seq_no_inner;

	if (err_status) {
		dev_dbg(sv_dev->dev, "camsv-%d: error status:0x%x seq_no:%d_%d",
			sv_dev->id, err_status,
			frm_seq_no_inner, frm_seq_no);

		irq_info.irq_type |= (1 << CAMSYS_IRQ_ERROR);
		irq_info.e.err_status = err_status;

		if (err_status & ERR_ST_MASK_TAG1_ERR)
			irq_info.err_tags |= (1 << SVTAG_0);
		if (err_status & ERR_ST_MASK_TAG2_ERR)
			irq_info.err_tags |= (1 << SVTAG_1);
		if (err_status & ERR_ST_MASK_TAG3_ERR)
			irq_info.err_tags |= (1 << SVTAG_2);
		if (err_status & ERR_ST_MASK_TAG4_ERR)
			irq_info.err_tags |= (1 << SVTAG_3);
		if (err_status & ERR_ST_MASK_TAG5_ERR)
			irq_info.err_tags |= (1 << SVTAG_4);
		if (err_status & ERR_ST_MASK_TAG6_ERR)
			irq_info.err_tags |= (1 << SVTAG_5);
		if (err_status & ERR_ST_MASK_TAG7_ERR)
			irq_info.err_tags |= (1 << SVTAG_6);
		if (err_status & ERR_ST_MASK_TAG8_ERR)
			irq_info.err_tags |= (1 << SVTAG_7);

		trace_camsv_irq_err(sv_dev->dev, frm_seq_no_inner, frm_seq_no,
					err_status);
	}

	if (done_status) {
		dev_dbg(sv_dev->dev, "camsv-%d: done status:0x%x seq_no:0x%x_0x%x",
			sv_dev->id, done_status, frm_seq_no_inner, frm_seq_no);
		irq_info.ts_ns = ktime_get_boottime_ns();
		sv_dev->camsv_error_count = 0;
		irq_info.irq_type |= (1 << CAMSYS_IRQ_FRAME_DONE);
		if (done_status & CAMSVCENTRAL_SW_GP_PASS1_DONE_0_ST)
			irq_info.done_tags |= sv_dev->active_group_info[0];
		if (done_status & CAMSVCENTRAL_SW_GP_PASS1_DONE_1_ST)
			irq_info.done_tags |= sv_dev->active_group_info[1];
		if (done_status & CAMSVCENTRAL_SW_GP_PASS1_DONE_2_ST)
			irq_info.done_tags |= sv_dev->active_group_info[2];
		if (done_status & CAMSVCENTRAL_SW_GP_PASS1_DONE_3_ST)
			irq_info.done_tags |= sv_dev->active_group_info[3];
		trace_camsv_irq_done(sv_dev->dev, frm_seq_no_inner, frm_seq_no,
					 done_status);
	}

	if (cq_status) {
		if (cq_status & ERR_ST_MASK_CQ_ERR) {
			dev_dbg(sv_dev->dev, "camsv-%d: cq error status:0x%x seq_no:%d_%d",
				sv_dev->id, cq_status,
				frm_seq_no_inner, frm_seq_no);

			irq_info.irq_type |= (1 << CAMSYS_IRQ_ERROR);
			irq_info.e.err_status2 = cq_status & ERR_ST_MASK_CQ_ERR;
		}

		if (cq_status & CAMSVCQTOP_SCQ_SUB_THR_DONE) {
			dev_dbg(sv_dev->dev, "camsv-%d: cq done status:0x%x seq_no:%d_%d",
				sv_dev->id, cq_status,
			frm_seq_no_inner, frm_seq_no);
			if (sv_dev->cq_ref != NULL) {
				long mask = bit_map_bit(MAP_HW_CAMSV, sv_dev->id);

				if (engine_handle_cq_done(&sv_dev->cq_ref, mask))
					irq_info.irq_type |= (1 << CAMSYS_IRQ_SETTING_DONE);
			}
		}

		trace_camsv_irq_cq_done(sv_dev->dev, frm_seq_no_inner, frm_seq_no,
					cq_status);
	}

	if (irq_info.irq_type && push_msgfifo(sv_dev, &irq_info) == 0)
		wake_thread = true;

	return wake_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}

static irqreturn_t mtk_irq_camsv_sof(int irq, void *data)
{
	struct mtk_camsv_device *sv_dev = (struct mtk_camsv_device *)data;
	struct mtk_camsys_irq_info irq_info;
	unsigned int frm_seq_no, frm_seq_no_inner;
	unsigned int tg_cnt, enable_tags = 0;
	unsigned int irq_sof_status, irq_channel_status, i, m;
	unsigned int addr_frm_seq_no = REG_CAMSVCENTRAL_FH_SPARE_TAG_1;
	bool wake_thread = false;
	unsigned int debug_sel = 0;

	memset(&irq_info, 0, sizeof(irq_info));

	sv_dev->first_tag =
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FIRST_TAG);
	sv_dev->last_tag =
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_LAST_TAG);
	for (i = 0; i < CAMSV_MAX_TAGS; i++) {
		if (sv_dev->first_tag & (1 << i)) {
			addr_frm_seq_no = REG_CAMSVCENTRAL_FH_SPARE_TAG_1 +
				CAMSVCENTRAL_FH_SPARE_SHIFT * i;
			break;
		}
	}

	frm_seq_no =
		readl_relaxed(sv_dev->base + addr_frm_seq_no);
	frm_seq_no_inner =
		readl_relaxed(sv_dev->base_inner + addr_frm_seq_no);
	irq_sof_status =
		readl_relaxed(sv_dev->base + REG_CAMSVCENTRAL_SOF_STATUS);
	irq_channel_status =
		readl_relaxed(sv_dev->base + REG_CAMSVCENTRAL_CHANNEL_STATUS);

	for (int dma_core = 0; dma_core < MAX_DMA_CORE; dma_core++) {
		debug_sel = 0;
		debug_sel |= (1 << 7);  // real time val
		debug_sel |= (dma_core << 4);
		writel_relaxed(debug_sel, sv_dev->base_dma + REG_CAMSVDMATOP_DMA_DEBUG_SEL);
		sv_dev->fifo_dbg[dma_core][(sv_dev->fifo_dbg_cnt % RECORD_FRAME_NUM)] =
			readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DBG_PORT5);
	}
	sv_dev->fifo_dbg_cnt++;

	for (i = 0; i < MAX_SV_HW_GROUPS; i++) {
		sv_dev->active_group_info[i] =
			readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_GROUP_TAG0 +
				CAMSVCENTRAL_GROUP_TAG_SHIFT * i);
		enable_tags |= sv_dev->active_group_info[i];
	}

	tg_cnt =
		readl_relaxed(sv_dev->base + REG_CAMSVCENTRAL_VF_ST_TAG1 +
				CAMSVCENTRAL_VF_ST_TAG_SHIFT * 3);
	tg_cnt = (sv_dev->tg_cnt & 0xffffff00) + ((tg_cnt & 0xff000000) >> 24);

	if (CAM_DEBUG_ENABLED(RAW_INT))
		dev_info(sv_dev->dev, "camsv-%d: sof status:0x%x channel status:0x%x seq_no:0x%x_0x%x group_tags:0x%x_%x_%x_%x first_tag:0x%x last_tag:0x%x tg_cnt:%d/%lld dcif sel/set:0x%x_%x pda_dcif0x%x",
		sv_dev->id, irq_sof_status,
		irq_channel_status,
		frm_seq_no_inner, frm_seq_no,
		sv_dev->active_group_info[0],
		sv_dev->active_group_info[1],
		sv_dev->active_group_info[2],
		sv_dev->active_group_info[3],
		sv_dev->first_tag,
		sv_dev->last_tag,
		tg_cnt, sv_dev->sof_count,
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_DCIF_SEL),
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_DCIF_SET),
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_DCIF_SET));

	irq_info.ts_ns = ktime_get_boottime_ns();
	irq_info.frame_idx = frm_seq_no;
	irq_info.frame_idx_inner = frm_seq_no_inner;
	irq_info.tg_cnt = sv_dev->tg_cnt;
	sv_dev->sof_count++;
	/* sof */
	for (i = SVTAG_START; i < SVTAG_END; i++) {
		m = i * CAMSVCENTRAL_SOF_BIT_OFFSET + CAMSVCENTRAL_SOF_BIT_START;
		if (irq_sof_status & BIT(m))
			irq_info.sof_tags |= (1 << i);
	}
	if (irq_info.sof_tags)
		irq_info.irq_type |= (1 << CAMSYS_IRQ_FRAME_START);
	irq_info.fbc_empty = (irq_info.sof_tags & sv_dev->last_tag) ?
		mtk_cam_sv_is_zero_fbc_cnt(sv_dev, enable_tags) : 0;

	/* sw enq error */
	for (i = SVTAG_START; i < SVTAG_END; i++) {
		m = i * CAMSVCENTRAL_SW_ENQ_ERR_BIT_OFFSET +
			CAMSVCENTRAL_SW_ENQ_ERR_BIT_START;
		if (irq_channel_status & BIT(m))
			irq_info.done_tags |= (1 << i);
	}
	if (irq_info.done_tags)
		irq_info.irq_type |= (1 << CAMSYS_IRQ_FRAME_DROP);

	/* dma done */
	for (i = SVTAG_START; i < SVTAG_END; i++) {
		m = i * CAMSVCENTRAL_DMA_DONE_BIT_OFFSET +
			CAMSVCENTRAL_DMA_DONE_BIT_START;
		if (irq_channel_status & BIT(m))
			irq_info.dma_done_tags |= (1 << i);
	}
	if (irq_info.dma_done_tags)
		irq_info.irq_type |= (1 << CAMSYS_IRQ_SV_DMAO_DONE);

	if (tg_cnt < sv_dev->tg_cnt)
		sv_dev->tg_cnt = tg_cnt + BIT(8);
	else
		sv_dev->tg_cnt = tg_cnt;
	if (CAM_DEBUG_ENABLED(EXTISP_SW_CNT))
		irq_info.tg_cnt = sv_dev->sof_count - 1;

	if ((irq_info.sof_tags & sv_dev->last_tag) &&
		is_all_tag_setting_to_inner(sv_dev, frm_seq_no_inner)) {

		engine_handle_sof(&sv_dev->cq_ref,
				  bit_map_bit(MAP_HW_CAMSV, sv_dev->id),
				  irq_info.frame_idx_inner);
	}
	irq_info.ts_before_push_msgfifo_sof = ktime_get_boottime_ns();
	if (irq_info.irq_type && push_msgfifo(sv_dev, &irq_info) == 0)
		wake_thread = true;

	trace_camsv_irq_sof(sv_dev->dev,
			    frm_seq_no_inner, frm_seq_no,
			    irq_sof_status,
			    irq_channel_status,
			    sv_dev->active_group_info,
			    sv_dev->first_tag,
			    sv_dev->last_tag,
			    tg_cnt);

	return wake_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}

static irqreturn_t mtk_irq_camsv_debug(int irq, void *data)
{
	struct mtk_camsv_device *sv_dev = (struct mtk_camsv_device *)data;
	struct mtk_camsys_irq_info irq_info;
	unsigned int frm_seq_no, frm_seq_no_inner;
	unsigned int i, j, first_tag;
	unsigned int common_status, top_status, fifo_status, channel_status;
	unsigned int exp_0_bid = 0, exp_1_bid = 0;
	unsigned int addr_frm_seq_no = REG_CAMSVCENTRAL_FH_SPARE_TAG_1;
	bool wake_thread = false;

	memset(&irq_info, 0, sizeof(irq_info));
	irq_info.ts_ns = ktime_get_boottime_ns();

	first_tag =
		readl_relaxed(sv_dev->base_inner + REG_CAMSVCENTRAL_FIRST_TAG);
	for (i = 0; i < CAMSV_MAX_TAGS; i++) {
		if (first_tag & (1 << i)) {
			addr_frm_seq_no = REG_CAMSVCENTRAL_FH_SPARE_TAG_1 +
				CAMSVCENTRAL_FH_SPARE_SHIFT * i;
			break;
		}
	}

	frm_seq_no =
		readl_relaxed(sv_dev->base + addr_frm_seq_no);
	frm_seq_no_inner =
		readl_relaxed(sv_dev->base_inner + addr_frm_seq_no);

	irq_info.frame_idx = frm_seq_no;
	irq_info.frame_idx_inner = frm_seq_no_inner;

	common_status =
		readl_relaxed(sv_dev->base + REG_CAMSVCENTRAL_COMMON_STATUS);
	top_status =
		readl_relaxed(sv_dev->base + REG_CAMSVCENTRAL_TOP_STATUS);
	fifo_status =
		readl_relaxed(sv_dev->base_dma + REG_CAMSVDMATOP_DMA_INT_FIFO_STAT);

	if (sv_dev->is_buf_early_return)
		channel_status = readl_relaxed(sv_dev->base + REG_CAMSVCENTRAL_CHANNEL_STATUS);
	else
		channel_status = 0;

	if (CAM_DEBUG_ENABLED(RAW_INT))
		dev_info(sv_dev->dev, "camsv-%d: common_status:0x%x, channel_status:0x%x, fifo_status:0x%x, frm_seq_no:0x%x/0x%x, ts:%llu\n",
			sv_dev->id, common_status, channel_status,
			fifo_status, frm_seq_no, frm_seq_no_inner,
			irq_info.ts_ns);

	if (first_tag) {
		exp_1_bid = CAMSVCENTRAL_DBG_INT_BIT_START +
			(CAMSVCENTRAL_DBG_INT_BIT_OFFSET * (ffs(first_tag) - 1));
		exp_0_bid = exp_1_bid + 1;

		/* exp0 */
		if (common_status & BIT(exp_0_bid) &&
				!sv_dev->is_skip_raw_unlock_done)
			irq_info.irq_type |= (1 << CAMSYS_IRQ_TUNING_UPDATE);

		/* exp1 */
		if (common_status & BIT(exp_1_bid)) {
			if (sv_dev->is_skip_raw_unlock_done)
				sv_dev->is_skip_raw_unlock_done = false;
			else if (sv_dev->ois_updated_seq < frm_seq_no_inner) {
				writel_relaxed(0, sv_dev->raw_lock_done_sel);
				sv_dev->ois_updated_seq = frm_seq_no_inner;
			}
		}
	}

	if (top_status) {
		irq_info.irq_type |= (1 << CAMSYS_IRQ_DF);
		irq_info.n.status = top_status;
	}

	if (channel_status) {
		for (i = SVTAG_START; i < SVTAG_END; i++) {
			j = i * CAMSVCENTRAL_DMA_DONE_BIT_OFFSET +
				CAMSVCENTRAL_DMA_DONE_BIT_START;
			if (channel_status & BIT(j))
				irq_info.dma_done_tags |= (1 << i);
		}
		if (irq_info.dma_done_tags)
			irq_info.irq_type |= (1 << CAMSYS_IRQ_SV_DMAO_DONE);
	}

	if (irq_info.irq_type && push_msgfifo(sv_dev, &irq_info) == 0)
		wake_thread = true;

	return wake_thread ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}

static irqreturn_t mtk_thread_irq_camsv(int irq, void *data)
{
	struct mtk_camsv_device *sv_dev = (struct mtk_camsv_device *)data;
	struct mtk_camsys_irq_info irq_info;
	int recovered_done;
	int do_recover;
	u64 ts_before_msgfifo;
	u64 ts_after_msgfifo;

	if (unlikely(atomic_cmpxchg(&sv_dev->is_fifo_overflow, 1, 0)))
		dev_info(sv_dev->dev, "msg fifo overflow\n");

	ts_before_msgfifo = ktime_get_boottime_ns();
	while (pop_msgfifo(sv_dev, &irq_info)) {
		ts_after_msgfifo = ktime_get_boottime_ns();
		if (CAM_DEBUG_ENABLED(CTRL))
			dev_info(sv_dev->dev, "ts=%llu irq_type %d, req:0x%x/0x%x, tg_cnt:%d\n",
			irq_info.ts_ns / 1000,
			irq_info.irq_type,
			irq_info.frame_idx_inner,
			irq_info.frame_idx,
			irq_info.tg_cnt);

		/* debug dump once */
		if (camsv_debug_dump_once &&
			(irq_info.irq_type & (1 << CAMSYS_IRQ_FRAME_START))) {
			camsv_debug_dump_once = 0;
			mtk_cam_sv_debug_dump(sv_dev, 0);
		}

		/* error case */
		if (unlikely(irq_info.irq_type & (1 << CAMSYS_IRQ_ERROR)) &&
			irq_info.e.err_status != 0) {
			camsv_handle_err(sv_dev, &irq_info);
		}

		/* error cq case */
		if (unlikely(irq_info.irq_type & (1 << CAMSYS_IRQ_ERROR)) &&
			irq_info.e.err_status2 != 0) {
			camsv_handle_cq_err(sv_dev, &irq_info);
		}

		if ((irq_info.irq_type & (1 << CAMSYS_IRQ_DF)) &&
			(irq_info.n.status != 0)) {
			mtk_cam_sv_run_df_action_ack(sv_dev, irq_info.n.status);
		}

		if (sv_dev->id == 3 && atomic_read(&sv_dev->is_sv_stress_test))
			continue;

		/* normal case */
		do_recover = sv_process_fsm(sv_dev, &irq_info,
					    &recovered_done);
		if (irq_info.irq_type & (1 << CAMSYS_IRQ_FRAME_START)) {
			irq_info.ts_before_pop_msgfifo_sof = ts_before_msgfifo;
			irq_info.ts_after_pop_msgfifo_sof = ts_after_msgfifo;
		}
		/* inform interrupt information to camsys controller */
		mtk_cam_ctrl_isr_event(sv_dev->cam,
				       CAMSYS_ENGINE_CAMSV, sv_dev->id,
				       &irq_info);

		if (do_recover) {
			irq_info.irq_type = BIT(CAMSYS_IRQ_FRAME_DONE);
			irq_info.cookie_done = recovered_done;

			mtk_cam_ctrl_isr_event(sv_dev->cam,
					       CAMSYS_ENGINE_CAMSV, sv_dev->id,
					       &irq_info);
		}
	}

	return IRQ_HANDLED;
}

static int mtk_camsv_pm_suspend(struct device *dev)
{
	int ret;

	dev_info_ratelimited(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Force ISP HW to idle */
	ret = pm_runtime_force_suspend(dev);
	return ret;
}

static int mtk_camsv_pm_resume(struct device *dev)
{
	int ret;

	dev_info_ratelimited(dev, "- %s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	/* Force ISP HW to resume */
	ret = pm_runtime_force_resume(dev);
	if (ret)
		return ret;

	return 0;
}

static int mtk_camsv_suspend_pm_event(struct notifier_block *notifier,
			unsigned long pm_event, void *unused)
{
	struct mtk_camsv_device *sv_dev =
		container_of(notifier, struct mtk_camsv_device, notifier_blk);
	struct device *dev = sv_dev->dev;

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		return NOTIFY_DONE;
	case PM_RESTORE_PREPARE:
		return NOTIFY_DONE;
	case PM_POST_HIBERNATION:
		return NOTIFY_DONE;
	case PM_SUSPEND_PREPARE: /* before enter suspend */
		mtk_camsv_pm_suspend(dev);
		return NOTIFY_DONE;
	case PM_POST_SUSPEND: /* after resume */
		mtk_camsv_pm_resume(dev);
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static int mtk_camsv_of_probe(struct platform_device *pdev,
			    struct mtk_camsv_device *sv_dev)
{
	struct device *dev = &pdev->dev;
	struct platform_device *larb_pdev;
	struct device_node *larb_node;
	struct device_node *iommu_node;
	struct of_phandle_args args;
	struct device_link *link;
	struct resource *res;
	unsigned int i, j;
	int ret, num_clks, num_iommus, num_ports, smmus;
	unsigned int raw_lock_sel_addr = 0;

	spin_lock_init(&sv_dev->msg_lock);

	ret = of_property_read_u32(dev->of_node, "mediatek,camsv-id",
						       &sv_dev->id);
	if (ret) {
		dev_dbg(dev, "missing camid property\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "mediatek,cammux-id",
						       &sv_dev->cammux_id);
	if (ret) {
		dev_dbg(dev, "missing cammux id property\n");
		return ret;
	}

	/* base outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv_dev->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv_dev->base)) {
		dev_dbg(dev, "failed to map register base\n");
		return PTR_ERR(sv_dev->base);
	}
	dev_dbg(dev, "camsv, map_addr=0x%pK\n", sv_dev->base);

	/* base dma outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base_DMA");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv_dev->base_dma = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv_dev->base_dma)) {
		dev_dbg(dev, "failed to map register base dma\n");
		return PTR_ERR(sv_dev->base_dma);
	}
	dev_dbg(dev, "camsv, map_dma_addr=0x%pK\n", sv_dev->base_dma);

	/* base scq outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base_SCQ");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv_dev->base_scq = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv_dev->base_scq)) {
		dev_dbg(dev, "failed to map register base scq\n");
		return PTR_ERR(sv_dev->base_scq);
	}
	dev_dbg(dev, "camsv, map_scq_addr=0x%pK\n", sv_dev->base_scq);

	/* base pdp outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base_pdp");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv_dev->base_pdp = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv_dev->base_pdp)) {
		dev_dbg(dev, "failed to map register base pdp\n");
		return PTR_ERR(sv_dev->base_pdp);
	}
	dev_dbg(dev, "camsv, map_pdp_addr=0x%pK\n", sv_dev->base_pdp);

	/* base stg outer register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base_stg");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv_dev->base_stg = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv_dev->base_stg)) {
		dev_dbg(dev, "failed to map register base stg\n");
		return PTR_ERR(sv_dev->base_stg);
	}
	dev_dbg(dev, "camsv, map_stg_addr=0x%pK\n", sv_dev->base_stg);

	/* base inner register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inner_base");
	if (!res) {
		dev_dbg(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv_dev->base_inner = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv_dev->base_inner)) {
		dev_dbg(dev, "failed to map register inner base\n");
		return PTR_ERR(sv_dev->base_inner);
	}

	dev_dbg(dev, "camsv, map_addr=0x%pK\n", sv_dev->base_inner);

	/* base inner dma register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inner_base_DMA");
	if (!res) {
		dev_dbg(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv_dev->base_dma_inner = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv_dev->base_dma_inner)) {
		dev_dbg(dev, "failed to map register inner base dma\n");
		return PTR_ERR(sv_dev->base_dma_inner);
	}
	dev_dbg(dev, "camsv, map_addr(inner dma)=0x%pK\n", sv_dev->base_dma_inner);

	/* base inner scq register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inner_base_SCQ");
	if (!res) {
		dev_dbg(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv_dev->base_scq_inner = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv_dev->base_scq_inner)) {
		dev_dbg(dev, "failed to map register inner base\n");
		return PTR_ERR(sv_dev->base_scq_inner);
	}
	dev_dbg(dev, "camsv, map_addr(inner scq)=0x%pK\n", sv_dev->base_scq_inner);


	/* base inner pdp register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inner_base_pdp");
	if (!res) {
		dev_dbg(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv_dev->base_pdp_inner = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv_dev->base_pdp_inner)) {
		dev_dbg(dev, "failed to map register inner base pdp\n");
		return PTR_ERR(sv_dev->base_scq_inner);
	}
	dev_dbg(dev, "camsv, map_addr(inner pdp)=0x%pK\n", sv_dev->base_pdp_inner);

	/* base inner stg register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "inner_base_stg");
	if (!res) {
		dev_dbg(dev, "failed to get mem\n");
		return -ENODEV;
	}

	sv_dev->base_stg_inner = devm_ioremap_resource(dev, res);
	if (IS_ERR(sv_dev->base_stg_inner)) {
		dev_dbg(dev, "failed to map register inner base stg\n");
		return PTR_ERR(sv_dev->base_scq_inner);
	}
	dev_dbg(dev, "camsv, map_addr(inner stg)=0x%pK\n", sv_dev->base_stg_inner);

	sv_dev->top = ioremap(REG_CAMSYS_MRAW, 0x1000);

	for (i = 0; i < CAMSV_IRQ_NUM; i++) {
		sv_dev->irq[i] = platform_get_irq(pdev, i);
		if (!sv_dev->irq[i]) {
			dev_dbg(dev, "failed to get irq\n");
			return -ENODEV;
		}
	}

	/* ois compensation */
	if (sv_dev->id < GET_PLAT_V4L2(raw_pipeline_num)) {
		CALL_PLAT_V4L2(
			get_raw_lock_sel_addr, sv_dev->id, &raw_lock_sel_addr);

		sv_dev->is_skip_raw_unlock_done = false;
		if (raw_lock_sel_addr)
			sv_dev->raw_lock_done_sel =
				ioremap(raw_lock_sel_addr, 0x4);
		else
			sv_dev->raw_lock_done_sel = NULL;

	} else {
		sv_dev->is_skip_raw_unlock_done = false;
		sv_dev->raw_lock_done_sel = NULL;
	}

	for (i = 0; i < CAMSV_IRQ_NUM; i++) {
		if (i == 0)
			ret = devm_request_threaded_irq(dev, sv_dev->irq[i],
						mtk_irq_camsv_hybrid,
						mtk_thread_irq_camsv,
						0, dev_name(dev), sv_dev);
		else if (i == 1)
			ret = devm_request_threaded_irq(dev, sv_dev->irq[i],
						mtk_irq_camsv_sof,
						mtk_thread_irq_camsv,
						0, dev_name(dev), sv_dev);
		else
			ret = devm_request_threaded_irq(dev, sv_dev->irq[i],
						mtk_irq_camsv_debug,
						mtk_thread_irq_camsv,
						0, dev_name(dev), sv_dev);
		if (ret) {
			dev_dbg(dev, "failed to request irq=%d\n", sv_dev->irq[i]);
			return ret;
		}

		dev_info(dev, "registered irq=%d\n", sv_dev->irq[i]);

		disable_irq(sv_dev->irq[i]);

		dev_info(dev, "%s:disable irq %d\n", __func__, sv_dev->irq[i]);
	}

	num_clks = of_count_phandle_with_args(pdev->dev.of_node, "clocks",
			"#clock-cells");

	sv_dev->num_clks = (num_clks < 0) ? 0 : num_clks;
	dev_info(dev, "clk_num:%d\n", sv_dev->num_clks);

	if (sv_dev->num_clks) {
		sv_dev->clks = devm_kcalloc(dev, sv_dev->num_clks, sizeof(*sv_dev->clks),
					 GFP_KERNEL);
		if (!sv_dev->clks)
			return -ENOMEM;
	}

	for (i = 0; i < sv_dev->num_clks; i++) {
		sv_dev->clks[i] = of_clk_get(pdev->dev.of_node, i);
		if (IS_ERR(sv_dev->clks[i])) {
			dev_info(dev, "failed to get clk %d\n", i);
			return -ENODEV;
		}
	}

	sv_dev->num_larbs = of_count_phandle_with_args(
					pdev->dev.of_node, "mediatek,larbs", NULL);
	sv_dev->num_larbs = (sv_dev->num_larbs <= 0) ? 0 : sv_dev->num_larbs;
	dev_info(dev, "sv_dev larb_num:%d\n", sv_dev->num_larbs);

	if (sv_dev->num_larbs) {
		sv_dev->larb_pdev = devm_kcalloc(dev,
					     sv_dev->num_larbs, sizeof(*sv_dev->larb_pdev),
					     GFP_KERNEL);
		if (!sv_dev->larb_pdev)
			return -ENOMEM;
	}

	for (i = 0; i < sv_dev->num_larbs; i++) {
		larb_node = of_parse_phandle(
					pdev->dev.of_node, "mediatek,larbs", i);
		if (!larb_node) {
			dev_info(dev, "failed to get larb node\n");
			continue;
		}

		larb_pdev = of_find_device_by_node(larb_node);
		if (WARN_ON(!larb_pdev)) {
			of_node_put(larb_node);
			dev_info(dev, "failed to get larb pdev\n");
			continue;
		}
		of_node_put(larb_node);

		link = device_link_add(&pdev->dev, &larb_pdev->dev,
						DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
		if (!link)
			dev_info(dev, "unable to link smi larb%d\n", i);
		else {
			sv_dev->larb_pdev[i] = larb_pdev;
			dev_info(dev, "link smi larb%d\n", i);
		}
	}

	num_iommus = of_property_count_strings(
					pdev->dev.of_node, "mediatek,larb-node-names");
	num_iommus = (num_iommus < 0) ? 0 : num_iommus;
	dev_info(dev, "iommu_num:%d\n", num_iommus);

	for (i = 0; i < num_iommus; i++) {
		const char *node_name;

		ret = of_property_read_string_index(
					pdev->dev.of_node, "mediatek,larb-node-names",
					i, &node_name);
		if (ret) {
			dev_info(dev, "failed to read larb node name(%d)\n", i);
			continue;
		}
		iommu_node = of_find_node_by_name(NULL, node_name);
		if (!iommu_node) {
			dev_info(dev, "failed to get iommu node(%s)\n", node_name);
			continue;
		}

		num_ports = of_count_phandle_with_args(
						iommu_node, "iommus", "#iommu-cells");
		num_ports = (num_ports < 0) ? 0 : num_ports;
		dev_info(dev, "port_num:%d\n", num_ports);

		for (j = 0; j < num_ports; j++) {
			if (!of_parse_phandle_with_args(iommu_node,
				"iommus", "#iommu-cells", j, &args)) {
				mtk_iommu_register_fault_callback(
					args.args[0],
					mtk_camsv_translation_fault_callback,
					(void *)sv_dev, false);
			}

		}
	}

	num_ports = of_count_phandle_with_args(
					pdev->dev.of_node, "iommus", "#iommu-cells");
	num_ports = (num_ports < 0) ? 0 : num_ports;
	dev_info(dev, "port_num:%d\n", num_ports);

	for (i = 0; i < num_ports; i++) {
		if (!of_parse_phandle_with_args(pdev->dev.of_node,
			"iommus", "#iommu-cells", i, &args)) {
			mtk_iommu_register_fault_callback(
				args.args[0],
				mtk_camsv_translation_fault_callback,
				(void *)sv_dev, false);
		}
	}

	smmus = of_property_count_u32_elems(
		pdev->dev.of_node, "mediatek,smmu-dma-axid");
	smmus = (smmus > 0) ? smmus : 0;
	dev_info(dev, "smmu_num:%d\n", smmus);
	for (i = 0; i < smmus; i++) {
		u32 axid;

		if (!of_property_read_u32_index(
			pdev->dev.of_node, "mediatek,smmu-dma-axid", i, &axid)) {
			mtk_iommu_register_fault_callback(
				axid, mtk_camsv_translation_fault_callback,
				(void *)sv_dev, false);
		}
	}
#ifdef CONFIG_PM_SLEEP
	sv_dev->notifier_blk.notifier_call = mtk_camsv_suspend_pm_event;
	sv_dev->notifier_blk.priority = 0;
	ret = register_pm_notifier(&sv_dev->notifier_blk);
	if (ret) {
		dev_info(dev, "Failed to register PM notifier");
		return -ENODEV;
	}
#endif
	return 0;
}

static int mtk_camsv_component_bind(
	struct device *dev,
	struct device *master,
	void *data)
{
	struct mtk_camsv_device *sv_dev = dev_get_drvdata(dev);
	struct mtk_cam_device *cam_dev = data;

	sv_dev->cam = cam_dev;
	return mtk_cam_set_dev_sv(cam_dev->dev, sv_dev->id, dev);
}

static void mtk_camsv_component_unbind(
	struct device *dev,
	struct device *master,
	void *data)
{
}

static const struct component_ops mtk_camsv_component_ops = {
	.bind = mtk_camsv_component_bind,
	.unbind = mtk_camsv_component_unbind,
};

static int mtk_camsv_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_camsv_device *sv_dev;
	unsigned int sv_port_num = 0;
	int ret;

	sv_dev = devm_kzalloc(dev, sizeof(*sv_dev), GFP_KERNEL);
	if (!sv_dev)
		return -ENOMEM;

	sv_dev->dev = dev;
	dev_set_drvdata(dev, sv_dev);

	ret = mtk_camsv_of_probe(pdev, sv_dev);
	if (ret)
		return ret;

	CALL_PLAT_V4L2(
		get_sv_smi_setting, sv_dev->id, &sv_port_num);
	if (sv_port_num == SMI_PORT_SV_TYPE0_NUM) {
		ret = mtk_cam_qos_probe(dev, &sv_dev->qos, SMI_PORT_SV_TYPE0_NUM);
		if (ret)
			goto UNREGISTER_PM_NOTIFIER;
	} else if (sv_port_num == SMI_PORT_SV_TYPE1_NUM) {
		ret = mtk_cam_qos_probe(dev, &sv_dev->qos, SMI_PORT_SV_TYPE1_NUM);
		if (ret)
			goto UNREGISTER_PM_NOTIFIER;
	} else {
		ret = mtk_cam_qos_probe(dev, &sv_dev->qos, SMI_PORT_SV_TYPE2_NUM);
		if (ret)
			goto UNREGISTER_PM_NOTIFIER;
	}

	sv_dev->fifo_size =
		roundup_pow_of_two(8 * sizeof(struct mtk_camsys_irq_info));
	sv_dev->msg_buffer = devm_kzalloc(dev, sv_dev->fifo_size,
					     GFP_KERNEL);
	if (!sv_dev->msg_buffer) {
		ret = -ENOMEM;
		goto UNREGISTER_PM_NOTIFIER;
	}

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_camsv_component_ops);

	if (ret)
		goto UNREGISTER_PM_NOTIFIER;

	return ret;

UNREGISTER_PM_NOTIFIER:
	unregister_pm_notifier(&sv_dev->notifier_blk);
	return ret;
}

static void mtk_camsv_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_camsv_device *sv_dev = dev_get_drvdata(dev);

#ifdef CONFIG_PM_SLEEP
	unregister_pm_notifier(&sv_dev->notifier_blk);
#endif
	pm_runtime_disable(dev);

	mtk_cam_qos_remove(&sv_dev->qos);

	component_del(dev, &mtk_camsv_component_ops);
}

int mtk_camsv_runtime_suspend(struct device *dev)
{
	struct mtk_camsv_device *sv_dev = dev_get_drvdata(dev);
	int i, ret = 0;
	unsigned int sv_output_port;

	dev_dbg(dev, "%s:disable clock\n", __func__);

	if (atomic_read(&sv_dev->is_slave_on)) {
		struct mtk_camsv_device *slave_sv_dev = sv_dev->slave_sv_dev;

		for (i = 0; i < CAMSV_IRQ_NUM; i++) {
			disable_irq(slave_sv_dev->irq[i]);
			dev_info(slave_sv_dev->dev, "%s:disable irq %d\n", __func__, slave_sv_dev->irq[i]);
		}

		pr_info("%s slave sv device off", __func__);
		mtk_cam_reset_qos(slave_sv_dev->dev, &slave_sv_dev->qos);
		for (i = slave_sv_dev->num_clks - 1; i >= 0; i--)
			clk_disable_unprepare(slave_sv_dev->clks[i]);

		for (i = slave_sv_dev->num_larbs - 1; i >=0; i--)
			mtk_smi_larb_disable(&slave_sv_dev->larb_pdev[i]->dev);
		for (i = 1; i < SLAVE_SV_PORT_NUM; i++)
			mtk_cam_isp8s_bwr_clr_bw(slave_sv_dev->cam->bwr,
				get_sv_bwr_engine(slave_sv_dev->id),
				get_sv_axi_port(slave_sv_dev->id, i));
		sv_dev->slave_sv_dev = NULL;
		atomic_set(&sv_dev->is_slave_on, 0);
		atomic_set(&slave_sv_dev->is_sv_stress_test, 0);
	}

	if (camsv_fifo_detect) {
		if (atomic_read(&sv_dev->enable_fifo_detect))
			ret |= mtk_cam_sv_stop_fifo_detection(sv_dev);
		if (ret)
			dev_info(dev, "disable fifo_detection fail\n");
	}

	mtk_cam_reset_qos(dev, &sv_dev->qos);

	sv_output_port = get_sv_axi_port_num(sv_dev->id);
	for (i = 0; i < sv_output_port; i++) {
		mtk_cam_isp8s_bwr_set_chn_bw(sv_dev->cam->bwr,
			get_sv_bwr_engine(sv_dev->id), get_sv_axi_port(sv_dev->id, i),
			0, -(sv_dev->sv_avg_applied_bw_w / sv_output_port),
			0, -(sv_dev->sv_peak_applied_bw_w / sv_output_port), false);
	}

	mtk_cam_isp8s_bwr_set_ttl_bw(sv_dev->cam->bwr,
		get_sv_bwr_engine(sv_dev->id), -sv_dev->sv_avg_applied_bw_w,
		-sv_dev->sv_peak_applied_bw_w, false);

	sv_dev->sv_avg_applied_bw_w = 0;
	sv_dev->sv_peak_applied_bw_w = 0;

	mtk_cam_sv_golden_set(sv_dev, false);

	mtk_cam_sv_run_df_reset(sv_dev, false);
	mtk_cam_sv_run_df_bw_update(sv_dev, 0);

	if (atomic_dec_and_test(&sv_dev->cam->sv_ref_cnt)) {
		sv_top_reset_by_camsys_top(sv_dev);
		mtk_cam_sv_df_mgr_init(&sv_dev->cam->sv_df_mgr);
	}

	for (i = sv_dev->num_clks - 1; i >= 0; i--)
		clk_disable_unprepare(sv_dev->clks[i]);

	for (i = sv_dev->num_larbs - 1; i >=0; i--)
		mtk_smi_larb_disable(&sv_dev->larb_pdev[i]->dev);

	return 0;
}
bool mtk_camsv_check_stress_mode_on(struct mtk_camsv_device *sv_dev)
{
	struct device *slave_dev = sv_dev->cam->engines.sv_devs[3];
	struct mtk_camsv_device *slave_sv_dev = dev_get_drvdata(slave_dev);

	if (atomic_read(&slave_sv_dev->is_sv_stress_test))
		return false;

	if (camsv_stress_test_mode >= 1 && camsv_stress_test_mode <= 3
		&& sv_dev->id == 0 && !atomic_read(&slave_sv_dev->is_sv_stress_test))
		return true;
	else if (camsv_stress_test_mode >= 4 && camsv_stress_test_mode <= 6
		&& sv_dev->id == 1 && !atomic_read(&slave_sv_dev->is_sv_stress_test))
		return true;
	else if (camsv_stress_test_mode >= 7 && camsv_stress_test_mode <= 9
		&& sv_dev->id == 2 && !atomic_read(&slave_sv_dev->is_sv_stress_test))
		return true;


	return false;
}
int mtk_camsv_runtime_resume(struct device *dev)
{
	struct mtk_camsv_device *sv_dev = dev_get_drvdata(dev);
	int i, ret;

	if (mtk_camsv_check_stress_mode_on(sv_dev)) {
		struct device *slave_dev = sv_dev->cam->engines.sv_devs[3];
		struct mtk_camsv_device *slave_sv_dev = dev_get_drvdata(slave_dev);

		pr_info("%s open slave power/clock/larb/irq", __func__);

		atomic_set(&sv_dev->is_slave_on, 1);
		atomic_set(&slave_sv_dev->is_sv_stress_test, 1);

		slave_sv_dev->stress_test_mode = mtk_camsv_get_stress_mode(slave_sv_dev,
			camsv_stress_test_mode);

		sv_dev->slave_sv_dev = slave_sv_dev;

		for (i = 0; i < slave_sv_dev->num_larbs; i++)
			mtk_smi_larb_enable(&slave_sv_dev->larb_pdev[i]->dev);
		/* reset_msgfifo before enable_irq */
		ret = mtk_cam_sv_reset_msgfifo(slave_sv_dev);
		if (ret)
			return ret;
		for (i = 0; i < slave_sv_dev->num_clks; i++) {
			ret = clk_prepare_enable(slave_sv_dev->clks[i]);
			if (ret) {
				dev_info(slave_sv_dev->dev, "enable failed at clk #%d, ret = %d\n",
						i, ret);
				i--;
				while (i >= 0)
					clk_disable_unprepare(slave_sv_dev->clks[i--]);

				return ret;
			}
		}
		sv_reset_by_camsys_top(slave_sv_dev);
		for (i = 0; i < CAMSV_IRQ_NUM; i++) {
			enable_irq(slave_sv_dev->irq[i]);
			dev_info(slave_sv_dev->dev, "%s:enable irq %d\n", __func__, slave_sv_dev->irq[i]);
		}
	}

	for (i = 0; i < sv_dev->num_larbs; i++)
		mtk_smi_larb_enable(&sv_dev->larb_pdev[i]->dev);

	/* reset_msgfifo before enable_irq */
	ret = mtk_cam_sv_reset_msgfifo(sv_dev);
	if (ret)
		return ret;

	dev_info(dev, "%s:enable clock\n", __func__);
	for (i = 0; i < sv_dev->num_clks; i++) {
		ret = clk_prepare_enable(sv_dev->clks[i]);
		if (ret) {
			dev_info(dev, "enable failed at clk #%d, ret = %d\n",
				 i, ret);
			i--;
			while (i >= 0)
				clk_disable_unprepare(sv_dev->clks[i--]);

			return ret;
		}
	}
	sv_reset_by_camsys_top(sv_dev);

	atomic_inc(&sv_dev->cam->sv_ref_cnt);
	mtk_cam_sv_run_df_reset(sv_dev, true);

	for (i = 0; i < CAMSV_IRQ_NUM; i++) {
		enable_irq(sv_dev->irq[i]);
		dev_dbg(dev, "%s:enable irq %d\n", __func__, sv_dev->irq[i]);
	}

	if (camsv_fifo_detect) {
		if (atomic_read(&sv_dev->enable_fifo_detect))
			ret |= mtk_cam_sv_start_fifo_detection(sv_dev);
		if (ret)
			dev_info(dev, "enable fifo_detection fail\n");
	}
	return 0;
}

static const struct dev_pm_ops mtk_camsv_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_camsv_runtime_suspend, mtk_camsv_runtime_resume,
			   NULL)
};

struct platform_driver mtk_cam_sv_driver = {
	.probe   = mtk_camsv_probe,
	.remove  = mtk_camsv_remove,
	.driver  = {
		.name  = "mtk-cam camsv",
		.of_match_table = of_match_ptr(mtk_camsv_of_ids),
		.pm     = &mtk_camsv_pm_ops,
	}
};

