// Copyright (c) 2024-2026 dere3046
//
// Portions derived from ShadowHook:
// Copyright (c) 2021-2025 ByteDance Inc.

#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "rh_island.h"
#include "rh_linker.h"

typedef void (*rh_arm64_inst_set_orig_addr_t)(uintptr_t orig_addr, void *arg);

typedef struct {
  uint8_t backup[24];
  size_t backup_len;
  uint32_t exit[6];
  uintptr_t enter;
  rh_island_t island_exit;
  rh_island_t island_enter;
  rh_island_t island_rewrite;
} rh_arm64_inst_t;

int rh_arm64_inst_hook(rh_arm64_inst_t *self, void *target, void *replace, void **origin);
int rh_arm64_inst_unhook(rh_arm64_inst_t *self, void *target);
