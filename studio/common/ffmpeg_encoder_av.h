/*
 * $Id: ffmpeg_encoder_av.h,v 1.4 2009/04/16 18:00:17 john_f Exp $
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

#ifndef  ffmpeg_encoder_av_h
#define ffmpeg_encoder_av_h

#include "integer_types.h"
#include "ffmpeg_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void ffmpeg_encoder_av_t;
typedef enum {
    FF_ENCODER_RESOLUTION_DVD,
    FF_ENCODER_RESOLUTION_MPEG4_MOV,
    FF_ENCODER_RESOLUTION_DV25_MOV,
    FF_ENCODER_RESOLUTION_DV50_MOV,
    FF_ENCODER_RESOLUTION_DV100_MOV
} ffmpeg_encoder_av_resolution_t;

/*
* ffmpeg_encoder_av_init : Creates an encoder for the specified format and returns a pointer to it.
* Input            : filename  - file name to write formatted data to
* Return           : Pointer to ffmpeg_encoder_av_t object if successful
*                    NULL if a problem occurred
*/
extern ffmpeg_encoder_av_t * ffmpeg_encoder_av_init (const char * filename, ffmpeg_encoder_av_resolution_t res, int wide_aspect, int64_t start_tc, int num_threads,
                                                     int num_audio_streams, int num_audio_channels_per_stream);

/*
* ffmpeg_encoder_av_encode : Encodes the input video and audio frames to the format
*                    specified in input ffmpeg_encoder_av_t object
* Input            : in_enc  - pointer to ffmpeg_encoder_av_t object returned by ffmpeg_encoder_av_init
*                  : p_video - pointer to one frame of video data
*                  : p_audio - pointer to one frame of audio data (stereo, 16 bit/sample)
* Return           : 0 if operation is successful
*                  : -1 if operation failed
*/
extern int ffmpeg_encoder_av_encode_audio (ffmpeg_encoder_av_t * in_enc, int stream_i, int num_samples, short * p_audio);
extern int ffmpeg_encoder_av_encode_video (ffmpeg_encoder_av_t * in_enc, uint8_t * p_video);

/*
* ffmpeg_encoder_av_close : Releases resources allocated by the ffmpeg_encoder_av_init function
* Input            : in_enc  - ffmpeg_encoder_av_t object returned by ffmpeg_encoder_av_init
* Return           : 0 if operation is successful
*                  : -1 if operation failed
*/
extern int ffmpeg_encoder_av_close (ffmpeg_encoder_av_t * in_enc);

#ifdef __cplusplus
}
#endif

#endif //#ifndef  ffmpeg_encoder_av_h
