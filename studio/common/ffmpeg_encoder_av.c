/*
 * $Id: ffmpeg_encoder_av.c,v 1.10 2010/01/12 17:01:00 john_f Exp $
 *
 * Encode AV and write to file.
 *
 * Copyright (C) 2005  British Broadcasting Corporation
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/*
This file is based on ffmpeg/output_example.c from the ffmpeg source tree.
*/

#include <string.h>
#include <stdlib.h>

#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#include <ffmpeg/swscale.h>
#else
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#endif

#include "ffmpeg_encoder_av.h"

#define MPA_FRAME_SIZE              1152 // taken from ffmpeg's private mpegaudio.h
#define PAL_VIDEO_FRAME_AUDIO_SIZE  1920
#define MAX_AUDIO_STREAMS 8

typedef struct
{
    AVStream * audio_st;
    int num_audio_channels;
    int audio_samples_per_input_frame;
    int audio_samples_per_output_frame;
    short * audio_inbuf;
    int audio_inbuf_size;
    int audio_inbuf_offset;
    uint8_t * audio_outbuf;
    int audio_outbuf_size;
} audio_encoder_t;

/*
Note that avcodec_encode_audio wants a short * for input audio samples.
Need to check what is really happening regarding input sample size and
whether we rely on sizeof(short) being 2.
*/


typedef struct 
{
    // output format context
    AVFormatContext * oc;

    // video
    AVStream * video_st; ///< video stream
    AVFrame * inputFrame;
    uint8_t * video_outbuf;
    int video_outbuf_size;
    int64_t video_pts;

    // audio
    int num_audio_streams;
    audio_encoder_t * audio_encoder[MAX_AUDIO_STREAMS];

    // following needed for video scaling
    AVPicture * tmpFrame;
    uint8_t * inputBuffer;
    int input_width;
    int input_height;
    struct SwsContext * scale_context;
    int scale_image;
} internal_ffmpeg_encoder_t;



/* initialise video stream for DVD encoding */
static int init_video_dvd(internal_ffmpeg_encoder_t * enc)
{
    AVCodecContext * codec_context = enc->video_st->codec;

    /* mpeg2video is the default codec for DVD format */
    codec_context->codec_id = CODEC_ID_MPEG2VIDEO;
    codec_context->codec_type = CODEC_TYPE_VIDEO;
    codec_context->pix_fmt = PIX_FMT_YUV420P;

    int width = 720;
    int height = 576;
    avcodec_set_dimensions(codec_context, width, height);

    const int dvd_kbit_rate = 5000;

    enc->video_st->r_frame_rate.num = 25;
    enc->video_st->r_frame_rate.den = 1;


    /* put dvd parameters */
    codec_context->bit_rate = dvd_kbit_rate * 1000;
    codec_context->rc_max_rate = 9000000;
    codec_context->rc_min_rate = 0;
    codec_context->rc_buffer_size = 224*1024*8;
    codec_context->gop_size = 15; /* emit one intra frame every 15 frames at most */

    if (codec_context->codec_id == CODEC_ID_MPEG2VIDEO)
    {
        /* just for testing, we also add B frames */
        codec_context->max_b_frames = 2;
    }
    if (codec_context->codec_id == CODEC_ID_MPEG1VIDEO)
    {
        /* needed to avoid using macroblocks in which some coeffs overflow 
           this doesnt happen with normal video, it just happens here as the 
           motion of the chroma plane doesnt match the luma plane */
        codec_context->mb_decision = 2;
    }

    /* find the video encoder */
    AVCodec * codec = avcodec_find_encoder(codec_context->codec_id);
    if (!codec)
    {
        fprintf(stderr, "video codec id=%d not found\n",
            codec_context->codec_id);
        return -1;
    }

    /* open the codec */
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open video codec\n");
        return -1;
    }

    /* allocate input video frame */
    enc->inputFrame = avcodec_alloc_frame();
    if (!enc->inputFrame)
    {
        fprintf(stderr, "Could not allocate input frame\n");
        return 0;
    }
    enc->inputFrame->top_field_first = 1;

    /* allocate output buffer */
    enc->video_outbuf = NULL;
    if (!(enc->oc->oformat->flags & AVFMT_RAWPICTURE))
    {
        enc->video_outbuf_size = 550000;
        enc->video_outbuf = malloc(enc->video_outbuf_size);
    }

    return 0;
}

