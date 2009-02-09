/*
 * $Id: recorder_functions.cpp,v 1.14 2009/02/09 19:26:29 john_f Exp $
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

#include "YUVlib/YUV_frame.h"
#include "YUVlib/YUV_quarter_frame.h"

// prodautodb lib
#include "Database.h"
#include "DBException.h"
#include "DatabaseEnums.h"
#include "Utilities.h"
#include "DataTypes.h"

// prodautodb recordmxf
#include "MXFWriter.h"
#include "MXFWriterException.h"

#include <ace/Thread.h>
#include <ace/OS_NS_unistd.h>

#include <iostream>
#include <sstream>

/// Mutex to ensure only one thread at a time can call avcodec open/close
static ACE_Thread_Mutex avcodec_mutex;

const bool THREADED_MJPEG = false;
const bool DEBUG_NOWRITE = false;
#define USE_SOURCE   0 // Eventually will move to encoding a source, rather than a hardware input
#define SAVE_PACKAGE_DATA 1 // Write to database for non-MXF files

// Macro to log an error using both the ACE_DEBUG() macro and the
// shared memory placeholder for error messages
// Note that format directives are not always the same
// e.g. %C with ACE_DEBUG vs. %s with printf
#define LOG_RECORD_ERROR(fmt, args...) { \
    ACE_DEBUG((LM_ERROR, ACE_TEXT( fmt ), ## args)); \
    IngexShm::Instance()->InfoSetRecordError(channel_i, p_opt->index, quad_video, fmt, ## args); }


/**
Thread function to encode and record a particular source.
*/
ACE_THR_FUNC_RETURN start_record_thread(void * p_arg)
{
    ThreadParam * param = (ThreadParam *)p_arg;
    RecordOptions * p_opt = param->p_opt;
    IngexRecorder * p_rec = param->p_rec;
    RecorderImpl * p_impl = p_rec->mpImpl;

    bool quad_video = p_opt->quad;

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

    // Note the recording location
    long location_id = 0;
    if (sc)
    {
        location_id = sc->recordingLocation;
    }
    std::string location;
    if (location_id)
    {
        // Here we need a database method to get the name from the id
        std::ostringstream ss;
        ss << "location " << location_id;
        location = ss.str();
    }

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

    // Start timecode passed as metadata to MXFWriter
    int start_tc = p_rec->mStartTimecode;

#if 0
    // We make sure these offsets are positive numbers.
    // For quad, channel_i is first enabled channel.
    int quad0_offset = (p_rec->mStartFrame[0] - p_rec->mStartFrame[channel_i] + ring_length) % ring_length;
    int quad1_offset = (p_rec->mStartFrame[1] - p_rec->mStartFrame[channel_i] + ring_length) % ring_length;
    int quad2_offset = (p_rec->mStartFrame[2] - p_rec->mStartFrame[channel_i] + ring_length) % ring_length;
    int quad3_offset = (p_rec->mStartFrame[3] - p_rec->mStartFrame[channel_i] + ring_length) % ring_length;
#endif

    const int WIDTH = IngexShm::Instance()->Width();
    const int HEIGHT = IngexShm::Instance()->Height();
    const int SIZE_420 = WIDTH * HEIGHT * 3/2;
    const int SIZE_422 = WIDTH * HEIGHT * 2;

#if 0
    // Audio samples per frame not constant in NTSC so we add a few.
    const int max_audio_samples_per_frame = 48000 * IngexShm::Instance()->FrameRateDenominator()
                                              / IngexShm::Instance()->FrameRateNumerator()
                                              + 5;
#endif

    // Mask for MXF track enables
    uint32_t mxf_mask = 0;
    for (unsigned int i = 0; i < track_enables.size(); ++i)
    {
        if (track_enables[i])
        {
            mxf_mask |= (1 << i);
        }
    }

    // Settings pointer
    RecorderSettings * settings = RecorderSettings::Instance();

    //bool browse_audio = (channel_i == 0 && !quad_video && settings->browse_audio);
    bool browse_audio = false;

    // Get encode settings for this thread
    int resolution = p_opt->resolution;
    int file_format = p_opt->file_format;
    bool uncompressed_video = (UNC_MATERIAL_RESOLUTION == resolution);

    bool mxf = (MXF_FILE_FORMAT_TYPE == file_format);
    bool one_file_per_track = mxf;
    bool raw = !mxf;

    // video resolution name
    std::string resolution_name = DatabaseEnums::Instance()->ResolutionName(resolution);
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Resolution %d (%C)\n"), resolution, resolution_name.c_str()));

    // file format name
    std::string file_format_name = DatabaseEnums::Instance()->FileFormatName(file_format);
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("File format %d (%C)\n"), file_format, file_format_name.c_str()));

    // Encode parameters
    prodauto::Rational image_aspect = settings->image_aspect;
    int raw_audio_bits = settings->raw_audio_bits;
    //int mxf_audio_bits = settings->mxf_audio_bits;
    int mxf_audio_bits = 16; // has to be 16 with current deinterleave function
    int browse_audio_bits = settings->browse_audio_bits;

    // burnt-in timecode settings
    bool bitc = p_opt->bitc;

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


    ACE_DEBUG((LM_INFO,
        ACE_TEXT("start_record_thread(%C, start_tc=%C %C %C %C\n"),
        src_name.c_str(), Timecode(start_tc, fps, df).Text(),
        file_format_name.c_str(), resolution_name.c_str(),
        (bitc ? "with BITC" : "")));

    // Set some flags based on encoding type

    // Initialising these to defaults which you will get for unsupported
    // resolution/file_format combinations.
    ffmpeg_encoder_resolution_t    ff_res    = FF_ENCODER_RESOLUTION_IMX30;
    ffmpeg_encoder_av_resolution_t ff_av_res = FF_ENCODER_RESOLUTION_MPEG4_MOV;

    // Initialising these just to avoid compiler warnings
    MJPEGResolutionID mjpeg_res = MJPEG_20_1;
    enum {PIXFMT_420, PIXFMT_422} pix_fmt = PIXFMT_422;
    enum {ENCODER_NONE, ENCODER_FFMPEG, ENCODER_FFMPEG_AV, ENCODER_MJPEG} encoder = ENCODER_NONE;

    std::string filename_extension;
    switch (resolution)
    {
    // DV formats
    case DV25_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_420;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DV25;
        ff_av_res = FF_ENCODER_RESOLUTION_DV25_MOV;
        filename_extension = ".dv";
        break;
    case DV50_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DV50;
        filename_extension = ".dv";
        break;
    // MJPEG formats
    case MJPEG21_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_MJPEG;
        mjpeg_res = MJPEG_2_1;
        break;
    case MJPEG31_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
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
        encoder = ENCODER_MJPEG;
        mjpeg_res = MJPEG_15_1s;
        break;
    case MJPEG201_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_MJPEG;
        mjpeg_res = MJPEG_20_1;
        break;
    case MJPEG101M_MATERIAL_RESOLUTION:
        encoder = ENCODER_MJPEG;
        pix_fmt = PIXFMT_422;
        mjpeg_res = MJPEG_10_1m;
        break;
    // IMX formats
    case IMX30_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_IMX30;
        filename_extension = ".m2v";
        break;
    case IMX40_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_IMX40;
        filename_extension = ".m2v";
        break;
    case IMX50_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_IMX50;
        filename_extension = ".m2v";
        break;
    // DNxHD formats
    case DNX36p_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DNX36p;
        break;
    case DNX120p_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DNX120p;
        break;
    case DNX185p_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DNX185p;
        break;
    case DNX120i_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DNX120i;
        break;
    case DNX185i_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DNX185i;
        break;
    // H264
    case DMIH264_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_420;
        encoder = ENCODER_FFMPEG;
        ff_res = FF_ENCODER_RESOLUTION_DMIH264;
        filename_extension = ".h264";
        break;
    // Browse formats
    case DVD_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_420;
        encoder = ENCODER_FFMPEG_AV;
        ff_av_res = FF_ENCODER_RESOLUTION_DVD;
        mxf = false;
        raw = false;
        filename_extension = ".mpg";
        break;
    case MPEG4_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_420;
        encoder = ENCODER_FFMPEG_AV;
        ff_av_res = FF_ENCODER_RESOLUTION_MPEG4_MOV;
        mxf = false;
        raw = false;
        break;
    case UNC_MATERIAL_RESOLUTION:
        pix_fmt = PIXFMT_422;
        encoder = ENCODER_NONE;
        filename_extension = ".yuv";
        break;
    default:
        break;
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
    case UNSPECIFIED_FILE_FORMAT_TYPE:
    default:
        break;
    }

    // (video) filename
    std::ostringstream filename;
    filename << p_opt->dir << PATH_SEPARATOR << p_opt->file_ident << filename_extension;

    // Get project name
    prodauto::ProjectName project_name;
    std::vector<prodauto::UserComment> user_comments;
    p_rec->GetMetadata(project_name, user_comments); // user comments empty at the moment


    // Create package data to go into database.

    // Start with the SourcePackage being recording and 
    // assume only one being recorded in this thread.
    // (Not necessarily the case at the moment but it will be eventually.)
    prodauto::SourcePackage * rsp = 0;
    if (sc)
    {
        rsp = sc->getSourcePackage();
    }
    // So, rsp is the SourcePackage to be recorded.
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("SourcePackage: %C\n"), rsp->name.c_str()));

    // track_enables correspond to tracks of sc/rsp

    prodauto::Timestamp now = prodauto::generateTimestampNow();

    // Make a material package for this recording.
    // The material package will only have the enabled tracks.
    // After this we only use material package so no need to
    // check track enables.
    prodauto::MaterialPackage * mp = new prodauto::MaterialPackage();
    mp->uid = prodauto::generateUMID();
    mp->name = p_opt->file_ident;
    mp->creationDate = now;
    mp->projectName = project_name;

    // File packages will be stored here
    std::vector<prodauto::SourcePackage *> file_packages;
    prodauto::SourcePackage * fp_one = 0;

    // For each track in mp, we keep a record of corresponding
    // hardware track.
    std::vector<HardwareTrack> mp_hw_trks;

    // For each track in mp, we keep a record of corresponding
    // SourceTrackConfig database id
    std::vector<long> mp_stc_dbids;

    // Go through the tracks.
    for (unsigned int i = 0; i < rsp->tracks.size(); ++i)
    {
        if (track_enables[i])
        {
            prodauto::Track * rsp_trk = rsp->tracks[i];

            // Get HardwareTrack based on SourceTrackConfig database id;
            prodauto::SourceTrackConfig * stc = sc->trackConfigs[i];
            mp_stc_dbids.push_back(stc->getDatabaseID());
            mp_hw_trks.push_back(p_impl->TrackHwMap(stc->getDatabaseID()));

            // Material package track
            prodauto::Track * mp_trk = new prodauto::Track();
            mp->tracks.push_back(mp_trk);

            mp_trk->id = rsp_trk->id;
            mp_trk->dataDef = rsp_trk->dataDef;
            if (mp_trk->dataDef == SOUND_DATA_DEFINITION)
            {
                mp_trk->editRate = prodauto::g_audioEditRate;
            }
            else
            {
                mp_trk->editRate = prodauto::g_palEditRate;
            }
            //mp_trk->editRate = rsp_trk->editRate;
            //ACE_DEBUG((LM_INFO, ACE_TEXT("Track %d, edit rate %d/%d\n"), i, mp_trk->editRate.numerator, mp_trk->editRate.denominator));
            mp_trk->number = rsp_trk->number;
            mp_trk->name = rsp_trk->name;

            // File source package
            prodauto::SourcePackage * fp = 0;
            if (!one_file_per_track)
            {
                fp = fp_one;
            }

            if (!fp)
            {
                fp = new prodauto::SourcePackage();
                file_packages.push_back(fp);

                std::ostringstream name;
                name << p_opt->file_ident << "_" << i;
                fp->name = name.str();
                fp->uid = prodauto::generateUMID();
                fp->creationDate = now;
                fp->projectName = project_name;
                fp->sourceConfigName = sc->name;

                // Create FileEssenceDescriptor
                prodauto::FileEssenceDescriptor * fd = new prodauto::FileEssenceDescriptor();
                fp->descriptor = fd;

                fd->fileLocation = filename.str(); // should be track filename
                fd->fileFormat = file_format;
                fd->videoResolutionID = resolution;
                fd->imageAspectRatio = image_aspect;
                fd->audioQuantizationBits = mxf_audio_bits;

                if (!one_file_per_track)
                {
                    fp_one = fp;
                }
            }

            // Create the file package track
            prodauto::Track * fp_trk = new prodauto::Track();
            fp->tracks.push_back(fp_trk);

            fp_trk->id = rsp_trk->id; // using same id as src track makes things easier later on
            fp_trk->dataDef = rsp_trk->dataDef;
            if (fp_trk->dataDef == SOUND_DATA_DEFINITION)
            {
                fp_trk->editRate = prodauto::g_audioEditRate;
            }
            else
            {
                fp_trk->editRate = prodauto::g_palEditRate;
            }
            //fp_trk->editRate = rsp_trk->editRate;
            fp_trk->name = rsp_trk->name;
            fp_trk->number = rsp_trk->number;

            // Add SourceClip refering to source/track being recorded
            fp_trk->sourceClip = new prodauto::SourceClip();
            fp_trk->sourceClip->sourcePackageUID = rsp->uid;
            fp_trk->sourceClip->sourceTrackID = rsp_trk->id;
            if (rsp_trk->dataDef == PICTURE_DATA_DEFINITION)
            {
                fp_trk->sourceClip->position = start_tc;
            }
            else
            {
                double audio_pos = start_tc;
                audio_pos /= prodauto::g_palEditRate.numerator;
                audio_pos *= prodauto::g_palEditRate.denominator;
                audio_pos *= rsp_trk->editRate.numerator;
                audio_pos /= rsp_trk->editRate.denominator;
                fp_trk->sourceClip->position = (uint64_t) (audio_pos + 0.5);
            }

            // Add file package track as source clip of material package track
            mp_trk->sourceClip = new prodauto::SourceClip();
            mp_trk->sourceClip->sourcePackageUID = fp->uid;
            mp_trk->sourceClip->sourceTrackID = fp_trk->id; // file package track id is same as source track id
        }
    }


    // Work out whether the primary video buffer (4:2:2 only) or the secondary
    // video (4:2:2 or 4:2:0) is to be used as the video input for encoding.
    bool use_primary_video = true;
    if (PIXFMT_422 == pix_fmt)
    {
        // Of the 4:2:2 formats only DV50 requires the line shift
        if (DV50_MATERIAL_RESOLUTION == resolution)
        {
            if (IngexShm::Instance()->PrimaryCaptureFormat() == Format422PlanarYUVShifted)
            {
                use_primary_video = true;
            }
            else if (IngexShm::Instance()->SecondaryCaptureFormat() == Format422PlanarYUVShifted)
            {
                use_primary_video = false;
            }
            else
            {
                LOG_RECORD_ERROR("Incompatible 422 capture format for encoding resolution %s\n", resolution_name.c_str());
            }
        }
        else // all other 4:2:2 resolutions
        {
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
                LOG_RECORD_ERROR("Incompatible 422 capture format for encoding resolution %s\n", resolution_name.c_str());
            }
        }
    }
    else if (PIXFMT_420 == pix_fmt)
    {
        // Only the secondary buffer supports 4:2:0
        use_primary_video = false;

        // Of the 4:2:0 formats only DV25 requires the line shift
        if (DV25_MATERIAL_RESOLUTION == resolution)
        {
            if (IngexShm::Instance()->SecondaryCaptureFormat() != Format420PlanarYUVShifted)
            {
                LOG_RECORD_ERROR("Incompatible 420 capture format for encoding resolution %s\n", resolution_name.c_str());
            }
        }
        else // all other 4:2:0 resolutions
        {
            if (IngexShm::Instance()->SecondaryCaptureFormat() != Format420PlanarYUV)
            {
                LOG_RECORD_ERROR("Incompatible 420 capture format for encoding resolution %s\n", resolution_name.c_str());
            }
        }
    }

    // Directories
    const std::string & mxf_subdir_creating = settings->mxf_subdir_creating;
    const std::string & mxf_subdir_failures = settings->mxf_subdir_failures;

    // Raw files
    std::vector<FILE *> fp_raw;
    for (unsigned int i = 0; i < mp->tracks.size(); ++i)
    {
        FILE * fp = NULL;
        prodauto::Track * mp_trk = mp->tracks[i];

        if (raw)
        {
            // Make filename
            std::ostringstream fname;
            if (mp_trk->dataDef == PICTURE_DATA_DEFINITION)
            {
                // We have already created the video filename
                fname << filename.str();
            }
            else
            {
                fname << p_opt->dir << PATH_SEPARATOR << p_opt->file_ident << "_" << mp_trk->name << ".wav";
            }
            // Open file
            if (NULL == (fp = fopen(fname.str().c_str(), "wb")))
            {
                ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not open %C\n"), fname.str().c_str()));
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
    FILE * fp_audio_browse = NULL;
    if (browse_audio)
    {
        std::ostringstream fname;
        fname << p_opt->dir << PATH_SEPARATOR << p_opt->file_ident << ".wav";
        const char * f = fname.str().c_str();

        if (NULL == (fp_audio_browse = fopen(f, "wb")))
        {
            browse_audio = false;
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not open %s\n"), f));
        }
        else
        {
            writeWavHeader(fp_audio_browse, browse_audio_bits, 2);
        }
    }
    
    // Initialise ffmpeg encoder
    ffmpeg_encoder_t * ffmpeg_encoder = 0;
    if (ENCODER_FFMPEG == encoder)
    {
        // Prevent "insufficient thread locking around avcodec_open/close()"
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

        ffmpeg_encoder = ffmpeg_encoder_init(ff_res);
        if (!ffmpeg_encoder)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C: ffmpeg encoder init failed.\n"), src_name.c_str()));
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
    if (ENCODER_FFMPEG_AV == encoder)
    {
        // Prevent simultaneous calls to init function causing
        // "insufficient thread locking around avcodec_open/close()"
        // and also "format not registered".
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

        if (0 == (enc_av = ffmpeg_encoder_av_init(filename.str().c_str(),
            ff_av_res, start_tc)))
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C: ffmpeg_encoder_av_init() failed\n"), src_name.c_str()));
            encoder = ENCODER_NONE;
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
            prodauto::MXFWriter * p = new prodauto::MXFWriter(
                                            mp,
                                            file_packages,
                                            rsp,
                                            false,
                                            true, // is PAL
                                            resolution,
                                            image_aspect,
                                            mxf_audio_bits,
                                            start_tc,
                                            creating_path.str().c_str(),
                                            destination_path.str().c_str(),
                                            failures_path.str().c_str(),
                                            p_opt->file_ident,
                                            user_comments,
                                            project_name
                                            );
            writer.reset(p);
        }
        catch (const prodauto::DBException & dbe)
        {
            LOG_RECORD_ERROR("Database exception: %C\n", dbe.getMessage().c_str());
            mxf = false;
        }
        catch (const prodauto::MXFWriterException & e)
        {
            LOG_RECORD_ERROR("MXFWriterException: %C\n", e.getMessage().c_str());
            mxf = false;
        }
    }

    // Store filenames for each track but only for first encoding (index == 0)
    if (p_opt->index == 0)
    {
        for (unsigned int i = 0; i < mp->tracks.size(); ++i)
        {
            //prodauto::Track * mp_trk = mp->tracks[i];

            std::string fname;
            if (mxf)
            {
                unsigned int track = i + 1;
                if (writer->trackIsPresent(track))
                {
                    fname = writer->getDestinationFilename(writer->getFilename(track));
                }
            }
            else
            {
                fname = filename.str();
            }

            // Now need to find out which member of mTracks we are dealing with
            unsigned int track_index = p_impl->TrackIndexMap(mp_stc_dbids[i]);
            p_rec->mFileNames[track_index] = fname;
        }
    }

    // Initialisation for timecode overlay
    tc_overlay_t * tco = 0;
    uint8_t * tc_overlay_buffer = 0;
    if (bitc)
    {
        tco = tc_overlay_init();
        if (pix_fmt == PIXFMT_420)
        {
            tc_overlay_buffer = new uint8_t[SIZE_420];
        }
        else
        {
            tc_overlay_buffer = new uint8_t[SIZE_422];
        }
    }
    if (tco)
    {
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("TC overlay initialised\n")));
    }

    AudioMixer mixer;
    if (channel_i == 0)
    {
        mixer.SetMix(AudioMixer::CH12);
        //mixer.SetMix(AudioMixer::COMMENTARY3);
        //mixer.SetMix(AudioMixer::COMMENTARY4);
    }
    else
    {
        mixer.SetMix(AudioMixer::ALL);
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
    

    // ***************************************************
    // This is the loop which records audio/video frames
    // ***************************************************

    // Set lastsaved to make the record start at the target frame

    //int last_saved = start_frame[channel_i] - 1;
    int lastsaved[MAX_CHANNELS];
    for (unsigned int i = 0; i < channels_in_use.size(); ++i)
    {
        unsigned int ch = channels_in_use[i];
        lastsaved[ch] = start_frame[ch] - 1;
        ACE_DEBUG((LM_DEBUG, ACE_TEXT("Channel %d, start_frame=%d\n"), ch, start_frame[ch]));
    }

    // Initialise last_tc which will be used to check for timecode discontinuities.
    //framecount_t last_tc = IngexShm::Instance()->Timecode(channel_i, lastsaved[channel_i]);
    // Timecode value from first track (usually video).
    //HardwareTrack tc_hw = mp_hw_trks[0];
    HardwareTrack tc_hw = p_impl->TrackHwMap(mp_stc_dbids[0]);
    framecount_t last_tc = IngexShm::Instance()->Timecode(tc_hw.channel, lastsaved[tc_hw.channel]);


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
            while ((IngexShm::Instance()->LastFrame(*it) % ring_length) == (lastsaved[*it] % ring_length))
            {
                //ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C sleeping %d ms for channel %d\n"), src_name.c_str(), sleep_ms, *it));
                ACE_OS::sleep(ACE_Time_Value(0, sleep_ms * 1000));
            }
        }

        // Get number of frames to code from channel with fewest available
        int frames_to_code = 0;
        for (unsigned int i = 0; i < channels_in_use.size(); ++i)
        {
            unsigned int ch = channels_in_use[i];
            int ftc = (IngexShm::Instance()->LastFrame(ch) - lastsaved[ch] + ring_length) % ring_length;
            if (i == 0 || ftc < frames_to_code)
            {
                frames_to_code = ftc;
            }
        }

        //ACE_DEBUG((LM_DEBUG, ACE_TEXT("frames_to_code=%d\n"), frames_to_code));

        IngexShm::Instance()->InfoSetBacklog(channel_i, p_opt->index, quad_video, frames_to_code);

        // Now we have some frames to code, check the backlog.
        int allowed_backlog = ring_length - 3;
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
                lastsaved[ch] += drop;
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

        for (unsigned int fi = 0; fi < frames_to_code && !finished_record; ++fi)
        {
            int frame[MAX_CHANNELS];
            for (unsigned int i = 0; i < channels_in_use.size(); ++i)
            {
                unsigned int ch = channels_in_use[i];
                frame[ch] = (lastsaved[ch] + 1) % ring_length;
                ACE_DEBUG((LM_DEBUG, ACE_TEXT("Channel %d, Frame %d\n"),
                    ch, frame[ch]));
            }

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

            // Set up all the input pointers
            std::vector<void *> p_input;
            void * p_inp_video = 0;
            for (unsigned int i = 0; i < mp->tracks.size(); ++i)
            {
                void * p = 0;
                prodauto::Track * mp_trk = mp->tracks[i];
#if 1
                // Just to be slightly more efficient, avoid
                // accessing the TrackHwMap in this loop.
                HardwareTrack hw = mp_hw_trks[i];
#else
                HardwareTrack hw = p_impl->TrackHwMap(mp_stc_dbids[i]);
#endif

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
                }
                else
                {
                    //p = IngexShm::Instance()->pAudio(hw.channel, hw.track, frames[hw.channel]);
                    // Mono audio buffers not yet available
                    p = a[i - 1];
                }
                p_input.push_back(p);
            }

            // Timecode value from first track (usually video)
            framecount_t tc_i = IngexShm::Instance()->Timecode(tc_hw.channel, frame[tc_hw.channel]);

            // Check for timecode irregularities
            if (tc_i != last_tc + 1)
            {
                ACE_DEBUG((LM_ERROR, ACE_TEXT("%C thread %d Timecode discontinuity: %d frames missing at frame=%d tc=%C\n"),
                    src_name.c_str(),
                    p_opt->index,
                    tc_i - last_tc - 1,
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
            }

            // Add timecode overlay
            if (bitc && p_inp_video)
            {
                tc_overlay_setup(tco, tc_i);
                if (pix_fmt == PIXFMT_420)
                {
                    // 420
                    // Need to copy video as can't overwrite shared memory
                    memcpy(tc_overlay_buffer, p_inp_video, SIZE_420);
                    uint8_t * p_y = tc_overlay_buffer;
                    uint8_t * p_u = p_y + WIDTH * HEIGHT;
                    uint8_t * p_v = p_u + WIDTH * HEIGHT / 4;
                    tc_overlay_apply(tco, p_y, p_u, p_v, WIDTH, HEIGHT, tc_xoffset, tc_yoffset, TC420);
                    // Source for encoding is now the timecode overlay buffer
                    p_inp_video = tc_overlay_buffer;
                }
                else
                {
                    // 422
                    // Need to copy video as can't overwrite shared memory
                    memcpy(tc_overlay_buffer, p_inp_video, SIZE_422);
                    uint8_t * p_y = tc_overlay_buffer;
                    uint8_t * p_u = p_y + WIDTH * HEIGHT;
                    uint8_t * p_v = p_u + WIDTH * HEIGHT / 2;
                    tc_overlay_apply(tco, p_y, p_u, p_v, WIDTH, HEIGHT, tc_xoffset, tc_yoffset, TC422);
                    // Source for encoding is now the timecode overlay buffer
                    p_inp_video = tc_overlay_buffer;
                }
            }

            // Mix audio for browse version
            int16_t mixed_audio[audio_samples_per_frame * 2];    // for stereo pair output
#if 1
// Needs update for mono 32-bit audio buffers
            if (browse_audio || ENCODER_FFMPEG_AV == encoder)
            {
                mixer.Mix(p_audio12, p_audio34, mixed_audio, audio_samples_per_frame);
            }

            // Save browse audio
            if (browse_audio)
            {
                write_audio(fp_audio_browse, (uint8_t *)mixed_audio, audio_samples_per_frame * 2, 16, browse_audio_bits);
            }
#endif

            // encode to browse av formats
            if (ENCODER_FFMPEG_AV == encoder && p_inp_video)
            {
                if (ffmpeg_encoder_av_encode(enc_av, (uint8_t *)p_inp_video, mixed_audio) != 0)
                {
                    ACE_DEBUG((LM_ERROR, ACE_TEXT("dvd_encoder_encode failed\n")));
                    encoder = ENCODER_NONE;
                }
            }


            uint8_t * p_enc_video = 0;
            int size_enc_video = 0;

            // Encode with FFMPEG (IMX, DV, H264)
            if (ENCODER_FFMPEG == encoder && p_inp_video)
            {
                size_enc_video = ffmpeg_encoder_encode(ffmpeg_encoder, (uint8_t *)p_inp_video, &p_enc_video);
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
            }


            // Write raw (non-MXF) files
            if (raw && !DEBUG_NOWRITE)
            {
                if (uncompressed_video && p_inp_video)
                {
                    //fwrite(p_inp_video, SIZE_422, 1, fp_video);
                    fwrite(p_inp_video, SIZE_422, 1, fp_raw[0]);
                }
                else if (p_enc_video)
                {
                    //fwrite(p_enc_video, size_enc_video, 1, fp_video);
                    fwrite(p_enc_video, size_enc_video, 1, fp_raw[0]);
                }
            }

            // Write uncompressed audio
            for (unsigned int i = 0; i < mp->tracks.size(); ++i)
            {
                prodauto::Track * mp_trk = mp->tracks[i];
                if (raw && mp_trk->dataDef == SOUND_DATA_DEFINITION && !DEBUG_NOWRITE)
                {
                    //write_audio(fp_raw[i], (uint8_t *)p_input[i], audio_samples_per_frame, 32, raw_audio_bits);
                    write_audio(fp_raw[i], (uint8_t *)p_input[i], audio_samples_per_frame, 16, raw_audio_bits);
                }
            }

            // Write to tracks of MXF files
            if (mxf && !DEBUG_NOWRITE)
            {
                for (unsigned int i = 0; i < mp->tracks.size(); ++i)
                {
                    prodauto::Track * mp_trk = mp->tracks[i];
                    try
                    {
                        if (SOUND_DATA_DEFINITION == mp_trk->dataDef)
                        {
                            // write pcm audio
                            writer->writeSample(mp_trk->id, audio_samples_per_frame, (uint8_t *)p_input[i], audio_samples_per_frame * 2);
                        }
                        else if (UNC_MATERIAL_RESOLUTION == resolution)
                        {
                            // write uncompressed video
                            writer->writeSample(mp_trk->id, 1, (uint8_t *)p_inp_video, SIZE_422);
                        }
                        else
                        {
                            // write encoded video
                            writer->writeSample(mp_trk->id, 1, p_enc_video, size_enc_video);
                        }
                    }
                    catch (const prodauto::MXFWriterException & e)
                    {
                        LOG_RECORD_ERROR("MXFWriterException: %C\n", e.getMessage().c_str());
                        p_rec->NoteFailure();
                    }
                }
            }


            // Completed this frame
            p_opt->IncFramesWritten();
            for (unsigned int i = 0; i < channels_in_use.size(); ++i)
            {
                unsigned int ch = channels_in_use[i];
                lastsaved[ch] = frame[ch];
            }
            last_tc = tc_i;

            IngexShm::Instance()->InfoSetFramesWritten(channel_i, p_opt->index, quad_video, p_opt->FramesWritten());

            // Finish when we've reached target duration
            framecount_t target = p_rec->TargetDuration();
            framecount_t written = p_opt->FramesWritten();
            framecount_t dropped = p_opt->FramesDropped();
            framecount_t total = written + dropped;
            if (target > 0 && total >= target)
            {
                Timecode out_tc(last_tc + 1, fps, df);
                ACE_DEBUG((LM_INFO, ACE_TEXT("  %C index %d duration %d reached (total=%d written=%d dropped=%d) out frame %C\n"),
                    src_name.c_str(), p_opt->index, target, total, written, dropped, out_tc.Text()));
                finished_record = true;
            }

        } // Save all frames which have not been saved

    }
    // ************************
    // End of main record loop
    // ************************

    // Update and close raw files
    for (unsigned int i = 0; i < mp->tracks.size(); ++i)
    {
        FILE * fp = fp_raw[i];
        if (fp != NULL)
        {
            prodauto::Track * mp_trk = mp->tracks[i];
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
        update_WAV_header(fp_audio_browse);
    }

    // shutdown dvd encoder
    if (enc_av)
    {
        ACE_Guard<ACE_Thread_Mutex> guard(avcodec_mutex);

        if (ffmpeg_encoder_av_close(enc_av) != 0)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C: dvd_encoder_close() failed\n"), src_name.c_str()));
        }
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


    // Complete MXF writing and save packages to database
    if (mxf)
    {
        try
        {
            ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C record thread %d completing MXF save\n"), src_name.c_str(), p_opt->index));
            //writer->completeAndSaveToDatabase();
            writer->completeAndSaveToDatabase(user_comments, project_name);
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

    // Store non-MXF recordings in database
#if SAVE_PACKAGE_DATA
    if (ENCODER_FFMPEG_AV == encoder && !quad_video)
    {
        // Update material package tracks with duration
        for (std::vector<prodauto::Track *>::iterator
            it = mp->tracks.begin(); it != mp->tracks.end(); ++it)
        {
            prodauto::Track * mt = *it;

            if (mt->dataDef == PICTURE_DATA_DEFINITION)
            {
                mt->sourceClip->length = p_opt->FramesWritten();
            }
            else
            {
                double audio_len = p_opt->FramesWritten();
                audio_len /= prodauto::g_palEditRate.numerator;
                audio_len *= prodauto::g_palEditRate.denominator;
                audio_len *= mt->editRate.numerator;
                audio_len /= mt->editRate.denominator;
                mt->sourceClip->position = (uint64_t) (audio_len + 0.5);
            }
        }

        // Add user comments to material package
        for (std::vector<prodauto::UserComment>::const_iterator
            it = user_comments.begin(); it != user_comments.end(); ++it)
        {
            mp->addUserComment(it->name, it->value, it->position, it->colour);
        }

        // Now save packages in database
        prodauto::Database * db = 0;
        try
        {
            db = prodauto::Database::getInstance();
            std::auto_ptr<prodauto::Transaction> transaction(db->getTransaction());
        
            // Save the file source packages first because material package has
            // foreign keys referencing them.
            for (std::vector<prodauto::SourcePackage *>::const_iterator
                it = file_packages.begin(); it != file_packages.end(); ++it)
            {
                prodauto::SourcePackage * fp = *it;
                ACE_DEBUG((LM_INFO, ACE_TEXT("Saving file package %C\n"), fp->name.c_str()));
                db->savePackage(fp, transaction.get());
            }
            // Now save material package
            ACE_DEBUG((LM_INFO, ACE_TEXT("Saving material package %C\n"), mp->name.c_str()));
            db->savePackage(mp, transaction.get());

            transaction->commitTransaction();
        }
        catch (const prodauto::DBException & dbe)
        {
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Database Exception: %C\n"), dbe.getMessage().c_str()));
        }
    }
#endif

    // Clean up packages
    for (std::vector<prodauto::SourcePackage *>::const_iterator
        it = file_packages.begin(); it != file_packages.end(); ++it)
    {
        delete *it;
    }
    delete mp;

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



