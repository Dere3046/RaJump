// Copyright (c) 2024-2026 dere3046
//
// Portions derived from ShadowHook:
// Copyright (c) 2021-2025 ByteDance Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Copyright (c) 2021-2025 ByteDance Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// Created by Kelun Cai (caikelun@bytedance.com) on 2021-04-11.

#include "rh_recorder.h"

#include "rh_config.h"
#include <stdlib.h>
#include <string.h>

#ifdef RH_CONFIG_OPERATION_RECORDS

#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "rh_sig.h"
#include "rh_util.h"
#include "rahook.h"
#include "xdl.h"

#define RH_RECORDER_LIB_NAME_MAX 512
#define RH_RECORDER_SYM_NAME_MAX 1024

#define RH_RECORDER_STRINGS_BUF_EXPAND_STEP (1024 * 32)
#define RH_RECORDER_STRINGS_BUF_MAX         (1024 * 512)
#define RH_RECORDER_RECORDS_BUF_EXPAND_STEP (1024 * 32)
#define RH_RECORDER_RECORDS_BUF_MAX         (1024 * 512)
#define RH_RECORDER_OUTPUT_BUF_EXPAND_STEP  (1024 * 128)
#define RH_RECORDER_OUTPUT_BUF_MAX          (1024 * 1024)

static bool rh_recorder_recordable = false;

bool rh_recorder_get_recordable(void) {
  return rh_recorder_recordable;
}

void rh_recorder_set_recordable(bool recordable) {
  rh_recorder_recordable = recordable;
}

typedef struct {
  void *ptr;
  size_t cap;
  size_t sz;
  pthread_mutex_t lock;
} rh_recorder_buf_t;

static int rh_recorder_buf_append(rh_recorder_buf_t *buf, size_t step, size_t max, const void *header,
                                  size_t header_sz, const void *body, size_t body_sz) {
  size_t needs = (header_sz + (NULL != body ? body_sz : 0));
  if (needs > step) return -1;

  if (buf->cap - buf->sz < needs) {
    size_t new_cap = buf->cap + step;
    if (new_cap > max) return -1;
    void *new_ptr = realloc(buf->ptr, new_cap);
    if (NULL == new_ptr) return -1;
    buf->ptr = new_ptr;
    buf->cap = new_cap;
  }

  memcpy((void *)((uintptr_t)buf->ptr + buf->sz), header, header_sz);
  if (NULL != body) memcpy((void *)((uintptr_t)buf->ptr + buf->sz + header_sz), body, body_sz);
  buf->sz += needs;
  return 0;
}

static void rh_recorder_buf_free(rh_recorder_buf_t *buf) {
  if (NULL != buf->ptr) {
    free(buf->ptr);
    buf->ptr = NULL;
  }
}

static rh_recorder_buf_t rh_recorder_strings = {NULL, 0, 0, PTHREAD_MUTEX_INITIALIZER};
static rh_recorder_buf_t rh_recorder_records = {NULL, 0, 0, PTHREAD_MUTEX_INITIALIZER};
static bool rh_recorder_error = false;

typedef struct {
  uint16_t str_len;  // body length, in order to speed up the search
} __attribute__((packed)) rh_recorder_str_header_t;
// +body: string, including the terminating null byte ('\0')

typedef struct {
  uint64_t op : 8;
  uint64_t error_number : 8;
  uint64_t ts_ms : 48;
  uintptr_t stub;
  uint16_t caller_lib_name_idx;
  uint8_t backup_len;
  uint16_t lib_name_idx;
  uint16_t sym_name_idx;
  uintptr_t sym_addr;
  uintptr_t new_addr;
  uint32_t flags;
} __attribute__((packed)) rh_recorder_record_op_header_t;
// no body

typedef struct {
  uint64_t op : 8;
  uint64_t error_number : 8;
  uint64_t ts_ms : 48;
  uintptr_t stub;
  uint16_t caller_lib_name_idx;
} __attribute__((packed)) rh_recorder_record_unop_header_t;
// no body

