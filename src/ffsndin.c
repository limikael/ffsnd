#include "ffsndin.h"
#include "ffsndutil.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

struct FFSNDIN {
	AVFormatContext *format_context;
	AVCodecContext *codec_context;
	AVCodec *codec;
	int stream_index;

	float *buffer;
	int buffer_index;
	int buffer_size;

	int eof_detected;
};

static int initialized;

/**
 * Read packet for our stream.
 */
static void read_next_stream_packet(FFSNDIN *pragmasound, AVPacket *packet) {
	int ret;

	while(1) {
		ret=av_read_frame(pragmasound->format_context,packet);

		if (ret<0) {
			if (ret==AVERROR_EOF)
				pragmasound->eof_detected=1;

			else
				ffsnd_fail("Unable to read frame: %d",ret);
		}

		if (packet->stream_index==pragmasound->stream_index)
			return;
	}
}

/**
 * Set size of internal buffer.
 */
static void check_buffer(FFSNDIN *pragmasound, int samples) {
	if (samples!=pragmasound->buffer_size) {
		TRACE("reallocating buffers in sound reader: %d",samples);
		pragmasound->buffer_size=samples;
		pragmasound->buffer=ffsnd_nicerealloc(pragmasound->buffer,2*sizeof(float)*pragmasound->buffer_size);
	}
}

/**
 * Fill with zeros.
 */
static void fill_zero_buffer(FFSNDIN *pragmasound) {
	int i;

	check_buffer(pragmasound,1024);

	for (i=0; i<pragmasound->buffer_size*2; i++) {
		pragmasound->buffer[i]=(float)0;
	}

	pragmasound->buffer_index=0;
}

/**
 * Fill buffer with data.
 */
static void fill_buffer(FFSNDIN *pragmasound) {
	AVPacket packet;
	AVFrame frame;
	int got_frame=0;
	int ret;

	if (pragmasound->eof_detected) {
		fill_zero_buffer(pragmasound);
		return;
	}

	//TRACE("reading packet");
	read_next_stream_packet(pragmasound,&packet);

	if (pragmasound->eof_detected) {
		fill_zero_buffer(pragmasound);
		return;
	}

	avcodec_get_frame_defaults(&frame);
 	got_frame = 0;
	ret = avcodec_decode_audio4(pragmasound->codec_context, &frame, &got_frame, &packet);
 	av_free_packet(&packet);

	if (ret<0)
		ffsnd_fail("Error decoding audio");

	if (!got_frame)
		ffsnd_fail("Didn't get any frame");

	check_buffer(pragmasound,frame.nb_samples);
	pragmasound->buffer_index=0;

	if (frame.sample_rate!=44100)
		ffsnd_fail("samplerate must be 44100Hz");

	if (pragmasound->codec_context->channels!=2)
		ffsnd_fail("only 2 channels supported");

	/*if (av_sample_fmt_is_planar(pragmasound->codec_context->sample_fmt))
		fail("cannot handle planar audio formats");*/

	int i;

	int16_t *int16_buf_0=(int16_t *)frame.extended_data[0];
	int16_t *int16_buf_1=(int16_t *)frame.extended_data[1];

	switch (frame.format) {
		case AV_SAMPLE_FMT_S16:
			for (i=0; i<frame.nb_samples*2; i++)
				pragmasound->buffer[i]=(float)int16_buf_0[i]/(float)0x7fff;
			break;

		case AV_SAMPLE_FMT_S16P:
			for (i=0; i<frame.nb_samples; i++) {
				pragmasound->buffer[i*2]=(float)int16_buf_0[i]/(float)0x7fff;
				pragmasound->buffer[i*2+1]=(float)int16_buf_1[i]/(float)0x7fff;
			}
			break;

		default:
			ffsnd_fail("Unsupported encoding byte layout: %d expected: %d",frame.format,AV_SAMPLE_FMT_S16P);
			break;
	}

	//TRACE("ret");
}

/**
 * Read.
 */
