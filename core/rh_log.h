#pragma once

#if RH_CONFIG_DEBUG
#include <stdio.h>
extern bool rahook_get_debug(void);
#define RH_LOG(fmt, ...)       do { if (rahook_get_debug()) fprintf(stderr, "rahook: " fmt "\n", ##__VA_ARGS__); } while(0)
#define RH_LOG_ERR(fmt, ...)   fprintf(stderr, "rahook error: " fmt "\n", ##__VA_ARGS__)
#define RH_LOG_DEBUG(fmt, ...) do { if (rahook_get_debug()) fprintf(stderr, "rahook debug: " fmt "\n", ##__VA_ARGS__); } while(0)
#else
#define RH_LOG(fmt, ...)       ((void)0)
#define RH_LOG_ERR(fmt, ...)   ((void)0)
#define RH_LOG_DEBUG(fmt, ...) ((void)0)
#endif
