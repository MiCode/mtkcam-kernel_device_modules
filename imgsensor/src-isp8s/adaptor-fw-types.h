/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2024 MediaTek Inc. */

#ifndef __ADAPTOR_FW_TYPES_H__
#define __ADAPTOR_FW_TYPES_H__

#include <linux/types.h>

/* --- Sections --- */

enum fw_section_enum {
	SECTION_HEADER = 0,
	SECTION_PW_SEQ,
	SECTION_EEPROM_INFO,
	SECTION_SENSOR_GLOBAL_INFO,
	SECTION_MODE_INFO,
	SECTION_EMBEDDED_INFO,
	SECTION_MAX_NUM
};

struct fw_section {
	u16 flag;
	u32 fw_offset;
	u32 fw_size;
} __packed;

/* --- Header section --- */

#define NAME_LENGTH_MAX 64
struct fw_header {
	u32 major_version;
	u32 revision;
	u64 last_modified_ts;
	char generator_version[NAME_LENGTH_MAX];
	char sensor_name[NAME_LENGTH_MAX];
	u32 sensor_id;
} __packed;

/* --- power sequence section --- */

struct fw_subdrv_pw_val {
	int para1;
	int para2;
} __packed;

struct fw_subdrv_pw_seq_entry {
	int id;
	struct fw_subdrv_pw_val val;
	int delay;
} __packed;

struct fw_pw_seq {
	int pw_seq_cnt;
	struct fw_subdrv_pw_seq_entry *pw_seq;
	int aov_pw_seq_cnt;
	struct fw_subdrv_pw_seq_entry *aov_pw_seq;
	int hw2sw_standby_pw_seq_cnt;
	struct fw_subdrv_pw_seq_entry *hw2sw_standby_pw_seq;
} __packed;

/* --- eeprom info section --- */

struct fw_eeprom_info_dynamic_size {
	u8 *qsc_table;
	u8 *pdc_table;
	u8 *lrc_table;
	u8 *xtalk_table;
} __packed;

struct fw_eeprom_info_struct {
	u32 header_id;
	u32 addr_header_id;
	u8 i2c_write_id;

	u8 qsc_support;
	u16 qsc_size;
	u16 addr_qsc;
	u16 sensor_reg_addr_qsc;
	u16 qsc_table_size;

	u8 pdc_support;
	u16 pdc_size;
	u16 addr_pdc;
	u16 sensor_reg_addr_pdc;
	u16 pdc_table_size;

	u8 lrc_support;
	u16 lrc_size;
	u16 addr_lrc;
	u16 sensor_reg_addr_lrc;
	u16 lrc_table_size;

	u8 xtalk_support; /* [1]sw-remo; [0]hw-remo; 0, not support */
	u16 xtalk_size;
	u16 addr_xtalk;
	u16 sensor_reg_addr_xtalk;
	u16 xtalk_table_size;

	/* the struct contains dynamic size parts */
	struct fw_eeprom_info_dynamic_size dynamic;
} __packed;

struct fw_eeprom_infos {
	u32 eeprom_info_num;
	struct fw_eeprom_info_struct *eeprom_info_list;
} __packed;

/* --- sensor global info section --- */

#define MAX_EXPOSURE_CNT 5
#define MAX_LUT_CNT 5

#define REG_ADDR_MAXCNT 4
struct fw_reg_ {
	u16 addr[REG_ADDR_MAXCNT];
} __packed;

struct fw_mtk_sensor_saturation_info {
	u32 gain_ratio;
	u32 OB_pedestal;
	u32 saturation_level;
	u32 adc_bit;
	u32 ob_bm;
} __packed;

struct fw_mtk_sensor_ctle_param {
	u8 eq_latch_en;
	u8 eq_dg1_en;
	u8 eq_dg0_en;
	u8 cdr_delay;
	u8 eq_is;
	u8 eq_bw;
	u8 eq_sr0;
	u8 eq_sr1;
} __packed;

struct fw_mtk_sensor_insertion_loss {
	u32 loss;
} __packed;

struct fw_reg_setting_values {
	u16 *setting_table;
} __packed;

struct fw_reg_setting_entry {
	u32 i2c_transfer_tlb_data_type;
	u32 setting_table_len;
	int delay;

	struct fw_reg_setting_values reg_addr_value;
} __packed;

struct fw_sensor_global_info_dynamic_size {
	u32 *ana_gain_table;
	struct fw_mtk_sensor_saturation_info *saturation_info;
	struct fw_mtk_sensor_ctle_param *ctle_param;
	struct fw_mtk_sensor_insertion_loss *insertion_loss;
	struct fw_reg_setting_entry *init_setting_table;
	u16 *i3c_precfg_setting_table;
	char *cust_global_data;
} __packed;

