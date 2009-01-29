/*
 * $Id: transcode_avid_mxf.cpp,v 1.9 2009/01/29 07:36:59 stuart_hc Exp $
 *
 * Transcodes Avid MXF files
 *
 * Copyright (C) 2007  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
// TODO: delete/free/clear memory is not complete
 
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

extern "C"
{
#ifdef FFMPEG_OLD_INCLUDE_PATHS
#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#else
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#endif

#include <mxf/mxf.h>
#include <mxf/mxf_avid.h>
#include <mxf/mxf_macros.h>

#include "mjpeg_compress.h"
}

// the names clash with structs in the mxf_metadata (eg MXFTrack) so we put it
// in another namespace
// TODO: change the names in the MXF reader
namespace mxfr
{
extern "C"
{
#include <mxf_reader.h>
}
}

#include <vector>
#include <map>
#include <string>

#include <Database.h>
#include <MXFWriter.h>
#include <MXFWriterException.h>
#include <MXFUtils.h>
#include <Utilities.h>
#include <DBException.h>
#include <Logging.h>


using namespace std;


#define MAX_MXF_TRANSCODE_INPUTS        16

#define CHECK_USER_QUIT(quitVar, action) \
    if (quitVar) \
    { \
        action; \
    } \
    else if (user_quit()) \
    { \
        prodauto::Logging::info("User has selected to quit\n"); \
        quitVar = true; \
        action; \
    }


static const char* g_dsn = "prodautodb";
static const char* g_databaseUserName = "bamzooki";
static const char* g_databasePassword = "bamzooki";

// elapsed time before completed transcode entries are deleted
static const prodauto::Interval completedDeleteTime = {0, 0, 15, 0, 0, 0, 0};

// elapsed time before failed transcode entries are deleted
static const prodauto::Interval failedDeleteTime = {0, 0, 30, 0, 0, 0, 0};

// Handle to MJPEG compress object
static mjpeg_compress_t *p_mjpeg_compressor = NULL;




// helper class to initialise and free the mjpeg library
class MJPEGCompressHolder
{
public:
    MJPEGCompressHolder() : _initialised(false) {}
    ~MJPEGCompressHolder()
    {
        if (_initialised)
        {
            mjpeg_compress_free(p_mjpeg_compressor);
        }
    }
    
    bool initialise(int width, int height)
    {
        // free existing compressor if the width or height has changed
        if (_initialised && (width != _width || height != _height))
        {
            mjpeg_compress_free(p_mjpeg_compressor);
            p_mjpeg_compressor = NULL;
            _initialised = false;
        }
        
        if (!mjpeg_compress_init(MJPEG_20_1, width, height, &p_mjpeg_compressor))
        {
            return false;
        }

        _initialised = true;
        _width = width;
        _height = height;
        return true;
    }
    
private:
    bool _initialised;
    int _width;
    int _height;
};

// helper class to initialise and close the database
class DatabaseHolder
{
public:
    DatabaseHolder() : _initialised(false) {}
    ~DatabaseHolder()
    {
        if (_initialised)
        {
            prodauto::Database::close();
        }
    }
    
    void initialise(string dsn, string username, string password,
        unsigned int initialConnections, unsigned int maxConnections)
    {
        prodauto::Database::initialise(dsn, username, password, 1, 3);
        _initialised = true;
    }
    
    void close()
    {
        if (_initialised)
        {
            _initialised = false;
            prodauto::Database::close();
        }
    }
    
    prodauto::Database* instance()
    {
        return prodauto::Database::getInstance();
    }
    
private:
    bool _initialised;
};

// utility class to clean-up Package pointers
class MaterialHolder
{
public:
    MaterialHolder() : sourceMaterialPackage(0) {}
    ~MaterialHolder()
    {
        prodauto::PackageSet::iterator iter;
        for (iter = packages.begin(); iter != packages.end(); iter++)
        {
            delete *iter;
        }
        
        // sourceMaterialPackage is in packages so we don't delete it
    }
    
    prodauto::Package* sourceMaterialPackage; 
    prodauto::PackageSet packages;

    map<prodauto::UMID, string> packageToFilepathMap;
};

    
class Decoder
{
public:
    enum DecoderType
    {
        DV_DECODER,
        MJPEG_DECODER
    };
    
public:
    Decoder()
    : _width(0), _height(0), _dec(NULL), _openedDecoder(false), _decFrame(NULL), 
    _isThreaded(false), _initialised(false)
    {}
    
    ~Decoder()
    {
        if (_dec != NULL)
        {
            if (_isThreaded)
            {
                // TODO: this causes thread deadlock when checking with valgrind 
                // avcodec_thread_free(connect->dec); 
            }
            if (_openedDecoder)
            {
                avcodec_close(_dec);
            }
            free(_dec);
            free(_decFrame);
        }
    }
    
    bool initialise(DecoderType decoderType, int numFFMPEGThreads, int width, int height)
    {
        if (_initialised)
        {
            if (width != _width || height != _height)
            {
                return false;
            }
            return true;
        }
        
        _width = width;
        _height = height;
        
        
        // initialise decoder 
        
        enum CodecID codecId;
        switch (decoderType)
        {
            case DV_DECODER:
                codecId = CODEC_ID_DVVIDEO;
                break;
            case MJPEG_DECODER:
            default:
                codecId = CODEC_ID_MJPEG;
                break;
        }
        
        AVCodec* decoder = avcodec_find_decoder(codecId);
        if (!decoder) 
        {
            prodauto::Logging::error("Could not find FFMPEG decoder\n");
            return false;
        }
    
        if ((_dec = avcodec_alloc_context()) == NULL)
        {
            return false;
        }
    
        if (numFFMPEGThreads > 1)
        {
            avcodec_thread_init(_dec, numFFMPEGThreads);
            _isThreaded = true;
        }
        
        avcodec_set_dimensions(_dec, width, height);
        _dec->pix_fmt = PIX_FMT_YUV422P;
    
        if (avcodec_open(_dec, decoder) < 0)
        {
            return false;
        }
        _openedDecoder = true;
    
        if ((_decFrame = avcodec_alloc_frame()) == NULL)
        {
            return false;
        }
        
        _initialised = true;
        return true;
    }
    
    AVCodecContext* getDecoder()
    {
        return _dec;
    }
    
    AVFrame* getFrame()
    {
        return _decFrame;
    }
    
private:    
    int _width;
    int _height;
    AVCodecContext* _dec;
    bool _openedDecoder; // only close if decoder opened 
    AVFrame* _decFrame;
    bool _isThreaded;
    bool _initialised;
};

typedef struct TranscodeAvidMXF TranscodeAvidMXF;


// the names clash with structs in the mxf_metadata (eg MXFTrack) so we put it
// in another namespace
// TODO: change the names in the MXF reader
namespace mxfr
{
    
    
struct _MXFReaderListenerData
{
    struct TranscodeAvidMXF* transcode;
    int streamIndex;
};
}

typedef struct
{
    uint32_t materialTrackID;    

    int isVideo;
    int width;
    int height;

    int requireTranscode;
    int inputVideoResolutionID;
    int outputVideoResolutionID;

    uint8_t* inputBuffer;
    uint32_t inputBufferSize;

    
    mxfr::MXFReader* reader;
    mxfr::MXFReaderListener listener;
    struct mxfr::_MXFReaderListenerData listenerData;
    
} TranscodeStream;

struct TranscodeAvidMXF
{
    bool isPALProject;
    
    MJPEGCompressHolder mjpeg;
    
    TranscodeStream streams[MAX_MXF_TRANSCODE_INPUTS];
    int numStreams;
    
    Decoder* decoder;
    
    prodauto::MXFWriter* mxfWriter;
};


static inline bool user_quit()
{
    char stdinBuf[1];
    fd_set rfds;
    struct timeval tv;
    int retval;
    
    /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    /* return straight away */
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    
    retval = select(1, &rfds, NULL, NULL, &tv);
    
    if (retval)
    {
        if (read(0, stdinBuf, sizeof(stdinBuf)) > 0)
        {
            if (stdinBuf[0] == 'q')
            {
                return true;
            }
        }
    }
    
    return false;
}

