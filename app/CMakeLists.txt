cmake_minimum_required(VERSION 3.10)
project(rahook_jni C)
set(CMAKE_C_STANDARD 11)

set(RAHOOK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(CORE_DIR ${RAHOOK_DIR}/core)
set(XDL_DIR ${RAHOOK_DIR}/third_party/xdl)
set(HDE64_DIR ${RAHOOK_DIR}/third_party/hde64)
set(BSD_DIR ${RAHOOK_DIR}/third_party/bsd)

file(GLOB RAHOOK_SRC
    ${RAHOOK_DIR}/rahook.c
    ${CORE_DIR}/rh_ref.c ${CORE_DIR}/rh_safe.c ${CORE_DIR}/rh_trampo.c
    ${CORE_DIR}/rh_island.c ${CORE_DIR}/rh_linker.c ${CORE_DIR}/rh_hub.c
    ${CORE_DIR}/rh_mode.c ${CORE_DIR}/rh_recorder.c ${CORE_DIR}/rh_bytesig.c
    ${CORE_DIR}/rh_deflector.c ${CORE_DIR}/rh_invocation_stack.c
)

list(APPEND RAHOOK_SRC
    ${XDL_DIR}/xdl.c ${XDL_DIR}/xdl_iterate.c ${XDL_DIR}/xdl_linker.c
    ${XDL_DIR}/xdl_lzma.c ${XDL_DIR}/xdl_util.c
    ${HDE64_DIR}/hde64.c
)

if(${ANDROID_ABI} STREQUAL "arm64-v8a")
    message(WARNING "ARM64 backend interface mismatched with rahook.c — skipping ARM64 ABI")
    # Build as empty stub for now
    add_library(rahook_jni SHARED ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/rahook_jni.c)
    target_include_directories(rahook_jni PRIVATE ${RAHOOK_DIR} ${CORE_DIR} ${XDL_DIR})
    target_compile_options(rahook_jni PRIVATE -Os -fvisibility=hidden)
    target_link_libraries(rahook_jni log dl z)
    return()
elseif(${ANDROID_ABI} STREQUAL "armeabi-v7a")
    message(WARNING "ARM32 backend interface mismatched — skipping ARM32 ABI")
    add_library(rahook_jni SHARED ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/rahook_jni.c)
    target_include_directories(rahook_jni PRIVATE ${RAHOOK_DIR} ${CORE_DIR} ${XDL_DIR})
    target_compile_options(rahook_jni PRIVATE -Os -fvisibility=hidden)
    target_link_libraries(rahook_jni log dl z)
    return()
elseif(${ANDROID_ABI} STREQUAL "x86")
    add_definitions(-DRH_ARCH_X86=1)
    list(APPEND RAHOOK_SRC ${RAHOOK_DIR}/arch/x86/rh_ia32.c ${RAHOOK_DIR}/arch/x86/rh_x86_inst.c)
    include_directories(${RAHOOK_DIR}/arch/x86)
else()
    add_definitions(-DRH_ARCH_X86_64=1)
    list(APPEND RAHOOK_SRC ${RAHOOK_DIR}/arch/x86_64/rh_x64.c ${RAHOOK_DIR}/arch/x86_64/rh_x64_inst.c)
    include_directories(${RAHOOK_DIR}/arch/x86_64)
endif()

add_library(rahook_jni SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main/cpp/rahook_jni.c
    ${RAHOOK_SRC}
)

target_include_directories(rahook_jni PRIVATE
    ${RAHOOK_DIR}
    ${CORE_DIR}
    ${XDL_DIR}
    ${BSD_DIR}
    ${HDE64_DIR}
)

target_compile_options(rahook_jni PRIVATE -Os -fvisibility=hidden -ffunction-sections -fdata-sections -fno-stack-protector)
target_link_options(rahook_jni PRIVATE -Wl,--gc-sections -Wl,--exclude-libs,ALL)
target_link_libraries(rahook_jni log dl z)
