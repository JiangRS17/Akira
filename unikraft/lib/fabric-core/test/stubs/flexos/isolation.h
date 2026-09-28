#ifndef _FLEXOS_ISOLATION_H_MOCK_
#define _FLEXOS_ISOLATION_H_MOCK_

/* In native test mode, isolation is a no-op.
 * Pull in the allocator stub so flexos_shared_alloc is visible. */
#include "../uk/alloc.h"

#endif /* _FLEXOS_ISOLATION_H_MOCK_ */
