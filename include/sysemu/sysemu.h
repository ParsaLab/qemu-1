//  DO-NOT-REMOVE begin-copyright-block
// QFlex consists of several software components that are governed by various
// licensing terms, in addition to software that was developed internally.
// Anyone interested in using QFlex needs to fully understand and abide by the
// licenses governing all the software components.
// 
// ### Software developed externally (not by the QFlex group)
// 
//     * [NS-3] (https://www.gnu.org/copyleft/gpl.html)
//     * [QEMU] (http://wiki.qemu.org/License)
//     * [SimFlex] (http://parsa.epfl.ch/simflex/)
//     * [GNU PTH] (https://www.gnu.org/software/pth/)
// 
// ### Software developed internally (by the QFlex group)
// **QFlex License**
// 
// QFlex
// Copyright (c) 2020, Parallel Systems Architecture Lab, EPFL
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright notice,
//       this list of conditions and the following disclaimer in the documentation
//       and/or other materials provided with the distribution.
//     * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
//       nor the names of its contributors may be used to endorse or promote
//       products derived from this software without specific prior written
//       permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
// EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//  DO-NOT-REMOVE end-copyright-block
#ifndef SYSEMU_H
#define SYSEMU_H
/* Misc. things related to the system emulator.  */

#include "qemu/option.h"
#include "qemu/queue.h"
#include "qemu/timer.h"
#include "qapi-types.h"
#include "qemu/notify.h"
#include "qemu/main-loop.h"
#include "qemu/bitmap.h"
#include "qemu/uuid.h"
#include "qom/object.h"


#ifdef CONFIG_FLEXUS
#include "../libqflex/api.h"
#endif

/* vl.c */

extern const char *bios_name;
extern const char *qemu_name;
extern QemuUUID qemu_uuid;
extern bool qemu_uuid_set;

bool runstate_check(RunState state);
void runstate_set(RunState new_state);
int runstate_is_running(void);
bool runstate_needs_reset(void);
bool runstate_store(char *str, size_t size);
typedef struct vm_change_state_entry VMChangeStateEntry;
typedef void VMChangeStateHandler(void *opaque, int running, RunState state);

VMChangeStateEntry *qemu_add_vm_change_state_handler(VMChangeStateHandler *cb,
                                                     void *opaque);
void qemu_del_vm_change_state_handler(VMChangeStateEntry *e);
void vm_state_notify(int running, RunState state);

/* Enumeration of various causes for shutdown. */
typedef enum ShutdownCause {
    SHUTDOWN_CAUSE_NONE,          /* No shutdown request pending */
    SHUTDOWN_CAUSE_HOST_ERROR,    /* An error prevents further use of guest */
    SHUTDOWN_CAUSE_HOST_QMP,      /* Reaction to a QMP command, like 'quit' */
    SHUTDOWN_CAUSE_HOST_SIGNAL,   /* Reaction to a signal, such as SIGINT */
    SHUTDOWN_CAUSE_HOST_UI,       /* Reaction to UI event, like window close */
    SHUTDOWN_CAUSE_GUEST_SHUTDOWN,/* Guest shutdown/suspend request, via
                                     ACPI or other hardware-specific means */
    SHUTDOWN_CAUSE_GUEST_RESET,   /* Guest reset request, and command line
                                     turns that into a shutdown */
    SHUTDOWN_CAUSE_GUEST_PANIC,   /* Guest panicked, and command line turns
                                     that into a shutdown */
    SHUTDOWN_CAUSE__MAX,
} ShutdownCause;

static inline bool shutdown_caused_by_guest(ShutdownCause cause)
{
    return cause >= SHUTDOWN_CAUSE_GUEST_SHUTDOWN;
}

void vm_start(void);
int vm_prepare_start(void);
int vm_stop(RunState state);
int vm_stop_force_state(RunState state);

typedef enum WakeupReason {
    /* Always keep QEMU_WAKEUP_REASON_NONE = 0 */
    QEMU_WAKEUP_REASON_NONE = 0,
    QEMU_WAKEUP_REASON_RTC,
    QEMU_WAKEUP_REASON_PMTIMER,
    QEMU_WAKEUP_REASON_OTHER,
} WakeupReason;

void qemu_system_reset_request(ShutdownCause reason);
void qemu_system_suspend_request(void);
void qemu_register_suspend_notifier(Notifier *notifier);
void qemu_system_wakeup_request(WakeupReason reason);
void qemu_system_wakeup_enable(WakeupReason reason, bool enabled);
void qemu_register_wakeup_notifier(Notifier *notifier);
void qemu_system_shutdown_request(ShutdownCause reason);
void qemu_system_powerdown_request(void);
void qemu_register_powerdown_notifier(Notifier *notifier);
void qemu_system_debug_request(void);
void qemu_system_vmstop_request(RunState reason);
void qemu_system_vmstop_request_prepare(void);
bool qemu_vmstop_requested(RunState *r);
ShutdownCause qemu_shutdown_requested_get(void);
ShutdownCause qemu_reset_requested_get(void);
void qemu_system_killed(int signal, pid_t pid);
void qemu_system_reset(ShutdownCause reason);
void qemu_system_guest_panicked(GuestPanicInformation *info);

