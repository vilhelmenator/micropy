#ifndef NATHRA_TYPES_H
#define NATHRA_TYPES_H

/*
 * nathra_types.h — primitive types and forward declarations only.
 *
 * This header intentionally has no #include directives beyond the two
 * standard headers below. It is safe to include from any other header
 * without triggering transitive inclusion of stdio.h, stdlib.h,
 * pthread.h, windows.h, etc.
 *
 * Full struct definitions and runtime functions live in nathra_rt.h,
 * which #includes this file.
 */

#include <stdint.h>
#include <stddef.h>

/* Branch-prediction hints — portable across GCC, Clang, MSVC */
#if defined(__GNUC__) || defined(__clang__)
#  define NR_LIKELY(x)   __builtin_expect(!!(x), 1)
#  define NR_UNLIKELY(x) __builtin_expect(!!(x), 0)
#  define NR_PREFETCH(p, rw, loc) __builtin_prefetch((p), (rw), (loc))
#else
#  define NR_LIKELY(x)   (x)
#  define NR_UNLIKELY(x) (x)
#  define NR_PREFETCH(p, rw, loc) ((void)0)
#endif

/* Thread-local storage qualifier — portable across GCC, Clang, MSVC */
#if defined(_MSC_VER)
#  define NR_TLS __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#  define NR_TLS __thread
#else
#  define NR_TLS _Thread_local
#endif

/* Primitive value type (int or float packed into 64 bits) */
typedef uint64_t NrVal;

/* Forward declarations — pointer-safe without full struct definitions */
typedef struct NrStr        NrStr;
typedef struct NrList       NrList;
typedef struct NrDict       NrDict;
typedef struct NrDictEntry  NrDictEntry;
typedef struct NrArena      NrArena;
typedef struct NrChannel    NrChannel;
typedef struct NrThreadPool NrThreadPool;
typedef struct NrThread     NrThread;
typedef struct NrMutex      NrMutex;
typedef struct NrCond       NrCond;
typedef struct NrWriter     NrWriter;
typedef struct NrReader     NrReader;

/* Debug allocation counter — referenced by nathra_rt.h debug wrappers.
 * The definition lives in the main translation unit (emitted by the compiler
 * when --debug is active). */
#ifdef NATHRA_DEBUG
extern volatile long long _nr_alloc_count;
#endif

#endif /* NATHRA_TYPES_H */