/*
 * $Id: recorder_functions.cpp,v 1.6 2008/09/04 15:38:44 john_f Exp $
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

const bool THREADED_MJPEG = true;
#define USE_SOURCE   0 // Eventually will move to encoding a source, rather than a hardware input
#define PACKAGE_DATA 0 // Write to database for non-MXF files

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

    std::string src_name;
#if USE_SOURCE
    prodauto::SourceConfig * sc = 0;
    if (p_impl->mSourceConfigs.find(p_opt->source_id) != p_impl->mSourceConfigs.end())
    {
        sc = p_impl->mSourceConfigs[p_opt->source_id];
    }
    if (sc)
    {
        src_name = (quad_video ? QUAD_NAME : sc->name);
    }
#else
    int channel_i = p_opt->channel_num;
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

    // Start timecode passed as metadata to MXFWriter
    int start_tc = p_rec->mStartTimecode;

    // We make sure these offsets are positive numbers.
    // For quad, channel_i is first enabled channel.
    int quad0_offset = (p_rec->mStartFrame[0] - p_rec->mStartFrame[channel_i] + ring_length) % ring_length;
    int quad1_offset = (p_rec->mStartFrame[1] - p_rec->mStartFrame[channel_i] + ring_length) % ring_length;
    int quad2_offset = (p_rec->mStartFrame[2] - p_rec->mStartFrame[channel_i] + ring_length) % ring_length;
    int quad3_offset = (p_rec->mStartFrame[3] - p_rec->mStartFrame[channel_i] + ring_length) % ring_length;


    const int WIDTH = IngexShm::Instance()->Width();
    const int HEIGHT = IngexShm::Instance()->Height();
    const int SIZE_420 = WIDTH * HEIGHT * 3/2;
    const int SIZE_422 = WIDTH * HEIGHT * 2;
    // Audio samples per frame not constant in NTSC so the
    // calculation below doesn't quite work.
    const int audio_samples_per_frame = 48000 * IngexShm::Instance()->FrameRateDenominator()
                                              / IngexShm::Instance()->FrameRateNumerator();

    // Track enables
    unsigned int track_offset = channel_i * p_rec->mTracksPerChannel;
    bool enable_video = p_rec->mTrackEnable[track_offset];
    bool enable_audio12 = false;
    bool enable_audio34 = false;
    bool enable_audio56 = false;
    bool enable_audio78 = false;
    if (p_rec->mTracksPerChannel >= 3)
    {
        enable_audio12 = p_rec->mTrackEnable[track_offset + 1]
            || p_rec->mTrackEnable[track_offset + 2];
    }
    if (p_rec->mTracksPerChannel >= 5)
    {
        enable_audio34 = p_rec->mTrackEnable[track_offset + 3]
            || p_rec->mTrackEnable[track_offset + 4];
    }
    if (p_rec->mTracksPerChannel >= 9)
    {
        enable_audio56 = p_rec->mTrackEnable[track_offset + 5]
            || p_rec->mTrackEnable[track_offset + 6];
        enable_audio78 = p_rec->mTrackEnable[track_offset + 7]
            || p_rec->mTrackEnable[track_offset + 8];
    }

    // Mask for MXF track enables
    uint32_t mxf_mask = 0;
    if (enable_video)    mxf_mask |= 0x00000001;
    if (enable_audio12)  mxf_mask |= 0x00000006;
    if (enable_audio34)  mxf_mask |= 0x00000018;
    if (enable_audio56)  mxf_mask |= 0x00000060;
    if (enable_audio78)  mxf_mask |= 0x00000180;

    // Settings pointer
    RecorderSettings * settings = RecorderSettings::Instance();

    bool browse_audio = (channel_i == 0 && !quad_video && settings->browse_audio);

    // Get encode settings for this thread
    int resolution = p_opt->resolution;
    int file_format = p_opt->file_format;
    bool uncompressed_video = (UNC_MATERIAL_RESOLUTION == resolution);

    bool mxf = (MXF_FILE_FORMAT_TYPE == file_format);
    bool one_file_per_track = mxf;
    bool raw = !mxf;

    // Encode parameters
    prodauto::Rational image_aspect = settings->image_aspect;
    int raw_audio_bits = settings->raw_audio_bits;
    int mxf_audio_bits = settings->mxf_audio_bits;
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
        ACE_TEXT("start_record_thread(%C, start_tc=%C res=%d bitc=%C)\n"),
        src_name.c_str(), Timecode(start_tc, fps, df).Text(), resolution, (bitc ? "on" : "off")));

    // Set up packages and tracks
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
    if (mxf && (0 == rc))
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("MXF metadata unavailable.\n")));
        mxf = false;
    }

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
    filename << p_opt->dir << p_opt->file_ident << filename_extension;

    // video resolution name
    std::string resolution_name = DatabaseEnums::Instance()->ResolutionName(resolution);

    // Get project name
    prodauto::ProjectName project_name;
    std::vector<prodauto::UserComment> user_comments;
    p_rec->GetMetadata(project_name, user_comments); // user comments empty at the moment

#if PACKAGE_DATA
    // I'll start all this with the SourcePackage I'm recording and will
    // assume only one being recorded in this thread.  (Not necessarily
    // the case at the moment but it will be eventually.)
    prodauto::RecorderInputTrackConfig * ritc = 0;
    if (ric)
    {
        ritc = ric->getTrackConfig(1); // index starts from 1
    }
    prodauto::SourceConfig * sc = 0;
    if (ritc)
    {
        sc = ritc->sourceConfig;
    }
    prodauto::SourcePackage * rsp = 0;
    if (sc)
    {
        rsp = sc->getSourcePackage();
    }
    // So, rsp is the SourcePackage to be recorded.
    ACE_DEBUG((LM_INFO, ACE_TEXT("SourcePackage: %C\n"), rsp->name.c_str()));

    // Now make a material package for this recording.

    prodauto::Timestamp now = prodauto::generateTimestampNow();

    // Go through tracks
    std::vector<prodauto::Track *>::const_iterator rsp_trk_it = rsp->tracks.begin();


    // Create file packages
    std::vector<prodauto::SourcePackage *> file_packages;
    unsigned int n_files = (one_file_per_track ? rsp->tracks.size() : 1);
    for (unsigned int i = 0; i < n_files; ++i)
    {
        // Create FileEssenceDescriptor
        prodauto::FileEssenceDescriptor * fd = new prodauto::FileEssenceDescriptor();
        fd->fileLocation = filename.str();
        fd->fileFormat = file_format;
        fd->videoResolutionID = resolution;
        fd->imageAspectRatio = image_aspect;
        fd->audioQuantizationBits = mxf_audio_bits;

        // Create file SourcePackage
        std::ostringstream name;
        name << p_opt->file_ident << "_" << i;
        prodauto::SourcePackage * fp = new prodauto::SourcePackage();
        fp->uid = prodauto::generateUMID();
        fp->name = name.str();
        fp->creationDate = now;
        fp->projectName = project_name;
        fp->sourceConfigName = sc->name;
        fp->descriptor = fd;

        ACE_DEBUG((LM_INFO, ACE_TEXT("File package %C\n"), fp->name.c_str()));


        unsigned int n_tracks_per_file = (one_file_per_track ? 1 : rsp->tracks.size());
        // tmp: limit tracks per file to 3 i.e. VA1A2
        if (n_tracks_per_file > 3)
        {
            n_tracks_per_file = 3;
        }

        // Create file package tracks
        for (unsigned int i = 0; i < n_tracks_per_file; ++i)
        {
            prodauto::Track * rsp_trk = *rsp_trk_it;

            // Check here if track enabled

            prodauto::Track * trk = new prodauto::Track();
            fp->tracks.push_back(trk);
            trk->id = rsp_trk->id; // using same id as src track makes things easier later on
            trk->dataDef = rsp_trk->dataDef;
            trk->editRate = rsp_trk->editRate;
            trk->name = rsp_trk->name;
            trk->number = rsp_trk->number;

            // Add track SourceClip refering to source/track being recorded
            trk->sourceClip = new prodauto::SourceClip();
            trk->sourceClip->sourcePackageUID = rsp->uid;
            trk->sourceClip->sourceTrackID = rsp_trk->id;
            if (rsp_trk->dataDef == PICTURE_DATA_DEFINITION)
            {
                trk->sourceClip->position = start_tc;
            }
            else
            {
                double audio_pos = start_tc;
                audio_pos /= prodauto::g_palEditRate.numerator;
                audio_pos *= prodauto::g_palEditRate.denominator;
                audio_pos *= rsp_trk->editRate.numerator;
                audio_pos /= rsp_trk->editRate.denominator;
                trk->sourceClip->position = (uint64_t) (audio_pos + 0.5);
            }

            ACE_DEBUG((LM_INFO, ACE_TEXT("Adding source track %d from %C\n"),
                rsp_trk->id, rsp->name.c_str()));

            ++rsp_trk_it;
        }

        // Add file package to vector, only if it has enabled tracks
        if (fp->tracks.size())
        {
            file_packages.push_back(fp);
        }
    }

    // Create MaterialPackage
    prodauto::MaterialPackage * mp = new prodauto::MaterialPackage();
    mp->uid = prodauto::generateUMID();
    mp->name = p_opt->file_ident;
    mp->creationDate = now;
    mp->projectName = project_name;

    ACE_DEBUG((LM_INFO, ACE_TEXT("Material package %C\n"), mp->name.c_str()));

    // Create material package tracks
    for (unsigned int i = 0; i < rsp->tracks.size(); ++i)
    {
        prodauto::Track * src_trk = rsp->tracks[i];

        prodauto::Track * trk = new prodauto::Track();
        mp->tracks.push_back(trk);

        trk->id = src_trk->id;
        trk->dataDef = src_trk->dataDef;
        trk->editRate = src_trk->editRate;
        trk->number = src_trk->number;
        trk->name = src_trk->name;

        // Add track SourceClip refering to file package.
        trk->sourceClip = new prodauto::SourceClip();
        unsigned int fi = (one_file_per_track ? i : 0);
        prodauto::SourcePackage * fp = file_packages[fi];
        trk->sourceClip->sourcePackageUID = fp->uid;
        trk->sourceClip->sourceTrackID = src_trk->id; // file package track id is same as source track id

        ACE_DEBUG((LM_INFO, ACE_TEXT("Adding source track %d from %C\n"),
            src_trk->id, fp->name.c_str()));

        trk->sourceClip->position = 0;
        trk->sourceClip->length = 0; // will need to be filled in with p_opt->FramesWritten();
    }


#endif

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
                use_primary_video = true;
            else if (IngexShm::Instance()->SecondaryCaptureFormat() == Format422PlanarYUV)
                use_primary_video = false;
            else
                LOG_RECORD_ERROR("Incompatible 422 capture format for encoding resolution %s\n", resolution_name.c_str());
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
                LOG_RECORD_ERROR("Incompatible 420 capture format for encoding resolution %s\n", resolution_name.c_str());
        }
        else // all other 4:2:0 resolutions
        {
            if (IngexShm::Instance()->SecondaryCaptureFormat() != Format420PlanarYUV)
                LOG_RECORD_ERROR("Incompatible 420 capture format for encoding resolution %s\n", resolution_name.c_str());
        }
    }

    // Directories
    const std::string & mxf_subdir_creating = settings->mxf_subdir_creating;
    const std::string & mxf_subdir_failures = settings->mxf_subdir_failures;

    // File pointers
    FILE * fp_video = NULL;
    FILE * fp_audio12 = NULL;
    FILE * fp_audio34 = NULL;
    FILE * fp_audio56 = NULL;
    FILE * fp_audio78 = NULL;
    FILE * fp_audio_browse = NULL;

    // Initialisation for raw (non-MXF) files
    if (raw && enable_video)
    {
        // We have already created the video filename

        if (NULL == (fp_video = fopen(filename.str().c_str(), "wb")))
        {
            enable_video = false;
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not open %C\n"), filename.str().c_str()));
        }
    }
    if (raw && enable_audio12)
    {
        std::ostringstream fname;
        fname << p_opt->dir << p_opt->file_ident << "_12.wav";

        if (NULL == (fp_audio12 = fopen(fname.str().c_str(), "wb")))
        {
            enable_audio12 = false;
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not open %C\n"), fname.str().c_str()));
        }
        else
        {
            writeWavHeader(fp_audio12, raw_audio_bits);
        }
    }
    if (raw && enable_audio34)
    {
        std::ostringstream fname;
        fname << p_opt->dir << p_opt->file_ident << "_34.wav";

        if (NULL == (fp_audio34 = fopen(fname.str().c_str(), "wb")))
        {
            enable_audio34 = false;
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not open %s\n"), fname.str().c_str()));
        }
        else
        {
            writeWavHeader(fp_audio34, raw_audio_bits);
        }
    }
    if (raw && enable_audio56)
    {
        std::ostringstream fname;
        fname << p_opt->dir << p_opt->file_ident << "_56.wav";

        if (NULL == (fp_audio56 = fopen(fname.str().c_str(), "wb")))
        {
            enable_audio56 = false;
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not open %C\n"), fname.str().c_str()));
        }
        else
        {
            writeWavHeader(fp_audio56, raw_audio_bits);
        }
    }
    if (raw && enable_audio78)
    {
        std::ostringstream fname;
        fname << p_opt->dir << p_opt->file_ident << "_78.wav";

        if (NULL == (fp_audio78 = fopen(fname.str().c_str(), "wb")))
        {
            enable_audio78 = false;
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not open %s\n"), fname.str().c_str()));
        }
        else
        {
            writeWavHeader(fp_audio78, raw_audio_bits);
        }
    }

    // Initialisation for browse audio
    if (browse_audio)
    {
        std::ostringstream fname;
        fname << p_opt->dir << p_opt->file_ident << ".wav";
        const char * f = fname.str().c_str();

        if (NULL == (fp_audio_browse = fopen(f, "wb")))
        {
            browse_audio = false;
            ACE_DEBUG((LM_ERROR, ACE_TEXT("Could not open %s\n"), f));
        }
        else
        {
            writeWavHeader(fp_audio_browse, browse_audio_bits);
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
        creating_path << p_opt->dir << '/' << mxf_subdir_creating;
        failures_path << p_opt->dir << '/' << mxf_subdir_failures;

        try
        {
            prodauto::MXFWriter * p = new prodauto::MXFWriter(
                                            ric,
                                            resolution,
                                            image_aspect,
                                            mxf_audio_bits,
                                            mxf_mask,
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

        // Store filenames for each track but only for first encoding (index == 0)
        if (mxf)
        {
            for (unsigned int i = 0; i < p_rec->mTracksPerChannel; ++i)
            {
                unsigned int track = i + 1;
                if (writer->trackIsPresent(track))
                {
                    std::string fname = writer->getDestinationFilename(writer->getFilename(track));
                    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Index %d file name: %C\n"), p_opt->index, fname.c_str()));
                    if (p_opt->index == 0)
                    {
                        p_rec->mFileNames[track_offset + i] = fname;
                    }
                }
                else
                {
                    ACE_DEBUG((LM_DEBUG, ACE_TEXT("Track %d not present\n"), track));
                }
            }
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

    // This makes the record start at the target frame
    int last_saved = start_frame[channel_i] - 1;
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("start_frame=%d\n"), start_frame));

    // Initialise last_tc which will be used to check for timecode discontinuities.
    framecount_t last_tc = IngexShm::Instance()->Timecode(channel_i, last_saved);

    // Update Record info
    IngexShm::Instance()->InfoSetRecording(channel_i, p_opt->index, quad_video, true);
    IngexShm::Instance()->InfoSetDesc(channel_i, p_opt->index, quad_video,
                "%s%s %s%s%s%s%s",
                resolution_name.c_str(),
                quad_video ? "(quad)" : "",
                raw ? "RAW" : "MXF",
                enable_audio12 ? " A12" : "",
                enable_audio34 ? " A34" : "",
                enable_audio56 ? " A56" : "",
                enable_audio78 ? " A78" : ""
                );

    p_opt->FramesWritten(0);
    p_opt->FramesDropped(0);
    bool finished_record = false;
    while (!finished_record)
    {
        // We read lastframe counter from shared memory in a thread safe manner.
        // Capture daemon updates lastframe after the frame has been written
        // to shared memory.

        int lastframe;
        int diff;

        // Are there frames available to save?
        // If not, sleep and check again.
        int guard = (quad_video ? 2 : 1);
        bool slept = false;
        while ((diff = (lastframe = IngexShm::Instance()->LastFrame(channel_i)) - last_saved) < guard)
        {
            // Caught up to latest available frame - sleep for a while.
            const int sleep_ms = 20;
            //ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C sleeping %d ms\n"), src_name.c_str(), sleep_ms));
            ACE_OS::sleep(ACE_Time_Value(0, sleep_ms * 1000));
            slept = true;
        }
        if (slept)
        {
            //ACE_DEBUG((LM_DEBUG, ACE_TEXT("%C processing %d frame(s)\n"), src_name.c_str(), diff));
        }

        IngexShm::Instance()->InfoSetBacklog(channel_i, p_opt->index, quad_video, diff);

        // Now we have some frames to save, check the backlog.
        int allowed_backlog = ring_length - 3;
        if (diff > allowed_backlog / 2)
        {
            // Warn about backlog
            ACE_DEBUG((LM_WARNING, ACE_TEXT("%C thread %d res=%d frames waiting = %d, max = %d\n"),
                src_name.c_str(), p_opt->index, resolution, diff, allowed_backlog));
        }
        if (diff > allowed_backlog)
        {
            // Dump frames
            int drop = diff - allowed_backlog;
            diff -= drop;
            last_saved += drop;
            p_opt->IncFramesDropped(drop);
            p_rec->NoteDroppedFrames();
            IngexShm::Instance()->InfoSetFramesDropped(channel_i, p_opt->index, quad_video, p_opt->FramesDropped());
            ACE_DEBUG((LM_ERROR, ACE_TEXT("%C dropped %d frames!\n"),
                src_name.c_str(), drop));
        }

#if 1
        // Save all frames which have not been saved
        for (int fi = diff - 1; fi >= 0 && !finished_record; fi--)
        /*
        initial frame = lastframe - diff + 1
                      = last_saved + 1
        final frame = lastframe
        */
