// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/of.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/kernel.h>
#include <linux/gcd.h>

#include <mtk-interconnect.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include <soc/mediatek/mmqos.h>
#include <media/v4l2-subdev.h>

#include "mtk_cam.h"
#include "mtk_cam-defs.h"
#include "mtk_cam-raw_pipeline.h"
#include "mtk_cam-job.h"
#include "mtk_cam-job_utils.h"
#include "mtk_cam-fmt_utils.h"
#include "mtk_cam-dvfs_qos.h"
#include "mtk_cam-ufbc-def.h"
#include "mtk_cam-qof.h"
#include "mtk_cam-plat.h"
#include "mtk_cam-bwr.h"
#include "mtk_cam-sv-df.h"
#include "swpm_isp_wrapper.h"

#define BOOST_DVFS_OPP    2

#define LCM(a,b) ((a) / gcd(a, b) * (b))

struct dvfs_stream_info {
	int opp_idx;
	bool boostable;

	int switching_opp_idx;
	bool switching_boostable;
};
static unsigned int cam_sspm_en;
module_param(cam_sspm_en, int, 0644);
MODULE_PARM_DESC(cam_sspm_en, "sspm report enable");

/* dvfs */
static int mtk_cam_build_freq_table(struct device *dev,
				    struct camsys_opp_table *tbl,
				    int size)
{
	int count = dev_pm_opp_get_opp_count(dev);
	struct dev_pm_opp *opp;
	int i, index = 0;
	unsigned long freq = 0;

	if (WARN(count > size,
		 "opp table is being truncated\n"))
		count = size;

	for (i = 0; i < count; i++) {
		opp = dev_pm_opp_find_freq_ceil(dev, &freq);
		if (IS_ERR(opp))
			break;

		tbl[index].freq_hz = freq;
		tbl[index].volt_uv = dev_pm_opp_get_voltage(opp);

		dev_pm_opp_put(opp);
		index++;
		freq++;
	}

	return index;
}

int mtk_cam_dvfs_probe(struct device *dev,
		       struct mtk_camsys_dvfs *dvfs, int max_stream_num)
{
	int ret;
	int i;

	dvfs->dev = dev;
	mutex_init(&dvfs->dvfs_lock);

	if (max_stream_num <= 0) {
		dev_info(dev, "invalid stream num %d\n", max_stream_num);
		return -1;
	}

	dvfs->max_stream_num = max_stream_num;
	dvfs->stream_infos = devm_kcalloc(dev, max_stream_num,
					  sizeof(*dvfs->stream_infos),
					  GFP_KERNEL);
	if (!dvfs->stream_infos)
		return -ENOMEM;

	ret = dev_pm_opp_of_add_table(dev);
	if (ret < 0) {
		dev_info(dev, "fail to init opp table: %d\n", ret);
		goto opp_default_table;
	}

	dvfs->opp_num = mtk_cam_build_freq_table(dev, dvfs->opp,
						 ARRAY_SIZE(dvfs->opp));
	for (i = 0; i < dvfs->opp_num; i++)
		dev_info(dev, "[%s] idx=%d, clk=%d volt=%d\n", __func__,
			 i, dvfs->opp[i].freq_hz, dvfs->opp[i].volt_uv);
	dvfs->mmdvfs_clk = devm_clk_get(dev,
					!mmdvfs_get_version() ? "mmdvfs_clk" : "mmdvfs_mux");
	if (IS_ERR(dvfs->mmdvfs_clk)) {
		dvfs->mmdvfs_clk = NULL;
		dev_info(dev, "failed to get mmdvfs_clk\n");
	}

	return 0;

opp_default_table:
	dvfs->opp_num = 1;
	dvfs->opp[0] = (struct camsys_opp_table) {
		.freq_hz = GET_PLAT_HW(default_opp_freq_hz),
		.volt_uv = GET_PLAT_HW(default_opp_volt_uv),
	};
	return 0;
}

int mtk_cam_dvfs_remove(struct mtk_camsys_dvfs *dvfs)
{
	dev_pm_opp_of_remove_table(dvfs->dev);
	return 0;
}

void mtk_cam_dvfs_reset_runtime_info(struct mtk_camsys_dvfs *dvfs)
{
	dvfs->cur_opp_idx = -1;

	memset(dvfs->stream_infos, 0,
	       dvfs->max_stream_num * sizeof(*dvfs->stream_infos));
}

unsigned int mtk_cam_dvfs_query(struct mtk_camsys_dvfs *dvfs, int opp_idx)
{
	unsigned int idx;

	idx = clamp_t(unsigned int, opp_idx, 0, dvfs->opp_num);
	return dvfs->opp[idx].freq_hz;
}

int freq_to_oppidx(struct mtk_camsys_dvfs *dvfs,
			  unsigned int freq)
{
	int i;

	for (i = 0; i < dvfs->opp_num; i++)
		if (freq <= dvfs->opp[i].freq_hz)
			break;

	if (i == dvfs->opp_num) {
		dev_info(dvfs->dev, "failed to find idx for freq %u\n", freq);
		return -1;
	}
	return i;
}

static int mtk_cam_dvfs_update_opp(struct mtk_camsys_dvfs *dvfs,
				   int opp_idx)
{
	int ret;

	if (!dvfs->mmdvfs_clk) {
		dev_info(dvfs->dev, "%s: mmdvfs_clk is not ready\n", __func__);
		return -1;
	}

	if (opp_idx < 0 || opp_idx >= ARRAY_SIZE(dvfs->opp)) {
		dev_info(dvfs->dev, "%s: invalid opp_idx %d\n", __func__,
			 opp_idx);
		return -1;
	}

	ret = clk_set_rate(dvfs->mmdvfs_clk, dvfs->opp[opp_idx].freq_hz);
	if (ret < 0) {
		dev_info(dvfs->dev, "[%s] clk set rate %u fail",
			 __func__, dvfs->opp[opp_idx].freq_hz);
		return -1;
	}

	return 0;
}

static int mtk_cam_dvfs_adjust_opp(struct mtk_camsys_dvfs *dvfs,
				   int opp_idx)
{
	if (opp_idx == dvfs->cur_opp_idx)
		return 0;

	if (mtk_cam_dvfs_update_opp(dvfs, opp_idx))
		return -1;

	dvfs->cur_opp_idx = opp_idx;
	return 0;
}

static struct dvfs_stream_info *get_stream_info(struct mtk_camsys_dvfs *dvfs,
						int stream_id)
{
	if (WARN_ON(stream_id < 0 || stream_id >= dvfs->max_stream_num))
		return NULL;

	return &dvfs->stream_infos[stream_id];
}

static void stream_info_set(struct dvfs_stream_info *s_info,
			    int opp_idx, bool boostable,
			    bool is_switching)
{
	if (!is_switching) {
		s_info->opp_idx = opp_idx;
		s_info->boostable = boostable;

		s_info->switching_opp_idx = 0;
		s_info->switching_boostable = 0;
	} else {
		s_info->switching_opp_idx = opp_idx;
		s_info->switching_boostable = boostable;
	}
}

static void stream_info_do_switch(struct dvfs_stream_info *s_info)
{
	s_info->opp_idx = s_info->switching_opp_idx;
	s_info->boostable = s_info->switching_boostable;

	s_info->switching_opp_idx = 0;
	s_info->switching_boostable = 0;
}

static int find_max_oppidx(struct mtk_camsys_dvfs *dvfs,
			   struct dvfs_stream_info *s_info,
			   bool is_switching)
{
	int max_opp = 0;
	int boost_opp;
	int i;

	for (i = 0; i < dvfs->max_stream_num; i++)
		max_opp = max3(max_opp,
			dvfs->stream_infos[i].opp_idx,
			dvfs->stream_infos[i].switching_opp_idx);

	if (!is_switching)
		return max_opp;

	boost_opp = s_info->boostable ?
		min(s_info->opp_idx + BOOST_DVFS_OPP, (int)dvfs->opp_num) :
		s_info->opp_idx;

	max_opp = max3(max_opp, boost_opp, s_info->switching_opp_idx);
	return max_opp;
}

