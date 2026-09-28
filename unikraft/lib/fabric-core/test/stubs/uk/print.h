#ifndef _UK_PRINT_H_MOCK_
#define _UK_PRINT_H_MOCK_

#include <stdio.h>

#define uk_pr_info(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define uk_pr_err(fmt, ...)  fprintf(stderr, fmt, ##__VA_ARGS__)
#define uk_pr_warn(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)

#endif /* _UK_PRINT_H_MOCK_ */