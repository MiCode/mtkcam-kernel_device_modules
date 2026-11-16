// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/timer.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define_v4l2.h"
#include "kd_imgsensor_errcode.h"


#include "adaptor.h"
#include "adaptor-subdrv-ctrl.h"
#include "adaptor-i2c.h"
#include "adaptor-ctrls.h"
#include "adaptor-util.h"
#include "adaptor-hw.h"
#include "adaptor-trace.h"

static const char * const clk_names[] = {
	ADAPTOR_CLK_NAMES
};

static const char * const reg_names[] = {
	ADAPTOR_REGULATOR_NAMES
};

static const char * const state_names[] = {
	ADAPTOR_STATE_NAMES
};

void check_current_scenario_id_bound(struct subdrv_ctx *ctx)
{
	if (ctx->current_scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid cur_sid:%u, mode_num:%u set default\n",
			ctx->current_scenario_id, ctx->s_ctx.sensor_mode_num);
		ctx->current_scenario_id = 0;
	}
}

void i2c_table_write(struct subdrv_ctx *ctx, u16 *list, u32 len)
{
	switch (ctx->s_ctx.i2c_transfer_data_type) {
	case I2C_DT_ADDR_16_DATA_16:
		subdrv_i2c_wr_regs_u16(ctx, list, len);
		break;
	case I2C_DT_ADDR_8_DATA_8:
		subdrv_i2c_wr_regs_u8_u8(ctx, list, len);
		break;
	case I2C_DT_ADDR_16_DATA_8:
	default:
		subdrv_i2c_wr_regs_u8(ctx, list, len);
		break;
	}
}

u64 ixc_table_write(struct subdrv_ctx *ctx, u16 *list, u32 len)
{
	u64 ixc_start = 0, ixc_end = 0;

	ixc_start = ktime_get_boottime_ns();
	switch (ctx->s_ctx.i2c_transfer_data_type) {
	case I2C_DT_ADDR_16_DATA_16:
		subdrv_ixc_wr_regs_u16(ctx, list, len);
		break;
	case I2C_DT_ADDR_8_DATA_8:
		subdrv_ixc_wr_regs_u8_u8(ctx, list, len);
		break;
	case I2C_DT_ADDR_16_DATA_8:
	default:
		subdrv_ixc_wr_regs_u8(ctx, list, len);
		break;
	}
	ixc_end = ktime_get_boottime_ns();

	return (ixc_end-ixc_start)/1000;
}

u64 ixc_table_write_with_entry(struct subdrv_ctx *ctx, struct reg_setting_entry *entry)
{
	u64 ixc_start = 0, ixc_end = 0;
	u16 *list_16;
	u8 *list_8;
	u32 len;
	int i;

	if (unlikely(entry == NULL || entry->setting_table == NULL || entry->setting_table_len < 1))
		return 0;

	list_16 = entry->setting_table;
	len = entry->setting_table_len;

	ixc_start = ktime_get_boottime_ns();
	switch (entry->i2c_transfer_tlb_data_type) {
	case I2C_TABLE_DT_ADDR_16_DATA_16_BURST:
		subdrv_ixc_wr_p16(ctx, list_16[0], list_16 + 1, len - 1);
		break;
	case I2C_TABLE_DT_ADDR_16_DATA_8_BURST:
		list_8 = kzalloc(sizeof(u8) * (len - 1), GFP_KERNEL);
		if (list_8) {
			for (i = 0; i < len - 1; i++)
				list_8[i] = (u8) list_16[i + 1];
			subdrv_ixc_wr_p8(ctx, list_16[0], list_8, len - 1);
		}
		break;
	case I2C_TABLE_DT_ADDR_16_DATA_8_SEQ:
		list_8 = kzalloc(sizeof(u8) * (len - 1), GFP_KERNEL);
		if (list_8) {
			for (i = 0; i < len - 1; i++)
				list_8[i] = (u8) list_16[i + 1];
			subdrv_ixc_wr_seq_p8(ctx, list_16[0], list_8, len - 1);
		}
		break;
	case I2C_TABLE_DT_ADDR_16_DATA_16:
		subdrv_ixc_wr_regs_u16(ctx, list_16, len);
		break;
	case I2C_TABLE_DT_ADDR_8_DATA_8:
		subdrv_ixc_wr_regs_u8_u8(ctx, list_16, len);
		break;
	case I2C_TABLE_DT_ADDR_16_DATA_8:
	default:
		subdrv_ixc_wr_regs_u8(ctx, list_16, len);
		break;
	}
	ixc_end = ktime_get_boottime_ns();

	return (ixc_end - ixc_start) / 1000;
}

static void dump_i2c_buf(struct subdrv_ctx *ctx)
{
	int i, j;
	char *out_str = NULL;
	char *strptr = NULL;
	size_t buf_size = MAX_BUF_SIZE_U8 * sizeof(char);
	size_t remind = buf_size;
	int num = 0;

	out_str = kzalloc(buf_size + 1, GFP_KERNEL);
	if (!out_str)
		return;

	strptr = out_str;
	for (i = 0, j = i; ctx->_size_to_write > (i + 2); i += 3) {
		num = snprintf(strptr, remind,
			       "%d, [0x%02x%02x, 0x00%02x] ",
			       i,
			       ctx->i2cmem.buf[i],
			       ctx->i2cmem.buf[i + 1],
			       ctx->i2cmem.buf[i + 2]);

		if (num <= 0) {
			DRV_LOG(ctx, "snprintf return negative at line %d\n", __LINE__);
			kfree(out_str);
			return;
		}

		remind -= num;
		strptr += num;

		if (remind <= 20) {
			DRV_LOG(ctx, "remind <= 20 write idx %d to %d: %s\n",
				j, i + 1, out_str);
			j = i + 3;
			strptr = out_str;
			remind = buf_size;
		}
	}
	if (ctx->_size_to_write > i) {
		num = snprintf(strptr, remind,
			       "[0x00%02x]",
			       ctx->i2cmem.buf[i]);
		DRV_LOGE(ctx, "ctx->_size_to_write > i at line %d\n", __LINE__);

		if (num <= 0) {
			DRV_LOG(ctx, "snprintf return negative at line %d\n", __LINE__);
			kfree(out_str);
			return;
		}

		i++;
		remind -= num;
		strptr += num;
	}
	if (strptr != out_str) {
		DRV_LOG(ctx, "strptr != out_str write idx %d to %d: %s\n",
			j, i - 1, out_str);
		strptr = out_str;
		remind = buf_size;
	}

	kfree(out_str);
}

#define DUMP_I2C_BUF_IF_DEBUG(ctx) do { \
	struct v4l2_subdev *_sd = NULL; \
	struct adaptor_ctx *_adaptor_ctx = NULL; \
	if (ctx->i2c_client) \
		_sd = i2c_get_clientdata(ctx->i2c_client); \
	if (ctx->ixc_client.protocol) \
		_sd = adaptor_ixc_get_clientdata(&ctx->ixc_client); \
	if (_sd) \
		_adaptor_ctx = to_ctx(_sd); \
	if (_adaptor_ctx && (_adaptor_ctx)->subdrv \
		&& unlikely(*((_adaptor_ctx)->sensor_debug_flag))) { \
		dump_i2c_buf(ctx); \
	} \
} while (0)

void commit_i2c_buffer(struct subdrv_ctx *ctx)
{
	int ret;
	struct device *dev = adaptor_ixc_get_dev(&ctx->ixc_client);

	if (ctx->_size_to_write && !ctx->fast_mode_on) {
		mutex_lock(&ctx->i2c_buffer_lock);
		ret = i3c_i2c_transfer(&ctx->ixc_client, ctx->i2cmem.msg, ctx->_i2c_msg_size);
		DUMP_I2C_BUF_IF_DEBUG(ctx);
		memset(&ctx->i2cmem, 0x0, sizeof(ctx->i2cmem));
		ctx->_size_to_write = 0;
		ctx->_i2c_msg_size = 0;
		mutex_unlock(&ctx->i2c_buffer_lock);
		if (ret < 0){
			dev_info(dev, "[%s]ixc transfer failed (%d)\n", __func__,ret);
			return;
		}
	}
}

void set_i2c_buffer(struct subdrv_ctx *ctx, u16 reg, u16 val)
{
	u8 *pbuf;
	struct i3c_i2c_xfer *pmsg;

	if (ctx->_size_to_write + 3 >= MAX_BUF_SIZE_U8) {
		DRV_LOGE(ctx, "[%s] is full and forced to commit\n", __func__);
		commit_i2c_buffer(ctx);
	}
	if (!ctx->fast_mode_on) {
		mutex_lock(&ctx->i2c_buffer_lock);
		pbuf = &ctx->i2cmem.buf[ctx->_size_to_write];
		pmsg = &ctx->i2cmem.msg[ctx->_i2c_msg_size];

		pbuf[0] = reg >> 8;
		pbuf[1] = reg & 0xff;
		pbuf[2] = val & 0xff;
		if (ctx->ixc_client.protocol == I2C_PROTOCOL)
			pmsg->addr = ctx->i2c_write_id >> 1;

		pmsg->flags = 0;
		pmsg->len = 3;
		pmsg->buf = pbuf;
		ctx->_i2c_msg_size++;
		ctx->_size_to_write = ctx->_size_to_write +3;
		mutex_unlock(&ctx->i2c_buffer_lock);
	}
}

void set_table_to_buffer(struct subdrv_ctx *ctx, u16 *list, u32 len)
{
	u8 *pbuf;
	struct i3c_i2c_xfer *pmsg;

	for (int i = 0; i < len;) {
		if ((ctx->_size_to_write + 3) >= MAX_BUF_SIZE_U8) {
			DRV_LOGE(ctx, "[%s] is full and forced to commit\n", __func__);
			commit_i2c_buffer(ctx);
		}
		if (!ctx->fast_mode_on) {
			mutex_lock(&ctx->i2c_buffer_lock);

			pbuf = &ctx->i2cmem.buf[ctx->_size_to_write];
			pmsg = &ctx->i2cmem.msg[ctx->_i2c_msg_size];

			pbuf[0] = list[i] >> 8;
			pbuf[1] = list[i] & 0xff;
			pbuf[2] = list[i+1] & 0xff;

			if (ctx->ixc_client.protocol == I2C_PROTOCOL)
				pmsg->addr = ctx->i2c_write_id >> 1;

			pmsg->flags = 0;
			pmsg->len = 3;
			pmsg->buf = pbuf;
			ctx->_i2c_msg_size++;
			ctx->_size_to_write = ctx->_size_to_write +3;
			mutex_unlock(&ctx->i2c_buffer_lock);
			i =  i + 2;
		}
	}
}

u16 i2c_multi_read_eeprom(struct subdrv_ctx *ctx, u16 addr, u16 size, u8 *pbuf)
{
	u8 write_id;
	int ret = read_cam_cal(ctx->s_ctx.sensor_id, pbuf, addr, size);

	if (ret < 0) {
		if (ctx->eeprom_index < ctx->s_ctx.eeprom_num) {
			DRV_LOG_MUST(ctx, "addr = 0x%x size = 0x%x", addr, size);
			write_id = ctx->s_ctx.eeprom_info[ctx->eeprom_index].i2c_write_id;
			adaptor_i2c_rd_p8(ctx->i2c_client, write_id >> 1, addr, pbuf, size);
		} else {
			DRV_LOGE(ctx, "eeprom_index(%u) out of bound eeprom_num(%u)\n",
				ctx->eeprom_index, ctx->s_ctx.eeprom_num);
		}
	} else if (ret == 0) {
		DRV_LOGE(ctx, "read_cam_cal failed\n");
	}

	return 0;
}

void get_pdaf_reg_setting(struct subdrv_ctx *ctx, u32 regNum, u16 *regDa)
{
	int i, idx;

	for (i = 0; i < regNum; i++) {
		idx = 2 * i;
		regDa[idx + 1] = subdrv_ixc_rd_u8(ctx, regDa[idx]);
	}
	DRV_LOG(ctx, "register num:%u\n", regNum);
}

void set_pdaf_reg_setting(struct subdrv_ctx *ctx, u32 regNum, u16 *regDa)
{
	subdrv_ixc_wr_regs_u8(ctx, regDa, regNum*2);
}

void set_mirror_flip(struct subdrv_ctx *ctx, u8 image_mirror)
{
	u8 itemp = 0;

	if (ctx->s_ctx.reg_addr_mirror_flip == PARAM_UNDEFINED) {
		DRV_LOG(ctx, "sensor no support\n");
		return;
	}
	if (ctx->s_ctx.aov_sensor_support &&
		ctx->s_ctx.reg_addr_aov_mode_mirror_flip)
		itemp = subdrv_ixc_rd_u8(ctx,
			ctx->s_ctx.reg_addr_aov_mode_mirror_flip) & ~0x03;
	else
		itemp = subdrv_ixc_rd_u8(ctx, ctx->s_ctx.reg_addr_mirror_flip) & ~0x03;
	switch (image_mirror) {
	case IMAGE_NORMAL:
		itemp |= 0x00;
		break;
	case IMAGE_V_MIRROR:
		itemp |= 0x02;
		break;
	case IMAGE_H_MIRROR:
		itemp |= 0x01;
		break;
	case IMAGE_HV_MIRROR:
		itemp |= 0x03;
		break;
	default:
		DRV_LOGE(ctx, "there is something wrong. mirror:%u\n", image_mirror);
		break;
	}
	subdrv_ixc_wr_u8(ctx, ctx->s_ctx.reg_addr_mirror_flip, itemp);
	DRV_LOG(ctx, "mirror:%u\n", image_mirror);
}

bool probe_eeprom(struct subdrv_ctx *ctx)
{
	u32 eeprom_num = 0;
	u16 idx = 0;
	u32 header_id = 0;
	u32 addr_header_id = 0;
	struct eeprom_info_struct *info = ctx->s_ctx.eeprom_info;

	if (info == NULL) {
		DRV_LOG_MUST(ctx, "sensor no support eeprom\n");
		return FALSE;
	}

	eeprom_num = ctx->s_ctx.eeprom_num;
	if (ctx->eeprom_index < ctx->s_ctx.eeprom_num) {
		DRV_LOG_MUST(ctx, "index:%u\n", ctx->eeprom_index);
		return TRUE;
	}

	for (idx = 0; idx < eeprom_num; idx++) {
		ctx->eeprom_index = idx;
		addr_header_id = info[idx].addr_header_id;
		i2c_multi_read_eeprom(ctx, addr_header_id, sizeof(header_id), (u8 *)&header_id);
		DRV_LOG_MUST(ctx, "eeprom index[cur/total]:%u/%u, header id[cur/exp]:0x%08x/0x%08x\n",
			idx, eeprom_num, header_id, info[idx].header_id);
		if (header_id == info[idx].header_id) {
			DRV_LOG_MUST(ctx, "Probe done. index:%u\n", ctx->eeprom_index);
			return TRUE;
		}
	}
	ctx->eeprom_index = idx;
	DRV_LOG_MUST(ctx, "Probe failed! No EEPROM device.\n");
	return FALSE;
}

void read_sensor_Cali(struct subdrv_ctx *ctx)
{
	u16 idx = 0;
	u8 support = FALSE;
	u8 *buf = NULL;
	u16 size = 0;
	u16 addr = 0;
	struct eeprom_info_struct *info = ctx->s_ctx.eeprom_info;

	/* Probe EEPROM device */
	if (!probe_eeprom(ctx))
		return;

	idx = ctx->eeprom_index;

	/* QSC data */
	support = info[idx].qsc_support;
	size = info[idx].qsc_size;
	addr = info[idx].addr_qsc;
	buf = info[idx].qsc_table;
	if (support && size > 0) {
		if (info[idx].preload_qsc_table == NULL) {
			info[idx].preload_qsc_table = kmalloc(size, GFP_KERNEL);
			if (buf == NULL)
				i2c_multi_read_eeprom(ctx, addr, size, info[idx].preload_qsc_table);
			else
				memcpy(info[idx].preload_qsc_table, buf, size);
			DRV_LOG(ctx, "preload QSC data %u bytes", size);
		} else {
			DRV_LOG(ctx, "QSC data is already preloaded %u bytes", size);
		}
	}

	/* PDC data */
	support = info[idx].pdc_support;
	size = info[idx].pdc_size;
	addr = info[idx].addr_pdc;
	buf = info[idx].pdc_table;
	if (support && size > 0) {
		if (info[idx].preload_pdc_table == NULL) {
			info[idx].preload_pdc_table = kmalloc(size, GFP_KERNEL);
			if (buf == NULL)
				i2c_multi_read_eeprom(ctx, addr, size, info[idx].preload_pdc_table);
			else
				memcpy(info[idx].preload_pdc_table, buf, size);
			DRV_LOG(ctx, "preload PDC data %u bytes", size);
		} else {
			DRV_LOG(ctx, "PDC data is already preloaded %u bytes", size);
		}
	}

	/* LRC data */
	support = info[idx].lrc_support;
	size = info[idx].lrc_size;
	addr = info[idx].addr_lrc;
	buf = info[idx].lrc_table;
	if (support && size > 0) {
		if (info[idx].preload_lrc_table == NULL) {
			info[idx].preload_lrc_table = kmalloc(size, GFP_KERNEL);
			if (buf == NULL)
				i2c_multi_read_eeprom(ctx, addr, size, info[idx].preload_lrc_table);
			else
				memcpy(info[idx].preload_lrc_table, buf, size);
			DRV_LOG(ctx, "preload LRC data %u bytes", size);
		} else {
			DRV_LOG(ctx, "LRC data is already preloaded %u bytes", size);
		}
	}

	/* XTALK data */
	support = info[idx].xtalk_support;
	size = info[idx].xtalk_size;
	addr = info[idx].addr_xtalk;
	buf = info[idx].xtalk_table;
	if (support && size > 0) {
		if (info[idx].preload_xtalk_table == NULL) {
			info[idx].preload_xtalk_table = kmalloc(size, GFP_KERNEL);
			if (buf == NULL)
				i2c_multi_read_eeprom(ctx, addr, size, info[idx].preload_xtalk_table);
			else
				memcpy(info[idx].preload_xtalk_table, buf, size);
			DRV_LOG(ctx, "preload XTALK data %u bytes", size);
		} else {
			DRV_LOG(ctx, "XTALK data is already preloaded %u bytes", size);
		}
	}

	ctx->is_read_preload_eeprom = 1;
}

void write_sensor_Cali(struct subdrv_ctx *ctx)
{
	DRV_LOG(ctx, "no calibration data applied to sensor.");
}

void check_frame_length_limitation(struct subdrv_ctx *ctx)
{
	DRV_LOG(ctx, "no calibration data applied to sensor.");
}

void check_write_frame_length_need_lshift(struct subdrv_ctx *ctx, u32 *fll)
{
	u8 lshift_max = ctx->s_ctx.fll_lshift_max ? ctx->s_ctx.fll_lshift_max : DEFAULT_LSHIFT_MAX;
	u32 l_framelength = 0;
	u16 l_shift = 0;
	u32 frame_length_max_without_lshift = ctx->s_ctx.frame_length_max_without_lshift ?
		ctx->s_ctx.frame_length_max_without_lshift : ctx->s_ctx.frame_length_max;

	if (!(*fll > frame_length_max_without_lshift)) {
		if (ctx->s_ctx.reg_addr_frame_length_lshift != PARAM_UNDEFINED)
			set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_frame_length_lshift, 0);
		return;
	}

	if (ctx->s_ctx.reg_addr_frame_length_lshift == PARAM_UNDEFINED) {
		DRV_LOGE(ctx,
				"reg_addr_frame_length_lshift == PARAM_UNDEFINED, fll:0x%x", (*fll));
		return;
	}

	for (l_shift = 1; l_shift < lshift_max; l_shift++) {
		l_framelength = (((*fll) - 1) >> l_shift) + 1;
		if (l_framelength < frame_length_max_without_lshift)
			break;
	}
	if (l_shift > lshift_max) {
		DRV_LOGE(ctx,
				"unable to set framelength l_shift:%u, set to lshift_max:%u\n",
				l_shift, lshift_max);
		l_shift = lshift_max;
	}
	(*fll) = (((*fll) - 1) >> l_shift) + 1;

	set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_frame_length_lshift, l_shift);
	DRV_LOG_MUST(ctx,
			"write framelength with lshift fll:0x%x fll_lshift:0x%x",
			(*fll), l_shift);
}

void write_frame_length(struct subdrv_ctx *ctx, u32 fll)
{
	u32 addr_h = ctx->s_ctx.reg_addr_frame_length.addr[0];
	u32 addr_l = ctx->s_ctx.reg_addr_frame_length.addr[1];
	u32 addr_ll = ctx->s_ctx.reg_addr_frame_length.addr[2];
	u32 fll_step = 0;
	u32 dol_cnt = 1;

	check_current_scenario_id_bound(ctx);
	fll_step = ctx->s_ctx.mode[ctx->current_scenario_id].framelength_step;
	if (fll_step)
		fll = roundup(fll, fll_step);
	ctx->frame_length = fll;

	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_STAGGER)
		dol_cnt = ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt;

	if (ctx->s_ctx.stagger_fl_type == IMGSENSOR_STAGGER_FL_AUTO_DIVIDED)
		fll = fll / dol_cnt;

	check_write_frame_length_need_lshift(ctx, &fll);

	if (ctx->extend_frame_length_en == FALSE) {
		if (addr_ll) {
			set_i2c_buffer(ctx,	addr_h,	(fll >> 16) & 0xFF);
			set_i2c_buffer(ctx,	addr_l, (fll >> 8) & 0xFF);
			set_i2c_buffer(ctx,	addr_ll, fll & 0xFF);
		} else {
			set_i2c_buffer(ctx,	addr_h, (fll >> 8) & 0xFF);
			set_i2c_buffer(ctx,	addr_l, fll & 0xFF);
		}
		/* update FL RG value after setting buffer for writing RG */
		ctx->frame_length_rg = ctx->frame_length;

		DRV_LOG(ctx,
			"ctx:(fl(RG):%u), fll[0x%x] multiply %u, fll_step:%u\n",
			ctx->frame_length_rg, fll, dol_cnt, fll_step);
	}
}
/**
 * @brief: implementation for updating fll addr for lbmf:
 * for auto mode, the FLL addr is equal to general FLL.
 * for manual mode, new member declaired in s_ctx structure such as
 * reg_addr_frame_length_in_lbmf for storing  fll_a, fll_b, etc.
 * @param ctx: subdrv_ctx
 * @param fll: frame length lines for auto mode
 * @param fll_in_lut: frame length array in lut for manual mode
 */
void write_frame_length_in_lut(struct subdrv_ctx *ctx, u32 fll, u32 *fll_in_lut)
{
	int i = 0;
	u32 fll_step = 0;

	check_current_scenario_id_bound(ctx);
	fll_step = ctx->s_ctx.mode[ctx->current_scenario_id].framelength_step;

	// manual mode
	switch (ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt) {
	case 2:
		if (fll_step) {
			fll_in_lut[0] =
				roundup(fll_in_lut[0], fll_step);
			fll_in_lut[1] =
				roundup(fll_in_lut[1], fll_step);
		}
		fll_in_lut[2] = 0;
		fll_in_lut[3] = 0;
		fll_in_lut[4] = 0;
		ctx->frame_length_in_lut[0] = fll_in_lut[0];
		ctx->frame_length_in_lut[1] = fll_in_lut[1];
		if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 2) {
			ctx->frame_length =
				ctx->frame_length_in_lut[0] +
				ctx->frame_length_pre_store_in_lut[1];
		} else if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 3) {
			ctx->frame_length =
				ctx->frame_length_in_lut[0] + ctx->frame_length_in_lut[1];
		} else {
			DRV_LOGE(ctx, "pls check delay_frame value!\n");
			return;
		}
		break;
	case 3:
		if (fll_step) {
			fll_in_lut[0] =
				roundup(fll_in_lut[0], fll_step);
			fll_in_lut[1] =
				roundup(fll_in_lut[1], fll_step);
			fll_in_lut[2] =
				roundup(fll_in_lut[2], fll_step);
		}
		fll_in_lut[3] = 0;
		fll_in_lut[4] = 0;
		ctx->frame_length_in_lut[0] = fll_in_lut[0];
		ctx->frame_length_in_lut[1] = fll_in_lut[1];
		ctx->frame_length_in_lut[2] = fll_in_lut[2];
		if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 2) {
			ctx->frame_length =
				ctx->frame_length_in_lut[0] +
				ctx->frame_length_pre_store_in_lut[1] +
				ctx->frame_length_pre_store_in_lut[2];
		} else if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 3) {
			ctx->frame_length =
				ctx->frame_length_in_lut[0] +
				ctx->frame_length_in_lut[1] +
				ctx->frame_length_in_lut[2];
		} else {
			DRV_LOGE(ctx, "pls check delay_frame value!\n");
			return;
		}
		break;
	default:
		break;
	}

	if (ctx->extend_frame_length_en == FALSE) {
		for (i = 0; i < 3; i++) {
			if (fll_in_lut[i]) {
				if (ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[2]) {
					set_i2c_buffer(ctx,
						ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[0],
						(fll_in_lut[i] >> 16) & 0xFF);
					set_i2c_buffer(ctx,
						ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[1],
						(fll_in_lut[i] >> 8) & 0xFF);
					set_i2c_buffer(ctx,
						ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[2],
						fll_in_lut[i] & 0xFF);
				} else {
					set_i2c_buffer(ctx,
						ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[0],
						(fll_in_lut[i] >> 8) & 0xFF);
					set_i2c_buffer(ctx,
						ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[1],
						fll_in_lut[i] & 0xFF);
				}
				/* update FL_lut RG value after setting buffer for writing RG */
				ctx->frame_length_in_lut_rg[i] = fll_in_lut[i];
			}
		}
		/* update FL RG value simultaneously */
		ctx->frame_length_rg = ctx->frame_length;

		DRV_LOG(ctx,
			"ctx:(fl(RG):%u,%u/%u/%u/%u/%u), scen_id:%u,fll(input/ctx/output_a/b/c/d/e):0x%x/%x/%x/%x/%x/%x/%x,fl(pre_store):%u/%u/%u/%u/%u,fll_step:%u\n",
			ctx->frame_length_rg,
			ctx->frame_length_in_lut_rg[0],
			ctx->frame_length_in_lut_rg[1],
			ctx->frame_length_in_lut_rg[2],
			ctx->frame_length_in_lut_rg[3],
			ctx->frame_length_in_lut_rg[4],
			ctx->current_scenario_id,
			fll,
			ctx->frame_length,
			fll_in_lut[0],
			fll_in_lut[1],
			fll_in_lut[2],
			fll_in_lut[3],
			fll_in_lut[4],
			ctx->frame_length_pre_store_in_lut[0],
			ctx->frame_length_pre_store_in_lut[1],
			ctx->frame_length_pre_store_in_lut[2],
			ctx->frame_length_pre_store_in_lut[3],
			ctx->frame_length_pre_store_in_lut[4],
			fll_step);
	} else {
		DRV_LOG(ctx,
			"sid:%u,extend_frame_length_en:%u,default won't write fll!\n",
			ctx->current_scenario_id, ctx->extend_frame_length_en);
		return;
	}
}
/**
 * @brief: implementation for updating fll addr for dcg+vs:
 * for auto mode, the FLL addr is equal to general FLL.
 * for manual mode, new member declaired in s_ctx structure such as
 * reg_addr_frame_length_in_lbmf for storing  fll_a, fll_b, etc.
 * @param ctx: subdrv_ctx
 * @param fll: frame length lines for auto mode
 * @param fll_in_lut: frame length array in lut for manual mode
 */
void write_dcg_vs_frame_length_in_lut(struct subdrv_ctx *ctx, u32 fll, u32 *fll_in_lut)
{
	int i = 0;
	u32 frame_length_buf;
	u32 fl_a_step = 0, fl_b_step = 0;
	struct struct_dcg_vsl_info dcg_vsl_info = {0};
	u32 linetime_ns_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT];

	check_current_scenario_id_bound(ctx);

	get_dcg_vsl_info_by_scenario(ctx, ctx->current_scenario_id, &dcg_vsl_info);

	memset(linetime_ns_in_lut, 0, sizeof(linetime_ns_in_lut));

	// manual mode
	switch (dcg_vsl_info.lut_cnt) {
	case 2:
		/* lut-A & lut-B */
		fl_a_step = dcg_vsl_info.lut_info[IMGSENSOR_LUT_A].framelength_step;
		fl_b_step = dcg_vsl_info.lut_info[IMGSENSOR_LUT_B].framelength_step;
		if (fl_a_step)
			fll_in_lut[0] = roundup(fll_in_lut[0], fl_a_step);
		if (fl_b_step)
			fll_in_lut[1] = roundup(fll_in_lut[1], fl_b_step);
		fll_in_lut[2] = 0;
		fll_in_lut[3] = 0;
		fll_in_lut[4] = 0;
		ctx->frame_length_in_lut[0] = fll_in_lut[0];
		ctx->frame_length_in_lut[1] = fll_in_lut[1];
		ctx->frame_length_in_lut[2] = fll_in_lut[2];

		linetime_ns_in_lut[0] = dcg_vsl_info.lut_info[IMGSENSOR_LUT_A].linetime_in_ns;
		linetime_ns_in_lut[1] = dcg_vsl_info.lut_info[IMGSENSOR_LUT_B].linetime_in_ns;
		break;
	default:
		break;
	}

	if (ctx->extend_frame_length_en == FALSE) {
		frame_length_buf = 0;
		for (i = 0; i < 3; i++) {
			if (fll_in_lut[i]) {
				if (ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[2]) {
					set_i2c_buffer(ctx,
						ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[0],
						(fll_in_lut[i] >> 16) & 0xFF);
					set_i2c_buffer(ctx,
						ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[1],
						(fll_in_lut[i] >> 8) & 0xFF);
					set_i2c_buffer(ctx,
						ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[2],
						fll_in_lut[i] & 0xFF);
				} else {
					set_i2c_buffer(ctx,
						ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[0],
						(fll_in_lut[i] >> 8) & 0xFF);
					set_i2c_buffer(ctx,
						ctx->s_ctx.reg_addr_frame_length_in_lut[i].addr[1],
						fll_in_lut[i] & 0xFF);
				}
				/* update FL_lut RG value after setting buffer for writing RG */
				ctx->frame_length_in_lut_rg[i] = fll_in_lut[i];
				if (i == 0) {
					frame_length_buf +=
						ctx->frame_length_in_lut_rg[i];
				} else if (linetime_ns_in_lut[i] && linetime_ns_in_lut[0]){
					/* convert ctx->frame_length base on lut-A line time */
					frame_length_buf +=
						ntime2line(line2ntime(ctx->frame_length_in_lut_rg[i],
							linetime_ns_in_lut[i]),
							linetime_ns_in_lut[0]);
				}
			}
		}
		/* update FL RG value simultaneously */
		ctx->frame_length_rg = frame_length_buf;

		DRV_LOG(ctx,
			"ctx:(fl(RG):%u,%u/%u/%u/%u/%u), scen_id:%u,fll(input/ctx/output_a/b/c/d/e):0x%x/%x/%x/%x/%x/%x/%x,fll_step:%u/%u\n",
			ctx->frame_length_rg,
			ctx->frame_length_in_lut_rg[0],
			ctx->frame_length_in_lut_rg[1],
			ctx->frame_length_in_lut_rg[2],
			ctx->frame_length_in_lut_rg[3],
			ctx->frame_length_in_lut_rg[4],
			ctx->current_scenario_id,
			fll,
			ctx->frame_length,
			fll_in_lut[0],
			fll_in_lut[1],
			fll_in_lut[2],
			fll_in_lut[3],
			fll_in_lut[4],
			fl_a_step,
			fl_b_step);
	} else {
		DRV_LOG(ctx,
			"sid:%u,extend_frame_length_en:%u,default won't write fll!\n",
			ctx->current_scenario_id, ctx->extend_frame_length_en);
		return;
	}
}

void set_dummy(struct subdrv_ctx *ctx)
{
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF)
		write_frame_length_in_lut(ctx, ctx->frame_length, ctx->frame_length_in_lut);
	else if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_RAW_VS ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE_VS)
		write_dcg_vs_frame_length_in_lut(ctx, ctx->frame_length, ctx->frame_length_in_lut);
	else
		write_frame_length(ctx, ctx->frame_length);
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 0);

	commit_i2c_buffer(ctx);
}

void set_frame_length(struct subdrv_ctx *ctx, u16 frame_length)
{
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	if (frame_length)
		ctx->frame_length = frame_length;
	ctx->frame_length = max(ctx->frame_length, ctx->min_frame_length);
	ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);

	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	write_frame_length(ctx, ctx->frame_length);
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 0);
	commit_i2c_buffer(ctx);

	DRV_LOG(ctx, "fll(input/output/min):%u/%u/%u\n",
		frame_length, ctx->frame_length, ctx->min_frame_length);
}
/**
 * @brief: implementation for updating fll for lbmf.
 * Basically frame-sync will calculate itself, so this function only need to
 * simply prevent framelength constraints (no need to consider
 * previous or current shutter) and finally send them to write framelength.
 * user: frame-sync.
 * @param ctx: subdrv_ctx
 * @param frame_length: overall frame length lines
 * @param frame_length_in_lut: frame length array in lut for manual mode
 */
void set_frame_length_in_lut(struct subdrv_ctx *ctx,
	u32 frame_length, u32 *frame_length_in_lut)
{
	u16 exp_cnt = 0;
	int i = 0;
	u32 calc_fl_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	if (frame_length)
		ctx->frame_length = frame_length;
	ctx->frame_length = max(ctx->frame_length, ctx->min_frame_length);

	exp_cnt = ARRAY_SIZE(ctx->exposure);
	if (exp_cnt) {
		for (i = 0; i < exp_cnt; i++) {
			if (frame_length_in_lut[i])
				ctx->frame_length_in_lut[i] = frame_length_in_lut[i];
			else {
				ctx->frame_length_in_lut[i] = 0;
				DRV_LOG_MUST(ctx,
					"pls depend on exp_cnt to assign frame_length_in_lut!\n");
				continue;
			}
			/* fll_a/b/c_min = readout + xx lines(margin) */
			calc_fl_in_lut[i] =
				ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
				ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
			/* fll_a/b/c = max(readout, userInput_fll_a/b/c) */
			ctx->frame_length_in_lut[i] =
				max(ctx->frame_length_in_lut[i], calc_fl_in_lut[i]);
			switch (ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt) {
			case 2:
			{
				if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 2 &&
					i == 0 && ctx->frame_length >= ctx->frame_length_pre_store_in_lut[1]) {
					/* fll_a = max(fll_a, fll-fll_b) */
					ctx->frame_length_in_lut[i] =
						max(ctx->frame_length_in_lut[i],
							ctx->frame_length - ctx->frame_length_pre_store_in_lut[1]);
				} else if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 3 &&
					i == 1 && ctx->frame_length >= ctx->frame_length_in_lut[0]) {
					/* fll_b = max(fll_b, fll-fll_a) */
					ctx->frame_length_in_lut[i] =
						max(ctx->frame_length_in_lut[i],
						ctx->frame_length - ctx->frame_length_in_lut[0]);
				}
			}
				break;
			case 3:
			{
				if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 2 &&
				i == 0 && ctx->frame_length >=
				(ctx->frame_length_pre_store_in_lut[1] + ctx->frame_length_pre_store_in_lut[2])) {
					/* fll_a = max(fll_a, fll-fll_b-fll_c) */
					ctx->frame_length_in_lut[i] =
						max(ctx->frame_length_in_lut[i],
							(ctx->frame_length -
							ctx->frame_length_pre_store_in_lut[1] -
							ctx->frame_length_pre_store_in_lut[2]));
				} else if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 3 &&
				i == 2 && ctx->frame_length >=
				(ctx->frame_length_in_lut[0] + ctx->frame_length_in_lut[1])) {
					/* fll_c = max(fll_c, fll-fll_b-fll_a) */
					ctx->frame_length_in_lut[i] =
						max(ctx->frame_length_in_lut[i],
							(ctx->frame_length -
							ctx->frame_length_in_lut[1] -
							ctx->frame_length_in_lut[0]));
				}
			}
				break;
			default:
				break;
			}
			/* fll_a/b/c = min(fll_a/b/c, fll_max) */
			ctx->frame_length_in_lut[i] =
				min(ctx->frame_length_in_lut[i], ctx->s_ctx.frame_length_max);
		}
	} else {
		DRV_LOGE(ctx, "pls check exp_cnt!\n");
		return;
	}

	/* update framelength */
	ctx->frame_length = 0;
	for (i = 0; i < exp_cnt; i++) {
		if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 2) {
			/* frame_length = fll_pre_store_in_lut[1] + fll_pre_store_in_lut[2] +
			 * fll_pre_store_in_lut[i] + ... + fll_current_cal_in_lut[0]
			 */
			if (i == 0)
				ctx->frame_length += ctx->frame_length_in_lut[i];
			else
				ctx->frame_length += ctx->frame_length_pre_store_in_lut[i];
		} else if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 3) {
			/* frame_length = fll_current_cal_in_lut[0] + fll_current_cal_in_lut[1] +
			 * ... + fll_current_cal_in_lut[i]
			 */
			ctx->frame_length += ctx->frame_length_in_lut[i];
		} else {
			DRV_LOGE(ctx, "pls check delay_frame value!\n");
			return;
		}
	}

	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	write_frame_length_in_lut(ctx, ctx->frame_length, ctx->frame_length_in_lut);
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 0);
	commit_i2c_buffer(ctx);
	DRV_LOG(ctx,
		"sid:%u,fll(input/ctx/output_a/b/c/d/e/min):%u/%u/%u/%u/%u/%u/%u/%u,delay_frame:%u\n",
		ctx->current_scenario_id,
		frame_length,
		ctx->frame_length,
		ctx->frame_length_in_lut[0],
		ctx->frame_length_in_lut[1],
		ctx->frame_length_in_lut[2],
		ctx->frame_length_in_lut[3],
		ctx->frame_length_in_lut[4],
		ctx->min_frame_length,
		ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame);
}
/**
 * @brief: implementation for updating fll for dcg vsl.
 * Basically frame-sync will calculate itself, so this function only need to
 * simply prevent framelength constraints (no need to consider
 * previous or current shutter) and finally send them to write framelength.
 * user: frame-sync.
 * @param ctx: subdrv_ctx
 * @param frame_time_ns: overall frame time in ns
 * @param frame_length_in_lut: frame length array in lut for manual mode
 */
