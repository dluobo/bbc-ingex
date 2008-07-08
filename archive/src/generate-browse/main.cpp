/*
 * $Id: main.cpp,v 1.1 2008/07/08 16:24:07 philipn Exp $
 *
 * Generates a MPEG-2 browse copy, timecode and source info file from a D3 MXF file.
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
 
/*
    This utility is used to generate the browse copy file set (MPEG-2 file, 
    timecode text file, and source information text file) from a D3 MXF file. 
    This set of files is the same as that generated at the time of ingest.
*/
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include <string>

#include "D3MXFFile.h"
#include "RecorderException.h"
#include "Logging.h"

#include "browse_encoder.h"
#include "video_conversion.h"


using namespace std;
using namespace rec;


static int g_defaultVideoKBps = 2700;
static int g_defaultThreadCount = 4;




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

static bool directory_exists(string directory)
{
    struct stat statBuf;
    
    if (stat(directory.c_str(), &statBuf) == 0 && S_ISDIR(statBuf.st_mode))
    {
        return true;
    }
    
    return false;
}

static bool file_exists(string filename)
{
    struct stat statBuf;
    if (stat(filename.c_str(), &statBuf) == 0)
    {
        return true;
    }
    
    return false;
}

static string get_date_string(mxfTimestamp input)
{
    char buf[64];
    
    // Infax 'date' time is ignored
    sprintf(buf, "%04d-%02d-%02d", input.year, input.month, input.day);
    
    return buf;
}

static string get_duration_string(int64_t duration)
{
    char buf[64];
    int hour;
    int min;
    int sec;
    int frame;

    buf[0] = '\0';
    
    if (duration >= 0)
    {
        hour = (int)(duration / (60 * 60 * 25));
        min = (int)((duration % (60 * 60 * 25)) / (60 * 25));
        sec = (int)(((duration % (60 * 60 * 25)) % (60 * 25)) / 25);
        frame = (int)(((duration % (60 * 60 * 25)) % (60 * 25)) % 25);
    }
    else
    {
        return "?";
    }
    
    sprintf(buf, "%02d:%02d:%02d:%02d", hour, min, sec, frame);
    
    return buf;
}

static string get_host_name()
{
    char buf[256];
    
    if (gethostname(buf, 256) != 0)
    {
        return "";
    }
    buf[255] = '\0';
    if (buf[0] == '\0')
    {
        return "";
    }
    
    return buf;
}

string get_user_timestamp_string()
{
    time_t t = time(NULL);
    struct tm lt;
    memset(&lt, 0, sizeof(struct tm));
    if (localtime_r(&t, &lt) == NULL)
    {
        Logging::error("Failed to generate timestamp now\n");
        return "";
    }
    
    char buf[128];
    buf[0] = '\0';
    
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", 
        lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
        lt.tm_hour, lt.tm_min, lt.tm_sec);
    
    return buf;
}

static string last_path_component(string filepath)
{
    size_t sepIndex;
    size_t sepIndex2;
    if ((sepIndex = filepath.rfind("/")) != string::npos)
    {
        if (sepIndex + 1 == filepath.size())
        {
            if (sepIndex >= 1 &&
                (sepIndex2 = filepath.rfind("/", sepIndex - 1)) != string::npos)
            {
                return filepath.substr(sepIndex2 + 1, sepIndex - sepIndex2 - 1);
            }
            return filepath.substr(0, sepIndex);
        }
        return filepath.substr(sepIndex + 1);
    }
    return filepath;
}




void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s [options] <mxf filename>\n", cmd);
    fprintf(stderr, "Options: (* means is required)\n");
    fprintf(stderr, "  -h, --help                   Display this usage message\n");
    fprintf(stderr, "* --dest <dir>                 Destination directory\n");
    fprintf(stderr, "  --video <kbps>               Number of kbps for the video (default %d)\n", g_defaultVideoKBps);
    fprintf(stderr, "  --thread <count>             Number of threads to use for encoding (default %d)\n", g_defaultThreadCount);
    fprintf(stderr, "  --log <name>                 Log messages to file <name>\n");
    fprintf(stderr, "\n");
}

