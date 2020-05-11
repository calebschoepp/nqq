#ifndef nqq_common_h
#define nqq_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// TODO maybe don't need #ifdef DEBUG
#ifdef DEBUG
#define DEBUG_PRINT_CODE
#define DEBUG_TRACE_EXECUTION
#endif

#define UINT8_COUNT (UINT8_MAX + 1) // TODO remove if not used
#define UINT16_COUNT (UINT16_MAX + 1)


#endif