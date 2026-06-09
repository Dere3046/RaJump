#include "rh_x64.h"
#include <string.h>

#include "core/rh_trampo.h"

static const uint8_t *skip_prefixes(const uint8_t *code, const uint8_t *end, int *rex_byte)
{
    const uint8_t *p = code;
    *rex_byte = 0;
    while (p < end) {
        uint8_t b = *p;
        if (b >= 0x40 && b <= 0x4F) {
            *rex_byte = b;
            p++;
        } else if (b == 0x66 || b == 0x67 || b == 0xF0 || b == 0xF2 || b == 0xF3 ||
                   (b >= 0x26 && b <= 0x2E) || b == 0x64 || b == 0x65) {
            p++;
        } else if (b == 0xC4 && p + 2 < end) {    
            p += 2;
        } else if (b == 0xC5 && p + 2 < end) {    
            p += 3;
        } else if (b == 0x62 && p + 3 < end) {    
            p += 4;
        } else {
            break;
        }
    }
    return p;
}

static int find_modrm(const uint8_t *code)
{
    const uint8_t *end = code + 16;
    int rex = 0;
    const uint8_t *p = skip_prefixes(code, end, &rex);
    if (p >= end) return -1;
    uint8_t op = *p++;

    if (op == 0x0F) {
        if (p >= end) return -1;
        uint8_t op2 = *p++;
        if (op2 == 0x38 || op2 == 0x3A) {
            if (p >= end) return -1;
            p++;
            return (int)(p - code);
        }
        switch (op2) {
        case 0x05: case 0x06: case 0x07: case 0x09: case 0x0B:
        case 0x30: case 0x31: case 0x32: case 0x34: case 0x35:
        case 0xA2: case 0xAA:
            return -1;
        }
        if (op2 >= 0x80 && op2 <= 0x8F) return -1;
        return (int)(p - code);
    }

    if (op >= 0x50 && op <= 0x5F) return -1;
    if (op >= 0x70 && op <= 0x7F) return -1;
    if (op >= 0xB0 && op <= 0xBF) return -1;
    switch (op) {
    case 0x68: case 0x6A: case 0x6C: case 0x6D: case 0x6E: case 0x6F:
    case 0x9A: case 0x9C: case 0x9D: case 0x9E: case 0x9F:
    case 0xA0: case 0xA1: case 0xA2: case 0xA3:
    case 0xA4: case 0xA5: case 0xA6: case 0xA7:
    case 0xA8: case 0xA9:
    case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAE: case 0xAF:
    case 0xC2: case 0xC3: case 0xC8: case 0xC9: case 0xCA: case 0xCB:
    case 0xCC: case 0xCD: case 0xCF:
    case 0xD7:
    case 0xE0: case 0xE1: case 0xE2: case 0xE3:
    case 0xE4: case 0xE5: case 0xE6: case 0xE7:
    case 0xE8: case 0xE9: case 0xEA: case 0xEB:
    case 0xF4: case 0xF5:
    case 0xF8: case 0xF9: case 0xFA: case 0xFB: case 0xFC: case 0xFD:
        return -1;
    }
    return (int)(p - code);
}

static int opcode_len(const uint8_t *code)
{
    const uint8_t *end = code + 16;
    int rex = 0;
    const uint8_t *p = skip_prefixes(code, end, &rex);
    if (p >= end) return 1;
    uint8_t op = *p++;
    if (op == 0x0F) {
        if (p >= end) return 2;
        uint8_t op2 = *p++;
        if (op2 == 0x38 || op2 == 0x3A) return 3;
        return 2;
    }
    return 1;
}

