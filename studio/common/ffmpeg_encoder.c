/*
 * $Id: ffmpeg_encoder.c,v 1.7 2009/09/18 16:25:20 philipn Exp $
 *
 * Encode uncompressed video to DV using libavcodec
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

#include <stdio.h>
#include <string.h>

#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#include <ffmpeg/swscale.h>
#else
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#endif

#define MPA_FRAME_SIZE              1152 // taken from ffmpeg's private mpegaudio.h
#define PAL_VIDEO_FRAME_AUDIO_SIZE  1920

#include "ffmpeg_encoder.h"


const uint16_t imx30_intra_matrix[] = {
  32, 16, 16, 17, 19, 22, 26, 34,
  16, 16, 18, 19, 21, 26, 33, 37,
  19, 18, 19, 23, 21, 28, 35, 45,
  18, 19, 20, 22, 28, 35, 32, 49,
  18, 20, 23, 29, 32, 33, 44, 45,
  18, 20, 20, 25, 35, 39, 52, 58,
  20, 22, 23, 28, 31, 44, 51, 57,
  19, 21, 26, 30, 38, 48, 45, 75
};

const uint16_t imx4050_intra_matrix[] = {
  32, 16, 16, 17, 19, 22, 23, 31,
  16, 16, 17, 19, 20, 23, 29, 29,
  19, 17, 19, 22, 19, 25, 28, 35,
  17, 19, 19, 20, 25, 28, 25, 33,
  17, 19, 22, 25, 26, 25, 31, 31,
  17, 19, 17, 20, 26, 28, 35, 36,
  19, 19, 19, 20, 22, 29, 32, 35,
  16, 17, 19, 22, 25, 31, 28, 45 
};

const uint16_t jpeg2to1_intra_matrix[] = {
  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  2,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,
  1,  3,  2,  2,  3,  1,  1,  1,
  1,  1,  1,  1,  5,  3,  3,  5,
  1,  1,  1,  6,  4,  5,  5,  5
};

const uint16_t jpeg2to1_chroma_intra_matrix[] = {
  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  2,  1,  5,
  5,  2,  2,  2,  1,  2,  2,  2,
  5,  5,  5,  5,  5,  5,  5,  5,
  5,  5,  5,  2,  5,  5,  5,  5,
  5,  5,  5,  5,  5,  5,  5,  5,
  5,  5,  5,  5,  5,  5,  5,  5,
  5,  5,  5,  5,  5,  5,  5,  5,
};



typedef struct 
{
    AVCodec * codec;
    AVCodecContext * codec_context;
    // audio
    int num_audio_channels;
    int audio_samples_per_input_frame;
    int audio_samples_per_output_frame;
    short * audio_inbuf;
    int audio_inbuf_size;
    int audio_inbuf_offset;
    uint8_t * audio_outbuf;
    int audio_outbuf_size;
    // video
    AVFrame * inputFrame;
    uint8_t * outputBuffer;
    unsigned int bufferSize;
    // following needed for frame padding and input scaling
    AVPicture * tmpFrame;
    uint8_t * inputBuffer;
    int input_width;
    int input_height;
    struct SwsContext* scale_context;
    int scale_image;
    int padtop;
    int padColour[3];
} internal_ffmpeg_encoder_t;


static void cleanup (internal_ffmpeg_encoder_t * encoder)
{
    if (encoder)
    {
        av_free(encoder->outputBuffer);
        av_free(encoder->inputBuffer);
        av_free(encoder->inputFrame);
        av_free(encoder->tmpFrame);
        if (encoder->codec_context)
        {
            if (encoder->codec_context->intra_matrix)
            {
                av_free(encoder->codec_context->intra_matrix);
            }
            avcodec_close(encoder->codec_context);
        }
        sws_freeContext(encoder->scale_context);
        av_free(encoder->codec_context);
        av_free(encoder);
    }
}

extern ffmpeg_encoder_t * ffmpeg_encoder_init(ffmpeg_encoder_resolution_t res, int num_threads)
{
    internal_ffmpeg_encoder_t * encoder;

    /* register all ffmpeg codecs and formats */
    av_register_all();

    /* Allocate encoder object and set all members to zero */
    encoder = av_mallocz(sizeof(internal_ffmpeg_encoder_t));
    if (!encoder)
    {
        fprintf(stderr, "Could not allocate encoder object\n");
        return 0;
    }

    /* Setup encoder */
    int codec_id;
    int codec_type;
    int imx = 0;
    switch (res)
    {
    case FF_ENCODER_RESOLUTION_DV25:
    case FF_ENCODER_RESOLUTION_DV50:
    case FF_ENCODER_RESOLUTION_DV100_1080i50:
    case FF_ENCODER_RESOLUTION_DV100_720p50:
        codec_id = CODEC_ID_DVVIDEO;
        codec_type = CODEC_TYPE_VIDEO;
        break;
    case FF_ENCODER_RESOLUTION_IMX30:
    case FF_ENCODER_RESOLUTION_IMX40:
    case FF_ENCODER_RESOLUTION_IMX50:
        codec_id = CODEC_ID_MPEG2VIDEO;
        codec_type = CODEC_TYPE_VIDEO;
        imx = 1;
        break;
    case FF_ENCODER_RESOLUTION_DNX36p:
    case FF_ENCODER_RESOLUTION_DNX120p:
    case FF_ENCODER_RESOLUTION_DNX185p:
    case FF_ENCODER_RESOLUTION_DNX120i:
    case FF_ENCODER_RESOLUTION_DNX185i:
        codec_id = CODEC_ID_DNXHD;
        codec_type = CODEC_TYPE_VIDEO;
        break;
    case FF_ENCODER_RESOLUTION_DMIH264:
        codec_id = CODEC_ID_H264;
        codec_type = CODEC_TYPE_VIDEO;
        break;
    case FF_ENCODER_RESOLUTION_JPEG:
        codec_id = CODEC_ID_MJPEG;
        codec_type = CODEC_TYPE_VIDEO;
        break;
    case FF_ENCODER_RESOLUTION_MP3:
        codec_id = CODEC_ID_MP3;
        codec_type = CODEC_TYPE_AUDIO;
        break;
    default:
        codec_id = 0;
    }

    encoder->codec = avcodec_find_encoder(codec_id);
    if (! encoder->codec)
    {
        fprintf(stderr, "Could not find encoder\n");
        cleanup(encoder);
        return 0;
    }

    // Allocate AVCodecContext and set its fields to
    // default values.
    encoder->codec_context = avcodec_alloc_context();
    if (!encoder->codec_context)
    {
        fprintf(stderr, "Could not allocate encoder context\n");
        cleanup(encoder);
        return 0;
    }

    // Set fields in AVCodecContext
    encoder->codec_context->codec_type = codec_type;
    if (CODEC_TYPE_VIDEO == codec_type)
    {
        encoder->codec_context->time_base.num = 1;
        encoder->codec_context->time_base.den = 25;
        encoder->codec_context->pix_fmt = PIX_FMT_YUV422P;
        switch (res)
        {
        case FF_ENCODER_RESOLUTION_DV25:
        case FF_ENCODER_RESOLUTION_DMIH264:
            encoder->codec_context->pix_fmt = PIX_FMT_YUV420P;
            break;
        case FF_ENCODER_RESOLUTION_JPEG:
            encoder->codec_context->pix_fmt = PIX_FMT_YUVJ422P;
            break;
        case FF_ENCODER_RESOLUTION_DV100_720p50:
            encoder->codec_context->time_base.num = 1;
            encoder->codec_context->time_base.den = 50;
            break;
        default:
            break;
        }

        int width = 720;
        int height = 576;
        int bit_rate = 0;
        int buffer_size = 0;
        int encoded_frame_size = 0;
        const uint16_t * intra_matrix = 0;
        int top_field_first = 1;
        switch (res)
        {
        case FF_ENCODER_RESOLUTION_JPEG:
            encoder->codec_context->bit_rate = 65 * 1000000;
            encoded_frame_size = 800000;        // guess
            encoder->codec_context->intra_matrix = av_mallocz(sizeof(uint16_t) * 64);
            memcpy(encoder->codec_context->intra_matrix, jpeg2to1_intra_matrix, sizeof(uint16_t) * 64);
            break;
        case FF_ENCODER_RESOLUTION_DV25:
            encoded_frame_size = 144000;
            top_field_first = 0;
            break;
        case FF_ENCODER_RESOLUTION_DV50:
            encoded_frame_size = 288000;
            top_field_first = 0;
            break;
        case FF_ENCODER_RESOLUTION_IMX30:
            bit_rate = 30 * 1000000;
            buffer_size = 30 * 40000;
            encoded_frame_size = bit_rate / 200;
            intra_matrix = imx30_intra_matrix;
            break;
        case FF_ENCODER_RESOLUTION_IMX40:
            bit_rate = 40 * 1000000;
            buffer_size = 40 * 40000;
            encoded_frame_size = bit_rate / 200;
            intra_matrix = imx4050_intra_matrix;
            break;
        case FF_ENCODER_RESOLUTION_IMX50:
            bit_rate = 50 * 1000000;
            buffer_size = 50 * 40000;
            encoded_frame_size = bit_rate / 200;
            intra_matrix = imx4050_intra_matrix;
            break;
        case FF_ENCODER_RESOLUTION_DNX36p:
            width = 1920;
            height = 1080;
            encoder->codec_context->bit_rate = 36 * 1000000;
            encoder->codec_context->qmax = 1024;
            encoded_frame_size = 188416;        // from VC-3 spec
            break;
        case FF_ENCODER_RESOLUTION_DNX120p:
        case FF_ENCODER_RESOLUTION_DNX120i:
            width = 1920;
            height = 1080;
            encoder->codec_context->bit_rate = 120 * 1000000;
            encoder->codec_context->qmax = 1024;
            if (res == FF_ENCODER_RESOLUTION_DNX120i)
                encoder->codec_context->flags |= CODEC_FLAG_INTERLACED_DCT;
            encoded_frame_size = 606208;        // from VC-3 spec
            break;
        case FF_ENCODER_RESOLUTION_DNX185p:
        case FF_ENCODER_RESOLUTION_DNX185i:
            width = 1920;
            height = 1080;
            encoder->codec_context->bit_rate = 185 * 1000000;
            encoder->codec_context->qmax = 1024;
            if (res == FF_ENCODER_RESOLUTION_DNX185i)
                encoder->codec_context->flags |= CODEC_FLAG_INTERLACED_DCT;
            encoded_frame_size = 917504;        // from VC-3 spec
            break;
        case FF_ENCODER_RESOLUTION_DV100_1080i50:
            width = 1440;                       // coded width (input scaled horizontally from 1920)
            height = 1080;
            encoder->input_width = 1920;
            encoder->input_height = 1080;
            encoder->scale_image = 1;
            encoded_frame_size = 576000;        // SMPTE 370M spec
            break;
        case FF_ENCODER_RESOLUTION_DV100_720p50:
            width = 960;                        // coded width (input scaled horizontally from 1280)
            height = 720;
            encoder->input_width = 1280;
            encoder->input_height = 720;
            encoder->scale_image = 1;
            encoded_frame_size = 576000;        // SMPTE 370M spec
            break;
        case FF_ENCODER_RESOLUTION_DMIH264:
            encoded_frame_size = 200000;            // guess at maximum encoded size
            encoder->codec_context->bit_rate = 15 * 1000000;    // a guess
            encoder->codec_context->gop_size = 1;   // I-frame only
            break;
        default:
            break;
        }

        // setup ffmpeg threads if specified
        if (num_threads != 0) {
            int threads = 0;
            if (num_threads == THREADS_USE_BUILTIN_TUNING)
            {
                // select number of threaded based on picture size
                if (width > 720)
                    threads = 4;
            }
            else
            {
                // use number of threads specified by function arg
                threads = num_threads;
            }
            if (threads > 0)
            {
                avcodec_thread_init(encoder->codec_context, threads);
                encoder->codec_context->thread_count= threads;
            }
        }

        if (imx)
        {
            encoder->padtop = 32;

            encoder->codec_context->rc_min_rate = bit_rate;
            encoder->codec_context->rc_max_rate = bit_rate;
            encoder->codec_context->bit_rate = bit_rate;
            encoder->codec_context->rc_buffer_size = buffer_size;
            encoder->codec_context->rc_initial_buffer_occupancy = buffer_size;

            // -ps 1
            encoder->codec_context->rtp_payload_size = 1;

            // -qscale 1
            encoder->codec_context->global_quality = 1 * FF_QP2LAMBDA;

            // -intra
            encoder->codec_context->gop_size = 0;

            encoder->codec_context->qmin = 1;
            encoder->codec_context->qmax = 3;

            encoder->codec_context->lmin = 1 * FF_QP2LAMBDA;
            encoder->codec_context->lmax = 3 * FF_QP2LAMBDA;

            encoder->codec_context->flags |=
                CODEC_FLAG_INTERLACED_DCT | CODEC_FLAG_LOW_DELAY;

            // See SMPTE 356M Type D-10 Stream Specifications

            // intra_dc_precision = 2
            // -dc 10  (8 gets subtracted)
            encoder->codec_context->intra_dc_precision = 2;

            // frame_pred_frame_dct = 0 (field/frame adaptive)
            // We get this by default

            // q_scale_type = 1 (nonlinear quantiser)
            encoder->codec_context->flags2 |= CODEC_FLAG2_NON_LINEAR_QUANT;

            // intra_vlc_format = 1 (use intra-VLC table)
            encoder->codec_context->flags2 |= CODEC_FLAG2_INTRA_VLC;

            // Avid uses special intra quantiser matrix
            encoder->codec_context->intra_matrix = av_mallocz(sizeof(uint16_t) * 64);
            memcpy(encoder->codec_context->intra_matrix, intra_matrix, sizeof(uint16_t) * 64);

            /* alloc input buffer */
            encoder->tmpFrame = av_mallocz(sizeof(AVPicture));
            encoder->inputBuffer = av_mallocz(width * (height + encoder->padtop) * 2);
            encoder->padColour[0] = 16;
            encoder->padColour[1] = 128;
            encoder->padColour[2] = 128;
        }

        if (encoder->scale_image)
        {
            encoder->scale_image = 1;
            // alloc buffers for scaling input video
            encoder->tmpFrame = av_mallocz(sizeof(AVPicture));
            encoder->inputBuffer = av_mallocz(width * height * 2);
        }

        avcodec_set_dimensions(encoder->codec_context, width, height + encoder->padtop);

        //encoder->codec_context->sample_aspect_ratio = av_d2q(16.0/9*576/720, 255); // {64, 45}
        // Actually the active picture is less than 720 wide, the correct numbers appear below
    #if 0
        // 4:3
        encoder->codec_context->sample_aspect_ratio.num = 59;
        encoder->codec_context->sample_aspect_ratio.den = 54;
    #else
        // 16:9
        encoder->codec_context->sample_aspect_ratio.num = 118;
        encoder->codec_context->sample_aspect_ratio.den = 81;
    #endif

        /* prepare codec */
        if (avcodec_open(encoder->codec_context, encoder->codec) < 0)
        {
            fprintf(stderr, "Could not open encoder\n");
            return 0;
        }

        /* allocate input frame */
        encoder->inputFrame = avcodec_alloc_frame();
        if (!encoder->inputFrame)
        {
            fprintf(stderr, "Could not allocate input frame\n");
            return 0;
        }
        encoder->inputFrame->top_field_first = top_field_first;

        /* allocate output buffer */
        encoder->bufferSize = encoded_frame_size + 50000; // not sure why extra is needed
        encoder->outputBuffer = av_mallocz(encoder->bufferSize);
    }
    else
    {
    // audio
        /* set parameters */
        const int kbit_rate = 128;
        encoder->codec_context->bit_rate = kbit_rate * 1000;
        encoder->codec_context->sample_rate = 48000;
        const int nch = 2;
        encoder->num_audio_channels = nch;
        encoder->codec_context->channels = nch;

        /* Set up audio buffer parameters */
        encoder->audio_samples_per_input_frame = PAL_VIDEO_FRAME_AUDIO_SIZE;
        /* We have to do some buffering of input, hence the * 2 on the end */
        encoder->audio_inbuf_size = encoder->audio_samples_per_input_frame * nch * sizeof(short) * 2;

        /* samples per coded frame */
        encoder->audio_samples_per_output_frame = MPA_FRAME_SIZE;
        /* coded frame size */
        encoder->audio_outbuf_size = 2 * 960; // enough for 2 audio frames at 320 kbit/s

        /* prepare codec */
        if (avcodec_open(encoder->codec_context, encoder->codec) < 0)
        {
            fprintf(stderr, "Could not open encoder\n");
            return 0;
        }
    }

    return (ffmpeg_encoder_t *) encoder;
}

