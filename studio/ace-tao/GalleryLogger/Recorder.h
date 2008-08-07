/*
 * $Id: Recorder.h,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Class to represent a recorder (tape- or file-based).
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

#ifndef Recorder_h
#define Recorder_h

class RecordingDevice;

/**
A recorder, such as a VTR or file-based recorder, with which you could
record a Source.  The result of making such a recording is a Clip.
*/
class Recorder
{
public:
    enum RecorderType { TAPE, FILE };

    void Type(RecorderType type);
    RecorderType Type();

    void Identifier(const char * identifier) { mIdentifier = identifier; }
    const char * Identifier() { return mIdentifier.c_str(); }

    RecordingDevice * Device(); // don't necessarily have one, e.g. for tape
private:
    RecorderType mType;
    std::string mIdentifier;
};

#endif //#ifndef Recorder_h
