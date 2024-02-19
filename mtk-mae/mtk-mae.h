/* SPDX-License-Identifier: GPL-2.0 */
//
// Copyright (c) 2018 MediaTek Inc.

#ifndef __MTK_MAE_H__
#define __MTK_MAE_H__

#include <linux/completion.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/pm_opp.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-v4l2.h>
#include <linux/types.h>
#include <linux/time.h>

#define M2M_ENABLE 1
#define MEMCPY_KERNEL_STRUCT_ENABLE 1
#define MAE_CMDQ_SEC_READY 1

#define CMDQ_SEC_READY 0

#define MAE_IRQ_STATUS_VALUE 0x1
#define MAE_IRQ_MASK 0x1

#define CMDQ_REG_MASK 0xffffffff

#define DEBUG_BUFFER_LINE_LEN (120)
#define DEBUG_BUFFER_LINE_NUM (1024)
#define DEBUG_BUFFER_SIZE (DEBUG_BUFFER_LINE_LEN * DEBUG_BUFFER_LINE_NUM)

#define LSB_MASK 0x000fffff
#define MSB_MASK 0xffff00000
#define LSB_ADDR_SHIFT_BITS 4
#define MSB_ADDR_SHIFT_BITS 20
#define LSB_ADDR(ADDR) ((ADDR & LSB_MASK) >> LSB_ADDR_SHIFT_BITS)
#define MSB_ADDR(ADDR) ((ADDR & MSB_MASK) >> MSB_ADDR_SHIFT_BITS)
#define REG_RANGE(VALUE,MSB,LSB) (uint32_t)((VALUE >> LSB) & ((1<<(MSB-LSB+1)) - 1))

#define INTERNAL_BUFFER_SIZE (512 * 1024)

/* ============ should align userspace define (start) ================== */
#define MAE_BASE_ADDR_ALIGN 16

#define V0_FD_640_480_COEF_SIZE     (11479 * MAE_BASE_ADDR_ALIGN)
#define V0_FD_640_480_CONFIG_SIZE   (4086 * MAE_BASE_ADDR_ALIGN)
#define V0_FD_640_480_COEF_PAT_OFFSET   0
#define V0_FD_640_480_CONFIG_PAT_OFFSET 0

#define V0_FD_480_360_COEF_SIZE     (11471 * MAE_BASE_ADDR_ALIGN)
#define V0_FD_480_360_CONFIG_SIZE   (2862 * MAE_BASE_ADDR_ALIGN)
#define V0_FD_480_360_COEF_PAT_OFFSET   \
		(V0_FD_640_480_COEF_PAT_OFFSET + V0_FD_640_480_COEF_SIZE)
#define V0_FD_480_360_CONFIG_PAT_OFFSET \
		(V0_FD_640_480_CONFIG_PAT_OFFSET + V0_FD_640_480_CONFIG_SIZE)

#define V0_FD_240_180_COEF_SIZE     (11471 * MAE_BASE_ADDR_ALIGN)
#define V0_FD_240_180_CONFIG_SIZE   (2180 * MAE_BASE_ADDR_ALIGN)
#define V0_FD_240_180_COEF_PAT_OFFSET   \
		(V0_FD_480_360_COEF_PAT_OFFSET + V0_FD_480_360_COEF_SIZE)
#define V0_FD_240_180_CONFIG_PAT_OFFSET \
		(V0_FD_480_360_CONFIG_PAT_OFFSET + V0_FD_480_360_CONFIG_SIZE)

#define V0_FD_120_90_COEF_SIZE      (11471 * MAE_BASE_ADDR_ALIGN)
#define V0_FD_120_90_CONFIG_SIZE    (1834 * MAE_BASE_ADDR_ALIGN)
#define V0_FD_120_90_COEF_PAT_OFFSET   \
		(V0_FD_240_180_COEF_PAT_OFFSET + V0_FD_240_180_COEF_SIZE)
