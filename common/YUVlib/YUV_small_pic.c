#include "YUV_frame.h"
#include "YUV_small_pic.h"

static inline int max(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int min(int a, int b)
{
    return (a < b) ? a : b;
}

// Sub sample a line with no filtering
static void h_sub_alias(BYTE* srcLine, BYTE* dstLine,
                          const int inStride, const int outStride,
                          const int hsub, const int w)
{
    int		i;
    int		inSkip;

    inSkip = inStride * hsub / 100;
    for (i = 0; i < w; i++)
    {
        *dstLine = *srcLine;
        dstLine += outStride;
        srcLine += inSkip;
    }
}

// Sub sample a line with (1,2,1)/4 filtering
static void h_sub_2_121(BYTE* srcLine, BYTE* dstLine,
                        const int inStride, const int outStride,
                        const int hsub, const int w)
{
    int		acc;
    int		i;

    // repeat first edge sample
    acc = *srcLine * 3;
    srcLine += inStride;
    acc += *srcLine;
    *dstLine = acc / 4;
    for (i = 0; i < w - 1; i++)
    {
        acc = *srcLine;
        srcLine += inStride;
        acc += *srcLine * 2;
        srcLine += inStride;
        acc += *srcLine;
        *dstLine = acc / 4;
        dstLine += outStride;
    }
}

// Output is shifted one pixel to right
static void h_sub_2_121_s(BYTE* srcLine, BYTE* dstLine,
                          const int inStride, const int outStride,
                          const int hsub, const int w)
{
    int		acc;
    int		i;

    for (i = 0; i < w - 1; i++)
    {
        acc = *srcLine;
        srcLine += inStride;
        acc += *srcLine * 2;
        srcLine += inStride;
        acc += *srcLine;
        *dstLine = acc / 4;
        dstLine += outStride;
    }
    // repeat last edge sample
    acc = *srcLine;
    srcLine += inStride;
    acc += *srcLine * 3;
    *dstLine = acc / 4;
}

// Sub sample a line with (1,1)/2 filtering
// Output is shifted 1/2 pixel to right
static void h_sub_2_11_s(BYTE* srcLine, BYTE* dstLine,
                         const int inStride, const int outStride,
                         const int hsub, const int w)
{
    int		acc;
    int		i;

    for (i = 0; i < w; i++)
    {
        acc = *srcLine;
        srcLine += inStride;
        acc += *srcLine;
        srcLine += inStride;
        *dstLine = acc / 2;
        dstLine += outStride;
    }
}

// Sub sample a line with (1,1,1)/3 filtering
static void h_sub_3_111(BYTE* srcLine, BYTE* dstLine,
                        const int inStride, const int outStride,
                        const int hsub, const int w)
{
    int		acc;
    int		i;

    // repeat first edge sample
    acc = *srcLine;
    for (i = 0; i < w; i++)
    {
        acc += *srcLine;
        srcLine += inStride;
        acc += *srcLine;
        srcLine += inStride;
        *dstLine = acc / 3;
        dstLine += outStride;
        acc = *srcLine;
        srcLine += inStride;
    }
}

// Do (1,2,1)/4 vertical filtering
static void v_fil_121(BYTE* inBuffA, BYTE* inBuffB,
                      BYTE* inBuffC, BYTE* outBuff,
                      const int inStride, const int outStride, const int w)
{
    int		i;

    if (inStride == 1 && outStride == 1)
        for (i = 0; i < w; i++)
            *outBuff++ = (*inBuffA++ + (*inBuffB++ * 2) + *inBuffC++) / 4;
    else
        for (i = 0; i < w; i++)
        {
            *outBuff = (*inBuffA + (*inBuffB * 2) + *inBuffC) / 4;
            outBuff += outStride;
            inBuffA += inStride;
            inBuffB += inStride;
            inBuffC += inStride;
        }
}

// Do (1,1)/2 vertical filtering
static void v_fil_11(BYTE* inBuffA, BYTE* inBuffB, BYTE* outBuff,
                     const int inStride, const int outStride, const int w)
{
    int		i;

    if (inStride == 1 && outStride == 1)
        for (i = 0; i < w; i++)
            *outBuff++ = (*inBuffA++ + *inBuffB++) / 2;
    else
        for (i = 0; i < w; i++)
        {
            *outBuff = (*inBuffA + *inBuffB) / 2;
            outBuff += outStride;
            inBuffA += inStride;
            inBuffB += inStride;
        }
}

// Do (1,1,1)/3 vertical filtering
static void v_fil_111(BYTE* inBuffA, BYTE* inBuffB,
                      BYTE* inBuffC, BYTE* outBuff,
                      const int inStride, const int outStride, const int w)
{
    int		i;

    if (inStride == 1 && outStride == 1)
        for (i = 0; i < w; i++)
            *outBuff++ = (*inBuffA++ + *inBuffB++ + *inBuffC++) / 3;
    else
        for (i = 0; i < w; i++)
        {
            *outBuff = (*inBuffA + *inBuffB + *inBuffC) / 3;
            outBuff += outStride;
            inBuffA += inStride;
            inBuffB += inStride;
            inBuffC += inStride;
        }
}

typedef void sub_line_proc(BYTE*, BYTE*, const int, const int, const int,
                           const int);

// Make a half height copy of inBuff in outBuff
static void comp_2(component* inFrame, component* outFrame,
                   const int vfil, const int yoff,
                   sub_line_proc* do_h_sub, const int hsub, BYTE** work)
{
    BYTE*	inBuff = inFrame->buff;
    BYTE*	outBuff = outFrame->buff;
    int		j;

    if (vfil <= 0)
    {
        j = (yoff + 50) / 100;
        inBuff += inFrame->lineStride * j;
        for (j = 0; j < outFrame->h; j++)
        {
            do_h_sub(inBuff, outBuff,
                     inFrame->pixelStride, outFrame->pixelStride,
                     hsub, outFrame->w);
            inBuff += inFrame->lineStride * 2;
            outBuff += outFrame->lineStride;
        }
    }
    else if (vfil >= 2 && ((yoff % 100) < 25 || (yoff % 100) >= 75))
    {
        int A, B, C;
        // repeat first edge line
        A = 1;
        B = 1;
        C = 2;
        for (j = 0; j < outFrame->h; j++)
        {
            do_h_sub(inBuff, work[B], inFrame->pixelStride, 1,
                     hsub, outFrame->w);
            inBuff += inFrame->lineStride;
            do_h_sub(inBuff, work[C], inFrame->pixelStride, 1,
                     hsub, outFrame->w);
            inBuff += inFrame->lineStride;
            v_fil_121(work[A], work[B], work[C], outBuff,
                      1, outFrame->pixelStride, outFrame->w);
            outBuff += outFrame->lineStride;
            A = C;
            C = 2 - A;
        }
    }
    else
    {
        for (j = 0; j < outFrame->h; j++)
        {
            do_h_sub(inBuff, work[0], inFrame->pixelStride, 1,
                     hsub, outFrame->w);
            inBuff += inFrame->lineStride;
            do_h_sub(inBuff, work[1], inFrame->pixelStride, 1,
                     hsub, outFrame->w);
            inBuff += inFrame->lineStride;
            v_fil_11(work[0], work[1], outBuff, 1, outFrame->pixelStride,
                     outFrame->w);
            outBuff += outFrame->lineStride;
        }
    }
}

// Make a third height copy of inBuff in outBuff
static void comp_3(component* inFrame, component* outFrame,
                   const int vfil, const int yoff,
                   sub_line_proc* do_h_sub, const int hsub, BYTE** work)
{
    BYTE*	inBuff = inFrame->buff;
    BYTE*	outBuff = outFrame->buff;
    int		j;

    if (vfil <= 0)
    {
        j = (yoff + 25) / 50;
        inBuff += inFrame->lineStride * j;
        for (j = 0; j < outFrame->h; j++)
        {
            do_h_sub(inBuff, outBuff,
                     inFrame->pixelStride, outFrame->pixelStride,
                     hsub, outFrame->w);
            inBuff += inFrame->lineStride * 3;
            outBuff += outFrame->lineStride;
        }
    }
    else
    {
        j = (yoff + 25) / 50;
        if (j > 1)
            inBuff += inFrame->lineStride * (j - 1);
        // repeat first edge line
        do_h_sub(inBuff, work[0], inFrame->pixelStride, 1, hsub, outFrame->w);
        if (j > 0)
            inBuff += inFrame->lineStride;
        for (j = 0; j < outFrame->h - 1; j++)
        {
            do_h_sub(inBuff, work[1], inFrame->pixelStride, 1,
                     hsub, outFrame->w);
            inBuff += inFrame->lineStride;
            do_h_sub(inBuff, work[2], inFrame->pixelStride, 1,
                     hsub, outFrame->w);
            inBuff += inFrame->lineStride;
            v_fil_111(work[0], work[1], work[2], outBuff,
                      1, outFrame->pixelStride, outFrame->w);
            outBuff += outFrame->lineStride;
            do_h_sub(inBuff, work[0], inFrame->pixelStride, 1,
                     hsub, outFrame->w);
            inBuff += inFrame->lineStride;
        }
        do_h_sub(inBuff, work[1], inFrame->pixelStride, 1, hsub, outFrame->w);
        inBuff += inFrame->lineStride;
        do_h_sub(inBuff, work[2], inFrame->pixelStride, 1, hsub, outFrame->w);
        v_fil_111(work[0], work[1], work[2], outBuff,
                  1, outFrame->pixelStride, outFrame->w);
    }
}

int small_pic(YUV_frame* in_frame, YUV_frame* out_frame,
              int x, int y, int hsub, int vsub,
              int intlc, int hfil, int vfil, void* workSpace)
{
    sub_line_proc*	sub_line_Y;
    sub_line_proc*	sub_line_UV;
    BYTE*		work[3];
    int			ssx_in, ssx_out;
    int			ssy_in, ssy_out;
    int			hsub_Y, hsub_UV;
    int			vsub_Y, vsub_UV;
    YUV_frame		sub_frame;

    ssx_in = in_frame->Y.w / in_frame->U.w;
    ssy_in = in_frame->Y.h / in_frame->U.h;
    ssx_out = out_frame->Y.w / out_frame->U.w;
    ssy_out = out_frame->Y.h / out_frame->U.h;
    hsub_Y = hsub * 100;
    vsub_Y = vsub * 100;
    hsub_UV = hsub_Y * ssx_out / ssx_in;
    vsub_UV = vsub_Y * ssy_out / ssy_in;
    // adjust position, if required
    x = max(x, 0);
    y = max(y, 0);
    x -= x % ssx_out;
    y -= y % ssy_out;
    if (intlc)
        y -= y % 2;
    // create a sub frame representing the portion of out_frame to be written
    sub_frame = *out_frame;
    sub_frame.Y.buff += (y * sub_frame.Y.lineStride) +
                        (x * sub_frame.Y.pixelStride);
    sub_frame.U.buff += ((y/ssy_out) * sub_frame.U.lineStride) +
                        ((x/ssx_out) * sub_frame.U.pixelStride);
    sub_frame.V.buff += ((y/ssy_out) * sub_frame.V.lineStride) +
                        ((x/ssx_out) * sub_frame.V.pixelStride);
    sub_frame.Y.w = min(in_frame->Y.w * 100 / hsub_Y, out_frame->Y.w - x);
    sub_frame.Y.h = min(in_frame->Y.h * 100 / vsub_Y, out_frame->Y.h - y);
    sub_frame.U.w = min(in_frame->U.w * 100 / hsub_UV,
                        out_frame->U.w - (x/ssx_out));
    sub_frame.U.h = min(in_frame->U.h * 100 / vsub_UV,
                        out_frame->U.h - (y/ssy_out));
    sub_frame.V.w = min(in_frame->V.w * 100 / hsub_UV,
                        out_frame->V.w - (x/ssx_out));
    sub_frame.V.h = min(in_frame->V.h * 100 / vsub_UV,
                        out_frame->V.h - (y/ssy_out));
    // select horizontal sub sampling routines
    switch (hsub_Y)
    {
        case 200:
            if (hfil <= 0)
                sub_line_Y = &h_sub_alias;
            else if ((ssx_in == 2) && (ssx_out == 2))
                sub_line_Y = &h_sub_2_121_s;
            else
                sub_line_Y = &h_sub_2_121;
            break;
        case 300:
            if (hfil <= 0)
                sub_line_Y = &h_sub_alias;
            else
                sub_line_Y = &h_sub_3_111;
            break;
        default:
            return -1;
    }
    switch (hsub_UV)
    {
        case 100:
            sub_line_UV = &h_sub_alias;
            break;
        case 200:
            if (hfil <= 0)
                sub_line_UV = &h_sub_alias;
            else if ((ssx_in == 2) && (ssx_out == 2))
                sub_line_UV = &h_sub_2_11_s;
            else
                sub_line_UV = &h_sub_2_121;
            break;
        case 300:
            if (hfil <= 0)
                sub_line_UV = &h_sub_alias;
            else
                sub_line_UV = &h_sub_3_111;
            break;
        default:
            return -1;
    }
    work[0] = workSpace;
    work[1] = work[0] + in_frame->Y.w;
    work[2] = work[1] + in_frame->Y.w;
    if (intlc)
    {
        YUV_frame	in_field;
        YUV_frame	out_field;
        int		f;
        for (f = 0; f < 2; f++)
        {
            extract_YUV_field(in_frame, &in_field, f);
            extract_YUV_field(&sub_frame, &out_field, f);
            switch (vsub_Y)
            {
                case 200:
                    comp_2(&in_field.Y, &out_field.Y, vfil, f * 50,
                           sub_line_Y, hsub_Y, work);
                    break;
                case 300:
                    comp_3(&in_field.Y, &out_field.Y, vfil, f * 50,
                           sub_line_Y, hsub_Y, work);
                    break;
                default:
                    return -1;
            }
            switch (vsub_UV)
            {
                case 200:
                    comp_2(&in_field.U, &out_field.U, vfil, f * 50,
                           sub_line_UV, hsub_UV, work);
                    comp_2(&in_field.V, &out_field.V, vfil, f * 50,
                           sub_line_UV, hsub_UV, work);
                    break;
                case 300:
                    comp_3(&in_field.U, &out_field.U, vfil, f * 50,
                           sub_line_UV, hsub_UV, work);
                    comp_3(&in_field.V, &out_field.V, vfil, f * 50,
                           sub_line_UV, hsub_UV, work);
                    break;
                default:
                    return -1;
            }
        }
    }
    else // not interlaced
    {
        switch (vsub_Y)
        {
            case 200:
                comp_2(&in_frame->Y, &sub_frame.Y, vfil, 0,
                       sub_line_Y, hsub_Y, work);
                break;
            case 300:
                comp_3(&in_frame->Y, &sub_frame.Y, vfil, 0,
                       sub_line_Y, hsub_Y, work);
                break;
            default:
                return -1;
        }
        switch (vsub_UV)
        {
            case 200:
                comp_2(&in_frame->U, &sub_frame.U, vfil, 0,
                       sub_line_UV, hsub_UV, work);
                comp_2(&in_frame->V, &sub_frame.V, vfil, 0,
                       sub_line_UV, hsub_UV, work);
                break;
            case 300:
                comp_3(&in_frame->U, &sub_frame.U, vfil, 0,
                       sub_line_UV, hsub_UV, work);
                comp_3(&in_frame->V, &sub_frame.V, vfil, 0,
                       sub_line_UV, hsub_UV, work);
                break;
            default:
                return -1;
        }
    }
    return 0;
}
