#include <string.h>
#include <stdlib.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#else
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#endif


#define MPA_FRAME_SIZE 1152

#include "multicast_compressed.h"

typedef struct 
{
	/* output format context */
	AVFormatContext *oc;
	/* audio stream */
	AVStream *audio_st;
	/* audio output data */
	uint8_t *audio_outbuf;
	int audio_outbuf_size;
	int audio_input_frame_size;
	int16_t *audio_inbuf;
	int audio_inbuf_size;
	int audio_inbuf_offset;
	/* video stream */
	AVStream *video_st;
	/* video output data */
	AVFrame *picture;
	AVFrame *tmp_picture;
	uint8_t *video_outbuf;
	int frame_count, video_outbuf_size;
	/* time code mask */
	uint8_t *tc_mask;
	int tc_width;
	int tc_height;
	int tc_xoffset;
	int tc_yoffset;
} internal_mpegts_encoder_t;

typedef struct {
	char Hours;
	char Minutes;
	char Seconds;
	char Frames;
} TimeCode;

static int initialise_tc_mask (internal_mpegts_encoder_t *ts)
{
	ts->tc_width = 199;
	ts->tc_height = 24;
	ts->tc_xoffset = 460;
	ts->tc_yoffset = 30;
	//ts->tc_yoffset = ts->video_st->codec->height - 30;
	ts->tc_mask = av_mallocz (ts->tc_width* ts->tc_height);
	return 0;
}


/* add a video output stream */
AVStream *add_video_stream(AVFormatContext *oc, int codec_id, int width, int height, int bit_rate, int thread_count)
{
    //Modifiying all values to ts format values
    AVCodecContext *c;
    AVStream *st;

    oc->packet_size = 2048;
    oc->mux_rate = 10080000;
	
    st = av_new_stream(oc, 0);
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        return NULL;
    }
    avcodec_get_context_defaults2(st->codec, CODEC_TYPE_VIDEO);

    st->r_frame_rate.num = 25;
    st->r_frame_rate.den = 1;
    st->sample_aspect_ratio = av_d2q(16.0/9 * height/width, 255);
    c = st->codec;
    c->codec_id = codec_id;
    c->codec_type = CODEC_TYPE_VIDEO;

    /* put ts parameters */
    c->bit_rate = bit_rate * 1000;
    c->rc_max_rate = bit_rate * 1000; //9000000;
    c->rc_min_rate = bit_rate * 1000; //0
    c->rc_buffer_size = 224*1024*8;
    c->rc_initial_buffer_occupancy = c->rc_buffer_size * 3/4;

    /* parameters which were found to help the encoder cope better with difficult picture */    
    //c->max_qdiff = 10;
    //c->rc_eq = "tex^qComp";
    //c->qcompress = 0.1;
    
    /* resolution must be a multiple of two */
    c->width = width;  
    c->height = height;
    c->sample_aspect_ratio = st->sample_aspect_ratio;
    /* time base: this is the fundamental unit of time (in seconds) in terms
       of which frame timestamps are represented. for fixed-fps content,
       timebase should be 1/framerate and timestamp increments should be
       identically 1. */
    c->time_base.den = 25;  
    c->time_base.num = 1;
    c->gop_size = 1;		/* I-frame only encoding */
    c->pix_fmt = PIX_FMT_YUV420P;
    if (c->codec_id == CODEC_ID_MPEG2VIDEO) {
        /* just for testing, we also add B frames */
        c->max_b_frames = 2;
    }

    // some formats want stream headers to be seperate
    if(!strcmp(oc->oformat->name, "mp4") || !strcmp(oc->oformat->name, "mov") || !strcmp(oc->oformat->name, "3gp"))
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
    
    /* initialise the threads */
    if (thread_count > 0)
    {
        avcodec_thread_init(c, thread_count);
    }

