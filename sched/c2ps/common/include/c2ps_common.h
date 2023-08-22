// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef C2PS_COMMON_INCLUDE_C2PS_COMMON_H_
#define C2PS_COMMON_INCLUDE_C2PS_COMMON_H_

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/unistd.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/topology.h>
#include <linux/time.h>
#include <uapi/linux/sched/types.h>

#define MAX_WINDOW_SIZE 70
#define MAX_CPU_NUM CONFIG_MAX_NR_CPUS
// ms
#define BACKGROUND_MONITOR_DURATION 2000
#define NUMBER_OF_CLUSTER 3
#define LCORE_ID 3
#define MCORE_ID 6
#define BCORE_ID 7
#define BACKGROUND_UCLAMPMAX_ALERT 10
#define MAX_TASK_NAME_SIZE 10

extern int proc_time_window_size;
extern int debug_log_on;

struct c2ps_task_info {
	u32 task_id;
	pid_t pid;
	u32 loading;
	u32 nr_hist_info;
	u32 default_uclamp;
	u64 task_target_time;
	u64 start_time;
	u64 end_time;
	u64 proc_time;
	u64 sum_exec_runtime_start;
	u64 real_exec_runtime;
	u64 hist_proc_time_sum;
	u64 hist_window_size;
	u64 hist_proc_time_avg;
	u32 hist_loading[MAX_WINDOW_SIZE];
	u64 hist_proc_time[MAX_WINDOW_SIZE];
	char task_name[MAX_TASK_NAME_SIZE];
	struct hlist_node hlist;
	struct c2ps_task_info *overlap_task;
	struct list_head task_group_list;
	struct task_group_info *tsk_group;
	struct mutex mlock;
	bool is_dynamic_tid;
	bool is_vip_task;
	bool is_active;
	bool is_scene_changed;

	/**
	* update by uclamp regulator
	*/
	unsigned int latest_uclamp;
};

struct task_group_info {
	int group_head;
	u64 group_start_time;
	u64 group_target_time;
	u64 accumulate_time;
	struct hlist_node hlist;
	struct mutex mlock;
};

struct per_cpu_idle_rate {
	unsigned int idle;
	u64 wall_time;
	u64 idle_time;
};

struct global_info {
	int cfg_camfps;
	int max_uclamp[NUMBER_OF_CLUSTER];
	// int max_uclamp_cluster1;
	// int max_uclamp_cluster2;
	int camfps;
	u64 vsync_time;
	struct per_cpu_idle_rate cpu_idle_rates[MAX_CPU_NUM];
	/**
	 * need_update_uclamp definition:
	 * [if any needs update,
	 *  LCore needs update, MCore needs update, LCore needs update]
	 *
	 *  set 1: need to increase uclamp
	 *  set 0: no need to modify uclamp
	 *  set -1: be able to decrease uclamp
	 */
	int need_update_uclamp[1 + NUMBER_OF_CLUSTER];
	int curr_max_uclamp[NUMBER_OF_CLUSTER];
	struct mutex mlock;
};

struct regulator_req {
	struct c2ps_task_info *tsk_info;
	struct global_info *glb_info;
	struct list_head queue_list;
	bool is_flush;
};


#define C2PS_LOGD(fmt, ...)                                         \
	do {                                                            \
		if (debug_log_on)                                           \
			pr_debug("[C2PS]: %s " fmt, __func__, ##__VA_ARGS__);   \
	} while (0)

#define C2PS_LOGW(fmt, ...)                                         \
	do {                                                            \
		if (debug_log_on)                                           \
			pr_warn("[C2PS]: %s " fmt, __func__, ##__VA_ARGS__);    \
	} while (0)

#define C2PS_LOGW_ONCE(fmt, ...) pr_warn_once("[C2PS]: %s %s %d " fmt, \
	__FILE__, __func__, __LINE__, ##__VA_ARGS__)

#define C2PS_LOGE(fmt, ...) pr_err("[C2PS]: %s %s %d " fmt, \
	__FILE__, __func__, __LINE__, ##__VA_ARGS__)

int init_c2ps_common(void);
void exit_c2ps_common(void);
int set_curr_uclamp_hint(int pid, int set);
int set_curr_uclamp_hint_wo_lock(struct task_struct *p, int set);
struct c2ps_task_info *c2ps_find_task_info_by_tskid(int task_id);
int c2ps_add_task_info(struct c2ps_task_info *tsk_info);
void c2ps_clear_task_info_table(void);
u64 c2ps_task_sched_runtime(struct task_struct *p);
u64 c2ps_get_sum_exec_runtime(int pid);
void c2ps_task_info_tbl_lock(const char *tag);
void c2ps_task_info_tbl_unlock(const char *tag);
struct task_group_info *c2ps_find_task_group_info_by_grphd(int group_head);
int c2ps_create_task_group(int group_head, u64 task_group_target_time);
int c2ps_add_task_to_group(struct c2ps_task_info *tsk_info, int group_head);
void c2ps_clear_task_group_info_table(void);
void c2ps_task_group_info_tbl_lock(const char *tag);
void c2ps_task_group_info_tbl_unlock(const char *tag);
void c2ps_info_lock(struct mutex *mlock);
void c2ps_info_unlock(struct mutex *mlock);
u64 c2ps_get_time(void);
void c2ps_update_task_info_hist(struct c2ps_task_info *tsk_info);
struct global_info *get_glb_info(void);
void set_config_camfps(int camfps);
void update_vsync_time(u64 ts);
void update_camfps(int camfps);
bool is_group_head(struct c2ps_task_info *tsk_info);
void c2ps_systrace_c(pid_t pid, int val, const char *fmt, ...);
void c2ps_bg_info_systrace(const char *fmt, ...);
void c2ps_main_systrace(const char *fmt, ...);
void c2ps_critical_task_systrace(struct c2ps_task_info *tsk_info);
void *c2ps_alloc_atomic(int i32Size);
void c2ps_free(void *pvBuf, int i32Size);
void set_glb_info_bg_uclamp_max(void);
void update_cpu_idle_rate(void);
bool need_update_background(void);
void reset_need_update_status(void);
unsigned long c2ps_get_uclamp_freq(int cpu,  unsigned int uclamp);

extern void set_curr_uclamp_ctrl(int val);
extern void set_gear_uclamp_ctrl(int val);
extern void set_gear_uclamp_max(int gearid, int val);
extern int get_gear_uclamp_max(int gearid);
extern unsigned long pd_get_util_freq(int cpu, unsigned long util);
extern unsigned int get_nr_gears(void);
extern void set_wl_type_manual(int val);
extern int get_nr_wl_type(void);
extern void set_rt_aggre_preempt(int val);
extern unsigned int get_adaptive_margin(int cpu);

#endif  // C2PS_COMMON_INCLUDE_C2PS_COMMON_H_
