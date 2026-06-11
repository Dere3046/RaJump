#pragma once
#include <link.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <dlfcn.h>

#include "rh_config.h"

typedef struct {
    void *dli_fbase;
    char *dli_fname;
    void *dli_saddr;
    size_t dli_ssize;
    const ElfW(Phdr) *dlpi_phdr;
    size_t dlpi_phnum;
    bool is_sym_addr;
    bool is_proc_start;
} rh_addr_info_t;

typedef struct {
    void *start;
    void *end;
    size_t size;
} rh_gap_t;

typedef void (*rh_dl_cb_t)(const char *lib_name);

int rh_linker_init(void);

void *rh_dlopen(const char *lib_name);
void rh_dlclose(void *handle);
void *rh_dlsym(void *handle, const char *sym_name);
void *rh_dlsym_dynsym(void *handle, const char *sym_name);
void *rh_dlsym_symtab(void *handle, const char *sym_name);

int rh_linker_scan_gaps(const char *lib, rh_gap_t **gaps, size_t *count);
void *rh_linker_find_sym(const char *lib, const char *sym);

int rh_linker_register_dl_init_cb(rh_dl_cb_t cb);
int rh_linker_register_dl_fini_cb(rh_dl_cb_t cb);

typedef struct rh_deferred_task {
    char *lib_name;
    char *sym_name;
    void *replace;
    void *pre;
    void *post;
    void *user_data;
    void **origin;
    int mode;
    bool has_callback;
    void (*callback)(int err, const char *lib, const char *sym,
                     void *sym_addr, void *new_addr, void *orig_addr, void *arg);
    void *callback_arg;
    struct rh_deferred_task *next;
} rh_deferred_task_t;

int rh_linker_add_deferred(rh_deferred_task_t *task);
int rh_linker_process_deferred(const char *lib_name);
