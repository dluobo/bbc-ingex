/*
 * $Id: test_mxfwriter.cpp,v 1.5 2008/10/29 17:54:26 john_f Exp $
 *
 * Tests the MXF writer
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <pthread.h>

#include <sstream>


#include "MXFWriter.h"
#include "MXFWriterException.h"
#include "MXFUtils.h"
#include <Database.h>
#include <Recorder.h>
#include <Utilities.h>
#include <DBException.h>
#include <Logging.h>


using namespace std;
using namespace prodauto;


//#define TEST_WRITE_SAMPLE_DATA      1


#define NUM_RECORD_THREADS          4
#define NUM_FRAMES                  15
#define NUM_RECORDS                 1

#define NUM_RECORDER_INPUTS         4

//#define CREATING_FILE_PATH          "creating"
//#define DESTINATION_FILE_PATH       "destination"
//#define FAILURES_FILE_PATH          "failures"
#define CREATING_FILE_PATH          ""
#define DESTINATION_FILE_PATH       ""
#define FAILURES_FILE_PATH          ""

static const char* g_defaultRecorderName = "Ingex";
static const char* g_defaultTapeNumberPrefix = "DPP";
static const char* g_defaultStartTimecode = "10:00:00:00";
static int64_t g_defaultStartPosition = 10 * 60 * 60 * 25;


typedef struct
{
    unsigned char* buffer;
    long bufferSize;
    long position;
    long prevPosition;
    long dataSize;
    
    int endOfField;
    int field2;
    long skipCount;
    int haveLenByte1;
    int haveLenByte2;
    // states
    // 0 = search for 0xFF
    // 1 = test for 0xD8 (start of image)
    // 2 = search for 0xFF - start of marker
    // 3 = test for 0xD9 (end of image), else skip
    // 4 = skip marker segment data
    //
    // transitions
    // 0 -> 1 (data == 0xFF)
    // 1 -> 0 (data != 0xD8 && data != 0xFF)
    // 1 -> 2 (data == 0xD8)
    // 2 -> 3 (data == 0xFF)
    // 3 -> 0 (data == 0xD9)
    // 3 -> 2 (data >= 0xD0 && data <= 0xD7 || data == 0x01 || data == 0x00)
    // 3 -> 4 (else and data != 0xFF)
    int markerState;
    
} MJPEGState;

typedef struct
{
    int64_t startPosition;
    string creatingFilePath;
    string destinationFilePath;
    string failuresFilePath;
    string filenamePrefix;
    Recorder* recorder; 
    int32_t inputIndex;
    vector<UserComment> userComments;
    ProjectName projectName;
    string dv50Filename;
    string mjpeg21Filename;
} RecordData;




/* TODO: have a problem if start-of-frame marker not found directly after end-of-frame */
static int read_next_mjpeg_image_data(FILE* file, MJPEGState* state, 
    unsigned char** dataOut, long* dataOutSize, int* haveImage)
{
    *haveImage = 0;
    
    if (state->position >= state->dataSize)
    {
        if (state->dataSize < state->bufferSize)
        {
            /* EOF if previous read was less than capacity of buffer */
            return 0;
        }
        
        if ((state->dataSize = (long)fread(state->buffer, 1, state->bufferSize, file)) == 0)
        {
            /* EOF if nothing was read */
            return 0;
        }
        state->prevPosition = 0;
        state->position = 0;
    }
    
    /* locate start and end of image */
    while (!(*haveImage) && state->position < state->dataSize)
    {
        switch (state->markerState)
        {
            case 0:
                if (state->buffer[state->position] == 0xFF)
                {
                    state->markerState = 1;
                }
                else
                {
                    fprintf(stderr, "Error: image start is non-0xFF byte\n");
                    return 0;
                }
                break;
            case 1:
                if (state->buffer[state->position] == 0xD8) /* start of frame */
                {
                    state->markerState = 2;
                }
                else if (state->buffer[state->position] != 0xFF) /* 0xFF is fill byte */
                {
                    state->markerState = 0;
                }
                break;
            case 2:
                if (state->buffer[state->position] == 0xFF)
                {
                    state->markerState = 3;
                }
                /* else wait here */
                break;
            case 3:
                if (state->buffer[state->position] == 0xD9) /* end of field */
                {
                    state->markerState = 0;
                    state->endOfField = 1;
                }
                /* 0xD0-0xD7 and 0x01 are empty markers and 0x00 is stuffed zero */
                else if ((state->buffer[state->position] >= 0xD0 && state->buffer[state->position] <= 0xD7) ||
                        state->buffer[state->position] == 0x01 || 
                        state->buffer[state->position] == 0x00)
                {
                    state->markerState = 2;
                }
                else if (state->buffer[state->position] != 0xFF) /* 0xFF is fill byte */
                {
                    state->markerState = 4;
                    /* initialise for state 4 */
                    state->haveLenByte1 = 0;
                    state->haveLenByte2 = 0;
                    state->skipCount = 0;
                }
                break;
            case 4:
                if (!state->haveLenByte1)
                {
                    state->haveLenByte1 = 1;
                    state->skipCount = state->buffer[state->position] << 8;
                }
                else if (!state->haveLenByte2)
                {
                    state->haveLenByte2 = 1;
                    state->skipCount += state->buffer[state->position];
                    state->skipCount -= 1; /* length includes the 2 length bytes, one subtracted here and one below */
                }

                if (state->haveLenByte1 && state->haveLenByte2)
                {
                    state->skipCount--;
                    if (state->skipCount == 0)
                    {
                        state->markerState = 2;
                    }
                }
                break;
            default:
                assert(0);
                return 0;
                break;
        }
        state->position++;
        
        if (state->endOfField)
        {
            if (state->field2)
            {
                *haveImage = 1;
            }
            state->endOfField = 0;
            state->field2 = !state->field2;
        }
    }
    
    *dataOut = &state->buffer[state->prevPosition];
    *dataOutSize = state->position - state->prevPosition;
    state->prevPosition = state->position;
    return 1;
}