/* initialise video stream for MPEG-4 encoding */
static int init_video_mpeg4(internal_ffmpeg_encoder_t * enc)
{
    AVCodecContext * codec_context = enc->video_st->codec;

    codec_context->codec_id = CODEC_ID_MPEG4;
    codec_context->codec_type = CODEC_TYPE_VIDEO;
    codec_context->pix_fmt = PIX_FMT_YUV420P;

    int width = 720;
    int height = 576;
    avcodec_set_dimensions(codec_context, width, height);

    /* bit rate */
    const int kbit_rate = 800;

    enc->video_st->r_frame_rate.num = 25;
    enc->video_st->r_frame_rate.den = 1;

    /* set coding parameters */
    codec_context->bit_rate = kbit_rate * 1000;


    // some formats want stream headers to be seperate
    if(!strcmp(enc->oc->oformat->name, "mp4")
        || !strcmp(enc->oc->oformat->name, "mov")
        || !strcmp(enc->oc->oformat->name, "3gp"))
    {
        codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    /* find the video encoder */
    AVCodec * codec = avcodec_find_encoder(codec_context->codec_id);
    if (!codec)
    {
        fprintf(stderr, "video codec id=%d not found\n",
            codec_context->codec_id);
        return -1;
    }

    /* open the codec */
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open video codec\n");
        return -1;
    }

    /* allocate input video frame */
    enc->inputFrame = avcodec_alloc_frame();
    if (!enc->inputFrame)
    {
        fprintf(stderr, "Could not allocate input frame\n");
        return 0;
    }
    enc->inputFrame->top_field_first = 1;

    /* allocate output buffer */
    enc->video_outbuf = NULL;
    if (!(enc->oc->oformat->flags & AVFMT_RAWPICTURE))
    {
        // not sure what size is appropriate for mp4
        enc->video_outbuf_size = 550000;
        enc->video_outbuf = malloc(enc->video_outbuf_size);
    }

    return 0;
}

/* initialise video stream for DV encoding */
static int init_video_dv(internal_ffmpeg_encoder_t * enc, ffmpeg_encoder_av_resolution_t res, int64_t start_tc)
{
    AVCodecContext * codec_context = enc->video_st->codec;

    codec_context->codec_id = CODEC_ID_DVVIDEO;
    codec_context->codec_type = CODEC_TYPE_VIDEO;

    int encoded_frame_size = 0;
    int top_field_first = 1;
    int width = 720;
    int height = 576;

    switch (res)
    {
    case FF_ENCODER_RESOLUTION_DV25_MOV:
        codec_context->pix_fmt = PIX_FMT_YUV420P;
        encoded_frame_size = 144000;
        top_field_first = 0;
        break;
    case FF_ENCODER_RESOLUTION_DV50_MOV:
        codec_context->pix_fmt = PIX_FMT_YUV422P;
        encoded_frame_size = 288000;
        top_field_first = 0;
        break;
    case FF_ENCODER_RESOLUTION_DV100_MOV:
        codec_context->pix_fmt = PIX_FMT_YUV422P;
        enc->scale_image = 1;
        enc->input_width = 1920;
        enc->input_height = 1080;
        width = 1440;                       // coded width (input scaled horizontally from 1920)
        height = 1080;
        enc->tmpFrame = av_mallocz(sizeof(AVPicture));
        enc->inputBuffer = av_mallocz(width * height * 2);
        encoded_frame_size = 576000;        // SMPTE 370M spec
        top_field_first = 1;
        break;
    default:
        break;
    }

    avcodec_set_dimensions(codec_context, width, height);

    enc->video_st->r_frame_rate.num = 25;
    enc->video_st->r_frame_rate.den = 1;

    /* Setting this gives us a timecode track in MOV format */
    codec_context->timecode_frame_start = start_tc;


    // some formats want stream headers to be seperate
    if (!strcmp(enc->oc->oformat->name, "mp4")
        || !strcmp(enc->oc->oformat->name, "mov")
        || !strcmp(enc->oc->oformat->name, "3gp"))
    {
        codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    /* find the video encoder */
    AVCodec * codec = avcodec_find_encoder(codec_context->codec_id);
    if (!codec)
    {
        fprintf(stderr, "video codec id=%d not found\n",
            codec_context->codec_id);
        return -1;
    }

    /* open the codec */
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open video codec\n");
        return -1;
    }

    /* allocate input video frame */
    enc->inputFrame = avcodec_alloc_frame();
    if (!enc->inputFrame)
    {
        fprintf(stderr, "Could not allocate input frame\n");
        return 0;
    }
    enc->inputFrame->top_field_first = top_field_first;

    /* allocate output buffer */
    enc->video_outbuf = NULL;
    if (!(enc->oc->oformat->flags & AVFMT_RAWPICTURE))
    {
        enc->video_outbuf_size = encoded_frame_size;
        enc->video_outbuf = malloc(enc->video_outbuf_size);
    }

    return 0;
}

