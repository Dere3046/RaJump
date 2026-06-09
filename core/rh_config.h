#pragma once

#define RH_CONFIG_DEBUG 0
#define RH_CONFIG_ISLAND_MIN_SIZE 8

// ARM32 thumb tail alignment detection
#define RH_CONFIG_DETECT_THUMB_TAIL_ALIGNED 1

// ARM64 island hook mode (try 4B B first, fallback to direct)
#define RH_CONFIG_TRY_HOOK_WITH_ISLAND 1
#define RH_CONFIG_TRY_HOOK_WITHOUT_ISLAND 1

// bytesig crash protection
#define RH_CONFIG_BYTESIG 1

// maximum backup bytes for no-island mode
#define RH_CONFIG_NO_ISLAND_BACKUP_MAX 24

// recorder capacity
#define RH_CONFIG_RECORDER_CAPACITY 256