void qemu_add_exit_notifier(Notifier *notify);
void qemu_remove_exit_notifier(Notifier *notify);

void qemu_add_machine_init_done_notifier(Notifier *notify);
void qemu_remove_machine_init_done_notifier(Notifier *notify);
#ifdef CONFIG_EXTSNAP
int save_vmstate_ext(Monitor *mon, const char *name);
int save_vmstate_ext_test(Monitor *mon, const char *name);
int incremental_load_vmstate_ext(const char *name, Monitor* mon);
int create_tmp_overlay(void);
int delete_tmp_overlay(void);

#endif

#ifdef CONFIG_FLEXUS
void set_flexus_load_dir(const char* dir_name);
void configure_flexus(QemuOpts *opts, Error **errp);
const char* flexus_simulation_status(void);
bool hasSimulator(void);
void quitFlexus(void);
void prepareFlexus(void);
void initFlexus(void);
void startFlexus(void);
bool flexus_in_simulation(void);
void flexus_qmp(qmp_flexus_cmd_t cmd, const char* args, Error **errp);
void flexus_addDebugCfg(const char *filename, Error **errp);
void flexus_setBreakCPU(const char * value, Error **errp);
void flexus_backupStats(const char *filename, Error **errp);
void flexus_disableCategory(const char *component, Error **errp);
void flexus_disableComponent(const char *component, const char *index, Error **errp);
void flexus_enableCategory(const char *component, Error **errp);
void flexus_enableComponent(const char *component, const char *index, Error **errp);
void flexus_enterFastMode(Error **errp);
void flexus_leaveFastMode(Error **errp);
void flexus_listCategories(Error **errp);
void flexus_listComponents(Error **errp);
void flexus_listMeasurements(Error **errp);
void flexus_log(const char *name, const char *interval, const char *regex, Error **errp);
void flexus_parseConfiguration(const char *filename, Error **errp);
void flexus_printConfiguration(Error **errp);
void flexus_printCycleCount(Error **errp);
void flexus_printDebugConfiguration(Error **errp);
void flexus_printMMU(const char * cpu, Error **errp);
void flexus_printMeasurement(const char *measurement, Error **errp);
void flexus_printProfile(Error **errp);
void flexus_quiesce(Error **errp);
void flexus_reloadDebugCfg(Error **errp);
void flexus_resetProfile(Error **errp);
void flexus_saveStats(const char *filename, Error **errp);
void flexus_setBreakInsn(const char *value, Error **errp);
void flexus_setConfiguration(const char *name, const char *value, Error **errp);
void flexus_setDebug(const char *debugseverity, Error **errp);
void flexus_setProfileInterval(const char *value, Error **errp);
void flexus_setRegionInterval(const char *value, Error **errp);
void flexus_setStatInterval(const char *value, Error **errp);
void flexus_setStopCycle(const char *value, Error **errp);
void flexus_setTimestampInterval(const char *value, Error **errp);
void flexus_status(Error **errp);
void flexus_terminateSimulation(Error **errp);
void flexus_writeConfiguration(const char *filename, Error **errp);
void flexus_writeDebugConfiguration(Error **errp);
void flexus_writeMeasurement(const char *measurement, const char *filename, Error **errp);
void flexus_writeProfile(const char *filename, Error **errp);
int flexus_in_timing(void);
int flexus_in_trace(void);
void flexus_doSave(const char* dir_name, Error **errp);
void flexus_doLoad(const char* dir_name, Error **errp);
#endif

#ifdef CONFIG_EXTSNAP
void configure_phases(QemuOpts *opts, Error **errp);
void configure_ckpt(QemuOpts *opts, Error **errp);
uint64_t get_phase_value(void);
bool is_phases_enabled(void);
bool is_ckpt_enabled(void);
void toggle_phases_creation(void);
void toggle_ckpt_creation(void);
bool phase_is_valid(void);
void save_phase(void);
void save_ckpt(void);
void pop_phase(void);
bool save_request_pending(void);
bool cont_request_pending(void);
bool quit_request_pending(void);
void request_cont(void);
void request_quit(void);
void toggle_cont_request(void);
void toggle_save_request(void);
void set_base_ckpt_name(const char* str);
const char* get_ckpt_name(void);
uint64_t get_ckpt_interval(void);
uint64_t get_ckpt_end(void);
bool can_quit(void);
void toggle_can_quit(void);
#endif