/* initialise audio streams for PCM encoding */
static int init_audio_pcm(audio_encoder_t * aenc)
{
    AVCodecContext * codec_context = aenc->audio_st->codec;

    /* 16-bit signed big-endian */
    codec_context->codec_id = CODEC_ID_PCM_S16BE;
    codec_context->codec_type = CODEC_TYPE_AUDIO;

    /* find the audio encoder */
    AVCodec * codec = avcodec_find_encoder(codec_context->codec_id);
    if (!codec)
    {
        fprintf(stderr, "audio codec id=%d not found\n",
            codec_context->codec_id);
        return -1;
    }

    /* coding parameters */
    codec_context->sample_rate = 48000;
    codec_context->channels = aenc->num_audio_channels;

    /* open it */
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open audio codec\n");
        return -1;
    }

    /* Set up audio buffer parameters */
    int nch = aenc->num_audio_channels;
    aenc->audio_samples_per_input_frame = PAL_VIDEO_FRAME_AUDIO_SIZE;
    aenc->audio_inbuf_size = aenc->audio_samples_per_input_frame * nch * sizeof(short);

    aenc->audio_samples_per_output_frame = PAL_VIDEO_FRAME_AUDIO_SIZE;
    /* coded frame size */
    aenc->audio_outbuf_size = aenc->audio_samples_per_output_frame * nch * sizeof(short);

    return 0;
}

/* initialise audio stream for DVD encoding */
static int init_audio_dvd(audio_encoder_t * aenc)
{
    AVCodecContext * codec_context = aenc->audio_st->codec;

    /* mp2 is the default audio codec for DVD format */
    codec_context->codec_id = CODEC_ID_MP2;
    codec_context->codec_type = CODEC_TYPE_AUDIO;

    /* find the audio encoder */
    AVCodec * codec = avcodec_find_encoder(codec_context->codec_id);
    if (!codec)
    {
        fprintf(stderr, "audio codec id=%d not found\n",
            codec_context->codec_id);
        return -1;
    }

    /* coding parameters */
    codec_context->bit_rate = 256000;
    codec_context->sample_rate = 48000;
    codec_context->channels = 2;

    /* open it */
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open audio codec\n");
        return -1;
    }

    /* Set up audio buffer parameters */
    int nch = aenc->num_audio_channels;
    aenc->audio_samples_per_input_frame = PAL_VIDEO_FRAME_AUDIO_SIZE;
    /* We have to do some buffering of input, hence the * 2 on the end */
    aenc->audio_inbuf_size = aenc->audio_samples_per_input_frame * nch * sizeof(short) * 2;

    /* samples per coded frame */
    aenc->audio_samples_per_output_frame = MPA_FRAME_SIZE;
    /* coded frame size */
    aenc->audio_outbuf_size = 960; // enough for 320 kbit/s

    return 0;
}

