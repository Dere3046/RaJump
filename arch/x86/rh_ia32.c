#include "rh_ia32.h"
#include <string.h>

static const uint8_t *skip_prefixes(const uint8_t *code, const uint8_t *end)
{
    const uint8_t *p = code;
    while (p < end) {
        uint8_t b = *p;
        if (b == 0x66 || b == 0x67 || b == 0xF0 || b == 0xF2 || b == 0xF3 ||
            (b >= 0x26 && b <= 0x2E) || b == 0x64 || b == 0x65) {
            p++;
        } else {
            break;
        }
    }
    return p;
}

size_t rh_ia32_insn_len(const uint8_t *code)
{
    const uint8_t *start = code;
    const uint8_t *end = code + 16;
    const uint8_t *p = skip_prefixes(code, end);

    if (p >= end) return 1;
    uint8_t op = *p++;

    if (op >= 0x50 && op <= 0x5F) return (size_t)(p - start);
    if (op >= 0xB0 && op <= 0xB7) return (size_t)(p - start) + 1;
    if (op >= 0xB8 && op <= 0xBF) return (size_t)(p - start) + 4;

    if (op >= 0x70 && op <= 0x7F) return (size_t)(p - start) + 1;

    switch (op) {
    case 0x68: return (size_t)(p - start) + 4;
    case 0x6A: return (size_t)(p - start) + 1;
    case 0x6C: case 0x6D: case 0x6E: case 0x6F:
        return (size_t)(p - start);
    case 0xA0: case 0xA1: return (size_t)(p - start) + 4;
    case 0xA2: case 0xA3: return (size_t)(p - start) + 4;
    case 0xA4: case 0xA5: case 0xA6: case 0xA7:
    case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAE: case 0xAF:
        return (size_t)(p - start);
    case 0xA8: return (size_t)(p - start) + 1;
    case 0xA9: return (size_t)(p - start) + 4;
    case 0xC2: return (size_t)(p - start) + 2;
    case 0xC3: return (size_t)(p - start);
    case 0xC8: return (size_t)(p - start) + 3;
    case 0xC9: return (size_t)(p - start);
    case 0xCA: return (size_t)(p - start) + 2;
    case 0xCB: return (size_t)(p - start);
    case 0xCC: return (size_t)(p - start);
    case 0xCD: return (size_t)(p - start) + 1;
    case 0xCF: return (size_t)(p - start);
    case 0xD7: return (size_t)(p - start);
    case 0xE0: case 0xE1: case 0xE2: case 0xE3:
        return (size_t)(p - start) + 1;
    case 0xE4: case 0xE5: return (size_t)(p - start) + 1;
    case 0xE6: case 0xE7: return (size_t)(p - start) + 1;
    case 0xE8: case 0xE9: return (size_t)(p - start) + 4;
    case 0xEB: return (size_t)(p - start) + 1;
    case 0xF4: case 0xF5: return (size_t)(p - start);
    case 0xF8: case 0xF9: case 0xFA: case 0xFB: case 0xFC: case 0xFD:
        return (size_t)(p - start);
    case 0x9A: return (size_t)(p - start) + 6;
    case 0x9C: case 0x9D: case 0x9E: case 0x9F:
        return (size_t)(p - start);
    }

    /* 0F two-byte escape */
    if (op == 0x0F) {
        if (p >= end) return 1;
        uint8_t op2 = *p++;
        if (op2 == 0x38 || op2 == 0x3A) {
            if (p >= end) return 1;
            p++;
            /* all 0F38/0F3A have ModRM, fall through */
            goto parse_modrm;
        }
        switch (op2) {
        case 0x05: case 0x06: case 0x07: case 0x09: case 0x0B:
        case 0x30: case 0x31: case 0x32: case 0x34: case 0x35:
        case 0xA2: case 0xAA:
            return (size_t)(p - start);
        }
        if (op2 >= 0x80 && op2 <= 0x8F) return (size_t)(p - start) + 4; /* Jcc rel32 */
        goto parse_modrm;
    }

    /* opcodes with ModRM */
parse_modrm:
    if (p >= end) return (size_t)(p - start);
    uint8_t modrm = *p++;
    uint8_t mod = (modrm >> 6) & 3;
    uint8_t rm  = modrm & 7;

    int disp = 0;
    if (mod == 3) {
        disp = 0;
    } else if (mod == 1) {
        disp = 1;
    } else if (mod == 2) {
        disp = 4;
    } else {
        /* mod == 0 */
        if (rm == 4) {
            if (p >= end) return (size_t)(p - start) + disp;
            uint8_t sib = *p++;
            if ((sib & 7) == 5) disp = 4;
        } else if (rm == 5) {
            disp = 4;
        }
    }

    int imm = 0;
    switch (op) {
    case 0x80: case 0x82: case 0x83: imm = 1; break;
    case 0x81: imm = 4; break;
    case 0xC0: case 0xC1: imm = 1; break;
    case 0xC6: if ((modrm >> 3) == 0) imm = 1; break;
    case 0xC7: if ((modrm >> 3) == 0) imm = 4; break;
    case 0xF6: if ((modrm >> 3) == 0) imm = 1; break; /* TEST */
    case 0xF7: if ((modrm >> 3) == 0) imm = 4; break;
    default: break;
    }

    /* 0F 1F NOP has no immediate */
    (void)imm;

    return (size_t)((p - start) + disp + imm);
}

