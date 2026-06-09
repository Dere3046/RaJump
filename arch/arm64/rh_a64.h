// Copyright (c) 2024-2026 dere3046
//
// Portions derived from ShadowHook:
// Copyright (c) 2021-2025 ByteDance Inc.

#pragma once
#include <stddef.h>
#include <stdint.h>

#include "rh_arm64_inst.h"
#include "rh_island.h"
#include "rh_linker.h"

typedef struct {
  uintptr_t start_addr;
  uintptr_t end_addr;
  uint32_t *buf;
  size_t buf_offset;
  size_t inst_prolog_len;
  size_t inst_lens[6];
  size_t inst_lens_cnt;
  rh_island_t *island_rewrite;
  rh_addr_info_t *addr_info;
} rh_a64_rewrite_info_t;

size_t rh_a64_get_rewrite_inst_len(uint32_t inst);
size_t rh_a64_rewrite(uint32_t *buf, uint32_t inst, uintptr_t pc, rh_a64_rewrite_info_t *rinfo);

size_t rh_a64_nop(uint32_t *buf);

size_t rh_a64_absolute_jump_with_br_ip(uint32_t *buf, uintptr_t addr);
size_t rh_a64_absolute_jump_with_ret_ip(uint32_t *buf, uintptr_t addr);
size_t rh_a64_restore_ip(uint32_t *buf);

size_t rh_a64_absolute_jump_with_br_rx(uint32_t *buf, uintptr_t addr);
size_t rh_a64_absolute_jump_with_ret_rx(uint32_t *buf, uintptr_t addr);
size_t rh_a64_restore_rx(uint32_t *buf);

size_t rh_a64_relative_jump(uint32_t *buf, uintptr_t addr, uintptr_t pc);

size_t rh_a64_cfg_safe_size(uintptr_t addr, size_t min_bytes, size_t max_bytes);
