#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    RH_LR_NONE = 0,
    RH_LR_MOV_B,
    RH_LR_MOV_BX,
    RH_LR_PUSH_MOV_BL,
} rh_lr_pattern_t;

rh_lr_pattern_t rh_lr_detect_pattern(uintptr_t target_addr, size_t backup_len);

size_t rh_lr_emit_translate_call(uint16_t *buf, uint16_t orig_inst, rh_lr_pattern_t pattern, uintptr_t translate_fn);
