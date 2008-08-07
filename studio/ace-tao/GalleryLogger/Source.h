/*
 * $Id: Source.h,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Class to describe a "source" which might be recorded during
 * TV production.
 *
 * Copyright (C) 2007  British Broadcasting Corporation.
 * All Rights Reserved.
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

#ifndef Source_h
#define Source_h

#include "Recorder.h"

/**
Describes a "source" which might be recorded during TV production, for example
"Mixer out", "Camera 2".
I have also included a couple of Recorders connected to the source.  There may
be a better way of describing that relationship.
*/
class Source
{
public:
	Source() : mEnabled(false) {}

	const char * Name() const { return mName.c_str(); }
	void Name(const char * s) { mName = s; }
	bool NameIsEmpty() const { return mName.empty(); }

	const char * Description() const { return mDescription.c_str(); }
	void Description(const char * s) { mDescription = s; }
	bool DescriptionIsEmpty() const { return mDescription.empty(); }

	/*
	const char * Tape() const { return mTape.c_str(); }
	void Tape(const char * s) { mTape = s; }
	bool TapeIsEmpty() const { return (0 == mTape.size()); }

	const char * Device() const { return mDevice.c_str(); }
	void Device(const char * s) { mDevice = s; }
	bool DeviceIsEmpty() const { return (0 == mDevice.size()); }
	*/

	Recorder & TapeRecorder() { return mTapeRecorder; }
	Recorder & FileRecorder() { return mFileRecorder; }

	bool Enabled() { return mEnabled; }
	void Enabled(bool b) { mEnabled = b; }

private:
	std::string mName;
	std::string mDescription;
	Recorder mTapeRecorder;  // Potentially there could be more recorders
	Recorder mFileRecorder;  // but we'll fix at one of each type for the moment.
	//std::string mTape;
	//std::string mDevice;
	bool mEnabled;
};

#endif //#ifndef Source_h