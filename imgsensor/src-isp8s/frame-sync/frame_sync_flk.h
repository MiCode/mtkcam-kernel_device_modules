/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_FLK_H
#define _FRAME_SYNC_FLK_H


/*******************************************************************************
 * define / macro / variable.
 * => e.g., frame sync flicker table
 ******************************************************************************/
enum fs_flk_err_type {
	FLK_ERR_NONE = 0,
	FLK_ERR_INVALID_INPUT,
	FLK_ERR_INVALID_FLK_EN_TYPE,
};


#define FLK_TABLE_CNT 4
#define FLK_TABLE_SIZE 6
static const unsigned int fs_flk_table[FLK_TABLE_CNT][FLK_TABLE_SIZE][2] = {
	{ /* [0] => flicker_en == 1 */
		/* 14.6 ~ 15.3 */
		{68493, 65359},

		/* 23.6 ~ 24.3 */
		{42372, 41152},

		/* 24.6 ~ 25.3 */
		{40650, 39525},

		/* 29.6 ~ 30.5 */
		{33783, 32786},

		/* 59.2 ~ 60.7 */
		{16891, 16474},

		/* END */
		{0, 0}
	},

	{ /* [1] => flicker_en == 2 */
		/* 14.6 ~ 15.3 */
		{68493, 65359},

		/* 23.6 ~ 24.3 */
		{42372, 41152},

		/* 24.6 ~ 25.3 */
		{40650, 39525},

		/* 29.9 ~ 30.5 */
		{33445, 32786},

		/* 59.2 ~ 60.7 */
		{16891, 16474},

		/* END */
		{0, 0}
	},

	{ /* [2] => flicker_en == 3 */
		/* 14.6 ~ 15.3 */
		{68493, 65359},

		/* 23.6 ~ 24.3 */
		{42372, 41152},

		/* 24.6 ~ 25.3 */
		{40650, 39525},

		/* 29.99 ~ 30.5 */
		{33345, 32786},

		/* 59.2 ~ 60.7 */
		{16891, 16474},

		/* END */
		{0, 0}
	},

	{ /* [3] => flicker_en == 4 */
		/* 14.6 ~ 15.3 */
		{68493, 65359},

		/* 23.6 ~ 24.3 */
		{42372, 41152},

		/* 24.6 ~ 25.3 */
		{40650, 39525},

		/* 30.0 ~ 30.5 */
		{33333, 32786},

		/* 59.2 ~ 60.7 */
		{16891, 16474},

		/* END */
		{0, 0}
	}
};
/******************************************************************************/





/*******************************************************************************
 * Frame Sync flicker inline functions.
 ******************************************************************************/
/**
 * return:
 *          0 => NO error, the result is valid for using.
 *      non-0 => some error (see fs_flk_err_type).
 */
static inline unsigned int fs_flk_chk_and_get_valid_flk_en_type(
	unsigned int *p_flk_en_type)
{
	/* error hanndling, for checking flk table boundary */
	if (unlikely(p_flk_en_type == NULL))
		return FLK_ERR_INVALID_INPUT;
	if (unlikely(*p_flk_en_type > FLK_TABLE_CNT)) {
		/* assign a type to prevent 'out of bound' error on flk table */
		*p_flk_en_type = 1;
		return FLK_ERR_INVALID_FLK_EN_TYPE;
	}
	return FLK_ERR_NONE;
}


/**
 * return:
 *          0 => NO error, the result is valid for using.
 *      non-0 => some error (see fs_flk_err_type).
 */
static inline unsigned int fs_flk_get_anti_flicker_fl(unsigned int flk_en_type,
	const unsigned int fl_us_orig, unsigned int *p_flk_us_result)
{
	unsigned int idx, i, ret;

	/* unexpected/error case */
	if (unlikely((flk_en_type == 0) || (p_flk_us_result == NULL)))
		return FLK_ERR_INVALID_INPUT;

	/* MUST init a default FL */
	*p_flk_us_result = fl_us_orig;

	/* check & covert to a valid flk en type */
	ret = fs_flk_chk_and_get_valid_flk_en_type(&flk_en_type);
	idx = (flk_en_type - 1);
	for (i = 0; i < (FLK_TABLE_SIZE - 1); ++i) {
		if (unlikely(fs_flk_table[idx][i][0] == 0))
			break;
		if ((fs_flk_table[idx][i][0] > fl_us_orig)
				&& (fl_us_orig >= fs_flk_table[idx][i][1])) {
			/* sync & update the result out */
			*p_flk_us_result = fs_flk_table[idx][i][0];
			break;
		}
	}
	return ret;
}
/******************************************************************************/


#endif
