// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include "mtk_cam_ut.h"
#include "mtk_cam_ut-engines.h"
#include "mtk_cam_regs.h"

static unsigned int testmdl_hblank = 0x400;
module_param(testmdl_hblank, int, 0644);
MODULE_PARM_DESC(testmdl_hblank, "h-blanking for testmdl");

#define WRITE_BITS(addr, offset, mask, val) do { \
	u32 __v = readl(addr); \
	u32 field_mask = mask << offset;\
	__v &= ~field_mask; \
	__v |= (((val) << offset) & field_mask); \
	writel(__v, addr); \
} while (0)

#define WRITE_SENINF_TOP_MUX_BITS(addr, mux_idx, val) do { \
	u32 idx = mux_idx % 4; \
	u32 offset = idx * 8;\
	WRITE_BITS(addr, offset, 0x1f, val);\
} while (0)

#define SET_TAG(ptr, page, sel, vc, dt, first) do { \
	u32 offset = 24;\
	WRITE_BITS(ISP_SENINF_CAM_MUX_PCSR_OPT(ptr), offset, 0x7, page);\
	offset = sel * 8;\
	WRITE_BITS(ISP_SENINF_CAM_MUX_PCSR_VC_SEL(ptr), offset, 0x1f, vc);\
	offset = ((sel + 1) * 8) - 1;\
	WRITE_BITS(ISP_SENINF_CAM_MUX_PCSR_VC_SEL(ptr), offset, 0x1, first);\
	offset = sel * 8;\
	WRITE_BITS(ISP_SENINF_CAM_MUX_PCSR_DT_SEL(ptr), offset, 0x1f, dt);\
	offset = ((sel + 1) * 8) - 1;\
	WRITE_BITS(ISP_SENINF_CAM_MUX_PCSR_DT_SEL(ptr), offset, 0x1, first);\
} while (0)


#define SENINF_LOGD(fmt, arg...) dev_info(dev, "[%s][%d] " fmt, __func__, __LINE__, ##arg)
#define SENINF_LOGE(fmt, arg...) dev_info(dev, "[ERROR:][%s][%d] " fmt, __func__, __LINE__, ##arg)


enum CAMMUX_TYPE_ENUM {
	TYPE_CAMSV_SAT,
	TYPE_CAMSV_NORMAL,
	TYPE_RAW,
	TYPE_PDP,
	TYPE_MAX_NUM,
};

enum HDR_NUM {
	NE,
	ME,
	SE,
	MAX_HDR_NUM,
};

struct exposure_info {
	u8 VC;
	u8 DT;
};

struct seninf_tm_cfg {
	u8 seninf_idx;
	u8 bit_depth;
	u8 exposure_num;
	u16 img_w;
	u16 img_h;
	struct exposure_info vc_info[MAX_HDR_NUM];
};

struct seninf_out_mux_cfg {
	struct mtk_cam_ut_tm_para *para;
	struct exposure_info vc_info[MAX_HDR_NUM];
	u8 para_cnt;
	u8 exposure_num;
	u8 seninf_idx;
	u16 img_w;
	u16 img_h;
};

/* seninf */
static int get_test_hmargin(u16 w, u16 h, u8 clk_cnt, u16 clk_mhz, u8 fps)
{
	int target_h = clk_mhz * (1000000/fps) / w * max(16/(clk_cnt+1), 1);

	return max(target_h - h, 0x80);
}

#define SENINF_TM_OFFSET 0x200

static inline u16 check_width_tm_limitation(u16 width_tm)
{
	return (width_tm % 8) ? (((width_tm >> 3) + 1) << 3) : width_tm;
}

