#include "qflex/qflex.h"
#include "../libqflex/api.h"


bool qflex_inst_done = false;
bool qflex_prologue_done = false;
uint64_t qflex_prologue_pc = 0xDEADBEEF;
bool qflex_control_with_flexus = false;
bool qflex_trace_enabled = false;

#ifdef CONFIG_FLEXUS

void qflex_api_values_init(CPUState *cpu) {
    qflex_inst_done = false;
    qflex_prologue_done = false;
    qflex_prologue_pc = cpu_get_program_counter(cpu);
}

int qflex_prologue(CPUState *cpu) {
    int ret = 0;
    qflex_api_values_init(cpu);
    qflex_log_mask(QFLEX_LOG_GENERAL, "QFLEX: PROLOGUE START:%08lx\n"
                   "    -> Skips initial snapshot load long interrupt routine to normal user program\n", cpu_get_program_counter(cpu));
    while(!qflex_is_prologue_done()) {
        ret = qflex_cpu_step(cpu, PROLOGUE);
    }
    qflex_log_mask(QFLEX_LOG_GENERAL, "QFLEX: PROLOGUE END  :%08lx\n", cpu_get_program_counter(cpu));
    return ret;
}

int qflex_singlestep(CPUState *cpu) {
    int ret = 0;
    while(!qflex_is_inst_done()) {
        ret = qflex_cpu_step(cpu, SINGLESTEP);
    }
    qflex_update_inst_done(false);
    return ret;
}

int advance_qemu(void * obj){
    CPUState *cpu = obj;
    return qflex_singlestep(cpu);
}

#endif // CONFIG_FLEXUS

