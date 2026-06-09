#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
    void *input;
    void *output;
    size_t input_size;
    size_t output_size;
    uintptr_t input_pc;
    uintptr_t output_pc;
    void *data_pool;
    size_t data_pool_offset;
} rh_relocator_t;

int rh_relocator_relocate(rh_relocator_t *ctx);