static int initialise_transcode(TranscodeAvidMXF* transcode, TranscodeStream* stream, int numFFMPEGThreads)
{
    // check transcode is supported 
    if (!stream->isVideo || 
        (stream->inputVideoResolutionID != DV50_MATERIAL_RESOLUTION &&
            stream->inputVideoResolutionID != MJPEG21_MATERIAL_RESOLUTION))
    {
        fprintf(stderr, "Input essence type not supported for transcode\n");
        return 0;
    }
    if (stream->outputVideoResolutionID != MJPEG201_MATERIAL_RESOLUTION)
    {
        fprintf(stderr, "Output essence type not supported for transcode\n");
        return 0;
    }
    
    Decoder::DecoderType decoderType;
    switch (stream->inputVideoResolutionID)
    {
        case DV50_MATERIAL_RESOLUTION:
            decoderType = Decoder::DV_DECODER;
            break;
        case MJPEG21_MATERIAL_RESOLUTION:
        default:
            decoderType = Decoder::MJPEG_DECODER;
            break;
    }

    // TODO: hardcoded WxH and assuming DV50 height
    stream->width = 720;
    stream->height = (transcode->isPALProject ? 576 : 480);

    CHK_ORET(transcode->decoder->initialise(decoderType, numFFMPEGThreads, stream->width, stream->height));
    
    return 1;
}

static void free_transcode(TranscodeStream* stream)
{}

static int transcode_stream(TranscodeStream* stream, uint8_t** buffer, uint32_t* bufferSize)
{
    TranscodeAvidMXF* transcode = stream->listenerData.transcode;
    unsigned char* mjpegBuffer = NULL;
    unsigned int mjpegBufferSize;
    int finished;
    int i, j;
    unsigned char* y;
    unsigned char* u;
    unsigned char* v;
    int result;

    // check transcode is supported
    if (!stream->isVideo ||
        (stream->inputVideoResolutionID != DV50_MATERIAL_RESOLUTION &&
            stream->inputVideoResolutionID != MJPEG21_MATERIAL_RESOLUTION))
    {
        fprintf(stderr, "Input essence type not supported for transcode\n");
        return 0;
    }
    if (stream->outputVideoResolutionID != MJPEG201_MATERIAL_RESOLUTION)
    {
        fprintf(stderr, "Output essence type not supported for transcode\n");
        return 0;
    }
    
    // decode 

    AVCodecContext* dec = transcode->decoder->getDecoder();
    AVFrame* decFrame = transcode->decoder->getFrame();
    
    
    result = avcodec_decode_video(dec, decFrame, &finished, stream->inputBuffer, stream->inputBufferSize);
    if (result < 0 || !finished) 
    {
        fprintf(stderr, "Error decoding DV video\n");
        return 0;
    }
    
    
    // shift DV material up one line because the field order is reversed
    if (stream->inputVideoResolutionID == DV50_MATERIAL_RESOLUTION)
    {
        // shift picture up one line
        y = decFrame->data[0];
        u = decFrame->data[1];
        v = decFrame->data[2];
        for (i = 1; i < stream->height; i++)
        {
            for (j = 0; j < decFrame->linesize[0]; j++)
            {
                *y = *(decFrame->data[0] + i * decFrame->linesize[0] + j);
                if (j < decFrame->linesize[1])
                {
                    *u = *(decFrame->data[1] + i * decFrame->linesize[1] + j);
                    *v = *(decFrame->data[2] + i * decFrame->linesize[2] + j);
                    u++;
                    v++;
                }
                y++;
            }
        }
        // fill last line with black
        for (j = 0; j < decFrame->linesize[0]; j++)
        {
            *y = 0x10;
            if (j < decFrame->linesize[1])
            {
                *u = 0x80;
                *v = 0x80;
                u++;
                v++;
            }
            y++;
        }
    }
    
    
    // encode MJPEG 20:1

    mjpegBufferSize = mjpeg_compress_frame_yuv(
        p_mjpeg_compressor,
        decFrame->data[0], 
        decFrame->data[1], 
        decFrame->data[2],
        decFrame->linesize[0], 
        decFrame->linesize[1], 
        decFrame->linesize[2],
        &mjpegBuffer);
    CHK_ORET(mjpegBufferSize != 0);

    *buffer = mjpegBuffer;
    *bufferSize = mjpegBufferSize;
    return 1;
}