rh_ia32_insn_type_t rh_ia32_classify(const uint8_t *code, rh_ia32_insn_t *out)
{
    const uint8_t *end = code + 16;
    const uint8_t *p = skip_prefixes(code, end);
    int prefix_len = (int)(p - code);

    if (p >= end) goto other;
    uint8_t op = *p++;

    if (op == 0xE8) {
        out->type = RH_IA32_CALL_REL;
        out->size = (uint8_t)(prefix_len + 5);
        out->output_size = 5;
        out->rel_offset = (int32_t)(prefix_len + 1);
        return out->type;
    }
    if (op == 0xE9) {
        out->type = RH_IA32_JMP_REL;
        out->size = (uint8_t)(prefix_len + 5);
        out->output_size = 5;
        out->rel_offset = (int32_t)(prefix_len + 1);
        return out->type;
    }
    if (op == 0xEB) {
        out->type = RH_IA32_JMP_REL;
        out->size = (uint8_t)(prefix_len + 2);
        out->output_size = 5;
        out->rel_offset = (int32_t)(prefix_len + 1);
        return out->type;
    }
    if (op == 0x0F) {
        if (p >= end) goto other;
        uint8_t op2 = *p++;
        if (op2 >= 0x80 && op2 <= 0x8F) {
            out->type = RH_IA32_JCC;
            out->size = (uint8_t)(prefix_len + 6);
            out->output_size = 6;
            out->rel_offset = (int32_t)(prefix_len + 2);
            return out->type;
        }
        out->size = (uint8_t)rh_ia32_insn_len(code);
        out->output_size = out->size;
        out->rel_offset = 0;
        out->type = RH_IA32_OTHER;
        return out->type;
    }
    if (op >= 0x70 && op <= 0x7F) {
        out->type = RH_IA32_JCC;
        out->size = (uint8_t)(prefix_len + 2);
        out->output_size = 6;
        out->rel_offset = (int32_t)(prefix_len + 1);
        return out->type;
    }
    if (op == 0xC3) {
        out->type = RH_IA32_RET;
        out->size = (uint8_t)(prefix_len + 1);
        out->output_size = 1;
        out->rel_offset = 0;
        return out->type;
    }
    if (op == 0xC2) {
        out->type = RH_IA32_RET;
        out->size = (uint8_t)(prefix_len + 3);
        out->output_size = 3;
        out->rel_offset = 0;
        return out->type;
    }

other:
    out->size = (uint8_t)rh_ia32_insn_len(code);
    out->output_size = out->size;
    out->rel_offset = 0;
    out->type = RH_IA32_OTHER;
    return out->type;
}

size_t rh_ia32_rewrite_size(rh_ia32_insn_type_t type)
{
    switch (type) {
    case RH_IA32_JMP_REL:  return 5;
    case RH_IA32_CALL_REL: return 5;
    case RH_IA32_JCC:      return 6;
    case RH_IA32_RET:
    case RH_IA32_OTHER:
    default:
        return 0;
    }
}

int32_t rh_ia32_decode_branch_target(uintptr_t pc, const uint8_t *insn, size_t size)
{
    if (size < 2) return 0;
    const uint8_t *end = insn + 16;
    const uint8_t *p = skip_prefixes(insn, end);
    uint8_t op = *p++;

    if (op == 0xE8 || op == 0xE9) {
        if (size < 5) return 0;
        int32_t rel = (int32_t)(*(const uint32_t *)(p));
        return (int32_t)((int32_t)pc + (int32_t)(p - insn) + 4 + rel);
    }
    if (op == 0xEB) {
        int32_t rel = (int32_t)(*(const int8_t *)(p));
        return (int32_t)((int32_t)pc + (int32_t)(p - insn) + 1 + rel);
    }
    if (op == 0x0F && (p[0] >= 0x80 && p[0] <= 0x8F)) {
        if (size < 6) return 0;
        int32_t rel = (int32_t)(*(const uint32_t *)(p + 1));
        return (int32_t)((int32_t)pc + (int32_t)(p - insn) + 2 + 4 + rel);
    }
    if (op >= 0x70 && op <= 0x7F) {
        int32_t rel = (int32_t)(*(const int8_t *)(p));
        return (int32_t)((int32_t)pc + (int32_t)(p - insn) + 1 + rel);
    }
    return 0;
}

