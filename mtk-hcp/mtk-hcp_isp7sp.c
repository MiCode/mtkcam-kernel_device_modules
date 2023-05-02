// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/slab.h>
#include <linux/kref.h>
#include <mtk_heap.h>
#include "mtk-hcp_isp7sp.h"

static struct mtk_hcp_reserve_mblock *mb;

enum isp7sp_rsv_mem_id_t {
	DIP_MEM_FOR_HW_ID,
	IMG_MEM_FOR_HW_ID = DIP_MEM_FOR_HW_ID, /*shared buffer for ipi_param*/
	/*need replace DIP_MEM_FOR_HW_ID & DIP_MEM_FOR_SW_ID*/
	WPE_MEM_C_ID,	/*module cq buffer*/
	WPE_MEM_T_ID,	/*module tdr buffer*/
	TRAW_MEM_C_ID,	/*module cq buffer*/
	TRAW_MEM_T_ID,	/*module tdr buffer*/
	DIP_MEM_C_ID,	/*module cq buffer*/
	DIP_MEM_T_ID,	/*module tdr buffer*/
	PQDIP_MEM_C_ID,	/*module cq buffer*/
	PQDIP_MEM_T_ID,	/*module tdr buffer*/
	ADL_MEM_C_ID,	/*module cq buffer*/
	ADL_MEM_T_ID,	/*module tdr buffer*/
	IMG_MEM_G_ID,	/*gce cmd buffer*/
	NUMS_MEM_ID,
};

static struct mtk_hcp_reserve_mblock isp7sp_smvr_mblock[] = {
	{
		/*share buffer for frame setting, to be sw usage*/
		.name = "IMG_MEM_FOR_HW_ID",
		.num = IMG_MEM_FOR_HW_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x400000,   /*need more than 4MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "WPE_MEM_C_ID",
		.num = WPE_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x19B000,   /*1680132 bytes*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "WPE_MEM_T_ID",
		.num = WPE_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x233000,  /*2305536 bytes*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "TRAW_MEM_C_ID",
		.num = TRAW_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x71F000,  /*7463736 bytes*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "TRAW_MEM_T_ID",
		.num = TRAW_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x16F4000,  /*24065056 bytes*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "DIP_MEM_C_ID",
		.num = DIP_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0xA91000,  /*11077920 bytes*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "DIP_MEM_T_ID",
		.num = DIP_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x17D5000,  /*24987648 bytes*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "PQDIP_MEM_C_ID",
		.num = PQDIP_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x16E000,  /*1498320 bytes*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "PQDIP_MEM_T_ID",
		.num = PQDIP_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x11E000,  /*1169408 bytes - use 6897 size*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "ADL_MEM_C_ID",
		.num = ADL_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x100000,
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL
	},
	{
		.name = "ADL_MEM_T_ID",
		.num = ADL_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x200000,
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL
	},
	{
		.name = "IMG_MEM_G_ID",
		.num = IMG_MEM_G_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		/* align with isp7s for newp2 change */
		.size = 0x1F45E00, //0x1F21E00, //0x1885E00,//0x17F5E00, //0x3400000
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
};


struct mtk_hcp_reserve_mblock isp7sp_reserve_mblock[] = {
	{
		/*share buffer for frame setting, to be sw usage*/
		.name = "IMG_MEM_FOR_HW_ID",
		.num = IMG_MEM_FOR_HW_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x400000,   /*need more than 4MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "WPE_MEM_C_ID",
		.num = WPE_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x73000,   /*470532 bytes*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "WPE_MEM_T_ID",
		.num = WPE_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x9E000,   /*646656 bytes*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "TRAW_MEM_C_ID",
		.num = TRAW_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x380000,   /*3668616 bytes*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "TRAW_MEM_T_ID",
		.num = TRAW_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x1484000,   /*21509152 bytes*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "DIP_MEM_C_ID",
		.num = DIP_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x564000,   /*5652000 bytes*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "DIP_MEM_T_ID",
		.num = DIP_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x1463000,   /*21374976 bytes*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "PQDIP_MEM_C_ID",
		.num = PQDIP_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0xDC000,   /*898992 bytes*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "PQDIP_MEM_T_ID",
		.num = PQDIP_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0xAC000,   /*702464 bytes - use 6897 size*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
	{
		.name = "ADL_MEM_C_ID",
		.num = ADL_MEM_C_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x100000,   /*1MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL
	},
	{
		.name = "ADL_MEM_T_ID",
		.num = ADL_MEM_T_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		.size = 0x200000,   /*2MB*/
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL
	},
	{
		.name = "IMG_MEM_G_ID",
		.num = IMG_MEM_G_ID,
		.start_phys = 0x0,
		.start_virt = 0x0,
		.start_dma  = 0x0,
		/* align with isp7s for newp2 change */
		.size = 0x11D8400, //0x11CC400, //0xF98400,//0xF68400, //0x2000000,
		.is_dma_buf = true,
		.mmap_cnt = 0,
		.mem_priv = NULL,
		.d_buf = NULL,
		.fd = -1,
		.pIonHandle = NULL,
		.attach = NULL,
		.sgt = NULL
	},
};

phys_addr_t isp7sp_get_reserve_mem_phys(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[HCP] no reserve memory for %d", id);
		return 0;
	} else {
		return mb[id].start_phys;
	}
}
EXPORT_SYMBOL(isp7sp_get_reserve_mem_phys);

