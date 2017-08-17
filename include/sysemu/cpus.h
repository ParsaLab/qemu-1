#ifndef QEMU_CPUS_H
#define QEMU_CPUS_H

#include "qemu/timer.h"

/* cpus.c */
bool qemu_in_vcpu_thread(void);
void qemu_init_cpu_loop(void);
void resume_all_vcpus(void);
void pause_all_vcpus(void);
void cpu_stop_current(void);
void cpu_ticks_init(void);

#ifdef CONFIG_QUANTUM
void cpu_get_quantum(const char* val);
void cpu_set_quantum(const char* val);
void cpu_get_ic(const char *str);
void cpu_zero_all(void);
void configure_quantum(QemuOpts *opts, Error **errp);
void processForOpts(int64_t *val, const char* qopt, Error **errp);
void processLetterforExponent(int64_t *val, char c, Error **errp);
#endif

#ifdef CONFIG_MULTINODE
void configure_multinode(QemuOpts *opts, Error **errp);

typedef struct multinode
{
    const char* m_hostaddress;
    unsigned m_hostport;

    const char* m_clientaddress;
    unsigned m_clientport;

    bool m_bindclient;

}multinode;
#endif
void configure_icount(QemuOpts *opts, Error **errp);
extern int use_icount;
extern int icount_align_option;

/* drift information for info jit command */
extern int64_t max_delay;
extern int64_t max_advance;
void dump_drift_info(FILE *f, fprintf_function cpu_fprintf);

/* Unblock cpu */
void qemu_cpu_kick_self(void);
void qemu_timer_notify_cb(void *opaque, QEMUClockType type);

void cpu_synchronize_all_states(void);
void cpu_synchronize_all_post_reset(void);
void cpu_synchronize_all_post_init(void);

void qtest_clock_warp(int64_t dest);

#ifndef CONFIG_USER_ONLY
/* vl.c */
/* *-user doesn't have configurable SMP topology */
extern int smp_cores;
extern int smp_threads;
#endif

void list_cpus(FILE *f, fprintf_function cpu_fprintf, const char *optarg);

void qemu_tcg_configure(QemuOpts *opts, Error **errp);

#endif