static int dvfs_update(struct mtk_camsys_dvfs *dvfs, int stream_id,
		       unsigned int freq_hz, bool boostable,
		       bool is_switching, const char *caller)
{
	struct dvfs_stream_info *s_info;
	int opp_idx = -1;
	int max_opp_idx = -1;
	int prev_opp_idx;
	int ret = 0;

	s_info = get_stream_info(dvfs, stream_id);
	if (!s_info)
		return -1;

	opp_idx = freq_to_oppidx(dvfs, freq_hz);
	if (opp_idx < 0)
		return -1;

	mutex_lock(&dvfs->dvfs_lock);

	stream_info_set(s_info, opp_idx, boostable, is_switching);
	max_opp_idx = find_max_oppidx(dvfs, s_info, is_switching);

	prev_opp_idx = dvfs->cur_opp_idx;
	ret = mtk_cam_dvfs_adjust_opp(dvfs, max_opp_idx);
	if (ret)
		goto EXIT_UNLOCK;

	mutex_unlock(&dvfs->dvfs_lock);

	dev_info(dvfs->dev,
		 "%s: stream %d freq %u boostable %d, opp_idx: %d->%d\n",
		 caller, stream_id, freq_hz, boostable,
		 prev_opp_idx, max_opp_idx);
	return 0;

EXIT_UNLOCK:
	mutex_unlock(&dvfs->dvfs_lock);
	return ret;
}

static int dvfs_switch_end(struct mtk_camsys_dvfs *dvfs, int stream_id)
{
	struct dvfs_stream_info *s_info;
	int prev_opp_idx;
	int max_opp_idx = -1;
	int ret = 0;

	s_info = get_stream_info(dvfs, stream_id);
	if (!s_info)
		return -1;

	mutex_lock(&dvfs->dvfs_lock);

	stream_info_do_switch(s_info);
	max_opp_idx = find_max_oppidx(dvfs, s_info, false);

	prev_opp_idx = dvfs->cur_opp_idx;
	ret = mtk_cam_dvfs_adjust_opp(dvfs, max_opp_idx);
	if (ret)
		goto EXIT_UNLOCK;

	mutex_unlock(&dvfs->dvfs_lock);

	dev_info(dvfs->dev,
		 "%s: stream %d opp_idx: %d->%d\n",
		 __func__, stream_id, prev_opp_idx, max_opp_idx);
	return 0;

EXIT_UNLOCK:
	mutex_unlock(&dvfs->dvfs_lock);
	return ret;
}

int mtk_cam_dvfs_update(struct mtk_camsys_dvfs *dvfs, int stream_id,
			unsigned int freq_hz, bool boostable)
{
	if (is_dvc_support())
		return 0;
	else
		return dvfs_update(dvfs, stream_id, freq_hz, boostable, false, __func__);
}

int mtk_cam_dvfs_switch_begin(struct mtk_camsys_dvfs *dvfs, int stream_id, int raw_id,
			      unsigned int freq_hz, bool boostable)
{
	if (is_dvc_support())
		return mtk_cam_dvc_vote(&dvfs->dvc, raw_id, freq_to_oppidx(dvfs, freq_hz), 1);
	else
		return dvfs_update(dvfs, stream_id, freq_hz, boostable, true,
		   __func__);
}

int mtk_cam_dvfs_switch_end(struct mtk_camsys_dvfs *dvfs, int stream_id, int raw_id,
			      unsigned int freq_hz)
{
	if (is_dvc_support())
		return mtk_cam_dvc_vote(&dvfs->dvc, raw_id, freq_to_oppidx(dvfs, freq_hz), 0);
	else
		return dvfs_switch_end(dvfs, stream_id);
}

int mtk_cam_qos_probe(struct device *dev,
		      struct mtk_camsys_qos *qos, int qos_num)
{
	const char *names[32];
	struct mtk_camsys_qos_path *cam_path;
	int i;
	int n;
	int ret = 0;

	n = of_property_count_strings(dev->of_node, "interconnect-names");
	if (n <= 0) {
		dev_info(dev, "skip without interconnect-names\n");
		return 0;
	}

	qos->n_path = n;
	qos->cam_path = devm_kcalloc(dev, qos->n_path,
				     sizeof(*qos->cam_path), GFP_KERNEL);
	if (!qos->cam_path)
		return -ENOMEM;

	dev_info(dev, "icc_path num %d\n", qos->n_path);
	if (qos->n_path > ARRAY_SIZE(names)) {
		dev_info(dev, "%s: array size of names is not enough.\n",
			 __func__);
		return -EINVAL;
	}

	if (qos->n_path != qos_num) {
		dev_info(dev, "icc num(%d) mismatch with qos num(%d)\n",
			 qos->n_path, qos_num);
		return -EINVAL;
	}

	of_property_read_string_array(dev->of_node, "interconnect-names",
				      names, qos->n_path);

	for (i = 0, cam_path = qos->cam_path; i < qos->n_path; i++, cam_path++) {
		dev_info(dev, "interconnect: idx %d [%s]\n", i, names[i]);
		cam_path->path = of_mtk_icc_get(dev, names[i]);
		if (IS_ERR_OR_NULL(cam_path->path)) {
			dev_info(dev, "failed to get icc of %s\n",
				 names[i]);
			ret = -EINVAL;
		}
		strscpy(cam_path->name, names[i], ICCPATH_NAME_SIZE);

		cam_path->applied_bw = -1;
		cam_path->pending_bw = -1;
	}

	return ret;
}

int mtk_cam_qos_remove(struct mtk_camsys_qos *qos)
{
	int i;

	if (!qos->cam_path)
		return 0;

	for (i = 0; i < qos->n_path; i++)
		icc_put(qos->cam_path[i].path);

	return 0;
}

static inline u32 apply_ufo_com_ratio(u32 avg_bw)
{
	return avg_bw / 10 * 7;
}

/* unit : KB/s */
static inline u32 calc_bw(u32 size, u64 linet, u32 active_h)
{
	//pr_info("%s: size:%d linet:%llu, active_h:%d\n",
	//		__func__, size, linet, active_h);

	return to_qos_icc((1000000000L*size)/(linet * active_h));
}

static struct mtkcam_qos_desc *get_qos_desc_by_uid(int uid)
{
	if (uid >= QOS_DESC_TABLE_SIZE || uid < 0)
		return NULL;

	return &mmqos_table[uid];
}

static inline bool is_bitstream(u8 ufbc_type)
{
	return (ufbc_type == UFBC_BITSTREAM_0) || (ufbc_type == UFBC_BITSTREAM_1);
}

static int get_ufbc_size(int ipi_fmt, int ufbc_type, int img_w, int img_h)
{
	int size = 0, aligned_w = 0, stride = 0;

	if (ipifmt_is_yuv_ufo(ipi_fmt) || ipifmt_is_raw_ufo(ipi_fmt)) {
		aligned_w = ALIGN(img_w, 64);
		stride = aligned_w * mtk_cam_get_pixel_bits(ipi_fmt) / 8;

		switch (ufbc_type) {
		case UFBC_BITSTREAM_0:
			size = stride * img_h;
		break;
		case UFBC_BITSTREAM_1:
			size = stride * img_h / 2;
		break;
		case UFBC_TABLE_0:
			size = ALIGN((aligned_w / 64),
				ipifmt_is_raw_ufo(ipi_fmt) ? 16 : 8) * img_h;
		break;
		case UFBC_TABLE_1:
			size = ALIGN((aligned_w / 64),
				ipifmt_is_raw_ufo(ipi_fmt) ? 16 : 8) * img_h / 2;
		break;
		default:
		break;
		}
	}

	return size;
}

static inline bool is_srt(struct mtk_cam_job *job)
{
	return (is_dc_mode(job) || is_m2m(job));
}