void *isp7sp_get_reserve_mem_virt(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[HCP] no reserve memory for %u", id);
		return 0;
	} else
		return mb[id].start_virt;
}
EXPORT_SYMBOL(isp7sp_get_reserve_mem_virt);

phys_addr_t isp7sp_get_reserve_mem_dma(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[HCP] no reserve memory for %u", id);
		return 0;
	} else {
		return mb[id].start_dma;
	}
}
EXPORT_SYMBOL(isp7sp_get_reserve_mem_dma);

phys_addr_t isp7sp_get_reserve_mem_size(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[HCP] no reserve memory for %u", id);
		return 0;
	} else {
		return mb[id].size;
	}
}
EXPORT_SYMBOL(isp7sp_get_reserve_mem_size);

uint32_t isp7sp_get_reserve_mem_fd(unsigned int id)
{
	if (id >= NUMS_MEM_ID) {
		pr_info("[HCP] no reserve memory for %u", id);
		return 0;
	} else
		return mb[id].fd;
}
EXPORT_SYMBOL(isp7sp_get_reserve_mem_fd);

void *isp7sp_get_gce_virt(void)
{
	return mb[IMG_MEM_G_ID].start_virt;
}
EXPORT_SYMBOL(isp7sp_get_gce_virt);

void *isp7sp_get_wpe_virt(void)
{
	return mb[WPE_MEM_C_ID].start_virt;
}
EXPORT_SYMBOL(isp7sp_get_wpe_virt);

int isp7sp_get_wpe_cq_fd(void)
{
	return mb[WPE_MEM_C_ID].fd;
}
EXPORT_SYMBOL(isp7sp_get_wpe_cq_fd);

int isp7sp_get_wpe_tdr_fd(void)
{
	return mb[WPE_MEM_T_ID].fd;
}
EXPORT_SYMBOL(isp7sp_get_wpe_tdr_fd);

void *isp7sp_get_dip_virt(void)
{
	return mb[DIP_MEM_C_ID].start_virt;
}
EXPORT_SYMBOL(isp7sp_get_dip_virt);

int isp7sp_get_dip_cq_fd(void)
{
	return mb[DIP_MEM_C_ID].fd;
}
EXPORT_SYMBOL(isp7sp_get_dip_cq_fd);

int isp7sp_get_dip_tdr_fd(void)
{
	return mb[DIP_MEM_T_ID].fd;
}
EXPORT_SYMBOL(isp7sp_get_dip_tdr_fd);

void *isp7sp_get_traw_virt(void)
{
	return mb[TRAW_MEM_C_ID].start_virt;
}
EXPORT_SYMBOL(isp7sp_get_traw_virt);

