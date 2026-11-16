/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/printk.h>

#define HCP_LOG_LVL_DBG (0)
#define HCP_LOG_LVL_INF (1)
#define HCP_LOG_LVL_KEY (2)
#define HCP_LOG_LVL_WRN (3)
#define HCP_LOG_LVL_ERR (4)

extern int hcp_dbg_en;

static inline int hcp_dbg_enable(void)
{
	return hcp_dbg_en;
}

#define HCP_PRINT_DBG(fmt, args...) \
	do { \
		if (hcp_dbg_en <= HCP_LOG_LVL_DBG) \
			pr_info("[HCP_DBG] %s(): " fmt , __func__, ##args); \
	} while(0)

#define HCP_PRINT_INF(fmt, args...) \
	do { \
		if (hcp_dbg_en <= HCP_LOG_LVL_INF) \
			pr_info("[HCP_INF] %s(): " fmt , __func__, ##args); \
	} while(0)

#define HCP_PRINT_KEY(fmt, args...) \
	do { \
		if (hcp_dbg_en <= HCP_LOG_LVL_KEY) \
			pr_info("[HCP_KEY] %s(): " fmt , __func__, ##args); \
	} while(0)

#define HCP_PRINT_WRN(fmt, args...) \
	do { \
		if (hcp_dbg_en <= HCP_LOG_LVL_WRN) \
			pr_info("[HCP_WRN] %s(): " fmt , __func__, ##args); \
	} while(0)

#define HCP_PRINT_ERR(fmt, args...) \
	do { \
		if (hcp_dbg_en <= HCP_LOG_LVL_ERR) \
			pr_info("[HCP_ERR] %s(): " fmt , __func__, ##args); \
	} while(0)