void set_dcg_vs_frame_length_in_lut(struct subdrv_ctx *ctx,
	u64 frame_time_ns, u32 *frame_length_in_lut)
{
	u16 lut_cnt = 0;
	int i = 0;
	u32 calc_fl_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	struct struct_dcg_vsl_info dcg_vsl_info = {0};
	int dcg_vs_exp_id[] = {
		IMGSENSOR_EXPOSURE_LE,
		IMGSENSOR_EXPOSURE_SE
	};
	u32 frame_length_step_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT];
	u32 readout_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT];
	u8 read_margin_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT];
	u32 linetime_ns_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT];
	u64 lut_0_ft_ns;

	/* fall back to max fps */
	if (!frame_time_ns && ctx->s_ctx.mode[ctx->current_scenario_id].max_framerate)
		frame_time_ns = 10000000000 / ctx->s_ctx.mode[ctx->current_scenario_id].max_framerate;

	get_dcg_vsl_info_by_scenario(ctx, ctx->current_scenario_id, &dcg_vsl_info);

	/* get static data of lut */
	memset(frame_length_step_in_lut, 0, sizeof(frame_length_step_in_lut));
	memset(readout_length_in_lut, 0, sizeof(readout_length_in_lut));
	memset(read_margin_in_lut, 0, sizeof(read_margin_in_lut));
	memset(linetime_ns_in_lut, 0, sizeof(linetime_ns_in_lut));
	for (i = 0; i < ARRAY_SIZE(dcg_vs_exp_id); i++) {
		frame_length_step_in_lut[i] = dcg_vsl_info.lut_info[i].framelength_step;
		readout_length_in_lut[i] = dcg_vsl_info.lut_info[i].readout_length;
		read_margin_in_lut[i] = dcg_vsl_info.lut_info[i].read_margin;
		linetime_ns_in_lut[i] = dcg_vsl_info.lut_info[i].linetime_in_ns;
	}

	lut_cnt = dcg_vsl_info.lut_cnt;
	if (lut_cnt == 2) {  /* 2 lut only */
		for (i = 0; i < lut_cnt; i++) {
			if (frame_length_in_lut[i])
				ctx->frame_length_in_lut[i] = frame_length_in_lut[i];
			else {
				ctx->frame_length_in_lut[i] = 0;
				DRV_LOG_MUST(ctx,
					"pls depend on lut_cnt to assign frame_length_in_lut!\n");
				continue;
			}
			/* fll_a/b/c_min = readout + xx lines(margin) */
			calc_fl_in_lut[i] =
				readout_length_in_lut[i] + read_margin_in_lut[i];
			/* fll_a/b/c = max(readout, userInput_fll_a/b/c) */
			ctx->frame_length_in_lut[i] =
				max(ctx->frame_length_in_lut[i], calc_fl_in_lut[i]);

			if (i == 1) {
				/* fll_b = max(fll_b, fll-fll_a) */
				lut_0_ft_ns = line2ntime(ctx->frame_length_in_lut[0], linetime_ns_in_lut[0]);
				if (frame_time_ns >= lut_0_ft_ns) {
					/* fll_b = max(fll_b, fll-fll_a) */
					ctx->frame_length_in_lut[i] = max(ctx->frame_length_in_lut[i],
						    ntime2line(frame_time_ns - lut_0_ft_ns, linetime_ns_in_lut[i]));
				}
			}

			/* fll_a/b/c = min(fll_a/b/c, fll_max) */
			ctx->frame_length_in_lut[i] =
				min(ctx->frame_length_in_lut[i], ctx->s_ctx.frame_length_max);

			ctx->frame_length_in_lut[i] = frame_length_step_in_lut[i] ?
				roundup(ctx->frame_length_in_lut[i], frame_length_step_in_lut[i]) :
				ctx->frame_length_in_lut[i];
		}
	} else {
		DRV_LOGE(ctx, "pls check exp_cnt!\n");
		return;
	}

	/* convert ctx->frame_length base on lut-A line time */
	ctx->frame_length = ctx->frame_length_in_lut[0] +
		ntime2line(line2ntime(ctx->frame_length_in_lut[1], linetime_ns_in_lut[1]),
			   linetime_ns_in_lut[0]);

	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	write_dcg_vs_frame_length_in_lut(ctx, ctx->frame_length, ctx->frame_length_in_lut);
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 0);
	commit_i2c_buffer(ctx);
	DRV_LOG(ctx,
		"sid:%u,fll(input/ctx/output_a/b/c/d/e/min):%llu/%u/%u/%u/%u/%u/%u/%u\n",
		ctx->current_scenario_id,
		frame_time_ns,
		ctx->frame_length,
		ctx->frame_length_in_lut[0],
		ctx->frame_length_in_lut[1],
		ctx->frame_length_in_lut[2],
		ctx->frame_length_in_lut[3],
		ctx->frame_length_in_lut[4],
		ctx->min_frame_length);
}

void set_lut_multi_frame_length(struct subdrv_ctx *ctx,
	u32 frame_length, u32 *frame_length_in_lut)
{
	switch(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode) {
	case HDR_RAW_LBMF:
		set_frame_length_in_lut(ctx, frame_length, frame_length_in_lut);
		break;
	case HDR_RAW_DCG_RAW_VS:
	case HDR_RAW_DCG_COMPOSE_VS:
		/* DCG-VS */
		{
			struct struct_dcg_vsl_info dcg_vsl_info = {0};
			u64 frame_time_ns = 0;

			/* convert frame length to frame time (lut-A) based */
			get_dcg_vsl_info_by_scenario(ctx, ctx->current_scenario_id, &dcg_vsl_info);
			frame_time_ns = line2ntime(frame_length,
					dcg_vsl_info.lut_info[IMGSENSOR_LUT_A].linetime_in_ns);

			set_dcg_vs_frame_length_in_lut(ctx,
				frame_time_ns, frame_length_in_lut);
		}
		break;
	default:
		break;
	}
}

/**
 * @brief: This api is used to assign general FLL for case except for lbmf or
 * FLL_A/FLL_B in lut for lbmf manual mode. It might be called between open and
 * set_shutter or after set_shutter, so we need to confirm as below:
 * 1. Framelength in lut is longer than readout + margin.
 * 2. If framelength is longer than readout + margin after set_shutter, we need
 * to compare and choose the bigger one.
 * @param ctx: subdrv_ctx
 * @param framerate: input framerate
 * @param min_framelength_en: by enabling it, it will update framelength in ctx.
 */
void set_max_framerate(struct subdrv_ctx *ctx, u16 framerate, bool min_framelength_en)
{
	u32 frame_length = 0;
	u32 linetime_in_ns = 0;

	if (framerate && ctx->line_length) {
		if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
			ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
				ctx->current_scenario_id, &linetime_in_ns, 0, 0);
			frame_length = 1000000000 / framerate * 10
				/ linetime_in_ns;
		} else {
			frame_length = ctx->pclk / framerate * 10 / ctx->line_length;
		}
	}

	ctx->frame_length = max(frame_length, ctx->min_frame_length);
	/*
	 * For manual mode:
	 * update FLL_B (2 exposure) / FLL_C (3 exposure) to extend the framelength
	 */
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF) {
		switch (ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt) {
		case 2:
			ctx->frame_length_in_lut[0] =
				max(ctx->frame_length_in_lut[0],
					ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
					ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
			ctx->frame_length_in_lut[0] =
				min(ctx->frame_length_in_lut[0], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[1] =
				max(ctx->frame_length_in_lut[1],
					ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
					ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
			if (ctx->frame_length >= ctx->frame_length_in_lut[0]) {
				ctx->frame_length_in_lut[1] =
					max(ctx->frame_length_in_lut[1],
						ctx->frame_length - ctx->frame_length_in_lut[0]);
			}
			ctx->frame_length_in_lut[1] =
				min(ctx->frame_length_in_lut[1], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[2] = 0;
			ctx->frame_length_in_lut[3] = 0;
			ctx->frame_length_in_lut[4] = 0;
			/* update framelength */
			ctx->frame_length =
				ctx->frame_length_in_lut[0] + ctx->frame_length_in_lut[1];
			break;
		case 3:
			ctx->frame_length_in_lut[0] =
				max(ctx->frame_length_in_lut[0],
					ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
					ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
			ctx->frame_length_in_lut[0] =
				min(ctx->frame_length_in_lut[0], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[1] =
				max(ctx->frame_length_in_lut[1],
					ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
					ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
			ctx->frame_length_in_lut[1] =
				min(ctx->frame_length_in_lut[1], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[2] =
				max(ctx->frame_length_in_lut[2],
					ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
					ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
			if (ctx->frame_length >=
				(ctx->frame_length_in_lut[0] + ctx->frame_length_in_lut[1])) {
				ctx->frame_length_in_lut[2] =
					max(ctx->frame_length_in_lut[2],
						(ctx->frame_length - ctx->frame_length_in_lut[1] -
						ctx->frame_length_in_lut[0]));
			}
			ctx->frame_length_in_lut[2] =
				min(ctx->frame_length_in_lut[2], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[3] = 0;
			ctx->frame_length_in_lut[4] = 0;
			/* update framelength */
			ctx->frame_length =
				ctx->frame_length_in_lut[0] +
				ctx->frame_length_in_lut[1] +
				ctx->frame_length_in_lut[2];
			break;
		default:
			break;
		}
	} else if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_RAW_VS ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE_VS) {
		// DONE: DCG-VS, margin would diff between lut
		struct struct_dcg_vsl_info dcg_vsl_info = {0};

		get_dcg_vsl_info_by_scenario(ctx, ctx->current_scenario_id, &dcg_vsl_info);

		switch (dcg_vsl_info.lut_cnt) {
		case 2: /* DCG + VS. lut-A + lut-B */

			ctx->frame_length_in_lut[0] =
				max(ctx->frame_length_in_lut[0],
				    dcg_vsl_info.lut_info[IMGSENSOR_LUT_A].readout_length +
				    dcg_vsl_info.lut_info[IMGSENSOR_LUT_A].read_margin);
			ctx->frame_length_in_lut[0] =
				min(ctx->frame_length_in_lut[0], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[1] =
				max(ctx->frame_length_in_lut[1],
				    dcg_vsl_info.lut_info[IMGSENSOR_LUT_B].readout_length +
				    dcg_vsl_info.lut_info[IMGSENSOR_LUT_B].read_margin);
			if (framerate && ctx->frame_length >= ctx->frame_length_in_lut[0]) {
				u64 vs_ft_ns = (1000000000 / framerate * 10)
					- line2ntime(ctx->frame_length_in_lut[0],
						     dcg_vsl_info.lut_info[IMGSENSOR_LUT_A].linetime_in_ns);
				ctx->frame_length_in_lut[1] =
					max(ctx->frame_length_in_lut[1],
					    ntime2line(vs_ft_ns,
						       dcg_vsl_info.lut_info[IMGSENSOR_LUT_B].linetime_in_ns));
			}
			ctx->frame_length_in_lut[1] =
				min(ctx->frame_length_in_lut[1], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[2] = 0;
			ctx->frame_length_in_lut[3] = 0;
			ctx->frame_length_in_lut[4] = 0;
			break;
		}
	} else
		ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);

	if (ctx->frame_length && ctx->line_length) {
		if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
			ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
				ctx->current_scenario_id, &linetime_in_ns, 0, 0);
			ctx->current_fps = 1000000000 / ctx->frame_length * 10 / linetime_in_ns;
		} else {
			ctx->current_fps = ctx->pclk / ctx->frame_length * 10 / ctx->line_length;
		}
	}

	if (min_framelength_en)
		ctx->min_frame_length = ctx->frame_length;
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_RAW_VS ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE_VS)
		DRV_LOG(ctx,
			"sid:%u,max_fps(input/output):%u/%u,min_fl_en:%u,fll(input/ctx/output_a/b/c/d/e):%u/%u/%u/%u/%u/%u/%u\n",
			ctx->current_scenario_id,
			framerate, ctx->current_fps, min_framelength_en,
			frame_length,
			ctx->frame_length,
			ctx->frame_length_in_lut[0],
			ctx->frame_length_in_lut[1],
			ctx->frame_length_in_lut[2],
			ctx->frame_length_in_lut[3],
			ctx->frame_length_in_lut[4]);
	else
		DRV_LOG(ctx, "max_fps(input/output):%u/%u, min_fl_en:%u\n",
			framerate, ctx->current_fps, min_framelength_en);
}	/*	set_max_framerate  */

/**
 * @brief: This api is used to assign general FLL for case except for lbmf or
 * FLL_A/FLL_B in lut for lbmf manual mode. It might be called between open and
 * set_shutter or after set_shutter, so we need to confirm as below:
 * 1. Framelength in lut is longer than readout + margin.
 * 2. If framelength is longer than readout + margin after set_shutter, we need
 * to compare and choose the bigger one.
 * 3. frame rate is 100-base.
 * @param ctx: subdrv_ctx
 * @param framerate: input framerate (100-base)
 * @param min_framelength_en: by enabling it, it will update framelength in ctx.
 */
void set_max_framerate_base100(struct subdrv_ctx *ctx, u16 framerate, bool min_framelength_en)
{
	u32 frame_length = 0, current_fps = 0;
	u32 linetime_in_ns = 0;
	u32 frame_length_step;

	if (framerate && ctx->line_length) {
		if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
			ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
				ctx->current_scenario_id, &linetime_in_ns, 0, 0);
			frame_length = 1000000000 / framerate * 100
				/ linetime_in_ns;
		} else {
			frame_length = ctx->pclk / framerate * 100 / ctx->line_length;
		}
	}

	frame_length_step = ctx->s_ctx.mode[ctx->current_scenario_id].framelength_step;
	ctx->frame_length = max(frame_length, ctx->min_frame_length);
	/*
	 * For manual mode:
	 * update FLL_B (2 exposure) / FLL_C (3 exposure) to extend the framelength
	 */
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF) {
		switch (ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt) {
		case 2:
		{
			if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 2) {
				ctx->frame_length_in_lut[0] =
					max(ctx->frame_length_in_lut[0],
						ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
						ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
				if (ctx->frame_length >= ctx->frame_length_pre_store_in_lut[1]) {
					ctx->frame_length_in_lut[0] =
						max(ctx->frame_length_in_lut[0],
							ctx->frame_length - ctx->frame_length_pre_store_in_lut[1]);
				}
				ctx->frame_length_in_lut[0] =
					min(ctx->frame_length_in_lut[0], ctx->s_ctx.frame_length_max);
				ctx->frame_length_in_lut[0] = frame_length_step ?
					roundup(ctx->frame_length_in_lut[0], frame_length_step) :
					ctx->frame_length_in_lut[0];
				ctx->frame_length_in_lut[1] =
					max(ctx->frame_length_in_lut[1],
						ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
						ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
				ctx->frame_length_in_lut[1] =
					min(ctx->frame_length_in_lut[1], ctx->s_ctx.frame_length_max);
				ctx->frame_length_in_lut[1] = frame_length_step ?
					roundup(ctx->frame_length_in_lut[1], frame_length_step) :
					ctx->frame_length_in_lut[1];
				ctx->frame_length_in_lut[2] = 0;
				ctx->frame_length_in_lut[3] = 0;
				ctx->frame_length_in_lut[4] = 0;
				/* update framelength */
				ctx->frame_length =
					ctx->frame_length_in_lut[0] + ctx->frame_length_pre_store_in_lut[1];
			} else if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 3) {
				ctx->frame_length_in_lut[0] =
					max(ctx->frame_length_in_lut[0],
						ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
						ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
				ctx->frame_length_in_lut[0] =
					min(ctx->frame_length_in_lut[0], ctx->s_ctx.frame_length_max);
				ctx->frame_length_in_lut[0] = frame_length_step ?
					roundup(ctx->frame_length_in_lut[0], frame_length_step) :
					ctx->frame_length_in_lut[0];
				ctx->frame_length_in_lut[1] =
					max(ctx->frame_length_in_lut[1],
						ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
						ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
				if (ctx->frame_length >= ctx->frame_length_in_lut[0]) {
					ctx->frame_length_in_lut[1] =
						max(ctx->frame_length_in_lut[1],
							ctx->frame_length - ctx->frame_length_in_lut[0]);
				}
				ctx->frame_length_in_lut[1] =
					min(ctx->frame_length_in_lut[1], ctx->s_ctx.frame_length_max);
				ctx->frame_length_in_lut[1] = frame_length_step ?
					roundup(ctx->frame_length_in_lut[1], frame_length_step) :
					ctx->frame_length_in_lut[1];
				ctx->frame_length_in_lut[2] = 0;
				ctx->frame_length_in_lut[3] = 0;
				ctx->frame_length_in_lut[4] = 0;
				/* update framelength */
				ctx->frame_length =
					ctx->frame_length_in_lut[0] + ctx->frame_length_in_lut[1];
			} else {
				DRV_LOGE(ctx, "pls check delay_frame value!\n");
				return;
			}
		}
			break;
		case 3:
		{
			if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 2) {
				ctx->frame_length_in_lut[0] =
					max(ctx->frame_length_in_lut[0],
						ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
						ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
				if (ctx->frame_length >=
					(ctx->frame_length_pre_store_in_lut[1] +
					ctx->frame_length_pre_store_in_lut[2])) {
					ctx->frame_length_in_lut[0] =
						max(ctx->frame_length_in_lut[0],
							(ctx->frame_length - ctx->frame_length_pre_store_in_lut[1] -
							ctx->frame_length_pre_store_in_lut[2]));
				}
				ctx->frame_length_in_lut[0] =
					min(ctx->frame_length_in_lut[0], ctx->s_ctx.frame_length_max);
				ctx->frame_length_in_lut[0] = frame_length_step ?
					roundup(ctx->frame_length_in_lut[0], frame_length_step) :
					ctx->frame_length_in_lut[0];
				ctx->frame_length_in_lut[1] =
					min(ctx->frame_length_in_lut[1], ctx->s_ctx.frame_length_max);
				ctx->frame_length_in_lut[1] = frame_length_step ?
					roundup(ctx->frame_length_in_lut[1], frame_length_step) :
					ctx->frame_length_in_lut[1];
				ctx->frame_length_in_lut[2] =
					max(ctx->frame_length_in_lut[2],
						ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
						ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
				ctx->frame_length_in_lut[2] =
					min(ctx->frame_length_in_lut[2], ctx->s_ctx.frame_length_max);
				ctx->frame_length_in_lut[2] = frame_length_step ?
					roundup(ctx->frame_length_in_lut[2], frame_length_step) :
					ctx->frame_length_in_lut[2];
				ctx->frame_length_in_lut[3] = 0;
				ctx->frame_length_in_lut[4] = 0;
				/* update framelength */
				ctx->frame_length =
					ctx->frame_length_in_lut[0] +
					ctx->frame_length_pre_store_in_lut[1] +
					ctx->frame_length_pre_store_in_lut[2];
			} else if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 3) {
				ctx->frame_length_in_lut[0] =
					max(ctx->frame_length_in_lut[0],
						ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
						ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
				ctx->frame_length_in_lut[0] =
					min(ctx->frame_length_in_lut[0], ctx->s_ctx.frame_length_max);
				ctx->frame_length_in_lut[0] = frame_length_step ?
					roundup(ctx->frame_length_in_lut[0], frame_length_step) :
					ctx->frame_length_in_lut[0];
				ctx->frame_length_in_lut[1] =
					max(ctx->frame_length_in_lut[1],
						ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
						ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
				ctx->frame_length_in_lut[1] =
					min(ctx->frame_length_in_lut[1], ctx->s_ctx.frame_length_max);
				ctx->frame_length_in_lut[1] = frame_length_step ?
					roundup(ctx->frame_length_in_lut[1], frame_length_step) :
					ctx->frame_length_in_lut[1];
				ctx->frame_length_in_lut[2] =
					max(ctx->frame_length_in_lut[2],
						ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
						ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
				if (ctx->frame_length >=
					(ctx->frame_length_in_lut[0] + ctx->frame_length_in_lut[1])) {
					ctx->frame_length_in_lut[2] =
						max(ctx->frame_length_in_lut[2],
							(ctx->frame_length - ctx->frame_length_in_lut[1] -
							ctx->frame_length_in_lut[0]));
				}
				ctx->frame_length_in_lut[2] =
					min(ctx->frame_length_in_lut[2], ctx->s_ctx.frame_length_max);
				ctx->frame_length_in_lut[2] = frame_length_step ?
					roundup(ctx->frame_length_in_lut[2], frame_length_step) :
					ctx->frame_length_in_lut[2];
				ctx->frame_length_in_lut[3] = 0;
				ctx->frame_length_in_lut[4] = 0;
				/* update framelength */
				ctx->frame_length =
					ctx->frame_length_in_lut[0] +
					ctx->frame_length_in_lut[1] +
					ctx->frame_length_in_lut[2];
			} else {
				DRV_LOGE(ctx, "pls check delay_frame value!\n");
				return;
			}
		}
			break;
		default:
			break;
		}
	} else if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_RAW_VS ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE_VS) {
		// DONE: DCG-VS, margin would diff between lut
		struct struct_dcg_vsl_info dcg_vsl_info = {0};

		get_dcg_vsl_info_by_scenario(ctx, ctx->current_scenario_id, &dcg_vsl_info);

		switch (dcg_vsl_info.lut_cnt) {
		case 2: /* DCG + VS. lut-A + lut-B */

			ctx->frame_length_in_lut[0] =
				max(ctx->frame_length_in_lut[0],
				    dcg_vsl_info.lut_info[IMGSENSOR_LUT_A].readout_length +
				    dcg_vsl_info.lut_info[IMGSENSOR_LUT_A].read_margin);
			ctx->frame_length_in_lut[0] =
				min(ctx->frame_length_in_lut[0], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[1] =
				max(ctx->frame_length_in_lut[1],
				    dcg_vsl_info.lut_info[IMGSENSOR_LUT_B].readout_length +
				    dcg_vsl_info.lut_info[IMGSENSOR_LUT_B].read_margin);
			// ctx->frame_length is calculated with lut-a's line time
			if (framerate && ctx->frame_length >= ctx->frame_length_in_lut[0]) {
				u64 vs_ft_ns = (1000000000 / framerate * 100)
					- line2ntime(ctx->frame_length_in_lut[0],
						     dcg_vsl_info.lut_info[IMGSENSOR_LUT_A].linetime_in_ns);
				ctx->frame_length_in_lut[1] =
					max(ctx->frame_length_in_lut[1],
					    ntime2line(vs_ft_ns,
						       dcg_vsl_info.lut_info[IMGSENSOR_LUT_B].linetime_in_ns));
			}
			ctx->frame_length_in_lut[1] =
				min(ctx->frame_length_in_lut[1], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[2] = 0;
			ctx->frame_length_in_lut[3] = 0;
			ctx->frame_length_in_lut[4] = 0;
			break;
		}
	} else
		ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);
	if (ctx->frame_length && ctx->line_length) {
		if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
			ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
				ctx->current_scenario_id, &linetime_in_ns, 0, 0);
			ctx->current_fps = 1000000000 / ctx->frame_length * 10 / linetime_in_ns;
			current_fps =1000000000 / ctx->frame_length * 100 / linetime_in_ns;
		} else {
			ctx->current_fps = ctx->pclk / ctx->frame_length * 10 / ctx->line_length;
			current_fps = ctx->pclk / ctx->frame_length * 100 / ctx->line_length;
		}
	}
	if (min_framelength_en)
		ctx->min_frame_length = ctx->frame_length;
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_RAW_VS ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE_VS)
		DRV_LOG(ctx,
			"sid:%u,max_fps(input/output):%u(100base)/%u(10base),min_fl_en:%u,fll(input/ctx/output_a/b/c/d/e):%u/%u/%u/%u/%u/%u/%u,fl(pre_store):%u/%u/%u/%u/%u\n",
			ctx->current_scenario_id,
			framerate, ctx->current_fps, min_framelength_en,
			frame_length,
			ctx->frame_length,
			ctx->frame_length_in_lut[0],
			ctx->frame_length_in_lut[1],
			ctx->frame_length_in_lut[2],
			ctx->frame_length_in_lut[3],
			ctx->frame_length_in_lut[4],
			ctx->frame_length_pre_store_in_lut[0],
			ctx->frame_length_pre_store_in_lut[1],
			ctx->frame_length_pre_store_in_lut[2],
			ctx->frame_length_pre_store_in_lut[3],
			ctx->frame_length_pre_store_in_lut[4]);
	else
		DRV_LOG(ctx,
			"max_fps(input/output-100base/output-10base):%u(100base)/%u(100base)/%u(10base), min_fl_en:%u\n",
			framerate, current_fps, ctx->current_fps, min_framelength_en);
}	/*	set_max_framerate_base100  */

void set_max_framerate_mcss_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 framerate)
{
	u32 frame_length;
	u32 frame_length_step;
	u32 frame_length_min;
	u32 frame_length_max;
	u32 linetime_in_ns = 0;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}

	ctx->s_ctx.mcss_update_subdrv_para((void *) ctx, scenario_id);

	frame_length_step = ctx->s_ctx.mode[scenario_id].framelength_step;
	/* set on the step of frame length */

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			scenario_id, &linetime_in_ns, 0, 0);
		frame_length = 1000000000 / framerate * 10
			/ linetime_in_ns;
	} else {
		frame_length = ctx->s_ctx.mode[scenario_id].pclk / framerate * 10
			/ ctx->s_ctx.mode[scenario_id].linelength;
	}

	frame_length_min = ctx->frame_length;

	if (framerate == ctx->s_ctx.mode[scenario_id].max_framerate) {
		frame_length = ctx->s_ctx.mode[scenario_id].framelength;
		frame_length_min = frame_length;
		ctx->min_frame_length = frame_length;
	}

	frame_length = frame_length_step ?
		(frame_length - (frame_length % frame_length_step)) : frame_length;

	frame_length_max = ctx->s_ctx.frame_length_max;
	frame_length_max = frame_length_step ?
		(frame_length_max - (frame_length_max % frame_length_step)) : frame_length_max;
	/* set in the range of frame length */
	ctx->frame_length = max(frame_length, frame_length_min);
	ctx->frame_length = min(ctx->frame_length, frame_length_max);

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			scenario_id, &linetime_in_ns, 0, 0);
		ctx->current_fps = 1000000000 / ctx->frame_length * 10 / linetime_in_ns;
	} else {
		ctx->current_fps = ctx->s_ctx.mode[scenario_id].pclk /
			ctx->frame_length * 10 / ctx->s_ctx.mode[scenario_id].linelength;
	}

	ctx->min_frame_length = ctx->frame_length;
	DRV_LOG(ctx, "max_fps(input/output):%u/%u(sid:%u), min_fl_en:1\n",
		framerate, ctx->current_fps, scenario_id);
	if (ctx->s_ctx.reg_addr_auto_extend ||
			(ctx->frame_length >
			(ctx->exposure[0] + ctx->s_ctx.mode[scenario_id].exposure_margin))) {
		if (ctx->s_ctx.aov_sensor_support &&
			ctx->s_ctx.mode[scenario_id].aov_mode &&
			!ctx->s_ctx.mode[scenario_id].s_dummy_support)
			DRV_LOG_MUST(ctx, "AOV mode not support set_dummy!\n");
		else
			set_dummy(ctx);
	}
}

void set_max_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 framerate)
{
	u32 frame_length;
	u32 frame_length_step;
	u32 frame_length_min;
	u32 frame_length_max;
	u32 linetime_in_ns = 0;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	if (!framerate) {
		DRV_LOGE(ctx, "framerate (%u) is invalid\n", framerate);
		return;
	}
	if (!ctx->s_ctx.mode[scenario_id].linelength) {
		DRV_LOGE(ctx, "linelength (%u) is invalid\n",
			ctx->s_ctx.mode[scenario_id].linelength);
		return;
	}
	if (framerate > ctx->s_ctx.mode[scenario_id].max_framerate) {
		DRV_LOGE(ctx, "framerate (%u) is greater than max_framerate (%u)\n",
			framerate, ctx->s_ctx.mode[scenario_id].max_framerate);
		framerate = ctx->s_ctx.mode[scenario_id].max_framerate;
	}
	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_LBMF) {
		set_max_framerate_in_lut_by_scenario(ctx, scenario_id, framerate);
		return;
	}
	// DONE: DCG-VS
	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_DCG_RAW_VS ||
	    ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE_VS) {
		set_dcg_vs_max_framerate_in_lut_by_scenario(ctx, scenario_id, framerate);
		return;
	}

	/* MCSS low power mode update para */
	if (ctx->s_ctx.mcss_update_subdrv_para != NULL) {
		set_max_framerate_mcss_by_scenario(ctx, scenario_id, framerate);
		return;
	}
	frame_length_step = ctx->s_ctx.mode[scenario_id].framelength_step;
	/* set on the step of frame length */
	frame_length = ctx->s_ctx.mode[scenario_id].pclk / framerate * 10
		/ ctx->s_ctx.mode[scenario_id].linelength;

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			scenario_id, &linetime_in_ns, 0, 0);
		frame_length = 1000000000 / framerate * 10
			/ linetime_in_ns;
	} else {
		frame_length = ctx->s_ctx.mode[scenario_id].pclk / framerate * 10
			/ ctx->s_ctx.mode[scenario_id].linelength;
	}

	frame_length = frame_length_step ?
		(frame_length - (frame_length % frame_length_step)) : frame_length;
	frame_length_min = ctx->s_ctx.mode[scenario_id].framelength;
	frame_length_max = ctx->s_ctx.frame_length_max;
	frame_length_max = frame_length_step ?
		(frame_length_max - (frame_length_max % frame_length_step)) : frame_length_max;


	/* set in the range of frame length */
	ctx->frame_length = max(frame_length, frame_length_min);
	ctx->frame_length = min(ctx->frame_length, frame_length_max);
	ctx->frame_length = frame_length_step ?
		roundup(ctx->frame_length,frame_length_step) : ctx->frame_length;

	/* set default frame length if given default framerate */
	if (framerate == ctx->s_ctx.mode[scenario_id].max_framerate)
		ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			scenario_id, &linetime_in_ns, 0, 0);
		ctx->current_fps = 1000000000 / linetime_in_ns / ctx->frame_length * 10;
	} else {
		ctx->current_fps = ctx->s_ctx.mode[scenario_id].pclk /
				ctx->frame_length * 10 /
				ctx->s_ctx.mode[scenario_id].linelength;
	}

	ctx->min_frame_length = ctx->frame_length;
	DRV_LOG_MUST(ctx, "max_fps(input/output):%u/%u(sid:%u), min_fl_en:1, ctx->frame_length:%u\n",
		framerate, ctx->current_fps, scenario_id, ctx->frame_length);
	if (ctx->s_ctx.reg_addr_auto_extend ||
			(ctx->frame_length >
			(ctx->exposure[0] + ctx->s_ctx.mode[scenario_id].exposure_margin))) {
		if (ctx->s_ctx.aov_sensor_support &&
			ctx->s_ctx.mode[scenario_id].aov_mode &&
			!ctx->s_ctx.mode[scenario_id].s_dummy_support)
			DRV_LOG_MUST(ctx, "AOV mode not support set_dummy!\n");
		else
			set_dummy(ctx);
	}
}
/**
 * @brief: This api is used to assign FLL_A/FLL_B in lut for manual mode.
 * It should refer to previous shutter because per-frame multi shutter framelength
 * might not be called.
 * @param ctx: subdrv_ctx
 * @param scenario_id: current scenario id
 * @param framerate: input framerate
 */
void set_max_framerate_in_lut_by_scenario(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 framerate)
{
	u32 frame_length = 0;
	u32 frame_length_step = 0;
	u16 exp_cnt = 0;
	u32 cit_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 calc_fl_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	int i;
	u32 linetime_in_ns = 0;

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			scenario_id, &linetime_in_ns, 0, 0);
		frame_length = 1000000000 / framerate * 10
			/ linetime_in_ns;
	} else {
		frame_length = ctx->s_ctx.mode[scenario_id].pclk / framerate * 10
			/ ctx->s_ctx.mode[scenario_id].linelength;
	}

	if (framerate == ctx->s_ctx.mode[scenario_id].max_framerate)
		frame_length = ctx->s_ctx.mode[scenario_id].framelength;

	frame_length_step = ctx->s_ctx.mode[scenario_id].framelength_step;
	frame_length = frame_length_step ?
		(frame_length - (frame_length % frame_length_step)) : frame_length;
	ctx->frame_length =
		max(frame_length, ctx->s_ctx.mode[scenario_id].framelength);

	/* set default frame length if given default framerate */
	if (framerate ==  ctx->s_ctx.mode[scenario_id].max_framerate)
		ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;

	// manual mode
	exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
	switch (exp_cnt) {
	case 2:
		for (i = 0; i < exp_cnt; i++) {
			/*  update cit_in_lut depends on exposure_order_in_lbmf */
			if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf ==
				IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
				/* 2exp: cit_lut_a = SE / cit_lut_b = LE */
				/* 3exp: cit_lut_a = SE / cit_lut_b = ME / cit_lut_c = LE */
				cit_in_lut[i] = ctx->exposure[exp_cnt - 1 - i];
			} else if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf ==
				IMGSENSOR_LBMF_EXPOSURE_LE_FIRST) {
				/* 2exp: cit_lut_a = LE / cit_lut_b = SE */
				/* 3exp: cit_lut_a = LE / cit_lut_b = ME / cit_lut_c = SE */
				cit_in_lut[i] = ctx->exposure[i];
			} else {
				DRV_LOGE(ctx, "pls assign exposure_order_in_lbmf value!\n");
				return;
			}
		}

		if (ctx->s_ctx.mode[scenario_id].delay_frame == 2) {
			/* fll_a_min = readout + xx lines(margin) */
			calc_fl_in_lut[0] =
				ctx->s_ctx.mode[scenario_id].readout_length +
				ctx->s_ctx.mode[scenario_id].read_margin;
			/* fll_a = max(readout, previous shutter_a) */
			calc_fl_in_lut[0] =
				max(calc_fl_in_lut[0], cit_in_lut[0] + ctx->s_ctx.exposure_margin);
			if (ctx->frame_length >= ctx->frame_length_pre_store_in_lut[1]) {
				/* fll_a = max(fll_a, fll_mode_max-fll_b) */
				calc_fl_in_lut[0] =
					max(calc_fl_in_lut[0],
						ctx->frame_length - ctx->frame_length_in_lut[1]);
			}
			/* fll_a = min(fll_a, fll_max) */
			ctx->frame_length_in_lut[0] =
				min(calc_fl_in_lut[0], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[0] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[0], frame_length_step) :
				ctx->frame_length_in_lut[0];
			/* fll_b_min = readout + xx lines(margin) */
			calc_fl_in_lut[1] =
				ctx->s_ctx.mode[scenario_id].readout_length +
				ctx->s_ctx.mode[scenario_id].read_margin;
			/* fll_b = max(readout, previous shutter_b) */
			calc_fl_in_lut[1] =
				max(calc_fl_in_lut[1], cit_in_lut[1] + ctx->s_ctx.exposure_margin);
			/* fll_b = min(fll_b, fll_max) */
			ctx->frame_length_in_lut[1] =
				min(calc_fl_in_lut[1], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[1] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[1], frame_length_step) :
				ctx->frame_length_in_lut[1];
			ctx->frame_length_in_lut[2] = 0;
			ctx->frame_length_in_lut[3] = 0;
			ctx->frame_length_in_lut[4] = 0;
			/* update framelength */
			ctx->frame_length =
				ctx->frame_length_in_lut[0] + ctx->frame_length_pre_store_in_lut[1];
		} else if (ctx->s_ctx.mode[scenario_id].delay_frame == 3) {
			/* fll_a_min = readout + xx lines(margin) */
			calc_fl_in_lut[0] =
				ctx->s_ctx.mode[scenario_id].readout_length +
				ctx->s_ctx.mode[scenario_id].read_margin;
			/* fll_a = max(readout, previous shutter_b) */
			calc_fl_in_lut[0] =
				max(calc_fl_in_lut[0], cit_in_lut[1] + ctx->s_ctx.exposure_margin);
			/* fll_a = min(fll_a, fll_max) */
			ctx->frame_length_in_lut[0] =
				min(calc_fl_in_lut[0], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[0] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[0], frame_length_step) :
				ctx->frame_length_in_lut[0];
			/* fll_b_min = readout + xx lines(margin) */
			calc_fl_in_lut[1] =
				ctx->s_ctx.mode[scenario_id].readout_length +
				ctx->s_ctx.mode[scenario_id].read_margin;
			/* fll_b = max(readout, previous shutter_a) */
			calc_fl_in_lut[1] =
				max(calc_fl_in_lut[1], cit_in_lut[0] + ctx->s_ctx.exposure_margin);
			if (ctx->frame_length >= ctx->frame_length_in_lut[0]) {
				/* fll_b = max(fll_b, fll_mode_max-fll_a) */
				calc_fl_in_lut[1] =
					max(calc_fl_in_lut[1],
						ctx->frame_length - ctx->frame_length_in_lut[0]);
			}
			/* fll_b = min(fll_b, fll_max) */
			ctx->frame_length_in_lut[1] =
				min(calc_fl_in_lut[1], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[1] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[1], frame_length_step) :
				ctx->frame_length_in_lut[1];
			ctx->frame_length_in_lut[2] = 0;
			ctx->frame_length_in_lut[3] = 0;
			ctx->frame_length_in_lut[4] = 0;
			/* update framelength */
			ctx->frame_length =
				ctx->frame_length_in_lut[0] + ctx->frame_length_in_lut[1];
		} else {
			DRV_LOGE(ctx, "pls check delay_frame value!\n");
			return;
		}
		if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
			ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
				scenario_id, &linetime_in_ns, 0, 0);
			ctx->current_fps = 1000000000 / linetime_in_ns / ctx->frame_length * 10;
		} else {
			ctx->current_fps = ctx->s_ctx.mode[scenario_id].pclk /
							ctx->frame_length * 10 /
							ctx->s_ctx.mode[scenario_id].linelength;
		}
		ctx->min_frame_length = ctx->frame_length;
		DRV_LOG(ctx,
			"sid:%u,max_fps(input/output):%u/%u,min_fl_en:1,lut order:%u,fll(input/ctx/output_a/b/c/d/e):%u/%u/%u/%u/%u/%u/%u,delay_frame:%u\n",
			scenario_id,
			framerate, ctx->current_fps,
			ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf,
			frame_length,
			ctx->frame_length,
			ctx->frame_length_in_lut[0],
			ctx->frame_length_in_lut[1],
			ctx->frame_length_in_lut[2],
			ctx->frame_length_in_lut[3],
			ctx->frame_length_in_lut[4],
			ctx->s_ctx.mode[scenario_id].delay_frame);
		if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
			if (ctx->s_ctx.reg_addr_auto_extend ||
				(ctx->frame_length_in_lut[0] >
				(ctx->exposure[0] + ctx->s_ctx.mode[scenario_id].read_margin)) ||
				(ctx->frame_length_in_lut[1] >
				(ctx->exposure[1] + ctx->s_ctx.mode[scenario_id].read_margin)))
				set_dummy(ctx);
		} else {
			if (ctx->s_ctx.reg_addr_auto_extend ||
				(ctx->frame_length_in_lut[0] >
				(ctx->exposure[1] + ctx->s_ctx.mode[scenario_id].read_margin)) ||
				(ctx->frame_length_in_lut[1] >
				(ctx->exposure[0] + ctx->s_ctx.mode[scenario_id].read_margin)))
				set_dummy(ctx);
		}
		break;
	case 3:
		for (i = 0; i < exp_cnt; i++) {
			/*  update cit_in_lut depends on exposure_order_in_lbmf */
			if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf ==
				IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
				/* 2exp: cit_lut_a = SE / cit_lut_b = LE */
				/* 3exp: cit_lut_a = SE / cit_lut_b = ME / cit_lut_c = LE */
				cit_in_lut[i] = ctx->exposure[exp_cnt - 1 - i];
			} else if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf ==
				IMGSENSOR_LBMF_EXPOSURE_LE_FIRST) {
				/* 2exp: cit_lut_a = LE / cit_lut_b = SE */
				/* 3exp: cit_lut_a = LE / cit_lut_b = ME / cit_lut_c = SE */
				cit_in_lut[i] = ctx->exposure[i];
			} else {
				DRV_LOGE(ctx, "pls assign exposure_order_in_lbmf value!\n");
				return;
			}
		}

		if (ctx->s_ctx.mode[scenario_id].delay_frame == 2) {
			/* fll_a_min = readout + xx lines(margin) */
			calc_fl_in_lut[0] =
				ctx->s_ctx.mode[scenario_id].readout_length +
				ctx->s_ctx.mode[scenario_id].read_margin;
			/* fll_a = max(readout, previous shutter_a) */
			calc_fl_in_lut[0] =
				max(calc_fl_in_lut[0], cit_in_lut[0] + ctx->s_ctx.exposure_margin);
			if (ctx->frame_length >=
				(ctx->frame_length_pre_store_in_lut[1] +
				ctx->frame_length_pre_store_in_lut[2])) {
				/* fll_a = max(fll_a, fll_mode_max-fll_b-fll_c) */
				calc_fl_in_lut[0] =
					max(calc_fl_in_lut[0],
						(ctx->frame_length -
						ctx->frame_length_pre_store_in_lut[1] -
						ctx->frame_length_pre_store_in_lut[2]));
			}
			/* fll_a = min(fll_a, fll_max) */
			ctx->frame_length_in_lut[0] =
				min(calc_fl_in_lut[0], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[0] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[0], frame_length_step) :
				ctx->frame_length_in_lut[0];
			/* fll_b_min = readout + xx lines(margin) */
			calc_fl_in_lut[1] =
				ctx->s_ctx.mode[scenario_id].readout_length +
				ctx->s_ctx.mode[scenario_id].read_margin;
			/* fll_b = max(readout, previous shutter_b) */
			calc_fl_in_lut[1] =
				max(calc_fl_in_lut[1], cit_in_lut[1] + ctx->s_ctx.exposure_margin);
			/* fll_b = min(fll_b, fll_max) */
			ctx->frame_length_in_lut[1] =
				min(calc_fl_in_lut[1], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[1] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[1], frame_length_step) :
				ctx->frame_length_in_lut[1];
			/* fll_c_min = readout + xx lines(margin) */
			calc_fl_in_lut[2] =
				ctx->s_ctx.mode[scenario_id].readout_length +
				ctx->s_ctx.mode[scenario_id].read_margin;
			/* fll_c = max(readout, previous shutter_c) */
			calc_fl_in_lut[2] =
				max(calc_fl_in_lut[2], cit_in_lut[2] + ctx->s_ctx.exposure_margin);
			/* fll_c = min(fll_c, fll_max) */
			ctx->frame_length_in_lut[2] =
				min(calc_fl_in_lut[2], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[2] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[2], frame_length_step) :
				ctx->frame_length_in_lut[2];
			ctx->frame_length_in_lut[3] = 0;
			ctx->frame_length_in_lut[4] = 0;
			/* update framelength */
			ctx->frame_length =
				ctx->frame_length_in_lut[0] +
				ctx->frame_length_pre_store_in_lut[1] +
				ctx->frame_length_pre_store_in_lut[2];
		} else if (ctx->s_ctx.mode[scenario_id].delay_frame == 3) {
			/* fll_a_min = readout + xx lines(margin) */
			calc_fl_in_lut[0] =
				ctx->s_ctx.mode[scenario_id].readout_length +
				ctx->s_ctx.mode[scenario_id].read_margin;
			/* fll_a = max(readout, previous shutter_b) */
			calc_fl_in_lut[0] =
				max(calc_fl_in_lut[0], cit_in_lut[1] + ctx->s_ctx.exposure_margin);
			/* fll_a = min(fll_a, fll_max) */
			ctx->frame_length_in_lut[0] =
				min(calc_fl_in_lut[0], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[0] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[0], frame_length_step) :
				ctx->frame_length_in_lut[0];
			/* fll_b_min = readout + xx lines(margin) */
			calc_fl_in_lut[1] =
				ctx->s_ctx.mode[scenario_id].readout_length +
				ctx->s_ctx.mode[scenario_id].read_margin;
			/* fll_b = max(readout, previous shutter_c) */
			calc_fl_in_lut[1] =
				max(calc_fl_in_lut[1], cit_in_lut[2] + ctx->s_ctx.exposure_margin);
			/* fll_b = min(fll_b, fll_max) */
			ctx->frame_length_in_lut[1] =
				min(calc_fl_in_lut[1], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[1] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[1], frame_length_step) :
				ctx->frame_length_in_lut[1];
			/* fll_c_min = readout + xx lines(margin) */
			calc_fl_in_lut[2] =
				ctx->s_ctx.mode[scenario_id].readout_length +
				ctx->s_ctx.mode[scenario_id].read_margin;
			/* fll_c = max(readout, previous shutter_a) */
			calc_fl_in_lut[2] =
				max(calc_fl_in_lut[2], cit_in_lut[0] + ctx->s_ctx.exposure_margin);
			if (ctx->frame_length >=
				(ctx->frame_length_in_lut[0] + ctx->frame_length_in_lut[1])) {
				/* fll_c = max(fll_c, fll_mode_max-fll_b-fll_a) */
				calc_fl_in_lut[2] =
					max(calc_fl_in_lut[2],
						(ctx->frame_length - ctx->frame_length_in_lut[1] -
						ctx->frame_length_in_lut[0]));
			}
			/* fll_c = min(fll_c, fll_max) */
			ctx->frame_length_in_lut[2] =
				min(calc_fl_in_lut[2], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[2] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[2], frame_length_step) :
				ctx->frame_length_in_lut[2];
			ctx->frame_length_in_lut[3] = 0;
			ctx->frame_length_in_lut[4] = 0;
			/* update framelength */
			ctx->frame_length =
				ctx->frame_length_in_lut[0] +
				ctx->frame_length_in_lut[1] +
				ctx->frame_length_in_lut[2];
		} else {
			DRV_LOGE(ctx, "pls check delay_frame value!\n");
			return;
		}
		if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
			ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
				scenario_id, &linetime_in_ns, 0, 0);
			ctx->current_fps = 1000000000 / linetime_in_ns / ctx->frame_length * 10;
		} else {
			ctx->current_fps = ctx->s_ctx.mode[scenario_id].pclk /
							ctx->frame_length * 10 /
							ctx->s_ctx.mode[scenario_id].linelength;
		}
		ctx->min_frame_length = ctx->frame_length;
		DRV_LOG(ctx,
			"sid:%u,max_fps(input/output):%u/%u,min_fl_en:1,lut order:%u,fll(input/ctx/output_a/b/c/d/e):%u/%u/%u/%u/%u/%u/%u,delay_frame:%u\n",
			scenario_id,
			framerate, ctx->current_fps,
			ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf,
			frame_length,
			ctx->frame_length,
			ctx->frame_length_in_lut[0],
			ctx->frame_length_in_lut[1],
			ctx->frame_length_in_lut[2],
			ctx->frame_length_in_lut[3],
			ctx->frame_length_in_lut[4],
			ctx->s_ctx.mode[scenario_id].delay_frame);
		if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
			if (ctx->s_ctx.reg_addr_auto_extend ||
				(ctx->frame_length_in_lut[0] >
				(ctx->exposure[1] + ctx->s_ctx.mode[scenario_id].read_margin)) ||
				(ctx->frame_length_in_lut[1] >
				(ctx->exposure[0] + ctx->s_ctx.mode[scenario_id].read_margin)) ||
				(ctx->frame_length_in_lut[2] >
				(ctx->exposure[2] + ctx->s_ctx.mode[scenario_id].read_margin)))
				set_dummy(ctx);
		} else {
			if (ctx->s_ctx.reg_addr_auto_extend ||
				(ctx->frame_length_in_lut[0] >
				(ctx->exposure[1] + ctx->s_ctx.mode[scenario_id].read_margin)) ||
				(ctx->frame_length_in_lut[1] >
				(ctx->exposure[2] + ctx->s_ctx.mode[scenario_id].read_margin)) ||
				(ctx->frame_length_in_lut[2] >
				(ctx->exposure[0] + ctx->s_ctx.mode[scenario_id].read_margin)))
				set_dummy(ctx);
		}
		break;
	default:
		break;
	}
}