int isp7sp_get_traw_cq_fd(void)
{
	return mb[TRAW_MEM_C_ID].fd;
}
EXPORT_SYMBOL(isp7sp_get_traw_cq_fd);

int isp7sp_get_traw_tdr_fd(void)
{
	return mb[TRAW_MEM_T_ID].fd;
}
EXPORT_SYMBOL(isp7sp_get_traw_tdr_fd);

void *isp7sp_get_pqdip_virt(void)
{
	return mb[PQDIP_MEM_C_ID].start_virt;
}
EXPORT_SYMBOL(isp7sp_get_pqdip_virt);

int isp7sp_get_pqdip_cq_fd(void)
{
	return mb[PQDIP_MEM_C_ID].fd;
}
EXPORT_SYMBOL(isp7sp_get_pqdip_cq_fd);

int isp7sp_get_pqdip_tdr_fd(void)
{
	return mb[PQDIP_MEM_T_ID].fd;
}
EXPORT_SYMBOL(isp7sp_get_pqdip_tdr_fd);
void *isp7sp_get_hwid_virt(void)
{
	return mb[DIP_MEM_FOR_HW_ID].start_virt;
}
EXPORT_SYMBOL(isp7sp_get_hwid_virt);


int isp7sp_allocate_working_buffer(struct mtk_hcp *hcp_dev, unsigned int mode)
{
	enum isp7sp_rsv_mem_id_t id = 0;
	struct mtk_hcp_reserve_mblock *mblock = NULL;
	unsigned int block_num = 0;
	struct sg_table *sgt = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct dma_heap *pdma_heap = NULL;
	struct iosys_map map = {0};
	int ret = 0;

	if (mode)
		mblock = hcp_dev->data->smblock;
	else
		mblock = hcp_dev->data->mblock;

	mb = mblock;
	block_num = hcp_dev->data->block_num;
	for (id = 0; id < block_num; id++) {
		if (mblock[id].is_dma_buf) {
			switch (id) {
			case IMG_MEM_FOR_HW_ID:
				/*allocated at probe via dts*/
				break;
			case IMG_MEM_G_ID:
				/* all supported heap name you can find with cmd */
				/* (ls /dev/dma_heap/) in shell */
				pdma_heap = dma_heap_find("mtk_mm");
				if (!pdma_heap) {
					pr_info("pdma_heap find fail\n");
					return -1;
				}
				mblock[id].d_buf = dma_heap_buffer_alloc(
					pdma_heap,
					mblock[id].size,
					O_RDWR | O_CLOEXEC,
					DMA_HEAP_VALID_HEAP_FLAGS);
				if (IS_ERR(mblock[id].d_buf)) {
					pr_info("dma_heap_buffer_alloc fail :%ld\n",
					PTR_ERR(mblock[id].d_buf));
					return -1;
				}
				mtk_dma_buf_set_name(mblock[id].d_buf, mblock[id].name);
				mblock[id].attach =
					dma_buf_attach(mblock[id].d_buf, hcp_dev->smmu_dev);
				attach = mblock[id].attach;
				if (IS_ERR(attach)) {
					pr_info("dma_buf_attach fail :%ld\n",
					PTR_ERR(attach));
					return -1;
				}

				mblock[id].sgt =
					dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
				sgt = mblock[id].sgt;
				if (IS_ERR(sgt)) {
					dma_buf_detach(mblock[id].d_buf, attach);
					pr_info("dma_buf_map_attachment fail sgt:%ld\n",
					PTR_ERR(sgt));
					return -1;
				}
				mblock[id].start_phys = sg_dma_address(sgt->sgl);
				mblock[id].start_dma = mblock[id].start_phys;
				ret = dma_buf_vmap(mblock[id].d_buf, &map);
				if (ret) {
					pr_info("sg_dma_address fail\n");
					return ret;
				}
				mblock[id].start_virt = (void *)map.vaddr;
				mblock[id].map = map;
				get_dma_buf(mblock[id].d_buf);
				mblock[id].fd =
					dma_buf_fd(mblock[id].d_buf, O_RDWR | O_CLOEXEC);
				dma_buf_begin_cpu_access(mblock[id].d_buf, DMA_BIDIRECTIONAL);
				kref_init(&mblock[id].kref);
				pr_info("%s:[HCP][%s] phys:0x%llx, virt:0x%p, dma:0x%llx, size:0x%llx, is_dma_buf:%d, fd:%d, d_buf:0x%p\n",
					__func__, mblock[id].name,
					isp7sp_get_reserve_mem_phys(id),
					isp7sp_get_reserve_mem_virt(id),
					isp7sp_get_reserve_mem_dma(id),
					isp7sp_get_reserve_mem_size(id),
					mblock[id].is_dma_buf,
					isp7sp_get_reserve_mem_fd(id),
					mblock[id].d_buf);
				break;
			case WPE_MEM_C_ID:
			case WPE_MEM_T_ID:
			case DIP_MEM_C_ID:
			case DIP_MEM_T_ID:
			case TRAW_MEM_C_ID:
			case TRAW_MEM_T_ID:
			case PQDIP_MEM_C_ID:
			case PQDIP_MEM_T_ID:
				/* all supported heap name you can find with cmd */
				/* (ls /dev/dma_heap/) in shell */
				pdma_heap = dma_heap_find("mtk_mm-uncached");
				if (!pdma_heap) {
					pr_info("pdma_heap find fail\n");
					return -1;
				}
				mblock[id].d_buf = dma_heap_buffer_alloc(
					pdma_heap,
					mblock[id].size, O_RDWR | O_CLOEXEC,
					DMA_HEAP_VALID_HEAP_FLAGS);
				if (IS_ERR(mblock[id].d_buf)) {
					pr_info("dma_heap_buffer_alloc fail :%ld\n",
					PTR_ERR(mblock[id].d_buf));
					return -1;
				}
				mtk_dma_buf_set_name(mblock[id].d_buf, mblock[id].name);
				mblock[id].attach = dma_buf_attach(
				mblock[id].d_buf, hcp_dev->smmu_dev);
				attach = mblock[id].attach;
				if (IS_ERR(attach)) {
					pr_info("dma_buf_attach fail :%ld\n",
					PTR_ERR(attach));
					return -1;
				}

				mblock[id].sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
				sgt = mblock[id].sgt;
				if (IS_ERR(sgt)) {
					dma_buf_detach(mblock[id].d_buf, attach);
					pr_info("dma_buf_map_attachment fail sgt:%ld\n",
					PTR_ERR(sgt));
					return -1;
				}
				mblock[id].start_phys = sg_dma_address(sgt->sgl);
				mblock[id].start_dma = mblock[id].start_phys;
				ret = dma_buf_vmap(mblock[id].d_buf, &map);
				if (ret) {
					pr_info("sg_dma_address fail\n");
					return ret;
				}
				mblock[id].start_virt = (void *)map.vaddr;
				mblock[id].map = map;
				get_dma_buf(mblock[id].d_buf);
				mblock[id].fd =
					dma_buf_fd(mblock[id].d_buf, O_RDWR | O_CLOEXEC);
				break;
			default:

				/* all supported heap name you can find with cmd */
				/* (ls /dev/dma_heap/) in shell */
				pdma_heap = dma_heap_find("mtk_mm-uncached");
				if (!pdma_heap) {
					pr_info("pdma_heap find fail\n");
					return -1;
				}
				mblock[id].d_buf = dma_heap_buffer_alloc(
					pdma_heap,
					mblock[id].size, O_RDWR | O_CLOEXEC,
					DMA_HEAP_VALID_HEAP_FLAGS);
				if (IS_ERR(mblock[id].d_buf)) {
					pr_info("dma_heap_buffer_alloc fail :%ld\n",
					PTR_ERR(mblock[id].d_buf));
					return -1;
				}
				mtk_dma_buf_set_name(mblock[id].d_buf, mblock[id].name);
				mblock[id].attach = dma_buf_attach(
				mblock[id].d_buf, hcp_dev->smmu_dev);
				attach = mblock[id].attach;
				if (IS_ERR(attach)) {
					pr_info("dma_buf_attach fail :%ld\n",
					PTR_ERR(attach));
					return -1;
				}

				mblock[id].sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
				sgt = mblock[id].sgt;
				if (IS_ERR(sgt)) {
					dma_buf_detach(mblock[id].d_buf, attach);
					pr_info("dma_buf_map_attachment fail sgt:%ld\n",
					PTR_ERR(sgt));
					return -1;
				}
				mblock[id].start_phys = sg_dma_address(sgt->sgl);
				mblock[id].start_dma = mblock[id].start_phys;
				ret = dma_buf_vmap(mblock[id].d_buf, &map);
				if (ret) {
					pr_info("sg_dma_address fail\n");
					return ret;
				}
				mblock[id].start_virt = (void *)map.vaddr;
				mblock[id].map = map;
				get_dma_buf(mblock[id].d_buf);
				mblock[id].fd =
					dma_buf_fd(mblock[id].d_buf, O_RDWR | O_CLOEXEC);
				//dma_buf_begin_cpu_access(mblock[id].d_buf, DMA_BIDIRECTIONAL);
				break;
			}
		} else {
			mblock[id].start_virt =
				kzalloc(mblock[id].size,
					GFP_KERNEL);
			mblock[id].start_phys =
				virt_to_phys(
					mblock[id].start_virt);
			mblock[id].start_dma = 0;
		}
        if (hcp_dbg_enable()) {
		pr_debug(
			"%s: [HCP][mem_reserve-%d] phys:0x%llx, virt:0x%p, dma:0x%llx, size:0x%llx, is_dma_buf:%d, fd:%d, d_buf:0x%p\n",
			__func__, id,
			isp7sp_get_reserve_mem_phys(id),
			isp7sp_get_reserve_mem_virt(id),
			isp7sp_get_reserve_mem_dma(id),
			isp7sp_get_reserve_mem_size(id),
			mblock[id].is_dma_buf,
			isp7sp_get_reserve_mem_fd(id),
			mblock[id].d_buf);
	}
	}

	return 0;
}
EXPORT_SYMBOL(isp7sp_allocate_working_buffer);

