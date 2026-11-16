// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>
#include <linux/bitops.h>

#include "mtk_cam-plat.h"
#include "mtk_cam-raw_regs.h"
#include "mtk_cam-meta-mt6993.h"
#include "mtk_cam-ipi.h"
#include "mtk_camera-v4l2-controls-8s.h"
#include "mtk_camera-videodev2.h"
#include "mtk_cam-dvfs_qos_raw.h"
#include "mtk_cam-debug_option.h"

#define RAW_STATS_CFG_SIZE \
	ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_cfg), SZ_4K)

#define RAW_STAT_0_BUF_SIZE_STATIC \
			(MTK_CAM_UAPI_AEHO_MAX_BUF_SIZE + \
			MTK_CAM_UAPI_LTMSBO_SIZE + \
			MTK_CAM_UAPI_LTMSGO_SIZE + \
			MTK_CAM_UAPI_TSFSO_SIZE + \
			MTK_CAM_UAPI_TCYSO_SIZE)

#define RAW_STAT_0_STRUCT_SIZE \
	ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_0), SZ_4K)

/* meta out max size include 1k meta info and dma buffer size */
#define RAW_STATS_0_SIZE \
	(RAW_STAT_0_STRUCT_SIZE + \
	 RAW_STAT_0_BUF_SIZE_STATIC)

#define RAW_STATS_1_SIZE_HEADER \
	ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_1), SZ_4K)

#define RAW_STATS_1_SIZE \
	(RAW_STATS_1_SIZE_HEADER + MTK_CAM_UAPI_AFO_MAX_BUF_SIZE)

#define SV_STATS_0_SIZE \
	sizeof(struct mtk_cam_uapi_meta_camsv_stats_0)

#define MRAW_STATS_0_SIZE \
	sizeof(struct mtk_cam_uapi_meta_mraw_stats_0)

#define PDA_STATS_0_SIZE \
	sizeof(struct mtk_cam_uapi_meta_pda_dc_stats_0)

#define DYNAMIC_SIZE

#define DMA_SIZE_ALIGNMENT		32
static void set_payload(struct mtk_cam_uapi_meta_hw_buf *buf,
			unsigned int size, size_t *offset)
{
	size = ALIGN(size, DMA_SIZE_ALIGNMENT);

	buf->offset = *offset;
	buf->size = size;
	*offset += size;
}

static void meta_state0_reset_all(struct mtk_cam_uapi_meta_raw_stats_0 *stats)
{
	size_t offset = sizeof(*stats);

	set_payload(&stats->awb_stats.awbo1_buf, 0, &offset);
	set_payload(&stats->awb_stats.awbo2_buf, 0, &offset);
	set_payload(&stats->ae_stats.aeo_buf, 0, &offset);
	set_payload(&stats->ae_stats.aeho_buf, 0, &offset);
	set_payload(&stats->ae_stats.aedo_buf, 0, &offset);
	set_payload(&stats->statcolor_stats.statcolo_buf, 0, &offset);
	set_payload(&stats->ltm_stats.ltmsbo_buf, 0, &offset);
	set_payload(&stats->ltm_stats.ltmsgo_buf, 0, &offset);
	set_payload(&stats->flk_stats.flko_buf, 0, &offset);
	set_payload(&stats->dflk_stats.dflko_buf, 0, &offset);
	set_payload(&stats->dflk_stats.dflkbo_buf, 0, &offset);
	set_payload(&stats->tsf_stats.tsfo_r1_buf, 0, &offset);
	set_payload(&stats->tsf_stats.tsfo_r2_buf, 0, &offset);
	set_payload(&stats->tsf_stats.tsfo_r3_buf, 0, &offset);
	set_payload(&stats->tsf_stats.tsfo_r4_buf, 0, &offset);
	set_payload(&stats->tcys_stats.tcyso_buf, 0, &offset);
	// TODO: FIX PDE
	set_payload(&stats->pde_stats.pdo_buf, 0, &offset);
}
static int get_ltms_free_run(const struct set_meta_stats_info_param *p)
{
	struct mtk_cam_uapi_meta_raw_stats_cfg *cfg = p->meta_cfg;

	return cfg->ltms_param.select_control;
}

static int get_ltmsgo_offset(void *addr)
{
	struct mtk_cam_uapi_meta_raw_stats_0 *stats = addr;

	return stats->ltm_stats.ltmsgo_buf.offset;
}

