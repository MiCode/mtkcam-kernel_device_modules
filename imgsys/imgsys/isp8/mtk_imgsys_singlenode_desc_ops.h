/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */

#ifndef _MTK_IMGSYS_SINGLENODE_DESC_OPS_H_
#define _MTK_IMGSYS_SINGLENODE_DESC_OPS_H_

#include "mtk_header_desc.h"

#if defined (IMGSYS_SINGLE_NODE_DESC_V2)
struct singlenode_desc_ops_t {
	uint8_t					(*_dmas_num)(uint64_t desc, uint32_t t);
	uint8_t					(*_dmas_enable)(uint64_t desc, uint32_t dma_id,
								uint32_t t);
	struct frameparams*		(*_dmas)(uint64_t desc, uint32_t dma_id, uint32_t t);
	uint32_t				(*_frame_num)(uint64_t desc);
	struct frameparams*		(*_tuning_meta)(uint64_t desc, uint32_t t);
	struct frameparams*		(*_ctrl_meta)(uint64_t desc, uint32_t t);
	struct frameparams*		(*_ctrl_meta_from_user_mae)(uint64_t desc, uint32_t t);
	struct frameparams*		(*_ctrl_meta_from_user)(uint64_t desc, uint32_t t);
	uint64_t*				(*_req_stat)(uint64_t desc);
};
#else
struct header_desc_ops_t {
	uint32_t				(*_fparams_tnum)(uint64_t hdr_desc);
	struct buf_info*		(*_buf_info)(uint64_t hdr_desc, uint32_t t);
	struct frameparams*		(*_fparams)(uint64_t hdr_desc, uint32_t t);
	uint32_t				(*_plane_num)(uint64_t hdr_desc, uint32_t t);
	uint32_t				(*_buf_fd)(uint64_t hdr_desc, uint32_t t, uint32_t p);
	uint32_t				(*_buf_offset)(uint64_t hdr_desc, uint32_t t, uint32_t p);
};

struct singlenode_desc_ops_t {
	const struct header_desc_ops_t *hdr_ops;
	uint8_t					(*_dmas_enable)(uint64_t desc, uint32_t dma_id,
											uint32_t t);
	uint64_t				(*_dmas)(uint64_t desc, uint32_t dma_id);
	uint64_t				(*_tuning_meta)(uint64_t desc);
	uint64_t				(*_ctrl_meta)(uint64_t desc);
	uint64_t				(*_ctrl_meta_from_user)(uint64_t desc);
	uint64_t*				(*_req_stat)(uint64_t desc);
};
#endif

extern const struct singlenode_desc_ops_t singlenode_desc_ops_norm;
extern const struct singlenode_desc_ops_t singlenode_desc_ops_smvr;
/* extern const struct singlenode_desc_ops_t singlenode_desc_ops_vsdof; */

static inline const struct singlenode_desc_ops_t*
fetch_singlenode_desc_ops(uint32_t request_type) {
	switch (request_type) {
	case V4L2_META_FMT_MTISP_SDNORM:
		return &singlenode_desc_ops_norm;
	case V4L2_META_FMT_MTISP_SD:
		return &singlenode_desc_ops_smvr;
	default:
		return &singlenode_desc_ops_norm;
	}
}

static inline unsigned int
fetch_singlenode_desc_tmax(uint32_t request_type) {
	switch (request_type) {
	case V4L2_META_FMT_MTISP_SDNORM:
		return TMAX;
	case V4L2_META_FMT_MTISP_SD:
		return TIME_MAX;
	default:
		return TMAX;
	}
}

#endif  // _MTK_IMGSYS_SINGLENODE_DESC_OPS_H_
