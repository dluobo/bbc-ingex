/*
 * $Id: QCReport.cpp,v 1.1 2008/07/08 16:25:16 philipn Exp $
 *
 * Writes a QC report given a session file and D3 MXF file
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
    An HTML template HTML, qc_report_template.html, is converted into a .cpp 
    file when building with GEN_QC_REPORT_TEMPLATE set. This compiled in 
    template is filled in using information provided by the qc_player session 
    file and the D3 MXF file.
*/
 
#define __STDC_FORMAT_MACROS 1

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>

#include "QCReport.h"
#include "RecorderException.h"
#include "Logging.h"


using namespace std;
using namespace rec;


#define RED_MARK_TYPE                0x00000001
#define MAGENTA_MARK_TYPE            0x00000002
#define GREEN_MARK_TYPE              0x00000004
#define BLUE_MARK_TYPE               0x00000008
#define CYAN_MARK_TYPE               0x00000010




#if !defined(GEN_QC_REPORT_TEMPLATE)


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

static void set_html_value(string& html, string name, string value)
{
    size_t pos = html.find(name);
    if (pos == string::npos)
    {
        return;
    }
    
    html.replace(pos, name.size(), value);
}

static string get_date_string_now()
{
    time_t t = time(NULL);
    struct tm gmt;
    memset(&gmt, 0, sizeof(struct tm));
    if (gmtime_r(&t, &gmt) == NULL)
    {
        throw RecorderException("Failed to generate timestamp now\n");
    }

    char buf[64];
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d UTC", 
        gmt.tm_year + 1900, gmt.tm_mon + 1, gmt.tm_mday, 
        gmt.tm_hour, gmt.tm_min, gmt.tm_sec);
        
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

static string get_timecode_string(Timecode& tc)
{
    char buf[64];

    sprintf(buf, "%02d:%02d:%02d:%02d", tc.hour, tc.min, tc.sec, tc.frame);
    
    return buf;
}

static Timecode get_timecode_from_pos(int64_t position)
{
    Timecode tc;

    tc.hour = (int)(position / (60 * 60 * 25));
    tc.min = (int)((position % (60 * 60 * 25)) / (60 * 25));
    tc.sec = (int)(((position % (60 * 60 * 25)) % (60 * 25)) / 25);
    tc.frame = (int)(((position % (60 * 60 * 25)) % (60 * 25)) % 25);
    
    return tc;
}

static string get_prog_no(string magPrefix, string progNo, string prodCode)
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

static string get_date_string(mxfTimestamp input)
{
    char buf[64];
    
    // Infax 'date' time is ignored
    sprintf(buf, "%04d-%02d-%02d", input.year, input.month, input.day);
    
    return buf;
}

static string get_int64_string(int64_t val)
{
    char buf[64];
    sprintf(buf, "%"PRId64"", val);
    
    return buf;
}

