/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_LOG_H
#define _FRAME_SYNC_LOG_H

#ifndef FS_UT
#include <linux/printk.h>		/* for kernel log reduction */
#include <linux/spinlock.h>
#else
#include <stdio.h>			/* printf */
#endif


/******************************************************************************/
// log CTRL extern variables
/******************************************************************************/
#ifndef FS_UT
/* declare in frame_sync_console.c */
extern unsigned int fs_log_tracer;

extern spinlock_t fs_log_concurrency_lock;
#else /* => FS_UT */
#define fs_log_tracer 0xffffffff
#endif


/******************************************************************************/
// Log CTRL define/enum/macro
/******************************************************************************/
#define LOG_TRACER_DEF 0

#if defined(FS_UT)
#define TRACE_FS_FREC_LOG
#else
// #define TRACE_FS_FREC_LOG
#endif

enum fs_log_ctrl_category {
	/* basic category */
	LOG_FS_PF = 0,
	LOG_FS,
	LOG_FS_ALGO,
	LOG_FRM,
	LOG_SEN_REC,
	LOG_FS_UTIL,

	/* custom category */
};

#define _FS_LOG_ENABLED(category) \
	((fs_log_tracer) & (1UL << (category)))


/******************************************************************************/
// Log message macro
/******************************************************************************/
#define LOG_BUF_STR_LEN 2048

#ifdef FS_UT
#define LOG_INF(format, args...) printf(PFX "[%s] " format, __func__, ##args)
#define LOG_INF_CAT(log_cat, format, args...) printf(PFX "[%s] " format, __func__, ##args)
#define LOG_PF_INF(format, args...) printf(PFX "[%s] " format, __func__, ##args)
#define LOG_MUST(format, args...) printf(PFX "[%s] " format, __func__, ##args)
#define LOG_INF_CAT_SPIN(log_cat, format, args...) printf(PFX "[%s] " format, __func__, ##args)
#define LOG_PF_INF_SPIN(format, args...) printf(PFX "[%s] " format, __func__, ##args)
#define LOG_MUST_SPIN(format, args...) printf(PFX "[%s] " format, __func__, ##args)
#define LOG_PR_WARN(format, args...) printf(PFX "[%s] " format, __func__, ##args)
#define LOG_PR_ERR(format, args...) printf(PFX "[%s] " format, __func__, ##args)

#else /* => !FS_UT */

#define DY_INFO(log_cat, format, args...) \
do { \
	if (unlikely(_FS_LOG_ENABLED(log_cat))) { \
		pr_info(PFX "[%s] " format, __func__, ##args); \
	} \
} while (0)

#define DY_INFO_SPIN(log_cat, format, args...) \
do { \
	if (unlikely(_FS_LOG_ENABLED(log_cat))) { \
		spin_lock(&fs_log_concurrency_lock); \
		pr_info(PFX "[%s] " format, __func__, ##args); \
		spin_unlock(&fs_log_concurrency_lock); \
	} \
} while (0)

#define LOG_INF(format, args...) DY_INFO(FS_LOG_DBG_DEF_CAT, format, args)
#define LOG_INF_CAT(log_cat, format, args...) DY_INFO(log_cat, format, args)
#define LOG_PF_INF(format, args...) DY_INFO(LOG_FS_PF, format, args)
#define LOG_MUST(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)

#define LOG_INF_CAT_SPIN(log_cat, format, args...) DY_INFO_SPIN(log_cat, format, args)
#define LOG_PF_INF_SPIN(format, args...) DY_INFO_SPIN(LOG_FS_PF, format, args)
#define LOG_MUST_SPIN(format, args...) \
do { \
	spin_lock(&fs_log_concurrency_lock); \
	pr_info(PFX "[%s] " format, __func__, ##args); \
	spin_unlock(&fs_log_concurrency_lock); \
} while (0)


#define LOG_PR_WARN(format, args...) pr_warn(PFX "[%s] " format, __func__, ##args)
#define LOG_PR_ERR(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)
#endif // FS_UT


#define FS_SNPRINTF(buf, len, fmt, ...) { \
	len += snprintf(buf + len, LOG_BUF_STR_LEN - len, fmt, ##__VA_ARGS__); \
}
/******************************************************************************/


#endif
