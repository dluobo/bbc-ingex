#include "video_conversion_10bits.h"

/*
These routines convert video frames to and from the 10-bit YUV "v210"
data format. This format stores 4:2:2 YUV, with 10 bits for each data
value. Groups of three 10-bit values are packed into 4 8-bit bytes, with
an overhead of 2 bits per group, or 6.7%.

Although the "v210" format is quite efficient in terms of storage, it is
not well suited to any image manipulation (such as replay on a computer)
as there is a lot of bit shifting required to unpack it. Hence it is
quite common to store 10-bit values in 16-bit shorts, a 60% overhead.

For many purposes 8 bits per sample are quite sufficient, as long as the
quantisation from 10 bits is done without introducing artefacts. There
are many image quantisation schemes available, such as Heckbert's
"median cut" or Floyd-Steinberg as used in the NETPBM package's
"ppmquant" routine. However, to convert video from 10 to 8 bits, a
simple "error feedback" algorithm is sufficient, and not too hard to
implement.

The principle of error feedback is quite simple to explain. After
quantising each value, the difference between quantised and unquantised
values is added to the next value before it is quantised. For example,
if we wish to remove the two least significant bits from a constant
input value of 5, we get the following sequence:

input     modified input      quantised      error
    5              5              4            1
    5              6              4            2
    5              8              8            0
    5              5              4            1
    5              6              4            2
    5              8              8            0

Our constant 5 is represented by a 4,4,8 sequence that has an average
value of 5.3, a "quantisation error" of 0.3 instead of the error of 1
that would be introduced by simply dropping the lsbs.

In general, error feedback reduces quantisation error at the expense of
introducing a noise like signal, as the lsbs of 10-bit video will be mostly
noise anyway. However, for some special cases (such as an electronic graphic)
there may be insufficient noise in the signal to disguise the patterns error
feedback can produce. These patterns can be rendered invisible by starting each
line of video with a different error value. Rather than use a random number
generator, which could produce moving patterns, a small static array of random
numbers is used.

The routine "DitherFrame" converts from 10 bits to 8 bits, with error
feedback. It processes each input line in groups of 6 pixels, i.e. 12 YUV
samples. Firstly the 12 10-bit values are unpacked to 16-bit unsigned short
ints by the routine "unpack12". Then these numbers are quantised to 8 bits
using error feedback.

The routine "ConvertFrame10to8" provides the same unpacking and quantisation,
but without any error feedback. The routine "ConvertFrame8to10" repacks 8-bit
YUV data into 10-bit v210 format.
*/

#define Q_LOSE	0x0003
#define Q_KEEP	(~Q_LOSE)
#define MAX_10	0x03ff

// Routine to unpack 16 bytes of 10-bit input to 12 unsigned shorts
static void unpack12(const uint8_t* pIn, unsigned short* pOut)
{
	int		i;

	for (i = 0; i < 4; i++)
	{
		*pOut = *pIn++;
		*pOut++ += (*pIn & 0x03) << 8;
		*pOut = *pIn++ >> 2;
		*pOut++ += (*pIn & 0x0f) << 6;
		*pOut = *pIn++ >> 4;
		*pOut++ += (*pIn++ & 0x3f) << 4;
	}
}

typedef struct
{
	unsigned short	U0, Y0, V0, Y1;
} pixels10;

// Random array of error values to seed each line of Y, U & V
// Doesn't need to be very long, just enough to make any quantising patterns invisible.
#if Q_LOSE == 0x0003
// final 10 to 8-bit version
static unsigned short Err0[] =
{
	2, 3, 0, 0, 3, 1, 2, 3,
	1, 1, 1, 1, 0, 1, 0, 3,
	0, 2, 3, 0, 0, 3, 1, 1,
	0, 3, 3, 1, 2, 2, 3, 2
};
#else
// temporary 10 to 5-bit version
static unsigned short Err0[] =
{
	1,  29, 29, 7,  23, 24, 9,  17,
	31, 5,  30, 18, 1,  1,  2,  20,
	0,  7,  30, 1,  9,  5,  4,  15,
	14, 23, 17, 12, 25, 14, 11, 23
};
#endif
#define Err0Len (sizeof(Err0) / sizeof(Err0[0]))