int ffsndin_read(FFSNDIN *pragmasound, float *target, size_t values) {
	int i;

	//TRACE("reading: %d",values);
	for (i=0; i<values; i++) {
		if (pragmasound->buffer_index>=pragmasound->buffer_size) {
			//TRACE("fill buffer");
			fill_buffer(pragmasound);
			//TRACE("buffer filled");
		}

		if (pragmasound->buffer_index>=pragmasound->buffer_size) {
			//TRACE("returning from read: %d",i);
			return i;
		}

		//TRACE("filling at: %d",i);
		target[i*2]=pragmasound->buffer[pragmasound->buffer_index*2];
		target[i*2+1]=pragmasound->buffer[pragmasound->buffer_index*2+1];
		pragmasound->buffer_index++;
	}

	//TRACE("return from read");
	return values;
}

/**
 * Seek to pos.
 */
void ffsndin_seek(FFSNDIN *pragmasound, size_t frames) {
	pragmasound->eof_detected=0;

	double pos_in_sec=frames/44100;

	TRACE("seeking to frames: %d",frames);
	TRACE("timebase is: %d",AV_TIME_BASE);

	//long long seek_target=frames*AV_TIME_BASE/44100;
	long long seek_target=pos_in_sec*AV_TIME_BASE;
//	double d=seek_target;
	TRACE("initial seek target: %lld",seek_target);

	int ret;

	if (pragmasound->buffer)
		free(pragmasound->buffer);

	pragmasound->buffer=NULL;
	pragmasound->buffer_index=0;
	pragmasound->buffer_size=0;

//    seek_target=AV_TIME_BASE*16;

    seek_target=av_rescale_q(seek_target, AV_TIME_BASE_Q,
    	pragmasound->format_context->streams[pragmasound->stream_index]->time_base);

    TRACE("seektarget: %lld",seek_target);

    ret=av_seek_frame(pragmasound->format_context,
    	pragmasound->stream_index,
    	seek_target,0);

    if (ret<0)
		ffsnd_fail("unable to seek in stream.");
}

/**
 * At eof?
 */
int ffsndin_eof(FFSNDIN *pragmasound) {
	return pragmasound->eof_detected;
}

/**
 * Close.
 */
void ffsndin_close(FFSNDIN *pragmasound) {
	avformat_close_input(&pragmasound->format_context);
	//fail("not implemented");
}

/**
 * Open.
 */
FFSNDIN *ffsndin_open(const char *fn) {
	FFSNDIN *sound;
	int ret;

	if (!initialized) {
		avcodec_register_all();
		av_register_all();
		initialized=1;
	}

	sound=ffsnd_nicemalloc(sizeof (FFSNDIN));
	sound->format_context=NULL;

	ret=avformat_open_input(&sound->format_context,fn,NULL,NULL);
	if (ret<0)
		ffsnd_fail("Unable to open input file.");

	ret=avformat_find_stream_info(sound->format_context,NULL);
	if (ret<0)
		ffsnd_fail("Unable to find stream info.");

	sound->stream_index=av_find_best_stream(
		sound->format_context,
		AVMEDIA_TYPE_AUDIO,
		-1,-1,&sound->codec,0);

	if (sound->stream_index<0)
		ffsnd_fail("Can't find audio stream.");

	sound->codec_context=sound->format_context->streams[sound->stream_index]->codec;

	ret=avcodec_open2(sound->codec_context,sound->codec, NULL);
	if (ret<0)
		ffsnd_fail("Can't open decoder.");

	AVStream *stream=sound->format_context->streams[sound->stream_index];

	TRACE("stream: time_base=%d/%d r_frame_rate=%d/%d",
		stream->time_base.num,stream->time_base.den,
		stream->r_frame_rate.num,stream->r_frame_rate.den);

	sound->buffer=NULL;
	sound->buffer_index=0;
	sound->buffer_size=0;
	sound->eof_detected=0;

	return sound;
//	fail("reached");
}

/**
 * Get number of frames.
 */
size_t ffsndin_get_num_frames(FFSNDIN *ffsndin) {
        long long d=ffsndin->format_context->duration;
        long long e=44100*d;
        long long ret=e/AV_TIME_BASE;

//      TRACE("duration: %d\n",ret);
        return ret;
}

/**
 * Num channels.
 */
int ffsndin_get_num_channels(FFSNDIN *pragmasound) {
	return 2;
}

/**
 * Samplerate.
 */
int ffsndin_get_samplerate(FFSNDIN *pragmasound) {
	return 44100;
}
