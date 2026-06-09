#pragma once
#include <stddef.h>
#include <stdint.h>

typedef enum {
    RH_X64_JMP_REL = 0,
    RH_X64_CALL_REL = 1,
    RH_X64_JCC = 2,
    RH_X64_RIP_REL = 3,
    RH_X64_RET = 4,
    RH_X64_CET = 5,
    RH_X64_OTHER = 6,
} rh_x64_insn_type_t;

typedef struct {
    rh_x64_insn_type_t type;
    uint8_t  size;
    uint8_t  output_size;
    int32_t  rel_offset;
    int32_t  rip_disp;
} rh_x64_insn_t;

rh_x64_insn_type_t rh_x64_classify(const uint8_t *code, rh_x64_insn_t *out);
size_t rh_x64_rewrite_size(rh_x64_insn_type_t type);
int64_t rh_x64_decode_branch_target(uintptr_t pc, const uint8_t *insn, size_t size);
size_t rh_x64_rewrite(uint8_t *output, const uint8_t *input, size_t input_size,
                      uintptr_t input_pc, uintptr_t output_pc);
size_t rh_x64_relocate_instructions(void *input, void *output, size_t input_size,
                                     uintptr_t input_pc, uintptr_t output_pc);
size_t rh_x64_insn_len(const uint8_t *code);
