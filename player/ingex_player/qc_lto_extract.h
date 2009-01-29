/*
 * $Id: qc_lto_extract.h,v 1.4 2009/01/29 07:10:27 stuart_hc Exp $
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

#ifndef __QC_LTO_EXTRACT_H__
#define __QC_LTO_EXTRACT_H__


typedef enum
{
    LTO_STARTING_STATUS = 0,
    LTO_POLL_FAILED_STATUS,
    LTO_NO_TAPE_DEVICE_ACCESS_STATUS,
    LTO_NO_TAPE_STATUS,
    LTO_BUSY_LOAD_OR_EJECT_STATUS,
    LTO_BUSY_STATUS,
    LTO_BUSY_EXTRACTING_FILE_STATUS,
    LTO_BUSY_REWINDING_STATUS,
    LTO_READ_FAILED_STATUS,
    LTO_BAD_TAPE_STATUS,
    LTO_ONLINE_STATUS,
    LTO_OPERATION_FAILED_STATUS,
    LTO_BUSY_EXTRACTING_INDEX_STATUS,
    LTO_REWIND_FAILED_STATUS,
    LTO_INDEX_EXTRACT_FAILED_STATUS,
    LTO_READY_TO_EXTRACT_STATUS,
    LTO_BUSY_SEEKING_STATUS,
    LTO_FILE_EXTRACT_FAILED_STATUS,
    LTO_BUSY_LOAD_POS_STATUS,
    LTO_DISK_FULL_STATUS,
    LTO_TAPE_EMPTY_OR_IO_ERROR_STATUS
} QCLTOExtractStatus;

typedef struct
{
    QCLTOExtractStatus status;
    char ltoSpoolNumber[32];
    char currentExtractingFile[32];
    int extractAll;
    int updated;
} QCLTOExtractState;

typedef struct QCLTOExtract QCLTOExtract;


int qce_create_lto_extract(const char* cacheDirectory, const char* tapeDevice, QCLTOExtract** extract);
void qce_free_lto_extract(QCLTOExtract** extract);

void qce_start_extract(QCLTOExtract* extract, const char* ltoSpoolNumber, const char* filename);
void qce_start_extract_all(QCLTOExtract* extract, const char* ltoSpoolNumber);
void qce_start_extract_remainder(QCLTOExtract* extract, const char* ltoSpoolNumber);
void qce_stop_extract(QCLTOExtract* extract);

void qce_get_lto_state(QCLTOExtract* extract, QCLTOExtractState* status, int updateMask);

int qce_is_current_lto(QCLTOExtract* extract, const char* ltoNumber);
int qce_is_extracting(QCLTOExtract* extract, const char* ltoNumber, const char* filename);
int qce_is_extracting_from(QCLTOExtract* extract, const char* ltoNumber);
int qce_is_extracting_all_from(QCLTOExtract* extract, const char* ltoNumber);

void qce_set_current_play_name(QCLTOExtract* extract, const char* directory, const char* name);

int qce_can_play(QCLTOExtract* extract, const char* directory, const char* name);

int qce_is_seeking(QCLTOExtract* extract);

#endif


