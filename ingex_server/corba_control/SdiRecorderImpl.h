/*
 * $Id: SdiRecorderImpl.h,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Corba servant for event-based control of ingex recorder.
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

#ifndef SdiRecorderImpl_h
#define SdiRecorderImpl_h

#include <string>
#include "recorder.h"

const unsigned int RECORD_CHANNELS = 3;

/**
Interface to low-level recorder implementation.
*/
class SdiRecorderImpl
{
public:
// singleton access
	static SdiRecorderImpl * Instance()
	{
		if(0 == mInstance)
		{
			mInstance = new SdiRecorderImpl();
		}
		return mInstance;
	}
// singleton destroy
	void Destroy() { mInstance = 0; delete this; }

// recorder control
	bool Init();
	void CleanUp();
	bool  StartRecording(unsigned long & start_time,
		const char * directory,
		bool enable[],
		const char * tn[],
		const char * description);
	void StopRecording(unsigned long & stop_time,
		const char * file[],
		unsigned long & duration);

protected:
// constructor protected as this is a singleton
	SdiRecorderImpl() {}

private:
// singleton instance pointer
	static SdiRecorderImpl * mInstance;
// start time
	unsigned long mStartTime;
// record paths so we can return a const char *
	std::string dvd_path;
	char dvd_fullpath[RECORD_CHANNELS][FILENAME_MAX];
	// pointer to recorder for current session
	Recorder *mRec;
};

#endif //#ifndef SdiRecorderImpl_h
