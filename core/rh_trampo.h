#pragma once
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "rh_ref.h"

#define RH_TRAMPO_ALIGN 64

typedef struct rh_trampo_page {
    uintptr_t ptr;
    size_t size;
    size_t used;
    size_t trampo_count;
    uint32_t *flags;
    struct rh_trampo_page *next;
} rh_trampo_page_t;

typedef struct {
    rh_trampo_page_t *pages;
    pthread_mutex_t lock;
    size_t default_page_size;
} rh_trampo_t;

int rh_trampo_init(rh_trampo_t *mgr, size_t default_page_size);
uintptr_t rh_trampo_alloc(rh_trampo_t *mgr, size_t size);
uintptr_t rh_trampo_alloc_near(rh_trampo_t *mgr, size_t size, uintptr_t range_low, uintptr_t range_high);
uintptr_t rh_trampo_alloc_near_3tier(rh_trampo_t *mgr, size_t size, uintptr_t range_low, uintptr_t range_high);
void rh_trampo_free(rh_trampo_t *mgr, uintptr_t addr, size_t size);
rh_trampo_t *rh_trampo_get_global(void);
