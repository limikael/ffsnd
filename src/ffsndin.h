#ifndef __FFSNDIN_H__
#define __FFSNDIN_H__

#include <stdio.h>

typedef struct FFSNDIN FFSNDIN;

FFSNDIN *ffsndin_open(const char *fn);
void ffsndin_close(FFSNDIN *ffsndin);
int ffsndin_read(FFSNDIN *pragmasound, float *target, size_t values);
int ffsndin_eof(FFSNDIN *ffsndin);

#endif