// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2024 MediaTek Inc.

#include <linux/module.h>
#include <linux/types.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/path.h>
#include <linux/namei.h>
#include <linux/dirent.h>
#include <linux/firmware.h>
#include <linux/version.h>

#include "adaptor-fw-loader.h"
#include "adaptor-fw-types.h"
#include "adaptor-profile.h"

#define IMGSENSOR_FW_PATH "/vendor/firmware/sensor"
#define IMGSENSOR_FW_SUB_FOLDER "sensor"

#if (KERNEL_VERSION(6, 13, 0) > LINUX_VERSION_CODE)
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_IMPORT_NS(ANDROID_GKI_VFS_EXPORT_ONLY);
#else
MODULE_IMPORT_NS("VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver");
MODULE_IMPORT_NS("ANDROID_GKI_VFS_EXPORT_ONLY");
#endif

static LIST_HEAD(all_sensor_fw_list);
static int all_sensor_fw_list_cnt;

static inline void *ctx_fw_kcalloc(struct adaptor_ctx *ctx, struct sensor_firmware *fw,
				size_t n, size_t size, gfp_t flags)
{
	struct sensor_firmware_res *res;
	void *p;

	if (n == 0)
		return NULL;

	res = devm_kzalloc(ctx->dev, sizeof(struct sensor_firmware_res), GFP_KERNEL);
	if (!res)
		return NULL;

	p = devm_kcalloc(ctx->dev, n, size, flags);
	if (!p) {
		devm_kfree(ctx->dev, res);
		return NULL;
	}

	res->res_ptr = p;
	list_add(&res->list, &fw->res_list);

	return p;
}

static inline void *ctx_fw_kzalloc(struct adaptor_ctx *ctx, struct sensor_firmware *fw,
				size_t size, gfp_t flags)
{
	struct sensor_firmware_res *res;
	void *p;

	res = devm_kzalloc(ctx->dev, sizeof(struct sensor_firmware_res), GFP_KERNEL);
	if (!res)
		return NULL;

	p = devm_kzalloc(ctx->dev, size, flags);
	if (!p) {
		devm_kfree(ctx->dev, res);
		return NULL;
	}

	res->res_ptr = p;
	list_add(&res->list, &fw->res_list);

	return p;
}

static inline void ctx_fw_kfree(struct adaptor_ctx *ctx, struct sensor_firmware *fw, const void *p)
{
	struct sensor_firmware_res *res = NULL, *r = NULL;

	list_for_each_entry_safe(res, r, &fw->res_list, list) {
		if (res->res_ptr == p) {
			devm_kfree(ctx->dev, res->res_ptr);
			res->res_ptr = NULL;
			list_del(&res->list);
			devm_kfree(ctx->dev, res);
		}
	}
}

static char *get_firmware_path(struct adaptor_ctx *ctx, const char *folder, const char *file_name)
{
	int num = 0;
	int full_path_len = 0;
	char *full_path = NULL;

	if (unlikely(folder == NULL || file_name == NULL)) {
		pr_err("invalid parameter, 'folder' or 'file_name' is null\n");
		return NULL;
	}

	full_path_len = strlen(folder) + strlen(file_name) + 6;

	full_path = devm_kcalloc(ctx->dev, full_path_len, sizeof(char), GFP_KERNEL);
	if (!full_path)
		return NULL;

	num = snprintf(full_path, full_path_len, "%s/%s.bin",
		       folder, file_name);
	if (num < 0) {
		pr_err("string formating failed\n");
		devm_kfree(ctx->dev, full_path);
		full_path = NULL;
		return NULL;
	}

	return full_path;
}

static int add_fw_list(struct list_head *list, const char *file_name, const int len)
{
	struct sensor_firmware *item = NULL;
	size_t buf_sz;

	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (!item)
		return -ENOMEM;

	buf_sz = sizeof(item->name) - 1;

	if (len > buf_sz) {
		pr_info("firmware name size %d is larger than sizelimit %lu.\n",
			len, buf_sz);
		return -EINVAL;
	}

	strncpy(item->name, file_name, buf_sz);
	item->name[buf_sz] = '\0';  /* ensure null terminated */
	INIT_LIST_HEAD(&item->res_list);
	list_add_tail(&item->list, list);

	pr_info("firmware %s has been added.", item->name);

	return 0;
}

static bool actor(struct dir_context *ctx, const char *name, int namlen,
				loff_t offset, u64 ino, unsigned int d_type)
{
	/* ignore "." and ".." dir */
	if (namlen == 1 && name[0] == '.')
		return true;
	if (namlen == 2 && name[0] == '.' && name[1] == '.')
		return true;

	if (!add_fw_list(&all_sensor_fw_list, name, namlen))
		all_sensor_fw_list_cnt++;

	return true;
}

static int lookup_all_fw_list(struct sensor_firmware_loader * const loader)
{
	struct file *file;
	struct path path;
	struct dir_context ctx = { .actor = actor, };
	struct sensor_firmware *firmware;
	int err;

	if (!all_sensor_fw_list_cnt) {
		pr_info("imgsensor_firmware: query all fw list\n");

		err = kern_path(IMGSENSOR_FW_PATH, LOOKUP_DIRECTORY, &path);
		if (err) {
			pr_err("Error getting path: %d\n", err);
			return err;
		}

		file = dentry_open(&path, O_RDONLY, current_cred());
		if (IS_ERR(file)) {
			pr_err("Error opening directory\n");
			return PTR_ERR(file);
		}

		err = iterate_dir(file, &ctx);
		if (err) {
			pr_err("Error iterating directory\n");
			return err;
		}

		fput(file);
	}

	list_for_each_entry(firmware, &all_sensor_fw_list, list) {
		add_fw_list(&loader->fw_list, firmware->name, strlen(firmware->name));
	}

	return 0;
}

static bool check_firmware_exist(struct adaptor_ctx *ctx, const char *file_name)
{
	bool ret = false;
	struct path path;
	char *full_path = NULL;

	if (!file_name) {
		pr_err("invalid parameter 'file_name' is null\n");
		return ret;
	}

	full_path = get_firmware_path(ctx, IMGSENSOR_FW_PATH, file_name);
	if (full_path) {
		if (kern_path(full_path, 0, &path) == 0) {
			/* file exist */
			path_put(&path);
			ret = true;
		}
	}

	devm_kfree(ctx->dev, full_path);
	full_path = NULL;

	return ret;
}

int lookup_firmwares(struct adaptor_ctx *ctx,
		     struct sensor_firmware_loader * const loader,
		     const char *fw_file_name[], int count)
{
	int i;
	struct sensor_firmware *firmware;

	if (unlikely(loader == NULL))
		return -EINVAL;

	if (loader->fw_list_inited) {
		pr_info("The firmware has been lookup\n");
		return 0;
	}

	INIT_LIST_HEAD(&loader->fw_list);
	loader->fw_list_inited = true;

	if (count == 0) {
		/* add all fw in folder to list */
		lookup_all_fw_list(loader);
		return 0;
	}

	if (!fw_file_name)
		return -EINVAL;

	for (i = 0; i < count; i++) {
		pr_info("imgsensor_fw lookup firmwares '%s' ret = %d\n",
			fw_file_name[i],
			check_firmware_exist(ctx, fw_file_name[i]));
		if (check_firmware_exist(ctx, fw_file_name[i]))
			add_fw_list(&loader->fw_list, fw_file_name[i], strlen(fw_file_name[i]));
	}

	/* print list */
	i = 0;
	list_for_each_entry(firmware, &loader->fw_list, list) {
		pr_info("imgsensor_fw probe firmware list[%d] = %s\n", i, firmware->name);
		i++;
	}

	return 0;
}

int deinit_firmware_loader(struct adaptor_ctx *ctx, struct sensor_firmware_loader *loader)
{
	struct sensor_firmware *firmware = NULL, *n = NULL;
	struct sensor_firmware_res *res = NULL, *r = NULL;

	if (unlikely(loader == NULL))
		return -EINVAL;

	loader->fw_list_inited = false;

	list_for_each_entry_safe(firmware, n, &loader->fw_list, list) {
		list_del(&firmware->list);

		list_for_each_entry_safe(res, r, &firmware->res_list, list) {
			list_del(&res->list);
			devm_kfree(ctx->dev, res);
		}
		kfree(firmware); /* free alloc by kzalloc */
	}

	return 0;
}

typedef int (*init_section)(struct adaptor_ctx *ctx, struct sensor_firmware *sensor_fw,
			const u8 *data, const size_t size,
			void *dest, size_t dest_sz);
typedef int (*update_subdrv_entry)(struct adaptor_ctx *ctx, struct sensor_firmware *sensor_fw,
			void *dest, size_t dest_sz);
typedef int (*release_section)(struct adaptor_ctx *ctx, struct sensor_firmware *sensor_fw,
			void *dest, size_t dest_sz);

static int init_header_section(struct adaptor_ctx *ctx, struct sensor_firmware *sensor_fw,
			const u8 *data, const size_t size,
			void *dest, size_t dest_sz)
{
	int ret = 0;

	if (unlikely(data == NULL || dest == NULL))
		return -EINVAL;

	if (unlikely(size < dest_sz))
		return -EINVAL;

	if (sizeof(struct fw_header) != dest_sz)
		return -EINVAL;

	/*
	 * the data struct don't have pointer
	 * so copy it directly
	 */
	memcpy(dest, data, dest_sz);

	return ret;
}

static int update_subdrv_entry_header(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			void *dest, size_t dest_sz)
{
	int ret = 0;
	struct fw_header *fw_struct;

	if (unlikely(dest == NULL))
		return -EINVAL;
	if (sizeof(struct fw_header) != dest_sz)
		return -EINVAL;

	fw_struct = (struct fw_header *)dest;

	if (ctx->subdrv) {
		size_t name_sz = sizeof(fw_struct->sensor_name);
		char *sname = ctx_fw_kzalloc(ctx, sensor_fw, name_sz, GFP_KERNEL);

		if (!sname)
			return -ENOMEM;
		memcpy(sname, fw_struct->sensor_name, name_sz);

		ctx->subdrv->name = sname;
		ctx->subdrv->id = fw_struct->sensor_id;
		ctx->subdrv->fw_major_ver = fw_struct->major_version;
		ctx->subdrv->fw_revision = fw_struct->revision;
		ctx->subdrv->fw_modified_ts = fw_struct->last_modified_ts;

		/* Check generator version */
		if (strncmp(fw_struct->generator_version, SUPPORT_GENERATOR_VERSION, NAME_LENGTH_MAX)) {
			/* Check fail, stop loading */
			fw_struct->generator_version[NAME_LENGTH_MAX - 1] = '\0';
			pr_err("support generator version is '%s', but loaded version '%s'\n",
			       SUPPORT_GENERATOR_VERSION,
			       fw_struct->generator_version);
			return -EINVAL;
		}
	}

	return ret;
}

static int release_header_section(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			void *dest, size_t dest_sz)
{
	int ret = 0;

	if (unlikely(dest == NULL))
		return -EINVAL;
	if (sizeof(struct fw_header) != dest_sz)
		return -EINVAL;

	/*
	 * the data struct don't have pointer
	 * nothing to be released
	 */

	adaptor_logi(ctx, "resource release done\n");

	return ret;
}

static int init_pw_seq_section(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			const u8 *data, const size_t size,
			void *dest, size_t dest_sz)
{
	int ret = 0;
	struct fw_pw_seq *pdata;

	if (unlikely(data == NULL || dest == NULL))
		return -EINVAL;

	if (unlikely(size < dest_sz))
		return -EINVAL;

	if (sizeof(struct fw_pw_seq) != dest_sz)
		return -EINVAL;

	pdata = (struct fw_pw_seq *)dest;

	/* normal power seq */
	memcpy(&pdata->pw_seq_cnt, data, sizeof(pdata->pw_seq_cnt));
	data += sizeof(pdata->pw_seq_cnt);

	if (pdata->pw_seq_cnt) {
		pdata->pw_seq = ctx_fw_kcalloc(ctx, sensor_fw, pdata->pw_seq_cnt,
					sizeof(struct fw_subdrv_pw_seq_entry),
					GFP_KERNEL);
		if (!pdata->pw_seq)
			return -ENOMEM;

		memcpy(pdata->pw_seq, data,
		       sizeof(struct fw_subdrv_pw_seq_entry) * pdata->pw_seq_cnt);
		data += sizeof(struct fw_subdrv_pw_seq_entry) * pdata->pw_seq_cnt;
	}

	/* aov power seq */
	memcpy(&pdata->aov_pw_seq_cnt, data, sizeof(pdata->aov_pw_seq_cnt));
	data += sizeof(pdata->aov_pw_seq_cnt);

	if (pdata->aov_pw_seq_cnt) {
		pdata->aov_pw_seq = ctx_fw_kcalloc(ctx, sensor_fw, pdata->aov_pw_seq_cnt,
					sizeof(struct fw_subdrv_pw_seq_entry),
					GFP_KERNEL);
		if (!pdata->aov_pw_seq)
			return -ENOMEM;

		memcpy(pdata->aov_pw_seq, data,
		       sizeof(struct fw_subdrv_pw_seq_entry) * pdata->aov_pw_seq_cnt);
		data += sizeof(struct fw_subdrv_pw_seq_entry) * pdata->aov_pw_seq_cnt;
	}

	/* hw2sw_standby power seq */
	memcpy(&pdata->hw2sw_standby_pw_seq_cnt, data,
		sizeof(pdata->hw2sw_standby_pw_seq_cnt));
	data += sizeof(pdata->hw2sw_standby_pw_seq_cnt);

	if (pdata->hw2sw_standby_pw_seq_cnt) {
		pdata->hw2sw_standby_pw_seq = ctx_fw_kcalloc(ctx, sensor_fw,
			pdata->hw2sw_standby_pw_seq_cnt,
			sizeof(struct fw_subdrv_pw_seq_entry),
			GFP_KERNEL);
		if (!pdata->hw2sw_standby_pw_seq)
			return -ENOMEM;

		memcpy(pdata->hw2sw_standby_pw_seq, data,
			sizeof(struct fw_subdrv_pw_seq_entry) * pdata->hw2sw_standby_pw_seq_cnt);
		data += sizeof(struct fw_subdrv_pw_seq_entry) * pdata->hw2sw_standby_pw_seq_cnt;
	}

	return ret;
}

static int update_subdrv_entry_pw_seq(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			void *dest, size_t dest_sz)
{
	int ret = 0;
	struct fw_pw_seq *fw_struct;
	struct fw_subdrv_pw_seq_entry *pfw;
	struct subdrv_pw_seq_entry *pseq;
	int i;

	if (unlikely(dest == NULL))
		return -EINVAL;
	if (sizeof(struct fw_pw_seq) != dest_sz)
		return -EINVAL;

	fw_struct = (struct fw_pw_seq *)dest;

	if (ctx->subdrv) {
		/* Normal power seq */
		struct subdrv_pw_seq_entry *pw_seq = ctx_fw_kzalloc(ctx, sensor_fw,
				sizeof(struct subdrv_pw_seq_entry) * fw_struct->pw_seq_cnt,
				GFP_KERNEL);

		if (!pw_seq)
			return -ENOMEM;

		for (i = 0, pfw = fw_struct->pw_seq, pseq = pw_seq;
		     i < fw_struct->pw_seq_cnt;
		     i++, pfw++, pseq++) {
			pseq->id = pfw->id;
			pseq->val.para1 = pfw->val.para1;
			pseq->val.para2 = pfw->val.para2;
			pseq->delay = pfw->delay;
		}

		ctx->subdrv->pw_seq_cnt = fw_struct->pw_seq_cnt;
		ctx->subdrv->pw_seq = pw_seq;

		/* AOV power seq */
		if (fw_struct->aov_pw_seq_cnt) {
			struct subdrv_pw_seq_entry *aov_pw_seq = ctx_fw_kzalloc(ctx, sensor_fw,
					sizeof(struct subdrv_pw_seq_entry) * fw_struct->aov_pw_seq_cnt,
					GFP_KERNEL);

			if (!aov_pw_seq)
				return -ENOMEM;

			for (i = 0, pfw = fw_struct->aov_pw_seq, pseq = aov_pw_seq;
			     i < fw_struct->aov_pw_seq_cnt;
			     i++, pfw++, pseq++) {
				pseq->id = pfw->id;
				pseq->val.para1 = pfw->val.para1;
				pseq->val.para2 = pfw->val.para2;
				pseq->delay = pfw->delay;
			}

			ctx->subdrv->aov_pw_seq_cnt = fw_struct->aov_pw_seq_cnt;
			ctx->subdrv->aov_pw_seq = aov_pw_seq;
		}

		/* HW2SW power seq */
		if (fw_struct->hw2sw_standby_pw_seq_cnt) {
			struct subdrv_pw_seq_entry *hw2sw_standby_pw_seq = ctx_fw_kzalloc(ctx, sensor_fw,
					sizeof(struct subdrv_pw_seq_entry) * fw_struct->hw2sw_standby_pw_seq_cnt,
					GFP_KERNEL);

			if (!hw2sw_standby_pw_seq)
				return -ENOMEM;

			for (i = 0, pfw = fw_struct->hw2sw_standby_pw_seq, pseq = hw2sw_standby_pw_seq;
			     i < fw_struct->hw2sw_standby_pw_seq_cnt;
			     i++, pfw++, pseq++) {
				pseq->id = pfw->id;
				pseq->val.para1 = pfw->val.para1;
				pseq->val.para2 = pfw->val.para2;
				pseq->delay = pfw->delay;
			}

			ctx->subdrv->hw2sw_standby_pw_seq_cnt = fw_struct->hw2sw_standby_pw_seq_cnt;
			ctx->subdrv->hw2sw_standby_pw_seq = hw2sw_standby_pw_seq;
		}
	}

	return ret;
}