static int accept_frame(mxfr::MXFReaderListener* listener, int trackIndex)
{
    // only single track expected 
    return trackIndex == 0;
}

static int allocate_buffer(mxfr::MXFReaderListener* listener, int trackIndex, uint8_t** buffer, uint32_t bufferSize)
{
    TranscodeAvidMXF* transcode = listener->data->transcode;
    TranscodeStream* stream = &transcode->streams[listener->data->streamIndex];
    
    // only single track expected 
    if (trackIndex != 0)
    {
        return 0;
    }
    
    if (stream->inputBufferSize < bufferSize)
    {
        SAFE_FREE(&stream->inputBuffer);
        stream->inputBufferSize = bufferSize + FF_INPUT_BUFFER_PADDING_SIZE;
        CHK_MALLOC_ARRAY_ORET(stream->inputBuffer, uint8_t, stream->inputBufferSize);
    }
    
    *buffer = stream->inputBuffer;
    return 1;
}

static void deallocate_buffer(mxfr::MXFReaderListener* listener, int trackIndex, uint8_t** buffer)
{
    // ignore request because buffer is reused 
}

static int receive_frame(mxfr::MXFReaderListener* listener, int trackIndex, uint8_t* buffer, uint32_t bufferSize)
{
    TranscodeAvidMXF* transcode = listener->data->transcode;
    TranscodeStream* stream = &listener->data->transcode->streams[listener->data->streamIndex];
    uint8_t* outputBuffer = NULL;
    uint32_t outputBufferSize = 0;
    
    // only single track expected 
    if (trackIndex != 0)
    {
        return 0;
    }
    
    CHK_ORET(stream->inputBufferSize >= bufferSize);
    
    if (!stream->requireTranscode)
    {
        // no transcode required     
        outputBuffer = stream->inputBuffer;
        outputBufferSize = bufferSize;
    }
    else
    {
        CHK_ORET(transcode_stream(stream, &outputBuffer, &outputBufferSize));
    }
    

    if (stream->isVideo)
    {
        transcode->mxfWriter->writeSample(stream->materialTrackID, 1, outputBuffer, outputBufferSize);    
    }
    else
    {
        // TODO: hardcoded num samples
        if (transcode->isPALProject)
        {
            transcode->mxfWriter->writeSample(stream->materialTrackID, 48000/25, outputBuffer, outputBufferSize);
        }
        else
        {
            assert(false); // NTSC audio reading not yet supported in MXFReader
            transcode->mxfWriter->writeSample(stream->materialTrackID, 48000/30, outputBuffer, outputBufferSize);
        }
    }
    
    return 1;
}

int get_stream(MaterialHolder& sourceMaterial, prodauto::UMID uid, vector<string>& inputFiles)
{
    int streamIndex;
    
    map<prodauto::UMID, string>::iterator iter1 = sourceMaterial.packageToFilepathMap.find(uid);
    assert(iter1 != sourceMaterial.packageToFilepathMap.end());
    
    vector<string>::const_iterator iter2;
    for (streamIndex = 0, iter2 = inputFiles.begin(); iter2 != inputFiles.end(); streamIndex++, iter2++)
    {
        if ((*iter2).compare((*iter1).second) == 0)
        {
            break;
        }
    }
    assert(iter2 != inputFiles.end());
    
    return streamIndex;    
}