#define V0_FD_120_90_CONFIG_PAT_OFFSET \
		(V0_FD_240_180_CONFIG_PAT_OFFSET + V0_FD_240_180_CONFIG_SIZE)

#define V0_ATTR_128_128_COEF_SIZE   (15290 * MAE_BASE_ADDR_ALIGN)
#define V0_ATTR_128_128_CONFIG_SIZE (2000 * MAE_BASE_ADDR_ALIGN)

#define V1_FD_IPN_640_480_COEF_SIZE (10964 * MAE_BASE_ADDR_ALIGN)
#define V1_FD_IPN_640_480_CONFIG_SIZE (8203 * MAE_BASE_ADDR_ALIGN)
#define V1_FD_IPN_640_480_COEF_PAT_OFFSET   0
#define V1_FD_IPN_640_480_CONFIG_PAT_OFFSET 0

#define V1_FD_IPN_480_360_COEF_SIZE (10964 * MAE_BASE_ADDR_ALIGN)
#define V1_FD_IPN_480_360_CONFIG_SIZE (7616 * MAE_BASE_ADDR_ALIGN)
#define V1_FD_IPN_480_360_COEF_PAT_OFFSET \
		(V1_FD_IPN_640_480_COEF_PAT_OFFSET + V1_FD_IPN_640_480_COEF_SIZE)
#define V1_FD_IPN_480_360_CONFIG_PAT_OFFSET \
		(V1_FD_IPN_640_480_CONFIG_PAT_OFFSET + V1_FD_IPN_640_480_CONFIG_SIZE)

#define V1_FD_IPN_240_180_COEF_SIZE (10956 * MAE_BASE_ADDR_ALIGN)
#define V1_FD_IPN_240_180_CONFIG_SIZE (6030 * MAE_BASE_ADDR_ALIGN)
#define V1_FD_IPN_240_180_COEF_PAT_OFFSET \
		(V1_FD_IPN_480_360_COEF_PAT_OFFSET + V1_FD_IPN_480_360_COEF_SIZE)
#define V1_FD_IPN_240_180_CONFIG_PAT_OFFSET \
		(V1_FD_IPN_480_360_CONFIG_PAT_OFFSET + V1_FD_IPN_480_360_CONFIG_SIZE)

#define V1_FD_IPN_120_90_COEF_SIZE (10948 * MAE_BASE_ADDR_ALIGN)
#define V1_FD_IPN_120_90_CONFIG_SIZE (4860 * MAE_BASE_ADDR_ALIGN)
#define V1_FD_IPN_120_90_COEF_PAT_OFFSET \
		(V1_FD_IPN_240_180_COEF_PAT_OFFSET + V1_FD_IPN_240_180_COEF_SIZE)
#define V1_FD_IPN_120_90_CONFIG_PAT_OFFSET \
		(V1_FD_IPN_240_180_CONFIG_PAT_OFFSET + V1_FD_IPN_240_180_CONFIG_SIZE)

#define V1_FD_FPN_480_360_COEF_SIZE (59130 * MAE_BASE_ADDR_ALIGN)
#define V1_FD_FPN_480_360_CONFIG_SIZE (8182 * MAE_BASE_ADDR_ALIGN)

#define V1_FLD_FAC_112_112_COEF_SIZE (21770 * MAE_BASE_ADDR_ALIGN)
#define V1_FLD_FAC_112_112_CONFIG_SIZE (4420 * MAE_BASE_ADDR_ALIGN)

#define MAX_OUTER_LOOP_NUM 3
#define MAX_IMG_NUM MAX_OUTER_LOOP_NUM
#define REQUEST_BUFFER_NUM 1
#define MAX_PYRAMID_NUM 3
#define MAX_FACE_NUM 1024
#define DATA_SIZE 16
#define FD_OUTPUT_SIZE ((round_up(MAX_FACE_NUM, DATA_SIZE) * 2 + 1) * DATA_SIZE)