/* initialise audio stream for MP3 encoding */
static int init_audio_mp3(audio_encoder_t * aenc)
{
    AVCodecContext * codec_context = aenc->audio_st->codec;

    /* We use mp3 codec rather than default */
    const int codec_id = CODEC_ID_MP3;
    const int kbit_rate = 96;

    codec_context->codec_id = codec_id;
    codec_context->codec_type = CODEC_TYPE_AUDIO;

    /* find the audio encoder */
    AVCodec * codec = avcodec_find_encoder(codec_context->codec_id);
    if (!codec)
    {
        fprintf(stderr, "audio codec id=%d not found\n",
            codec_context->codec_id);
        return -1;
    }

    /* coding parameters */
    codec_context->bit_rate = kbit_rate * 1000;
    codec_context->sample_rate = 48000;
    codec_context->channels = 2;

    /* open it */
    if (avcodec_open(codec_context, codec) < 0)
    {
        fprintf(stderr, "could not open audio codec\n");
        return -1;
    }

    /* Set up audio buffer parameters */
    int nch = aenc->num_audio_channels;
    aenc->audio_samples_per_input_frame = PAL_VIDEO_FRAME_AUDIO_SIZE;
    /* We have to do some buffering of input, hence the * 2 on the end */
    aenc->audio_inbuf_size = aenc->audio_samples_per_input_frame * nch * sizeof(short) * 2;

    /* samples per coded frame */
    aenc->audio_samples_per_output_frame = MPA_FRAME_SIZE;
    /* coded frame size */
    aenc->audio_outbuf_size = 960; // enough for 320 kbit/s

    return 0;
}

static void cleanup (internal_ffmpeg_encoder_t * enc)
{
    if (enc)
    {
        if (enc->oc)
        {
            // free the streams
            unsigned int i;
            for (i = 0; i < enc->oc->nb_streams; ++i)
            {
                av_freep(&enc->oc->streams[i]);
            }
            av_free(enc->oc);
        }

        av_free(enc->inputFrame);
        av_free(enc->video_outbuf);
        av_free(enc->inputBuffer);
        av_free(enc->tmpFrame);

        int i;
        for (i = 0; i < enc->num_audio_streams; ++i)
        {
            audio_encoder_t * aenc = enc->audio_encoder[i];
            av_free(aenc->audio_outbuf);
            av_free(aenc->audio_inbuf);
            av_free(aenc);
        }

        av_free(enc);
        enc = NULL;
    }
}


static int write_video_frame(internal_ffmpeg_encoder_t * enc, uint8_t * p_video)
{
    AVFormatContext * oc = enc->oc;
    AVStream * st = enc->video_st;
    AVCodecContext * c = enc->video_st->codec;
    int out_size;
    int ret;
    
    if (enc->scale_image)
    {
        // Set up parameters for scale
        enc->scale_context = sws_getCachedContext(enc->scale_context,
                    enc->input_width, enc->input_height,                   // input WxH
                    c->pix_fmt,
                    c->width, c->height, // output WxH
                    c->pix_fmt,
                    SWS_FAST_BILINEAR,
                    NULL, NULL, NULL);

        // Input image goes into tmpFrame
        avpicture_fill((AVPicture*)enc->tmpFrame, p_video,
            c->pix_fmt,
            enc->input_width, enc->input_height);

        // Output of scale operation goes into inputFrame
        avpicture_fill((AVPicture*)enc->inputFrame, enc->inputBuffer,
            c->pix_fmt,
            c->width, c->height);

        // Perform the scale
        sws_scale(enc->scale_context,
            enc->tmpFrame->data, enc->tmpFrame->linesize,
            0, enc->input_height,
            enc->inputFrame->data, enc->inputFrame->linesize);
    }
    else
    {
        /* set pointers in inputFrame to point to planes in p_video */
        avpicture_fill((AVPicture *)enc->inputFrame, p_video,
            c->pix_fmt,
            c->width, c->height);
    }

    /* encode the image */
    out_size = avcodec_encode_video(c, enc->video_outbuf, enc->video_outbuf_size, enc->inputFrame);

    if (out_size > 0)
    {
        AVPacket pkt;
        av_init_packet(&pkt);
            
        const int64_t av_nopts_value = AV_NOPTS_VALUE; // avoids compiler warning
        if (c->coded_frame->pts != av_nopts_value)
        {
            pkt.pts = av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base);
        }
        else
        {
            pkt.pts = av_rescale_q(enc->video_pts++, c->time_base, st->time_base);
        }
        if (c->coded_frame->key_frame)
        {
            pkt.flags |= PKT_FLAG_KEY;
        }
        pkt.stream_index = st->index;
        pkt.data = enc->video_outbuf;
        pkt.size = out_size;
        
        /* write the compressed frame to the media file */
        ret = av_write_frame(oc, &pkt);
    }
    else
    {
        // out_size == 0 means the image was buffered
        ret = 0;
    }

    if (ret != 0)
    {
        fprintf(stderr, "Error while writing video frame\n");
        return -1;
    }
    else
    {
        //enc->frame_count++;
        return 0;
    }
}

