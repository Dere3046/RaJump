#include "rahook.h"
#include "core/rh_bytesig.h"
#include "core/rh_hub.h"
#include "core/rh_island.h"
#include "core/rh_linker.h"
#include "core/rh_mode.h"
#include "core/rh_plt.h"
#include "core/rh_recorder.h"
#include "core/rh_trampo.h"
#include "core/rh_util.h"

#if defined(RH_ARCH_ARM64)
#include "arch/arm64/rh_a64.h"
#include "arch/arm64/rh_arm64_inst.h"
#include "arch/arm64/rh_bridge.h"
#include "arch/arm64/rh_pac.h"
#elif defined(RH_ARCH_ARM)
#include "arch/arm/rh_a32.h"
#include "arch/arm/rh_arm_inst.h"
#include "arch/arm/rh_bridge.h"
#include "arch/arm/rh_t16.h"
#include "arch/arm/rh_t32.h"
#elif defined(RH_ARCH_X86)
#include "arch/x86/rh_ia32.h"
#include "arch/x86/rh_x86_inst.h"
#elif defined(RH_ARCH_X86_64)
#include "arch/x86_64/rh_x64.h"
#include "arch/x86_64/rh_x64_inst.h"
#endif

#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct rh_entry {
    void       *target;
    void       *replace;
    void       *origin;
    rh_mode_t   mode;
    bool        active;
    bool        deferred;
    char       *lib_name;
    char       *sym_name;
    rh_hub_t   *hub;
    uint32_t    flags;
    rahook_pre_t  pre;
    rahook_post_t post;
    void       *user_data;
    rahook_interceptor_t interceptor;

#if defined(RH_ARCH_ARM64)
    rh_arm64_inst_t inst;
#elif defined(RH_ARCH_ARM)
    rh_arm_inst_t inst;
#elif defined(RH_ARCH_X86)
    rh_x86_inst_t inst;
#elif defined(RH_ARCH_X86_64)
    rh_x64_inst_t inst;
#endif

    struct rh_entry *next;
    struct rh_entry *next_tx;
} rh_entry_t;

static rh_entry_t      *g_entries       = NULL;
static pthread_mutex_t  g_lock          = PTHREAD_MUTEX_INITIALIZER;
static bool             g_initialized   = false;
static bool             g_bytesig_ok    = false;
static rh_trampo_t      g_trampo;
static int              g_errno         = RAHOOK_ERR_OK;
static bool             g_disabled      = false;
static bool             g_debug         = false;
static _Thread_local bool g_thread_ignored = false;
static bool             g_in_transaction       = false;

void *g_bridge_enter = NULL;
void *g_bridge_leave = NULL;
static rh_entry_t      *g_transaction_entries  = NULL;
static int              g_transaction_count    = 0;

rh_trampo_t *rh_trampo_get_global(void) { return &g_trampo; }

static rh_entry_t *rh_entry_alloc(void) {
    return calloc(1, sizeof(rh_entry_t));
}

static void rh_entry_free(rh_entry_t *e) {
    if (!e) return;
    free(e->lib_name);
    free(e->sym_name);
    free(e);
}

static rh_entry_t *rh_find_entry_by_target(void *target) {
    for (rh_entry_t *e = g_entries; e; e = e->next)
        if (e->active && e->target == target) return e;
    return NULL;
}

static rh_entry_t *rh_find_hub_entry(void *target) {
    for (rh_entry_t *e = g_entries; e; e = e->next)
        if (e->active && e->target == target &&
            e->mode == RH_MODE_SHARED && e->hub)
            return e;
    return NULL;
}

static rh_hub_t *rh_find_or_create_hub(void *target) {
    pthread_mutex_lock(&g_lock);
    rh_entry_t *cur = g_entries;
    while (cur) {
        if (cur->target == target && cur->hub) {
            pthread_mutex_unlock(&g_lock);
            return cur->hub;
        }
        cur = cur->next;
    }
    pthread_mutex_unlock(&g_lock);

    rh_hub_t *hub = calloc(1, sizeof(rh_hub_t));
    if (!hub) return NULL;
    int r = rh_hub_create(hub, (uintptr_t)target, NULL);
    if (r != 0) { free(hub); return NULL; }
    return hub;
}

