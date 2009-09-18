/*
 * $Id: YUV_DirectShow.h,v 1.2 2009/09/18 15:07:24 philipn Exp $
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

#ifndef __YUVLIB_DIRECTSHOW__
#define __YUVLIB_DIRECTSHOW__

// Interface between DirectShow video samples and simple video frame

// Get bit map info header from a media type
extern BITMAPINFOHEADER*
GetBMI(const AM_MEDIA_TYPE *mt);

// Assign a DirectShow YUV sample to a YUV_frame
extern HRESULT
YUV_frame_from_DirectShow(const CMediaType* pMT, BYTE* buff, YUV_frame* pFrame);

#endif // __YUVLIB_DIRECTSHOW__
