#ifndef nqq_common_h
#define nqq_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#ifdef DEBUG_ALL
// Print a disassembly of a programs code after compilation and before execution
#define DEBUG_PRINT_CODE
// Print the opcode and stack at the beginning of every loop through the VM
#define DEBUG_TRACE_EXECUTION
// Print the opcode and stack at the beginning of every loop through the VM
#define DEBUG_TRACE_EXECUTION
// Log the inner workings of the GC when it runs
#define DEBUG_LOG_GC
// Force the GC to run on eery loop through the VM
#define DEBUG_STRESS_GC
#endif

// Integer maxes used throughout codebase
#define UINT8_COUNT (UINT8_MAX + 1)
#define UINT16_COUNT (UINT16_MAX + 1)

#endif