int main(int argc, const char** argv)
{
    int cmdlnIndex = 1;
    string destDirectoryName;
    string logFilename;
    string mxfFilename;
    int videoKBps = g_defaultVideoKBps;
    int threadCount = g_defaultThreadCount;
    
    if (argc == 1)
    {
        usage(argv[0]);
        return 0;
    }
    
    while (cmdlnIndex < argc)
    {
        if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
            strcmp(argv[cmdlnIndex], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdlnIndex], "--dest") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            destDirectoryName = argv[cmdlnIndex + 1];
            if (destDirectoryName.empty())
            {
                destDirectoryName = ".";
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--log") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            logFilename = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--video") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d", &videoKBps) != 1 || 
                videoKBps <= 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument '%s' for %s\n", argv[cmdlnIndex + 1], argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--thread") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            if (sscanf(argv[cmdlnIndex + 1], "%d", &threadCount) != 1 || 
                threadCount <= 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument '%s' for %s\n", argv[cmdlnIndex + 1], argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else
        {
            if (cmdlnIndex != argc - 1)
            {
                usage(argv[0]);
                fprintf(stderr, "Unknown option '%s'\n", argv[cmdlnIndex]);
                return 1;
            }
            break;
        }
    }
    
    mxfFilename = argv[cmdlnIndex];
    if (!file_exists(mxfFilename))
    {
        usage(argv[0]);
        fprintf(stderr, "MXF file '%s' does not exist\n", mxfFilename.c_str());
        return 1;
    }

    if (destDirectoryName.empty())
    {
        usage(argv[0]);
        fprintf(stderr, "Missing destination directory\n");
        return 1;
    }
    
    if (!directory_exists(destDirectoryName))
    {
        fprintf(stderr, "Destination directory '%s' does not exist\n", destDirectoryName.c_str());
        return 1;
    }
    
    
    try
    {
        if (logFilename.empty())
        {
            Logging::initialise(LOG_LEVEL_INFO);
        }
        else
        {
            Logging::initialise(new DualLogging(logFilename, LOG_LEVEL_INFO));
            printf("QC report log: %s\n", logFilename.c_str());
        }
    }
    catch (const RecorderException& ex)
    {
        fprintf(stderr, "Failed to initialise logging: %s\n", ex.getMessage().c_str());
        return 1;
    }
    catch (...)
    {
        fprintf(stderr, "Failed to initialise logging: Unknown exception\n");
        return 1;
    }
    

    int16_t* stereoAudio = new int16_t[1920 * 2];
    memset(stereoAudio, 0, 1920 * 2 * sizeof(int16_t));
    
    unsigned char* yuv420Video = new unsigned char[720*576*3/2];
    memset(yuv420Video, 0, 720*576*3/2);
    
    try
    {
        // open the MXF file
        D3MXFFile mxfFile(mxfFilename);
        Logging::info("MXF file: '%s'\n", mxfFilename.c_str());

        int64_t duration = mxfFile.getDuration();
        Logging::info("Duration: '%s'\n", get_duration_string(duration).c_str());
        
        
        // get the output filenames based on the mxfFilename
        string prefix = last_path_component(mxfFilename);
        size_t suffixIndex = prefix.rfind(".mxf");
        if (suffixIndex != string::npos && suffixIndex == prefix.size() - strlen(".mxf"))
        {
            prefix = prefix.substr(0, prefix.size() - strlen(".mxf"));
        }
        string infoFilename = join_path(destDirectoryName, prefix + ".txt");
        string browseFilename = join_path(destDirectoryName, prefix + ".mpg");
        string timecodeFilename = join_path(destDirectoryName, prefix + ".tc");

        Logging::info("Info file: '%s'\n", infoFilename.c_str());
        Logging::info("Browse file: '%s'\n", browseFilename.c_str());
        Logging::info("Timecode file: '%s'\n", timecodeFilename.c_str());
        
        
        // write the info file
        InfaxData* d3InfaxData = mxfFile.getD3InfaxData();
        FILE* infoFile = fopen(infoFilename.c_str(), "wb");
        if (infoFile == 0)
        {
            throw RecorderException("Failed to open the info file '%s': %s", infoFilename.c_str(), strerror(errno));
        }
        
        fprintf(infoFile, "Transfer Information\n\n");
        fprintf(infoFile, "Generated using '%s'\n", argv[0]);
        fprintf(infoFile, "Recorder station: '%s'\n", get_host_name().c_str());
        fprintf(infoFile, "Local time: %s\n", get_user_timestamp_string().c_str());
        
        fprintf(infoFile, "\n\n\nSource Information\n\n");
        fprintf(infoFile, "Format: %s\n", d3InfaxData->format);
        fprintf(infoFile, "Programme Title: %s\n", d3InfaxData->progTitle);
        fprintf(infoFile, "Episode Title: %s\n", d3InfaxData->epTitle);
        fprintf(infoFile, "Tx Date: %s\n", get_date_string(d3InfaxData->txDate).c_str());
        fprintf(infoFile, "Magazine Prefix: %s\n", d3InfaxData->magPrefix);
        fprintf(infoFile, "Programme No.: %s\n", d3InfaxData->progNo);
        fprintf(infoFile, "Production Code: %s\n", d3InfaxData->prodCode);
        fprintf(infoFile, "Spool Status: %s\n", d3InfaxData->spoolStatus);
        fprintf(infoFile, "Stock Date: %s\n", get_date_string(d3InfaxData->stockDate).c_str());
        fprintf(infoFile, "Spool Description: %s\n", d3InfaxData->spoolDesc);
        fprintf(infoFile, "Duration: %s\n", get_duration_string(d3InfaxData->duration * 25).c_str());
        fprintf(infoFile, "Spool No.: %s\n", d3InfaxData->spoolNo);
        fprintf(infoFile, "Accession No.: %s\n", d3InfaxData->accNo);
        fprintf(infoFile, "Catalogue Detail: %s\n", d3InfaxData->catDetail);
        fprintf(infoFile, "Memo: %s\n", d3InfaxData->memo);
        fprintf(infoFile, "Item No.: %d\n", d3InfaxData->itemNo);
        
        fclose(infoFile);

        
        // open the browse encoder
        browse_encoder_t* encoder = browse_encoder_init(browseFilename.c_str(), videoKBps, threadCount);
        if (encoder == 0)
        {
            throw RecorderException("Failed to open the browse encoder");
        }

        // open the timecode file
        FILE* timecodeFile = fopen(timecodeFilename.c_str(), "wb");
        if (timecodeFile == 0)
        {
            throw RecorderException("Failed to open the timecode file '%s': %s", timecodeFilename.c_str(), strerror(errno));
        }
        
        // encode
        ArchiveTimecode ctc;
        int64_t frameNumber = 0;
        D3MXFFrame* frame;
        int64_t percentCount = 0;
        while (mxfFile.nextFrame(frame))
        {
            // convert audio to 16 bit stereo
            if (frame->numAudioTracks > 0)
            {
                uint16_t* outputA12 = (uint16_t*)stereoAudio;
                unsigned char* inputA1 = frame->audio[0];
                unsigned char* inputA2 = frame->audio[1];
                int i;
                if (frame->numAudioTracks > 1)
                {
                    for (i = 0; i < 1920 * 3; i += 3) 
                    {
                        *outputA12++ = (((uint16_t)inputA1[i + 2]) << 8) | inputA1[i + 1];
                        *outputA12++ = (((uint16_t)inputA2[i + 2]) << 8) | inputA2[i + 1];
                    }
                }
                else
                {
                    for (i = 0; i < 1920 * 3; i += 3) 
                    {
                        *outputA12++ = (((uint16_t)inputA1[i + 2]) << 8) | inputA1[i + 1];
                        *outputA12++ = 0;
                    }
                }
            }
            
            // convert video to yuv420    
            uyvy_to_yuv420(720, 576, 0, frame->video, yuv420Video);

            
            // encode
            if (browse_encoder_encode(encoder, yuv420Video, stereoAudio, frame->ltc, frame->vitc, frameNumber) != 0)
            {
                throw RecorderException("Failed to encode frame");
            }

            // write timecode            
            ctc.hour = frameNumber / (60 * 60 * 25);
            ctc.min = (frameNumber % (60 * 60 * 25)) / (60 * 25);
            ctc.sec = ((frameNumber % (60 * 60 * 25)) % (60 * 25)) / 25;
            ctc.frame = ((frameNumber % (60 * 60 * 25)) % (60 * 25)) % 25;
            
            fprintf(timecodeFile, "C%02d:%02d:%02d:%02d V%02d:%02d:%02d:%02d L%02d:%02d:%02d:%02d\n", 
            ctc.hour, ctc.min, ctc.sec, ctc.frame,
            frame->vitc.hour, frame->vitc.min, frame->vitc.sec, frame->vitc.frame,
            frame->ltc.hour, frame->ltc.min, frame->ltc.sec, frame->ltc.frame);
            
            if (frameNumber * 100 / duration > percentCount * 10)
            {
                percentCount++;
                Logging::info("%d%%\n", percentCount * 10);
            }
            frameNumber++;
        }

        // close       
        if (browse_encoder_close(encoder) != 0)
        {
            throw RecorderException("Failed to complete the browse encoding");
        }
        fclose(timecodeFile);
        
        Logging::info("Success\n");
    }
    catch (const RecorderException& ex)
    {
        Logging::error("Exception: %s\n", ex.getMessage().c_str());
        Logging::info("FAILED\n");
        return 1;
    }
    catch (...)
    {
        Logging::error("Caught unknown exception\n");
        Logging::info("FAILED\n");
        return 1;
    }
    
    delete [] stereoAudio;
    delete [] yuv420Video;
    
    return 0;
}


