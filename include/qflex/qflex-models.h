#ifdef  QFLEX_MODELS_H
#define QFLEX_MODELS_H

#include <stdbool.h>

void qflex_models_init(void);

uint64_t qflex_cache_model_start(void);
uint64_t qflex_cache_model_stop(void);

void qflex_model_memaccess(uint64_t addr, bool isData);
bool qflex_model_is_running(void);

void qflex_cache_log_stats(void);

#endif /* QFLEX_MODELS_H */
