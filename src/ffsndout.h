#ifndef __FFSNDOUT_H__
#define __FFSNDOUT_H__

typedef struct FFSNDOUT FFSNDOUT;

FFSNDOUT *ffsndout_open(char *fn, char *format);
void ffsndout_close(FFSNDOUT *ffsndout);
int ffsndout_write(FFSNDOUT *ffsndout, float *target, int samples);

#endif