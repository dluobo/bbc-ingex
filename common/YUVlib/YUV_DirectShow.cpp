/*
 * $Id: YUV_DirectShow.cpp,v 1.2 2009/09/18 15:07:23 philipn Exp $
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

// Interface between DirectShow video samples and simple video frame

#include <streams.h>    // DirectShow (includes windows.h)
#include <dvdmedia.h>   // for VIDEOINFOHEADER2
#include <Aviriff.h>    // FCC

#include "YUV_frame.h"
#include "YUV_DirectShow.h"

// Get video info header rectangles from a media type
extern RECT*
GetSrcRect(const AM_MEDIA_TYPE *mt)
{
    if (mt->pbFormat == NULL)
        return NULL;
    if ((mt->formattype == FORMAT_VideoInfo) &&
        (mt->cbFormat >= sizeof(VIDEOINFOHEADER)))
        return &((VIDEOINFOHEADER*)mt->pbFormat)->rcSource;
    if ((mt->formattype == FORMAT_VideoInfo2) &&
        (mt->cbFormat >= sizeof(VIDEOINFOHEADER2)))
        return &((VIDEOINFOHEADER2*)mt->pbFormat)->rcSource;
    return NULL;
}

extern RECT*
GetTargetRect(const AM_MEDIA_TYPE *mt)
{
    if (mt->pbFormat == NULL)
        return NULL;
    if ((mt->formattype == FORMAT_VideoInfo) &&
        (mt->cbFormat >= sizeof(VIDEOINFOHEADER)))
        return &((VIDEOINFOHEADER*)mt->pbFormat)->rcTarget;
    if ((mt->formattype == FORMAT_VideoInfo2) &&
        (mt->cbFormat >= sizeof(VIDEOINFOHEADER2)))
        return &((VIDEOINFOHEADER2*)mt->pbFormat)->rcTarget;
    return NULL;
}

// Get bit map info header from a media type
extern BITMAPINFOHEADER*
GetBMI(const AM_MEDIA_TYPE *mt)
{
    if (mt->pbFormat == NULL)
        return NULL;
    if ((mt->formattype == FORMAT_VideoInfo) &&
        (mt->cbFormat >= sizeof(VIDEOINFOHEADER)))
        return &((VIDEOINFOHEADER*)mt->pbFormat)->bmiHeader;
    if ((mt->formattype == FORMAT_VideoInfo2) &&
        (mt->cbFormat >= sizeof(VIDEOINFOHEADER2)))
        return &((VIDEOINFOHEADER2*)mt->pbFormat)->bmiHeader;
    return NULL;
}

// Assign a DirectShow YUV sample to a YUV_frame
extern HRESULT
YUV_frame_from_DirectShow(const CMediaType* pMT, BYTE* buff, YUV_frame* pFrame)
{
    BITMAPINFOHEADER*   pBmi;

    pBmi = GetBMI(pMT);
    if (pBmi == NULL)
        return E_UNEXPECTED;
    // set line stride
    if ((pBmi->biBitCount != 0) && ((pBmi->biBitCount & 7) == 0))
        // For 'normal' formats, biWidth is in pixels.
        // Expand to bytes and round up to a multiple of 4.
        pFrame->Y.lineStride = ((pBmi->biWidth * (pBmi->biBitCount / 8)) + 3) & ~3;
    else   // Otherwise, biWidth is in bytes.
        pFrame->Y.lineStride = pBmi->biWidth;
    // set pixel stride
    switch (pBmi->biCompression)
    {
    case FCC('YV12'):   case FCC('IF09'):   case FCC('YVU9'):   case FCC('IYUV'):
        // planar formats
        pFrame->Y.pixelStride = 1;
        break;
    case FCC('UYVY'):   case FCC('YUY2'):   case FCC('YVYU'):   case FCC('HDYC'):
        // multiplexed formats
        pFrame->Y.pixelStride = 2;
        break;
    default:
        return E_UNEXPECTED;
    }
    // set dimensions
    RECT* rcTarget = GetTargetRect(pMT);
    //  If rcTarget is empty, use the whole image.
    if (IsRectEmpty(rcTarget))
    {
        pFrame->Y.w = pBmi->biWidth;
        pFrame->Y.h = abs(pBmi->biHeight);
    }
    else    // rcTarget is NOT empty. Use a sub-rectangle in the image.
    {
        pFrame->Y.w = rcTarget->right - rcTarget->left;
        pFrame->Y.h = rcTarget->bottom - rcTarget->top;
        buff += (rcTarget->top * pFrame->Y.lineStride) +
                (rcTarget->left * pFrame->Y.pixelStride);
    }
    // set subsampling
    pFrame->U = pFrame->Y;
    switch (pBmi->biCompression)
    {
    case FCC('UYVY'):   case FCC('YUY2'):   case FCC('YVYU'):   case FCC('HDYC'):
        // horizontal 2:1
        pFrame->U.w = pFrame->Y.w / 2;
        pFrame->U.pixelStride = pFrame->Y.pixelStride * 2;
        break;
    case FCC('YV12'):   case FCC('IYUV'):
        // horizontal 2:1, vertical 2:1
        pFrame->U.w = pFrame->Y.w / 2;
        pFrame->U.h = pFrame->Y.h / 2;
        pFrame->U.lineStride = pFrame->Y.lineStride / 2;
        break;
    case FCC('IF09'):   case FCC('YVU9'):
        // horizontal 4:1, vertical 4:1
        pFrame->U.w = pFrame->Y.w / 4;
        pFrame->U.h = pFrame->Y.h / 4;
        pFrame->U.lineStride = pFrame->Y.lineStride / 4;
        break;
    default:
        return E_UNEXPECTED;
    }
    pFrame->V = pFrame->U;
    // set buffer pointers
    switch (pBmi->biCompression)
    {
    case FCC('UYVY'):   case FCC('HDYC'):
        pFrame->Y.buff = buff + 1;
        pFrame->U.buff = buff;
        pFrame->V.buff = buff + 2;
        break;
    case FCC('YUY2'):
        pFrame->Y.buff = buff;
        pFrame->U.buff = buff + 1;
        pFrame->V.buff = buff + 3;
        break;
    case FCC('YVYU'):
        pFrame->Y.buff = buff;
        pFrame->U.buff = buff + 3;
        pFrame->V.buff = buff + 1;
        break;
    case FCC('IYUV'):   case FCC('IF09'):   case FCC('YVU9'):
        pFrame->Y.buff = buff;
        pFrame->U.buff = pFrame->Y.buff + (pFrame->Y.lineStride * pFrame->Y.h);
        pFrame->V.buff = pFrame->U.buff + (pFrame->U.lineStride * pFrame->U.h);
        break;
    case FCC('YV12'):
        pFrame->Y.buff = buff;
        pFrame->V.buff = pFrame->Y.buff + (pFrame->Y.lineStride * pFrame->Y.h);
        pFrame->U.buff = pFrame->V.buff + (pFrame->V.lineStride * pFrame->V.h);
        break;
    default:
        return E_UNEXPECTED;
    }
    return S_OK;
}