static int set_seninf_tm(struct device *dev, struct mtk_ut_seninf_device *seninf, struct seninf_tm_cfg cfg)
{
	void __iomem *seninf_tm;
	u8 last_vc = 0;
	u16 width_tm;
	u16 width_tm_bit;
	const u8 c_clk_div_cnt = 0xF;
	const u8 c_fps = 30;
	const u16 c_isp_clk = 412;
	const u16 c_dummy_pxl = testmdl_hblank;
	const u16 c_h_margin = 0x1000;
	const u16 c_dum_vsync = get_test_hmargin(
							cfg.img_w + c_dummy_pxl,
							cfg.img_h + c_h_margin,
							c_clk_div_cnt,
							c_isp_clk,
							c_fps);
	bool stream_on_en = 0;

	width_tm = check_width_tm_limitation(cfg.img_w >> 1);
	width_tm_bit = width_tm * cfg.bit_depth;

	dev_info(dev, "%s width %d5 x height %d dum_vsync %d clk_div_cnt %d, to width_tm %d, width_tm_bit %d\n",
					__func__,
					cfg.img_w,
					cfg.img_h,
					c_dum_vsync,
					c_clk_div_cnt,
					width_tm,
					width_tm_bit);


	seninf_tm = ISP_SENINF_TM_BASE_BY_ID(seninf->base_tm, cfg.seninf_idx);

	writel(cfg.img_h << 16 | width_tm, ISP_SENINF_TM_SIZE(seninf_tm));
	writel(c_dum_vsync << 16 | c_dummy_pxl, ISP_SENINF_TM_DUM(seninf_tm));
	writel(width_tm_bit, ISP_SENINF_TM_BIT(seninf_tm));

	/* setting 1 EXP data */
	writel( ((cfg.vc_info[NE].VC &  0x1f) << 16) |
			((cfg.vc_info[NE].VC &  0x1f) << 8) |
			(cfg.vc_info[NE].DT),
			ISP_SENINF_TM_CON0(seninf_tm));

	/* setting 2 EXP data */
	if (cfg.exposure_num > ME) {
		writel( ((cfg.vc_info[ME].VC &  0x1f) << 16) |
				((cfg.vc_info[ME].VC &  0x1f) << 8) |
				(cfg.vc_info[ME].DT),
				ISP_SENINF_TM_CON1(seninf_tm));

		// sof offset 100 and dedicated fs
		writel((0x64 << 12) | (0x1 << 30), ISP_SENINF_TM_EXP1_CTRL(seninf_tm));
		last_vc = 1;
	}

	/* setting 3 EXP data */
	if (cfg.exposure_num > SE) {
		writel( ((cfg.vc_info[SE].VC &  0x1f) << 16) |
				((cfg.vc_info[SE].VC &  0x1f) << 8) |
				(cfg.vc_info[SE].DT),
				ISP_SENINF_TM_CON2(seninf_tm));

		// sof offset 200 and dedicated fs
		writel((0xc8 << 12) | (0x1 << 30), ISP_SENINF_TM_EXP2_CTRL(seninf_tm));
		last_vc = 2;
	}

	stream_on_en = false;
	/* setting stream on  */
	writel(	(c_clk_div_cnt << 16) |
			((HOIZONTAL_COLOR_BAR & 0xF) << 8) |
			(stream_on_en & 0x1) |
			(((cfg.exposure_num - 1) & 0x7) << 12),
			ISP_SENINF_TM_CORE0_CTL(seninf_tm));


	stream_on_en = true;
	writel((stream_on_en & 0x1) |
			readl(ISP_SENINF_TM_CORE0_CTL(seninf_tm)),
			ISP_SENINF_TM_CORE0_CTL(seninf_tm));

	dev_info(dev, "%s reg core0 = 0x%x\n", __func__, readl(ISP_SENINF_TM_CORE0_CTL(seninf_tm)));

	return 0;
}

static int set_seninf_asnyc(struct device *dev, struct mtk_ut_seninf_device *seninf)
{
	void __iomem *seninf_async = seninf->base_async;

	/* seninf async */
	writel(0x1, ISP_SENINF_ASYNC_CFG(seninf_async));  // set tmdl as input src

	return 0;
}

static int set_seninf_top(struct device *dev, struct mtk_ut_seninf_device *seninf)
{
	void __iomem *seninf_top = seninf->base_top;

	/* seninf top */
	 // writel(0x1000000, ISP_SENINF_TOP_CTL(seninf_top));  // no need on jeyer
	writel(0x0000003F, ISP_SENINF_TOP_ASYNC_CG(seninf_top));
	writel(0x7FFFFF, ISP_SENINF_TOP_OUTMUX_CG(seninf_top));

	dev_info(dev, "%s target here %d\n", __func__, __LINE__);

	return 0;
}

