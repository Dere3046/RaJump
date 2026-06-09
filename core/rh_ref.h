#pragma once
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct rh_ref {
    pthread_mutex_t lock;
    atomic_int count;
    uint32_t destroy_ts;
    void (*destroy)(void *);
    void *ptr;
    struct rh_ref *next;
} rh_ref_t;

void rh_ref_init(rh_ref_t *ref, void (*destroy)(void *));
void *rh_ref_get(rh_ref_t *ref);
void rh_ref_hold(rh_ref_t *ref);
void rh_ref_put(rh_ref_t *ref);
bool rh_ref_is_destroyed(rh_ref_t *ref);
