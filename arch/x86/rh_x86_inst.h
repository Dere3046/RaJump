#pragma once
#include <stddef.h>
#include <stdint.h>

#include "core/rh_island.h"

typedef struct {
    uint8_t     backup[16];
    size_t      backup_len;
    uint8_t     exit[16];
    void       *enter;
    size_t      enter_size;
    rh_island_t island_exit;
} rh_x86_inst_t;

int rh_x86_inst_hook(rh_x86_inst_t *inst, void *target, void *replace, void **origin);
int rh_x86_inst_unhook(rh_x86_inst_t *inst, void *target);
