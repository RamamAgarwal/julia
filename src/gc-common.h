// This file is a part of Julia. License is MIT: https://julialang.org/license

#ifndef JL_GC_COMMON_H
#define JL_GC_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "julia.h"

// =========================================================================== //
// GC Callbacks
// =========================================================================== //

typedef void (*jl_gc_cb_func_t)(void);

typedef struct _jl_gc_callback_list_t {
    struct _jl_gc_callback_list_t *next;
    jl_gc_cb_func_t func;
} jl_gc_callback_list_t;

extern jl_gc_callback_list_t *gc_cblist_root_scanner;
extern jl_gc_callback_list_t *gc_cblist_task_scanner;
extern jl_gc_callback_list_t *gc_cblist_pre_gc;
extern jl_gc_callback_list_t *gc_cblist_post_gc;
extern jl_gc_callback_list_t *gc_cblist_notify_external_alloc;
extern jl_gc_callback_list_t *gc_cblist_notify_external_free;
extern jl_gc_callback_list_t *gc_cblist_notify_gc_pressure;

#define gc_invoke_callbacks(ty, list, args) \
    do { \
        for (jl_gc_callback_list_t *cb = list; \
                cb != NULL; \
                cb = cb->next) \
        { \
            ((ty)(cb->func)) args; \
        } \
    } while (0)

#ifdef __cplusplus
}
#endif

// =========================================================================== //
// malloc wrappers, aligned allocation
// =========================================================================== //

#if defined(_OS_WINDOWS_)
STATIC_INLINE void *jl_malloc_aligned(size_t sz, size_t align)
{
    return _aligned_malloc(sz ? sz : 1, align);
}
STATIC_INLINE void *jl_realloc_aligned(void *p, size_t sz, size_t oldsz,
                                       size_t align)
{
    (void)oldsz;
    return _aligned_realloc(p, sz ? sz : 1, align);
}
STATIC_INLINE void jl_free_aligned(void *p) JL_NOTSAFEPOINT
{
    _aligned_free(p);
}
#else
STATIC_INLINE void *jl_malloc_aligned(size_t sz, size_t align)
{
#if defined(_P64) || defined(__APPLE__)
    if (align <= 16)
        return malloc(sz);
#endif
    void *ptr;
    if (posix_memalign(&ptr, align, sz))
        return NULL;
    return ptr;
}
STATIC_INLINE void *jl_realloc_aligned(void *d, size_t sz, size_t oldsz,
                                       size_t align)
{
#if defined(_P64) || defined(__APPLE__)
    if (align <= 16)
        return realloc(d, sz);
#endif
    void *b = jl_malloc_aligned(sz, align);
    if (b != NULL) {
        memcpy(b, d, oldsz > sz ? sz : oldsz);
        free(d);
    }
    return b;
}
STATIC_INLINE void jl_free_aligned(void *p) JL_NOTSAFEPOINT
{
    free(p);
}
#endif
#define malloc_cache_align(sz) jl_malloc_aligned(sz, JL_CACHE_BYTE_ALIGNMENT)
#define realloc_cache_align(p, sz, oldsz) jl_realloc_aligned(p, sz, oldsz, JL_CACHE_BYTE_ALIGNMENT)

// =========================================================================== //
// Stop-the-world for GC
// =========================================================================== //
void jl_gc_wait_for_the_world(jl_ptls_t* gc_all_tls_states, int gc_n_threads);

// =========================================================================== //
// Finalization
// =========================================================================== //

// Protect all access to `finalizer_list_marked` and `to_finalize`.
// For accessing `ptls->finalizers`, the lock is needed if a thread
// is going to realloc the buffer (of its own list) or accessing the
// list of another thread
extern jl_mutex_t finalizers_lock;

void sweep_finalizer_list(arraylist_t *list);
void run_finalizers(jl_task_t *ct, int finalizers_thread);
JL_DLLEXPORT void jl_gc_add_finalizer_th(jl_ptls_t ptls, jl_value_t *v, jl_function_t *f) JL_NOTSAFEPOINT;
JL_DLLEXPORT void jl_finalize_th(jl_task_t *ct, jl_value_t *o);

// =========================================================================== //
// Permanent Allocations
// =========================================================================== //

void gc_sweep_perm_alloc(void);

#endif // JL_GC_COMMON_H
