/*
 * $Id: recorder_functions.cpp,v 1.30 2010/01/14 15:38:11 john_f Exp $
 *
 * Functions which execute in recording threads.
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

#include "integer_types.h"
#include "IngexShm.h"
#include "IngexRecorder.h"
#include "IngexRecorderImpl.h"
#include "RecorderSettings.h"
#include "Logfile.h"
#include "Timecode.h"
#include "AudioMixer.h"
#include "audio_utils.h"
#include "ffmpeg_encoder.h"
#include "ffmpeg_encoder_av.h"
#include "mjpeg_compress.h"
#include "tc_overlay.h"
#include "EncodeFrameBuffer.h"
#include "MtEncoder.h"

#include "YUVlib/YUV_frame.h"
#include "YUVlib/YUV_quarter_frame.h"

// prodauto database
#include "Database.h"
#include "DBException.h"
#include "DatabaseEnums.h"
#include "Utilities.h"
#include "DataTypes.h"
#include "OPAtomPackageCreator.h"
#include "OP1APackageCreator.h"

// prodauto mxfwriter
#include "MXFOPAtomWriter.h"
#include "MXFWriterException.h"

#include <ace/Thread.h>
#include <ace/OS_NS_unistd.h>

#include <cstdio>
#include <iostream>
#include <sstream>

/// Mutex to ensure only one thread at a time can call avcodec open/close
static ACE_Thread_Mutex avcodec_mutex;

const bool THREADED_MJPEG = false;
const bool MT_ENABLE = true;
const bool DEBUG_NOWRITE = false;
const bool SAVE_PACKAGE_DATA = true; // Write to database for non-MXF files

#define USE_SOURCE   0 // Eventually will move to encoding a source, rather than a hardware input

// Macro to log an error using both the ACE_DEBUG() macro and the
// shared memory placeholder for error messages
// Note that format directives are not always the same
// e.g. %C with ACE_DEBUG vs. %s with printf
#define LOG_RECORD_ERROR(fmt, args...) \
{ \
    ACE_DEBUG((LM_ERROR, ACE_TEXT( fmt ), ## args)); \
    IngexShm::Instance()->InfoSetRecordError(channel_i, p_opt->index, quad_video, fmt, ## args); \
}


// Local context only
namespace
{
int64_t tv_diff_microsecs(const struct timeval * a, const struct timeval * b)
{
    int64_t diff = (b->tv_sec - a->tv_sec) * INT64_C(1000000) + b->tv_usec - a->tv_usec;
    return diff;
}

std::string strip_path(const std::string & pathname)
{
    std::string filename = pathname;
    size_t index = pathname.rfind(PATH_SEPARATOR);
    if (index != std::string::npos)
    {
        filename = pathname.substr(index + 1);
    }

    return filename;
}
} // namespace


/**
Thread function to encode and record a particular source.
*/
ACE_THR_FUNC_RETURN start_record_thread(void * p_arg)
{
    ThreadParam * param = (ThreadParam *)p_arg;
    RecordOptions * p_opt = param->p_opt;
    IngexRecorder * p_rec = param->p_rec;
    IngexRecorderImpl * p_impl = p_rec->mpImpl;

    bool quad_video = p_opt->quad;

    int ffmpeg_threads;
    p_impl->GetGlobals(ffmpeg_threads);

    // Set up packages and tracks
    int channel_i = 0; // hardware channel for video track
    prodauto::SourceConfig * sc = 0;
    std::vector<bool> track_enables; // for tracks of sc
    std::string src_name;
    std::vector<unsigned int> channels_in_use; // hardware channels


#if USE_SOURCE
    // Recording the source identified by p_opt->source_id
    // Set SourceConfig
    if (p_impl->mSourceConfigs.find(p_opt->source_id) != p_impl->mSourceConfigs.end())
    {
        sc = p_impl->mSourceConfigs[p_opt->source_id];
    }

    // Set Source name
    if (sc)
    {
        src_name = (quad_video ? QUAD_NAME : sc->name);
    }

    // track enables
    for (unsigned int i = 0; i < sc->trackConfigs.size(); ++i)
    {
        // Need to translate from the hardware track enables in p_rec->mTrackEnable
        track_enables.push_back(true);
    }

    // Assume if there is a video track it is first track
    prodauto::SourceTrackConfig * stc = 0;
    if (sc->trackConfigs.size() > 0 &&
        (stc = sc->trackConfigs[0]) != 0 &&
        stc->dataDef == PICTURE_DATA_DEFINITION)
    {
        channel_i = p_impl->TrackHwMap(stc->getDatabaseID()).channel;
    }
#else
    // Recording the input identified by p_opt->channel_num
    channel_i = p_opt->channel_num;
    // Set SourceConfig
    prodauto::Recorder * rec = p_rec->Recorder();
    prodauto::RecorderConfig * rc = 0;
    if (rec && rec->hasConfig())
    {
        rc = rec->getConfig();
    }
    prodauto::RecorderInputConfig * ric = 0;
    if (rc)
    {
        ric = rc->getInputConfig(channel_i + 1); // index starts from 1
    }
    prodauto::RecorderInputTrackConfig * ritc = 0;
    if (ric)
    {
        ritc = ric->getTrackConfig(1); // index starts from 1 so this is video track
    }
    if (ritc)
    {
        sc = ritc->sourceConfig;
    }

    // Set track enables
    unsigned int track_offset = channel_i * (1 + IngexShm::Instance()->AudioTracksPerChannel());
    for (unsigned int i = 0; i < sc->trackConfigs.size(); ++i)
    {
        track_enables.push_back(p_rec->mTrackEnable[track_offset + i]);
    }


    // Set hardware channels in use
    if (quad_video)
    {
        for (unsigned int i = 0; i < 4; ++i)
        {
            if (p_rec->mChannelEnable[i])
            {
                channels_in_use.push_back(i);
            }
        }
    }
    else
    {
        channels_in_use.push_back(channel_i);
    }

    // Set Source name
    src_name = (quad_video ? QUAD_NAME : SOURCE_NAME[channel_i]);
#endif


    // ACE logging to file
    std::ostringstream id;
    id << "record_" << src_name << "_" << p_opt->index;
    Logfile::Open(id.str().c_str(), false);

    // Reset informational data in shared memory
    IngexShm::Instance()->InfoReset(channel_i, p_opt->index, quad_video);
    IngexShm::Instance()->InfoSetEnabled(channel_i, p_opt->index, quad_video, true);

    // Convenience variables
    const int ring_length = IngexShm::Instance()->RingLength();
    const int fps = p_impl->Fps();
    const bool df = p_impl->Df();

    // Recording starts from start_frame.
    //int start_frame = p_rec->mStartFrame[channel_i];
    int start_frame[MAX_CHANNELS];
    for (unsigned int i = 0; i < MAX_CHANNELS; ++i)
    {
        start_frame[i] = p_rec->mStartFrame[i];
    }

    // Get frame rate in use
    int frame_rate_numerator;
    int frame_rate_denominator;
    IngexShm::Instance()->GetFrameRate(frame_rate_numerator, frame_rate_denominator);
    const prodauto::Rational FRAME_RATE (frame_rate_numerator, frame_rate_denominator);

    // Start timecode passed as metadata to MXFWriter
    int start_tc = p_rec->mStartTimecode;
    int64_t start_position = start_tc;
    Timecode start_timecode(start_tc, fps, df);

#if 0
    // What if we add some days to start_position?
    const int day_offset = 2;
    const int frame_offset = day_offset * 24 * 60 * 60 * FRAME_RATE.numerator / FRAME_RATE.denominator;
    start_position += frame_offset;
#endif

    // Settings pointer
    RecorderSettings * settings = RecorderSettings::Instance();

    //bool browse_audio = (channel_i == 0 && !quad_video && settings->browse_audio);
    bool browse_audio = false;
    const bool browse_mp3 = true;

    // Get encode settings for this thread
    int resolution = p_opt->resolution;
    int file_format = p_opt->file_format;

    // For DVD resolution we override the file format
    if (resolution == DVD_MATERIAL_RESOLUTION)
    {
        file_format = MPG_FILE_FORMAT_TYPE;
    }

    bool mxf = (MXF_FILE_FORMAT_TYPE == file_format);
    bool raw = !mxf;

    // video resolution name
    std::string resolution_name = DatabaseEnums::Instance()->ResolutionName(resolution);

    // file format name
    std::string file_format_name = DatabaseEnums::Instance()->FileFormatName(file_format);

    // Encode parameters
    prodauto::Rational image_aspect = settings->image_aspect;
    int wide_aspect = (image_aspect == prodauto::g_16x9ImageAspect ? 1 : 0);
    //int raw_audio_bits = settings->raw_audio_bits;
    int raw_audio_bits = 16; // has to be 16 with current deinterleave function
    //int mxf_audio_bits = settings->mxf_audio_bits;
    int mxf_audio_bits = 16; // has to be 16 with current deinterleave function
    int browse_audio_bits = settings->browse_audio_bits;

    bool bitc = p_opt->bitc;

    ACE_DEBUG((LM_INFO,
        ACE_TEXT("start_record_thread(%C thread %d, start_tc=%C %C %C %C)\n"),
        src_name.c_str(),  p_opt->index,
        start_timecode.Text(),
        file_format_name.c_str(), resolution_name.c_str(),
        (bitc ? "with BITC" : "")));

    // Note the recording location
    long location_id = 0;
    if (sc)
    {
        location_id = sc->recordingLocation;
    }
    std::string location;
    if (location_id)
    {
        location = p_impl->RecordingLocationMap(location_id);
    }
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C thread %d recording location \"%C\"\n"),
        src_name.c_str(), p_opt->index, location.c_str()));

        
    // Set some flags based on encoding type

    // Initialising these to defaults which you will get for unsupported
    // resolution/file_format combinations.
    ffmpeg_encoder_resolution_t    ff_res    = FF_ENCODER_RESOLUTION_IMX30;
    ffmpeg_encoder_av_resolution_t ff_av_res = FF_ENCODER_RESOLUTION_MPEG4_MOV;

    // Initialising these just to avoid compiler warnings
    MJPEGResolutionID mjpeg_res = MJPEG_20_1;
    enum {PIXFMT_420, PIXFMT_422, PIXFMT_UYVY} pix_fmt = PIXFMT_422;
    enum {ENCODER_UNC, ENCODER_FFMPEG, ENCODER_FFMPEG_AV, ENCODER_MJPEG} encoder = ENCODER_UNC;

    // For checking of codec/capture compatibility
    enum {SD_422, SD_422_SHIFTED, SD_420, SD_420_SHIFTED, HD_422, ANY_422, ANY_UYVY} codec_input_format = SD_422;

    std::string filename_extension;
    bool mt_possible = false;
    switch (resolution)
    {
    // DV formats
    case DV25_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_420;
        codec_input_format = SD_420_SHIFTED;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DV25;
        ff_av_res = FF_ENCODER_RESOLUTION_DV25_MOV;
        filename_extension = ".dv";
        break;
    case DV50_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        codec_input_format = SD_422_SHIFTED;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DV50;
        ff_av_res = FF_ENCODER_RESOLUTION_DV50_MOV;
        filename_extension = ".dv";
        break;
    // DV HD format
    case DVCPROHD_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        codec_input_format = HD_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DV100_1080i50; // temporarily assuming not 720p
        ff_av_res = FF_ENCODER_RESOLUTION_DV100_MOV;
        filename_extension = ".dv";
        break;
    // MJPEG formats
    case MJPEG21_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        codec_input_format = SD_422;
        encoder = ENCODER_MJPEG;
        mjpeg_res = MJPEG_2_1;
        break;
    case MJPEG31_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        codec_input_format = SD_422;
        encoder = ENCODER_MJPEG;
        mjpeg_res = MJPEG_3_1;
        break;
    case MJPEG101_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_MJPEG;
        mjpeg_res = MJPEG_10_1;
        break;
    case MJPEG151S_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        codec_input_format = SD_422;
        encoder = ENCODER_MJPEG;
        mjpeg_res = MJPEG_15_1s;
        break;
    case MJPEG201_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        codec_input_format = SD_422;
        encoder = ENCODER_MJPEG;
        mjpeg_res = MJPEG_20_1;
        break;
    case MJPEG101M_MATERIAL_RESOLUTION:
        encoder = ENCODER_MJPEG;
        pix_fmt = PIXFMT_422;
        codec_input_format = SD_422;
        mjpeg_res = MJPEG_10_1m;
        break;
    // IMX formats
    case IMX30_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        codec_input_format = SD_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_IMX30;
        filename_extension = ".m2v";
        break;
    case IMX40_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        codec_input_format = SD_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_IMX40;
        filename_extension = ".m2v";
        break;
    case IMX50_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        codec_input_format = SD_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_IMX50;
        filename_extension = ".m2v";
        break;
    // DNxHD formats
    case DNX36p_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        codec_input_format = HD_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DNX36p;
        break;
    case DNX120p_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        codec_input_format = HD_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DNX120p;
        mt_possible = true;
        break;
    case DNX185p_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        codec_input_format = HD_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DNX185p;
        mt_possible = true;
        break;
    case DNX120i_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        codec_input_format = HD_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DNX120i;
        mt_possible = true;
        break;
    case DNX185i_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        codec_input_format = HD_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DNX185i;
        mt_possible = true;
        break;
    // H264
    case DMIH264_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_420;
        codec_input_format = ANY_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DMIH264;
        filename_extension = ".h264";
        break;
    // Browse formats
    case DVD_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_420;
        codec_input_format = SD_420;
        encoder = ENCODER_FFMPEG_AV;
        ff_av_res = FF_ENCODER_RESOLUTION_DVD;
        mxf = false;
        raw = false;
        filename_extension = ".mpg";
        break;
    case MPEG4_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_420;
        codec_input_format = SD_420;
        encoder = ENCODER_FFMPEG_AV;
        ff_av_res = FF_ENCODER_RESOLUTION_MPEG4_MOV;
        mxf = false;
        raw = false;
        filename_extension = ".mp4";
        break;
    case MP3_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        codec_input_format = ANY_422;
        mxf = false;
        raw = false;
        browse_audio = true;
        filename_extension = ".mp3";
        break;
    // Uncompressed
    case UNC_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_UYVY;
        codec_input_format = ANY_UYVY;
        encoder = ENCODER_UNC;
        filename_extension = ".yuv";
        break;
    default:
        break;
    }


    // Check which capture buffer has suitable format
    bool use_primary_video = true;
    switch (codec_input_format)
    {
    case SD_422:
        if (IngexShm::Instance()->PrimaryCaptureFormat() == Format422PlanarYUV
            && IngexShm::Instance()->PrimaryWidth() == 720)
        {
            use_primary_video = true;
        }
        else if (IngexShm::Instance()->SecondaryCaptureFormat() == Format422PlanarYUV
            && IngexShm::Instance()->SecondaryWidth() == 720)
        {
            use_primary_video = false;
        }
        else
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Incompatible capture format for %C\n"), resolution_name.c_str()));
            p_rec->NoteFailure();
        }
        break;

    case SD_422_SHIFTED:
        if (IngexShm::Instance()->PrimaryCaptureFormat() == Format422PlanarYUVShifted
            && IngexShm::Instance()->PrimaryWidth() == 720)
        {
            use_primary_video = true;
        }
        else if (IngexShm::Instance()->SecondaryCaptureFormat() == Format422PlanarYUVShifted
            && IngexShm::Instance()->SecondaryWidth() == 720)
        {
            use_primary_video = false;
        }
        else
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Incompatible capture format for %C\n"), resolution_name.c_str()));
            p_rec->NoteFailure();
        }
        break;

    case SD_420:
        if (IngexShm::Instance()->PrimaryCaptureFormat() == Format420PlanarYUV
            && IngexShm::Instance()->PrimaryWidth() == 720)
        {
            use_primary_video = true;
        }
        else if (IngexShm::Instance()->SecondaryCaptureFormat() == Format420PlanarYUV
            && IngexShm::Instance()->SecondaryWidth() == 720)
        {
            use_primary_video = false;
        }
        else
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Incompatible capture format for %C\n"), resolution_name.c_str()));
            p_rec->NoteFailure();
        }
        break;

    case SD_420_SHIFTED:
        if (IngexShm::Instance()->PrimaryCaptureFormat() == Format420PlanarYUVShifted
            && IngexShm::Instance()->PrimaryWidth() == 720)
        {
            use_primary_video = true;
        }
        else if (IngexShm::Instance()->SecondaryCaptureFormat() == Format420PlanarYUVShifted
            && IngexShm::Instance()->SecondaryWidth() == 720)
        {
            use_primary_video = false;
        }
        else
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Incompatible capture format for %C\n"), resolution_name.c_str()));
            p_rec->NoteFailure();
        }
        break;

    case HD_422:
        if (IngexShm::Instance()->PrimaryCaptureFormat() == Format422PlanarYUV
            && IngexShm::Instance()->PrimaryWidth() > 720)
        {
            use_primary_video = true;
        }
        else if (IngexShm::Instance()->SecondaryCaptureFormat() == Format422PlanarYUV
            && IngexShm::Instance()->SecondaryWidth() > 720)
        {
            use_primary_video = false;
        }
        else
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Incompatible capture format for %C\n"), resolution_name.c_str()));
            p_rec->NoteFailure();
        }
        break;

    case ANY_422:
        if (IngexShm::Instance()->PrimaryCaptureFormat() == Format422PlanarYUV)
        {
            use_primary_video = true;
        }
        else if (IngexShm::Instance()->SecondaryCaptureFormat() == Format422PlanarYUV)
        {
            use_primary_video = false;
        }
        else
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Incompatible capture format for %C\n"), resolution_name.c_str()));
            p_rec->NoteFailure();
        }
        break;

    case ANY_UYVY:
        if (IngexShm::Instance()->PrimaryCaptureFormat() == Format422UYVY)
        {
            use_primary_video = true;
        }
        else if (IngexShm::Instance()->SecondaryCaptureFormat() == Format422UYVY)
        {
            use_primary_video = false;
        }
        else
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Incompatible capture format for %C\n"), resolution_name.c_str()));
            p_rec->NoteFailure();
        }
        break;

    default:
        use_primary_video = true;
        break;
    }
    
    // Image parameters
    int WIDTH = 0;
    int HEIGHT = 0;
    if (use_primary_video)
    {
        WIDTH = IngexShm::Instance()->PrimaryWidth();
        HEIGHT = IngexShm::Instance()->PrimaryHeight();
    }
    else
    {
        WIDTH = IngexShm::Instance()->SecondaryWidth();
        HEIGHT = IngexShm::Instance()->SecondaryHeight();
    }
    //const bool INTERLACE = IngexShm::Instance()->Interlace();
    //const int SIZE_420 = WIDTH * HEIGHT * 3/2;
    //const int SIZE_422 = WIDTH * HEIGHT * 2;
    const int VIDEO_SIZE = (pix_fmt == PIXFMT_420 ? WIDTH * HEIGHT * 3/2 : WIDTH * HEIGHT * 2);
    // burnt-in timecode settings
    unsigned int tc_xoffset;
    unsigned int tc_yoffset;
    if (quad_video)
    {
        // centre
        tc_xoffset = WIDTH / 2 - 100;
        tc_yoffset = HEIGHT / 2 - 12;
    }
    else
    {
        // top centre
        tc_xoffset = WIDTH / 2 - 100;
        tc_yoffset = 30;
    }


    // Start with the SourcePackage being recording and 
    // assume only one being recorded in this thread.
    // (Not necessarily the case at the moment but it will be eventually.)
    prodauto::SourcePackage * rsp = 0;
    if (sc)
    {
        rsp = sc->getSourcePackage();
    }
    // So, rsp is the SourcePackage to be recorded.
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("SourceConfig: %C, SourcePackage: %C\n"), sc->name.c_str(), rsp->name.c_str()));
    if (rsp->tracks.size() < sc->trackConfigs.size())
    {
        ACE_DEBUG((LM_WARNING, ACE_TEXT("Warning: SourcePackage has fewer tracks than SourcConfig!\n")));
    }

    // Override file name extension for non-raw formats
    switch (file_format)
    {
    case MXF_FILE_FORMAT_TYPE:
        filename_extension = ".mxf";
        break;
    case MOV_FILE_FORMAT_TYPE:
        encoder = ENCODER_FFMPEG_AV;
        filename_extension = ".mov";
        raw = false;
        break;
    case RAW_FILE_FORMAT_TYPE:
    default:
        break;
    }

    // (video) filename
    std::ostringstream filename_prefix;
    std::ostringstream creating_filename_prefix;
    std::ostringstream creating_dir;
    std::ostringstream destination_dir;
    filename_prefix << p_opt->dir << PATH_SEPARATOR << p_opt->file_ident;
    creating_dir << p_opt->dir << PATH_SEPARATOR << CREATING_SUBDIR;
    creating_filename_prefix << creating_dir.str() << PATH_SEPARATOR << p_opt->file_ident;
    destination_dir << p_opt->dir;

    // Get project name
    prodauto::ProjectName project_name;
    std::vector<prodauto::UserComment> user_comments;
    p_rec->GetMetadata(project_name, user_comments); // user comments empty at the moment

    // Material package name (clip name in Avid)
    std::string clip_name;
    switch (4)
    {
    case 1:
        // tape name
        clip_name = rsp->name;
        break;
    case 2:
        // tape name plus timecode
        clip_name = rsp->name;
        clip_name += '.';
        clip_name += start_timecode.TextNoSeparators();
        break;
    case 3:
        // source (e.g. camera) name
        clip_name = sc->name;
        break;
    case 4:
        // source name plus timecode
        clip_name = sc->name;
        clip_name += '.';
        clip_name += start_timecode.TextNoSeparators();
        break;
    default:
        // long-winded name
        clip_name = p_opt->file_ident;
        break;
    }


    // Create package data to go into database.
    
    prodauto::RecorderPackageCreator *package_creator;
    
    switch (file_format)
    {
    case MXF_FILE_FORMAT_TYPE:
        package_creator = new prodauto::OPAtomPackageCreator(true);
        break;
    case MOV_FILE_FORMAT_TYPE:
    case MPG_FILE_FORMAT_TYPE:
        package_creator = new prodauto::OP1APackageCreator(true);
        break;
    case RAW_FILE_FORMAT_TYPE:
    default:
        package_creator = new prodauto::OPAtomPackageCreator(true);
        break;
    }
    
    package_creator->SetFileFormat(file_format);
    package_creator->SetStartPosition(start_position);
    package_creator->SetUserComments(user_comments);
    package_creator->SetProjectName(project_name);
    if (raw)
    {
        package_creator->SetFileLocationPrefix(filename_prefix.str());
        package_creator->SetVideoFileLocationSuffix(filename_extension);
        package_creator->SetAudioFileLocationSuffix(".wav");
    }
    else
    {
        package_creator->SetFileLocationPrefix(creating_filename_prefix.str());
    }
    package_creator->SetClipName(clip_name);
    package_creator->SetImageAspectRatio(image_aspect);
    package_creator->SetVideoResolutionID(resolution);
    package_creator->SetAudioQuantBits(mxf_audio_bits);
    
    package_creator->CreatePackageGroup(sc, track_enables, prodauto::Database::getInstance()->getUMIDGenOffset());
    

    // Count audio tracks to be recorded
    unsigned int num_audio_tracks = 0;
    
    // For each track in mp, we keep a record of corresponding
    // hardware track.
    std::vector<HardwareTrack> mp_hw_trks;

    // For each track in mp, we keep a record of corresponding
    // SourceTrackConfig database id
    std::vector<long> mp_stc_dbids;

    for (size_t i = 0; i < sc->trackConfigs.size(); ++i)
    {
        if (track_enables[i])
        {
            // Get HardwareTrack based on SourceTrackConfig database id;
            prodauto::SourceTrackConfig * stc = sc->getTrackConfig(i + 1);
            long db_id = stc->getDatabaseID();
            /*
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C thread %d i %d stc->db_id %d stc->name %C\n"),
                src_name.c_str(), p_opt->index, i, db_id, stc->name.c_str()));
            */
            mp_stc_dbids.push_back(db_id);
            mp_hw_trks.push_back(p_impl->TrackHwMap(db_id));
            
            if (stc->dataDef == SOUND_DATA_DEFINITION)
            {
                num_audio_tracks++;
            }
        }
    }

    for (std::vector<HardwareTrack>::const_iterator
        it = mp_hw_trks.begin(); it != mp_hw_trks.end(); ++it)
    {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C thread %d mp_hw_trks: %u %u\n"),
            src_name.c_str(), p_opt->index, it->channel, it->track));
    }


    // Directories
    const std::string & mxf_subdir_creating = settings->mxf_subdir_creating;
    const std::string & mxf_subdir_failures = settings->mxf_subdir_failures;

    // Raw files
    std::vector<FILE *> fp_raw;
    for (unsigned int i = 0; i < package_creator->GetMaterialPackage()->tracks.size(); ++i)
    {
        FILE * fp = NULL;
        prodauto::Track * mp_trk = package_creator->GetMaterialPackage()->tracks[i];

        if (raw)
        {
            // Open file
            std::string fname = package_creator->GetFileLocation(mp_trk->id);
            if (NULL == (fp = fopen(fname.c_str(), "wb")))
            {
                ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not open %C\n"), fname.c_str()));
            }
            // Write wav header if appropriate
            if (fp && mp_trk->dataDef == SOUND_DATA_DEFINITION)
            {
                writeWavHeader(fp, raw_audio_bits, 1);  // mono
            }
        }

        fp_raw.push_back(fp);
    }

    // Initialisation for browse audio
    ffmpeg_encoder_t * ffmpeg_audio_encoder = 0;
    FILE * fp_audio_browse = NULL;
    if (browse_audio)
    {
        std::ostringstream fname;
        fname << p_opt->dir << PATH_SEPARATOR << p_opt->file_ident
            << (browse_mp3 ? ".mp3" : ".wav");

        if (NULL == (fp_audio_browse = fopen(fname.str().c_str(), "wb")))
        {
            browse_audio = false;
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not open %s\n"), fname.str().c_str()));
        }
        else if (browse_mp3)
        {
            // Prevent "insufficient thread locking around avcodec_open/close()"
            ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

            // Initialise ffmpeg audio encoder
            ffmpeg_audio_encoder = ffmpeg_encoder_init(FF_ENCODER_RESOLUTION_MP3, 0);
            if (!ffmpeg_audio_encoder)
            {
                ACE_DEBUG((LM_ERROR, ACE_TEXT("%C: ffmpeg audio encoder init failed.\n"), src_name.c_str()));
            }
        }
        else
        {
            // Write WAV header
            writeWavHeader(fp_audio_browse, browse_audio_bits, 2);
        }
    }

    
    // Initialise ffmpeg encoder
    ffmpeg_encoder_t * ffmpeg_encoder = 0;
    MtEncoder * mt_encoder = 0;
    //bool mt_encode = MT_ENABLE && mt_possible;
    if (ENCODER_FFMPEG == encoder)
    {
        if (MT_ENABLE && mt_possible)
        {
            mt_encoder = new MtEncoder(&avcodec_mutex);
            mt_encoder->Init(ff_res, ffmpeg_threads);
        }
        else
        {
            // Prevent "insufficient thread locking around avcodec_open/close()"
            ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

            ffmpeg_encoder = ffmpeg_encoder_init(ff_res, ffmpeg_threads);
            if (!ffmpeg_encoder)
            {
                ACE_DEBUG((LM_ERROR, ACE_TEXT("%C: ffmpeg encoder init failed.\n"), src_name.c_str()));
            }
        }
    }
    
    // Initialise MJPEG encoder
    mjpeg_compress_threaded_t * mj_encoder_threaded = 0;
    mjpeg_compress_t * mj_encoder = 0;
    if (ENCODER_MJPEG == encoder)
    {
        if (THREADED_MJPEG)
        {
            mjpeg_compress_init_threaded(mjpeg_res, WIDTH, HEIGHT, &mj_encoder_threaded);
        }
        else
        {
            mjpeg_compress_init(mjpeg_res, WIDTH, HEIGHT, &mj_encoder);
        }
    }

    // Initialisation for av files
    // ffmpeg av encoder
    ffmpeg_encoder_av_t * enc_av = 0;
    unsigned int ff_av_num_audio_streams = 0;
    unsigned int ff_av_audio_channels_per_stream = 0;
    if (ENCODER_FFMPEG_AV == encoder)
    {
        // Prevent simultaneous calls to init function causing
        // "insufficient thread locking around avcodec_open/close()"
        // and also "format not registered".
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

        switch (ff_av_res)
        {
        case FF_ENCODER_RESOLUTION_DV25_MOV:
        case FF_ENCODER_RESOLUTION_DV50_MOV:
        case FF_ENCODER_RESOLUTION_DV100_MOV:
            ff_av_num_audio_streams = num_audio_tracks;
            ff_av_audio_channels_per_stream = 1;
            break;
        default:
            ff_av_num_audio_streams = 1;
            ff_av_audio_channels_per_stream = 2;
            break;
        }
        enc_av = ffmpeg_encoder_av_init(package_creator->GetFileLocation().c_str(), ff_av_res,
            wide_aspect, start_tc, ffmpeg_threads, ff_av_num_audio_streams, ff_av_audio_channels_per_stream);
        if (!enc_av)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C: ffmpeg_encoder_av_init() failed\n"), src_name.c_str()));
        }
    }

    // Initialisation for MXF files
    std::auto_ptr<prodauto::MXFWriter> writer;
    if (mxf)
    {
        std::ostringstream destination_path;
        std::ostringstream creating_path;
        std::ostringstream failures_path;
        destination_path << p_opt->dir;
        creating_path << p_opt->dir << PATH_SEPARATOR << mxf_subdir_creating;
        failures_path << p_opt->dir << PATH_SEPARATOR << mxf_subdir_failures;

        try
        {
            prodauto::MXFOPAtomWriter * p = new prodauto::MXFOPAtomWriter();
            p->SetCreatingDirectory(creating_path.str());
            p->SetDestinationDirectory(destination_path.str());
            p->SetFailureDirectory(failures_path.str());
            
            p->PrepareToWrite(package_creator, false);

            writer.reset(p);
        }
        catch (const prodauto::DBException & dbe)
        {
            LOG_RECORD_ERROR("Database exception: %C\n", dbe.getMessage().c_str());
            p_rec->NoteFailure();
            mxf = false;
        }
        catch (const prodauto::MXFWriterException & e)
        {
            LOG_RECORD_ERROR("MXFWriterException: %C\n", e.getMessage().c_str());
            p_rec->NoteFailure();
            mxf = false;
        }
    }

    // Store filenames for each track but only for first encoding (index == 0)
    if (p_opt->index == 0)
    {
        for (unsigned int i = 0; i < package_creator->GetMaterialPackage()->tracks.size(); ++i)
        {
            prodauto::Track * mp_trk = package_creator->GetMaterialPackage()->tracks[i];

            std::string pathname = package_creator->GetFileLocation(mp_trk->id);
            // That includes creating path so we modify to destination path
            std::string filename = strip_path(pathname);
            pathname = p_opt->dir;
            pathname += PATH_SEPARATOR;
            pathname += filename;

            // Now need to find out which member of mTracks we are dealing with
            unsigned int track_index = p_impl->TrackIndexMap(mp_stc_dbids[i]);
            p_rec->mFileNames[track_index] = pathname;

            ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C thread %d mp_track %d filename \"%C\" track_index %d\n"),
                src_name.c_str(), p_opt->index, i, pathname.c_str(), track_index));
        }
    }

    // Initialisation for timecode overlay
    tc_overlay_t * tco = 0;
    uint8_t * tc_overlay_buffer = 0;
    if (bitc)
    {
        tco = tc_overlay_init();
        tc_overlay_buffer = new uint8_t[VIDEO_SIZE];
    }
    if (tco)
    {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("TC overlay initialised\n")));
    }

    AudioMixer mixer;
    if (0 && MP3_MATERIAL_RESOLUTION == resolution)
    {
        // special for The Bottom Line
        mixer.SetMix(AudioMixer::CH12L3R);
    }
    else
    {
        mixer.SetMix(AudioMixer::CH12);
        //mixer.SetMix(AudioMixer::COMMENTARY3);
        //mixer.SetMix(AudioMixer::COMMENTARY4);
        //mixer.SetMix(AudioMixer::ALL);
    }

    // Buffer to receive quad-split video

    YUV_frame quad_frame;
    uint8_t * quad_workspace = 0;
    formats format = I420;
    if (quad_video)
    {
        switch (pix_fmt)
        {
        case PIXFMT_422:
            format = YV16;
            break;
        case PIXFMT_420:
        default:
            format = I420;
            break;
        }
        alloc_YUV_frame(&quad_frame, WIDTH, HEIGHT, format);
        clear_YUV_frame(&quad_frame);

        quad_workspace = new uint8_t[WIDTH * 3];
    }

    // Buffer to hold frame data during encoding
    EncodeFrameBuffer encode_frame_buffer;
    

    // ***************************************************
    // This is the loop which records audio/video frames
    // ***************************************************

    // Set lastcoded to make the record start at the target frame

    int lastcoded[MAX_CHANNELS];
    for (unsigned int i = 0; i < channels_in_use.size(); ++i)
    {
        unsigned int ch = channels_in_use[i];
        lastcoded[ch] = start_frame[ch] - 1;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Channel %d, start_frame=%d\n"), ch, start_frame[ch]));
    }

    // For the index that is stored in the EncodeFrame, we use the frame number
    // from the first channel.  This is what we keep track of in last_saved.
    int last_saved = lastcoded[channel_i];

    // Initialise last_tc which will be used to check for timecode discontinuities.
    // Timecode value from first track (usually video).
    HardwareTrack tc_hw = p_impl->TrackHwMap(mp_stc_dbids[0]);
    framecount_t last_tc = IngexShm::Instance()->Timecode(tc_hw.channel, lastcoded[tc_hw.channel]);
    //framecount_t initial_tc = last_tc + 1;

    // Update Record info
    IngexShm::Instance()->InfoSetRecording(channel_i, p_opt->index, quad_video, true);
    IngexShm::Instance()->InfoSetDesc(channel_i, p_opt->index, quad_video,
                "%s%s %s%s%s%s%s",
                resolution_name.c_str(),
                quad_video ? "(quad)" : "",
                raw ? "RAW" : "MXF",
                /*enable_audio12 ? " A12" : "",
                enable_audio34 ? " A34" : "",
                enable_audio56 ? " A56" : "",
                enable_audio78 ? " A78" : ""*/
                "", "", "", ""
                );

    p_opt->FramesWritten(0);
    p_opt->FramesDropped(0);
    bool finished_record = false;
    while (!finished_record)
    {
        // We read lastframe counter from shared memory in a thread safe manner.
        // Capture daemon updates lastframe after the frame has been written
        // to shared memory.

        // If we don't have new frames to code, sleep.
        const int sleep_ms = 20;
        for (std::vector<unsigned int>::const_iterator
            it = channels_in_use.begin(); it != channels_in_use.end(); ++it)
        {
            while (!finished_record && IngexShm::Instance()->LastFrame(*it) == lastcoded[*it])
            {
                //ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C sleeping %d ms for channel %d\n"), src_name.c_str(), sleep_ms, *it));
                ACE_OS::sleep(ACE_Time_Value(0, sleep_ms * 1000));

                // Check heartbeat
                struct timeval now;
                gettimeofday(&now, NULL);
                struct timeval heartbeat;
                IngexShm::Instance()->GetHeartbeat(&heartbeat);
                int64_t diff = tv_diff_microsecs(&heartbeat, &now);
                int64_t diff_secs = diff / 1000000;
                if (diff_secs > 5)
                {
                    ACE_DEBUG((LM_ERROR, ACE_TEXT("%C thread %d Shared memory lost - stopping recording!\n"),
                        src_name.c_str(),  p_opt->index));
                    finished_record = true;
                }
            }
        }

        // Get number of frames to code from channel with fewest available
        int frames_to_code = 0;
        for (unsigned int i = 0; i < channels_in_use.size(); ++i)
        {
            unsigned int ch = channels_in_use[i];
            int ftc = IngexShm::Instance()->LastFrame(ch) - lastcoded[ch];
            if (i == 0 || ftc < frames_to_code)
            {
                frames_to_code = ftc;
            }
        }

        //ACE_DEBUG((LM_DEBUG, ACE_TEXT("frames_to_code=%d\n"), frames_to_code));

        IngexShm::Instance()->InfoSetBacklog(channel_i, p_opt->index, quad_video, frames_to_code);

        // Now we have some frames to code, check the backlog.
        int allowed_backlog = ring_length - 3;
        unsigned int warn_backlog = allowed_backlog / 2;
        if (frames_to_code > allowed_backlog / 2)
        {
            // Warn about backlog
            ACE_DEBUG((LM_WARNING, ACE_TEXT("%C thread %d res=%d frames waiting = %d, max = %d\n"),
                src_name.c_str(), p_opt->index, resolution, frames_to_code, allowed_backlog));
        }
        if (frames_to_code > allowed_backlog)
        {
            // Dump frames
            int drop = frames_to_code - allowed_backlog;
            frames_to_code -= drop;
            for (unsigned int i = 0; i < channels_in_use.size(); ++i)
            {
                unsigned int ch = channels_in_use[i];
                lastcoded[ch] += drop;
                last_saved += drop; // not exactly right but better than not incrememting it
            }
            p_opt->IncFramesDropped(drop);
            p_rec->NoteDroppedFrames();
            IngexShm::Instance()->InfoSetFramesDropped(channel_i, p_opt->index, quad_video, p_opt->FramesDropped());
            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C dropped %d frames!\n"),
                src_name.c_str(), drop));
        }

