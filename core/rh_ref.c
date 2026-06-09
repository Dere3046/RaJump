#include "rh_ref.h"
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/sysinfo.h>

#define RH_REF_DELAY_MS 100

static pthread_mutex_t rh_ref_queue_lock = PTHREAD_MUTEX_INITIALIZER;
static rh_ref_t *rh_ref_queue_head = NULL;
static rh_ref_t *rh_ref_queue_tail = NULL;

static uint32_t rh_ref_get_timestamp(void) {
    struct sysinfo info;
    sysinfo(&info);
    return (uint32_t)(info.uptime * 1000);
}

void rh_ref_init(rh_ref_t *ref, void (*destroy)(void *)) {
    pthread_mutex_init(&ref->lock, NULL);
    atomic_init(&ref->count, 0);
    ref->destroy_ts = 0;
    ref->destroy = destroy;
    ref->ptr = NULL;
    ref->next = NULL;
}

void *rh_ref_get(rh_ref_t *ref) {
    return ref->ptr;
}

void rh_ref_hold(rh_ref_t *ref) {
    int old = atomic_fetch_add(&ref->count, 1);
    if (old == INT_MAX) abort();
}

bool rh_ref_is_destroyed(rh_ref_t *ref) {
    return ref->destroy_ts != 0;
}

static int rh_ref_count(rh_ref_t *ref) {
    return atomic_load(&ref->count);
}

void rh_ref_put(rh_ref_t *ref) {
    int old = atomic_fetch_sub(&ref->count, 1);
    if (old <= 0) abort();

    ref->destroy_ts = rh_ref_get_timestamp();

    pthread_mutex_lock(&rh_ref_queue_lock);

    if (rh_ref_queue_tail) {
        rh_ref_queue_tail->next = ref;
    } else {
        rh_ref_queue_head = ref;
    }
    rh_ref_queue_tail = ref;
    ref->next = NULL;

    uint32_t now = rh_ref_get_timestamp();
    rh_ref_t *prev = NULL;
    rh_ref_t *cur = rh_ref_queue_head;

    while (cur) {
        if (now > cur->destroy_ts && (now - cur->destroy_ts) > RH_REF_DELAY_MS &&
            rh_ref_count(cur) == 0) {
            rh_ref_t *rm = cur;
            if (prev) {
                prev->next = cur->next;
            } else {
                rh_ref_queue_head = cur->next;
            }
            if (rh_ref_queue_tail == cur) {
                rh_ref_queue_tail = prev;
            }
            cur = cur->next;

            pthread_mutex_unlock(&rh_ref_queue_lock);
            if (rm->destroy) {
                rm->destroy(rm);
            }
            pthread_mutex_lock(&rh_ref_queue_lock);
        } else {
            prev = cur;
            cur = cur->next;
        }
    }

    pthread_mutex_unlock(&rh_ref_queue_lock);
}