void* start_record_routine(void* data)
{
    RecordData* recordData = (RecordData*)data;
    RecorderInputConfig* inputConfig = recordData->recorder->getConfig()->getInputConfig(recordData->inputIndex);
    int resolutionId;
    unsigned int videoFrame1Size;
    unsigned char videoData1[720*576*2];
    unsigned int videoFrame2Size;
    unsigned char videoData2[720*576*2];
    int haveImage1;
    int haveImage2;
    MJPEGState mjpegState;
    unsigned char* mjpegData;
    long mjpegDataSize;

    if (recordData->dv50Filename.size() > 0)
    {
        // dv50 input
        resolutionId = DV50_MATERIAL_RESOLUTION;
        videoFrame1Size = 288000;
        FILE* file;
        if ((file = fopen(recordData->dv50Filename.c_str(), "r")) == NULL)
        {
            perror("fopen");
            pthread_exit((void *) 0);
            return NULL;
        }
        if (fread(videoData1, videoFrame1Size, 1, file) != 1)
        {
            fprintf(stderr, "Failed to read DV 50 frame\n");
            fclose(file);
            pthread_exit((void *) 0);
            return NULL;
        }
        fclose(file);
        // second frame is a duplicate of the first
        videoFrame2Size = videoFrame1Size;
        memcpy(videoData2, videoData1, videoFrame2Size);
    }
    else if (recordData->mjpeg21Filename.size() > 0)
    {
        // MJPEG 2:1 input
        resolutionId = MJPEG21_MATERIAL_RESOLUTION;
        FILE* file;
        if ((file = fopen(recordData->mjpeg21Filename.c_str(), "r")) == NULL)
        {
            perror("fopen");
            pthread_exit((void *) 0);
            return NULL;
        }

        memset(&mjpegState, 0, sizeof(mjpegState));
        mjpegState.buffer = (unsigned char*)malloc(8192);
        mjpegState.bufferSize = 8192;
        mjpegState.dataSize = mjpegState.bufferSize;
        mjpegState.position = mjpegState.bufferSize;
        mjpegState.prevPosition = mjpegState.bufferSize;

        haveImage1 = 0;
        haveImage2 = 0;
        videoFrame1Size = 0;        
        videoFrame2Size = 0;        
        while (!haveImage1 || !haveImage2)
        {
            if (!haveImage1)
            {
                if (!read_next_mjpeg_image_data(file, &mjpegState, &mjpegData, &mjpegDataSize, &haveImage1))
                {
                    fprintf(stderr, "Failed to read MJPEG 2:1 frame\n");
                    free(mjpegState.buffer);
                    fclose(file);
                    pthread_exit((void *) 0);
                    return NULL;
                }
                
                memcpy(&videoData1[videoFrame1Size], mjpegData, mjpegDataSize);
                videoFrame1Size += (int)mjpegDataSize;
            }
            else
            {
                if (!read_next_mjpeg_image_data(file, &mjpegState, &mjpegData, &mjpegDataSize, &haveImage2))
                {
                    fprintf(stderr, "Failed to read MJPEG 2:1 frame\n");
                    free(mjpegState.buffer);
                    fclose(file);
                    pthread_exit((void *) 0);
                    return NULL;
                }
                
                memcpy(&videoData2[videoFrame2Size], mjpegData, mjpegDataSize);
                videoFrame2Size += (int)mjpegDataSize;
            }
        }
        free(mjpegState.buffer);
        fclose(file);
    }
    else
    {
        // dummy essence data
        resolutionId = UNC_MATERIAL_RESOLUTION;
        videoFrame1Size = 720 * 576 * 2;
        memset(videoData1, 0, videoFrame1Size);
        videoFrame2Size = videoFrame1Size;
        memset(videoData2, 0, videoFrame2Size);
    }
    
    uint8_t pcmData[1920 * 2]; // 16-bit
    uint32_t pcmFrameSize = 1920;
    memset(pcmData, 1, pcmFrameSize * 2);
    
    printf("Starting... (%s)\n", recordData->filenamePrefix.c_str());
    
    try
    {
        // create writer for input (mask all except track 1,2,3)
        Rational imageAspectRatio = {16, 9};
        auto_ptr<MXFWriter> writer(new MXFWriter(
            recordData->recorder->getConfig()->getInputConfig(recordData->inputIndex),
            resolutionId, imageAspectRatio,
            16, 0xffffffff, recordData->startPosition, recordData->creatingFilePath,
            recordData->destinationFilePath, recordData->failuresFilePath,
            recordData->filenamePrefix, recordData->userComments, 
            recordData->projectName));
    
        int i;
        int trackIndex;
        
#if defined(TEST_WRITE_SAMPLE_DATA)
        // start the sample data for PCM
        vector<RecorderInputTrackConfig*>::const_iterator trackIter;
        for (trackIndex = 1, trackIter = inputConfig->trackConfigs.begin();
            trackIter != inputConfig->trackConfigs.end();
            trackIndex++, trackIter++)
        {
            RecorderInputTrackConfig* trackConfig = *trackIter;
            
            if (trackConfig->isConnectedToSource())
            {
                SourceTrackConfig* sourceTrackConfig = 
                    trackConfig->sourceConfig->getTrackConfig(trackConfig->sourceTrackID);
                if (sourceTrackConfig->dataDef == SOUND_DATA_DEFINITION)
                {
                    // start write pcm audio data
                    writer->startSampleData(trackIndex);
                }
            }
        }
#endif
        // write NUM_FRAMES of uncompressed video and audio
        for (i = 0; i < NUM_FRAMES; i++)
        {
            vector<RecorderInputTrackConfig*>::const_iterator trackIter;
            for (trackIndex = 1, trackIter = inputConfig->trackConfigs.begin();
                trackIter != inputConfig->trackConfigs.end();
                trackIndex++, trackIter++)
            {
                RecorderInputTrackConfig* trackConfig = *trackIter;
                
                if (trackConfig->isConnectedToSource())
                {
                    SourceTrackConfig* sourceTrackConfig = 
                        trackConfig->sourceConfig->getTrackConfig(trackConfig->sourceTrackID);
                    if (sourceTrackConfig->dataDef == SOUND_DATA_DEFINITION)
                    {
#if defined(TEST_WRITE_SAMPLE_DATA)
                        // write pcm audio data
                        writer->writeSampleData(trackIndex, pcmData, pcmFrameSize * 2);
#else
                        writer->writeSample(trackIndex, pcmFrameSize, pcmData, pcmFrameSize * 2);
#endif
                    }
                    else // PICTURE_DATA_DEFINITION
                    {
                        // write video data
                        if (i % 2 == 0)
                        {
                            writer->writeSample(trackIndex, 1, videoData1, videoFrame1Size);
                        }
                        else
                        {
                            writer->writeSample(trackIndex, 1, videoData2, videoFrame2Size);
                        }
                    }
                }
            }
    
        }

#if defined(TEST_WRITE_SAMPLE_DATA)
        // end the sample data for PCM
        vector<RecorderInputTrackConfig*>::const_iterator trackIter;
        for (trackIndex = 1, trackIter = inputConfig->trackConfigs.begin();
            trackIter != inputConfig->trackConfigs.end();
            trackIndex++, trackIter++)
        {
            RecorderInputTrackConfig* trackConfig = *trackIter;
            
            if (trackConfig->isConnectedToSource())
            {
                SourceTrackConfig* sourceTrackConfig = 
                    trackConfig->sourceConfig->getTrackConfig(trackConfig->sourceTrackID);
                if (sourceTrackConfig->dataDef == SOUND_DATA_DEFINITION)
                {
                    // end write pcm audio data
                    writer->endSampleData(trackIndex, pcmFrameSize * NUM_FRAMES);
                }
            }
        }
#endif        

        // complete the writing and save packages to database
        writer->completeAndSaveToDatabase();
        
        printf("Files created successfully:");
        for (i = 0; i < 20; i++)
        {
            if (writer->trackIsPresent(i) && writer->wasSuccessfull(i))
            {
                printf("  %s", writer->getFilename(i).c_str());
            }
        }
        printf("\n");
    }
    catch (const ProdAutoException& ex)
    {
        fprintf(stderr, "Failed to record (%s):\n  %s\n", recordData->filenamePrefix.c_str(), 
            ex.getMessage().c_str());
        pthread_exit((void *) 0);
        return NULL;
    }
    catch (...)
    {
        fprintf(stderr, "Unknown exception thrown (%s)\n", recordData->filenamePrefix.c_str());
        pthread_exit((void *) 0);
        return NULL;
    }
    
    printf("Successfully completed and saved (%s)\n", recordData->filenamePrefix.c_str());
    
    pthread_exit((void *) 0);
}