static int write_audio_frame(internal_ffmpeg_encoder_t * enc, int stream_i, short * p_audio)
{
    AVFormatContext * oc = enc->oc;
    audio_encoder_t * aenc = enc->audio_encoder[stream_i];
    AVStream * st = aenc->audio_st;
    AVCodecContext * c = st->codec;

    AVPacket pkt;
    av_init_packet(&pkt);

    //fprintf(stderr, "Writing audio frame, to audio stream %d, outbuf_size = %d\n", stream_i, aenc->audio_outbuf_size);
    pkt.size = avcodec_encode_audio(c, aenc->audio_outbuf, aenc->audio_outbuf_size, p_audio);

    pkt.pts = av_rescale_q(c->coded_frame->pts, c->time_base, st->time_base);
    pkt.flags |= PKT_FLAG_KEY;
    pkt.stream_index = st->index;
    pkt.data = aenc->audio_outbuf;

    /* write the compressed frame to the media file */
    if (av_write_frame(oc, &pkt) != 0)
    {
        fprintf(stderr, "Error while writing audio frame\n");
        return -1;
    }

    return 0;
}



extern ffmpeg_encoder_av_t * ffmpeg_encoder_av_init (const char * filename, ffmpeg_encoder_av_resolution_t res, int wide_aspect, int64_t start_tc, int num_threads,
                                                     int num_audio_streams, int num_audio_channels_per_stream)
{
    internal_ffmpeg_encoder_t * enc;
    AVOutputFormat * fmt;
    const char * fmt_name = 0;

    switch (res)
    {
    case FF_ENCODER_RESOLUTION_DVD:
        fmt_name = "dvd";
        break;
    case FF_ENCODER_RESOLUTION_MPEG4_MOV:
        fmt_name = "mov";
        break;
    case FF_ENCODER_RESOLUTION_DV25_MOV:
    case FF_ENCODER_RESOLUTION_DV50_MOV:
    case FF_ENCODER_RESOLUTION_DV100_MOV:
        fmt_name = "mov";
        break;
    default:
        break;
    }

    if (!fmt_name)
    {
        fprintf (stderr, "ffmpeg_encoder_av_init: resolution not recognised\n");
        return NULL;
    }

    /* register all ffmpeg codecs and formats */
    av_register_all();

    /* Check if format exists */
    fmt = guess_format (fmt_name, NULL, NULL);
    if (!fmt)
    {
        fprintf (stderr, "ffmpeg_encoder_av_init: format \"%s\" not registered\n", fmt_name);
        return NULL;
    }

    /* Check number of audio tracks */
    if (num_audio_streams > MAX_AUDIO_STREAMS)
    {
        fprintf (stderr, "ffmpeg_encoder_av_init: too many audio tracks (%d)\n", num_audio_streams);
        return NULL;
    }

    /* Allocate encoder object and set all members to zero */
    enc = av_mallocz (sizeof(internal_ffmpeg_encoder_t));
    if (!enc)
    {
        fprintf (stderr, "Could not allocate encoder object\n");
        return NULL;
    }

    /* Allocate audio encoder objects and set all members to zero */
    int i;
    for (i = 0; i < num_audio_streams; ++i)
    {
        enc->audio_encoder[i] = av_mallocz (sizeof(audio_encoder_t));
    }


    /* Set audio track info */
    enc->num_audio_streams = num_audio_streams;

    /* Allocate the output media context */
    enc->oc = avformat_alloc_context();
    if (!enc->oc)
    {
        cleanup(enc);
        fprintf (stderr, "Could not allocate output format context\n");
        return NULL;
    }
    enc->oc->oformat = fmt;

    /* Set parameters for output media context*/
    switch (res)
    {
    case FF_ENCODER_RESOLUTION_DVD:
        enc->oc->packet_size = 2048;
        enc->oc->mux_rate = 10080000;
        enc->oc->preload = (int) (0.5 * AV_TIME_BASE); /* 500 ms */
        enc->oc->max_delay = (int) (0.7 * AV_TIME_BASE); /* 700 ms */
        break;
    default:
        break;
    }


    /* Set aspect ratio for video stream */
    AVRational sar;
    if (FF_ENCODER_RESOLUTION_DV100_MOV == res)
    {
        sar.num = 4;
        sar.den = 3;
    }
    else if (wide_aspect)
    {
    // SD 16:9
        sar.num = 118;
        sar.den = 81;
    }
    else
    {
    // SD 4:3
        sar.num = 59;
        sar.den = 54;
    }
#if 0
    // Bodge for FCP - no longer needed.
    // When writing track header in mov file, ffmpeg (see movenc.c) writes
    // width of sample_aspect_ratio * track->enc->width
    // It gets sample_aspect_ratio from the AVStream
    // FCP seems to want the width in samples so set sar = 1/1 to force
    // that.
    // However, the ffmpeg DV encoder will look at sample_aspect_ratio in
    // the AVCodecContext to decide what aspect flag to put in the DV bitstream.
    // So this means we get 4:3 signalled.
    sar.num = 1;
    sar.den = 1;
#endif

    /* Add the video stream */
    enc->video_st = av_new_stream(enc->oc, 0);
    if (!enc->video_st)
    {
        fprintf(stderr, "Could not alloc video stream\n");
        cleanup(enc);
        return NULL;
    }

    enc->video_st->sample_aspect_ratio = sar;
    enc->video_st->codec->sample_aspect_ratio = sar;

    /* Set time base: This is the fundamental unit of time (in seconds) in terms
       in terms of which frame timestamps are represented.
       For fixed-fps content, timebase should be 1/framerate and timestamp
       increments should be identically 1. */
    enc->video_st->codec->time_base.num = 1;
    enc->video_st->codec->time_base.den = 25;

    /* Initialise video codec */
    switch (res)
    {
    case FF_ENCODER_RESOLUTION_DVD:
        init_video_dvd(enc);
        break;
    case FF_ENCODER_RESOLUTION_MPEG4_MOV:
        init_video_mpeg4(enc);
        break;
    case FF_ENCODER_RESOLUTION_DV25_MOV:
    case FF_ENCODER_RESOLUTION_DV50_MOV:
    case FF_ENCODER_RESOLUTION_DV100_MOV:
        init_video_dv(enc, res, start_tc);
        break;
    default:
        break;
    }
    
    /* Add the audio streams */
    for (i = 0; i < enc->num_audio_streams; ++i)
    {
        audio_encoder_t * aenc = enc->audio_encoder[i];

        aenc->audio_st = av_new_stream(enc->oc, 1 + i);
        if (!aenc->audio_st)
        {
            fprintf(stderr, "Could not alloc audio stream[%d]\n", i);
            cleanup(enc);
            return NULL;
        }
    }

    /* Initialise audio codecs */
    for (i = 0; i < enc->num_audio_streams; ++i)
    {
        audio_encoder_t * aenc = enc->audio_encoder[i];
        aenc->num_audio_channels = num_audio_channels_per_stream;
        switch (res)
        {
        case FF_ENCODER_RESOLUTION_DVD:
            init_audio_dvd(aenc);
            break;
        case FF_ENCODER_RESOLUTION_MPEG4_MOV:
            init_audio_mp3(aenc);
            break;
        case FF_ENCODER_RESOLUTION_DV25_MOV:
        case FF_ENCODER_RESOLUTION_DV50_MOV:
        case FF_ENCODER_RESOLUTION_DV100_MOV:
            init_audio_pcm(aenc);
            break;
        default:
            break;
        }

        /* allocate audio input buffer */
        aenc->audio_inbuf = av_malloc(aenc->audio_inbuf_size);

        /* allocate audio (coded) output buffer */
        aenc->audio_outbuf = av_malloc(aenc->audio_outbuf_size);
    }

    // Setup ffmpeg threads if specified
    AVCodecContext * vcodec_context = enc->video_st->codec;
    if (num_threads != 0) {
        int threads = 0;
        if (num_threads == THREADS_USE_BUILTIN_TUNING)
        {
            // select number of threaded based on picture size
            if (vcodec_context->width > 720)
            {
                threads = 4;
            }
        }
        else
        {
            // use number of threads specified by function arg
            threads = num_threads;
        }
        if (threads > 0)
        {
            avcodec_thread_init(vcodec_context, threads);
            vcodec_context->thread_count= threads;
        }
    }

    // Set separate stream header if format requires it.
    if (!strcmp(enc->oc->oformat->name, "mp4")
        || !strcmp(enc->oc->oformat->name, "mov")
        || !strcmp(enc->oc->oformat->name, "3gp"))
    {
        vcodec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
        for (i = 0; i < enc->num_audio_streams; ++i)
        {
            audio_encoder_t * aenc = enc->audio_encoder[i];
            AVCodecContext * acodec_context = aenc->audio_st->codec;
            acodec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }
    }

    /*
    * Set the output parameters - must be done even if no parameters 
    */
    if (av_set_parameters(enc->oc, NULL) < 0)
    {
        fprintf(stderr, "Invalid output format parameters\n");
        cleanup(enc);
        return NULL;
    }

    /* Set output filename */
    snprintf (enc->oc->filename, sizeof(enc->oc->filename), "%s", filename);

    /* Open the output file */
    if (!(fmt->flags & AVFMT_NOFILE))
    {
        if (url_fopen(&enc->oc->pb, enc->oc->filename, URL_WRONLY) < 0)
        {
            cleanup(enc);
            fprintf(stderr, "Could not open '%s'\n", filename);
            return NULL;
        }
    }

    /* Write stream header if any */
    av_write_header(enc->oc);

    return (ffmpeg_encoder_av_t *)enc;
}

