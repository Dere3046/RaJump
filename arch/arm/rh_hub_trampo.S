    .syntax unified
    .text
    .align 4
    .global rh_hub_trampo_entry
    .global rh_hub_trampo_data
    .thumb
    .thumb_func
rh_hub_trampo_entry:
    bx lr
    .align 4
rh_hub_trampo_data:
    .word 0
    .word 0
