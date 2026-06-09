#include "rh_hub.h"

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define RH_HUB_STACK_MAX 16

typedef struct {
    rh_hub_t *hub;
    void *proxy_addr;
    bool allow_reentrant;
} rh_hub_stack_frame_t;

static _Thread_local rh_hub_stack_frame_t rh_hub_stack[RH_HUB_STACK_MAX];
static _Thread_local int rh_hub_stack_depth = 0;

int rh_hub_create(rh_hub_t *hub, uintptr_t target_addr, void *proxy_addr)
{
    if (!hub) return -1;
    memset(hub, 0, sizeof(*hub));
    hub->target_addr = target_addr;
    hub->orig_addr = (void *)target_addr;

    if (proxy_addr) {
        rh_hub_proxy_t *p = calloc(1, sizeof(*p));
        if (!p) return -1;
        p->proxy_addr = proxy_addr;
        p->enabled = true;
        p->next = NULL;
        hub->proxies = p;
        hub->proxy_count = 1;
    }

    hub->trampoline = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (hub->trampoline == MAP_FAILED) {
        free(hub->proxies);
        return -1;
    }

    extern void *g_bridge_enter;
    extern void *g_bridge_leave;
    if (g_bridge_enter) {
        hub->enter_bridge = g_bridge_enter;
        hub->leave_bridge = g_bridge_leave;
    }

    return 0;
}

void rh_hub_destroy(rh_hub_t *hub)
{
    if (!hub) return;
    rh_hub_proxy_t *p = hub->proxies;
    while (p) {
        rh_hub_proxy_t *next = p->next;
        free(p);
        p = next;
    }
    if (hub->trampoline)
        munmap(hub->trampoline, 4096);
    memset(hub, 0, sizeof(*hub));
}

int rh_hub_add(rh_hub_t *hub, void *proxy_addr)
{
    if (!hub || !proxy_addr) return -1;
    rh_hub_proxy_t *p = calloc(1, sizeof(*p));
    if (!p) return -1;
    p->proxy_addr = proxy_addr;
    p->enabled = true;
    p->next = hub->proxies;
    hub->proxies = p;
    hub->proxy_count++;
    return 0;
}

int rh_hub_remove(rh_hub_t *hub, void *proxy_addr)
{
    if (!hub || !proxy_addr) return -1;
    rh_hub_proxy_t **prev = &hub->proxies;
    while (*prev) {
        if ((*prev)->proxy_addr == proxy_addr) {
            rh_hub_proxy_t *victim = *prev;
            *prev = victim->next;
            free(victim);
            hub->proxy_count--;
            return 0;
        }
        prev = &(*prev)->next;
    }
    return -1;
}

size_t rh_hub_count(rh_hub_t *hub)
{
    return hub ? hub->proxy_count : 0;
}

void *rh_hub_get_prev(rh_hub_t *hub, void *current_proxy)
{
    if (!hub) return hub->orig_addr;
    rh_hub_proxy_t *p = hub->proxies;
    while (p) {
        if (p->proxy_addr == current_proxy && p->enabled) {
            p = p->next;
            while (p && !p->enabled) p = p->next;
            return p ? p->proxy_addr : hub->orig_addr;
        }
        p = p->next;
    }
    return hub->orig_addr;
}

void rh_hub_push(rh_hub_t *hub, void *proxy_addr)
{
    if (rh_hub_stack_depth < RH_HUB_STACK_MAX) {
        rh_hub_stack[rh_hub_stack_depth].hub = hub;
        rh_hub_stack[rh_hub_stack_depth].proxy_addr = proxy_addr;
        rh_hub_stack[rh_hub_stack_depth].allow_reentrant = false;
        rh_hub_stack_depth++;
    }
}

void rh_hub_pop(void)
{
    if (rh_hub_stack_depth > 0)
        rh_hub_stack_depth--;
}

bool rh_hub_is_recursive(rh_hub_t *hub)
{
    for (int i = 0; i < rh_hub_stack_depth; i++) {
        if (rh_hub_stack[i].hub == hub && !rh_hub_stack[i].allow_reentrant)
            return true;
    }
    return false;
}

void rh_hub_set_reentrant(rh_hub_t *hub, bool allow)
{
    for (int i = rh_hub_stack_depth - 1; i >= 0; i--) {
        if (rh_hub_stack[i].hub == hub) {
            rh_hub_stack[i].allow_reentrant = allow;
            return;
        }
    }
}