static int release_pw_seq_section(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			void *dest, size_t dest_sz)
{
	int ret = 0;
	struct fw_pw_seq *pdata;

	if (unlikely(dest == NULL))
		return -EINVAL;
	if (sizeof(struct fw_pw_seq) != dest_sz)
		return -EINVAL;

	pdata = (struct fw_pw_seq *)dest;

	/* free resource */
	if (pdata->pw_seq) {
		ctx_fw_kfree(ctx, sensor_fw, pdata->pw_seq);
		pdata->pw_seq = NULL;
	}
	if (pdata->aov_pw_seq) {
		ctx_fw_kfree(ctx, sensor_fw, pdata->aov_pw_seq);
		pdata->aov_pw_seq = NULL;
	}
	if (pdata->hw2sw_standby_pw_seq) {
		ctx_fw_kfree(ctx, sensor_fw, pdata->hw2sw_standby_pw_seq);
		pdata->hw2sw_standby_pw_seq = NULL;
	}

	adaptor_logi(ctx, "resource release done\n");

	return ret;
}

static int init_eeprom_info_section(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			const u8 *data, const size_t size,
			void *dest, size_t dest_sz)
{
	int ret = 0;
	int i;
	struct fw_eeprom_infos *pdata;
	struct fw_eeprom_info_struct *p;
	u32 offset = 0;
	u32 sz = 0;

	if (unlikely(data == NULL || dest == NULL))
		return -EINVAL;

	if (unlikely(size < dest_sz))
		return -EINVAL;

	if (sizeof(struct fw_eeprom_infos) != dest_sz)
		return -EINVAL;

	pdata = (struct fw_eeprom_infos *)dest;

	sz = sizeof(struct fw_eeprom_infos) - sizeof(struct fw_eeprom_info_struct *);
	memcpy(pdata, data, sz);

	offset += sz;

	if (!pdata->eeprom_info_num)
		return ret;

	pdata->eeprom_info_list = ctx_fw_kcalloc(ctx, sensor_fw,
				pdata->eeprom_info_num,
				sizeof(struct fw_eeprom_info_struct), GFP_KERNEL);

	if (!pdata->eeprom_info_list)
		return -ENOMEM;

	for (i = 0; i < pdata->eeprom_info_num; i++) {
		sz = sizeof(struct fw_eeprom_info_struct) - sizeof(struct fw_eeprom_info_dynamic_size);
		memcpy(pdata->eeprom_info_list + i, data + offset, sz);
		offset += sz;

		p = pdata->eeprom_info_list + i;

		if (p->qsc_table_size) {
			sz = sizeof(u8) * p->qsc_table_size;
			p->dynamic.qsc_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
			if (!p->dynamic.qsc_table)
				return -ENOMEM;

			memcpy(p->dynamic.qsc_table, data + offset, sz);
			offset += sz;
		}
		if (p->pdc_table_size) {
			sz = sizeof(u8) * p->pdc_table_size;
			p->dynamic.pdc_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
			if (!p->dynamic.pdc_table)
				return -ENOMEM;

			memcpy(p->dynamic.pdc_table, data + offset, sz);
			offset += sz;
		}
		if (p->lrc_table_size) {
			sz = sizeof(u8) * p->lrc_table_size;
			p->dynamic.lrc_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
			if (!p->dynamic.lrc_table)
				return -ENOMEM;

			memcpy(p->dynamic.lrc_table, data + offset, sz);
			offset += sz;
		}
		if (p->xtalk_table_size) {
			sz = sizeof(u8) * p->xtalk_table_size;
			p->dynamic.xtalk_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
			if (!p->dynamic.xtalk_table)
				return -ENOMEM;

			memcpy(p->dynamic.xtalk_table, data + offset, sz);
			offset += sz;
		}
	}

	return ret;
}

static int update_s_ctx_eeprom_info(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			struct eeprom_info_struct *pinfo, struct fw_eeprom_info_struct *fw_struct)
{
	size_t sz;

	COPY_COMMON_MEMBER(pinfo, fw_struct, header_id);
	COPY_COMMON_MEMBER(pinfo, fw_struct, addr_header_id);
	COPY_COMMON_MEMBER(pinfo, fw_struct, i2c_write_id);

	COPY_COMMON_MEMBER(pinfo, fw_struct, qsc_support);
	COPY_COMMON_MEMBER(pinfo, fw_struct, qsc_size);
	COPY_COMMON_MEMBER(pinfo, fw_struct, addr_qsc);
	COPY_COMMON_MEMBER(pinfo, fw_struct, sensor_reg_addr_qsc);

	COPY_COMMON_MEMBER(pinfo, fw_struct, pdc_support);
	COPY_COMMON_MEMBER(pinfo, fw_struct, pdc_size);
	COPY_COMMON_MEMBER(pinfo, fw_struct, addr_pdc);
	COPY_COMMON_MEMBER(pinfo, fw_struct, sensor_reg_addr_pdc);

	COPY_COMMON_MEMBER(pinfo, fw_struct, lrc_support);
	COPY_COMMON_MEMBER(pinfo, fw_struct, lrc_size);
	COPY_COMMON_MEMBER(pinfo, fw_struct, addr_lrc);
	COPY_COMMON_MEMBER(pinfo, fw_struct, sensor_reg_addr_lrc);

	COPY_COMMON_MEMBER(pinfo, fw_struct, xtalk_support);
	COPY_COMMON_MEMBER(pinfo, fw_struct, xtalk_size);
	COPY_COMMON_MEMBER(pinfo, fw_struct, addr_xtalk);
	COPY_COMMON_MEMBER(pinfo, fw_struct, sensor_reg_addr_xtalk);

	if (fw_struct->qsc_table_size) {
		sz = fw_struct->qsc_table_size * sizeof(u8);
		pinfo->qsc_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pinfo->qsc_table)
			return -ENOMEM;
		memcpy(pinfo->qsc_table,
		       fw_struct->dynamic.qsc_table,
		       sz);

		/* override size with table size */
		COPY_SPECIFIC_MEMBER(pinfo, fw_struct,
			     qsc_size, qsc_table_size);
	}

	if (fw_struct->pdc_table_size) {
		sz = fw_struct->pdc_table_size * sizeof(u8);
		pinfo->pdc_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pinfo->pdc_table)
			return -ENOMEM;
		memcpy(pinfo->pdc_table,
		       fw_struct->dynamic.pdc_table,
		       sz);

		/* override size with table size */
		COPY_SPECIFIC_MEMBER(pinfo, fw_struct,
			     pdc_size, pdc_table_size);
	}

	if (fw_struct->lrc_table_size) {
		sz = fw_struct->lrc_table_size * sizeof(u8);
		pinfo->lrc_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pinfo->lrc_table)
			return -ENOMEM;
		memcpy(pinfo->lrc_table,
		       fw_struct->dynamic.lrc_table,
		       sz);

		/* override size with table size */
		COPY_SPECIFIC_MEMBER(pinfo, fw_struct,
			     lrc_size, lrc_table_size);
	}

	if (fw_struct->xtalk_table_size) {
		sz = fw_struct->xtalk_table_size * sizeof(u8);
		pinfo->xtalk_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pinfo->xtalk_table)
			return -ENOMEM;
		memcpy(pinfo->xtalk_table,
		       fw_struct->dynamic.xtalk_table,
		       sz);

		/* override size with table size */
		COPY_SPECIFIC_MEMBER(pinfo, fw_struct,
			     xtalk_size, xtalk_table_size);
	}

	return 0;
}

static int update_subdrv_entry_eeprom_info(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			void *dest, size_t dest_sz)
{
	int i, ret = 0;
	struct fw_eeprom_infos *fw_struct;
	size_t sz_eeproms;
	struct eeprom_info_struct *ptr;

	if (unlikely(dest == NULL))
		return -EINVAL;
	if (sizeof(struct fw_eeprom_infos) != dest_sz)
		return -EINVAL;

	fw_struct = (struct fw_eeprom_infos *)dest;

	COPY_SPECIFIC_MEMBER(&ctx->subctx.s_ctx, fw_struct,
			     eeprom_num, eeprom_info_num);

	if (fw_struct->eeprom_info_num) {
		sz_eeproms = fw_struct->eeprom_info_num * sizeof(struct eeprom_info_struct);
		ctx->subctx.s_ctx.eeprom_info = ctx_fw_kzalloc(ctx, sensor_fw, sz_eeproms, GFP_KERNEL);
		if (!ctx->subctx.s_ctx.eeprom_info)
			return -ENOMEM;
	}

	for (i = 0; i < fw_struct->eeprom_info_num; i++) {
		ptr = ctx->subctx.s_ctx.eeprom_info + i;
		update_s_ctx_eeprom_info(ctx, sensor_fw, ptr, fw_struct->eeprom_info_list + i);
	}

	return ret;
}

static int release_eeprom_info_section(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			void *dest, size_t dest_sz)
{
	int ret = 0;
	struct fw_eeprom_infos *pdata;
	struct fw_eeprom_info_struct *p;
	int i;

	if (unlikely(dest == NULL))
		return -EINVAL;
	if (sizeof(struct fw_eeprom_infos) != dest_sz)
		return -EINVAL;

	pdata = (struct fw_eeprom_infos *)dest;

	/* release resource */
	if (pdata->eeprom_info_list) {
		for (i = 0; i < pdata->eeprom_info_num; i++) {
			p = pdata->eeprom_info_list + i;

			if (p->dynamic.qsc_table) {
				ctx_fw_kfree(ctx, sensor_fw, p->dynamic.qsc_table);
				p->dynamic.qsc_table = NULL;
			}
			if (p->dynamic.pdc_table) {
				ctx_fw_kfree(ctx, sensor_fw, p->dynamic.pdc_table);
				p->dynamic.pdc_table = NULL;
			}
			if (p->dynamic.lrc_table) {
				ctx_fw_kfree(ctx, sensor_fw, p->dynamic.lrc_table);
				p->dynamic.lrc_table = NULL;
			}
			if (p->dynamic.xtalk_table) {
				ctx_fw_kfree(ctx, sensor_fw, p->dynamic.xtalk_table);
				p->dynamic.xtalk_table = NULL;
			}
		}

		ctx_fw_kfree(ctx, sensor_fw, pdata->eeprom_info_list);
		pdata->eeprom_info_list = NULL;
	}

	adaptor_logi(ctx, "resource release done\n");

	return ret;
}

static int init_sensor_global_info_section(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			const u8 *data, const size_t size,
			void *dest, size_t dest_sz)
{
	int ret = 0;
	struct fw_sensor_global_info *pdata;
	u32 offset = 0;
	u32 sz = 0;

	if (unlikely(data == NULL || dest == NULL))
		return -EINVAL;

	if (unlikely(size < dest_sz))
		return -EINVAL;

	if (sizeof(struct fw_sensor_global_info) != dest_sz)
		return -EINVAL;

	pdata = (struct fw_sensor_global_info *)dest;

	sz = sizeof(struct fw_sensor_global_info) -
		sizeof(struct fw_sensor_global_info_dynamic_size);
	memcpy(pdata, data, sz);

	offset += sz;

	if (pdata->ana_gain_table_cnt) {
		sz = sizeof(u32) * pdata->ana_gain_table_cnt;
		pdata->dynamic.ana_gain_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pdata->dynamic.ana_gain_table)
			return -ENOMEM;

		memcpy(pdata->dynamic.ana_gain_table, data + offset, sz);
		offset += sz;
	}
	if (pdata->has_saturation_info) {
		sz = sizeof(struct fw_mtk_sensor_saturation_info);
		pdata->dynamic.saturation_info = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pdata->dynamic.saturation_info)
			return -ENOMEM;

		memcpy(pdata->dynamic.saturation_info, data + offset, sz);
		offset += sz;
	}
	if (pdata->has_ctle_param) {
		sz = sizeof(struct fw_mtk_sensor_ctle_param);
		pdata->dynamic.ctle_param = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pdata->dynamic.ctle_param)
			return -ENOMEM;

		memcpy(pdata->dynamic.ctle_param, data + offset, sz);
		offset += sz;
	}
	if (pdata->has_insertion_loss) {
		sz = sizeof(struct fw_mtk_sensor_insertion_loss);
		pdata->dynamic.insertion_loss = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pdata->dynamic.insertion_loss)
			return -ENOMEM;

		memcpy(pdata->dynamic.insertion_loss, data + offset, sz);
		offset += sz;
	}
	if (pdata->init_setting_table_cnt) {
		int i;
		u32 sz2, sz3;

		sz = sizeof(struct fw_reg_setting_entry) * pdata->init_setting_table_cnt;
		pdata->dynamic.init_setting_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pdata->dynamic.init_setting_table)
			return -ENOMEM;

		sz2 = sizeof(struct fw_reg_setting_entry) - sizeof(struct fw_reg_setting_values);

		for (i = 0; i < pdata->init_setting_table_cnt; i++) {
			memcpy(&pdata->dynamic.init_setting_table[i], data + offset, sz2);
			offset += sz2;

			sz3 = sizeof(u16) * pdata->dynamic.init_setting_table[i].setting_table_len;
			pdata->dynamic.init_setting_table[i].reg_addr_value.setting_table =
				ctx_fw_kzalloc(ctx, sensor_fw, sz3, GFP_KERNEL);

			if (!pdata->dynamic.init_setting_table[i].reg_addr_value.setting_table)
				return -ENOMEM;

			memcpy(pdata->dynamic.init_setting_table[i].reg_addr_value.setting_table,
			       data + offset, sz3);
			offset += sz3;
		}
	}
	if (pdata->i3c_precfg_setting_len) {
		sz = sizeof(u16) * pdata->i3c_precfg_setting_len;
		pdata->dynamic.i3c_precfg_setting_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pdata->dynamic.i3c_precfg_setting_table)
			return -ENOMEM;

		memcpy(pdata->dynamic.i3c_precfg_setting_table, data + offset, sz);
		offset += sz;
	}
	if (pdata->cust_global_data_len) {
		sz = sizeof(char) * (pdata->cust_global_data_len);
		pdata->dynamic.cust_global_data = ctx_fw_kzalloc(ctx, sensor_fw, sz + sizeof(char), GFP_KERNEL);
		if (!pdata->dynamic.cust_global_data)
			return -ENOMEM;

		memcpy(pdata->dynamic.cust_global_data, data + offset, sz);
		offset += sz;
	}

	return ret;
}