static void usage(const char* prog)
{
    fprintf(stderr, "%s [options] <filename prefix>\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -h | --help            Show this usage message and exit\n");
    fprintf(stderr, "    --dv50 <filename>      DV50 essence file to read and wrap in MXF (default is blank uncompressed)\n");
    fprintf(stderr, "    --mjpeg21 <filename>   MJPEG 2:1 essence file to read and wrap in MXF (default is blank uncompressed)\n");
    fprintf(stderr, "    --old-session\n");
    fprintf(stderr, "    -r <recorder name>     Recorder name to use to connect to database (default '%s')\n", g_defaultRecorderName);
    fprintf(stderr, "    -t <prefix>            The tape number prefix (default '%s')\n", g_defaultTapeNumberPrefix);
    fprintf(stderr, "    -s <timecode>          The start timecode. Format is hh:mm:ss:ff (default '%s')\n", g_defaultStartTimecode);
}


int main(int argc, const char* argv[])
{
    int j;
    int k;
    const char* dv50Filename = NULL;
    const char* mjpeg21Filename = NULL;
    const char* filenamePrefix = NULL;
    const char* recorderName = g_defaultRecorderName;
    bool oldSession = false;
    string tapeNumberPrefix = g_defaultTapeNumberPrefix;
    int64_t startPosition = g_defaultStartPosition;
    int cmdlnIndex = 1;
    int hour, min, sec, frame;
    
    if (argc < 2)
    {
        usage(argv[0]);
        return 1;
    }
    
    if (argc == 2)
    {
        if (strcmp(argv[cmdlnIndex], "--help") == 0 ||
            strcmp(argv[cmdlnIndex], "-h") == 0)
        {
            usage(argv[0]);
            return 0;
        }
    }        
    
    // set logging
    Logging::initialise(LOG_LEVEL_DEBUG);
    
    // connect the MXF logging facility
    connectLibMXFLogging();
    
    while (cmdlnIndex + 1 < argc)
    {
        if (strcmp(argv[cmdlnIndex], "--help") == 0 ||
            strcmp(argv[cmdlnIndex], "-h") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdlnIndex], "--dv50") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing value for argument '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            dv50Filename = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--mjpeg21") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing value for argument '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            mjpeg21Filename = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--old-session") == 0)
        {
            oldSession = true;
            cmdlnIndex++;
        }
        else if (strcmp(argv[cmdlnIndex], "-r") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing value for argument '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            recorderName = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "-t") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing value for argument '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            tapeNumberPrefix = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "-s") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing value for argument '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d:%d:%d:%d", &hour, &min, &sec, &frame) != 4)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid value '%s' for argument '%s'\n", argv[cmdlnIndex + 1], argv[cmdlnIndex]);
                return 1;
            }
            startPosition = hour * 60 * 60 * 25 + min * 60 * 25 + sec * 25 + frame;
            cmdlnIndex += 2;
        }
        else
        {
            usage(argv[0]);
            fprintf(stderr, "Unknown argument '%s'\n", argv[cmdlnIndex]);
            return 1;
        }
    }

    if (cmdlnIndex >= argc)
    {
        usage(argv[0]);
        fprintf(stderr, "Missing filename prefix\n");
        return 1;
    }
    
    filenamePrefix = argv[cmdlnIndex];    


    // initialise the database
    try
    {
        Database::initialise("prodautodb", "bamzooki", "bamzooki", 1, 2 * NUM_RECORD_THREADS);
    }
    catch (const DBException& ex)
    {
        fprintf(stderr, "Failed to connect to database:\n  %s\n", ex.getMessage().c_str());
        return 1;
    }

    // load the recorder configuration
    auto_ptr<Recorder> recorder;
    try
    {
        Database* database = Database::getInstance();
        recorder = auto_ptr<Recorder>(database->loadRecorder(recorderName));
        if (!recorder->hasConfig())
        {
            fprintf(stderr, "Recorder '%s' has null config\n", recorderName);
            Database::close();
            return 1;
        }
    }
    catch (const DBException& ex)
    {
        fprintf(stderr, "Failed to load recorder '%s':\n  %s\n", recorderName, ex.getMessage().c_str());
        Database::close();
        return 1;
    }
    
    
    // save the project name
    ProjectName projectName;
    try
    {
        string name = "testproject";
        projectName = Database::getInstance()->loadOrCreateProjectName(name);
    }
    catch (const DBException& ex)
    {
        fprintf(stderr, "Failed to load recorder '%s':\n  %s\n", recorderName, ex.getMessage().c_str());
        Database::close();
        return 1;
    }


    // load the source session
    try
    {
        if (oldSession)
        {
            // use the source config name + ' - ' + date string as the source package name
            Date now = generateDateNow();
            
            vector<SourceConfig*>::const_iterator iter;
            for (iter = recorder->getConfig()->sourceConfigs.begin(); 
                iter != recorder->getConfig()->sourceConfigs.end();
                iter++)
            {
                SourceConfig* sourceConfig = *iter;
                
                string sourcePackageName = sourceConfig->name;
                sourcePackageName += " - " + getDateString(now);
                
                sourceConfig->setSourcePackage(sourcePackageName);
            }
        }
        else
        {
            // use a spool number for the source package name
            int i;
            char buf[16];
            vector<SourceConfig*>::const_iterator iter;
            for (iter = recorder->getConfig()->sourceConfigs.begin(), i = 0; 
                iter != recorder->getConfig()->sourceConfigs.end();
                iter++, i++)
            {
                SourceConfig* sourceConfig = *iter;
                
                sprintf(buf, "%s%06x", tapeNumberPrefix.c_str(), i);
                
                sourceConfig->setSourcePackage(buf);
            }
        }
    }
    catch (const DBException& ex)
    {
        fprintf(stderr, "Failed to load source session:\n  %s\n", ex.getMessage().c_str());
        Database::close();
        return 1;
    }

    
    RecorderInputConfig* inputConfig[NUM_RECORDER_INPUTS];
    for (k = 0; k < NUM_RECORDER_INPUTS; k++)
    {
        if ((inputConfig[k] = recorder.get()->getConfig()->getInputConfig(k + 1)) == 0)
        {
            fprintf(stderr, "Unknown recorder input config %d\n", k + 1);
            Database::close();
            return 1;
        }
    }
    
    for (j = 0; j < NUM_RECORDS; j++)
    {
        printf("** j == %d\n", j);
        
        try
        {
            pthread_t thread[NUM_RECORD_THREADS];
            RecordData recordData[NUM_RECORD_THREADS];
            pthread_attr_t attr;
            int i, rc, status;
            
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
            
            for (i = 0; i < NUM_RECORD_THREADS; i++)
            {
                stringstream prefix;
                prefix << filenamePrefix << "_" << i; 
                
                recordData[i].startPosition = startPosition;
                recordData[i].creatingFilePath = CREATING_FILE_PATH;
                recordData[i].destinationFilePath = DESTINATION_FILE_PATH;
                recordData[i].failuresFilePath = FAILURES_FILE_PATH;
                recordData[i].filenamePrefix = prefix.str();
                recordData[i].recorder = recorder.get();
                recordData[i].inputIndex = 1 + (i % NUM_RECORDER_INPUTS);
                recordData[i].userComments.push_back(UserComment(AVID_UC_COMMENTS_NAME, 
                    "a test file", STATIC_COMMENT_POSITION, 0));
                recordData[i].userComments.push_back(UserComment(AVID_UC_DESCRIPTION_NAME, 
                    "an mxfwriter test file produced by test_mxfwriter", STATIC_COMMENT_POSITION, 0));
                recordData[i].userComments.push_back(UserComment(POSITIONED_COMMENT_NAME, 
                    "event at position 0", 0, 1));
                recordData[i].userComments.push_back(UserComment(POSITIONED_COMMENT_NAME, 
                    "event at position 1", 1, (i % 8) + 1));
                recordData[i].userComments.push_back(UserComment(POSITIONED_COMMENT_NAME, 
                    "event at position 10000", 10000, 3));
                recordData[i].projectName = projectName;
                
                if (dv50Filename != NULL)
                {
                    recordData[i].dv50Filename = dv50Filename;
                }
                else if (mjpeg21Filename != NULL)
                {
                    recordData[i].mjpeg21Filename = mjpeg21Filename;
                }
                
                if (pthread_create(&thread[i], &attr, start_record_routine, (void *)(&recordData[i])))
                {
                    fprintf(stderr, "Failed to create record thread\n");
                    Database::close();
                    pthread_exit(NULL);
                    return 1;
                }
            }
            
            for (i = 0; i < NUM_RECORD_THREADS; i++)
            {
                rc = pthread_join(thread[i], (void **)&status);
                if (rc)
                {
                    fprintf(stderr, "Return code from pthread_join() is %d\n", rc);
                }
            }
            
            startPosition += NUM_FRAMES;
        }
        catch (...)
        {
            fprintf(stderr, "Unknown exception thrown\n");
            Database::close();
            pthread_exit(NULL);
            return 1;
        }
    
    }
        
    Database::close();
    
    pthread_exit(NULL);
    return 0;
}