static void gce_release(struct kref *ref)
{
	struct mtk_hcp_reserve_mblock *mblock =
		container_of(ref, struct mtk_hcp_reserve_mblock, kref);

	dma_buf_vunmap(mblock->d_buf, &mblock->map);
	/* free iova */
	dma_buf_unmap_attachment(mblock->attach, mblock->sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(mblock->d_buf, mblock->attach);
	dma_buf_end_cpu_access(mblock->d_buf, DMA_BIDIRECTIONAL);
	dma_buf_put(mblock->d_buf);
	pr_info("%s:[HCP][%s] phys:0x%llx, virt:0x%p, dma:0x%llx, size:0x%llx, is_dma_buf:%d, fd:%d, d_buf:0x%p\n",
		__func__, mblock->name,
		isp7sp_get_reserve_mem_phys(IMG_MEM_G_ID),
		isp7sp_get_reserve_mem_virt(IMG_MEM_G_ID),
		isp7sp_get_reserve_mem_dma(IMG_MEM_G_ID),
		isp7sp_get_reserve_mem_size(IMG_MEM_G_ID),
		mblock->is_dma_buf,
		isp7sp_get_reserve_mem_fd(IMG_MEM_G_ID),
		mblock->d_buf);
	// close fd in user space driver, you can't close fd in kernel site
	// dma_heap_buffer_free(mblock[id].d_buf);
	//dma_buf_put(my_dma_buf);
	//also can use this api, but not recommended
	mblock->mem_priv = NULL;
	mblock->mmap_cnt = 0;
	mblock->start_dma = 0x0;
	mblock->start_virt = 0x0;
	mblock->start_phys = 0x0;
	mblock->d_buf = NULL;
	mblock->fd = -1;
	mblock->pIonHandle = NULL;
	mblock->attach = NULL;
	mblock->sgt = NULL;
}


int isp7sp_release_working_buffer(struct mtk_hcp *hcp_dev)
{
	enum isp7sp_rsv_mem_id_t id;
	struct mtk_hcp_reserve_mblock *mblock;
	unsigned int block_num;

	mblock = mb;
	block_num = hcp_dev->data->block_num;

	/* release reserved memory */
	for (id = 0; id < NUMS_MEM_ID; id++) {
		if (mblock[id].is_dma_buf) {
			switch (id) {
			case IMG_MEM_FOR_HW_ID:
				/*allocated at probe via dts*/
				break;
			case IMG_MEM_G_ID:
				kref_put(&mblock[id].kref, gce_release);
				break;
			default:
				/* free va */
				dma_buf_vunmap(mblock[id].d_buf, &mblock[id].map);
				/* free iova */
				dma_buf_unmap_attachment(mblock[id].attach,
				mblock[id].sgt, DMA_BIDIRECTIONAL);
				dma_buf_detach(mblock[id].d_buf,
				mblock[id].attach);
				dma_buf_put(mblock[id].d_buf);
				// close fd in user space driver, you can't close fd in kernel site
				// dma_heap_buffer_free(mblock[id].d_buf);
				//dma_buf_put(my_dma_buf);
				//also can use this api, but not recommended
				mblock[id].mem_priv = NULL;
				mblock[id].mmap_cnt = 0;
				mblock[id].start_dma = 0x0;
				mblock[id].start_virt = 0x0;
				mblock[id].start_phys = 0x0;
				mblock[id].d_buf = NULL;
				mblock[id].fd = -1;
				mblock[id].pIonHandle = NULL;
				mblock[id].attach = NULL;
				mblock[id].sgt = NULL;
				break;
			}
		} else {
			kfree(mblock[id].start_virt);
			mblock[id].start_virt = 0x0;
			mblock[id].start_phys = 0x0;
			mblock[id].start_dma = 0x0;
			mblock[id].mmap_cnt = 0;
		}
        if (hcp_dbg_enable()) {
		pr_debug(
			"%s: [HCP][mem_reserve-%d] phys:0x%llx, virt:0x%p, dma:0x%llx, size:0x%llx, is_dma_buf:%d, fd:%d\n",
			__func__, id,
			isp7sp_get_reserve_mem_phys(id),
			isp7sp_get_reserve_mem_virt(id),
			isp7sp_get_reserve_mem_dma(id),
			isp7sp_get_reserve_mem_size(id),
			mblock[id].is_dma_buf,
			isp7sp_get_reserve_mem_fd(id));
	}
	}

	return 0;
}
EXPORT_SYMBOL(isp7sp_release_working_buffer);

int isp7sp_get_init_info(struct img_init_info *info)
{

	if (!info) {
		pr_info("%s:NULL info\n", __func__);
		return -1;
	}

	info->hw_buf = isp7sp_get_reserve_mem_phys(DIP_MEM_FOR_HW_ID);
	/*WPE:0, ADL:1, TRAW:2, DIP:3, PQDIP:4 */
	info->module_info[0].c_wbuf =
				isp7sp_get_reserve_mem_phys(WPE_MEM_C_ID);
	info->module_info[0].c_wbuf_dma =
				isp7sp_get_reserve_mem_dma(WPE_MEM_C_ID);
	info->module_info[0].c_wbuf_sz =
				isp7sp_get_reserve_mem_size(WPE_MEM_C_ID);
	info->module_info[0].c_wbuf_fd =
				isp7sp_get_reserve_mem_fd(WPE_MEM_C_ID);
	info->module_info[0].t_wbuf =
				isp7sp_get_reserve_mem_phys(WPE_MEM_T_ID);
	info->module_info[0].t_wbuf_dma =
				isp7sp_get_reserve_mem_dma(WPE_MEM_T_ID);
	info->module_info[0].t_wbuf_sz =
				isp7sp_get_reserve_mem_size(WPE_MEM_T_ID);
	info->module_info[0].t_wbuf_fd =
				isp7sp_get_reserve_mem_fd(WPE_MEM_T_ID);

  // ADL
	info->module_info[1].c_wbuf =
				isp7sp_get_reserve_mem_phys(ADL_MEM_C_ID);
	info->module_info[1].c_wbuf_dma =
				isp7sp_get_reserve_mem_dma(ADL_MEM_C_ID);
	info->module_info[1].c_wbuf_sz =
				isp7sp_get_reserve_mem_size(ADL_MEM_C_ID);
	info->module_info[1].c_wbuf_fd =
				isp7sp_get_reserve_mem_fd(ADL_MEM_C_ID);
	info->module_info[1].t_wbuf =
				isp7sp_get_reserve_mem_phys(ADL_MEM_T_ID);
	info->module_info[1].t_wbuf_dma =
				isp7sp_get_reserve_mem_dma(ADL_MEM_T_ID);
	info->module_info[1].t_wbuf_sz =
				isp7sp_get_reserve_mem_size(ADL_MEM_T_ID);
	info->module_info[1].t_wbuf_fd =
				isp7sp_get_reserve_mem_fd(ADL_MEM_T_ID);

	// TRAW
	info->module_info[2].c_wbuf =
				isp7sp_get_reserve_mem_phys(TRAW_MEM_C_ID);
	info->module_info[2].c_wbuf_dma =
				isp7sp_get_reserve_mem_dma(TRAW_MEM_C_ID);
	info->module_info[2].c_wbuf_sz =
				isp7sp_get_reserve_mem_size(TRAW_MEM_C_ID);
	info->module_info[2].c_wbuf_fd =
				isp7sp_get_reserve_mem_fd(TRAW_MEM_C_ID);
	info->module_info[2].t_wbuf =
				isp7sp_get_reserve_mem_phys(TRAW_MEM_T_ID);
	info->module_info[2].t_wbuf_dma =
				isp7sp_get_reserve_mem_dma(TRAW_MEM_T_ID);
	info->module_info[2].t_wbuf_sz =
				isp7sp_get_reserve_mem_size(TRAW_MEM_T_ID);
	info->module_info[2].t_wbuf_fd =
				isp7sp_get_reserve_mem_fd(TRAW_MEM_T_ID);

		// DIP
	info->module_info[3].c_wbuf =
				isp7sp_get_reserve_mem_phys(DIP_MEM_C_ID);
	info->module_info[3].c_wbuf_dma =
				isp7sp_get_reserve_mem_dma(DIP_MEM_C_ID);
	info->module_info[3].c_wbuf_sz =
				isp7sp_get_reserve_mem_size(DIP_MEM_C_ID);
	info->module_info[3].c_wbuf_fd =
				isp7sp_get_reserve_mem_fd(DIP_MEM_C_ID);
	info->module_info[3].t_wbuf =
				isp7sp_get_reserve_mem_phys(DIP_MEM_T_ID);
	info->module_info[3].t_wbuf_dma =
				isp7sp_get_reserve_mem_dma(DIP_MEM_T_ID);
	info->module_info[3].t_wbuf_sz =
				isp7sp_get_reserve_mem_size(DIP_MEM_T_ID);
	info->module_info[3].t_wbuf_fd =
				isp7sp_get_reserve_mem_fd(DIP_MEM_T_ID);

	// PQDIP
	info->module_info[4].c_wbuf =
				isp7sp_get_reserve_mem_phys(PQDIP_MEM_C_ID);
	info->module_info[4].c_wbuf_dma =
				isp7sp_get_reserve_mem_dma(PQDIP_MEM_C_ID);
	info->module_info[4].c_wbuf_sz =
				isp7sp_get_reserve_mem_size(PQDIP_MEM_C_ID);
	info->module_info[4].c_wbuf_fd =
				isp7sp_get_reserve_mem_fd(PQDIP_MEM_C_ID);
	info->module_info[4].t_wbuf =
				isp7sp_get_reserve_mem_phys(PQDIP_MEM_T_ID);
	info->module_info[4].t_wbuf_dma =
				isp7sp_get_reserve_mem_dma(PQDIP_MEM_T_ID);
	info->module_info[4].t_wbuf_sz =
				isp7sp_get_reserve_mem_size(PQDIP_MEM_T_ID);
	info->module_info[4].t_wbuf_fd =
				isp7sp_get_reserve_mem_fd(PQDIP_MEM_T_ID);

	/*common*/
	/* info->g_wbuf_fd = isp7sp_get_reserve_mem_fd(IMG_MEM_G_ID); */
	info->g_wbuf_fd = isp7sp_get_reserve_mem_fd(IMG_MEM_G_ID);
	info->g_wbuf = isp7sp_get_reserve_mem_phys(IMG_MEM_G_ID);
	/*info->g_wbuf_sw = isp7sp_get_reserve_mem_virt(IMG_MEM_G_ID);*/
	info->g_wbuf_sz = isp7sp_get_reserve_mem_size(IMG_MEM_G_ID);

	return 0;
}

static int isp7sp_put_gce(void)
{
	kref_put(&mb[IMG_MEM_G_ID].kref, gce_release);
	return 0;
}

static int isp7sp_get_gce(void)
{
	kref_get(&mb[IMG_MEM_G_ID].kref);
	return 0;
}

int isp7sp_partial_flush(struct mtk_hcp *hcp_dev, struct flush_buf_info *b_info)
{
	struct mtk_hcp_reserve_mblock *mblock = NULL;
	unsigned int block_num = 0;
	unsigned int id = 0;
	unsigned int mode = 0;

	if (b_info->is_tuning)
		dma_buf_end_cpu_access_partial(b_info->dbuf,
					       DMA_BIDIRECTIONAL,
					       b_info->offset,
					       b_info->len);
	else {
		mode = b_info->mode;
		if (mode == imgsys_smvr)
			mblock = hcp_dev->data->smblock;
		else if (mode == imgsys_capture)
			mblock = hcp_dev->data->mblock;
		else
			mblock = hcp_dev->data->mblock;

		block_num = hcp_dev->data->block_num;
		for (id = 0; id < block_num; id++) {
			if (b_info->fd == mblock[id].fd) {
				dma_buf_end_cpu_access_partial(mblock[id].d_buf,
							       DMA_BIDIRECTIONAL,
							       b_info->offset,
							       b_info->len);
				break;
			}
		}
	}
    if (hcp_dbg_enable()) {
	pr_debug("imgsys_fw partial flush info(%d/0x%x/0x%x), mode(%d), is_tuning(%d)",
		b_info->fd, b_info->len, b_info->offset, b_info->mode, b_info->is_tuning);
    }
	return 0;
}
struct mtk_hcp_data isp7sp_hcp_data = {
	.mblock = isp7sp_reserve_mblock,
	.block_num = ARRAY_SIZE(isp7sp_reserve_mblock),
	.smblock = isp7sp_smvr_mblock,
	.allocate = isp7sp_allocate_working_buffer,
	.release = isp7sp_release_working_buffer,
	.get_init_info = isp7sp_get_init_info,
	.get_gce_virt = isp7sp_get_gce_virt,
	.get_gce = isp7sp_get_gce,
	.put_gce = isp7sp_put_gce,
	.get_hwid_virt = isp7sp_get_hwid_virt,
	.get_wpe_virt = isp7sp_get_wpe_virt,
	.get_wpe_cq_fd = isp7sp_get_wpe_cq_fd,
	.get_wpe_tdr_fd = isp7sp_get_wpe_tdr_fd,
	.get_dip_virt = isp7sp_get_dip_virt,
	.get_dip_cq_fd = isp7sp_get_dip_cq_fd,
	.get_dip_tdr_fd = isp7sp_get_dip_tdr_fd,
	.get_traw_virt = isp7sp_get_traw_virt,
	.get_traw_cq_fd = isp7sp_get_traw_cq_fd,
	.get_traw_tdr_fd = isp7sp_get_traw_tdr_fd,
	.get_pqdip_virt = isp7sp_get_pqdip_virt,
	.get_pqdip_cq_fd = isp7sp_get_pqdip_cq_fd,
	.get_pqdip_tdr_fd = isp7sp_get_pqdip_tdr_fd,
	.partial_flush = NULL,
};
MODULE_IMPORT_NS(DMA_BUF);