static int rh_recorder_add_str(const char *str, size_t str_len, uint16_t *str_idx) {
  uint16_t idx = 0;
  bool ok = false;

  pthread_mutex_lock(&rh_recorder_strings.lock);

  // find in existing strings
  size_t i = 0;
  while (i < rh_recorder_strings.sz) {
    rh_recorder_str_header_t *header = (rh_recorder_str_header_t *)((uintptr_t)rh_recorder_strings.ptr + i);
    if (header->str_len == str_len) {
      void *tmp = (void *)((uintptr_t)rh_recorder_strings.ptr + i + sizeof(header->str_len));
      if (0 == memcmp(tmp, str, str_len)) {
        *str_idx = idx;
        ok = true;
        break;  // OK
      }
    }
    i += (sizeof(rh_recorder_str_header_t) + header->str_len + 1);
    idx++;
    if (idx == UINT16_MAX) break;  // failed
  }

  // insert a new string
  if (!ok && idx < UINT16_MAX) {
    // append new string
    rh_recorder_str_header_t header = {(uint16_t)str_len};
    if (0 == rh_recorder_buf_append(&rh_recorder_strings, RH_RECORDER_STRINGS_BUF_EXPAND_STEP,
                                    RH_RECORDER_STRINGS_BUF_MAX, &header, sizeof(header), str, str_len + 1)) {
      *str_idx = idx;
      ok = true;  // OK
    }
  }

  pthread_mutex_unlock(&rh_recorder_strings.lock);

  return ok ? 0 : -1;
}

static char *rh_recorder_find_str(uint16_t idx) {
  uint16_t cur_idx = 0;

  size_t i = 0;
  while (i < rh_recorder_strings.sz && cur_idx < idx) {
    rh_recorder_str_header_t *header = (rh_recorder_str_header_t *)((uintptr_t)rh_recorder_strings.ptr + i);
    i += (sizeof(rh_recorder_str_header_t) + header->str_len + 1);
    cur_idx++;
  }
  if (cur_idx != idx) return "error";

  rh_recorder_str_header_t *header = (rh_recorder_str_header_t *)((uintptr_t)rh_recorder_strings.ptr + i);
  return (char *)((uintptr_t)header + sizeof(rh_recorder_str_header_t));
}

static long rh_recorder_tz = LONG_MAX;

static uint64_t rh_recorder_get_timestamp_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  if (LONG_MAX == rh_recorder_tz) {
    // localtime_r() will call getenv() without lock protection,
    // and will crash when encountering concurrent setenv() calls.
    // We really encountered.
    rh_recorder_tz = 0;
    //    struct tm tm;
    //    if (NULL != localtime_r((time_t *)(&(tv.tv_sec)), &tm)) rh_recorder_tz = tm.tm_gmtoff;
  }

  return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

static size_t rh_recorder_format_timestamp_ms(uint64_t ts_ms, char *buf, size_t buf_len) {
  time_t sec = (time_t)(ts_ms / 1000);
  time_t msec = (time_t)(ts_ms % 1000);

  struct tm tm;
  rh_util_localtime_r(&sec, rh_recorder_tz, &tm);

  return rh_util_snprintf(buf, buf_len, "%04d-%02d-%02dT%02d:%02d:%02d.%03ld%c%02ld:%02ld,",
                          tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                          msec, rh_recorder_tz < 0 ? '-' : '+', labs(rh_recorder_tz / 3600),
                          labs(rh_recorder_tz % 3600));
}

static const char *rh_recorder_get_base_name(const char *lib_name) {
  const char *p = strrchr(lib_name, '/');
  if (NULL != p && '\0' != *(p + 1))
    return p + 1;
  else
    return lib_name;
}