size_t rh_x64_insn_len(const uint8_t *code)
{
    const uint8_t *start = code;
    const uint8_t *end = code + 16;
    int rex = 0;
    const uint8_t *p = skip_prefixes(code, end, &rex);
    if (p >= end) return 1;
    uint8_t op = *p++;

    if (op >= 0x50 && op <= 0x5F) return (size_t)(p - start);
    if (op >= 0xB0 && op <= 0xB7) return (size_t)(p - start) + 1;
    if (op >= 0xB8 && op <= 0xBF) return (size_t)(p - start) + (size_t)((rex & 0x8) ? 8 : 4);
    if (op >= 0x70 && op <= 0x7F) return (size_t)(p - start) + 1;

    switch (op) {
    case 0x68: return (size_t)(p - start) + 8;
    case 0x6A: return (size_t)(p - start) + 1;
    case 0x6C: case 0x6D: case 0x6E: case 0x6F:
        return (size_t)(p - start);
    case 0xA0: case 0xA1: return (size_t)(p - start) + 8;
    case 0xA2: case 0xA3: return (size_t)(p - start) + 8;
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
    case 0xE4: case 0xE5: case 0xE6: case 0xE7:
        return (size_t)(p - start) + 1;
    case 0xE8: case 0xE9: return (size_t)(p - start) + 4;
    case 0xEB: return (size_t)(p - start) + 1;
    case 0xF4: case 0xF5: return (size_t)(p - start);
    case 0xF8: case 0xF9: case 0xFA: case 0xFB: case 0xFC: case 0xFD:
        return (size_t)(p - start);
    case 0x9A: return (size_t)(p - start) + 6;
    case 0x9C: case 0x9D: case 0x9E: case 0x9F:
        return (size_t)(p - start);
    }

    if (op == 0x0F) {
        if (p >= end) return 1;
        uint8_t op2 = *p++;
        if (op2 == 0x38 || op2 == 0x3A) {
            if (p >= end) return 1;
            p++;
            goto parse_modrm;
        }
        switch (op2) {
        case 0x05: case 0x06: case 0x07: case 0x09: case 0x0B:
        case 0x30: case 0x31: case 0x32: case 0x34: case 0x35:
        case 0xA2: case 0xAA:
            return (size_t)(p - start);
        }
        if (op2 >= 0x80 && op2 <= 0x8F) return (size_t)(p - start) + 4;
        goto parse_modrm;
    }

parse_modrm:
{
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
    case 0xF6: if ((modrm >> 3) == 0) imm = 1; break;
    case 0xF7: if ((modrm >> 3) == 0) imm = 4; break;
    default: break;
    }
    (void)imm;
    return (size_t)((p - start) + disp + imm);
}
}