//assuming max image size 16000*12000*10/8
#define MMQOS_SIZE_WARNING 240000000
static int fill_raw_out_qos(struct mtk_cam_job *job,
			    struct mtkcam_ipi_img_output *out,
			    u32 sensor_h, u32 sensor_vb, u64 linet)
{
	struct mtkcam_qos_desc *qos_desc;
	unsigned int size;
	u32 peak_bw, avg_bw, active_h;
	int i, dst_port;

	qos_desc = get_qos_desc_by_uid(out->uid.id);
	if (!qos_desc) {
		if (CAM_DEBUG_ENABLED(MMQOS))
			pr_info("%s: can't find qos desc in table uid:%d", __func__, out->uid.id);
		return 0;
	}

	for (i = 0; i < qos_desc->desc_size &&
				i < ARRAY_SIZE(out->fmt.stride); i++) {
		size = get_ufbc_size(out->fmt.format,
					qos_desc->dma_desc[i].ufbc_type,
					out->fmt.s.w, out->fmt.s.h);
		/* if (size != 0) => ufo fmt */
		size = (size == 0) ? out->buf[0][i].size : size;
		if (!size)
			break;

		if (WARN_ON(size > MMQOS_SIZE_WARNING)) {
			pr_info("%s: %s: req_seq(%d) uid:%d size too large(%d), please check\n",
				__func__,
				qos_desc->dma_desc[i].dma_name,
				job->req_seq, out->uid.id, size);
			continue;
		}

		dst_port = qos_desc->dma_desc[i].dst_port;

		/* srt */
		active_h = sensor_h + sensor_vb;
		avg_bw = calc_bw(size, linet, active_h);
		avg_bw = is_bitstream(qos_desc->dma_desc[i].ufbc_type) ?
				apply_ufo_com_ratio(avg_bw) : avg_bw;

		/* hrt (otf case) */
		active_h = (out->crop.s.h == 0) ? out->fmt.s.h : out->crop.s.h;
		peak_bw = is_srt(job) ? 0 : calc_bw(size, linet, active_h);

		switch (qos_desc->dma_desc[i].domain) {
		case RAW_DOMAIN:
			job->raw_mmqos[dst_port].peak_bw += peak_bw;
			job->raw_mmqos[dst_port].avg_bw += avg_bw;
			break;
		case YUV_DOMAIN:
			job->yuv_mmqos[dst_port].peak_bw += peak_bw;
			job->yuv_mmqos[dst_port].avg_bw += avg_bw;
			break;
		default:
			pr_info("%s: unsupport domain(%d)\n", __func__,
				qos_desc->dma_desc[i].domain);
			break;
		}

		if (CAM_DEBUG_ENABLED(MMQOS))
			pr_info("%s: %s: req_seq(%d) dst_port:%d uid:%d avg_bw:%d KB/s, peak_bw:%d KB/s (size:%d active_h:%d vb:%d)\n",
				__func__, qos_desc->dma_desc[i].dma_name, job->req_seq,
				dst_port, out->uid.id, avg_bw, peak_bw,
				size, active_h, sensor_vb);
	}

	return 0;
}

static int fill_raw_in_qos(struct mtk_cam_job *job,
			   struct mtkcam_ipi_img_input *in,
			   u32 sensor_h, u32 sensor_vb, u64 linet)
{
	struct mtkcam_qos_desc *qos_desc = NULL;
	struct mtkcam_qos_desc *imgo_qos_desc = NULL;
	unsigned int size;
	u32 peak_bw, avg_bw;
	int i, dst_port;

	qos_desc = get_qos_desc_by_uid(in->uid.id);
	if (!qos_desc) {
		pr_info("%s: can't find qos desc in table uid:%d", __func__, in->uid.id);
		return 0;
	}

	/* for mstream 1st ipi update */
	if (job->job_type == JOB_TYPE_MSTREAM &&
	    in->uid.id == MTKCAM_IPI_RAW_RAWI_2) {
		imgo_qos_desc = get_qos_desc_by_uid(MTKCAM_IPI_RAW_IMGO);
	}

	for (i = 0; i < qos_desc->desc_size && i < ARRAY_SIZE(in->fmt.stride); i++) {
		size = get_ufbc_size(in->fmt.format,
					qos_desc->dma_desc[i].ufbc_type,
					in->fmt.s.w, in->fmt.s.h);
		size = (size == 0) ? in->buf[i].size : size;
		if (!size)
			break;

		if (WARN_ON(size > MMQOS_SIZE_WARNING)) {
			pr_info("%s: %s: req_seq(%d) uid:%d size too large(%d), please check\n",
				__func__,
				qos_desc->dma_desc[i].dma_name,
				job->req_seq, in->uid.id, size);
			continue;
		}

		dst_port = qos_desc->dma_desc[i].dst_port;

		/* srt */
		avg_bw = calc_bw(size, linet, sensor_h + sensor_vb);
		avg_bw = is_bitstream(qos_desc->dma_desc[i].ufbc_type) ?
				apply_ufo_com_ratio(avg_bw) : avg_bw;
		/* hrt */
		peak_bw = is_srt(job) ? 0 : calc_bw(size, linet, sensor_h);

		switch (qos_desc->dma_desc[i].domain) {
		case RAW_DOMAIN:
			job->raw_mmqos[dst_port].peak_bw += peak_bw;
			job->raw_mmqos[dst_port].avg_bw += avg_bw;
			break;
		default:
			pr_info("%s: unsupport domain(%d)\n", __func__,
				qos_desc->dma_desc[i].domain);
			break;
		}

		if (CAM_DEBUG_ENABLED(MMQOS))
			pr_info("%s: %s: req_seq(%d) dst_port:%d uid:%d avg_bw:%d KB/s, peak_bw:%d KB/s (size:%d height:%d vb:%d)\n",
				__func__, qos_desc->dma_desc[i].dma_name, job->req_seq,
				dst_port, in->uid.id, avg_bw, peak_bw, size, sensor_h, sensor_vb);

		/* for mstream 1st ipi */
		if (imgo_qos_desc) {
			dst_port = imgo_qos_desc->dma_desc[i].dst_port;
			if (dst_port < 0)
				continue;
			job->raw_mmqos[dst_port].peak_bw = peak_bw;
			job->raw_mmqos[dst_port].avg_bw = avg_bw;

			if (CAM_DEBUG_ENABLED(MMQOS))
				pr_info("%s: %s: req_seq(%d) dst_port:%d uid:%d avg_bw:%d KB/s, peak_bw:%d KB/s (size:%d height:%d vb:%d)\n",
					__func__, imgo_qos_desc->dma_desc[i].dma_name,
					job->req_seq, dst_port, in->uid.id, avg_bw, peak_bw,
					size, sensor_h, sensor_vb);
		}
	}

	return 0;
}