#if 0
        // Go round loop only once before re-checking backlog
        frames_to_code = 1;
#endif

        for (int fi = 0; fi < frames_to_code && !finished_record; ++fi)
        {
            int frame[MAX_CHANNELS];
            for (unsigned int i = 0; i < channels_in_use.size(); ++i)
            {
                unsigned int ch = channels_in_use[i];
                frame[ch] = lastcoded[ch] + 1;
                //ACE_DEBUG((LM_DEBUG, ACE_TEXT("Channel %d, Frame %d\n"), ch, frame[ch]));
            }

            // Set up some audio pointers.
            // NB. Assuming all from same hardware channel.

            // Pointer to audio12
            int32_t * p_audio12 = IngexShm::Instance()->pAudio12(channel_i, frame[channel_i]);

            // Pointer to audio34
            int32_t * p_audio34 = IngexShm::Instance()->pAudio34(channel_i, frame[channel_i]);

            // Pointer to audio56
            int32_t * p_audio56 = IngexShm::Instance()->pAudio56(channel_i, frame[channel_i]);

            // Pointer to audio78
            int32_t * p_audio78 = IngexShm::Instance()->pAudio78(channel_i, frame[channel_i]);

            // Need to set this for each captured frame because the number
            // varies in NTSC.
            int audio_samples_per_frame = 1920;

            // Until we have mono audio buffers in IngexShm, demultiplex the audio.
            // Buffers for audio de-interleaving
            int16_t a[8][audio_samples_per_frame];
            deinterleave_32to16(p_audio12, a[0], a[1], audio_samples_per_frame);
            deinterleave_32to16(p_audio34, a[2], a[3], audio_samples_per_frame);
            if (IngexShm::Instance()->AudioTracksPerChannel() >= 8)
            {
                deinterleave_32to16(p_audio56, a[4], a[5], audio_samples_per_frame);
                deinterleave_32to16(p_audio78, a[6], a[7], audio_samples_per_frame);
            }

            // Set up all the input pointers as tracks of an EncodeFrame
            int frame_index = frame[channel_i];
            EncodeFrame & ef = encode_frame_buffer.Frame(frame_index);
            //std::vector<void *> p_input;
            void * p_inp_video = 0;
            int * p_frame_number = 0;
            //ACE_DEBUG((LM_DEBUG, ACE_TEXT("package_creator->GetMaterialPackage()->tracks.size() = %u\n"), package_creator->GetMaterialPackage()->tracks.size()));
            for (unsigned int i = 0; i < package_creator->GetMaterialPackage()->tracks.size(); ++i)
            {
                void * p = 0;
                size_t size = 0;
                prodauto::Track * mp_trk = package_creator->GetMaterialPackage()->tracks[i];
#if 1
                // Just to be slightly more efficient, avoid
                // accessing the TrackHwMap in this loop.
                HardwareTrack hw = mp_hw_trks[i];
#else
                HardwareTrack hw = p_impl->TrackHwMap(mp_stc_dbids[i]);
#endif

                // Pointer to frame index (of first track's channel)
                NexusFrameData * nfd = IngexShm::Instance()->pFrameData(channel_i, frame[channel_i]);
                p_frame_number = &(nfd->frame_number);

                if (mp_trk->dataDef == PICTURE_DATA_DEFINITION)
                {
                    if (use_primary_video)
                    {
                        p = IngexShm::Instance()->pVideoPri(hw.channel, frame[hw.channel]);
                    }
                    else
                    {
                        p = IngexShm::Instance()->pVideoSec(hw.channel, frame[hw.channel]);
                    }
                    p_inp_video = p;
                    size = VIDEO_SIZE;
                }
                else
                {
                    //p = IngexShm::Instance()->pAudio(hw.channel, hw.track, frames[hw.channel]);
                    // Mono audio buffers not yet available
                    p = a[hw.track - 1];
                    size = audio_samples_per_frame * 2;
                }
                
                /*
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C thread %d Track %d data %@\n"), src_name.c_str(), p_opt->index, i, p));
                */
                ef.Track(i).Init(p, size, false, false, false, *p_frame_number, p_frame_number);
            }

            // Timecode value from first track (usually video)
            framecount_t tc_i = IngexShm::Instance()->Timecode(tc_hw.channel, frame[tc_hw.channel]);

            // Check for timecode irregularities
            framecount_t tc_diff = tc_i - (last_tc + 1); // difference from expected value
            //if (tc_diff != 0)
            if (tc_diff > 1 || tc_diff < -1)
            {
                ACE_DEBUG((LM_ERROR, ACE_TEXT("%C thread %d Timecode discontinuity: %d frames missing at frame=%d tc=%C\n"),
                    src_name.c_str(),
                    p_opt->index,
                    tc_diff,
                    frame[tc_hw.channel],
                    Timecode(tc_i, fps, df).Text()));
            }

            // Make quad split
            if (quad_video)
            {
                // Setup input frame as YUV_frame and
                // set source for encoding to the quad frame.
                YUV_frame in_frame;
                uint8_t * p_quad_inp_video[4];

                for (std::vector<unsigned int>::const_iterator
                    it = channels_in_use.begin(); it != channels_in_use.end(); ++it)
                {
                    unsigned int chan = *it;
                    if (use_primary_video)
                    {
                        p_quad_inp_video[chan] = IngexShm::Instance()->pVideoPri(chan, frame[chan]);
                    }
                    else
                    {
                        p_quad_inp_video[chan] = IngexShm::Instance()->pVideoSec(chan, frame[chan]);
                    }
                }

                // top left - channel0
                if (p_rec->mChannelEnable[0])
                {
                    YUV_frame_from_buffer(&in_frame, p_quad_inp_video[0],
                        WIDTH, HEIGHT, format);

                    quarter_frame(&in_frame, &quad_frame,
                                    0,      // x offset
                                    0,      // y offset
                                    1,      // interlace?
                                    1,      // horiz filter?
                                    1,      // vert filter?
                                    quad_workspace);
                }

                // top right - channel1
                if (p_rec->mChannelEnable[1])
                {
                    YUV_frame_from_buffer(&in_frame, p_quad_inp_video[1],
                        WIDTH, HEIGHT, format);

                    quarter_frame(&in_frame, &quad_frame,
                                    WIDTH/2, 0,
                                    1, 1, 1,
                                    quad_workspace);
                }

                // bottom left - channel2
                if (p_rec->mChannelEnable[2])
                {
                    YUV_frame_from_buffer(&in_frame, p_quad_inp_video[2],
                        WIDTH, HEIGHT, format);

                    quarter_frame(&in_frame, &quad_frame,
                                    0, HEIGHT/2,
                                    1, 1, 1,
                                    quad_workspace);
                }

                // bottom right - channel3
                if (p_rec->mChannelEnable[3])
                {
                    YUV_frame_from_buffer(&in_frame, p_quad_inp_video[3],
                        WIDTH, HEIGHT, format);

                    quarter_frame(&in_frame, &quad_frame,
                                    WIDTH/2, HEIGHT/2,
                                    1, 1, 1,
                                    quad_workspace);
                }

                // Source for encoding is now the quad buffer
                p_inp_video = quad_frame.Y.buff;
                ef.Track(0).Init(p_inp_video, VIDEO_SIZE, false, false, false, 0, 0);
            }

            // Add timecode overlay
            if (bitc && p_inp_video)
            {
                tc_overlay_setup(tco, tc_i);
                if (pix_fmt == PIXFMT_420)
                {
                    // 420
                    // Need to copy video as can't overwrite shared memory
                    memcpy(tc_overlay_buffer, p_inp_video, VIDEO_SIZE);
                    uint8_t * p_y = tc_overlay_buffer;
                    uint8_t * p_u = p_y + WIDTH * HEIGHT;
                    uint8_t * p_v = p_u + WIDTH * HEIGHT / 4;
                    tc_overlay_apply(tco, p_y, p_u, p_v, WIDTH, HEIGHT, tc_xoffset, tc_yoffset, TC420);
                }
                else
                {
                    // 422
                    // Need to copy video as can't overwrite shared memory
                    memcpy(tc_overlay_buffer, p_inp_video, VIDEO_SIZE);
                    uint8_t * p_y = tc_overlay_buffer;
                    uint8_t * p_u = p_y + WIDTH * HEIGHT;
                    uint8_t * p_v = p_u + WIDTH * HEIGHT / 2;
                    tc_overlay_apply(tco, p_y, p_u, p_v, WIDTH, HEIGHT, tc_xoffset, tc_yoffset, TC422);
                }
                // Source for encoding is now the timecode overlay buffer
                p_inp_video = tc_overlay_buffer;
                ef.Track(0).Init(p_inp_video, VIDEO_SIZE, false, false, false, 0, 0);
            }
 
            // Mix audio for browse version
            int16_t mixed_audio[audio_samples_per_frame * 2];    // for stereo pair output

// Needs update for mono 32-bit audio buffers when we have them
            if (browse_audio || ENCODER_FFMPEG_AV == encoder)
            {
                mixer.Mix(p_audio12, p_audio34, mixed_audio, audio_samples_per_frame);
            }

            //
            // We now have everything ready for encoding.


            // encode to av formats
            if (ENCODER_FFMPEG_AV == encoder && enc_av && p_inp_video)
            {
                int result = 0;
                if (ff_av_num_audio_streams == 1)
                {
                    result |= ffmpeg_encoder_av_encode_video(enc_av, (uint8_t *)p_inp_video);
                    result |= ffmpeg_encoder_av_encode_audio(enc_av, 0, audio_samples_per_frame, mixed_audio);
                }
                else if (ff_av_audio_channels_per_stream == 1)
                {
                    unsigned int astream_i = 0;
                    for (unsigned int i = 0; i < package_creator->GetMaterialPackage()->tracks.size() && astream_i < ff_av_num_audio_streams; ++i)
                    {
                        prodauto::Track * mp_trk = package_creator->GetMaterialPackage()->tracks[i];
                        if (PICTURE_DATA_DEFINITION == mp_trk->dataDef)
                        {
                            result |= ffmpeg_encoder_av_encode_video(enc_av, (uint8_t *) ef.Track(i).Data());
                        }
                        else if (SOUND_DATA_DEFINITION == mp_trk->dataDef)
                        {
                            result |= ffmpeg_encoder_av_encode_audio(enc_av, astream_i++, audio_samples_per_frame, (short *) ef.Track(i).Data());
                        }
                    }
                }
                else
                {
                    ACE_DEBUG((LM_ERROR, ACE_TEXT("AV audio streams mis-configured\n")));
                    result = 1;
                }

                if (result != 0)
                {
                    ACE_DEBUG((LM_ERROR, ACE_TEXT("AV encode failed\n")));
                }
            }


            //ACE_DEBUG((LM_DEBUG, ACE_TEXT("p_inp_video = %@\n"), p_inp_video));
            uint8_t * p_enc_video = 0;
            int size_enc_video = 0;

            // Encode with FFMPEG (IMX, DV, H264)
            if (ENCODER_FFMPEG == encoder && p_inp_video)
            {
                if (mt_encoder)
                {
                    mt_encoder->Encode(ef.Track(0));
                }
                else
                {
                    size_enc_video = ffmpeg_encoder_encode(ffmpeg_encoder, (uint8_t *)ef.Track(0).Data(), &p_enc_video);
                    ef.Track(0).Init(p_enc_video, size_enc_video, false, false, true, 0, 0);
                }
            }

            // Encode MJPEG
            if (ENCODER_MJPEG == encoder && p_inp_video)
            {
                uint8_t * y = (uint8_t *)p_inp_video;
                uint8_t * u = y + WIDTH * HEIGHT;
                uint8_t * v = u + WIDTH * HEIGHT / 2;
                if (THREADED_MJPEG)
                {
                    size_enc_video = mjpeg_compress_frame_yuv_threaded(mj_encoder_threaded, y, u, v, WIDTH, WIDTH/2, WIDTH/2, &p_enc_video);
                }
                else
                {
                    size_enc_video = mjpeg_compress_frame_yuv(mj_encoder, y, u, v, WIDTH, WIDTH/2, WIDTH/2, &p_enc_video);
                }
                ef.Track(0).Init(p_enc_video, size_enc_video, false, false, true, 0, 0);
            }

            // "Encode" uncompressed video and PCM audio, i.e. mark it as coded.
            if (ENCODER_FFMPEG_AV != encoder)
            {
                for (unsigned int i = 0; i < package_creator->GetMaterialPackage()->tracks.size(); ++i)
                {
                    prodauto::Track * mp_trk = package_creator->GetMaterialPackage()->tracks[i];

                    if ((PICTURE_DATA_DEFINITION == mp_trk->dataDef && ENCODER_UNC == encoder)
                        || (SOUND_DATA_DEFINITION == mp_trk->dataDef))
                    {
                        ef.Track(i).Coded(true);
                    }
                }
            }

            // Encode and write browse audio
            if (browse_audio)
            {
                if (browse_mp3)
                {
                    uint8_t * p_enc_audio = 0;
                    int size_enc_audio = ffmpeg_encoder_encode_audio(ffmpeg_audio_encoder, audio_samples_per_frame, mixed_audio, &p_enc_audio);
                    if (size_enc_audio > 0)
                    {
                        size_t n = fwrite(p_enc_audio, size_enc_audio, 1, fp_audio_browse);
                        if (n == 0)
                        {
                            ACE_DEBUG((LM_ERROR, ACE_TEXT("Browse audio file write error!\n")));
                            fclose(fp_audio_browse);
                            fp_audio_browse = NULL;
                        }
                    }
                }
                else
                {
                    write_audio(fp_audio_browse, (uint8_t *)mixed_audio, audio_samples_per_frame * 2, 16, browse_audio_bits);
                }
            }
            
            // Update lastcoded.
            // (It really means last queued for coding.)
            for (unsigned int i = 0; i < channels_in_use.size(); ++i)
            {
                unsigned int ch = channels_in_use[i];
                lastcoded[ch] = frame[ch];
            }
            last_tc = tc_i;

            // So now we have done or queued the encoding.
            // Next the writing to disc.

            // If we used the av encoder, the frames have already been written.
            if (ENCODER_FFMPEG_AV == encoder)
            {
                ++last_saved;
                encode_frame_buffer.EraseFrame(last_saved);

                p_opt->IncFramesWritten();

                // Check if we have finished
                framecount_t target = p_rec->TargetDuration();
                framecount_t written = p_opt->FramesWritten();
                framecount_t dropped = p_opt->FramesDropped();
                framecount_t total = written + dropped;
                if (target > 0 && total >= target)
                {
                    finished_record = true;
                    // NB. need to check and report on out-frame timecode here.
                    ACE_DEBUG((LM_INFO, ACE_TEXT("  %C index %d duration %d reached (total=%d written=%d dropped=%d)\n"),
                        src_name.c_str(), p_opt->index, target, total, written, dropped));
                }
            }

            // Otherwise, look for coded frames in the buffer.
            if (ENCODER_FFMPEG_AV != encoder)
            {
                // Check buffer occupancy
                size_t frames_in_buffer = encode_frame_buffer.QueueSize();
                if (frames_in_buffer > warn_backlog)
                {
                    ACE_DEBUG((LM_WARNING, ACE_TEXT("%C thread %d EncodeFrameBuffer contains %u frames (%u coded)\n"),
                        src_name.c_str(), p_opt->index, frames_in_buffer, encode_frame_buffer.CodedSize()));
                }
                else
                {
                    ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C thread %d EncodeFrameBuffer contains %u frames (%u coded)\n"),
                        src_name.c_str(), p_opt->index, frames_in_buffer, encode_frame_buffer.CodedSize()));
                }

                ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C thread %d Looking for frames starting from %d\n"),
                    src_name.c_str(), p_opt->index, last_saved + 1));

                while (!finished_record && encode_frame_buffer.Frame(last_saved + 1).IsCoded())
                {
                    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Have coded frame %d\n"), last_saved + 1));
                    EncodeFrame & ef = encode_frame_buffer.Frame(last_saved + 1);

                    for (unsigned int i = 0; i < package_creator->GetMaterialPackage()->tracks.size(); ++i)
                    {
                        prodauto::Track * mp_trk = package_creator->GetMaterialPackage()->tracks[i];
                        EncodeFrameTrack & eft = ef.Track(i);

                        if (eft.Error())
                        {
                            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C thread %d Coded frame %d track %d has error!\n"),
                                src_name.c_str(), p_opt->index, last_saved + 1, i));
                            p_rec->NoteFailure();
                        }

                        if (mxf && !DEBUG_NOWRITE)
                        {
                            uint32_t num_samples = (SOUND_DATA_DEFINITION == mp_trk->dataDef ? audio_samples_per_frame : 1);
                            /*
                            ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C thread %d WriteSamples track %u, track id %u, num_samples %u, data %@, size %u\n"),
                                src_name.c_str(), p_opt->index,
                                i, mp_trk->id, num_samples, eft.Data(), eft.Size()));
                            */
                            try
                            {
                                writer->WriteSamples(mp_trk->id, num_samples, (uint8_t *)eft.Data(), eft.Size());
                            }
                            catch (const prodauto::MXFWriterException & e)
                            {
                                LOG_RECORD_ERROR("MXFWriterException: %C\n", e.getMessage().c_str());
                                p_rec->NoteFailure();
                            }
                        }

                        if (raw && !DEBUG_NOWRITE)
                        {
                            if (fp_raw.size() > i && fp_raw[i] != NULL)
                            {
                                FILE * & fp = fp_raw[i];
                                if (PICTURE_DATA_DEFINITION == mp_trk->dataDef)
                                {
                                    // Video
                                    size_t n = fwrite(eft.Data(), eft.Size(), 1, fp);
                                    if (n == 0)
                                    {
                                        ACE_DEBUG((LM_ERROR, ACE_TEXT("Raw video file write error!\n")));
                                        fclose(fp);
                                        fp = NULL;
                                    }
                                }
                                else if (mp_trk->dataDef == SOUND_DATA_DEFINITION)
                                {
                                    // Audio
                                    //write_audio(fp, (uint8_t *)p_input[i], audio_samples_per_frame, 32, raw_audio_bits);
                                    write_audio(fp, (uint8_t *)eft.Data(), audio_samples_per_frame, 16, raw_audio_bits);
                                }
                            }
                        }
                    }

                    ++last_saved;
                    encode_frame_buffer.EraseFrame(last_saved);

                    p_opt->IncFramesWritten();

                    // Check if we have finished
                    framecount_t target = p_rec->TargetDuration();
                    framecount_t written = p_opt->FramesWritten();
                    framecount_t dropped = p_opt->FramesDropped();
                    framecount_t total = written + dropped;
                    if (target > 0 && total >= target)
                    {
                        finished_record = true;
                        // NB. need to check and report on out-frame timecode here.
                        ACE_DEBUG((LM_INFO, ACE_TEXT("  %C index %d duration %d reached (total=%d written=%d dropped=%d)\n"),
                            src_name.c_str(), p_opt->index, target, total, written, dropped));
                    }
                }
            }


            IngexShm::Instance()->InfoSetFramesWritten(channel_i, p_opt->index, quad_video, p_opt->FramesWritten());


        } // Save all frames which have not been saved

    }
    // ************************
    // End of main record loop
    // ************************


    // Update and close raw files
    for (unsigned int i = 0; i < package_creator->GetMaterialPackage()->tracks.size(); ++i)
    {
        FILE * fp = fp_raw[i];
        if (fp != NULL)
        {
            prodauto::Track * mp_trk = package_creator->GetMaterialPackage()->tracks[i];
            if (mp_trk->dataDef == SOUND_DATA_DEFINITION)
            {
                update_WAV_header(fp);
                // This also closes the file
            }
            else
            {
                fclose(fp);
            }
        }
    }

    if (browse_audio)
    {
        if (!browse_mp3)
  	    {
            update_WAV_header(fp_audio_browse);
        }
    }

    // shutdown av encoder
    if (enc_av)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

        if (ffmpeg_encoder_av_close(enc_av) != 0)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C: ffmpeg_encoder_av_close() failed\n"), src_name.c_str()));
        }
    }

    // move av file
    if (enc_av)
    {
        package_creator->RelocateFile(destination_dir.str());
    }

    // cleanup mt_encoder
    if (mt_encoder)
    {
        mt_encoder->Close();
        delete mt_encoder;
    }

    // shutdown ffmpeg encoder
    if (ffmpeg_encoder)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

        if (ffmpeg_encoder_close(ffmpeg_encoder) != 0)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C: ffmpeg_encoder_close() failed\n"), src_name.c_str()));
        }
    }

    // shutdown ffmpeg audio encoder
    if (ffmpeg_audio_encoder)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);
    
        if (ffmpeg_encoder_close(ffmpeg_audio_encoder) != 0)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C: ffmpeg_encoder_close() failed\n"), src_name.c_str()));
        }
    }
    
    // shutdown MJPEG encoder
    if (mj_encoder)
    {
        mjpeg_compress_free(mj_encoder);
    }
    if (mj_encoder_threaded)
    {
        mjpeg_compress_free_threaded(mj_encoder_threaded);
    }

    // cleanup timecode overlay
    if (tco)
    {
        tc_overlay_close(tco);
    }
    delete [] tc_overlay_buffer;

    // cleanup quad buffer
    if (quad_video)
    {
        free_YUV_frame(&quad_frame);
        delete [] quad_workspace;
    }

    IngexShm::Instance()->InfoSetRecording(channel_i, p_opt->index, quad_video, false);

    // Get user comments (only available after stop)
    p_rec->GetMetadata(project_name, user_comments);

    // Add a user comment for recording location
    user_comments.push_back(
        prodauto::UserComment(AVID_UC_LOCATION_NAME, location.c_str(), STATIC_COMMENT_POSITION, 0));

    // Add a user comment for organisation
    user_comments.push_back(
        prodauto::UserComment(AVID_UC_ORGANISATION_NAME, "BBC", STATIC_COMMENT_POSITION, 0));

    package_creator->UpdateUserComments(user_comments);
    package_creator->UpdateProjectName(project_name);
    
    // Debug user comments
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C %C: User comments...\n"), src_name.c_str(), resolution_name.c_str()));
    for (std::vector<prodauto::UserComment>::const_iterator
        it = user_comments.begin(); it != user_comments.end(); ++it)
    {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("  name=\"%C\" value=\"%C\" position=%d colour=%d\n"),
            it->name.c_str(), it->value.c_str(), (int)it->position, it->colour));
    }

    // Complete MXF writing
    if (mxf)
    {
        try
        {
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C record thread %d completing MXF save\n"), src_name.c_str(), p_opt->index));
            
            writer->CompleteWriting(false);
        }
        catch (const prodauto::DBException & e)
        {
            LOG_RECORD_ERROR("Database exception: %C\n", e.getMessage().c_str());
            p_rec->NoteFailure();
        }
        catch (const prodauto::MXFWriterException & e)
        {
            LOG_RECORD_ERROR("MXFWriterException: %C\n", e.getMessage().c_str());
            p_rec->NoteFailure();
        }
    }
    
    // Update durations for non-MXF files
    if (!mxf)
    {
        package_creator->UpdateDuration(p_opt->FramesWritten());
    }
    
    // Store recordings in database
    try
    {
        if (mxf || (SAVE_PACKAGE_DATA && ENCODER_FFMPEG_AV == encoder && !quad_video))
            package_creator->SaveToDatabase();
        
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Saved package group '%C' to database\n"), package_creator->GetMaterialPackage()->name.c_str()));
    }
    catch (const prodauto::DBException & dbe)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
    }
    
    // Store updated filenames for each track but only for first encoding (index == 0)
    if (p_opt->index == 0)
    {
        for (unsigned int i = 0; i < package_creator->GetMaterialPackage()->tracks.size(); ++i)
        {
            prodauto::Track * mp_trk = package_creator->GetMaterialPackage()->tracks[i];

            std::string fname = package_creator->GetFileLocation(mp_trk->id);

            // Now need to find out which member of mTracks we are dealing with
            unsigned int track_index = p_impl->TrackIndexMap(mp_stc_dbids[i]);
            p_rec->mFileNames[track_index] = fname;

            ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C thread %d mp_track %d filename \"%C\" track_index %d\n"),
                src_name.c_str(), p_opt->index, i, fname.c_str(), track_index));
        }
    }

    // Clean up packages
    delete package_creator;

    // All done
    ACE_DEBUG((LM_INFO, ACE_TEXT("%C record thread %d exiting\n"), src_name.c_str(), p_opt->index));
    return 0;
}


