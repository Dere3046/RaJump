    .text
    .align 4
    .global rh_hub_trampo_entry
    .global rh_hub_trampo_data
rh_hub_trampo_entry:
    ret
rh_hub_trampo_data:
    .quad 0
    .quad 0
