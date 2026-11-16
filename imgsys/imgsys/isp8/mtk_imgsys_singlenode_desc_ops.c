// SPDX-License-Identifier: GPL-2.0
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
 * Copyright  (C) 2022.  MediaTek Inc. All rights reserved.
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

/* local */
#include "mtk_imgsys_singlenode_desc_ops.h"

/******************************************************************************
 *
 ******************************************************************************/
#if defined (IMGSYS_SINGLE_NODE_DESC_V2)
#define DEFINE_SINGLENODE_DESC_OPS(_struct_, _name_)                                                            \
	static uint8_t                                                                                          \
	_dmas_num_of_singlenode_desc_ops_##_name_(uint64_t desc, uint32_t t) {                                  \
		return (((((_struct_ *)desc)->frames)[t]).dmas_num);                                            \
	}                                                                                                       \
	static uint8_t                                                                                          \
	_dmas_enable_of_singlenode_desc_ops_##_name_(uint64_t desc,                                             \
		uint32_t dma_id, uint32_t t) {                                                                  \
		return ((((((_struct_ *)desc)->frames)[t]).dmas_enable[dma_id]) != IMGSYS_INVALID_DMA_IDX);     \
	}                                                                                                       \
	static uint32_t                                                                                         \
	_frame_num_of_singlenode_desc_ops_##_name_(uint64_t desc) {                                             \
		return (((_struct_ *)desc)->frames_num);                                                        \
	}                                                                                                       \
	static struct frameparams*                                                                              \
	_dmas_of_singlenode_desc_ops_##_name_(uint64_t desc,                                                    \
		uint32_t dma_id, uint32_t t) {                                                                  \
		return (struct frameparams *)((uint64_t)&(((((_struct_ *)desc)->frames)[t]).dmas[               \
			(((((_struct_ *)desc)->frames)[t]).dmas_enable[dma_id])]));                              \
	}                                                                                                       \
	static struct frameparams*                                                                              \
	_tuning_meta_of_singlenode_desc_ops_##_name_(uint64_t desc, uint32_t t) {                               \
		return (struct frameparams *)((uint64_t)&(((((_struct_ *)desc)->frames)[t]).tuning_meta));      \
	}                                                                                                       \
	static struct frameparams*                                                                              \
	_ctrl_meta_of_singlenode_desc_ops_##_name_(uint64_t desc, uint32_t t) {                                 \
		return (struct frameparams *)((uint64_t)&(((((_struct_ *)desc)->frames)[t]).ctrl_meta));        \
	}                                                                                                       \
	static struct frameparams*                                                                              \
	_ctrl_meta_from_user_of_singlenode_desc_ops_##_name_(uint64_t desc, uint32_t t) {                       \
		return (struct frameparams *)((uint64_t)&(((((_struct_ *)desc)->frames)[t]).ctrl_meta_from_user));\
	}                                                                                                       \
	static struct frameparams*                                                                              \
	_ctrl_meta_from_user_mae_of_singlenode_desc_ops_##_name_(uint64_t desc, uint32_t t) {                   \
		return (struct frameparams *)((uint64_t)&(((((_struct_ *)desc)->frames)[t]).ctrl_meta_from_user_mae));\
	}                                                                                                       \
	static uint64_t*                                                                                        \
	_req_stat_of_singlenode_desc_ops_##_name_(uint64_t desc) {                                              \
		return &(((_struct_ *)desc)->req_stat);                                                         \
	}                                                                                                       \
	const struct singlenode_desc_ops_t singlenode_desc_ops_##_name_ = {                                     \
		._dmas_num    = _dmas_num_of_singlenode_desc_ops_##_name_,                                      \
		._dmas_enable = _dmas_enable_of_singlenode_desc_ops_##_name_,                                   \
		._dmas        = _dmas_of_singlenode_desc_ops_##_name_,                                          \
		._frame_num   = _frame_num_of_singlenode_desc_ops_##_name_,                                     \
		._tuning_meta = _tuning_meta_of_singlenode_desc_ops_##_name_,                                   \
		._ctrl_meta   = _ctrl_meta_of_singlenode_desc_ops_##_name_,                                     \
		._ctrl_meta_from_user =                                                                         \
			_ctrl_meta_from_user_of_singlenode_desc_ops_##_name_,                                   \
		._ctrl_meta_from_user_mae =                                                                     \
			_ctrl_meta_from_user_mae_of_singlenode_desc_ops_##_name_,                               \
		._req_stat = _req_stat_of_singlenode_desc_ops_##_name_,                                         \
	}
