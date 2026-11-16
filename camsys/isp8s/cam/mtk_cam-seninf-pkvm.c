// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/version.h>
#if (KERNEL_VERSION(6, 13, 0) > LINUX_VERSION_CODE)
#include <asm/kvm_pkvm_module.h>
#endif
#include <linux/arm-smccc.h>
#include <linux/printk.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <pkvm_mgmt/pkvm_mgmt.h>

#include "mtk_cam-seninf-pkvm.h"

#define PFX "mtk_cam-seninf-pkvm"
#define LOG_INF(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)

int seninf_pkvm_open_session(void)
{
	return SENINF_PKVM_RETURN_SUCCESS;
}

int seninf_pkvm_close_session(void)
{
	return SENINF_PKVM_RETURN_SUCCESS;
}

int seninf_pkvm_checkpipe(u64 SecInfo_addr)
{
	struct arm_smccc_res res;
#if IS_ENABLED(CONFIG_MTK_PKVM_SENINF)
	unsigned long hvc_id;
#endif

	if (!SecInfo_addr)
		return SENINF_PKVM_RETURN_ERROR;

	LOG_INF("[%s] secInfo_addr 0x%llx", __func__, SecInfo_addr);
#if IS_ENABLED(CONFIG_MTK_PKVM_SENINF)
	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_SENINF_CHECKPIPE,
		0, 0, 0, 0, 0, 0, &res);
	hvc_id = res.a1;
	pkvm_el2_mod_call(hvc_id, (u32)SecInfo_addr, (u32)(SecInfo_addr >> 32));
#else
	arm_smccc_smc(MTK_SIP_KERNEL_ISP_CONTROL, SENINF_TEE_CMD_CHECKPIPE,
		(u32)SecInfo_addr, (u32)(SecInfo_addr >> 32), 0, 0, 0, 0, &res);
#endif

	return SENINF_PKVM_RETURN_SUCCESS;
}

int seninf_pkvm_free(void)
{
	struct arm_smccc_res res;

#if IS_ENABLED(CONFIG_MTK_PKVM_SENINF)
	unsigned long hvc_id;

	arm_smccc_1_1_smc(SMC_ID_MTK_PKVM_SENINF_FREE,
		0, 0, 0, 0, 0, 0, &res);
	hvc_id = res.a1;
	pkvm_el2_mod_call(hvc_id);
#else
	arm_smccc_smc(MTK_SIP_KERNEL_ISP_CONTROL, SENINF_TEE_CMD_FREE,
		0, 0, 0, 0, 0, 0, &res);
#endif

	return SENINF_PKVM_RETURN_SUCCESS;
}
