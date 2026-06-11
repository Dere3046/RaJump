#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rh_island.h"

typedef struct {
  uint8_t backup[12];
  size_t backup_len;
  size_t rewritten_len;
  uint32_t exit[3];
  uintptr_t enter;
  rh_island_t island_exit;
} rh_arm_inst_t;

int rh_arm_inst_hook(rh_arm_inst_t *self, void *target, void *replace, void **origin);
int rh_arm_inst_unhook(rh_arm_inst_t *self, void *target);
void rh_arm_inst_free(rh_arm_inst_t *self);
