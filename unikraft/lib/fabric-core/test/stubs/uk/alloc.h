#ifndef _UK_ALLOC_H_MOCK_
#define _UK_ALLOC_H_MOCK_

#include <stdlib.h>
#include <string.h>

struct uk_alloc {
    int dummy;
};

/* A simple global allocator stub */
static struct uk_alloc _mock_alloc = { 0 };
#define flexos_shared_alloc (&_mock_alloc)

static inline void *uk_zalloc(struct uk_alloc *a, size_t size)
{
    (void)a;
    return calloc(1, size);
}

static inline void *uk_malloc(struct uk_alloc *a, size_t size)
{
    (void)a;
    return malloc(size);
}

static inline void uk_free(struct uk_alloc *a, void *ptr)
{
    (void)a;
    free(ptr);
}

#endif /* _UK_ALLOC_H_MOCK_ */