static int update_s_ctx(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			struct subdrv_static_ctx *s_ctx, struct fw_sensor_global_info *fw_struct)
{
	int i;
	size_t sz, sz2;

	COPY_SPECIFIC_MEMBER(s_ctx, fw_struct, sensor_id, sensor_id_match);

	memcpy(s_ctx->reg_addr_sensor_id.addr,
	       fw_struct->reg_addr_header_id.addr,
	       sizeof(s_ctx->reg_addr_sensor_id.addr));

	COPY_COMMON_MEMBER(s_ctx, fw_struct, i2c_burst_write_support);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, i2c_transfer_data_type);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, mirror);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, sensor_interface_type);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, mipi_sensor_type);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, mipi_lane_num);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, ob_pedestal);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, line_interleave_num);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, sensor_output_dataformat);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, ana_gain_def);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, ana_gain_min);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, ana_gain_max);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, ana_gain_type);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, ana_gain_step);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, dig_gain_min);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, dig_gain_max);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, dig_gain_step);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, tuning_iso_base);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, exposure_def);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, exposure_min);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, exposure_max);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, exposure_step);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, exposure_margin);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, frame_length_max);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, frame_length_max_without_lshift);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, ae_effective_frame);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, frame_time_delay_frame);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, start_exposure_offset);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, start_exposure_offset_custom);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, pdaf_type);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, hdr_type);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, rgbw_support);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, seamless_switch_support);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, seamless_switch_type);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, seamless_switch_hw_re_init_time_ns);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, seamless_switch_prsh_hw_fixed_value);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, seamless_switch_prsh_length_lc);

	memcpy(s_ctx->reg_addr_prsh_length_lines.addr,
	       fw_struct->reg_addr_prsh_length_lines.addr,
	       sizeof(s_ctx->reg_addr_prsh_length_lines.addr));

	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_prsh_mode);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, temperature_support);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_stream);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_mirror_flip);

	for (i = 0; i < MAX_EXPOSURE_CNT; i++) {
		memcpy(s_ctx->reg_addr_exposure[i].addr,
		       fw_struct->reg_addr_exposure[i].addr,
		       sizeof(s_ctx->reg_addr_exposure[i].addr));
		memcpy(s_ctx->reg_addr_exposure_in_lut[i].addr,
		       fw_struct->reg_addr_exposure_in_lut[i].addr,
		       sizeof(s_ctx->reg_addr_exposure_in_lut[i].addr));
	}

	COPY_COMMON_MEMBER(s_ctx, fw_struct, fll_lshift_max);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, cit_lshift_max);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, long_exposure_support);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_exposure_lshift);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_frame_length_lshift);

	for (i = 0; i < MAX_EXPOSURE_CNT; i++) {
		memcpy(s_ctx->reg_addr_ana_gain[i].addr,
		       fw_struct->reg_addr_ana_gain[i].addr,
		       sizeof(s_ctx->reg_addr_ana_gain[i].addr));
		memcpy(s_ctx->reg_addr_ana_gain_in_lut[i].addr,
		       fw_struct->reg_addr_ana_gain_in_lut[i].addr,
		       sizeof(s_ctx->reg_addr_ana_gain_in_lut[i].addr));
		memcpy(s_ctx->reg_addr_dig_gain[i].addr,
		       fw_struct->reg_addr_dig_gain[i].addr,
		       sizeof(s_ctx->reg_addr_dig_gain[i].addr));
		memcpy(s_ctx->reg_addr_dig_gain_in_lut[i].addr,
		       fw_struct->reg_addr_dig_gain_in_lut[i].addr,
		       sizeof(s_ctx->reg_addr_dig_gain_in_lut[i].addr));
	}

	memcpy(s_ctx->reg_addr_frame_length.addr,
	       fw_struct->reg_addr_frame_length.addr,
	       sizeof(s_ctx->reg_addr_frame_length.addr));

	for (i = 0; i < MAX_EXPOSURE_CNT; i++) {
		memcpy(s_ctx->reg_addr_frame_length_in_lut[i].addr,
		       fw_struct->reg_addr_frame_length_in_lut[i].addr,
		       sizeof(s_ctx->reg_addr_frame_length_in_lut[i].addr));
	}

	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_temp_en);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_temp_read);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_auto_extend);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_frame_count);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_fast_mode);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_dcg_ratio);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_fast_mode_in_lbmf);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_stream_in_lbmf);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, chk_s_off_sta);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, chk_s_off_end);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, checksum_value);

	COPY_COMMON_MEMBER(s_ctx, fw_struct, aov_sensor_support);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, aov_csi_clk);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, sensor_mode_ops);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, sensor_debug_sensing_ut_on_scp);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, sensor_debug_dphy_global_timing_continuous_clk);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_aov_mode_mirror_flip);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, init_in_open);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, streaming_ctrl_imp);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, custom_stream_ctrl_delay);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, cycle_base_ratio);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, stagger_rg_order);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, stagger_fl_type);

	COPY_COMMON_MEMBER(s_ctx, fw_struct, use_mcss_gph_sync);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_mcss_slave_add_en_2nd);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_mcss_slave_add_acken_2nd);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_mcss_controller_target_sel);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_mcss_xvs_io_ctrl);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_mcss_extout_en);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_mcss_sgmsync_sel);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_mcss_swdio_io_ctrl);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_mcss_gph_sync_mode);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_mcss_complete_sleep_en);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_mcss_mc_frm_lp_en);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_mcss_frm_length_reflect_timing);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, reg_addr_mcss_mc_frm_mask_num);
	COPY_COMMON_MEMBER(s_ctx, fw_struct, ocl_info);

	/* ana_gain_table */
	if (fw_struct->ana_gain_table_cnt) {
		sz = fw_struct->ana_gain_table_cnt * sizeof(u32);
		s_ctx->ana_gain_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!s_ctx->ana_gain_table)
			return -ENOMEM;

		s_ctx->ana_gain_table_size = sz;
		memcpy(s_ctx->ana_gain_table,
		       fw_struct->dynamic.ana_gain_table,
		       sz);
	}

	/* saturation_info */
	if (fw_struct->has_saturation_info) {
		sz = sizeof(struct mtk_sensor_saturation_info);
		s_ctx->saturation_info = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!s_ctx->saturation_info)
			return -ENOMEM;

		COPY_COMMON_MEMBER(s_ctx->saturation_info,
				   fw_struct->dynamic.saturation_info,
				   gain_ratio);
		COPY_COMMON_MEMBER(s_ctx->saturation_info,
				   fw_struct->dynamic.saturation_info,
				   OB_pedestal);
		COPY_COMMON_MEMBER(s_ctx->saturation_info,
				   fw_struct->dynamic.saturation_info,
				   saturation_level);
		COPY_COMMON_MEMBER(s_ctx->saturation_info,
				   fw_struct->dynamic.saturation_info,
				   adc_bit);
		COPY_COMMON_MEMBER(s_ctx->saturation_info,
				   fw_struct->dynamic.saturation_info,
				   ob_bm);
	}

	/* ctle param */
	if (fw_struct->has_ctle_param) {
		sz = sizeof(struct mtk_sensor_ctle_param);
		s_ctx->ctle_param = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!s_ctx->ctle_param)
			return -ENOMEM;

		COPY_COMMON_MEMBER(s_ctx->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   eq_latch_en);
		COPY_COMMON_MEMBER(s_ctx->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   eq_dg1_en);
		COPY_COMMON_MEMBER(s_ctx->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   eq_dg0_en);
		COPY_COMMON_MEMBER(s_ctx->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   cdr_delay);
		COPY_COMMON_MEMBER(s_ctx->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   eq_is);
		COPY_COMMON_MEMBER(s_ctx->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   eq_bw);
		COPY_COMMON_MEMBER(s_ctx->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   eq_sr0);
		COPY_COMMON_MEMBER(s_ctx->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   eq_sr1);
	}

	/* insertion loss */
	if (fw_struct->has_insertion_loss) {
		sz = sizeof(struct mtk_sensor_insertion_loss);
		s_ctx->insertion_loss = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!s_ctx->insertion_loss)
			return -ENOMEM;

		COPY_COMMON_MEMBER(s_ctx->insertion_loss,
				   fw_struct->dynamic.insertion_loss,
				   loss);
	}

	/* init_setting_table */
	COPY_SPECIFIC_MEMBER(s_ctx, fw_struct, init_setting_table_v2_cnt, init_setting_table_cnt);
	if (fw_struct->init_setting_table_cnt) {
		sz = fw_struct->init_setting_table_cnt * sizeof(struct reg_setting_entry);
		s_ctx->init_setting_table_v2 = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!s_ctx->init_setting_table_v2)
			return -ENOMEM;

		for (i = 0; i < fw_struct->init_setting_table_cnt; i++) {
			COPY_COMMON_MEMBER(s_ctx->init_setting_table_v2 + i,
					fw_struct->dynamic.init_setting_table + i,
					i2c_transfer_tlb_data_type);
			COPY_COMMON_MEMBER(s_ctx->init_setting_table_v2 + i,
					fw_struct->dynamic.init_setting_table + i,
					setting_table_len);
			COPY_COMMON_MEMBER(s_ctx->init_setting_table_v2 + i,
					fw_struct->dynamic.init_setting_table + i,
					delay);

			sz2 = fw_struct->dynamic.init_setting_table[i].setting_table_len * sizeof(u16);
			s_ctx->init_setting_table_v2[i].setting_table = ctx_fw_kzalloc(ctx, sensor_fw, sz2, GFP_KERNEL);
			if (!s_ctx->init_setting_table_v2[i].setting_table)
				return -ENOMEM;

			memcpy(s_ctx->init_setting_table_v2[i].setting_table,
			       fw_struct->dynamic.init_setting_table[i].reg_addr_value.setting_table,
			       sz2);

		}
	}

	/* i3c preconfig setting table */
	COPY_COMMON_MEMBER(s_ctx, fw_struct, i3c_precfg_setting_len);
	if (fw_struct->i3c_precfg_setting_len) {
		sz = fw_struct->i3c_precfg_setting_len * sizeof(u16);
		s_ctx->i3c_precfg_setting_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!s_ctx->i3c_precfg_setting_table)
			return -ENOMEM;
		memcpy(s_ctx->i3c_precfg_setting_table,
		       fw_struct->dynamic.i3c_precfg_setting_table,
		       sz);
	}

	/* reserved custom field */
	if (fw_struct->cust_global_data_len && fw_struct->dynamic.cust_global_data) {
		sz = fw_struct->cust_global_data_len * sizeof(char);
		s_ctx->cust_global_data = ctx_fw_kzalloc(ctx, sensor_fw, sz + sizeof(char), GFP_KERNEL);
		if (!s_ctx->cust_global_data)
			return -ENOMEM;

		COPY_COMMON_MEMBER(s_ctx, fw_struct, cust_global_data_len);
		memcpy(s_ctx->cust_global_data,
		       fw_struct->dynamic.cust_global_data,
		       sz);
	}

	return 0;
}

static int update_subdrv_entry_sensor_global(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			void *dest, size_t dest_sz)
{
	int ret = 0;
	struct fw_sensor_global_info *fw_struct;

	if (unlikely(dest == NULL))
		return -EINVAL;
	if (sizeof(struct fw_sensor_global_info) != dest_sz)
		return -EINVAL;

	fw_struct = (struct fw_sensor_global_info *)dest;

	update_s_ctx(ctx, sensor_fw, &ctx->subctx.s_ctx, fw_struct);

	return ret;
}

static int release_sensor_global_info_section(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			void *dest, size_t dest_sz)
{
	int ret = 0;
	struct fw_sensor_global_info *pdata;

	if (unlikely(dest == NULL))
		return -EINVAL;
	if (sizeof(struct fw_sensor_global_info) != dest_sz)
		return -EINVAL;

	pdata = (struct fw_sensor_global_info *)dest;

	/* release source */
	if (pdata->dynamic.ana_gain_table) {
		ctx_fw_kfree(ctx, sensor_fw, pdata->dynamic.ana_gain_table);
		pdata->dynamic.ana_gain_table = NULL;
	}
	if (pdata->dynamic.saturation_info) {
		ctx_fw_kfree(ctx, sensor_fw, pdata->dynamic.saturation_info);
		pdata->dynamic.saturation_info = NULL;
	}
	if (pdata->dynamic.init_setting_table) {
		if (pdata->dynamic.init_setting_table->reg_addr_value.setting_table) {
			ctx_fw_kfree(ctx, sensor_fw, pdata->dynamic.init_setting_table->reg_addr_value.setting_table);
			pdata->dynamic.init_setting_table->reg_addr_value.setting_table = NULL;
		}

		ctx_fw_kfree(ctx, sensor_fw, pdata->dynamic.init_setting_table);
		pdata->dynamic.init_setting_table = NULL;
	}
	if (pdata->dynamic.cust_global_data) {
		ctx_fw_kfree(ctx, sensor_fw, pdata->dynamic.cust_global_data);
		pdata->dynamic.cust_global_data = NULL;
	}

	adaptor_logi(ctx, "resource release done\n");

	return ret;
}

static int init_mode_info_section(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			const u8 *data, const size_t size,
			void *dest, size_t dest_sz)
{
	int ret = 0;
	int i;
	struct fw_modes *pdata;
	struct fw_mode_info *p;
	u32 offset = 0;
	u32 sz = 0;

	if (unlikely(data == NULL || dest == NULL))
		return -EINVAL;

	if (unlikely(size < dest_sz))
		return -EINVAL;

	if (sizeof(struct fw_modes) != dest_sz)
		return -EINVAL;

	pdata = (struct fw_modes *)dest;

	sz = sizeof(struct fw_modes) - sizeof(struct fw_mode_info *);
	memcpy(pdata, data, sz);

	offset += sz;

	if (!pdata->mode_num)
		return ret;

	pdata->mode_list = ctx_fw_kcalloc(ctx, sensor_fw, pdata->mode_num, sizeof(struct fw_mode_info), GFP_KERNEL);

	if (!pdata->mode_list)
		return -ENOMEM;

	for (i = 0; i < pdata->mode_num; i++) {
		sz = sizeof(struct fw_mode_info) - sizeof(struct fw_mode_info_dynamic_size);
		memcpy(pdata->mode_list + i, data + offset, sz);
		offset += sz;

		p = pdata->mode_list + i;

		if (p->has_imgsensor_pd_info) {
			u32 sz2;

			sz = sizeof(struct fw_set_pd_block_info_t);
			p->dynamic.imgsensor_pd_info = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
			if (!p->dynamic.imgsensor_pd_info)
				return -ENOMEM;

			sz2 = sz - sizeof(struct fw_set_pd_block_info_t_dynamic_size);

			memcpy(p->dynamic.imgsensor_pd_info, data + offset, sz2);
			offset += sz2;

			if (p->dynamic.imgsensor_pd_info->i4PosL_cnt) {
				sz = (p->dynamic.imgsensor_pd_info->i4PosL_cnt) *
					sizeof(struct fw_pd_u32_pair);

				p->dynamic.imgsensor_pd_info->dynamic.i4PosL = ctx_fw_kzalloc(ctx, sensor_fw,
											sz, GFP_KERNEL);
				if (!p->dynamic.imgsensor_pd_info->dynamic.i4PosL)
					return -ENOMEM;

				memcpy(p->dynamic.imgsensor_pd_info->dynamic.i4PosL, data + offset, sz);
				offset += sz;
			}
			if (p->dynamic.imgsensor_pd_info->i4PosR_cnt) {
				sz = (p->dynamic.imgsensor_pd_info->i4PosR_cnt) *
					sizeof(struct fw_pd_u32_pair);

				p->dynamic.imgsensor_pd_info->dynamic.i4PosR = ctx_fw_kzalloc(ctx, sensor_fw,
											sz, GFP_KERNEL);
				if (!p->dynamic.imgsensor_pd_info->dynamic.i4PosR)
					return -ENOMEM;

				memcpy(p->dynamic.imgsensor_pd_info->dynamic.i4PosR, data + offset, sz);
				offset += sz;
			}
			if (p->dynamic.imgsensor_pd_info->i4Crop_cnt) {
				sz = (p->dynamic.imgsensor_pd_info->i4Crop_cnt) *
					sizeof(struct fw_pd_u32_pair);

				p->dynamic.imgsensor_pd_info->dynamic.i4Crop = ctx_fw_kzalloc(ctx, sensor_fw,
											sz, GFP_KERNEL);
				if (!p->dynamic.imgsensor_pd_info->dynamic.i4Crop)
					return -ENOMEM;

				memcpy(p->dynamic.imgsensor_pd_info->dynamic.i4Crop, data + offset, sz);
				offset += sz;
			}
			if (p->dynamic.imgsensor_pd_info->sPDMapInfo_cnt) {
				int j;

				sz = (p->dynamic.imgsensor_pd_info->sPDMapInfo_cnt) *
					sizeof(struct fw_pd_map_info_t);

				p->dynamic.imgsensor_pd_info->dynamic.sPDMapInfo = ctx_fw_kzalloc(ctx, sensor_fw,
											sz, GFP_KERNEL);
				if (!p->dynamic.imgsensor_pd_info->dynamic.sPDMapInfo)
					return -ENOMEM;

				for (j = 0; j < p->dynamic.imgsensor_pd_info->sPDMapInfo_cnt; j++) {
					struct fw_pd_map_info_t *map_info =
						p->dynamic.imgsensor_pd_info->dynamic.sPDMapInfo + j;

					sz = sizeof(struct fw_pd_map_info_t) - sizeof(u32 *);
					memcpy(map_info, data + offset, sz);
					offset += sz;

					if (!map_info->i4PDOrder_cnt) {
						/* no pd order, skip to next pd map info */
						continue;
					}

					sz = (map_info->i4PDOrder_cnt) * sizeof(u32);
					map_info->i4PDOrder = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
					if (!map_info->i4PDOrder)
						return -ENOMEM;

					memcpy(map_info->i4PDOrder, data + offset, sz);
					offset += sz;
				}
			}
		}
		if (p->has_saturation_info) {
			sz = sizeof(struct fw_mtk_sensor_saturation_info);
			p->dynamic.saturation_info = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
			if (!p->dynamic.saturation_info)
				return -ENOMEM;

			memcpy(p->dynamic.saturation_info, data + offset, sz);
			offset += sz;
		}
		if (p->has_dcg_info) {
			u32 sz2;

			sz = sizeof(struct fw_dcg_info_struct);
			p->dynamic.dcg_info = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
			if (!p->dynamic.dcg_info)
				return -ENOMEM;

			sz2 = (sz - sizeof(u32 *));

			memcpy(p->dynamic.dcg_info, data + offset, sz2);
			offset += sz2;

			if (p->dynamic.dcg_info->dcg_gain_table_cnt) {
				sz = sizeof(u32) * p->dynamic.dcg_info->dcg_gain_table_cnt;
				p->dynamic.dcg_info->dcg_gain_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
				if (!p->dynamic.dcg_info->dcg_gain_table)
					return -ENOMEM;

				memcpy(p->dynamic.dcg_info->dcg_gain_table, data + offset, sz);
				offset += sz;
			}
		}
		if (p->has_ctle_param) {
			sz = sizeof(struct fw_mtk_sensor_ctle_param);
			p->dynamic.ctle_param = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
			if (!p->dynamic.ctle_param)
				return -ENOMEM;

			memcpy(p->dynamic.ctle_param, data + offset, sz);
			offset += sz;
		}
		if (p->frame_desc_num) {
			sz = sizeof(struct fw_mtk_mbus_frame_desc_entry_csi2) * (p->frame_desc_num);
			p->dynamic.frame_desc = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
			if (!p->dynamic.frame_desc)
				return -ENOMEM;

			memcpy(p->dynamic.frame_desc, data + offset, sz);
			offset += sz;
		}
		if (p->mode_setting_len) {
			sz = sizeof(u16) * p->mode_setting_len;
			p->dynamic.mode_setting_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
			if (!p->dynamic.mode_setting_table)
				return -ENOMEM;

			memcpy(p->dynamic.mode_setting_table, data + offset, sz);
			offset += sz;
		}
		if (p->mode_setting_len_for_md) {
			sz = sizeof(u16) * p->mode_setting_len_for_md;
			p->dynamic.mode_setting_table_for_md = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
			if (!p->dynamic.mode_setting_table_for_md)
				return -ENOMEM;

			memcpy(p->dynamic.mode_setting_table_for_md, data + offset, sz);
			offset += sz;
		}
		if (p->seamless_switch_mode_setting_len) {
			sz = sizeof(u16) * p->seamless_switch_mode_setting_len;
			p->dynamic.seamless_switch_mode_setting_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
			if (!p->dynamic.seamless_switch_mode_setting_table)
				return -ENOMEM;

			memcpy(p->dynamic.seamless_switch_mode_setting_table, data + offset, sz);
			offset += sz;
		}
		if (p->cust_sensor_mode_data_len) {
			sz = sizeof(char) * (p->cust_sensor_mode_data_len);
			p->dynamic.cust_sensor_mode_data = ctx_fw_kzalloc(ctx, sensor_fw,
									sz + sizeof(char), GFP_KERNEL);
			if (!p->dynamic.cust_sensor_mode_data)
				return -ENOMEM;

			memcpy(p->dynamic.cust_sensor_mode_data, data + offset, sz);
			offset += sz;
		}
	}

	return ret;
}

