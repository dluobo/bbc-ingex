/*
 * $Id: ffmpeg_encoder.h,v 1.5 2009/03/26 18:50:09 john_f Exp $
 *
 * Encode uncompressed video to DV frames using libavcodec
 *
 * Copyright (C) 2005  British Broadcasting Corporation
 * All Rights Reserved
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

#ifndef FFMPEG_ENCODER_H
#define FFMPEG_ENCODER_H

#include "integer_types.h"
#include "ffmpeg_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void ffmpeg_encoder_t;
typedef enum {
    FF_ENCODER_RESOLUTION_DV25,
    FF_ENCODER_RESOLUTION_DV50,
    FF_ENCODER_RESOLUTION_DV100_1080i50,
    FF_ENCODER_RESOLUTION_DV100_720p50,
    FF_ENCODER_RESOLUTION_IMX30,
    FF_ENCODER_RESOLUTION_IMX40,
    FF_ENCODER_RESOLUTION_IMX50,
    FF_ENCODER_RESOLUTION_DNX36p,
    FF_ENCODER_RESOLUTION_DNX120p,
    FF_ENCODER_RESOLUTION_DNX185p,
    FF_ENCODER_RESOLUTION_DNX120i,
    FF_ENCODER_RESOLUTION_DNX185i,
    FF_ENCODER_RESOLUTION_DMIH264,
    FF_ENCODER_RESOLUTION_JPEG
} ffmpeg_encoder_resolution_t;


/*
* ffmpeg_encoder_init : Creates a format context for an avcodec
* Input            : 
* Return           : Pointer to object if successful
*                    NULL if a problem occurred
*/
extern ffmpeg_encoder_t * ffmpeg_encoder_init(ffmpeg_encoder_resolution_t res, int num_threads);

/*
* ffmpeg_encoder_video : Encodes the input video
* Input            : in_dv  - ffmpeg_encoder_t object returned by ffmpeg_encoder_init
*                  : p_video - pointer to one frame of 4:2:2 video data
*                  : int32_t - frame number for DV frame header
* Return           : 0 if operation is successful
*                  : -1 if operation failed
*                   NOTE: expects an SD VIDEO frames in planar 4:2:2 format
*/
extern int ffmpeg_encoder_encode (ffmpeg_encoder_t * in_encoder, uint8_t * p_video, uint8_t * * pp_enc_video);

/*
* ffmpeg_encoder_close : Releases resources allocated by the ffmpeg_encoder_init
*                  : function
* Input            : in_dv  - ffmpeg_encoder_t object returned by ffmpeg_encoder_init
* Return           : compressed size (bytes) if operation is successful
*                  : -1 if operation failed
*/
extern int ffmpeg_encoder_close (ffmpeg_encoder_t * in_encoder);

#ifdef __cplusplus
}
#endif

#endif /* FFMPEG_ENCODER_H */