#else
        // Save one frame
        for (int fi = diff - 1; fi >= diff - 1; fi--)
#endif
        {
            int frame = lastframe - fi;
            if (frame < 0)
            {
                frame += ring_length;
            }
            int frame0 = (frame + quad0_offset) % ring_length;
            int frame1 = (frame + quad1_offset) % ring_length;
            int frame2 = (frame + quad2_offset) % ring_length;
            int frame3 = (frame + quad3_offset) % ring_length;

            // Either the primary or secondary buffer can be used as input
            void * p_inp_video;
            if (use_primary_video)
            {
                p_inp_video = IngexShm::Instance()->pVideoPri(channel_i, frame);
            }
            else
            {
                p_inp_video = IngexShm::Instance()->pVideoSec(channel_i, frame);
            }

            // Pointer to audio12
            int32_t * p_audio12 = IngexShm::Instance()->pAudio12(channel_i, frame);

            // Pointer to audio34
            int32_t * p_audio34 = IngexShm::Instance()->pAudio34(channel_i, frame);

            // Pointer to audio56
            int32_t * p_audio56 = IngexShm::Instance()->pAudio56(channel_i, frame);

            // Pointer to audio78
            int32_t * p_audio78 = IngexShm::Instance()->pAudio78(channel_i, frame);

            // Timecode value
            framecount_t tc_i = IngexShm::Instance()->Timecode(channel_i, frame);

            // Check for timecode irregularities
            if (tc_i != last_tc + 1)
            {
                ACE_DEBUG((LM_ERROR, ACE_TEXT("%C thread %d Timecode discontinuity: %d frames missing at frame=%d tc=%C\n"),
                    src_name.c_str(),
                    p_opt->index,
                    tc_i - last_tc - 1,
                    frame,
                    Timecode(tc_i, fps, df).Text()));
            }

            // Make quad split
            if (quad_video)
            {
                // Setup input frame as YUV_frame and
                // set source for encoding to the quad frame.
                YUV_frame in_frame;
                uint8_t * quad_input0 = 0;
                uint8_t * quad_input1 = 0;
                uint8_t * quad_input2 = 0;
                uint8_t * quad_input3 = 0;

                // Possibly need more checking on whether 422 should
                // in fact come from secondary buffer

                if (pix_fmt == PIXFMT_420)
                {
                    quad_input0 = IngexShm::Instance()->pVideoSec(0, frame0);
                    quad_input1 = IngexShm::Instance()->pVideoSec(1, frame1);
                    quad_input2 = IngexShm::Instance()->pVideoSec(2, frame2);
                    quad_input3 = IngexShm::Instance()->pVideoSec(3, frame3);
                }
                else if (pix_fmt == PIXFMT_422)
                {
                    quad_input0 = IngexShm::Instance()->pVideoPri(0, frame0);
                    quad_input1 = IngexShm::Instance()->pVideoPri(1, frame1);
                    quad_input2 = IngexShm::Instance()->pVideoPri(2, frame2);
                    quad_input3 = IngexShm::Instance()->pVideoPri(3, frame3);
                }

                // top left - channel0
                if (p_rec->mChannelEnable[0])
                {
                    YUV_frame_from_buffer(&in_frame, quad_input0,
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
                    YUV_frame_from_buffer(&in_frame, quad_input1,
                        WIDTH, HEIGHT, format);

                    quarter_frame(&in_frame, &quad_frame,
                                    WIDTH/2, 0,
                                    1, 1, 1,
                                    quad_workspace);
                }

                // bottom left - channel2
                if (p_rec->mChannelEnable[2])
                {
                    YUV_frame_from_buffer(&in_frame, quad_input2,
                        WIDTH, HEIGHT, format);

                    quarter_frame(&in_frame, &quad_frame,
                                    0, HEIGHT/2,
                                    1, 1, 1,
                                    quad_workspace);
                }

                // bottom right - channel3
                if (p_rec->mChannelEnable[3])
                {
                    YUV_frame_from_buffer(&in_frame, quad_input3,
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
            if (bitc)
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
            int16_t mixed_audio[1920*2];    // for stereo pair output
            if (browse_audio || ENCODER_FFMPEG_AV == encoder)
            {
                mixer.Mix(p_audio12, p_audio34, mixed_audio, 1920);
            }

            // Save browse audio
            if (browse_audio)
            {
                write_audio(fp_audio_browse, (uint8_t *)mixed_audio, 1920*2, browse_audio_bits, false);
            }

            // encode to browse av formats
            if (ENCODER_FFMPEG_AV == encoder)
            {
                if (ffmpeg_encoder_av_encode(enc_av, (uint8_t *)p_inp_video, mixed_audio) != 0)
                {
                    ACE_DEBUG((LM_ERROR, ACE_TEXT("dvd_encoder_encode failed\n")));
                    encoder = ENCODER_NONE;
                }
            }


            uint8_t * p_enc_video;
            int size_enc_video = 0;

            // Encode with FFMPEG (IMX, DV, H264)
            if (ENCODER_FFMPEG == encoder)
            {
                size_enc_video = ffmpeg_encoder_encode(ffmpeg_encoder, (uint8_t *)p_inp_video, &p_enc_video);
            }

            // Encode MJPEG
            if (ENCODER_MJPEG == encoder)
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
            if (raw && enable_video)
            {
                if (uncompressed_video)
                {
                    fwrite(p_inp_video, SIZE_422, 1, fp_video);
                }
                else
                {
                    fwrite(p_enc_video, size_enc_video, 1, fp_video);
                }
            }

            // Write uncompressed audio
            if (raw && enable_audio12)
            {
                write_audio(fp_audio12, (uint8_t *)p_audio12, 1920*2, raw_audio_bits, true);
            }
            if (raw && enable_audio34)
            {
                write_audio(fp_audio34, (uint8_t *)p_audio34, 1920*2, raw_audio_bits, true);
            }
            if (raw && enable_audio56)
            {
                write_audio(fp_audio56, (uint8_t *)p_audio56, 1920*2, raw_audio_bits, true);
            }
            if (raw && enable_audio78)
            {
                write_audio(fp_audio78, (uint8_t *)p_audio78, 1920*2, raw_audio_bits, true);
            }


            // Write OP-Atom MXF files

            // Buffers for audio de-interleaving
            int16_t a0[audio_samples_per_frame];
            int16_t a1[audio_samples_per_frame];

            // Write MXF video
            if (mxf && enable_video)
            {
                try
                {
                    if (UNC_MATERIAL_RESOLUTION == resolution)
                    {
                        // write uncompressed video data to input track 1
                        writer->writeSample(1, 1, (uint8_t *)p_inp_video, SIZE_422);
                    }
                    else
                    {
                        // write encoded video data to input track 1
                        writer->writeSample(1, 1, p_enc_video, size_enc_video);
                    }
                }
                catch (const prodauto::MXFWriterException & e)
                {
                    LOG_RECORD_ERROR("MXFWriterException: %C\n", e.getMessage().c_str());
                    p_rec->NoteFailure();
                    mxf = false;
                }
            }

            if (mxf && enable_audio12)
            {
                // de-interleave audio channels 1/2 and convert to 16-bit
                deinterleave_32to16(p_audio12, a0, a1, audio_samples_per_frame);

                try
                {
                    // write pcm audio data to input track 2
                    writer->writeSample(2, audio_samples_per_frame, (uint8_t *)a0, audio_samples_per_frame * 2);

                    // write pcm audio data to input track 3
                    writer->writeSample(3, audio_samples_per_frame, (uint8_t *)a1, audio_samples_per_frame * 2);
                }
                catch (const prodauto::MXFWriterException & e)
                {
                    LOG_RECORD_ERROR("MXFWriterException: %C\n", e.getMessage().c_str());
                    p_rec->NoteFailure();
                    mxf = false;
                }
            }

            if (mxf && enable_audio34)
            {
                // de-interleave audio channels 3/4 and convert to 16-bit
                deinterleave_32to16(p_audio34, a0, a1, audio_samples_per_frame);

                try
                {
                    // write pcm audio data to input track 4
                    writer->writeSample(4, audio_samples_per_frame, (uint8_t *)a0, audio_samples_per_frame * 2);

                    // write pcm audio data to input track 5
                    writer->writeSample(5, audio_samples_per_frame, (uint8_t *)a1, audio_samples_per_frame * 2);
                }
                catch (const prodauto::MXFWriterException & e)
                {
                    LOG_RECORD_ERROR("MXFWriterException: %C\n", e.getMessage().c_str());
                    p_rec->NoteFailure();
                    mxf = false;
                }
            }

            if (mxf && enable_audio56)
            {
                // de-interleave audio channels 5/6 and convert to 16-bit
                deinterleave_32to16(p_audio56, a0, a1, audio_samples_per_frame);

                try
                {
                    // write pcm audio data to input track 6
                    writer->writeSample(6, audio_samples_per_frame, (uint8_t *)a0, audio_samples_per_frame * 2);

                    // write pcm audio data to input track 7
                    writer->writeSample(7, audio_samples_per_frame, (uint8_t *)a1, audio_samples_per_frame * 2);
                }
                catch (const prodauto::MXFWriterException & e)
                {
                    LOG_RECORD_ERROR("MXFWriterException: %C\n", e.getMessage().c_str());
                    p_rec->NoteFailure();
                    mxf = false;
                }
            }

            if (mxf && enable_audio78)
            {
                // de-interleave audio channels 7/8 and convert to 16-bit
                deinterleave_32to16(p_audio78, a0, a1, audio_samples_per_frame);

                try
                {
                    // write pcm audio data to input track 8
                    writer->writeSample(8, audio_samples_per_frame, (uint8_t *)a0, audio_samples_per_frame * 2);

                    // write pcm audio data to input track 9
                    writer->writeSample(9, audio_samples_per_frame, (uint8_t *)a1, audio_samples_per_frame * 2);
                }
                catch (const prodauto::MXFWriterException & e)
                {
                    LOG_RECORD_ERROR("MXFWriterException: %C\n", e.getMessage().c_str());
                    p_rec->NoteFailure();
                    mxf = false;
                }
            }

            // Completed this frame
            p_opt->IncFramesWritten();
            last_saved = frame;
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


    // update and close files
    if (raw && enable_video)
    {
        fclose(fp_video);
    }
    if (raw && enable_audio12)
    {
        update_WAV_header(fp_audio12);
    }
    if (raw && enable_audio34)
    {
        update_WAV_header(fp_audio34);
    }
    if (raw && enable_audio56)
    {
        update_WAV_header(fp_audio56);
    }
    if (raw && enable_audio78)
    {
        update_WAV_header(fp_audio78);
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
#if PACKAGE_DATA
    if (ENCODER_FFMPEG_AV == encoder)
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

    // Clean up packages
    for (std::vector<prodauto::SourcePackage *>::const_iterator
        it = file_packages.begin(); it != file_packages.end(); ++it)
    {
        delete *it;
    }
    delete mp;
#endif

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



