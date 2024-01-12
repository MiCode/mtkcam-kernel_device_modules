// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Johnson-CH Chiu <Johnson-CH.chiu@mediatek.com>
 *
 */

#define CREATE_TRACE_POINTS
#include "mtk_imgsys-trace.h"

int imgsys_ftrace_en;
module_param(imgsys_ftrace_en, int, 0644);


void __imgsys_systrace(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	trace_tracing_mark_write(fmt, &args);
	va_end(args);
}

bool imgsys_core_ftrace_enabled(void)
{
	return imgsys_ftrace_en;
}


