#include "rh_lr_rewrite.h"
#include "rh_util.h"

rh_lr_pattern_t rh_lr_detect_pattern(uintptr_t target_addr, size_t backup_len) {
    bool mov_lr_seen = false;
    bool push_lr_seen = false;
    uintptr_t offset = 0;

    while (offset < backup_len) {
        bool is_t32 = rh_util_is_thumb32(target_addr + offset);

        if (is_t32) {
            uint16_t high = *(uint16_t *)(target_addr + offset);
            uint16_t low = *(uint16_t *)(target_addr + offset + 2);
            uint32_t inst = ((uint32_t)high << 16) | low;

            // MOV Rd, LR (T32): high = 0xEA4F (MOV) or 0xEA5F (MOVS), bits 3:0 = LR(14)
            if ((high == 0xEA4F || high == 0xEA5F) && (low & 0xF) == 0xE) {
                if (mov_lr_seen) {
                    // MOV already seen but without BL/BX after — reset, keep push flag
                    push_lr_seen = false;
                }
                mov_lr_seen = true;
            }
            // BL/BLX (T32)
            else if ((inst & 0xF800D000) == 0xF000C000 ||
                     (inst & 0xF800D000) == 0xF000D000) {
                if (mov_lr_seen)
                    return push_lr_seen ? RH_LR_PUSH_MOV_BL : RH_LR_MOV_B;
                mov_lr_seen = false;
            }
            // B T4 (T32 unconditional branch)
            else if ((inst & 0xF800D000) == 0xF0009000) {
                if (mov_lr_seen)
                    return push_lr_seen ? RH_LR_PUSH_MOV_BL : RH_LR_MOV_B;
                mov_lr_seen = false;
            }
            // B T3 (T32 conditional branch)
            else if ((inst & 0xF800D000) == 0xF0008000) {
                if (mov_lr_seen) return RH_LR_MOV_B;
                mov_lr_seen = false;
            }
            // PUSH.W (T32) — register list in low, LR = bit 14
            else if (high == 0xE92D && (low & (1 << 14))) {
                push_lr_seen = true;
                mov_lr_seen = false;
            }
            // Other T32 instruction
            else {
                mov_lr_seen = false;
            }

            offset += 4;
        } else {
            uint16_t inst = *(uint16_t *)(target_addr + offset);

            // MOV Rd, LR (T16): 0100 0110 D 1110 xxx
            if ((inst & 0xFF78) == 0x4670) {
                if (mov_lr_seen) push_lr_seen = false;
                mov_lr_seen = true;
            }
            // B T2 (T16 unconditional)
            else if ((inst & 0xF800) == 0xE000) {
                if (mov_lr_seen) return push_lr_seen ? RH_LR_PUSH_MOV_BL : RH_LR_MOV_B;
                mov_lr_seen = false;
            }
            // B T1 (T16 conditional)
            else if (((inst & 0xF000) == 0xD000) &&
                     ((inst & 0x0F00) != 0x0F00) &&
                     ((inst & 0x0F00) != 0x0E00)) {
                if (mov_lr_seen) return RH_LR_MOV_B;
                mov_lr_seen = false;
            }
            // BX Rm or BLX Rm (T16): 0100 0111 xxxx x000
            else if ((inst & 0xF000) == 0x4000 && (inst & 0x0007) == 0x0000 &&
                     (inst & 0xFF00) == 0x4700) {
                if (mov_lr_seen) return RH_LR_MOV_BX;
                mov_lr_seen = false;
            }
            // PUSH (T16): 1011 010x xxxx xxxx
            else if ((inst & 0xFE00) == 0xB400) {
                if (inst & (1 << 14)) {
                    push_lr_seen = true;
                    mov_lr_seen = false;
                } else {
                    mov_lr_seen = false;
                }
            }
            // Other T16 instruction
            else {
                mov_lr_seen = false;
            }

            offset += 2;
        }
    }

    (void)push_lr_seen;
    return RH_LR_NONE;
}

size_t rh_lr_emit_translate_call(uint16_t *buf, uint16_t orig_inst, rh_lr_pattern_t pattern, uintptr_t translate_fn) {
    (void)pattern;

    uint16_t D = (orig_inst >> 7) & 1;
    uint16_t Rd_low = orig_inst & 7;
    uint16_t Rd = (D << 3) | Rd_low;

    buf[0] = 0xE92D;  // PUSH.W {R0-R3, LR}
    buf[1] = 0x400F;
    buf[2] = 0x4670;  // MOV R0, LR (T16)

    buf[3] = 0xF8DF;  // LDR.W R1, [PC, #12]
    buf[4] = 0x100C;

    buf[5] = 0x4788;  // BLX R1 (T16)

    buf[6] = (uint16_t)(0x4600 | (D << 7) | Rd);  // MOV Rd, R0 (T16)

    buf[7] = 0xE8FD;  // POP.W {R0-R3, PC}
    buf[8] = 0x800F;

    buf[9]  = (uint16_t)(translate_fn & 0xFFFF);
    buf[10] = (uint16_t)(translate_fn >> 16);

    return 22;
}