/**
 * dcg exposure use dcg Tline and convert to previous lut's Tline
 * vs exposure use vs Tline and convert to previous lut's Tline
 */
static u32 convert_to_cit_in_lut(struct subdrv_ctx *ctx, struct struct_dcg_vsl_info *info, int exp_idx, u64 shutter)
{
	u32 linetime_ns_in_shutter_lut, linetime_ns_in_pre_lut;
	u8 shutter_lut_id, pre_lut_id;
	u32 result = 0;

	if (unlikely(info == NULL)) {
		DRV_LOGE(ctx, "parameter info is invalid\n");
		return (u32)shutter;
	}
	if (unlikely(exp_idx >= info->exp_cnt || exp_idx < 0)) {
		DRV_LOGE(ctx, "parameter exp_idx %d is invalid\n", exp_idx);
		return (u32)shutter;
	}
	shutter_lut_id = info->exp_info[exp_idx].lut_idx;
	/* Get previous lut id */
	if (shutter_lut_id == IMGSENSOR_LUT_A)
		pre_lut_id = info->lut_cnt ? (info->lut_cnt - 1) : 0;
	else
		pre_lut_id = shutter_lut_id - 1;

	linetime_ns_in_shutter_lut = info->lut_info[shutter_lut_id].linetime_in_ns;
	linetime_ns_in_pre_lut = info->lut_info[pre_lut_id].linetime_in_ns;


	result = ntime2line(line2ntime(shutter, linetime_ns_in_shutter_lut),
			    linetime_ns_in_pre_lut);

	DRV_LOG(ctx, "shutter:%llu, exp_idx:%d, Tline_shu:%u, Tline_pre_lut:%u, convert result: %u\n",
		shutter, exp_idx,
		linetime_ns_in_shutter_lut, linetime_ns_in_pre_lut,
		result);

	return result;
}

/**
 * @brief: This api is used to assign FLL_A/FLL_B in lut for dcg+vs mode.
 * It should refer to previous shutter because per-frame multi shutter framelength
 * might not be called.
 * @param ctx: subdrv_ctx
 * @param scenario_id: current scenario id
 * @param framerate: input framerate
 */
void set_dcg_vs_max_framerate_in_lut_by_scenario(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 framerate)
{
	u16 exp_cnt = 0;
	u32 cit_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 calc_fl_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	int i;
	u32 linetime_in_ns = 0;
	struct struct_dcg_vsl_info dcg_vsl_info = {0};
	u8 lut_a_exp_id, lut_b_exp_id;
	u32 lut_a_fl_step, lut_b_fl_step, lut_a_linet, lut_b_linet;
	u64 frame_time_ns, lut_0_ft_ns;

	/* set default frame length if given default framerate */
	if (framerate ==  ctx->s_ctx.mode[scenario_id].max_framerate)
		ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;

	get_dcg_vsl_info_by_scenario(ctx, ctx->current_scenario_id, &dcg_vsl_info);

	// manual mode
	exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
	if (exp_cnt == 3 && dcg_vsl_info.lut_cnt == 2) {
		/* DCG + VS.  lut-a + lut-b */
		for (i = 0; i < dcg_vsl_info.lut_cnt; i++) {
			/* update cit_in_lut */
			cit_in_lut[i] = convert_to_cit_in_lut(ctx, &dcg_vsl_info, i, ctx->exposure[i]);
		}
		if (ctx->s_ctx.mode[scenario_id].exposure_order_in_lbmf == IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
			// SE first
			lut_a_exp_id = IMGSENSOR_EXPOSURE_SE;
			lut_b_exp_id = IMGSENSOR_EXPOSURE_LE;
		} else {
			// LE first
			lut_a_exp_id = IMGSENSOR_EXPOSURE_LE;
			lut_b_exp_id = IMGSENSOR_EXPOSURE_SE;
		}
		lut_a_fl_step = dcg_vsl_info.lut_info[IMGSENSOR_LUT_A].framelength_step;
		lut_b_fl_step = dcg_vsl_info.lut_info[IMGSENSOR_LUT_B].framelength_step;
		lut_a_linet = dcg_vsl_info.lut_info[IMGSENSOR_LUT_A].linetime_in_ns;
		lut_b_linet = dcg_vsl_info.lut_info[IMGSENSOR_LUT_B].linetime_in_ns;
		/* fll_a_min = readout + xx lines(margin) */
		calc_fl_in_lut[0] =
			dcg_vsl_info.lut_info[IMGSENSOR_LUT_A].readout_length +
			dcg_vsl_info.lut_info[IMGSENSOR_LUT_A].read_margin;
		/* fll_a = max(readout, previous shutter_b) */
		calc_fl_in_lut[0] = max(calc_fl_in_lut[0],
				cit_in_lut[1] + dcg_vsl_info.exp_info[lut_b_exp_id].exposure_margin);
		/* fll_a = min(fll_a, fll_max) */
		ctx->frame_length_in_lut[0] =
			min(calc_fl_in_lut[0], ctx->s_ctx.frame_length_max);
		ctx->frame_length_in_lut[0] = lut_a_fl_step ?
			roundup(ctx->frame_length_in_lut[0], lut_a_fl_step) :
			ctx->frame_length_in_lut[0];
		/* fll_b_min = readout + xx lines(margin) */
		calc_fl_in_lut[1] =
			dcg_vsl_info.lut_info[IMGSENSOR_LUT_B].readout_length +
			dcg_vsl_info.lut_info[IMGSENSOR_LUT_B].read_margin;
		/* fll_b = max(readout, previous shutter_a) */
		calc_fl_in_lut[1] = max(calc_fl_in_lut[1],
				cit_in_lut[0] + dcg_vsl_info.exp_info[lut_a_exp_id].exposure_margin);

		if (framerate)
			frame_time_ns = 1000000000ULL / framerate * 10;
		else
			frame_time_ns = 1000000000ULL / 1 * 10;
		lut_0_ft_ns = line2ntime(ctx->frame_length_in_lut[0], lut_a_linet);
		DRV_LOG_MUST(ctx, "total ft = %llu  nslut-A ft = %llu ns\n", frame_time_ns, lut_0_ft_ns);
		if (frame_time_ns >= lut_0_ft_ns) {
			/* fll_b = max(fll_b, fll-fll_a) */
			calc_fl_in_lut[1] = max(calc_fl_in_lut[1],
				    ntime2line(frame_time_ns - lut_0_ft_ns, lut_b_linet));
		}
		/* fll_b = min(fll_b, fll_max) */
		ctx->frame_length_in_lut[1] =
			min(calc_fl_in_lut[1], ctx->s_ctx.frame_length_max);
		ctx->frame_length_in_lut[1] = lut_b_fl_step ?
			roundup(ctx->frame_length_in_lut[1], lut_b_fl_step) :
			ctx->frame_length_in_lut[1];
		ctx->frame_length_in_lut[2] = 0;
		ctx->frame_length_in_lut[3] = 0;
		ctx->frame_length_in_lut[4] = 0;
		/* convert ctx->frame_length base on lut-A line time */
		ctx->frame_length = ctx->frame_length_in_lut[0] +
			ntime2line(line2ntime(ctx->frame_length_in_lut[1], lut_b_linet),
				   lut_a_linet);

		if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
			ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
				ctx->current_scenario_id, &linetime_in_ns, 0, 0);
			ctx->current_fps = 1000000000 / linetime_in_ns / ctx->frame_length * 10;
		} else {
			ctx->current_fps = ctx->pclk / ctx->frame_length * 10 / ctx->line_length;
		}

		ctx->min_frame_length = ctx->frame_length;
		DRV_LOG(ctx,
			"sid:%u,max_fps(input/output):%u/%u,min_fl_en:1,fll(ctx/output_a/b/c/d/e):%u/%u/%u/%u/%u/%u,cit_in_lut(%u/%u/%u/%u/%u)\n",
			scenario_id,
			framerate, ctx->current_fps,
			ctx->frame_length,
			ctx->frame_length_in_lut[0],
			ctx->frame_length_in_lut[1],
			ctx->frame_length_in_lut[2],
			ctx->frame_length_in_lut[3],
			ctx->frame_length_in_lut[4],
			cit_in_lut[0],
			cit_in_lut[1],
			cit_in_lut[2],
			cit_in_lut[3],
			cit_in_lut[4]);
		if (ctx->s_ctx.reg_addr_auto_extend ||
			(ctx->frame_length_in_lut[0] >
			(ctx->exposure[2] + dcg_vsl_info.lut_info[IMGSENSOR_LUT_A].read_margin)) ||
			(ctx->frame_length_in_lut[1] >
			(ctx->exposure[0] + dcg_vsl_info.lut_info[IMGSENSOR_LUT_B].read_margin)))
			set_dummy(ctx);
	}
}

/**
 * @brief: This api is used to calculate framerate depending on framelength
 * in ctx. If auto_flicker_mode is enabled, then set_max_framerate
 * in this function might be called. Framelength in ctx might update after
 * set_max_framerate_by_scenario or set_shutter.
 * @param ctx: subdrv_ctx
 * @param min_framelength_en: by enabling it, it will update framelength in ctx.
 */
bool set_auto_flicker(struct subdrv_ctx *ctx, bool min_framelength_en)
{
	u16 framerate = 0;
	bool ret = TRUE;
	u32 linetime_in_ns = 0;
	u16 new_fps = 0;

	if (!ctx->line_length) {
		DRV_LOGE(ctx, "line_length(%u) is invalid\n", ctx->line_length);
		return FALSE;
	}

	if (!ctx->frame_length) {
		DRV_LOGE(ctx, "frame_length(%u) is invalid\n", ctx->frame_length);
		return FALSE;
	}

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			ctx->current_scenario_id, &linetime_in_ns, 0, 0);
		framerate = 1000000000 / linetime_in_ns * 100 / ctx->frame_length;
	} else {
		/* frame rate is 100-base. */
		framerate = ctx->pclk / ctx->line_length * 100 / ctx->frame_length;
	}

	switch (ctx->autoflicker_en) {
	case 1:
		if (framerate > 5920 && framerate <= 6070)
			set_max_framerate_base100(ctx, 5920, min_framelength_en);
		else if (framerate > 2960 && framerate <= 3050)
			set_max_framerate_base100(ctx, 2960, min_framelength_en);
		else if (framerate > 2460 && framerate <= 2530)
			set_max_framerate_base100(ctx, 2460, min_framelength_en);
		else if (framerate > 2360 && framerate <= 2430)
			set_max_framerate_base100(ctx, 2360, min_framelength_en);
		else if (framerate > 1460 && framerate <= 1530)
			set_max_framerate_base100(ctx, 1460, min_framelength_en);
		else
			ret = FALSE;
		break;
	case 2:
		if (framerate > 5920 && framerate <= 6070)
			set_max_framerate_base100(ctx, 5920, min_framelength_en);
		else if (framerate > 2990 && framerate <= 3050)
			set_max_framerate_base100(ctx, 2990, min_framelength_en);
		else if (framerate > 2460 && framerate <= 2530)
			set_max_framerate_base100(ctx, 2460, min_framelength_en);
		else if (framerate > 2360 && framerate <= 2430)
			set_max_framerate_base100(ctx, 2360, min_framelength_en);
		else if (framerate > 1460 && framerate <= 1530)
			set_max_framerate_base100(ctx, 1460, min_framelength_en);
		else
			ret = FALSE;
		break;
	case 3:
		if (framerate > 5920 && framerate <= 6070)
			set_max_framerate_base100(ctx, 5920, min_framelength_en);
		else if (framerate >= 2999 && framerate <= 3001)
			set_max_framerate_base100(ctx, 2999, min_framelength_en);
		else if (framerate > 2460 && framerate <= 2530)
			set_max_framerate_base100(ctx, 2460, min_framelength_en);
		else if (framerate > 2360 && framerate <= 2430)
			set_max_framerate_base100(ctx, 2360, min_framelength_en);
		else if (framerate > 1460 && framerate <= 1530)
			set_max_framerate_base100(ctx, 1460, min_framelength_en);
		else
			ret = FALSE;
		break;
	case 4:
		if (framerate > 5920 && framerate <= 6070)
			set_max_framerate_base100(ctx, 5920, min_framelength_en);
		else if (framerate > 3000 && framerate <= 3050)
			set_max_framerate_base100(ctx, 3000, min_framelength_en);
		else if (framerate > 2460 && framerate <= 2530)
			set_max_framerate_base100(ctx, 2460, min_framelength_en);
		else if (framerate > 2360 && framerate <= 2430)
			set_max_framerate_base100(ctx, 2360, min_framelength_en);
		else if (framerate > 1460 && framerate <= 1530)
			set_max_framerate_base100(ctx, 1460, min_framelength_en);
		else
			ret = FALSE;
		break;
	default:
		ret = FALSE;
	}

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			ctx->current_scenario_id, &linetime_in_ns, 0, 0);
		new_fps = 1000000000 / linetime_in_ns * 100 / ctx->frame_length;
	} else {
		/* frame rate is 100-base. */
		new_fps = ctx->pclk / ctx->line_length * 100 / ctx->frame_length;
	}

	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_RAW_VS ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE_VS)
		DRV_LOG(ctx,
	"sid:%u,cur_fps(100base):%u,flick_en:%d,min_fl_en:%u,fll(ctx/output_a/b/c/d/e):%u/%u/%u/%u/%u/%u,new_fps:%hu(100base)\n",
			ctx->current_scenario_id,
			framerate, ctx->autoflicker_en, min_framelength_en,
			ctx->frame_length,
			ctx->frame_length_in_lut[0],
			ctx->frame_length_in_lut[1],
			ctx->frame_length_in_lut[2],
			ctx->frame_length_in_lut[3],
			ctx->frame_length_in_lut[4],
			new_fps);
	else
		DRV_LOG(ctx, "cur_fps:%u(100base), flick_en:%d, min_fl_en:%u, new_fps:%hu(100base)\n",
			framerate, ctx->autoflicker_en, min_framelength_en,
			new_fps);

	return ret;
}

void set_long_exposure(struct subdrv_ctx *ctx, u16 exp_cnt)
{
	u32 shutter = ctx->exposure[IMGSENSOR_STAGGER_EXPOSURE_LE];
	u32 l_shutter = 0;
	u16 l_shift = 0;
	u32 scenario_id = ctx->current_scenario_id;
	u8 lshift_max = ctx->s_ctx.cit_lshift_max ? ctx->s_ctx.cit_lshift_max : DEFAULT_LSHIFT_MAX;
	u32 frame_length_max_without_lshift = ctx->s_ctx.frame_length_max_without_lshift ?
		ctx->s_ctx.frame_length_max_without_lshift : ctx->s_ctx.frame_length_max;

	if (exp_cnt != 1) {
		DRV_LOGE(ctx, "multi-exp (%u-exp) no support of exposure lshift!\n", exp_cnt);
		return;
	}

	if (shutter > (frame_length_max_without_lshift - ctx->s_ctx.mode[scenario_id].exposure_margin)) {
		if (ctx->mcss_init_info.enable_mcss) {
			DRV_LOGE(ctx, " MCSS no support of exposure lshift!\n");
			WRAP_AEE_EXCEPTION("[AEE] MCSS no support of exposure lshift!", "Err");
			return;
		}
		if (ctx->s_ctx.long_exposure_support == FALSE) {
			DRV_LOGE(ctx, "sensor no support of exposure lshift!\n");
			return;
		}
		if (ctx->s_ctx.reg_addr_exposure_lshift == PARAM_UNDEFINED) {
			DRV_LOGE(ctx, "please implement lshift register address\n");
			return;
		}
		for (l_shift = 1; l_shift < lshift_max; l_shift++) {
			l_shutter = ((shutter - 1) >> l_shift) + 1;
			if (l_shutter
				< (frame_length_max_without_lshift -
					ctx->s_ctx.mode[scenario_id].exposure_margin))
				break;
		}
		if (l_shift > lshift_max) {
			DRV_LOGE(ctx, "unable to set exposure:%u, set to max\n", shutter);
			l_shift = lshift_max;
		}

		/* At here, ctx->frame_length may greater than 0xFFFF and needs FLL_LSHIFT */
		ctx->frame_length = shutter + ctx->s_ctx.mode[scenario_id].exposure_margin;
		shutter = ((shutter - 1) >> l_shift) + 1;
		DRV_LOG_MUST(ctx, "long exposure mode: lshift %u times", l_shift);
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure_lshift, l_shift);
		ctx->l_shift = l_shift;
		/* Frame exposure mode customization for LE*/
		ctx->ae_frm_mode.frame_mode_1 = IMGSENSOR_AE_MODE_SE;
		ctx->ae_frm_mode.frame_mode_2 = IMGSENSOR_AE_MODE_SE;
		ctx->current_ae_effective_frame = 2;
	} else {
		if (ctx->s_ctx.reg_addr_exposure_lshift != PARAM_UNDEFINED) {
			set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure_lshift, 0);
			ctx->l_shift = l_shift;
		}
		ctx->current_ae_effective_frame = 2;
	}

	ctx->exposure[IMGSENSOR_STAGGER_EXPOSURE_LE] = shutter;
}

void set_shutter(struct subdrv_ctx *ctx, u64 shutter)
{
	set_shutter_frame_length(ctx, shutter, 0);
}

void set_shutter_frame_length(struct subdrv_ctx *ctx, u64 shutter, u32 frame_length)
{
	int fine_integ_line = 0;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	ctx->frame_length = frame_length ? frame_length : ctx->min_frame_length;
	check_current_scenario_id_bound(ctx);
	/* check boundary of shutter */
	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;
	shutter = FINE_INTEG_CONVERT(shutter, fine_integ_line);
	shutter = max_t(u64, shutter,
		(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[0].min);
	shutter = min_t(u64, shutter,
		(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[0].max);
	/* check boundary of framelength */
	ctx->frame_length = max(
		(u32)shutter + ctx->s_ctx.mode[ctx->current_scenario_id].exposure_margin,
			ctx->min_frame_length);
	ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);
	/* restore shutter */
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	ctx->exposure[0] = (u32) shutter;
	/* group hold start */
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	/* enable auto extend */
	if (ctx->s_ctx.reg_addr_auto_extend)
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_auto_extend, 0x01);
	/* write framelength */
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx, ctx->frame_length);
	else if (ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx, ctx->min_frame_length);
	/* write shutter */
	set_long_exposure(ctx, 1);
	if (ctx->s_ctx.reg_addr_exposure[0].addr[2]) {
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[0],
			(ctx->exposure[0] >> 16) & 0xFF);
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[1],
			(ctx->exposure[0] >> 8) & 0xFF);
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[2],
			ctx->exposure[0] & 0xFF);
	} else {
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[0],
			(ctx->exposure[0] >> 8) & 0xFF);
		set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[0].addr[1],
			ctx->exposure[0] & 0xFF);
	}
	DRV_LOG(ctx, "exp[0x%x], fll(input/output):%u/%u, flick_en:%d\n",
		ctx->exposure[0], frame_length, ctx->frame_length, ctx->autoflicker_en);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */
}
/**
 * @brief: set shutter for exp_cnt = 2 or 3
 * @param ctx: subdrv_ctx
 * @param shutters: current exposure from ae
 * @param exp_cnt: number depends on mode frame desc
 */
void set_hdr_tri_shutter(struct subdrv_ctx *ctx, u64 *shutters, u16 exp_cnt)
{
	int i = 0;
	u64 values[3] = {0};
	u32 frame_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};

	if (shutters != NULL) {
		for (i = 0; i < 3; i++)
			values[i] = (u64) *(shutters + i);
	}

	switch(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode) {
	case HDR_RAW_LBMF:
		set_multi_shutter_frame_length_in_lut(ctx,
			values, exp_cnt, 0, frame_length_in_lut);
		break;
	case HDR_RAW_DCG_RAW_VS:
	case HDR_RAW_DCG_COMPOSE_VS:
		// DONE: DCG-VS
		set_dcg_vs_multi_shutter_frame_length_in_lut(ctx,
			values, exp_cnt, 0, frame_length_in_lut);
		break;
	default:
		set_multi_shutter_frame_length(ctx, values, exp_cnt, 0);
		break;
	}
}

void set_lut_multi_shutter_frame_length(struct subdrv_ctx *ctx,
	u64 *shutters, u16 exp_cnt, u32 frame_length, u32 *frame_length_in_lut)
{
	switch(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode) {
	case HDR_RAW_LBMF:
		set_multi_shutter_frame_length_in_lut(ctx,
			shutters, exp_cnt, frame_length, frame_length_in_lut);
		break;
	case HDR_RAW_DCG_RAW_VS:
	case HDR_RAW_DCG_COMPOSE_VS:
		/* DCG-VS */
		{
			struct struct_dcg_vsl_info dcg_vsl_info = {0};
			u64 frame_time_ns = 0;

			/* convert frame length to frame time (lut-A) based */
			get_dcg_vsl_info_by_scenario(ctx, ctx->current_scenario_id, &dcg_vsl_info);
			frame_time_ns = line2ntime(frame_length,
					dcg_vsl_info.lut_info[IMGSENSOR_LUT_A].linetime_in_ns);

			set_dcg_vs_multi_shutter_frame_length_in_lut(ctx,
				shutters, exp_cnt, frame_time_ns, frame_length_in_lut);
		}
		break;
	default:
		set_multi_shutter_frame_length(ctx, shutters, exp_cnt, frame_length);
		break;
	}
}