static int set_meta_stat0_info(struct mtk_cam_uapi_meta_raw_stats_0 *stats,
			       size_t size,
			       const struct set_meta_stats_info_param *p)
{
	struct mtk_cam_uapi_meta_raw_stats_cfg *cfg = p->meta_cfg;
	size_t offset = sizeof(*stats);
	unsigned int flko_size;
	unsigned int dflko_size, dflkbo_size;
	unsigned int dflk_blk_num = MTK_CAM_UAPI_DFLK_MAX_STAT_BLK_NUM;
	unsigned int awbo_r1_size, awbo_r2_size, aeo_size;
	unsigned int pdo_size;
	unsigned int dflk_sample_rate = 1;

	if (!p->meta_cfg || !p->meta_cfg_size) {
		meta_state0_reset_all(stats);
		return 0;
	}

#ifdef DYNAMIC_SIZE
	flko_size = (p->height / p->bin_ratio) *
		MTK_CAM_UAPI_FLK_BLK_SIZE * MTK_CAM_UAPI_FLK_MAX_STAT_BLK_NUM;
	awbo_r1_size = (cfg->awb_param.stat_en & (0x1)) *
		cfg->awb_param.windownum_x *
		cfg->awb_param.windownum_y *
		32;
	awbo_r2_size = (cfg->awb_param.stat_en >> 2 & (0x1)) *
		(2 * cfg->awb_param.windownum_x) *
		(2 * cfg->awb_param.windownum_y) *
		16;
	aeo_size = (cfg->ae_enable & (0x1)) *
		(cfg->ae_param.block_win_cfg.block_num_x + MTK_CAM_UAPI_AEO_DUMMY_BLK_XNUM) *
		cfg->ae_param.block_win_cfg.block_num_y *
		32;

	if (hweight32(p->raws) == 2)
		dflk_blk_num += 2;
	else if (hweight32(p->raws) == 3)
		dflk_blk_num += 4;

	dflk_sample_rate = max(cfg->dflk_param.sample_rate, 1);

	dflko_size = (p->height / p->bin_ratio / dflk_sample_rate) *
		MTK_CAM_UAPI_DFLK_BLK_SIZE * dflk_blk_num;
	dflkbo_size = dflko_size;
#else
	flko_size = MTK_CAM_UAPI_FLK_MAX_BUF_SIZE;
	awbo_r1_size = MTK_CAM_UAPI_AWBO_R1_MAX_BUF_SIZE;
	awbo_r2_size = MTK_CAM_UAPI_AWBO_R2_MAX_BUF_SIZE;
	aeo_size = MTK_CAM_UAPI_AEO_MAX_BUF_SIZE;
	dflko_size = dflkbo_size = MTK_CAM_UAPI_DFLKO_MAX_BUF_SIZE;
#endif
	if (CAM_DEBUG_ENABLED(IPI_BUF))
		pr_info("[%s] flko/awb1/awb2/aeo/dflko(SR)/dflkbo:%d/%d/%d/%d/%d(%u)/%d",
			__func__, flko_size, awbo_r1_size, awbo_r2_size, aeo_size,
			dflko_size, dflk_sample_rate, dflkbo_size);

	// TODO: FIX PDE
	pdo_size = cfg->pde_enable ? cfg->pde_param.pdo_max_size : 0;

	set_payload(&stats->awb_stats.awbo1_buf,
		    awbo_r1_size, &offset);
	set_payload(&stats->awb_stats.awbo2_buf,
		    awbo_r2_size, &offset);
	set_payload(&stats->ae_stats.aeo_buf,
			aeo_size,
			&offset);
	set_payload(&stats->ae_stats.aeho_buf,
			MTK_CAM_UAPI_AEHO_MAX_BUF_SIZE,
			&offset);
	set_payload(&stats->ae_stats.aedo_buf,
			MTK_CAM_UAPI_AEDO_MAX_BUF_SIZE,
			&offset);
	set_payload(&stats->statcolor_stats.statcolo_buf,
				MTK_CAM_UAPI_STATCOLO_MAX_BUF_SIZE,
				&offset);
	set_payload(&stats->ltm_stats.ltmsbo_buf,
		    MTK_CAM_UAPI_LTMSBO_SIZE, &offset);
	set_payload(&stats->ltm_stats.ltmsgo_buf,
		    MTK_CAM_UAPI_LTMSGO_SIZE, &offset);
	set_payload(&stats->flk_stats.flko_buf,
		    flko_size, &offset);
	set_payload(&stats->dflk_stats.dflko_buf,
				dflko_size, &offset);
	set_payload(&stats->dflk_stats.dflkbo_buf,
				dflkbo_size, &offset);
	set_payload(&stats->tsf_stats.tsfo_r1_buf,
		    MTK_CAM_UAPI_TSFSO_SIZE, &offset);
	if (cfg->tsfs_r2_enable)
		set_payload(&stats->tsf_stats.tsfo_r2_buf,
				MTK_CAM_UAPI_TSFSO_SIZE, &offset);
	if (cfg->tsfs_r3_enable)
		set_payload(&stats->tsf_stats.tsfo_r3_buf,
				MTK_CAM_UAPI_TSFSO_SIZE, &offset);
	if (cfg->tsfs_r4_enable)
		set_payload(&stats->tsf_stats.tsfo_r4_buf,
				MTK_CAM_UAPI_TSFSO_SIZE, &offset);
	set_payload(&stats->tcys_stats.tcyso_buf,
		    MTK_CAM_UAPI_TCYSO_SIZE, &offset);
	set_payload(&stats->pde_stats.pdo_buf, pdo_size, &offset);

	if (offset > size) {
		pr_info("%s: required %zu > buffer size %zu\n",
			__func__, offset, size);
		return -1;
	}

	return 0;
}

static int set_meta_stat1_info(struct mtk_cam_uapi_meta_raw_stats_1 *stats,
			       size_t size,
			       const struct set_meta_stats_info_param *p)
{
	size_t offset = sizeof(*stats);

	set_payload(&stats->af_stats.afo_buf,
		    MTK_CAM_UAPI_AFO_MAX_BUF_SIZE, &offset);

	if (offset > size) {
		pr_info("%s: required %zu > buffer size %zu\n",
			__func__, offset, size);
		return -1;
	}

	return 0;
}

static int set_meta_stats_info(int ipi_id, void *addr, size_t size,
			       const struct set_meta_stats_info_param *p)
{
	if (WARN_ON(!addr))
		return -1;

	switch (ipi_id) {
	case MTKCAM_IPI_RAW_META_STATS_0:
		if (WARN_ON(size < sizeof(struct mtk_cam_uapi_meta_raw_stats_0)))
			return -1;

		return set_meta_stat0_info(addr, size, p);
	case MTKCAM_IPI_RAW_META_STATS_1:
		if (WARN_ON(size < sizeof(struct mtk_cam_uapi_meta_raw_stats_1)))
			return -1;

		return set_meta_stat1_info(addr, size, p);
	default:
		pr_info("%s: %s: not supported: %d\n",
			__FILE__, __func__, ipi_id);
		break;
	}
	return -1;
}

// TODO: be checked by backend
#define MTK_CAM_AEI_TABLE_SIZE (16384)
#define MTK_CAM_LSCI_TABLE_SIZE (32768)
static int get_meta_cfg_port_size(
	struct mtk_cam_uapi_meta_raw_stats_cfg *stats_cfg, int dma_port)
{
	switch (dma_port) {
	case PORT_CQI_R1:
	// case PORT_CQI_R2:
	// case PORT_CQI_R3:
	// case PORT_CQI_R4:
		return 0x10000;
	case PORT_BPCI_R1:
	case PORT_BPCI_R2:
	case PORT_BPCI_R3:
		return stats_cfg->pde_param.pdi_max_size;
	case PORT_PDI_R1:
		return stats_cfg->pde_param.pdi_max_size;
	case PORT_CACI_R1:
		return stats_cfg->cac_param.caci_buf.size;
	case PORT_AEI_R1:
		return MTK_CAM_AEI_TABLE_SIZE;
	case PORT_LSCI_R1:
		return MTK_CAM_LSCI_TABLE_SIZE;
	default:
		pr_info("%s: %s: not supported: %d\n",
			__FILE__, __func__, dma_port);
	}
	return 0;
}