static int update_s_ctx_mode(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			struct subdrv_mode_struct *pmode, struct fw_mode_info *fw_struct)
{
	int i, j;
	size_t sz;
	struct mtk_mbus_frame_desc_entry *p_fd;

	COPY_COMMON_MEMBER(pmode, fw_struct, mode_setting_len);
	COPY_COMMON_MEMBER(pmode, fw_struct, mode_setting_len_for_md);
	COPY_COMMON_MEMBER(pmode, fw_struct, seamless_switch_mode_setting_len);
	COPY_COMMON_MEMBER(pmode, fw_struct, seamless_switch_group);
	COPY_COMMON_MEMBER(pmode, fw_struct, hdr_mode);
	COPY_COMMON_MEMBER(pmode, fw_struct, raw_cnt);
	COPY_COMMON_MEMBER(pmode, fw_struct, exp_cnt);

	COPY_COMMON_MEMBER(pmode, fw_struct, pclk);
	COPY_COMMON_MEMBER(pmode, fw_struct, linelength);
	COPY_COMMON_MEMBER(pmode, fw_struct, framelength);
	COPY_COMMON_MEMBER(pmode, fw_struct, max_framerate);
	COPY_COMMON_MEMBER(pmode, fw_struct, mipi_pixel_rate);
	COPY_COMMON_MEMBER(pmode, fw_struct, readout_length);
	COPY_COMMON_MEMBER(pmode, fw_struct, read_margin);
	COPY_COMMON_MEMBER(pmode, fw_struct, framelength_step);
	COPY_COMMON_MEMBER(pmode, fw_struct, coarse_integ_step);
	COPY_COMMON_MEMBER(pmode, fw_struct, min_exposure_line);
	COPY_COMMON_MEMBER(pmode, fw_struct, min_vblanking_line);
	COPY_COMMON_MEMBER(pmode, fw_struct, exposure_margin);

	/* imgsensor_winsize_info */
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.full_w);
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.full_h);
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.x0_offset);
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.y0_offset);
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.w0_size);
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.h0_size);
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.scale_w);
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.scale_h);
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.x1_offset);
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.y1_offset);
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.w1_size);
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.h1_size);
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.x2_tg_offset);
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.y2_tg_offset);
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.w2_tg_size);
	COPY_COMMON_MEMBER(pmode, fw_struct, imgsensor_winsize_info.h2_tg_size);

	COPY_COMMON_MEMBER(pmode, fw_struct, pdaf_cap);
	COPY_COMMON_MEMBER(pmode, fw_struct, ae_binning_ratio);
	COPY_COMMON_MEMBER(pmode, fw_struct, fine_integ_line);
	COPY_COMMON_MEMBER(pmode, fw_struct, delay_frame);

	COPY_SPECIFIC_MEMBER(pmode, fw_struct, num_entries, frame_desc_num);

	/* csi_param */
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.dphy_trail);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.dphy_data_settle);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.dphy_clk_settle);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.cphy_settle);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.legacy_phy);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.not_fixed_trail_settle);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.dphy_csi2_resync_dmy_cycle);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.not_fixed_dphy_settle);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.dphy_init_deskew_support);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.dphy_periodic_deskew_support);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.dphy_lrte_support);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.cphy_lrte_support);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.dphy_alp_support);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.cphy_alp_support);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.dphy_ulps_support);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.cphy_ulps_support);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.clk_lane_no_initial_flow);
	COPY_COMMON_MEMBER(pmode, fw_struct, csi_param.initial_skew);


	COPY_COMMON_MEMBER(pmode, fw_struct, sensor_output_dataformat);
	COPY_COMMON_MEMBER(pmode, fw_struct, sensor_output_dataformat_cell_type);
	COPY_COMMON_MEMBER(pmode, fw_struct, rgbw_output_mode);
	COPY_COMMON_MEMBER(pmode, fw_struct, dig_gain_min);
	COPY_COMMON_MEMBER(pmode, fw_struct, dig_gain_max);
	COPY_COMMON_MEMBER(pmode, fw_struct, dig_gain_step);
	COPY_COMMON_MEMBER(pmode, fw_struct, dpc_enabled);
	COPY_COMMON_MEMBER(pmode, fw_struct, pdc_enabled);
	COPY_COMMON_MEMBER(pmode, fw_struct, awb_enabled);

	for (i = 0; i < MAX_EXPOSURE_CNT; i++) {
		COPY_COMMON_MEMBER(&pmode->multi_exposure_ana_gain_range[i],
				   &fw_struct->multi_exposure_ana_gain_range[i],
				   min);
		COPY_COMMON_MEMBER(&pmode->multi_exposure_ana_gain_range[i],
				   &fw_struct->multi_exposure_ana_gain_range[i],
				   max);
		COPY_COMMON_MEMBER(&pmode->multi_exposure_shutter_range[i],
				   &fw_struct->multi_exposure_shutter_range[i],
				   min);
		COPY_COMMON_MEMBER(&pmode->multi_exposure_shutter_range[i],
				   &fw_struct->multi_exposure_shutter_range[i],
				   max);

		COPY_COMMON_MEMBER(&pmode->multiexp_s_info[i],
				   &fw_struct->multiexp_s_info[i],
				   belong_to_lut_id);
		COPY_COMMON_MEMBER(&pmode->multiexp_s_info[i],
				   &fw_struct->multiexp_s_info[i],
				   coarse_integ_step);
		COPY_COMMON_MEMBER(&pmode->multiexp_s_info[i],
				   &fw_struct->multiexp_s_info[i],
				   exposure_margin);
		COPY_COMMON_MEMBER(&pmode->multiexp_s_info[i],
				   &fw_struct->multiexp_s_info[i],
				   ae_binning_ratio);
		COPY_COMMON_MEMBER(&pmode->multiexp_s_info[i],
				   &fw_struct->multiexp_s_info[i],
				   fine_integ_line);
		COPY_COMMON_MEMBER(&pmode->multiexp_s_info[i],
				   &fw_struct->multiexp_s_info[i],
				   dig_gain_min);
		COPY_COMMON_MEMBER(&pmode->multiexp_s_info[i],
				   &fw_struct->multiexp_s_info[i],
				   dig_gain_max);
		COPY_COMMON_MEMBER(&pmode->multiexp_s_info[i],
				   &fw_struct->multiexp_s_info[i],
				   dig_gain_step);
	}

	for (i = 0; i < MAX_LUT_CNT; i++) {
		COPY_COMMON_MEMBER(&pmode->mode_lut_s_info[i],
				   &fw_struct->mode_lut_s_info[i],
				   pclk);
		COPY_COMMON_MEMBER(&pmode->mode_lut_s_info[i],
				   &fw_struct->mode_lut_s_info[i],
				   linelength);
		COPY_COMMON_MEMBER(&pmode->mode_lut_s_info[i],
				   &fw_struct->mode_lut_s_info[i],
				   framelength);
		COPY_COMMON_MEMBER(&pmode->mode_lut_s_info[i],
				   &fw_struct->mode_lut_s_info[i],
				   readout_length);
		COPY_COMMON_MEMBER(&pmode->mode_lut_s_info[i],
				   &fw_struct->mode_lut_s_info[i],
				   read_margin);
		COPY_COMMON_MEMBER(&pmode->mode_lut_s_info[i],
				   &fw_struct->mode_lut_s_info[i],
				   framelength_step);
		COPY_COMMON_MEMBER(&pmode->mode_lut_s_info[i],
				   &fw_struct->mode_lut_s_info[i],
				   min_vblanking_line);
	}

	COPY_COMMON_MEMBER(pmode, fw_struct, awb_enabled);
	COPY_COMMON_MEMBER(pmode, fw_struct, aov_mode);
	COPY_COMMON_MEMBER(pmode, fw_struct, rosc_mode);
	COPY_COMMON_MEMBER(pmode, fw_struct, s_dummy_support);
	COPY_COMMON_MEMBER(pmode, fw_struct, ae_ctrl_support);
	COPY_COMMON_MEMBER(pmode, fw_struct, exposure_order_in_lbmf);
	COPY_COMMON_MEMBER(pmode, fw_struct, exposure_order_in_dcg);
	COPY_COMMON_MEMBER(pmode, fw_struct, mode_type_in_lbmf);
	COPY_COMMON_MEMBER(pmode, fw_struct, sw_fl_delay);
	COPY_COMMON_MEMBER(pmode, fw_struct, support_mcss);
	COPY_COMMON_MEMBER(pmode, fw_struct, is_stick_hdr_vsync);
	COPY_COMMON_MEMBER(pmode, fw_struct, force_wr_mode_setting);

	/* imgsensor_pd_info */
	if (fw_struct->has_imgsensor_pd_info) {
		sz = sizeof(struct SET_PD_BLOCK_INFO_T);
		pmode->imgsensor_pd_info = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pmode->imgsensor_pd_info)
			return -ENOMEM;

		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4OffsetX);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4OffsetY);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4PitchX);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4PitchY);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4PairNum);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4SubBlkW);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4SubBlkH);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   iMirrorFlip);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4BlockNumX);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4BlockNumY);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4LeFirst);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4VolumeX);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4VolumeY);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4FullRawW);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4FullRawH);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4VCPackNum);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4ModeIndex);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   i4NoTrs);
		COPY_COMMON_MEMBER(pmode->imgsensor_pd_info,
				   fw_struct->dynamic.imgsensor_pd_info,
				   PDAF_Support);

		for (i = 0; (fw_struct->dynamic.imgsensor_pd_info->dynamic.i4PosL) &&
		     (i < PD_POS_MAX) && (i < fw_struct->dynamic.imgsensor_pd_info->i4PosL_cnt);
		     i++) {
			pmode->imgsensor_pd_info->i4PosL[i][0] =
				fw_struct->dynamic.imgsensor_pd_info->dynamic.i4PosL[i].para1;
			pmode->imgsensor_pd_info->i4PosL[i][1] =
				fw_struct->dynamic.imgsensor_pd_info->dynamic.i4PosL[i].para2;
		}
		for (i = 0; (fw_struct->dynamic.imgsensor_pd_info->dynamic.i4PosR) &&
		     (i < PD_POS_MAX) && (i < fw_struct->dynamic.imgsensor_pd_info->i4PosR_cnt);
		     i++) {
			pmode->imgsensor_pd_info->i4PosR[i][0] =
				fw_struct->dynamic.imgsensor_pd_info->dynamic.i4PosR[i].para1;
			pmode->imgsensor_pd_info->i4PosR[i][1] =
				fw_struct->dynamic.imgsensor_pd_info->dynamic.i4PosR[i].para2;
		}
		for (i = 0; (fw_struct->dynamic.imgsensor_pd_info->dynamic.i4Crop) &&
		     (i < SENSOR_SCENARIO_ID_MAX) && (i < fw_struct->dynamic.imgsensor_pd_info->i4Crop_cnt);
		     i++) {
			pmode->imgsensor_pd_info->i4Crop[i][0] =
				fw_struct->dynamic.imgsensor_pd_info->dynamic.i4Crop[i].para1;
			pmode->imgsensor_pd_info->i4Crop[i][1] =
				fw_struct->dynamic.imgsensor_pd_info->dynamic.i4Crop[i].para2;
		}
		for (i = 0; (fw_struct->dynamic.imgsensor_pd_info->dynamic.sPDMapInfo) &&
		     (i < PD_MAP_INFO_MAX) && (i < fw_struct->dynamic.imgsensor_pd_info->sPDMapInfo_cnt);
		     i++) {
			struct PD_MAP_INFO_T *pd_map_info = (pmode->imgsensor_pd_info->sPDMapInfo + i);
			struct fw_pd_map_info_t *fw_pd_map_info =
				fw_struct->dynamic.imgsensor_pd_info->dynamic.sPDMapInfo + i;

			COPY_COMMON_MEMBER(pd_map_info, fw_pd_map_info, i4VCFeature);
			COPY_COMMON_MEMBER(pd_map_info, fw_pd_map_info, i4PDPattern);
			COPY_COMMON_MEMBER(pd_map_info, fw_pd_map_info, i4BinFacX);
			COPY_COMMON_MEMBER(pd_map_info, fw_pd_map_info, i4BinFacY);
			COPY_COMMON_MEMBER(pd_map_info, fw_pd_map_info, i4PDRepetition);

			for (j = 0; (fw_pd_map_info->i4PDOrder) &&
			     (j < PD_ORDER_MAX) && (j < fw_pd_map_info->i4PDOrder_cnt);
			     j++) {
				pd_map_info->i4PDOrder[j] = fw_pd_map_info->i4PDOrder[j];
			}
		}
	}

	/* saturation_info */
	if (fw_struct->has_saturation_info) {
		sz = sizeof(struct mtk_sensor_saturation_info);
		pmode->saturation_info = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pmode->saturation_info)
			return -ENOMEM;

		COPY_COMMON_MEMBER(pmode->saturation_info,
				   fw_struct->dynamic.saturation_info,
				   gain_ratio);
		COPY_COMMON_MEMBER(pmode->saturation_info,
				   fw_struct->dynamic.saturation_info,
				   OB_pedestal);
		COPY_COMMON_MEMBER(pmode->saturation_info,
				   fw_struct->dynamic.saturation_info,
				   saturation_level);
		COPY_COMMON_MEMBER(pmode->saturation_info,
				   fw_struct->dynamic.saturation_info,
				   adc_bit);
		COPY_COMMON_MEMBER(pmode->saturation_info,
				   fw_struct->dynamic.saturation_info,
				   ob_bm);
	}

	/* dcg info */
	if (fw_struct->has_dcg_info) {
		struct dcg_info_struct *dcg_info = &pmode->dcg_info;

		COPY_COMMON_MEMBER(dcg_info, fw_struct->dynamic.dcg_info, dcg_mode);
		COPY_COMMON_MEMBER(dcg_info, fw_struct->dynamic.dcg_info, dcg_gain_mode);
		COPY_COMMON_MEMBER(dcg_info, fw_struct->dynamic.dcg_info, dcg_gain_base);
		COPY_COMMON_MEMBER(dcg_info, fw_struct->dynamic.dcg_info, dcg_gain_ratio_min);
		COPY_COMMON_MEMBER(dcg_info, fw_struct->dynamic.dcg_info, dcg_gain_ratio_max);
		COPY_COMMON_MEMBER(dcg_info, fw_struct->dynamic.dcg_info, dcg_gain_ratio_step);

		for (i = 0; i < MAX_EXPOSURE_CNT; i++)
			dcg_info->dcg_ratio_group[i] = fw_struct->dynamic.dcg_info->dcg_ratio_group[i];

		if (fw_struct->dynamic.dcg_info->dcg_gain_table_cnt) {
			sz = sizeof(u32) * fw_struct->dynamic.dcg_info->dcg_gain_table_cnt;
			dcg_info->dcg_gain_table_size = sz;
			dcg_info->dcg_gain_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
			if (!dcg_info->dcg_gain_table)
				return -ENOMEM;

			memcpy(dcg_info->dcg_gain_table,
			       fw_struct->dynamic.dcg_info->dcg_gain_table,
			       sz);
		}
	}

	/* ctle param */
	if (fw_struct->has_ctle_param) {
		sz = sizeof(struct mtk_sensor_ctle_param);
		pmode->ctle_param = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pmode->ctle_param)
			return -ENOMEM;

		COPY_COMMON_MEMBER(pmode->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   eq_latch_en);
		COPY_COMMON_MEMBER(pmode->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   eq_dg1_en);
		COPY_COMMON_MEMBER(pmode->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   eq_dg0_en);
		COPY_COMMON_MEMBER(pmode->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   cdr_delay);
		COPY_COMMON_MEMBER(pmode->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   eq_is);
		COPY_COMMON_MEMBER(pmode->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   eq_bw);
		COPY_COMMON_MEMBER(pmode->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   eq_sr0);
		COPY_COMMON_MEMBER(pmode->ctle_param,
				   fw_struct->dynamic.ctle_param,
				   eq_sr1);
	}

	/* frame desc */
	if (fw_struct->frame_desc_num) {
		sz = fw_struct->frame_desc_num * sizeof(struct mtk_mbus_frame_desc_entry);
		pmode->frame_desc = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pmode->frame_desc)
			return -ENOMEM;

		for (i = 0; i < fw_struct->frame_desc_num; i++) {
			p_fd = pmode->frame_desc + i;

			COPY_COMMON_MEMBER(&p_fd->bus.csi2,
					   fw_struct->dynamic.frame_desc + i,
					   channel);
			COPY_COMMON_MEMBER(&p_fd->bus.csi2,
					   fw_struct->dynamic.frame_desc + i,
					   data_type);
			COPY_COMMON_MEMBER(&p_fd->bus.csi2,
					   fw_struct->dynamic.frame_desc + i,
					   enable);
			COPY_COMMON_MEMBER(&p_fd->bus.csi2,
					   fw_struct->dynamic.frame_desc + i,
					   dt_remap_to_type);
			COPY_COMMON_MEMBER(&p_fd->bus.csi2,
					   fw_struct->dynamic.frame_desc + i,
					   hsize);
			COPY_COMMON_MEMBER(&p_fd->bus.csi2,
					   fw_struct->dynamic.frame_desc + i,
					   vsize);
			COPY_COMMON_MEMBER(&p_fd->bus.csi2,
					   fw_struct->dynamic.frame_desc + i,
					   user_data_desc);
			COPY_COMMON_MEMBER(&p_fd->bus.csi2,
					   fw_struct->dynamic.frame_desc + i,
					   is_sensor_hw_pre_latch_exp);
			COPY_COMMON_MEMBER(&p_fd->bus.csi2,
					   fw_struct->dynamic.frame_desc + i,
					   cust_assign_to_tsrec_exp_id);
			COPY_COMMON_MEMBER(&p_fd->bus.csi2,
					   fw_struct->dynamic.frame_desc + i,
					   valid_bit);
			COPY_COMMON_MEMBER(&p_fd->bus.csi2,
					   fw_struct->dynamic.frame_desc + i,
					   is_active_line);
			COPY_COMMON_MEMBER(&p_fd->bus.csi2,
					   fw_struct->dynamic.frame_desc + i,
					   ebd_parsing_type);
			COPY_COMMON_MEMBER(&p_fd->bus.csi2,
					   fw_struct->dynamic.frame_desc + i,
					   fs_seq);
		}
	}

	if (fw_struct->mode_setting_len) {
		sz = fw_struct->mode_setting_len * sizeof(u16);
		pmode->mode_setting_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pmode->mode_setting_table)
			return -ENOMEM;
		memcpy(pmode->mode_setting_table,
		       fw_struct->dynamic.mode_setting_table,
		       sz);
	}
	if (fw_struct->mode_setting_len_for_md) {
		sz = fw_struct->mode_setting_len_for_md * sizeof(u16);
		pmode->mode_setting_table_for_md = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pmode->mode_setting_table_for_md)
			return -ENOMEM;
		memcpy(pmode->mode_setting_table_for_md,
		       fw_struct->dynamic.mode_setting_table_for_md,
		       sz);
	}
	if (fw_struct->seamless_switch_mode_setting_len) {
		sz = fw_struct->seamless_switch_mode_setting_len * sizeof(u16);
		pmode->seamless_switch_mode_setting_table = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pmode->seamless_switch_mode_setting_table)
			return -ENOMEM;
		memcpy(pmode->seamless_switch_mode_setting_table,
		       fw_struct->dynamic.seamless_switch_mode_setting_table,
		       sz);
	}

	/* reserved custom field */
	if (fw_struct->cust_sensor_mode_data_len && fw_struct->dynamic.cust_sensor_mode_data) {
		sz = fw_struct->cust_sensor_mode_data_len * sizeof(char);
		pmode->cust_sensor_mode_data = ctx_fw_kzalloc(ctx, sensor_fw, sz + sizeof(char), GFP_KERNEL);
		if (!pmode->cust_sensor_mode_data)
			return -ENOMEM;

		COPY_COMMON_MEMBER(pmode, fw_struct, cust_sensor_mode_data_len);
		memcpy(pmode->cust_sensor_mode_data,
		       fw_struct->dynamic.cust_sensor_mode_data,
		       sz);
	}

	return 0;
}