struct fw_sensor_global_info {
	u32 sensor_id_match;
	struct fw_reg_ reg_addr_header_id;
	u32 i2c_burst_write_support;
	u32 i2c_transfer_data_type;
	u8 mirror;

	u8 sensor_interface_type;
	u8 mipi_sensor_type;
	u8 mipi_lane_num;
	u32 ob_pedestal;
	u8 line_interleave_num;
	u8 sensor_output_dataformat;

	u32 ana_gain_def;
	u32 ana_gain_min;
	u32 ana_gain_max;
	u32 ana_gain_type;
	u32 ana_gain_step;
	u32 ana_gain_table_cnt;

	u32 dig_gain_min;
	u32 dig_gain_max;
	u32 dig_gain_step;
	u32 tuning_iso_base;
	u32 exposure_def;
	u32 exposure_min;
	u32 exposure_max;
	u32 exposure_step;
	u8 exposure_margin;
	u8 has_saturation_info;
	u8 has_ctle_param;
	u8 has_insertion_loss;

	u32 frame_length_max;
	u32 frame_length_max_without_lshift;
	u8 ae_effective_frame;
	u8 frame_time_delay_frame; /* EX: sony => 3 ; non-sony => 2 */
	u32 start_exposure_offset;
	u32 start_exposure_offset_custom;

	u32 pdaf_type;
	u32 hdr_type;
	u32 rgbw_support;
	u8 seamless_switch_support;

	u32 seamless_switch_type;
	u32 seamless_switch_hw_re_init_time_ns;
	u8 seamless_switch_prsh_hw_fixed_value;
	u32 seamless_switch_prsh_length_lc;
	struct fw_reg_ reg_addr_prsh_length_lines;
	u16 reg_addr_prsh_mode;
	u8 temperature_support;

	u16 reg_addr_stream;
	u16 reg_addr_mirror_flip;
	struct fw_reg_ reg_addr_exposure[MAX_EXPOSURE_CNT];
	struct fw_reg_ reg_addr_exposure_in_lut[MAX_EXPOSURE_CNT];
	u8 fll_lshift_max;
	u8 cit_lshift_max;
	u16 long_exposure_support;
	u16 reg_addr_exposure_lshift;
	u16 reg_addr_frame_length_lshift;
	struct fw_reg_ reg_addr_ana_gain[MAX_EXPOSURE_CNT];
	struct fw_reg_ reg_addr_ana_gain_in_lut[MAX_EXPOSURE_CNT];
	struct fw_reg_ reg_addr_dig_gain[MAX_EXPOSURE_CNT];
	struct fw_reg_ reg_addr_dig_gain_in_lut[MAX_EXPOSURE_CNT];
	struct fw_reg_ reg_addr_frame_length;
	struct fw_reg_ reg_addr_frame_length_in_lut[MAX_EXPOSURE_CNT];
	u16 reg_addr_temp_en;
	u16 reg_addr_temp_read;
	u16 reg_addr_auto_extend;
	u16 reg_addr_frame_count;
	u16 reg_addr_fast_mode;
	u16 reg_addr_dcg_ratio;
	u16 reg_addr_fast_mode_in_lbmf;
	u16 reg_addr_stream_in_lbmf;

	u32 init_setting_table_cnt;

	u8 chk_s_off_sta;
	u8 chk_s_off_end;
	u32 checksum_value;

	u8 aov_sensor_support;
	u32 aov_csi_clk;
	u32 sensor_mode_ops;
	u8 sensor_debug_sensing_ut_on_scp;
	u8 sensor_debug_dphy_global_timing_continuous_clk;
	u16 reg_addr_aov_mode_mirror_flip;
	u8 init_in_open;
	u8 streaming_ctrl_imp;
	u64 custom_stream_ctrl_delay;

	u32 cycle_base_ratio;
	u32 stagger_rg_order;
	u32 stagger_fl_type;

	u8 use_mcss_gph_sync;
	u16 reg_addr_mcss_slave_add_en_2nd;
	u16 reg_addr_mcss_slave_add_acken_2nd;
	u16 reg_addr_mcss_controller_target_sel;
	u16 reg_addr_mcss_xvs_io_ctrl;
	u16 reg_addr_mcss_extout_en;
	u16 reg_addr_mcss_sgmsync_sel;
	u16 reg_addr_mcss_swdio_io_ctrl;
	u16 reg_addr_mcss_gph_sync_mode;
	u16 reg_addr_mcss_complete_sleep_en;
	u16 reg_addr_mcss_mc_frm_lp_en;
	u16 reg_addr_mcss_frm_length_reflect_timing;
	u16 reg_addr_mcss_mc_frm_mask_num;

