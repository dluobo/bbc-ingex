/*
 * $Id: PSEReport.cpp,v 1.1 2008/07/08 16:24:56 philipn Exp $
 *
 * Used to write a PSE report.
 *
 * Copyright (C) 2007 BBC Research, Jim Easterbrook, <easter@users.sourceforge.net>
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
    The template file for the PSE report, pse_report_template.html, is
    converted into a .cpp file using the main() code at the bottom of this file. 
    This code is activated when GEN_PSE_REPORT_TEMPLATE is set - see Makefile. 
    The PSEReport class is used to write a report using the template and it 
    fills in the source Infax metadata and PSE results data.
*/

#if !defined(GEN_PSE_REPORT_TEMPLATE)

#define __STDC_FORMAT_MACROS 1

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>

#include "PSEReport.h"

#include "write_archive_mxf.h"


using namespace std;


#define CHECK_PRINTF_ORET(cmd) \
    if ((cmd) < 0) \
    { \
        fprintf(stderr, "Failed to print to file\n"); \
        return false; \
    }


static inline int max(int a, int b)
{
    return (a > b) ? a : b;
}

static inline int min(int a, int b)
{
    return (a < b) ? a : b;
}

static void frameno_to_timecode(int64_t frameno, ArchiveTimecode* timecode)
{
    timecode->frame = frameno % 25;
    frameno = (frameno - timecode->frame) / 25;
    timecode->sec = frameno % 60;
    frameno = (frameno - timecode->sec) / 60;
    timecode->min = frameno % 60;
    frameno = (frameno - timecode->min) / 60;
    timecode->hour = frameno % 60;
    frameno = (frameno - timecode->hour) / 60;
}

static string trim_string(string val)
{
    size_t start;
    size_t len;
    
    // trim spaces from the start
    start = 0;
    while (start < val.size() && isspace(val[start]))
    {
        start++;
    }
    if (start >= val.size())
    {
        // empty string or string with spaces only
        return "";
    }
    
    // trim spaces from the end by reducing the length
    len = val.size() - start;
    while (len > 0 && isspace(val[start + len - 1]))
    {
        len--;
    }
    
    return val.substr(start, len);
}

