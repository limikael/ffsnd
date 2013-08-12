#include "ffsndout.h"
#include "ffsndutil.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

struct FFSNDOUT {
    AVFormatContext *oc;
    AVStream *audio_st;

	void *frame_samples;
	int frame_size;

	float *buffer_samples;
	int buffer_index;
};

/**
 * Clamp float value.
 */
static float clamp(float v) {
	if (v>1.0)
		v=1.0;

	if (v<-1.0)
		v=-1.0;

	return v;
}

/**
 * Pack samples for encoding.
 */
static void pack_samples(void *target, enum AVSampleFormat format, float *samples, int num_samples) {
	int16_t *int16target=(int16_t *)target;
	float *floattarget=(float *)target;
	double *doubletarget=(double *)target;
	int32_t *int32target=(int32_t *)target;

	int i;
	for (i=0; i<num_samples; i++) {
		float left=clamp(samples[i*2]);
		float right=clamp(samples[i*2+1]);

		switch (format) {
			case AV_SAMPLE_FMT_S16:
				int16target[i*2]=left*0x7fff;
				int16target[i*2+1]=right*0x7fff;
				break;

			case AV_SAMPLE_FMT_S16P:
				int16target[i]=left*0x7fff;
				int16target[i+num_samples]=right*0x7fff;
				break;

			case AV_SAMPLE_FMT_S32:
				int32target[i*2]=left*0x7fffffff;
				int32target[i*2+1]=right*0x7fffffff;
				break;

			case AV_SAMPLE_FMT_S32P:
				int32target[i]=left*0x7fffffff;
				int32target[i+num_samples]=right*0x7fffffff;
				break;

			case AV_SAMPLE_FMT_FLT:
				floattarget[i*2]=left;
				floattarget[i*2+1]=right;
				break;

			case AV_SAMPLE_FMT_FLTP:
				floattarget[i]=left;
				floattarget[i+num_samples]=right;
				break;

			case AV_SAMPLE_FMT_DBL:
				doubletarget[i*2]=left;
				doubletarget[i*2+1]=right;
				break;

			case AV_SAMPLE_FMT_DBLP:
				doubletarget[i]=left;
				doubletarget[i+num_samples]=right;
				break;

			default:
				ffsnd_fail("sample format not supported.");
				break;
		}
	}
}

/**
 * Write frame.
 */
static void ffsndout_write_frame(FFSNDOUT *ffsndout) {
	if (ffsndout->buffer_index==0)
		ffsnd_fail("got no frame to write");

	ffsndout->buffer_index=0;

    AVFrame *frame;
	AVCodecContext *c=ffsndout->audio_st->codec;

    AVPacket pkt = { 0 }; // data and size must be 0;
	frame=avcodec_alloc_frame();
	av_init_packet(&pkt);

	pack_samples(ffsndout->frame_samples,c->sample_fmt,ffsndout->buffer_samples,ffsndout->frame_size);
	frame->nb_samples = ffsndout->frame_size;

	int fillres=avcodec_fill_audio_frame(frame, c->channels, c->sample_fmt,
		(uint8_t *)ffsndout->frame_samples,
		ffsndout->frame_size *
		av_get_bytes_per_sample(c->sample_fmt) *
		c->channels, 1);

	if (fillres<0)
		ffsnd_fail("unable to fill audio frame: %d",fillres);

	int got_packet;

	int encres=avcodec_encode_audio2(c, &pkt, frame, &got_packet);

	if (encres!=0)
		ffsnd_fail("got no packet, encres=%d",encres);

//		if (got_packet) {
		pkt.stream_index=ffsndout->audio_st->index;

		/* Write the compressed frame to the media file. */
		//TRACE("*********** writing: %d\n",length);
		if (av_interleaved_write_frame(ffsndout->oc, &pkt) != 0) {
			ffsnd_fail("unable to write audio frame.");
		}
}

/**
 * Write one sample.
 */
void ffsndout_write_sample(FFSNDOUT *ffsndout, float left, float right) {
	ffsndout->buffer_samples[ffsndout->buffer_index*2]=left;
	ffsndout->buffer_samples[ffsndout->buffer_index*2+1]=right;
	ffsndout->buffer_index++;

	if (ffsndout->buffer_index==ffsndout->frame_size)
		ffsndout_write_frame(ffsndout);
}

/**
 * Encode.
 */ 
int ffsndout_write(FFSNDOUT *ffsndout, float *data, int numframes) {
	int i;

	for (i=0; i<numframes; i++)
		ffsndout_write_sample(ffsndout,data[i*2],data[i*2+1]);

	return numframes;
}

/**
 * Destructor.
 */
void ffsndout_close(FFSNDOUT *ffsndout) {
	if (ffsndout->buffer_index!=0) {
		TRACE("have data on close, 0 filling and writing");

		int i;

		for (i=ffsndout->buffer_index; i<ffsndout->frame_size; i++) {
			ffsndout->buffer_samples[i*2]=0;
			ffsndout->buffer_samples[i*2+1]=0;
		}

		ffsndout_write_frame(ffsndout);
	}

	int i;

    av_write_trailer(ffsndout->oc);

    /* Free the streams. */
    for (i = 0; i < ffsndout->oc->nb_streams; i++) {
        av_freep(&ffsndout->oc->streams[i]->codec);
        av_freep(&ffsndout->oc->streams[i]);
    }

    /* Close each codec. */
    //avcodec_close(ffsndout->audio_st->codec);

	avio_close(ffsndout->oc->pb);

    /* free the stream */
    av_free(ffsndout->oc);

	free(ffsndout);
}

