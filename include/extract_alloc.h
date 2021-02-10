#ifndef EXTRACT_ALLOC_H
#define EXTRACT_ALLOC_H

#include <stdlib.h>

typedef void* (*extract_realloc_fn_t)(void* state, void* prev, size_t size);

typedef struct
{
    extract_realloc_fn_t    realloc;
    void*                   realloc_state;
    size_t                  exp_min_alloc_size;
} extract_alloc_t;

int extract_malloc(extract_alloc_t* alloc, void** pptr, size_t size);
/* Sets *pptr to point to new buffer and returns 0. On error return -1 with
errno set and *pptr=NULL. */

int extract_realloc(extract_alloc_t* alloc, void** pptr, size_t newsize);
/* Sets *pptr to point to reallocated buffer and returns 0. On error return -1
with errno set and *pptr unchanged (pointing to the existing buffer). */

void extract_free(extract_alloc_t* alloc, void** pptr);
/* Frees block pointed to by *pptr and sets *pptr to NULL. */

#define extract_malloc(alloc, pptr, size) (extract_malloc)(alloc, (void**) pptr, size)
#define extract_realloc(alloc, pptr, newsize) (extract_realloc)(alloc, (void**) pptr, newsize)
#define extract_free(alloc, pptr) (extract_free)(alloc, (void**) pptr)
/* These allow callers to use any pointer type, not just void*. */

typedef struct
{
    int num_malloc;
    int num_realloc;
    int num_free;
    int num_libc_realloc;
} extract_alloc_info_t;

extern extract_alloc_info_t extract_alloc_info;

int extract_realloc2(extract_alloc_t* alloc, void** pptr, size_t oldsize, size_t newsize);
/* A realloc variant that takes the existing buffer size.

If <oldsize> is not zero and *pptr is not NULL, <oldsize> must be the size of
the existing buffer and may used internally to over-allocate in order to avoid
too many calls to realloc(). See extract_alloc_exp_min() for more information.
*/

#define extract_realloc2(alloc, pptr, oldsize, newsize) (extract_realloc2)(alloc, (void**) pptr, oldsize, newsize)

void extract_alloc_exp_min(extract_alloc_t* alloc, size_t size);

#endif
