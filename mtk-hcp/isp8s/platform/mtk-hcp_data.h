/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef _MTK_HCP_ISP8S_PLATFORM_MTK_HCP_DATA_H_
#define _MTK_HCP_ISP8S_PLATFORM_MTK_HCP_DATA_H_

#include <linux/device.h>
#include <linux/types.h>

#define HCP_MB_CACHE_OFF     (0)
#define HCP_MB_CACHE_ON      (1)
#define HCP_MB_CACHE_ACP     (2)

/* forward declaration */
struct mtk_hcp_rsv_mb;

struct mtk_hcp_rsv_mb_cfg  {
	const char *name;
	u64 size;
	unsigned int id;
	u8 cache_mode;
	u8 rsv[3];
};

typedef int (*mtk_hcp_rsv_mb_op) (struct mtk_hcp_rsv_mb *mb);

struct mtk_hcp_rsv_mb {
	struct mtk_hcp_rsv_mb_cfg cfg;
	struct kref kref;
	struct iosys_map map;
	struct dma_buf *d_buf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	int fd;
	u64 start_phys;
	u64 start_dma;
	void *start_virt;
	void *mem_priv;
	mtk_hcp_rsv_mb_op alloc;
	mtk_hcp_rsv_mb_op free;
	mtk_hcp_rsv_mb_op get_ref;
	mtk_hcp_rsv_mb_op put_ref;
};


struct mtk_hcp_plat_op {
	struct mtk_hcp_rsv_mb *(*fetch_mb)(
		unsigned int mb_id);
	struct mtk_hcp_rsv_mb *(*fetch_mod_mb)(
		unsigned int mem_mode,
		unsigned int mod_id,
		unsigned int mb_type);
	struct mtk_hcp_rsv_mb *(*fetch_gce_mb)(
		unsigned int mem_mode);
	struct mtk_hcp_rsv_mb *(*fetch_gce_clr_token_mb)(
		unsigned int mem_mode);

	/* set device for cacheable memory */
	void (*set_c_dev)(struct device *dev);
	/* set device for non-cacheable memory */
	void (*set_nc_dev)(struct device *dev);
	/* set device for acp cacheable memory */
	void (*set_acp_dev)(struct device *dev);
};



#endif /* _MTK_HCP_ISP8S_PLATFORM_MTK_HCP_DATA_H_ */
