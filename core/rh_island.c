#include "rh_island.h"

#include <stdint.h>
#include <sys/mman.h>

#include "rh_linker.h"
#include "rh_trampo.h"
#include "rh_util.h"

static rh_trampo_t rh_island_trampo;

int rh_island_init(void) {
    rh_trampo_init(&rh_island_trampo, PAGE_SIZE);
    return 0;
}

int rh_island_alloc(rh_island_t *island, uintptr_t target, size_t size, size_t range) {
    island->size = size;

    uintptr_t range_low = (range > 0) ? (target > range ? target - range : 0) : 0;
    uintptr_t range_high = target + range;

    uintptr_t addr = rh_trampo_alloc_near_3tier(&rh_island_trampo, size, range_low, range_high);
    if (addr) { island->addr = addr; return 0; }

    addr = rh_trampo_alloc(&rh_island_trampo, size);
    if (addr) { island->addr = addr; return 0; }

    return -1;
}

int rh_island_alloc_ex(rh_island_t *island, uintptr_t target, size_t size,
                        size_t range, const char *lib_filter) {
    island->size = size;

    uintptr_t range_low = (range > 0) ? (target > range ? target - range : 0) : 0;
    uintptr_t range_high = target + range;

    uintptr_t addr = rh_trampo_alloc_near(&rh_island_trampo, size, range_low, range_high);
    if (addr) { island->addr = addr; return 0; }

    rh_gap_t *gaps = NULL;
    size_t gap_count = 0;
    if (lib_filter)
        rh_linker_scan_gaps(lib_filter, &gaps, &gap_count);
    else
        rh_linker_scan_gaps(NULL, &gaps, &gap_count);

    if (gaps && gap_count > 0) {
        size_t aligned = rh_util_align_up(size, RH_INSN_ALIGN);
        for (size_t i = 0; i < gap_count; i++) {
            uintptr_t gap_start = (uintptr_t)gaps[i].start;
            uintptr_t gap_end = (uintptr_t)gaps[i].end;
            if (range > 0) {
                if (gap_end < range_low || gap_start > range_high) continue;
                if (gap_start < range_low) gap_start = range_low;
                if (gap_end > range_high) gap_end = range_high;
            }
            if (gap_end - gap_start >= aligned) {
                addr = rh_util_align_up(gap_start, RH_INSN_ALIGN);
                void *p = mmap((void *)addr, size, RH_PROT_RWX,
                                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
                if (p != MAP_FAILED) { island->addr = addr; return 0; }
            }
        }
    }

    addr = rh_trampo_alloc(&rh_island_trampo, size);
    if (addr) { island->addr = addr; return 0; }

    return -1;
}

void rh_island_free(rh_island_t *island) {
    if (island->addr) {
        rh_trampo_free(&rh_island_trampo, island->addr, island->size);
        island->addr = 0;
    }
}