static int rh_arch_hook(rh_entry_t *e, void *target, void *replace) {
#if defined(RH_ARCH_ARM64)
    return rh_arm64_inst_hook(&e->inst, target, replace, &e->origin);
#elif defined(RH_ARCH_ARM)
    return rh_arm_inst_hook(&e->inst, target, replace, &e->origin);
#elif defined(RH_ARCH_X86)
    return rh_x86_inst_hook(&e->inst, target, replace, &e->origin);
#elif defined(RH_ARCH_X86_64)
    return rh_x64_inst_hook(&e->inst, target, replace, &e->origin);
#endif
}

static int rh_arch_unhook(rh_entry_t *e, void *target) {
#if defined(RH_ARCH_ARM64)
    return rh_arm64_inst_unhook(&e->inst, target);
#elif defined(RH_ARCH_ARM)
    return rh_arm_inst_unhook(&e->inst, target);
#elif defined(RH_ARCH_X86)
    return rh_x86_inst_unhook(&e->inst, target);
#elif defined(RH_ARCH_X86_64)
    return rh_x64_inst_unhook(&e->inst, target);
#endif
}

static void rh_entry_register(rh_entry_t *e) {
    pthread_mutex_lock(&g_lock);
    e->next = g_entries;
    g_entries = e;
    pthread_mutex_unlock(&g_lock);
}

static void rh_entry_unlink(rh_entry_t *e) {
    pthread_mutex_lock(&g_lock);
    rh_entry_t **prev = &g_entries;
    while (*prev) {
        if (*prev == e) { *prev = e->next; break; }
        prev = &(*prev)->next;
    }
    pthread_mutex_unlock(&g_lock);
}

int rahook_init(void) {
    if (g_initialized) return 0;
    rh_util_init();
    rh_trampo_init(&g_trampo, 4096);
    rh_island_init();
#if defined(RH_ARCH_ARM64)
    rh_bridge_init();
    rh_bridge_page_t *bp = rh_bridge_alloc();
    if (bp) {
        g_bridge_enter = bp->enter_bridge;
        g_bridge_leave = bp->leave_bridge;
    }
#elif defined(RH_ARCH_ARM)
    rh_bridge_init();
    rh_arm_bridge_page_t *bp = rh_arm_bridge_alloc();
    if (bp) {
        g_bridge_enter = bp->enter_bridge;
        g_bridge_leave = bp->leave_bridge;
    }
#endif
    if (!g_bytesig_ok) {
        rh_bytesig_init(SIGSEGV);
        rh_bytesig_init(SIGBUS);
        g_bytesig_ok = true;
    }
    rh_linker_init();
    rh_recorder_set_recordable(false);
    g_initialized = true;
    return 0;
}

void rahook_deinit(void) {
    if (!g_initialized) return;
    pthread_mutex_lock(&g_lock);
    rh_entry_t *cur = g_entries;
    while (cur) {
        rh_entry_t *next = cur->next;
        if (cur->active && !cur->hub)
            rh_arch_unhook(cur, cur->target);
        if (cur->hub) {
            rh_hub_destroy(cur->hub);
            cur->hub = NULL;
        }
        rh_entry_free(cur);
        cur = next;
    }
    g_entries = NULL;
    pthread_mutex_unlock(&g_lock);
    g_initialized = false;
}

int rahook_get_errno(void) {
    int err = g_errno;
    g_errno = RAHOOK_ERR_OK;
    return err;
}

const char *rahook_to_errmsg(int err) {
    switch (err) {
    case RAHOOK_ERR_OK:             return "ok";
    case RAHOOK_ERR_UNINIT:         return "not initialized";
    case RAHOOK_ERR_INVALID_ARG:    return "invalid argument";
    case RAHOOK_ERR_OOM:            return "out of memory";
    case RAHOOK_ERR_HOOK_FAILED:    return "hook failed";
    case RAHOOK_ERR_DUP:            return "duplicate hook";
    case RAHOOK_ERR_NOT_FOUND:      return "symbol not found";
    case RAHOOK_ERR_MODE_CONFLICT:  return "mode conflict";
    case RAHOOK_ERR_DEFERRED:       return "deferred";
    default:                        return "unknown error";
    }
}