static int update_subdrv_entry_mode_info(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			void *dest, size_t dest_sz)
{
	int i, ret = 0;
	struct fw_modes *fw_struct;
	size_t sz_modes;
	struct subdrv_mode_struct *ptr;

	if (unlikely(dest == NULL))
		return -EINVAL;
	if (sizeof(struct fw_modes) != dest_sz)
		return -EINVAL;

	fw_struct = (struct fw_modes *)dest;

	COPY_SPECIFIC_MEMBER(&ctx->subctx.s_ctx, fw_struct,
			     sensor_mode_num, mode_num);

	if (fw_struct->mode_num) {
		sz_modes = fw_struct->mode_num * sizeof(struct subdrv_mode_struct);
		ctx->subctx.s_ctx.mode = ctx_fw_kzalloc(ctx, sensor_fw, sz_modes, GFP_KERNEL);
		if (!ctx->subctx.s_ctx.mode)
			return -ENOMEM;
	}

	for (i = 0; i < fw_struct->mode_num; i++) {
		ptr = ctx->subctx.s_ctx.mode + i;
		update_s_ctx_mode(ctx, sensor_fw, ptr, fw_struct->mode_list + i);
	}

	return ret;
}

static int release_mode_info_section(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			void *dest, size_t dest_sz)
{
	int ret = 0;
	struct fw_modes *pdata;
	struct fw_mode_info *p;
	int i;

	if (unlikely(dest == NULL))
		return -EINVAL;
	if (sizeof(struct fw_modes) != dest_sz)
		return -EINVAL;

	pdata = (struct fw_modes *)dest;

	/* release resource */
	if (pdata->mode_list) {
		for (i = 0; i < pdata->mode_num; i++) {
			p = pdata->mode_list + i;

			if (p->dynamic.imgsensor_pd_info) {
				if (p->dynamic.imgsensor_pd_info->dynamic.i4PosL) {
					ctx_fw_kfree(ctx, sensor_fw, p->dynamic.imgsensor_pd_info->dynamic.i4PosL);
					p->dynamic.imgsensor_pd_info->dynamic.i4PosL = NULL;
				}
				if (p->dynamic.imgsensor_pd_info->dynamic.i4PosR) {
					ctx_fw_kfree(ctx, sensor_fw, p->dynamic.imgsensor_pd_info->dynamic.i4PosR);
					p->dynamic.imgsensor_pd_info->dynamic.i4PosR = NULL;
				}
				if (p->dynamic.imgsensor_pd_info->dynamic.i4Crop) {
					ctx_fw_kfree(ctx, sensor_fw, p->dynamic.imgsensor_pd_info->dynamic.i4Crop);
					p->dynamic.imgsensor_pd_info->dynamic.i4Crop = NULL;
				}
				if (p->dynamic.imgsensor_pd_info->dynamic.sPDMapInfo) {
					if (p->dynamic.imgsensor_pd_info->dynamic.sPDMapInfo->i4PDOrder) {
						ctx_fw_kfree(ctx, sensor_fw,
							p->dynamic.imgsensor_pd_info->dynamic.sPDMapInfo->i4PDOrder);
						p->dynamic.imgsensor_pd_info->dynamic.sPDMapInfo->i4PDOrder = NULL;
					}

					ctx_fw_kfree(ctx, sensor_fw, p->dynamic.imgsensor_pd_info->dynamic.sPDMapInfo);
					p->dynamic.imgsensor_pd_info->dynamic.sPDMapInfo = NULL;
				}

				ctx_fw_kfree(ctx, sensor_fw, p->dynamic.imgsensor_pd_info);
				p->dynamic.imgsensor_pd_info = NULL;
			}
			if (p->dynamic.saturation_info) {
				ctx_fw_kfree(ctx, sensor_fw, p->dynamic.saturation_info);
				p->dynamic.saturation_info = NULL;
			}
			if (p->dynamic.dcg_info) {
				if (p->dynamic.dcg_info->dcg_gain_table) {
					ctx_fw_kfree(ctx, sensor_fw, p->dynamic.dcg_info->dcg_gain_table);
					p->dynamic.dcg_info->dcg_gain_table = NULL;
				}

				ctx_fw_kfree(ctx, sensor_fw, p->dynamic.dcg_info);
				p->dynamic.dcg_info = NULL;
			}
			if (p->dynamic.frame_desc) {
				ctx_fw_kfree(ctx, sensor_fw, p->dynamic.frame_desc);
				p->dynamic.frame_desc = NULL;
			}
			if (p->dynamic.mode_setting_table) {
				ctx_fw_kfree(ctx, sensor_fw, p->dynamic.mode_setting_table);
				p->dynamic.mode_setting_table = NULL;
			}
			if (p->dynamic.mode_setting_table_for_md) {
				ctx_fw_kfree(ctx, sensor_fw, p->dynamic.mode_setting_table_for_md);
				p->dynamic.mode_setting_table_for_md = NULL;
			}
			if (p->dynamic.seamless_switch_mode_setting_table) {
				ctx_fw_kfree(ctx, sensor_fw, p->dynamic.seamless_switch_mode_setting_table);
				p->dynamic.seamless_switch_mode_setting_table = NULL;
			}
			if (p->dynamic.cust_sensor_mode_data) {
				ctx_fw_kfree(ctx, sensor_fw, p->dynamic.cust_sensor_mode_data);
				p->dynamic.cust_sensor_mode_data = NULL;
			}
		}

		ctx_fw_kfree(ctx, sensor_fw, pdata->mode_list);
		pdata->mode_list = NULL;
	}

	adaptor_logi(ctx, "resource release done\n");

	return ret;
}

static int init_embedded_info_section(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			const u8 *data, const size_t size,
			void *dest, size_t dest_sz)
{
	int ret = 0;
	struct fw_ebd *pdata;
	u32 offset = 0;
	u32 sz = 0;

	if (unlikely(data == NULL || dest == NULL))
		return -EINVAL;

	if (unlikely(size < dest_sz))
		return -EINVAL;

	if (sizeof(struct fw_ebd) != dest_sz)
		return -EINVAL;

	pdata = (struct fw_ebd *)dest;

	sz = sizeof(struct fw_ebd) - sizeof(struct fw_ebd_info_struct *);
	memcpy(pdata, data, sz);
	offset += sz;

	if (pdata->has_ebd_info) {
		sz = sizeof(struct fw_ebd_info_struct);
		pdata->ebd_info = ctx_fw_kzalloc(ctx, sensor_fw, sz, GFP_KERNEL);
		if (!pdata->ebd_info)
			return -ENOMEM;

		memcpy(pdata->ebd_info, data + offset, sz);
		offset += sz;
	}

	return ret;
}

static int _update_ebd_loc(struct ebd_loc *target, struct fw_ebd_loc *src)
{
	int i;

	target->loc_line = src->loc_line;
	for (i = 0; i < MAX_EBD_PIXEL_OFFSET_NUM; i++)
		target->loc_pix[i] = src->loc_pix[i];

	return 0;
}

static int update_subdrv_entry_embedded_info(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			void *dest, size_t dest_sz)
{
	int i, ret = 0;
	struct fw_ebd *fw_struct;
	struct ebd_info_struct *ptr;

	if (unlikely(dest == NULL))
		return -EINVAL;
	if (sizeof(struct fw_ebd) != dest_sz)
		return -EINVAL;

	fw_struct = (struct fw_ebd *)dest;

	if (fw_struct->has_ebd_info && fw_struct->ebd_info) {
		ptr = &ctx->subctx.s_ctx.ebd_info;

		_update_ebd_loc(&ptr->frm_cnt_loc, &fw_struct->ebd_info->frm_cnt_loc);
		_update_ebd_loc(&ptr->coarse_integ_shift_loc, &fw_struct->ebd_info->coarse_integ_shift_loc);
		_update_ebd_loc(&ptr->dol_loc, &fw_struct->ebd_info->dol_loc);
		_update_ebd_loc(&ptr->framelength_loc, &fw_struct->ebd_info->framelength_loc);
		_update_ebd_loc(&ptr->temperature_loc, &fw_struct->ebd_info->temperature_loc);

		for (i = 0; i < MAX_EXPOSURE_CNT; i++) {
			_update_ebd_loc(&ptr->coarse_integ_loc[i],
					&fw_struct->ebd_info->coarse_integ_loc[i]);
			_update_ebd_loc(&ptr->ana_gain_loc[i],
					&fw_struct->ebd_info->ana_gain_loc[i]);
			_update_ebd_loc(&ptr->dig_gain_loc[i],
					&fw_struct->ebd_info->dig_gain_loc[i]);
		}
	}

	return ret;
}

static int release_embedded_info_section(struct adaptor_ctx *ctx,
			struct sensor_firmware *sensor_fw,
			void *dest, size_t dest_sz)
{
	int ret = 0;
	struct fw_ebd *pdata;

	if (unlikely(dest == NULL))
		return -EINVAL;
	if (sizeof(struct fw_ebd) != dest_sz)
		return -EINVAL;

	pdata = (struct fw_ebd *)dest;

	/* release resource */
	if (pdata->ebd_info) {
		ctx_fw_kfree(ctx, sensor_fw, pdata->ebd_info);
		pdata->ebd_info = NULL;
	}

	adaptor_logi(ctx, "resource release done\n");

	return ret;
}

static init_section section_fp[SECTION_MAX_NUM] = {
	[SECTION_HEADER] = init_header_section,
	[SECTION_PW_SEQ] = init_pw_seq_section,
	[SECTION_EEPROM_INFO] = init_eeprom_info_section,
	[SECTION_SENSOR_GLOBAL_INFO] = init_sensor_global_info_section,
	[SECTION_MODE_INFO] = init_mode_info_section,
	[SECTION_EMBEDDED_INFO] = init_embedded_info_section,
};
static update_subdrv_entry update_ctx_fp[SECTION_MAX_NUM] = {
	[SECTION_HEADER] = update_subdrv_entry_header,
	[SECTION_PW_SEQ] = update_subdrv_entry_pw_seq,
	[SECTION_EEPROM_INFO] = update_subdrv_entry_eeprom_info,
	[SECTION_SENSOR_GLOBAL_INFO] = update_subdrv_entry_sensor_global,
	[SECTION_MODE_INFO] = update_subdrv_entry_mode_info,
	[SECTION_EMBEDDED_INFO] = update_subdrv_entry_embedded_info,
};
static release_section release_section_fp[SECTION_MAX_NUM] = {
	[SECTION_HEADER] = release_header_section,
	[SECTION_PW_SEQ] = release_pw_seq_section,
	[SECTION_EEPROM_INFO] = release_eeprom_info_section,
	[SECTION_SENSOR_GLOBAL_INFO] = release_sensor_global_info_section,
	[SECTION_MODE_INFO] = release_mode_info_section,
	[SECTION_EMBEDDED_INFO] = release_embedded_info_section,
};

struct section {
	bool has_data;
	struct fw_section fw_sect;
	init_section init_fp;
	update_subdrv_entry update_fp;
	release_section release_fp;
	void *data;
	size_t data_sz;
};

static int parse_section(struct adaptor_ctx *ctx, const u8 *data, const size_t size,
			struct section *sect, int sect_sz)
{
	int i, j;
	u16 sect_num = 0;
	struct fw_section fw_sect;
	size_t ofs = sizeof(sect_num);

	memcpy(&sect_num, data, sizeof(sect_num));

	//adaptor_logi(ctx, "sect_num = %d\n", sect_num);

	for (i = 0; i < sect_num; i++) {
		memcpy(&fw_sect, data + ofs, sizeof(fw_sect));
		ofs += sizeof(fw_sect);

		for (j = 0; j < sect_sz; j++) {
			if (sect[j].fw_sect.flag == fw_sect.flag) {
				memcpy(&sect[j].fw_sect, &fw_sect, sizeof(fw_sect));
				sect[j].has_data = true;
				//adaptor_logi(ctx, "sect[%d] flag=%u, offset=%u, size=%u\n",
				//	     j,
				//	     sect[j].fw_sect.flag,
				//	     sect[j].fw_sect.fw_offset,
				//	     sect[j].fw_sect.fw_size);
			}
		}
	}

	return 0;
}