	u32 i3c_precfg_setting_len;

	u32 cust_global_data_len;
	u32 ocl_info;
	/* the struct contains dynamic size parts */
	struct fw_sensor_global_info_dynamic_size dynamic;
} __packed;

/* --- mode section --- */

struct fw_pd_u32_pair {
	u32 para1;
	u32 para2;
} __packed;

struct fw_pd_map_info_t {
	u32 i4VCFeature;
	u32 i4PDPattern;
	u32 i4BinFacX;
	u32 i4BinFacY;
	u32 i4PDRepetition;

	u32 i4PDOrder_cnt;
	u32 *i4PDOrder;
} __packed;

struct fw_set_pd_block_info_t_dynamic_size {
	struct fw_pd_u32_pair *i4PosL;
	struct fw_pd_u32_pair *i4PosR;
	struct fw_pd_u32_pair *i4Crop;
	struct fw_pd_map_info_t *sPDMapInfo;
} __packed;

struct fw_set_pd_block_info_t {
	u32 i4OffsetX;
	u32 i4OffsetY;
	u32 i4PitchX;
	u32 i4PitchY;
	u32 i4PairNum;
	u32 i4SubBlkW;
	u32 i4SubBlkH;
	u32 iMirrorFlip;
	u32 i4BlockNumX;
	u32 i4BlockNumY;
	u32 i4LeFirst;
	u32 i4VolumeX;
	u32 i4VolumeY;
	u32 i4FullRawW;
	u32 i4FullRawH;
	u32 i4VCPackNum;
	u32 i4ModeIndex;
	u32 i4NoTrs;
	u32 PDAF_Support;

	u32 i4PosL_cnt;
	u32 i4PosR_cnt;
	u32 i4Crop_cnt;
	u32 sPDMapInfo_cnt;

	struct fw_set_pd_block_info_t_dynamic_size dynamic;
} __packed;

struct fw_winsize_info {
	u16 full_w;
	u16 full_h;
	u16 x0_offset;
	u16 y0_offset;
	u16 w0_size;
	u16 h0_size;
	u16 scale_w;
	u16 scale_h;
	u16 x1_offset;
	u16 y1_offset;
	u16 w1_size;
	u16 h1_size;
	u16 x2_tg_offset;
	u16 y2_tg_offset;
	u16 w2_tg_size;
	u16 h2_tg_size;
} __packed;

struct fw_mtk_csi_param {
	u32 dphy_trail;
	u32 dphy_data_settle;
	u32 dphy_clk_settle;
	u32 cphy_settle;
	u8 legacy_phy;
	u8 not_fixed_trail_settle;
	u32 dphy_csi2_resync_dmy_cycle;
	u8 not_fixed_dphy_settle;
	u8 dphy_init_deskew_support;
	u8 dphy_periodic_deskew_support;
	u8 dphy_lrte_support;
	u8 cphy_lrte_support;
	u8 dphy_alp_support;
	u8 cphy_alp_support;
	u8 dphy_ulps_support;
	u8 cphy_ulps_support;
	u8 clk_lane_no_initial_flow;
	u8 initial_skew;
} __packed;

struct fw_u32_min_max {
	u32 min;
	u32 max;
} __packed;

struct fw_u64_min_max {
	u64 min;
	u64 max;
} __packed;

struct fw_mtk_mbus_frame_desc_entry_csi2 {
	u8 channel;
	u8 data_type;
	u8 enable;
	u8 dt_remap_to_type;
	u16 hsize;
	u16 vsize;
	u16 user_data_desc;
	u8 is_sensor_hw_pre_latch_exp;
	u32 cust_assign_to_tsrec_exp_id;
	u16 valid_bit;
	u8 is_active_line;
	u8 ebd_parsing_type;
	u32 fs_seq;
} __packed;

struct fw_dcg_info_struct {
	u32 dcg_mode;
	u32 dcg_gain_mode;
	u32 dcg_gain_base;
	u32 dcg_gain_ratio_min;
	u32 dcg_gain_ratio_max;
	u32 dcg_gain_ratio_step;
	u32 dcg_ratio_group[MAX_EXPOSURE_CNT];
	u32 dcg_gain_table_cnt;
	u32 *dcg_gain_table;
} __packed;

