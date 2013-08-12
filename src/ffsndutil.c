#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

/**
 * Exit with an error.
 */
void ffsnd_fail(char *format, ...) {
	va_list args;

	va_start(args,format);
	vprintf (format,args);
	va_end(args);	

	printf("\n");
	exit(1);
}

/**
 * Allocate memory, exit on fail.
 */
void *ffsnd_nicemalloc(size_t size) {
	void *p=malloc(size);

	if (!p)
		ffsnd_fail("Out of memory");

	return p;
}

/**
 * Reallocate memory, exit on fail.
 */
void *ffsnd_nicerealloc(void *org, size_t size) {
	void *p;

	if (org)
		p=realloc(org,size);

	else
		p=malloc(size);

	if (!p)
		ffsnd_fail("Out of memory");

	return p;
}

/**
 * Do nothing.
 */
void ffsnd_noop(char *s, ...) {
}

/**
 * Debug trace.
 */
void ffsnd_trace(char *format, ...) {
	va_list args;

	va_start(args,format);
	vfprintf(stderr,format,args);
	va_end(args);	

	printf("\n");
}