// tests
    //c->qcompress = 0.05;
    //c->qblur = 0;
    //c->rc_max_rate = bit_rate * 1000;
    //c->rc_min_rate = bit_rate * 1000;
    //c->rc_buffer_aggressivity = 1.0;//0.25;
    //c->flags |= CODEC_FLAG_INTERLACED_DCT;

    //c->bit_rate_tolerance = c->bit_rate;
    //c->mb_decision = FF_MB_DECISION_RD;

    //c->rc_buffer_size=224*1024*8*4.0;

    //c->max_qdiff = 3;
    //c->frame_skip_cmp = FF_CMP_DCTMAX;
    
    //c->rc_override_count=0;
    //c->me_threshold= 0;
    //c->intra_dc_precision= 8 - 8;
    //c->strict_std_compliance = 0;
    
    return st;
}

static AVFrame *alloc_picture(int pix_fmt, int width, int height)
{
    AVFrame *picture;
    uint8_t *picture_buf;
    int size;
    
    picture = avcodec_alloc_frame();
    if (!picture)
        return NULL;
    size = avpicture_get_size(pix_fmt, width, height);
    picture_buf = malloc(size);
    if (!picture_buf) {
        av_free(picture);
        return NULL;
    }
    avpicture_fill((AVPicture *)picture, picture_buf, 
                   pix_fmt, width, height);
    return picture;
}

/* open the video stream */
static int open_video(internal_mpegts_encoder_t *ts)
{
	AVFormatContext *oc = ts->oc;
	AVStream *st = ts->video_st;
    AVCodec *codec;
    AVCodecContext *c;

    c = st->codec;

    /* find the video encoder */
    codec = avcodec_find_encoder(c->codec_id);
    if (!codec) {
        fprintf(stderr, "codec not found\n");
        return -1;
    }

    /* open the codec */
    if (avcodec_open(c, codec) < 0) {
        fprintf(stderr, "could not open codec\n");
        return -1;
    }

    ts->video_outbuf = NULL;
    if (!(oc->oformat->flags & AVFMT_RAWPICTURE)) {
        /* allocate output buffer */
        /* XXX: API change will be done */
        ts->video_outbuf_size = 550000;
        ts->video_outbuf = malloc(ts->video_outbuf_size);
    }

    /* allocate the encoded raw picture */
    ts->picture = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!ts->picture) {
        fprintf(stderr, "Could not allocate picture\n");
        return -1;
    }

	return 0;
}

static void close_video(internal_mpegts_encoder_t *ts)
{
    avcodec_close(ts->video_st->codec);
    av_free(ts->picture->data[0]);
    av_free(ts->picture);
    if (ts->tmp_picture) {
        av_free(ts->tmp_picture->data[0]);
        av_free(ts->tmp_picture);
    }
    av_free(ts->video_outbuf);
}

/* 
 * add an audio output stream
 */
static AVStream *add_audio_stream(AVFormatContext *oc, int codec_id)
{
    AVCodecContext *c;
    AVStream *st;

    st = av_new_stream(oc, 1);
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        return NULL;
    }
    avcodec_get_context_defaults2(st->codec, CODEC_TYPE_AUDIO);

    c = st->codec;
    c->codec_id = codec_id;
    c->codec_type = CODEC_TYPE_AUDIO;

    /* put sample parameters */
    c->bit_rate = 256000;
    c->sample_rate = 48000;
    c->channels = 2;
    return st;
}

/* open the audio stream */
static int open_audio(internal_mpegts_encoder_t *ts)
{
	AVStream *st = ts->audio_st;
    AVCodecContext *c;
    AVCodec *codec;

    c = st->codec;

    /* find the audio encoder */
    codec = avcodec_find_encoder(c->codec_id);
    if (!codec) {
        fprintf(stderr, "codec not found\n");
        return -1;
    }

    /* open it */
    if (avcodec_open(c, codec) < 0) {
        fprintf(stderr, "could not open codec\n");
        return -1;
    }

    ts->audio_outbuf_size = 15000;
    ts->audio_outbuf = malloc(ts->audio_outbuf_size);

    ts->audio_inbuf_size = 20000;
    ts->audio_inbuf = malloc(ts->audio_inbuf_size);
    /* ugly hack for PCM codecs (will be removed ASAP with new PCM
       support to compute the input frame size in samples */
    if (c->frame_size <= 1) {
        ts->audio_input_frame_size = ts->audio_outbuf_size / c->channels;
        switch(st->codec->codec_id) {
        case CODEC_ID_PCM_S16LE:
        case CODEC_ID_PCM_S16BE:
        case CODEC_ID_PCM_U16LE:
        case CODEC_ID_PCM_U16BE:
            ts->audio_input_frame_size >>= 1;
            break;
        default:
            break;
        }
    } else {
        ts->audio_input_frame_size = c->frame_size;
    }
	return 0;
}

