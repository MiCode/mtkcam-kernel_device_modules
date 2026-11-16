/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __MTK_IMG_IPI_H__
#define __MTK_IMG_IPI_H__

#include <linux/types.h>
#include <linux/time.h>
#include "mtk_header_desc.h"
#include "mtk_imgsys-engine-isp8.h"

#ifndef __KERNEL__
#define BIT(nr)	(1UL << (nr))
typedef u_int8_t u8;
typedef int8_t s8;
typedef u_int16_t u16;
typedef int16_t s16;
typedef u_int32_t u32;
typedef int32_t s32;
typedef u_int64_t u64;
typedef int64_t s64;
#endif

/* updated in W1948.1 */
#define HEADER_VER 19481

#define IMG_MAX_HW_OUTPUTS	4

#ifndef IMG_MAX_HW_DMAS
#define IMG_MAX_HW_DMAS	(165)
#endif

#define IMG_MAX_PLANES	3

/* Imgsys memory mode type definitions */
#define IMGSYS_MEMORY_MODE_NORMAL_STREAMING (0)
#define IMGSYS_MEMORY_MODE_CAPTURE          (1)
#define IMGSYS_MEMORY_MODE_SMVR             (2)
#define IMGSYS_MEMORY_MODE_MAE              (3)
#if defined(IMGSYS_MEMORY_MODE_MAE)
#define IMGSYS_MEMORY_MODE_NUM_MAX          (IMGSYS_MEMORY_MODE_MAE + 1)
#else
#define IMGSYS_MEMORY_MODE_NUM_MAX          (IMGSYS_MEMORY_MODE_SMVR + 1)
#endif

/* legacy definition, prefer to use IMGSYS_MEMORY_MODE_XXX instead */
enum Mem_Mode {
	imgsys_streaming = IMGSYS_MEMORY_MODE_NORMAL_STREAMING,
	imgsys_capture   = IMGSYS_MEMORY_MODE_CAPTURE,
	imgsys_smvr      = IMGSYS_MEMORY_MODE_SMVR,
#if defined(IMGSYS_MEMORY_MODE_MAE)
	imgsys_mae     = IMGSYS_MEMORY_MODE_MAE,
#endif
	imgsys_mem_max   = IMGSYS_MEMORY_MODE_NUM_MAX,
};

/* Module working buffer type definitions */
#define IMGSYS_MODULE_WORKING_BUF_TYPE_CQ      (0)
#define IMGSYS_MODULE_WORKING_BUF_TYPE_TDR     (1)
#define IMGSYS_MODULE_WORKING_BUF_TYPE_C_MISC  (2)
#define IMGSYS_MODULE_WORKING_BUF_TYPE_NC_MISC (3)
#define IMGSYS_MODULE_WORKING_BUF_TYPE_NUM_MAX \
				(IMGSYS_MODULE_WORKING_BUF_TYPE_NC_MISC + 1)

#define IMGSYS_INIT_INFO_VERSION 2

#if (IMGSYS_INIT_INFO_VERSION == 1)
#define img_init_info img_init_info_v1
#elif (IMGSYS_INIT_INFO_VERSION == 2)
#define img_init_info img_init_info_v2
#endif

struct module_init_info {
	uint64_t	c_wbuf;
	uint64_t	c_wbuf_dma;
	uint32_t	c_wbuf_sz;
	uint32_t	c_wbuf_fd;
	uint64_t	t_wbuf;
	uint64_t	t_wbuf_dma;
	uint32_t	t_wbuf_sz;
	uint32_t	t_wbuf_fd;
} __packed;
struct gce_init_info {
	uint32_t	g_wbuf_fd;
	uint64_t	g_wbuf;
	uint32_t	g_wbuf_sz;
} __packed;
struct img_init_info_v1 {
	uint32_t	header_version;
	uint32_t	isp_version;
	uint32_t	dip_save_file;
	uint32_t	param_pack_size;
	uint32_t	dip_param_size;
	uint32_t	frameparam_size;
	uint32_t	reg_phys_addr;
	uint32_t	reg_range;
	uint64_t	hw_buf_fd;
	uint64_t	hw_buf;
	uint32_t	hw_buf_size;
	uint32_t	reg_table_size;
	uint32_t	sub_frm_size;
	uint32_t	cq_size;
	uint64_t	drv_data;

	/*new add, need refine*/
	struct module_init_info module_info_streaming[IMGSYS_MOD_DRV_NUM_MAX];
	struct module_init_info module_info_capture[IMGSYS_MOD_DRV_NUM_MAX];
	struct module_init_info module_info_smvr[IMGSYS_MOD_DRV_NUM_MAX];