/**
Thread function to wait for recording threads to finish and
then perform completion tasks.
*/
ACE_THR_FUNC_RETURN manage_record_thread(void *p_arg)
{
    IngexRecorder * p = (IngexRecorder *)p_arg;

    ACE_DEBUG((LM_DEBUG, ACE_TEXT("manage_record_thread:\n")));

    // Wait for all threads to finish
    std::vector<ThreadParam>::iterator it;
    for (it = p->mThreadParams.begin(); it != p->mThreadParams.end(); ++it)
    {
        if (it->id)
        {
            int err = ACE_Thread::join(it->id, 0, 0);
            if (0 == err)
            {
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("Joined record thread %x\n"), *it));
            }
            else
            {
                ACE_DEBUG((LM_ERROR, ACE_TEXT("Failed to join record thread %x: %s\n"), *it, strerror(err)));
            }
        }
    }

#if 0
    // Set pathnames for metadata files
    std::string vid_meta_name = p->mVideoPath;
    vid_meta_name += "metadata.txt";
    std::string dvd_meta_name = p->mDvdPath;
    dvd_meta_name += "metadata.txt";

    // Write metadata file alongside uncompressed video files
    p->WriteMetadataFile(vid_meta_name.c_str());

    // If dvd path is different, write another copy alongside
    // the MPEG2 files.
    if (dvd_meta_name != vid_meta_name)
    {
        p->WriteMetadataFile(dvd_meta_name.c_str());
    }
#endif

    // Signal completion of recording
    p->DoCompletionCallback();

    return 0;
}



