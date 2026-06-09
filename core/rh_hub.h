#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct rh_hub_proxy {
    void *proxy_addr;
    bool enabled;
    struct rh_hub_proxy *next;
} rh_hub_proxy_t;

typedef struct rh_hub {
    uintptr_t target_addr;
    rh_hub_proxy_t *proxies;
    size_t proxy_count;
    void *trampoline;
    void *orig_addr;
    void *enter_bridge;
    void *leave_bridge;
} rh_hub_t;

int  rh_hub_create(rh_hub_t *hub, uintptr_t target_addr, void *proxy_addr);
void rh_hub_destroy(rh_hub_t *hub);
int  rh_hub_add(rh_hub_t *hub, void *proxy_addr);
int  rh_hub_remove(rh_hub_t *hub, void *proxy_addr);
size_t rh_hub_count(rh_hub_t *hub);
void *rh_hub_get_prev(rh_hub_t *hub, void *current_proxy);
void rh_hub_push(rh_hub_t *hub, void *proxy_addr);
void rh_hub_pop(void);
bool rh_hub_is_recursive(rh_hub_t *hub);
void rh_hub_set_reentrant(rh_hub_t *hub, bool allow);