	uint32_t	g_wbuf_fd;
	uint64_t	g_wbuf;
	uint32_t	g_wbuf_sz;
	uint32_t	sec_tag;
	uint16_t	full_wd;
	uint16_t	full_ht;
	uint32_t	smvr_mode;
	struct gce_init_info gce_info[IMGSYS_MEMORY_MODE_NUM_MAX];
	uint32_t	g_token_wbuf_fd;
	uint64_t	g_token_wbuf;
	uint32_t	g_token_wbuf_sz;
	uint32_t	is_capture;
} __packed;
struct imgsys_w_buf_info {
	uint64_t wbuf_dma;
	uint32_t wbuf_size;
	uint32_t wbuf_fd;
} __packed;
struct img_init_info_v2 {
	struct imgsys_w_buf_info module_wb_info
		[IMGSYS_MOD_DRV_NUM_MAX][IMGSYS_MODULE_WORKING_BUF_TYPE_NUM_MAX];
	struct imgsys_w_buf_info gce_wb_info;
	struct imgsys_w_buf_info gce_clr_token_wb_info;
	uint32_t sec_tag;
	uint32_t memory_mode;
	uint16_t full_wd;
	uint16_t full_ht;
} __packed;

struct private_data {
	int8_t need_update_desc;
	int8_t need_flush_tdr;
	uint32_t buf_fd;
	uint32_t buf_offset;
	uint32_t desc_offset;
	uint32_t tdr_offset;
} __packed;

struct img_swfrm_info {
	uint32_t hw_comb;
	int sw_ridx;
	uint8_t is_time_shared;
	uint8_t is_secFrm;
	uint8_t is_earlycb;
	uint8_t is_lastingroup;
	uint64_t sw_goft;
	uint64_t sw_bwoft;
	int subfrm_idx;
	void *g_swbuf;
	void *bw_swbuf;
	uint64_t pixel_bw;
	int tunmeta_size;
	struct private_data priv[IMGSYS_HW_NUM_MAX];
} __packed;

struct img_addr {
	u64	va;	/* Used by Linux OS access */
	u32	pa;	/* Used by CM4 access */
	u32	iova;	/* Used by IOMMU HW access */
	u32	offset; /* Used by User Daemon access */
	int	fd;
} __packed;

struct tuning_addr {
	u64	va;	/* Used by Linux OS access */
	u64	present;
	u32	pa;	/* Used by CM4 access */
	u32	iova;	/* Used by IOMMU HW access */
} __packed;


struct img_sw_addr {
	u64	va;	/* Used by APMCU access */
	u32	pa;	/* Used by CM4 access */
	u32	offset; /* Used by User Daemon access */
	u32	fd; /* Used by User Daemon access */

} __packed;

struct img_plane_format {
	u32	size;
	u16	stride;
} __packed;

struct img_pix_format {
	u16		width;
	u16		height;
	u32		colorformat;	/* enum mdp_color */
	u16		ycbcr_prof;	/* enum mdp_ycbcr_profile */
	struct img_plane_format	plane_fmt[IMG_MAX_PLANES];
} __packed;

struct img_image_buffer {
	struct img_pix_format	format;
	u32		iova[IMG_MAX_PLANES];
	/* enum mdp_buffer_usage, FD or advanced ISP usages */
	u32		usage;
	s32		fd[IMG_MAX_PLANES];
} __packed;

struct img_rect {
	int16_t		left;
	int16_t		top;
	uint16_t	width;
	uint16_t	height;
} __packed;

#define IMG_SUBPIXEL_SHIFT	20

struct img_crop {
	s16		left;
	s16		top;
	u16	width;
	u16	height;
	u32	left_subpix;
	u32	top_subpix;
	u32	width_subpix;
	u32	height_subpix;
} __packed;

#define IMG_CTRL_FLAG_HFLIP	BIT(0)
#define IMG_CTRL_FLAG_DITHER	BIT(1)
#define IMG_CTRL_FLAG_SHARPNESS	BIT(4)
#define IMG_CTRL_FLAG_HDR	BIT(5)
#define IMG_CTRL_FLAG_DRE	BIT(6)

struct img_input {
	struct img_image_buffer	buffer;
	u16		flags;	/* HDR, DRE, dither */
} __packed;

struct img_output {
	struct img_image_buffer	buffer;
	struct img_crop		crop;
	struct img_rect		compose;
	s16			rotation;
	u16		flags;	/* H-flip, sharpness, dither */
	u8		type; /* MCRP-D1, MCRP-D2 */
} __packed;

