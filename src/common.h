#ifndef nqq_common_h
#define nqq_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef DEBUG
#define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION
#define DEBUG_STRESS_GC
#define DEBUG_LOG_GC
#endif

#define UINT8_COUNT (UINT8_MAX + 1)
#define UINT16_COUNT (UINT16_MAX + 1)


#endif