#define FD_PATTERN_NUM 4
#define FD_PYRAMID_MIN_WIDTH  64
#define FD_PYRAMID_MIN_HEIGHT 64

#define FD_V0_WDMA_NUM 1
#define ATTR_V0_WDMA_NUM 4
#define ATTR_V0_WDMA_SIZE 1
#define FAC_V1_WDMA_NUM 6
#define FAC_V1_WDMA_SIZE 54
#define FD_V1_IPN_WDMA_NUM 5
#define FD_V1_IPN_WDMA_SIZE FD_OUTPUT_SIZE
#define FD_V1_FPN_WDMA_NUM 4
#define FD_V1_FPN_WDMA_SIZE FD_OUTPUT_SIZE
#define HW_OUTPUT_SIZE (FD_V1_IPN_WDMA_NUM * FD_OUTPUT_SIZE)

#define BASE_ADDR_REG_SIZE 8
#define COMMON_REG_SIZE 4
#define WDMA_DATA_UNIT 16
#define RSZ_BASE_ADDR_OFFSET 0x200
#define RSZ_NUM 3

#define MAX_FLD_V0_FACE_NUM 15
#define FLD_V0_POINT 500

#define AISEG_MAP_NUM 11
#define SEMANTIC_MERGE_NUM 16
#define PERSON_MERGE_NUM 16
#define MERGE_CONFIDENCE_NUM 16
#define AISEG_POP_GROUP_SIZE 3
#define FLD_V1_INPUT_FACE_NUM 2

#define FPN_PYRAMID_WIDTH 480
#define FPN_PYRAMID_HEIGHT 360

const uint32_t fd_pattern_width[FD_PATTERN_NUM] = {640, 480, 240, 120};
const uint32_t fd_pattern_height[FD_PATTERN_NUM] = {480, 360, 180, 90};

const uint32_t v0_fd_coef_offset[FD_PATTERN_NUM] = {
						V0_FD_640_480_COEF_PAT_OFFSET,
						V0_FD_480_360_COEF_PAT_OFFSET,
						V0_FD_240_180_COEF_PAT_OFFSET,
						V0_FD_120_90_COEF_PAT_OFFSET
						};

const uint32_t v0_fd_config_offset[FD_PATTERN_NUM] = {
					V0_FD_640_480_CONFIG_PAT_OFFSET,
					V0_FD_480_360_CONFIG_PAT_OFFSET,
					V0_FD_240_180_CONFIG_PAT_OFFSET,
					V0_FD_120_90_CONFIG_PAT_OFFSET
					};


const uint32_t v1_fd_ipn_coef_offset[FD_PATTERN_NUM] = {
					V1_FD_IPN_640_480_COEF_PAT_OFFSET,
					V1_FD_IPN_480_360_COEF_PAT_OFFSET,
					V1_FD_IPN_240_180_COEF_PAT_OFFSET,
					V1_FD_IPN_120_90_COEF_PAT_OFFSET
					};

const uint32_t v1_fd_ipn_config_offset[FD_PATTERN_NUM] = {
					V1_FD_IPN_640_480_CONFIG_PAT_OFFSET,
					V1_FD_IPN_480_360_CONFIG_PAT_OFFSET,
					V1_FD_IPN_240_180_CONFIG_PAT_OFFSET,
					V1_FD_IPN_120_90_CONFIG_PAT_OFFSET
					};

struct config_info {
	const uint32_t size;
	const uint32_t rotate_offset;
	const uint32_t rotate_size;
};

struct coef_info {
	const uint32_t size;
};

const struct config_info fd_v0_config_info[FD_PATTERN_NUM] = {
	{
		.size = 1004,
		.rotate_offset = 2043,
		.rotate_size = 1004,
	},
	{
		.size = 968,
		.rotate_offset = 1431,
		.rotate_size = 968,
	},
	{
		.size = 671,
		.rotate_offset = 1090,
		.rotate_size = 671,
	},
	{
		.size = 543,
		.rotate_offset = 917,
		.rotate_size = 543,
	}
};

