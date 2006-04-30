/*
 * $Id: SdiRecorderImpl.cpp,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
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

#include <iostream>
#include "SdiRecorderImpl.h"
#include "Misc/Utils.h"
#include "utils.h"
#include "recorder.h"

// Static member for instance pointer
SdiRecorderImpl * SdiRecorderImpl::mInstance = 0;

const unsigned long PRE_ROLL = 100;
const unsigned long POST_ROLL = 75;
#if defined(WIN32)
const char * DIR_DELIM = "\\";
const char * VIDEO_ROOT = "C:\\TEMP\\";
#else
const char * DIR_DELIM = "/";
const char * VIDEO_ROOT = "/video/";
const char * DVD_ROOT = "/dvd/";
#endif

bool SdiRecorderImpl::Init()
{
	openLogFileWithDate("recorder");

	logTF("SdiRecorderImpl::Init()\n");
	if (! Recorder::recorder_init()) {
		logTF("  [RET] false\n");
		return false;
	}
	logTF("  [RET] true\n");
	return true;
}

void SdiRecorderImpl::CleanUp()
{
	std::cerr << "SdiRecorderImpl::CleanUp()\n";
}

bool SdiRecorderImpl::StartRecording(unsigned long & start_time,
									 const char * directory,
									 bool enable[],
									 const char * tn[],
									 const char * description)
{
	// Create the recorder for this start->stop record session
	mRec = new Recorder();

	// Create and check absolute record path
	std::string full_path = VIDEO_ROOT;
	full_path += directory;
	Utils::CreatePath(full_path);
	full_path += DIR_DELIM;

	// Create and check absolute record path
	dvd_path = DVD_ROOT;
	dvd_path += directory;
	Utils::CreatePath(dvd_path);
	dvd_path += DIR_DELIM;

	// Report recording details
	char tmp[32];
	logTF("\nSdiRecorderImpl::StartRecording():\n");
	logTF("  start_time  %ld %s\n", start_time, framesToStr(start_time, tmp));
	logTF("  directory   \"%s\"\n", directory);
	logTF("  tapenames   %s,%s,%s\n", tn[0], tn[1], tn[2]);
	logTF("  enable      %d,%d,%d\n", enable[0], enable[1], enable[2]);
	logTF("  description \"%s\"\n", description);

	// Modify start time with pre-roll
	start_time -= PRE_ROLL;
	// and save for calculating duration
	mStartTime = start_time;

	logTF("  ->start_time-PRE  %lu %s\n", start_time, framesToStr(start_time, tmp));
	logTF("  ->full path \"%s\"\n", full_path.c_str());
	logTF("  ->dvd path  \"%s\"\n", dvd_path.c_str());

	// kludge to test crash record
	bool b_crash = false;
	if (strcmp(description, "crash test") == 0)
		b_crash = true;

	// copy enable array since we need to include the 4th source i.e. quad
	bool source_enable[4] = {enable[0], enable[1], enable[2], true};

	if (! mRec->recorder_prepare_start(
							start_time,			// target timecode
							source_enable,		// array of sources to record
							tn,
							description,
							full_path.c_str(),	// path to uncompressed dir
							dvd_path.c_str(),	// path to dvd dir
							5000,				// bitrate in kbps e.g. 5000 = 5Mbps
							b_crash				// boolean crash record
							)) {
		logTF("SdiRecorderImpl::StartRecording():\n  [RET] false\n");
		return false;
	}

	if (! mRec->recorder_start()) {
		logTF("SdiRecorderImpl::StartRecording():\n  [RET] false\n");
		return false;
	}

	logTF("SdiRecorderImpl::StartRecording():\n  [RET] true\n");
	return true;
}

void SdiRecorderImpl::StopRecording(unsigned long & stop_time,
									const char * file[],
									unsigned long & duration)
{
	char tmp[32];
	logTF("\nSdiRecorderImpl::StopRecording():\n");
	logTF("  stop_time              %ld %s\n", stop_time, framesToStr(stop_time, tmp));

	// set filenames
	for (unsigned i = 0; i < RECORD_CHANNELS; i++) {
		strcpy(dvd_fullpath[i], dvd_path.c_str());
		switch (i) {
			case 0: strcat(dvd_fullpath[i], "MAIN"); break;
			case 1: strcat(dvd_fullpath[i], "ISO1"); break;
			case 2: strcat(dvd_fullpath[i], "ISO2"); break;
		}
		// skip dvd root to return path component
		file[i] = dvd_fullpath[i] + strlen(DVD_ROOT);
	}

	// return stop time with post-roll
	stop_time += POST_ROLL;
	logTF("  ->stop_time+POST_ROLL  %ld %s\n", stop_time, framesToStr(stop_time, tmp));

	// return duration
	int desired_duration = stop_time - mStartTime;

	int signalled_duration = mRec->recorder_stop(desired_duration);

	// This is the duration returned by the recorder
	// If no recordings were started this value is 0
	duration = signalled_duration;

	logTF("SdiRecorderImpl::StopRecording():\n");
	for (unsigned i = 0; i < RECORD_CHANNELS; ++i)
		logTF("  [RET] file[%u]=\"%s\"\n", i, file[i]);
	logTF("  [RET] duration=%ld\n", duration);
}