void set_multi_shutter_frame_length(struct subdrv_ctx *ctx,
		u64 *shutters, u16 exp_cnt,	u16 frame_length)
{
	int i = 0;
	int fine_integ_line = 0;
	u16 last_exp_cnt = 1;
	u32 calc_fl[4] = {0};
	int readout_diff = 0;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	u32 rg_shutters[3] = {0};
	u32 cit_step = 0;
	u32 fll = 0, fll_temp = 0, s_fll;

	fll = frame_length ? frame_length : ctx->min_frame_length;
	if (exp_cnt > ARRAY_SIZE(ctx->exposure)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->exposure));
		exp_cnt = ARRAY_SIZE(ctx->exposure);
	}
	check_current_scenario_id_bound(ctx);

	/* check boundary of shutter */
	for (i = 1; i < ARRAY_SIZE(ctx->exposure); i++)
		last_exp_cnt += ctx->exposure[i] ? 1 : 0;
	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;
	cit_step = ctx->s_ctx.mode[ctx->current_scenario_id].coarse_integ_step;
	for (i = 0; i < exp_cnt; i++) {
		shutters[i] = FINE_INTEG_CONVERT(shutters[i], fine_integ_line);
		shutters[i] = max_t(u64, shutters[i],
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[i].min);
		shutters[i] = min_t(u64, shutters[i],
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[i].max);
		if (cit_step)
			shutters[i] = roundup(shutters[i], cit_step);
	}

	/* check boundary of framelength */
	/* - (1) previous se + previous me + current le */
	calc_fl[0] = (u32) shutters[0];
	for (i = 1; i < last_exp_cnt; i++)
		calc_fl[0] += ctx->exposure[i];
	calc_fl[0] += ctx->s_ctx.mode[ctx->current_scenario_id].exposure_margin*exp_cnt*exp_cnt;

	/* - (2) current se + current me + current le */
	calc_fl[1] = (u32) shutters[0];
	for (i = 1; i < exp_cnt; i++)
		calc_fl[1] += (u32) shutters[i];
	calc_fl[1] += ctx->s_ctx.mode[ctx->current_scenario_id].exposure_margin*exp_cnt*exp_cnt;

	/* - (3) readout time cannot be overlapped */
	calc_fl[2] =
		(ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
		ctx->s_ctx.mode[ctx->current_scenario_id].read_margin);
	if (last_exp_cnt == exp_cnt)
		for (i = 1; i < exp_cnt; i++) {
			readout_diff = ctx->exposure[i] - (u32) shutters[i];
			calc_fl[2] += readout_diff > 0 ? readout_diff : 0;
		}
	/* - (4) For DOL (non-FDOL), N-th frame SE and N+1-th frame LE readout cannot be overlapped */
	if ((ctx->s_ctx.hdr_type & HDR_SUPPORT_STAGGER_DOL) &&
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_STAGGER) {
		for (i = 1; i < last_exp_cnt; i++)
			calc_fl[3] += ctx->exposure[i];
		calc_fl[3] += ctx->s_ctx.mode[ctx->current_scenario_id].exposure_margin*exp_cnt*(exp_cnt-1);
		calc_fl[3] += ctx->readout_length + ctx->min_vblanking_line;
		DRV_LOG(ctx,
			"calc_fl[3]: %u, pre-LE/ME/SE (%u/%u/%u), cur-LE/ME/SE (%llu/%llu/%llu), readout_length:%u, min_vblanking_line:%u\n",
			calc_fl[3],
			ctx->exposure[0], ctx->exposure[1], ctx->exposure[2],
			shutters[0], shutters[1], shutters[2],
			ctx->readout_length,
			ctx->min_vblanking_line);
	}

	for (i = 0; i < ARRAY_SIZE(calc_fl); i++)
		fll = max(fll, calc_fl[i]);
	fll =	max(fll, ctx->min_frame_length);
	fll =	min(fll, ctx->s_ctx.frame_length_max);
	/* restore shutter */
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	for (i = 0; i < exp_cnt; i++)
		ctx->exposure[i] = (u32) shutters[i];
	/* group hold start */
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	/* enable auto extend */
	if (ctx->s_ctx.reg_addr_auto_extend)
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_auto_extend, 0x01);

	if (ctx->s_ctx.mode[ctx->current_scenario_id].sw_fl_delay) {
		fll_temp = ctx->frame_length_next;
		ctx->frame_length_next = fll;
		s_fll = calc_fl[0];
		for (i = 1; i < ARRAY_SIZE(calc_fl); i++)
			s_fll = max(s_fll, calc_fl[i]);
		if (s_fll > frame_length) {
			fll = s_fll;
			fll = max(fll, ctx->min_frame_length);
			fll = min(fll, ctx->s_ctx.frame_length_max);
			if(fll<fll_temp)
				fll = fll_temp;
		} else {
			if (s_fll > fll_temp)
				fll = s_fll;
			else
				fll = fll_temp;
		}
		DRV_LOG(ctx, "fll:%u, s_fll:%u, fll_temp:%u, frame_length:%u\n",
			fll, s_fll, fll_temp, frame_length);
	}
	ctx->frame_length = fll;
	/* write framelength */
	if (set_auto_flicker(ctx, 0) || frame_length || !ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx, ctx->frame_length);
	else if (ctx->s_ctx.reg_addr_auto_extend)
		write_frame_length(ctx, ctx->min_frame_length);
	/* write shutter */
	set_long_exposure(ctx, exp_cnt);
	if (ctx->s_ctx.stagger_rg_order == IMGSENSOR_STAGGER_RG_SE_FIRST) {
		/* swapping for customized sensor */
		u32 exposure[IMGSENSOR_STAGGER_EXPOSURE_CNT]; /* recover L and S exposure */

		for (i = 0; i < exp_cnt; i++)
			exposure[exp_cnt -1 - i] = (u32) shutters[i];

		for (i = 0; i < exp_cnt; i++)
			shutters[i] = exposure[i];
	}
	switch (exp_cnt) {
	case 1:
		rg_shutters[0] = (u32) shutters[0] / exp_cnt;
		break;
	case 2:
		if (ctx->s_ctx.stagger_fl_type == IMGSENSOR_STAGGER_FL_AUTO_DIVIDED) {
			rg_shutters[0] = (u32) shutters[0] / exp_cnt;
			rg_shutters[2] = (u32) shutters[1] / exp_cnt;
		} else {
			rg_shutters[0] = (u32) shutters[0];
			rg_shutters[2] = (u32) shutters[1];
		}
		break;
	case 3:
		if (ctx->s_ctx.stagger_fl_type == IMGSENSOR_STAGGER_FL_AUTO_DIVIDED) {
			rg_shutters[0] = (u32) shutters[0] / exp_cnt;
			rg_shutters[1] = (u32) shutters[1] / exp_cnt;
			rg_shutters[2] = (u32) shutters[2] / exp_cnt;
		} else {
			rg_shutters[0] = (u32) shutters[0];
			rg_shutters[1] = (u32) shutters[1];
			rg_shutters[2] = (u32) shutters[2];
		}
		break;
	default:
		break;
	}

	for (i = 0; i < 3; i++) {
		if (rg_shutters[i]) {
			if (ctx->s_ctx.reg_addr_exposure[i].addr[2]) {
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[0],
					(rg_shutters[i] >> 16) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[1],
					(rg_shutters[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[2],
					rg_shutters[i] & 0xFF);
			} else {
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[0],
					(rg_shutters[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_exposure[i].addr[1],
					rg_shutters[i] & 0xFF);
			}
		}
	}
	DRV_LOG(ctx, "exp[0x%x/0x%x/0x%x], fll(input/output):%u/%u, flick_en:%d\n",
		rg_shutters[0], rg_shutters[1], rg_shutters[2],
		frame_length, ctx->frame_length, ctx->autoflicker_en);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */
}

/**
 * @brief: This api is used to set multi shutter & fll for lbmf scenario,
 * and it differs from general api. It assigns LUT params and calculate
 * accurate value for shutter and fll that could be set to sensor.
 *
 * formula:
 * - auto mode (optional): assigned general FLL value.
 * - manual mode:
 * a. 2 exposure count
 * FLL_A: max (readout, shutter_B), then min (FLL_A, frame_length_max)
 * FLL_B: max (readout, shutter_A, FLL-FLL_A) , then min (FLL_B, frame_length_max)
 * b. 3 exposure count
 * FLL_A: max (readout, shutter_B), then min (FLL_A, frame_length_max)
 * FLL_B: max (readout, shutter_C), then min (FLL_B, frame_length_max)
 * FLL_C: max (readout, shutter_A, FLL-FLL_B-FLL_A) , then min (FLL_C, frame_length_max)
 * Compare to set_max_framerate_by_scenario, it has one additional condition to
 * check input framelength is valid or not.
 *
 * @param ctx: subdrv_ctx
 * @param shutters: current exposure from ae
 * @param exp_cnt: number depends on mode frame desc
 * @param frame_length: frame length for auto mode
 * @param frame_length_in_lut: frame length array in lut for manual mode
 */
void set_multi_shutter_frame_length_in_lut(struct subdrv_ctx *ctx,
	u64 *shutters, u16 exp_cnt, u32 frame_length, u32 *frame_length_in_lut)
{
	int i = 0;
	u16 last_exp_cnt = 1;
	int fine_integ_line = 0;
	u32 frame_length_step;
	u32 cit_step = 0;
	u32 cit_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 calc_fl_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	ctx->frame_length = frame_length ? frame_length : ctx->min_frame_length;

	if (exp_cnt > ARRAY_SIZE(ctx->exposure)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->exposure));
		exp_cnt = ARRAY_SIZE(ctx->exposure);
	}
	check_current_scenario_id_bound(ctx);

	/* check boundary of shutter */
	for (i = 1; i < ARRAY_SIZE(ctx->exposure); i++)
		last_exp_cnt += ctx->exposure[i] ? 1 : 0;

	fine_integ_line = ctx->s_ctx.mode[ctx->current_scenario_id].fine_integ_line;
	cit_step = ctx->s_ctx.mode[ctx->current_scenario_id].coarse_integ_step;
	frame_length_step = ctx->s_ctx.mode[ctx->current_scenario_id].framelength_step;

	/* manual mode */
	for (i = 0; i < exp_cnt; i++) {
		shutters[i] = FINE_INTEG_CONVERT(shutters[i], fine_integ_line);
		shutters[i] = max_t(u64, shutters[i],
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[i].min);
		shutters[i] = min_t(u64, shutters[i],
			(u64)ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_shutter_range[i].max);
		if (cit_step)
			shutters[i] = roundup(shutters[i], cit_step);

		/* update frame_length_in_lut */
		ctx->frame_length_in_lut[i] = frame_length_in_lut[i] ?
			frame_length_in_lut[i] : 0;
		/* check boundary of framelength in lut */
		ctx->frame_length_in_lut[i] =
			min(ctx->frame_length_in_lut[i], ctx->s_ctx.frame_length_max);
	}

	for (i = 0; i < exp_cnt; i++) {
		/* update cit_in_lut depends on exposure_order_in_lbmf */
		if (ctx->s_ctx.mode[ctx->current_scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
			/* 2exp: cit_lut_a = SE / cit_lut_b = LE */
			/* 3exp: cit_lut_a = SE / cit_lut_b = ME / cit_lut_c = LE */
			cit_in_lut[i] = shutters[exp_cnt - 1 - i];
		} else if (ctx->s_ctx.mode[ctx->current_scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_LE_FIRST) {
			/* 2exp: cit_lut_a = LE / cit_lut_b = SE */
			/* 3exp: cit_lut_a = LE / cit_lut_b = ME / cit_lut_c = SE */
			cit_in_lut[i] = shutters[i];
		} else {
			DRV_LOGE(ctx, "pls assign exposure_order_in_lbmf value!\n");
			return;
		}
	}

	switch (ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt) {
	case 2:
		if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 2) {
			/* fll_a_min = readout + xx lines(margin) */
			calc_fl_in_lut[0] =
				ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
				ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
			/* fll_a = max(readout, current shutter_a) */
			calc_fl_in_lut[0] =
				max(calc_fl_in_lut[0], cit_in_lut[0] + ctx->s_ctx.exposure_margin);
			/* fll_b_min = readout + xx lines(margin) */
			calc_fl_in_lut[1] =
				ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
				ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
			/* fll_b = max(readout, current shutter_b) */
			calc_fl_in_lut[1] =
				max(calc_fl_in_lut[1], cit_in_lut[1] + ctx->s_ctx.exposure_margin);

			/* fll_a = max(fll_a, userInput_fll_a) */
			ctx->frame_length_in_lut[0] =
				max(ctx->frame_length_in_lut[0], calc_fl_in_lut[0]);

			if (ctx->frame_length >= ctx->frame_length_pre_store_in_lut[1]) {
				/* fll_a = max(fll_a, fll-fll_b) */
				ctx->frame_length_in_lut[0] =
					max(ctx->frame_length_in_lut[0],
						ctx->frame_length - ctx->frame_length_pre_store_in_lut[1]);
			}

			/* fll_a = min(fll_a, fll_max) */
			ctx->frame_length_in_lut[0] =
				min(ctx->frame_length_in_lut[0], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[0] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[0], frame_length_step) :
				ctx->frame_length_in_lut[0];

			/* fll_b = max(fll_b, userInput_fll_b) */
			ctx->frame_length_in_lut[1] =
				max(ctx->frame_length_in_lut[1], calc_fl_in_lut[1]);
			/* fll_b = min(fll_b, fll_max) */
			ctx->frame_length_in_lut[1] =
				min(ctx->frame_length_in_lut[1], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[1] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[1], frame_length_step) :
				ctx->frame_length_in_lut[1];
			/* lut[2] no use, and assign zero */
			ctx->frame_length_in_lut[2] = 0;
			/* lut[3] no use, and assign zero */
			ctx->frame_length_in_lut[3] = 0;
			/* lut[4] no use, and assign zero */
			ctx->frame_length_in_lut[4] = 0;
		} else if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 3) {
			/* fll_a_min = readout + xx lines(margin) */
			calc_fl_in_lut[0] =
				ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
				ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
			/* fll_a = max(readout, current shutter_b) */
			calc_fl_in_lut[0] =
				max(calc_fl_in_lut[0], cit_in_lut[1] + ctx->s_ctx.exposure_margin);
			/* fll_b_min = readout + xx lines(margin) */
			calc_fl_in_lut[1] =
				ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
				ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
			/* fll_b = max(readout, current shutter_a) */
			calc_fl_in_lut[1] =
				max(calc_fl_in_lut[1], cit_in_lut[0] + ctx->s_ctx.exposure_margin);

			/* fll_a = max(fll_a, userInput_fll_a) */
			ctx->frame_length_in_lut[0] =
				max(ctx->frame_length_in_lut[0], calc_fl_in_lut[0]);
			/* fll_a = min(fll_a, fll_max) */
			ctx->frame_length_in_lut[0] =
				min(ctx->frame_length_in_lut[0], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[0] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[0], frame_length_step) :
				ctx->frame_length_in_lut[0];
			/* fll_b = max(fll_b, userInput_fll_b) */
			ctx->frame_length_in_lut[1] =
				max(ctx->frame_length_in_lut[1], calc_fl_in_lut[1]);

			if (ctx->frame_length >= ctx->frame_length_in_lut[0]) {
				/* fll_b = max(fll_b, fll-fll_a) */
				ctx->frame_length_in_lut[1] =
					max(ctx->frame_length_in_lut[1],
						ctx->frame_length - ctx->frame_length_in_lut[0]);
			}

			/* fll_b = min(fll_b, fll_max) */
			ctx->frame_length_in_lut[1] =
				min(ctx->frame_length_in_lut[1], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[1] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[1], frame_length_step) :
				ctx->frame_length_in_lut[1];
			/* lut[2] no use, and assign zero */
			ctx->frame_length_in_lut[2] = 0;
			/* lut[3] no use, and assign zero */
			ctx->frame_length_in_lut[3] = 0;
			/* lut[4] no use, and assign zero */
			ctx->frame_length_in_lut[4] = 0;
		} else {
			DRV_LOGE(ctx, "pls check delay_frame value!\n");
			return;
		}
		break;
	case 3:
		if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 2) {
			/* fll_a_min = readout + xx lines(margin) */
			calc_fl_in_lut[0] =
				ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
				ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
			/* fll_a = max(readout, current shutter_a) */
			calc_fl_in_lut[0] =
				max(calc_fl_in_lut[0], cit_in_lut[0] + ctx->s_ctx.exposure_margin);
			/* fll_b_min = readout + xx lines(margin) */
			calc_fl_in_lut[1] =
				ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
				ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
			/* fll_b = max(readout, current shutter_b) */
			calc_fl_in_lut[1] =
				max(calc_fl_in_lut[1], cit_in_lut[1] + ctx->s_ctx.exposure_margin);
			/* fll_c_min = readout + xx lines(margin) */
			calc_fl_in_lut[2] =
				ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
				ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
			/* fll_c = max(readout, current shutter_c) */
			calc_fl_in_lut[2] =
				max(calc_fl_in_lut[2], cit_in_lut[2] + ctx->s_ctx.exposure_margin);

			/* fll_a = max(fll_a, userInput_fll_a) */
			ctx->frame_length_in_lut[0] =
				max(ctx->frame_length_in_lut[0], calc_fl_in_lut[0]);

			if (ctx->frame_length >=
				(ctx->frame_length_pre_store_in_lut[1] +
				ctx->frame_length_pre_store_in_lut[2])) {
				/* fll_a = max(fll_a, fll-fll_b-fll_c) */
				ctx->frame_length_in_lut[0] =
					max(ctx->frame_length_in_lut[0],
						(ctx->frame_length -
						ctx->frame_length_pre_store_in_lut[1] -
						ctx->frame_length_pre_store_in_lut[2]));
			}

			/* fll_a = min(fll_a, fll_max) */
			ctx->frame_length_in_lut[0] =
				min(ctx->frame_length_in_lut[0], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[0] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[0], frame_length_step) :
				ctx->frame_length_in_lut[0];

			/* fll_b = max(fll_b, userInput_fll_b) */
			ctx->frame_length_in_lut[1] =
				max(ctx->frame_length_in_lut[1], calc_fl_in_lut[1]);
			/* fll_b = min(fll_b, fll_max) */
			ctx->frame_length_in_lut[1] =
				min(ctx->frame_length_in_lut[1], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[1] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[1], frame_length_step) :
				ctx->frame_length_in_lut[1];

			/* fll_c = max(fll_c, userInput_fll_c) */
			ctx->frame_length_in_lut[2] =
				max(ctx->frame_length_in_lut[2], calc_fl_in_lut[2]);
			/* fll_c = min(fll_c, fll_max) */
			ctx->frame_length_in_lut[2] =
				min(ctx->frame_length_in_lut[2], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[2] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[2], frame_length_step) :
				ctx->frame_length_in_lut[2];
			/* lut[3] no use, and assign zero */
			ctx->frame_length_in_lut[3] = 0;
			/* lut[4] no use, and assign zero */
			ctx->frame_length_in_lut[4] = 0;
		} else if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 3) {
			/* fll_a_min = readout + xx lines(margin) */
			calc_fl_in_lut[0] =
				ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
				ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
			/* fll_a = max(readout, current shutter_b) */
			calc_fl_in_lut[0] =
				max(calc_fl_in_lut[0], cit_in_lut[1] + ctx->s_ctx.exposure_margin);
			/* fll_b_min = readout + xx lines(margin) */
			calc_fl_in_lut[1] =
				ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
				ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
			/* fll_b = max(readout, current shutter_c) */
			calc_fl_in_lut[1] =
				max(calc_fl_in_lut[1], cit_in_lut[2] + ctx->s_ctx.exposure_margin);
			/* fll_c_min = readout + xx lines(margin) */
			calc_fl_in_lut[2] =
				ctx->s_ctx.mode[ctx->current_scenario_id].readout_length +
				ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;
			/* fll_c = max(readout, current shutter_a) */
			calc_fl_in_lut[2] =
				max(calc_fl_in_lut[2], cit_in_lut[0] + ctx->s_ctx.exposure_margin);

			/* fll_a = max(fll_a, userInput_fll_a) */
			ctx->frame_length_in_lut[0] =
				max(ctx->frame_length_in_lut[0], calc_fl_in_lut[0]);
			/* fll_a = min(fll_a, fll_max) */
			ctx->frame_length_in_lut[0] =
				min(ctx->frame_length_in_lut[0], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[0] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[0], frame_length_step) :
				ctx->frame_length_in_lut[0];
			/* fll_b = max(fll_b, userInput_fll_b) */
			ctx->frame_length_in_lut[1] =
				max(ctx->frame_length_in_lut[1], calc_fl_in_lut[1]);
			/* fll_b = min(fll_b, fll_max) */
			ctx->frame_length_in_lut[1] =
				min(ctx->frame_length_in_lut[1], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[1] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[1], frame_length_step) :
				ctx->frame_length_in_lut[1];
			/* fll_c = max(fll_c, userInput_fll_c) */
			ctx->frame_length_in_lut[2] =
				max(ctx->frame_length_in_lut[2], calc_fl_in_lut[2]);

			if (ctx->frame_length >=
				(ctx->frame_length_in_lut[0] + ctx->frame_length_in_lut[1])) {
				/* fll_c = max(fll_c, fll-fll_b-fll_a) */
				ctx->frame_length_in_lut[2] =
					max(ctx->frame_length_in_lut[2],
						(ctx->frame_length - ctx->frame_length_in_lut[1] -
						ctx->frame_length_in_lut[0]));
			}

			/* fll_c = min(fll_c, fll_max) */
			ctx->frame_length_in_lut[2] =
				min(ctx->frame_length_in_lut[2], ctx->s_ctx.frame_length_max);
			ctx->frame_length_in_lut[2] = frame_length_step ?
				roundup(ctx->frame_length_in_lut[2], frame_length_step) :
				ctx->frame_length_in_lut[2];
			/* lut[3] no use, and assign zero */
			ctx->frame_length_in_lut[3] = 0;
			/* lut[4] no use, and assign zero */
			ctx->frame_length_in_lut[4] = 0;
		} else {
			DRV_LOGE(ctx, "pls check delay_frame value!\n");
			return;
		}
		break;
	default:
		break;
	}

	DRV_LOG(ctx,
		"sid:%u,before update exposure_lut:%u/%u/%u/%u/%u,flInPreStoreLUT(input/ctx/output_a/b/c/d/e):%u/%u/%u/%u/%u/%u/%u\n",
		ctx->current_scenario_id,
		ctx->exposure[0], ctx->exposure[1], ctx->exposure[2], ctx->exposure[3], ctx->exposure[4],
		frame_length, ctx->frame_length,
		ctx->frame_length_pre_store_in_lut[0],
		ctx->frame_length_pre_store_in_lut[1],
		ctx->frame_length_pre_store_in_lut[2],
		ctx->frame_length_pre_store_in_lut[3],
		ctx->frame_length_pre_store_in_lut[4]);

	/* restore shutter & update framelength */
	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	ctx->frame_length = 0;
	for (i = 0; i < exp_cnt; i++) {
		ctx->exposure[i] = (u32) shutters[i];
		if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 2) {
			/* frame_length = fll_pre_store_in_lut[1] + fll_pre_store_in_lut[2] +
			 * fll_pre_store_in_lut[i] + ... + fll_current_cal_in_lut[0]
			 */
			if (i == 0)
				ctx->frame_length += ctx->frame_length_in_lut[i];
			else
				ctx->frame_length += ctx->frame_length_pre_store_in_lut[i];
		} else if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 3) {
			/* frame_length = fll_current_cal_in_lut[0] + fll_current_cal_in_lut[1] +
			 * ... + fll_current_cal_in_lut[i]
			 */
			ctx->frame_length += ctx->frame_length_in_lut[i];
		} else {
			DRV_LOGE(ctx, "pls check delay_frame value!\n");
			return;
		}
	}
	/* check boundary of framelength */
	ctx->frame_length =	max(ctx->frame_length, ctx->min_frame_length);
	/* group hold start */
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	/* enable auto extend */
	if (ctx->s_ctx.reg_addr_auto_extend)
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_auto_extend, 0x01);
	/* write framelength */
	set_auto_flicker(ctx, 0);
	write_frame_length_in_lut(ctx, ctx->frame_length, ctx->frame_length_in_lut);
	/* update prestore framelength */
	for (i = 0; i < exp_cnt; i++) {
		// store fll_current_cal_in_lut into fll_pre_store_in_lut for next calculation
		ctx->frame_length_pre_store_in_lut[i] = ctx->frame_length_in_lut[i];
	}
	/* write shutter: LUT register differs from DOL */
	if (ctx->s_ctx.reg_addr_exposure_lshift != PARAM_UNDEFINED) {
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure_lshift, 0);
		ctx->l_shift = 0;
	}
	if (ctx->s_ctx.reg_addr_frame_length_lshift != PARAM_UNDEFINED)
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_frame_length_lshift, 0);

	for (i = 0; i < 3; i++) {
		if (cit_in_lut[i]) {
			if (ctx->s_ctx.reg_addr_exposure_in_lut[i].addr[2]) {
				set_i2c_buffer(ctx,
					ctx->s_ctx.reg_addr_exposure_in_lut[i].addr[0],
					(cit_in_lut[i] >> 16) & 0xFF);
				set_i2c_buffer(ctx,
					ctx->s_ctx.reg_addr_exposure_in_lut[i].addr[1],
					(cit_in_lut[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,
					ctx->s_ctx.reg_addr_exposure_in_lut[i].addr[2],
					cit_in_lut[i] & 0xFF);
			} else {
				set_i2c_buffer(ctx,
					ctx->s_ctx.reg_addr_exposure_in_lut[i].addr[0],
					(cit_in_lut[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,
					ctx->s_ctx.reg_addr_exposure_in_lut[i].addr[1],
					cit_in_lut[i] & 0xFF);
			}
		}
	}

	if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 2) {
		u64 aeb_ae_ctrl_cnt;

		spin_lock(&ctx->aeb_ae_ctrl_cnt_lock);
		ctx->aeb_ae_ctrl_cnt++;
		aeb_ae_ctrl_cnt = ctx->aeb_ae_ctrl_cnt;
		spin_unlock(&ctx->aeb_ae_ctrl_cnt_lock);
		DRV_LOG(ctx,
			"sid:%u,shutter(input/lut):0x%llx/%llx/%llx,%x/%x/%x,flInLUT(input/ctx/output_a/b/c/d/e):%u/%u/%u/%u/%u/%u/%u,flick_en:%d,delay_frame:%u,fl(pre_store):%u/%u/%u/%u/%u,aeb_ctrl_cnt:%llu\n",
			ctx->current_scenario_id,
			shutters[0], shutters[1], shutters[2],
			cit_in_lut[0], cit_in_lut[1], cit_in_lut[2],
			frame_length, ctx->frame_length,
			ctx->frame_length_in_lut[0],
			ctx->frame_length_in_lut[1],
			ctx->frame_length_in_lut[2],
			ctx->frame_length_in_lut[3],
			ctx->frame_length_in_lut[4],
			ctx->autoflicker_en,
			ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame,
			ctx->frame_length_pre_store_in_lut[0],
			ctx->frame_length_pre_store_in_lut[1],
			ctx->frame_length_pre_store_in_lut[2],
			ctx->frame_length_pre_store_in_lut[3],
			ctx->frame_length_pre_store_in_lut[4],
			aeb_ae_ctrl_cnt);
	} else {
		DRV_LOG(ctx,
			"sid:%u,shutter(input/lut):0x%llx/%llx/%llx,%x/%x/%x,flInLUT(input/ctx/output_a/b/c/d/e):%u/%u/%u/%u/%u/%u/%u,flick_en:%d,delay_frame:%u,fl(pre_store):%u/%u/%u/%u/%u\n",
			ctx->current_scenario_id,
			shutters[0], shutters[1], shutters[2],
			cit_in_lut[0], cit_in_lut[1], cit_in_lut[2],
			frame_length, ctx->frame_length,
			ctx->frame_length_in_lut[0],
			ctx->frame_length_in_lut[1],
			ctx->frame_length_in_lut[2],
			ctx->frame_length_in_lut[3],
			ctx->frame_length_in_lut[4],
			ctx->autoflicker_en,
			ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame,
			ctx->frame_length_pre_store_in_lut[0],
			ctx->frame_length_pre_store_in_lut[1],
			ctx->frame_length_pre_store_in_lut[2],
			ctx->frame_length_pre_store_in_lut[3],
			ctx->frame_length_pre_store_in_lut[4]);
	}
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */
}

/**
 * @brief: This api is used to set multi shutter & fll for dcg+vs scenario,
 * and it differs from general api. It assigns LUT params and calculate
 * accurate value for shutter and fll that could be set to sensor.
 * And the LUT params might be different between LUTs
 *
 * formula:
 * - auto mode (optional): assigned general FLL value.
 * - manual mode:
 * a. 2 exposure count
 * FLL_A: max (readout, shutter_B), then min (FLL_A, frame_length_max)
 * FLL_B: max (readout, shutter_A, FLL-FLL_A) , then min (FLL_B, frame_length_max)
 * b. 3 exposure count
 * FLL_A: max (readout, shutter_B), then min (FLL_A, frame_length_max)
 * FLL_B: max (readout, shutter_C), then min (FLL_B, frame_length_max)
 * FLL_C: max (readout, shutter_A, FLL-FLL_B-FLL_A) , then min (FLL_C, frame_length_max)
 * Compare to set_max_framerate_by_scenario, it has one additional condition to
 * check input framelength is valid or not.
 *
 * @param ctx: subdrv_ctx
 * @param shutters: current exposure from ae
 * @param exp_cnt: number depends on mode frame desc
 * @param frame_time_ns: toal frame time for auto mode (in ns)
 * @param frame_length_in_lut: frame length array in lut for manual mode
 */
void set_dcg_vs_multi_shutter_frame_length_in_lut(struct subdrv_ctx *ctx,
	u64 *shutters, u16 exp_cnt, u64 frame_time_ns, u32 *frame_length_in_lut)
{
	int i = 0;
	u32 cit_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	u32 calc_fl_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	struct struct_dcg_vsl_info dcg_vsl_info = {0};
	int dcg_vs_exp_id[] = {
		IMGSENSOR_EXPOSURE_LE,
		IMGSENSOR_EXPOSURE_SE
	};
	u32 frame_length_step_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT];
	u32 readout_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT];
	u8 read_margin_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT];
	u32 linetime_ns_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT];
	u64 lut_0_ft_ns;


	// NOTE: ctx->frame_length is not precise
	//ctx->frame_length = frame_length ? frame_length : ctx->min_frame_length;

	// fall back to max fps
	if (!frame_time_ns && ctx->s_ctx.mode[ctx->current_scenario_id].max_framerate) {
		frame_time_ns = 10000000000 / ctx->s_ctx.mode[ctx->current_scenario_id].max_framerate;
	}

	if (exp_cnt > ARRAY_SIZE(ctx->exposure)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->exposure));
		exp_cnt = ARRAY_SIZE(ctx->exposure);
	}
	check_current_scenario_id_bound(ctx);

	get_dcg_vsl_info_by_scenario(ctx, ctx->current_scenario_id, &dcg_vsl_info);

	if (ctx->s_ctx.mode[ctx->current_scenario_id].exposure_order_in_lbmf == IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
		// SE first
		dcg_vs_exp_id[0] = IMGSENSOR_EXPOSURE_SE;
		dcg_vs_exp_id[1] = IMGSENSOR_EXPOSURE_LE;
	}

	/* get static data of lut */
	memset(frame_length_step_in_lut, 0, sizeof(frame_length_step_in_lut));
	memset(readout_length_in_lut, 0, sizeof(readout_length_in_lut));
	memset(read_margin_in_lut, 0, sizeof(read_margin_in_lut));
	memset(linetime_ns_in_lut, 0, sizeof(linetime_ns_in_lut));
	for (i = 0; i < ARRAY_SIZE(dcg_vs_exp_id); i++) {
		frame_length_step_in_lut[i] = dcg_vsl_info.lut_info[i].framelength_step;
		readout_length_in_lut[i] = dcg_vsl_info.lut_info[i].readout_length;
		read_margin_in_lut[i] = dcg_vsl_info.lut_info[i].read_margin;
		linetime_ns_in_lut[i] = dcg_vsl_info.lut_info[i].linetime_in_ns;
	}

	/* manual mode */
	for (i = 0; i < exp_cnt && i < ARRAY_SIZE(dcg_vs_exp_id); i++) {
		shutters[i] = FINE_INTEG_CONVERT(shutters[i], dcg_vsl_info.exp_info[dcg_vs_exp_id[i]].fine_integ_line);
		shutters[i] = max_t(u64, shutters[i], dcg_vsl_info.exp_info[dcg_vs_exp_id[i]].shutter_min);
		shutters[i] = min_t(u64, shutters[i], dcg_vsl_info.exp_info[dcg_vs_exp_id[i]].shutter_max);
		if (dcg_vsl_info.exp_info[dcg_vs_exp_id[i]].coarse_integ_step)
			shutters[i] = roundup(shutters[i], dcg_vsl_info.exp_info[dcg_vs_exp_id[i]].coarse_integ_step);

		/* update frame_length_in_lut */
		ctx->frame_length_in_lut[i] = frame_length_in_lut[i] ?
			frame_length_in_lut[i] : 0;
		/* check boundary of framelength in lut */
		ctx->frame_length_in_lut[i] =
			min(ctx->frame_length_in_lut[i], ctx->s_ctx.frame_length_max);

		/* update cit_in_lut */
		cit_in_lut[i] = convert_to_cit_in_lut(ctx, &dcg_vsl_info, dcg_vs_exp_id[i], shutters[i]);
	}

	DRV_LOG_MUST(ctx,
		"sid:%u,frame_length_in_lut:%u/%u/%u/%u/%u, ctx->frame_length_in_lut:%u/%u/%u/%u/%u\n",
		ctx->current_scenario_id,
		frame_length_in_lut[0],
		frame_length_in_lut[1],
		frame_length_in_lut[2],
		frame_length_in_lut[3],
		frame_length_in_lut[4],
		ctx->frame_length_in_lut[0],
		ctx->frame_length_in_lut[1],
		ctx->frame_length_in_lut[2],
		ctx->frame_length_in_lut[3],
		ctx->frame_length_in_lut[4]);

	if (ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt == 3 && dcg_vsl_info.lut_cnt == 2) {
		// DCG+VS. lut-a + lut-b
		/* fll_a_min = readout + xx lines(margin) */
		calc_fl_in_lut[0] = readout_length_in_lut[0] + read_margin_in_lut[0];
		/* fll_a = max(readout, current shutter_b) */
		calc_fl_in_lut[0] = max(calc_fl_in_lut[0],
				cit_in_lut[1] + dcg_vsl_info.exp_info[dcg_vs_exp_id[1]].exposure_margin);
		/* fll_b_min = readout + xx lines(margin) */
		calc_fl_in_lut[1] = readout_length_in_lut[1] + read_margin_in_lut[1];
		/* fll_b = max(readout, current shutter_a) */
		calc_fl_in_lut[1] = max(calc_fl_in_lut[1],
				cit_in_lut[0] + dcg_vsl_info.exp_info[dcg_vs_exp_id[0]].exposure_margin);

		/* fll_a = max(fll_a, userInput_fll_a) */
		ctx->frame_length_in_lut[0] =
			max(ctx->frame_length_in_lut[0], calc_fl_in_lut[0]);
		/* fll_a = min(fll_a, fll_max) */
		ctx->frame_length_in_lut[0] =
			min(ctx->frame_length_in_lut[0], ctx->s_ctx.frame_length_max);
		ctx->frame_length_in_lut[0] = frame_length_step_in_lut[0] ?
			roundup(ctx->frame_length_in_lut[0], frame_length_step_in_lut[0]) :
			ctx->frame_length_in_lut[0];
		/* fll_b = max(fll_b, userInput_fll_b) */
		ctx->frame_length_in_lut[1] =
			max(ctx->frame_length_in_lut[1], calc_fl_in_lut[1]);

		DRV_LOG_MUST(ctx,
			"sid:%u,calc_fl_in_lut:%u/%u/%u/%u/%u, ctx->frame_length_in_lut:%u/%u/%u/%u/%u\n",
			ctx->current_scenario_id,
			calc_fl_in_lut[0],
			calc_fl_in_lut[1],
			calc_fl_in_lut[2],
			calc_fl_in_lut[3],
			calc_fl_in_lut[4],
			ctx->frame_length_in_lut[0],
			ctx->frame_length_in_lut[1],
			ctx->frame_length_in_lut[2],
			ctx->frame_length_in_lut[3],
			ctx->frame_length_in_lut[4]);

		lut_0_ft_ns = line2ntime(ctx->frame_length_in_lut[0], linetime_ns_in_lut[0]);
		DRV_LOG_MUST(ctx, "lut-A ft = %llu ns, frame time = %llu ns\n", lut_0_ft_ns, frame_time_ns);
		if (frame_time_ns >= lut_0_ft_ns) {
			/* fll_b = max(fll_b, fll-fll_a) */
			ctx->frame_length_in_lut[1] = max(ctx->frame_length_in_lut[1],
				    ntime2line(frame_time_ns - lut_0_ft_ns, linetime_ns_in_lut[1]));
		}

		/* fll_b = min(fll_b, fll_max) */
		ctx->frame_length_in_lut[1] =
			min(ctx->frame_length_in_lut[1], ctx->s_ctx.frame_length_max);
		ctx->frame_length_in_lut[1] = frame_length_step_in_lut[1] ?
			roundup(ctx->frame_length_in_lut[1], frame_length_step_in_lut[1]) :
			ctx->frame_length_in_lut[1];
		/* lut[2] no use, and assign zero */
		ctx->frame_length_in_lut[2] = 0;
		/* lut[3] no use, and assign zero */
		ctx->frame_length_in_lut[3] = 0;
		/* lut[4] no use, and assign zero */
		ctx->frame_length_in_lut[4] = 0;

		/* restore shutter & update framelength */
		memset(ctx->exposure, 0, sizeof(ctx->exposure));
		//ctx->frame_length = 0;
		/* keep each luts exposure value (fine_integ_line divided) */
		for (i = 0; i < exp_cnt; i++) {
			ctx->exposure[i] = shutters[i];
			//ctx->frame_length += ctx->frame_length_in_lut[i];
		}
		/* check boundary of framelength */
		//ctx->frame_length = max(ctx->frame_length, ctx->min_frame_length);
		/* group hold start */
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 1);
		/* enable auto extend */
		if (ctx->s_ctx.reg_addr_auto_extend)
			set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_auto_extend, 0x01);

		set_auto_flicker(ctx, 0);

		/* convert ctx->frame_length base on lut-A line time */
		ctx->frame_length = ctx->frame_length_in_lut[0] +
				ntime2line(line2ntime(ctx->frame_length_in_lut[1], linetime_ns_in_lut[1]),
					   linetime_ns_in_lut[0]);

		/* write framelength */
		write_dcg_vs_frame_length_in_lut(ctx, ctx->frame_length, ctx->frame_length_in_lut);

		/* write shutter: LUT register differs from DOL */
		if (ctx->s_ctx.reg_addr_exposure_lshift != PARAM_UNDEFINED) {
			set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_exposure_lshift, 0);
			ctx->l_shift = 0;
		}

		/* set cit index 0 LE(DCG) & 2 SE(VS) */
		for (i = 0; i < 2; i++) {
			if (shutters[i]) {
				set_i2c_buffer(ctx,
					       ctx->s_ctx.reg_addr_exposure_in_lut[i].addr[0],
					       (shutters[i] >> 8) & 0xFF);
				set_i2c_buffer(ctx,
					       ctx->s_ctx.reg_addr_exposure_in_lut[i].addr[1],
					       shutters[i] & 0xFF);
			}
		}

		DRV_LOG(ctx,
			"sid:%u,shutter(input/lut):0x%llx/%llx/%llx,0x%x/0x%x/0x%x,flInLUT(input/ctx/output_a/b/c/d/e):%llu/%u/%u/%u/%u/%u/%u,flick_en:%d\n",
			ctx->current_scenario_id,
			shutters[0], shutters[1], shutters[2],
			cit_in_lut[0], cit_in_lut[1], cit_in_lut[2],
			frame_time_ns, ctx->frame_length,
			ctx->frame_length_in_lut[0],
			ctx->frame_length_in_lut[1],
			ctx->frame_length_in_lut[2],
			ctx->frame_length_in_lut[3],
			ctx->frame_length_in_lut[4],
			ctx->autoflicker_en);
		if (!ctx->ae_ctrl_gph_en) {
			if (gph)
				ctx->s_ctx.s_gph((void *)ctx, 0);
			commit_i2c_buffer(ctx);
		}
	}
	/* group hold end */
}

u16 gain2reg(u32 gain)
{
	return (16384 - (16384 * BASEGAIN) / gain);
}

void set_gain(struct subdrv_ctx *ctx, u32 gain)
{
	u16 rg_gain;
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	u32 i = 0, sid = ctx->current_scenario_id;
	enum IMGSENSOR_HDR_MODE_ENUM hdr_mode = ctx->s_ctx.mode[sid].hdr_mode;

	/* check DCG gain base*/
	if (hdr_mode == HDR_RAW_DCG_RAW || hdr_mode == HDR_RAW_DCG_COMPOSE) {
		if (ctx->s_ctx.mode[sid].dcg_info.dcg_gain_base == IMGSENSOR_DCG_GAIN_LCG_BASE)
			i = IMGSENSOR_EXPOSURE_ME;
	}

	/* check boundary of gain */
	gain = max(gain,
		ctx->s_ctx.mode[sid].multi_exposure_ana_gain_range[i].min);
	gain = min(gain,
		ctx->s_ctx.mode[sid].multi_exposure_ana_gain_range[i].max);
	/* mapping of gain to register value */
	if (ctx->s_ctx.g_gain2reg != NULL)
		rg_gain = ctx->s_ctx.g_gain2reg(gain);
	else
		rg_gain = gain2reg(gain);
	/* restore gain */
	memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
	ctx->ana_gain[0] = gain;
	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	/* write gain */
	set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_ana_gain[0].addr[0],
		(rg_gain >> 8) & 0xFF);
	set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_ana_gain[0].addr[1],
		rg_gain & 0xFF);
	DRV_LOG(ctx, "gain[0x%x]\n", rg_gain);
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 0);
	/* commit_i2c_buffer(ctx);*/
	/* group hold end */
}

void set_hdr_tri_gain(struct subdrv_ctx *ctx, u64 *gains, u16 exp_cnt)
{
	int i = 0;
	u32 values[3] = {0};

	if (gains != NULL) {
		for (i = 0; i < 3; i++)
			values[i] = (u32) *(gains + i);
	}

	switch(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode) {
	case HDR_RAW_LBMF:
		set_multi_gain_in_lut(ctx, values, exp_cnt);
		break;
	case HDR_RAW_DCG_RAW_VS:
	case HDR_RAW_DCG_COMPOSE_VS:
		// DONE: DCG-VS
		set_dcg_vs_multi_gain_in_lut(ctx, values, exp_cnt);
		break;
	default:
		set_multi_gain(ctx,	values, exp_cnt);
		break;
	}
}

void set_multi_gain(struct subdrv_ctx *ctx, u32 *gains, u16 exp_cnt)
{
	int i = 0;
	u16 rg_gains[3] = {0};
	u8 has_gains[3] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	if (exp_cnt > ARRAY_SIZE(ctx->ana_gain)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->ana_gain));
		exp_cnt = ARRAY_SIZE(ctx->ana_gain);
	}

	for (i = 0; i < exp_cnt; i++) {
		/* check boundary of gain */
		gains[i] = max(gains[i],
			ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].min);
		gains[i] = min(gains[i],
			ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].max);
		/* mapping of gain to register value */
		if (ctx->s_ctx.g_gain2reg != NULL)
			gains[i] = ctx->s_ctx.g_gain2reg(gains[i]);
		else
			gains[i] = gain2reg(gains[i]);
	}
	/* restore gain */
	memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
	for (i = 0; i < exp_cnt; i++)
		ctx->ana_gain[i] = gains[i];
	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);
	/* write gain */
	memset(has_gains, 1, sizeof(has_gains));
	if (ctx->s_ctx.stagger_rg_order == IMGSENSOR_STAGGER_RG_SE_FIRST) {
		/* swapping for customized sensor */
		u32 ana_gain[IMGSENSOR_STAGGER_EXPOSURE_CNT]; /* recover L and S gain */

		for (i = 0; i < exp_cnt; i++)
			ana_gain[exp_cnt -1 - i] = (u32) gains[i];

		for (i = 0; i < exp_cnt; i++)
			gains[i] = ana_gain[i];
	}
	switch (exp_cnt) {
	case 2:
		rg_gains[0] = gains[0];
		has_gains[1] = 0;
		rg_gains[2] = gains[1];
		break;
	case 3:
		rg_gains[0] = gains[0];
		rg_gains[1] = gains[1];
		rg_gains[2] = gains[2];
		break;
	default:
		has_gains[0] = 0;
		has_gains[1] = 0;
		has_gains[2] = 0;
		break;
	}
	for (i = 0; i < 3; i++) {
		if (has_gains[i]) {
			set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_ana_gain[i].addr[0],
				(rg_gains[i] >> 8) & 0xFF);
			set_i2c_buffer(ctx,	ctx->s_ctx.reg_addr_ana_gain[i].addr[1],
				rg_gains[i] & 0xFF);
		}
	}
	DRV_LOG(ctx, "reg[lg/mg/sg]: 0x%x 0x%x 0x%x\n", rg_gains[0], rg_gains[1], rg_gains[2]);
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 0);
	/*commit_i2c_buffer(ctx);*/
	/* group hold end */
}
/**
 * @brief: This api is used to set multi gain for lbmf scenario,
 * and it differs from general api. It assigns LUT params and calculate
 * accurate value for gain that could be set to sensor.
 * @param ctx: subdrv_ctx
 * @param gains: current gain from ae
 * @param exp_cnt: number depends on mode frame desc
 */
void set_multi_gain_in_lut(struct subdrv_ctx *ctx, u32 *gains, u16 exp_cnt)
{
	int i = 0;
	u16 ana_gain_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	if (exp_cnt > ARRAY_SIZE(ctx->ana_gain)) {
		DRV_LOGE(ctx,
			"invalid exp_cnt:%u>%lu\n",
			exp_cnt, ARRAY_SIZE(ctx->ana_gain));
		exp_cnt = ARRAY_SIZE(ctx->ana_gain);
	}
	for (i = 0; i < exp_cnt; i++) {
		/* check boundary of gain */
		gains[i] = max(gains[i],
			ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].min);
		gains[i] = min(gains[i],
			ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].max);
		/* mapping of gain to register value */
		if (ctx->s_ctx.g_gain2reg != NULL)
			gains[i] = ctx->s_ctx.g_gain2reg(gains[i]);
		else
			gains[i] = gain2reg(gains[i]);
	}
	for (i = 0; i < exp_cnt; i++) {
		/* update ana_gain_in_lut depends on exposure_order_in_lbmf */
		if (ctx->s_ctx.mode[ctx->current_scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
			/* 2exp: ana_gain_lut_a = SE / ana_gain_lut_b = LE */
			/* 3exp: ana_gain_lut_a = SE / ana_gain_lut_b = ME / ana_gain_lut_c = LE */
			ana_gain_in_lut[i] = gains[exp_cnt - 1 - i];
		} else if (ctx->s_ctx.mode[ctx->current_scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_LE_FIRST) {
			/* 2exp: ana_gain_lut_a = LE / ana_gain_lut_b = SE */
			/* 3exp: ana_gain_lut_a = LE / ana_gain_lut_b = ME / ana_gain_lut_c = SE */
			ana_gain_in_lut[i] = gains[i];
		} else {
			DRV_LOGE(ctx, "pls assign exposure_order_in_lbmf value!\n");
			return;
		}
	}
	/* restore gain */
	memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
	for (i = 0; i < exp_cnt; i++)
		ctx->ana_gain[i] = gains[i];
	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);

	/* write gain: update ana gain addr and set gain for lbmf */
	for (i = 0; i < exp_cnt; i++) {
		set_i2c_buffer(ctx,
			ctx->s_ctx.reg_addr_ana_gain_in_lut[i].addr[0],
			(ana_gain_in_lut[i] >> 8) & 0xFF);
		set_i2c_buffer(ctx,
			ctx->s_ctx.reg_addr_ana_gain_in_lut[i].addr[1],
			ana_gain_in_lut[i] & 0xFF);
	}
	DRV_LOG(ctx,
		"sid:%u,gain(input/lut):0x%x/%x/%x,%x/%x/%x\n",
		ctx->current_scenario_id,
		gains[0], gains[1], gains[2],
		ana_gain_in_lut[0], ana_gain_in_lut[1], ana_gain_in_lut[2]);
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 0);
	/*commit_i2c_buffer(ctx);*/
	/* group hold end */
}

/**
 * @brief: This api is used to set multi gain for DCG+VS scenario,
 * and it differs from general api. It assigns LUT params and calculate
 * accurate value for gain that could be set to sensor.
 * @param ctx: subdrv_ctx
 * @param gains: current gain from ae
 * @param exp_cnt: number depends on mode frame desc
 */