int transcode_avid_mxf(Decoder* decoder,
                       vector<string>& inputFiles, 
                       MaterialHolder& sourceMaterial,
                       string outputPrefix,
                       string creatingDirectory, 
                       string destinationDirectory, 
                       string failureDirectory, 
                       int numFFMPEGThreads, 
                       int inputVideoResolutionID,
                       int outputVideoResolutionID)
{
    TranscodeAvidMXF transcode;
    int i;
    int result;
    int eof;
    prodauto::MaterialPackage* pa_materialPackage = 0;
    prodauto::SourcePackage* pa_tapePackage = 0;
    vector<prodauto::SourcePackage*> pa_filePackages;
    mxfPosition startPosition = 0;
    uint32_t audioQuantizationBits = 16;
    int videoResolutionID = MJPEG201_MATERIAL_RESOLUTION;
    prodauto::Rational imageAspectRatio = {4, 3};
    prodauto::PackageSet::iterator iter1;
    vector<prodauto::SourcePackage*>::const_iterator iter2;

    memset(&transcode, 0, sizeof(TranscodeAvidMXF));
    transcode.decoder = decoder;
    transcode.isPALProject = true;
    
    // get material, file and tape source packages
    CHK_OFAIL(sourceMaterial.sourceMaterialPackage->getType() == prodauto::MATERIAL_PACKAGE);
    pa_materialPackage = dynamic_cast<prodauto::MaterialPackage*>(sourceMaterial.sourceMaterialPackage);
    for (iter1 = sourceMaterial.packages.begin(); iter1 != sourceMaterial.packages.end(); iter1++)
    {
        prodauto::Package* package = *iter1;
        
        if (package->getType() != prodauto::SOURCE_PACKAGE)
        {
            continue;
        }
        prodauto::SourcePackage* sourcePackage = dynamic_cast<prodauto::SourcePackage*>(package);
        
        if (sourcePackage->descriptor->getType() == FILE_ESSENCE_DESC_TYPE)
        {
            pa_filePackages.push_back(sourcePackage);
        }
        else
        {
            pa_tapePackage = sourcePackage;
        }
    }
    
    
    // modify existing packages to create the new material
    
    // modify package id and recuresively set objects as not loaded
    pa_materialPackage->cloneInPlace(prodauto::generateUMID(), true);

    // modify the file source package UID and video resolution if appropriate; also get info
    for (iter2 = pa_filePackages.begin(); iter2 != pa_filePackages.end(); iter2++)
    {
        prodauto::SourcePackage* filePackage = *iter2;
        prodauto::UMID newFilePackageUID = prodauto::generateUMID();
        int streamIndex = get_stream(sourceMaterial, filePackage->uid, inputFiles);
        
        prodauto::FileEssenceDescriptor* fileDescriptor = dynamic_cast<prodauto::FileEssenceDescriptor*>(filePackage->descriptor);
        if (fileDescriptor->videoResolutionID != 0)
        {
            transcode.streams[streamIndex].isVideo = 1;
            transcode.streams[streamIndex].inputVideoResolutionID = fileDescriptor->videoResolutionID;
            transcode.streams[streamIndex].outputVideoResolutionID = outputVideoResolutionID;
            if (fileDescriptor->videoResolutionID != outputVideoResolutionID)
            {
                transcode.streams[streamIndex].requireTranscode = 1;
            }
            
            // modify video resolution
            fileDescriptor->videoResolutionID = outputVideoResolutionID;
            
            // get image data
            // TODO: MXFWriter should be updated to read this data itself
            imageAspectRatio = fileDescriptor->imageAspectRatio;
            vector<prodauto::Track*>::const_iterator iter5;
            for (iter5 = filePackage->tracks.begin(); iter5 != filePackage->tracks.end(); iter5++)
            {
                prodauto::Track* track = *iter5;
            
                if (track->sourceClip != 0 && track->sourceClip->sourcePackageUID == pa_tapePackage->uid)
                {
                    startPosition = track->sourceClip->position;
                    if (track->editRate.numerator == 25 &&
                        track->editRate.denominator == 1)
                    {
                        transcode.isPALProject = true;
                    }
                    else
                    {
                        transcode.isPALProject = false;
                    }
                    break;
                }
            }
        }
        else
        {
            // get image data
            // TODO: MXFWriter should be updated to read this data itself
            audioQuantizationBits = fileDescriptor->audioQuantizationBits;
        }
        
        // find the track in the material package and modify the source clip
        // also set the materialTrackID for the stream
        vector<prodauto::Track*>::const_iterator iter3;
        for (iter3 = pa_materialPackage->tracks.begin(); iter3 != pa_materialPackage->tracks.end(); iter3++)
        {
            prodauto::Track* track = *iter3;
            
            if (track->sourceClip != 0 && track->sourceClip->sourcePackageUID == filePackage->uid)
            {
                track->sourceClip->sourcePackageUID = newFilePackageUID;
                transcode.streams[streamIndex].materialTrackID = track->id;
            }
        }

        // modify package id and recuresively set objects as not loaded
        filePackage->cloneInPlace(newFilePackageUID, true); 
    }
    
    
    try
    {
        // create mxf writer
    
        // TODO: MXFWriter should extract videoResolution, imageAspectRatio, audioQuantizationBits, startPosition
        // from the file packages.
        transcode.mxfWriter = new prodauto::MXFWriter(pa_materialPackage, 
            pa_filePackages,
            pa_tapePackage, 
            false, 
            transcode.isPALProject,
            videoResolutionID,
            imageAspectRatio,
            audioQuantizationBits,
            startPosition,
            creatingDirectory,
            destinationDirectory,
            failureDirectory,
            outputPrefix,
            pa_materialPackage->getUserComments(),
            pa_materialPackage->projectName); 

            
        // initialise transcode
        
        transcode.numStreams = (int)inputFiles.size();
        for (i = 0; i < transcode.numStreams; i++)
        {
            TranscodeStream* stream = &transcode.streams[i];
            
            if (stream->requireTranscode)
            {
                CHK_OFAIL(initialise_transcode(&transcode, stream, numFFMPEGThreads));
            }
        }

        // initialise mjpeg library
        // TODO: hardcoded WxH and assuming DV50 height
        CHK_OFAIL(transcode.mjpeg.initialise(720, (transcode.isPALProject ? 576 : 480)));
        
        
        
        // open input files
        
        vector<string>::const_iterator iter4;
        for (i = 0, iter4 = inputFiles.begin(); iter4 != inputFiles.end(); i++, iter4++)
        {
            string inputFile = *iter4;
            TranscodeStream* stream = &transcode.streams[i];

            // open reader
            CHK_OFAIL(mxfr::open_mxf_reader(inputFile.c_str(), &stream->reader));
            
            stream->listenerData.transcode = &transcode;
            stream->listenerData.streamIndex = i;
            stream->listener.data = &transcode.streams[i].listenerData;
            stream->listener.accept_frame = accept_frame;
            stream->listener.allocate_buffer = allocate_buffer;
            stream->listener.deallocate_buffer = deallocate_buffer;
            stream->listener.receive_frame = receive_frame;
        }

        
        // transcode
    
        eof = 0;
        while (!eof)
        {
            for (i = 0; i < transcode.numStreams; i++)
            {
                TranscodeStream* stream = &transcode.streams[i];

                CHK_OFAIL(result = mxfr::read_next_frame(stream->reader, &stream->listener));
                if (result == -1)
                {
                    eof = 1;
                }
            }
        }
    
        
        // complete writing
        
        transcode.mxfWriter->completeAndSaveToDatabase();
    }
    catch (...)
    {
        goto fail;
    }
    
    // close
    
    delete transcode.mxfWriter;
    for (i = 0; i < transcode.numStreams; i++)
    {
        mxfr::close_mxf_reader(&transcode.streams[i].reader);
        SAFE_FREE(&transcode.streams[i].inputBuffer);
        free_transcode(&transcode.streams[i]);
    }
    
    return 1;

    
fail:
    delete transcode.mxfWriter;
    for (i = 0; i < transcode.numStreams; i++)
    {
        mxfr::close_mxf_reader(&transcode.streams[i].reader);
        SAFE_FREE(&transcode.streams[i].inputBuffer);
        free_transcode(&transcode.streams[i]);
    }
    return 0;
}

