#include "rh_linker.h"

#include <link.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "rh_util.h"
#include "rh_sig.h"

#ifdef __ANDROID__
#include "xdl.h"
#include "xdl_util.h"
#endif

static rh_dl_cb_t g_dl_init_cb = NULL;
static rh_dl_cb_t g_dl_fini_cb = NULL;
static rh_deferred_task_t *g_deferred_tasks = NULL;

static char *g_gap_cache_lib = NULL;
static rh_gap_t *g_gap_cache = NULL;
static size_t g_gap_cache_count = 0;

int rh_linker_init(void) {
#ifdef __ANDROID__
    int api = xdl_util_get_api_level();
    if (api >= 21) {
        // hook linker call_constructors
        void *h = xdl_open("linker", RTLD_NOW);
        if (h) {
            xdl_close(h);
        }
    }
#endif
    return 0;
}

void *rh_dlopen(const char *lib) {
#ifdef __ANDROID__
    return xdl_open(lib, RTLD_NOW | RTLD_NOLOAD);
#else
    return dlopen(lib, RTLD_NOW | RTLD_NOLOAD);
#endif
}

void rh_dlclose(void *handle) {
    if (!handle) return;
#ifdef __ANDROID__
    xdl_close(handle);
#else
    dlclose(handle);
#endif
}

void *rh_dlsym(void *handle, const char *sym) {
#ifdef __ANDROID__
    return xdl_sym(handle, sym, NULL);
#else
    return dlsym(handle, sym);
#endif
}

void *rh_dlsym_dynsym(void *handle, const char *sym) {
#ifdef __ANDROID__
    return xdl_dsym(handle, sym, NULL);
#else
    return dlsym(handle, sym);
#endif
}

void *rh_dlsym_symtab(void *handle, const char *sym) {
#ifdef __ANDROID__
    return xdl_dsym(handle, sym, NULL);
#else
    return dlsym(handle, sym);
#endif
}

// gap_cb — dl_iterate_phdr callback
struct gap_ctx { const char *lib; rh_gap_t *gaps; size_t count; size_t cap; };

static int gap_cb(struct dl_phdr_info *info, size_t sz, void *arg) {
    struct gap_ctx *c = (struct gap_ctx *)arg;
    (void)sz;
    if (!c->lib || strstr(info->dlpi_name, c->lib)) {
        for (size_t i = 0; i + 1 < info->dlpi_phnum; i++) {
            if (info->dlpi_phdr[i].p_type != PT_LOAD || info->dlpi_phdr[i+1].p_type != PT_LOAD) continue;
            uintptr_t end1 = (uintptr_t)info->dlpi_addr + info->dlpi_phdr[i].p_vaddr + info->dlpi_phdr[i].p_memsz;
            uintptr_t start2 = (uintptr_t)info->dlpi_addr + info->dlpi_phdr[i+1].p_vaddr;
            uintptr_t pg_end = (end1 + 4095) & ~4095UL;
            if (pg_end < start2 && start2 - pg_end >= 8) {
                c->gaps = realloc(c->gaps, (c->count + 1) * sizeof(rh_gap_t));
                c->gaps[c->count].start = (void *)pg_end;
                c->gaps[c->count].end = (void *)start2;
                c->gaps[c->count].size = start2 - pg_end;
                c->count++;
            }
        }
    }
    return 0;
}

int rh_linker_scan_gaps(const char *lib, rh_gap_t **gaps, size_t *count) {
    *gaps = NULL; *count = 0;
    (void)lib;

    if (lib && g_gap_cache_lib && strcmp(lib, g_gap_cache_lib) == 0) {
        *gaps = g_gap_cache;
        *count = g_gap_cache_count;
        return g_gap_cache_count > 0 ? 0 : -1;
    }

#ifdef __ANDROID__
    // use dl_iterate_phdr to find ELF gaps

    struct gap_ctx data = {lib, NULL, 0, 0};
    dl_iterate_phdr(gap_cb, &data);

    *gaps = data.gaps;
    *count = data.count;
#endif

    free(g_gap_cache_lib);
    free(g_gap_cache);
    g_gap_cache_lib = lib ? strdup(lib) : NULL;
    g_gap_cache = *gaps;
    g_gap_cache_count = *count;
    return *count > 0 ? 0 : -1;
}

void *rh_linker_find_sym(const char *lib, const char *sym) {
    void *h = rh_dlopen(lib);
    if (!h) return NULL;
    void *addr = rh_dlsym(h, sym);
    rh_dlclose(h);
    return addr;
}

int rh_linker_register_dl_init_cb(rh_dl_cb_t cb) {
    g_dl_init_cb = cb;
    return 0;
}

int rh_linker_register_dl_fini_cb(rh_dl_cb_t cb) {
    g_dl_fini_cb = cb;
    return 0;
}

int rh_linker_add_deferred(rh_deferred_task_t *task) {
    if (!task) return -1;
    task->next = g_deferred_tasks;
    g_deferred_tasks = task;
    return 0;
}

int rh_linker_process_deferred(const char *lib_name) {
    rh_deferred_task_t *prev = NULL, *task = g_deferred_tasks;
    while (task) {
        rh_deferred_task_t *next = task->next;
        if (!lib_name || strstr(task->lib_name, lib_name)) {
            void *target = rh_linker_find_sym(task->lib_name, task->sym_name);
            if (target) {
                // execute the hook (delegated to rahook.c)
                if (task->callback)
                    task->callback(0, task->lib_name, task->sym_name,
                                   target, task->replace, NULL, task->callback_arg);
                // remove from list
                if (prev) prev->next = next;
                else g_deferred_tasks = next;
                free(task->lib_name);
                free(task->sym_name);
                free(task);
                task = next;
                continue;
            }
        }
        prev = task;
        task = next;
    }
    return 0;
}

#include <dlfcn.h>

int rh_linker_get_addr_info_by_addr(rh_addr_info_t *info, void *addr, bool is_sym, bool is_proc, bool ignore_sym) {
    (void)is_sym; (void)ignore_sym;
    memset(info, 0, sizeof(*info));
    info->is_sym_addr = true;
    info->is_proc_start = is_proc;
    Dl_info dl;
    if (dladdr(addr, &dl)) {
        info->dli_fbase = dl.dli_fbase;
        info->dli_fname = strdup(dl.dli_fname);
        info->dli_saddr = dl.dli_saddr;
        info->dli_ssize = 0;
    }
    return 0;
}