void set_dcg_vs_multi_gain_in_lut(struct subdrv_ctx *ctx, u32 *gains, u16 exp_cnt)
{
	int i = 0;
	u16 ana_gain_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	struct subdrv_mode_struct *mode_info = &ctx->s_ctx.mode[ctx->current_scenario_id];
	enum IMGSENSOR_DCG_GAIN_MODE dcg_gain_mode = mode_info->dcg_info.dcg_gain_mode;

	if (exp_cnt > ARRAY_SIZE(ctx->ana_gain)) {
		DRV_LOGE(ctx,
			"invalid exp_cnt:%u>%lu\n",
			exp_cnt, ARRAY_SIZE(ctx->ana_gain));
		exp_cnt = ARRAY_SIZE(ctx->ana_gain);
	}
	for (i = 0; i < exp_cnt; i++) {
		DRV_LOG_MUST(ctx, "input gain[%d] = %u\n", i, gains[i]);

		/* check boundary of gain */
		gains[i] = max(gains[i],
			ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].min);
		gains[i] = min(gains[i],
			ctx->s_ctx.mode[ctx->current_scenario_id].multi_exposure_ana_gain_range[i].max);
		/* mapping of gain to register value */
		if (ctx->s_ctx.g_gain2reg != NULL)
			gains[i] = ctx->s_ctx.g_gain2reg(gains[i]);
		else
			gains[i] = gain2reg(gains[i]);
	}
	for (i = 0; i < exp_cnt; i++) {
		/* update ana_gain_in_lut */
		/* 3exp: ana_gain_lut_a = DCG(HSG) / ana_gain_lut_b = DCG(LSG) / ana_gain_lut_c = VS */
		ana_gain_in_lut[i] = gains[i];
	}
	/* restore gain */
	memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
	for (i = 0; i < exp_cnt; i++)
		ctx->ana_gain[i] = gains[i];
	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);

	/* check dcg ratio/direct mode */
	/* write gain: update ana gain addr and set gain for dcg */
	switch (dcg_gain_mode) {
	case IMGSENSOR_DCG_DIRECT_MODE:
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_ana_gain[0].addr[0],
			(ana_gain_in_lut[0] >> 8) & 0xFF);
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_ana_gain[0].addr[1],
			ana_gain_in_lut[0] & 0xFF);
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_ana_gain[1].addr[0],
			(ana_gain_in_lut[1] >> 8) & 0xFF);
		set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_ana_gain[1].addr[1],
			ana_gain_in_lut[1] & 0xFF);
		break;
	case IMGSENSOR_DCG_RATIO_MODE:
		set_i2c_buffer(ctx,
			ctx->s_ctx.reg_addr_ana_gain_in_lut[0].addr[0],
			(ana_gain_in_lut[0] >> 8) & 0xFF);
		set_i2c_buffer(ctx,
			ctx->s_ctx.reg_addr_ana_gain_in_lut[0].addr[1],
			ana_gain_in_lut[0] & 0xFF);
		break;
	}

	/* write gain: update ana gain addr and set gain for vs */
	set_i2c_buffer(ctx,
		ctx->s_ctx.reg_addr_ana_gain_in_lut[1].addr[0],
		(ana_gain_in_lut[2] >> 8) & 0xFF);
	set_i2c_buffer(ctx,
		ctx->s_ctx.reg_addr_ana_gain_in_lut[1].addr[1],
		ana_gain_in_lut[2] & 0xFF);

	DRV_LOG(ctx,
		"sid:%u,dcg_gain_mode(%d)gain(input/lut):0x%x/%x/%x,%x/%x/%x\n",
		ctx->current_scenario_id,
		dcg_gain_mode,
		gains[0], gains[1], gains[2],
		ana_gain_in_lut[0], ana_gain_in_lut[1], ana_gain_in_lut[2]);
	if (gph)
		ctx->s_ctx.s_gph((void *)ctx, 0);
	/*commit_i2c_buffer(ctx);*/
	/* group hold end */
}

static u16 dgain2reg(struct subdrv_ctx *ctx, u32 dgain)
{
	u32 step = max((ctx->s_ctx.dig_gain_step), (u32)1);
	u8 integ = (u8) (dgain / BASE_DGAIN); // integer parts
	u8 dec = (u8) ((dgain % BASE_DGAIN) / step); // decimal parts
	u16 ret = ((u16)integ << 8) | dec;

	DRV_LOG(ctx, "dgain reg = 0x%x\n", ret);

	return ret;
}

void set_multi_dig_gain(struct subdrv_ctx *ctx, u32 *gains, u16 exp_cnt)
{
	int i = 0;
	u16 rg_gains[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF) {
		set_multi_dig_gain_in_lut(ctx, gains, exp_cnt);
		return;
	}
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_RAW_VS ||
	    ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE_VS) {
		// DONE: DCG-VS need to separate from LBMF for dig gain
		set_dcg_vs_multi_dig_gain_in_lut(ctx, gains, exp_cnt);
		return;
	}
	// skip if no porting digital gain
	if (!ctx->s_ctx.reg_addr_dig_gain[0].addr[0])
		return;

	if (exp_cnt > ARRAY_SIZE(ctx->dig_gain)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->dig_gain));
		exp_cnt = ARRAY_SIZE(ctx->dig_gain);
	}
	for (i = 0; i < exp_cnt; i++) {
		/* check boundary of gain */
		gains[i] = max(gains[i], ctx->s_ctx.dig_gain_min);
		gains[i] = min(gains[i], ctx->s_ctx.dig_gain_max);
		gains[i] = dgain2reg(ctx, gains[i]);
	}

	/* restore gain */
	memset(ctx->dig_gain, 0, sizeof(ctx->dig_gain));
	for (i = 0; i < exp_cnt; i++)
		ctx->dig_gain[i] = gains[i];

	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);

	/* write gain */
	switch (exp_cnt) {
	case 1:
		rg_gains[0] = gains[0];
		break;
	case 2:
		rg_gains[0] = gains[0];
		rg_gains[2] = gains[1];
		break;
	case 3:
		rg_gains[0] = gains[0];
		rg_gains[1] = gains[1];
		rg_gains[2] = gains[2];
		break;
	default:
		break;
	}
	for (i = 0;
	     (i < ARRAY_SIZE(rg_gains)) && (i < ARRAY_SIZE(ctx->s_ctx.reg_addr_dig_gain));
	     i++) {
		if (!rg_gains[i])
			continue; // skip zero gain setting

		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[0]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[0],
				(rg_gains[i] >> 8) & 0x0F);
		}
		if (ctx->s_ctx.reg_addr_dig_gain[i].addr[1]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[i].addr[1],
				rg_gains[i] & 0xFF);
		}
	}

	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}

	DRV_LOG(ctx, "dgain reg[lg/mg/sg]: 0x%x 0x%x 0x%x\n",
		rg_gains[0], rg_gains[1], rg_gains[2]);
}
/**
 * @brief: This api is used to set multi digital gain for lbmf scenario,
 * and it differs from general api. It assigns LUT params and calculate
 * accurate value for gain that could be set to sensor.
 * @param ctx: subdrv_ctx
 * @param gains: current digital gain from ae
 * @param exp_cnt: number depends on mode frame desc
 */
void set_multi_dig_gain_in_lut(struct subdrv_ctx *ctx, u32 *gains, u16 exp_cnt)
{
	int i = 0;
	u16 dig_gain_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	// skip if no porting digital gain
	if (!ctx->s_ctx.reg_addr_dig_gain_in_lut[0].addr[0])
		return;

	if (exp_cnt > ARRAY_SIZE(ctx->dig_gain)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->dig_gain));
		exp_cnt = ARRAY_SIZE(ctx->dig_gain);
	}
	for (i = 0; i < exp_cnt; i++) {
		/* check boundary of gain */
		gains[i] = max(gains[i], ctx->s_ctx.dig_gain_min);
		gains[i] = min(gains[i], ctx->s_ctx.dig_gain_max);
		gains[i] = dgain2reg(ctx, gains[i]);
	}
	for (i = 0; i < exp_cnt; i++) {
		/* update dig_gain_in_lut depends on exposure_order_in_lbmf */
		if (ctx->s_ctx.mode[ctx->current_scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
			/* 2exp: dig_gain_lut_a = SE / dig_gain_lut_b = LE */
			/* 3exp: dig_gain_lut_a = SE / dig_gain_lut_b = ME / dig_gain_lut_c = LE */
			dig_gain_in_lut[i] = gains[exp_cnt - 1 - i];
		} else if (ctx->s_ctx.mode[ctx->current_scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_LE_FIRST) {
			/* 2exp: dig_gain_lut_a = LE / dig_gain_lut_b = SE */
			/* 3exp: dig_gain_lut_a = LE / dig_gain_lut_b = ME / dig_gain_lut_c = SE */
			dig_gain_in_lut[i] = gains[i];
		} else {
			DRV_LOGE(ctx, "pls assign exposure_order_in_lbmf value!\n");
			return;
		}
	}
	/* restore gain */
	memset(ctx->dig_gain, 0, sizeof(ctx->dig_gain));
	for (i = 0; i < exp_cnt; i++)
		ctx->dig_gain[i] = gains[i];

	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);

	/* write gain: update dig gain addr and set gain for lbmf */
	for (i = 0; i < ARRAY_SIZE(dig_gain_in_lut) &&
		i < ARRAY_SIZE(ctx->s_ctx.reg_addr_dig_gain_in_lut); i++) {
		if (!dig_gain_in_lut[i])
			continue; // skip zero gain setting

		if (ctx->s_ctx.reg_addr_dig_gain_in_lut[i].addr[0]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain_in_lut[i].addr[0],
				(dig_gain_in_lut[i] >> 8) & 0x0F);
		}
		if (ctx->s_ctx.reg_addr_dig_gain_in_lut[i].addr[1]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain_in_lut[i].addr[1],
				dig_gain_in_lut[i] & 0xFF);
		}
	}

	DRV_LOG(ctx,
		"sid:%u,dgain(ori/lut):0x%x/%x/%x,%x/%x/%x\n",
		ctx->current_scenario_id,
		gains[0], gains[1], gains[2],
		dig_gain_in_lut[0], dig_gain_in_lut[1], dig_gain_in_lut[2]);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */
}

/**
 * @brief: This api is used to set multi digital gain for DCG+VS scenario,
 * and it differs from general api. It assigns LUT params and calculate
 * accurate value for gain that could be set to sensor.
 * @param ctx: subdrv_ctx
 * @param gains: current digital gain from ae
 * @param exp_cnt: number depends on mode frame desc
 */
void set_dcg_vs_multi_dig_gain_in_lut(struct subdrv_ctx *ctx, u32 *gains, u16 exp_cnt)
{
	int i = 0;
	u16 dig_gain_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};
	bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);
	struct subdrv_mode_struct *mode_info = &ctx->s_ctx.mode[ctx->current_scenario_id];
	enum IMGSENSOR_DCG_GAIN_MODE dcg_gain_mode = mode_info->dcg_info.dcg_gain_mode;

	// skip if no porting digital gain
	if (!ctx->s_ctx.reg_addr_dig_gain_in_lut[0].addr[0])
		return;

	if (exp_cnt > ARRAY_SIZE(ctx->dig_gain)) {
		DRV_LOGE(ctx, "invalid exp_cnt:%u>%lu\n", exp_cnt, ARRAY_SIZE(ctx->dig_gain));
		exp_cnt = ARRAY_SIZE(ctx->dig_gain);
	}
	for (i = 0; i < exp_cnt; i++) {
		/* check boundary of gain */
		gains[i] = max(gains[i], ctx->s_ctx.dig_gain_min);
		gains[i] = min(gains[i], ctx->s_ctx.dig_gain_max);
		gains[i] = dgain2reg(ctx, gains[i]);

		/* update dig_gain_in_lut */
		/* 3exp: dig_gain_lut_a = DCG(HSG) / dig_gain_lut_b = DCG(LSG) / dig_gain_lut_c = VS */
		dig_gain_in_lut[i] = gains[i];
	}
	/* restore gain */
	memset(ctx->dig_gain, 0, sizeof(ctx->dig_gain));
	for (i = 0; i < exp_cnt; i++)
		ctx->dig_gain[i] = gains[i];

	/* group hold start */
	if (gph && !ctx->ae_ctrl_gph_en)
		ctx->s_ctx.s_gph((void *)ctx, 1);

	/* check dcg ratio/direct mode */
	/* write gain: update dig gain addr and set gain for DCG */
	switch (dcg_gain_mode) {
	case IMGSENSOR_DCG_DIRECT_MODE:
		if (ctx->s_ctx.reg_addr_dig_gain[0].addr[0]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[0].addr[0],
				(dig_gain_in_lut[0] >> 8) & 0x0F);
		}
		if (ctx->s_ctx.reg_addr_dig_gain[0].addr[1]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[0].addr[1],
				dig_gain_in_lut[0] & 0xFF);
		}
		if (ctx->s_ctx.reg_addr_dig_gain[1].addr[0]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[1].addr[0],
				(dig_gain_in_lut[1] >> 8) & 0x0F);
		}
		if (ctx->s_ctx.reg_addr_dig_gain[1].addr[1]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain[1].addr[1],
				dig_gain_in_lut[1] & 0xFF);
		}
		break;
	case IMGSENSOR_DCG_RATIO_MODE:
		if (ctx->s_ctx.reg_addr_dig_gain_in_lut[0].addr[0]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain_in_lut[0].addr[0],
				(dig_gain_in_lut[0] >> 8) & 0x0F);
		}
		if (ctx->s_ctx.reg_addr_dig_gain_in_lut[0].addr[1]) {
			set_i2c_buffer(ctx,
				ctx->s_ctx.reg_addr_dig_gain_in_lut[0].addr[1],
				dig_gain_in_lut[0] & 0xFF);
		}
		break;
	}

	/* write gain: update dig gain addr and set gain for VS */
	if (ctx->s_ctx.reg_addr_dig_gain_in_lut[1].addr[0]) {
		set_i2c_buffer(ctx,
			ctx->s_ctx.reg_addr_dig_gain_in_lut[1].addr[0],
			(dig_gain_in_lut[2] >> 8) & 0x0F);
	}
	if (ctx->s_ctx.reg_addr_dig_gain_in_lut[1].addr[1]) {
		set_i2c_buffer(ctx,
			ctx->s_ctx.reg_addr_dig_gain_in_lut[1].addr[1],
			dig_gain_in_lut[2] & 0xFF);
	}

	DRV_LOG(ctx,
		"sid:%u,dgain(ori/lut):0x%x/%x/%x,%x/%x/%x\n",
		ctx->current_scenario_id,
		gains[0], gains[1], gains[2],
		dig_gain_in_lut[0], dig_gain_in_lut[1], dig_gain_in_lut[2]);
	if (!ctx->ae_ctrl_gph_en) {
		if (gph)
			ctx->s_ctx.s_gph((void *)ctx, 0);
		commit_i2c_buffer(ctx);
	}
	/* group hold end */
}

void get_lens_driver_id(struct subdrv_ctx *ctx, u32 *lens_id)
{
	*lens_id = LENS_DRIVER_ID_DO_NOT_CARE;
}

void check_stream_off(struct subdrv_ctx *ctx)
{
	u32 i = 0, framecnt = 0;
	u32 timeout = 0;

	ctx->current_fps = ctx->pclk / ctx->line_length * 10 / ctx->frame_length;
	timeout = ctx->current_fps ? (10000 / ctx->current_fps) + 1 : 101;

	if (!ctx->s_ctx.reg_addr_frame_count)
		return;
	for (i = 0; i < timeout; i++) {
		framecnt = subdrv_ixc_rd_u8(ctx, ctx->s_ctx.reg_addr_frame_count);
		if (framecnt == 0xFF)
			return;
		mdelay(1);
	}
	DRV_LOGE(ctx, "stream off fail!,cur_fps:%u,timeout:%u\n",
		ctx->current_fps, timeout);
}

void streaming_control(struct subdrv_ctx *ctx, bool enable)
{
	u64 stream_ctrl_delay_timing = 0;
	u64 stream_ctrl_delay = 0;
	struct adaptor_ctx *_adaptor_ctx = NULL;
	struct v4l2_subdev *sd = NULL;

	DRV_LOG(ctx, "E! enable:%u\n", enable);

	if (ctx->i2c_client)
		sd = i2c_get_clientdata(ctx->i2c_client);
	if (ctx->ixc_client.protocol)
		sd = adaptor_ixc_get_clientdata(&ctx->ixc_client);
	if (sd)
		_adaptor_ctx = to_ctx(sd);
	if (!_adaptor_ctx) {
		DRV_LOGE(ctx, "null _adaptor_ctx\n");
		return;
	}

	check_current_scenario_id_bound(ctx);
	if (ctx->s_ctx.aov_sensor_support && ctx->s_ctx.streaming_ctrl_imp) {
		if (ctx->s_ctx.s_streaming_control != NULL)
			ctx->s_ctx.s_streaming_control((void *) ctx, enable);
		else
			DRV_LOG_MUST(ctx,
				"please implement drive own streaming control!(sid:%u)\n",
				ctx->current_scenario_id);
		ctx->is_streaming = enable;
		DRV_LOG_MUST(ctx, "enable:%u\n", enable);
		return;
	}
	if (ctx->s_ctx.aov_sensor_support && ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode) {
		DRV_LOG_MUST(ctx,
			"stream ctrl implement on scp side!(sid:%u)\n",
			ctx->current_scenario_id);
		ctx->is_streaming = enable;
		DRV_LOG_MUST(ctx, "enable:%u\n", enable);
		return;
	}

	if (_adaptor_ctx->standby_mode_info == STANDBY_MODE_HW) {
		DRV_LOGE(ctx, "still in hw standby mode, can't stream control\n");
		WRAP_AEE_EXCEPTION("stream control in hw standby mode", "Err");
		return;
	}

	if (enable) {
		/* MCSS low power mode update para */
		if (ctx->s_ctx.mcss_update_subdrv_para != NULL)
			ctx->s_ctx.mcss_update_subdrv_para((void *) ctx, ctx->current_scenario_id);
		/* MCSS register init */
		if (ctx->s_ctx.mcss_init != NULL)
			ctx->s_ctx.mcss_init((void *) ctx);

		set_dummy(ctx);
		subdrv_ixc_wr_u8(ctx, ctx->s_ctx.reg_addr_stream, 0x01);
		ctx->stream_ctrl_start_time = ktime_get_boottime_ns();
		ctx->stream_ctrl_start_time_mono = ktime_get_ns();
	} else {
		ctx->stream_ctrl_end_time = ktime_get_boottime_ns();
		if (ctx->s_ctx.custom_stream_ctrl_delay &&
			ctx->stream_ctrl_start_time && ctx->stream_ctrl_end_time) {
			stream_ctrl_delay_timing =
				(ctx->stream_ctrl_end_time - ctx->stream_ctrl_start_time) / 1000000;
			stream_ctrl_delay = (u64)get_sof_timeout(_adaptor_ctx, _adaptor_ctx->cur_mode) / 1000;
			DRV_LOG_MUST(ctx,
				"stream_ctrl_delay(sof)/stream_ctrl_delay_timing(end-start):%llums/%llums\n",
				stream_ctrl_delay,
				stream_ctrl_delay_timing);
			if (ctx->sof_cnt <=1 && stream_ctrl_delay_timing < stream_ctrl_delay)
				mdelay(stream_ctrl_delay - stream_ctrl_delay_timing);
		}
		if (ctx->s_ctx.chk_streaming_st != NULL)
			ctx->s_ctx.chk_streaming_st((void *) ctx);

		subdrv_ixc_wr_u8(ctx, ctx->s_ctx.reg_addr_stream, 0x00);

		/* due to not all sensor support fast mode addr, but ctx must be clear */
		if (ctx->fast_mode_on) {
			DRV_LOG(ctx, "seamless_switch disabled.");
			ctx->fast_mode_on = FALSE;
			ctx->ref_sof_cnt = 0;

			if (ctx->s_ctx.reg_addr_fast_mode)
				set_i2c_buffer(ctx, ctx->s_ctx.reg_addr_fast_mode, 0x00);
			commit_i2c_buffer(ctx);
		}
		ctx->fast_mode_on = FALSE;
		ctx->ref_sof_cnt = 0;
		memset(ctx->exposure, 0, sizeof(ctx->exposure));
		memset(ctx->ana_gain, 0, sizeof(ctx->ana_gain));
		ctx->autoflicker_en = FALSE;
		ctx->extend_frame_length_en = 0;
		ctx->is_seamless = 0;
		if (ctx->s_ctx.chk_s_off_end)
			check_stream_off(ctx);
		ctx->stream_ctrl_start_time = 0;
		ctx->stream_ctrl_end_time = 0;
		ctx->stream_ctrl_start_time_mono = 0;

		ctx->mcss_init_info.enable_mcss = 0;
		if (ctx->s_ctx.mcss_init != NULL)
			ctx->s_ctx.mcss_init((void *) ctx); // disable MCSS

		if (ctx->s_ctx.mode[ctx->current_scenario_id].force_wr_mode_setting) {
			DRV_LOG(ctx,
				"force_wr_mode_setting:%u,is_sensor_scenario_inited:%u->0\n",
				ctx->s_ctx.mode[ctx->current_scenario_id].force_wr_mode_setting,
				_adaptor_ctx->is_sensor_scenario_inited);
			_adaptor_ctx->is_sensor_scenario_inited = 0;
		}
	}
	ctx->sof_no = 0;
	ctx->is_streaming = enable;
	DRV_LOG(ctx, "X! enable:%u\n", enable);
}

void set_video_mode(struct subdrv_ctx *ctx, u16 framerate)
{
	if (!framerate)
		return;
	set_max_framerate(ctx, framerate, 0);
	set_auto_flicker(ctx, 1);
	set_dummy(ctx);
	DRV_LOG(ctx, "fps(input/max):%u/%u\n", framerate, ctx->current_fps);
}

void set_auto_flicker_mode(struct subdrv_ctx *ctx, int enable, u16 framerate)
{
	(void) framerate;

	ctx->autoflicker_en = enable;

	DRV_LOG(ctx, "enable:%d\n", enable);
}

void get_output_format_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u64 *sensor_output_dataformat)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return;
	}
	*sensor_output_dataformat =
		(enum ACDK_SENSOR_OUTPUT_DATA_FORMAT_ENUM)
		ctx->s_ctx.mode[scenario_id].sensor_output_dataformat;
}

void get_ana_gain_table(struct subdrv_ctx *ctx, u64 *size, void *data)
{
	u32 *gain_table = ctx->s_ctx.ana_gain_table;

	if (data == NULL)
		*size =	ctx->s_ctx.ana_gain_table_size;
	else
		memcpy(data, (void *)gain_table, ctx->s_ctx.ana_gain_table_size);
}

void get_gain_range_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u64 *min_gain, u64 *max_gain)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	*min_gain = ctx->s_ctx.mode[scenario_id].multi_exposure_ana_gain_range[0].min;
	*max_gain = ctx->s_ctx.mode[scenario_id].multi_exposure_ana_gain_range[0].max;
}

void get_base_gain_iso_and_step(struct subdrv_ctx *ctx,
		u64 *min_gain_iso, u64 *gain_step, u64 *gain_type)
{
	*min_gain_iso = ctx->s_ctx.tuning_iso_base;
	*gain_step = ctx->s_ctx.ana_gain_step;
	*gain_type = ctx->s_ctx.ana_gain_type;
}

void get_dig_gain_range_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u64 *min_dgain, u64 *max_dgain)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	*min_dgain = ctx->s_ctx.mode[scenario_id].dig_gain_min;
	*max_dgain = ctx->s_ctx.mode[scenario_id].dig_gain_max;
}

void get_dig_gain_step(struct subdrv_ctx *ctx, u64 *dgain_step)
{
	*dgain_step = ctx->s_ctx.dig_gain_step;
}

void get_min_shutter_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		u64 *min_shutter, u64 *exposure_step)
{
	u64 ratio = 1;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid cur_sid:%u, mode_num:%u set default\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = 0;
	}

	*min_shutter = ctx->s_ctx.exposure_min;
	if (ctx->s_ctx.mode[scenario_id].coarse_integ_step) {
		*exposure_step = ctx->s_ctx.mode[scenario_id].coarse_integ_step;
		return;
	}
	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_STAGGER)
		ratio = ctx->s_ctx.mode[scenario_id].exp_cnt;
	*exposure_step = ctx->s_ctx.exposure_step * ratio;
}

void get_offset_to_start_of_exposure(struct subdrv_ctx *ctx, u32 *offset)
{
	if (is_imgsensor_fusion_test_workaround &&
		ctx->s_ctx.start_exposure_offset_custom)
		*offset = ctx->s_ctx.start_exposure_offset_custom;
	else
		*offset = ctx->s_ctx.start_exposure_offset;
}

void get_pixel_clock_freq_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u64 *pclk)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	*pclk = ctx->s_ctx.mode[scenario_id].pclk;
}

void get_period_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *period, u64 flag)
{
	u32 ratio = 1;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	if (flag & SENSOR_GET_LINELENGTH_FOR_READOUT &&
		ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_STAGGER)
		ratio = ctx->s_ctx.mode[scenario_id].exp_cnt;
	*period = (ctx->s_ctx.mode[scenario_id].framelength << 16)
			+ (ctx->s_ctx.mode[scenario_id].linelength * ratio);
}

void get_period(struct subdrv_ctx *ctx,
		u16 *line_length, u16 *frame_length)
{
	*line_length = ctx->line_length;
	*frame_length = ctx->frame_length;
}

void get_pixel_clock_freq(struct subdrv_ctx *ctx, u64 *pclk)
{
	*pclk = ctx->pclk;
}

void get_default_framerate_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *framerate)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return;
	}
	*framerate = ctx->s_ctx.mode[scenario_id].max_framerate;
}

void get_fine_integ_line_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, int *fine_integ_line)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return;
	}
	*fine_integ_line = ctx->s_ctx.mode[scenario_id].fine_integ_line;
}

void set_test_pattern(struct subdrv_ctx *ctx, u32 mode)
{
	DRV_LOGE(ctx, "sensor no support.");
}

void set_test_pattern_data(struct subdrv_ctx *ctx, struct mtk_test_pattern_data *data)
{
	DRV_LOGE(ctx, "sensor no support.");
}

void get_test_pattern_checksum_value(struct subdrv_ctx *ctx, u32 *checksum)
{
	*checksum = ctx->s_ctx.checksum_value;
}

void set_framerate(struct subdrv_ctx *ctx, u32 framerate)
{
	ctx->current_fps = framerate;
	DRV_LOG(ctx, "fps:%u\n", ctx->current_fps);
}

void set_hdr(struct subdrv_ctx *ctx, u32 mode)
{
	ctx->ihdr_mode = mode;
	DRV_LOG(ctx, "ihdr_mode:%u\n", ctx->ihdr_mode);
}

void get_crop_info(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		struct SENSOR_WINSIZE_INFO_STRUCT *wininfo)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	memcpy((void *)wininfo,
		(void *)&(ctx->s_ctx.mode[scenario_id].imgsensor_winsize_info),
		sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
	DRV_LOG(ctx, "sid:%u\n", scenario_id);
}

void get_pdaf_info(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		struct SET_PD_BLOCK_INFO_T *pd_info)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	if (ctx->s_ctx.mode[scenario_id].imgsensor_pd_info != NULL)
		memcpy((void *)pd_info,
			(void *)(ctx->s_ctx.mode[scenario_id].imgsensor_pd_info),
			sizeof(struct SET_PD_BLOCK_INFO_T));
	DRV_LOG(ctx, "sid:%u\n", scenario_id);
}

void get_sensor_pdaf_capacity(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *pdaf_cap)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		*pdaf_cap = 0;
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return;
	}
	*pdaf_cap = ctx->s_ctx.mode[scenario_id].pdaf_cap;
	DRV_LOG(ctx, "pdaf_cap:%u(sid:%u)\n", *pdaf_cap, scenario_id);
}

void extend_frame_length(struct subdrv_ctx *ctx, u32 ns)
{
	int i;
	u32 last_exp_cnt = 1;
	u32 old_fl = ctx->frame_length;
	u32 calc_fl = 0;
	u32 readoutLength = 0;
	u32 readMargin = 0;
	u32 per_frame_ns = 0;
	u32 linetime_in_ns = 0;

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			ctx->current_scenario_id, &linetime_in_ns, 0, 0);
		per_frame_ns = (u64)ctx->frame_length *
			linetime_in_ns;
	} else {
		per_frame_ns = (u64)ctx->frame_length *
			(u64)ctx->line_length * 1000000000 / ctx->pclk;
	}


	check_current_scenario_id_bound(ctx);
	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_RAW_VS ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE_VS)
		return;
	readoutLength = ctx->s_ctx.mode[ctx->current_scenario_id].readout_length;
	readMargin = ctx->s_ctx.mode[ctx->current_scenario_id].read_margin;

	for (i = 1; i < ARRAY_SIZE(ctx->exposure); i++)
		last_exp_cnt += ctx->exposure[i] ? 1 : 0;
	if (ns)
		ctx->frame_length = (u32)(((u64)(per_frame_ns + ns)) *
			ctx->frame_length / per_frame_ns);
	if (last_exp_cnt > 1) {
		calc_fl = (readoutLength + readMargin);
		for (i = 1; i < last_exp_cnt; i++)
			calc_fl += (ctx->exposure[i] +
				ctx->s_ctx.mode[ctx->current_scenario_id].exposure_margin *
				last_exp_cnt);
		ctx->frame_length = max(calc_fl, ctx->frame_length);
	}
	set_dummy(ctx);
	ctx->extend_frame_length_en = TRUE;

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			ctx->current_scenario_id, &linetime_in_ns, 0, 0);
		ns = (u64)(ctx->frame_length - old_fl) *
			linetime_in_ns;
	} else {
		ns = (u64)(ctx->frame_length - old_fl) *
			(u64)ctx->line_length * 1000000000 / ctx->pclk;
	}

	DRV_LOG(ctx, "fll(old/new):%u/%u, add %u ns", old_fl, ctx->frame_length, ns);
}

void seamless_switch(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u64 *ae_ctrl)
{
	DRV_LOGE(ctx, "please check get_seamless_scenarios or implement this in sensor driver.");
}

void get_seamless_scenarios(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *pScenarios)
{
	int i = 0;
	u32 num = 0;
	u32 group = 0;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		*pScenarios = 0xff;
		return;
	}
	group = ctx->s_ctx.mode[scenario_id].seamless_switch_group;
	for (i = 0; i < ctx->s_ctx.sensor_mode_num; i++) {
		if (group != 0 && i != scenario_id &&
		(ctx->s_ctx.mode[i].seamless_switch_group == group)) {
			*(pScenarios + num) = i;
			DRV_LOG(ctx, "sid(input/output):%u/%u\n", scenario_id, *(pScenarios + num));
			num++;
		}
	}
	if (num == 0)
		*pScenarios = 0xff;
}

void get_sensor_hdr_capacity(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *hdr_mode)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		*hdr_mode = HDR_NONE;
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return;
	}
	*hdr_mode = ctx->s_ctx.mode[scenario_id].hdr_mode;
	DRV_LOG(ctx, "hdr_mode:%u(sid:%u)\n", *hdr_mode, scenario_id);
}

void get_frame_ctrl_info_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *margin)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	*margin = ctx->s_ctx.mode[scenario_id].exposure_margin;
}

void get_feature_get_4cell_data(struct subdrv_ctx *ctx, u16 type, char *data, u16 size)
{
	u16 idx = 0;
	u8 support = FALSE;
	u8 *pbuf = NULL;
	u16 info_sz = 0;
	struct eeprom_info_struct *info = ctx->s_ctx.eeprom_info;

	if (!probe_eeprom(ctx))
		return;

	idx = ctx->eeprom_index;

	if (type == FOUR_CELL_CAL_TYPE_XTALK_CAL) {
		support = info[idx].xtalk_support;
		pbuf = info[idx].preload_xtalk_table;
		info_sz = info[idx].xtalk_size;
		if (size < 2 + info_sz) {
			DRV_LOGE(ctx, "XTALK data size not enough %u, expected %u", size, info_sz);
			return;
		}
		if (support) {
			data[0] = info_sz & 0xFF;
			data[1] = (info_sz >> 8) & 0xFF;
			if (pbuf != NULL && info_sz > 0) {
				memcpy(data + 2, pbuf, info_sz);
				DRV_LOG(ctx, "memcpy XTALK data done %u bytes", info_sz);
			}
		}
	}
}

void get_stagger_max_exp_time(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		enum VC_FEATURE vc, u64 *exposure_max)
{
	*exposure_max = 0;
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return;
	}
	/* LBMF sets the same vc_feature as Stagger */
	switch (vc) {
	case VC_STAGGER_ME:
		switch (ctx->s_ctx.mode[scenario_id].exp_cnt) {
		case 2:
		case 3:
			*exposure_max = ctx->s_ctx.exposure_max;
			break;
		default:
			break;
		}
		break;
	case VC_STAGGER_SE:
		switch (ctx->s_ctx.mode[scenario_id].exp_cnt) {
		case 3:
			*exposure_max = ctx->s_ctx.exposure_max;
			break;
		default:
			break;
		}
		break;
	case VC_STAGGER_NE:
	default:
		*exposure_max = ctx->s_ctx.exposure_max;
		break;
	}
}

void get_temperature_value(struct subdrv_ctx *ctx, u32 *value)
{
	if (!ctx->s_ctx.temperature_support) {
		DRV_LOGE(ctx, "temperature sensor no support\n");
		return;
	}
	if (ctx->s_ctx.g_temp == NULL) {
		DRV_LOGE(ctx, "please implement g_temp function\n");
		return;
	}
	*value = ctx->s_ctx.g_temp((void *)ctx);
	DRV_LOG(ctx, "temperature value:%u\n", *value);
}

void set_pdaf(struct subdrv_ctx *ctx, u16 mode)
{
	ctx->pdaf_mode = mode;
	DRV_LOG(ctx, "pdaf_mode:%u\n", ctx->pdaf_mode);
}

void get_binning_type(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *binning_ratio)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	*binning_ratio = ctx->s_ctx.mode[scenario_id].ae_binning_ratio;
}

void get_ae_frame_mode_for_le(struct subdrv_ctx *ctx, u32 *ae_frm_mode)
{
	memcpy(ae_frm_mode,	&ctx->ae_frm_mode, sizeof(struct IMGSENSOR_AE_FRM_MODE));
}

void get_ae_effective_frame_for_le(struct subdrv_ctx *ctx, u32 *ae_effective_frame)
{
	*ae_effective_frame = ctx->current_ae_effective_frame;
}

void preload_eeprom_data(struct subdrv_ctx *ctx, u32 *is_read)
{
	*is_read = ctx->is_read_preload_eeprom;
	if (!ctx->is_read_preload_eeprom) {
		DRV_LOG(ctx, "start to preload\n");
		if (ctx->s_ctx.g_cali != NULL)
			ctx->s_ctx.g_cali((void *) ctx);
		else
			read_sensor_Cali(ctx);
	}
	DRV_LOG(ctx, "already preloaded\n");
}

void get_mipi_pixel_rate(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *mipi_pixel_rate)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	*mipi_pixel_rate = ctx->s_ctx.mode[scenario_id].mipi_pixel_rate;
}

void get_sensor_rgbw_output_mode(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *rgbw_output_mode)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		*rgbw_output_mode = 0;
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return;
	}
	*rgbw_output_mode = ctx->s_ctx.mode[scenario_id].rgbw_output_mode;
	DRV_LOG(ctx, "rgbw_output_mode:%u(sid:%u)\n", *rgbw_output_mode, scenario_id);
}

void get_readout_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u64 *readout_time)
{
	u32 ratio = 1;
	u32 linetime_in_ns = 0;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return;
	}
	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_STAGGER)
		ratio = ctx->s_ctx.mode[scenario_id].exp_cnt;

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			scenario_id, &linetime_in_ns, 0, 0);
		*readout_time =
			(u64)linetime_in_ns
			* ctx->s_ctx.mode[scenario_id].imgsensor_winsize_info.h2_tg_size * ratio;
	} else {
		*readout_time =
			(u64)ctx->s_ctx.mode[scenario_id].linelength
			* ctx->s_ctx.mode[scenario_id].imgsensor_winsize_info.h2_tg_size
			* 1000000000 / ctx->s_ctx.mode[scenario_id].pclk * ratio;
	}
}

void get_exposure_count_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u32 *scenario_exp_cnt)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return;
	}
	*scenario_exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt;
}

void get_dcg_gain_ratio_table_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u64 *size, void *data)
{
	u32 *gain_ratio_table = NULL;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		*size = 0;
		return;
	}
	gain_ratio_table = ctx->s_ctx.mode[scenario_id].dcg_info.dcg_gain_table;

	if (data == NULL)
		*size = ctx->s_ctx.mode[scenario_id].dcg_info.dcg_gain_table_size;
	else
		memcpy(data, (void *)gain_ratio_table,
			ctx->s_ctx.mode[scenario_id].dcg_info.dcg_gain_table_size);
}

void get_dcg_gain_ratio_range_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u64 *min_gain_ratio, u64 *max_gain_ratio)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	*min_gain_ratio = ctx->s_ctx.mode[scenario_id].dcg_info.dcg_gain_ratio_min;
	*max_gain_ratio = ctx->s_ctx.mode[scenario_id].dcg_info.dcg_gain_ratio_max;
}

void get_dcg_type_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		u64 *dcg_mode, u64 *dcg_gain_mode)
{
	enum IMGSENSOR_HDR_MODE_ENUM hdr_mode = 0;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	hdr_mode = ctx->s_ctx.mode[scenario_id].hdr_mode;

	if (hdr_mode != HDR_RAW_DCG_RAW && hdr_mode != HDR_RAW_DCG_COMPOSE)
		DRV_LOG(ctx, "This mode doesn't support DCG:%u, hdr_mode:%u\n",
			scenario_id, hdr_mode);
	*dcg_mode = ctx->s_ctx.mode[scenario_id].dcg_info.dcg_mode;
	*dcg_gain_mode = ctx->s_ctx.mode[scenario_id].dcg_info.dcg_gain_mode;
}

void get_multi_exp_gain_range_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		u64 *exp_cnt, void *data)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	*exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt
				? ctx->s_ctx.mode[scenario_id].exp_cnt : 1;
	memcpy(data,
		(void *)ctx->s_ctx.mode[scenario_id].multi_exposure_ana_gain_range,
		sizeof(struct u32_min_max)*IMGSENSOR_EXPOSURE_CNT);
}

void get_multi_exp_shutter_range_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		u64 *exp_cnt, void *data)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	*exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt
				? ctx->s_ctx.mode[scenario_id].exp_cnt : 1;
	memcpy(data,
		(void *)ctx->s_ctx.mode[scenario_id].multi_exposure_shutter_range,
		sizeof(struct u64_min_max)*IMGSENSOR_EXPOSURE_CNT);
}

void get_sensor_frame_count(struct subdrv_ctx *ctx, u32 *frame_cnt)
{
	u32 framecnt = 0;

	if (!ctx->s_ctx.reg_addr_frame_count)
		return;
	framecnt = subdrv_ixc_rd_u8(ctx, ctx->s_ctx.reg_addr_frame_count);
	*frame_cnt = framecnt;
}