// TODO: be checked by backend
static int get_meta_stats0_port_size(
	struct mtk_cam_uapi_meta_raw_stats_0 *stats_0, int dma_port)
{
	switch (dma_port) {
	case PORT_FHO_R1:
		return 4096;  /* 128 * 32(subsample) */
	case PORT_AEO_R1:
		return stats_0->ae_stats.aeo_buf.size;
	case PORT_AEHO_R1:
		return stats_0->ae_stats.aeho_buf.size;
	case PORT_AEDO_R1:
		return stats_0->ae_stats.aedo_buf.size;
	case PORT_STATCOLO_R1:
		return stats_0->statcolor_stats.statcolo_buf.size;
	case PORT_AWBO_R1:
		return stats_0->awb_stats.awbo1_buf.size;
	case PORT_AWBO_R2:
		return stats_0->awb_stats.awbo2_buf.size;
	case PORT_LTMSBO_R1:
		return stats_0->ltm_stats.ltmsbo_buf.size;
	case PORT_LTMSGO_R1:
		return stats_0->ltm_stats.ltmsgo_buf.size;
	case PORT_FLKO_R1:
		return stats_0->flk_stats.flko_buf.size;
	case PORT_DFLKO_R1:
		return stats_0->dflk_stats.dflko_buf.size;
	case PORT_DFLKBO_R1:
		return stats_0->dflk_stats.dflkbo_buf.size;
	case PORT_TCYSO_R1:
		return stats_0->tcys_stats.tcyso_buf.size;
	case PORT_TSFSO_R1:
		return stats_0->tsf_stats.tsfo_r1_buf.size;
	case PORT_TSFSO_R2:
		return stats_0->tsf_stats.tsfo_r2_buf.size;
	case PORT_TSFSO_R3:
		return stats_0->tsf_stats.tsfo_r3_buf.size;
	case PORT_TSFSO_R4:
		return stats_0->tsf_stats.tsfo_r4_buf.size;
	case PORT_PDO_R1: // TODO: FIX PDE
		return stats_0->pde_stats.pdo_buf.size;
	default:
		pr_info("%s: %s: not supported: %d\n",
			__FILE__, __func__, dma_port);
	}

	return 0;
}

static int get_meta_stats1_port_size(
	struct mtk_cam_uapi_meta_raw_stats_1 *stats_1, int dma_port)
{
	switch (dma_port) {
	case PORT_AFO_R1:
		return stats_1->af_stats.afo_buf.size;
	default:
		pr_info("%s: %s: not supported: %d\n",
			__FILE__, __func__, dma_port);
	}
	return 0;
}

static int get_meta_stats_port_size(
		int ipi_id, void *addr, int dma_port, int *size)
{
	if (!addr)
		return -1;

	switch (ipi_id) {
	case MTKCAM_IPI_RAW_META_STATS_CFG:
		*size = get_meta_cfg_port_size(addr, dma_port);
		break;
	case MTKCAM_IPI_RAW_META_STATS_0:
		*size = get_meta_stats0_port_size(addr, dma_port);
		break;
	case MTKCAM_IPI_RAW_META_STATS_1:
		*size = get_meta_stats1_port_size(addr, dma_port);
		break;
	default:
		*size = 0;
		break;
	}

	return 0;
}

static int set_sv_meta_stats_info(
	int ipi_id, void *addr, struct dma_info *info)
{
	struct mtk_cam_uapi_meta_camsv_stats_0 *sv_stats0;
	unsigned long offset;
	unsigned int size;

	switch (ipi_id) {
	case MTKCAM_IPI_CAMSV_MAIN_OUT:
		sv_stats0 = (struct mtk_cam_uapi_meta_camsv_stats_0 *)addr;
		size = info->stride * info->height;
		/* calculate offset for 16-alignment limitation */
		offset = ((((dma_addr_t)sv_stats0 + SV_STATS_0_SIZE + 15) >> 4) << 4)
			- (dma_addr_t)sv_stats0;
		set_payload(&sv_stats0->pd_stats.pdo_buf, size, &offset);
		sv_stats0->pd_stats_enabled = 1;
		sv_stats0->pd_stats.stats_src.width = info->width;
		sv_stats0->pd_stats.stats_src.height = info->height;
		sv_stats0->pd_stats.stride = info->stride;
		break;
	default:
		pr_info("%s: %s: not supported: %d\n",
			__FILE__, __func__, ipi_id);
		break;
	}

	return 0;
}

static int get_sv_max_pixel_mode(unsigned int dev_id,
	unsigned int *max_pixel_mode)
{
	if (dev_id < MULTI_SMI_SV_HW_NUM)
		*max_pixel_mode = 4;
	else
		*max_pixel_mode = 3;

	return 0;
}

static int get_is_smmu_enabled(bool *is_smmu_enabled)
{
	*is_smmu_enabled = true;
	return 0;
}

static int get_sv_smi_setting(unsigned int dev_id,
	unsigned int *sv_smi_num)
{
	if (dev_id < 3)
		*sv_smi_num = 7;
	else if (dev_id == 3)
		*sv_smi_num = 5;
	else
		*sv_smi_num = 3;

	return 0;
}

static int get_raw_lock_sel_addr(unsigned int dev_id,
	unsigned int *addr)
{
	switch (dev_id) {
	case 0:
		*addr = 0x3A700160;
		break;
	case 1:
		*addr = 0x3A900160;
		break;
	case 2:
		*addr = 0x3AB00160;
		break;
	default:
		*addr = 0;
		break;
	}

	return 0;
}

static int get_single_sv_opp_idx(unsigned int *opp_idx)
{
	*opp_idx = 1;

	return 0;
}

static int get_sv_fifo_core_setting(unsigned int dev_id, unsigned int *fifo_core1,
unsigned int *fifo_core2, unsigned int *fifo_core3)
{
	const unsigned int max_fifo_img_p1[CAMSV_END] = {1536, 1536, 1536, 1024, 512, 512};
	const unsigned int max_fifo_img_p2[CAMSV_END] = {1536, 1536, 1536, 1024, 0, 0};
	const unsigned int max_fifo_img_p3[CAMSV_END] = {1536, 1536, 1536, 0, 0, 0};

	if (dev_id >= CAMSV_3) {
		pr_info("%s unexpected dev_id(%d) only camsv a/b/c need fifo monitor\n",
			__func__, dev_id);
		*fifo_core1 = 0;
		*fifo_core2 = 0;
		*fifo_core3 = 0;
		return 0;
	}

	*fifo_core1 = max_fifo_img_p1[dev_id];
	*fifo_core2 = max_fifo_img_p2[dev_id];
	*fifo_core3 = max_fifo_img_p3[dev_id];

	return 0;
}

static int get_sv_dma_th_setting(unsigned int dev_id, unsigned int fifo_img_p1,
	unsigned int fifo_img_p2, unsigned int fifo_img_p3,
	struct sv_dma_th_setting *th_setting, struct sv_dma_bw_setting *bw_setting)
{
	const unsigned int max_fifo_img_p1[CAMSV_END] = {1536, 1536, 1536, 1024, 512, 512};
	const unsigned int max_fifo_img_p2[CAMSV_END] = {1536, 1536, 1536, 1024, 0, 0};
	const unsigned int max_fifo_img_p3[CAMSV_END] = {1536, 1536, 1536, 0, 0, 0};
	unsigned int lb_len_fifo = 36;
	const unsigned int max_fifo_cq1 = 64;
	const unsigned int max_fifo_cq2 = 64;
	unsigned int img_p1, img_p2, img_p3;
	int urgent_high, urgent_low, ultra_high, ultra_low, pultra_high, pultra_low;