#define MAX_SRZ_CONFIGS  5
#define MAX_EXTRA_PARAMS  5

enum dip_srz_module {
	DIP_SRZ_NONE  = 0x0000,
	DIP_SRZ1      = 0x0001,
	DIP_SRZ2      = 0x0002,
	DIP_SRZ3      = 0x0004,
	DIP_SRZ4      = 0x0008,
	DIP_SRZ5      = 0x0010,
};

struct srz_config {
	enum dip_srz_module srz_module_id;
	uint32_t in_w;
	uint32_t in_h;
	uint32_t out_w;
	uint32_t out_h;
	uint32_t crop_x;
	uint32_t crop_y;
	uint32_t crop_floatX; /* x float precise - 32 bit */
	uint32_t crop_floatY; /* y float precise - 32 bit */
	unsigned long crop_w;
	unsigned long crop_h;
} __packed;

/* need to refine it for better memory usage.
 * See FEInfo(48B), FMInfo(32B), CrspInfo(24B), MFBConfig(...)
 */
#define EXTRA_PARAM_SIZE (64)
struct extra_param {
	uint32_t cmd_idx;
	uint8_t module_struct[EXTRA_PARAM_SIZE];
} __packed;

struct dip_param {
	uint32_t frame_no;
	uint32_t request_no;
	uint32_t unique_key;
	/*struct timeval expected_end_time;*/
	int32_t user_enum;

	uint8_t num_srz_cfg;
	struct srz_config srz_cfg[MAX_SRZ_CONFIGS];

	uint8_t num_extra_param;
	struct extra_param extra_param[MAX_EXTRA_PARAMS];
	// V3 batch mode added {
	uint64_t next_frame;
	// } V3 batch mode added
	u32 frameflag;
	u8 StreamTag;
} __packed;

struct dip_config_data {
	struct img_addr ref;
	struct img_addr alloc_buf;
	struct img_addr output;
};

struct img_ipi_frameparam {
	u8		dmas_enable[IMG_MAX_HW_DMAS][TIME_MAX];
	struct header_desc	dmas[IMG_MAX_HW_DMAS];
	struct header_desc	tuning_meta;
	struct header_desc	ctrl_meta;
};

struct img_sw_buffer {
	u64	handle;		/* Used by APMCU access */
	u32	scp_addr;	/* Used by CM4 access */
	u32	fd;		/* Used by User Daemon access */
	u32	offset;		/* Used by User Daemon access */
} __packed;

struct img_ipi_param {
	u8	usage;
	u8	smvr_mode;
	struct img_sw_buffer frm_param;
	u8	is_batch_mode;
	u8	num_frames;
	u64	req_addr_va;
	u64	frm_param_offset;
} __packed;

#ifdef __KERNEL__
struct img_frameparam {
	struct list_head	list_entry;
	struct img_ipi_frameparam frameparam;
};
#endif

/* ISP-MDP generic output information */

struct img_comp_frame {
	u32	output_disable:1;
	u32	bypass:1;
	u16	in_width;
	u16	in_height;
	u16	out_width;
	u16	out_height;
	struct img_crop	crop;
	u16	in_total_width;
	u16	out_total_width;
} __packed;

struct img_region {
	s16	left;
	s16	right;
	s16	top;
	s16	bottom;
} __packed;

struct img_offset {
	s16		left;
	s16		top;
	u32	left_subpix;
	u32	top_subpix;
} __packed;

struct img_comp_subfrm {
	u32		tile_disable:1;
	struct img_region	in;
	struct img_region	out;
	struct img_offset	luma;
	struct img_offset	chroma;
	s16			out_vertical;	/* Output vertical index */
	s16			out_horizontal;	/* Output horizontal index */
} __packed;

#define IMG_MAX_SUBFRAMES	14

struct mdp_rdma_subfrm {
	u32	offset[IMG_MAX_PLANES];
	u32	offset_0_p;
	u32	src;
	u32	clip;
	u32	clip_ofst;
} __packed;