void get_exp_line_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		u32 framerate, u64 *exposure_line)
{
	u64 shutter = 0;
	u32 linetime_in_ns = 0;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}

	if (framerate > ctx->s_ctx.mode[scenario_id].max_framerate)
		framerate = ctx->s_ctx.mode[scenario_id].max_framerate;

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			scenario_id, &linetime_in_ns, 0, 0);
		shutter = (u64)1000000000 / framerate * 10
			/ linetime_in_ns;
	} else {
		shutter = ctx->s_ctx.mode[scenario_id].pclk / framerate * 10
			/ ctx->s_ctx.mode[scenario_id].linelength;
	}

	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_STAGGER) {
		shutter = shutter -(ctx->s_ctx.mode[scenario_id].readout_length +
			ctx->s_ctx.mode[scenario_id].read_margin);
	}
	shutter = max_t(u64, shutter,
			(u64)ctx->s_ctx.mode[scenario_id].multi_exposure_shutter_range[0].min);
	shutter = min_t(u64, shutter,
			(u64)ctx->s_ctx.mode[scenario_id].multi_exposure_shutter_range[0].max);
	*exposure_line = shutter;
}

void get_multi_exp_static_info_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		void *data)
{
	struct mtk_multi_exp_static_info *info;
	struct mtk_ae_info *ae_info;
	struct struct_dcg_vsl_info dcg_vsl_info = {0};
	u8 lut_id;
	int i;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}

	info = (struct mtk_multi_exp_static_info *) data;

	info->exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt
				? ctx->s_ctx.mode[scenario_id].exp_cnt : 1;

	DRV_LOG(ctx, "scenario_id:%u,exp_cnt:%u\n", scenario_id, info->exp_cnt);

	get_dcg_vsl_info_by_scenario(ctx, scenario_id, &dcg_vsl_info);

	for (i = 0; (i < ARRAY_SIZE(info->ae_info)) && (i < info->exp_cnt); i++) {
		ae_info = info->ae_info + i;

		ae_info->gain.min = dcg_vsl_info.exp_info[i].ana_gain_min;
		ae_info->gain.max = dcg_vsl_info.exp_info[i].ana_gain_max;
		ae_info->gain.step = dcg_vsl_info.exp_info[i].ana_gain_step;
		ae_info->shutter.min = dcg_vsl_info.exp_info[i].shutter_min;
		ae_info->shutter.max = dcg_vsl_info.exp_info[i].shutter_max;
		ae_info->shutter.step = dcg_vsl_info.exp_info[i].coarse_integ_step;

		if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
			ctx->s_ctx.cust_get_linetime_in_ns(ctx, scenario_id,
							   (u32 *)&ae_info->line_time_in_ns,
							   GET_SHUTTER_LINETIME, i);
		} else {
			lut_id = dcg_vsl_info.exp_info[i].lut_idx;
			ae_info->line_time_in_ns = dcg_vsl_info.lut_info[lut_id].linetime_in_ns;
		}

		ae_info->fine_integ_line = dcg_vsl_info.exp_info[i].fine_integ_line;
		ae_info->margin = dcg_vsl_info.exp_info[i].exposure_margin;
		ae_info->binning_ratio = dcg_vsl_info.exp_info[i].ae_binning_ratio;

		DRV_LOG(ctx,
			"scenario_id(%d),exp_cnt(%u)=> ae_static_info[%d]:gain_min/max/stp(%u/%u/%u),shut_min/max_stp(%llu/%llu/%u),ltInNs(%llu),fineIntegL(%d),margin(%u),binRatio(%u)\n",
			scenario_id, info->exp_cnt, i,
			ae_info->gain.min, ae_info->gain.max, ae_info->gain.step,
			ae_info->shutter.min, ae_info->shutter.max, ae_info->shutter.step,
			ae_info->line_time_in_ns,
			ae_info->fine_integ_line,
			ae_info->margin,
			ae_info->binning_ratio);
	}

}

void get_dcg_vsl_info_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		void *data)
{
	struct struct_dcg_vsl_info *info;
	struct struct_exp_info *exp_info;
	struct struct_lut_info *lut_info;
	int i, j;
	u8 max_lut_id = IMGSENSOR_LUT_A;
	u64 lut_pclk;
	u32 lut_linelength;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}

	info = (struct struct_dcg_vsl_info *) data;

	/* exp info */
	info->exp_cnt = ctx->s_ctx.mode[scenario_id].exp_cnt
				? ctx->s_ctx.mode[scenario_id].exp_cnt : 1;

	for (i = 0; (i < ARRAY_SIZE(info->exp_info)) && (i < info->exp_cnt); i++) {
		exp_info = info->exp_info + i;

		exp_info->lut_idx = get_multiexp_belong_lut(ctx, scenario_id, i);
		exp_info->coarse_integ_step = get_multiexp_static_info(ctx, u32, coarse_integ_step, scenario_id, i);
		exp_info->exposure_margin = get_multiexp_static_info(ctx, u32, exposure_margin, scenario_id, i);
		exp_info->ae_binning_ratio = get_multiexp_static_info(ctx, u32, ae_binning_ratio, scenario_id, i);
		exp_info->fine_integ_line = get_multiexp_static_info(ctx, int, fine_integ_line, scenario_id, i);
		exp_info->dig_gain_min = get_multiexp_static_info(ctx, u32, dig_gain_min, scenario_id, i);
		exp_info->dig_gain_max = get_multiexp_static_info(ctx, u32, dig_gain_max, scenario_id, i);
		exp_info->dig_gain_step = get_multiexp_static_info(ctx, u32, dig_gain_step, scenario_id, i);
		exp_info->ana_gain_min = (u32)ctx->s_ctx.mode[scenario_id].multi_exposure_ana_gain_range[i].min;
		exp_info->ana_gain_max = (u32)ctx->s_ctx.mode[scenario_id].multi_exposure_ana_gain_range[i].max;
		exp_info->ana_gain_step = ctx->s_ctx.ana_gain_step;
		exp_info->shutter_min = (u64)ctx->s_ctx.mode[scenario_id].multi_exposure_shutter_range[i].min;
		exp_info->shutter_max = (u64)ctx->s_ctx.mode[scenario_id].multi_exposure_shutter_range[i].max;

		if (exp_info->lut_idx > max_lut_id)
			max_lut_id = exp_info->lut_idx;

		DRV_LOG(ctx,
			"scenario_id(%d),exp_cnt(%u)=> exp_info[%d]:lut(%u),gain_min/max/stp(%u/%u/%u),dgain_min/max/stp(%u/%u/%u),shut_min/max_stp(%llu/%llu/%u),exp_margin(%u),fil(%i),binRatio(%u)\n",
			scenario_id, info->exp_cnt, i,
			exp_info->lut_idx,
			exp_info->ana_gain_min, exp_info->ana_gain_max, exp_info->ana_gain_step,
			exp_info->dig_gain_min, exp_info->dig_gain_max, exp_info->dig_gain_step,
			exp_info->shutter_min, exp_info->shutter_max, exp_info->coarse_integ_step,
			exp_info->exposure_margin, exp_info->fine_integ_line, exp_info->ae_binning_ratio);
	}

	/* lut info */
	if (max_lut_id >= IMGSENSOR_LUT_MAXCNT) {
		DRV_LOG_MUST(ctx, "Invalid lut count: %u\n", max_lut_id + 1);
		info->lut_cnt = 0;
		return;
	}
	info->lut_cnt = max_lut_id + 1;

	for (i = 0; (i < ARRAY_SIZE(info->lut_info)) && (i < info->lut_cnt); i++) {
		lut_info = info->lut_info + i;

		lut_info->readout_length = get_lut_static_info(ctx, u32, readout_length, scenario_id, i);
		lut_info->read_margin = get_lut_static_info(ctx, u32, read_margin, scenario_id, i);
		lut_info->framelength_step = get_lut_static_info(ctx, u32, framelength_step, scenario_id, i);
		lut_info->min_vblanking_line = get_lut_static_info(ctx, u32, min_vblanking_line, scenario_id, i);
		lut_info->cit_loss = ctx->s_ctx.mode[scenario_id].mode_lut_s_info[i].cit_loss;

		lut_pclk = get_lut_static_info(ctx, u64, pclk, scenario_id, i);
		lut_linelength = get_lut_static_info(ctx, u32, linelength, scenario_id, i);
		if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
			for (j = 0; (j < ARRAY_SIZE(info->exp_info)) && (j < info->exp_cnt); j++) {
				if (get_multiexp_belong_lut(ctx, scenario_id, j) == i)
					break;
			}
			ctx->s_ctx.cust_get_linetime_in_ns(ctx, scenario_id,
							   (u32 *)&lut_info->linetime_in_ns,
							   GET_READOUT_LINETIME, j);
		} else {
			/* traidional */
			lut_info->linetime_in_ns = (u32)CALC_LINE_TIME_IN_NS(lut_pclk, lut_linelength);
		}

		DRV_LOG(ctx,
			"scenario_id(%d),lut_cnt(%u)=> lut_info[%d]:rout_len/r_margin/frm_stp/min_vb_lc/cit_loss(%u/%u/%u/%u/%u),pclk/llc(%llu/%u),line_time_ns(%u)\n",
			scenario_id, info->lut_cnt, i,
			lut_info->readout_length, lut_info->read_margin,
			lut_info->framelength_step, lut_info->min_vblanking_line,
			lut_info->cit_loss,
			lut_pclk, lut_linelength,
			lut_info->linetime_in_ns);
	}
}

void update_hw_init_time(struct subdrv_ctx *ctx, u64 fisrt_vsync_time)
{	u32 cur_id, shutter_lines, times;
	u64 line_time_ns, shutter_time, cur_init_time, old_init_time, new_init_time;
	u32 linetime_in_ns = 0;

	cur_id = ctx->current_scenario_id;
	times = ctx->hw_time_info[cur_id].times;
	new_init_time = 0;
	old_init_time = ctx->hw_time_info[cur_id].init_time_ns;

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			cur_id, &linetime_in_ns, 0, 0);
		line_time_ns = linetime_in_ns;
	} else {
		line_time_ns = ((u64)ctx->s_ctx.mode[cur_id].linelength*1000000000)
						/ ctx->s_ctx.mode[cur_id].pclk;
	}

	if ((ctx->s_ctx.mode[cur_id].hdr_mode == HDR_RAW_LBMF) &&
		(ctx->s_ctx.mode[cur_id].exposure_order_in_lbmf
				== IMGSENSOR_LBMF_EXPOSURE_SE_FIRST)) {
		shutter_lines = ctx->exposure[IMGSENSOR_STAGGER_EXPOSURE_ME];
	} else {
		shutter_lines = ctx->exposure[IMGSENSOR_STAGGER_EXPOSURE_LE];
	}
	shutter_time = shutter_lines*line_time_ns;
	cur_init_time = fisrt_vsync_time
					- ctx->stream_ctrl_start_time_mono
					- shutter_time;
	if (cur_init_time < HW_INIT_TIME_MAX) {
		new_init_time =
			((old_init_time*times)+cur_init_time) / (times+1);
		ctx->hw_time_info[cur_id].init_time_ns = new_init_time;
		ctx->hw_time_info[cur_id].times = (times > MAX_UPDATED_TIMES)
				? MAX_UPDATED_TIMES : times+1;
	}
	DRV_LOG_MUST(ctx,
			"sid:%d, fisrt_vsync_time:%llu, stream_ctrl_start_time:%llu, stream_ctrl_start_time_mono:%llu, cur_init_time:%llu, new_init_time:%llu, old_init_time:%llu, times:%u, shutter_time:%llu, shutter_lines:%u, line_time_ns:%llu\n",
			cur_id,
			fisrt_vsync_time,
			ctx->stream_ctrl_start_time,
			ctx->stream_ctrl_start_time_mono,
			cur_init_time,
			new_init_time,
			old_init_time,
			times,
			shutter_time,
			shutter_lines,
			line_time_ns);
}

void get_dcg_ratio_group_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, void *data)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	memcpy(data,
		(void *)ctx->s_ctx.mode[scenario_id].dcg_info.dcg_ratio_group,
		sizeof(u32)*IMGSENSOR_EXPOSURE_CNT);
}

void get_stagger_min_vb_by_scenario(struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id, u64 *min_vblanking_line)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}
	*min_vblanking_line = ctx->s_ctx.mode[scenario_id].min_vblanking_line;
}

/**
 * Check the character is a space/tab/newline or not
 */
static inline int is_whitespace(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/**
 * Trim a string and get the start and end position
 */
static void trim_str(const char **start, const char **end)
{
	while (*start < *end && is_whitespace(**start))
		(*start)++;

	while (*end > *start && is_whitespace(*(*end - 1)))
		(*end)--;
}

/**
 * Get the value of key with allocated string
 * User must free memory after used
 *
 * @param str The string of key-value pairs
 * @param str_len The string length of {@code str}
 * @param key The key string
 */
char *get_string_with_key(const char *str, const size_t str_len, const char *key)
{
	const char *current_pos = str;
	const char *end_pos = str + str_len;
	size_t key_len;

	/* parameter check */
	if (!str || !str_len || !key)
		return NULL;

	key_len = strlen(key);

	/* loop to search key */
	while (current_pos < end_pos) {
		const char *key_start = current_pos;
		const char *sep_pos = strchr(current_pos, '=');
		const char *value_start;
		const char *value_end;
		const char *key_end;
		size_t trimmed_key_len;

		if (!sep_pos)
			break;

		value_start = sep_pos + 1;
		value_end = strchr(value_start, '\n');
		if (!value_end)
			value_end = end_pos;

		key_end = sep_pos;

		trim_str(&key_start, &key_end);
		trim_str(&value_start, &value_end);

		trimmed_key_len = key_end - key_start;

		if (trimmed_key_len == key_len && strncmp(key_start, key, key_len) == 0) {
			char *value;
			size_t value_len = value_end - value_start;

			value = kmalloc(value_len + 1, GFP_KERNEL);
			if (!value)
				return NULL;

			strncpy(value, value_start, value_len);
			value[value_len] = '\0';
			return value;
		}
		current_pos = value_end + 1;
	}

	return NULL;
}

static enum VC_FEATURE fd_desc_to_vc_feature(
		u16 fd_user)
{
	enum VC_FEATURE ret;

	switch (fd_user) {
	case VC_RAW_DATA://V4L2_MBUS_CSI2_USER_DEFINED_DATA_DESC_NONE:
		ret = VC_RAW_DATA;
		break;
	case VC_3HDR_Y:
		ret = VC_3HDR_Y;
		break;
	case VC_3HDR_AE:
		ret = VC_3HDR_AE;
		break;
	case VC_3HDR_FLICKER:
		ret = VC_3HDR_FLICKER;
		break;
	case VC_3HDR_EMBEDDED:
		ret = VC_3HDR_EMBEDDED;
		break;
	case VC_PDAF_STATS:
	//case V4L2_MBUS_CSI2_USER_DEFINED_DATA_DESC_PDAF_DIFF:
		ret = VC_PDAF_STATS;
		break;
	case VC_STAGGER_NE:
		ret = VC_STAGGER_NE;
		break;
	case VC_STAGGER_ME:
		ret = VC_STAGGER_ME;
		break;
	case VC_STAGGER_SE:
		ret = VC_STAGGER_SE;
		break;
	case VC_PDAF_STATS_PIX_1:
		ret = VC_PDAF_STATS_PIX_1;
		break;
	case VC_PDAF_STATS_PIX_2:
		ret = VC_PDAF_STATS_PIX_2;
		break;
	case VC_PDAF_STATS_ME_PIX_1:
		ret = VC_PDAF_STATS_ME_PIX_1;
		break;
	case VC_PDAF_STATS_ME_PIX_2:
		ret = VC_PDAF_STATS_ME_PIX_2;
		break;
	case VC_PDAF_STATS_SE_PIX_1:
		ret = VC_PDAF_STATS_SE_PIX_1;
		break;
	case VC_PDAF_STATS_SE_PIX_2:
		ret = VC_PDAF_STATS_SE_PIX_2;
		break;
	case VC_YUV_Y:
		ret = VC_YUV_Y;
		break;
	case VC_YUV_UV:
		ret = VC_YUV_UV;
		break;
	case VC_RAW_NE_W_DATA: /* eq to VC_RAW_W_DATA */
		ret = VC_RAW_NE_W_DATA;
		break;
	case VC_RAW_ME_W_DATA:
		ret = VC_RAW_ME_W_DATA;
		break;
	case VC_RAW_SE_W_DATA:
		ret = VC_RAW_SE_W_DATA;
		break;
	case VC_RAW_PROCESSED_DATA:
		ret = VC_RAW_PROCESSED_DATA;
		break;
	case VC_GENERAL_EMBEDDED:
		ret = VC_GENERAL_EMBEDDED;
		break;
	case VC_RAW_FLICKER_DATA:
		ret = VC_RAW_FLICKER_DATA;
		break;
	case VC_META_DATA_0:
		ret = VC_META_DATA_0;
		break;
	case VC_META_DATA_1:
		ret = VC_META_DATA_1;
		break;
	default:
		ret = VC_RAW_DATA;
		break;
	}

	return ret;
}

static void frame_desc_to_vcinfo2(
		struct subdrv_ctx *ctx,
		enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		struct mtk_mbus_frame_desc *fd,
		struct SENSOR_VC_INFO2_STRUCT *vcinfo2)
{
	int i;
	struct SINGLE_VC_INFO2 *vc;
	struct mtk_mbus_frame_desc_entry_csi2 *entry;
	struct mtk_sensor_saturation_info *p_saturation_info;
	struct mtk_sensor_saturation_info default_saturation_info;

	vcinfo2->VC_Num = fd->num_entries;
	vcinfo2->VC_PixelNum = 0x0a;
	vcinfo2->ModeSelect = 0x00;
	vcinfo2->EXPO_Ratio = 0x08;
	vcinfo2->ODValue = 0x40;
	vcinfo2->RG_STATSMODE = 0x00;

	default_saturation_info.OB_pedestal = 64;
	default_saturation_info.ob_bm = 64;
	default_saturation_info.adc_bit = 10;
	default_saturation_info.bit_depth = 10;
	default_saturation_info.valid_bit = 10;
	default_saturation_info.dummy_padding = NONE_PADDING;

	for (i = 0; i < fd->num_entries; i++) {
		vc = &vcinfo2->vc_info[i];
		entry = &fd->entry[i].bus.csi2;
		vc->VC_FEATURE = fd_desc_to_vc_feature(entry->user_data_desc);
		vc->VC_ID = entry->channel;
		vc->VC_DataType = entry->data_type;
		vc->VC_SIZEH_PIXEL = entry->hsize;
		vc->VC_SIZEV = entry->vsize;
		vc->DT_REMAP_TO_TYPE = entry->dt_remap_to_type;
		vc->fs_seq = entry->fs_seq;
		if (vc->VC_DataType == 0x2b ||
			vc->DT_REMAP_TO_TYPE == MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10)
			vc->VC_SIZEH_BYTE = vc->VC_SIZEH_PIXEL * 10 / 8;
		else if (vc->VC_DataType == 0x2c ||
			vc->DT_REMAP_TO_TYPE == MTK_MBUS_FRAME_DESC_REMAP_TO_RAW12)
			vc->VC_SIZEH_BYTE = vc->VC_SIZEH_PIXEL * 12 / 8;
		else if (vc->VC_DataType == 0x2d ||
			vc->DT_REMAP_TO_TYPE == MTK_MBUS_FRAME_DESC_REMAP_TO_RAW14)
			vc->VC_SIZEH_BYTE = vc->VC_SIZEH_PIXEL * 14 / 8;
		else
			vc->VC_SIZEH_BYTE = vc->VC_SIZEH_PIXEL;


		/* check which version of  saturation_info is ported ready */
		if (entry->saturation_info) {
			/* used  saturation_info in frame desc */
			p_saturation_info = entry->saturation_info;

		} else if (ctx->s_ctx.mode[scenario_id].saturation_info) {
			/* used  saturation_info in mode info  */
			p_saturation_info = ctx->s_ctx.mode[scenario_id].saturation_info;

		} else if (ctx->s_ctx.saturation_info) {
			/* used  saturation_info in golbal info  */
			p_saturation_info = ctx->s_ctx.saturation_info;

		} else {
			DRV_LOG(ctx, "no saturation info for this sensor, using default value\n");
			/* used default saturation_info  */
			p_saturation_info = &default_saturation_info;
		}

		vc->OB_pedestal = p_saturation_info->OB_pedestal;
		vc->OB_bm = p_saturation_info->ob_bm;
		vc->Adc_bit = p_saturation_info->adc_bit;
		vc->bit_depth = p_saturation_info->bit_depth;
		vc->valid_bit = p_saturation_info->valid_bit;
		vc->dummy_padding = p_saturation_info->dummy_padding;

		/* Backward compatibility for whose valid_bit is in frame desc */
		if (entry->valid_bit)
			vc->valid_bit = entry->valid_bit;

		/* if bit depth is not descripted in saturation info -->  using dt to distinguish */
		if (vc->bit_depth == 0) {
			switch (vc->VC_DataType) {
			case 0x2c:
				vc->bit_depth = 12;
				break;
			case 0x2d:
				vc->bit_depth = 14;
				break;
			case 0x2e:
				vc->bit_depth = 16;
				break;
			case 0x2f:
				vc->bit_depth = 20;
				break;
			case 0x27:
				vc->bit_depth = 24;
				break;
			case 0x2b:
			default:
				vc->bit_depth = 10;
				break;
			}
		}
		/* if bit depth is not descripted in saturation info -->  using bit depth info instead */
		if (vc->valid_bit == 0)
			vc->valid_bit = vc->bit_depth;

		if (vc->OB_pedestal == 0)
			vc->OB_pedestal = default_saturation_info.OB_pedestal;

		if (vc->OB_bm == 0)
			vc->OB_bm = default_saturation_info.ob_bm;

		if (vc->Adc_bit == 0)
			vc->Adc_bit = default_saturation_info.adc_bit;

		if (vc->dummy_padding == 0)
			vc->dummy_padding = default_saturation_info.dummy_padding;
	}
}

void get_frame_desc_to_vc_info(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id,
		struct SENSOR_VC_INFO2_STRUCT *vcinfo2)
{
	struct mtk_mbus_frame_desc fd;

	/* get frame description by scenario */
	if (common_get_frame_desc(ctx, scenario_id, &fd) < 0) {
		DRV_LOGE(ctx, "common_get_frame_desc return failed with scenario %u\n", scenario_id);
		return;
	}
	/* mapping frame desc to vcinfo struct */
	frame_desc_to_vcinfo2(ctx, scenario_id, &fd, vcinfo2);
}

int common_get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id)
{
	u8 i = 0;
	u8 retry = 2;
	u32 addr_h = ctx->s_ctx.reg_addr_sensor_id.addr[0];
	u32 addr_l = ctx->s_ctx.reg_addr_sensor_id.addr[1];
	u32 addr_ll = ctx->s_ctx.reg_addr_sensor_id.addr[2];

	while (ctx->s_ctx.i2c_addr_table[i] != 0xFF && ctx->s_ctx.i2c_addr_table[i] != 0) {
		ctx->i2c_write_id = ctx->s_ctx.i2c_addr_table[i];
		do {
			*sensor_id = (subdrv_ixc_rd_u8(ctx, addr_h) << 8) |
				subdrv_ixc_rd_u8(ctx, addr_l);
			if (addr_ll)
				*sensor_id = ((*sensor_id) << 8) | subdrv_ixc_rd_u8(ctx, addr_ll);
			DRV_LOG(ctx, "i2c_write_id:0x%x sensor_id(cur/exp):0x%x/0x%x\n",
				ctx->i2c_write_id, *sensor_id, ctx->s_ctx.sensor_id);
			if (*sensor_id == ctx->s_ctx.sensor_id)
				return ERROR_NONE;
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != ctx->s_ctx.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

void subdrv_ctx_init(struct subdrv_ctx *ctx)
{
	int i, j, exp_cnt;

	ctx->ana_gain_def = ctx->s_ctx.ana_gain_def;
	ctx->ana_gain_max = ctx->s_ctx.ana_gain_max;
	ctx->ana_gain_min = ctx->s_ctx.ana_gain_min;
	ctx->ana_gain_step = ctx->s_ctx.ana_gain_step;
	ctx->exposure_def = ctx->s_ctx.exposure_def;
	ctx->exposure_max = ctx->s_ctx.exposure_max;
	ctx->exposure_min = ctx->s_ctx.exposure_min;
	ctx->exposure_step = ctx->s_ctx.exposure_step;
	ctx->frame_time_delay_frame = ctx->s_ctx.frame_time_delay_frame;
	ctx->margin = ctx->s_ctx.exposure_margin;
	ctx->max_frame_length = ctx->s_ctx.frame_length_max;
	ctx->mirror = ctx->s_ctx.mirror;
	ctx->sensor_mode = 0;
	ctx->shutter = ctx->s_ctx.exposure_def;
	ctx->gain = ctx->s_ctx.ana_gain_def;
	ctx->dummy_pixel = 0;
	ctx->dummy_line = 0;
	ctx->current_fps = 300;
	ctx->autoflicker_en = FALSE;
	ctx->test_pattern = FALSE;
	ctx->current_scenario_id = SENSOR_SCENARIO_ID_MIN;
	ctx->ihdr_mode = 0;
	ctx->i2c_write_id = 0x20;
	ctx->readout_length = 0;
	ctx->read_margin = 0;
	ctx->current_ae_effective_frame = ctx->s_ctx.ae_effective_frame;
	ctx->extend_frame_length_en = FALSE;
	ctx->ae_ctrl_gph_en = FALSE;
	ctx->l_shift = 0;
	for (i = 0; i < ctx->s_ctx.sensor_mode_num ; i++) {
		if (!ctx->s_ctx.mode[i].sensor_output_dataformat)
			ctx->s_ctx.mode[i].sensor_output_dataformat =
				ctx->s_ctx.sensor_output_dataformat;
		if (!ctx->s_ctx.mode[i].dig_gain_min)
			ctx->s_ctx.mode[i].dig_gain_min = ctx->s_ctx.dig_gain_min;
		if (!ctx->s_ctx.mode[i].dig_gain_max)
			ctx->s_ctx.mode[i].dig_gain_max = ctx->s_ctx.dig_gain_max;
		if (!ctx->s_ctx.mode[i].min_exposure_line)
			ctx->exposure_min = ctx->s_ctx.mode[i].min_exposure_line;
		if (!ctx->s_ctx.mode[i].saturation_info)
			ctx->s_ctx.mode[i].saturation_info = ctx->s_ctx.saturation_info;
		if (!ctx->s_ctx.mode[i].exposure_margin)
			ctx->s_ctx.mode[i].exposure_margin = ctx->s_ctx.exposure_margin;
		exp_cnt = ctx->s_ctx.mode[i].exp_cnt ?
					ctx->s_ctx.mode[i].exp_cnt : 1;
		for (j = 0; j < exp_cnt; j++) {
			if (!ctx->s_ctx.mode[i].multi_exposure_ana_gain_range[j].min)
				ctx->s_ctx.mode[i].multi_exposure_ana_gain_range[j].min =
					ctx->s_ctx.ana_gain_min;
			if (!ctx->s_ctx.mode[i].multi_exposure_ana_gain_range[j].max)
				ctx->s_ctx.mode[i].multi_exposure_ana_gain_range[j].max =
					ctx->s_ctx.ana_gain_max;
			if (!ctx->s_ctx.mode[i].multi_exposure_shutter_range[j].min)
				ctx->s_ctx.mode[i].multi_exposure_shutter_range[j].min =
					ctx->s_ctx.exposure_min;
			if (!ctx->s_ctx.mode[i].multi_exposure_shutter_range[j].max)
				ctx->s_ctx.mode[i].multi_exposure_shutter_range[j].max =
					ctx->s_ctx.exposure_max;
		}
	}
	ctx->aov_sensor_support = ctx->s_ctx.aov_sensor_support;
	ctx->aov_csi_clk = ctx->s_ctx.aov_csi_clk;
	if (ctx->aov_sensor_support && !ctx->aov_csi_clk)
		DRV_LOGE(ctx, "please assign aov csi clk!\n");
	ctx->sensor_mode_ops = ctx->s_ctx.sensor_mode_ops;
	ctx->sensor_debug_sensing_ut_on_scp =
		ctx->s_ctx.sensor_debug_sensing_ut_on_scp;
	ctx->sensor_debug_dphy_global_timing_continuous_clk =
		ctx->s_ctx.sensor_debug_dphy_global_timing_continuous_clk;
	ctx->eeprom_index = ctx->s_ctx.eeprom_num;
}

void sensor_init(struct subdrv_ctx *ctx)
{
	u64 time_boot_begin = 0;
	u64 ixc_time = 0;
	int i;
	u32 delay_in_us, tlb_len;
	u32 max_tlb_len = 0;

	/* write init setting */
	if (ctx->s_ctx.init_setting_table_v2 != NULL) {
		/* using init setting table v2 for initialization */

		/* apply setting */
		for (i = 0; i < ctx->s_ctx.init_setting_table_v2_cnt; i++) {
			delay_in_us = ctx->s_ctx.init_setting_table_v2[i].delay;
			tlb_len = ctx->s_ctx.init_setting_table_v2[i].setting_table_len;

			ixc_time = ixc_table_write_with_entry(ctx, &ctx->s_ctx.init_setting_table_v2[i]);

			if (tlb_len > max_tlb_len) {
				/*
				 * report the i2c init setting period only on
				 * the maximum table to prevent affected by delay
				 */
				max_tlb_len = tlb_len;
				if ((ctx->power_on_profile_en != NULL) &&
					(*ctx->power_on_profile_en)) {
					ctx->sensor_pw_on_profile.i2c_init_period = ixc_time * 1000;

					ctx->sensor_pw_on_profile.i2c_init_table_len = tlb_len;
				}
			}

			udelay(delay_in_us);
			DRV_LOG_MUST(ctx, "init table v2: item[%d] size:%u, i2c time(us):%lld, delay(us):%u\n",
				     i, tlb_len, ixc_time, delay_in_us);
		}
	} else if (ctx->s_ctx.init_setting_table != NULL) {
		/* using legacy init setting table for initialization */
		DRV_LOG(ctx, "S: size:%u\n", ctx->s_ctx.init_setting_len);
		if ((ctx->power_on_profile_en != NULL) &&
			(*ctx->power_on_profile_en))
			time_boot_begin = ktime_get_boottime_ns();

		ixc_time = ixc_table_write(ctx, ctx->s_ctx.init_setting_table, ctx->s_ctx.init_setting_len);

		if ((ctx->power_on_profile_en != NULL) &&
			(*ctx->power_on_profile_en)) {
			ctx->sensor_pw_on_profile.i2c_init_period =
				ktime_get_boottime_ns() - time_boot_begin;

			 ctx->sensor_pw_on_profile.i2c_init_table_len =
							ctx->s_ctx.init_setting_len;
		}
		DRV_LOG_MUST(ctx, "X: size:%u, time(us):%lld\n", ctx->s_ctx.init_setting_len,
			ixc_time);
	} else {
		DRV_LOG_MUST(ctx, "please implement initial setting!\n");
	}
	/* enable temperature sensor */
#if IMGSENSOR_AOV_EINT_UT
#else
	if (ctx->s_ctx.temperature_support && ctx->s_ctx.reg_addr_temp_en)
		subdrv_ixc_wr_u8(ctx, ctx->s_ctx.reg_addr_temp_en, 0x01);
	/* enable mirror or flip */
	set_mirror_flip(ctx, ctx->mirror);
#endif
}

int common_open(struct subdrv_ctx *ctx)
{
	u32 sensor_id = 0;
	u32 scenario_id = 0;
	u32 linetime_in_ns = 0;
	struct adaptor_ctx *_adaptor_ctx = NULL;
	struct v4l2_subdev *sd = NULL;

	if (ctx->i2c_client)
		sd = i2c_get_clientdata(ctx->i2c_client);
	if (ctx->ixc_client.protocol)
		sd = adaptor_ixc_get_clientdata(&ctx->ixc_client);
	if (sd)
		_adaptor_ctx = to_ctx(sd);
	if (!_adaptor_ctx) {
		DRV_LOGE(ctx, "null _adaptor_ctx\n");
		return -ENODEV;
	}

	ADAPTOR_SYSTRACE_BEGIN("imgsensor::common_get_imgsensor_id");
	/* get sensor id */
	if (common_get_imgsensor_id(ctx, &sensor_id) != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;
	ADAPTOR_SYSTRACE_END();

	/* no init setting due to scp sensor power on */
	if (ctx->s_ctx.aov_sensor_support &&
		ctx->s_ctx.mode[scenario_id].aov_mode &&
		_adaptor_ctx->aov_scp_pwr) {
		DRV_LOG_MUST(ctx, "no init setting due to scp sensor power on\n");
		return ERROR_NONE;
	}
	ADAPTOR_SYSTRACE_BEGIN("imgsensor::sensor_init i2c table write");
	/* initail setting */
	if (ctx->s_ctx.aov_sensor_support && !ctx->s_ctx.init_in_open)
		DRV_LOG_MUST(ctx, "sensor init not in open stage!\n");
	else
		sensor_init(ctx);
	ADAPTOR_SYSTRACE_END();

	ADAPTOR_SYSTRACE_BEGIN("imgsensor::write_sensor_Cali");
	if (ctx->s_ctx.s_cali != NULL)
		ctx->s_ctx.s_cali((void *) ctx);
	else
		write_sensor_Cali(ctx);
	ADAPTOR_SYSTRACE_END();

	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	memset(ctx->ana_gain, 0, sizeof(ctx->gain));
	ctx->exposure[0] = ctx->s_ctx.exposure_def;
	ctx->ana_gain[0] = ctx->s_ctx.ana_gain_def;
	ctx->current_scenario_id = scenario_id;
	ctx->pclk = ctx->s_ctx.mode[scenario_id].pclk;
	ctx->line_length = ctx->s_ctx.mode[scenario_id].linelength;
	ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;
	ctx->frame_length_rg = ctx->frame_length;

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			ctx->current_scenario_id, &linetime_in_ns, 0, 0);
		ctx->current_fps = 1000000000 / linetime_in_ns * 10 / ctx->frame_length;
	} else {
		ctx->current_fps = ctx->pclk / ctx->line_length * 10 / ctx->frame_length;
	}

	ctx->readout_length = ctx->s_ctx.mode[scenario_id].readout_length;
	ctx->read_margin = ctx->s_ctx.mode[scenario_id].read_margin;
	ctx->min_frame_length = ctx->frame_length;
	ctx->autoflicker_en = FALSE;
	ctx->test_pattern = 0;
	ctx->ihdr_mode = 0;
	ctx->pdaf_mode = 0;
	ctx->hdr_mode = 0;
	ctx->extend_frame_length_en = 0;
	ctx->is_seamless = 0;
	ctx->fast_mode_on = 0;
	ctx->sof_cnt = 0;
	ctx->ref_sof_cnt = 0;
	ctx->is_streaming = 0;

	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF) {
		memset(ctx->frame_length_in_lut, 0,
			sizeof(ctx->frame_length_in_lut));
		memset(ctx->frame_length_pre_store_in_lut, 0,
			sizeof(ctx->frame_length_pre_store_in_lut));

		switch (ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt) {
		case 2:
			if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 2) {
				spin_lock(&ctx->aeb_ae_ctrl_cnt_lock);
				ctx->aeb_ae_ctrl_cnt = 0;
				ctx->aeb_ae_ctrl_cnt_last = 0;
				spin_unlock(&ctx->aeb_ae_ctrl_cnt_lock);
				ctx->frame_length_in_lut[1] = ctx->readout_length + ctx->read_margin;
				ctx->frame_length_pre_store_in_lut[1] =
					ctx->frame_length_in_lut[1];
				ctx->frame_length_in_lut[0] = ctx->frame_length -
					ctx->frame_length_in_lut[1];
				ctx->frame_length_pre_store_in_lut[0] =
					ctx->frame_length_in_lut[0];
			} else if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 3) {
				ctx->frame_length_in_lut[0] = ctx->readout_length + ctx->read_margin;
				ctx->frame_length_in_lut[1] = ctx->frame_length -
					ctx->frame_length_in_lut[0];
			} else
				DRV_LOGE(ctx, "pls check delay_frame value!\n");
			break;
		case 3:
			if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 2) {
				spin_lock(&ctx->aeb_ae_ctrl_cnt_lock);
				ctx->aeb_ae_ctrl_cnt = 0;
				ctx->aeb_ae_ctrl_cnt_last = 0;
				spin_unlock(&ctx->aeb_ae_ctrl_cnt_lock);
				ctx->frame_length_in_lut[2] = ctx->readout_length + ctx->read_margin;
				ctx->frame_length_pre_store_in_lut[2] =
					ctx->frame_length_in_lut[2];
				ctx->frame_length_in_lut[1] = ctx->readout_length + ctx->read_margin;
				ctx->frame_length_pre_store_in_lut[1] =
					ctx->frame_length_in_lut[1];
				ctx->frame_length_in_lut[0] = ctx->frame_length -
					ctx->frame_length_in_lut[1] - ctx->frame_length_in_lut[2];
				ctx->frame_length_pre_store_in_lut[0] =
					ctx->frame_length_in_lut[0];
			} else if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 3) {
				ctx->frame_length_in_lut[0] = ctx->readout_length + ctx->read_margin;
				ctx->frame_length_in_lut[1] = ctx->readout_length + ctx->read_margin;
				ctx->frame_length_in_lut[2] = ctx->frame_length -
					ctx->frame_length_in_lut[1] - ctx->frame_length_in_lut[0];
			} else
				DRV_LOGE(ctx, "pls check delay_frame value!\n");
			break;
		default:
			break;
		}

		memcpy(ctx->frame_length_in_lut_rg, ctx->frame_length_in_lut,
			sizeof(ctx->frame_length_in_lut_rg));
	} else if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_RAW_VS ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE_VS) {
		// DONE: DCG-VS
		memset(ctx->frame_length_in_lut, 0,
			sizeof(ctx->frame_length_in_lut));

		switch (ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt) {
		case 3:
			ctx->frame_length_in_lut[0] = get_lut_static_info(ctx, u32, framelength,
								ctx->current_scenario_id, 0);
			ctx->frame_length_in_lut[1] = get_lut_static_info(ctx, u32, framelength,
								ctx->current_scenario_id, 1);
			break;
		default:
			break;
		}

		memcpy(ctx->frame_length_in_lut_rg, ctx->frame_length_in_lut,
			sizeof(ctx->frame_length_in_lut_rg));
	}

	return ERROR_NONE;
}