void DitherFrame(uint8_t* pOutFrame, const uint8_t* pInFrame,
				 const int StrideOut, const int StrideIn,
				 const int xLen, const int yLen)
{
	int				Err0Idx;
	const uint8_t*	pIn;
	uint8_t*		pOut;
	pixels10		in10[3];
	unsigned short	Yerr, Uerr, Verr;
	int				i, x, y;

	Err0Idx = 0;
	for (y = 0; y < yLen; y++)
	{
		pIn = pInFrame;
		pOut = pOutFrame;
		Yerr = Err0[Err0Idx];
		Err0Idx = (Err0Idx + 1) % Err0Len;
		Uerr = Err0[Err0Idx];
		Err0Idx = (Err0Idx + 1) % Err0Len;
		Verr = Err0[Err0Idx];
		Err0Idx = (Err0Idx + 1) % Err0Len;
		for (x = 0; x < xLen; x += 6)
		{
			// decode input
			unpack12(pIn, (unsigned short*)&in10);
			pIn += 16;
			// quantise with error feedback
			for (i = 0; i < 3; i++)
			{
				in10[i].U0 += Uerr;
				if (in10[i].U0 > MAX_10) in10[i].U0 = MAX_10;
				Uerr = in10[i].U0 & Q_LOSE;
				*pOut++ = (in10[i].U0 & Q_KEEP) >> 2;

				in10[i].Y0 += Yerr;
				if (in10[i].Y0 > MAX_10) in10[i].Y0 = MAX_10;
				Yerr = in10[i].Y0 & Q_LOSE;
				*pOut++ = (in10[i].Y0 & Q_KEEP) >> 2;

				in10[i].V0 += Verr;
				if (in10[i].V0 > MAX_10) in10[i].V0 = MAX_10;
				Verr = in10[i].V0 & Q_LOSE;
				*pOut++ = (in10[i].V0 & Q_KEEP) >> 2;

				in10[i].Y1 += Yerr;
				if (in10[i].Y1 > MAX_10) in10[i].Y1 = MAX_10;
				Yerr = in10[i].Y1 & Q_LOSE;
				*pOut++ = (in10[i].Y1 & Q_KEEP) >> 2;
			}
		}
		pInFrame += StrideIn;
		pOutFrame += StrideOut;
	}
}

// Routine to pack 12 unsigned shorts into 16 bytes of 10-bit output
static void pack12(unsigned short* pIn, uint8_t* pOut)
{
	int		i;

	for (i = 0; i < 4; i++)
	{
		*pOut++ = *pIn & 0xff;
		*pOut = (*pIn++ >> 8) & 0x03;
		*pOut++ += (*pIn & 0x3f) << 2;
		*pOut = (*pIn++ >> 6) & 0x0f;
		*pOut++ += (*pIn & 0x0f) << 4;
		*pOut++ = (*pIn++ >> 4) & 0x3f;
	}
}

void ConvertFrame10to8(uint8_t* pOutFrame, const uint8_t* pInFrame,
                       const int StrideOut, const int StrideIn,
                       const int xLen, const int yLen)
{
	const uint8_t*		pIn;
	uint8_t*			pOut;
	pixels10		in10[3];
	int				i, x, y;

	for (y = 0; y < yLen; y++)
	{
		pIn = pInFrame;
		pOut = pOutFrame;
		for (x = 0; x < xLen; x += 6)
		{
			// decode input
			unpack12(pIn, (unsigned short*)&in10);
			pIn += 16;
			// quantise, discarding lsbs
			for (i = 0; i < 3; i++)
			{
				*pOut++ = (in10[i].U0 & Q_KEEP) >> 2;
				*pOut++ = (in10[i].Y0 & Q_KEEP) >> 2;
				*pOut++ = (in10[i].V0 & Q_KEEP) >> 2;
				*pOut++ = (in10[i].Y1 & Q_KEEP) >> 2;
			}
		}
		pInFrame += StrideIn;
		pOutFrame += StrideOut;
	}
}

void ConvertFrame8to10(uint8_t* pOutFrame, const uint8_t* pInFrame,
                       const int StrideOut, const int StrideIn,
                       const int xLen, const int yLen)
{
	const uint8_t*		pIn;
	uint8_t*			pOut;
	pixels10		out10[3];
	int				i, x, y;

	for (y = 0; y < yLen; y++)
	{
		pIn = pInFrame;
		pOut = pOutFrame;
		for (x = 0; x < xLen; x += 6)
		{
			// copy input to array of 10-bit values
			for (i = 0; i != 3; i++)
			{
				out10[i].U0 = *pIn++ << 2;
				out10[i].Y0 = *pIn++ << 2;
				out10[i].V0 = *pIn++ << 2;
				out10[i].Y1 = *pIn++ << 2;
			}
			// encode output
			pack12((unsigned short*)&out10, pOut);
			pOut += 16;
		}
		pInFrame += StrideIn;
		pOutFrame += StrideOut;
	}
}
