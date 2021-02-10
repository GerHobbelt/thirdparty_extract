#include "../include/extract_alloc.h"
#include "memento.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>


extract_alloc_info_t    extract_alloc_info = {0};

static size_t round_up(extract_alloc_t* alloc, size_t n)
{
    if (alloc && alloc->exp_min_alloc_size) {
        /* Round up to power of two. */
        size_t ret;
        if (n==0) return 0;
        ret = alloc->exp_min_alloc_size;
        for(;;) {
            size_t ret_old;
            if (ret >= n) return ret;
            ret_old = ret;
            ret *= 2;
            assert(ret > ret_old);
            (void) ret_old;
        }
    }
    else {
        return n;
    }
}

int (extract_malloc)(extract_alloc_t* alloc, void** pptr, size_t size)
{
    void* p;
    size = round_up(alloc, size);
    extract_alloc_info.num_malloc += 1;
    p = (alloc) ? alloc->realloc(alloc->realloc_state, NULL, size) : malloc(size);
    *pptr = p;
    if (!p)
    {
        if (alloc) errno = ENOMEM;
        return -1;
    }
    return 0;
}

int (extract_realloc)(extract_alloc_t* alloc, void** pptr, size_t newsize)
{
    void* p = (alloc) ? alloc->realloc(alloc->realloc_state, *pptr, newsize) : realloc(*pptr, newsize);
    if (!p)
    {
        if (alloc) errno = ENOMEM;
        return -1;
    }
    *pptr = p;
    return 0;
}

int (extract_realloc2)(extract_alloc_t* alloc, void** pptr, size_t oldsize, size_t newsize)
{
    /* We ignore <oldsize> if <ptr> is NULL - allows callers to not worry about
    edge cases e.g. with strlen+1. */
    oldsize = (*pptr) ? round_up(alloc, oldsize) : 0;
    newsize = round_up(alloc, newsize);
    extract_alloc_info.num_realloc += 1;
    if (newsize == oldsize) return 0;
    return (extract_realloc)(alloc, pptr, newsize);
}

void (extract_free)(extract_alloc_t* alloc, void** pptr)
{
    extract_alloc_info.num_free += 1;
    (alloc) ? alloc->realloc(alloc->realloc_state, *pptr, 0) : free(*pptr);
    *pptr = NULL;
}

void extract_alloc_exp_min(extract_alloc_t* alloc, size_t size)
{
    alloc->exp_min_alloc_size = size;
}
