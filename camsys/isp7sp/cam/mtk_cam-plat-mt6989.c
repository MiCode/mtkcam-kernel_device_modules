// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/module.h>

#include "mtk_cam-plat.h"
#include "mtk_cam-raw_regs.h"
#include "mtk_cam-meta-mt6989.h"
#include "mtk_cam-ipi.h"
#include "mtk_camera-v4l2-controls-7sp.h"
#include "mtk_camera-videodev2.h"
#include "mtk_cam-dvfs_qos_raw.h"

#define RAW_STATS_CFG_SIZE \
	ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_cfg), SZ_4K)

#define RAW_STATS_CFG_SIZE_RGBW \
	ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_rgbw_cfg), SZ_4K)

#define RAW_STAT_0_BUF_SIZE_STATIC \
			(MTK_CAM_UAPI_AAO_MAX_BUF_SIZE + \
			MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE + \
			MTK_CAM_UAPI_LTMSO_SIZE + \
			MTK_CAM_UAPI_LTMSHO_SIZE + \
			MTK_CAM_UAPI_TSFSO_SIZE + \
			MTK_CAM_UAPI_TCYSO_SIZE)

#define RAW_STAT_0_STRUCT_SIZE \
	ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_0), SZ_4K)

/* meta out max size include 1k meta info and dma buffer size */
#define RAW_STATS_0_SIZE \
	(RAW_STAT_0_STRUCT_SIZE + \
	 RAW_STAT_0_BUF_SIZE_STATIC)

#define RAW_STATS_0_SIZE_RGBW \
	(RAW_STAT_0_STRUCT_SIZE + \
	 RAW_STAT_0_BUF_SIZE_STATIC * 2)

#define RAW_STATS_1_SIZE_HEADER \
	ALIGN(sizeof(struct mtk_cam_uapi_meta_raw_stats_1), SZ_4K)

#define RAW_STATS_1_SIZE \
	(RAW_STATS_1_SIZE_HEADER + MTK_CAM_UAPI_AFO_MAX_BUF_SIZE)

#define RAW_STATS_1_SIZE_RGBW \
	(RAW_STATS_1_SIZE_HEADER + MTK_CAM_UAPI_AFO_MAX_BUF_SIZE * 2)

#define SV_STATS_0_SIZE \
	sizeof(struct mtk_cam_uapi_meta_camsv_stats_0)

#define MRAW_STATS_0_SIZE \
	sizeof(struct mtk_cam_uapi_meta_mraw_stats_0)

#define DYNAMIC_SIZE

enum RAW_ICC_PATH_IDX {
	ICC_PATH_CQI_R1 = 0,
	ICC_PATH_RAWI_R2,
	ICC_PATH_RAWI_R3,
	ICC_PATH_RAWI_R5,
	ICC_PATH_IMGO_R1,
	ICC_PATH_FPRI_R1,
	ICC_PATH_BPCI_R1,
	ICC_PATH_BPCI_R4,
	ICC_PATH_LSCI_R1,
	ICC_PATH_UFEO_R1,
	ICC_PATH_LTMSO_R1,
	ICC_PATH_DRZB2NO_R1,
	ICC_PATH_AFO_R1,
	ICC_PATH_AAO_R1,
	RAW_ICC_PATH_NUM,
};

enum YUV_ICC_PATH_IDX {
	ICC_PATH_YUVO_R1 = 0,
	ICC_PATH_YUVO_R3,
	ICC_PATH_YUVO_R2,
	ICC_PATH_YUVO_R5,
	ICC_PATH_RGBWI_R1,
	ICC_PATH_TSYSO_R1,
	ICC_PATH_DRZHNO_R3,
	YUV_ICC_PATH_NUM,
};

static void set_payload(struct mtk_cam_uapi_meta_hw_buf *buf,
			unsigned int size, size_t *offset)
{
	buf->offset = *offset;
	buf->size = size;
	*offset += size;
}

