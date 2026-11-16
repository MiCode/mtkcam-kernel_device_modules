/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 MediaTek Inc. */

#ifndef __ADAPTOR_I2C_H__
#define __ADAPTOR_I2C_H__

#include <linux/i2c.h>
#include <linux/slab.h>

#include "mtk-i3c-i2c-wrap.h"

#define MAX_BUF_SIZE_U8 765
#define MAX_BUF_SIZE_U8_U8 510
#define MAX_BUF_SIZE_U16 1020

#define MAX_MSG_NUM_U8 255
#define MAX_MSG_NUM_U8_U8 255
#define MAX_MSG_NUM_U16 255
#define MAX_VAL_NUM_U8 (MAX_BUF_SIZE_U8 - 2)
#define MAX_VAL_NUM_U16 ((MAX_BUF_SIZE_U16 - 2) >> 1)

struct cache_wr_regs_u8 {
	u8 buf[MAX_BUF_SIZE_U8];
	struct i2c_msg msg[MAX_MSG_NUM_U8];
};

struct cache_wr_regs_u8_u8 {
	u8 buf[MAX_BUF_SIZE_U8_U8];
	struct i2c_msg msg[MAX_MSG_NUM_U8_U8];
};

struct cache_wr_regs_u16 {
	u8 buf[MAX_BUF_SIZE_U16];
	struct i2c_msg msg[MAX_MSG_NUM_U16];
};

struct cache_wr_regs_u8_ixc {
	u8 buf[MAX_BUF_SIZE_U8];
	struct i3c_i2c_xfer msg[MAX_MSG_NUM_U8];
};

struct cache_wr_regs_u8_u8_ixc {
	u8 buf[MAX_BUF_SIZE_U8_U8];
	struct i3c_i2c_xfer msg[MAX_MSG_NUM_U8_U8];
};

struct cache_wr_regs_u16_ixc {
	u8 buf[MAX_BUF_SIZE_U16];
	struct i3c_i2c_xfer msg[MAX_MSG_NUM_U16];
};
struct device *adaptor_ixc_get_dev (struct i3c_i2c_device *client);
void *adaptor_ixc_get_clientdata (struct i3c_i2c_device *client);
int adaptor_ixc_do_daa (struct i3c_i2c_device *client);
void adaptor_i3c_device_prepare(struct i3c_i2c_device *client);

int adaptor_i2c_rd_u8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *val);

int adaptor_i2c_rd_u8_u8(struct i2c_client *i2c_client,
		u16 addr, u8 reg, u8 *val);

int adaptor_i2c_rd_u16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 *val);

int adaptor_i2c_rd_p8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals);

int adaptor_i2c_wr_u8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 val);

int adaptor_i2c_wr_u8_u8(struct i2c_client *i2c_client,
		u16 addr, u8 reg, u8 val);

int adaptor_i2c_wr_u16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 val);

int adaptor_i2c_wr_p8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals);

int adaptor_i2c_wr_p16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 *p_vals, u32 n_vals);

int adaptor_i2c_wr_seq_p8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals);

int adaptor_i2c_wr_regs_u8(struct i2c_client *i2c_client,
		u16 addr, u16 *list, u32 len);

int adaptor_i2c_wr_regs_u8_u8(struct i2c_client *i2c_client,
		u16 addr, u16 *list, u32 len);

int adaptor_i2c_wr_regs_u16(struct i2c_client *i2c_client,
		u16 addr, u16 *list, u32 len);

/* for i2c/i3c data transfer*/
int adaptor_ixc_rd_u8(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u8 *val);

int adaptor_ixc_rd_u16(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u16 *val);

int adaptor_ixc_rd_p8(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals);

int adaptor_ixc_wr_u8(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u8 val);

int adaptor_ixc_wr_u16(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u16 val);

int adaptor_ixc_wr_p8(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals);

int adaptor_ixc_wr_p16(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u16 *p_vals, u32 n_vals);

int adaptor_ixc_wr_seq_p8(struct i3c_i2c_device *client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals);

int adaptor_ixc_wr_regs_u8(struct i3c_i2c_device *client,
		u16 addr, u16 *list, u32 len);

int adaptor_ixc_wr_regs_u8_u8(struct i3c_i2c_device *client,
		u16 addr, u16 *list, u32 len);

int adaptor_ixc_wr_regs_u16(struct i3c_i2c_device *client,
		u16 addr, u16 *list, u32 len);

#endif