extern int ffmpeg_encoder_encode(ffmpeg_encoder_t * in_encoder, uint8_t * p_video, uint8_t * * pp_enc_video)
{
    internal_ffmpeg_encoder_t * encoder = (internal_ffmpeg_encoder_t *)in_encoder;
    if (!encoder)
    {
        return -1;
    }

    if (encoder->padtop)
    {
        /* set pointers in tmpFrame to point to planes in p_video */
        avpicture_fill(encoder->tmpFrame, p_video,
            encoder->codec_context->pix_fmt,
            720, 576);

        /* set pointers in inputFrame to point to our buffer */
        avpicture_fill((AVPicture*)encoder->inputFrame, encoder->inputBuffer,
            encoder->codec_context->pix_fmt,
            encoder->codec_context->width, encoder->codec_context->height);

        /* copy video data into our buffer with padding */
        av_picture_pad((AVPicture*)encoder->inputFrame, encoder->tmpFrame,
            encoder->codec_context->height,
            encoder->codec_context->width,
            encoder->codec_context->pix_fmt,
            encoder->padtop, 0, 0, 0,
            encoder->padColour);
    }
    else if (encoder->scale_image)
    {
        encoder->scale_context = sws_getCachedContext(encoder->scale_context,
                    encoder->input_width, encoder->input_height,                   // input WxH
                    encoder->codec_context->pix_fmt,
                    encoder->codec_context->width, encoder->codec_context->height, // output WxH
                    encoder->codec_context->pix_fmt,
                    SWS_FAST_BILINEAR,
                    NULL, NULL, NULL);

        // Input image goes into tmpFrame
        avpicture_fill((AVPicture*)encoder->tmpFrame, p_video,
            encoder->codec_context->pix_fmt,
            encoder->input_width, encoder->input_height);

        // Output of scale operation goes into inputFrame
        avpicture_fill((AVPicture*)encoder->inputFrame, encoder->inputBuffer,
            encoder->codec_context->pix_fmt,
            encoder->codec_context->width, encoder->codec_context->height);

        // Perform the scale
        sws_scale(encoder->scale_context,
            encoder->tmpFrame->data, encoder->tmpFrame->linesize,
            0, encoder->input_height,
            encoder->inputFrame->data, encoder->inputFrame->linesize);
    }
    else
    {
        /* set pointers in inputFrame to point to planes in p_video */
        avpicture_fill((AVPicture*)encoder->inputFrame, p_video,
            encoder->codec_context->pix_fmt,
            encoder->codec_context->width, encoder->codec_context->height);
    }

    /* compress video */
    int size = avcodec_encode_video(encoder->codec_context,
        encoder->outputBuffer, encoder->bufferSize, encoder->inputFrame);
    if (size < 0)
    {
        fprintf(stderr, "error compressing video!\n");
        return -1;
    }
    else
    {
        // debug only
        //fprintf(stderr, "size = %d\n", size);
    }


    /* set pointer to encoded video */
    *pp_enc_video = encoder->outputBuffer;

    return size;
}

