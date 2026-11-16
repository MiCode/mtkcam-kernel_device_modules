/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef _MTK_HCP_ISP8S_MTK_HCP_INTERFACE_H_
#define _MTK_HCP_ISP8S_MTK_HCP_INTERFACE_H_

#include <linux/dma-buf.h>
#include <linux/types.h>
#include <linux/platform_device.h>

/**
 * HCP ID definition
 */
enum hcp_id {
	HCP_INIT_ID = 0,
	HCP_ISP_CMD_ID,
	HCP_ISP_FRAME_ID,
	HCP_IMGSYS_INIT_ID,
	HCP_IMGSYS_FRAME_ID,
	HCP_IMGSYS_HW_TIMEOUT_ID,
	HCP_IMGSYS_SW_TIMEOUT_ID,
	HCP_IMGSYS_DEQUE_DUMP_ID,
	HCP_IMGSYS_DEQUE_DONE_ID,
	HCP_IMGSYS_ASYNC_DEQUE_DONE_ID,
	HCP_IMGSYS_DEINIT_ID,
	HCP_IMGSYS_IOVA_FDS_ADD_ID,
	HCP_IMGSYS_IOVA_FDS_DEL_ID,
	HCP_IMGSYS_UVA_FDS_ADD_ID,
	HCP_IMGSYS_UVA_FDS_DEL_ID,
	HCP_IMGSYS_SET_CONTROL_ID,
	HCP_IMGSYS_GET_CONTROL_ID,
	HCP_IMGSYS_CLEAR_HWTOKEN_ID,
	HCP_IMGSYS_AEE_DUMP_ID,
	HCP_IMGSYS_ALLOC_WORKING_BUF_ID,
	HCP_IMGSYS_FREE_WORKING_BUF_ID,
	HCP_MAX_ID,
};

typedef void (*hcp_handler_t) (void *data, unsigned int len, void *priv);

struct mtk_hcp_module_callbacks {
	hcp_handler_t init_done;
	hcp_handler_t send_frames;
	hcp_handler_t clear_hw_token;
	hcp_handler_t dump_for_aee;
};

#define DECLARE_MOD_W_BUF_OPS_EXT(_mod_, _mb_) \
	unsigned int (*fetch_##_mod_##_##_mb_##_mb_id) ( \
		struct platform_device *pdev, \
		unsigned int mem_mode); \
	int (*fetch_##_mod_##_##_mb_##_mb_fd) ( \
		struct platform_device *pdev, \
		unsigned int mem_mode); \
	void* (*fetch_##_mod_##_##_mb_##_mb_virt) ( \
		struct platform_device *pdev, \
		unsigned int mem_mode); \
	u64 (*fetch_##_mod_##_##_mb_##_mb_phy) ( \
		struct platform_device *pdev, \
		unsigned int mem_mode); \
	u64 (*fetch_##_mod_##_##_mb_##_mb_dma) ( \
		struct platform_device *pdev, \
		unsigned int mem_mode); \
	size_t (*fetch_##_mod_##_##_mb_##_mb_size) ( \
		struct platform_device *pdev, \
		unsigned int mem_mode)

#define DECLARE_MOD_W_BUF_OPS_CQ(_mod_) DECLARE_MOD_W_BUF_OPS_EXT(_mod_, cq)
#define DECLARE_MOD_W_BUF_OPS_TDR(_mod_) DECLARE_MOD_W_BUF_OPS_EXT(_mod_, tdr)
#define DECLARE_MOD_W_BUF_OPS_C_MISC(_mod_) DECLARE_MOD_W_BUF_OPS_EXT(_mod_, c_misc)
#define DECLARE_MOD_W_BUF_OPS_NC_MISC(_mod_) DECLARE_MOD_W_BUF_OPS_EXT(_mod_, nc_misc)

#define DECLARE_W_BUF_OPS(_mod_) \
	unsigned int (*fetch_##_mod_##_mb_id) ( \
		struct platform_device *pdev, \
		unsigned int mem_mode); \
	int (*fetch_##_mod_##_mb_fd) ( \
		struct platform_device *pdev, \
		unsigned int mem_mode); \
	void* (*fetch_##_mod_##_mb_virt) ( \
		struct platform_device *pdev, \
		unsigned int mem_mode); \
	u64 (*fetch_##_mod_##_mb_phy) ( \
		struct platform_device *pdev, \
		unsigned int mem_mode); \
	u64 (*fetch_##_mod_##_mb_dma) ( \
		struct platform_device *pdev, \
		unsigned int mem_mode); \
	size_t (*fetch_##_mod_##_mb_size) ( \
		struct platform_device *pdev, \
		unsigned int mem_mode)