void *rahook_dlopen(const char *lib_name)    { return rh_dlopen(lib_name); }
void  rahook_dlclose(void *handle)           { rh_dlclose(handle); }
void *rahook_dlsym(void *handle, const char *n)  { return rh_dlsym(handle, n); }
void *rahook_dlsym_dynsym(void *handle, const char *n) { return rh_dlsym_dynsym(handle, n); }
void *rahook_dlsym_symtab(void *handle, const char *n) { return rh_dlsym_symtab(handle, n); }

static rh_mode_t rh_flags_to_mode(uint32_t flags) {
    if (flags & RAHOOK_FLAG_UNIQUE) return RH_MODE_UNIQUE;
    if (flags & RAHOOK_FLAG_MULTI)  return RH_MODE_MULTI;
    return RH_MODE_SHARED;
}

static int rh_check_mode_conflict(void *target, rh_mode_t mode) {
    rh_entry_t *existing = rh_find_entry_by_target(target);
    if (!existing) return 0;
    if (existing->mode == RH_MODE_UNIQUE || mode == RH_MODE_UNIQUE) {
        if (existing->mode != mode) return RAHOOK_ERR_MODE_CONFLICT;
        return RAHOOK_ERR_DUP;
    }
    if (existing->mode != mode) return RAHOOK_ERR_MODE_CONFLICT;
    return 0;
}

void *rahook(void *target, void *replace, void **origin, uint32_t flags) {
    if (!g_initialized || !target || !replace) {
        g_errno = RAHOOK_ERR_INVALID_ARG;
        return NULL;
    }
    if (g_disabled) {
        g_errno = RAHOOK_ERR_DISABLED;
        return NULL;
    }
    if (g_thread_ignored) {
        *origin = target;
        return NULL;
    }

#if defined(RH_ARCH_ARM64)
    target = (void *)rh_pac_strip((uintptr_t)target);
    replace = (void *)rh_pac_strip((uintptr_t)replace);
#endif

    rh_mode_t mode = rh_flags_to_mode(flags);
    int conflict = rh_check_mode_conflict(target, mode);
    if (conflict != 0) { g_errno = conflict; return NULL; }

    rh_entry_t *e = rh_entry_alloc();
    if (!e) { g_errno = RAHOOK_ERR_OOM; return NULL; }
    e->target  = target;
    e->replace = replace;
    e->mode    = mode;
    e->flags   = flags;

    if (g_in_transaction) {
        e->next_tx = g_transaction_entries;
        g_transaction_entries = e;
        g_transaction_count++;
        if (origin && e->origin) *origin = e->origin;
        rh_entry_register(e);
        return e;
    }

    if (mode == RH_MODE_SHARED) {
#if defined(RH_ARCH_ARM64) || defined(RH_ARCH_ARM)
        rh_hub_t *hub = rh_find_or_create_hub(target);
        if (!hub) {
            g_errno = RAHOOK_ERR_HOOK_FAILED;
            return NULL;
        }
        e->hub = hub;
        int hr = rh_hub_add(hub, replace);
        if (hr != 0) { g_errno = RAHOOK_ERR_HOOK_FAILED; rh_entry_free(e); return NULL; }

        if (rh_hub_count(hub) == 1) {
            void *trampo_target = (void *)hub->trampoline;
            if (!trampo_target) {
                g_errno = RAHOOK_ERR_HOOK_FAILED;
                rh_entry_free(e);
                return NULL;
            }

            int r;
#if defined(RH_ARCH_ARM64)
            r = rh_arm64_inst_hook(&e->inst, target, trampo_target, &e->origin);
#elif defined(RH_ARCH_ARM)
            r = rh_arm_inst_hook(&e->inst, target, trampo_target, &e->origin);
#elif defined(RH_ARCH_X86)
            r = rh_x86_inst_hook(&e->inst, target, trampo_target, NULL);
#elif defined(RH_ARCH_X86_64)
            r = rh_x64_inst_hook(&e->inst, target, trampo_target, NULL);
#endif
            if (r != 0) {
                rh_hub_remove(hub, replace);
                rh_entry_free(e);
                return NULL;
            }
        }
        e->active = true;
        if (origin) *origin = hub->trampoline;
        goto done;
    }
#else
        mode = RH_MODE_UNIQUE;
    }