static int fill_raw_stats_qos(struct req_buffer_helper *helper,
					u32 sensor_h, u32 sensor_vb, u64 linet)
{
	struct mtk_cam_job *job = helper->job;
	struct mtkcam_qos_desc *qos_desc;
	unsigned int size = 0;
	void *meta_va = NULL;
	u32 peak_bw, avg_bw;
	int table_idx, ipi_id, j, dst_port;
	int stats_ipi_id_table[] = {
		MTKCAM_IPI_RAW_META_STATS_CFG,
		MTKCAM_IPI_RAW_META_STATS_0,
		MTKCAM_IPI_RAW_META_STATS_1,
	};

	for (table_idx = 0; table_idx < ARRAY_SIZE(stats_ipi_id_table); table_idx++) {
		ipi_id = stats_ipi_id_table[table_idx];
		qos_desc = get_qos_desc_by_uid(ipi_id);

		if (ipi_id == MTKCAM_IPI_RAW_META_STATS_CFG)
			meta_va = helper->meta_cfg_buf_va;
		else if (ipi_id == MTKCAM_IPI_RAW_META_STATS_0)
			meta_va = helper->meta_stats0_buf_va;
		else if (ipi_id == MTKCAM_IPI_RAW_META_STATS_1)
			meta_va = helper->meta_stats1_buf_va;
		else
			continue;

		if (!meta_va || !qos_desc)
			continue;

		for (j = 0; j < qos_desc->desc_size; j++) {
			/* for multi exposure */
			if (qos_desc->dma_desc[j].exp_num > job_exp_num(job))
				continue;

			CALL_PLAT_V4L2(get_meta_stats_port_size, ipi_id, meta_va,
				       qos_desc->dma_desc[j].src_port, &size);
			if (!size)
				continue;  // no used port, the define in desc could be removed

			if (WARN_ON(size > MMQOS_SIZE_WARNING)) {
				pr_info("%s: %s: req_seq(%d) size too large(%d), please check\n",
					__func__,
					qos_desc->dma_desc[j].dma_name,
					job->req_seq, size);
				continue;
			}

			dst_port = qos_desc->dma_desc[j].dst_port;

			/* srt */
			avg_bw = calc_bw(size, linet, sensor_h + sensor_vb);
			/* hrt */
			peak_bw = is_srt(job) ? 0 : avg_bw;

			switch (qos_desc->dma_desc[j].domain) {
			case RAW_DOMAIN:
				job->raw_mmqos[dst_port].peak_bw += peak_bw;
				job->raw_mmqos[dst_port].avg_bw += avg_bw;
				break;
			case YUV_DOMAIN:
				job->yuv_mmqos[dst_port].peak_bw += peak_bw;
				job->yuv_mmqos[dst_port].avg_bw += avg_bw;
				break;
			default:
				pr_info("%s: unsupport domain(%d)\n", __func__,
					qos_desc->dma_desc[j].domain);
				break;
			}

			if (CAM_DEBUG_ENABLED(MMQOS))
				pr_info("%s: %s: req_seq(%d) dst_port:%d avg_bw:%d KB/s, peak_bw:%d KB/s (size:%d height:%d)\n",
					__func__, qos_desc->dma_desc[j].dma_name, job->req_seq,
					dst_port, avg_bw, peak_bw, size, sensor_h);
		}
	}

	return 0;
}

