/*
 * Copyright (C) 2019-2024 jmpews@gmail.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Adapted for RaHook:
 * Copyright (C) 2026 Dere3046
 */

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum {
    RH_EXECUTE_ARM   = 0,
    RH_EXECUTE_THUMB = 1,
} rh_execute_state_t;

#define RH_INTERWORK_MAX_SWITCHES 32

typedef struct {
    rh_execute_state_t current_state;

    struct {
        uintptr_t          addr;
        rh_execute_state_t new_state;
    } switches[RH_INTERWORK_MAX_SWITCHES];
    int switch_count;
} rh_interwork_t;

static inline void
rh_interwork_init(rh_interwork_t *tracker, uintptr_t target_addr) {
    memset(tracker, 0, sizeof(*tracker));
    tracker->current_state = (target_addr & 1) ? RH_EXECUTE_THUMB : RH_EXECUTE_ARM;
}

static inline bool
rh_interwork_record(rh_interwork_t *tracker, uintptr_t addr, rh_execute_state_t state) {
    if (tracker->switch_count >= RH_INTERWORK_MAX_SWITCHES)
        return false;

    tracker->switches[tracker->switch_count].addr      = addr;
    tracker->switches[tracker->switch_count].new_state = state;
    tracker->switch_count++;
    return true;
}

static inline rh_execute_state_t
rh_interwork_get_state(const rh_interwork_t *tracker, uintptr_t addr) {
    for (int i = 0; i < tracker->switch_count; i++) {
        if (tracker->switches[i].addr == addr)
            return tracker->switches[i].new_state;
    }
    return tracker->current_state;
}

static inline bool
rh_interwork_is_switch_point(const rh_interwork_t *tracker, uintptr_t addr) {
    for (int i = 0; i < tracker->switch_count; i++) {
        if (tracker->switches[i].addr == addr)
            return true;
    }
    return false;
}

static inline rh_execute_state_t
rh_interwork_check(rh_interwork_t *tracker, uintptr_t cur_pc, uint32_t insn, bool is_thumb) {
    if (is_thumb) {
        uint16_t insn16 = insn & 0xFFFF;

        if ((insn16 & 0xFF87) == 0x4700) {
            uint8_t rm = (insn16 >> 3) & 0xF;
            bool    L  = (insn16 >> 7) & 1;

            if (rm == 15) {
                uintptr_t target = (cur_pc + 4) & ~(uintptr_t)1;

                if (L) {
                    rh_interwork_record(tracker, target, RH_EXECUTE_ARM);
                } else {
                    rh_interwork_record(tracker, target, RH_EXECUTE_ARM);
                    return RH_EXECUTE_ARM;
                }
            }
        }

        uint16_t insn32_top = insn16 & 0xF800;
        if (insn32_top == 0xF000 || insn32_top == 0xE800 || insn32_top == 0xF800) {
            uint16_t insn2 = (insn >> 16) & 0xFFFF;
            if ((insn2 & 0x8000) == 0x8000) {
                uint32_t op3 = (insn2 >> 12) & 0x7;

                if ((op3 & 0x5) == 0x4) {
                    uintptr_t target = (cur_pc + 4) & ~(uintptr_t)1;
                    rh_interwork_record(tracker, target, RH_EXECUTE_ARM);
                }
            }
        }
    } else {
        if ((insn & 0x0FFFFFF0) == 0x012FFF10) {
            uintptr_t target = cur_pc + 8;
            rh_interwork_record(tracker, target, RH_EXECUTE_THUMB);
            return RH_EXECUTE_THUMB;
        }

        if ((insn & 0x0FFFFFF0) == 0x012FFF30) {
            uintptr_t target = cur_pc + 8;
            rh_interwork_record(tracker, target, RH_EXECUTE_THUMB);
            return RH_EXECUTE_THUMB;
        }

        if ((insn & 0xFF000000) == 0xFB000000) {
            uintptr_t target = cur_pc + 8;
            rh_interwork_record(tracker, target, RH_EXECUTE_THUMB);
        }

        if ((insn & 0x0F000000) == 0x0A000000) {
            if ((insn >> 24) & 1) {
                uintptr_t target = cur_pc + 8;
                rh_interwork_record(tracker, target, RH_EXECUTE_THUMB);
            }
        }
    }

    return tracker->current_state;
}