static void rh_recorder_get_base_name_by_addr(uintptr_t addr, char *base_name, size_t base_name_sz) {
#define dlcache_timeout 60
  static time_t dlcache_ts = 0;
  static void *dlcache = NULL;
  static pthread_mutex_t dlcache_lock = PTHREAD_MUTEX_INITIALIZER;

  xdl_info_t dlinfo;
  memset(&dlinfo, 0, sizeof(xdl_info_t));
  int r = 0;
  bool crashed = false;
  time_t now = rh_util_get_stable_timestamp();

  pthread_mutex_lock(&dlcache_lock);

  if (NULL != dlcache && dlcache_ts > 0 && now - dlcache_ts > dlcache_timeout) {
    xdl_addr_clean(&dlcache);
  }
  dlcache_ts = now;

  if (rh_util_get_api_level() >= __ANDROID_API_L__) {
    r = xdl_addr4((void *)addr, &dlinfo, &dlcache, XDL_NON_SYM);
  } else {
    RH_SIG_TRY(SIGSEGV, SIGBUS) {
      r = xdl_addr4((void *)addr, &dlinfo, &dlcache, XDL_NON_SYM);
    }
    RH_SIG_CATCH() {
      crashed = true;
    }
    RH_SIG_EXIT
  }

  pthread_mutex_unlock(&dlcache_lock);

  const char *str;
  if (crashed) {
    str = "error";
  } else if (0 == r || NULL == dlinfo.dli_fbase || NULL == dlinfo.dli_fname || '\0' == dlinfo.dli_fname[0]) {
    str = "unknown";
  } else {
    str = rh_recorder_get_base_name(dlinfo.dli_fname);
    if ('\0' == str[0]) {
      str = "unknown";
    }
  }
  strlcpy(base_name, str, base_name_sz);
}

int rh_recorder_add_op(int error_number, uint8_t op, uintptr_t sym_addr, const char *lib_name,
                       const char *sym_name, uintptr_t new_addr, uint32_t flags, size_t backup_len,
                       uintptr_t stub, uintptr_t caller_addr, const char *caller_lib_name) {
  if (!rh_recorder_recordable) return 0;
  if (rh_recorder_error) return -1;

  // lib_name
  if (NULL == lib_name) return -1;
  lib_name = rh_recorder_get_base_name(lib_name);
  size_t lib_name_len = strlen(lib_name);
  if (0 == lib_name_len || lib_name_len > RH_RECORDER_LIB_NAME_MAX) return -1;

  // sym_name
  if (NULL == sym_name) return -1;
  size_t sym_name_len = strlen(sym_name);
  if (0 == sym_name_len || sym_name_len > RH_RECORDER_SYM_NAME_MAX) return -1;

  // caller_lib_name
  char buf[RH_RECORDER_LIB_NAME_MAX];
  if (NULL == caller_lib_name) {
    rh_recorder_get_base_name_by_addr(caller_addr, buf, sizeof(buf));
    caller_lib_name = buf;
  }
  size_t caller_lib_name_len = strlen(caller_lib_name);

  // add strings to strings-pool
  uint16_t lib_name_idx, sym_name_idx, caller_lib_name_idx;
  if (0 != rh_recorder_add_str(lib_name, lib_name_len, &lib_name_idx)) goto err;
  if (0 != rh_recorder_add_str(sym_name, sym_name_len, &sym_name_idx)) goto err;
  if (0 != rh_recorder_add_str(caller_lib_name, caller_lib_name_len, &caller_lib_name_idx)) goto err;

  // append new op record
  rh_recorder_record_op_header_t header = {op,
                                           (uint8_t)error_number,
                                           rh_recorder_get_timestamp_ms(),
                                           stub,
                                           caller_lib_name_idx,
                                           (uint8_t)backup_len,
                                           lib_name_idx,
                                           sym_name_idx,
                                           sym_addr,
                                           new_addr,
                                           flags};
  pthread_mutex_lock(&rh_recorder_records.lock);
  int r = rh_recorder_buf_append(&rh_recorder_records, RH_RECORDER_RECORDS_BUF_EXPAND_STEP,
                                 RH_RECORDER_RECORDS_BUF_MAX, &header, sizeof(header), NULL, 0);
  pthread_mutex_unlock(&rh_recorder_records.lock);
  if (0 != r) goto err;

  return 0;

err:
  rh_recorder_error = true;
  return -1;
}

