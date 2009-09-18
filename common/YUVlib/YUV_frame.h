/*
 * $Id: YUV_frame.h,v 1.5 2009/09/18 15:07:24 philipn Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Jim Easterbrook
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

#ifndef __YUVLIB_YUV_FRAME__
#define __YUVLIB_YUV_FRAME__

#ifdef __cplusplus
extern "C" {
#endif

// A collection of routines to process a "general" video frame format
typedef unsigned char BYTE;

typedef struct
{
    int     w;      // image width in samples
    int     h;      // image height in picture lines
    int     lineStride; // bytes between vertically adjacent samples
    int     pixelStride;    // bytes between horizontally adjacent samples
    BYTE*   buff;       // pointer to first sample
} component;

typedef struct
{
    component   Y, U, V;
} YUV_frame;

// list of "fourcc" codes (see www.fourcc.org) for usable video formats
typedef enum
{
    YV12, IF09, YVU9, IYUV,
    UYVY, YUY2, YVYU, HDYC,
    Y42B, I420, YV16, YV24
} formats;

typedef enum
{
    YUV_OK             =     0,
    YUV_Fail           =    -1,
    YUV_no_memory      =    -2,
    YUV_unknown_format =    -3,
    YUV_size_error     =    -4,
    YUV_format_error   =    -5,
    YUV_fontconfig     = -1001,
    YUV_freetype       = -1002,
} YUV_error;

// Set up a contiguous YUV_frame with a user allocated buffer.
extern int YUV_frame_from_buffer(YUV_frame* frame, void* buffer,
                                 const int w, const int h,
                                 const formats format);

// Get minimum size of a YUV frame
extern int frame_size(const int w, const int h, const formats format);

// Allocate and free (using malloc & free) a YUV_frame.
extern int alloc_YUV_frame(YUV_frame* frame,
                           const int w, const int h, const formats format);

extern void free_YUV_frame(YUV_frame* frame);

// Set a component to a particular value
extern void clear_component(component* frame, BYTE value);

// Clear a YUV_frame to black.
extern void clear_YUV_frame(YUV_frame* frame);

// Convert RGB (in range 0.0 to 1.0) to YUV, according to Rec 601
extern void YUV_601(float R, float G, float B, BYTE* Y, BYTE* U, BYTE* V);

// Convert RGB (in range 0.0 to 1.0) to YUV, according to Rec 709 (HDTV)
extern void YUV_709(float R, float G, float B, BYTE* Y, BYTE* U, BYTE* V);

// Extract first or second field of a frame.
// field_no (0 or 1) selects the even/top or odd/bottom field.
// No data copying is done, the output points to the same storage as the input.
void extract_field(component* in_frame, component* out_field, int field_no);
void extract_YUV_field(YUV_frame* in_frame, YUV_frame* out_field, int field_no);

#ifdef __cplusplus
}
#endif

#endif // __YUVLIB_YUV_FRAME__