extern int ffmpeg_encoder_av_encode_video (ffmpeg_encoder_av_t * in_enc, uint8_t * p_video)
{
    internal_ffmpeg_encoder_t * enc = (internal_ffmpeg_encoder_t *)in_enc;
    if (!enc)
    {
        return -1;
    }

    /* encode and write video */
    if (p_video && write_video_frame (enc, p_video) == -1)
    {
        return -1;
    }

    return 0;
}

extern int ffmpeg_encoder_av_encode_audio (ffmpeg_encoder_av_t * in_enc, int stream_i, int num_samples, short * p_audio)
{
    internal_ffmpeg_encoder_t * enc = (internal_ffmpeg_encoder_t *)in_enc;
    if (!enc)
    {
        return -1;
    }

    audio_encoder_t * aenc = enc->audio_encoder[stream_i];
    int nch = aenc->num_audio_channels;

    /* encode and write audio */
    if (p_audio)
    {
        /*
        Audio comes in in video frames (num_samples e.g. 1920) but may be encoded
        as MPEG frames (1152 samples) so we need to buffer.
        NB. audio_inbuf_offset is in terms of 16-bit samples, hence the
        rather confusing pointer arithmetic.
        */
        //fprintf (stderr, "audio_inbuf_offset = %d, num_samples = %d, num_channels = %d\n", aenc->audio_inbuf_offset, num_samples, nch);
        memcpy (aenc->audio_inbuf + aenc->audio_inbuf_offset, p_audio, num_samples * 2 * nch);
        aenc->audio_inbuf_offset += (num_samples * nch);
        int diff;
        while ((diff = aenc->audio_inbuf_offset - (aenc->audio_samples_per_output_frame * nch)) >= 0)
        {
            if (write_audio_frame (enc, stream_i, aenc->audio_inbuf) == -1)
            {
                return -1;
            }
            memmove (aenc->audio_inbuf, aenc->audio_inbuf + (aenc->audio_samples_per_output_frame * nch), diff * 2);
            aenc->audio_inbuf_offset -= (aenc->audio_samples_per_output_frame * nch);
        }
    }

    return 0;
}

extern int ffmpeg_encoder_av_close (ffmpeg_encoder_av_t * in_enc)
{
    internal_ffmpeg_encoder_t * enc = (internal_ffmpeg_encoder_t *)in_enc;
    if (!enc)
    {
        return -1;
    }

    if (enc->video_st)
    {
        avcodec_close(enc->video_st->codec);
        enc->video_st = NULL;
    }
    int i;
    for (i = 0; i < enc->num_audio_streams; ++i)
    {
        audio_encoder_t * aenc = enc->audio_encoder[i];
        if (aenc->audio_st)
        {
            avcodec_close(aenc->audio_st->codec);
            aenc->audio_st = NULL;
        }
    }

    /* write the trailer, if any */
    av_write_trailer(enc->oc);

    if (!(enc->oc->oformat->flags & AVFMT_NOFILE))
    {
        /* close the output file */
#ifdef FFMPEG_OLD_INCLUDE_PATHS
        url_fclose(&enc->oc->pb);
#else
        url_fclose(enc->oc->pb);
#endif
    }
    cleanup (enc);
    return 0;
}

