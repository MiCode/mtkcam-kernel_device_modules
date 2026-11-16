/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 MediaTek Inc.
 */

#ifndef __ADAPTOR_FSYNC_DEF_H__
#define __ADAPTOR_FSYNC_DEF_H__

#include "adaptor.h"			/* for adaptor_log */
#include "adaptor-trace.h"		/* for adaptor_trace_ */


/*******************************************************************************
 * macro --- USER special define
 ******************************************************************************/
/**
 * ONLY for testing or bypass fsync mgr
 * (if you define this, all log msg will also be disable)
 */
/* #define FORCE_DISABLE_FSYNC_MGR */


/*******************************************************************************
 * macro --- common / basic
 ******************************************************************************/
#define FSYNC_WAIT_TSREC_UPDATE_DELAY_CNT (5)
#define FSYNC_WAIT_TSREC_UPDATE_DELAY_US  (400)


/*******************************************************************************
 * macro --- log
 ******************************************************************************/
#if !defined(FORCE_DISABLE_FSYNC_MGR)
#define FSYNC_MGR_LOG_INF(ctx, format, ...) { \
	dev_info(ctx->dev, PFX "[%s] " format, __func__, ##__VA_ARGS__); \
}

#define FSYNC_MGR_LOGD(ctx, format, ...) { \
	adaptor_logd(ctx, PFX ": " format, ##__VA_ARGS__); \
}

#define FSYNC_MGR_LOGI(ctx, format, ...) { \
	adaptor_logi(ctx, PFX ": " format, ##__VA_ARGS__); \
}
#else
#define FSYNC_MGR_LOG_INF(ctx, format, ...)
#define FSYNC_MGR_LOGD(ctx, format, ...)
#define FSYNC_MGR_LOGI(ctx, format, ...)
#endif


/*******************************************************************************
 * macro --- trace
 ******************************************************************************/
#define FSYNC_TRACE_BEGIN(fmt, args...) \
do { \
	if (adaptor_trace_enabled()) { \
		__adaptor_systrace( \
			"B|%d|%s::" fmt, task_tgid_nr(current), PFX, ##args); \
	} \
} while (0)

#define FSYNC_TRACE_FUNC_BEGIN() \
	FSYNC_TRACE_BEGIN("%s", __func__)

#define FSYNC_TRACE_END() \
do { \
	if (adaptor_trace_enabled()) { \
		__adaptor_systrace("E|%d", task_tgid_nr(current)); \
	} \
} while (0)

#define FSYNC_TRACE_PR_LOG_INF(fmt, args...) \
do { \
	if (adaptor_trace_enabled()) { \
		__adaptor_systrace( \
			"B|%d|%s[%s]" fmt, task_tgid_nr(current), PFX, __func__, ##args); \
		__adaptor_systrace("E|%d", task_tgid_nr(current)); \
	} \
} while (0)


#endif /* __ADAPTOR_FSYNC_DEF_H__ */