const struct coef_info fd_v0_coef_info[FD_PATTERN_NUM] = {
	{
		.size = 1309,
	},
	{
		.size = 5583,
	},
	{
		.size = 5583,
	},
	{
		.size = 9540,
	}
};

// fd_v1_ipn size and rotate offset
const struct config_info fd_v1_ipn_config_info[FD_PATTERN_NUM] = {
	{
		.size = 787,
		.rotate_offset = 4100,
		.rotate_size = 787,
	},
	{
		.size = 895,
		.rotate_offset = 3807,
		.rotate_size = 897,
	},
	{
		.size = 709,
		.rotate_offset = 3015,
		.rotate_size = 709,
	},
	{
		.size = 559,
		.rotate_offset = 2429,
		.rotate_size = 561,
	}
};

const struct coef_info fd_v1_ipn_coef_info[FD_PATTERN_NUM] = {
	{
		.size = 289,
	},
	{
		.size = 1312,
	},
	{
		.size = 5589,
	},
	{
		.size = 9549,
	}
};

const struct config_info attr_v0_config_info = {
	.size = 558,
	.rotate_offset = 1000,
	.rotate_size = 558,
};

const struct coef_info attr_v0_coef_info = {
	.size = 5180,
};

const struct config_info fac_v1_config_info = {
	.size = 619,
	.rotate_offset = 2210,
	.rotate_size = 619,
};

const struct coef_info fac_v1_coef_info = {
	.size = 7937,
};

const struct config_info fd_v1_fpn_config_info = {
	.size = 777,
	.rotate_offset = 4091,
	.rotate_size = 777,
};

const struct coef_info fd_v1_fpn_coef_info = {
	.size = 6449,
};

typedef enum {
	IMAGE_PLANE_0 = 0,
	OUTPUT_PLANE = 1,
	PARAM_PLANE = OUTPUT_PLANE + MAX_PYRAMID_NUM, // 4
	MODEL_TABLE_PLANE, // 5
	DEBUG_PLANE, // 6
	MAX_PLANE // 7
} ENQ_PLANE;


typedef enum {
	UNKNOWN_CMD = 0,
	DEQUE_CMD = 1,
	UNINIT_CMD =2
} DEQ_THREAD_CMD;

typedef enum {
	MODEL_TYPE_FD_V0 = 0,
	MODEL_TYPE_FD_V1_IPN = 1,
	MODEL_TYPE_FD_V1_FPN = 2,
	MODEL_TYPE_FLD_FAC_V0 = 3,
	MODEL_TYPE_FLD_FAC_V1 = 4,
	MODEL_TYPE_AISEG = 5,
	MODEL_TYPE_MAX
} MODEL_TYPE;

struct ModelEntry {
	int fd;
	size_t size;
	size_t offset;
	int isReady;
};

struct ModelTable {
	struct ModelEntry configTable[MODEL_TYPE_MAX];
	struct ModelEntry coefTable[MODEL_TYPE_MAX];

	// MAE_TO_DO: rearrange the usage of the buffers
	struct ModelEntry aisegOutput[AISEG_MAP_NUM];
};

#if MEMCPY_KERNEL_STRUCT_ENABLE
typedef enum {
	USER_FD = 0,
	USER_AISEG = 1,
	USER_SUPER,
	USER_MAX
} MAE_USER;

enum FD_INPUT_DEGREE {
	DEGREE_0 = 0,
	DEGREE_90 = 1,
	DEGREE_180 = 2,
	DEGREE_270 = 3
};

enum AISEG_INPUT_DEGREE {
	AISEG_ROT_DEGREE_0 = 0,
	AISEG_ROT_DEGREE_90 = 1,
	AISEG_ROT_DEGREE_180 = 2,
	AISEG_ROT_DEGREE_270 = 3
};

