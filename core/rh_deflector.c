#include "rh_deflector.h"
#include "rh_island.h"
#include "rh_util.h"

#if defined(__aarch64__)
#define RH_DEFLECTOR_SIZE 16
#elif defined(__arm__)
#define RH_DEFLECTOR_SIZE 8
#elif defined(__x86_64__)
#define RH_DEFLECTOR_SIZE 14
#elif defined(__i386__)
#define RH_DEFLECTOR_SIZE 5
#endif

int rh_deflector_alloc(rh_deflector_t *deflector, uintptr_t target, size_t range)
{
    rh_island_t island;
    if (0 != rh_island_alloc(&island, target, RH_DEFLECTOR_SIZE, range))
        return -1;

    uint8_t *code = (uint8_t *)island.addr;

#if defined(__aarch64__)
    // LDR X16, [PC, #8]: imm19=2, Rt=X16(0x10)
    // BR X16: Rn=X16(0x10)
    uint32_t ldr_insn = 0x58000000 | (2UL << 5) | (16UL << 0);
    uint32_t br_insn  = 0xD61F0000 | (16UL << 5);
    *(uint32_t *)(code + 0) = ldr_insn;
    *(uint32_t *)(code + 4) = br_insn;
#elif defined(__arm__)
    code[0] = 0x04; code[1] = 0xF0; code[2] = 0x1F; code[3] = 0xE5;
#elif defined(__x86_64__)
    code[0] = 0xFF; code[1] = 0x25;
    *(int32_t *)(code + 2) = 0;
#elif defined(__i386__)
    code[0] = 0xE9;
#endif

    rh_util_cache_flush(island.addr, RH_DEFLECTOR_SIZE);

    deflector->addr = island.addr;
    deflector->size = island.size;
    return 0;
}

void rh_deflector_set_target(rh_deflector_t *deflector, uintptr_t proxy_addr)
{
    uint8_t *code = (uint8_t *)deflector->addr;

#if defined(__aarch64__)
    *(uintptr_t *)(code + 8) = proxy_addr;
#elif defined(__arm__)
    *(uint32_t *)(code + 4) = (uint32_t)proxy_addr;
#elif defined(__x86_64__)
    *(uintptr_t *)(code + 6) = proxy_addr;
#elif defined(__i386__)
    int32_t rel = (int32_t)(proxy_addr - (deflector->addr + 5));
    *(int32_t *)(code + 1) = rel;
#endif

    rh_util_cache_flush(deflector->addr, deflector->size);
}

void rh_deflector_free(rh_deflector_t *deflector)
{
    if (!deflector->addr) return;
    rh_island_t island = { .addr = deflector->addr, .size = deflector->size };
    rh_island_free(&island);
    deflector->addr = 0;
    deflector->size = 0;
}