	if (dev_id >= CAMSV_END)
		return 0;

	if (dev_id == CAMSV_3)
		lb_len_fifo = 18;

	img_p1 = fifo_img_p1 ? fifo_img_p1 : max_fifo_img_p1[dev_id];
	img_p2 = fifo_img_p2 ? fifo_img_p2 : max_fifo_img_p2[dev_id];
	img_p3 = fifo_img_p3 ? fifo_img_p3 : max_fifo_img_p3[dev_id];

	urgent_high = ((bw_setting->urgent_high >= 0) ? bw_setting->urgent_high : 40);
	urgent_low = ((bw_setting->urgent_low >= 0) ? bw_setting->urgent_low : 30);
	ultra_high = ((bw_setting->ultra_high >= 0) ? bw_setting->ultra_high : 20);
	ultra_low = ((bw_setting->ultra_low >= 0) ? bw_setting->ultra_low : 10);
	pultra_high = ((bw_setting->pultra_high >= 0) ? bw_setting->pultra_high : 20);
	pultra_low = ((bw_setting->pultra_low >= 0) ? bw_setting->pultra_low : 10);

	th_setting->urgent_th =
		1 << 31 | FIFO_THRESHOLD(img_p1, urgent_high / 100, urgent_low / 100);
	th_setting->ultra_th =
		1 << 31 | FIFO_THRESHOLD(img_p1, 0, 0);
	th_setting->pultra_th =
		1 << 31 | FIFO_THRESHOLD(img_p1, 0, 0);
	th_setting->dvfs_th =
		1 << 31 | FIFO_THRESHOLD(img_p1, 0, 0);

	th_setting->urgent_th2 =
		1 << 31 | FIFO_THRESHOLD(img_p2, urgent_high / 100, urgent_low / 100);
	th_setting->ultra_th2 =
		1 << 31 | FIFO_THRESHOLD(img_p2, 0, 0);
	th_setting->pultra_th2 =
		1 << 31 | FIFO_THRESHOLD(img_p2, 0, 0);
	th_setting->dvfs_th2 =
		1 << 31 | FIFO_THRESHOLD(img_p2, 0, 0);

	th_setting->urgent_th3 =
		1 << 31 | FIFO_THRESHOLD(img_p3, urgent_high / 100, urgent_low / 100);
	th_setting->ultra_th3 =
		1 << 31 | FIFO_THRESHOLD(img_p3, 0, 0);
	th_setting->pultra_th3 =
		1 << 31 | FIFO_THRESHOLD(img_p3, 0, 0);
	th_setting->dvfs_th3 =
		1 << 31 | FIFO_THRESHOLD(img_p3, 0, 0);

	th_setting->urgent_len1_th =
		1 << 31 | FIFO_THRESHOLD(lb_len_fifo, 1, 80 / 100);
	th_setting->ultra_len1_th =
		1 << 31 | FIFO_THRESHOLD(lb_len_fifo, 0, 0);
	th_setting->pultra_len1_th =
		1 << 31 | FIFO_THRESHOLD(lb_len_fifo, 0, 0);
	th_setting->dvfs_len1_th =
		1 << 31 | FIFO_THRESHOLD(lb_len_fifo, 0, 0);

	th_setting->urgent_len2_th =
		1 << 31 | FIFO_THRESHOLD(lb_len_fifo, 1, 80 / 100);
	th_setting->ultra_len2_th =
		1 << 31 | FIFO_THRESHOLD(lb_len_fifo, 0, 0);
	th_setting->pultra_len2_th =
		1 << 31 | FIFO_THRESHOLD(lb_len_fifo, 0, 0);
	th_setting->dvfs_len2_th =
		1 << 31 | FIFO_THRESHOLD(lb_len_fifo, 0, 0);

	th_setting->urgent_len3_th =
		1 << 31 | FIFO_THRESHOLD(lb_len_fifo, 1, 80 / 100);
	th_setting->ultra_len3_th =
		1 << 31 | FIFO_THRESHOLD(lb_len_fifo, 0, 0);
	th_setting->pultra_len3_th =
		1 << 31 | FIFO_THRESHOLD(lb_len_fifo, 0, 0);
	th_setting->dvfs_len3_th =
		1 << 31 | FIFO_THRESHOLD(lb_len_fifo, 0, 0);

	th_setting->cq1_fifo_size = (0x10 << 24) | max_fifo_cq1;
	th_setting->cq1_urgent_th =
		1 << 31 | FIFO_THRESHOLD(max_fifo_cq1, urgent_high / 100, urgent_low / 100);
	th_setting->cq1_ultra_th =
		1 << 28 | FIFO_THRESHOLD(max_fifo_cq1, ultra_high / 100, ultra_low / 100);
	th_setting->cq1_pultra_th =
		1 << 28 | FIFO_THRESHOLD(max_fifo_cq1, pultra_high / 100, pultra_low / 100);
	th_setting->cq1_dvfs_th =
		1 << 31 | FIFO_THRESHOLD(max_fifo_cq1, 1/10, 0);

	th_setting->cq2_fifo_size = (0x10 << 24) | max_fifo_cq2;
	th_setting->cq2_urgent_th =
		1 << 31 | FIFO_THRESHOLD(max_fifo_cq2, urgent_high / 100, urgent_low / 100);
	th_setting->cq2_ultra_th =
		1 << 28 | FIFO_THRESHOLD(max_fifo_cq2, ultra_high / 100, ultra_low / 100);
	th_setting->cq2_pultra_th =
		1 << 28 | FIFO_THRESHOLD(max_fifo_cq2, pultra_high / 100, pultra_low / 100);
	th_setting->cq2_dvfs_th =
		1 << 31 | FIFO_THRESHOLD(max_fifo_cq2, 1/10, 0);

	return 0;
}

static int get_mraw_dmao_common_setting(struct mraw_dma_th_setting *mraw_th_setting,
	struct mraw_cq_th_setting *mraw_cq_setting)
{
	mraw_th_setting[imgo_m1].urgent_th = 1<<31|FIFO_THRESHOLD(488, 6/10, 5/10);
	mraw_th_setting[imgo_m1].ultra_th = 1<<28|FIFO_THRESHOLD(488, 4/10, 3/10);
	mraw_th_setting[imgo_m1].pultra_th = 1<<28|FIFO_THRESHOLD(488, 2/10, 1/10);
	mraw_th_setting[imgo_m1].dvfs_th = 1<<31|FIFO_THRESHOLD(488, 1/10, 0);
	mraw_th_setting[imgo_m1].fifo_size = (0x10 << 24) | 488;