rh_x64_insn_type_t rh_x64_classify(const uint8_t *code, rh_x64_insn_t *out)
{
    memset(out, 0, sizeof(*out));
    const uint8_t *end = code + 16;
    int rex = 0;
    const uint8_t *p = skip_prefixes(code, end, &rex);
    const uint8_t *prefix_end = p;
    int prefix_len = (int)(p - code);

    {
        const uint8_t *cs = code;
        int has_f3 = 0;
        while (cs < prefix_end) {
            if (*cs == 0xF3) { has_f3 = 1; break; }
            cs++;
        }
        if (has_f3 && p + 3 <= end && p[0] == 0x0F && p[1] == 0x1E && p[2] == 0xFA) {
            out->type = RH_X64_CET;
            out->size = (uint8_t)(prefix_len + 4);
            out->output_size = out->size;
            return out->type;
        }
    }

    if (p >= end) goto other;
    uint8_t op = *p++;

    if (op == 0xE8) {
        out->type = RH_X64_CALL_REL;
        out->size = (uint8_t)(prefix_len + 5);
        out->output_size = 5;
        out->rel_offset = (int32_t)(prefix_len + 1);
        return out->type;
    }
    if (op == 0xE9) {
        out->type = RH_X64_JMP_REL;
        out->size = (uint8_t)(prefix_len + 5);
        out->output_size = 5;
        out->rel_offset = (int32_t)(prefix_len + 1);
        return out->type;
    }
    if (op == 0xEB) {
        out->type = RH_X64_JMP_REL;
        out->size = (uint8_t)(prefix_len + 2);
        out->output_size = 5;
        out->rel_offset = (int32_t)(prefix_len + 1);
        return out->type;
    }
    if (op == 0x0F) {
        if (p >= end) goto other;
        uint8_t op2 = *p++;
        if (op2 >= 0x80 && op2 <= 0x8F) {
            out->type = RH_X64_JCC;
            out->size = (uint8_t)(prefix_len + 6);
            out->output_size = 6;
            out->rel_offset = (int32_t)(prefix_len + 2);
            return out->type;
        }
        /* other 0F: use find_modrm for accurate detection */
        int moff = find_modrm(code);
        if (moff >= 0 && code + moff < end) {
            uint8_t modrm = code[moff];
            uint8_t mod = (modrm >> 6) & 3;
            uint8_t rm  = modrm & 7;
            if (mod == 0 && rm == 5) {
                out->type = RH_X64_RIP_REL;
                out->size = (uint8_t)rh_x64_insn_len(code);
                out->output_size = 0;
                out->rip_disp = (code + moff + 4 < code + out->size)
                    ? *(const int32_t *)(code + moff + 1) : 0;
                return out->type;
            }
        }
        out->size = (uint8_t)rh_x64_insn_len(code);
        out->output_size = out->size;
        out->type = RH_X64_OTHER;
        return out->type;
    }
    if (op >= 0x70 && op <= 0x7F) {
        out->type = RH_X64_JCC;
        out->size = (uint8_t)(prefix_len + 2);
        out->output_size = 6;
        out->rel_offset = (int32_t)(prefix_len + 1);
        return out->type;
    }
    if (op == 0xC3) {
        out->type = RH_X64_RET;
        out->size = (uint8_t)(prefix_len + 1);
        out->output_size = 1;
        return out->type;
    }
    if (op == 0xC2) {
        out->type = RH_X64_RET;
        out->size = (uint8_t)(prefix_len + 3);
        out->output_size = 3;
        return out->type;
    }

    /* 1-byte opcode with ModRM: check for RIP-relative */
    {
        int moff = find_modrm(code);
        if (moff >= 0 && code + moff < end) {
            uint8_t modrm = code[moff];
            uint8_t mod = (modrm >> 6) & 3;
            uint8_t rm  = modrm & 7;
            if (mod == 0 && rm == 5) {
                out->type = RH_X64_RIP_REL;
                out->size = (uint8_t)rh_x64_insn_len(code);
                out->output_size = 0;
                out->rip_disp = (code + moff + 4 < code + out->size)
                    ? *(const int32_t *)(code + moff + 1) : 0;
                return out->type;
            }
        }
    }

other:
    out->size = (uint8_t)rh_x64_insn_len(code);
    out->output_size = out->size;
    out->rel_offset = 0;
    out->rip_disp = 0;
    out->type = RH_X64_OTHER;
    return out->type;
}

size_t rh_x64_rewrite_size(rh_x64_insn_type_t type)
{
    switch (type) {
    case RH_X64_JMP_REL:  return 5;
    case RH_X64_CALL_REL: return 5;
    case RH_X64_JCC:      return 6;
    case RH_X64_CET:      return 0;
    case RH_X64_RET:
    case RH_X64_RIP_REL:
    case RH_X64_OTHER:
    default:
        return 0;
    }
}

int64_t rh_x64_decode_branch_target(uintptr_t pc, const uint8_t *insn, size_t size)
{
    if (size < 2) return 0;
    const uint8_t *end = insn + 16;
    int rex = 0;
    const uint8_t *p = skip_prefixes(insn, end, &rex);
    int off = (int)(p - insn);
    uint8_t op = *p++;

    if (op == 0xE8 || op == 0xE9) {
        if (size < (size_t)(off + 5)) return 0;
        int32_t rel = (int32_t)(*(const uint32_t *)(p));
        return (int64_t)((int64_t)(int32_t)(pc + off + 4) + rel);
    }
    if (op == 0xEB) {
        int32_t rel = (int32_t)(*(const int8_t *)(p));
        return (int64_t)((int64_t)(int32_t)(pc + off + 1) + rel);
    }
    if (op == 0x0F && (p[0] >= 0x80 && p[0] <= 0x8F)) {
        if (size < (size_t)(off + 6)) return 0;
        int32_t rel = (int32_t)(*(const uint32_t *)(p + 1));
        return (int64_t)((int64_t)(int32_t)(pc + off + 2 + 4) + rel);
    }
    if (op >= 0x70 && op <= 0x7F) {
        int32_t rel = (int32_t)(*(const int8_t *)(p));
        return (int64_t)((int64_t)(int32_t)(pc + off + 1) + rel);
    }
    return 0;
}