#endif
    uintptr_t got_slot;
    if (rh_plt_is_stub((uintptr_t)target, &got_slot)) {
        if (origin) *origin = *(void **)got_slot;
        rh_plt_patch_got((uintptr_t *)got_slot, (uintptr_t)replace);
        e->active = true;
        goto done;
    }

    if (mode == RH_MODE_MULTI) {
        rh_entry_t *head = rh_find_entry_by_target(target);
        int r = rh_arch_hook(e, target, replace);
        if (r != 0) { rh_entry_free(e); g_errno = RAHOOK_ERR_HOOK_FAILED; return NULL; }
        if (head && head->active && head->mode == RH_MODE_MULTI)
            head->origin = replace;
        e->active = true;
        if (origin) *origin = e->origin;
    } else {
        int r = rh_arch_hook(e, target, replace);
        if (r != 0) { rh_entry_free(e); g_errno = RAHOOK_ERR_HOOK_FAILED; return NULL; }
        e->active = true;
        if (origin) *origin = e->origin;
    }

done:
    rh_entry_register(e);

    rh_recorder_add(e->lib_name, e->sym_name, (uintptr_t)target, (uintptr_t)replace,
                    NULL, flags, 0, (uintptr_t)e);

    return e;
}

void *rahook_symbol(const char *lib, const char *sym, void *replace,
                    void **origin, uint32_t flags) {
    if (!g_initialized || !lib || !sym || !replace) {
        g_errno = RAHOOK_ERR_INVALID_ARG;
        return NULL;
    }

    void *handle = rh_dlopen(lib);
    if (!handle) { g_errno = RAHOOK_ERR_NOT_FOUND; return NULL; }

    void *target = rh_dlsym(handle, sym);
    if (!target) { g_errno = RAHOOK_ERR_NOT_FOUND; return NULL; }

    void *stub = rahook(target, replace, origin, flags);
    if (stub) {
        rh_entry_t *e = (rh_entry_t *)stub;
        e->lib_name = strdup(lib);
        e->sym_name = strdup(sym);
    }
    return stub;
}

void *rahook_symbol_callback(const char *lib, const char *sym, void *replace,
                             void **origin, uint32_t flags,
                             rahook_hooked_cb_t cb, void *cb_arg) {
    if (!g_initialized || !lib || !sym || !replace) {
        g_errno = RAHOOK_ERR_INVALID_ARG;
        if (cb) cb(RAHOOK_ERR_INVALID_ARG, lib, sym, NULL, NULL, NULL, cb_arg);
        return NULL;
    }

    void *handle = rh_dlopen(lib);
    void *target = handle ? rh_dlsym(handle, sym) : NULL;

    if (!target) {
        rh_deferred_task_t *task = calloc(1, sizeof(rh_deferred_task_t));
        if (!task) {
            g_errno = RAHOOK_ERR_OOM;
            if (cb) cb(RAHOOK_ERR_OOM, lib, sym, NULL, NULL, NULL, cb_arg);
            return NULL;
        }
        task->lib_name     = strdup(lib);
        task->sym_name     = strdup(sym);
        task->replace      = replace;
        task->origin       = origin;
        task->mode         = (int)rh_flags_to_mode(flags);
        task->has_callback = true;
        task->callback     = (void (*)(int, const char *, const char *,
                                       void *, void *, void *, void *))cb;
        task->callback_arg = cb_arg;
        rh_linker_add_deferred(task);
        g_errno = RAHOOK_ERR_DEFERRED;
        if (origin) *origin = NULL;
        return NULL;
    }

    void *stub = rahook(target, replace, origin, flags);
    if (stub) {
        rh_entry_t *e = (rh_entry_t *)stub;
        e->lib_name = strdup(lib);
        e->sym_name = strdup(sym);
        if (cb) cb(0, lib, sym, target, replace, e->origin, cb_arg);
    } else {
        if (cb) cb(g_errno, lib, sym, target, NULL, NULL, cb_arg);
    }
    return stub;
}