/**
 * Is format available.
 */
static int is_sample_fmt_available(const enum AVSampleFormat *fmt, int cand) {
	int i=0;

	while (fmt[i]!=-1) {
		if (fmt[i]==cand)
			return 1;

		i++;
	}

	return 0;
}

/**
 * Choose format.
 */
static int choose_sample_format(const enum AVSampleFormat *available) {
	if (is_sample_fmt_available(available,AV_SAMPLE_FMT_S16))
		return AV_SAMPLE_FMT_S16;

	if (is_sample_fmt_available(available,AV_SAMPLE_FMT_S16P))
		return AV_SAMPLE_FMT_S16P;

	if (is_sample_fmt_available(available,AV_SAMPLE_FMT_S32))
		return AV_SAMPLE_FMT_S32;

	if (is_sample_fmt_available(available,AV_SAMPLE_FMT_S32P))
		return AV_SAMPLE_FMT_S32P;

	if (is_sample_fmt_available(available,AV_SAMPLE_FMT_DBL))
		return AV_SAMPLE_FMT_DBL;

	if (is_sample_fmt_available(available,AV_SAMPLE_FMT_DBLP))
		return AV_SAMPLE_FMT_DBLP;

	if (is_sample_fmt_available(available,AV_SAMPLE_FMT_FLT))
		return AV_SAMPLE_FMT_FLT;

	if (is_sample_fmt_available(available,AV_SAMPLE_FMT_FLTP))
		return AV_SAMPLE_FMT_FLTP;

    int i=0;

    while (available[i]!=-1) {
    	TRACE("sampleformat %d: %d %s\n",i,available[i],av_get_sample_fmt_name(available[i]));
    	i++;
    }

	ffsnd_fail("no suitable sample format found...");
	return 0;
}

/*
 * add an audio output stream
 */
static AVStream *add_audio_stream(AVFormatContext *oc, enum AVCodecID codec_id)
{
    AVCodecContext *c;
    AVStream *st;
    AVCodec *codec;

    /* find the audio encoder */
    TRACE("finding encoder for: %d\n",codec_id);
    codec = avcodec_find_encoder(codec_id);

//	codec = avcodec_find_encoder_by_name("libvorbis");
//	codec = avcodec_find_encoder_by_name("vorbis");
    if (!codec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }

    TRACE("got encoder: %s\n",codec->name);

    st = avformat_new_stream(oc, codec);
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        exit(1);
    }
    st->id = 1;

    c = st->codec;

    c->codec=codec;
    TRACE("default: bitrate: %d, samplerate: %d, fmt: %d\n",c->bit_rate,c->sample_rate,c->sample_fmt);

//    fail("exit");
//    codec->sample_fmts;

    /* put sample parameters */
    c->sample_fmt  = choose_sample_format(codec->sample_fmts);
    c->bit_rate    = 128000;
    c->sample_rate = 44100;
    c->channels    = 2;

    // some formats want stream headers to be separate
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return st;
}

/**
 * Open codec.
 */
static void open_audio(FFSNDOUT *ffsndout, AVFormatContext *oc, AVStream *st)
{
    AVCodecContext *c;

    c = st->codec;

    TRACE("opening audio codec: %p\n",c->codec);

    /* open it */
    if (avcodec_open2(c, NULL, NULL) < 0)
    	ffsnd_fail("could not open codec\n");

    if (c->channels!=2)
    	ffsnd_fail("expected 2 channels for output.");

    if (c->frame_size==0) {
    	TRACE("framesize is 0, setting 1024");
    	c->frame_size=1024;
    }

    TRACE("frame size: %d sample fmt: %d\n",c->frame_size,c->sample_fmt);

	ffsndout->frame_size=c->frame_size;
	ffsndout->frame_samples = av_malloc(ffsndout->frame_size *
		av_get_bytes_per_sample(c->sample_fmt) *
		c->channels);

	ffsndout->buffer_samples=malloc(ffsndout->frame_size*c->channels*sizeof(float));
	ffsndout->buffer_index=0;

	if (!ffsndout->frame_samples || !ffsndout->buffer_samples)
		ffsnd_fail("unable to malloc sample buffer");
}

/**
 * Open.
 */
FFSNDOUT *ffsndout_open(char *fn, char *format) {
	FFSNDOUT *ffsndout;

	ffsndout=ffsnd_nicemalloc(sizeof(FFSNDOUT));

	if (!fn)
		fn="pipe:1";

    av_register_all();

	avformat_alloc_output_context2(&ffsndout->oc, NULL, format, fn);
	if (!ffsndout->oc)
		ffsnd_fail("Unable to create output context.");

    AVOutputFormat *fmt=ffsndout->oc->oformat;

    TRACE("mp3 codec id: %d",AV_CODEC_ID_MP3);

/*    if (!strcmp(format,"ogg"))
		ffsndout->audio_st = add_audio_stream(ffsndout->oc, AV_CODEC_ID_LIBVORBIS);

	else*/
		ffsndout->audio_st = add_audio_stream(ffsndout->oc, fmt->audio_codec);

	open_audio(ffsndout, ffsndout->oc, ffsndout->audio_st);

	if (avio_open(&ffsndout->oc->pb, fn, AVIO_FLAG_WRITE) < 0)
		ffsnd_fail("Could not open output file.");

    avformat_write_header(ffsndout->oc, NULL);

	return ffsndout;	
}