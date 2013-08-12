ffsnd
=====

Wrapper for reading and writing sound files using the ffmpeg library.

Reading is done like so:

    #include "ffsndin.h"

    FFSNDIN *ffsndin;
    float buffer[1024*2]; // Always float, 2 channels
    
    ffsndin=ffsndin_open("my_file.mp3");
    while (!ffsndin_eof(ffsndin))
        ffsndin_read(ffsndin,buffer,1024);

    ffsndin_close(ffsndin);

And writing:

    #include "ffsndout.h"

    FFSNDIN *ffsndout;
    float buffer[1024*2]; // Always float, 2 channels
    
    ffsndout=ffsndout_open("my_file.mp3",NULL); // Second argument is format, NULL=guess from filename
    ffsndout_write(ffsndout,buffer,1024);
    ffsndout_close(ffsndout);
