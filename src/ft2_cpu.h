#pragma once

#include <stdint.h>

#ifdef _WIN32

#ifdef _WIN64
#define CPU_64BIT 1
#else
#define CPU_64BIT 0
#endif

#else
#include <limits.h>

#if __WORDSIZE == 64
#define CPU_64BIT 1
#else
#define CPU_64BIT 0
#endif

#endif

#if CPU_64BIT
#define CPU_BITS 64
#define uintCPUWord_t uint64_t
#define intCPUWord_t int64_t
#else
#define CPU_BITS 32
#define uintCPUWord_t uint32_t
#define intCPUWord_t int32_t
#endif