/*
 * Pick a scratch register that does NOT conflict with the operand register
 * of a RIP-relative instruction.
 * modrm bits[5:3] give the reg/opcode field (extended by REX.R).
 * We also check the REX byte (if present just before the opcode) for REX.R (bit 2).
 */
static int rip_scratch_reg(const uint8_t *code, int modrm_off)
{
    uint8_t modrm = code[modrm_off];
    int reg_field = (modrm >> 3) & 7;

    /* find REX.R: the REX byte is the byte just before the last opcode byte.
     * The last opcode byte is at modrm_off - 1.
     * REX (if present) is at modrm_off - 2 for 1-byte opcodes,
     * at modrm_off - 3 for 2-byte (0F xx), at modrm_off - 4 for 3-byte (0F 38/3A xx). */
    int olen = opcode_len(code);
    int rex_off = modrm_off - olen - 1;
    int rex_r = 0;
    if (rex_off >= 0) {
        uint8_t maybe_rex = code[rex_off];
        if (maybe_rex >= 0x40 && maybe_rex <= 0x4F) {
            rex_r = (maybe_rex >> 2) & 1;
        }
    }

    int dest_reg = reg_field | (rex_r << 3);

    /* Try R11 first (reg 11). If dest is 11, use R10. */
    if (dest_reg == 11) return 10;
    return 11;
}

