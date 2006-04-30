/*
 * $Id: dvd_encoder.h,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Write MPEG-2 in DVD-ready program stream.
 *
 * Copyright (C) 2005  Stuart Cunningham <stuart_hc@users.sourceforge.net>
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

#ifndef  DVD_ENCODER_H
#define DVD_ENCODER_H
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void dvd_encoder_t;
/*
* dvd_encoder_init : Creates a format context for the "dvd" format and returns
*                    a pointer to it.
* Input            : filename  - file name to write "dvd" formatted data to
*                  : kbit_rate - required bit_rate in kilobits per sec
* Return           : Pointer to AVFormatContext object if successful
*                    NULL if a problem occurred
*/
extern dvd_encoder_t *dvd_encoder_init (const char *filename, uint32_t kbit_rate);

/*
* dvd_encoder_encode : Encodes the input video and audio frames to the format
*					 specified in input AVFormatContext object
*                    a pointer to it.
* Input            : dvd  - dvd_encoder_t object returned by dvd_encoder_init
*                  : p_video - pointer to one frame of video data
*                  : p_audio - pointer to one frame of audio data
*                  : int32_t - frame number
* Return           : 0 if operation is successful
*                  : -1 if operation failed
*					NOTE: expects an SD VIDEO frames in Planar YUV420 format
*					      MP2 audion - stero 16 bits/sample
*/
extern int dvd_encoder_encode (dvd_encoder_t *in_dvd, uint8_t *p_video, int16_t *p_audio, int32_t frame_number);

/*
* dvd_encoder_close : Releases resources allocated by the dvd_encoder_init
*                   : function
* Input            : dvd  - dvd_encoder_t object returned by dvd_encoder_init
* Return           : 0 if operation is successful
*                  : -1 if operation failed
*/
extern int dvd_encoder_close (dvd_encoder_t *in_dvd);

#ifdef __cplusplus
}
#endif

#endif /* DVD_ENCODER_H */
