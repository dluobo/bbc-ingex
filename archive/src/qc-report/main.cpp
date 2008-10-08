/*
 * $Id: main.cpp,v 1.2 2008/10/08 10:22:13 john_f Exp $
 *
 * Utility to generate a QC and PSE report from one or more QC sessions
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
    This utility is called by the qc_player application, via the 
    qc_report_script.sh script, when a session is completed. The session file 
    and D3 MXF file are used to create the QC report and PSE report. The 
    progress is shown using a KDE progress bar dialog.
    
    The URL of the QC report is printed to stdout so that the calling script, 
    qc_report_script.sh, can open firefox with the given web page. The page is
    served by the qc_player on port 9006, which allow the user to update the
    comments field.
    
    This utility can also be used to generate reports for all sessions in an LTO
    directory.
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
#include "LTODirectory.h"
#include "QCSessionFile.h"
#include "QCReport.h"
#include "PSEReportWrapper.h"
#include "RecorderException.h"
#include "Logging.h"

using namespace std;
using namespace rec;


static const int g_defaultReportPort = 9006;



class KDEProgressBar
{
public:
    KDEProgressBar() : _progressPercentage(0.0), _start(0.0), _end(100.0) {}
    ~KDEProgressBar() {}
    
    void enable(string dcopRef) 
    { 
        _dcopRef = dcopRef;
    }
    
    bool isEnabled() 
    { 
        return !_dcopRef.empty();
    }
    
    void start(float start)
    {
        _start = start;
        
        if (_progressPercentage < _start)
        {
            _progressPercentage = _start;
        }
    }
    
    void end(float end)
    {
        _end = end;
    }

    
    float remainingPercentage() 
    { 
        return _end - _progressPercentage;
    }
    
    void update(string label, float increment)
    {
        if (increment < 0.0)
        {
            _progressPercentage = _end;
        }
        else
        {
            _progressPercentage += increment;
            if (_progressPercentage > _end)
            {
                _progressPercentage = _start;
            }
        }            
        
        char cmd[128];

        // update the progress percentage        
        sprintf(cmd, "dcop \"%s\" setProgress %d", _dcopRef.c_str(), (int)_progressPercentage);
        system(cmd);
        
        // update the label        
        sprintf(cmd, "dcop \"%s\" setLabel \"%s\"", _dcopRef.c_str(), label.c_str());
        system(cmd);
    }
    
private:    
    string _dcopRef;
    float _progressPercentage;
    float _start;
    float _end;
};



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

static string get_log_filename(string dir, string prefix)
{
    time_t t = time(NULL);
    struct tm lt;
    
    lt = *localtime(&t); 

    char buf[64];
    sprintf(buf, "%04d%02d%02d%02d%02d%02d.txt", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec);
    
    return join_path(dir, prefix + buf);
}

string get_lto_name(string filename)
{
    string ltoName = filename;
    if (ltoName[ltoName.size() - 1] == '/')
    {
        ltoName = ltoName.substr(0, ltoName.size() - 1);
    }
    size_t sepIndex;
    if ((sepIndex = ltoName.rfind("/")) != string::npos)
    {
        return ltoName.substr(sepIndex + 1);
    }
    return ltoName;
}

string strip_path(string filename)
{
    size_t sepIndex;
    if ((sepIndex = filename.rfind("/")) != string::npos)
    {
        return filename.substr(sepIndex + 1);
    }
    return filename;
}



static void set_string_value(string& target, string name, string value)
{
    size_t pos = target.find(name);
    if (pos == string::npos)
    {
        return;
    }
    
    target.replace(pos, name.size(), value);
}



void usage(const char* cmd)
{
    fprintf(stderr, "Usage: %s [options]\n", cmd);
    fprintf(stderr, "Options: (* means is required)\n");
    fprintf(stderr, "  -h, --help                   Display this usage message\n");
    fprintf(stderr, "* --report <dir>               Report directory\n");
    fprintf(stderr, "* --lto <dir>                  LTO cache directory\n");
    fprintf(stderr, "* --log <dir>                  Log messages to file in <dir>\n");
    fprintf(stderr, "  --session <name>             QC session filename in LTO directory\n");
    fprintf(stderr, "  --report-port <num>          The HTTP port of the reports server (default %d)\n", g_defaultReportPort);
    fprintf(stderr, "  --progress-dcop <name>       Name of KDE DCOP used for the progress bar\n");
    fprintf(stderr, "  --progress-start <perc>      Start percentage showing KDE progress bar\n");
    fprintf(stderr, "  --progress-end <perc>        End percentage showing KDE progress bar\n");
    fprintf(stderr, "  --backup-only                Don't create reports but backup only\n");
    fprintf(stderr, "  --print-qc-url               Output to stdout the URL of each QC report\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "The QC and PSE reports are written to the --report directory\n");
    fprintf(stderr, "Backups of the session files are written to a subdirectory of the --report directory\n");
    fprintf(stderr, "Omitting the --session option will cause reports to be generated for all sessions\n");
    fprintf(stderr, "The PSE report filename appears before the QC report filename when printing to stdout\n");
}

int main(int argc, const char** argv)
{
    int cmdlnIndex = 1;
    string ltoDirectoryName;
    string reportDirectoryName;
    string sessionName;
    KDEProgressBar progressBar;
    float progressStart = 0.0;
    float progressEnd = 100.0;
    bool backupOnly = false;
    bool printQCURL = false;
    string logDir;
    int reportPort = g_defaultReportPort;
    
    while (cmdlnIndex < argc)
    {
       if (strcmp(argv[cmdlnIndex], "-h") == 0 ||
            strcmp(argv[cmdlnIndex], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
         else if (strcmp(argv[cmdlnIndex], "--report") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            reportDirectoryName = argv[cmdlnIndex + 1];
            if (reportDirectoryName.empty())
            {
                reportDirectoryName = ".";
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--lto") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            ltoDirectoryName = argv[cmdlnIndex + 1];
            if (ltoDirectoryName.empty())
            {
                ltoDirectoryName = ".";
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
            logDir = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--session") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            sessionName = argv[cmdlnIndex + 1];
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--report-port") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            else if (sscanf(argv[cmdlnIndex + 1], "%d", &reportPort) != 1 ||
                reportPort < 0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--progress-dcop") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            progressBar.enable(argv[cmdlnIndex + 1]);
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--progress-start") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            else if (sscanf(argv[cmdlnIndex + 1], "%f", &progressStart) != 1 ||
                progressStart < 0.0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            progressBar.start(progressStart);
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--progress-end") == 0)
        {
            if (cmdlnIndex + 1 >= argc)
            {
                usage(argv[0]);
                fprintf(stderr, "Missing argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            else if (sscanf(argv[cmdlnIndex + 1], "%f", &progressEnd) != 1 ||
                progressEnd < 0.0)
            {
                usage(argv[0]);
                fprintf(stderr, "Invalid argument for %s\n", argv[cmdlnIndex]);
                return 1;
            }
            progressBar.end(progressEnd);
            cmdlnIndex += 2;
        }
        else if (strcmp(argv[cmdlnIndex], "--backup-only") == 0)
        {
            backupOnly = true;
            cmdlnIndex++;
        }
        else if (strcmp(argv[cmdlnIndex], "--print-qc-url") == 0)
        {
            printQCURL = true;
            cmdlnIndex++;
        }
        else
        {
            usage(argv[0]);
            fprintf(stderr, "Unexpected argument '%s'\n", argv[cmdlnIndex]);
            return 1;
        }
    }

    if (reportDirectoryName.empty())
    {
        usage(argv[0]);
        fprintf(stderr, "Missing report directory\n");
        return 1;
    }
    if (ltoDirectoryName.empty())
    {
        usage(argv[0]);
        fprintf(stderr, "Missing lto directory\n");
        return 1;
    }
    
    if (logDir.empty())
    {
        usage(argv[0]);
        fprintf(stderr, "Missing log files directory\n");
        return 1;
    }

    
    if (!directory_exists(logDir))
    {
        fprintf(stderr, "Log files directory '%s' does not exist\n", logDir.c_str());
        return 1;
    }
    
    if (!directory_exists(ltoDirectoryName))
    {
        fprintf(stderr, "LTO directory '%s' does not exist\n", ltoDirectoryName.c_str());
        return 1;
    }
    
    
    try
    {
        string logFilename;
        if (sessionName.empty())
        {
            logFilename = get_log_filename(logDir, "qcreport_log_");
        }
        else
        {
            logFilename = "qcreport_log_";
            logFilename.append(sessionName);
            logFilename = join_path(logDir, logFilename);
        }
        Logging::initialise(new FileLogging(logFilename, LOG_LEVEL_INFO));
        fprintf(stderr, "QC report log: %s\n", logFilename.c_str());
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
    

    Logging::info("----\n");
    if (sessionName.empty())
    {
        Logging::info("Starting report generation and backup of session files ...\n");
    }
    else
    {
        Logging::info("Starting report generation and backup of session file '%s' ...\n", sessionName.c_str());
    }
    
    Logging::info("LTO cache directory is '%s'\n", ltoDirectoryName.c_str());
    
    try
    {
        if (progressBar.isEnabled())
        {
            progressBar.update("Creating/checking directories", 1.0);
        }
        
        // create the report directory if it doesn't already exist
        if (!directory_exists(reportDirectoryName))
        {
            if (mkdir(reportDirectoryName.c_str(), 0777) != 0)
            {
                Logging::error("Failed to create report directory '%s': %s\n", reportDirectoryName.c_str(), strerror(errno));
                return 1;
            }
            Logging::info("Created report directory '%s'\n", reportDirectoryName.c_str());
        }
        else
        {
            Logging::info("Using report directory '%s'\n", reportDirectoryName.c_str());
        }
        
        // create the session file backup directory is it doesn't already exist
        string backupDirectoryName = join_path(reportDirectoryName, last_path_component(ltoDirectoryName)) + "_backup";
        if (!directory_exists(backupDirectoryName))
        {
            if (mkdir(backupDirectoryName.c_str(), 0777) != 0)
            {
                Logging::error("Failed to create session backup directory '%s': %s\n", backupDirectoryName.c_str(), strerror(errno));
                return 1;
            }
            Logging::info("Created session backup directory '%s'\n", backupDirectoryName.c_str());
        }
        else
        {
            Logging::info("Using session backup directory '%s'\n", backupDirectoryName.c_str());
        }

        
        
        if (progressBar.isEnabled())
        {
            progressBar.update("Reading list of files", 1.0);
        }
        
        LTODirectory ltoDir(ltoDirectoryName, sessionName);
        vector<LTOItem> items = ltoDir.getItems();
        
        if (backupOnly)
        {
            Logging::info("Skipping report generation\n");
            progressBar.update("Skipping report generation", progressBar.remainingPercentage() - 5.0);
        }
        else
        {
            // calculate number of progress increments to process the items
            float progressIncrement = 0.0;
            int processItemCount = 0;
            vector<LTOItem>::iterator iter;
            for (iter = items.begin(); iter != items.end(); iter++)
            {
                LTOItem& item = *iter;
                
                if (!file_exists(join_path(ltoDirectoryName, item.mxfFilename)))
                {
                    continue;
                }
                else if (!file_exists(join_path(ltoDirectoryName, item.sessionFilename)))
                {
                    // generate the PSE report only
                    processItemCount++;
                }
                else
                {
                    // generate both the PSE and QC report
                    processItemCount += 2;
                }
            }
            if (processItemCount > 0)
            {
                progressIncrement = (progressBar.remainingPercentage() - 5.0) / processItemCount;
            }
    
            
            // loop through each item and generate reports
    
            for (iter = items.begin(); iter != items.end(); iter++)
            {
                LTOItem& item = *iter;
                
                if (!file_exists(join_path(ltoDirectoryName, item.mxfFilename)))
                {
                    // no mxf file means no reports
                    continue;
                }
                else if (!file_exists(join_path(ltoDirectoryName, item.sessionFilename)))
                {
                    // output the PSE report only
    
                    D3MXFFile mxfFile(join_path(ltoDirectoryName, item.mxfFilename));
    
                    
                    // PSE report
                    
                    if (progressBar.isEnabled())
                    {
                        progressBar.update(item.mxfFilename + ": PSE Report", progressIncrement);
                    }

                    string pseReportName = join_path(reportDirectoryName, item.mxfFilename) + "_pse_report.html";
                    if (!mxfFile.isComplete())
                    {
                        Logging::info("Warning: Not creating PSE report '%s' because MXF file is incomplete\n", pseReportName.c_str());
                    }
                    else
                    {
                        PSEReportWrapper pseReport(pseReportName, &mxfFile, 0);
                        pseReport.write();
                        Logging::info("Created PSE report '%s'\n", pseReportName.c_str());
                    }
                    
                }
                else
                {
                    // generate both the PSE and QC report
                
                    QCSessionFile sessionFile(join_path(ltoDirectoryName, item.sessionFilename));
                    D3MXFFile mxfFile(join_path(ltoDirectoryName, item.mxfFilename));
                    
                    
                    // PSE report
                    
                    if (progressBar.isEnabled())
                    {
                        progressBar.update(item.mxfFilename + ": PSE Report", progressIncrement);
                    }
                
                    QCPSEResult pseResult = PSE_RESULT_UNKNOWN;
                    string pseReportName = join_path(reportDirectoryName, item.mxfFilename) + "_pse_report.html";
                    if (!mxfFile.isComplete())
                    {
                        Logging::info("Warning: Not creating PSE report '%s' because MXF file is incomplete\n", pseReportName.c_str());
                    }
                    else
                    {
                        PSEReportWrapper pseReport(pseReportName, &mxfFile, &sessionFile);
                        pseReport.write();
                        pseResult = pseReport.getPSEResult() ? PSE_PASSED : PSE_FAILED;
                        Logging::info("Created PSE report '%s'\n", pseReportName.c_str());
                    }

                    
                    // QC report
                    
                    if (progressBar.isEnabled())
                    {
                        progressBar.update(item.mxfFilename + ": QC Report", progressIncrement);
                    }
                
                    string qcReportName = join_path(reportDirectoryName, item.mxfFilename) + "_qc_report.html";
                    if (!mxfFile.isComplete())
                    {
                        Logging::info("Warning: Not creating QC report '%s' because MXF file is incomplete\n", qcReportName.c_str());
                    }
                    else
                    {
                        QCReport qcReport(qcReportName, &mxfFile, &sessionFile);
                        qcReport.write(pseResult);
                        Logging::info("Created QC report '%s'\n", qcReportName.c_str());
                    }
                    
                    // Remove previous png's
                    system(("rm " + reportDirectoryName + "/" + "*" + ".png").c_str());
                    
                    const int TimeStampSize = 15;
                    string fileItemPng = mxfFile.getFilename();
                    set_string_value(fileItemPng, ".mxf", "");
                    string timeStampPng = last_path_component(sessionFile.getFilename()).c_str();
                    set_string_value(timeStampPng, ".txt", "");
                    string::size_type pos = timeStampPng.size();
                    string timeStampPng2(timeStampPng, pos-TimeStampSize);
                    
                    const string barcodeTemplate = "barcode -e code128 -b \"$barString\" | convert -extract $Wx120+0+672 - $imgFile";
                    string sysCmd = barcodeTemplate;
                    set_string_value(sysCmd, "$barString", fileItemPng);
                    set_string_value(sysCmd, "$W", "150");
                    set_string_value(sysCmd, "$imgFile", (reportDirectoryName + "/" + fileItemPng + ".png") );
                    Logging::info("barcode creation cmd : '%s'\n", sysCmd.c_str());
                    int success = system(sysCmd.c_str());
                    if (success != 0)
                    {
                    	Logging::info("barcode image creation failed\n");
                    }

                    sysCmd = barcodeTemplate;
                    set_string_value(sysCmd, "$barString", timeStampPng2);
                    set_string_value(sysCmd, "$W", "250");
                    set_string_value(sysCmd, "$imgFile", (reportDirectoryName + "/" + timeStampPng2 + ".png") );
                    Logging::info("barcode creation cmd : '%s'\n", sysCmd.c_str());
                    success = system(sysCmd.c_str());
                    if (success != 0)
                    {
                    	Logging::info("barcode image creation failed\n");
                    }
                   
                    // output QC report filename to stdout
                    if (printQCURL)
                    {
                        // Note: we don't do URL encoding because the query parameters only consist of letters,
                        // numbers and non-reserved characters '.' and '_'
                        printf("http://localhost:%d/qcreport/framed.html?report=%s&session=%s&carrier=%s\n", 
                            reportPort, strip_path(qcReportName).c_str(), item.sessionFilename.c_str(),
                            get_lto_name(ltoDirectoryName).c_str());
                    }
                }
            }
        }
        
        
        // backup session files and index file
        
        if (progressBar.isEnabled())
        {
            progressBar.update("Backup of session files", 5.0);
        }
        
        Logging::info("Backing up session files and index file to '%s'\n", backupDirectoryName.c_str());
        ltoDir.backup(backupDirectoryName);
    }
    catch (const RecorderException& ex)
    {
        Logging::error("Exception: %s\n", ex.getMessage().c_str());
        return 1;
    }
    catch (...)
    {
        Logging::error("Caught unknown exception\n");
        return 1;
    }
    
    
    Logging::info("Successfully completed report generation and backup\n");
    Logging::info("----\n");
    
    return 0;
}