struct mtk_hcp_ops {
	/**
	 * register - register the callbacks for hcp to call
	 *
	 * @pdev: HCP platform device
	 * @cbs:  callbacks to registered
	 * @priv: private data of caller
	 *
	 * Return: Return 0 if hcp registers successfully, otherwise it is failed.
	 */
	int (*register_cbs)(
		struct platform_device *pdev,
		const struct mtk_hcp_module_callbacks *cbs,
		void *priv);

	/**
	 * unregister_cbs - unregister the callbacks
	 *
	 * @pdev: HCP platform device
	 *
	 * Return: Return 0 if hcp unregisters successfully, otherwise it is failed.
	 */
	int (*unregister_cbs)(
		struct platform_device *pdev);

	/* Communication ops */
	/* TODO: change hcp id to api */

	/**
	 * send - send data from camera kernel driver to HCP and wait the
	 *      command send to demand.
	 *
	 * @pdev:   HCP platform device
	 * @id:     HCP ID
	 * @buf:    the data buffer
	 * @len:    the data buffer length
	 * @frame_no: frame number
	 *
	 * This function is thread-safe. When this function returns,
	 * HCP has received the data and save the data in the workqueue.
	 * After that it will schedule work to dequeue to send data to
	 * RED for programming register.
	 * Return: Return 0 if sending data successfully, otherwise it is failed.
	 **/
	int (*send)(
		struct platform_device *pdev,
		enum hcp_id id,
		void *buf,
		unsigned int len,
		int req_fd);

	/**
	 * send_async - send data from camera kernel driver to HCP without
	 *      waiting demand receives the command.
	 *
	 * @pdev:   HCP platform device
	 * @id:     HCP ID
	 * @buf:    the data buffer
	 * @len:    the data buffer length
	 * @frame_no: frame number
	 *
	 * This function is thread-safe. When this function returns,
	 * HCP has received the data and save the data in the workqueue.
	 * After that it will schedule work to dequeue to send data to CM4 or
	 * RED for programming register.
	 * Return: Return 0 if sending data successfully, otherwise it is failed.
	 **/
	int (*send_async)(
		struct platform_device *pdev,
		enum hcp_id id,
		void *buf,
		unsigned int len,
		int frame_no);
	/**
	 * purge_msgs - purge messages which are leaved in the queue
	 *
	 * @pdev: HCP platform device
	 *
	 * This function purges messages
	 **/
	void (*purge_msgs)(
		struct platform_device *pdev);

	/* deprecated later */
	int (*fill_init_info)(
		struct platform_device *pdev,
		void *info);

	int (*flush_dma_buf)(
		struct platform_device *pdev,
		struct dma_buf *dbuf,
		const unsigned int offset,
		const unsigned int len);

	int (*flush_mb)(
		struct platform_device *pdev,
		const unsigned int wb_id,
		const unsigned int offset,
		const unsigned int len);
	/**
	 * write_krn_db - Write a buffer into kernel DB.
	 *
	 * @pdev:   HCP platform device
	 * @buf:    the data buffer
	 * @len:    the data buffer length
	 *
	 * This function is thread-safe. When this function returns
	 * Note this function should use in user context.
	 *
	 * Return: Return actual size which has been write into kernel DB. Caller
	 * should compare return value with argument "len" to determine whether
	 * whole data has been written successfully.
	 **/
	ssize_t (*write_krn_db)(
		struct platform_device *pdev,
		const char *buf,
		size_t len);

	/* Memory block */
	int (*allocate_gce_mb)(
		struct platform_device *pdev,
		unsigned int mem_mode);
	int (*free_gce_mb)(
		struct platform_device *pdev,
		unsigned int mem_mode);
	int (*get_gce_mb)(
		struct platform_device *pdev,
		unsigned int mem_mode);
	int (*put_gce_mb)(
		struct platform_device *pdev,
		unsigned int mem_mode);
	int (*allocate_gce_clr_token_mb)(
		struct platform_device *pdev,
		unsigned int mem_mode);
	int (*free_gce_clr_token_mb)(
		struct platform_device *pdev,
		unsigned int mem_mode);
	int (*allocate_mod_mbs)(
		struct platform_device *pdev,
		unsigned int mem_mode);
	int (*free_mod_mbs)(
		struct platform_device *pdev,
		unsigned int mem_mode);

	/* e.g. fetch_gce_mb_{fd/id/virt/phy/dma/size} */
	DECLARE_W_BUF_OPS(gce);
	/* e.g. fetch_gce_clr_token_mb_{fd/id/virt/phy/dma/size} */
	DECLARE_W_BUF_OPS(gce_clr_token);