	mraw_th_setting[imgbo_m1].urgent_th = 1<<31|FIFO_THRESHOLD(392, 6/10, 5/10);
	mraw_th_setting[imgbo_m1].ultra_th = 1<<28|FIFO_THRESHOLD(392, 4/10, 3/10);
	mraw_th_setting[imgbo_m1].pultra_th = 1<<28|FIFO_THRESHOLD(392, 2/10, 1/10);
	mraw_th_setting[imgbo_m1].dvfs_th = 1<<31|FIFO_THRESHOLD(392, 1/10, 0);
	mraw_th_setting[imgbo_m1].fifo_size = (0x10 << 24) | 392;

	mraw_th_setting[cpio_m1].urgent_th = 1<<31|FIFO_THRESHOLD(64, 6/10, 5/10);
	mraw_th_setting[cpio_m1].ultra_th = 1<<28|FIFO_THRESHOLD(64, 4/10, 3/10);
	mraw_th_setting[cpio_m1].pultra_th = 1<<28|FIFO_THRESHOLD(64, 2/10, 1/10);
	mraw_th_setting[cpio_m1].dvfs_th = 1<<31|FIFO_THRESHOLD(64, 1/10, 0);
	mraw_th_setting[cpio_m1].fifo_size = (0x10 << 24) | 64;

	mraw_cq_setting->cq1_fifo_size = (0x10 << 24) | 64;
	mraw_cq_setting->cq1_urgent_th = 1<<31|FIFO_THRESHOLD(64, 6/10, 5/10);
	mraw_cq_setting->cq1_ultra_th = 1<<28|FIFO_THRESHOLD(64, 4/10, 3/10);
	mraw_cq_setting->cq1_pultra_th = 1<<28|FIFO_THRESHOLD(64, 2/10, 1/10);
	mraw_cq_setting->cq1_dvfs_th = 1<<31|FIFO_THRESHOLD(64, 1/10, 0);

	mraw_cq_setting->cq2_fifo_size = (0x10 << 24) | 64;
	mraw_cq_setting->cq2_urgent_th = 1<<31|FIFO_THRESHOLD(64, 6/10, 5/10);
	mraw_cq_setting->cq2_ultra_th = 1<<28|FIFO_THRESHOLD(64, 4/10, 3/10);
	mraw_cq_setting->cq2_pultra_th = 1<<28|FIFO_THRESHOLD(64, 2/10, 1/10);
	mraw_cq_setting->cq2_dvfs_th = 1<<31|FIFO_THRESHOLD(64, 1/10, 0);

	return 0;
}

static int set_pda_status(void *addr, bool pda_support)
{
	struct mtk_cam_uapi_meta_pda_dc_stats_0 *pda_stats0;

	pda_stats0 = (struct mtk_cam_uapi_meta_pda_dc_stats_0 *)addr;
	pda_stats0->pda_dc_status = pda_support ? 1 : 0;
	return 0;
}

static int set_mraw_meta_stats_info(
	int ipi_id, void *addr, struct dma_info *info, bool pdp_support, bool pda_support)
{
	struct mtk_cam_uapi_meta_mraw_stats_0 *mraw_stats0;
	struct mtk_cam_uapi_meta_pda_dc_stats_0 *pda_stats0;
	unsigned long offset;
	unsigned int size;

	switch (ipi_id) {
	case MTKCAM_IPI_MRAW_META_STATS_0:
		mraw_stats0 = (struct mtk_cam_uapi_meta_mraw_stats_0 *)addr;
		mraw_stats0->pdp_en_status = pdp_support ? 1 : 0;
		/* imgo */
		size = info[imgo_m1].stride * info[imgo_m1].height;
		/* calculate offset for 16-alignment limitation */
		offset = ((((dma_addr_t)mraw_stats0 + MRAW_STATS_0_SIZE + 15) >> 4) << 4)
			- (dma_addr_t)mraw_stats0;
		set_payload(&mraw_stats0->pdp_0_stats.pdo_buf, size, &offset);
		mraw_stats0->pdp_0_stats_enabled = 1;
		mraw_stats0->pdp_0_stats.stats_src.width = info[imgo_m1].width;
		mraw_stats0->pdp_0_stats.stats_src.height = info[imgo_m1].height;
		mraw_stats0->pdp_0_stats.stride = info[imgo_m1].stride;
		if (!pdp_support)
			return 0;
		/* imgbo */
		size = info[imgbo_m1].stride * info[imgbo_m1].height;
		/* calculate offset for 16-alignment limitation */
		offset = ((((dma_addr_t)mraw_stats0 + offset + 15) >> 4) << 4)
			- (dma_addr_t)mraw_stats0;
		set_payload(&mraw_stats0->pdp_1_stats.pdo_buf, size, &offset);
		mraw_stats0->pdp_1_stats_enabled = 1;
		mraw_stats0->pdp_1_stats.stats_src.width = info[imgbo_m1].width;
		mraw_stats0->pdp_1_stats.stats_src.height = info[imgbo_m1].height;
		mraw_stats0->pdp_1_stats.stride = info[imgbo_m1].stride;
		/* cpio m1*/
		size = info[cpio_m1].stride * info[cpio_m1].height;
		/* calculate offset for 16-alignment limitation */
		offset = ((((dma_addr_t)mraw_stats0 + offset + 15) >> 4) << 4)
			- (dma_addr_t)mraw_stats0;
		set_payload(&mraw_stats0->cpi_0_stats.cpio_buf, size, &offset);
		mraw_stats0->cpi_0_stats_enabled = 1;
		mraw_stats0->cpi_0_stats.stats_src.width = info[cpio_m1].width;
		mraw_stats0->cpi_0_stats.stats_src.height = info[cpio_m1].height;
		mraw_stats0->cpi_0_stats.stride = info[cpio_m1].stride;
		/* cpio_m2*/
		size = info[cpio_m2].stride * info[cpio_m2].height;
		/* calculate offset for 16-alignment limitation */
		offset = ((((dma_addr_t)mraw_stats0 + offset + 15) >> 4) << 4)
			- (dma_addr_t)mraw_stats0;
		set_payload(&mraw_stats0->cpi_1_stats.cpio_buf, size, &offset);
		mraw_stats0->cpi_1_stats_enabled = 1;
		mraw_stats0->cpi_1_stats.stats_src.width = info[cpio_m2].width;
		mraw_stats0->cpi_1_stats.stats_src.height = info[cpio_m2].height;
		mraw_stats0->cpi_1_stats.stride = info[cpio_m2].stride;
		break;
	case MTKCAM_IPI_MRAW_PDA_OUT:
		pda_stats0 = (struct mtk_cam_uapi_meta_pda_dc_stats_0 *)addr;
		pda_stats0->pda_dc_stats_enabled = pda_support ? 1 : 0;
		size = info[pdao_m1].stride * info[pdao_m1].height;
		/* calculate offset for 16-alignment limitation */
		offset = ((((dma_addr_t)pda_stats0 + PDA_STATS_0_SIZE + 15) >> 4) << 4)
			- (dma_addr_t)pda_stats0;
		set_payload(&pda_stats0->pda_dc_stats.pda_buf, size, &offset);
		pda_stats0->pda_dc_status = 1;
		pda_stats0->pda_dc_stats.stats_src.width = info[pdao_m1].width;
		pda_stats0->pda_dc_stats.stats_src.height = info[pdao_m1].height;
		pda_stats0->pda_dc_stats.stride = info[pdao_m1].stride;
		break;
	default:
		pr_info("%s: %s: not supported: %d\n",
			__FILE__, __func__, ipi_id);
		break;
	}

	return 0;
}