int rh_recorder_add_unop(int error_number, uint8_t op, uintptr_t stub, uintptr_t caller_addr,
                         const char *caller_lib_name) {
  if (!rh_recorder_recordable) return 0;
  if (rh_recorder_error) return -1;

  char buf[RH_RECORDER_LIB_NAME_MAX];
  if (NULL == caller_lib_name) {
    rh_recorder_get_base_name_by_addr(caller_addr, buf, sizeof(buf));
    caller_lib_name = buf;
  }
  size_t caller_lib_name_len = strlen(caller_lib_name);

  uint16_t caller_lib_name_idx;
  if (0 != rh_recorder_add_str(caller_lib_name, caller_lib_name_len, &caller_lib_name_idx)) goto err;

  rh_recorder_record_unop_header_t header = {op, (uint8_t)error_number, rh_recorder_get_timestamp_ms(), stub,
                                             caller_lib_name_idx};
  pthread_mutex_lock(&rh_recorder_records.lock);
  int r = rh_recorder_buf_append(&rh_recorder_records, RH_RECORDER_RECORDS_BUF_EXPAND_STEP,
                                 RH_RECORDER_RECORDS_BUF_MAX, &header, sizeof(header), NULL, 0);
  pthread_mutex_unlock(&rh_recorder_records.lock);
  if (0 != r) goto err;

  return 0;

err:
  rh_recorder_error = true;
  return -1;
}

static const char *rh_recorder_get_op_name(uint8_t op) {
  switch (op) {
    case RH_RECORDER_OP_HOOK_INSTR_ADDR:
      return "hook_instr_addr";
    case RH_RECORDER_OP_HOOK_FUNC_ADDR:
      return "hook_func_addr";
    case RH_RECORDER_OP_HOOK_SYM_ADDR:
      return "hook_sym_addr";
    case RH_RECORDER_OP_HOOK_SYM_NAME:
      return "hook_sym_name";
    case RH_RECORDER_OP_UNHOOK:
      return "unhook";
    case RH_RECORDER_OP_INTERCEPT_INSTR_ADDR:
      return "intercept_instr_addr";
    case RH_RECORDER_OP_INTERCEPT_FUNC_ADDR:
      return "intercept_func_addr";
    case RH_RECORDER_OP_INTERCEPT_SYM_ADDR:
      return "intercept_sym_addr";
    case RH_RECORDER_OP_INTERCEPT_SYM_NAME:
      return "intercept_sym_name";
    case RH_RECORDER_OP_UNINTERCEPT:
      return "unintercept";
    default:
      return "error";
  }
}

static bool rh_recorder_op_is_unop(uint8_t op) {
  return RH_RECORDER_OP_UNHOOK == op || RH_RECORDER_OP_UNINTERCEPT == op;
}