	/* e.g. fetch_wpe_{cq/tdr/c_misc/nc_misc}_mb_{fd/id/virt/phy/dma/size} */
	DECLARE_MOD_W_BUF_OPS_CQ(wpe);
	DECLARE_MOD_W_BUF_OPS_TDR(wpe);
	DECLARE_MOD_W_BUF_OPS_C_MISC(wpe);
	DECLARE_MOD_W_BUF_OPS_NC_MISC(wpe);
	DECLARE_MOD_W_BUF_OPS_CQ(omc);
	DECLARE_MOD_W_BUF_OPS_TDR(omc);
	DECLARE_MOD_W_BUF_OPS_C_MISC(omc);
	DECLARE_MOD_W_BUF_OPS_NC_MISC(omc);
	DECLARE_MOD_W_BUF_OPS_CQ(adl);
	DECLARE_MOD_W_BUF_OPS_TDR(adl);
	DECLARE_MOD_W_BUF_OPS_C_MISC(adl);
	DECLARE_MOD_W_BUF_OPS_NC_MISC(adl);
	DECLARE_MOD_W_BUF_OPS_CQ(traw);
	DECLARE_MOD_W_BUF_OPS_TDR(traw);
	DECLARE_MOD_W_BUF_OPS_C_MISC(traw);
	DECLARE_MOD_W_BUF_OPS_NC_MISC(traw);
	DECLARE_MOD_W_BUF_OPS_CQ(dip);
	DECLARE_MOD_W_BUF_OPS_TDR(dip);
	DECLARE_MOD_W_BUF_OPS_C_MISC(dip);
	DECLARE_MOD_W_BUF_OPS_NC_MISC(dip);
	DECLARE_MOD_W_BUF_OPS_CQ(pqdip);
	DECLARE_MOD_W_BUF_OPS_TDR(pqdip);
	DECLARE_MOD_W_BUF_OPS_C_MISC(pqdip);
	DECLARE_MOD_W_BUF_OPS_NC_MISC(pqdip);
	DECLARE_MOD_W_BUF_OPS_CQ(me);
	DECLARE_MOD_W_BUF_OPS_TDR(me);
	DECLARE_MOD_W_BUF_OPS_C_MISC(me);
	DECLARE_MOD_W_BUF_OPS_NC_MISC(me);
	DECLARE_MOD_W_BUF_OPS_CQ(mae);
	DECLARE_MOD_W_BUF_OPS_TDR(mae);
	DECLARE_MOD_W_BUF_OPS_C_MISC(mae);
	DECLARE_MOD_W_BUF_OPS_NC_MISC(mae);
	DECLARE_MOD_W_BUF_OPS_CQ(dfp);
	DECLARE_MOD_W_BUF_OPS_TDR(dfp);
	DECLARE_MOD_W_BUF_OPS_C_MISC(dfp);
	DECLARE_MOD_W_BUF_OPS_NC_MISC(dfp);
	DECLARE_MOD_W_BUF_OPS_CQ(dpe);
	DECLARE_MOD_W_BUF_OPS_TDR(dpe);
	DECLARE_MOD_W_BUF_OPS_C_MISC(dpe);
	DECLARE_MOD_W_BUF_OPS_NC_MISC(dpe);

	/* not used in ISP8S */
	void* (*fetch_hwid_virt)(
		struct platform_device *pdev,
		unsigned int mem_mode);
	int (*set_apu_dc)(
		struct platform_device *pdev,
		int32_t value,
		size_t size);
};


/**
 * @brief Retrieves the MTK HCP operations structure associated with the given platform device.
 *
 * This function returns a pointer to an mtk_hcp_ops struct that contains
 * function pointers to various operations supported by the MTK HCP.
 *
 * @param pdev A pointer to the platform_device structure representing the platform device for
 *             which the MTK HCP operations are to be retrieved.
 * @return A pointer to the mtk_hcp_ops struct associated with the platform device,
 *         or NULL if not found.
 */
const struct mtk_hcp_ops *mtk_hcp_fetch_ops(struct platform_device *pdev);

/**
 * mtk_hcp_get_plat_device - get HCP's platform device
 *
 * @pdev: the platform device of the module requesting HCP platform
 *    device for using HCP API.
 *
 * Return: Return NULL if it is failed.
 * otherwise it is HCP's platform device
 **/
struct platform_device *mtk_hcp_get_plat_device(struct platform_device *pdev);

#endif /* _MTK_HCP_ISP8S_MTK_HCP_INTERFACE_H_ */