void close_audio(internal_mpegts_encoder_t *ts)
{
	AVStream *st = ts->audio_st;
    avcodec_close(st->codec);
    
    av_free(ts->audio_outbuf);
    av_free(ts->audio_inbuf);
}

static void cleanup (internal_mpegts_encoder_t *ts)
{
	if (ts)
	{
		if (ts->video_st)
			close_video(ts);
		if (ts->audio_st)
			close_audio(ts);

		if (ts->oc)
		{
			unsigned i;
			//free the streams
			for (i = 0; i < ts->oc->nb_streams; i++)
			{
                av_freep(&ts->oc->streams[i]->codec);
				av_freep(&ts->oc->streams[i]);
			}
			av_free(ts->oc);
		}
		if (ts->tc_mask)
			av_free(ts->tc_mask);

		av_free (ts);
		ts = NULL;
	}
}

extern mpegts_encoder_t *mpegts_encoder_init (const char *filename, int width, int height, uint32_t kbit_rate, int thread_count)
{
	internal_mpegts_encoder_t *mpegts;
	AVOutputFormat *fmt;

	/* register all ffmpeg codecs and formats */
	av_register_all();

	/* Check if mpegts format exists */
	fmt = guess_format("mpegts", NULL, NULL);
	if (!fmt) {
		fprintf (stderr, "format not registered\n");
		return NULL;
	}

	mpegts = av_mallocz (sizeof(internal_mpegts_encoder_t));
	if (!mpegts) {
		fprintf (stderr, "Could not allocate encoder object\n");
		return NULL;
	}

	/* Allocate the output media context */
	mpegts->oc = av_alloc_format_context();
	if (!mpegts->oc)
	{
		cleanup(mpegts);
		fprintf (stderr, "Could not allocate output format context\n");
		return NULL;
	}
	mpegts->oc->oformat = fmt;
	snprintf (mpegts->oc->filename, sizeof(mpegts->oc->filename), "%s", filename);

	// H264, MPEG2VIDEO, MPEG1VIDEO, NONE, H263, MJPEG, MPEG4, RAWVIDEO, MSMPEG4V2, FLV1, XVID, RV40, VC1, FFH264
	// segv: H264, MPEG1VIDEO
	// SIGFPE: RAWVIDEO
	// other fail: MPEG2VIDEO, MPEG4, MSMPEG4V2
    fmt->video_codec = CODEC_ID_MPEG2VIDEO;
	fmt->audio_codec = CODEC_ID_MP2;

	/* 
	* add the audio and videostreams using the default format codecs and
	* initialise the codecs
	*/
	if (fmt->video_codec != CODEC_ID_NONE)
	{
		mpegts->video_st = add_video_stream(mpegts->oc, fmt->video_codec, width, height, kbit_rate, thread_count);
		if (!mpegts->video_st)
		{
			cleanup(mpegts);
			return NULL;
		}
	}
	if (fmt->audio_codec != CODEC_ID_NONE)
	{
		mpegts->audio_st = add_audio_stream(mpegts->oc, fmt->audio_codec);
		if (!mpegts->audio_st)
		{
			cleanup(mpegts);
			return NULL;
		}
	}

	/* Set the output parameters - must be done even if no parameters */
	if (av_set_parameters(mpegts->oc, NULL) < 0)
	{
		fprintf(stderr, "Invalid output format parameters\n");
		cleanup(mpegts);
		return NULL;
	}

    /* set preload and maximum delay to avoid vob buffer underflow errors */
    mpegts->oc->preload = (int)(0.5 * AV_TIME_BASE); /* 500 ms */
    mpegts->oc->max_delay = (int)(0.7 * AV_TIME_BASE); /* 700ms */
    mpegts->oc->loop_output = AVFMT_NOOUTPUTLOOP;

	/* open the audio and video codecs */
	if (open_video(mpegts) == -1) {
		return NULL;
	}
	if (open_audio(mpegts) == -1) {
		return NULL;
	}

	/*	open the output file */
	if (!(fmt->flags & AVFMT_NOFILE)) {
        if (url_fopen(&mpegts->oc->pb, filename, URL_WRONLY) < 0) {
			cleanup(mpegts);
            fprintf(stderr, "Could not open '%s'\n", filename);
            return NULL;
        }
    }

	initialise_tc_mask (mpegts);

	/* write stream header if any */
	av_write_header(mpegts->oc);

	return (mpegts_encoder_t *)mpegts;
}