static void rh_recorder_output(char **str, int fd, uint32_t item_flags) {
  if (NULL == rh_recorder_records.ptr || 0 == rh_recorder_records.sz) return;

  rh_recorder_buf_t output = {NULL, 0, 0, PTHREAD_MUTEX_INITIALIZER};

  pthread_mutex_lock(&rh_recorder_records.lock);
  pthread_mutex_lock(&rh_recorder_strings.lock);

  char line[RH_RECORDER_LIB_NAME_MAX * 2 + RH_RECORDER_SYM_NAME_MAX + 256];
  size_t line_sz;
  size_t i = 0;
  while (i < rh_recorder_records.sz) {
    line_sz = 0;
    rh_recorder_record_op_header_t *header =
        (rh_recorder_record_op_header_t *)((uintptr_t)rh_recorder_records.ptr + i);

    if (item_flags & RAHOOK_RECORD_ITEM_TIMESTAMP)
      line_sz += rh_recorder_format_timestamp_ms(header->ts_ms, line + line_sz, sizeof(line) - line_sz);
    if (item_flags & RAHOOK_RECORD_ITEM_CALLER_LIB_NAME)
      line_sz += rh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%s,",
                                  rh_recorder_find_str(header->caller_lib_name_idx));
    if (item_flags & RAHOOK_RECORD_ITEM_OP)
      line_sz += rh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%s,",
                                  rh_recorder_get_op_name(header->op));
    if ((item_flags & RAHOOK_RECORD_ITEM_LIB_NAME) && !rh_recorder_op_is_unop(header->op))
      line_sz += rh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%s,",
                                  rh_recorder_find_str(header->lib_name_idx));
    if ((item_flags & RAHOOK_RECORD_ITEM_SYM_NAME) && !rh_recorder_op_is_unop(header->op))
      line_sz += rh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%s,",
                                  rh_recorder_find_str(header->sym_name_idx));
    if ((item_flags & RAHOOK_RECORD_ITEM_SYM_ADDR) && !rh_recorder_op_is_unop(header->op))
      line_sz += rh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%" PRIxPTR ",", header->sym_addr);
    if ((item_flags & RAHOOK_RECORD_ITEM_NEW_ADDR) && !rh_recorder_op_is_unop(header->op))
      line_sz += rh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%" PRIxPTR ",", header->new_addr);
    if ((item_flags & RAHOOK_RECORD_ITEM_BACKUP_LEN) && !rh_recorder_op_is_unop(header->op))
      line_sz += rh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%" PRIu8 ",", header->backup_len);
    if (item_flags & RAHOOK_RECORD_ITEM_ERRNO)
      line_sz +=
          rh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%" PRIu8 ",", header->error_number);
    if (item_flags & RAHOOK_RECORD_ITEM_STUB)
      line_sz += rh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%" PRIxPTR ",", header->stub);
    if ((item_flags & RAHOOK_RECORD_ITEM_FLAGS) && !rh_recorder_op_is_unop(header->op))
      line_sz += rh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%" PRIu32 ",", header->flags);
    line[line_sz - 1] = '\n';

    if (NULL != str) {
      // append to string
      if (0 != rh_recorder_buf_append(&output, RH_RECORDER_OUTPUT_BUF_EXPAND_STEP, RH_RECORDER_OUTPUT_BUF_MAX,
                                      line, line_sz, NULL, 0)) {
        rh_recorder_buf_free(&output);
        break;  // failed
      }
    } else {
      // write to FD
      if (0 != rh_util_write(fd, line, line_sz)) break;  // failed
    }

    i += (rh_recorder_op_is_unop(header->op) ? sizeof(rh_recorder_record_unop_header_t)
                                             : sizeof(rh_recorder_record_op_header_t));
  }

  pthread_mutex_unlock(&rh_recorder_strings.lock);
  pthread_mutex_unlock(&rh_recorder_records.lock);

  // error message
  if (rh_recorder_error) {
    line_sz = 0;

    if (item_flags & RAHOOK_RECORD_ITEM_TIMESTAMP)
      line_sz += rh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "9999-99-99T00:00:00.000+00:00,");
    if (item_flags & RAHOOK_RECORD_ITEM_CALLER_LIB_NAME)
      line_sz += rh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "error,");
    if (item_flags & RAHOOK_RECORD_ITEM_OP)
      line_sz += rh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "error,");

    if (0 == line_sz) line_sz = rh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "error,");

    line[line_sz - 1] = '\n';

    if (NULL != str) {
      // append to string
      if (0 != rh_recorder_buf_append(&output, RH_RECORDER_OUTPUT_BUF_EXPAND_STEP, RH_RECORDER_OUTPUT_BUF_MAX,
                                      line, line_sz, NULL, 0)) {
        rh_recorder_buf_free(&output);
        return;  // failed
      }
    } else {
      // write to FD
      if (0 != rh_util_write(fd, line, line_sz)) return;  // failed
    }
  }

  // return string
  if (NULL != str) {
    if (0 != rh_recorder_buf_append(&output, RH_RECORDER_OUTPUT_BUF_EXPAND_STEP, RH_RECORDER_OUTPUT_BUF_MAX,
                                    "", 1, NULL, 0)) {
      rh_recorder_buf_free(&output);
      return;  // failed
    }
    *str = output.ptr;
  }
}

