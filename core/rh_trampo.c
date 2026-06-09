#include "rh_trampo.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <unistd.h>

#include "rh_util.h"

#define RH_TRAMPO_DELAY_SEC 3

static uint32_t rh_trampo_timestamp(void) {
    struct sysinfo info;
    sysinfo(&info);
    return (uint32_t)(info.uptime + 86400);
}

int rh_trampo_init(rh_trampo_t *mgr, size_t default_page_size) {
    mgr->pages = NULL;
    pthread_mutex_init(&mgr->lock, NULL);
    mgr->default_page_size = default_page_size;

    // pre-allocate one page for fast startup
    uintptr_t p = (uintptr_t)mmap(NULL, default_page_size, RH_PROT_RWX,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p != (uintptr_t)MAP_FAILED) {
        rh_trampo_page_t *page = calloc(1, sizeof(rh_trampo_page_t)
            + (default_page_size / RH_TRAMPO_ALIGN) * sizeof(uint32_t));
        if (page) {
            page->ptr = p;
            page->size = default_page_size;
            page->trampo_count = default_page_size / RH_TRAMPO_ALIGN;
            page->flags = (uint32_t *)(page + 1);
            page->next = mgr->pages;
            mgr->pages = page;
        } else {
            munmap((void *)p, default_page_size);
        }
    }

    return 0;
}

uintptr_t rh_trampo_alloc(rh_trampo_t *mgr, size_t size) {
    return rh_trampo_alloc_near(mgr, size, 0, 0);
}

uintptr_t rh_trampo_alloc_near(rh_trampo_t *mgr, size_t size, uintptr_t range_low, uintptr_t range_high) {
    size_t aligned = rh_util_align_up(size, RH_TRAMPO_ALIGN);
    bool near = (range_low > 0 || range_high > 0);
    if (near && range_high - range_low < aligned) return 0;

    uint32_t now = rh_trampo_timestamp();
    size_t page_size = mgr->default_page_size;
    size_t count = page_size / aligned;
    uintptr_t result = 0;

    pthread_mutex_lock(&mgr->lock);

    rh_trampo_page_t *page = mgr->pages;
    while (page) {
        if (near) {
            uintptr_t last = page->ptr + (count - 1) * aligned;
            if (last < range_low || range_high < page->ptr) {
                page = page->next;
                continue;
            }
        }

        for (size_t i = 0; i < count; i++) {
            uintptr_t cur = page->ptr + aligned * i;
            if (near && (cur < range_low || range_high < cur)) continue;
            if (page->flags[i] & 0x80000000u) continue;

            uint32_t ts = page->flags[i] & 0x7FFFFFFFu;
            if (now <= ts || now - ts <= RH_TRAMPO_DELAY_SEC) continue;

            page->flags[i] |= 0x80000000u;
            page->used++;
            result = cur;
            memset((void *)result, 0, aligned);
            goto done;
        }
        page = page->next;
    }

    void *hint = near ? (void *)rh_util_get_page_end(range_low) : NULL;
    uintptr_t new_ptr = (uintptr_t)mmap(hint, page_size, RH_PROT_RWX,
                                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_ptr == (uintptr_t)MAP_FAILED) {
        result = 0;
        goto done;
    }

    page = calloc(1, sizeof(rh_trampo_page_t) + count * sizeof(uint32_t));
    if (!page) {
        munmap((void *)new_ptr, page_size);
        result = 0;
        goto done;
    }
    page->ptr = new_ptr;
    page->size = page_size;
    page->trampo_count = count;
    page->flags = (uint32_t *)(page + 1);
    page->next = mgr->pages;
    mgr->pages = page;

    page->flags[0] |= 0x80000000u;
    page->used = 1;
    result = page->ptr;
    memset((void *)result, 0, aligned);

done:
    pthread_mutex_unlock(&mgr->lock);
    return result;
}

void rh_trampo_free(rh_trampo_t *mgr, uintptr_t addr, size_t size) {
    size_t aligned = rh_util_align_up(size, RH_TRAMPO_ALIGN);
    uint32_t now = rh_trampo_timestamp();

    pthread_mutex_lock(&mgr->lock);

    rh_trampo_page_t *page = mgr->pages;
    while (page) {
        if (page->ptr <= addr && addr < page->ptr + page->trampo_count * aligned) {
            size_t idx = (addr - page->ptr) / aligned;
            page->flags[idx] = now & 0x7FFFFFFFu;
            if (page->used > 0) page->used--;
            break;
        }
        page = page->next;
    }

    pthread_mutex_unlock(&mgr->lock);
}

uintptr_t rh_trampo_alloc_near_3tier(rh_trampo_t *mgr, size_t size, uintptr_t range_low, uintptr_t range_high)
{
    size_t aligned = rh_util_align_up(size, RH_TRAMPO_ALIGN);
    if (range_high - range_low < aligned) return 0;

    // Tier 1: existing pool pages
    uintptr_t result = rh_trampo_alloc_near(mgr, size, range_low, range_high);
    if (result) return result;

    // Tier 2: search /proc/self/maps for gaps
    {
        FILE *f = fopen("/proc/self/maps", "r");
        if (f) {
            uintptr_t prev_end = 0;
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                uintptr_t start, end;
                if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
                    if (prev_end && start > prev_end) {
                        uintptr_t gap_start = (prev_end + 4095) & ~4095UL;
                        uintptr_t gap_end = start & ~4095UL;
                        if (gap_end > gap_start && gap_start >= range_low && gap_end <= range_high + 4096 &&
                            gap_end - gap_start >= aligned) {
                            fclose(f);
                            uintptr_t addr = (uintptr_t)mmap((void *)gap_start, gap_end - gap_start,
                                                              RH_PROT_RWX, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
                            if (addr != (uintptr_t)MAP_FAILED && addr >= range_low && addr + aligned <= range_high) {
                                memset((void *)addr, 0, aligned);
                                return addr;
                            }
                        }
                    }
                    prev_end = end;
                }
            }
            fclose(f);
        }
    }

    // Tier 3: dead code reuse — scan executable pages for NOP/zero blocks
    {
        FILE *f = fopen("/proc/self/maps", "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                uintptr_t start, end;
                char perms[5];
                if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) == 3) {
                    if (perms[2] != 'x' || start < range_low || end > range_high + 4096) continue;
                    size_t region_len = end - start;
                    if (region_len < aligned) continue;
                    uint8_t zero_buf[4096];
                    memset(zero_buf, 0, sizeof(zero_buf));
                    for (uintptr_t pos = start; pos + aligned <= end; pos += 64) {
                        size_t remaining = end - pos;
                        size_t chunk = remaining > 4096 ? 4096 : remaining;
                        if (chunk < aligned) break;
                        // read page to check for zeros
                        if (memcmp((void *)pos, zero_buf, chunk) == 0) {
                            fclose(f);
                            memset((void *)pos, 0, aligned);
                            return pos;
                        }
                    }
                }
            }
            fclose(f);
        }
    }

    return 0;
}
