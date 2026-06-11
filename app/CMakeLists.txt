cmake_minimum_required(VERSION 3.10)
project(rahook_jni C)

set(CMAKE_C_STANDARD 11)

file(GLOB RAHOOK_SRC
    ${CMAKE_SOURCE_DIR}/../../../rahook.c
    ${CMAKE_SOURCE_DIR}/../../../core/*.c
    ${CMAKE_SOURCE_DIR}/../../../third_party/xdl/*.c
    ${CMAKE_SOURCE_DIR}/../../../third_party/hde64/hde64.c
)

if(${ANDROID_ABI} STREQUAL "arm64-v8a")
    list(APPEND RAHOOK_SRC ${CMAKE_SOURCE_DIR}/../../../arch/arm64/rh_a64.c ${CMAKE_SOURCE_DIR}/../../../arch/arm64/rh_arm64_inst.c)
    add_definitions(-DRH_ARCH_ARM64=1)
    include_directories(${CMAKE_SOURCE_DIR}/../../../arch/arm64)
elseif(${ANDROID_ABI} STREQUAL "armeabi-v7a")
    list(APPEND RAHOOK_SRC ${CMAKE_SOURCE_DIR}/../../../arch/arm/rh_a32.c ${CMAKE_SOURCE_DIR}/../../../arch/arm/rh_t16.c ${CMAKE_SOURCE_DIR}/../../../arch/arm/rh_t32.c ${CMAKE_SOURCE_DIR}/../../../arch/arm/rh_txx.c ${CMAKE_SOURCE_DIR}/../../../arch/arm/rh_arm_inst.c)
    add_definitions(-DRH_ARCH_ARM=1)
    include_directories(${CMAKE_SOURCE_DIR}/../../../arch/arm)
elseif(${ANDROID_ABI} STREQUAL "x86")
    list(APPEND RAHOOK_SRC ${CMAKE_SOURCE_DIR}/../../../arch/x86/rh_ia32.c ${CMAKE_SOURCE_DIR}/../../../arch/x86/rh_x86_inst.c)
    add_definitions(-DRH_ARCH_X86=1)
    include_directories(${CMAKE_SOURCE_DIR}/../../../arch/x86)
elseif(${ANDROID_ABI} STREQUAL "x86_64")
    list(APPEND RAHOOK_SRC ${CMAKE_SOURCE_DIR}/../../../arch/x86_64/rh_x64.c ${CMAKE_SOURCE_DIR}/../../../arch/x86_64/rh_x64_inst.c)
    add_definitions(-DRH_ARCH_X86_64=1)
    include_directories(${CMAKE_SOURCE_DIR}/../../../arch/x86_64)
endif()

add_library(rahook_jni SHARED
    ${CMAKE_SOURCE_DIR}/rahook_jni.c
    ${RAHOOK_SRC}
)

target_include_directories(rahook_jni PRIVATE
    ${CMAKE_SOURCE_DIR}/../../..
    ${CMAKE_SOURCE_DIR}/../../../core
    ${CMAKE_SOURCE_DIR}/../../../third_party/xdl
    ${CMAKE_SOURCE_DIR}/../../../third_party/bsd
    ${CMAKE_SOURCE_DIR}/../../../third_party/hde64
)

target_compile_options(rahook_jni PRIVATE
    -Os -fvisibility=hidden -ffunction-sections -fdata-sections -fno-stack-protector
)
target_link_options(rahook_jni PRIVATE -Wl,--gc-sections -Wl,--exclude-libs,ALL)
target_link_libraries(rahook_jni log dl z)