char *rh_recorder_get(uint32_t item_flags) {
  if (!rh_recorder_recordable) return NULL;
  if (0 == (item_flags & RAHOOK_RECORD_ITEM_ALL)) return NULL;

  char *str = NULL;
  rh_recorder_output(&str, -1, item_flags);
  return str;
}

void rh_recorder_dump(int fd, uint32_t item_flags) {
  if (!rh_recorder_recordable) return;
  if (0 == (item_flags & RAHOOK_RECORD_ITEM_ALL)) return;
  if (fd < 0) return;
  rh_recorder_output(NULL, fd, item_flags);
}

#else

bool rh_recorder_get_recordable(void) {
  return false;
}

void rh_recorder_set_recordable(bool recordable) {
  (void)recordable;
}

int rh_recorder_add_hook(int error_number, bool is_hook_sym_addr, uintptr_t sym_addr, const char *lib_name,
                         const char *sym_name, uintptr_t new_addr, uint32_t flags, size_t backup_len,
                         uintptr_t stub, uintptr_t caller_addr) {
  (void)error_number, (void)is_hook_sym_addr, (void)sym_addr, (void)lib_name, (void)sym_name, (void)new_addr,
      (void)flags, (void)backup_len, (void)stub, (void)caller_addr;
  return 0;
}

int rh_recorder_add_unhook(int error_number, uintptr_t stub, uintptr_t caller_addr) {
  (void)error_number, (void)stub, (void)caller_addr;
  return 0;
}

char *rh_recorder_get(uint32_t item_flags) {
  (void)item_flags;
  return NULL;
}

void rh_recorder_dump(int fd, uint32_t item_flags) {
  (void)fd, (void)item_flags;
}

// compat wrappers for rahook.c
#ifdef RH_CONFIG_OPERATION_RECORDS
void rh_recorder_start(void) { rh_recorder_set_recordable(true); }
void rh_recorder_stop(void) { rh_recorder_set_recordable(false); }
bool rh_recorder_is_active(void) { return rh_recorder_get_recordable(); }
char *rh_recorder_export(void) { return rh_recorder_get(0); }
#else
static bool g_rahook_recordable = false;
void rh_recorder_start(void) { g_rahook_recordable = true; }
void rh_recorder_stop(void) { g_rahook_recordable = false; }
bool rh_recorder_is_active(void) { return g_rahook_recordable; }
char *rh_recorder_export(void) { return strdup("[]"); }
#endif
void rh_recorder_free(char *json) { free(json); }
int rh_recorder_add(const char *lib, const char *sym, uintptr_t target,
                    uintptr_t new_addr, uintptr_t *orig, uint32_t flags,
                    size_t backup_len, uintptr_t stub) {
#ifdef RH_CONFIG_OPERATION_RECORDS
    return rh_recorder_add_op(0, RH_RECORDER_OP_HOOK_SYM_NAME, target,
                              lib, sym, new_addr, flags, backup_len, stub, 0, NULL);
#else
    (void)lib; (void)sym; (void)target; (void)new_addr;
    (void)orig; (void)flags; (void)backup_len; (void)stub;
    return 0;
#endif
}

#endif