void rh_ia32_rewrite(uint8_t *output, const uint8_t *input, size_t input_size,
                      uintptr_t input_pc, uintptr_t output_pc)
{
    const uint8_t *end_buf = input + 16;
    const uint8_t *p = skip_prefixes(input, end_buf);
    size_t prefix_len = (size_t)(p - input);
    (void)input_size;

    if (p >= end_buf) {
        memcpy(output, input, 1);
        return;
    }
    uint8_t op = *p++;

    /* E8 rel32 */
    if (op == 0xE8) {
        int32_t rel = *(const int32_t *)p;
        if (rel == 0) {
            /* get-EIP idiom: PUSH output_pc + 5 */
            output[0] = 0x68;
            *(uint32_t *)(output + 1) = (uint32_t)(output_pc + 5);
        } else {
            int32_t target = (int32_t)(input_pc + prefix_len + 1 + 4 + rel);
            int32_t new_rel = target - (int32_t)(output_pc + prefix_len + 5);
            memcpy(output, input, prefix_len);
            output[prefix_len] = 0xE8;
            *(uint32_t *)(output + prefix_len + 1) = (uint32_t)new_rel;
        }
        return;
    }

    /* E9 rel32 */
    if (op == 0xE9) {
        int32_t rel = *(const int32_t *)p;
        int32_t target = (int32_t)(input_pc + prefix_len + 1 + 4 + rel);
        int32_t new_rel = target - (int32_t)(output_pc + prefix_len + 5);
        memcpy(output, input, prefix_len);
        output[prefix_len] = 0xE9;
        *(uint32_t *)(output + prefix_len + 1) = (uint32_t)new_rel;
        return;
    }

    /* EB rel8 */
    if (op == 0xEB) {
        int32_t rel = (int32_t)(*(const int8_t *)p);
        int32_t target = (int32_t)(input_pc + prefix_len + 1 + 1 + rel);
        int32_t new_rel = target - (int32_t)(output_pc + prefix_len + 5);
        memcpy(output, input, prefix_len);
        output[prefix_len] = 0xE9;
        *(uint32_t *)(output + prefix_len + 1) = (uint32_t)new_rel;
        return;
    }

    /* 0F 8x rel32 */
    if (op == 0x0F && (p[0] >= 0x80 && p[0] <= 0x8F)) {
        uint8_t cc = p[0];
        int32_t rel = *(const int32_t *)(p + 1);
        int32_t target = (int32_t)(input_pc + prefix_len + 2 + 4 + rel);
        int32_t new_rel = target - (int32_t)(output_pc + prefix_len + 6);
        memcpy(output, input, prefix_len);
        output[prefix_len] = 0x0F;
        output[prefix_len + 1] = cc;
        *(uint32_t *)(output + prefix_len + 2) = (uint32_t)new_rel;
        return;
    }

    /* 7x rel8 */
    if (op >= 0x70 && op <= 0x7F) {
        int32_t rel = (int32_t)(*(const int8_t *)p);
        int32_t target = (int32_t)(input_pc + prefix_len + 1 + 1 + rel);
        int32_t new_rel = target - (int32_t)(output_pc + prefix_len + 6);
        memcpy(output, input, prefix_len);
        output[prefix_len] = 0x0F;
        output[prefix_len + 1] = 0x80 | (op & 0x0F);
        *(uint32_t *)(output + prefix_len + 2) = (uint32_t)new_rel;
        return;
    }

    /* C3 / C2 / OTHER: copy verbatim */
    size_t bytes = rh_ia32_insn_len(input);
    memcpy(output, input, bytes);
}

size_t rh_ia32_relocate_instructions(void *input, void *output, size_t input_size,
                                      uintptr_t input_pc, uintptr_t output_pc)
{
    uint8_t *out = (uint8_t *)output;
    const uint8_t *in = (const uint8_t *)input;
    size_t in_off = 0;
    size_t out_off = 0;

    while (in_off < input_size) {
        rh_ia32_insn_t insn;
        rh_ia32_classify(in + in_off, &insn);
        if (in_off + insn.size > input_size) break;
        rh_ia32_rewrite(out + out_off, in + in_off, insn.size,
                         input_pc + in_off, output_pc + out_off);
        in_off += insn.size;
        out_off += (insn.output_size == 0) ? insn.size : insn.output_size;
    }

    /* append JMP back to input_pc + input_size */
    int32_t jmp_rel = (int32_t)(input_pc + input_size) - (int32_t)(output_pc + out_off) - 5;
    out[out_off] = 0xE9;
    *(uint32_t *)(out + out_off + 1) = (uint32_t)jmp_rel;
    out_off += 5;

    return out_off;
}