size_t rh_x64_rewrite(uint8_t *output, const uint8_t *input, size_t input_size,
                     uintptr_t input_pc, uintptr_t output_pc)
{
    const uint8_t *end_buf = input + 16;
    int rex = 0;
    const uint8_t *p = skip_prefixes(input, end_buf, &rex);
    size_t prefix_len = (size_t)(p - input);

    if (p >= end_buf) { memcpy(output, input, 1); return 1; }

    uint8_t op = *p++;

    {
        const uint8_t *cs = input;
        int has_f3 = 0;
        while (cs < p - 1) { if (*cs == 0xF3) { has_f3 = 1; break; } cs++; }
        (void)cs;
        if (has_f3 && op == 0x0F && p + 2 < end_buf && p[0] == 0x1E && p[1] == 0xFA) {
            memcpy(output, input, input_size);
            return input_size;
        }
    }

    if (op == 0xE8) {
        int32_t rel = *(const int32_t *)p;
        if (rel == 0) {
            output[0] = 0x68;
            *(uint32_t *)(output + 1) = (uint32_t)(output_pc + 5);
            return 5;
        } else {
            int32_t target = (int32_t)((int32_t)input_pc + (int32_t)(prefix_len + 5) + rel);
            int32_t new_rel = target - (int32_t)((int32_t)output_pc + (int32_t)(prefix_len + 5));
            memcpy(output, input, prefix_len);
            output[prefix_len] = 0xE8;
            *(uint32_t *)(output + prefix_len + 1) = (uint32_t)new_rel;
        }
        return prefix_len + 5;
    }

    if (op == 0xE9) {
        int32_t rel = *(const int32_t *)p;
        int32_t target = (int32_t)((int32_t)input_pc + (int32_t)(prefix_len + 5) + rel);
        int32_t new_rel = target - (int32_t)((int32_t)output_pc + (int32_t)(prefix_len + 5));
        memcpy(output, input, prefix_len);
        output[prefix_len] = 0xE9;
        *(uint32_t *)(output + prefix_len + 1) = (uint32_t)new_rel;
        return prefix_len + 5;
    }

    if (op == 0xEB) {
        int32_t rel = (int32_t)(*(const int8_t *)p);
        int32_t target = (int32_t)((int32_t)input_pc + (int32_t)(prefix_len + 2) + rel);
        int32_t new_rel = target - (int32_t)((int32_t)output_pc + (int32_t)(prefix_len + 5));
        memcpy(output, input, prefix_len);
        output[prefix_len] = 0xE9;
        *(uint32_t *)(output + prefix_len + 1) = (uint32_t)new_rel;
        return prefix_len + 5;
    }

    if (op == 0x0F && (p[0] >= 0x80 && p[0] <= 0x8F)) {
        uint8_t cc = p[0];
        int32_t rel = *(const int32_t *)(p + 1);
        int32_t target = (int32_t)((int32_t)input_pc + (int32_t)(prefix_len + 6) + rel);
        int32_t new_rel = target - (int32_t)((int32_t)output_pc + (int32_t)(prefix_len + 6));
        memcpy(output, input, prefix_len);
        output[prefix_len] = 0x0F;
        output[prefix_len + 1] = cc;
        *(uint32_t *)(output + prefix_len + 2) = (uint32_t)new_rel;
        return prefix_len + 6;
    }

    if (op >= 0x70 && op <= 0x7F) {
        int32_t rel = (int32_t)(*(const int8_t *)p);
        int32_t target = (int32_t)((int32_t)input_pc + (int32_t)(prefix_len + 2) + rel);
        int32_t new_rel = target - (int32_t)((int32_t)output_pc + (int32_t)(prefix_len + 6));
        memcpy(output, input, prefix_len);
        output[prefix_len] = 0x0F;
        output[prefix_len + 1] = 0x80 | (op & 0x0F);
        *(uint32_t *)(output + prefix_len + 2) = (uint32_t)new_rel;
        return prefix_len + 6;
    }

    /* RIP-relative addressing */
    {
        int moff = find_modrm(input);
        if (moff >= 0) {
            uint8_t modrm = input[moff];
            uint8_t mod = (modrm >> 6) & 3;
            uint8_t rm  = modrm & 7;
            if (mod == 0 && rm == 5) {
                int32_t old_disp = *(const int32_t *)(input + moff + 1);
                intptr_t orig_ea = (intptr_t)input_pc + (intptr_t)input_size + old_disp;

                /* Try Dobby-style near code block allocation */
                size_t clone_size = input_size + 14;
                uintptr_t near_block = rh_trampo_alloc_near_3tier(
                    rh_trampo_get_global(), clone_size,
                    orig_ea - 0x7FFFFFFFLL, orig_ea + 0x7FFFFFFFLL);

                if (near_block) {
                    /* Write cloned instruction with recalculated displacement */
                    uint8_t *clone = (uint8_t *)near_block;
                    memcpy(clone, input, input_size);
                    int32_t new_clone_disp = (int32_t)(orig_ea - (intptr_t)(near_block + input_size));
                    *(int32_t *)(clone + moff + 1) = new_clone_disp;

                    /* Write back-jump in near block: JMP [RIP+0]; .quad resume_addr */
                    uintptr_t resume_addr = output_pc + 14;
                    clone[input_size] = 0xFF;
                    clone[input_size + 1] = 0x25;
                    *(int32_t *)(clone + input_size + 2) = 0;
                    *(uintptr_t *)(clone + input_size + 6) = resume_addr;

                    /* Write JMP [RIP+0]; .quad near_block in relocated output */
                    output[0] = 0xFF;
                    output[1] = 0x25;
                    *(int32_t *)(output + 2) = 0;
                    *(uintptr_t *)(output + 6) = near_block;

                    return 14;
                }

                /* Fallback: PUSH/LEA/POP pattern */
                int scr = rip_scratch_reg(input, moff);

                size_t out_off = 0;

                if (scr == 0) {
                    output[out_off++] = 0x50;
                } else {
                    output[out_off++] = 0x41;
                    output[out_off++] = 0x50 | (scr & 7);
                }

                intptr_t lea_rip = (intptr_t)output_pc + (intptr_t)out_off + 7;
                int32_t new_disp = (int32_t)(orig_ea - lea_rip);

                if (scr >= 8) {
                    output[out_off++] = 0x4C;
                    output[out_off++] = 0x8D;
                    output[out_off++] = 0x05 | ((scr & 7) << 3);
                } else {
                    output[out_off++] = 0x48;
                    output[out_off++] = 0x8D;
                    output[out_off++] = 0x05 | ((scr & 7) << 3);
                }
                *(int32_t *)(output + out_off) = new_disp;
                out_off += 4;

                memcpy(output + out_off, input, (size_t)moff);
                {
                    int olen = opcode_len(input);
                    int rex_pos = (int)out_off + moff - olen - 1;
                    if (rex_pos >= 0) {
                        uint8_t *rex_ptr = output + rex_pos;
                        if (*rex_ptr >= 0x40 && *rex_ptr <= 0x4F) {
                            if (scr >= 8) {
                                *rex_ptr |= 0x01;
                            }
                        } else if (scr >= 8) {
                            size_t op_off = (size_t)moff - (size_t)olen;
                            memmove(output + out_off + 1, output + out_off, (size_t)moff - op_off);
                            output[out_off + op_off] = 0x41;
                            out_off += 1;
                        }
                    }
                }
                out_off += (size_t)moff;

                {
                    uint8_t new_modrm = (modrm & 0x38) | ((uint8_t)scr & 7);
                    output[out_off++] = new_modrm;
                }

                size_t tail_start = (size_t)(moff + 1 + 4);
                size_t tail_len = input_size - tail_start;
                if (tail_len > 0) {
                    memcpy(output + out_off, input + tail_start, tail_len);
                    out_off += tail_len;
                }

                {
                    int reg_field = (modrm >> 3) & 7;
                    int olen = opcode_len(input);
                    int rex_off = moff - olen - 1;
                    int rex_r = 0;
                    if (rex_off >= 0) {
                        uint8_t maybe_rex = input[rex_off];
                        if (maybe_rex >= 0x40 && maybe_rex <= 0x4F) rex_r = (maybe_rex >> 2) & 1;
                    }
                    int dest_reg = reg_field | (rex_r << 3);

                    if (dest_reg == scr) {
                        if (scr >= 8) {
                            output[out_off++] = 0x49;
                        } else {
                            output[out_off++] = 0x48;
                        }
                        output[out_off++] = 0x83;
                        output[out_off++] = 0xC4;
                        output[out_off++] = 0x08;
                    } else {
                        if (scr == 0) {
                            output[out_off++] = 0x58;
                        } else {
                            output[out_off++] = 0x41;
                            output[out_off++] = 0x58 | (scr & 7);
                        }
                    }
                }

                return out_off;
            }
        }
    }


    {
        size_t bytes = rh_x64_insn_len(input);
        memcpy(output, input, bytes);
        return bytes;
    }
}