// assume filename is 'xxxxx' + '_' + card num + '_' + 'A'/'V' + track num + '.mxf'
static string get_output_prefix(string filename)
{
    string prefix = "20to1_";
    size_t mxfSuffix = filename.rfind(".mxf");
    size_t underscore = filename.rfind("_");
    
    if (underscore != string::npos && mxfSuffix != string::npos)
    {
        return prefix.append(filename.substr(0, underscore));
    }
    else
    {
        // filename is not in expected format - use a timestamp string
        prodauto::Logging::warning("Filename ('%d;) is not in expected format", filename.c_str());
        
        prodauto::Timestamp now = prodauto::generateTimestampNow();
        return prefix.append(getTimestampString(now));
    }
}

static string strip_path(string filename)
{
    size_t sepIndex;
    if ((sepIndex = filename.rfind("/")) != string::npos)
    {
        return filename.substr(sepIndex + 1);
    }
    return filename;
}

static string join_path(string path1, string path2)
{
    string result;
    
    result = path1;
    if (path1.size() > 0 && path1.at(path1.size() - 1) != '/' && 
        (path2.size() == 0 || path2.at(0) != '/'))
    {
        result.append("/");
    }
    result.append(path2);
    
    return result;
}

static string join_path(string path1, string path2, string path3)
{
    string result;
    
    result = join_path(path1, path2);
    result = join_path(result, path3);

    return result;
}

static bool check_file_exists(string filename)
{
    FILE* file;
    if ((file = fopen(filename.c_str(), "rb")) == NULL)
    {
        return false;
    }
    else
    {
        fclose(file);
        return true;
    }
}

static bool check_directory_exists(string directory)
{
    struct stat statBuf;
    return stat(directory.c_str(), &statBuf) == 0 && S_ISDIR(statBuf.st_mode);
}

// a date is assumed to be a sequence of 8 digits in the filename
static string parse_date_from_filename(string filename)
{
    unsigned int startIndex = 0;
    unsigned int count = 0;
    while (startIndex + count < filename.size())
    {
        if (filename[startIndex + count] >= '0' && filename[startIndex + count] <= '9')
        {
            count++;
            if (count >= 8)
            {
                return filename.substr(startIndex, 8);
            }
        }
        else
        {
            startIndex += count + 1;
            count = 0;
        }
    }
    
    return "";
}



static void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s <<options>>\n", cmd);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options: (options marked with * are required)\n");
    fprintf(stderr, "  -h, --help               display this usage message\n");
    fprintf(stderr, "  --source-dir <path>      path to input files directory (default .)\n");
    fprintf(stderr, "  --multiple-transcoder    all 'started' transcodes will NOT be reset to 'not started'\n");
    fprintf(stderr, "  --loop                   loop indefinitely or until an exception occurs\n");
    fprintf(stderr, "  --prefix <filename>      output filename prefix (default is to derive prefix from inputs)\n");
    fprintf(stderr, "  --create-dir <path>      path to creating directory (default .)\n");
    fprintf(stderr, "  --dest-dir <path>        path to destination directory (default .)\n");
    fprintf(stderr, "  --fail-dir <path>        path to failure directory (default .)\n");
    fprintf(stderr, "  --input-mjpeg-21         transcode from Avid MJPEG 2:1 (default)\n");
    fprintf(stderr, "  --input-dv-50            transcode from DV 50\n");
    fprintf(stderr, "  --output-mjpeg-201       transcode to Avid MJPEG 20:1 (default)\n");
    fprintf(stderr, "  --fthreads               number of FFMPEG threads (default = 4)\n");
    fprintf(stderr, "  --dsn <string>           database DNS (default '%s')\n", g_dsn);
    fprintf(stderr, "  --db-user <string>       database user name (default '%s')\n", g_databaseUserName);
    fprintf(stderr, "  --db-password <string>   database user password (default ***)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Press 'q' followed by <ENTER> to quit.\n");
}