struct fw_mode_lut_static_info {
	u64 pclk;
	u32 linelength;
	u32 framelength;
	u32 readout_length;
	u32 read_margin;
	u32 framelength_step;
	u32 min_vblanking_line;
} __packed;

struct fw_multiexp_static_info {
	u8 belong_to_lut_id;
	u32 coarse_integ_step;
	u32 exposure_margin;
	u32 ae_binning_ratio;
	int fine_integ_line;
	u32 dig_gain_min;
	u32 dig_gain_max;
	u32 dig_gain_step;
} __packed;

struct fw_mode_info_dynamic_size {
	struct fw_set_pd_block_info_t *imgsensor_pd_info;
	struct fw_mtk_sensor_saturation_info *saturation_info;
	struct fw_dcg_info_struct *dcg_info;
	struct fw_mtk_sensor_ctle_param *ctle_param;
	struct fw_mtk_mbus_frame_desc_entry_csi2 *frame_desc;
	u16 *mode_setting_table;
	u16 *mode_setting_table_for_md;
	u16 *seamless_switch_mode_setting_table;
	char *cust_sensor_mode_data;
} __packed;

struct fw_mode_info {
	u32 mode_setting_len;
	u32 mode_setting_len_for_md;
	u32 seamless_switch_mode_setting_len;
	u32 seamless_switch_group;
	u32 hdr_mode;
	u32 raw_cnt;
	u32 exp_cnt;

	u64 pclk;
	u32 linelength;
	u32 framelength;
	u16 max_framerate;
	u32 mipi_pixel_rate;
	u32 readout_length;
	u32 read_margin;
	u32 framelength_step;
	u32 coarse_integ_step;
	u32 min_exposure_line;
	u32 min_vblanking_line;
	u32 exposure_margin;

	struct fw_winsize_info imgsensor_winsize_info;

	u8 pdaf_cap;
	u32 ae_binning_ratio;
	int fine_integ_line;
	u8 delay_frame;
	u32 frame_desc_num;

	struct fw_mtk_csi_param csi_param;

	u32 sensor_output_dataformat;
	u32 sensor_output_dataformat_cell_type;
	u32 rgbw_output_mode;
	u32 dig_gain_min;
	u32 dig_gain_max;
	u32 dig_gain_step;
	u8 dpc_enabled;
	u8 pdc_enabled;
	u8 awb_enabled;
	u8 has_imgsensor_pd_info;
	u8 has_saturation_info;
	u8 has_dcg_info;
	u8 has_ctle_param;
	struct fw_u32_min_max multi_exposure_ana_gain_range[MAX_EXPOSURE_CNT];
	struct fw_u64_min_max multi_exposure_shutter_range[MAX_EXPOSURE_CNT];
	struct fw_multiexp_static_info multiexp_s_info[MAX_EXPOSURE_CNT];
	struct fw_mode_lut_static_info mode_lut_s_info[MAX_LUT_CNT];

	u8 aov_mode;
	u8 rosc_mode;
	u8 s_dummy_support;
	u32 ae_ctrl_support;
	u32 exposure_order_in_lbmf;
	u32 exposure_order_in_dcg;
	u32 mode_type_in_lbmf;
	u32 sw_fl_delay;
	u8 support_mcss;
	u8 is_stick_hdr_vsync;
	u8 force_wr_mode_setting;

	u32 cust_sensor_mode_data_len;

	struct fw_mode_info_dynamic_size dynamic;
} __packed;

struct fw_modes {
	u32 mode_num;
	struct fw_mode_info *mode_list;
} __packed;

/* --- ebd section --- */

struct fw_ebd_loc {
	u16 loc_line;
	u16 loc_pix[MAX_EBD_PIXEL_OFFSET_NUM];
} __packed;

struct fw_ebd_info_struct {
	struct fw_ebd_loc frm_cnt_loc;
	struct fw_ebd_loc coarse_integ_loc[MAX_EXPOSURE_CNT];
	struct fw_ebd_loc ana_gain_loc[MAX_EXPOSURE_CNT];
	struct fw_ebd_loc dig_gain_loc[MAX_EXPOSURE_CNT];
	struct fw_ebd_loc coarse_integ_shift_loc;
	struct fw_ebd_loc dol_loc;
	struct fw_ebd_loc framelength_loc;
	struct fw_ebd_loc temperature_loc;
} __packed;

struct fw_ebd {
	u8 has_ebd_info;
	struct fw_ebd_info_struct *ebd_info;
} __packed;

#endif