#ifdef CONFIG_QUANTUM
bool query_quantum_pause_state(void);
void quantum_pause(void);
void quantum_unpause(void);
uint64_t* increment_total_num_instr(void);
uint64_t query_total_num_instr(void);
void set_total_num_instr(uint64_t val);
uint64_t query_quantum_core_value(void);
uint64_t query_quantum_record_value(void);
uint64_t query_quantum_step_value(void);
uint64_t query_quantum_node_value(void);
const char* query_quantum_file_value(void);
void set_quantum_value(uint64_t val);
void set_quantum_record_value(uint64_t val);
void set_quantum_node_value(uint64_t val);
void cpu_dbg(DbgDataAll *info);
void cpu_zero_all(void);
void configure_quantum(QemuOpts *opts, Error **errp);
#endif

#if defined(CONFIG_QUANTUM) || defined(CONFIG_FLEXUS) || defined(CONFIG_EXTSNAP)
 void processForOpts(uint64_t *val, const char* qopt, Error **errp);
void processLetterforExponent(uint64_t *val, char c, Error **errp);
#endif


void qemu_announce_self(void);

extern int autostart;

typedef enum {
    VGA_NONE, VGA_STD, VGA_CIRRUS, VGA_VMWARE, VGA_XENFB, VGA_QXL,
    VGA_TCX, VGA_CG3, VGA_DEVICE, VGA_VIRTIO,
    VGA_TYPE_MAX,
} VGAInterfaceType;

extern int vga_interface_type;
#define xenfb_enabled (vga_interface_type == VGA_XENFB)

extern int graphic_width;
extern int graphic_height;
extern int graphic_depth;
extern int display_opengl;
extern const char *keyboard_layout;
extern int win2k_install_hack;
extern int alt_grab;
extern int ctrl_grab;
extern int smp_cpus;
extern int max_cpus;
extern int cursor_hide;
extern int graphic_rotate;
extern int no_quit;
extern int no_shutdown;
extern int old_param;
extern int boot_menu;
extern bool boot_strict;
extern uint8_t *boot_splash_filedata;
extern size_t boot_splash_filedata_size;
extern bool enable_mlock;
extern uint8_t qemu_extra_params_fw[2];
extern QEMUClockType rtc_clock;
extern const char *mem_path;
extern int mem_prealloc;

#define MAX_NODES 128
#define NUMA_NODE_UNASSIGNED MAX_NODES
#define NUMA_DISTANCE_MIN         10
#define NUMA_DISTANCE_DEFAULT     20
#define NUMA_DISTANCE_MAX         254
#define NUMA_DISTANCE_UNREACHABLE 255

#define MAX_OPTION_ROMS 16
typedef struct QEMUOptionRom {
    const char *name;
    int32_t bootindex;
} QEMUOptionRom;
extern QEMUOptionRom option_rom[MAX_OPTION_ROMS];
extern int nb_option_roms;

#define MAX_PROM_ENVS 128
extern const char *prom_envs[MAX_PROM_ENVS];
extern unsigned int nb_prom_envs;

/* generic hotplug */
void hmp_drive_add(Monitor *mon, const QDict *qdict);

/* pcie aer error injection */
void hmp_pcie_aer_inject_error(Monitor *mon, const QDict *qdict);

/* serial ports */

#define MAX_SERIAL_PORTS 4

extern Chardev *serial_hds[MAX_SERIAL_PORTS];

/* parallel ports */

#define MAX_PARALLEL_PORTS 3

extern Chardev *parallel_hds[MAX_PARALLEL_PORTS];

void hmp_usb_add(Monitor *mon, const QDict *qdict);
void hmp_usb_del(Monitor *mon, const QDict *qdict);
void hmp_info_usb(Monitor *mon, const QDict *qdict);

void add_boot_device_path(int32_t bootindex, DeviceState *dev,
                          const char *suffix);
char *get_boot_devices_list(size_t *size, bool ignore_suffixes);

DeviceState *get_boot_device(uint32_t position);
void check_boot_index(int32_t bootindex, Error **errp);
void del_boot_device_path(DeviceState *dev, const char *suffix);
void device_add_bootindex_property(Object *obj, int32_t *bootindex,
                                   const char *name, const char *suffix,
                                   DeviceState *dev, Error **errp);
void restore_boot_order(void *opaque);
void validate_bootdevices(const char *devices, Error **errp);

/* handler to set the boot_device order for a specific type of MachineClass */
typedef void QEMUBootSetHandler(void *opaque, const char *boot_order,
                                Error **errp);
void qemu_register_boot_set(QEMUBootSetHandler *func, void *opaque);
void qemu_boot_set(const char *boot_order, Error **errp);

QemuOpts *qemu_get_machine_opts(void);

bool defaults_enabled(void);

extern QemuOptsList qemu_legacy_drive_opts;
extern QemuOptsList qemu_common_drive_opts;
extern QemuOptsList qemu_drive_opts;
extern QemuOptsList bdrv_runtime_opts;
extern QemuOptsList qemu_chardev_opts;
extern QemuOptsList qemu_device_opts;
extern QemuOptsList qemu_netdev_opts;
extern QemuOptsList qemu_net_opts;
extern QemuOptsList qemu_global_opts;
extern QemuOptsList qemu_mon_opts;

#endif