int common_get_info(struct subdrv_ctx *ctx,
		enum MSDK_SCENARIO_ID_ENUM scenario_id,
		MSDK_SENSOR_INFO_STRUCT *sensor_info,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	int i = 0;
	enum IMGSENSOR_HDR_MODE_ENUM hdr_mode;

	(void) sensor_config_data;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	} else {
		DRV_LOG(ctx, "sid:%u\n", scenario_id);
	}

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensroInterfaceType = ctx->s_ctx.sensor_interface_type;
	sensor_info->MIPIsensorType = ctx->s_ctx.mipi_sensor_type;
	sensor_info->SensorOutputDataFormat =
		ctx->s_ctx.mode[scenario_id].sensor_output_dataformat;
	for (i = 0; i < ctx->s_ctx.sensor_mode_num; i++) {
		sensor_info->sensorFormatCellType[i] = ctx->s_ctx.mode[i].sensor_output_dataformat_cell_type;
		sensor_info->DelayFrame[i] = ctx->s_ctx.mode[i].delay_frame;
		sensor_info->ModeInfo[i].SensorDpcEnabled = ctx->s_ctx.mode[i].dpc_enabled;
		sensor_info->ModeInfo[i].SensorPdcEnabled = ctx->s_ctx.mode[i].pdc_enabled;
		if (ctx->s_ctx.mode[i].saturation_info) {
			sensor_info->gain_ratio[i] =
				ctx->s_ctx.mode[i].saturation_info->gain_ratio;
			sensor_info->OB_pedestals[i] =
				ctx->s_ctx.mode[i].saturation_info->OB_pedestal;
			sensor_info->saturation_level[i] =
				ctx->s_ctx.mode[i].saturation_info->saturation_level;
			sensor_info->adc_bit[i] =
				ctx->s_ctx.mode[i].saturation_info->adc_bit;
			if (ctx->s_ctx.mode[i].saturation_info->ob_bm)
				sensor_info->ob_bm[i] =
					ctx->s_ctx.mode[i].saturation_info->ob_bm;
			else
				sensor_info->ob_bm[i] = ctx->s_ctx.ob_pedestal;
		} else {
			sensor_info->gain_ratio[i] = 1000;
			sensor_info->OB_pedestals[i] = ctx->s_ctx.ob_pedestal;
			sensor_info->saturation_level[i] = 1023;
			sensor_info->adc_bit[i] = 10;
			sensor_info->ob_bm[i] = ctx->s_ctx.ob_pedestal;
		}
		sensor_info->Mode_AE_Ctrl_Support[i] = ctx->s_ctx.mode[i].ae_ctrl_support;
		sensor_info->hdr_cap[i] = ctx->s_ctx.mode[i].hdr_mode;
		sensor_info->raw_cnt[i] = ctx->s_ctx.mode[i].raw_cnt ?
			ctx->s_ctx.mode[i].raw_cnt : 1;
		sensor_info->exp_cnt[i] = ctx->s_ctx.mode[i].exp_cnt ?
			ctx->s_ctx.mode[i].exp_cnt : 1;
		sensor_info->max_framelength[i] = ctx->s_ctx.frame_length_max;
		switch (ctx->s_ctx.frame_time_delay_frame) {
		case 3:
			sensor_info->max_framelength[i] /= ctx->s_ctx.reg_addr_exposure_lshift ? 2 : 1;
			break;
		default:
			break;
		}
		sensor_info->fine_integ_line[i] = ctx->s_ctx.mode[i].fine_integ_line;
		sensor_info->aov_mode[i] = ctx->s_ctx.mode[i].aov_mode;
		sensor_info->support_mcss[i] = ctx->s_ctx.mode[i].support_mcss;
		sensor_info->SensorAwbEnabled[i] = ctx->s_ctx.mode[i].awb_enabled;
		sensor_info->sensor_stick_hdr_vsync_status[i] = ctx->s_ctx.mode[i].is_stick_hdr_vsync;
		sensor_info->exposure_order_in_lbmf[i] = ctx->s_ctx.mode[i].exposure_order_in_lbmf;
		sensor_info->exposure_order_in_dcg[i] = ctx->s_ctx.mode[i].exposure_order_in_dcg;
		sensor_info->mipi_pixel_rate[i] = ctx->s_ctx.mode[i].mipi_pixel_rate;
	}
	sensor_info->SensorDrivingCurrent = g_mclk_info(ctx, pw_seq, HW_ID_MCLK_DRIVING_CURRENT);
	sensor_info->IHDR_Support = 0;
	sensor_info->IHDR_LE_FirstLine = 0;
	sensor_info->TEMPERATURE_SUPPORT = ctx->s_ctx.temperature_support;
	sensor_info->SensorModeNum = ctx->s_ctx.sensor_mode_num;
	sensor_info->PDAF_Support = ctx->s_ctx.pdaf_type;
	sensor_info->HDR_Support = ctx->s_ctx.hdr_type;
	sensor_info->RGBW_Support = ctx->s_ctx.rgbw_support;
	sensor_info->SensorMIPILaneNumber = ctx->s_ctx.mipi_lane_num;
	sensor_info->SensorClockFreq = g_mclk_info(ctx, pw_seq, HW_ID_MCLK);
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorWidthSampling = 0;
	sensor_info->SensorHightSampling = 0;
	sensor_info->SensorPacketECCOrder = 1;
	sensor_info->FrameTimeDelayFrame = ctx->s_ctx.frame_time_delay_frame;
	sensor_info->OB_pedestal = ctx->s_ctx.ob_pedestal;
	sensor_info->Def_MirrorFlip = ctx->s_ctx.mirror;
	sensor_info->SensorLineInterleaveNum = (ctx->s_ctx.line_interleave_num)
		? ctx->s_ctx.line_interleave_num : 2;
	sensor_info->OCL_info = (ctx->s_ctx.ocl_info) ? ctx->s_ctx.ocl_info : 1;


	for (i = 0; i < ctx->s_ctx.sensor_mode_num; i++) {
		hdr_mode = ctx->s_ctx.mode[scenario_id].hdr_mode;
		if (hdr_mode == HDR_RAW_DCG_RAW || hdr_mode == HDR_RAW_DCG_RAW_VS) {
			if (ctx->s_ctx.line_interleave_num == 0)
				WRAP_AEE_EXCEPTION("common_get_info",
					"line_interleave_num should not be 0");
		}
	}

	return ERROR_NONE;
}

void mcss_get_prsh_length_lines(struct subdrv_ctx *ctx,
	struct mtk_hdr_ae *ae_ctrl,
	enum SENSOR_SCENARIO_ID_ENUM pre_seamless_scenario_id,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id)
{
#define MCSS_TH 50
	u32 ae_ctrl_cit;
	u32 prsh_length_lc = 0;
	u32 cit_step = 1;
	u64 frame_duration_us = 0;
	u64 orig_readout_time_us = 0;
	u64 current_fps;
	u8 hw_fixed_value = ctx->s_ctx.seamless_switch_prsh_hw_fixed_value;
	enum IMGSENSOR_HDR_MODE_ENUM hdr_mode;
	u32 linetime_in_ns = 0;

	current_fps = (u64)ctx->current_fps;
	frame_duration_us = 10 * (1000000 / current_fps);


	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			pre_seamless_scenario_id, &linetime_in_ns, 0, 0);
		orig_readout_time_us =
		(u64)ctx->s_ctx.mode[pre_seamless_scenario_id].imgsensor_winsize_info.h1_size
			* linetime_in_ns / 1000;
	} else {
		orig_readout_time_us = 1000000 *
		(u64)ctx->s_ctx.mode[pre_seamless_scenario_id].imgsensor_winsize_info.h1_size
		* (ctx->line_length) // ctx->s_ctx.mode[pre_seamless_scenario_id].linelength
		/ctx->s_ctx.mode[pre_seamless_scenario_id].pclk;
	}

	DRV_LOG_MUST(ctx,
		"calc_prsh_length_lc(%d->%d): orig:pclk(%llu),linelength(%u),frame_length(%u),fps(%llu),frame_duration_us(%llu),readout_us(%llu) new:pclk(%llu),linelength(%u)\n",
					pre_seamless_scenario_id,scenario_id,
					ctx->s_ctx.mode[pre_seamless_scenario_id].pclk,
					ctx->line_length,
					ctx->s_ctx.mode[pre_seamless_scenario_id].framelength,
					current_fps,
					10 * (1000000 / current_fps),
					orig_readout_time_us,
					ctx->s_ctx.mode[scenario_id].pclk,
					ctx->line_length);

	if (frame_duration_us < orig_readout_time_us) {
		DRV_LOG_MUST(ctx,
				"pre-shutter no need : max_framerate:(%u->%u), current_fl(%llu) < (orig_readout_time(%llu)\n",
				ctx->s_ctx.mode[pre_seamless_scenario_id].max_framerate,
				ctx->s_ctx.mode[scenario_id].max_framerate,
				frame_duration_us,orig_readout_time_us);
		ctx->s_ctx.seamless_switch_prsh_length_lc = 0;
		return;
	}
	frame_duration_us -= orig_readout_time_us;
	if (frame_duration_us < (ctx->s_ctx.seamless_switch_hw_re_init_time_ns / 1000)) {
		DRV_LOG_MUST(ctx,
			"pre-shutter no need: current_fl(%llu) < (orig_readout_time(%llu) + hw_re_init_time(%u))\n",
			frame_duration_us,orig_readout_time_us,
			(ctx->s_ctx.seamless_switch_hw_re_init_time_ns / 1000));
		ctx->s_ctx.seamless_switch_prsh_length_lc = 0;
		return;
	}
	frame_duration_us -= (ctx->s_ctx.seamless_switch_hw_re_init_time_ns / 1000); // CIS boot time

	hdr_mode = ctx->s_ctx.mode[scenario_id].hdr_mode;
	switch (hdr_mode) {
	case HDR_RAW_LBMF:
		{
			DRV_LOGE(ctx, "HDR_RAW_LBMF not support !\n");
			return;
		}
		break;
	case HDR_NONE:
	case HDR_RAW:
	case HDR_CAMSV:
	case HDR_RAW_ZHDR:
	case HDR_MultiCAMSV:
	case HDR_RAW_STAGGER:
	case HDR_RAW_DCG_RAW:
	case HDR_RAW_DCG_COMPOSE:
	default:
		ae_ctrl_cit =ae_ctrl->exposure.le_exposure;
		break;
	}

	ae_ctrl_cit = FINE_INTEG_CONVERT(ae_ctrl_cit, ctx->s_ctx.mode[scenario_id].fine_integ_line);

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			scenario_id, &linetime_in_ns, 0, 0);
		prsh_length_lc = frame_duration_us
						* 1000 / linetime_in_ns;
	} else {
		prsh_length_lc = frame_duration_us
						* ctx->s_ctx.mode[scenario_id].pclk
						/ ctx->line_length
						/ 1000000;
	}

	ae_ctrl_cit = max(ae_ctrl_cit, ctx->s_ctx.exposure_min);
	ae_ctrl_cit = min(ae_ctrl_cit, ctx->s_ctx.exposure_max);
	cit_step = ctx->s_ctx.mode[ctx->current_scenario_id].coarse_integ_step ?: 1;
	if (cit_step) {
		ae_ctrl_cit = roundup(ae_ctrl_cit, cit_step);
		prsh_length_lc = roundup(prsh_length_lc, cit_step);
	}

	prsh_length_lc = (prsh_length_lc > (ae_ctrl_cit + hw_fixed_value)) ? prsh_length_lc : 0;
	if (prsh_length_lc < (ae_ctrl_cit + hw_fixed_value)) {
		DRV_LOG_MUST(ctx,
			"pre-shutter no need: prsh_length_lc(%u) < (ae_ctrl_cit(%u(max=%u,min=%u)) + hw_fixed_value(%u))\n",
			prsh_length_lc, ae_ctrl_cit, ctx->s_ctx.exposure_max, ctx->s_ctx.exposure_min, hw_fixed_value);
		ctx->s_ctx.seamless_switch_prsh_length_lc = 0;
		return;
	}

	if ((ctx->mcss_init_info.enable_mcss) && (ctx->mcss_init_info.is_mcss_master))
		prsh_length_lc += MCSS_TH;
	if ((ctx->mcss_init_info.enable_mcss) && (!(ctx->mcss_init_info.is_mcss_master)))
		prsh_length_lc = (prsh_length_lc > MCSS_TH) ? (prsh_length_lc - MCSS_TH) : 0;
	ctx->s_ctx.seamless_switch_prsh_length_lc = prsh_length_lc;
}

void common_get_prsh_length_lines(struct subdrv_ctx *ctx,
	struct mtk_hdr_ae *ae_ctrl,
	enum SENSOR_SCENARIO_ID_ENUM pre_seamless_scenario_id,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id)
{
	u32 ae_ctrl_cit;
	u32 prsh_length_lc = 0;
	u32 cit_step = 1;
	u64 frame_duration_us = 0;
	u64 orig_readout_time_us = 0;
	u64 current_fps;
	u8 hw_fixed_value = ctx->s_ctx.seamless_switch_prsh_hw_fixed_value;
	enum IMGSENSOR_HDR_MODE_ENUM hdr_mode;
	u32 linetime_in_ns = 0;

	current_fps = (u64)ctx->current_fps;
	frame_duration_us = 10 * (1000000 / current_fps);

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			pre_seamless_scenario_id, &linetime_in_ns, 0, 0);
		orig_readout_time_us = (u64)ctx->s_ctx.mode[pre_seamless_scenario_id].imgsensor_winsize_info.h1_size
					* linetime_in_ns / 1000;
	} else {
		orig_readout_time_us = 1000000
					* (u64)ctx->s_ctx.mode[pre_seamless_scenario_id].imgsensor_winsize_info.h1_size
					*ctx->s_ctx.mode[pre_seamless_scenario_id].linelength
					/ctx->s_ctx.mode[pre_seamless_scenario_id].pclk;
	}

	DRV_LOG_MUST(ctx,
		"calc_prsh_length_lc(%d->%d): orig:pclk(%llu),linelength(%u),frame_length(%u),fps(%llu),frame_duration_us(%llu),readout_us(%llu) new:pclk(%llu),linelength(%u)\n",
					pre_seamless_scenario_id,scenario_id,
					ctx->s_ctx.mode[pre_seamless_scenario_id].pclk,
					ctx->s_ctx.mode[pre_seamless_scenario_id].linelength,
					ctx->s_ctx.mode[pre_seamless_scenario_id].framelength,
					current_fps,
					10 * (1000000 / current_fps),
					orig_readout_time_us,
					ctx->s_ctx.mode[scenario_id].pclk,
					ctx->s_ctx.mode[scenario_id].linelength);

	if (frame_duration_us < orig_readout_time_us) {
		DRV_LOG_MUST(ctx,
				"pre-shutter no need : max_framerate:(%u->%u), current_fl(%llu) < (orig_readout_time(%llu)\n",
				ctx->s_ctx.mode[pre_seamless_scenario_id].max_framerate,
				ctx->s_ctx.mode[scenario_id].max_framerate,
				frame_duration_us,orig_readout_time_us);
		ctx->s_ctx.seamless_switch_prsh_length_lc = 0;
		return;
	}
	frame_duration_us -= orig_readout_time_us;
	if (frame_duration_us < (ctx->s_ctx.seamless_switch_hw_re_init_time_ns / 1000)) {
		DRV_LOG_MUST(ctx,
			"pre-shutter no need: current_fl(%llu) < (orig_readout_time(%llu) + hw_re_init_time(%u))\n",
			frame_duration_us,orig_readout_time_us,
			(ctx->s_ctx.seamless_switch_hw_re_init_time_ns / 1000));
		ctx->s_ctx.seamless_switch_prsh_length_lc = 0;
		return;
	}
	frame_duration_us -= (ctx->s_ctx.seamless_switch_hw_re_init_time_ns / 1000); // CIS boot time

	hdr_mode = ctx->s_ctx.mode[scenario_id].hdr_mode;
	switch (hdr_mode) {
	case HDR_RAW_LBMF:
		if (ctx->s_ctx.mode[ctx->current_scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
			/* 2exp: dig_gain_lut_a = SE / dig_gain_lut_b = LE */
			/* 3exp: dig_gain_lut_a = SE / dig_gain_lut_b = ME / dig_gain_lut_c = LE */
			ae_ctrl_cit =ae_ctrl->exposure.se_exposure;
		} else if (ctx->s_ctx.mode[ctx->current_scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_LE_FIRST) {
			/* 2exp: dig_gain_lut_a = LE / dig_gain_lut_b = SE */
			/* 3exp: dig_gain_lut_a = LE / dig_gain_lut_b = ME / dig_gain_lut_c = SE */
			ae_ctrl_cit =ae_ctrl->exposure.le_exposure;
		} else {
			DRV_LOGE(ctx, "pls assign exposure_order_in_lbmf value!\n");
			return;
		}
		break;
	case HDR_NONE:
	case HDR_RAW:
	case HDR_CAMSV:
	case HDR_RAW_ZHDR:
	case HDR_MultiCAMSV:
	case HDR_RAW_STAGGER:
	case HDR_RAW_DCG_RAW:
	case HDR_RAW_DCG_COMPOSE:
	default:
		ae_ctrl_cit =ae_ctrl->exposure.le_exposure;
		break;
	}

	ae_ctrl_cit = FINE_INTEG_CONVERT(ae_ctrl_cit, ctx->s_ctx.mode[scenario_id].fine_integ_line);

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			scenario_id, &linetime_in_ns, 0, 0);
		prsh_length_lc = frame_duration_us
						* 1000 / linetime_in_ns;
	} else {
		prsh_length_lc = frame_duration_us
						* ctx->s_ctx.mode[scenario_id].pclk
						/ ctx->s_ctx.mode[scenario_id].linelength
						/ 1000000;
	}

	ae_ctrl_cit = max(ae_ctrl_cit, ctx->s_ctx.exposure_min);
	ae_ctrl_cit = min(ae_ctrl_cit, ctx->s_ctx.exposure_max);
	cit_step = ctx->s_ctx.mode[ctx->current_scenario_id].coarse_integ_step ?: 1;
	if (cit_step) {
		ae_ctrl_cit = roundup(ae_ctrl_cit, cit_step);
		prsh_length_lc = roundup(prsh_length_lc, cit_step);
	}

	prsh_length_lc = (prsh_length_lc > (ae_ctrl_cit + hw_fixed_value)) ? prsh_length_lc : 0;
	if (prsh_length_lc < (ae_ctrl_cit + hw_fixed_value)) {
		DRV_LOG_MUST(ctx,
			"pre-shutter no need: prsh_length_lc(%u) < (ae_ctrl_cit(%u(max=%u,min=%u)) + hw_fixed_value(%u))\n",
			prsh_length_lc, ae_ctrl_cit, ctx->s_ctx.exposure_max, ctx->s_ctx.exposure_min, hw_fixed_value);
		ctx->s_ctx.seamless_switch_prsh_length_lc = 0;
		return;
	}
	ctx->s_ctx.seamless_switch_prsh_length_lc = prsh_length_lc;
}

/* get prsh line by user define time that is between end of last frame readout and next frame vsync */
void common_get_prsh_length_lines_by_time(struct subdrv_ctx *ctx,
	struct mtk_hdr_ae *ae_ctrl,
	enum SENSOR_SCENARIO_ID_ENUM pre_seamless_scenario_id,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	int time_ms)
{
	u64 ae_ctrl_cit;
	u64 prsh_length_lc = 0;
	u32 cit_step = 1;
	u64 prsh_time_us = 0;
	u64 orig_readout_time_us = 0;
	u64 current_fps;
	u8 hw_fixed_value = ctx->s_ctx.seamless_switch_prsh_hw_fixed_value;
	enum IMGSENSOR_HDR_MODE_ENUM hdr_mode;
	u32 linetime_in_ns = 0;

	if (time_ms <= 0){
		ctx->s_ctx.seamless_switch_prsh_length_lc = 0;
		return;
	}

	current_fps = (u64)ctx->current_fps;
	prsh_time_us = 1000 * (u64)time_ms;

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			pre_seamless_scenario_id, &linetime_in_ns, 0, 0);
		orig_readout_time_us = (u64)ctx->s_ctx.mode[pre_seamless_scenario_id].imgsensor_winsize_info.h1_size
					* linetime_in_ns / 1000;
	} else {
		orig_readout_time_us = 1000000
					* (u64)ctx->s_ctx.mode[pre_seamless_scenario_id].imgsensor_winsize_info.h1_size
					*ctx->s_ctx.mode[pre_seamless_scenario_id].linelength
					/ctx->s_ctx.mode[pre_seamless_scenario_id].pclk;
	}

	if (prsh_time_us < (ctx->s_ctx.seamless_switch_hw_re_init_time_ns / 1000)) {
		DRV_LOG_MUST(ctx,
			"pre-shutter no need: current_fl(%llu) <  hw_re_init_time(%u)\n",
			prsh_time_us,
			(ctx->s_ctx.seamless_switch_hw_re_init_time_ns / 1000));
		ctx->s_ctx.seamless_switch_prsh_length_lc = 0;
		return;
	}
	prsh_time_us -= (ctx->s_ctx.seamless_switch_hw_re_init_time_ns / 1000); // CIS boot time

	hdr_mode = ctx->s_ctx.mode[scenario_id].hdr_mode;
	switch (hdr_mode) {
	case HDR_RAW_LBMF:
		if (ctx->s_ctx.mode[ctx->current_scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_SE_FIRST) {
			/* 2exp: dig_gain_lut_a = SE / dig_gain_lut_b = LE */
			/* 3exp: dig_gain_lut_a = SE / dig_gain_lut_b = ME / dig_gain_lut_c = LE */
			ae_ctrl_cit =ae_ctrl->exposure.me_exposure;
		} else if (ctx->s_ctx.mode[ctx->current_scenario_id].exposure_order_in_lbmf ==
			IMGSENSOR_LBMF_EXPOSURE_LE_FIRST) {
			/* 2exp: dig_gain_lut_a = LE / dig_gain_lut_b = SE */
			/* 3exp: dig_gain_lut_a = LE / dig_gain_lut_b = ME / dig_gain_lut_c = SE */
			ae_ctrl_cit =ae_ctrl->exposure.le_exposure;
		} else {
			DRV_LOGE(ctx, "pls assign exposure_order_in_lbmf value!\n");
			return;
		}
		break;
	case HDR_NONE:
	case HDR_RAW:
	case HDR_CAMSV:
	case HDR_RAW_ZHDR:
	case HDR_MultiCAMSV:
	case HDR_RAW_STAGGER:
	case HDR_RAW_DCG_RAW:
	case HDR_RAW_DCG_COMPOSE:
	default:
		ae_ctrl_cit =ae_ctrl->exposure.le_exposure;
		break;
	}

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			scenario_id, &linetime_in_ns, 0, 0);
		prsh_length_lc = prsh_time_us
						* 1000 / linetime_in_ns;
	} else {
		prsh_length_lc = prsh_time_us
						* ctx->s_ctx.mode[scenario_id].pclk
						/ ctx->s_ctx.mode[scenario_id].linelength
						/ 1000000;
	}

	ae_ctrl_cit = max_t(u64, ae_ctrl_cit, (u64)ctx->s_ctx.exposure_min);
	ae_ctrl_cit = min_t(u64, ae_ctrl_cit, (u64)ctx->s_ctx.exposure_max);
	cit_step = ctx->s_ctx.mode[ctx->current_scenario_id].coarse_integ_step ?: 1;
	if (cit_step) {
		ae_ctrl_cit = roundup(ae_ctrl_cit, cit_step);
		prsh_length_lc = roundup(prsh_length_lc, cit_step);
	}

	prsh_length_lc = (prsh_length_lc > (ae_ctrl_cit + hw_fixed_value)) ? prsh_length_lc : 0;
	if (prsh_length_lc < (ae_ctrl_cit + hw_fixed_value)) {
		DRV_LOG_MUST(ctx,
			"pre-shutter no need: prsh_length_lc(%llu) < (ae_ctrl_cit(%llu(max=%u,min=%u)) + hw_fixed_value(%u))\n",
			prsh_length_lc, ae_ctrl_cit, ctx->s_ctx.exposure_max, ctx->s_ctx.exposure_min, hw_fixed_value);
		ctx->s_ctx.seamless_switch_prsh_length_lc = 0;
		return;
	}
	ctx->s_ctx.seamless_switch_prsh_length_lc = prsh_length_lc;
	DRV_LOG_MUST(ctx,
		"calc_prsh_length_lc(%d->%d): time_ms(%d), fps(%llu), readout_us(%llu), prsh_length_lc(%llu), new mode shutter_line(%llu)\n",
			pre_seamless_scenario_id,scenario_id,
			time_ms,
			current_fps,
			orig_readout_time_us,
			prsh_length_lc,
			ae_ctrl_cit);
}

int common_get_resolution(struct subdrv_ctx *ctx,
	MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	int i;

	for (i = 0; i < ctx->s_ctx.sensor_mode_num; i++) {
		sensor_resolution->SensorWidth[i] =
			ctx->s_ctx.mode[i].imgsensor_winsize_info.w2_tg_size;
		sensor_resolution->SensorHeight[i] =
			ctx->s_ctx.mode[i].imgsensor_winsize_info.h2_tg_size;
	}
	return ERROR_NONE;
}

void update_mode_info(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id)
{
	u32 linetime_in_ns = 0;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return;
	}
	ctx->current_scenario_id = scenario_id;
	ctx->pclk = ctx->s_ctx.mode[scenario_id].pclk;
	ctx->line_length = ctx->s_ctx.mode[scenario_id].linelength;
	ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;
	ctx->frame_length_rg = ctx->frame_length;

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			ctx->current_scenario_id, &linetime_in_ns, 0, 0);
		ctx->current_fps = 1000000000 / linetime_in_ns * 10 / ctx->frame_length;
	} else {
		ctx->current_fps = ctx->pclk / ctx->line_length * 10 / ctx->frame_length;
	}

	ctx->readout_length = ctx->s_ctx.mode[scenario_id].readout_length;
	ctx->read_margin = ctx->s_ctx.mode[scenario_id].read_margin;
	ctx->min_frame_length = ctx->frame_length;
	ctx->margin = ctx->s_ctx.mode[scenario_id].exposure_margin;
	ctx->autoflicker_en = FALSE;
	ctx->l_shift = 0;
	ctx->min_vblanking_line = ctx->s_ctx.mode[scenario_id].min_vblanking_line;

	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_LBMF) {
		memset(ctx->frame_length_in_lut, 0,
			sizeof(ctx->frame_length_in_lut));
		memset(ctx->frame_length_pre_store_in_lut, 0,
			sizeof(ctx->frame_length_pre_store_in_lut));

		switch (ctx->s_ctx.mode[scenario_id].exp_cnt) {
		case 2:
			if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 2) {
				spin_lock(&ctx->aeb_ae_ctrl_cnt_lock);
				ctx->aeb_ae_ctrl_cnt = 0;
				ctx->aeb_ae_ctrl_cnt_last = 0;
				spin_unlock(&ctx->aeb_ae_ctrl_cnt_lock);
				ctx->frame_length_in_lut[1] = ctx->readout_length + ctx->read_margin;
				ctx->frame_length_pre_store_in_lut[1] =
					ctx->frame_length_in_lut[1];
				ctx->frame_length_in_lut[0] = ctx->frame_length -
					ctx->frame_length_in_lut[1];
				ctx->frame_length_pre_store_in_lut[0] =
					ctx->frame_length_in_lut[0];
			} else if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 3) {
				ctx->frame_length_in_lut[0] = ctx->readout_length + ctx->read_margin;
				ctx->frame_length_in_lut[1] = ctx->frame_length -
					ctx->frame_length_in_lut[0];
			} else
				DRV_LOGE(ctx, "pls check delay_frame value!\n");
			break;
		case 3:
			if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 2) {
				spin_lock(&ctx->aeb_ae_ctrl_cnt_lock);
				ctx->aeb_ae_ctrl_cnt = 0;
				ctx->aeb_ae_ctrl_cnt_last = 0;
				spin_unlock(&ctx->aeb_ae_ctrl_cnt_lock);
				ctx->frame_length_in_lut[2] = ctx->readout_length + ctx->read_margin;
				ctx->frame_length_pre_store_in_lut[2] =
					ctx->frame_length_in_lut[2];
				ctx->frame_length_in_lut[1] = ctx->readout_length + ctx->read_margin;
				ctx->frame_length_pre_store_in_lut[1] =
					ctx->frame_length_in_lut[1];
				ctx->frame_length_in_lut[0] = ctx->frame_length -
					ctx->frame_length_in_lut[1] - ctx->frame_length_in_lut[2];
				ctx->frame_length_pre_store_in_lut[0] =
					ctx->frame_length_in_lut[0];
			} else if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame == 3) {
				ctx->frame_length_in_lut[0] = ctx->readout_length + ctx->read_margin;
				ctx->frame_length_in_lut[1] = ctx->readout_length + ctx->read_margin;
				ctx->frame_length_in_lut[2] = ctx->frame_length -
					ctx->frame_length_in_lut[1] - ctx->frame_length_in_lut[0];
			} else
				DRV_LOGE(ctx, "pls check delay_frame value!\n");
			break;
		default:
			break;
		}
		memcpy(ctx->frame_length_in_lut_rg, ctx->frame_length_in_lut,
			sizeof(ctx->frame_length_in_lut_rg));
	}

	/* MCSS low power mode update para */
	if (ctx->s_ctx.mcss_update_subdrv_para != NULL)
		ctx->s_ctx.mcss_update_subdrv_para((void *) ctx, scenario_id);
}

/* This function is used for updating the sensor mode info when seamless_switch
 * Unlike `update_mode_info`, `current_fps` will continue from the previous mode.
 */
void update_mode_info_seamless_switch(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id)
{
	u32 linetime_in_ns = 0;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return;
	}
	ctx->current_scenario_id = scenario_id;
	ctx->pclk = ctx->s_ctx.mode[scenario_id].pclk;
	ctx->line_length = ctx->s_ctx.mode[scenario_id].linelength;

	if (ctx->s_ctx.cust_get_linetime_in_ns != NULL) {
		ctx->s_ctx.cust_get_linetime_in_ns((void *) ctx,
			scenario_id, &linetime_in_ns, 0, 0);
	}
	if (ctx->current_fps <= ctx->s_ctx.mode[scenario_id].max_framerate) {
		//ctx->frame_length = 1 / ctx->current_fps  * 10 * ctx->pclk / ctx->line_length;
		ctx->frame_length = linetime_in_ns
				? 1000000000 / linetime_in_ns / (ctx->current_fps / 10)
				:ctx->pclk / ctx->line_length / (ctx->current_fps / 10);
		ctx->frame_length_rg = ctx->frame_length;
		DRV_LOG_MUST(ctx,
			"sid:%u, keep previous fps:%u, frame_length:%u, linetime_in_ns:%u, pclk:%llu, line_length:%u\n",
			scenario_id,  ctx->current_fps,
			ctx->frame_length, linetime_in_ns, ctx->pclk, ctx->line_length);
	} else {
		ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;
		ctx->frame_length_rg = ctx->frame_length;
		ctx->current_fps = linetime_in_ns
				?1000000000 / linetime_in_ns * 10 / ctx->frame_length
				:ctx->pclk / ctx->line_length * 10 / ctx->frame_length;
		DRV_LOG_MUST(ctx,
				"sid:%u, use max fps:%u, frame_length:%u, linetime_in_ns:%u, pclk:%llu, line_length:%u\n",
				scenario_id,  ctx->current_fps,
				ctx->frame_length, linetime_in_ns, ctx->pclk, ctx->line_length);
	}

	ctx->readout_length = ctx->s_ctx.mode[scenario_id].readout_length;
	ctx->read_margin = ctx->s_ctx.mode[scenario_id].read_margin;
	ctx->min_frame_length = ctx->frame_length;
	ctx->margin = ctx->s_ctx.mode[scenario_id].exposure_margin;
	ctx->autoflicker_en = FALSE;
	ctx->l_shift = 0;
	ctx->min_vblanking_line = ctx->s_ctx.mode[scenario_id].min_vblanking_line;
	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_LBMF) {
		memset(ctx->frame_length_in_lut, 0,
			sizeof(ctx->frame_length_in_lut));

		switch (ctx->s_ctx.mode[scenario_id].exp_cnt) {
		case 2:
			ctx->frame_length_in_lut[0] = ctx->readout_length + ctx->read_margin;
			ctx->frame_length_in_lut[1] = ctx->frame_length -
				ctx->frame_length_in_lut[0];
			break;
		case 3:
			ctx->frame_length_in_lut[0] = ctx->readout_length + ctx->read_margin;
			ctx->frame_length_in_lut[1] = ctx->readout_length + ctx->read_margin;
			ctx->frame_length_in_lut[2] = ctx->frame_length -
				ctx->frame_length_in_lut[1] - ctx->frame_length_in_lut[0];
			break;
		default:
			break;
		}
		memcpy(ctx->frame_length_in_lut_rg, ctx->frame_length_in_lut,
			sizeof(ctx->frame_length_in_lut_rg));
	}

	/* MCSS low power mode update para */
	if (ctx->s_ctx.mcss_update_subdrv_para != NULL)
		ctx->s_ctx.mcss_update_subdrv_para((void *) ctx, scenario_id);
}

bool check_is_no_crop(struct subdrv_ctx *ctx, enum SENSOR_SCENARIO_ID_ENUM scenario_id)
{
	struct SENSOR_WINSIZE_INFO_STRUCT *pinfo;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return false;
	}
	pinfo = &(ctx->s_ctx.mode[scenario_id].imgsensor_winsize_info);
	return (pinfo->w0_size == pinfo->scale_w && pinfo->h0_size == pinfo->scale_h);
}

int common_control(struct subdrv_ctx *ctx,
			enum SENSOR_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	int ret = ERROR_NONE;
	u16 idx = 0;
	u8 support = FALSE;
	u8 *pbuf = NULL;
	u16 size = 0;
	u16 addr = 0;
	u64 time_boot_begin = 0;
	u64 ixc_time = 0;
	u32 fast_mode_in_lbmf = 0;
	struct eeprom_info_struct *info = ctx->s_ctx.eeprom_info;
	struct adaptor_ctx *_adaptor_ctx = NULL;
	struct v4l2_subdev *sd = NULL;

	if (ctx->i2c_client)
		sd = i2c_get_clientdata(ctx->i2c_client);
	if (ctx->ixc_client.protocol)
		sd = adaptor_ixc_get_clientdata(&ctx->ixc_client);
	if (sd)
		_adaptor_ctx = to_ctx(sd);
	if (!_adaptor_ctx) {
		DRV_LOGE(ctx, "null _adaptor_ctx\n");
		return -ENODEV;
	}

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
		ret = ERROR_INVALID_SCENARIO_ID;
	}
	if (ctx->s_ctx.chk_s_off_sta)
		check_stream_off(ctx);
	update_mode_info(ctx, scenario_id);

	/* no mode setting due to scp sensor power on */
	if (ctx->s_ctx.aov_sensor_support &&
		ctx->s_ctx.mode[scenario_id].aov_mode &&
		_adaptor_ctx->aov_scp_pwr) {
		DRV_LOG_MUST(ctx, "no mode setting due to scp sensor power on\n");
		return ret;
	}

	if (ctx->s_ctx.mode[scenario_id].mode_setting_table != NULL) {
		DRV_LOG(ctx, "E: sid:%u size:%u\n", scenario_id,
			ctx->s_ctx.mode[scenario_id].mode_setting_len);
		if ((ctx->power_on_profile_en != NULL) &&
			(*ctx->power_on_profile_en))
			time_boot_begin = ktime_get_boottime_ns();

		/* initail setting */
		if (ctx->s_ctx.aov_sensor_support) {
			if (ctx->s_ctx.mode[scenario_id].aov_mode &&
				ctx->s_ctx.s_pwr_seq_reset_view_to_sensing != NULL)
				ctx->s_ctx.s_pwr_seq_reset_view_to_sensing((void *) ctx);

			if (!ctx->s_ctx.init_in_open)
				sensor_init(ctx);
		}
		switch (ctx->sensor_mode_ops) {
		case AOV_MODE_CTRL_OPS_SENSING_CTRL:
		default:
			ixc_time = ixc_table_write(ctx, ctx->s_ctx.mode[scenario_id].mode_setting_table,
				ctx->s_ctx.mode[scenario_id].mode_setting_len);
			break;
		case AOV_MODE_CTRL_OPS_MONTION_DETECTION_CTRL:
			/* set eint gpio */
			ret = pinctrl_select_state(
				_adaptor_ctx->pinctrl,
				_adaptor_ctx->state[STATE_EINT]);
			if (ret < 0)
				DRV_LOGE(ctx,
					"select(%s)(fail),ret(%d)\n",
					state_names[STATE_EINT], ret);
			else
				DRV_LOG(ctx, "select(%s)(correct)\n", state_names[STATE_EINT]);

			ixc_time = ixc_table_write(ctx,
				ctx->s_ctx.mode[scenario_id].mode_setting_table_for_md,
				ctx->s_ctx.mode[scenario_id].mode_setting_len_for_md);
			break;
		}

		if ((ctx->power_on_profile_en != NULL) &&
			(*ctx->power_on_profile_en)) {
			ctx->sensor_pw_on_profile.i2c_cfg_period =
					ktime_get_boottime_ns() - time_boot_begin;

			ctx->sensor_pw_on_profile.i2c_cfg_table_len =
					ctx->s_ctx.mode[scenario_id].mode_setting_len;
		}
		DRV_LOG_MUST(ctx, "X: sid:%u size:%u, ixc_time(us): %lld\n", scenario_id,
			ctx->s_ctx.mode[scenario_id].mode_setting_len,
			ixc_time);
	} else {
		DRV_LOGE(ctx, "please implement mode setting(sid:%u)!\n", scenario_id);
	}

	if (check_is_no_crop(ctx, scenario_id) && probe_eeprom(ctx)) {
		idx = ctx->eeprom_index;
		support = info[idx].xtalk_support;
		pbuf = info[idx].preload_xtalk_table;
		size = info[idx].xtalk_size;
		addr = info[idx].sensor_reg_addr_xtalk;
		if (support) {
			if (pbuf != NULL && addr > 0 && size > 0) {
				subdrv_ixc_wr_seq_p8(ctx, addr, pbuf, size);
				DRV_LOG(ctx, "set XTALK calibration data done.");
			}
		}
	}

	if (ctx->s_ctx.aov_sensor_support &&
		ctx->s_ctx.s_data_rate_global_timing_phy_ctrl != NULL)
		ctx->s_ctx.s_data_rate_global_timing_phy_ctrl((void *) ctx);

	set_mirror_flip(ctx, ctx->s_ctx.mirror);

	if (ctx->s_ctx.reg_addr_fast_mode_in_lbmf &&
		(ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_LBMF ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_RAW_VS ||
		ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE_VS)) {
		// enable bit[2] on lbmf mode
		fast_mode_in_lbmf =
			subdrv_ixc_rd_u8(ctx, ctx->s_ctx.reg_addr_fast_mode_in_lbmf) | 0x4;
		subdrv_ixc_wr_u8(ctx,
			ctx->s_ctx.reg_addr_fast_mode_in_lbmf,
			fast_mode_in_lbmf);
	}

	return ret;
}

