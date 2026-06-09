#include "rh_lr_rewrite.h"

rh_lr_pattern_t rh_lr_detect_pattern(uintptr_t addr, size_t len) {
    if (len < 8) return RH_LR_NONE;

    uint32_t insn0 = *(uint32_t *)(addr + 0);
    uint32_t insn1 = *(uint32_t *)(addr + 4);

    if ((insn0 & 0xFFFF03E0) == 0xAA0003E0) {
        if ((insn1 & 0xFC000000) == 0x14000000 ||
            (insn1 & 0xFC000000) == 0x94000000) {
            return RH_LR_MOV_B;
        }
    }

    if (len >= 12) {
        uint32_t insn2 = *(uint32_t *)(addr + 8);

        if ((insn0 & 0xFFC00000) == 0xA9800000) {
            if (insn1 == 0x910003FD) {
                if ((insn2 & 0xFFFF03E0) == 0xAA0003E0) {
                    return RH_LR_STP_MOV_MOV_BL;
                }
            } else if ((insn1 & 0xFFFF03E0) == 0xAA0003E0) {
                return RH_LR_MOV_B;
            }
        }
    }

    return RH_LR_NONE;
}
