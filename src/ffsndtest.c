#include <stdio.h>
#include "ffsndin.h"
#include "ffsndout.h"
#include "ffsndutil.h"

/**
 * Main.
 */
int main(int argc, char **argv) {

	if (argc!=3)
		ffsnd_fail("Usage: fsndtest <infile> <outfile>");

	TRACE("opening...");

	FFSNDIN *ffsndin=ffsndin_open(argv[1]);
	FFSNDOUT *ffsndout=ffsndout_open(argv[2],NULL);

	float buf[1024*2];

	while (!ffsndin_eof(ffsndin)) {
		ffsndin_read(ffsndin,buf,1024);
		ffsndout_write(ffsndout,buf,1024);
	}

	ffsndin_close(ffsndin);
	ffsndout_close(ffsndout);

	return 0;
}