static const char* get_duration_string(int64_t duration)
{
    static char buf[64];
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

static string get_prog_no_string(string magPrefix, string progNo, string prodCode)
{
    string trimProdCode = trim_string(prodCode);
    string trimMagPrefix = trim_string(magPrefix);
    
    string result;
    if (!trimMagPrefix.empty())
    {
        result = trimMagPrefix + ":";
    }
    result += trim_string(progNo);
    if (!trimProdCode.empty())
    {
        result += "/";
        if (trimProdCode.size() == 1 && 
            trimProdCode[0] >= '0' && trimProdCode[0] <= '9')
        {
            // replace single digit with 2 digit starting with '0'
            result += "0";
        }
        result += trimProdCode;
    }
    
    return result;
}

static string get_int_string(int val)
{
    char buf[32];
    sprintf(buf, "%d", val);
    
    return buf;
}

static void set_html_value(string& html, string name, string value)
{
    size_t pos = html.find(name);
    if (pos == string::npos)
    {
        return;
    }
    
    html.replace(pos, name.size(), value);
}




PSEReport* PSEReport::open(string filename)
{
    FILE* reportFile = fopen(filename.c_str(), "wb");
    if (reportFile == 0)
    {
        fprintf(stderr, "Failed to open PSE report '%s': %s\n", filename.c_str(), strerror(errno));
        return 0;
    }
    
    return new PSEReport(reportFile);
}

bool PSEReport::hasPassed(const PSEFailure* results, int numResults)
{
    int i;
    for (i = 0; i < numResults; i++)
    {
        if (results[i].redFlash >= 500 ||
            results[i].spatialPattern >= 500 ||
            results[i].luminanceFlash >= 500 ||
            results[i].extendedFailure != 0)
        {
            return false;
        }
    }

    return true;    
}



PSEReport::PSEReport(FILE* reportFile)
: _reportFile(reportFile)
{
}

PSEReport::~PSEReport()
{
    fclose(_reportFile);
}

bool PSEReport::write(int64_t firstAnalysisFrame, int64_t lastAnalysisFrame,
    std::string mxfFilename, const InfaxData* infaxData,
    const PSEFailure* results, int numResults, bool* passed)
{
    #define HIST_SIZE 35
    int histogram[HIST_SIZE];
    int redCount;
    int spatialCount;
    int luminanceCount;
    int extendedCount;
    int threshold;
    int i;
    char dateString[11];

    // get histogram and counts of PSE result data
    for (i = 0; i < HIST_SIZE; i++)
    {
        histogram[i] = 0;
    }
    redCount = 0;
    spatialCount = 0;
    luminanceCount = 0;
    extendedCount = 0;
    int bin;
    for (i = 0; i < numResults; i++)
    {
        if (results[i].redFlash >= 500)
        {
            redCount++;
        }
        if (results[i].spatialPattern >= 500)
        {
            spatialCount++;
        }
        if (results[i].luminanceFlash >= 500)
        {
            luminanceCount++;
        }
        if (results[i].extendedFailure != 0)
        {
            extendedCount++;
        }
        else
        {
            bin = min(results[i].redFlash / 100, HIST_SIZE - 1);
            histogram[bin]++;
            bin = min(results[i].spatialPattern / 100, HIST_SIZE - 1);
            histogram[bin]++;
            bin = min(results[i].luminanceFlash / 100, HIST_SIZE - 1);
            histogram[bin]++;
        }
    }
    
    // set pass/fail
    *passed = (redCount + spatialCount + luminanceCount + extendedCount == 0);
    
    // compute threshold for detail list
    threshold = HIST_SIZE;
    int count = 0;
    while (threshold > 1 && count < 100)
    {
        threshold--;
        count += histogram[threshold];
    }
    threshold = threshold * 100;
    
    // format some data into strings
    time_t now = time(NULL);
    strftime(dateString, sizeof(dateString), "%Y/%m/%d", localtime(&now));
    
    
    // fill in report header template and write
    
    string reportHeader = g_pseReportHeader;

    set_html_value(reportHeader, "$pseStatusClass", *passed ? "pse-passed" : "pse-failed");
    set_html_value(reportHeader, "$pseStatus", *passed ? "PASSED" : "FAILED");
    
    set_html_value(reportHeader, "$dateOfAnalysis", dateString);

    set_html_value(reportHeader, "$mxfFilename", mxfFilename);
    
    set_html_value(reportHeader, "$firstAnalysisFrame", get_duration_string(firstAnalysisFrame));
    set_html_value(reportHeader, "$lastAnalysisFrame", get_duration_string(lastAnalysisFrame));

    set_html_value(reportHeader, "$duration", get_duration_string(lastAnalysisFrame - firstAnalysisFrame + 1));

    set_html_value(reportHeader, "$progTitle", infaxData->progTitle);
    set_html_value(reportHeader, "$episodeTitle", infaxData->epTitle);
    set_html_value(reportHeader, "$progNumber", get_prog_no_string(infaxData->magPrefix, infaxData->progNo, infaxData->prodCode));
    set_html_value(reportHeader, "$infaxDuration", get_duration_string(25 * infaxData->duration));
    set_html_value(reportHeader, "$sourceSpoolNumber", infaxData->spoolNo);
    
    set_html_value(reportHeader, "$redCount", get_int_string(redCount));
    set_html_value(reportHeader, "$spatialCount", get_int_string(spatialCount));
    set_html_value(reportHeader, "$luminanceCount", get_int_string(luminanceCount));
    set_html_value(reportHeader, "$extendedCount", get_int_string(extendedCount));

    CHECK_PRINTF_ORET(fprintf(_reportFile, "%s", reportHeader.c_str()));
    
    
    // fill in failures header template and write

    string failuresHeader = g_pseFailuresHeader;

    set_html_value(failuresHeader, "$failureShowCount", get_int_string(count + extendedCount));
    set_html_value(failuresHeader, "$failureDescription", threshold >= 500 ? "violations" : "warnings &amp; violations");

    set_html_value(failuresHeader, "$dateOfAnalysis", dateString);
    set_html_value(failuresHeader, "$duration", get_duration_string(lastAnalysisFrame - firstAnalysisFrame + 1));
    
    CHECK_PRINTF_ORET(fprintf(_reportFile, "%s", failuresHeader.c_str()));
    

    // write failures    
    
    int64_t lastFrame = -1;
    ArchiveTimecode ctrlTC;
    for (i = 0; i < numResults; i++)
    {
        if (results[i].redFlash >= threshold ||
            results[i].spatialPattern >= threshold ||
            results[i].luminanceFlash >= threshold ||
            results[i].extendedFailure != 0)
        {
            if (lastFrame >= 0 && results[i].position != lastFrame + 1)
            {
                CHECK_PRINTF_ORET(fprintf(_reportFile, "<tr><td colspan='8'></td></tr>\n"));
            }
            
            frameno_to_timecode(results[i].position, &ctrlTC);
            
            CHECK_PRINTF_ORET(fprintf(_reportFile,
                "<tr><td align=\"right\">%"PRId64"</td>"
                "<td>%02d:%02d:%02d:%02d</td><td>%02d:%02d:%02d:%02d</td>"
                "<td>%02d:%02d:%02d:%02d</td><td align=\"center\">%3.1f</td>"
                "<td align=\"center\">%3.1f</td>"
                "<td align=\"center\">%3.1f</td><td>%c</td></tr>\n",
                results[i].position,
                ctrlTC.hour, ctrlTC.min, ctrlTC.sec, ctrlTC.frame,
                results[i].vitcTimecode.hour,
                results[i].vitcTimecode.min,
                results[i].vitcTimecode.sec,
                results[i].vitcTimecode.frame,
                results[i].ltcTimecode.hour,
                results[i].ltcTimecode.min,
                results[i].ltcTimecode.sec,
                results[i].ltcTimecode.frame,
                (float)results[i].redFlash / 1000.0,
                (float)results[i].spatialPattern / 1000.0,
                (float)results[i].luminanceFlash / 1000.0,
                results[i].extendedFailure != 0 ? 'X' : ' '
                ));
            lastFrame = results[i].position;
        }
    }
    
    
    CHECK_PRINTF_ORET(fprintf(_reportFile, "%s", g_pseFailuresFooter));

    
    CHECK_PRINTF_ORET(fprintf(_reportFile, "%s", g_pseReportFooter));
    
    
    return true;
}







#else // GEN_PSE_REPORT_TEMPLATE 


#include <stdio.h>
#include <errno.h>

#include <string>


using namespace std;


static const char* g_headerEndMarker = "<!-- HEADER END -->";
static const char* g_failuresHeaderStartMarker = "<!-- FAILURES HEADER START -->";
static const char* g_failuresHeaderEndMarker = "<!-- FAILURES HEADER END -->";
static const char* g_failuresFooterStartMarker = "<!-- FAILURES FOOTER START -->";
static const char* g_failuresFooterEndMarker = "<!-- FAILURES FOOTER END -->";
static const char* g_footerStartMarker = "<!-- FOOTER START -->";


static void write_html_string(FILE* out, const char* html, size_t len)
{
    size_t i;
    
    for (i = 0; i < len; i++)
    {
        if (html[i] == '\"')
        {
            fprintf(out, "\\\"");
        }
        else if (html[i] == '\\')
        {
            fprintf(out, "\\");
        }
        else if (html[i] == '\n')
        {
            fprintf(out, "\\n\"\n    \"");
        }
        else
        {
            fprintf(out, "%c", html[i]);
        }
    }
}

int main(int argc, const char** argv)
{
    string htmlTemplateFilename;
    string cppSourceFilename;
    
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <html template> <cpp filename>\n", argv[0]);
        return 1;
    }
    
    htmlTemplateFilename = argv[1];
    cppSourceFilename = argv[2];
    
    
    FILE* htmlTemplateFile = fopen(htmlTemplateFilename.c_str(), "rb");
    if (htmlTemplateFile == 0)
    {
        fprintf(stderr, "Failed to open html template file '%s': %s\n", htmlTemplateFilename.c_str(), strerror(errno));
        return 1;
    }
    FILE* cppSourceFile = fopen(cppSourceFilename.c_str(), "wb");
    if (cppSourceFile == 0)
    {
        fprintf(stderr, "Failed to open cpp source file '%s': %s\n", cppSourceFilename.c_str(), strerror(errno));
        return 1;
    }
    
    
    // read the html template into memory
    
    string htmlTemplate;
    htmlTemplate.reserve(2048);
    char buffer[1024];
    int numRead;
    while (true)
    {
        numRead = fread(buffer, 1, 1024, htmlTemplateFile);

        if (numRead != 0)
        {
            htmlTemplate.append(buffer, 0, numRead);
        }
        
        if (numRead != 1024)
        {
            if (!feof(htmlTemplateFile))
            {
                fprintf(stderr, "Failed to read from html template file: %s\n", strerror(errno));
                return 1;
            }
            break;
        }
        
    }
    
    
    // locate the header, failures and footer boundaries
    
    size_t headerEnd = htmlTemplate.rfind(g_headerEndMarker);
    size_t failuresHeaderStart = htmlTemplate.rfind(g_failuresHeaderStartMarker);
    size_t failuresHeaderEnd = htmlTemplate.rfind(g_failuresHeaderEndMarker);
    size_t failuresFooterStart = htmlTemplate.rfind(g_failuresFooterStartMarker);
    size_t failuresFooterEnd = htmlTemplate.rfind(g_failuresFooterEndMarker);
    size_t footerStart = htmlTemplate.rfind(g_footerStartMarker);
    
    if (headerEnd == string::npos)
    {
        fprintf(stderr, "Failed to find header end marker: %s\n", g_headerEndMarker);
        return 1;
    }
    if (failuresHeaderStart == string::npos)
    {
        fprintf(stderr, "Failed to find failures header start marker: %s\n", g_failuresHeaderStartMarker);
        return 1;
    }
    failuresHeaderStart += strlen(g_failuresHeaderStartMarker);
    if (failuresHeaderEnd == string::npos)
    {
        fprintf(stderr, "Failed to find failures header end marker: %s\n", g_failuresHeaderEndMarker);
        return 1;
    }
    if (failuresFooterStart == string::npos)
    {
        fprintf(stderr, "Failed to find failures footer start marker: %s\n", g_failuresFooterStartMarker);
        return 1;
    }
    failuresFooterStart += strlen(g_failuresFooterStartMarker);
    if (failuresFooterEnd == string::npos)
    {
        fprintf(stderr, "Failed to find failures footer end marker: %s\n", g_failuresFooterEndMarker);
        return 1;
    }
    if (footerStart == string::npos)
    {
        fprintf(stderr, "Failed to find footer start marker: %s\n", g_footerStartMarker); 
        return 1;
    }
    footerStart += strlen(g_footerStartMarker);
    
    
    
    // write the cpp file
    
    fprintf(cppSourceFile, "// generated from '%s'\n", htmlTemplateFilename.c_str());
    fprintf(cppSourceFile, "\n");
    fprintf(cppSourceFile, "\n");
    fprintf(cppSourceFile, "\n");
    
    fprintf(cppSourceFile, "const char* g_pseReportHeader = \n");
    fprintf(cppSourceFile, "    \"");
    write_html_string(cppSourceFile, &htmlTemplate[0], headerEnd - 0);
    fprintf(cppSourceFile, "\";\n");
    fprintf(cppSourceFile, "\n");
    fprintf(cppSourceFile, "\n");
    
    fprintf(cppSourceFile, "const char* g_pseFailuresHeader = \n");
    fprintf(cppSourceFile, "    \"");
    write_html_string(cppSourceFile, &htmlTemplate[failuresHeaderStart], failuresHeaderEnd - failuresHeaderStart);
    fprintf(cppSourceFile, "\";\n");
    fprintf(cppSourceFile, "\n");
    fprintf(cppSourceFile, "\n");
    
    fprintf(cppSourceFile, "const char* g_pseFailuresFooter = \n");
    fprintf(cppSourceFile, "    \"");
    write_html_string(cppSourceFile, &htmlTemplate[failuresFooterStart], failuresFooterEnd - failuresFooterStart);
    fprintf(cppSourceFile, "\";\n");
    fprintf(cppSourceFile, "\n");
    fprintf(cppSourceFile, "\n");
    
    fprintf(cppSourceFile, "const char* g_pseReportFooter = \n");
    fprintf(cppSourceFile, "    \"");
    write_html_string(cppSourceFile, &htmlTemplate[footerStart], htmlTemplate.size() - footerStart);
    fprintf(cppSourceFile, "\";\n");
    fprintf(cppSourceFile, "\n");
    fprintf(cppSourceFile, "\n");
    
    
    fclose(cppSourceFile);
    fclose(htmlTemplateFile);
    
    
    return 0;
}

#endif

