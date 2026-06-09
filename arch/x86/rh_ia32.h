#pragma once
#include <stddef.h>
#include <stdint.h>

typedef enum {
    RH_IA32_JMP_REL = 0,
    RH_IA32_CALL_REL = 1,
    RH_IA32_JCC = 2,
    RH_IA32_RET = 3,
    RH_IA32_OTHER = 4,
} rh_ia32_insn_type_t;

typedef struct {
    rh_ia32_insn_type_t type;
    uint8_t  size;
    uint8_t  output_size;
    int32_t  rel_offset;
} rh_ia32_insn_t;

rh_ia32_insn_type_t rh_ia32_classify(const uint8_t *code, rh_ia32_insn_t *out);
size_t rh_ia32_rewrite_size(rh_ia32_insn_type_t type);
int32_t rh_ia32_decode_branch_target(uintptr_t pc, const uint8_t *insn, size_t size);
void rh_ia32_rewrite(uint8_t *output, const uint8_t *input, size_t input_size,
                      uintptr_t input_pc, uintptr_t output_pc);
size_t rh_ia32_relocate_instructions(void *input, void *output, size_t input_size,
                                      uintptr_t input_pc, uintptr_t output_pc);
size_t rh_ia32_insn_len(const uint8_t *code);
