/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2023 MediaTek Inc. */

#ifndef __ADAPTOR_EINT_CB_IMPL_H__
#define __ADAPTOR_EINT_CB_IMPL_H__

#include "adaptor.h"
#include "adaptor-eint-cb-ctrl.h"


/******************************************************************************/
// adaptor tsrec call back ctrl --- function
/******************************************************************************/
static inline void adaptor_eint_cb_ctrl_info_setup(struct adaptor_ctx *ctx,
	struct mtk_cam_seninf_eint_cb_info *info, const unsigned int flag)
{
	if (unlikely(ctx == NULL))
		return;

	if (flag) {
		/* set */
		ctx->eint_cb_ctrl.cb_info = *info;
	} else {
		ctx->eint_cb_ctrl.cb_info.is_start = info->is_start;
	}
}

static inline int adaptor_eint_cb_ctrl_execute(struct adaptor_ctx *ctx,
	const enum mtk_cam_seninf_eint_cb_cmd cb_cmd, void *arg, const char *caller)
{
	int ret;
	int *is_streaming = NULL;

	if (unlikely(ctx == NULL))
		return -EINVAL;

	if (unlikely(arg == NULL)) {
		adaptor_logi(ctx,
			"[%s] ERROR: idx:%d, invalid arg:(nullptr)\n",
			caller, ctx->idx);
		return -EINVAL;
	}

	is_streaming = (int *)arg;

	/* error case, eint cb function pointer is NULL */
	if (unlikely(ctx->eint_cb_ctrl.cb_info.eint_cb_handler == NULL))
		return -EINVAL;


	ret = ctx->eint_cb_ctrl.cb_info.eint_cb_handler(
		ctx->eint_cb_ctrl.cb_info.eint_no,
		(const unsigned int)cb_cmd, arg, caller);

	return ret;
}

static inline int notify_seninf_eint_streaming(struct adaptor_ctx *ctx, int flag)
{
	int is_streaming = flag ? 1 : 0;

	if (unlikely(ctx == NULL))
		return -EINVAL;

	adaptor_logi(ctx, "eint_no:%d, eint is_start:%u, is_streaming:%d\n",
		ctx->eint_cb_ctrl.cb_info.eint_no,
		ctx->eint_cb_ctrl.cb_info.is_start,
		is_streaming);
	return (adaptor_eint_cb_ctrl_execute(ctx,
		EINT_CB_CMD_NOTIFY_STREAMON, &is_streaming, __func__));
}

static inline int notify_seninf_eint_seamless_switch(struct adaptor_ctx *ctx, int flag)
{
	int is_seamless_switch = flag ? 1 : 0;

	if (unlikely(ctx == NULL))
		return -EINVAL;

	adaptor_logi(ctx, "eint_no:%d, eint is_start:%u, is_streaming:%d prsh_length_lc:%u\n",
		ctx->eint_cb_ctrl.cb_info.eint_no,
		ctx->eint_cb_ctrl.cb_info.is_start,
		is_seamless_switch,
		ctx->subctx.s_ctx.seamless_switch_prsh_length_lc);

	if (ctx->subctx.s_ctx.seamless_switch_prsh_length_lc == 0)
		return 0;

	return (adaptor_eint_cb_ctrl_execute(ctx,
		EINT_CB_CMD_NOTIFY_SEAMLESS_SWITCH, &is_seamless_switch, __func__));
}

#endif
