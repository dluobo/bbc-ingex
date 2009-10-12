/*
 * $Id: AudioMixer.cpp,v 1.2 2009/10/12 14:42:40 john_f Exp $
 *
 * Simple class for mixing 4 audio tracks into 2 tracks.
 *
 * Copyright (C) 2005  John Fletcher <john_f@users.sourceforge.net>
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

#include "AudioMixer.h"

#include <assert.h>
#include <limits>

/**
Default Constructor - pass-through of channels 1 and 2
*/
AudioMixer::AudioMixer()
{
    // Check that audio sample types are suitable.
    assert(std::numeric_limits<audio_sample32_t>::is_integer);
    assert(std::numeric_limits<audio_sample32_t>::is_signed);
    assert(std::numeric_limits<audio_sample32_t>::digits == 31);

    assert(std::numeric_limits<audio_sample16_t>::is_integer);
    assert(std::numeric_limits<audio_sample16_t>::is_signed);
    assert(std::numeric_limits<audio_sample16_t>::digits == 15);

    // We set limits with a little headroom to avoid any
    // problems checking value as double and then converting to int.
    mSampleMax = std::numeric_limits<audio_sample32_t>::max() - 5;
    mSampleMin = std::numeric_limits<audio_sample32_t>::min() + 5;


    // Initialise the mixing matrix to copy channels 1 and 2 to output.
    m11 = 1.0;
    m12 = 0.0;
    m13 = 0.0;
    m14 = 0.0;
    m21 = 0.0;
    m22 = 1.0;
    m23 = 0.0;
    m24 = 0.0;
}


/**
Specify the type of mix matrix you want.
*/
void AudioMixer::SetMix(EnumeratedMatrix matrix)
{
    // Setup the mixing matrix.
    switch (matrix)
    {
    case CH12:
        // Simply copy channels 1 and 2 to output
        m11 = 1.0;
        m12 = 0.0;
        m13 = 0.0;
        m14 = 0.0;
        m21 = 0.0;
        m22 = 1.0;
        m23 = 0.0;
        m24 = 0.0;
        break;
    case CH12L3R:
        // Channel 1 & 2 mixed to left, channel 3 to right e.g. for speaking clock
        m11 = 0.707;
        m12 = 0.707;
        m13 = 0.0;
        m14 = 0.0;
        m21 = 0.0;
        m22 = 0.0;
        m23 = 1.0;
        m24 = 0.0;
        break;
    case COMMENTARY3:
        // Add commentary on channel 3 to clean mix on channel 1/2.
        m11 = 1.0;
        m12 = 0.0;
        m13 = 0.707;
        m14 = 0.0;
        m21 = 0.0;
        m22 = 1.0;
        m23 = 0.707;
        m24 = 0.0;
        break;
    case COMMENTARY4:
        // Add commentary on channel 4 to clean mix on channel 1/2.
        m11 = 1.0;
        m12 = 0.0;
        m13 = 0.0;
        m14 = 0.707;
        m21 = 0.0;
        m22 = 1.0;
        m23 = 0.0;
        m24 = 0.707;
        break;
    case ALL:
        // Space individual mics across stereo image in order 1,3,4,2
        m11 = 1.0;
        m12 = 0.0;
        m13 = 0.866;
        m14 = 0.5;
        m21 = 0.0;
        m22 = 1.0;
        m23 = 0.5;
        m24 = 0.866;
        break;
    }
}

/**
Mix 4 co-timed input samples to 2 co-timed output samples for a time series
of num_samples sample sets.
Input and output samples are presented as alternating L/R type sequence.
Input samples are 32-bit, output samples are 16 bit.
*/
void AudioMixer::Mix(const audio_sample32_t * in1,
        const audio_sample32_t * in3,
        audio_sample16_t * out,
        int num_samples)
{
    double mix1;
    double mix2;

    for (int i = 0; i < num_samples; ++i)
    {
        // Apply mixing matrix.
        mix1 = m11 * (*in1)
            + m13 * (*in3);

        mix2 = m21 * (*in1)
            + m23 * (*in3);

        ++in1;
        ++in3;

        mix1 += m12 * (*in1)
            + m14 * (*in3);

        mix2 += m22 * (*in1)
            + m24 * (*in3);

        ++in1;
        ++in3;

        // Clip samples to max/min permitted values.
        if(mix1 > mSampleMax)
        {
            mix1 = mSampleMax;
        }
        else if(mix1 < mSampleMin)
        {
            mix1 = mSampleMin;
        }
        if(mix2 > mSampleMax)
        {
            mix2 = mSampleMax;
        }
        else if(mix2 < mSampleMin)
        {
            mix2 = mSampleMin;
        }

        // Store results with scaling from 32 to 16-bit.
        *out = (audio_sample16_t) (mix1 / 65536);
        ++out;
        *out = (audio_sample16_t) (mix2 / 65536);
        ++out;
    }
}