int common_feature_control(struct subdrv_ctx *ctx, MSDK_SENSOR_FEATURE_ENUM feature_id,
				u8 *feature_para, u32 *feature_para_len)
{
	u16 *feature_data_16 = (u16 *) feature_para;
	u32 *feature_data_32 = (u32 *) feature_para;
	u64 *feature_data = (u64 *) feature_para;
	int i = 0;
	int ret = ERROR_NONE;

	if (ctx->s_ctx.list != NULL) {
		for (i = 0; i < ctx->s_ctx.list_len; i++)
			if (ctx->s_ctx.list[i].feature_id == feature_id &&
				ctx->s_ctx.list[i].func != NULL) {
				ret = ctx->s_ctx.list[i].func(ctx, feature_para, feature_para_len);
				return ret;
			}
	}
	switch (feature_id) {
	case SENSOR_FEATURE_GET_OUTPUT_FORMAT_BY_SCENARIO:
		get_output_format_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			feature_data + 1);
		break;
	case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
		get_ana_gain_table(ctx, feature_data,
			(void *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		// DONE: DCG-VS by another control. not modify this
		get_gain_range_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			feature_data + 1, feature_data + 2);
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		// DONE: DCG-VS by another control. not modify this
		get_base_gain_iso_and_step(ctx,
			feature_data, feature_data + 1,	feature_data + 2);
		break;
	case SENSOR_FEATURE_GET_DIG_GAIN_RANGE_BY_SCENARIO:
		// DONE: DCG-VS by another control. not modify this
		get_dig_gain_range_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			feature_data + 1, feature_data + 2);
		break;
	case SENSOR_FEATURE_GET_DIG_GAIN_STEP:
		// DONE: DCG-VS by another control. not modify this
		get_dig_gain_step(ctx, feature_data);
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		// DONE: DCG-VS by another control. not modify this
		get_min_shutter_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			feature_data + 1, feature_data + 2);
		break;
	case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE: // what is + 0 ?
		get_offset_to_start_of_exposure(ctx,
			(u32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
		// DONE: DCG-VS by another control. not modify this
		get_pixel_clock_freq_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			(u64 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
		// DONE: DCG-VS by another control. not modify this
		get_period_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			(u32 *)(uintptr_t)(*(feature_data + 1)),
			*(feature_data + 2));
		break;
	case SENSOR_FEATURE_GET_PERIOD:
		// DONE: DCG-VS by another control. not modify this
		get_period(ctx,	feature_data_16, feature_data_16 + 1);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		// DONE: DCG-VS by another control. not modify this
		get_pixel_clock_freq(ctx, feature_data);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		if (ctx->s_ctx.aov_sensor_support &&
			ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode &&
			(ctx->s_ctx.mode[ctx->current_scenario_id].ae_ctrl_support !=
				IMGSENSOR_AE_CONTROL_SUPPORT_VIEWING_MODE))
			DRV_LOG_MUST(ctx,
				"AOV mode not support ae shutter control!\n");
		else
			set_shutter(ctx, *feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		if (ctx->s_ctx.aov_sensor_support &&
			ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode &&
			(ctx->s_ctx.mode[ctx->current_scenario_id].ae_ctrl_support !=
				IMGSENSOR_AE_CONTROL_SUPPORT_VIEWING_MODE))
			DRV_LOG_MUST(ctx,
				"AOV mode not support ae gain control!\n");
		else {
			set_gain(ctx, *feature_data);
			commit_i2c_buffer(ctx);
		}
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		subdrv_ixc_wr_u8(ctx,
			((MSDK_SENSOR_REG_INFO_STRUCT *)feature_para)->RegAddr,
			((MSDK_SENSOR_REG_INFO_STRUCT *)feature_para)->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		((MSDK_SENSOR_REG_INFO_STRUCT *)feature_para)->RegData =
		subdrv_ixc_rd_u8(ctx, ((MSDK_SENSOR_REG_INFO_STRUCT *)feature_para)->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		get_lens_driver_id(ctx,	feature_data_32);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(ctx, *feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		ret = common_get_imgsensor_id(ctx, feature_data_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode(ctx,
			*feature_data_16, *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		// DONE: DCG-VS modify in set_max_framerate_by_scenario
		set_max_framerate_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*feature_data,
			*(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*feature_data,
			(u32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_FINE_INTEG_LINE_BY_SCENARIO:
		// DONE: DCG-VS by another control. not modify this
		get_fine_integ_line_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*feature_data,
			(int *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern(ctx, *feature_data);
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN_DATA:
		set_test_pattern_data(ctx,
			(struct mtk_test_pattern_data *)feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		get_test_pattern_checksum_value(ctx,
			feature_data_32);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		set_framerate(ctx, *feature_data_32);
		break;
	case SENSOR_FEATURE_SET_HDR:
		set_hdr(ctx, *feature_data_32);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		get_crop_info(ctx, *feature_data_32,
			(struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_PDAF_INFO:
		get_pdaf_info(ctx, *feature_data_32,
			(struct SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		get_sensor_pdaf_capacity(ctx, *feature_data_32,
			(u32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_SEAMLESS_EXTEND_FRAME_LENGTH:
		extend_frame_length(ctx, (u32) *feature_data);
		break;
	case SENSOR_FEATURE_SEAMLESS_SWITCH:
		if ((feature_data + 1) != NULL)
			seamless_switch(ctx, (*feature_data),
				(u64 *)((uintptr_t)(*(feature_data + 1))));
		else {
			DRV_LOGE(ctx, "no ae_ctrl input\n");
			seamless_switch(ctx, (*feature_data), NULL);
		}
		break;
	case SENSOR_FEATURE_GET_SEAMLESS_SCENARIOS:
		if ((feature_data + 1) != NULL)
			get_seamless_scenarios(ctx, *feature_data,
				(u32 *)((uintptr_t)(*(feature_data + 1))));
		else {
			DRV_LOGE(ctx, "input pScenarios vector is NULL!\n");
			return ERROR_INVALID_SCENARIO_ID;
		}
		break;
	case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		get_sensor_hdr_capacity(ctx, *feature_data_32,
			(u32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
		// DONE: DCG-VS return by new API
		*(feature_data + 1) = 1;
		get_frame_ctrl_info_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*feature_data,
			(u32 *)(feature_data + 2));
		break;
	case SENSOR_FEATURE_GET_4CELL_DATA:
		get_feature_get_4cell_data(ctx, (u16)(*feature_data),
			(char *)(uintptr_t)(*(feature_data + 1)), (u16)(*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_MAX_EXP_LINE:
	case SENSOR_FEATURE_GET_STAGGER_MAX_EXP_TIME:
		get_stagger_max_exp_time(ctx, *feature_data,
			*(feature_data + 1), (feature_data + 2));
		break;
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		// DONE: DCG-VS modify in set_hdr_tri_shutter
		if (ctx->s_ctx.aov_sensor_support &&
			ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode &&
			(ctx->s_ctx.mode[ctx->current_scenario_id].ae_ctrl_support !=
				IMGSENSOR_AE_CONTROL_SUPPORT_VIEWING_MODE))
			DRV_LOG_MUST(ctx,
				"AOV mode not support ae shutter control!\n");
		else
			set_hdr_tri_shutter(ctx, feature_data, 2);
		break;
	case SENSOR_FEATURE_SET_DUAL_GAIN:
		// DONE: DCG-VS modify in set_hdr_tri_gain
		if (ctx->s_ctx.aov_sensor_support &&
			ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode &&
			(ctx->s_ctx.mode[ctx->current_scenario_id].ae_ctrl_support !=
				IMGSENSOR_AE_CONTROL_SUPPORT_VIEWING_MODE))
			DRV_LOG_MUST(ctx,
				"AOV mode not support ae gain control!\n");
		else {
			set_hdr_tri_gain(ctx, feature_data, 2);
			commit_i2c_buffer(ctx);
		}
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_SHUTTER:
		// DONE: DCG-VS modify in set_hdr_tri_shutter
		if (ctx->s_ctx.aov_sensor_support &&
			ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode &&
			(ctx->s_ctx.mode[ctx->current_scenario_id].ae_ctrl_support !=
				IMGSENSOR_AE_CONTROL_SUPPORT_VIEWING_MODE))
			DRV_LOG_MUST(ctx,
				"AOV mode not support ae shutter control!\n");
		else
			set_hdr_tri_shutter(ctx, feature_data, 3);
		break;
	case SENSOR_FEATURE_SET_HDR_TRI_GAIN:
		// DONE: DCG-VS modify in set_hdr_tri_gain
		if (ctx->s_ctx.aov_sensor_support &&
			ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode &&
			(ctx->s_ctx.mode[ctx->current_scenario_id].ae_ctrl_support !=
				IMGSENSOR_AE_CONTROL_SUPPORT_VIEWING_MODE))
			DRV_LOG_MUST(ctx,
				"AOV mode not support ae gain control!\n");
		else {
			set_hdr_tri_gain(ctx, feature_data, 3);
			commit_i2c_buffer(ctx);
		}
		break;
	case SENSOR_FEATURE_SET_MULTI_DIG_GAIN:
		if (ctx->s_ctx.aov_sensor_support &&
			ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode &&
			(ctx->s_ctx.mode[ctx->current_scenario_id].ae_ctrl_support !=
				IMGSENSOR_AE_CONTROL_SUPPORT_VIEWING_MODE))
			DRV_LOG_MUST(ctx,
				"AOV mode not support ae gain control!\n");
		else
			set_multi_dig_gain(
				ctx, (u32 *)(*feature_data), (u16) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		get_temperature_value(ctx, feature_data_32);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PDAF_REG_SETTING:
		get_pdaf_reg_setting(ctx, (*feature_para_len)/sizeof(u32), feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF_REG_SETTING:
		set_pdaf_reg_setting(ctx, (*feature_para_len)/sizeof(u32), feature_data_16);
		break;
	case SENSOR_FEATURE_SET_PDAF:
		set_pdaf(ctx, *feature_data_16);
		break;
	case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
		// DONE: DCG-VS will not called this function
		set_shutter_frame_length(ctx,
			(u64) (*feature_data),
			(u32) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		break;
	case SENSOR_FEATURE_SET_AWB_GAIN:
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		streaming_control(ctx, FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data) {
			if (ctx->s_ctx.aov_sensor_support &&
				ctx->s_ctx.mode[ctx->current_scenario_id].aov_mode &&
				(ctx->s_ctx.mode[ctx->current_scenario_id].ae_ctrl_support !=
					IMGSENSOR_AE_CONTROL_SUPPORT_VIEWING_MODE))
				DRV_LOG_MUST(ctx,
					"AOV mode not support ae shutter control!\n");
			else
				set_shutter(ctx, *feature_data);
		}
		streaming_control(ctx, TRUE);
		break;
	case SENSOR_FEATURE_GET_BINNING_TYPE:
		get_binning_type(ctx, *(feature_data + 1), feature_data_32);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
		get_ae_frame_mode_for_le(ctx, feature_data_32);
		break;
	case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
		get_ae_effective_frame_for_le(ctx, feature_data_32);
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
		get_mipi_pixel_rate(ctx, *feature_data,
			(u32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_PRELOAD_EEPROM_DATA:
		preload_eeprom_data(ctx, feature_data_32);
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMELENGTH:
		set_frame_length(ctx, (u16) (*feature_data));
		break;
	case SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME:
		// DONE: DCG-VS not use this control, use set_multi_shutter_frame_length_in_lut instead
		set_multi_shutter_frame_length(ctx, (u64 *)(*feature_data),
					(u16) (*(feature_data + 1)),
					(u16) (*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_SENSOR_RGBW_OUTPUT_MODE:
		get_sensor_rgbw_output_mode(ctx, *feature_data_32,
			(u32 *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_READOUT_BY_SCENARIO:
		// DONE: DCG-VS return by new API
		get_readout_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*feature_data,
			feature_data + 1);
		break;
	case SENSOR_FEATURE_GET_EXPOSURE_COUNT_BY_SCENARIO:
		get_exposure_count_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*feature_data,
			(u32 *)(feature_data + 1));
		break;
	case SENSOR_FEATURE_SET_FRAMELENGTH_IN_LUT:
		/* DCG-VS done */
		set_lut_multi_frame_length(ctx,
			(u32) (*feature_data),
			(u32 *) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_SET_MULTI_SHUTTER_FRAME_TIME_IN_LUT:
		/* DCG-VS done */
		set_lut_multi_shutter_frame_length(ctx,
			(u64 *)(*feature_data),
			(u16) (*(feature_data + 1)),
			(u32) (*(feature_data + 2)),
			(u32 *) (*(feature_data + 3)));
		break;
	case SENSOR_FEATURE_GET_DCG_GAIN_RATIO_TABLE_BY_SCENARIO:
		get_dcg_gain_ratio_table_by_scenario(ctx,
		(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
		feature_data+1,
			(void *)(uintptr_t)(*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_DCG_GAIN_RATIO_RANGE_BY_SCENARIO:
		get_dcg_gain_ratio_range_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			feature_data + 1, feature_data + 2);
		break;
	case SENSOR_FEATURE_GET_DCG_TYPE_BY_SCENARIO:
		get_dcg_type_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			feature_data + 1, feature_data + 2);
		break;
	case SENSOR_FEATURE_SET_AOV_CSI_CLK:
		switch (*feature_data_32) {
		case 130:
			ctx->aov_csi_clk = 130;
			break;
		case 242:
			ctx->aov_csi_clk = 242;
			break;
		case 312:
			ctx->aov_csi_clk = 312;
			break;
		default:
			DRV_LOGE(ctx,
				"plz assign csi clk when running aov!\n");
			break;
		}
		DRV_LOG_MUST(ctx,
			"[%s] SENSOR_FEATURE_SET_AOV_CSI_CLK(%u)\n",
			__func__, *feature_data_32);
		break;
	case SENSOR_FEATURE_GET_MULTI_EXP_GAIN_RANGE_BY_SCENARIO:
		// DONE: DCG-VS by another control. not modify this
		get_multi_exp_gain_range_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			feature_data + 1, (void *)(uintptr_t)(*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_MULTI_EXP_SHUTTER_RANGE_BY_SCENARIO:
		// DONE: DCG-VS by another control. not modify this
		get_multi_exp_shutter_range_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			feature_data + 1, (void *)(uintptr_t)(*(feature_data + 2)));
		break;
	case SENSOR_FEATURE_GET_FRAME_CNT:
		get_sensor_frame_count(ctx, (u32 *) feature_data);
		break;
	case SENSOR_FEATURE_GET_EXP_LINE_BY_SCENARIO:
		// DONE: DCG-VS by new API
		get_exp_line_by_scenario(ctx, *feature_data,
			*(feature_data + 1), (feature_data + 2));
		break;
	case SENSOR_FEATURE_UPDATE_HW_INIT_TIME:
		update_hw_init_time(ctx, *(feature_data));
		break;
	case SENSOR_FEATURE_GET_DCG_RATIO_GROUP_BY_SCENARIO:
		get_dcg_ratio_group_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			(u32 *)((uintptr_t)(*(feature_data + 1))));
		break;
	case SENSOR_FEATURE_GET_FRAME_DESC_TO_VC_INFO:
		get_frame_desc_to_vc_info(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			(struct SENSOR_VC_INFO2_STRUCT *)((uintptr_t)(*(feature_data + 1))));
		break;
	case SENSOR_FEATURE_GET_MULTI_EXP_STATIC_INFO_BY_SCENARIO:
		get_multi_exp_static_info_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			(void *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_DCG_VSL_INFO:
		get_dcg_vsl_info_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			(void *)(uintptr_t)(*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_STAGGER_MIN_VB:
		get_stagger_min_vb_by_scenario(ctx,
			(enum SENSOR_SCENARIO_ID_ENUM)*(feature_data),
			feature_data + 1);
		break;
	default:
		DRV_LOG(ctx, "feature_id %u is invalid\n", feature_id);
		ret = ERROR_INVALID_FEATURE_ID;
		break;
	}
	return ret;
}

int common_close(struct subdrv_ctx *ctx)
{
	streaming_control(ctx, FALSE);
	DRV_LOG(ctx, "subdrv closed\n");
	return ERROR_NONE;
}

int common_get_frame_desc(struct subdrv_ctx *ctx,
		int scenario_id, struct mtk_mbus_frame_desc *fd)
{
	int ret = 0;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid scenario_id(%u)\n", scenario_id);
		return -1;
	}

	if (ctx->s_ctx.mode[scenario_id].frame_desc != NULL) {
		fd->type = MTK_MBUS_FRAME_DESC_TYPE_CSI2;
		fd->num_entries = ctx->s_ctx.mode[scenario_id].num_entries;
		memcpy(fd->entry, ctx->s_ctx.mode[scenario_id].frame_desc,
			sizeof(struct mtk_mbus_frame_desc_entry)*fd->num_entries);
	} else
		ret = -1;
	return ret;
}

int common_get_temp(struct subdrv_ctx *ctx, int *temp)
{
	if (!ctx->s_ctx.temperature_support) {
		DRV_LOGE(ctx, "temperature sensor no support\n");
		return -1; // Need confirm if -1.
	}
	if (ctx->s_ctx.g_temp == NULL) {
		DRV_LOGE(ctx, "please implement g_temp function\n");
		return -1; // Need confirm if -1.
	}
	*temp = ctx->s_ctx.g_temp((void *)ctx) * 1000;
	return 0;
}

int common_get_csi_param(struct subdrv_ctx *ctx,
	enum SENSOR_SCENARIO_ID_ENUM scenario_id,
	struct mtk_csi_param *csi_param)
{
	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return 0;
	}
	memcpy(csi_param, &(ctx->s_ctx.mode[scenario_id].csi_param),
		sizeof(struct mtk_csi_param));
	return 0;
}

static int check_aeb_ae_ctrl_cnt_if_delay(struct subdrv_ctx *ctx)
{
	int i = 0;
	u64 aeb_ae_ctrl_cnt = 0;
	u64 aeb_ae_ctrl_cnt_last_orin = 0;
	u64 aeb_ae_ctrl_cnt_last_update = 0;
	u32 frame_length_orin = 0;
	u32 frame_length_update = 0;
	u32 frame_length_rg_orin = 0;
	u32 frame_length_rg_update = 0;
	u32 frame_length_in_lut[IMGSENSOR_STAGGER_EXPOSURE_CNT] = {0};

	check_current_scenario_id_bound(ctx);

	if (ctx->s_ctx.mode[ctx->current_scenario_id].hdr_mode != HDR_RAW_LBMF)
		return 0;

	if (ctx->s_ctx.mode[ctx->current_scenario_id].delay_frame != 2)
		return 0;

	if (ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt > IMGSENSOR_STAGGER_EXPOSURE_CNT) {
		DRV_LOGE(ctx,
			"invalid exp_cnt:%u\n",
			ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt);
		return 0;
	}

	spin_lock(&ctx->aeb_ae_ctrl_cnt_lock);
	/* update local ae ctrl cnt (before) */
	aeb_ae_ctrl_cnt = ctx->aeb_ae_ctrl_cnt;
	aeb_ae_ctrl_cnt_last_orin = ctx->aeb_ae_ctrl_cnt_last;
	/* update local framelength (before) */
	frame_length_orin = ctx->frame_length;
	frame_length_rg_orin = ctx->frame_length_rg;
	/*
	 * abnormal case: when ae ctrl delay or not do ae ctrl
	 * ctx framelength will keep same as lut[0]+lut[1] if no aeb_ae_ctrl
	 */
	if (ctx->aeb_ae_ctrl_cnt_last == ctx->aeb_ae_ctrl_cnt) {
		frame_length_update = 0;
		for (i = 0; i < ctx->s_ctx.mode[ctx->current_scenario_id].exp_cnt; i++) {
			frame_length_in_lut[i] = ctx->frame_length_in_lut[i];
			frame_length_update += ctx->frame_length_in_lut[i];
		}
		/* update ctx framelength & _rg */
		ctx->frame_length = frame_length_update;
		ctx->frame_length_rg = ctx->frame_length;
	}
	/* update ctx ae ctrl cnt last */
	ctx->aeb_ae_ctrl_cnt_last = ctx->aeb_ae_ctrl_cnt;
	/* update local ae ctrl cnt last (after) */
	aeb_ae_ctrl_cnt_last_update = ctx->aeb_ae_ctrl_cnt_last;
	/* update local framelength & _rg (after) */
	frame_length_update = ctx->frame_length;
	frame_length_rg_update = ctx->frame_length_rg;
	spin_unlock(&ctx->aeb_ae_ctrl_cnt_lock);

	DRV_LOG(ctx,
		"aeb_ae_ctrl_cnt/last_orin/last_update:%llu/%llu/%llu,fll(orin/update/a/b/c/d/e):%u/%u/%u/%u/%u/%u/%u,fll_rg(orin/update):%u/%u\n",
		aeb_ae_ctrl_cnt,
		aeb_ae_ctrl_cnt_last_orin,
		aeb_ae_ctrl_cnt_last_update,
		frame_length_orin,
		frame_length_update,
		frame_length_in_lut[0],
		frame_length_in_lut[1],
		frame_length_in_lut[2],
		frame_length_in_lut[3],
		frame_length_in_lut[4],
		frame_length_rg_orin,
		frame_length_rg_update);

	return 0;
}

int common_update_sof_cnt(struct subdrv_ctx *ctx, u32 sof_cnt)
{
	DRV_LOG(ctx, "update ctx->sof_cnt(%u)", sof_cnt);
	ctx->sof_cnt = sof_cnt;
	ctx->sof_no++;

	check_aeb_ae_ctrl_cnt_if_delay(ctx);

	return 0;
}

static int read_mipi_raw8(char *buf, size_t buf_sz, int base_ofs, int pix,
			  u8 parsing_type, u32 *val)
{
	int ofs;
	int p = pix - 1;
	int b = 0;

	switch (parsing_type) {
	case MTK_EBD_PARSING_TYPE_MIPI_RAW10:
		/* 5 bytes for 4 pixels */
		b = ((p >> 2) * 5) + (p % 4);
		break;
	case MTK_EBD_PARSING_TYPE_MIPI_RAW12:
		/* 3 bytes for 2 pixels */
		b = ((p >> 1) * 3) + (p % 2);
		break;
	case MTK_EBD_PARSING_TYPE_MIPI_RAW14:
		/* 7 bytes for 4 pixels */
		b = ((p >> 2) * 7) + (p % 4);
		break;
	case MTK_EBD_PARSING_TYPE_MIPI_RAW8:
	default:
		/* 1 byte for 1 pixel */
		b = p;
		break;
	}

	if (val) {
		ofs = base_ofs + b;
		if (ofs < buf_sz) {
			*val = buf[ofs];
			return 0;
		}
	}

	return -EINVAL;
}

static int read_mipi_raw10(char *buf, size_t buf_sz, int base_ofs, int pix,
			   u8 parsing_type, u32 *val)
{
	int ofs1, ofs2, ls, hs, mask;
	int p = pix - 1;

	/*
	 * pix1 = (byte[0] >> 0) | ((byte[1] & 0x03) << 8)
	 * pix2 = (byte[1] >> 2) | ((byte[2] & 0x0f) << 6)
	 * pix3 = (byte[2] >> 4) | ((byte[3] & 0x3f) << 4)
	 * pix4 = (byte[3] >> 6) | ((byte[4] & 0xff) << 2)
	 * pix5 = (byte[5] >> 0) | ((byte[6] & 0x03) << 8)
	 * -> general:
	 * pix{pix} = (byte[ofs1] >> ls) | ((byte[ofs2] & mask) << hs)
	 */
	if (val) {
		switch (p % 4) {
		case 0:
			ls = 0;
			mask = 0x03;
			break;
		case 1:
			ls = 2;
			mask = 0x0f;
			break;
		case 2:
			ls = 4;
			mask = 0x3f;
			break;
		case 3:
			ls = 6;
			mask = 0xff;
			break;
		default:
			// never here
			return -EINVAL;
		}
		hs = 8 - ls;

		ofs1 = base_ofs + (p >> 2) + p;
		ofs2 = ofs1 + 1;

		if ((ofs1 < buf_sz) && (ofs2 < buf_sz)) {
			*val = (buf[ofs1] >> ls) | ((buf[ofs2] & mask) << hs);
			// truncate bit 0 ~ 1 to convert to 8-bit
			*val = (*val) >> 2;
			return 0;
		}
	}

	return -EINVAL;
}

static int read_mipi_raw12(char *buf, size_t buf_sz, int base_ofs, int pix,
			   u8 parsing_type, u32 *val)
{
	int ofs1, ofs2, ls, hs, mask;
	int p = pix - 1;

	/*
	 * pix1 = (byte[0] >> 0) | ((byte[1] & 0x0f) << 8)
	 * pix2 = (byte[1] >> 4) | ((byte[2] & 0xff) << 4)
	 * pix3 = (byte[3] >> 0) | ((byte[4] & 0x0f) << 8)
	 * -> general:
	 * pix{pix} = (byte[ofs1] >> ls) | ((byte[ofs2] & mask) << hs)
	 */
	if (val) {
		switch (p % 2) {
		case 0:
			ls = 0;
			mask = 0x0f;
			break;
		case 1:
			ls = 4;
			mask = 0xff;
			break;
		default:
			// never here
			return -EINVAL;
		}
		hs = 8 - ls;

		ofs1 = base_ofs + (p >> 1) + p;
		ofs2 = ofs1 + 1;

		if ((ofs1 < buf_sz) && (ofs2 < buf_sz)) {
			*val = (buf[ofs1] >> ls) | ((buf[ofs2] & mask) << hs);
			// truncate bit 0 ~ 3 to convert to 8-bit
			*val = (*val) >> 4;
			return 0;
		}
	}

	return -EINVAL;
}

static int read_mipi_raw14(char *buf, size_t buf_sz, int base_ofs, int pix,
			   u8 parsing_type, u32 *val)
{
	int ofs1, ofs2, ofs3, ls, hs2, hs3, mask2, mask3;
	int p = pix - 1;

	/*
	 * pix1 = (byte[0] >> 0) | ((byte[1] & 0x3f) << 8)
	 * pix2 = (byte[1] >> 6) | ((byte[2] & 0xff) << 2) | ((byte[3] & 0x0f) << 10)
	 * pix3 = (byte[3] >> 4) | ((byte[4] & 0xff) << 4) | ((byte[5] & 0x03) << 12)
	 * pix4 = (byte[5] >> 2) | ((byte[6] & 0xff) << 6)
	 * pix5 = (byte[7] >> 0) | ((byte[8] & 0x3f) << 8)
	 * -> general:
	 * pix{pix} = (byte[ofs1] >> ls) | ((byte[ofs2] & mask2) << hs2)
	 *             | ((byte[ofs3] & mask3) << hs3)
	 */
	if (val) {
		switch (p % 4) {
		case 0:
			ofs1 = (p >> 2) * 7;
			ls = 0;
			hs2 = 8;
			hs3 = 0;
			mask2 = 0x3f;
			mask3 = 0x00;
			break;
		case 1:
			ofs1 = (p >> 2) * 7 + 1;
			ls = 6;
			hs2 = 2;
			hs3 = 10;
			mask2 = 0xff;
			mask3 = 0x0f;
			break;
		case 2:
			ofs1 = (p >> 2) * 7 + 3;
			ls = 4;
			hs2 = 4;
			hs3 = 12;
			mask2 = 0xff;
			mask3 = 0x03;
			break;
		case 3:
			ofs1 = (p >> 2) * 7 + 5;
			ls = 2;
			hs2 = 6;
			hs3 = 0;
			mask2 = 0xff;
			mask3 = 0x00;
			break;
		default:
			// never here
			return -EINVAL;
		}

		ofs1 += base_ofs;
		ofs2 = ofs1 + 1;
		ofs3 = ofs2 + 1;

		if ((ofs1 < buf_sz) && (ofs2 < buf_sz)) {
			*val = (buf[ofs1] >> ls) | ((buf[ofs2] & mask2) << hs2)
				| ((buf[ofs3] & mask3) << hs3);
			// truncate bit 0 ~ 5 to convert to 8-bit
			*val = (*val) >> 6;
			return 0;
		}
	}

	return -EINVAL;
}

static u32 read_ebd(char *buf, size_t buf_sz, u32 stride, u32 mbus, u8 parsing_type,
		    struct ebd_loc *loc)
{
	u32 result = 0;
	u32 tmp = 0;
	int i, base_ofs;
	int (*read)(char *, size_t, int, int, u8, u32 *);

	if (!buf || !loc || (loc->loc_line <= 0))
		return 0;

	base_ofs = (loc->loc_line - 1) * stride;

	switch (mbus) {
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		read = read_mipi_raw10;
		break;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		read = read_mipi_raw12;
		break;
	case MEDIA_BUS_FMT_SBGGR14_1X14:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		read = read_mipi_raw14;
		break;
	default:
		read = read_mipi_raw8;
		break;
	}

	for (i = 0; i < ARRAY_SIZE(loc->loc_pix) && (loc->loc_pix[i] > 0); i++) {
		if (read(buf, buf_sz, base_ofs, loc->loc_pix[i], parsing_type, &tmp) >= 0)
			result = (result << 8) | tmp;
	}

	return result;
}

int common_parse_ebd_line(struct subdrv_ctx *ctx, struct mtk_recv_sensor_ebd_line *data,
	struct mtk_ebd_dump *obj)
{
	struct ebd_info_struct *ebd_info = &ctx->s_ctx.ebd_info;
	int i;

	if (!obj) {
		DRV_LOG_MUST(ctx, "obj is NULL\n");
		return -EINVAL;
	}

	if (data && data->buf) {
		obj->frm_cnt = read_ebd(data->buf, data->buf_sz, data->stride,
					data->mbus_code, data->ebd_parsing_type,
					&ebd_info->frm_cnt_loc);
		for (i = 0; i < IMGSENSOR_STAGGER_EXPOSURE_CNT; i++) {
			obj->cit[i] = read_ebd(data->buf, data->buf_sz, data->stride,
					data->mbus_code, data->ebd_parsing_type,
					&ebd_info->coarse_integ_loc[i]);
			obj->again[i] = read_ebd(data->buf, data->buf_sz, data->stride,
					data->mbus_code, data->ebd_parsing_type,
					&ebd_info->ana_gain_loc[i]);
			obj->dgain[i] = read_ebd(data->buf, data->buf_sz, data->stride,
					data->mbus_code, data->ebd_parsing_type,
					&ebd_info->dig_gain_loc[i]);
		}
		obj->cit_shift = read_ebd(data->buf, data->buf_sz, data->stride,
					data->mbus_code, data->ebd_parsing_type,
					&ebd_info->coarse_integ_shift_loc);
		obj->dol = read_ebd(data->buf, data->buf_sz, data->stride,
					data->mbus_code, data->ebd_parsing_type,
					&ebd_info->dol_loc);
		obj->fll = read_ebd(data->buf, data->buf_sz, data->stride,
					data->mbus_code, data->ebd_parsing_type,
					&ebd_info->framelength_loc);
		obj->temperature = read_ebd(data->buf, data->buf_sz, data->stride,
					data->mbus_code, data->ebd_parsing_type,
					&ebd_info->temperature_loc);

		DRV_LOG(ctx,
			"req_id/stride/bufsz/mbus/p_type(%u,%u,%u,0x%x,%u) frm_cnt/cit_sft/dol/fll/temp(%u/%u/0x%x/%u/0x%x) cit(%u,%u,%u,%u,%u) again(0x%x,0x%x,0x%x,0x%x,0x%x) dgain(0x%x,0x%x,0x%x,0x%x,0x%x)\n",
			data->req_id, data->stride, data->buf_sz,
			data->mbus_code, data->ebd_parsing_type,
			obj->frm_cnt, obj->cit_shift, obj->dol, obj->fll, obj->temperature,
			obj->cit[IMGSENSOR_STAGGER_EXPOSURE_LE],
			obj->cit[IMGSENSOR_STAGGER_EXPOSURE_ME],
			obj->cit[IMGSENSOR_STAGGER_EXPOSURE_SE],
			obj->cit[IMGSENSOR_STAGGER_EXPOSURE_SSE],
			obj->cit[IMGSENSOR_STAGGER_EXPOSURE_SSSE],
			obj->again[IMGSENSOR_STAGGER_EXPOSURE_LE],
			obj->again[IMGSENSOR_STAGGER_EXPOSURE_ME],
			obj->again[IMGSENSOR_STAGGER_EXPOSURE_SE],
			obj->again[IMGSENSOR_STAGGER_EXPOSURE_SSE],
			obj->again[IMGSENSOR_STAGGER_EXPOSURE_SSSE],
			obj->dgain[IMGSENSOR_STAGGER_EXPOSURE_LE],
			obj->dgain[IMGSENSOR_STAGGER_EXPOSURE_ME],
			obj->dgain[IMGSENSOR_STAGGER_EXPOSURE_SE],
			obj->dgain[IMGSENSOR_STAGGER_EXPOSURE_SSE],
			obj->dgain[IMGSENSOR_STAGGER_EXPOSURE_SSSE]);
	} else
		DRV_LOG_MUST(ctx, "buffer is null\n");

	return 0;
}

int common_get_pixel_clk_base_linetime_in_ns(void *arg,
	u32 scenario_id, u32 *linetime_in_ns, enum GET_LINETIME_ENUM linetime_type,
	enum IMGSENSOR_EXPOSURE exp_idx)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	unsigned long long pclk = 0;
	unsigned int linelength = 0;
	u8 lut_idx;

	if (!ctx) {
		return -EINVAL;
	}

	if (!linetime_in_ns) {
		DRV_LOGE(ctx, "linetime_in_ns is NULL\n");
		return -EINVAL;
	}

	pclk = ctx->s_ctx.mode[scenario_id].pclk;
	linelength = ctx->s_ctx.mode[scenario_id].linelength;

	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_DCG_RAW_VS ||
	    ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE_VS) {
		/* different exp has different linetime */
		/* need return by exp_idx */
		if (exp_idx >= ctx->s_ctx.mode[scenario_id].exp_cnt) {
			DRV_LOGE(ctx, "scenario_id(%d) invalid exp_idx(%u) mode max exp_cnt(%u)\n",
				 scenario_id, exp_idx, ctx->s_ctx.mode[scenario_id].exp_cnt);
			return -EINVAL;
		}

		lut_idx = get_multiexp_belong_lut(ctx, scenario_id, exp_idx);

		if (lut_idx >= IMGSENSOR_LUT_MAXCNT) {
			DRV_LOGE(ctx, "scenario_id(%d) invalid exp_idx(%u) mode max exp_cnt(%u)\n",
				 scenario_id, exp_idx, ctx->s_ctx.mode[scenario_id].exp_cnt);
			return -EINVAL;
		}
		pclk = get_lut_static_info(ctx, u64, pclk, scenario_id, lut_idx);
		linelength = get_lut_static_info(ctx, u32, linelength, scenario_id, lut_idx);
	}

	if (pclk != 0 && linelength != 0) {
		*linetime_in_ns = (u32)CALC_LINE_TIME_IN_NS(pclk, linelength);
		DRV_LOG_MUST(ctx, "linetime_in_ns(%d)\n", *linetime_in_ns);
	} else {
		DRV_LOGE(ctx, "pclk(%lld) or linelength(%d) is 0\n", pclk, linelength);
		return -EINVAL;
	}

	return 0;
}

int common_get_cycle_base_v1_linetime_in_ns(void *arg,
	u32 scenario_id, u32 *linetime_in_ns, enum GET_LINETIME_ENUM linetime_type,
	enum IMGSENSOR_EXPOSURE exp_idx)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u32 cycle_base_ratio = 0;
	u32 linelength = 0;
	u8 mclk = 0;
	u64 pclk = 0;
	u32 cycle_base_linetime_in_ns = 0;
	u8 lut_idx;

	if (!ctx) {
		return -EINVAL;
	}

	if (!linetime_in_ns) {
		DRV_LOGE(ctx, "linetime_in_ns is NULL\n");
		return -EINVAL;
	}

	cycle_base_ratio = ctx->s_ctx.cycle_base_ratio;
	linelength = ctx->s_ctx.mode[scenario_id].linelength;

	if (ctx->s_ctx.mode[scenario_id].aov_mode)
		mclk = g_mclk_info(ctx, aov_pw_seq, HW_ID_MCLK);
	else
		mclk = g_mclk_info(ctx, pw_seq, HW_ID_MCLK);

	pclk = ctx->s_ctx.mode[scenario_id].pclk;

	if (ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_DCG_RAW_VS ||
	    ctx->s_ctx.mode[scenario_id].hdr_mode == HDR_RAW_DCG_COMPOSE_VS) {
		/* different exp has different linetime */
		/* need return by exp_idx */
		if (exp_idx >= ctx->s_ctx.mode[scenario_id].exp_cnt) {
			DRV_LOGE(ctx, "scenario_id(%d) invalid exp_idx(%u) mode max exp_cnt(%u)\n",
				 scenario_id, exp_idx, ctx->s_ctx.mode[scenario_id].exp_cnt);
			return -EINVAL;
		}

		lut_idx = get_multiexp_belong_lut(ctx, scenario_id, exp_idx);

		if (lut_idx >= IMGSENSOR_LUT_MAXCNT) {
			DRV_LOGE(ctx, "scenario_id(%d) invalid exp_idx(%u) mode max exp_cnt(%u)\n",
				 scenario_id, exp_idx, ctx->s_ctx.mode[scenario_id].exp_cnt);
			return -EINVAL;
		}
		pclk = get_lut_static_info(ctx, u64, pclk, scenario_id, lut_idx);
		linelength = get_lut_static_info(ctx, u32, linelength, scenario_id, lut_idx);
	}

	if (cycle_base_ratio != 0 && linelength != 0 && mclk != 0 && pclk != 0) {
		cycle_base_linetime_in_ns = (linelength + (2 * cycle_base_ratio)) * mclk / (pclk/1000000);
		cycle_base_linetime_in_ns += 1; /* Round Up */
		cycle_base_linetime_in_ns = cycle_base_linetime_in_ns * 1000 / mclk;
		*linetime_in_ns = cycle_base_linetime_in_ns;
		DRV_LOG(ctx, "linetime_in_ns(%d)\n", *linetime_in_ns);
	} else {
		DRV_LOGE(ctx, "scenario_id (%d) cycle_base_ratio(%d) linelength(%d)  is 0\n",
			scenario_id, cycle_base_ratio, linelength);
		DRV_LOGE(ctx, "scenario_id (%d) aov_mode(%d) mclk(%d) pclk(%llu) is 0\n",
			scenario_id, ctx->s_ctx.mode[scenario_id].aov_mode, mclk, pclk);
		return -EINVAL;
	}

	return 0;
}

int common_init_ctx(struct subdrv_ctx *ctx, struct i2c_client *i2c_client, u8 i2c_write_id)
{
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	DRV_LOG_MUST(ctx, "i2c_write_id = 0x%x\n", i2c_write_id);

	return 0;
}

int common_i3c_pre_config(struct subdrv_ctx *ctx)
{
	int ret = 0;

	if ((ctx->ixc_client.protocol == I3C_PROTOCOL)
		&& (ctx->i2c_vir_client.i2c_dev)) {
		switch (ctx->s_ctx.i2c_transfer_data_type) {
		case I2C_DT_ADDR_16_DATA_16:
			ret = adaptor_ixc_wr_regs_u16(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				ctx->s_ctx.i3c_precfg_setting_table,
				ctx->s_ctx.i3c_precfg_setting_len);
			break;
		case I2C_DT_ADDR_16_DATA_8:
		default:
			ret = adaptor_ixc_wr_regs_u8(&ctx->i2c_vir_client, ctx->pre_cfg_addr,
				ctx->s_ctx.i3c_precfg_setting_table,
				ctx->s_ctx.i3c_precfg_setting_len);
			break;
		}
		if (ret) {
			DRV_LOGE(ctx, "fail. ret=%d\n", ret);
			return ERROR_SENSOR_CONNECT_FAIL;
		}
		DRV_LOG_MUST(ctx, "success. ret=%d, setting_len: %d\n",
				ret, ctx->s_ctx.i3c_precfg_setting_len);
		mdelay(15);
	}
	return ERROR_NONE;
} /* pre_config */