static int set_out_mux(struct device *dev, struct mtk_ut_seninf_device *seninf, struct seninf_out_mux_cfg cfg)
{
	int i;
	struct mtk_cam_ut_tm_para *para;
	int outmux_idx, tag;
	void __iomem *outmux_base;

	u8 last_vc = 0;
	u8 pix_m = 0;
	u8 bit_depth = 16;
	u8 bit2byte = 8;
	u8 vc = 0x00;
	u8 dt = 0x00;

	u8 set_outmux_list[SENINF_MUX_NUM];

	memset(set_outmux_list, 0, sizeof(set_outmux_list));

	if (cfg.exposure_num  == 0) {
		SENINF_LOGE("cfg.exposure_num %d is invalid\n", cfg.exposure_num);
		return -EINVAL;
	}

	last_vc = cfg.vc_info[cfg.exposure_num -1].VC;

	for (i = 0; i < cfg.para_cnt; i++) {
		para = cfg.para + i;
		outmux_idx = para->tg_idx;
		tag = para->tag;
		pix_m = (para->pixmode == tm_pix_mode_16) ? 1 : 0;
		vc = cfg.vc_info[i].VC;
		dt = cfg.vc_info[i].DT;

		dev_info(dev, "%s seninf_idx %d outmux_idx %d tag %d pixmode %d vc = 0x%x dt = 0x%x last_vc = 0x%x\n",
			 __func__, cfg.seninf_idx, outmux_idx, tag, para->pixmode, vc, dt, last_vc);

		if (outmux_idx >= SENINF_MUX_NUM) {
			SENINF_LOGE("outmux_idx %d is invalid\n", outmux_idx);
			return -EINVAL;
		}

		set_outmux_list[outmux_idx] = 1;

		/* outmux */
		outmux_base = seninf->base_outmux[outmux_idx];
		writel(0x21001000, ISP_SENINF_OUTMUX_CFG_CTRL(outmux_base));  // set DL_EN on
		writel(pix_m, ISP_SENINF_OUTMUX_PIX_MODE(outmux_base));
		writel(0x0 | (last_vc << 16), ISP_SENINF_OUTMUX_SOURCE_CFG0(outmux_base));
		writel(0x0, ISP_SENINF_OUTMUX_SRC_SEL(outmux_base));
		writel(0xFFFFFFFF, ISP_SENINF_OUTMUX_IRQ_EN(outmux_base));

		writel(	((dt & 0x3f) << 16) |
				((vc & 0x1f) << 8) |
				0x1 , ISP_SENINF_OUTMUX_TAG_VCDT(outmux_base, tag));
		writel((((cfg.img_w * bit_depth / bit2byte) - 1) & 0xFFFF) |
			(((cfg.img_h - 1) & 0xFFFF) << 16),
			ISP_SENINF_OUTMUX_TAG_EXP_SIZE(outmux_base, tag));
		writel(0x4, ISP_SENINF_OUTMUX_TAG_EXP_BYTE_2_PIX(outmux_base, tag));


		if (tag >= tag_0 && tag <= tag_3)
			writel((vc << (tag * 8)), ISP_SENINF_OUTMUX_SOURCE_CFG1(outmux_base));
		else if (tag >= tag_4 && tag <= tag_7)
			writel((vc << ((tag - tag_4) * 8)), ISP_SENINF_OUTMUX_SOURCE_CFG2(outmux_base));
	}

	for (i = 0; i < SENINF_MUX_NUM; i++) {
		if (set_outmux_list[i]) {
			outmux_base = seninf->base_outmux[i];
			writel(0x1, ISP_SENINF_OUTMUX_CFG_RDY(outmux_base));
			writel(0x1, ISP_SENINF_OUTMUX_CFG_DONE(outmux_base));
		}

	}
	return 0;
}