static int write_audio_frame(internal_mpegts_encoder_t *ts, const int16_t *p_audio)
{
	AVFormatContext *oc = ts->oc;
	AVStream *st = ts->audio_st;
	AVCodecContext *c;
    AVPacket pkt;
    av_init_packet(&pkt);
    
    c = st->codec;

    pkt.size= avcodec_encode_audio(c, ts->audio_outbuf, ts->audio_outbuf_size, p_audio);

    //static int g_frame_count = 0;

    pkt.pts= av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base);
    pkt.flags |= PKT_FLAG_KEY;
    pkt.stream_index= st->index;
    pkt.data= ts->audio_outbuf;

    /* write the compressed frame in the media file */
//    if (av_interleaved_write_frame(oc, &pkt) != 0) {
    if (av_write_frame(oc, &pkt) != 0) {
        fprintf(stderr, "Error while writing audio frame\n");
        return -1;
    }
	return 0;
}

static void fill_yuv_image (const uint8_t *p_video, AVFrame *pict, int width, int height)
{
	int chroma_size = (width * height)/4;
	const uint8_t *u_comp = p_video + (width*height);
	const uint8_t *v_comp = u_comp + (width*height)/4;
	/* Y */
	memcpy (pict->data[0], p_video, width * height);
	/* U */
	memcpy (pict->data[1], u_comp, chroma_size);
	/* V */
	memcpy (pict->data[2], v_comp, chroma_size);
}

static int write_video_frame(internal_mpegts_encoder_t *ts, const uint8_t *p_video, int32_t frame_num)
{
	AVFormatContext *oc = ts->oc;
	AVStream *st = ts->video_st;
    int out_size, ret;
    AVCodecContext *c;
    
    c = st->codec;
    
    fill_yuv_image(p_video, ts->picture, c->width, c->height);

    /* encode the image */
    out_size = avcodec_encode_video(c, ts->video_outbuf, ts->video_outbuf_size, ts->picture);
    
    /* if zero size, it means the image was buffered */
    if (out_size > 0) {
  		//static int g_frame_count = 0;
        AVPacket pkt;
        av_init_packet(&pkt);
            
        pkt.pts= av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base);
        //pkt.pts = g_frame_count++;
        if(c->coded_frame->key_frame)
            pkt.flags |= PKT_FLAG_KEY;
        pkt.stream_index= st->index;
        pkt.data= ts->video_outbuf;
        pkt.size= out_size;
        
		if (0)
			printf("pkt: pts=%20"PRIi64" dts=%20"PRIi64" sidx=%d fl=%d dur=%d pos=%"PRIi64" size=%5d data[0..7]={%02x %02x %02x %02x %02x %02x %02x %02x}\n",
				pkt.pts,
				pkt.dts,
				pkt.stream_index,
				pkt.flags,
				pkt.duration,
				pkt.pos,
				pkt.size,
				pkt.data[0], pkt.data[1], pkt.data[2], pkt.data[3], pkt.data[4], pkt.data[5], pkt.data[6], pkt.data[7]
				);

        /* write the compressed frame in the media file */
//        ret = av_interleaved_write_frame(oc, &pkt);
        ret = av_write_frame(oc, &pkt);
    } else {
        ret = 0;
    }
    if (ret != 0) {
        fprintf(stderr, "Error while writing video frame\n");
        return -1;
    }
    ts->frame_count++;
	return ret;
}

