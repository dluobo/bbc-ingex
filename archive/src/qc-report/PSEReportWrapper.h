/*
 * $Id: PSEReportWrapper.h,v 1.1 2008/07/08 16:25:21 philipn Exp $
 *
 * Wraps the PSEReport class and provides it with the info to write the report 
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

#ifndef __PSE_REPORT_WRAPPER_H__
#define __PSE_REPORT_WRAPPER_H__


#include <string>
#include <vector>

#include "D3MXFFile.h"
#include "QCSessionFile.h"

#include <PSEReport.h>


class PSEReportWrapper
{
public:
    PSEReportWrapper(std::string filename, D3MXFFile* mxfFile, QCSessionFile* sessionFile);
    ~PSEReportWrapper();

    void write();
    
    bool getPSEResult();
    
private:
    void getPSEResultRange(PSEFailure const ** firstPSEFailure, int* numPSEFailures);

    PSEReport* _report;
    D3MXFFile* _mxfFile;
    QCSessionFile* _sessionFile;
};





#endif


