cmake_minimum_required(VERSION 3.10)
project(rahook C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(${ANDROID_ABI} STREQUAL "arm64-v8a")
    add_definitions(-DRH_ARCH_ARM64=1)
    set(ARCH_SRC
        arch/arm64/rh_a64.c
        arch/arm64/rh_arm64_inst.c
        arch/arm64/rh_hub_trampo.S
        arch/arm64/rh_bridge.c)
    set(ARCH_INC arch/arm64)
elseif(${ANDROID_ABI} STREQUAL "armeabi-v7a")
    add_definitions(-DRH_ARCH_ARM=1)
    set(ARCH_SRC
        arch/arm/rh_a32.c
        arch/arm/rh_t16.c
        arch/arm/rh_t32.c
        arch/arm/rh_txx.c
        arch/arm/rh_arm_inst.c
        arch/arm/rh_hub_trampo.S
        arch/arm/rh_bridge.c)
    set(ARCH_INC arch/arm)
elseif(${ANDROID_ABI} STREQUAL "x86")
    add_definitions(-DRH_ARCH_X86=1)
    set(ARCH_SRC
        arch/x86/rh_ia32.c
        arch/x86/rh_x86_inst.c)
    set(ARCH_INC arch/x86)
elseif(${ANDROID_ABI} STREQUAL "x86_64")
    add_definitions(-DRH_ARCH_X86_64=1)
    set(ARCH_SRC
        arch/x86_64/rh_x64.c
        arch/x86_64/rh_x64_inst.c)
    set(ARCH_INC arch/x86_64)
endif()

file(GLOB CORE_SRC core/*.c)
file(GLOB XDL_SRC third_party/xdl/*.c)
add_library(rahook SHARED
    rahook.c
    ${CORE_SRC}
    ${XDL_SRC}
    ${ARCH_SRC})

target_include_directories(rahook PRIVATE . core ${ARCH_INC} third_party/xdl third_party/bsd)
target_compile_options(rahook PRIVATE
    -Os
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
    -fno-stack-protector
    -Wall -Wextra -Werror)
target_link_options(rahook PRIVATE
    -Wl,--gc-sections
    -Wl,--exclude-libs,ALL
    -Wl,-z,max-page-size=16384)
target_link_libraries(rahook log dl z)

option(BUILD_TESTS "Build test suite" ON)

if(BUILD_TESTS)
    enable_testing()
    include_directories(tests)

    set(TEST_LIST
        test_core test_mode test_api test_concurrent test_safety
        test_stress test_perf test_bytesig test_memory test_regress
    )

    foreach(TEST_NAME ${TEST_LIST})
        add_executable(${TEST_NAME} tests/${TEST_NAME}.c)
        target_include_directories(${TEST_NAME} PRIVATE . core tests third_party/xdl third_party/bsd)
        target_link_libraries(${TEST_NAME} rahook log dl pthread z)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endforeach()
endif()
