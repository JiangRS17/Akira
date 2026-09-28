#ifndef _UK_MUTEX_H_MOCK_
#define _UK_MUTEX_H_MOCK_

#include <pthread.h>

struct uk_mutex {
    pthread_mutex_t mtx;
};

static inline void uk_mutex_init(struct uk_mutex *m)
{
    pthread_mutex_init(&m->mtx, NULL);
}

static inline void uk_mutex_lock(struct uk_mutex *m)
{
    pthread_mutex_lock(&m->mtx);
}

static inline void uk_mutex_unlock(struct uk_mutex *m)
{
    pthread_mutex_unlock(&m->mtx);
}

#endif /* _UK_MUTEX_H_MOCK_ */