static int ut_seninf_set_testmdl(struct device *dev,
				 int width, int height,
				 int pattern,/*unused*/
				 u8 exp_num,
				 struct mtk_cam_ut_tm_para *tm_para,
				 int para_cnt)
{

	struct mtk_ut_seninf_device *seninf = dev_get_drvdata(dev);
	struct seninf_tm_cfg tml_cfg;
	struct seninf_out_mux_cfg out_mux_cfg;
	int seninf_idx;

	// hard code seninf idx use first seninf_idx
	seninf_idx = 0;
	seninf->seninf_status[seninf_idx] = USING;

	SENINF_LOGD("width %d x height %dexp_num %d para_cnt %d\n",
				width, height, exp_num, para_cnt);

	if (!exp_num)// exp_num is 0, set to 1
		exp_num = 1;


	/* test mdl */
	tml_cfg.img_h = height;
	tml_cfg.img_w = width;
	tml_cfg.exposure_num = exp_num;
	tml_cfg.seninf_idx = seninf_idx;
	tml_cfg.vc_info[NE].VC = 0x00;
	tml_cfg.vc_info[NE].DT = 0x2B;

	tml_cfg.vc_info[ME].VC = 0x01;
	tml_cfg.vc_info[ME].DT = 0x2B;

	tml_cfg.vc_info[SE].VC = 0x02;
	tml_cfg.vc_info[SE].DT = 0x2B;
	tml_cfg.bit_depth = 16;

	if (set_seninf_top(dev, seninf)) {
		SENINF_LOGE("set_seninf_top return failed\n");
		return -EINVAL;
	}

	if (set_seninf_asnyc(dev, seninf)) {
		SENINF_LOGE("set_seninf_asnyc return failed\n");
		return -EINVAL;
	}

	out_mux_cfg.para = (struct mtk_cam_ut_tm_para *)tm_para;
	out_mux_cfg.para_cnt = para_cnt;
	out_mux_cfg.exposure_num = exp_num;
	out_mux_cfg.seninf_idx = seninf_idx;
	out_mux_cfg.img_h = height;
	out_mux_cfg.img_w = width;

	memcpy(out_mux_cfg.vc_info, tml_cfg.vc_info, sizeof(out_mux_cfg.vc_info));

	if (set_out_mux(dev, seninf, out_mux_cfg)) {
		SENINF_LOGE("set_out_mux return failed\n");
		return -EINVAL;
	}

	if (set_seninf_tm(dev, seninf, tml_cfg)) {
		SENINF_LOGE("set_seninf_tm return failed\n");
		return -EINVAL;
	}

	return 0;
}

static int ut_seninf_reset(struct device *dev)
{
	return 0;
}

static void ut_seninf_set_ops(struct device *dev)
{
	struct mtk_ut_seninf_device *seninf = dev_get_drvdata(dev);

	seninf->ops.set_size = ut_seninf_set_testmdl;
	seninf->ops.reset = ut_seninf_reset;
}

static int mtk_ut_seninf_component_bind(struct device *dev,
					struct device *master,
					void *data)
{
	struct mtk_cam_ut *ut = data;

	ut->seninf = dev;

	return 0;
}

static void mtk_ut_seninf_component_unbind(struct device *dev,
					   struct device *master,
					   void *data)
{
	struct mtk_cam_ut *ut = data;

	ut->seninf = NULL;
}

static const struct component_ops mtk_ut_seninf_component_ops = {
	.bind = mtk_ut_seninf_component_bind,
	.unbind = mtk_ut_seninf_component_unbind,
};