static int init_with_firmware(struct adaptor_ctx *ctx, struct sensor_firmware *sensor_fw,
			      const u8 *data, const size_t size)
{
	int ret = 0;
	int i;
	struct fw_header header;
	struct fw_pw_seq pw_seq;
	struct fw_eeprom_infos eeprom_infos;
	struct fw_sensor_global_info global_info;
	struct fw_modes modes;
	struct fw_ebd ebd;
	struct section sect[SECTION_MAX_NUM];
#ifdef FW_LOAD_LOG
	int j, k;
#endif

	//adaptor_logi(ctx, "data size is %zu bytes\n", size);

	/* init variable */
	memset(&header, 0, sizeof(header));
	memset(&pw_seq, 0, sizeof(pw_seq));
	memset(&eeprom_infos, 0, sizeof(eeprom_infos));
	memset(&global_info, 0, sizeof(global_info));
	memset(&modes, 0, sizeof(modes));
	memset(&ebd, 0, sizeof(ebd));

	/* init section */
	memset(sect, 0, sizeof(sect));
	for (i = 0; i < SECTION_MAX_NUM; i++) {
		sect[i].init_fp = section_fp[i];
		sect[i].update_fp = update_ctx_fp[i];
		sect[i].release_fp = release_section_fp[i];
		sect[i].fw_sect.flag = i + 1;
	}
	sect[SECTION_HEADER].data = &header;
	sect[SECTION_HEADER].data_sz = sizeof(header);
	sect[SECTION_PW_SEQ].data = &pw_seq;
	sect[SECTION_PW_SEQ].data_sz = sizeof(pw_seq);
	sect[SECTION_EEPROM_INFO].data = &eeprom_infos;
	sect[SECTION_EEPROM_INFO].data_sz = sizeof(eeprom_infos);
	sect[SECTION_SENSOR_GLOBAL_INFO].data = &global_info;
	sect[SECTION_SENSOR_GLOBAL_INFO].data_sz = sizeof(global_info);
	sect[SECTION_MODE_INFO].data = &modes;
	sect[SECTION_MODE_INFO].data_sz = sizeof(modes);
	sect[SECTION_EMBEDDED_INFO].data = &ebd;
	sect[SECTION_EMBEDDED_INFO].data_sz = sizeof(ebd);

	parse_section(ctx, data, size, sect, ARRAY_SIZE(sect));

	/* make sure s_ctx is reseted */
	memset(&ctx->subctx.s_ctx, 0, sizeof(ctx->subctx.s_ctx));

	for (i = 0; i < SECTION_MAX_NUM; i++) {
		if (sect[i].init_fp && sect[i].has_data &&
		    sect[i].init_fp(ctx, sensor_fw, data + sect[i].fw_sect.fw_offset, size,
				    sect[i].data, sect[i].data_sz) == 0) {

			if (sect[i].update_fp &&
			    sect[i].update_fp(ctx, sensor_fw, sect[i].data, sect[i].data_sz) == 0) {
				adaptor_logi(ctx, "parsing %d success\n", i);
			} else if (sect[i].update_fp) {
				/* update fp failed */
				adaptor_loge(ctx, "update fp[%d] failed\n", i);
				ret = -EINVAL;
				goto LABEL_INIT_FW_RELEASE;
			}

#ifdef FW_LOAD_LOG
			/* print log */
			switch (i) {
			case SECTION_HEADER:
				adaptor_logi(ctx, "header size of packed = %zu\n", sizeof(struct fw_header));
				adaptor_logi(ctx, "header major version = %u", header.major_version);
				adaptor_logi(ctx, "header revision = %u", header.major_version);
				adaptor_logi(ctx, "header last modified ts = %llu", header.last_modified_ts);
				adaptor_logi(ctx, "header sensor name = %s", header.sensor_name);
				adaptor_logi(ctx, "header sensor id = 0x%x", header.sensor_id);
				break;
			case SECTION_PW_SEQ:
				adaptor_logi(ctx, "pw_seq size of packed = %zu, cnt = %d, aov_cnt = %d\n",
					     sizeof(struct fw_pw_seq), pw_seq.pw_seq_cnt, pw_seq.aov_pw_seq_cnt);
				for (j = 0; j < pw_seq.pw_seq_cnt; j++) {
					adaptor_logi(ctx, "pw_seq[%d] hw_id(%d), val(%d/%d), delay(%d)\n",
						     j,
						     pw_seq.pw_seq[j].id,
						     pw_seq.pw_seq[j].val.para1,
						     pw_seq.pw_seq[j].val.para2,
						     pw_seq.pw_seq[j].delay);
				}
				for (j = 0; j < pw_seq.aov_pw_seq_cnt; j++) {
					adaptor_logi(ctx, "aov_pw_seq[%d] hw_id(%d), val(%d/%d), delay(%d)\n",
						     j,
						     pw_seq.aov_pw_seq[j].id,
						     pw_seq.aov_pw_seq[j].val.para1,
						     pw_seq.aov_pw_seq[j].val.para2,
						     pw_seq.aov_pw_seq[j].delay);
				}
				break;
			case SECTION_EEPROM_INFO:
				adaptor_logi(ctx, "eeprom info size of packed = %zu, eeprom info num = %u\n",
					     sizeof(struct fw_eeprom_infos), eeprom_infos.eeprom_info_num);
				for (j = 0; j < eeprom_infos.eeprom_info_num; j++) {
					adaptor_logi(ctx,
						"eeprom info [%d] headerId=0x%x, addrHeaderId=0x%x, i2cWid=0x%x\n",
						j,
						eeprom_infos.eeprom_info_list[j].header_id,
						eeprom_infos.eeprom_info_list[j].addr_header_id,
						eeprom_infos.eeprom_info_list[j].i2c_write_id);

					adaptor_logi(ctx,
						"eeprom info [%d] qsc supp=%d, size=%u, addr=0x%x, reg_addr=0x%x, tlb_sz=%u\n",
						j,
						eeprom_infos.eeprom_info_list[j].qsc_support,
						eeprom_infos.eeprom_info_list[j].qsc_size,
						eeprom_infos.eeprom_info_list[j].addr_qsc,
						eeprom_infos.eeprom_info_list[j].sensor_reg_addr_qsc,
						eeprom_infos.eeprom_info_list[j].qsc_table_size);

					adaptor_logi(ctx,
						"eeprom info [%d] pdc supp=%d, size=%u, addr=0x%x, reg_addr=0x%x, tlb_sz=%u\n",
						j,
						eeprom_infos.eeprom_info_list[j].pdc_support,
						eeprom_infos.eeprom_info_list[j].pdc_size,
						eeprom_infos.eeprom_info_list[j].addr_pdc,
						eeprom_infos.eeprom_info_list[j].sensor_reg_addr_pdc,
						eeprom_infos.eeprom_info_list[j].pdc_table_size);

					adaptor_logi(ctx,
						"eeprom info [%d] lrc supp=%d, size=%u, addr=0x%x, reg_addr=0x%x, tlb_sz=%u\n",
						j,
						eeprom_infos.eeprom_info_list[j].lrc_support,
						eeprom_infos.eeprom_info_list[j].lrc_size,
						eeprom_infos.eeprom_info_list[j].addr_lrc,
						eeprom_infos.eeprom_info_list[j].sensor_reg_addr_lrc,
						eeprom_infos.eeprom_info_list[j].lrc_table_size);

					adaptor_logi(ctx,
						"eeprom info [%d] xtalk supp=%d, size=%u, addr=0x%x, reg_addr=0x%x, tlb_sz=%u\n",
						j,
						eeprom_infos.eeprom_info_list[j].xtalk_support,
						eeprom_infos.eeprom_info_list[j].xtalk_size,
						eeprom_infos.eeprom_info_list[j].addr_xtalk,
						eeprom_infos.eeprom_info_list[j].sensor_reg_addr_xtalk,
						eeprom_infos.eeprom_info_list[j].xtalk_table_size);
				}
				break;
			case SECTION_SENSOR_GLOBAL_INFO:
				adaptor_logi(ctx, "global info size of packed = %zu\n",
					     sizeof(struct fw_sensor_global_info));
				adaptor_logi(ctx, "global info match sensor id = 0x%x",
					     global_info.sensor_id_match);
				for (j = 0; j < REG_ADDR_MAXCNT; j++) {
					adaptor_logi(ctx, "global info reg header id[%d] = 0x%x\n",
						     j, global_info.reg_addr_header_id.addr[j]);
				}
				adaptor_logi(ctx, "global info i2c burst=%u, type=%u, mirror=%u",
					global_info.i2c_burst_write_support,
					global_info.i2c_transfer_data_type,
					global_info.mirror);
				adaptor_logi(ctx,
					"global info if_type=%u, mipi=%u, lane=%u, ob=%u, il_num=%u, output=%u",
					global_info.sensor_interface_type,
					global_info.mipi_sensor_type,
					global_info.mipi_lane_num,
					global_info.ob_pedestal,
					global_info.line_interleave_num,
					global_info.sensor_output_dataformat);
				adaptor_logi(ctx,
					"global info ag_def=%u, ag_min=%u, ag_max=%u, ag_tp=%u, ag_step=%u, ag_cnt=%u",
					global_info.ana_gain_def,
					global_info.ana_gain_min,
					global_info.ana_gain_max,
					global_info.ana_gain_type,
					global_info.ana_gain_step,
					global_info.ana_gain_table_cnt);
				adaptor_logi(ctx,
					"global info dg_min=%u, dg_max=%u, dg_step=%u, iso=%u",
					global_info.dig_gain_min,
					global_info.dig_gain_max,
					global_info.dig_gain_step,
					global_info.tuning_iso_base);
				adaptor_logi(ctx,
					"global info exp_def=%u, exp_min=%u, exp_max=%u, exp_step=%u, exp_mg=%u",
					global_info.exposure_def,
					global_info.exposure_min,
					global_info.exposure_max,
					global_info.exposure_step,
					global_info.exposure_margin);
				adaptor_logi(ctx,
					"global info has_satu=%u, has_ctle=%u, fl_max=%u, fl_max_wo_lsft=%u, ae_eff=%u, ftd=%u, start_exp_ofs=%u, start_exp_ofs_cust=%u",
					global_info.has_saturation_info,
					global_info.has_ctle_param,
					global_info.frame_length_max,
					global_info.frame_length_max_without_lshift,
					global_info.ae_effective_frame,
					global_info.frame_time_delay_frame,
					global_info.start_exposure_offset,
					global_info.start_exposure_offset_custom);
				adaptor_logi(ctx,
					"global info pd_ty=%u, hdr_ty=%u, rgbw_sup=%u, seam_sup=%u",
					global_info.pdaf_type,
					global_info.hdr_type,
					global_info.rgbw_support,
					global_info.seamless_switch_support);
				adaptor_logi(ctx,
					"global info seam_ty=%u, seam_reinit=%u, seam_prsh_fix=%u, seam_prsh_len=%u",
					global_info.seamless_switch_type,
					global_info.seamless_switch_hw_re_init_time_ns,
					global_info.seamless_switch_prsh_hw_fixed_value,
					global_info.seamless_switch_prsh_length_lc);
				for (j = 0; j < REG_ADDR_MAXCNT; j++) {
					adaptor_logi(ctx, "global info reg prsh[%d] = 0x%x\n",
						     j, global_info.reg_addr_prsh_length_lines.addr[j]);
				}
				adaptor_logi(ctx,
					"global info reg_prsh=0x%x, tempert_sup=%u, reg_stream=0x%x, reg_mirr=0x%x",
					global_info.reg_addr_prsh_mode,
					global_info.temperature_support,
					global_info.reg_addr_stream,
					global_info.reg_addr_mirror_flip);
				for (k = 0; k < MAX_EXPOSURE_CNT; k++) {
					for (j = 0; j < REG_ADDR_MAXCNT; j++) {
						adaptor_logi(ctx, "global info reg exp[%d][%d] = 0x%x\n",
							     k, j, global_info.reg_addr_exposure[k].addr[j]);
					}
				}
				for (k = 0; k < MAX_EXPOSURE_CNT; k++) {
					for (j = 0; j < REG_ADDR_MAXCNT; j++) {
						adaptor_logi(ctx, "global info reg exp_in_lut[%d][%d] = 0x%x\n",
							     k, j, global_info.reg_addr_exposure_in_lut[k].addr[j]);
					}
				}
				adaptor_logi(ctx,
					"global info fll_lsft_max=%u, cit_lsft_max=%u, long_sup=%u, lshift=0x%x, fl_lshift=0x%x",
					global_info.fll_lshift_max,
					global_info.cit_lshift_max,
					global_info.long_exposure_support,
					global_info.reg_addr_exposure_lshift,
					global_info.reg_addr_frame_length_lshift);
				for (k = 0; k < MAX_EXPOSURE_CNT; k++) {
					for (j = 0; j < REG_ADDR_MAXCNT; j++) {
						adaptor_logi(ctx, "global info reg_addr_ana_gain[%d][%d] = 0x%x\n",
							     k, j, global_info.reg_addr_ana_gain[k].addr[j]);
					}
				}
				for (k = 0; k < MAX_EXPOSURE_CNT; k++) {
					for (j = 0; j < REG_ADDR_MAXCNT; j++) {
						adaptor_logi(ctx,
							"global info reg_addr_ana_gain_in_lut[%d][%d] = 0x%x\n",
							k, j, global_info.reg_addr_ana_gain_in_lut[k].addr[j]);
					}
				}
				for (k = 0; k < MAX_EXPOSURE_CNT; k++) {
					for (j = 0; j < REG_ADDR_MAXCNT; j++) {
						adaptor_logi(ctx,
							"global info reg_addr_dig_gain[%d][%d] = 0x%x\n",
							k, j, global_info.reg_addr_dig_gain[k].addr[j]);
					}
				}
				for (k = 0; k < MAX_EXPOSURE_CNT; k++) {
					for (j = 0; j < REG_ADDR_MAXCNT; j++) {
						adaptor_logi(ctx,
							"global info reg_addr_dig_gain_in_lut[%d][%d] = 0x%x\n",
							k, j, global_info.reg_addr_dig_gain_in_lut[k].addr[j]);
					}
				}
				for (j = 0; j < REG_ADDR_MAXCNT; j++) {
					adaptor_logi(ctx,
						"global info reg_addr_frame_length[%d] = 0x%x\n",
						j, global_info.reg_addr_frame_length.addr[j]);
				}
				for (k = 0; k < MAX_EXPOSURE_CNT; k++) {
					for (j = 0; j < REG_ADDR_MAXCNT; j++) {
						adaptor_logi(ctx,
							"global info reg_addr_frame_length_in_lut[%d][%d] = 0x%x\n",
							k, j, global_info.reg_addr_frame_length_in_lut[k].addr[j]);
					}
				}
				adaptor_logi(ctx,
					"global info reg_temp_en=0x%x, reg_temp_read=0x%x, reg_auto_ext=0x%x, reg_fcnt=0x%x, reg_fast_m=0x%x, reg_dcg_r=0x%x, reg_fast_m_lbmf=0x%x, reg_str_lbmf=0x%x",
					global_info.reg_addr_temp_en,
					global_info.reg_addr_temp_read,
					global_info.reg_addr_auto_extend,
					global_info.reg_addr_frame_count,
					global_info.reg_addr_fast_mode,
					global_info.reg_addr_dcg_ratio,
					global_info.reg_addr_fast_mode_in_lbmf,
					global_info.reg_addr_stream_in_lbmf);
				adaptor_logi(ctx,
					"global info init_set_len=%u, chk_sta=%u, chk_end=%u, checksum=0x%x",
					global_info.init_setting_table_cnt,
					global_info.chk_s_off_sta,
					global_info.chk_s_off_end,
					global_info.checksum_value);
				adaptor_logi(ctx,
					"global info aov_sensor_support=%u, aov_csi_clk=%u, sensor_mode_ops=%u, sensor_debug_sensing_ut_on_scp=%u, sensor_debug_dphy_global_timing_continuous_clk=%u",
					global_info.aov_sensor_support,
					global_info.aov_csi_clk,
					global_info.sensor_mode_ops,
					global_info.sensor_debug_sensing_ut_on_scp,
					global_info.sensor_debug_dphy_global_timing_continuous_clk);
				adaptor_logi(ctx,
					"global info reg_addr_aov_mode_mirror_flip=%u, init_in_open=%u, streaming_ctrl_imp=%u, custom_stream_ctrl_delay=%llu, cycle_base_ratio=%u, stg_rg_order=%u, stg_fl_type=%u, use_mcss_gph_sync=%u",
					global_info.reg_addr_aov_mode_mirror_flip,
					global_info.init_in_open,
					global_info.streaming_ctrl_imp,
					global_info.custom_stream_ctrl_delay,
					global_info.cycle_base_ratio,
					global_info.stagger_rg_order,
					global_info.stagger_fl_type,
					global_info.use_mcss_gph_sync);
				adaptor_logi(ctx,
					"global info reg_addr_mcss_slave_add_en_2nd=0x%x, reg_addr_mcss_slave_add_acken_2nd=0x%x, reg_addr_mcss_controller_target_sel=0x%x, reg_addr_mcss_xvs_io_ctrl=0x%x",
					global_info.reg_addr_mcss_slave_add_en_2nd,
					global_info.reg_addr_mcss_slave_add_acken_2nd,
					global_info.reg_addr_mcss_controller_target_sel,
					global_info.reg_addr_mcss_xvs_io_ctrl);
				adaptor_logi(ctx,
					"global info reg_addr_mcss_extout_en=0x%x, reg_addr_mcss_sgmsync_sel=0x%x, reg_addr_mcss_swdio_io_ctrl=0x%x, reg_addr_mcss_gph_sync_mode=0x%x",
					global_info.reg_addr_mcss_extout_en,
					global_info.reg_addr_mcss_sgmsync_sel,
					global_info.reg_addr_mcss_swdio_io_ctrl,
					global_info.reg_addr_mcss_gph_sync_mode);
				adaptor_logi(ctx,
					"global info reg_addr_mcss_complete_sleep_en=0x%x, reg_addr_mcss_mc_frm_lp_en=0x%x, reg_addr_mcss_frm_length_reflect_timing=0x%x, reg_addr_mcss_mc_frm_mask_num=0x%x, cust_glo_str_len=%u",
					global_info.reg_addr_mcss_complete_sleep_en,
					global_info.reg_addr_mcss_mc_frm_lp_en,
					global_info.reg_addr_mcss_frm_length_reflect_timing,
					global_info.reg_addr_mcss_mc_frm_mask_num,
					global_info.cust_global_data_len);
				adaptor_logi(ctx,
					"global info ocl_info %u",
					global_info.ocl_info);

				for (j = 0; j < global_info.ana_gain_table_cnt; j++) {
					adaptor_logi(ctx, "global info ana_gain_table[%d] = 0x%x\n",
						     j, global_info.dynamic.ana_gain_table[j]);
				}
				if (global_info.dynamic.saturation_info) {
					adaptor_logi(ctx, "global info saturation_info = %u / %u / %u / %u / %u\n",
						     global_info.dynamic.saturation_info->gain_ratio,
						     global_info.dynamic.saturation_info->OB_pedestal,
						     global_info.dynamic.saturation_info->saturation_level,
						     global_info.dynamic.saturation_info->adc_bit,
						     global_info.dynamic.saturation_info->ob_bm);
				}
				for (j = 0; j < global_info.init_setting_table_cnt; j++) {
					int m;

					adaptor_logi(ctx,
						"global info init_setting_table[%d] type(%u),len(%u),delay(%d)\n",
						j,
						global_info.dynamic.init_setting_table[j].i2c_transfer_tlb_data_type,
						global_info.dynamic.init_setting_table[j].setting_table_len,
						global_info.dynamic.init_setting_table[j].delay);

					for (m = 0;
					     m < global_info.dynamic.init_setting_table[j].setting_table_len;
					     m++) {
						adaptor_logi(ctx, "global info init_setting_table[%d][%d] = 0x%x\n",
							j, m,
							global_info.dynamic.init_setting_table[j]
							.reg_addr_value.setting_table[m]);
					}
				}
				if (global_info.cust_global_data_len) {
					adaptor_logi(ctx, "global info custom global data len(%u): %s\n",
						     global_info.cust_global_data_len,
						     global_info.dynamic.cust_global_data);
				}

				if (global_info.ocl_info) {
					adaptor_logi(ctx, "global info ocl_info(%u)\n",
						     global_info.ocl_info);
				}
				break;
			case SECTION_MODE_INFO:
				adaptor_logi(ctx, "mode info size of packed = %zu, mode num = %u\n",
					     sizeof(struct fw_modes), modes.mode_num);
				for (j = 0; j < modes.mode_num; j++) {
					adaptor_logi(ctx,
						"mode info [%d] m_set_len=%u, m_set_len_md=%u, seam_set_len=%u, seam_grp=%u, hdr_m=%u, raw_cnt=%u, exp_cnt=%u",
						j,
						modes.mode_list[j].mode_setting_len,
						modes.mode_list[j].mode_setting_len_for_md,
						modes.mode_list[j].seamless_switch_mode_setting_len,
						modes.mode_list[j].seamless_switch_group,
						modes.mode_list[j].hdr_mode,
						modes.mode_list[j].raw_cnt,
						modes.mode_list[j].exp_cnt);
					adaptor_logi(ctx,
						"mode info [%d] pclk=%llu, ll=%u, fl=%u, max_fps=%u, mipi_p_r=%u, readout_l=%u",
						j,
						modes.mode_list[j].pclk,
						modes.mode_list[j].linelength,
						modes.mode_list[j].framelength,
						modes.mode_list[j].max_framerate,
						modes.mode_list[j].mipi_pixel_rate,
						modes.mode_list[j].readout_length);
					adaptor_logi(ctx,
						"mode info [%d] read_m=%u, fl_step=%u, ci_step=%u, min_exp_l=%u, min_vb_l=%u, exposure_margin=%u",
						j,
						modes.mode_list[j].read_margin,
						modes.mode_list[j].framelength_step,
						modes.mode_list[j].coarse_integ_step,
						modes.mode_list[j].min_exposure_line,
						modes.mode_list[j].min_vblanking_line,
						modes.mode_list[j].exposure_margin);
					adaptor_logi(ctx,
						"mode info [%d] win full=%u/%u, x0=%u/%u/%u/%u, scal=%u/%u, x1=%u/%u/%u/%u",
						j,
						modes.mode_list[j].imgsensor_winsize_info.full_w,
						modes.mode_list[j].imgsensor_winsize_info.full_h,
						modes.mode_list[j].imgsensor_winsize_info.x0_offset,
						modes.mode_list[j].imgsensor_winsize_info.y0_offset,
						modes.mode_list[j].imgsensor_winsize_info.w0_size,
						modes.mode_list[j].imgsensor_winsize_info.h0_size,
						modes.mode_list[j].imgsensor_winsize_info.scale_w,
						modes.mode_list[j].imgsensor_winsize_info.scale_h,
						modes.mode_list[j].imgsensor_winsize_info.x1_offset,
						modes.mode_list[j].imgsensor_winsize_info.y1_offset,
						modes.mode_list[j].imgsensor_winsize_info.w1_size,
						modes.mode_list[j].imgsensor_winsize_info.h1_size);
					adaptor_logi(ctx,
						"mode info [%d] pdaf_cap=%u, ae_bin_r=%u, fine_int_l=%d, delay_f=%u, fd_num=%u",
						j,
						modes.mode_list[j].pdaf_cap,
						modes.mode_list[j].ae_binning_ratio,
						modes.mode_list[j].fine_integ_line,
						modes.mode_list[j].delay_frame,
						modes.mode_list[j].frame_desc_num);
					adaptor_logi(ctx,
						"mode info [%d] csi_para dphy_trail=%u, dphy_d_settle=%u, dphy_c_settle=%u, cphy_settle=%u, legacy=%u, n_fix_trail=%u",
						j,
						modes.mode_list[j].csi_param.dphy_trail,
						modes.mode_list[j].csi_param.dphy_data_settle,
						modes.mode_list[j].csi_param.dphy_clk_settle,
						modes.mode_list[j].csi_param.cphy_settle,
						modes.mode_list[j].csi_param.legacy_phy,
						modes.mode_list[j].csi_param.not_fixed_trail_settle);
					adaptor_logi(ctx,
						"mode info [%d] csi_para dphy_rsync=%u, n_fix_dphy_s=%u, dphy_init_deskew=%u, dphy_periodic_deskew=%u, dphy_lrte=%u, cphy_lrte=%u",
						j,
						modes.mode_list[j].csi_param.dphy_csi2_resync_dmy_cycle,
						modes.mode_list[j].csi_param.not_fixed_dphy_settle,
						modes.mode_list[j].csi_param.dphy_init_deskew_support,
						modes.mode_list[j].csi_param.dphy_periodic_deskew_support,
						modes.mode_list[j].csi_param.dphy_lrte_support,
						modes.mode_list[j].csi_param.cphy_lrte_support);
					adaptor_logi(ctx,
						"mode info [%d] csi_para dphy_alp=%u, cphy_alp=%u, dphy_ulps=%u, cphy_ulps=%u,clk_lane_no=%u, init_skew=%u",
						j,
						modes.mode_list[j].csi_param.dphy_alp_support,
						modes.mode_list[j].csi_param.cphy_alp_support,
						modes.mode_list[j].csi_param.dphy_ulps_support,
						modes.mode_list[j].csi_param.cphy_ulps_support,
						modes.mode_list[j].csi_param.clk_lane_no_initial_flow,
						modes.mode_list[j].csi_param.initial_skew);
					adaptor_logi(ctx,
						"mode info [%d] out_fmt=%u, output_fmt_cell_t=%u, rgbw_output_mode=%u, dg_min=%u, dg_max=%u, dg_step=%u, dpc_en=%u, pdc_en=%u, awb_en=%u, has_satu=%u, has_ctle=%u",
						j,
						modes.mode_list[j].sensor_output_dataformat,
						modes.mode_list[j].sensor_output_dataformat_cell_type,
						modes.mode_list[j].rgbw_output_mode,
						modes.mode_list[j].dig_gain_min,
						modes.mode_list[j].dig_gain_max,
						modes.mode_list[j].dig_gain_step,
						modes.mode_list[j].dpc_enabled,
						modes.mode_list[j].pdc_enabled,
						modes.mode_list[j].awb_enabled,
						modes.mode_list[j].has_saturation_info,
						modes.mode_list[j].has_ctle_param);

					for (k = 0; k < MAX_EXPOSURE_CNT; k++) {
						adaptor_logi(ctx, "mode info [%d] ag range[%d] = %u/%u",
							     j, k,
							     modes.mode_list[j].multi_exposure_ana_gain_range[k].min,
							     modes.mode_list[j].multi_exposure_ana_gain_range[k].max);
					}
					for (k = 0; k < MAX_EXPOSURE_CNT; k++) {
						adaptor_logi(ctx, "mode info [%d] shut range[%d] = %llu/%llu",
							     j, k,
							     modes.mode_list[j].multi_exposure_shutter_range[k].min,
							     modes.mode_list[j].multi_exposure_shutter_range[k].max);
					}
					adaptor_logi(ctx,
						"mode info [%d] aov_m=%u, rosc_m=%u, s_dum_sup=%u, ae_ctrl_sup=%u, expo_order_lbmf=%u, expo_order_dcg=%u, mode_type_lbmf=%u, sw_fl_delay=%u, supp_mcss=%u, stick_hdr_vsync=%u, force_wr_mode_setting=%u, cust_m_str_len=%u",
						j,
						modes.mode_list[j].aov_mode,
						modes.mode_list[j].rosc_mode,
						modes.mode_list[j].s_dummy_support,
						modes.mode_list[j].ae_ctrl_support,
						modes.mode_list[j].exposure_order_in_lbmf,
						modes.mode_list[j].exposure_order_in_dcg,
						modes.mode_list[j].mode_type_in_lbmf,
						modes.mode_list[j].sw_fl_delay,
						modes.mode_list[j].support_mcss,
						modes.mode_list[j].is_stick_hdr_vsync,
						modes.mode_list[j].force_wr_mode_setting,
						modes.mode_list[j].cust_sensor_mode_data_len);

					if (modes.mode_list[j].dynamic.saturation_info) {
						adaptor_logi(ctx,
							"mode info [%d] saturation_info = %u / %u / %u / %u / %u\n",
							j,
							modes.mode_list[j].dynamic.saturation_info->gain_ratio,
							modes.mode_list[j].dynamic.saturation_info->OB_pedestal,
							modes.mode_list[j].dynamic.saturation_info->saturation_level,
							modes.mode_list[j].dynamic.saturation_info->adc_bit,
							modes.mode_list[j].dynamic.saturation_info->ob_bm);
					}
					if (modes.mode_list[j].dynamic.dcg_info) {
						adaptor_logi(ctx,
							"mode info dcg_info = %u / %u / %u / %u / %u / %u, tlb_cnt(%u)\n",
							modes.mode_list[j].dynamic.dcg_info->dcg_mode,
							modes.mode_list[j].dynamic.dcg_info->dcg_gain_mode,
							modes.mode_list[j].dynamic.dcg_info->dcg_gain_base,
							modes.mode_list[j].dynamic.dcg_info->dcg_gain_ratio_min,
							modes.mode_list[j].dynamic.dcg_info->dcg_gain_ratio_max,
							modes.mode_list[j].dynamic.dcg_info->dcg_gain_ratio_step,
							modes.mode_list[j].dynamic.dcg_info->dcg_gain_table_cnt);
						adaptor_logi(ctx,
							"mode info dcg_info.ratio_group = (%u/%u/%u/%u/%u)\n",
							modes.mode_list[j].dynamic.dcg_info->dcg_ratio_group[0],
							modes.mode_list[j].dynamic.dcg_info->dcg_ratio_group[1],
							modes.mode_list[j].dynamic.dcg_info->dcg_ratio_group[2],
							modes.mode_list[j].dynamic.dcg_info->dcg_ratio_group[3],
							modes.mode_list[j].dynamic.dcg_info->dcg_ratio_group[4]);
					}
					for (k = 0;
					     (modes.mode_list[j].dynamic.dcg_info) &&
					     (k < modes.mode_list[j].dynamic.dcg_info->dcg_gain_table_cnt);
					     k++) {
						adaptor_logi(ctx,
							"mode info dcg_info gain tlb[%d] = %u\n",
							k, modes.mode_list[j].dynamic.dcg_info->dcg_gain_table[k]);
					}
					for (k = 0; k < modes.mode_list[j].frame_desc_num; k++) {
						adaptor_logi(ctx,
							"mode info [%d] fd = %u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u\n",
							j,
							modes.mode_list[j].dynamic.frame_desc[k].channel,
							modes.mode_list[j].dynamic.frame_desc[k].data_type,
							modes.mode_list[j].dynamic.frame_desc[k].enable,
							modes.mode_list[j].dynamic.frame_desc[k].dt_remap_to_type,
							modes.mode_list[j].dynamic.frame_desc[k].hsize,
							modes.mode_list[j].dynamic.frame_desc[k].vsize,
							modes.mode_list[j].dynamic.frame_desc[k].user_data_desc,
							modes.mode_list[j].dynamic.frame_desc[k]
								.is_sensor_hw_pre_latch_exp,
							modes.mode_list[j].dynamic.frame_desc[k]
								.cust_assign_to_tsrec_exp_id,
							modes.mode_list[j].dynamic.frame_desc[k].valid_bit,
							modes.mode_list[j].dynamic.frame_desc[k].is_active_line,
							modes.mode_list[j].dynamic.frame_desc[k].ebd_parsing_type,
							modes.mode_list[j].dynamic.frame_desc[k].fs_seq);
					}
					for (k = 0; k < modes.mode_list[j].mode_setting_len; k++) {
						adaptor_logi(ctx,
							"mode info [%d] mode_setting_table[%d] = 0x%x\n",
							j, k,
							modes.mode_list[j].dynamic.mode_setting_table[k]);
					}
					for (k = 0; k < modes.mode_list[j].mode_setting_len_for_md; k++) {
						adaptor_logi(ctx,
							"mode info [%d] mode_setting_table_for_md[%d] = 0x%x\n",
							j, k,
							modes.mode_list[j].dynamic.mode_setting_table_for_md[k]);
					}
					for (k = 0; k < modes.mode_list[j].seamless_switch_mode_setting_len; k++) {
						adaptor_logi(ctx,
							"mode info [%d] seamless_switch_mode_setting_table[%d] = 0x%x\n",
							j, k,
							modes.mode_list[j]
								.dynamic.seamless_switch_mode_setting_table[k]);
					}
					if (modes.mode_list[j].cust_sensor_mode_data_len) {
						adaptor_logi(ctx,
							"mode info [%d] custom mode data len(%u): %s\n",
							j,
							modes.mode_list[j].cust_sensor_mode_data_len,
							modes.mode_list[j].dynamic.cust_sensor_mode_data);
					}
				}
				break;
			}
#endif
		} else if (sect[i].init_fp && sect[i].has_data) {
			/* init fp failed */
			adaptor_loge(ctx, "init fp[%d] failed\n", i);
			ret = -EINVAL;
			goto LABEL_INIT_FW_RELEASE;
		}
	}

LABEL_INIT_FW_RELEASE:
	for (i = 0; i < SECTION_MAX_NUM; i++) {
		if (sect[i].release_fp)
			sect[i].release_fp(ctx, sensor_fw, sect[i].data, sect[i].data_sz);
	}

	return ret;
}

