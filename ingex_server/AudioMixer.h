/*
 * $Id: AudioMixer.h,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
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

#ifndef AudioMixer_h
#define AudioMixer_h

/**
Audio mixer to mix from 4 channels to 2 channels.
*/

// We will check in constructor that these types are appropriate.
typedef int audio_sample32_t; // 32-bit
typedef short audio_sample16_t; // 16-bit

class AudioMixer
{
public:
	enum EnumeratedMatrix { MAIN, ISO, CH12 };
	AudioMixer(EnumeratedMatrix matrix);
	void Mix(const audio_sample32_t * in1,
		const audio_sample32_t * in3,
		audio_sample16_t * out,
		int num_samples);
private:
	double m11;
	double m12;
	double m13;
	double m14;
	double m21;
	double m22;
	double m23;
	double m24;
	double mSampleMax;
	double mSampleMin;
};

#endif //#ifndef AudioMixer_h
