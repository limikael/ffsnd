#ifndef __FFSNDUTIL_H__
#define __FFSNDUTIL_H__

#include <stdio.h>

void ffsnd_fail(char *format, ...);
void *ffsnd_nicemalloc(size_t size);
void *ffsnd_nicerealloc(void *org, size_t size);
void ffsnd_noop(char *s, ...);
void ffsnd_trace(char *s, ...);

#ifdef DEBUG
#define TRACE ffsnd_trace
#else
#define TRACE ffsnd_noop
#endif

#endif