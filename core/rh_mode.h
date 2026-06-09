/*
 * rh_mode.h — hook mode definitions and proxy macros
 *
 * Copyright (C) 2026 dere3046
 */

#pragma once

#include <stdint.h>

#include "rh_hub.h"

typedef enum {
    RH_MODE_SHARED = 0,
    RH_MODE_MULTI = 1,
    RH_MODE_UNIQUE = 2,
} rh_mode_t;

#define RH_CALL_PREV(proxy_func, ...) \
    ((__typeof__(proxy_func))rh_hub_get_prev(NULL, (void *)(proxy_func)))(__VA_ARGS__)

#define RH_POP_STACK() \
    rh_hub_pop()

#define RH_ALLOW_REENTRANT() \
    rh_hub_set_reentrant(NULL, true)

#define RH_DISALLOW_REENTRANT() \
    rh_hub_set_reentrant(NULL, false)

#ifdef __cplusplus
struct RhStackScope {
    ~RhStackScope() { rh_hub_pop(); }
};
#define RH_STACK_SCOPE() RhStackScope __rh_scope
#else
#define RH_STACK_SCOPE() ((void)0)
#endif
