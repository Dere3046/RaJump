#pragma once
#include <stddef.h>
#include <stdint.h>

#include "rh_arch.h"

typedef enum {
    RH_ISLAND_EXIT = 0,
    RH_ISLAND_ENTER = 1,
} rh_island_type_t;

typedef struct {
    uintptr_t addr;
    size_t size;
    rh_island_type_t type;
} rh_island_t;

int rh_island_init(void);
int rh_island_alloc(rh_island_t *island, uintptr_t target, size_t size, size_t range);
int rh_island_alloc_ex(rh_island_t *island, uintptr_t target, size_t size,
                        size_t range, const char *lib_filter);
void rh_island_free(rh_island_t *island);