static int get_pda_idx(
	void *addr, int *pda_idx)
{
	struct mtk_cam_uapi_meta_mraw_stats_cfg *stats_cfg =
		(struct mtk_cam_uapi_meta_mraw_stats_cfg *)addr;
	if (stats_cfg->pda_dc_enable)
		*pda_idx = stats_cfg->pda_dc_param.pda_index;
	else
		*pda_idx = -1;

	return 0;
}
static int get_mraw_stats_cfg_param(
	void *addr, struct mraw_stats_cfg_param *param)
{
	struct mtk_cam_uapi_meta_mraw_stats_cfg *stats_cfg =
		(struct mtk_cam_uapi_meta_mraw_stats_cfg *)addr;

	param->pda_dc_en = stats_cfg->pda_dc_enable;
	param->pdp_en = stats_cfg->pdp_enable;
	param->pda_idx = stats_cfg->pda_dc_param.pda_index;
	param->pda_stride = stats_cfg->pda_dc_param.stride;
	param->pda_width = stats_cfg->pda_dc_param.width;
	param->pda_height = stats_cfg->pda_dc_param.height;
	memcpy(param->pda_cfg, stats_cfg->pda_dc_param.cfg, sizeof(param->pda_cfg));

	param->mqe_en = stats_cfg->mqe_enable;
	param->mobc_en = stats_cfg->mobc_enable;
	param->plsc_en = stats_cfg->plsc_enable;
	param->dbg_en = stats_cfg->dbg_enable;
	param->lm_en = stats_cfg->lm_enable;

	param->crop_width = stats_cfg->crop_param.crop_x_end -
		stats_cfg->crop_param.crop_x_start + 1;
	param->crop_height = stats_cfg->crop_param.crop_y_end -
		stats_cfg->crop_param.crop_y_start + 1;

	param->mqe_mode = stats_cfg->mqe_param.mqe_mode;

	param->mbn_hei = stats_cfg->mbn_param.mbn_hei;
	param->mbn_pow = stats_cfg->mbn_param.mbn_pow;
	param->mbn_dir = stats_cfg->mbn_param.mbn_dir;
	param->mbn_spar_hei = stats_cfg->mbn_param.mbn_spar_hei;
	param->mbn_spar_pow = stats_cfg->mbn_param.mbn_spar_pow;
	param->mbn_spar_fac = stats_cfg->mbn_param.mbn_spar_fac;
	param->mbn_spar_con1 = stats_cfg->mbn_param.mbn_spar_con1;
	param->mbn_spar_con0 = stats_cfg->mbn_param.mbn_spar_con0;

	param->cpi_th = stats_cfg->cpi_param.cpi_th;
	param->cpi_pow = stats_cfg->cpi_param.cpi_pow;
	param->cpi_dir = stats_cfg->cpi_param.cpi_dir;
	param->cpi_spar_hei = stats_cfg->cpi_param.cpi_spar_hei;
	param->cpi_spar_pow = stats_cfg->cpi_param.cpi_spar_pow;
	param->cpi_spar_fac = stats_cfg->cpi_param.cpi_spar_fac;
	param->cpi_spar_con1 = stats_cfg->cpi_param.cpi_spar_con1;
	param->cpi_spar_con0 = stats_cfg->cpi_param.cpi_spar_con0;

	param->img_sel = stats_cfg->img_sel_param.img_sel;
	param->imgo_sel = stats_cfg->img_sel_param.imgo_sel;

	param->lm_mode_ctrl = stats_cfg->lm_param.lm_mode_ctrl;

	return 0;
}

static struct dma_group raw_dma_group[] = {
	/* Larb 16/43/44 - 19 */
	/* RDMA, port 0~9 */
	{REG_CQI_R1_BASE, REG_CQI_R2_BASE, REG_CQI_R3_BASE, REG_CQI_R4_BASE},
	{REG_RAWI_R2_BASE, REG_UFDI_R2_BASE, 0x0, 0x0},
	{REG_RAWI_R3_BASE, REG_UFDI_R3_BASE, REG_AEDI_R1_BASE, 0x0/*REG_STATDISI_R1_BASE*/},
	{REG_RAWI_R5_BASE, REG_UFDI_R5_BASE, 0x0, 0x0},
	{REG_BPCI_R1_BASE, REG_BPCI_R2_BASE, REG_BPCI_R3_BASE, REG_FPRI_R1_BASE},
	{REG_PDI_R1_BASE, REG_CACI_R1_BASE, REG_AEI_R1_BASE, REG_GRMGI_R1_BASE},
	{REG_LSCI_R1_BASE, REG_LTMSCTI_R1_BASE, REG_LTMSTI_R1_BASE, REG_LTMSTI_R2_BASE},
	{REG_TCYSI_R1_BASE, REG_AEHI_R1_BASE, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0},
	{0x0, 0x0, 0x0, 0x0},
	/* WDMA, port, 10~18 */
	{REG_IMGO_R1_BASE, REG_UFEO_R1_BASE, 0x0, 0x0},
	{REG_DRZB2NO_R1_BASE, REG_AFO_R1_BASE, 0x0, 0x0},
	{0x0, REG_FHO_R1_BASE, 0x0, REG_STATCOLO_R1_BASE},
	{0x0, REG_AEO_R1_BASE, REG_LTMSBO_R1_BASE, REG_LTMSGO_R1_BASE},
	{0x0, 0x0, 0x0, 0x0},
	{REG_GMPO_R1_BASE, REG_GRMGO_R1_BASE, REG_FLKO_R1_BASE, REG_PDO_R1_BASE},
	{REG_AWBO_R1_BASE, REG_AWBO_R2_BASE, REG_AEHO_R1_BASE, REG_AEDO_R1_BASE},
	{REG_DFLKO_R1_BASE, REG_DFLKBO_R1_BASE, REG_LTMSTO_R1_BASE, REG_LTMSTO_R2_BASE},
	{REG_TSFSO_R1_BASE, REG_TSFSO_R2_BASE, REG_TSFSO_R3_BASE, REG_TSFSO_R4_BASE},
};