static void meta_state0_reset_all(struct mtk_cam_uapi_meta_raw_stats_0 *stats)
{
	size_t offset = sizeof(*stats);

	set_payload(&stats->ae_awb_stats.aao_buf, 0, &offset);
	set_payload(&stats->ae_awb_stats.aaho_buf, 0, &offset);
	set_payload(&stats->ltm_stats.ltmso_buf, 0, &offset);
	set_payload(&stats->ltm_stats.ltmsho_buf, 0, &offset);
	set_payload(&stats->flk_stats.flko_buf, 0, &offset);
	set_payload(&stats->tsf_stats.tsfo_r1_buf, 0, &offset);
	set_payload(&stats->tcys_stats.tcyso_buf, 0, &offset);
	set_payload(&stats->pde_stats.pdo_buf, 0, &offset);

	set_payload(&stats->ae_awb_stats_w.aao_buf, 0, &offset);
	set_payload(&stats->ae_awb_stats_w.aaho_buf, 0, &offset);
	set_payload(&stats->ltm_stats_w.ltmso_buf, 0, &offset);
	set_payload(&stats->ltm_stats_w.ltmsho_buf, 0, &offset);
	set_payload(&stats->flk_stats_w.flko_buf, 0, &offset);
	set_payload(&stats->tsf_stats_w.tsfo_r1_buf, 0, &offset);
	set_payload(&stats->tcys_stats_w.tcyso_buf, 0, &offset);
	set_payload(&stats->pde_stats_w.pdo_buf, 0, &offset);
}