typedef enum {
	FD_V0 = 0,
	ATTR_V0 = 1,
	FLD_V0 = 2,
	AISEG = 3,
	FD_V1_IPN = 4,
	FD_V1_FPN = 5,
	FAC_V1 = 6,
	MAE_MODE_MAX
} MAE_MODE;

typedef enum {
	YUYV = 1,  // AISEG
	NV12 = 2,  // fd, fld, attr
	Y8 = 3,
	MAE_FORMAT_MAX
} MAE_FORMAT;

struct MAE_ROI {
	uint32_t x1;
	uint32_t y1;
	uint32_t x2;
	uint32_t y2;
};

struct MAE_PADDING {
	uint32_t left;
	uint32_t right;
	uint32_t down;
	uint32_t up;
};

struct EnqueImage {
	MAE_FORMAT srcImgFmt;
	uint32_t imgWidth;
	uint32_t imgHeight;
	bool enRoi;
	struct MAE_ROI roi;
	bool enResize;
	uint32_t resizeWidth;
	uint32_t resizeHeight;
	bool enPadding;
	struct MAE_PADDING padding;
};

typedef enum {
	NORMAL = 0,
	RIGHT = 1,
	LEFT = 2
} FLD_ROP;

typedef enum {
	FLD_0 = 0,
	FLD_1 = 1,
	FLD_2 = 2,
	FLD_3 = 3,
	FLD_4 = 4,
	FLD_5 = 5,
	FLD_6 = 6,
	FLD_7 = 7,
	FLD_8 = 8,
	FLD_9 = 9,
	FLD_10 = 10,
	FLD_11 = 11
} FLD_RIP;

struct FldCropRipRop {
	struct MAE_ROI roi;
	FLD_ROP rop;
	FLD_RIP rip;
};

struct FldConfig {
	uint32_t fldFaceNum;
	struct FldCropRipRop fldSetting[MAX_FLD_V0_FACE_NUM];
};

struct EnqueParam {
	// init parameters
	MAE_USER user;

	// fd init parameters
	uint32_t imgMaxWidth;
	uint32_t imgMaxHeight;
	bool isSecure;
	uint32_t FDModelSel;
	uint32_t FACModelSel;

	// perframe Parameters
	int32_t pyramidNumber;
	enum FD_INPUT_DEGREE fdInputDegree;
	enum AISEG_INPUT_DEGREE aisegInputDegree;
	int32_t attrFaceNumber;
	enum FD_INPUT_DEGREE attrInputDegree[FLD_V1_INPUT_FACE_NUM];


	MAE_MODE maeMode;
	int32_t requestNum;

	// fld v0 param
	struct FldConfig fldConfig;

	struct EnqueImage image[MAX_IMG_NUM];

	int32_t faceNum[MAX_OUTER_LOOP_NUM][FD_V1_IPN_WDMA_NUM];

	// aiseg post
	uint8_t semanticMerge[AISEG_POP_GROUP_SIZE][SEMANTIC_MERGE_NUM];
	uint8_t personMerge[AISEG_POP_GROUP_SIZE][PERSON_MERGE_NUM];
	uint8_t mergeConfidence[AISEG_POP_GROUP_SIZE][MERGE_CONFIDENCE_NUM];
};
#endif

#if M2M_ENABLE
struct fd_result {
	// uint8_t result[MAX_PYRAMID_NUM][FD_OUTPUT_SIZE]  __aligned(32);
	int16_t fd_total_num;
	uint16_t fd_pyramid_num[MAX_PYRAMID_NUM];
};

// MAE_TO_DO
struct mtk_mae_enq_info {
	struct fd_result fd_out  __aligned(32);
};
#endif

/* ============ should align userspace define (end) ================== */
enum mae_log_level {
	MAE_INFO = 0,
	MAE_DEBUG = 1
};

