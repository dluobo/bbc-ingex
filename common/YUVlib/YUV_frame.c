#include <stdlib.h>	// free
#include <string.h>	// memset

#include "YUV_frame.h"

int YUV_frame_from_buffer(YUV_frame* frame, void* buffer,
                          const int w, const int h,
                          const formats format)
{
    if (buffer == NULL)
        return 0;
    if ((w < 1) || (h < 1))
        return 0;
    frame->Y.w = w;
    frame->Y.h = h;
    switch (format)
    {
    case YV12: case IF09: case YVU9: case IYUV:
    case Y42B: case I420: case YV16:
        frame->Y.pixelStride = 1;
        break;
    case UYVY: case YUY2: case YVYU: case HDYC:
        frame->Y.pixelStride = 2;
        break;
    default:
        return 0;
    }
    frame->Y.lineStride = w * frame->Y.pixelStride;
    frame->U = frame->Y;
    switch (format)
    {
    case UYVY: case YUY2: case YVYU: case HDYC:
        if (w % 2 != 0)
            return 0;
        frame->U.w = w / 2;
        frame->U.pixelStride = frame->Y.pixelStride * 2;
        break;
    case YV12: case IYUV: case I420:
        if (w % 2 != 0)
            return 0;
        if (h % 2 != 0)
            return 0;
        frame->U.w = w / 2;
        frame->U.h = h / 2;
        frame->U.lineStride = frame->Y.lineStride / 2;
        break;
    case IF09: case YVU9:
        if (w % 4 != 0)
            return 0;
        if (h % 4 != 0)
            return 0;
        frame->U.w = w / 4;
        frame->U.h = h / 4;
        frame->U.lineStride = frame->Y.lineStride / 4;
        break;
    case Y42B: case YV16:
        if (w % 2 != 0)
            return 0;
        frame->U.w = w / 2;
        frame->U.lineStride = frame->Y.lineStride / 2;
        break;
    default:
        return 0;
    }
    frame->V = frame->U;
    switch (format)
    {
    case UYVY: case HDYC:
        frame->U.buff = buffer;
        frame->Y.buff = frame->U.buff + 1;
        frame->V.buff = frame->U.buff + 2;
        break;
    case YUY2:
        frame->Y.buff = buffer;
        frame->U.buff = frame->Y.buff + 1;
        frame->V.buff = frame->Y.buff + 3;
        break;
    case YVYU:
        frame->Y.buff = buffer;
        frame->U.buff = frame->Y.buff + 3;
        frame->V.buff = frame->Y.buff + 1;
        break;
    case IYUV: case IF09: case YVU9: case Y42B: case I420:
        frame->Y.buff = buffer;
        frame->U.buff = frame->Y.buff + (frame->Y.lineStride * frame->Y.h);
        frame->V.buff = frame->U.buff + (frame->U.lineStride * frame->U.h);
        break;
    case YV12: case YV16:
        frame->Y.buff = buffer;
        frame->V.buff = frame->Y.buff + (frame->Y.lineStride * frame->Y.h);
        frame->U.buff = frame->V.buff + (frame->V.lineStride * frame->V.h);
        break;
    default:
        return 0;
    }
    return 1;
}

int alloc_YUV_frame(YUV_frame* frame,
                    const int w, const int h,
                    const formats format)
{
    int		size;

    switch (format)
    {
    case UYVY: case YUY2: case YVYU: case HDYC: case Y42B: case YV16:
        size = (w * h) * 2;
        break;
    case YV12: case IYUV: case I420:
        size = (w * h) * 3 / 2;
        break;
    case IF09: case YVU9:
        size = (w * h) * 9 / 8;
        break;
    default:
        return 0;
    }
    return YUV_frame_from_buffer(frame, malloc(size), w, h, format);
}

void free_YUV_frame(YUV_frame* frame)
{
    BYTE* buff;

    buff = frame->Y.buff;
    if (frame->U.buff < buff)
        buff = frame->U.buff;
    if (frame->V.buff < buff)
        buff = frame->V.buff;
    free(buff);
    frame->Y.buff = NULL;
    frame->U.buff = NULL;
    frame->V.buff = NULL;
}

extern void clear_component(component* frame, BYTE value)
{
    BYTE*	linePtr;
    BYTE*	samplePtr;
    int		x, y;

    linePtr = frame->buff;
    for (y = 0; y < frame->h; y++)
    {
        if (frame->pixelStride == 1)
            memset(linePtr, value, frame->w);
        else
        {
            samplePtr = linePtr;
            for (x = 0; x < frame->w; x++)
            {
                *samplePtr = value;
                samplePtr += frame->pixelStride;
            }
        }
        linePtr += frame->lineStride;
    }
}

extern void clear_YUV_frame(YUV_frame* frame)
{
    clear_component(&frame->Y, 16);
    clear_component(&frame->U, 128);
    clear_component(&frame->V, 128);
}

extern void YUV_601(float R, float G, float B, BYTE* Y, BYTE* U, BYTE* V)
{
    *Y =  16+((219.0*( (0.299*R)+(0.587*G)+(0.114*B)))+0.5);
    *U = 128+((112.0*(-(0.299*R)-(0.587*G)+(0.886*B))/0.886)+0.5);
    *V = 128+((112.0*( (0.701*R)-(0.587*G)-(0.114*B))/0.701)+0.5);
}

extern void YUV_709(float R, float G, float B, BYTE* Y, BYTE* U, BYTE* V)
{
    *Y =  16+((219.0*( (0.2126*R)+(0.7152*G)+(0.0722*B)))+0.5);
    *U = 128+((112.0*(-(0.2126*R)-(0.7152*G)+(0.9278*B))/0.9278)+0.5);
    *V = 128+((112.0*( (0.7874*R)-(0.7152*G)-(0.0722*B))/0.7874)+0.5);
}

void extract_field(component* in_frame, component* out_field, int field_no)
{
    *out_field = *in_frame;
    if (field_no == 0)
        out_field->h = (out_field->h + 1) / 2;
    else
    {
        out_field->h = out_field->h / 2;
        out_field->buff += out_field->lineStride;
    }
    out_field->lineStride = out_field->lineStride * 2;
}

void extract_YUV_field(YUV_frame* in_frame, YUV_frame* out_field, int field_no)
{
    extract_field(&in_frame->Y, &out_field->Y, field_no);
    extract_field(&in_frame->U, &out_field->U, field_no);
    extract_field(&in_frame->V, &out_field->V, field_no);
}