static int set_meta_stat0_info(struct mtk_cam_uapi_meta_raw_stats_0 *stats,
			       size_t size,
			       const struct set_meta_stats_info_param *p)
{
	struct mtk_cam_uapi_meta_raw_stats_cfg *cfg = p->meta_cfg;
	size_t offset = sizeof(*stats);
	unsigned int flko_size;
	unsigned int pdo_size;

	if (!p->meta_cfg || !p->meta_cfg_size) {
		meta_state0_reset_all(stats);
		return 0;
	}

#ifdef DYNAMIC_SIZE
	flko_size = (p->height / p->bin_ratio) *
		MTK_CAM_UAPI_FLK_BLK_SIZE * MTK_CAM_UAPI_FLK_MAX_STAT_BLK_NUM;
#else
	flko_size = MTK_CAM_UAPI_FLK_MAX_BUF_SIZE;
#endif
	pdo_size = cfg->pde_enable ? cfg->pde_param.pdo_max_size : 0;

	set_payload(&stats->ae_awb_stats.aao_buf,
		    MTK_CAM_UAPI_AAO_MAX_BUF_SIZE, &offset);
	set_payload(&stats->ae_awb_stats.aaho_buf,
		    (p->rgbw) ? MTK_CAM_UAPI_AAHO_HIST_SIZE :
				MTK_CAM_UAPI_AAHO_MAX_BUF_SIZE,
			&offset);
	set_payload(&stats->ltm_stats.ltmso_buf,
		    MTK_CAM_UAPI_LTMSO_SIZE, &offset);
	set_payload(&stats->ltm_stats.ltmsho_buf,
		    MTK_CAM_UAPI_LTMSHO_SIZE, &offset);
	set_payload(&stats->flk_stats.flko_buf,
		    (p->rgbw) ? 0 : flko_size, &offset);
	set_payload(&stats->tsf_stats.tsfo_r1_buf,
		    MTK_CAM_UAPI_TSFSO_SIZE, &offset);
	set_payload(&stats->tcys_stats.tcyso_buf,
		    MTK_CAM_UAPI_TCYSO_SIZE, &offset);
	set_payload(&stats->pde_stats.pdo_buf, pdo_size, &offset);

	set_payload(&stats->ae_awb_stats_w.aao_buf,
				(p->rgbw) ? MTK_CAM_UAPI_AAO_MAX_BUF_SIZE : 0,
				&offset);
	set_payload(&stats->ae_awb_stats_w.aaho_buf,
				(p->rgbw) ? MTK_CAM_UAPI_AAHO_HIST_SIZE : 0,
				&offset);
	set_payload(&stats->ltm_stats_w.ltmso_buf,
				(p->rgbw) ? MTK_CAM_UAPI_LTMSO_SIZE : 0,
				&offset);
	set_payload(&stats->ltm_stats_w.ltmsho_buf,
				(p->rgbw) ? MTK_CAM_UAPI_LTMSHO_SIZE : 0,
				&offset);
	set_payload(&stats->flk_stats_w.flko_buf,
				(p->rgbw) ? flko_size : 0,
				&offset);
	set_payload(&stats->tsf_stats_w.tsfo_r1_buf,
				(p->rgbw) ? MTK_CAM_UAPI_TSFSO_SIZE : 0,
				&offset);
	set_payload(&stats->tcys_stats_w.tcyso_buf,
				(p->rgbw) ? MTK_CAM_UAPI_TCYSO_SIZE : 0,
				&offset);

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

	/* w part */
	set_payload(&stats->af_stats_w.afo_buf,
			(p->rgbw) ? MTK_CAM_UAPI_AFO_MAX_BUF_SIZE : 0,
			&offset);

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

static int get_meta_cfg_port_size(
	struct mtk_cam_uapi_meta_raw_stats_cfg *stats_cfg, int dma_port)
{
	switch (dma_port) {
	case PORT_CQI:
		return 0x10000;
	case PORT_BPCI:
		return stats_cfg->pde_param.pdi_max_size;
	case PORT_PDI:
		return stats_cfg->pde_param.pdi_max_size;
	case PORT_CACI:
		return stats_cfg->cac_param.caci_buf.size;
	default:
		pr_info("%s: %s: not supported: %d\n",
			__FILE__, __func__, dma_port);
	}
	return 0;
}

static int get_meta_stats0_port_size(
	struct mtk_cam_uapi_meta_raw_stats_0 *stats_0, int dma_port)
{
	switch (dma_port) {
	case PORT_AAO:
		return stats_0->ae_awb_stats.aao_buf.size;
	case PORT_AAHO:
		return stats_0->ae_awb_stats.aaho_buf.size;
	case PORT_TSFSO:
		return stats_0->tsf_stats.tsfo_r1_buf.size;
	case PORT_LTMSO:
		return stats_0->ltm_stats.ltmso_buf.size;
	case PORT_LTMSHO:
		return stats_0->ltm_stats.ltmsho_buf.size;
	case PORT_FLKO:
		return stats_0->flk_stats.flko_buf.size;
	case PORT_TCYSO:
		return stats_0->tcys_stats.tcyso_buf.size;
	case PORT_PDO:
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
	case PORT_AFO:
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
		return 0;
	case MTKCAM_IPI_RAW_META_STATS_0:
		*size = get_meta_stats0_port_size(addr, dma_port);
		return 0;
	case MTKCAM_IPI_RAW_META_STATS_1:
		*size = get_meta_stats1_port_size(addr, dma_port);
		return 0;
	default:
		pr_info("%s: %s: not supported: %d\n",
			__FILE__, __func__, ipi_id);
		return -1;
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

static int get_sv_two_smi_setting(int *sv_two_smi_en)
{
	*sv_two_smi_en = 1;

	return 0;
}

static int get_sv_dmao_common_setting(struct sv_dma_th_setting *sv_th_setting,
	struct sv_cq_th_setting *sv_cq_setting)
{
	sv_th_setting[CAMSV_0].urgent_th = 1<<31|FIFO_THRESHOLD(3412, 4/10, 3/10);
	sv_th_setting[CAMSV_0].ultra_th = 1<<28|FIFO_THRESHOLD(3412, 2/10, 1/10);
	sv_th_setting[CAMSV_0].pultra_th = 1<<28|FIFO_THRESHOLD(3412, 2/10, 1/10);
	sv_th_setting[CAMSV_0].dvfs_th = 1<<31|FIFO_THRESHOLD(3412, 1/10, 0);
	sv_th_setting[CAMSV_0].urgent_th2 = 1<<31|FIFO_THRESHOLD(2132, 4/10, 3/10);
	sv_th_setting[CAMSV_0].ultra_th2 = 1<<28|FIFO_THRESHOLD(2132, 2/10, 1/10);
	sv_th_setting[CAMSV_0].pultra_th2 = 1<<28|FIFO_THRESHOLD(2132, 2/10, 1/10);
	sv_th_setting[CAMSV_0].dvfs_th2 = 1<<31|FIFO_THRESHOLD(2132, 1/10, 0);
	sv_th_setting[CAMSV_0].urgent_len1_th = 1<<31|FIFO_THRESHOLD(128, 4/10, 3/10);
	sv_th_setting[CAMSV_0].ultra_len1_th = 1<<28|FIFO_THRESHOLD(128, 2/10, 1/10);
	sv_th_setting[CAMSV_0].pultra_len1_th = 1<<28|FIFO_THRESHOLD(128, 2/10, 1/10);
	sv_th_setting[CAMSV_0].dvfs_len1_th = 1<<31|FIFO_THRESHOLD(128, 1/10, 0);
	sv_th_setting[CAMSV_0].urgent_len2_th = 1<<31|FIFO_THRESHOLD(64, 4/10, 3/10);
	sv_th_setting[CAMSV_0].ultra_len2_th = 1<<28|FIFO_THRESHOLD(64, 2/10, 1/10);
	sv_th_setting[CAMSV_0].pultra_len2_th = 1<<28|FIFO_THRESHOLD(64, 2/10, 1/10);
	sv_th_setting[CAMSV_0].dvfs_len2_th = 1<<31|FIFO_THRESHOLD(64, 1/10, 0);

	sv_th_setting[CAMSV_1].urgent_th = 1<<31|FIFO_THRESHOLD(3412, 4/10, 3/10);
	sv_th_setting[CAMSV_1].ultra_th = 1<<28|FIFO_THRESHOLD(3412, 2/10, 1/10);
	sv_th_setting[CAMSV_1].pultra_th = 1<<28|FIFO_THRESHOLD(3412, 2/10, 1/10);
	sv_th_setting[CAMSV_1].dvfs_th = 1<<31|FIFO_THRESHOLD(3412, 1/10, 0);
	sv_th_setting[CAMSV_1].urgent_th2 = 1<<31|FIFO_THRESHOLD(2132, 4/10, 3/10);
	sv_th_setting[CAMSV_1].ultra_th2 = 1<<28|FIFO_THRESHOLD(2132, 2/10, 1/10);
	sv_th_setting[CAMSV_1].pultra_th2 = 1<<28|FIFO_THRESHOLD(2132, 2/10, 1/10);
	sv_th_setting[CAMSV_1].dvfs_th2 = 1<<31|FIFO_THRESHOLD(2132, 1/10, 0);
	sv_th_setting[CAMSV_1].urgent_len1_th = 1<<31|FIFO_THRESHOLD(128, 4/10, 3/10);
	sv_th_setting[CAMSV_1].ultra_len1_th = 1<<28|FIFO_THRESHOLD(128, 2/10, 1/10);
	sv_th_setting[CAMSV_1].pultra_len1_th = 1<<28|FIFO_THRESHOLD(128, 2/10, 1/10);
	sv_th_setting[CAMSV_1].dvfs_len1_th = 1<<31|FIFO_THRESHOLD(128, 1/10, 0);
	sv_th_setting[CAMSV_1].urgent_len2_th = 1<<31|FIFO_THRESHOLD(64, 4/10, 3/10);
	sv_th_setting[CAMSV_1].ultra_len2_th = 1<<28|FIFO_THRESHOLD(64, 2/10, 1/10);
	sv_th_setting[CAMSV_1].pultra_len2_th = 1<<28|FIFO_THRESHOLD(64, 2/10, 1/10);
	sv_th_setting[CAMSV_1].dvfs_len2_th = 1<<31|FIFO_THRESHOLD(64, 1/10, 0);

	sv_th_setting[CAMSV_2].urgent_th = 1<<31|FIFO_THRESHOLD(2560, 4/10, 3/10);
	sv_th_setting[CAMSV_2].ultra_th = 1<<28|FIFO_THRESHOLD(2560, 2/10, 1/10);
	sv_th_setting[CAMSV_2].pultra_th = 1<<28|FIFO_THRESHOLD(2560, 2/10, 1/10);
	sv_th_setting[CAMSV_2].dvfs_th = 1<<31|FIFO_THRESHOLD(2560, 1/10, 0);
	sv_th_setting[CAMSV_2].urgent_len1_th = 1<<31|FIFO_THRESHOLD(128, 4/10, 3/10);
	sv_th_setting[CAMSV_2].ultra_len1_th = 1<<28|FIFO_THRESHOLD(128, 2/10, 1/10);
	sv_th_setting[CAMSV_2].pultra_len1_th = 1<<28|FIFO_THRESHOLD(128, 2/10, 1/10);
	sv_th_setting[CAMSV_2].dvfs_len1_th = 1<<31|FIFO_THRESHOLD(128, 1/10, 0);

	sv_th_setting[CAMSV_3].urgent_th = 1<<31|FIFO_THRESHOLD(1600, 4/10, 3/10);
	sv_th_setting[CAMSV_3].ultra_th = 1<<28|FIFO_THRESHOLD(1600, 2/10, 1/10);
	sv_th_setting[CAMSV_3].pultra_th = 1<<28|FIFO_THRESHOLD(1600, 2/10, 1/10);;
	sv_th_setting[CAMSV_3].dvfs_th = 1<<31|FIFO_THRESHOLD(1600, 1/10, 0);
	sv_th_setting[CAMSV_3].urgent_len1_th = 1<<31|FIFO_THRESHOLD(64, 4/10, 3/10);
	sv_th_setting[CAMSV_3].ultra_len1_th = 1<<28|FIFO_THRESHOLD(64, 2/10, 1/10);
	sv_th_setting[CAMSV_3].pultra_len1_th = 1<<28|FIFO_THRESHOLD(64, 2/10, 1/10);
	sv_th_setting[CAMSV_3].dvfs_len1_th = 1<<31|FIFO_THRESHOLD(64, 1/10, 0);

	sv_th_setting[CAMSV_4].urgent_th = 1<<31|FIFO_THRESHOLD(440, 4/10, 3/10);
	sv_th_setting[CAMSV_4].ultra_th = 1<<28|FIFO_THRESHOLD(440, 2/10, 1/10);
	sv_th_setting[CAMSV_4].pultra_th = 1<<28|FIFO_THRESHOLD(440, 2/10, 1/10);
	sv_th_setting[CAMSV_4].dvfs_th = 1<<31|FIFO_THRESHOLD(440, 1/10, 0);

	sv_th_setting[CAMSV_5].urgent_th = 1<<31|FIFO_THRESHOLD(440, 4/10, 3/10);
	sv_th_setting[CAMSV_5].ultra_th = 1<<28|FIFO_THRESHOLD(440, 2/10, 1/10);
	sv_th_setting[CAMSV_5].pultra_th = 1<<28|FIFO_THRESHOLD(440, 2/10, 1/10);
	sv_th_setting[CAMSV_5].dvfs_th = 1<<31|FIFO_THRESHOLD(440, 1/10, 0);

	sv_cq_setting->cq1_fifo_size = (0x10 << 24) | 64;
	sv_cq_setting->cq1_urgent_th = 1<<31|FIFO_THRESHOLD(64, 4/10, 3/10);
	sv_cq_setting->cq1_ultra_th = 1<<28|FIFO_THRESHOLD(64, 2/10, 1/10);
	sv_cq_setting->cq1_pultra_th = 1<<28|FIFO_THRESHOLD(64, 2/10, 1/10);
	sv_cq_setting->cq1_dvfs_th = 1<<31|FIFO_THRESHOLD(64, 1/10, 0);

	sv_cq_setting->cq2_fifo_size = (0x10 << 24) | 64;
	sv_cq_setting->cq2_urgent_th = 1<<31|FIFO_THRESHOLD(64, 4/10, 3/10);
	sv_cq_setting->cq2_ultra_th = 1<<28|FIFO_THRESHOLD(64, 2/10, 1/10);
	sv_cq_setting->cq2_pultra_th = 1<<28|FIFO_THRESHOLD(64, 2/10, 1/10);
	sv_cq_setting->cq2_dvfs_th = 1<<31|FIFO_THRESHOLD(64, 1/10, 0);
	return 0;
}

static int get_mraw_dmao_common_setting(struct mraw_dma_th_setting *mraw_th_setting,
	struct mraw_cq_th_setting *mraw_cq_setting)
{
	mraw_th_setting[imgo_m1].urgent_th = 1<<31|FIFO_THRESHOLD(472, 6/10, 5/10);
	mraw_th_setting[imgo_m1].ultra_th = 1<<28|FIFO_THRESHOLD(472, 4/10, 3/10);
	mraw_th_setting[imgo_m1].pultra_th = 1<<28|FIFO_THRESHOLD(472, 2/10, 1/10);
	mraw_th_setting[imgo_m1].dvfs_th = 1<<31|FIFO_THRESHOLD(472, 1/10, 0);
	mraw_th_setting[imgo_m1].fifo_size = (0x10 << 24) | 472;

	mraw_th_setting[imgbo_m1].urgent_th = 1<<31|FIFO_THRESHOLD(376, 6/10, 5/10);
	mraw_th_setting[imgbo_m1].ultra_th = 1<<28|FIFO_THRESHOLD(376, 4/10, 3/10);
	mraw_th_setting[imgbo_m1].pultra_th = 1<<28|FIFO_THRESHOLD(376, 2/10, 1/10);
	mraw_th_setting[imgbo_m1].dvfs_th = 1<<31|FIFO_THRESHOLD(376, 1/10, 0);
	mraw_th_setting[imgbo_m1].fifo_size = (0x10 << 24) | 376;

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

static int set_mraw_meta_stats_info(
	int ipi_id, void *addr, struct dma_info *info)
{
	struct mtk_cam_uapi_meta_mraw_stats_0 *mraw_stats0;
	unsigned long offset;
	unsigned int size;

	switch (ipi_id) {
	case MTKCAM_IPI_MRAW_META_STATS_0:
		mraw_stats0 = (struct mtk_cam_uapi_meta_mraw_stats_0 *)addr;
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
		/* cpio */
		size = info[cpio_m1].stride * info[cpio_m1].height;
		/* calculate offset for 16-alignment limitation */
		offset = ((((dma_addr_t)mraw_stats0 + offset + 15) >> 4) << 4)
			- (dma_addr_t)mraw_stats0;
		set_payload(&mraw_stats0->cpi_stats.cpio_buf, size, &offset);
		mraw_stats0->cpi_stats_enabled = 1;
		mraw_stats0->cpi_stats.stats_src.width = info[cpio_m1].width;
		mraw_stats0->cpi_stats.stats_src.height = info[cpio_m1].height;
		mraw_stats0->cpi_stats.stride = info[cpio_m1].stride;
		break;
	default:
		pr_info("%s: %s: not supported: %d\n",
			__FILE__, __func__, ipi_id);
		break;
	}

	return 0;
}

static int get_mraw_stats_cfg_param(
	void *addr, struct mraw_stats_cfg_param *param)
{
	struct mtk_cam_uapi_meta_mraw_stats_cfg *stats_cfg =
		(struct mtk_cam_uapi_meta_mraw_stats_cfg *)addr;

	param->mqe_en = stats_cfg->mqe_enable;
	param->mobc_en = stats_cfg->mobc_enable;
	param->plsc_en = stats_cfg->plsc_enable;
	param->dbg_en = stats_cfg->dbg_enable;
	param->lm_en = stats_cfg->lm_enable;

	param->crop_width = stats_cfg->crop_param.crop_x_end -
		stats_cfg->crop_param.crop_x_start;
	param->crop_height = stats_cfg->crop_param.crop_y_end -
		stats_cfg->crop_param.crop_y_start;

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

/* TODO: iommu debug */
#define RAW_M4U_PORT_NUM 17
#define YUV_M4U_PORT_NUM 8
#define DMA_GROUP_SIZE 4
static u32 raw_dma_group[RAW_M4U_PORT_NUM][DMA_GROUP_SIZE] = {
	/* port-0 */
	{0x0, 0x0, 0x0, 0x0},
	/* port-1 */
	{REG_RAWI_R2_BASE, REG_UFDI_R2_BASE, 0x0, 0x0},
	/* port-2 */
	{REG_RAWI_R3_BASE, REG_UFDI_R3_BASE, 0x0, 0x0},
	/* port-3 */
	{0x0, 0x0, 0x0, 0x0},
	/* port-4 */
	{REG_RAWI_R5_BASE, REG_UFDI_R5_BASE, 0x0, 0x0},
	/* port-5 */
	{REG_IMGO_R1_BASE, REG_FHO_R1_BASE, 0x0, 0x0},
	/* port-6 */
	{0x0, 0x0, 0x0, 0x0},
	/* port-7 */
	{REG_CACI_R1_BASE, 0x0, 0x0, 0x0},
	/* port-8 */
	{REG_BPCI_R1_BASE, REG_BPCI_R2_BASE, 0x0, 0x0},
	/* port-9 */
	{REG_BPCI_R3_BASE, 0x0, 0x0, 0x0},
	/* port-10 */
	{REG_LSCI_R1_BASE, REG_PDI_R1_BASE, REG_AAI_R1_BASE, 0x0},
	/* port-11 */
	{REG_UFEO_R1_BASE, REG_FLKO_R1_BASE, REG_PDO_R1_BASE, 0x0},
	/* port-12 */
	{REG_LTMSO_R1_BASE, REG_LTMSHO_R1_BASE, 0x0, 0x0},
	/* port-13  */
	{REG_DRZB2NO_R1_BASE, REG_DRZB2NBO_R1_BASE, REG_DRZB2NCO_R1_BASE, 0x0},
	/* port-14 */
	{0x0, 0x0, 0x0, 0x0},
	/* port-15 */
	{REG_AFO_R1_BASE, REG_TSFSO_R1_BASE, 0x0, 0x0},
	/* port-16 */
	{REG_AAO_R1_BASE, REG_AAHO_R1_BASE, 0x0, 0x0},
};

static u32 yuv_dma_group[YUV_M4U_PORT_NUM][DMA_GROUP_SIZE] = {
	/* port-0 */
	{REG_YUVO_R1_BASE, REG_YUVBO_R1_BASE, REG_YUVCO_R1_BASE, REG_YUVDO_R1_BASE},
	/* port-1 */
	{REG_YUVO_R3_BASE, REG_YUVBO_R3_BASE, REG_YUVCO_R3_BASE, REG_YUVDO_R3_BASE},
	/* port-2 */
	{REG_YUVO_R2_BASE, REG_YUVBO_R2_BASE, REG_YUVO_R4_BASE, REG_YUVBO_R4_BASE},
	/* port-3 */
	{REG_YUVO_R5_BASE, REG_YUVBO_R5_BASE, REG_RZH1N2TBO_R1_BASE, REG_RZH1N2TBO_R3_BASE},
	/* port-4 */
	{REG_RGBWI_R1_BASE, 0x0, 0x0, 0x0},
	/* port-5 */
	{0x0, 0x0, 0x0, 0x0},
	/* port-6 */
	{REG_TCYSO_R1_BASE, REG_RZH1N2TO_R2_BASE, REG_DRZS4NO_R1_BASE, REG_DRZH2NO_R8_BASE},
	/* port-7 */
	{REG_DRZS4NO_R3_BASE, REG_RZH1N2TO_R3_BASE, REG_RZH1N2TO_R1_BASE, 0x0},
};

static int query_raw_dma_group(int m4u_id, u32 group[4])
{
	if (m4u_id < RAW_M4U_PORT_NUM)
		memcpy(group, raw_dma_group[m4u_id], sizeof(u32)*4);
	else
		pr_info("%s: %s: not supported: %d\n",
			__FILE__, __func__, m4u_id);

	return 0;
}

static int query_yuv_dma_group(int m4u_id, u32 group[4])
{
	if (m4u_id < YUV_M4U_PORT_NUM)
		memcpy(group, yuv_dma_group[m4u_id], sizeof(u32)*4);
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
	// raw_idx: {1, 2, 3...} = {RAW_A, RAW_B, RAW_C ...}
	switch (raw_idx) {
	case 1:
	case 2:
		return 3;
	default:
		return 2;
	}
}

static int map_raw_icc_path(int smi_port)
{
	switch (smi_port) {
	case SMI_PORT_CQI_R1:
		return ICC_PATH_CQI_R1;
	case SMI_PORT_RAWI_R2:
		return ICC_PATH_RAWI_R2;
	case SMI_PORT_RAWI_R3:
		return ICC_PATH_RAWI_R3;
	case SMI_PORT_RAWI_R5:
		return ICC_PATH_RAWI_R5;
	case SMI_PORT_IMGO_R1:
		return ICC_PATH_IMGO_R1;
	case SMI_PORT_FPRI_R1:
		return ICC_PATH_FPRI_R1;
	case SMI_PORT_BPCI_R1:
		return ICC_PATH_BPCI_R1;
	case SMI_PORT_BPCI_R4:
		return ICC_PATH_BPCI_R4;
	case SMI_PORT_LSCI_R1:
		return ICC_PATH_LSCI_R1;
	case SMI_PORT_UFEO_R1:
		return ICC_PATH_UFEO_R1;
	case SMI_PORT_LTMSO_R1:
		return ICC_PATH_LTMSO_R1;
	case SMI_PORT_DRZB2NO_R1:
		return ICC_PATH_DRZB2NO_R1;
	case SMI_PORT_AFO_R1:
		return ICC_PATH_AFO_R1;
	case SMI_PORT_AAO_R1:
		return ICC_PATH_AAO_R1;
	default:
		return -1;
	}
}

static int map_yuv_icc_path(int smi_port)
{
	switch (smi_port) {
	case SMI_PORT_YUVO_R1:
		return ICC_PATH_YUVO_R1;
	case SMI_PORT_YUVO_R3:
		return ICC_PATH_YUVO_R3;
	case SMI_PORT_YUVO_R2:
		return ICC_PATH_YUVO_R2;
	case SMI_PORT_YUVO_R5:
		return ICC_PATH_YUVO_R5;
	case SMI_PORT_RGBWI_R1:
		return ICC_PATH_RGBWI_R1;
	case SMI_PORT_TCYSO_R1:
		return ICC_PATH_TSYSO_R1;
	case SMI_PORT_DRZHNO_R3:
		return ICC_PATH_DRZHNO_R3;
	default:
		return -1;
	}
}

static int query_icc_path_idx(int domain, int smi_port)
{
	if (domain == YUV_DOMAIN)
		return map_yuv_icc_path(smi_port);
	else
		return map_raw_icc_path(smi_port);
}

static const struct plat_v4l2_data mt6989_v4l2_data = {
	.raw_pipeline_num = 3,
	.camsv_pipeline_num = 16,
	.mraw_pipeline_num = 4,

	.meta_major = MTK_CAM_META_VERSION_MAJOR,
	.meta_minor = MTK_CAM_META_VERSION_MINOR,

	.meta_cfg_size = RAW_STATS_CFG_SIZE,
	.meta_cfg_size_rgbw = RAW_STATS_CFG_SIZE_RGBW,
	.meta_stats0_size = RAW_STATS_0_SIZE,
	.meta_stats0_size_rgbw = RAW_STATS_0_SIZE_RGBW,
	.meta_stats1_size = RAW_STATS_1_SIZE,
	.meta_stats1_size_rgbw = RAW_STATS_1_SIZE_RGBW,
	.meta_sv_ext_size = SV_STATS_0_SIZE,
	.meta_mraw_ext_size = MRAW_STATS_0_SIZE,

	.timestamp_buffer_ofst = offsetof(struct mtk_cam_uapi_meta_raw_stats_0,
					  timestamp),

	.reserved_camsv_dev_id = 3,

	.set_meta_stats_info = set_meta_stats_info,
	.get_meta_stats_port_size = get_meta_stats_port_size,

	.set_sv_meta_stats_info = set_sv_meta_stats_info,
	.get_sv_dmao_common_setting = get_sv_dmao_common_setting,
	.get_sv_two_smi_setting = get_sv_two_smi_setting,
	.get_mraw_dmao_common_setting = get_mraw_dmao_common_setting,
	.set_mraw_meta_stats_info = set_mraw_meta_stats_info,
	.get_mraw_stats_cfg_param = get_mraw_stats_cfg_param,
};

static const struct plat_data_hw mt6989_hw_data = {
	.cammux_id_raw_start = 34,  /* TBC(AY) */
	.raw_icc_path_num = RAW_ICC_PATH_NUM,
	.yuv_icc_path_num = YUV_ICC_PATH_NUM,
	.platform_id = 6989,
	.query_raw_dma_group = query_raw_dma_group,
	.query_yuv_dma_group = query_yuv_dma_group,
	.query_caci_size = query_caci_size,
	.query_max_exp_support = query_max_exp_support,
	.query_icc_path_idx = query_icc_path_idx,
	.dcif_slb_support = true,
};

struct camsys_platform_data mt6989_data = {
	.platform = "mt6989",
	.v4l2 = &mt6989_v4l2_data,
	.hw = &mt6989_hw_data,
};