static struct dma_group yuv_dma_group[SMI_PORT_YUV_NUM] = {
	/* Larb 17/45/46 - 9 */
	/* WDNA, port 0~7 */
	{REG_YUVO_R1_BASE, 0x0, 0x0, 0x0},
	{REG_YUVBO_R1_BASE, REG_YUVCO_R1_BASE, REG_YUVDO_R1_BASE, 0x0},
	{REG_YUVO_R3_BASE, 0x0, 0x0, 0x0},
	{REG_YUVBO_R3_BASE, REG_YUVCO_R3_BASE, REG_YUVDO_R3_BASE, 0x0},
	{REG_YUVO_R2_BASE, REG_YUVBO_R2_BASE, 0x0, 0x0},
	{REG_DRZH2NO_R1_BASE, REG_DRZH2NO_R8_BASE, REG_RZH1N2TO_R2_BASE, 0x0},
	{REG_YUVO_R4_BASE, REG_YUVBO_R4_BASE, REG_TCYSO_R1_BASE, 0x0},
	{REG_DRZH1NO_R1_BASE, REG_DRZH1NBO_R1_BASE, REG_DRZS4NO_R3_BASE, 0x0},
};

static int query_raw_dma_group(int m4u_id, struct dma_group *group)
{
	if (m4u_id < ARRAY_SIZE(raw_dma_group))
		memcpy(group, &raw_dma_group[m4u_id], sizeof(struct dma_group));
	else
		pr_info("%s: %s: not supported: %d\n",
			__FILE__, __func__, m4u_id);

	return 0;
}

static int query_yuv_dma_group(int m4u_id, struct dma_group *group)
{
	if (m4u_id <  ARRAY_SIZE(yuv_dma_group))
		memcpy(group, &yuv_dma_group[m4u_id], sizeof(struct dma_group));
	else
		pr_info("%s: %s: not supported: %d\n",
			__FILE__, __func__, m4u_id);

	return 0;
}

static int query_caci_size(int w, int h, size_t *size)
{
	int blk_w, blk_h;

	blk_w = (w + MTK_CAM_CAC_BLK_SIZE - 1) / MTK_CAM_CAC_BLK_SIZE;
	blk_h = (h + MTK_CAM_CAC_BLK_SIZE - 1) / MTK_CAM_CAC_BLK_SIZE;
	if (size)
		*size = blk_w * blk_h * 4;
	return 0;
}

static int query_max_exp_support(u32 raw_idx)
{
	// raw_idx: {0,1,2...} = {RAW_A, RAW_B, RAW_C}
	return 2;
}

static struct reg_to_dump raw_dma_list[] = {
	/* RDMA */
	/* CQI_R1, CQI_R2, CQI_R3, CQI_R4 */
	ADD_DMA_ERR(RAWI_R2), ADD_DMA_ERR(UFDI_R2),
	ADD_DMA_ERR(RAWI_R3), ADD_DMA_ERR(UFDI_R3), ADD_DMA_ERR(AEDI_R1), /*ADD_DMA_ERR(STATDISI_R1),*/
	ADD_DMA_ERR(RAWI_R5), ADD_DMA_ERR(UFDI_R5),
	ADD_DMA_ERR(BPCI_R1), ADD_DMA_ERR(BPCI_R2), ADD_DMA_ERR(BPCI_R3), ADD_DMA_ERR(FPRI_R1),
	ADD_DMA_ERR(PDI_R1), ADD_DMA_ERR(CACI_R1), ADD_DMA_ERR(AEI_R1), ADD_DMA_ERR(GRMGI_R1),
	ADD_DMA_ERR(LSCI_R1), ADD_DMA_ERR(LTMSCTI_R1), ADD_DMA_ERR(LTMSTI_R1), ADD_DMA_ERR(LTMSTI_R2),
	ADD_DMA_ERR(TCYSI_R1), ADD_DMA_ERR(AEHI_R1),
	/* WDMA */
	ADD_DMA_ERR(IMGO_R1), ADD_DMA_ERR(UFEO_R1),
	ADD_DMA_ERR(DRZB2NO_R1), ADD_DMA_ERR(AFO_R1),
	ADD_DMA_ERR(FHO_R1), ADD_DMA_ERR(STATCOLO_R1),
	ADD_DMA_ERR(AEO_R1), ADD_DMA_ERR(LTMSBO_R1), ADD_DMA_ERR(LTMSGO_R1),
	ADD_DMA_ERR(GMPO_R1), ADD_DMA_ERR(GRMGO_R1), ADD_DMA_ERR(FLKO_R1), ADD_DMA_ERR(PDO_R1),
	ADD_DMA_ERR(AWBO_R1), ADD_DMA_ERR(AWBO_R2), ADD_DMA_ERR(AEHO_R1), ADD_DMA_ERR(AEDO_R1),
	ADD_DMA_ERR(DFLKO_R1), ADD_DMA_ERR(DFLKBO_R1), ADD_DMA_ERR(LTMSTO_R1), ADD_DMA_ERR(LTMSTO_R2),
	ADD_DMA_ERR(TSFSO_R1), ADD_DMA_ERR(TSFSO_R2), ADD_DMA_ERR(TSFSO_R3), ADD_DMA_ERR(TSFSO_R4),
};

static int query_raw_dma_list(size_t *num, struct reg_to_dump **reg_list)
{
	*num = ARRAY_SIZE(raw_dma_list);
	*reg_list = raw_dma_list;
	return 0;
}

static struct reg_to_dump yuv_dma_list[] = {
	/* WDMA */
	ADD_DMA_ERR(YUVO_R1),
	ADD_DMA_ERR(YUVBO_R1), ADD_DMA_ERR(YUVCO_R1), ADD_DMA_ERR(YUVDO_R1),
	ADD_DMA_ERR(YUVO_R3),
	ADD_DMA_ERR(YUVBO_R3), ADD_DMA_ERR(YUVCO_R3), ADD_DMA_ERR(YUVDO_R3),
	ADD_DMA_ERR(YUVO_R2), ADD_DMA_ERR(YUVBO_R2),
	ADD_DMA_ERR(DRZH2NO_R1), ADD_DMA_ERR(DRZH2NO_R8), ADD_DMA_ERR(RZH1N2TO_R2),
	ADD_DMA_ERR(YUVO_R4), ADD_DMA_ERR(YUVBO_R4), ADD_DMA_ERR(TCYSO_R1),
	ADD_DMA_ERR(DRZH1NO_R1), ADD_DMA_ERR(DRZH1NBO_R1), ADD_DMA_ERR(DRZS4NO_R3),
};

