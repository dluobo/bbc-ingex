/*
 * $Id: RecorderSettings.cpp,v 1.9 2009/10/12 15:05:38 john_f Exp $
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

#include <ace/Log_Msg.h>
#include "Database.h"
#include "RecorderSettings.h"

// These are the default values that will be used if
// config cannot be read from the database.

const prodauto::Rational    IMAGE_ASPECT    = prodauto::g_16x9ImageAspect;
//const int                   TIMECODE_MODE   = LTC_PARAMETER_VALUE;
const char * const          COPY_COMMAND    = "";

const int           ENCODE1_RESOLUTION      = MJPEG21_MATERIAL_RESOLUTION;
const int           ENCODE1_FILE_FORMAT     = MXF_FILE_FORMAT_TYPE;
const bool          ENCODE1_BITC            = false;
const char * const  ENCODE1_DIR             = "/video";
const char * const  ENCODE1_COPY_DEST       = "/store";
const int           ENCODE1_COPY_PRIORITY   = 1;

const int           ENCODE2_RESOLUTION      = 0;
const int           ENCODE2_FILE_FORMAT     = MXF_FILE_FORMAT_TYPE;
const bool          ENCODE2_BITC            = false;
const char * const  ENCODE2_DIR             = "/video";
const char * const  ENCODE2_COPY_DEST       = "/store";
const int           ENCODE2_COPY_PRIORITY   = 2;

const int           ENCODE3_RESOLUTION      = 0;
const int           ENCODE3_FILE_FORMAT     = MXF_FILE_FORMAT_TYPE;
const bool          ENCODE3_BITC            = false;
const char * const  ENCODE3_DIR             = "/video";
const char * const  ENCODE3_COPY_DEST       = "/store";
const int           ENCODE3_COPY_PRIORITY   = 3;

const int           QUAD_RESOLUTION     = 0;
const int           QUAD_FILE_FORMAT    = RAW_FILE_FORMAT_TYPE;
const bool          QUAD_BITC           = true;
const char * const  QUAD_DIR            = "/video";
const char * const  QUAD_COPY_DEST      = "";
const int           QUAD_COPY_PRIORITY  = 3;

const bool BROWSE_AUDIO = false;

const int MPEG2_BITRATE = 5000;
const int RAW_AUDIO_BITS = 16;
const int MXF_AUDIO_BITS = 16;
const int BROWSE_AUDIO_BITS = 16;

const char * const MXF_SUBDIR_CREATING = "Creating/";
const char * const MXF_SUBDIR_FAILURES = "Failures/";
const char * const MXF_SUBDIR_METADATA = "xml/";



// static instance pointer
RecorderSettings * RecorderSettings::mInstance = 0;

// Singelton access
RecorderSettings * RecorderSettings::Instance()
{
    if (mInstance == 0)  // is it the first call?
    {  
        mInstance = new RecorderSettings; // create sole instance
    }
    return mInstance; // address of sole instance
}

// Constructor
RecorderSettings::RecorderSettings() :
    image_aspect(IMAGE_ASPECT),
    //timecode_mode(TIMECODE_MODE),
    copy_command(COPY_COMMAND),
    browse_audio(BROWSE_AUDIO),
    mpeg2_bitrate(MPEG2_BITRATE),
    raw_audio_bits(RAW_AUDIO_BITS),
    mxf_audio_bits(MXF_AUDIO_BITS),
    browse_audio_bits(BROWSE_AUDIO_BITS),
    mxf_subdir_creating(MXF_SUBDIR_CREATING),
    mxf_subdir_failures(MXF_SUBDIR_FAILURES),
    mxf_subdir_metadata(MXF_SUBDIR_METADATA)
{
}

/**
Update the configuration parameters, for example from a database.
*/
bool RecorderSettings::Update(prodauto::Recorder * rec)
{
    // NB. Do we need a freah instance of rec here to
    // catch any changes/
    prodauto::RecorderConfig * rc = 0;
    if (rec && rec->hasConfig())
    {
        rc = rec->getConfig();
    }

    int encode1_resolution;
    int encode1_file_format;
    bool encode1_bitc;
    std::string encode1_dir;
    std::string encode1_copy_dest;
    int encode1_copy_priority;

    int encode2_resolution;
    int encode2_file_format;
    bool encode2_bitc;
    std::string encode2_dir;
    std::string encode2_copy_dest;
    int encode2_copy_priority;

    int encode3_resolution;
    int encode3_file_format;
    bool encode3_bitc;
    std::string encode3_dir;
    std::string encode3_copy_dest;
    int encode3_copy_priority;

    int quad_resolution;
    int quad_file_format;
    bool quad_bitc;
    std::string quad_dir;
    std::string quad_copy_dest;
    int quad_copy_priority;

    if (rc)
    {
        image_aspect = rc->getRationalParam("IMAGE_ASPECT", IMAGE_ASPECT);
        //timecode_mode = rc->getIntParam("TIMECODE_MODE", TIMECODE_MODE);
        copy_command = rc->getStringParam("COPY_COMMAND", COPY_COMMAND);

        encode1_resolution = rc->getIntParam("ENCODE1_RESOLUTION", ENCODE1_RESOLUTION);
        encode1_file_format = rc->getIntParam("ENCODE1_WRAPPING", ENCODE1_FILE_FORMAT);
        encode1_bitc = rc->getBoolParam("ENCODE1_BITC", ENCODE1_BITC);
        encode1_dir = rc->getStringParam("ENCODE1_DIR", ENCODE1_DIR);
        encode1_copy_dest = rc->getStringParam("ENCODE1_COPY_DEST", ENCODE1_COPY_DEST);
        encode1_copy_priority = rc->getIntParam("ENCODE1_COPY_PRIORITY", ENCODE1_COPY_PRIORITY);

        encode2_resolution = rc->getIntParam("ENCODE2_RESOLUTION", ENCODE2_RESOLUTION);
        encode2_file_format = rc->getIntParam("ENCODE2_WRAPPING", ENCODE2_FILE_FORMAT);
        encode2_bitc = rc->getBoolParam("ENCODE2_BITC", ENCODE2_BITC);
        encode2_dir = rc->getStringParam("ENCODE2_DIR", ENCODE2_DIR);
        encode2_copy_dest = rc->getStringParam("ENCODE2_COPY_DEST", ENCODE2_COPY_DEST);
        encode2_copy_priority = rc->getIntParam("ENCODE2_COPY_PRIORITY", ENCODE2_COPY_PRIORITY);

        encode3_resolution = rc->getIntParam("ENCODE3_RESOLUTION", ENCODE3_RESOLUTION);
        encode3_file_format = rc->getIntParam("ENCODE3_WRAPPING", ENCODE3_FILE_FORMAT);
        encode3_bitc = rc->getBoolParam("ENCODE3_BITC", ENCODE3_BITC);
        encode3_dir = rc->getStringParam("ENCODE3_DIR", ENCODE3_DIR);
        encode3_copy_dest = rc->getStringParam("ENCODE3_COPY_DEST", ENCODE3_COPY_DEST);
        encode3_copy_priority = rc->getIntParam("ENCODE3_COPY_PRIORITY", ENCODE3_COPY_PRIORITY);

        quad_resolution = rc->getIntParam("QUAD_RESOLUTION", QUAD_RESOLUTION);
        quad_file_format = rc->getIntParam("QUAD_WRAPPING", QUAD_FILE_FORMAT);
        quad_bitc = rc->getBoolParam("QUAD_BITC", QUAD_BITC);
        quad_dir = rc->getStringParam("QUAD_DIR", QUAD_DIR);
        quad_copy_dest = rc->getStringParam("QUAD_COPY_DEST", QUAD_COPY_DEST);
        quad_copy_priority = rc->getIntParam("QUAD_COPY_PRIORITY", QUAD_COPY_PRIORITY);
    }
    else
    {
        image_aspect = IMAGE_ASPECT;
        //timecode_mode = TIMECODE_MODE;
        copy_command = COPY_COMMAND;

        encode1_resolution = ENCODE1_RESOLUTION;
        encode1_file_format = ENCODE1_FILE_FORMAT;
        encode1_bitc = ENCODE1_BITC;
        encode1_dir = ENCODE1_DIR;
        encode1_copy_dest = ENCODE1_COPY_DEST;
        encode1_copy_priority = ENCODE1_COPY_PRIORITY;

        encode2_resolution = ENCODE2_RESOLUTION;
        encode2_file_format = ENCODE2_FILE_FORMAT;
        encode2_bitc = ENCODE2_BITC;
        encode2_dir = ENCODE2_DIR;
        encode2_copy_dest = ENCODE2_COPY_DEST;
        encode2_copy_priority = ENCODE2_COPY_PRIORITY;

        encode3_resolution = ENCODE3_RESOLUTION;
        encode3_file_format = ENCODE3_FILE_FORMAT;
        encode3_bitc = ENCODE3_BITC;
        encode3_dir = ENCODE3_DIR;
        encode3_copy_dest = ENCODE3_COPY_DEST;
        encode3_copy_priority = ENCODE3_COPY_PRIORITY;

        quad_resolution = QUAD_RESOLUTION;
        quad_file_format = QUAD_FILE_FORMAT;
        quad_bitc = QUAD_BITC;
        quad_dir = QUAD_DIR;
        quad_copy_dest = QUAD_COPY_DEST;
        quad_copy_priority = QUAD_COPY_PRIORITY;
    }

    encodings.clear();
    if (encode1_resolution)
    {
        EncodeParams ep;
        ep.resolution = encode1_resolution;
        ep.file_format = encode1_file_format;
        ep.source = Input::NORMAL;
        ep.bitc = encode1_bitc;
        ep.dir = encode1_dir;
        ep.copy_dest = encode1_copy_dest;
        ep.copy_priority = encode1_copy_priority;
        encodings.push_back(ep);
    }
    if (encode2_resolution)
    {
        EncodeParams ep;
        ep.resolution = encode2_resolution;
        ep.file_format = encode2_file_format;
        ep.source = Input::NORMAL;
        ep.bitc = encode2_bitc;
        ep.dir = encode2_dir;
        ep.copy_dest = encode2_copy_dest;
        ep.copy_priority = encode2_copy_priority;
        encodings.push_back(ep);
    }
    if (encode3_resolution)
    {
        EncodeParams ep;
        ep.resolution = encode3_resolution;
        ep.file_format = encode3_file_format;
        ep.source = Input::NORMAL;
        ep.bitc = encode3_bitc;
        ep.dir = encode3_dir;
        ep.copy_dest = encode3_copy_dest;
        ep.copy_priority = encode3_copy_priority;
        encodings.push_back(ep);
    }
    if (quad_resolution)
    {
        EncodeParams ep;
        ep.resolution = quad_resolution;
        ep.file_format = quad_file_format;
        ep.source = Input::QUAD;
        ep.bitc = quad_bitc;
        ep.dir = quad_dir;
        ep.copy_dest = quad_copy_dest;
        ep.copy_priority = quad_copy_priority;
        encodings.push_back(ep);
    }
#if 0
    if (quad_resolution) // bodge for browse tracks
    {
        EncodeParams ep;
        ep.resolution = quad_resolution;
        ep.file_format = quad_file_format;
        ep.source = Input::NORMAL;
        ep.bitc = quad_bitc;
        ep.dir = quad_dir;
        ep.copy_dest = quad_copy_dest;
        encodings.push_back(ep);
    }
#endif

    return true;
}



