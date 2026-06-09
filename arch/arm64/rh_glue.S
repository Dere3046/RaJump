/*
 * rh_glue.S — ARM64 hub trampoline glue
 *
 * Copyright (C) 2026 dere3046
 *
 * Based on ShadowHook, MIT license.
 */

.globl rh_hub_trampo_entry
.globl rh_hub_trampo_data

.text
.balign 64

rh_hub_trampo_entry:
    stp   x29, x30, [sp, #-0xe0]!
    stp   x0,  x1,  [sp, #0x10]
    stp   x2,  x3,  [sp, #0x20]
    stp   x4,  x5,  [sp, #0x30]
    stp   x6,  x7,  [sp, #0x40]
    stp   x8,  lr,  [sp, #0x50]
    stp   q0,  q1,  [sp, #0x60]
    stp   q2,  q3,  [sp, #0x80]
    stp   q4,  q5,  [sp, #0xa0]
    stp   q6,  q7,  [sp, #0xc0]

    adr   x16, rh_hub_trampo_data
    ldp   x0,  x17, [x16]
    blr   x17
    mov   x17, x0

    ldp   q6,  q7,  [sp, #0xc0]
    ldp   q4,  q5,  [sp, #0xa0]
    ldp   q2,  q3,  [sp, #0x80]
    ldp   q0,  q1,  [sp, #0x60]
    ldp   x8,  lr,  [sp, #0x50]
    ldp   x6,  x7,  [sp, #0x40]
    ldp   x4,  x5,  [sp, #0x30]
    ldp   x2,  x3,  [sp, #0x20]
    ldp   x0,  x1,  [sp, #0x10]
    ldp   x29, x30, [sp], #0xe0

    br    x17

rh_hub_trampo_data:
    .quad 0
    .quad 0