static string get_int_string(int val)
{
    return get_int64_string(val);
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

static string string_to_html(string value)
{
    string result;
    size_t i;
    
    result.reserve(value.size());
    
    for (i = 0; i < value.size(); i++)
    {
        if (value[i] == '<')
        {
            result.append("&lt;");
        }
        else if (value[i] == '>')
        {
            result.append("&gt;");
        }
        else if (value[i] == '&')
        {
            result.append("&amp;");
        }
        else if (value[i] == '\n')
        {
            result.append("<br/>\n");
        }
        else
        {
            result.append(1, value[i]);
        }
    }
    
    return result;
}





QCReport::QCReport(string filename, D3MXFFile* mxfFile, QCSessionFile* sessionFile)
: _report(0), _mxfFile(mxfFile), _sessionFile(sessionFile)
{
    _report = fopen(filename.c_str(), "wb");
    if (_report == 0)
    {
        throw RecorderException("Failed to open qc report '%s': %s\n", filename.c_str(), strerror(errno));
    }
}

QCReport::~QCReport()
{
    fclose(_report);
}

void QCReport::write(QCPSEResult pseResult)
{
    const InfaxData& d3InfaxData = _mxfFile->getD3InfaxData();
    const InfaxData& ltoInfaxData = _mxfFile->getLTOInfaxData();
    Mark programmeClip = _sessionFile->getProgrammeClip();
    const std::vector<Mark>& userMarks = _sessionFile->getUserMarks();
    
    string reportHeader = g_qcReportHeader;

    
    set_html_value(reportHeader, "$Filename1", _mxfFile->getFilename());
    set_html_value(reportHeader, "$Filename2", _mxfFile->getFilename());
    
    set_html_value(reportHeader, "$Filename3", _mxfFile->getFilename());
    set_html_value(reportHeader, "$Carrier", ltoInfaxData.spoolNo);
    set_html_value(reportHeader, "$CarrierType", "LTO-3");

    if (programmeClip.position >= 0)
    {
        set_html_value(reportHeader, "$ProgStart", get_duration_string(programmeClip.position));
        set_html_value(reportHeader, "$ProgEnd", get_duration_string(programmeClip.position + programmeClip.duration - 1));
        set_html_value(reportHeader, "$ProgDuration", get_duration_string(programmeClip.duration));
    }
    else
    {
        set_html_value(reportHeader, "$ProgStart", "?");
        set_html_value(reportHeader, "$ProgEnd", "?");        
        set_html_value(reportHeader, "$ProgDuration", get_duration_string(_mxfFile->getMXFDuration()));
    }
    
    set_html_value(reportHeader, "$ClassQCResult", _sessionFile->requireRetransfer() ? "qcresult-fail": "qcresult-pass");
    set_html_value(reportHeader, "$QCResult", _sessionFile->requireRetransfer() ? "FAIL": "PASS");
    string pseResultClass;
    string pseResultStr;
    if (pseResult == PSE_FAILED)
    {
        pseResultClass = "pseresult-fail";
        pseResultStr = "FAIL";
    }
    else if (pseResult == PSE_PASSED)
    {
        pseResultClass = "pseresult-pass";
        pseResultStr = "PASS";
    }
    else
    {
        pseResultClass = "";
        pseResultStr = "UNKNOWN";
    }
    set_html_value(reportHeader, "$PSEResultClass", pseResultClass.c_str());
    set_html_value(reportHeader, "$PSEResult", pseResultStr.c_str());

    set_html_value(reportHeader, "$Comments", string_to_html(_sessionFile->getComments()));
    
    set_html_value(reportHeader, "$ReportDate", get_date_string_now());
    set_html_value(reportHeader, "$Operator", _sessionFile->getOperator());
    set_html_value(reportHeader, "$QCStation", _sessionFile->getStation());
    set_html_value(reportHeader, "$SessionFilename", last_path_component(_sessionFile->getFilename()).c_str());
    set_html_value(reportHeader, "$LoadedSessionFilename", last_path_component(_sessionFile->getLoadedSessionFilename()));

    set_html_value(reportHeader, "$SrcSpoolNo", d3InfaxData.spoolNo);
    set_html_value(reportHeader, "$SrcSpoolType", "D3");
    set_html_value(reportHeader, "$ItemNo", get_int_string(d3InfaxData.itemNo >= 1 ? d3InfaxData.itemNo : 1));
    set_html_value(reportHeader, "$ProgNo", get_prog_no(d3InfaxData.magPrefix, d3InfaxData.progNo, d3InfaxData.prodCode));
    set_html_value(reportHeader, "$ProgTitle", d3InfaxData.progTitle);
    set_html_value(reportHeader, "$EpisodeTitle", d3InfaxData.epTitle);
    set_html_value(reportHeader, "$TxDate", get_date_string(d3InfaxData.txDate));
    set_html_value(reportHeader, "$InfaxDur", get_duration_string(d3InfaxData.duration * 25 /* infax duration is in seconds */));
    

    
    // start writing
    
    fprintf(_report, "%s", reportHeader.c_str());
    
    fprintf(_report, "%s", g_qcMarksHeader);
    
    vector<Mark>::const_iterator markIter;
    for (markIter = userMarks.begin(); markIter != userMarks.end(); markIter++)
    {
        Mark mark = *markIter;
        
        if (mark.duration < 0)
        {
            // skip end of clip mark because it has been included with the start of the clip mark
            continue;
        }
        
        fprintf(_report, "<table class='marks'>\n");
        fprintf(_report, "<tbody>\n");
        
        writeMark(&mark);
        
        // write end mark of clip marks
        if (mark.duration > 0)
        {
            vector<Mark>::const_iterator endMarkIter = markIter;
            endMarkIter++;
            for (; endMarkIter != userMarks.end(); endMarkIter++)
            {
                Mark endMark = *endMarkIter;

                if (endMark.duration < 0 && 
                    mark.position + mark.duration - 1 == endMark.position)
                {
                    writeMark(&endMark);
                    break;
                }
            }
        }

        fprintf(_report, "</tbody>\n");
        fprintf(_report, "</table>\n");
        
    }
    
    fprintf(_report, "%s", g_qcMarksFooter);
    
    fprintf(_report, "%s", g_qcReportFooter);
}

void QCReport::writeMark(Mark* mark)
{
    TimecodeGroup timecodeGroup;
    timecodeGroup.ctc = get_timecode_from_pos(mark->position);
    
    _mxfFile->completeTimecodeInfo(timecodeGroup);
    
    fprintf(_report, "<tr>\n");
    
    fprintf(_report, "<td class='m1 mark-frame-num'>");
    fprintf(_report, "%s", get_int64_string(mark->position).c_str());
    fprintf(_report, "</td>\n");
    fprintf(_report, "<td class='m2'>");
    fprintf(_report, "%s", get_timecode_string(timecodeGroup.ctc).c_str());
    fprintf(_report, "</td>\n");
    fprintf(_report, "<td class='m3'>");
    fprintf(_report, "%s", get_timecode_string(timecodeGroup.vitc).c_str());
    fprintf(_report, "</td>\n");
    fprintf(_report, "<td class='m4'>");
    fprintf(_report, "%s", get_timecode_string(timecodeGroup.ltc).c_str());
    fprintf(_report, "</td>\n");

    if (mark->duration != 0)
    {
        fprintf(_report, "<td class='m5 clip-mark'>");
        if (mark->duration < 0)
        {
            fprintf(_report, "%s", get_int64_string(mark->position + mark->duration + 1).c_str());
        }
        else
        {
            fprintf(_report, "X");
        }
        fprintf(_report, "</td>\n");
        fprintf(_report, "<td class='m6 clip-mark'>");
        if (mark->duration < 0)
        {
            fprintf(_report, "X");
        }
        else
        {
            fprintf(_report, "%s", get_int64_string(mark->position + mark->duration - 1).c_str());
        }
        fprintf(_report, "</td>\n");
    }
    else
    {
        fprintf(_report, "<td class='m5'>");
        fprintf(_report, "</td>\n");
        fprintf(_report, "<td class='m6'>");
        fprintf(_report, "</td>\n");
    }
    fprintf(_report, "<td %s>", (mark->type & TRANSFER_MARK_TYPE) ? "class='m7 transfer-mark'" : "class='m7'");
    fprintf(_report, "%s", (mark->type & TRANSFER_MARK_TYPE) ? "X" : "");
    fprintf(_report, "</td>\n");
    fprintf(_report, "<td %s>", (mark->type & HISTORIC_FAULT_MARK_TYPE) ? "class='m8 historic-mark'" : "class='m8'");
    fprintf(_report, "%s", (mark->type & HISTORIC_FAULT_MARK_TYPE) ? "X" : "");
    fprintf(_report, "</td>\n");
    fprintf(_report, "<td %s>", (mark->type & VIDEO_FAULT_MARK_TYPE) ? "class='m9 video-fault-mark'" : "class='m9'");
    fprintf(_report, "%s", (mark->type & VIDEO_FAULT_MARK_TYPE) ? "X" : "");
    fprintf(_report, "</td>\n");
    fprintf(_report, "<td %s>", (mark->type & AUDIO_FAULT_MARK_TYPE) ? "class='m10 audio-fault-mark'" : "class='m10'");
    fprintf(_report, "%s", (mark->type & AUDIO_FAULT_MARK_TYPE) ? "X" : "");
    fprintf(_report, "</td>\n");
    
    fprintf(_report, "</tr>\n");
}






#else // GEN_QC_REPORT_TEMPLATE 


static const char* g_headerEndMarker = "<!-- HEADER END -->";
static const char* g_marksHeaderStartMarker = "<!-- MARKS HEADER START -->";
static const char* g_marksHeaderEndMarker = "<!-- MARKS HEADER END -->";
static const char* g_marksFooterStartMarker = "<!-- MARKS FOOTER START -->";
static const char* g_marksFooterEndMarker = "<!-- MARKS FOOTER END -->";
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
    
    
    // locate the header, marks and footer boundaries
    
    size_t headerEnd = htmlTemplate.rfind(g_headerEndMarker);
    size_t marksHeaderStart = htmlTemplate.rfind(g_marksHeaderStartMarker);
    size_t marksHeaderEnd = htmlTemplate.rfind(g_marksHeaderEndMarker);
    size_t marksFooterStart = htmlTemplate.rfind(g_marksFooterStartMarker);
    size_t marksFooterEnd = htmlTemplate.rfind(g_marksFooterEndMarker);
    size_t footerStart = htmlTemplate.rfind(g_footerStartMarker);
    
    if (headerEnd == string::npos)
    {
        fprintf(stderr, "Failed to find header end marker: %s\n", g_headerEndMarker);
        return 1;
    }
    if (marksHeaderStart == string::npos)
    {
        fprintf(stderr, "Failed to find marks header start marker: %s\n", g_marksHeaderStartMarker);
        return 1;
    }
    marksHeaderStart += strlen(g_marksHeaderStartMarker);
    if (marksHeaderEnd == string::npos)
    {
        fprintf(stderr, "Failed to find marks header end marker: %s\n", g_marksHeaderEndMarker);
        return 1;
    }
    if (marksFooterStart == string::npos)
    {
        fprintf(stderr, "Failed to find marks footer start marker: %s\n", g_marksFooterStartMarker);
        return 1;
    }
    marksFooterStart += strlen(g_marksFooterStartMarker);
    if (marksFooterEnd == string::npos)
    {
        fprintf(stderr, "Failed to find marks footer end marker: %s\n", g_marksFooterEndMarker);
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
    
    fprintf(cppSourceFile, "const char* g_qcReportHeader = \n");
    fprintf(cppSourceFile, "    \"");
    write_html_string(cppSourceFile, &htmlTemplate[0], headerEnd - 0);
    fprintf(cppSourceFile, "\";\n");
    fprintf(cppSourceFile, "\n");
    fprintf(cppSourceFile, "\n");
    
    fprintf(cppSourceFile, "const char* g_qcMarksHeader = \n");
    fprintf(cppSourceFile, "    \"");
    write_html_string(cppSourceFile, &htmlTemplate[marksHeaderStart], marksHeaderEnd - marksHeaderStart);
    fprintf(cppSourceFile, "\";\n");
    fprintf(cppSourceFile, "\n");
    fprintf(cppSourceFile, "\n");
    
    fprintf(cppSourceFile, "const char* g_qcMarksFooter = \n");
    fprintf(cppSourceFile, "    \"");
    write_html_string(cppSourceFile, &htmlTemplate[marksFooterStart], marksFooterEnd - marksFooterStart);
    fprintf(cppSourceFile, "\";\n");
    fprintf(cppSourceFile, "\n");
    fprintf(cppSourceFile, "\n");
    
    fprintf(cppSourceFile, "const char* g_qcReportFooter = \n");
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
