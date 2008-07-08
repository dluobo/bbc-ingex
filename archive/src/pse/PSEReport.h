/*
 * $Id: PSEReport.h,v 1.1 2008/07/08 16:24:58 philipn Exp $
 *
 * Used to write a PSE report.
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

#ifndef __PSE_REPORT_H__
#define __PSE_REPORT_H__


#include "archive_types.h"

#include <string>


extern const char* g_pseReportHeader;
extern const char* g_pseFailuresHeader;
extern const char* g_pseFailuresFooter;
extern const char* g_pseReportFooter;


class PSEReport
{
public:
    static PSEReport* open(std::string filename);

    static bool hasPassed(const PSEFailure* results, int numResults);
    
public:
    ~PSEReport();

    bool write(int64_t firstAnalysisFrame, int64_t lastAnalysisFrame,
        std::string mxfFilename, const InfaxData* infaxData,
        const PSEFailure* results, int numResults, bool* passed);
    
private:
    PSEReport(FILE* reportFile);
    
    FILE* _reportFile;
};




#endif