static int fill_sv_qos(struct mtk_cam_job *job,
						struct mtkcam_ipi_frame_param *fp,
						u32 sensor_h, u32 sensor_vb, u64 linet,
						u32 sensor_fps)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtkcam_ipi_camsv_frame_param *sv_param;
	struct mtkcam_ipi_img_output *in;
	struct mtk_camsv_device *sv_dev;
	unsigned int i, x_size, img_h, sv_id;
	u64 avg_bw = 0, peak_bw = 0, total_peak_bw = 0;
	u64 pd_avg_bw = 0, pd_peak_bw = 0;
	u64 pure_pd_avg_bw = 0, pure_pd_peak_bw = 0;
	u64 stash_avg_bw = 0, stash_peak_bw = 0;
	unsigned int sv_port_num = 0;
	bool is_smmu_enabled = true;

	/* cqi */
	avg_bw = peak_bw = to_qos_icc(CQ_BUF_SIZE * sensor_fps);
	total_peak_bw += peak_bw;
	job->sv_mmqos[SMI_PORT_SV_CQI].avg_bw += avg_bw;
	job->sv_mmqos[SMI_PORT_SV_CQI].peak_bw += peak_bw;
	if (CAM_DEBUG_ENABLED(MMQOS))
		pr_info("%s: sensor_h:%u/vb:%u/linet:%llu(ns)/fps:%u avg_bw:%lluKB/s, %uKB/s/peak_bw:%lluKB/s, %uKB/s\n",
			__func__,
			sensor_h, sensor_vb, linet, sensor_fps,
			avg_bw, job->sv_mmqos[SMI_PORT_SV_CQI].avg_bw,
			peak_bw, job->sv_mmqos[SMI_PORT_SV_CQI].peak_bw);

	if (bit_map_subset_of(MAP_HW_CAMSV, job->used_engine) == 0)
		return 0;
	sv_id = get_master_sv_id(job->used_engine);
	sv_dev = dev_get_drvdata(ctx->cam->engines.sv_devs[sv_id]);

	CALL_PLAT_V4L2(get_is_smmu_enabled, &is_smmu_enabled);

	/* wdma */
	for (i = 0; i < CAMSV_MAX_TAGS; i++) {
		stash_peak_bw = stash_avg_bw = 0;
		sv_param = &fp->camsv_param[i];
		in = &sv_param->camsv_img_outputs[0];

		x_size = in->fmt.stride[0];
		img_h = in->fmt.s.h;

		CALL_PLAT_V4L2(
			get_sv_smi_setting, sv_dev->id, &sv_port_num);

		if (i < SVTAG_IMG_END) {
			avg_bw =
				calc_bw(x_size * img_h, linet, sensor_h + sensor_vb);
			peak_bw =
				calc_bw(x_size * img_h, linet, sensor_h);
			total_peak_bw += peak_bw;
			if (ipifmt_is_raw_ufo(in->fmt.format)) {
				/* compression ratio: 0.7x */
				avg_bw = avg_bw * 7 / 10;
				/* table */
				avg_bw = avg_bw +
					calc_bw(DIV_ROUND_UP(in->fmt.s.w, 64) * img_h,
						linet, img_h + sensor_vb);
			}
			if (is_smmu_enabled && (avg_bw || peak_bw) && (x_size != 0)) {
				/* stash */
				stash_peak_bw = peak_bw * 16 / 4096;
				stash_avg_bw = x_size * img_h * 16 / 4096 * (u64)sensor_fps;
				stash_avg_bw = to_qos_icc(stash_avg_bw);
			}

		} else {
			if (job->tag_info[i].is_pdp_enable) {
				pd_avg_bw =
					calc_bw(x_size * img_h, linet, sensor_h + sensor_vb);
				pd_peak_bw =
					calc_bw(x_size * img_h, linet, sensor_h);
				total_peak_bw += pd_peak_bw;
				if (is_smmu_enabled && (pd_avg_bw || pd_peak_bw) && x_size != 0) {
					/* stash */
					stash_peak_bw = pd_peak_bw * 16 / 4096;
					stash_avg_bw = x_size * img_h * 16 / 4096 * (u64)sensor_fps;
					stash_avg_bw = to_qos_icc(stash_avg_bw);
				}
			} else {
				pure_pd_avg_bw =
					calc_bw(x_size * img_h, linet, sensor_h + sensor_vb);
				pure_pd_peak_bw =
					calc_bw(x_size * img_h, linet, sensor_h);
				total_peak_bw += pure_pd_peak_bw;
				if (is_smmu_enabled && (pure_pd_avg_bw || pure_pd_peak_bw)
					&& x_size != 0) {
					/* stash */
					stash_peak_bw = pure_pd_peak_bw * 16 / 4096;
					stash_avg_bw = x_size * img_h * 16 / 4096 * (u64)sensor_fps;
					stash_avg_bw = to_qos_icc(stash_avg_bw);
				}
			}
		}

		if (sv_port_num == SMI_PORT_SV_TYPE0_NUM) {
			job->sv_mmqos[SMI_PORT_SV_WDMA_0].avg_bw += avg_bw;
			job->sv_mmqos[SMI_PORT_SV_WDMA_0].peak_bw += peak_bw;
			job->sv_mmqos[SMI_PORT_SV_WDMA_0].avg_bw += (pd_avg_bw + pure_pd_avg_bw);
			job->sv_mmqos[SMI_PORT_SV_WDMA_0].peak_bw += (pd_peak_bw + pure_pd_peak_bw);
			if (is_smmu_enabled) {
				job->sv_mmqos[SMI_PORT_SV_STG_0].avg_bw += stash_avg_bw;
				job->sv_mmqos[SMI_PORT_SV_STG_0].peak_bw += stash_peak_bw;
			}
		} else if (sv_port_num == SMI_PORT_SV_TYPE1_NUM){
			job->sv_mmqos[SMI_PORT_SV_WDMA_0].avg_bw += avg_bw / 2;
			job->sv_mmqos[SMI_PORT_SV_WDMA_0].peak_bw += peak_bw / 2;
			job->sv_mmqos[SMI_PORT_SV_WDMA_1].avg_bw += avg_bw / 2;
			job->sv_mmqos[SMI_PORT_SV_WDMA_1].peak_bw += peak_bw / 2;
			job->sv_mmqos[SMI_PORT_SV_WDMA_0].avg_bw += (pd_avg_bw + pure_pd_avg_bw) / 2;
			job->sv_mmqos[SMI_PORT_SV_WDMA_0].peak_bw +=
				(pd_peak_bw + pure_pd_peak_bw) / 2;
			job->sv_mmqos[SMI_PORT_SV_WDMA_1].avg_bw += (pd_avg_bw + pure_pd_avg_bw) / 2;
			job->sv_mmqos[SMI_PORT_SV_WDMA_1].peak_bw +=
				(pd_peak_bw + pure_pd_peak_bw) / 2;
			if (is_smmu_enabled) {
				job->sv_mmqos[SMI_PORT_SV_STG_0].avg_bw += stash_avg_bw / 2;
				job->sv_mmqos[SMI_PORT_SV_STG_0].peak_bw += stash_peak_bw / 2;
				job->sv_mmqos[SMI_PORT_SV_STG_1].avg_bw += stash_avg_bw / 2;
				job->sv_mmqos[SMI_PORT_SV_STG_1].peak_bw += stash_peak_bw / 2;
			}
		} else if (sv_port_num == SMI_PORT_SV_TYPE2_NUM) {
			job->sv_mmqos[SMI_PORT_SV_WDMA_0].avg_bw += (avg_bw + pure_pd_avg_bw) / 3;
			job->sv_mmqos[SMI_PORT_SV_WDMA_0].peak_bw += (peak_bw + pure_pd_peak_bw) / 3;
			job->sv_mmqos[SMI_PORT_SV_WDMA_1].avg_bw += (avg_bw + pure_pd_avg_bw) / 3;
			job->sv_mmqos[SMI_PORT_SV_WDMA_1].peak_bw += (peak_bw + pure_pd_peak_bw) / 3;
			job->sv_mmqos[SMI_PORT_SV_WDMA_2].avg_bw += (avg_bw + pure_pd_avg_bw) / 3;
			job->sv_mmqos[SMI_PORT_SV_WDMA_2].peak_bw += (peak_bw + pure_pd_peak_bw) / 3;
			/* pdp pd data only output dma 0 & dma 1*/
			job->sv_mmqos[SMI_PORT_SV_WDMA_0].avg_bw += pd_avg_bw / 2;
			job->sv_mmqos[SMI_PORT_SV_WDMA_0].peak_bw += pd_peak_bw / 2;
			job->sv_mmqos[SMI_PORT_SV_WDMA_1].avg_bw += pd_avg_bw / 2;
			job->sv_mmqos[SMI_PORT_SV_WDMA_1].peak_bw += pd_peak_bw / 2;
			if (is_smmu_enabled) {
				job->sv_mmqos[SMI_PORT_SV_STG_0].avg_bw += stash_avg_bw / 3;
				job->sv_mmqos[SMI_PORT_SV_STG_0].peak_bw += stash_peak_bw / 3;
				job->sv_mmqos[SMI_PORT_SV_STG_1].avg_bw += stash_avg_bw / 3;
				job->sv_mmqos[SMI_PORT_SV_STG_1].peak_bw += stash_peak_bw / 3;
				job->sv_mmqos[SMI_PORT_SV_STG_2].avg_bw += stash_avg_bw / 3;
				job->sv_mmqos[SMI_PORT_SV_STG_2].peak_bw += stash_peak_bw / 3;
			}
		}
		if (CAM_DEBUG_ENABLED(MMQOS) && is_smmu_enabled)
			pr_info("%s: xsize:%u/height:%u sensor_h:%u/vb:%u/linet:%llu(ns)/fps:%u avg_bw:%lluKB/s, %uKB/s, %uKB/s, %uKB/s %uKB/s %uKB/s, %uKB/s, peak_bw:%lluKB/s, %uKB/s, %uKB/s, %uKB/s %uKB/s %uKB/s, %uKB/s\n",
				__func__, x_size, img_h,
				sensor_h, sensor_vb, linet, sensor_fps, avg_bw + pd_avg_bw + pure_pd_avg_bw,
				job->sv_mmqos[SMI_PORT_SV_WDMA_0].avg_bw,
				job->sv_mmqos[SMI_PORT_SV_STG_0].avg_bw,
				job->sv_mmqos[SMI_PORT_SV_WDMA_1].avg_bw,
				job->sv_mmqos[SMI_PORT_SV_STG_1].avg_bw,
				job->sv_mmqos[SMI_PORT_SV_WDMA_2].avg_bw,
				job->sv_mmqos[SMI_PORT_SV_STG_2].avg_bw,
				peak_bw + pd_peak_bw + pure_pd_peak_bw,
				job->sv_mmqos[SMI_PORT_SV_WDMA_0].peak_bw,
				job->sv_mmqos[SMI_PORT_SV_STG_0].peak_bw,
				job->sv_mmqos[SMI_PORT_SV_WDMA_1].peak_bw,
				job->sv_mmqos[SMI_PORT_SV_STG_1].peak_bw,
				job->sv_mmqos[SMI_PORT_SV_WDMA_2].peak_bw,
				job->sv_mmqos[SMI_PORT_SV_STG_2].peak_bw);
		if (CAM_DEBUG_ENABLED(MMQOS) && !is_smmu_enabled)
			pr_info("%s: xsize:%u/height:%u sensor_h:%u/vb:%u/linet:%llu/fps:%u avg_bw:%llu KB/s, %u KB/s, %u KB/s, %u KB/s %u KB/s %u KB/s, %u KB/s, peak_bw:%llu KB/s, %u KB/s, %u KB/s, %u KB/s %u KB/s %u KB/s, %u KB/s\n",
				__func__, x_size, img_h,
				sensor_h, sensor_vb, linet, sensor_fps, avg_bw + pd_avg_bw,
				job->sv_mmqos[SMI_PORT_SV_WDMA_0].avg_bw,
				job->sv_mmqos[SMI_PORT_SV_STG_0].avg_bw,
				job->sv_mmqos[SMI_PORT_SV_WDMA_1].avg_bw,
				job->sv_mmqos[SMI_PORT_SV_STG_1].avg_bw,
				job->sv_mmqos[SMI_PORT_SV_WDMA_2].avg_bw,
				job->sv_mmqos[SMI_PORT_SV_STG_2].avg_bw,
				peak_bw + pd_peak_bw,
				job->sv_mmqos[SMI_PORT_SV_WDMA_0].peak_bw,
				job->sv_mmqos[SMI_PORT_SV_STG_0].peak_bw,
				job->sv_mmqos[SMI_PORT_SV_WDMA_1].peak_bw,
				job->sv_mmqos[SMI_PORT_SV_STG_1].peak_bw,
				job->sv_mmqos[SMI_PORT_SV_WDMA_2].peak_bw,
				job->sv_mmqos[SMI_PORT_SV_STG_2].peak_bw);

		fp->camsv_param[i].peak_bw = peak_bw + pd_peak_bw;
	}

	mtk_cam_sv_run_df_bw_update(sv_dev, total_peak_bw);

	return 0;
}

static int fill_pda_qos(struct mtk_cam_job *job,
						struct mtkcam_ipi_frame_param *fp)
{
	struct mraw_stats_cfg_param *mraw_param;
	struct mtk_mraw_pipeline *mraw_pipe;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	unsigned int mraw_pipe_idx;
	int i;

	// ---- any platform needs to be confirmed ----
	unsigned int out_byte_per_ROI = 1200;
	unsigned int img_bit = 16;
	unsigned int tbl_bit = 1;
	unsigned int Frame_Rate = 30;
	// --------------------------------------------
	unsigned int ROInum = 0;
	unsigned int FOV = 0;
	unsigned int width_roi = 0, height_roi = 0;
	unsigned int total_area = 0;
	unsigned int nbx_roi = 0, nby_roi = 0;
	unsigned int total_roi = 0;

	unsigned int Inter_Frame_Size_Width = 0;
	unsigned int Inter_Frame_Size_Height = 0;
	unsigned int Inter_Frame_Size = 0;
	unsigned int Inter_Frame_Size_FOV = 0;
	unsigned int WDMA_Data = 0, RDMA_Data = 0;
	unsigned int WDMA_PEAK_BW = 0, WDMA_AVG_BW = 0;
	unsigned int IMAGE_TABLE_RDMA_PEAK_BW = 0;
	unsigned int IMAGE_TABLE_RDMA_AVG_BW = 0;

