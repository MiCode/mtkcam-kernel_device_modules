/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef __MTK_CAM_VIRT_ISP_H
#define __MTK_CAM_VIRT_ISP_H

//#define IS_VIRT_ISP // replace by kernel config
#ifdef IS_VIRT_ISP

static inline bool void_function_bool(void)
{
	return false;
}

static inline int void_function_int(void)
{
	return 0;
}

static inline void *void_function_nullptr(void)
{
	return NULL;
}

static inline u32 void_function_u32(void)
{
	return 0;
}

#define cmdq_mbox_create(...)           void_function_nullptr()

#define pm_runtime_enable(...)           void_function_int()
#define pm_runtime_disable(...)           void_function_int()
#define pm_runtime_get_sync(...)           void_function_int()
#define pm_runtime_put_sync(...)           void_function_int()
#define pm_runtime_get(...)           void_function_int()
#define pm_runtime_put(...)           void_function_int()

#define clk_disable_unprepare(...)
#define clk_prepare_enable(...)     void_function_int()

#define disable_irq(...)
#define enable_irq(...)

#undef readx_poll_timeout
#define readx_poll_timeout(op, addr, val, cond, sleep_us, timeout_us) \
({\
	(void)(op); \
	(void)(addr); \
	(void)(val); \
	(void)(sleep_us); \
	(void)(timeout_us); \
	void_function_int(); \
})

#undef readl
#define readl(addr) \
({\
	(void)(addr); \
	void_function_u32(); \
})


#undef readl_relaxed
#define readl_relaxed(addr) \
({\
	(void)(addr); \
	void_function_u32(); \
})

#undef writel
#define writel(val, addr) \
({\
	(void)(val); \
	(void)(addr); \
	void_function_u32(); \
})

#undef writel_relaxed
#define writel_relaxed(val, addr) \
({\
	(void)(val); \
	(void)(addr); \
	void_function_u32(); \
})

#define mtk_cam_job_state_init_basic(s, cb, sen) \
	mtk_cam_job_state_init_virt_isp(s, cb, sen)
#define mtk_cam_job_state_init_m2m(s, cb)  \
	mtk_cam_job_state_init_virt_isp(s, cb, false)
#define mtk_cam_job_state_init_subsample(s, cb, sen)  \
	mtk_cam_job_state_init_virt_isp(s, cb, sen)
#define mtk_cam_job_state_init_mstream(s, cb, sen)  \
	mtk_cam_job_state_init_virt_isp(s, cb, sen)
#define mtk_cam_job_state_init_extisp(s, cb, sen)  \
	mtk_cam_job_state_init_virt_isp(s, cb, sen)
#define mtk_cam_job_state_init_ts(s, cb, sen)  \
	mtk_cam_job_state_init_virt_isp(s, cb, sen)

#endif

#endif // __MTK_CAM_VIRT_ISP_H