int main(int argc, const char* argv[])
{
    int cmdlnIndex = 1;
    string outputPrefix;
    int inputVideoResolutionID = MJPEG21_MATERIAL_RESOLUTION;
    int outputVideoResolutionID = MJPEG201_MATERIAL_RESOLUTION;
    int numFFMPEGThreads = 4;
    string sourceDirectory;
    string creatingDirectory;
    string destinationDirectory;
    string failureDirectory;
    string dsn = g_dsn;
    string dbUserName = g_databaseUserName;
    string dbPassword = g_databasePassword;
    bool loop = false;
    bool multipleTranscoder = false;
    DatabaseHolder database;
    bool quit = false;
    MJPEGCompressHolder mjpeg;
    Decoder decoder;
    bool haveTranscoded;

    
    // read options 
    while (cmdlnIndex < argc)
    {
        if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
            strcmp(argv[cmdlnIndex], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdlnIndex], "--source-dir") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            sourceDirectory = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--multiple-transcoder") == 0)
        {
            multipleTranscoder = true;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--loop") == 0)
        {
            loop = true;
            cmdlnIndex += 1;
        }
        else if (strcmp(argv[cmdlnIndex], "--prefix") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            outputPrefix = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--create-dir") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            creatingDirectory = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--dest-dir") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            destinationDirectory = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--fail-dir") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            failureDirectory = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--input-mjpeg-21") == 0)
        {
            inputVideoResolutionID = MJPEG21_MATERIAL_RESOLUTION;
            cmdlnIndex++;
        }
        else if (strcmp(argv[cmdlnIndex], "--input-dv-50") == 0)
        {
            inputVideoResolutionID = DV50_MATERIAL_RESOLUTION;
            cmdlnIndex++;
        }
        else if (strcmp(argv[cmdlnIndex], "--output-mjpeg-201") == 0)
        {
            outputVideoResolutionID = MJPEG201_MATERIAL_RESOLUTION;
            cmdlnIndex++;
        }
        else if (strcmp(argv[cmdlnIndex], "--fthreads") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d\n", &numFFMPEGThreads) != 1 ||
                numFFMPEGThreads < 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--dsn") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            dsn = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--db-user") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            dbUserName = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--db-password") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            dbPassword = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else
        {
            usage(argv[0]);
            fprintf(stderr, "Unknown argument '%s'\n", argv[cmdlnIndex]);
            return 1;
        }
    }
    
    // check the source, creating and failures directories exist
    // the destination directory is created later on if it does not exist
    if (sourceDirectory.size() > 0 && !check_directory_exists(sourceDirectory))
    {
        fprintf(stderr, "Source directory '%s' does not exist\n", sourceDirectory.c_str());
        return 1;
    }
    if (creatingDirectory.size() > 0 && !check_directory_exists(creatingDirectory))
    {
        fprintf(stderr, "Creating directory '%s' does not exist\n", creatingDirectory.c_str());
        return 1;
    }
    if (failureDirectory.size() > 0 && !check_directory_exists(failureDirectory))
    {
        fprintf(stderr, "Failure directory '%s' does not exist\n", failureDirectory.c_str());
        return 1;
    }
    
    

    printf("Press 'q' followed by <ENTER> to quit\n");
    

    // initialise ffmpeg
    av_register_all();

    
    // set logging
    prodauto::Logging::initialise(prodauto::LOG_LEVEL_DEBUG);
    
    // connect the MXF logging facility
    prodauto::connectLibMXFLogging();

    
    // initialise the database
    
    bool haveDatabaseConnection = false;
    do
    {
        while (!haveDatabaseConnection)
        {
            try
            {
                prodauto::Logging::info("Initialising database connection...\n");
                database.initialise(dsn, dbUserName, dbPassword, 1, 3);
                prodauto::Logging::info("Initialised connection to database\n");
                haveDatabaseConnection = true;
            }
            catch (const prodauto::DBException& ex)
            {
                prodauto::Logging::error("Failed to connect to database:\n  %s\n", ex.getMessage().c_str());
            }

            if (haveDatabaseConnection)
            {
                if (!multipleTranscoder)
                {
                    // change any transcodes in "STARTED" state to "NOT STARTED" state
                    try
                    {
                        int numUpdates = database.instance()->resetTranscodeStatus(TRANSCODE_STATUS_STARTED, TRANSCODE_STATUS_NOTSTARTED);
                        if (numUpdates > 0)
                        {
                            prodauto::Logging::info("Updated %d transcode entries from 'STARTED' to 'NOT STARTED'\n",
                                numUpdates);
                        }
                    }
                    catch (...)
                    {
                        prodauto::Logging::error("Failed to reset transcode statuses\n");
                    }
                }
            }
            
            CHECK_USER_QUIT(quit, break);
            sleep(10);
        }
        
        if (quit)
        {
            break;
        }

        
        try
        {
            haveTranscoded = false;
    
            // load the transcodes from the database
            vector<prodauto::Transcode*> transcodes;
            try
            {
                transcodes = database.instance()->loadTranscodes(TRANSCODE_STATUS_NOTSTARTED);
            }
            catch (const prodauto::DBException& ex)
            {
                prodauto::Logging::error("Failed to load transcodes from the database:\n  %s\n", ex.getMessage().c_str());
                throw "Database problem";
            }
    
            if (transcodes.size() > 0)
            {
                prodauto::Logging::info("\nLoaded %d transcodes\n", transcodes.size());
            }
            
            // process each transcode
            size_t index;
            vector<prodauto::Transcode*>::const_iterator iter;
            for (iter = transcodes.begin(), index = 0; quit == false && iter != transcodes.end(); iter++, index++)
            {
                prodauto::Transcode* transcode = *iter;
                
                CHECK_USER_QUIT(quit, break);
                
                prodauto::Logging::info("Processing transcode %d...\n", index);
                
                // load the source material
                MaterialHolder sourceMaterial;
                try
                {
                    database.instance()->loadPackageChain(transcode->sourceMaterialPackageDbId, 
                        &sourceMaterial.sourceMaterialPackage, &sourceMaterial.packages);
                }
                catch (const prodauto::DBException& ex)
                {
                    prodauto::Logging::error("Failed to load package chain for transcode source material:\n  %s\n", ex.getMessage().c_str());
                    throw "Database problem";
                }
        
                CHECK_USER_QUIT(quit, break);
                
                // check the source material is supported for transcode
                // TODO: check that audio is also ok (ie PCM )
                
                bool supported = true;
                bool haveVideo = false;
                prodauto::PackageSet::iterator iter2;
                for (iter2 = sourceMaterial.packages.begin(); iter2 != sourceMaterial.packages.end(); iter2++)
                {
                    prodauto::Package* package = *iter2;
                    
                    if (package->getType() != prodauto::SOURCE_PACKAGE)
                    {
                        continue;
                    }
                    prodauto::SourcePackage* sourcePackage = dynamic_cast<prodauto::SourcePackage*>(package);
                    
                    if (sourcePackage->descriptor->getType() != FILE_ESSENCE_DESC_TYPE)
                    {
                        continue;
                    }
                    prodauto::FileEssenceDescriptor* fileDescriptor = dynamic_cast<prodauto::FileEssenceDescriptor*>(sourcePackage->descriptor);
                    
                    if (fileDescriptor->videoResolutionID != 0)
                    {
                        haveVideo = true;
                        if (fileDescriptor->videoResolutionID != inputVideoResolutionID)
                        {
                            supported = false;
                            break;
                        }
                    }
                    else if (fileDescriptor->fileLocation.size() == 0)
                    {
                        // need a file location
                        supported = false;
                    }
                }
                
                // update the transcode status if no video or not supported
                if (!haveVideo || !supported)
                {
                    if (!haveVideo)
                    {
                        transcode->status = TRANSCODE_STATUS_NOTFORTRANSCODE;
                    }
                    else // !supported
                    {
                        transcode->status = TRANSCODE_STATUS_NOTSUPPORTED;
                    }
                    
                    try
                    {
                        database.instance()->saveTranscode(transcode);
                        if (!haveVideo)
                        {
                            prodauto::Logging::info("  Update transcode status to NOT FOR TRANSCODE\n");
                        }
                        else
                        {
                            prodauto::Logging::info("  Update transcode status to NOT SUPPORTED\n");
                        }
                    }
                    catch (const prodauto::DBException& ex)
                    {
                        prodauto::Logging::error("Failed to update transcode:\n  %s\n", ex.getMessage().c_str());
                        throw "Database problem";
                    }
                    
                    if (!haveVideo)
                    {
                        prodauto::Logging::info("  Skipping because no video is present\n");
                    }
                    else
                    {
                        prodauto::Logging::info("  Skipping because video is not supported for transcode\n");
                    }
        
                    // process the next one
                    continue;
                }
                
                
                CHECK_USER_QUIT(quit, break);
                
                // try find the input files
                
                outputPrefix = "";
                string actualDestinationDirectory;
                bool haveAllInputFiles = true;
                vector<string> inputFiles;
                for (iter2 = sourceMaterial.packages.begin(); quit == false && iter2 != sourceMaterial.packages.end(); iter2++)
                {
                    prodauto::Package* package = *iter2;
                    
                    CHECK_USER_QUIT(quit, break);
                
                    if (package->getType() != prodauto::SOURCE_PACKAGE)
                    {
                        continue;
                    }
                    prodauto::SourcePackage* sourcePackage = dynamic_cast<prodauto::SourcePackage*>(package);
                    
                    if (sourcePackage->descriptor->getType() != FILE_ESSENCE_DESC_TYPE)
                    {
                        continue;
                    }
                    prodauto::FileEssenceDescriptor* fileDescriptor = dynamic_cast<prodauto::FileEssenceDescriptor*>(sourcePackage->descriptor);
                    
                    
                    // the file could be in the sourceDirectory or in a sub-directory of the sourceDirectory
                    // with name equal to the date string in the filename
                    
                    string filename = strip_path(fileDescriptor->fileLocation);
                    string filePath = join_path(sourceDirectory, filename);
                    string dateSubdir = parse_date_from_filename(filename);
     
                    bool fileExistsInSourceDir = 
                        check_file_exists(join_path(sourceDirectory, filename));
                    bool fileExistsInSourceSubDir = !dateSubdir.empty() &&
                        check_file_exists(join_path(sourceDirectory, dateSubdir, filename));
                        
                    if (!fileExistsInSourceDir && !fileExistsInSourceSubDir)
                    {
                        haveAllInputFiles = false;
                        break;
                    }
                    else
                    {
                        // construct the source filepath
                        string filePath;
                        if (fileExistsInSourceDir)
                        {
                            filePath = join_path(sourceDirectory, filename);
                        }
                        else // fileExistsInSourceSubDir
                        {
                            filePath = join_path(sourceDirectory, dateSubdir, filename);
                        }
                        
                        // get output prefix and actual destination directory from the first file in the group
                        if (outputPrefix.size() == 0 && inputFiles.size() == 0)
                        {
                            outputPrefix = get_output_prefix(filename);
                            if (fileExistsInSourceDir)
                            {
                                actualDestinationDirectory = destinationDirectory;
                            }
                            else // fileExistsInSourceSubDir
                            {
                                actualDestinationDirectory = join_path(destinationDirectory, dateSubdir);
                            }
                        }
                        
                        // found it
                        inputFiles.push_back(filePath);
                        sourceMaterial.packageToFilepathMap.insert(pair<prodauto::UMID, string>(package->uid, filePath));
                    }
                }
        
                if (!haveAllInputFiles)
                {
                    // skip and process the next transcode
                    prodauto::Logging::info("  Failed to find all input files\n");
                    continue;
                }
    
                
                CHECK_USER_QUIT(quit, break);
                
                
                // create the destination directory if it doesn't exist
                if (actualDestinationDirectory.compare(destinationDirectory) != 0 &&
                    !check_directory_exists(actualDestinationDirectory))
                {
                    if (mkdir(actualDestinationDirectory.c_str(), 0777) != 0)
                    {
                        prodauto::Logging::info("  Failed to create destination directory %s\n", actualDestinationDirectory.c_str());
                        throw "Failed to create destination directory";
                    }
                }
                
                // perform transcode
                
                haveTranscoded = true;
                try
                {
                    // update with started status                
                    transcode->status = TRANSCODE_STATUS_STARTED;
                    database.instance()->saveTranscode(transcode);
                    prodauto::Logging::info("  Updated transcode status to STARTED\n");
                    
                    prodauto::Logging::info("  Performing transcode\n");
                    if (transcode_avid_mxf(&decoder, inputFiles, sourceMaterial, 
                        outputPrefix.c_str(), 
                        creatingDirectory.c_str(), actualDestinationDirectory.c_str(), failureDirectory.c_str(),
                        numFFMPEGThreads, inputVideoResolutionID, outputVideoResolutionID))
                    {
                        // update with success status      
                        transcode->status = TRANSCODE_STATUS_COMPLETED;
                        transcode->destMaterialPackageDbId = sourceMaterial.sourceMaterialPackage->getDatabaseID();
                        database.instance()->saveTranscode(transcode);
                        prodauto::Logging::info("  Updated transcode status to COMPLETED\n");
                    }
                    else
                    {
                        // update with failed status                
                        prodauto::Logging::warning("Failed to transcode package '%s'\n", 
                            getUMIDString(sourceMaterial.sourceMaterialPackage->uid).c_str());
                        transcode->status = TRANSCODE_STATUS_FAILED;
                        database.instance()->saveTranscode(transcode);
                        prodauto::Logging::info("  Updated transcode status to FAILED\n");
                    }
                }
                catch (const prodauto::DBException& ex)
                {
                    prodauto::Logging::error("Failed to transcode:\n  %s\n", ex.getMessage().c_str());
                    try
                    {
                        transcode->status = TRANSCODE_STATUS_FAILED;
                        database.instance()->saveTranscode(transcode);
                        prodauto::Logging::info("  Updated transcode status to FAILED\n");
                    }
                    catch (const prodauto::DBException& ex)
                    {
                        prodauto::Logging::error("Failed to update transcode status to failed:\n  %s\n", ex.getMessage().c_str());
                    }
                    catch (...)
                    {
                        prodauto::Logging::error("Failed to update transcode status to failed\n");
                    }
                    throw "Database problem";
                }
                catch (...)
                {
                    prodauto::Logging::error("Failed to transcode\n");
                    try
                    {
                        transcode->status = TRANSCODE_STATUS_FAILED;
                        database.instance()->saveTranscode(transcode);
                        prodauto::Logging::info("  Updated transcode status to FAILED\n");
                    }
                    catch (const prodauto::DBException& ex)
                    {
                        prodauto::Logging::error("Failed to update transcode status to failed:\n  %s\n", ex.getMessage().c_str());
                    }
                    catch (...)
                    {
                        prodauto::Logging::error("Failed to update transcode status to failed\n");
                    }
                    throw "Database problem";
                }
                
                
            }
    
            
            // cleanups
            try
            {
                for (iter = transcodes.begin(); iter != transcodes.end(); iter++)
                {
                    delete *iter;
                }
                transcodes.clear();
                
    
                // delete skipped transcode entries 
                prodauto::Interval zeroTime = {0, 0, 0, 0, 0, 0, 0};
                vector<int> statuses;
                statuses.push_back(TRANSCODE_STATUS_NOTFORTRANSCODE);
                statuses.push_back(TRANSCODE_STATUS_NOTSUPPORTED);
                int numDeletes = database.instance()->deleteTranscodes(statuses, zeroTime);
                if (numDeletes > 0)
                {
                    prodauto::Logging::info("Deleted %d skipped transcode entries from database\n",
                        numDeletes);
                }
                
                // delete completed transcode
                statuses.clear();
                statuses.push_back(TRANSCODE_STATUS_COMPLETED);
                numDeletes = database.instance()->deleteTranscodes(statuses, completedDeleteTime);
                if (numDeletes > 0)
                {
                    prodauto::Logging::info("Deleted %d completed transcode entries (created '%s' before now) from database\n", 
                        numDeletes, getODBCInterval(completedDeleteTime).c_str());
                }
                
                // delete failed transcode entries
                statuses.clear();
                statuses.push_back(TRANSCODE_STATUS_FAILED);
                numDeletes = database.instance()->deleteTranscodes(statuses, failedDeleteTime);
                if (numDeletes > 0)
                {
                    prodauto::Logging::info("Deleted %d failed transcode entries (created'%s' before now) from database\n",
                        numDeletes, getODBCInterval(failedDeleteTime).c_str());
                }
            }
            catch (...)
            {
                prodauto::Logging::error("Failed to do transcode table cleanups\n");
                throw "Database problem";
            }
    
            if (haveTranscoded)
            {
                prodauto::Logging::info("Completed processing\n");
            }
            else if (loop)
            {
                CHECK_USER_QUIT(quit, break);
                
                printf("."); fflush(stdout);
                sleep(10);
            }
        }
        catch (...)
        {
            try
            {
                prodauto::Logging::info("Exception thrown - closing the database connection and restarting\n");
                database.close();
                haveDatabaseConnection = false;
            }
            catch (...)
            {}
        }

        CHECK_USER_QUIT(quit, break);
            
    } while (loop && !quit);
    

    return 1;
}