int rahook_remove(void *stub) {
    if (!stub) { g_errno = RAHOOK_ERR_INVALID_ARG; return -1; }

    rh_entry_t *e = (rh_entry_t *)stub;

    if (e->active) {
        if (e->mode == RH_MODE_SHARED && e->hub) {
            rh_hub_remove(e->hub, e->replace);
            if (rh_hub_count(e->hub) == 0) {
                rh_entry_t *hub_entry = rh_find_hub_entry(e->target);
                if (hub_entry && hub_entry->hub == e->hub) {
                    rh_arch_unhook(hub_entry, e->target);
                    rh_hub_destroy(e->hub);
                    e->hub = NULL;
                    rh_entry_unlink(hub_entry);
                    rh_entry_free(hub_entry);
                }
            }
        } else if (e->mode == RH_MODE_MULTI) {
            rh_entry_t *head = rh_find_entry_by_target(e->target);
            if (head && head == e) {
                rh_entry_t *next_entry = NULL;
                for (rh_entry_t *cur = g_entries; cur; cur = cur->next) {
                    if (cur->active && cur->target == e->target &&
                        cur->mode == RH_MODE_MULTI && cur != e) {
                        next_entry = cur; break;
                    }
                }
                if (next_entry) {
                    rh_arch_unhook(e, e->target);
                    rh_arch_hook(next_entry, e->target, next_entry->replace);
                    next_entry->origin = e->origin;
                } else {
                    rh_arch_unhook(e, e->target);
                }
            }
        } else {
            rh_arch_unhook(e, e->target);
        }
    }

    rh_recorder_add(e->lib_name, e->sym_name, (uintptr_t)e->target, 0,
                    NULL, e->flags | 0x100, 0, (uintptr_t)e);

    rh_entry_unlink(e);
    rh_entry_free(e);
    return 0;
}

void rahook_record_start(void)       { rh_recorder_start(); }
void rahook_record_stop(void)        { rh_recorder_stop(); }
bool rahook_record_is_active(void)   { return rh_recorder_is_active(); }
char *rahook_record_export(void)     { return rh_recorder_export(); }
void rahook_record_free(char *json)  { rh_recorder_free(json); }

int rahook_register_dl_init_cb(rahook_dl_cb_t cb) {
    return rh_linker_register_dl_init_cb((rh_dl_cb_t)cb);
}

int rahook_register_dl_fini_cb(rahook_dl_cb_t cb) {
    return rh_linker_register_dl_fini_cb((rh_dl_cb_t)cb);
}

int rahook_patch(void *addr, const void *code, size_t size) {
    if (!addr || !code || size == 0) return -1;
    int r = rh_util_mprotect((uintptr_t)addr, size, RH_PROT_RWX);
    if (r != 0) return -1;
    rh_util_write_and_flush((uintptr_t)addr, code, size);
    r = rh_util_mprotect((uintptr_t)addr, size, RH_PROT_RX);
    return r;
}

void rahook_cache_flush(void *addr, size_t size) {
    rh_util_cache_flush((uintptr_t)addr, size);
}

// global disable/enable

bool rahook_get_disable(void) { return g_disabled; }

void rahook_set_disable(bool disable) { g_disabled = disable; }

bool rahook_get_debug(void) { return g_debug; }

void rahook_set_debug(bool debug) { g_debug = debug; }

// transaction batching

int rahook_begin_transaction(void) {
    if (g_in_transaction) return -1;
    g_in_transaction = true;
    g_transaction_entries = NULL;
    g_transaction_count = 0;
    return 0;
}

int rahook_end_transaction(void) {
    if (!g_in_transaction) return -1;
    g_in_transaction = false;

    rh_entry_t *cur = g_transaction_entries;
    while (cur) {
        rh_entry_t *next = cur->next_tx;
        int r = rh_arch_hook(cur, cur->target, cur->replace);
        if (r != 0) { cur->active = false; }
        cur->next_tx = NULL;
        cur = next;
    }
    g_transaction_entries = NULL;
    g_transaction_count = 0;
    return 0;
}

int rahook_abort_transaction(void) {
    if (!g_in_transaction) return -1;

    rh_entry_t *cur = g_transaction_entries;
    while (cur) {
        rh_entry_t *next = cur->next_tx;
        rh_entry_unlink(cur);
        rh_entry_free(cur);
        cur = next;
    }
    g_transaction_entries = NULL;
    g_transaction_count = 0;
    g_in_transaction = false;
    return 0;
}

bool rahook_is_in_transaction(void) { return g_in_transaction; }

// per-instruction intercept

