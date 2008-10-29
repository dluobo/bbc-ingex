/*
 * $Id: qc_session.h,v 1.5 2008/10/29 17:47:42 john_f Exp $
 *
 *
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

#ifndef __QC_SESSION_H__
#define __QC_SESSION_H__


#ifdef __cplusplus
extern "C" 
{
#endif


#include "media_player.h"

#define MAX_SESSION_COMMENTS_SIZE       400


typedef struct QCSession QCSession;


int qcs_open(const char* mxfFilename, MediaSource* source, int argc, const char** argv, 
    const char* name, const char* loadedSessionFilename, const char* userName, const char* hostName, 
    QCSession** qcSession);
int qcs_connect_to_player(QCSession* qcSession, MediaPlayer* player);
void qcs_write_marks(QCSession* qcSession, int includeAll, int clipMarkType, const MarkConfigs* markConfigs, Mark* marks, int numMarks);
void qcs_flush(QCSession* qcSession);
void qcs_close(QCSession** qcSession, const char* reportDirectory, const char* sessionScriptName, const char* sessionScriptOptions, int reportAccessPort);

int qcs_restore_session(MediaControl* control, const char* filename, char* sessionComments);

int qcs_is_session_file(const char* filename, const char* mxfFilename);

const char* qcs_get_session_name(QCSession* qcSession);

int qcs_extract_timestamp(const char* sessionFilename, int* year, int* month, int* day, int* hour, int* min, int* sec);

void qcs_set_comments(QCSession* qcSession, const char* sessionComments);


#ifdef __cplusplus
}
#endif


#endif


