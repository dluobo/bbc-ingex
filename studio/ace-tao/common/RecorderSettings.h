/*
 * $Id: RecorderSettings.h,v 1.5 2009/02/09 19:18:17 john_f Exp $
 *
 * Recorder Configuration.
 *
 * Copyright (C) 2006  British Broadcasting Corporation.
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

#ifndef RecorderSettings_h
#define RecorderSettings_h

#include <string>
#include <vector>

#include "Database.h"

// NB. Wrapping enum type not used, file_format integer used instead.
namespace Wrapping
{
    enum EnumType { NONE, MXF, MOV };
}

// NB. Coding enum type not used, resolution integer used instead.
namespace Coding
{
    enum EnumType { UNCOMPRESSED, DV25, DV50, MJPEG21, MJPEG31, MJPEG101, MJPEG101M, MJPEG151S, MJPEG201, MPEG2 };
}

namespace Input
{
    enum EnumType { NORMAL, QUAD };
}

struct EncodeParams
{
    int resolution;
    int file_format;
    Input::EnumType source;
    bool bitc;
    std::string dir;
    std::string dest;
    int copy_priority;
};


/**
Holds configuration parameters for the Recorder.
This class translates from the less elegant database representation (encode1_resolution etc.)
to a more elegant vector of encodings to be performed.
*/
class RecorderSettings
{
public:
    static RecorderSettings * Instance();

    // Methods
    bool Update(prodauto::Recorder * rec);
    //const char *ResolutionName(int resolution);

    // Record Parameters
    prodauto::Rational image_aspect;
    int timecode_mode;
    std::string copy_command;

    std::vector<EncodeParams> encodings;

    bool browse_audio;

    int mpeg2_bitrate;
    int raw_audio_bits;
    int mxf_audio_bits;
    int browse_audio_bits;

    std::string mxf_subdir_creating;
    std::string mxf_subdir_failures;


private:
    RecorderSettings();
    static RecorderSettings * mInstance;
};

#endif //#ifndef RecorderSettings_h