void *rahook_intercept(void *instr_addr, rahook_interceptor_t cb, void *data) {
    if (!g_initialized || !instr_addr || !cb) {
        g_errno = RAHOOK_ERR_INVALID_ARG; return NULL;
    }
    rh_entry_t *e = rh_entry_alloc();
    if (!e) { g_errno = RAHOOK_ERR_OOM; return NULL; }
    e->target      = instr_addr;
    e->mode        = RH_MODE_UNIQUE;
    e->flags       = 1;
    e->interceptor = cb;
    e->user_data   = data;

    int r = rh_arch_hook(e, instr_addr, NULL);
    if (r != 0) { rh_entry_free(e); g_errno = RAHOOK_ERR_HOOK_FAILED; return NULL; }
    e->active = true;
    rh_entry_register(e);
    return e;
}

void *rahook_intercept_symbol(const char *lib, const char *sym, rahook_interceptor_t cb, void *data) {
    if (!g_initialized || !lib || !sym || !cb) {
        g_errno = RAHOOK_ERR_INVALID_ARG; return NULL;
    }
    void *target = rh_linker_find_sym(lib, sym);
    if (!target) { g_errno = RAHOOK_ERR_NOT_FOUND; return NULL; }
    return rahook_intercept(target, cb, data);
}

int rahook_unintercept(void *stub) {
    return rahook_remove(stub);
}


void *rahook_pre_post(void *target, rahook_pre_t pre, rahook_post_t post,
                      void *user_data, void **origin, uint32_t flags) {
    if (!g_initialized || !target) { g_errno = RAHOOK_ERR_INVALID_ARG; return NULL; }
    rh_entry_t *e = rh_entry_alloc();
    if (!e) { g_errno = RAHOOK_ERR_OOM; return NULL; }
    e->target    = target;
    e->pre       = pre;
    e->post      = post;
    e->user_data = user_data;
    e->mode      = rh_flags_to_mode(flags);

    int r = rh_arch_hook(e, target, NULL);
    if (r != 0) { rh_entry_free(e); g_errno = RAHOOK_ERR_HOOK_FAILED; return NULL; }
    e->active = true;
    if (origin) *origin = e->origin;
    rh_entry_register(e);
    return e;
}

void *rahook_pre_post_symbol(const char *lib, const char *sym,
                             rahook_pre_t pre, rahook_post_t post,
                             void *user_data, void **origin, uint32_t flags) {
    if (!g_initialized || !lib || !sym) { g_errno = RAHOOK_ERR_INVALID_ARG; return NULL; }
    void *target = rh_linker_find_sym(lib, sym);
    if (!target) { g_errno = RAHOOK_ERR_NOT_FOUND; return NULL; }
    return rahook_pre_post(target, pre, post, user_data, origin, flags);
}


void rahook_ignore_current_thread(void)   { g_thread_ignored = true; }
void rahook_unignore_current_thread(void) { g_thread_ignored = false; }
bool rahook_is_thread_ignored(void)       { return g_thread_ignored; }


int rahook_unregister_dl_init_cb(rahook_dl_cb_t cb) {
    (void)cb;
    return rh_linker_register_dl_init_cb(NULL);
}
int rahook_unregister_dl_fini_cb(rahook_dl_cb_t cb) {
    (void)cb;
    return rh_linker_register_dl_fini_cb(NULL);
}


void *rahook_plt(void *target, void *replace, void **origin) {
    if (!g_initialized || !target || !replace) {
        g_errno = RAHOOK_ERR_INVALID_ARG; return NULL;
    }
    uintptr_t got_slot;
    if (!rh_plt_is_stub((uintptr_t)target, &got_slot)) {
        g_errno = RAHOOK_ERR_HOOK_FAILED; return NULL;
    }
    if (origin) *origin = *(void **)got_slot;
    rh_plt_patch_got((uintptr_t *)got_slot, (uintptr_t)replace);
    rh_entry_t *e = rh_entry_alloc();
    if (e) {
        e->target = target; e->replace = replace; e->active = true;
        e->mode = RH_MODE_UNIQUE;
        rh_entry_register(e);
        return e;
    }
    return NULL;
}

void *rahook_plt_symbol(const char *lib, const char *sym, void *replace, void **origin) {
    if (!g_initialized || !lib || !sym || !replace) {
        g_errno = RAHOOK_ERR_INVALID_ARG; return NULL;
    }
    void *target = rh_linker_find_sym(lib, sym);
    if (!target) { g_errno = RAHOOK_ERR_NOT_FOUND; return NULL; }
    return rahook_plt(target, replace, origin);
}