	for (i = 0; i < ctx->num_mraw_subdevs; i++) {
		mraw_pipe_idx = ctx->mraw_subdev_idx[i];
		if (mraw_pipe_idx >= ctx->cam->pipelines.num_mraw)
			return 1;
		mraw_pipe = &ctx->cam->pipelines.mraw[mraw_pipe_idx];
		mraw_param = &mraw_pipe->res_config.stats_cfg_param;
		if (mraw_param->pda_dc_en)
			break;
	}

	if (!mraw_param || !mraw_param->pda_dc_en)
		return 0;

	Inter_Frame_Size_Width = (mraw_param->pda_cfg[0] & 0xFFFF);
	Inter_Frame_Size_Height = (mraw_param->pda_cfg[0] >> 16);

	if (Inter_Frame_Size_Width == 0 || Inter_Frame_Size_Height == 0) {
		pr_info("%s failed: Frame size is zero, width/height: %d/%d\n",
			__func__, Inter_Frame_Size_Width, Inter_Frame_Size_Height);
		return -EINVAL;
	}

	ROInum = (mraw_param->pda_cfg[2] & 0x1fc0) >> 6;

	if (CAM_DEBUG_ENABLED(MMQOS))
		pr_info("%s frame width/height/ROI num: %d %d %d\n",
			__func__,
			Inter_Frame_Size_Width,
			Inter_Frame_Size_Height,
			ROInum);

	if (ROInum == 0 || ROInum > 24) {
		pr_info("%s failed: ROI num(%d) is invalid\n", __func__, ROInum);
		return -EINVAL;
	}

	for (i = 0; i < ROInum; ++i) {
		width_roi = (mraw_param->pda_cfg[20 + i * 4] & 0xFFFF);
		height_roi = (mraw_param->pda_cfg[20 + i * 4] >> 16);

		nbx_roi = ((mraw_param->pda_cfg[21 + i * 4] >> 16) & 0x3F);
		nby_roi = ((mraw_param->pda_cfg[21 + i * 4] >> 22) & 0x3F);
		total_roi += nbx_roi * nby_roi;
		total_area += width_roi * height_roi * nbx_roi * nby_roi;

		if (CAM_DEBUG_ENABLED(MMQOS))
			pr_info("%s ROI:%d, w/h/nbx/nby:%d/%d/%d/%d, total_area/total_roi:%d/%d\n",
				__func__, i, width_roi, height_roi, nbx_roi, nby_roi,
				total_area, total_roi);
	}
	FOV = total_area * 100 / (Inter_Frame_Size_Width * Inter_Frame_Size_Height);

	if (CAM_DEBUG_ENABLED(MMQOS))
		pr_info("%s FOV:%d\n", __func__, FOV);

	if (total_roi > 1024) {
		pr_info("%s failed: total ROI num(%d) is out of range, max ROI num is 1024\n",
			__func__, total_roi);
		total_roi = 1024;
	}

	if (FOV > 200) {
		pr_info("%s failed: FOV(%d) is out of range, max FOV is 200\n",
			__func__, FOV);
		FOV = 200;
	}

	Inter_Frame_Size = Inter_Frame_Size_Width * Inter_Frame_Size_Height;

	Inter_Frame_Size_FOV = Inter_Frame_Size * FOV / 100;

	WDMA_Data = out_byte_per_ROI * total_roi;
	RDMA_Data = Inter_Frame_Size_FOV * (img_bit + tbl_bit) * 2 / 8;

	// WDMA BW estimate (KB/s)
	WDMA_AVG_BW = WDMA_Data * Frame_Rate / 1024;

	// Total RDMA BW (KB/s) (include LR image and table)
	IMAGE_TABLE_RDMA_AVG_BW = RDMA_Data * Frame_Rate / 1024;

	// pda is not HRT engine, no need to set HRT bw
	IMAGE_TABLE_RDMA_PEAK_BW = 0;
	WDMA_PEAK_BW = 0;

	if (CAM_DEBUG_ENABLED(MMQOS))
		pr_info("%s Total RDMA/WDMA BW (KB/s): %d/%d\n",
			__func__,
			IMAGE_TABLE_RDMA_AVG_BW,
			WDMA_AVG_BW);

	// one port only supports reading one image and one table, so divide by 2
	job->pda_mmqos[SMI_PORT_PDA_RDMA0].avg_bw = IMAGE_TABLE_RDMA_AVG_BW / 2;
	job->pda_mmqos[SMI_PORT_PDA_RDMA1].avg_bw = IMAGE_TABLE_RDMA_AVG_BW / 2;
	job->pda_mmqos[SMI_PORT_PDA_RDMA2].avg_bw = 0;
	job->pda_mmqos[SMI_PORT_PDA_RDMA3].avg_bw = 0;
	job->pda_mmqos[SMI_PORT_PDA_RDMA4].avg_bw = 0;
	job->pda_mmqos[SMI_PORT_PDA_WDMA].avg_bw = WDMA_AVG_BW;

	return 0;
}

static void update_sensor_active_info(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;

	if (!job->seninf)
		return;

	/* get active line time */
	if (ctx->act_line_info.avg_linetime_in_ns == 0 ||
		job->seamless_switch || job->raw_switch) {
		struct mtk_raw_sink_data *sink = get_raw_sink_data(job);

		if (!sink) {
			pr_info("%s: raw_data not found: ctx-%d job %d\n",
				 __func__, ctx->stream_id, job->frame_seq_no);
			return;
		}

		mtk_cam_seninf_get_active_line_info(
			job->seninf, sink->mbus_code, &ctx->act_line_info);
	}
}

void mtk_cam_fill_qos(struct req_buffer_helper *helper)
{
	struct mtkcam_ipi_frame_param *fp = helper->fp;
	struct mtk_cam_job *job = helper->job;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	u32 senser_vb, sensor_h, sensor_fps;
	u64 avg_linet;
	int i;
	struct ISP_P1 idx;

	memset(job->raw_mmqos, 0, sizeof(job->raw_mmqos));
	memset(job->yuv_mmqos, 0, sizeof(job->yuv_mmqos));
	memset(job->sv_mmqos, 0, sizeof(job->sv_mmqos));

	update_sensor_active_info(job);
	avg_linet =  ctx->act_line_info.avg_linetime_in_ns ? : get_line_time(job);
	sensor_h = ctx->act_line_info.active_line_num ? : get_sensor_h(job);
	senser_vb = get_sensor_vb(job);
	sensor_fps = get_sensor_fps(job);

	if (cam_sspm_en && (job->frame_seq_no % 10 == 0)) {
		idx.raw_num = get_used_raw_num(job);
		idx.exposure_num = job_exp_num(job);
		idx.fps = sensor_fps;
		idx.data = get_sensor_w(job) * sensor_h;
		dev_info(job->src_ctx->cam->dev,
			"[%s:p1_pmidx] FPS:%u, raw_num:%u, exp_num:%u, data:%u\n",
			__func__, idx.fps, idx.raw_num,
			idx.exposure_num, idx.data);
		set_p1_idx(idx);
	}

	if (avg_linet == 0 || sensor_h == 0 || sensor_fps == 0) {
		pr_info("%s: wrong sensor param h/vb/linetime/fps: %d/%d/%llu/%d",
			__func__, sensor_h, senser_vb, avg_linet, sensor_fps);
		return;
	}

	// re-do search, can we skip by fill qos while do "update_cam_buf_to_ipi_frame"
	for (i = 0; i < helper->io_idx; i++)
		fill_raw_out_qos(job, &fp->img_outs[i], sensor_h, senser_vb, avg_linet);

	for (i = 0; i < helper->ii_idx; i++)
		fill_raw_in_qos(job, &fp->img_ins[i], sensor_h, senser_vb, avg_linet);

	fill_raw_stats_qos(helper, sensor_h, senser_vb, avg_linet);

	/* camsv */
	fill_sv_qos(job, fp, sensor_h, senser_vb, avg_linet, sensor_fps);

	/* pda */
	fill_pda_qos(job, fp);
}