size_t rh_x64_relocate_instructions(void *input, void *output, size_t input_size,
                                     uintptr_t input_pc, uintptr_t output_pc)
{
    uint8_t *out = (uint8_t *)output;
    const uint8_t *in = (const uint8_t *)input;
    size_t in_off = 0;
    size_t out_off = 0;

    while (in_off < input_size) {
        rh_x64_insn_t insn;
        rh_x64_classify(in + in_off, &insn);
        if (insn.size == 0) { in_off++; continue; }
        if (in_off + insn.size > input_size) break;

        if (insn.type == RH_X64_CET) {
            memcpy(out + out_off, in + in_off, insn.size);
            out_off += insn.size;
            in_off += insn.size;
            continue;
        }

        if (insn.type == RH_X64_RIP_REL) {
            size_t n = rh_x64_rewrite(out + out_off, in + in_off, insn.size,
                                       input_pc + in_off, output_pc + out_off);
            out_off += n;
            in_off += insn.size;
            continue;
        }

        size_t write_size = rh_x64_rewrite(out + out_off, in + in_off, insn.size,
                                            input_pc + in_off, output_pc + out_off);
        out_off += write_size;
        in_off += insn.size;
    }

    /* append JMP back to input_pc + input_size */
    {
        int64_t jmp_rel = (int64_t)(input_pc + input_size) - (int64_t)(output_pc + out_off) - 5;
        if (jmp_rel >= -0x7FFFFFFFLL - 1 && jmp_rel <= 0x7FFFFFFFLL) {
            out[out_off] = 0xE9;
            *(int32_t *)(out + out_off + 1) = (int32_t)jmp_rel;
            out_off += 5;
        } else {
            out[out_off] = 0xFF;
            out[out_off + 1] = 0x25;
            *(int32_t *)(out + out_off + 2) = 0;
            *(uintptr_t *)(out + out_off + 6) = input_pc + input_size;
            out_off += 14;
        }
    }

    return out_off;
}
