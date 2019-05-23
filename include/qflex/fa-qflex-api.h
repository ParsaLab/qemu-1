#ifndef FA_QFLEX_API_H
#define FA_QFLEX_API_H

#include <stdbool.h>
#include "qflex/qflex-log.h"

extern bool fa_qflex_user_mode;
extern bool fa_qflex_running;
extern bool qflex_inst_done;

void qflex_api_values_init(void);

static inline bool qflex_is_inst_done(void)    { return qflex_inst_done; }
static inline bool fa_qflex_is_user_mode(void) { return fa_qflex_user_mode; }
static inline bool fa_qflex_is_running(void)   { return fa_qflex_running;   }

static inline void qflex_update_inst_done(bool done) { qflex_inst_done = done; }
static inline void fa_qflex_update_excp_in(int old_el, int new_el) {
    qflex_log_mask(QFLEX_LOG_INT, "EXCP IN  from EL%d to EL%d\n", old_el, new_el);
    fa_qflex_user_mode = false;
    assert(new_el);
}
static inline void fa_qflex_update_excp_out(int cur_el) {
    fa_qflex_user_mode = (cur_el == 0);
    qflex_log_mask(QFLEX_LOG_INT, "EXCP OUT from EL? to EL%d\n", cur_el);
}

#endif /* FA_QFLEX_API_H */