#define mae_dev_info(dev, fmt, ...)	\
do {								\
	if (mae_log_level_value >= MAE_INFO)	\
		dev_info(dev, fmt, ##__VA_ARGS__);	\
} while (0)

#define mae_dev_dbg(dev, fmt, ...)	\
do {								\
	if (mae_log_level_value >= MAE_DEBUG)	\
		dev_info(dev, fmt, ##__VA_ARGS__);	\
} while (0)

struct crop_setting_in {
	int32_t start_x;
	int32_t end_x;
	int32_t start_y;
	int32_t end_y;
	int32_t input_h_size;
	int32_t input_v_size;
};

struct crop_setting_out {
	int32_t reg_pre_crop_h_st;
	int32_t reg_pre_crop_h_length;
	int32_t reg_pre_crop_hfde_size;
	int32_t reg_pre_crop_v_st;
	int32_t reg_pre_crop_v_length;
	int32_t reg_pre_crop_vfde_size;
	int32_t reg_ins_path;
	int32_t reg_pre_crop_h_crop_en;
	int32_t reg_pre_crop_v_crop_en;
};

struct padding_setting_in {
	int32_t left;
	int32_t right;
	int32_t down;
	int32_t up;
	int32_t crop_output_h_size;
	int32_t crop_output_v_size;
};

struct padding_setting_out {
	int32_t reg_post_ins_blk_hpre;
	int32_t reg_post_ins_h_length;
	int32_t reg_post_ins_hfde_size;
	int32_t reg_post_ins_blk_vpre;
	int32_t reg_post_ins_v_length;
	int32_t reg_post_ins_vfde_size;
	int32_t reg_h_size;
	int32_t reg_v_size;
	int32_t reg_post_ins_hv_insert_en;
};

struct rsz_setting_in {
	int32_t rsz_input_h_size;
	int32_t rsz_input_v_size;
	int32_t rsz_output_h_size;
	int32_t rsz_output_v_size;
	int32_t rsz_input_ch;
};

struct rsz_setting_out {
	int32_t reg_scale_factor_ve;
	int32_t reg_v_shift_mode_en;
	int32_t reg_ini_factor_ve;
	int32_t reg_scale_factor_ho;
	int32_t reg_h_shift_mode_en;
	int32_t reg_ini_factor_ho;
	int32_t reg_1p_path_en;
	int32_t reg_order;
	int32_t reg_h_size_usr_md;
	int32_t reg_v_size_usr_md;
	int32_t reg_scale_ve_en;
	int32_t reg_scale_ho_en;
	int32_t reg_mode_c_ve;
	int32_t reg_mode_c_ho;
	int32_t reg_rsz_u2s;
	int32_t reg_rsz_mode_ho;
	int32_t reg_rsz_mode_ve;
	int32_t reg_cb_factor_ho;
	int32_t reg_cb_factor_ve;
};

struct mae_clocks {
	struct clk_bulk_data *clks;
	unsigned int clk_num;
};

struct mtk_mae_req_work {
	struct work_struct work;
	struct mtk_mae_dev *mae_dev;
};

struct mtk_mae_dev {
	struct device *dev;
	struct device *smmu_dev;
	struct media_device mdev;
	struct v4l2_device v4l2_dev;
	struct device *larb;
	struct mae_clocks clks_data;
#if M2M_ENABLE
	struct v4l2_m2m_dev *m2m_dev;
	uint64_t mae_out;
#endif
	struct video_device vdev;
	struct platform_device *aov_pdev;

	/* Lock for V4L2 operations */
	struct mutex vdev_lock;

	struct mtk_mae_ctx *ctx;
	struct mtk_mae_map_table *map_table;

	struct completion mae_job_finished;
	struct workqueue_struct *frame_done_wq;
	struct mtk_mae_req_work req_work;

	void __iomem *mae_base;
	int mae_event_id;
	uint32_t mae_sec_wait;
	uint32_t mae_sec_set;
	uint32_t mae_stream_count;

	struct cmdq_client *mae_clt;
	struct cmdq_client *mae_secure_clt;
	struct cmdq_pkt *pkt[REQUEST_BUFFER_NUM];
	struct cmdq_pkt *sec_pkt;
	int32_t core_sel[REQUEST_BUFFER_NUM][MAX_OUTER_LOOP_NUM];
	uint32_t outer_loop[REQUEST_BUFFER_NUM];

	bool is_hw_hang;
	bool is_secure;
	bool is_first_qbuf;

	struct mutex mae_device_lock;
	int open_video_device_cnt;
};

struct mtk_mae_ctx {
	struct mtk_mae_dev *mae_dev;
	struct device *dev;
	struct v4l2_fh fh;
	struct v4l2_pix_format_mplane src_fmt;
#if M2M_ENABLE
	struct v4l2_meta_format dst_fmt;
#else
	struct vb2_queue *vq;
#endif
	struct v4l2_ctrl_handler hdl;
};

enum MAE_ADDR_TYPE {
	GET_VA = 0,
	GET_PA = 1,
	GET_BOTH = 2
};

struct dmabuf_info {
	struct dma_buf *dmabuf;
	bool is_map;
	unsigned long long kva;
	struct iosys_map map;
	bool is_attach;
	unsigned long long pa;
	struct dma_buf_attachment *attach;
	struct sg_table *sg_table;
};

struct mtk_mae_map_table {
	struct dmabuf_info model_table_dmabuf_info;
	struct dmabuf_info config_dmabuf_info[MODEL_TYPE_MAX];
	struct dmabuf_info coef_dmabuf_info[MODEL_TYPE_MAX];
	struct dmabuf_info image_dmabuf_info[REQUEST_BUFFER_NUM];
	struct dmabuf_info param_dmabuf_info[REQUEST_BUFFER_NUM];
	struct dmabuf_info output_dmabuf_info[REQUEST_BUFFER_NUM][MAX_PYRAMID_NUM];
	struct dmabuf_info aiseg_output_dmabuf_info[REQUEST_BUFFER_NUM][AISEG_MAP_NUM];
	struct dmabuf_info debug_dmabuf_info[REQUEST_BUFFER_NUM];
	struct dmabuf_info internal_dmabuf_info;
};

struct mtk_mae_drv_ops {
	// MAE_TO_DO
	// void (*reset)(struct mtk_aie_dev *fd);
	// int (*alloc_buf)(struct mtk_aie_dev *fd);
	// int (*init)(struct mtk_aie_dev *fd);
	// void (*uninit)(struct mtk_aie_dev *fd);
	bool (*set_dma_address)(struct mtk_mae_dev *mae_dev, int idx);
	bool (*config_hw)(struct mtk_mae_dev *mae_dev, int idx);
	void (*config_fld)(struct mtk_mae_dev *mae_dev, int idx);
	void (*get_fd_v0_result)(struct mtk_mae_dev *mae_dev, int idx);
	void (*get_fd_v1_result)(struct mtk_mae_dev *mae_dev, int idx);
	// void (*get_attr_result)(struct mtk_aie_dev *fd,
	// 		struct aie_enq_info *aie_cfg);
	// void (*get_fld_result)(struct mtk_aie_dev *fd,
	// 		struct aie_enq_info *aie_cfg);
	void (*irq_handle)(struct mtk_mae_dev *mae_dev);
	// void (*config_fld_buf_reg)(struct mtk_aie_dev *fd);
	void (*dump_reg)(struct mtk_mae_dev *mae_dev);
	// void (*dump_cg_reg)(struct mtk_aie_dev *fd);
	// void (*enable_ddren)(struct mtk_aie_dev *fd);
	void (*secure_init)(struct mtk_mae_dev *fd);
	void (*secure_enable)(struct mtk_mae_dev *fd);
	void (*secure_disable)(struct mtk_mae_dev *fd);
};

void mtk_mae_register_drv_ops(const struct mtk_mae_drv_ops *ops);

#endif /* __MTK_MAE_H__ */