DEFINE_SINGLENODE_DESC_OPS(struct singlenode_desc_normal, norm);
DEFINE_SINGLENODE_DESC_OPS(struct singlenode_desc_smvr, smvr);
//DEFINE_SINGLENODE_DESC_OPS(struct singlenode_desc_vsdof, vsdof);
#else
#define DEFINE_HEADER_DESC_OPS(_struct_, _name_)                                    \
	static uint32_t                                                             \
	_fparams_tnum_of_header_desc_ops_##_name_(                                  \
		uint64_t hdr_desc) {                                                \
		return ((_struct_ *)hdr_desc)->fparams_tnum;                        \
	}                                                                           \
	static struct buf_info*                                                     \
	_buf_info_of_header_desc_ops_##_name_(                                      \
		uint64_t hdr_desc, uint32_t t) {                                    \
		return &(((_struct_ *)hdr_desc)->fparams[t][0].bufs[0]);            \
	}                                                                           \
	static struct frameparams*                                                  \
	_fparams_of_header_desc_ops_##_name_(                                       \
		uint64_t hdr_desc, uint32_t t) {                                    \
		return &(((_struct_ *)hdr_desc)->fparams[t][0]);                    \
	}                                                                           \
	static uint32_t                                                             \
	_plane_num_of_header_desc_ops_##_name_(                                     \
		uint64_t hdr_desc, uint32_t t) {                                    \
		return ((_struct_ *)hdr_desc)->fparams[t][0].bufs[0].buf.num_planes;\
	}                                                                           \
	static uint32_t                                                             \
	_buf_fd_of_header_desc_ops_##_name_(                                        \
		uint64_t hdr_desc, uint32_t t, uint32_t p) {                        \
		return ((_struct_ *)hdr_desc)->fparams[t][0].bufs[0].buf.planes[p].m.dma_buf.fd;\
	}                                                                           \
	static uint32_t                                                             \
	_buf_offset_of_header_desc_ops_##_name_(                                    \
		uint64_t hdr_desc, uint32_t t, uint32_t p) {                        \
		return ((_struct_ *)hdr_desc)->fparams[t][0].bufs[0].buf.planes[p].m.dma_buf.offset;\
	}                                                                           \
	static const struct header_desc_ops_t header_desc_ops_##_name_ = {          \
		._fparams_tnum = _fparams_tnum_of_header_desc_ops_##_name_,         \
		._buf_info = _buf_info_of_header_desc_ops_##_name_,                 \
		._fparams = _fparams_of_header_desc_ops_##_name_,                   \
		._plane_num = _plane_num_of_header_desc_ops_##_name_,               \
		._buf_fd = _buf_fd_of_header_desc_ops_##_name_,                     \
		._buf_offset = _buf_offset_of_header_desc_ops_##_name_,             \
	}
DEFINE_HEADER_DESC_OPS(struct header_desc_norm, norm);
DEFINE_HEADER_DESC_OPS(struct header_desc,      smvr);

#define DEFINE_SINGLENODE_DESC_OPS(_struct_, _name_)                                \
	static uint8_t                                                              \
	_dmas_enable_of_singlenode_desc_ops_##_name_(uint64_t desc,                 \
		uint32_t dma_id, uint32_t t) {                                      \
		return (((_struct_ *)desc)->dmas_enable[dma_id][t]);                \
	}                                                                           \
	static uint64_t                                                             \
	_dmas_of_singlenode_desc_ops_##_name_(uint64_t desc, uint32_t dma_id) {     \
		return (uint64_t)&(((_struct_ *)desc)->dmas[dma_id]);               \
	}                                                                           \
	static uint64_t                                                             \
	_tuning_meta_of_singlenode_desc_ops_##_name_(uint64_t desc) {               \
		return (uint64_t)&(((_struct_ *)desc)->tuning_meta);                \
	}                                                                           \
	static uint64_t                                                             \
	_ctrl_meta_of_singlenode_desc_ops_##_name_(uint64_t desc) {                 \
		return (uint64_t)&(((_struct_ *)desc)->ctrl_meta);                  \
	}                                                                           \
	static uint64_t                                                             \
	_ctrl_meta_from_user_of_singlenode_desc_ops_##_name_(uint64_t desc) {       \
		return (uint64_t)&(((_struct_ *)desc)->ctrl_meta_from_user);        \
	}                                                                           \
	static uint64_t*                                                            \
	_req_stat_of_singlenode_desc_ops_##_name_(uint64_t desc) {                  \
		return &(((_struct_ *)desc)->req_stat);                             \
	}                                                                           \
	const struct singlenode_desc_ops_t singlenode_desc_ops_##_name_ = {         \
		.hdr_ops = &header_desc_ops_##_name_,                               \
		._dmas_enable = &_dmas_enable_of_singlenode_desc_ops_##_name_,      \
		._dmas        = &_dmas_of_singlenode_desc_ops_##_name_,             \
		._tuning_meta = &_tuning_meta_of_singlenode_desc_ops_##_name_,      \
		._ctrl_meta   = &_ctrl_meta_of_singlenode_desc_ops_##_name_,        \
		._ctrl_meta_from_user =                                             \
			&_ctrl_meta_from_user_of_singlenode_desc_ops_##_name_,      \
		._req_stat = _req_stat_of_singlenode_desc_ops_##_name_,             \
	}
DEFINE_SINGLENODE_DESC_OPS(struct singlenode_desc_norm, norm);
DEFINE_SINGLENODE_DESC_OPS(struct singlenode_desc, smvr);
#endif