static int mtk_ut_seninf_of_probe(struct platform_device *pdev,
			    struct mtk_ut_seninf_device *seninf)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int i, clks;
	struct device_node *tmp_node = NULL;
	int index;

	/* top base register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "seninf-top");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	seninf->base_top = devm_ioremap_resource(dev, res);
	if (IS_ERR(seninf->base_top)) {
		dev_info(dev, "failed to map register base top\n");
		return PTR_ERR(seninf->base_top);
	}

	dev_info(dev, "seninf, map_addr=0x%lx\n", (unsigned long)seninf->base_top);

	/* async base register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "seninf-async-top");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	seninf->base_async = devm_ioremap_resource(dev, res);
	if (IS_ERR(seninf->base_async)) {
		dev_info(dev, "failed to map register base async\n");
		return PTR_ERR(seninf->base_async);
	}
	dev_info(dev, "seninf, map_addr=0x%lx\n", (unsigned long)seninf->base_async);

	/* tm base register */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "seninf-tm");
	if (!res) {
		dev_info(dev, "failed to get mem\n");
		return -ENODEV;
	}

	seninf->base_tm = devm_ioremap_resource(dev, res);
	if (IS_ERR(seninf->base_tm)) {
		dev_info(dev, "failed to map register base tm\n");
		return PTR_ERR(seninf->base_tm);
	}
	dev_info(dev, "seninf, map_addr=0x%lx\n", (unsigned long)seninf->base_tm);

	/* outmux base register */
	i = 0;
	while ((tmp_node = of_find_compatible_node(tmp_node, NULL, "mediatek,seninf-outmux"))) {
		index = of_property_match_string(tmp_node, "reg-names", "base");

		if (index < 0) { /* Fail */
			dev_info(dev, "get seninf outmux reg base failed\n");
			continue;
		}

		/* Success */
		dev_info(dev, "get seninf outmux reg base succeeded\n");

		seninf->base_outmux[i] = devm_of_iomap(dev, tmp_node, index, NULL);
		if (IS_ERR(seninf->base_outmux[i]))
			dev_info(dev, "seninf outmux[%d] ioremap failed\n", i);
		else
			i++;
	}
	seninf->num_outmux = i;


	clks = of_count_phandle_with_args(pdev->dev.of_node,
				"clocks", "#clock-cells");

	seninf->num_clks = (clks <= 0) ? 0:clks;
	dev_info(dev, "clk_num:%d\n", seninf->num_clks);

	if (seninf->num_clks) {
		seninf->clks = devm_kcalloc(dev, seninf->num_clks,
					    sizeof(*seninf->clks), GFP_KERNEL);
		if (!seninf->clks)
			return -ENODEV;
	}

	for (i = 0; i < seninf->num_clks; i++) {
		seninf->clks[i] = of_clk_get(pdev->dev.of_node, i);
		if (IS_ERR(seninf->clks[i])) {
			dev_info(dev, "failed to get clk %d\n", i);
			return -ENODEV;
		}
	}

	// init mux_status as all mux are free to used
	for (i = 0; i < SENINF_MUX_NUM; i++)
		seninf->seninf_mux_status[i] = IDLE;

	// init seninf_status as all mux are free to used
	for (i = 0; i < SENINF_NUM; i++)
		seninf->seninf_status[i] = IDLE;

	return 0;
}

static int mtk_ut_seninf_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_ut_seninf_device *seninf;
	int ret;

	seninf = devm_kzalloc(dev, sizeof(*seninf), GFP_KERNEL);
	if (!seninf)
		return -ENOMEM;

	seninf->dev = dev;
	dev_set_drvdata(dev, seninf);

	ret = mtk_ut_seninf_of_probe(pdev, seninf);
	if (ret)
		return ret;

	ut_seninf_set_ops(dev);

	pm_runtime_enable(dev);

	ret = component_add(dev, &mtk_ut_seninf_component_ops);
	if (ret)
		return ret;

	dev_info(dev, "%s: success\n", __func__);
	return 0;
}

static void mtk_ut_seninf_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_ut_seninf_device *seninf = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < seninf->num_clks; i++) {
		if (seninf->clks[i])
			clk_put(seninf->clks[i]);
	}

	pm_runtime_disable(dev);

	component_del(dev, &mtk_ut_seninf_component_ops);
}

static int mtk_ut_seninf_pm_suspend(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);
	return 0;
}

static int mtk_ut_seninf_pm_resume(struct device *dev)
{
	dev_dbg(dev, "- %s\n", __func__);
	return 0;
}

static int mtk_ut_seninf_runtime_suspend(struct device *dev)
{
	struct mtk_ut_seninf_device *seninf = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < seninf->num_clks; i++)
		clk_disable_unprepare(seninf->clks[i]);

	return 0;
}

static int mtk_ut_seninf_runtime_resume(struct device *dev)
{
	struct mtk_ut_seninf_device *seninf = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < seninf->num_clks; i++)
		clk_prepare_enable(seninf->clks[i]);

	return 0;
}

static const struct dev_pm_ops mtk_ut_seninf_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_ut_seninf_pm_suspend, mtk_ut_seninf_pm_resume)
	SET_RUNTIME_PM_OPS(mtk_ut_seninf_runtime_suspend, mtk_ut_seninf_runtime_resume,
			   NULL)
};

static const struct of_device_id mtk_ut_seninf_of_ids[] = {
	{.compatible = "mediatek,seninf-core",},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_ut_seninf_of_ids);

struct platform_driver mtk_ut_seninf_driver = {
	.probe   = mtk_ut_seninf_probe,
	.remove  = mtk_ut_seninf_remove,
	.driver  = {
		.name  = "mtk-cam seninf-ut",
		.of_match_table = of_match_ptr(mtk_ut_seninf_of_ids),
		.pm     = &mtk_ut_seninf_pm_ops,
	}
};
