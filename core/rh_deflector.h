#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uintptr_t addr;
    size_t    size;
} rh_deflector_t;

int  rh_deflector_alloc(rh_deflector_t *deflector, uintptr_t target, size_t range);
void rh_deflector_set_target(rh_deflector_t *deflector, uintptr_t proxy_addr);
void rh_deflector_free(rh_deflector_t *deflector);