extern int ffmpeg_encoder_encode_audio (ffmpeg_encoder_t * in_encoder, int num_samples, short * p_audio, uint8_t * * pp_enc_audio)
{
    internal_ffmpeg_encoder_t * aenc = (internal_ffmpeg_encoder_t *)in_encoder;
    if (!aenc)
    {
        return -1;
    }

    int size = 0;
    *pp_enc_audio = aenc->audio_outbuf;
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
            int s = avcodec_encode_audio(aenc->codec_context, aenc->audio_outbuf + size, aenc->audio_outbuf_size - size, p_audio);
            if (s < 0)
            {
                fprintf(stderr, "error compressing audio!\n");
                return -1;
            }
            size += s;

            /* shift samples in input buffer */
            memmove (aenc->audio_inbuf, aenc->audio_inbuf + (aenc->audio_samples_per_output_frame * nch), diff * 2);
            aenc->audio_inbuf_offset -= (aenc->audio_samples_per_output_frame * nch);
        }
    }

    return size;
}

extern int ffmpeg_encoder_close (ffmpeg_encoder_t * in_encoder)
{
    internal_ffmpeg_encoder_t * encoder = (internal_ffmpeg_encoder_t *)in_encoder;
    if (!encoder)
    {
        return -1;
    }

    cleanup(encoder);

    return 0;
}
