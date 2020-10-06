#include "qflex/qflex-models.h"

typedef struct cache_model_t {
    size_t block_size; // argument in bit width
    size_t nb_entries;
    size_t associativity;
    size_t curr_time;
    size_t misses;
    uint64_t* tag;
    size_t* time;
} cache_model_t;

static cache_model_t qflexICache;
static cache_model_t qflexDCache;
static cache_model_t qflexITLB;
static cache_model_t qflexDTLB;
static cache_model_t qflexTLB;
static bool is_running = false;

static inline size_t get_cache_tag(cache_model* cache, uint64_t addr) {
    return addr >> cache-block_size;
}

static inline size_t get_cache_slot(cache_model* cache, uint64_t addr) {
    uint64_t bits = addr >> cache->block_size;
    size_t slot = bits % (tlb.nb_entries/tlb.associativity);
    return slot;
}

static void cache_init(cache_model_t* cache, 
        size_t nb_entries, 
        size_t block_size, 
        size_t associativity) {
    cache->block_size = block_size;
    cache->nb_entries = nb_entries;
    cache->associativity = associativity;
    cache->curr_time = 0;
    cache->misses = 0;
    cache->tag  = calloc(cache->nb_entries, sizeof(size_t));
    cache->time = calloc(cache->nb_entries, sizeof(size_t));
    assert(cache->tag);
    assert(cache->time);
}

static void cache_close(cache_model_t* cache) {
    free(cache->tag);
    free(cache->time);
}

static inline size_t cache_get_lru(cache_model_t* cache, uint64_t slot) {
    size_t lru = slot;
    size_t time = cache->time[slot];
    for(size_t i = 0; i < cache->associativity; i++) {
        if(time < cache->time[slot+i]) {
            lru = slot+i;
            time = cache->time[slot+i];
        }

    }
    return lru;
}

static inline bool cache_access(cache_model_t* cache, uint64_t addr) {

    bool is_hit = false;
    size_t tag = get_cache_tag(cache, addr);
    size_t slot = get_cache_slot(cache, addr);
    size_t idx = 0;

    for(size_t i = 0; i < cache->associativity; i++) {
        idx = slot + i;
        if(cache->tag[idx] == tag) {
            is_hit = true;
            break;
        }
    }

    if(!is_hit) {
        idx = cache_get_lru(cache, slot);
        cache->tag[idx] = tag;
        cache->misses++;
    }
    cache->time[idx] = cache->curr_time;
    cache->curr_time++;
}

void qflex_model_memaccess(uint64_t addr, bool isData) {
    cache_model_t* tlb = isData ? &qflexDTLB : &qflexITLB;
    cache_model_t* cache = isData ? &qflexDCache : &qflexICache;
    bool is_hit = false;

    is_hit = cache_access(tlb, addr);
    if(!is_hit) {
        is_hit = cache_access(&qflexTLB, addr);
    }
    is_hit = cache_access(cache, addr);

    return;
}



void qflex_models_init() { return; }

uint64_t qflex_cache_model_start(void) { 
    cache_init(&qflexICache, 4096, 9, 4);
    cache_init(&qflexDCache, 4096, 9, 4);
    cache_init(&qflexITLB, 64, 12, 64);
    cache_init(&qflexDTLB, 64, 12, 64);
    cache_init(&qflexTLB, 4024, 12, 4);
    is_running = true; 
}

uint64_t qflex_cache_model_stop(void) { 
    cache_close(&qflexICache);
    cache_close(&qflexDCache);
    cache_close(&qflexITLB);
    cache_close(&qflexDTLB);
    cache_close(&qflexTLB);
    is_running = false; 
}


bool qflex_is_running(void) { return is_running; }