static void update_default_i2c_addr_table(struct adaptor_ctx *ctx)
{
	/* use default device tree i2c node reg value to i2c_addr_table */
	if (ctx->i2c_client) {
		/* i2c device. */
		ctx->subctx.s_ctx.i2c_addr_table[0] = (ctx->i2c_client->addr << 1);
		ctx->subctx.s_ctx.i2c_addr_table[1] = 0xFF;
		adaptor_logi(ctx, "default s_ctx i2c addr tlb: {0x%02x, 0x%02x}\n",
			     ctx->subctx.s_ctx.i2c_addr_table[0],
			     ctx->subctx.s_ctx.i2c_addr_table[1]);
	} else {
		/* i3c device. Fill any default value just for probe */
		ctx->subctx.s_ctx.i2c_addr_table[0] = 0x20;
		ctx->subctx.s_ctx.i2c_addr_table[1] = 0xFF;
		adaptor_logi(ctx, "default s_ctx i3c addr tlb: {0x%02x, 0x%02x}\n",
			     ctx->subctx.s_ctx.i2c_addr_table[0],
			     ctx->subctx.s_ctx.i2c_addr_table[1]);
	}
}

static bool compare_static_ctx(struct adaptor_ctx *ctx,
			       struct subdrv_static_ctx *target, struct subdrv_static_ctx *legacy)
{
	int i, j;
	int ret = 0;
	u8 target_mipi_type, legacy_mipi_type;

	if (target == NULL)
		return false;
	else if (legacy == NULL) /* ignore compare */
		return true;

	adaptor_logi(ctx, "Checking static ctx...\n");