static int query_yuv_dma_list(size_t *num, struct reg_to_dump **reg_list)
{
	*num = ARRAY_SIZE(yuv_dma_list);
	*reg_list = yuv_dma_list;
	return 0;
}

static int module_base[] = {
	[CAM_VCORE]     = 0x3C80D000,
	[CAM_MAIN_RAWA] = 0x3a7d0000,
	[CAM_MAIN_RAWB] = 0x3a9d0000,
	[CAM_MAIN_RAWC] = 0x3abd0000,
	[CAM_MAIN_RMSA] = 0x3a7f0000,
	[CAM_MAIN_RMSB] = 0x3a9f0000,
	[CAM_MAIN_RMSC] = 0x3abf0000,
	[CAM_MAIN_YUVA] = 0x3a810000,
	[CAM_MAIN_YUVB] = 0x3aa10000,
	[CAM_MAIN_YUVC] = 0x3ac10000,
	[PM_RAWA] = 0x3c813000,
	[PM_RAWB] = 0x3c814000,
	[PM_RAWC] = 0x3c815000,
	[PM_RMSA] = 0x3c816000,
	[PM_RMSB] = 0x3c817000,
	[PM_RMSC] = 0x3c818000,
};

static int query_module_base(int module_id, int *module_base_addr)
{
	*module_base_addr = module_base[module_id];
	return 0;
}

static u8 vb2_queues_support_list[] = {
	/* capture queues */
	MTK_RAW_MAIN_STREAM_OUT,
	MTK_RAW_PURE_RAW_OUT,
	MTK_RAW_YUVO_1_OUT,
	MTK_RAW_YUVO_2_OUT,
	MTK_RAW_YUVO_3_OUT,
	MTK_RAW_YUVO_4_OUT,
	MTK_RAW_DRZH2NO_1_OUT,
	MTK_RAW_DRZS4NO_3_OUT,
	MTK_RAW_DRZH1NO_1_OUT,
	MTK_RAW_RZH1N2TO_2_OUT,
	MTK_RAW_DRZB2NO_1_OUT,
	MTK_RAW_IPU_OUT,
	MTK_RAW_SV_BIN_IMGO_OUT,
	MTK_RAW_META_OUT_0,
	MTK_RAW_META_OUT_1,
	MTK_RAW_META_SV_OUT_0,
	MTK_RAW_META_GMPO_OUT,
	MTK_RAW_META_GRMGO_OUT,
	/* output queues*/
	MTK_RAW_META_IN,
	MTK_RAW_RAWI_2_IN,
	MTK_RAW_GRMGI_IN,
};

static const struct plat_v4l2_data mt6993_v4l2_data = {
	.raw_pipeline_num = 3,
	.camsv_pipeline_num = 12,
	.mraw_pipeline_num = 12,

	.meta_major = MTK_CAM_META_VERSION_MAJOR,
	.meta_minor = MTK_CAM_META_VERSION_MINOR,

	.meta_cfg_size = RAW_STATS_CFG_SIZE,
	.meta_stats0_size = RAW_STATS_0_SIZE,
	.meta_stats0_header_size = RAW_STAT_0_STRUCT_SIZE,
	.meta_stats1_size = RAW_STATS_1_SIZE,
	.meta_stats1_header_size = RAW_STATS_1_SIZE_HEADER,
	.meta_sv_ext_size = SV_STATS_0_SIZE,
	.meta_mraw_ext_size = MRAW_STATS_0_SIZE,
	.meta_pda_ext_size = PDA_STATS_0_SIZE,

	.timestamp_buffer_ofst = offsetof(struct mtk_cam_uapi_meta_raw_stats_0,
					  timestamp),
	.shading_tbl_ofst = 0x4468,
	.reserved_camsv_dev_id = 3,

	.raw_tg_pixelmode = 4,

	.vb2_queues_support_list = vb2_queues_support_list,
	.vb2_queues_support_list_num = ARRAY_SIZE(vb2_queues_support_list),

	.set_meta_stats_info = set_meta_stats_info,
	.get_meta_stats_port_size = get_meta_stats_port_size,
	.get_ltmsgo_freerun_need_copy = get_ltms_free_run,
	.ltmsgo_buffer_ofst = get_ltmsgo_offset,
	.set_sv_meta_stats_info = set_sv_meta_stats_info,
	.get_sv_dma_th_setting = get_sv_dma_th_setting,
	.get_sv_fifo_core_setting = get_sv_fifo_core_setting,
	.get_sv_max_pixel_mode = get_sv_max_pixel_mode,
	.get_is_smmu_enabled = get_is_smmu_enabled,
	.get_sv_smi_setting = get_sv_smi_setting,
	.get_single_sv_opp_idx = get_single_sv_opp_idx,
	.get_mraw_dmao_common_setting = get_mraw_dmao_common_setting,
	.set_mraw_meta_stats_info = set_mraw_meta_stats_info,
	.set_pda_status = set_pda_status,
	.get_pda_idx = get_pda_idx,
	.get_mraw_stats_cfg_param = get_mraw_stats_cfg_param,
	.get_raw_lock_sel_addr = get_raw_lock_sel_addr,
};

static const struct plat_data_hw mt6993_hw_data = {
	.cammux_id_raw_start = 6,  /* TBC(AY) */
	.raw_icc_path_num = SMI_PORT_RAW_NUM,
	.yuv_icc_path_num = SMI_PORT_YUV_NUM,
	.platform_id = 6993,
	/* SMI/DMA */
	.query_raw_dma_group = query_raw_dma_group,
	.query_yuv_dma_group = query_yuv_dma_group,
	.query_raw_dma_list = query_raw_dma_list,
	.query_yuv_dma_list = query_yuv_dma_list,

	.query_caci_size = query_caci_size,
	.query_max_exp_support = query_max_exp_support,
	.query_module_base = query_module_base,
	.dcif_slb_support = false,
	.bwr_support = true,
	.qof_support = true,
	.qos_remap_support = true,
	.dvc_support = true,
	.fmon_support = true,
	.max_main_pipe_w = 6632,
	.max_main_pipe_twin_w = 6200,
	.pixel_mode_max = 2,
	.has_pixel_mode_contraints = true,
	.default_opp_freq_hz = 564000000,
	.default_opp_volt_uv = 700000,
};

struct camsys_platform_data mt6993_data = {
	.platform = "mt6993",
	.v4l2 = &mt6993_v4l2_data,
	.hw = &mt6993_hw_data,
};
