#ifndef _UK_SCHED_H_MOCK_
#define _UK_SCHED_H_MOCK_

#include <sched.h>

static inline void uk_sched_yield(void)
{
    sched_yield();
}

#endif /* _UK_SCHED_H_MOCK_ */