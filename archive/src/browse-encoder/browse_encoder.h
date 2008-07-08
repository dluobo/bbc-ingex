/*
 * $Id: browse_encoder.h,v 1.1 2008/07/08 16:21:26 philipn Exp $
 *
 * MPEG-2 encodes planar YUV420 video and 16-bit PCM audio
 *
 * Copyright (C) 2007 BBC Research, Stuart Cunningham <stuart_hc@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef  BROWSE_ENCODER_H
#define BROWSE_ENCODER_H
#include <inttypes.h>

#include <archive_types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void browse_encoder_t;
/*
* browse_encoder_init : Creates a format context for the "browse" format and returns
*                    a pointer to it.
* Input            : filename  - file name to write "browse" formatted data to
*                  : kbit_rate - required bit_rate in kilobits per sec
*                  : thread_count - number ffmpeg threads to use for encoding the video; 0 means none
* Return           : Pointer to browse_encoder_t object if successful
*                    NULL if a problem occurred
*/
extern browse_encoder_t *browse_encoder_init (const char *filename, uint32_t kbit_rate, int thread_count);

/*
* browse_encoder_encode : Encodes the input video and audio frames to the format
*					 specified in input AVFormatContext object
*                    a pointer to it.
* Input            : browse  - browse_encoder_t object returned by browse_encoder_init
*                  : p_video - pointer to one frame of video data
*                  : p_audio - pointer to one frame of audio data
*                  : int32_t - frame number
* Return           : 0 if operation is successful
*                  : -1 if operation failed
*					NOTE: expects an SD VIDEO frames in Planar YUV420 format
*					      MP2 audion - stero 16 bits/sample
*/
extern int browse_encoder_encode (browse_encoder_t *in_browse, uint8_t *p_video, int16_t *p_audio, ArchiveTimecode ltc, ArchiveTimecode vitc, int32_t frame_number);

/*
* browse_encoder_close : Releases resources allocated by the browse_encoder_init
*                   : function
* Input            : browse  - browse_encoder_t object returned by browse_encoder_init
* Return           : 0 if operation is successful
*                  : -1 if operation failed
*/
extern int browse_encoder_close (browse_encoder_t *in_browse);

#ifdef __cplusplus
}
#endif

#endif /* BROWSE_ENCODER_H */