struct mdp_rdma_data {
	u32		src_ctrl;
	u32		cmpr_ctrl;
	u32		control;
	u32		iova[IMG_MAX_PLANES];
	u32		iova_end[IMG_MAX_PLANES];
	u32		mf_bkgd;
	u32		mf_bkgd_in_pxl;
	u32		sf_bkgd;
	u32		ufo_dec_y;
	u32		ufo_dec_c;
	u32		transform;
	struct mdp_rdma_subfrm	subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct mdp_rsz_subfrm {
	u32	control2;
	u32	src;
	u32	clip;
} __packed;

struct mdp_rsz_data {
	u32		coeff_step_x;
	u32		coeff_step_y;
	u32		control1;
	u32		control2;
	struct mdp_rsz_subfrm	subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct mdp_wrot_subfrm {
	u32	offset[IMG_MAX_PLANES];
	u32	src;
	u32	clip;
	u32	clip_ofst;
	u32	main_buf;
} __packed;

struct mdp_wrot_data {
	u32		iova[IMG_MAX_PLANES];
	u32		control;
	u32		scan_10bit;
	u32		pending_zero;
	u32		stride[IMG_MAX_PLANES];
	u32		mat_ctrl;
	u32		fifo_test;
	u32		filter;
	struct mdp_wrot_subfrm	subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct mdp_wdma_subfrm {
	u32	offset[IMG_MAX_PLANES];
	u32	src;
	u32	clip;
	u32	clip_ofst;
} __packed;

struct mdp_wdma_data {
	u32		wdma_cfg;
	u32		iova[IMG_MAX_PLANES];
	u32		w_in_byte;
	u32		uv_stride;
	struct mdp_wdma_subfrm	subfrms[IMG_MAX_SUBFRAMES];
} __packed;

struct mdp_hdr_subfrm {
	uint32_t	hist_left;
} __packed;

struct mdp_hdr_data {
	uint32_t		rounding;
	struct mdp_hdr_subfrm	subfrms[IMG_MAX_SUBFRAMES];
} __packed;

#define MAX_READ_REG_NUM 4


struct dip_data {
	/* subset of ISP_DRIVER_CONFIG_STRUCT */
	// TODO: DIP settings for GCE cmd
	unsigned int cq_basePA;
	//E_ISP_DIP_CQ p2Cq;
	unsigned long DesCqPa;
	unsigned long DesCqVa; // for debugging
	unsigned int *pIspVirRegAddr_pa;
	unsigned int *pIspVirRegAddr_va;
	unsigned long tpipeTablePa;
	unsigned int *tpipeTableVa;
	unsigned int RingBufIdx;
	unsigned int burstCqIdx;
	unsigned int dupCqIdx;
	unsigned int cqIdx;                     //! index of pass2 cmdQ
	unsigned int frameflag;
	unsigned long smx1iPa;
	unsigned long smx2iPa;
	unsigned long smx3iPa;
	unsigned long smx4iPa;
	//ISP2MDP_STRUCT isp2mdpcfg;
	char *m_pMetLogBuf;
	unsigned int m_MetLogBufSize;
	unsigned int debugRegDump; // dump isp reg from GCE
	//total width of in-dma for frame mode only
	unsigned int framemode_total_in_w;
	//total width od out-dma for frame mode only
	unsigned int framemode_total_out_w;
	unsigned int framemode_h;        //height for frame mode only
	unsigned int total_data_size;     // add for bandwidth
	unsigned int dmgi_data_size;
	unsigned int depi_data_size;
	unsigned int lcei_data_size;
	unsigned int timgo_data_size;
	unsigned int regCount;
	unsigned int ReadAddrList[MAX_READ_REG_NUM];
	unsigned long imgi_base_addr;
	unsigned long tpipeTablePa_wpe;
	unsigned int *tpipeTableVa_wpe;
	unsigned long tpipeTablePa_mfb;
	unsigned int *tpipeTableVa_mfb;
	unsigned int *dl_tpipeTableVa_wpe;
	unsigned int dupCqIdx_wpe;
	unsigned int regCount_wpe;
	unsigned int ReadAddrList_wpe[MAX_READ_REG_NUM];
	unsigned int *wpecommand;
	unsigned int *mfbcommand;
	// DIP settings for GCE cmd
	unsigned long cqSecHdl;
	unsigned long cqSecSize;
	unsigned long DesCqOft;
	unsigned long DesCqSize;
	unsigned long VirtRegPa;
	unsigned long VirtRegVa;
	unsigned long VirtRegOft;
	unsigned long tpipeTableSecHdl;
	unsigned long tpipeTableSecSize;
	unsigned long tpipeTableOft;
	unsigned long smxSecHdl;
	unsigned long smxSecSize;
	unsigned long smx1iOft;
	unsigned long smx2iOft;
	unsigned long smx3iOft;
	unsigned long smx4iOft;
	unsigned long dip_ctl_yuv_en;
	unsigned long dip_ctl_yuv2_en;
	unsigned long dip_ctl_rgb_en;
	unsigned long dip_ctl_rgb2_en;
	unsigned long dip_ctl_dma_en;
	unsigned long dip_ctl_dma2_en;
	unsigned long dip_ctl_fmt_sel;
	unsigned long dip_ctl_fmt2_sel;
	unsigned long dip_ctl_mux_sel;
	unsigned long dip_ctl_mux2_sel;
	unsigned long dip_ctl_misc_sel;
	unsigned long dip_img2o_base_addr;
	unsigned long dip_img2bo_base_addr;
	unsigned long dip_img3o_base_addr;
	unsigned long dip_img3bo_base_addr;
	unsigned long dip_img3co_base_addr;
	unsigned long dip_feo_base_addr;
	unsigned long dip_dceso_base_addr;
	unsigned long dip_timgo_base_addr;
	unsigned long dip_imgi_base_addr;
	unsigned long dip_imgbi_base_addr;
	unsigned long dip_imgci_base_addr;
	unsigned long dip_vipi_base_addr;
	unsigned long dip_vip2i_base_addr;
	unsigned long dip_vip3i_base_addr;
	unsigned long dip_dmgi_base_addr;
	unsigned long dip_depi_base_addr;
	unsigned long dip_lcei_base_addr;
	unsigned long dip_ufdi_base_addr;
	unsigned long dip_imgbi_base_vaddr;
	unsigned long dip_imgci_base_vaddr;
	unsigned long dip_dmgi_base_vaddr;
	unsigned long dip_depi_base_vaddr;
	unsigned long dip_lcei_base_vaddr;
	unsigned int dip_img2o_size[3];
	unsigned int dip_img3o_size[3];
	unsigned int dip_feo_size[3];
	unsigned int dip_dceso_size[3];
	unsigned int dip_timgo_size[3];
	unsigned int dip_imgi_size[3];
	unsigned int dip_imgbi_size[3];
	unsigned int dip_imgci_size[3];
	unsigned int dip_vipi_size[3];
	unsigned int dip_dmgi_size[3];
	unsigned int dip_depi_size[3];
	unsigned int dip_lcei_size[3];
	unsigned int dip_ufdi_size[3];
	unsigned int dip_secure_tag;
	unsigned int dip_img2o_secure_tag;
	unsigned int dip_img2bo_secure_tag;
	unsigned int dip_img3o_secure_tag;
	unsigned int dip_img3bo_secure_tag;
	unsigned int dip_img3co_secure_tag;
	unsigned int dip_feo_secure_tag;
	unsigned int dip_dceso_secure_tag;
	unsigned int dip_timgo_secure_tag;
	unsigned int dip_imgi_secure_tag;
	unsigned int dip_imgbi_secure_tag;
	unsigned int dip_imgci_secure_tag;
	unsigned int dip_vipi_secure_tag;
	unsigned int dip_vip2i_secure_tag;
	unsigned int dip_vip3i_secure_tag;
	unsigned int dip_dmgi_secure_tag;
	unsigned int dip_depi_secure_tag;
	unsigned int dip_lcei_secure_tag;
	unsigned int dip_ufdi_secure_tag;
} __packed;

struct isp_data {
	u64	dl_flags;	/* 1 << (enum mdp_comp_type) */
	u32	smxi_iova[4];
	u32	cq_idx;
	u32	cq_iova;
	u32	tpipe_iova[IMG_MAX_SUBFRAMES];
	u32	mfb_iova[IMG_MAX_SUBFRAMES];
	struct dip_data	dip_data;	// TODO: declared in a platform header
} __packed;

struct img_compparam {
	u16		type;	/* enum mdp_comp_type */
	u16		id;	/* enum mdp_comp_id */
	u32		input;
	u32		outputs[IMG_MAX_HW_OUTPUTS];
	u32		num_outputs;
	struct img_comp_frame	frame;
	struct img_comp_subfrm	subfrms[IMG_MAX_SUBFRAMES];
	u32		num_subfrms;
	union {
		struct mdp_rdma_data	rdma;
		struct mdp_rsz_data	rsz;
		struct mdp_wrot_data	wrot;
		struct mdp_wdma_data	wdma;
		struct mdp_hdr_data	hdr;
		struct isp_data		isp;
		/* struct wpe_data	wpe; */
	};
} __packed;

#define IMG_MAX_COMPONENTS	20

struct img_mux {
	u32	reg;
	u32	value;
};

struct img_mmsys_ctrl {
	struct img_mux	sets[IMG_MAX_COMPONENTS * 2];
	u32	num_sets;
};

struct img_config {
	struct img_compparam	components[IMG_MAX_COMPONENTS];
	u32		num_components;
	struct img_mmsys_ctrl	ctrls[IMG_MAX_SUBFRAMES];
	u32		num_subfrms;
} __packed;

#endif  /* __MTK_IMG_IPI_H__ */