	/* validate the s ctx */
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, sensor_id, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_sensor_id, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, i2c_addr_table, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, i2c_burst_write_support, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, i2c_transfer_data_type, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, eeprom_num, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, mirror, "global info");

	ret |= RET_IF_CHK_PTR_NULL(ctx, target, legacy, eeprom_info, "global info");
	for (i = 0; i < target->eeprom_num; i++) {
		struct eeprom_info_struct *eeprom_target = target->eeprom_info + i;
		struct eeprom_info_struct *eeprom_legacy = legacy->eeprom_info + i;

		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy, header_id, "eeprom info");
		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy, addr_header_id, "eeprom info");
		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy, i2c_write_id, "eeprom info");

		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy, qsc_support, "eeprom info");
		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy, qsc_size, "eeprom info");
		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy, addr_qsc, "eeprom info");
		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy,
					sensor_reg_addr_qsc, "eeprom info");
		ret |= RET_IF_CHK_PTR_FAIL(ctx, eeprom_target, eeprom_legacy, qsc_table,
				    sizeof(u8) * (eeprom_target->qsc_size), "eeprom info");

		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy, pdc_support, "eeprom info");
		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy, pdc_size, "eeprom info");
		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy, addr_pdc, "eeprom info");
		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy,
					sensor_reg_addr_pdc, "eeprom info");
		ret |= RET_IF_CHK_PTR_FAIL(ctx, eeprom_target, eeprom_legacy, pdc_table,
				    sizeof(u8) * (eeprom_target->pdc_size), "eeprom info");

		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy,
					lrc_support, "eeprom info");
		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy,
					lrc_size, "eeprom info");
		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy,
					addr_lrc, "eeprom info");
		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy,
					sensor_reg_addr_lrc, "eeprom info");
		ret |= RET_IF_CHK_PTR_FAIL(ctx, eeprom_target, eeprom_legacy, lrc_table,
				    sizeof(u8) * (eeprom_target->lrc_size), "eeprom info");

		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy,
					xtalk_support, "eeprom info");
		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy,
					xtalk_size, "eeprom info");
		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy,
					addr_xtalk, "eeprom info");
		ret |= RET_IF_CHK_FAIL(ctx, eeprom_target, eeprom_legacy,
					sensor_reg_addr_xtalk, "eeprom info");
		ret |= RET_IF_CHK_PTR_FAIL(ctx, eeprom_target, eeprom_legacy, xtalk_table,
				    sizeof(u8) * (eeprom_target->xtalk_size), "eeprom info");
	}

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_sensor_id, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, i2c_addr_table, "global info");

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, sensor_interface_type, "global info");

	/* both MIPI_OPHY_NCSI2 and MIPI_OPHY_CSI2 are DPHY, so it's equal */
	target_mipi_type = target->mipi_sensor_type;
	legacy_mipi_type = legacy->mipi_sensor_type;
	if (target_mipi_type == MIPI_OPHY_CSI2)
		target_mipi_type = MIPI_OPHY_NCSI2;
	if (legacy_mipi_type == MIPI_OPHY_CSI2)
		legacy_mipi_type = MIPI_OPHY_NCSI2;
	if (target_mipi_type != legacy_mipi_type) {
		adaptor_loge(ctx,
			"compare failed with field 'target->mipi_sensor_type' for global info");
		ret |= -EINVAL;
	}

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, mipi_lane_num, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, ob_pedestal, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, line_interleave_num, "global info");

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, sensor_output_dataformat, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, ana_gain_def, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, ana_gain_min, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, ana_gain_max, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, ana_gain_type, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, ana_gain_step, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, ana_gain_table_size, "global info");
	ret |= RET_IF_CHK_PTR_FAIL(ctx, target, legacy, ana_gain_table,
			target->ana_gain_table_size, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, tuning_iso_base, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, exposure_def, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, exposure_min, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, exposure_max, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, exposure_step, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, exposure_margin, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, dig_gain_min, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, dig_gain_max, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, dig_gain_step, "global info");
	ret |= RET_IF_CHK_PTR_FAIL(ctx, target, legacy, saturation_info,
			sizeof(struct mtk_sensor_saturation_info), "global info");
	ret |= RET_IF_CHK_PTR_FAIL(ctx, target, legacy, ctle_param,
			sizeof(struct mtk_sensor_ctle_param), "global info");

	ret |= RET_IF_CHK_PTR_FAIL(ctx, target, legacy, insertion_loss,
			sizeof(struct mtk_sensor_insertion_loss), "global info");

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, frame_length_max, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, frame_length_max_without_lshift, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, ae_effective_frame, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, frame_time_delay_frame, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, start_exposure_offset, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, start_exposure_offset_custom, "global info");

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, pdaf_type, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, hdr_type, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, rgbw_support, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, seamless_switch_support, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, seamless_switch_type, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy,
				seamless_switch_hw_re_init_time_ns, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy,
				seamless_switch_prsh_hw_fixed_value, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, seamless_switch_prsh_length_lc, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_prsh_length_lines, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_prsh_mode, "global info");

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, temperature_support, "global info");

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_stream, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_mirror_flip, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_exposure, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_exposure_in_lut, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, fll_lshift_max, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, cit_lshift_max, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, long_exposure_support, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_exposure_lshift, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_frame_length_lshift, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_ana_gain, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_ana_gain_in_lut, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_dig_gain, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_dig_gain_in_lut, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_frame_length, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_frame_length_in_lut, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_temp_en, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_temp_read, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_auto_extend, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_frame_count, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_fast_mode, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_dcg_ratio, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_fast_mode_in_lbmf, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_stream_in_lbmf, "global info");

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, sensor_mode_num, "global info");

	ret |= RET_IF_CHK_PTR_NULL(ctx, target, legacy, mode, "global info");
	for (i = 0; i < target->sensor_mode_num; i++) {
		struct subdrv_mode_struct *mode_target = target->mode + i;
		struct subdrv_mode_struct *mode_legacy = legacy->mode + i;
		struct dcg_info_struct *dcg_info_target = &mode_target->dcg_info;
		struct dcg_info_struct *dcg_info_legacy = &mode_legacy->dcg_info;

		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, mode_setting_len, "mode %d", i);
		ret |= RET_IF_CHK_PTR_FAIL(ctx, mode_target, mode_legacy, mode_setting_table,
				    sizeof(u16) * (mode_target->mode_setting_len), "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, mode_setting_len_for_md, "mode %d", i);
		ret |= RET_IF_CHK_PTR_FAIL(ctx, mode_target, mode_legacy, mode_setting_table_for_md,
				    sizeof(u16) * (mode_target->mode_setting_len_for_md), "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, seamless_switch_group, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy,
					seamless_switch_mode_setting_len, "mode %d", i);
		ret |= RET_IF_CHK_PTR_FAIL(ctx, mode_target, mode_legacy, seamless_switch_mode_setting_table,
				    sizeof(u16) * (mode_target->seamless_switch_mode_setting_len), "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, hdr_mode, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, raw_cnt, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, exp_cnt, "mode %d", i);

		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, pclk, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, linelength, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, framelength, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, max_framerate, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, mipi_pixel_rate, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, readout_length, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, read_margin, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, framelength_step, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, coarse_integ_step, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, min_exposure_line, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, min_vblanking_line, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, exposure_margin, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, imgsensor_winsize_info, "mode %d", i);

		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, rgbw_output_mode, "mode %d", i);

		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, aov_mode, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, rosc_mode, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, s_dummy_support, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, ae_ctrl_support, "mode %d", i);

		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, pdaf_cap, "mode %d", i);
		ret |= RET_IF_CHK_PTR_FAIL(ctx, mode_target, mode_legacy, imgsensor_pd_info,
				    sizeof(struct SET_PD_BLOCK_INFO_T), "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, ae_binning_ratio, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, fine_integ_line, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, delay_frame, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, csi_param, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, num_entries, "mode %d", i);

		ret |= RET_IF_CHK_PTR_NULL(ctx, mode_target, mode_legacy, frame_desc, "mode %d", i);
		for (j = 0; j < mode_target->num_entries; j++) {
			struct mtk_mbus_frame_desc_entry *fd_target = mode_target->frame_desc + j;
			struct mtk_mbus_frame_desc_entry *fd_legacy = mode_legacy->frame_desc + j;

			ret |= RET_IF_CHK_FAIL(ctx, fd_target, fd_legacy, bus, "mode %d", i);
		}

		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy,
					sensor_output_dataformat, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy,
					sensor_output_dataformat_cell_type, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, dig_gain_min, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, dig_gain_max, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, dig_gain_step, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy,
					multi_exposure_ana_gain_range, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy,
					multi_exposure_shutter_range, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy,
					multiexp_s_info, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy,
					mode_lut_s_info, "mode %d", i);

		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, dpc_enabled, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, pdc_enabled, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, awb_enabled, "mode %d", i);
		ret |= RET_IF_CHK_PTR_FAIL(ctx, mode_target, mode_legacy, saturation_info,
				    sizeof(struct mtk_sensor_saturation_info), "mode %d", i);
		ret |= RET_IF_CHK_PTR_FAIL(ctx, mode_target, mode_legacy, ctle_param,
				    sizeof(struct mtk_sensor_ctle_param), "mode %d", i);

		ret |= RET_IF_CHK_FAIL(ctx, dcg_info_target, dcg_info_legacy, dcg_mode,
				       "dcg_info mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, dcg_info_target, dcg_info_legacy, dcg_gain_mode,
				       "dcg_info mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, dcg_info_target, dcg_info_legacy, dcg_gain_base,
				       "dcg_info mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, dcg_info_target, dcg_info_legacy, dcg_gain_ratio_min,
				       "dcg_info mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, dcg_info_target, dcg_info_legacy, dcg_gain_ratio_max,
				       "dcg_info mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, dcg_info_target, dcg_info_legacy, dcg_gain_ratio_step,
				       "dcg_info mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, dcg_info_target, dcg_info_legacy, dcg_gain_table_size,
				       "dcg_info mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, dcg_info_target, dcg_info_legacy, dcg_ratio_group,
				       "dcg_info mode %d", i);
		ret |= RET_IF_CHK_PTR_FAIL(ctx, dcg_info_target, dcg_info_legacy, dcg_gain_table,
				dcg_info_target->dcg_gain_table_size, "dcg_info mode %d", i);

		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, exposure_order_in_lbmf, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, exposure_order_in_dcg, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, mode_type_in_lbmf, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, sw_fl_delay, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, support_mcss, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, is_stick_hdr_vsync, "mode %d", i);
		ret |= RET_IF_CHK_FAIL(ctx, mode_target, mode_legacy, force_wr_mode_setting, "mode %d", i);
	}

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, list_len, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, chk_s_off_sta, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, chk_s_off_end, "global info");

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, checksum_value, "global info");

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, aov_sensor_support, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, aov_csi_clk, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, sensor_mode_ops, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, sensor_debug_sensing_ut_on_scp, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy,
				sensor_debug_dphy_global_timing_continuous_clk, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_aov_mode_mirror_flip, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, init_in_open, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, streaming_ctrl_imp, "global info");

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, custom_stream_ctrl_delay, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, cycle_base_ratio, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, stagger_rg_order, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, stagger_fl_type, "global info");

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, ebd_info, "global info");

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, glp_dt, "global info");

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, use_mcss_gph_sync, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_mcss_slave_add_en_2nd, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_mcss_slave_add_acken_2nd, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_mcss_controller_target_sel, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_mcss_xvs_io_ctrl, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_mcss_extout_en, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_mcss_sgmsync_sel, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_mcss_swdio_io_ctrl, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_mcss_gph_sync_mode, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_mcss_complete_sleep_en, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_mcss_mc_frm_lp_en, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_mcss_frm_length_reflect_timing, "global info");
	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, reg_addr_mcss_mc_frm_mask_num, "global info");

	ret |= RET_IF_CHK_FAIL(ctx, target, legacy, i3c_precfg_setting_len, "global info");
	ret |= RET_IF_CHK_PTR_FAIL(ctx, target, legacy, i3c_precfg_setting_table,
				   sizeof(u16) * (target->i3c_precfg_setting_len), "global info");

	if (ret)  /* contains error */
		return false;

	adaptor_logi(ctx, "static ctx check pass\n");

	return true;
}

static int register_ext_ops(struct adaptor_ctx *ctx)
{
	const struct subdrv_static_ctx_ext_ops *fw_ext_ops = ctx->subdrv->fw_ext_ops;
	struct subdrv_static_ctx *target = &ctx->subctx.s_ctx;
	u32 i, t;

	if (fw_ext_ops) {
		COPY_COMMON_MEMBER(target, fw_ext_ops, list);
		COPY_COMMON_MEMBER(target, fw_ext_ops, list_len);
		COPY_COMMON_MEMBER(target, fw_ext_ops, g_temp);
		COPY_COMMON_MEMBER(target, fw_ext_ops, g_gain2reg);
		COPY_COMMON_MEMBER(target, fw_ext_ops, g_cali);
		COPY_COMMON_MEMBER(target, fw_ext_ops, s_gph);
		COPY_COMMON_MEMBER(target, fw_ext_ops, s_cali);
		COPY_COMMON_MEMBER(target, fw_ext_ops, s_streaming_control);
		COPY_COMMON_MEMBER(target, fw_ext_ops, s_data_rate_global_timing_phy_ctrl);
		COPY_COMMON_MEMBER(target, fw_ext_ops, s_pwr_seq_reset_view_to_sensing);
		COPY_COMMON_MEMBER(target, fw_ext_ops, mcss_init);
		COPY_COMMON_MEMBER(target, fw_ext_ops, mcss_update_subdrv_para);
		COPY_COMMON_MEMBER(target, fw_ext_ops, cust_get_linetime_in_ns);
		COPY_COMMON_MEMBER(target, fw_ext_ops, chk_streaming_st);

		/* copy by mode ops */
		for (i = 0; (fw_ext_ops->mode_ext_ops_list) && (i < fw_ext_ops->mode_ext_ops_list_len); i++) {
			t = fw_ext_ops->mode_ext_ops_list[i].mode_id;
			if (t > target->sensor_mode_num) {
				adaptor_loge(ctx,
					"The sensor mode id %u is larger then the number of sensor mode: %u\n",
					t, target->sensor_mode_num);
				continue;
			}

			/* Add mode ext ops here */
		}

		/* Copy customed i2c addr table */
		i = 0;
		while ((i < ARRAY_SIZE(target->i2c_addr_table)) && (i < ARRAY_SIZE(fw_ext_ops->i2c_addr_table)) &&
		       (fw_ext_ops->i2c_addr_table[i] != 0)) {
			target->i2c_addr_table[i] = fw_ext_ops->i2c_addr_table[i];
			i++;
		}
		if (i > 0) {
			adaptor_logi(ctx, "custom s_ctx i2c addr tlb: {0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x}\n",
				     ctx->subctx.s_ctx.i2c_addr_table[0],
				     ctx->subctx.s_ctx.i2c_addr_table[1],
				     ctx->subctx.s_ctx.i2c_addr_table[2],
				     ctx->subctx.s_ctx.i2c_addr_table[3],
				     ctx->subctx.s_ctx.i2c_addr_table[4]);
		}

		/* check debug operation */
		if (!compare_static_ctx(ctx, target, fw_ext_ops->debug_check_with_exist_s_ctx))
			return -EFAULT;
	}

	return 0;
}

int loading_firmware(struct adaptor_ctx *ctx, struct sensor_firmware *sensor_fw)
{
	char *fw_path = NULL;
	const struct firmware *fw = NULL;
	int ret = 0;
	struct adaptor_profile_tv tv0, tv1, tv2;
	const char * const fw_name = sensor_fw->name;

	if (unlikely(ctx == NULL))
		return -EINVAL;

	ADAPTOR_PROFILE_BEGIN(&tv0);
	adaptor_logi(ctx, "loading fw_name = %s", fw_name);
	ADAPTOR_PROFILE_END(&tv0);

	ADAPTOR_PROFILE_BEGIN(&tv1);
	fw_path = get_firmware_path(ctx, IMGSENSOR_FW_SUB_FOLDER, fw_name);
	if (!fw_path)
		return -ENOMEM;

	ret = request_firmware(&fw, fw_path, ctx->dev);
	if (ret) {
		adaptor_loge(ctx, "fail to load firmware %s, ret:%d\n", fw_path, ret);
		devm_kfree(ctx->dev, fw_path);
		fw_path = NULL;
		return ret;
	}
	ADAPTOR_PROFILE_END(&tv1);

	ADAPTOR_PROFILE_BEGIN(&tv2);
	ret = init_with_firmware(ctx, sensor_fw, fw->data, fw->size);

	release_firmware(fw);
	devm_kfree(ctx->dev, fw_path);
	fw_path = NULL;

	ADAPTOR_PROFILE_END(&tv2);

	adaptor_logi(ctx, "profile loading firmware: (%lld us + %lld us + %lld us)",
		     (ADAPTOR_PROFILE_G_DIFF_NS(&tv0) / 1000),
		     (ADAPTOR_PROFILE_G_DIFF_NS(&tv1) / 1000),
		     (ADAPTOR_PROFILE_G_DIFF_NS(&tv2) / 1000));

	if (ret) {
		adaptor_loge(ctx, "fail to init firmware\n");
		return ret;
	}

	update_default_i2c_addr_table(ctx);
	ret = register_ext_ops(ctx);

	return ret;
}

int release_firmware_resource(struct adaptor_ctx *ctx, struct sensor_firmware *sensor_fw)
{
	int ret = 0;
	struct sensor_firmware_res *res = NULL, *r = NULL;

	/* prevent adaptor_logx use subdrv->name after free */
	if (ctx && ctx->subdrv)
		ctx->subdrv->name = NULL;

	list_for_each_entry_safe(res, r, &sensor_fw->res_list, list) {
		list_del(&res->list);
		devm_kfree(ctx->dev, res->res_ptr);
		res->res_ptr = NULL;
		devm_kfree(ctx->dev, res);
	}

	return ret;
}
