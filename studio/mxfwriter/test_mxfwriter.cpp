/*
 * $Id: test_mxfwriter.cpp,v 1.4 2008/09/03 15:23:32 john_f Exp $
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
} RecordData;

void* start_record_routine(void* data)
{
    RecordData* recordData = (RecordData*)data;
    RecorderInputConfig* inputConfig = recordData->recorder->getConfig()->getInputConfig(recordData->inputIndex);
    int resolutionId;
    unsigned int videoFrameSize;
    unsigned char videoData[720*576*2];

    if (recordData->dv50Filename.size() > 0)
    {
        // dv50 input
        resolutionId = DV50_MATERIAL_RESOLUTION;
        videoFrameSize = 288000;
        FILE* file;
        if ((file = fopen(recordData->dv50Filename.c_str(), "r")) == NULL)
        {
            perror("fopen");
            pthread_exit((void *) 0);
            return NULL;
        }
        if (fread(videoData, videoFrameSize, 1, file) != 1)
        {
            fprintf(stderr, "Failed to read DV 50 frame\n");
            fclose(file);
            pthread_exit((void *) 0);
            return NULL;
        }
        fclose(file);
    }
    else
    {
        // dummy essence data
        resolutionId = UNC_MATERIAL_RESOLUTION;
        videoFrameSize = 720 * 576 * 2;
        memset(videoData, 0, videoFrameSize);
    }
    
    uint8_t pcmData[1920 * 2]; // 16-bit
    uint32_t pcmFrameSize = 1920;
    memset(pcmData, 0, pcmFrameSize * 2);
    
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
                        // write uncompressed video data
                        writer->writeSample(trackIndex, 1, videoData, videoFrameSize);
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
    fprintf(stderr, "    -h | --help          Show this usage message and exit\n");
    fprintf(stderr, "    --dv50 <filename>    Essence file to read and wrap in MXF (default is blank uncompressed)\n");
    fprintf(stderr, "    --old-session\n");
    fprintf(stderr, "    -r <recorder name>   Recorder name to use to connect to database (default '%s')\n", g_defaultRecorderName);
    fprintf(stderr, "    -t <prefix>          The tape number prefix (default '%s')\n", g_defaultTapeNumberPrefix);
    fprintf(stderr, "    -s <timecode>        The start timecode. Format is hh:mm:ss:ff (default '%s')\n", g_defaultStartTimecode);
}


int main(int argc, const char* argv[])
{
    int j;
    int k;
    const char* dv50Filename = NULL;
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