static bool apply_qos_chk(
		u32 new_avg, u32 new_peak, s64 *applied, s64 *pending)
{
	s64 bw = (new_peak > 0) ? new_peak : new_avg;
	bool ret = false;

	if (bw > 0 && bw > *applied) {
		*applied = bw;
		*pending = -1;
		ret = true;
	} else if (bw < *applied) {
		if (*pending >= 0 && bw >= *pending) {
			*applied = (bw == 0) ? -1 : bw;
			*pending = -1;
			ret = true;
		} else {
			*pending = bw;
			ret = false;
		}
	} else if (bw > 0 && bw == *applied) {
		*pending = -1;
		ret = false;
	}

	return ret;
}

static void apply_raw_qos(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_engines *eng = &cam->engines;
	struct mtk_raw_device *raw_dev;
	struct mtk_yuv_device *yuv_dev;
	unsigned long submask;
	u32 a_bw, p_bw, used_raw_num;
	bool apply, apply_bwr, is_w_port;
	int i, j, raw_num;
	int raw_avg_bw_r = 0, raw_avg_bw_w = 0, raw_peak_bw_r = 0, raw_peak_bw_w = 0;
	int yuv_avg_bw_r = 0, yuv_avg_bw_w = 0, yuv_peak_bw_r = 0, yuv_peak_bw_w = 0;
	int a_bw_ttl = 0, p_bw_ttl = 0;

	used_raw_num = get_used_raw_num(job);
	if (used_raw_num == 0) {
		pr_info("%s: req_seq(%d) wrong used raw number\n",
			__func__, job->req_seq);
		return;
	}

	raw_num = eng->num_raw_devices;
	submask = bit_map_subset_of(MAP_HW_RAW, ctx->used_engine);

	for (i = 0; i < raw_num && submask; i++, submask >>= 1) {
		if (!(submask & 0x1))
			continue;

		raw_dev = dev_get_drvdata(eng->raw_devs[i]);
		for (j = 0; j < raw_dev->qos.n_path; j++) {
			is_w_port = is_w_merge_port(j, RAW_DOMAIN);
			a_bw = (job->raw_mmqos[j].avg_bw / used_raw_num);
			p_bw = (job->raw_mmqos[j].peak_bw / used_raw_num);
			raw_peak_bw_w += is_w_port ? p_bw : 0;
			raw_avg_bw_w += is_w_port ? a_bw : 0;
			raw_peak_bw_r += is_w_port ? 0 : p_bw;
			raw_avg_bw_r += is_w_port ? 0 : a_bw;
			a_bw_ttl += a_bw;
			p_bw_ttl += p_bw;

			apply = apply_qos_chk(a_bw, p_bw,
					&raw_dev->qos.cam_path[j].applied_bw,
					&raw_dev->qos.cam_path[j].pending_bw);
			if (apply) {
				apply_bwr = true;
				mtk_icc_set_bw(raw_dev->qos.cam_path[j].path, a_bw, p_bw);
			}

			if (CAM_DEBUG_ENABLED(MMQOS))
				pr_info("%s: req_seq:%d %s raw-%d icc_path:%s avg/peak:%u/%u(KB/s) applied/pending:%lld/%lld(KB/s)\n",
					__func__, job->req_seq,
					apply ? "APPLY" : "BYPASS", i,
					raw_dev->qos.cam_path[j].name, a_bw, p_bw,
					raw_dev->qos.cam_path[j].applied_bw,
					raw_dev->qos.cam_path[j].pending_bw);
		}

		yuv_dev = dev_get_drvdata(eng->yuv_devs[i]);
		for (j = 0; j < yuv_dev->qos.n_path; j++) {
			is_w_port = is_w_merge_port(j, YUV_DOMAIN);
			a_bw = job->yuv_mmqos[j].avg_bw / used_raw_num;
			p_bw = job->yuv_mmqos[j].peak_bw / used_raw_num;
			yuv_peak_bw_w += is_w_port ? p_bw : 0;
			yuv_avg_bw_w += is_w_port ? a_bw : 0;
			yuv_peak_bw_r += is_w_port ? 0 : p_bw;
			yuv_avg_bw_r += is_w_port ? 0 : a_bw;
			a_bw_ttl += a_bw;
			p_bw_ttl += p_bw;

			apply = apply_qos_chk(a_bw, p_bw,
				&yuv_dev->qos.cam_path[j].applied_bw,
				&yuv_dev->qos.cam_path[j].pending_bw);

			if (apply) {
				apply_bwr = true;
				mtk_icc_set_bw(yuv_dev->qos.cam_path[j].path, a_bw, p_bw);
			}

			if (CAM_DEBUG_ENABLED(MMQOS))
				pr_info("%s: req_seq:%d %s yuv-%d icc_path:%s avg/peak:%u/%u(KB/s) applied/pending:%lld/%lld(KB/s)\n",
					__func__, job->req_seq,
					apply ? "APPLY" : "BYPASS", i,
					yuv_dev->qos.cam_path[j].name, a_bw, p_bw,
					yuv_dev->qos.cam_path[j].applied_bw,
					yuv_dev->qos.cam_path[j].pending_bw);
		}

		if (apply_bwr) {
			mtk_cam_isp8s_bwr_set_chn_bw(cam->bwr,
				get_bwr_engine(raw_dev->id), get_axi_port(raw_dev->id, true),
				KBps_to_bwr(raw_avg_bw_r), KBps_to_bwr(raw_avg_bw_w),
				KBps_to_bwr(raw_peak_bw_r), KBps_to_bwr(raw_peak_bw_w), true);

			mtk_cam_isp8s_bwr_set_chn_bw(cam->bwr,
				get_bwr_engine(yuv_dev->id), get_axi_port(yuv_dev->id, false),
				KBps_to_bwr(yuv_avg_bw_r), KBps_to_bwr(yuv_avg_bw_w),
				KBps_to_bwr(yuv_peak_bw_r), KBps_to_bwr(yuv_peak_bw_w), true);

			mtk_cam_isp8s_bwr_set_ttl_bw(cam->bwr,
				get_bwr_engine(raw_dev->id),
				KBps_to_bwr(a_bw_ttl), KBps_to_bwr(p_bw_ttl), true);

			raw_avg_bw_r = raw_avg_bw_w = raw_peak_bw_r = raw_peak_bw_w = 0;
			yuv_avg_bw_r = yuv_avg_bw_w = yuv_peak_bw_r = yuv_peak_bw_w = 0;
			a_bw_ttl = p_bw_ttl = 0;
		}
	}
}