extern int mpegts_encoder_encode (mpegts_encoder_t *in_ts, const uint8_t *p_video, const int16_t *p_audio, int32_t frame_number)
{
	internal_mpegts_encoder_t *ts = (internal_mpegts_encoder_t *)in_ts;
	if (!ts)
		return -1;

	if (p_video && write_video_frame (ts, p_video, frame_number) == -1)
			return -1;

	if (p_audio)
	{
        /*
        Audio comes in in video frames (1920 samples) but is encoded
        as MPEG frames (1152 samples) so we need to buffer.
        NB. audio_inbuf_offset is in terms of 16-bit samples, hence the
        rather confusing pointer arithmetic.
        */
		memcpy (ts->audio_inbuf + ts->audio_inbuf_offset, p_audio, 1920*2*2);
		ts->audio_inbuf_offset += (1920*2);
		while (ts->audio_inbuf_offset >= (MPA_FRAME_SIZE*4))
		{
			if (write_audio_frame (ts, ts->audio_inbuf) == -1)
				return -1;
			ts->audio_inbuf_offset -= (MPA_FRAME_SIZE*2);
			memmove (ts->audio_inbuf, ts->audio_inbuf+(MPA_FRAME_SIZE*2), ts->audio_inbuf_offset*2);
		}
	}
	return 0;
}

extern int mpegts_encoder_close (mpegts_encoder_t *in_ts)
{
	internal_mpegts_encoder_t *ts = (internal_mpegts_encoder_t *)in_ts;
	if (!ts)
		return -1;

	if (ts->video_st)
	{
		close_video(ts);
		ts->video_st = NULL;
	}
	if (ts->audio_st)
	{
		close_audio(ts);
		ts->audio_st = NULL;
	}
	/* write the trailer, if any */
    av_write_trailer(ts->oc);
	if (!(ts->oc->oformat->flags & AVFMT_NOFILE))
	{
        /* close the output file */
		// causes runtine error: *** glibc detected *** free(): invalid pointer: ...
        //url_fclose(&ts->oc->pb);
    }
	cleanup (ts);
	return 0;
}

#ifdef DEBUG_MPEGTS_MAIN
#include <sys/time.h>		// gettimeofday()
extern int main (int argc, char **argv)
{
	mpegts_encoder_t *ts;
	int frame_num = -1;
	FILE *vfp;
	FILE *afp;
	int video_frame_size = 720 * 576 * 3/2;
	uint8_t video_samples[video_frame_size];
	int frames_read;
	int idx = 1;
    struct timeval now, prev;

	if (argc < 4)
	{
		fprintf (stderr, "Usage: %s  planar_yuv_420_file pcm_file output_mpg_file\n", argv[0]);
		exit(-1);
	}

	if ((vfp = fopen (argv[idx], "rb")) == NULL)
	{
		perror ("Error: ");
		exit(-1);
	}

	if ((afp = fopen (argv[idx+1], "rb")) == NULL)
	{
		perror ("Error: ");
		exit(-1);
	}
#if DEBUG_MPEGTS_INPUT_WAV
	/* Seek past first 44 bytes in wav file */
	if (fseek(afp, 44L, SEEK_SET) != 0)
	{
		perror ("Error: ");
		exit(-1);
	}
#endif
	ts = mpegts_encoder_init(argv[idx+2], 720, 576, 2700, 4);
	if (ts == NULL) {
		return 1;
	}
	
    gettimeofday(&prev, NULL);
	while ((frames_read = fread (video_samples, video_frame_size, 1, vfp)) == 1)
	{
		int16_t audio_samples[1920*2];

		if (fread (audio_samples, sizeof(audio_samples), 1, afp) != 1)
			break;
		if (mpegts_encoder_encode (ts, video_samples, audio_samples, frame_num)!= 0)
			fprintf (stderr, "mpegts_encoder_encode failed\n");
		if (frame_num >= 0)
			++frame_num;

        if (frame_num % 25 == 0) {
            gettimeofday(&now, NULL);
            printf("fps=%.1f\n", 25.0/(((now.tv_sec - prev.tv_sec) * 1000000 + now.tv_usec - prev.tv_usec) / 1000000.0));
            prev = now;
        }
	}
	mpegts_encoder_close(ts);
	fclose(afp);
	fclose(vfp);

	return 0;
}
#endif
