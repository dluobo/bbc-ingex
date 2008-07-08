/*
 * $Id: main.cpp,v 1.1 2008/07/08 16:25:36 philipn Exp $
 *
 * Recorder application main function
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
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include <curl/curl.h>

#include "logF.h"

#include "Recorder.h"
#include "BarcodeScanner.h"
#include "BarcodeScannerFIFO.h"
#include "HTTPRecorder.h"
#include "HTTPVTRControl.h"
#include "HTTPServer.h"
#include "RecorderException.h"
#include "RecorderDatabase.h"
#include "Logging.h"
#include "Utilities.h"
#include "Config.h"
#include "version.h"


using namespace std;
using namespace rec;


static const char* g_recorderLogPrefix = "recorder_";
static const char* g_recorderLogSuffix = ".log";
static const char* g_captureLogPrefix = "capture_";
static const char* g_captureLogSuffix = ".log";

static const ConfigSpec g_configSpec[] = 
{
    // common
    {"log_dir", NON_EMPTY_STRING_CONFIG_TYPE, true, 0},
    {"keep_logs", INT_CONFIG_TYPE, true, "30"},
    {"cache_dir", NON_EMPTY_STRING_CONFIG_TYPE, true, 0},
    {"browse_dir", NON_EMPTY_STRING_CONFIG_TYPE, true, 0},
    {"pse_dir", NON_EMPTY_STRING_CONFIG_TYPE, true, 0},
    {"db_host", NON_EMPTY_STRING_CONFIG_TYPE, true, 0},
    {"db_name", NON_EMPTY_STRING_CONFIG_TYPE, true, 0},
    {"db_user", NON_EMPTY_STRING_CONFIG_TYPE, true, 0},
    {"db_password", NON_EMPTY_STRING_CONFIG_TYPE, true, 0},
    {"recorder_name", NON_EMPTY_STRING_CONFIG_TYPE, true, 0},
    {"tape_transfer_lock_file", NON_EMPTY_STRING_CONFIG_TYPE, true, 0},

    // recorder
    {"recorder_log_level", NON_EMPTY_STRING_CONFIG_TYPE, true, "DEBUG"},
    {"capture_log_level", INT_CONFIG_TYPE, true, "3"},
    {"recorder_www_root", NON_EMPTY_STRING_CONFIG_TYPE, true, 0},
    {"recorder_www_port", INT_CONFIG_TYPE, true, 0},
    {"recorder_www_replay_port", INT_CONFIG_TYPE, true, 0},
    {"recorder_vtr_device_1", NON_EMPTY_STRING_CONFIG_TYPE, true, 0},
    {"recorder_vtr_device_2", NON_EMPTY_STRING_CONFIG_TYPE, true, 0},
    {"recorder_server_barcode_scanner", BOOL_CONFIG_TYPE, true, "false"},
    {"recorder_barcode_fifo", NON_EMPTY_STRING_CONFIG_TYPE, false, 0},
    {"ring_buffer_size", INT_CONFIG_TYPE, true, "125"},
    {"browse_enable", BOOL_CONFIG_TYPE, true, "false"},
    {"browse_video_bit_rate", INT_CONFIG_TYPE, true, "2700"},
    {"browse_thread_count", INT_CONFIG_TYPE, true, "4"},
    {"browse_overflow_frames", INT_CONFIG_TYPE, true, "50"},
    {"pse_enable", BOOL_CONFIG_TYPE, true, "false"},
    {"vitc_lines", INT_ARRAY_CONFIG_TYPE, true, "19,21"},
    {"ltc_lines", INT_ARRAY_CONFIG_TYPE, true, "15,17"},
    {"digibeta_barcode_prefixes", NON_EMPTY_STRING_ARRAY_CONFIG_TYPE, true, "CU "},
    {"num_audio_tracks", INT_CONFIG_TYPE, true, "4"},
    {"chunking_throttle_fps", INT_CONFIG_TYPE, true, "75"},
    {"enable_chunking_junk", BOOL_CONFIG_TYPE, true, "true"},
    {"enable_multi_item", BOOL_CONFIG_TYPE, true, "true"},
    
    // debug
    {"dbg_av_sync", BOOL_CONFIG_TYPE, true, "false"},
    {"dbg_vitc_failures", BOOL_CONFIG_TYPE, true, "false"},
    {"dbg_vga_replay", BOOL_CONFIG_TYPE, true, "false"},
    {"dbg_null_barcode_scanner", BOOL_CONFIG_TYPE, true, "false"},
    {"dbg_store_thread_buffer", BOOL_CONFIG_TYPE, true, "false"},
    {"dbg_serial_port_open", BOOL_CONFIG_TYPE, true, "false"},
};


// from version.h

string rec::get_version()
{
    return "0.9.6";
}

string rec::get_build_date()
{
    return __DATE__;
}


static bool check_absolute_directory_exists(string path, string descriptiveName)
{
    if (!directory_path_is_absolute(path))
    {
        fprintf(stderr, "%s directory '%s' is not an absolute path\n", descriptiveName.c_str(), path.c_str());
        return false;
    }
    else if (!directory_exists(path))
    {
        fprintf(stderr, "%s directory '%s' does not exist\n", descriptiveName.c_str(), path.c_str());
        return false;
    }
    
    return true;
}


static void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s [options]\n", cmd);
    fprintf(stderr, "Options: (* means is required)\n");
    fprintf(stderr, "  -h, --help                   Display this usage message and exit\n");
    fprintf(stderr, "  -v | --version               Display the version and exit\n");
    fprintf(stderr, "* -c | --config <name>         The configuration filename\n");
    fprintf(stderr, "  --config-string <value>      Configuration string (name/value pairs separated by ;). Overrides configuration file entries.\n");
}

int main(int argc, const char** argv)
{
    int cmdlnIndex = 1;
    string configFilename;
    string configString;
    string recorderName;
    string logDir;
    LogLevel logLevel;
    string cacheDir;
    string browseDir;
    string pseDir;
    string dbHost;
    string dbName;
    string dbUser;
    string dbPassword;
    string documentRoot;
    int port;
    int replayPort;
    string vtrSerialDeviceName1;
    string vtrSerialDeviceName2;
    string scannerFIFO;
    
    
    // parse command line arguments
    while (cmdlnIndex < argc)
    {
        if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
            strcmp(argv[cmdlnIndex], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[cmdlnIndex], "-v") == 0 ||
            strcmp(argv[cmdlnIndex], "--version") == 0)
        {
            printf("%s (build %s)\n", get_version().c_str(), get_build_date().c_str());
            return 0;
        }
        else if (strcmp(argv[cmdlnIndex], "-c") == 0 ||
            strcmp(argv[cmdlnIndex], "--config") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            configFilename = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--config-string") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            configString = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else
        {
            usage(argv[0]);
            fprintf(stderr, "Unknown argument '%s'\n", argv[cmdlnIndex]);
            return 1;
        }
    }    
    
    if (configFilename.empty())
    {
        usage(argv[0]);
        fprintf(stderr, "Missing --config argument\n");
        return 1;
    }


    // setup, read and validate config
    
    if (!Config::setSpec(g_configSpec, sizeof(g_configSpec) / sizeof(ConfigSpec)))
    {
        fprintf(stderr, "Failed to set default configuration\n");
        return 1;
    }
    
    if (!Config::readFromFile(configFilename))
    {
        fprintf(stderr, "Failed to read configuration from '%s'\n", configFilename.c_str());
        return 1;
    }

    if (!configString.empty())
    {
        if (!Config::readFromString(configString))
        {
            fprintf(stderr, "Failed to read configuration from string '%s'\n", configString.c_str());
            return 1;
        }
    }

    if (!Config::haveValue("recorder_name"))
    {
        string hostName = get_host_name();
        if (hostName.empty())
        {
            fprintf(stderr, "Failed to get the hostname to set the default recorder name: %s\n", strerror(errno));
            return 1;
        }
        Config::setString("recorder_name", hostName);
    }
    
    string configError;
    if (!Config::validate(&configError))
    {
        fprintf(stderr, "Invalid configuration: %s\n", configError.c_str());
        return 1;
    }
    
    
    // read config values and do further checks

    logDir = Config::getString("log_dir");
    if (!directory_exists(logDir))
    {
        fprintf(stderr, "Log directory '%s' does not exist\n", logDir.c_str());
        return 1;
    }

    if (!parse_log_level_string(Config::getString("recorder_log_level"), &logLevel))
    {
        fprintf(stderr, "Invalid recorder_log_level in configuration file\n");
        return 1;
    }
    
    cacheDir = Config::getString("cache_dir");
    if (!check_absolute_directory_exists(cacheDir, "Cache"))
    {
        return 1;
    }
    
    browseDir = Config::getString("browse_dir");
    if (!check_absolute_directory_exists(browseDir, "Browse"))
    {
        return 1;
    }
    
    pseDir = Config::getString("pse_dir");
    if (!check_absolute_directory_exists(pseDir, "PSE"))
    {
        return 1;
    }

    dbHost = Config::getString("db_host");
    dbName = Config::getString("db_name");
    dbUser = Config::getString("db_user");
    dbPassword = Config::getString("db_password");
    
    recorderName = Config::getString("recorder_name");
    
    documentRoot = Config::getString("recorder_www_root");
    port = Config::getInt("recorder_www_port");
    replayPort = Config::getInt("recorder_www_replay_port");
    
    vtrSerialDeviceName1 = Config::getString("recorder_vtr_device_1");
    vtrSerialDeviceName2 = Config::getString("recorder_vtr_device_2");

    if (Config::getBool("recorder_server_barcode_scanner"))
    {
        if (!Config::getString("recorder_barcode_fifo", &scannerFIFO))
        {
            fprintf(stderr, "Missing recorder_barcode_fifo in configuration file\n");
            return 1;
        }
        else if (!file_exists(scannerFIFO))
        {
            fprintf(stderr, "Scanner FIFO file '%s' does not exist\n", scannerFIFO.c_str());
            return 1;
        }
    }

    if (Config::getInt("chunking_throttle_fps") <= 0)
    {
        fprintf(stderr, "Invalid chunking_throttle_fps in configuration file\n");
        return 1;
    }
    
    
    // initialise the curl library
    curl_global_init(CURL_GLOBAL_ALL);
    
    
    // clear old log files
    clear_old_log_files(logDir, g_recorderLogPrefix, g_recorderLogSuffix, Config::getInt("keep_logs"));    
    clear_old_log_files(logDir, g_captureLogPrefix, g_captureLogSuffix, Config::getInt("keep_logs"));    

    
    // initialise logging to file and console
    Date logFileStartDate = get_todays_date();
    string recorderLogFilename = join_path(logDir, add_timestamp_to_filename(g_recorderLogPrefix, g_recorderLogSuffix));
    string captureLogFilename = join_path(logDir, add_timestamp_to_filename(g_captureLogPrefix, g_captureLogSuffix));
    try
    {
        Logging::initialise(new DualLogging(recorderLogFilename, logLevel));
        openLogFile(captureLogFilename.c_str());
    }
    catch (const RecorderException& ex)
    {
        fprintf(stderr, "Failed to initialise logging: Recorder exception: %s\n", ex.getMessage().c_str());
        return 1;
    }
    catch (...)
    {
        fprintf(stderr, "Failed to initialise logging: Unknown exception\n");
        return 1;
    }

    Logging::info("Starting recorder '%s'...\n", recorderName.c_str());
    Logging::info("Version: %s (build %s)\n", get_version().c_str(), get_build_date().c_str());
    Logging::info("Configuration: %s\n", Config::writeToString().c_str());
    
    
    // initialise the database
    try
    {
        PostgresRecorderDatabase::initialise(dbHost, dbName, dbUser, dbPassword);
    }
    catch (const RecorderException& ex)
    {
        Logging::error("Failed to initialise the postgres database: Recorder exception: %s\n", ex.getMessage().c_str());
        return 1;
    }
    
    
    // open the scanner - either using exclusive device access or a fifo written to by an another process
    BarcodeScanner* scanner = 0;
    if (Config::getBool("recorder_server_barcode_scanner"))
    {
        try
        {
            if (Config::getBool("dbg_null_barcode_scanner"))
            {
                scanner = new BarcodeScanner();
                Logging::warning("Using null barcode scanner device\n");
            }
            else if (scannerFIFO.size() > 0)
            {
                scanner = new BarcodeScannerFIFO(scannerFIFO);
            }
            else
            {
                scanner = new BarcodeScannerDevice();
            }
        }
        catch (const RecorderException& ex)
        {
            Logging::error("Recorder exception: %s\n", ex.getMessage().c_str());
            return 1;
        }
    }
    
    // start
    try
    {
        // add an alias to the pse directory so that we can html link to files
        vector<pair<string, string> > pseAlias;
        pseAlias.push_back(pair<string, string>(HTTPRecorder::getPSEReportsURL(), pseDir));
        HTTPServer httpServer(port, documentRoot, pseAlias);

        Recorder recorder(recorderName, cacheDir, browseDir, pseDir, replayPort, 
            vtrSerialDeviceName1, vtrSerialDeviceName2);

        HTTPRecorder httpRec(&httpServer, &recorder, scanner);
        HTTPVTRControl httpVTRControl(&httpServer, recorder.getVTRControls());

        httpServer.start();
            
        while (true)
        {
            // sleep 5 minutes
            sleep_sec(5 * 60);

            
            // check if the log files must be moved to next day
            
            Date dateNow = get_todays_date();
            if (logFileStartDate != dateNow)
            {
                string newRecorderLogFilename = join_path(logDir, add_timestamp_to_filename(g_recorderLogPrefix, g_recorderLogSuffix));
                if (newRecorderLogFilename.compare(recorderLogFilename) != 0)
                {
                    FileLogging* fileLogging = dynamic_cast<FileLogging*>(Logging::getInstance());
                    if (fileLogging != 0)
                    {
                        Logging::info("Reopening log file '%s'\n", newRecorderLogFilename.c_str());
                        if (fileLogging->reopenLogFile(newRecorderLogFilename))
                        {
                            Logging::info("Reopened log file from '%s'\n", recorderLogFilename.c_str());
                            recorderLogFilename = newRecorderLogFilename;
                            
                            clear_old_log_files(logDir, g_recorderLogPrefix, g_recorderLogSuffix, Config::getInt("keep_logs"));    
    
                            Logging::info("Recorder '%s'\n", recorderName.c_str());
                            Logging::info("Version: %s (build %s)\n", get_version().c_str(), get_build_date().c_str());
                            Logging::info("Configuration: %s\n", Config::writeToString().c_str());
                        }
                        else
                        {
                            Logging::error("Failed to reopen log file '%s'\n", newRecorderLogFilename.c_str());
                        }
                    }
                }
                
                string newCaptureLogFilename = join_path(logDir, add_timestamp_to_filename(g_captureLogPrefix, g_captureLogSuffix));
                if (newCaptureLogFilename.compare(captureLogFilename) != 0)
                {
                    logTF("Reopening log file '%s'\n", newCaptureLogFilename.c_str());
                    if (reopenLogFile(newCaptureLogFilename.c_str()))
                    {
                        logTF("Reopened log file from '%s'\n", newCaptureLogFilename.c_str());
                        captureLogFilename = newCaptureLogFilename;
                        
                        clear_old_log_files(logDir, g_captureLogPrefix, g_captureLogSuffix, Config::getInt("keep_logs"));    
                    }
                    else
                    {
                        logTF("ERROR: Failed to reopen log file '%s'\n", newCaptureLogFilename.c_str());
                    }
                }

                logFileStartDate = dateNow; // whatever happens we try only once per day
            }
        }
    }
    catch (const RecorderException& ex)
    {
        Logging::error("Recorder exception: %s\n", ex.getMessage().c_str());
        return 1;
    }
    
    delete scanner;
    
    RecorderDatabase::close();

    curl_global_cleanup();
    
    return 0;
}


