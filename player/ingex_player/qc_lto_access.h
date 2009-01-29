/*
 * $Id: qc_lto_access.h,v 1.4 2009/01/29 07:10:27 stuart_hc Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Philip de Nier
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

#ifndef __QC_LTO_ACCESS_H__
#define __QC_LTO_ACCESS_H__


#include "media_player.h"
#include "qc_lto_extract.h"


typedef struct QCLTOAccess QCLTOAccess;


int qla_create_qc_lto_access(const char* cacheDirectory, QCLTOExtract* extract,
    const char* deleteScriptName, const char* deleteScriptOptions, QCLTOAccess** access);
void qla_free_qc_lto_access(QCLTOAccess** access);

int qla_connect_to_player(QCLTOAccess* access, MediaPlayer* player);

int qla_get_file_to_play(QCLTOAccess* access, char directory[FILENAME_MAX], char name[FILENAME_MAX],
    char sessionName[FILENAME_MAX]);

void qla_set_current_session_name(QCLTOAccess* access, const char* sessionName);

void qla_set_current_play_name(QCLTOAccess* access, const char* directory, const char* name);

void qla_remove_old_sessions(QCLTOAccess* access, const char* directory, const char* name);


#endif


