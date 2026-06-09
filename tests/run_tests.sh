#!/bin/bash
# RaHook test suite runner
# Copyright (C) 2026 Dere3046

set -e
cd "$(dirname "$0")/.."

BASE_FLAGS="-O2 -std=gnu11 -no-pie"
SRC="core/rh_ref.c core/rh_safe.c core/rh_trampo.c core/rh_island.c core/rh_linker.c
     core/rh_hub.c core/rh_mode.c core/rh_recorder.c core/rh_bytesig.c
     core/rh_deflector.c core/rh_invocation_stack.c
     arch/x86_64/rh_x64.c arch/x86_64/rh_x64_inst.c
     arch/x86/rh_ia32.c arch/x86/rh_x86_inst.c
     third_party/xdl/xdl.c third_party/xdl/xdl_iterate.c third_party/xdl/xdl_linker.c
     third_party/xdl/xdl_lzma.c third_party/xdl/xdl_util.c
     third_party/hde64/hde64.c
     rahook.c"

INCLUDES="-I. -Icore -Iarch/x86_64 -Iarch/x86 -Itests
          -Ithird_party/xdl -Ithird_party/xdl/android
          -Ithird_party/bsd -Ithird_party/hde64"

LIBS="-lpthread -ldl -lz"

total_pass=0
total_fail=0
total_skip=0

for test in test_core test_mode test_api test_concurrent test_safety; do
    echo "=== $test ==="
    gcc -w $BASE_FLAGS -D_GNU_SOURCE -DRH_ARCH_X86_64=1 $INCLUDES \
        -o build/$test tests/$test.c $SRC $LIBS
    build/$test
    echo ""
done

echo "All test suites complete"