static void apply_sv_qos(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_camsv_device *sv_dev = NULL;
	unsigned int fifo_img_p1, fifo_img_p2, fifo_img_p3 = 0;
	u32 a_bw, p_bw;
	int i, port_num = 0;
	unsigned int raw_id;
	bool apply, apply_sv_th = false, apply_bwr = false;
	int sv_avg_bw_w = 0, sv_peak_bw_w = 0, sv_avg_diff_bw_w = 0, sv_peak_diff_bw_w = 0;
	int sv_output_port;

	if (ctx->has_raw_subdev) {
		raw_id = get_master_raw_id(job->used_engine);
		sv_dev = dev_get_drvdata(cam->engines.sv_devs[raw_id]);
	} else {
		if (ctx->hw_sv)
			sv_dev = dev_get_drvdata(ctx->hw_sv);
	}

	if (sv_dev) {
		CALL_PLAT_V4L2(
			get_sv_smi_setting, sv_dev->id, &port_num);

		port_num = sv_dev->qos.n_path ? port_num : 0;

		for (i = 0; i < port_num; i++) {
			a_bw = job->sv_mmqos[i].avg_bw;
			p_bw = job->sv_mmqos[i].peak_bw;
			sv_avg_bw_w += a_bw;
			sv_peak_bw_w += p_bw;

			apply = apply_qos_chk(a_bw, p_bw,
					&sv_dev->qos.cam_path[i].applied_bw,
					&sv_dev->qos.cam_path[i].pending_bw);
			if (apply) {
				mtk_icc_set_bw(sv_dev->qos.cam_path[i].path, a_bw, p_bw);
				apply_sv_th = true;
				apply_bwr = true;
			}

			if (CAM_DEBUG_ENABLED(MMQOS))
				pr_info("%s: req_seq:%d %s sv-%d icc_path:%s avg/peak:%uKB/s, %uKB/s applied/pending:%lldKB/s, %lldKB/s\n",
						__func__, job->req_seq,
						apply ? "APPLY" : "BYPASS", sv_dev->id,
						sv_dev->qos.cam_path[i].name, a_bw, p_bw,
						sv_dev->qos.cam_path[i].applied_bw,
						sv_dev->qos.cam_path[i].pending_bw);
		}

		if (apply_bwr) {
			int a_bw_w_MB = KBps_to_bwr(sv_avg_bw_w);
			int p_bw_w_MB = KBps_to_bwr(sv_peak_bw_w);

			sv_avg_diff_bw_w = a_bw_w_MB - sv_dev->sv_avg_applied_bw_w;
			sv_peak_diff_bw_w = p_bw_w_MB - sv_dev->sv_peak_applied_bw_w;
			sv_dev->sv_avg_applied_bw_w = a_bw_w_MB;
			sv_dev->sv_peak_applied_bw_w = p_bw_w_MB;
			sv_output_port = get_sv_axi_port_num(sv_dev->id);
			for (i = 0; i < sv_output_port; i++) {
				mtk_cam_isp8s_bwr_set_chn_bw(cam->bwr,
					get_sv_bwr_engine(sv_dev->id), get_sv_axi_port(sv_dev->id, i),
					0, sv_avg_diff_bw_w / sv_output_port, 0,
					sv_peak_diff_bw_w / sv_output_port, false);
			}
			mtk_cam_isp8s_bwr_set_ttl_bw(cam->bwr,
				get_sv_bwr_engine(sv_dev->id), sv_avg_diff_bw_w, sv_peak_diff_bw_w,
				false);
		}

		if (apply_sv_th) {
			unsigned int fifo_core1 = 0, fifo_core2 = 0, fifo_core3 = 0;
			unsigned int lb_fifo = 0;

			/* fifo monitor */
			fifo_core1 =
				mtk_cam_sv_df_get_runtime_fifo_size(&cam->sv_df_mgr,
					sv_dev->id, 0);
			fifo_core2 =
				mtk_cam_sv_df_get_runtime_fifo_size(&cam->sv_df_mgr,
					sv_dev->id, 1);
			fifo_core3 =
				mtk_cam_sv_df_get_runtime_fifo_size(&cam->sv_df_mgr,
					sv_dev->id, 2);

			/* calculate fifo lowerbond */
			lb_fifo = (fifo_core1 + fifo_core2 + fifo_core3) * 2 / 10;

			/* apply fifo setting according to bandwidth */
			fifo_img_p1 =
				job->sv_mmqos[SMI_PORT_SV_WDMA_0].peak_bw * 64 * 12 / 1000000;
			fifo_img_p2 =
				job->sv_mmqos[SMI_PORT_SV_WDMA_1].peak_bw * 64 * 12 / 1000000;
			fifo_img_p3 =
				job->sv_mmqos[SMI_PORT_SV_WDMA_2].peak_bw * 64 * 12 / 1000000;

			fifo_img_p1 = max(min(fifo_img_p1, fifo_core1), lb_fifo);
			fifo_img_p2 = max(min(fifo_img_p2, fifo_core2), lb_fifo);
			fifo_img_p3 = max(min(fifo_img_p3, fifo_core3), lb_fifo);

			mtk_cam_sv_dmao_common_config(sv_dev, fifo_img_p1, fifo_img_p2, fifo_img_p3);

			/* apply golden setting */
			mtk_cam_sv_golden_set(sv_dev, is_dc_mode(job) ? true : false);
		}
	}
}

static void apply_pda_qos(struct mtk_cam_job *job)
{
	u32 a_bw, p_bw;
	struct mtk_pda_device *pda_dev = NULL;
	int i, port_num = 0;
	bool apply, apply_bwr = false;
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;
	unsigned int pda_rdma_totalbw = 0, pda_wdma_totalbw = 0;
	unsigned int ttl_bw_temp = 0;

	for (i = 0; i < ARRAY_SIZE(ctx->hw_pda); i++) {
		if (ctx->hw_pda[i])
			pda_dev = dev_get_drvdata(ctx->hw_pda[i]);
	}

	if (!pda_dev)
		return;

	port_num = pda_dev->qos.n_path ? pda_dev->qos.n_path : 0;
	for (i = 0; i < port_num; i++) {
		a_bw = job->pda_mmqos[i].avg_bw;
		if (i == (port_num - 1))
			pda_wdma_totalbw += a_bw;
		else
			pda_rdma_totalbw += a_bw;
		p_bw = 0;
		apply = apply_qos_chk(a_bw, p_bw,
			&pda_dev->qos.cam_path[i].applied_bw,
			&pda_dev->qos.cam_path[i].pending_bw);
		if (apply) {
			//report bw to mmqos
			mtk_icc_set_bw(pda_dev->qos.cam_path[i].path, a_bw, p_bw);
			apply_bwr = true;
		}
		if (CAM_DEBUG_ENABLED(MMQOS))
			pr_info("%s: req_seq:%d %s pda-%d icc_path:%s avg/peak:%uKB/s, %uKB/s applied/pending:%lldKB/s, %lldKB/s\n",
					__func__, job->req_seq,
					apply ? "APPLY" : "BYPASS", pda_dev->id,
					pda_dev->qos.cam_path[i].name, a_bw, p_bw,
					pda_dev->qos.cam_path[i].applied_bw,
					pda_dev->qos.cam_path[i].pending_bw);
	}


	if (apply_bwr) {
		//report bw to bwr, unit: KB/s to MB/s
		pda_rdma_totalbw /= 1024;
		pda_wdma_totalbw /= 1024;

		mtk_cam_isp8s_bwr_set_chn_bw(cam->bwr, ENGINE_PDA, CAM2_PORT,
			(int)(pda_rdma_totalbw), (int)(pda_wdma_totalbw), 0, 0, true);

		ttl_bw_temp = (pda_rdma_totalbw + pda_wdma_totalbw);

		mtk_cam_isp8s_bwr_set_ttl_bw(cam->bwr, ENGINE_PDA,
			(int)(ttl_bw_temp), 0, true);
		if (CAM_DEBUG_ENABLED(MMQOS))
			pr_info("Total RDMA/WDMA BW (MB/s): %d/%d\n",
				pda_rdma_totalbw, pda_wdma_totalbw);
	}

}

/* threaded irq context */
int mtk_cam_apply_qos(struct mtk_cam_job *job)
{
	struct mtk_cam_ctx *ctx = job->src_ctx;
	struct mtk_cam_device *cam = ctx->cam;

	qof_mtcmos_voter(&cam->engines, job->used_engine, true);
	apply_raw_qos(job);
	qof_mtcmos_voter(&cam->engines, job->used_engine, false);

	apply_sv_qos(job);
	apply_pda_qos(job);

	if (CAM_DEBUG_ENABLED(MMQOS))
		mtk_cam_isp8s_bwr_dbg_dump(cam->bwr);

	/* note: may sleep */
	mtk_mmqos_wait_throttle_done();

	return 0;
}

/* reset after disable irq */
int mtk_cam_reset_qos(struct device *dev, struct mtk_camsys_qos *qos)
{
	struct mtk_camsys_qos_path *cam_path;
	int i;

	for (i = 0, cam_path = qos->cam_path; i < qos->n_path; i++, cam_path++) {
		if (cam_path->applied_bw >= 0) {
			mtk_icc_set_bw(cam_path->path, 0, 0);
			cam_path->applied_bw = -1;
			cam_path->pending_bw = -1;
		}
	}

	if (CAM_DEBUG_ENABLED(MMQOS))
		dev_info(dev, "mmqos reset done\n");

	return 0;
}
