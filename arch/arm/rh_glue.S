/*
 * rh_glue.S — ARM32 hub trampoline glue
 *
 * Copyright (C) 2026 dere3046
 *
 * Based on ShadowHook, MIT license.
 */

.globl rh_hub_trampo_entry
.globl rh_hub_trampo_data

.text
.arm
.balign 64

rh_hub_trampo_entry:
    push  {r0 - r3, lr}
    vpush {d0 - d7}

    adr   r0, rh_hub_trampo_data
    ldr   r1, [r0, #4]
    ldr   r0, [r0]
    blx   r1
    mov   ip, r0

    vpop  {d0 - d7}
    pop   {r0 - r3, lr}

    bx    ip

rh_hub_trampo_data:
    .word 0
    .word 0
