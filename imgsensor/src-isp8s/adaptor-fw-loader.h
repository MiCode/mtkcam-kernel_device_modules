/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2024 MediaTek Inc. */

#ifndef __ADAPTOR_FW_LOADER_H__
#define __ADAPTOR_FW_LOADER_H__

#include "adaptor.h"

#define SUPPORT_GENERATOR_VERSION "V01_2024-11-12_11:02"

#define firmware_support(__x__) \
	((__x__) && (__x__)->is_fw_support)

#define for_each_firmware(__f__, __x__) \
	list_for_each_entry((__f__), &(__x__)->fw_list, list)

/* Macro to copy a member from one struct to another based on the sam member name */
#define COPY_COMMON_MEMBER(dest_struct, src_struct, member) \
{ \
	(dest_struct)->member = (src_struct)->member; \
}

/* Macro to copy a member from one struct to another based on specific member name */
#define COPY_SPECIFIC_MEMBER(dest_struct, src_struct, dest_member, src_member) \
{ \
	(dest_struct)->dest_member = (src_struct)->src_member; \
}

/* Macro to check two struct's member */
#define RET_IF_CHK_FAIL(ctx, __struct_a__, __struct_b__, __field__, format, args...) \
({ \
	struct adaptor_ctx *__ctx = (ctx); \
	int __ret = 0; \
	if (memcmp(&(__struct_a__)->__field__, &(__struct_b__)->__field__, \
		   sizeof((__struct_a__)->__field__)) != 0) { \
		adaptor_loge(__ctx, "compare failed with field '%s->%s' for " \
			format "\n", \
			#__struct_a__, #__field__, ##args); \
		__ret = -EINVAL; \
	} \
	__ret; \
})

/* Macro to check two struct's pointer member nullity */
#define RET_IF_CHK_PTR_NULL(ctx, __struct_a__, __struct_b__, __field__, format, args...) \
({ \
	struct adaptor_ctx *__ctx = (ctx); \
	int __ret = 0; \
	if (((__struct_a__)->__field__ == NULL && (__struct_b__)->__field__ != NULL) || \
	    ((__struct_a__)->__field__ != NULL && (__struct_b__)->__field__ == NULL)) { \
		adaptor_loge(__ctx, "compare failed with ptr field '%s->%s' left:%p, right:%p for " \
			format "\n", \
			#__struct_a__, #__field__, \
			(__struct_a__)->__field__, (__struct_b__)->__field__, ##args); \
		__ret = -EINVAL; \
	} \
	__ret; \
})

/* Macro to check two struct's pointer member */
#define RET_IF_CHK_PTR_FAIL(ctx, __struct_a__, __struct_b__, __field__, __size__, format, args...) \
({ \
	struct adaptor_ctx *__ctx = (ctx); \
	int __ret = 0; \
	__ret = RET_IF_CHK_PTR_NULL(ctx, __struct_a__, __struct_b__, __field__, format, ##args); \
	if (!__ret && (__struct_a__)->__field__ != NULL && (__struct_b__)->__field__ != NULL && \
	    memcmp((__struct_a__)->__field__, (__struct_b__)->__field__, __size__) != 0) { \
		adaptor_loge(__ctx, "compare failed with ptr field '%s->%s' for " \
			format "\n", \
			#__struct_a__, #__field__, ##args); \
		__ret = -EINVAL; \
	} \
	__ret; \
})


/**
 * Look up firmware list
 *
 * @param ctx The adaptor context
 * @param loader The firmware loader
 * @param fw_file_name The string array to be check if exist
 * @param count The array element count of {@code fw_file_name}
 *
 * @return zero if successful or negative number if error occurred
 */
int lookup_firmwares(struct adaptor_ctx *ctx,
		     struct sensor_firmware_loader * const loader,
		     const char *fw_file_name[], int count);

/**
 * Loading the correspond firmware
 *
 * @param ctx The adaptor context
 * @param sensor_fw The sensor firmware info
 *
 * @return zero if successful
 */
int loading_firmware(struct adaptor_ctx *ctx, struct sensor_firmware *sensor_fw);

/**
 * Deinit firmware loader, to release the list
 *
 * @param ctx The adaptor context
 * @param loader The firmware loader
 */
int deinit_firmware_loader(struct adaptor_ctx *ctx, struct sensor_firmware_loader *loader);


/**
 * Release sensor firmware allocated resources
 * Please don't release resource if search sensor pass, due to the allocated s_ctx
 * resource will be free also
 *
 * @param ctx The adaptor context
 * @param sensor_fw The sensor firmware info
 */
int release_firmware_resource(struct adaptor_ctx *ctx,
			      struct sensor_firmware *sensor_fw);

#endif
