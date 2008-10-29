/*
 * $Id: qc_lto_access.c,v 1.4 2008/10/29 17:47:42 john_f Exp $
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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/statvfs.h>

#include "qc_lto_access.h"
#include "qc_session.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"

#define __USE_ISOC99   1
#include <limits.h> /* for LLONG_MIN and LLONG_MAX */


#define INDEX_ENTRY_ALLOC_STEP      20

#define TAPE_DIR_ALLOC_STEP         40
#define TAPE_DIR_ENTRY_ALLOC_STEP   20

/* if length of file name is > this value, then it is truncated and suffixed with '...' in the OSD menu */
#define MAX_MXF_NAME_LENGTH_IN_OSD              25
#define MAX_SELECTED_MXF_NAME_LENGTH_IN_OSD     30

#define MAX_SELECT_SESSION          10
#define MAX_KEEP_SESSION            20

/* the first entry is '../' and optional second is 'Extract all' */
#define OSD_TAPE_DIR_ENTRY_INDEX(tapeDir, index)     (index + 1 + ((tapeDir)->haveExtractAllOption ? 1 : 0))


#if defined(DISABLE_QC_LTO_ACCESS)
/* For systems without <sys/inotify.h> */

int qla_create_qc_lto_access(const char* cacheDirectory, QCLTOExtract* extract, 
    const char* deleteScriptName, const char* deleteScriptOptions, QCLTOAccess** access)
{
    return 0;
}
void qla_free_qc_lto_access(QCLTOAccess** access)
{
}
int qla_connect_to_player(QCLTOAccess* access, MediaPlayer* player)
{
    return 0;
}
int qla_get_file_to_play(QCLTOAccess* access, char directory[FILENAME_MAX], char name[FILENAME_MAX], 
    char sessionName[FILENAME_MAX])
{
    return 0;
}
void qla_set_current_session_name(QCLTOAccess* access, const char* sessionName)
{
}
void qla_set_current_play_name(QCLTOAccess* access, const char* directory, const char* name)
{
}
void qla_remove_old_sessions(QCLTOAccess* access, const char* directory, const char* name)
{
}

#else

#include <sys/inotify.h>

typedef struct
{
    char name[256];
    time_t modTime;
} SessionInfo; 

typedef struct
{
    char* directory;
    char* name;
    
    char* sessionNames[MAX_SELECT_SESSION];
    
    int haveStartExtractOption;
    int haveStopExtractOption;
    
    OSDMenuModel* osdMenu;
} PlaySelect;

typedef struct
{
    char* directory;
    
    int haveStopExtractAllOption;
    int haveStartExtractAllOptions;
    
    OSDMenuModel* osdMenu;
} ExtractAll;

typedef struct
{
    char* filename;
    char* name;
    
    OSDMenuModel* osdMenu;
} DeleteTapeDir;

typedef struct
{
    char name[32];
    int64_t fileSize;
    int isSelfReference;
} IndexFileEntry;

typedef struct
{
    char* filename;
    const char* name; /* references into the filename */
    
    IndexFileEntry* entries;
    int numEntries;
    int allocEntries;
} IndexFile;

typedef struct
{
    char* filename;
    const char* name; /* references into the filename */
    int64_t sizeOnTape;
    int64_t sizeOnDisk;
    int extracting;
} TapeDirEntry;

typedef struct
{
    char* filename;
    const char* name; /* references into the filename */
    
    TapeDirEntry* entries;
    int numEntries;
    int allocEntries;

    int extracting;
    
    IndexFile indexFile;
    
    OSDMenuModel* osdMenu;
    int haveExtractAllOption;
    
    int wd;
} TapeDirectory;

typedef struct
{
    TapeDirectory* tapeDirs;
    int numTapeDirs;
    int allocTapeDirs;
    
    OSDMenuModel* osdMenu;
} CacheContents;

typedef enum
{
    ROOT_MENU_ACTIVE = 0,
    TAPE_DIR_MENU_ACTIVE,
    PLAY_SELECT_MENU_ACTIVE,
    EXTRACT_ALL_MENU_ACTIVE,
    DELETE_TAPE_DIR_MENU_ACTIVE
} ActiveMenu;

struct QCLTOAccess
{
    QCLTOExtract* extract;
    char* cacheDirName;

    char* deleteScriptName;
    char* deleteScriptOptions;

    MenuHandler menuHandler;
    
    MenuHandlerListener* listener;
    MediaControl* mediaControl; 
    
    OnScreenDisplay* osd;
    
    CacheContents cache;
    int cacheWD;
    pthread_mutex_t cacheMutex;
    
    ActiveMenu activeMenu;
    
    PlaySelect playSelect;

    ExtractAll extractAll;

    DeleteTapeDir deleteTapeDir;
    
    char* selectedDirectory;
    char* selectedName;
    char* selectedSessionName;
    
    char* currentSessionName;
    
    char* currentPlayLTONumber;
    char* currentPlayName;
    
    int stopping;
	pthread_t workerThreadId;
    
    int inotifyFD;
};


static const char* g_menuTitle = "Quality Check Player";

/* this array should match the QCLTOExtractStatus enum */
static const char* g_extractStatusString[] = 
{ 
    "Ready",                    /* LTO_STARTING_STATUS */
    "Tape status poll failed",  /* LTO_POLL_FAILED_STATUS */
    "Device access failed",     /* LTO_NO_TAPE_DEVICE_ACCESS_STATUS */
    "No tape",                  /* LTO_NO_TAPE_STATUS */
    "Busy loading or ejecting", /* LTO_BUSY_LOAD_OR_EJECT_STATUS */
    "Tape eject or in use",     /* LTO_BUSY_STATUS */
    "Extracting file",          /* LTO_BUSY_EXTRACTING_FILE_STATUS */
    "Rewinding",                /* LTO_BUSY_REWINDING_STATUS */
    "Read failed",              /* LTO_READ_FAILED_STATUS */
    "Bad tape",                 /* LTO_BAD_TAPE_STATUS */
    "Online",                   /* LTO_ONLINE_STATUS */
    "Operation failed",         /* LTO_OPERATION_FAILED_STATUS */
    "Extracting index",         /* LTO_BUSY_EXTRACTING_INDEX_STATUS */
    "Rewind failed",            /* LTO_REWIND_FAILED_STATUS */
    "Index extract failed",     /* LTO_INDEX_EXTRACT_FAILED_STATUS */
    "Ready to extract",         /* LTO_READY_TO_EXTRACT_STATUS */
    "Seeking",                  /* LTO_BUSY_SEEKING_STATUS */
    "File extract failed",      /* LTO_FILE_EXTRACT_FAILED_STATUS */
    "Checking tape position",   /* LTO_BUSY_LOAD_POS_STATUS */
    "Not enough space on disk", /* LTO_DISK_FULL_STATUS */
    "Empty tape or I/O error"    /* LTO_TAPE_EMPTY_OR_IO_ERROR_STATUS */
};
static const char* g_unknownStatusString = "Unknown";


static int get_file_stat(const char* name1, const char* name2, const char* name3, struct stat* statBuf)
{
    char filepath[FILENAME_MAX];
    int result;

    strcpy(filepath, name1);
    if (name2 != NULL)
    {
        strcat_separator(filepath);
        strcat(filepath, name2);
        if (name3 != NULL)
        {
            strcat_separator(filepath);
            strcat(filepath, name3);
        }
    }
    
    result = stat(filepath, statBuf);
    
    return result == 0;
}


static void get_sessions(QCLTOAccess* access, const char* directory, const char* name, SessionInfo sessions[MAX_SELECT_SESSION])
{
    DIR* tapeDirStream = NULL;
    struct dirent* tapeDirent;
    struct stat statBuf;
    int i;

    if ((tapeDirStream = opendir(directory)) != NULL)
    {
        while ((tapeDirent = readdir(tapeDirStream)) != NULL)
        {
            /* include in list if it is not the current session file and is a session file */
            if ((access->currentSessionName == NULL ||
                    strcmp(access->currentSessionName, tapeDirent->d_name) != 0) &&
                qcs_is_session_file(tapeDirent->d_name, name) &&
                get_file_stat(directory, tapeDirent->d_name, NULL, &statBuf))
            {
                for (i = 0; i < MAX_SELECT_SESSION; i++)
                {
                    if (statBuf.st_mtime > sessions[i].modTime)
                    {
                        /* shift sessions up if i is not the index of the last one*/
                        if (i < MAX_SELECT_SESSION - 1)
                        {
                            memmove(&sessions[i + 1], &sessions[i], (MAX_SELECT_SESSION - 1 - i) * sizeof(SessionInfo));
                        }
                        
                        /* set session */
                        sessions[i].modTime = statBuf.st_mtime;
                        strncpy(sessions[i].name, tapeDirent->d_name, 256);
                        sessions[i].name[255] = '\0';
                        break;
                    }
                }
            }
        }
        closedir(tapeDirStream);
    }
}


static void clear_index_file(IndexFile* indexFile)
{
    SAFE_FREE(&indexFile->entries);
    indexFile->numEntries = 0;
    indexFile->allocEntries = 0;
}

static void free_cache_osd_menus(QCLTOAccess* access)
{
    int i;
                                                                                       
    if (access->osd != NULL)
    {
        for (i = 0; i < access->cache.numTapeDirs; i++)
        {
            if (access->cache.tapeDirs[i].osdMenu != NULL)
            {
                osd_free_menu_model(access->osd, &access->cache.tapeDirs[i].osdMenu); 
            }
        }
    
        if (access->cache.osdMenu != NULL)
        {
            osd_free_menu_model(access->osd, &access->cache.osdMenu);
        }
    
        if (access->playSelect.osdMenu != NULL)
        {
            osd_free_menu_model(access->osd, &access->playSelect.osdMenu);
        }
        
        if (access->extractAll.osdMenu != NULL)
        {
            osd_free_menu_model(access->osd, &access->extractAll.osdMenu);
        }
        
        if (access->deleteTapeDir.osdMenu != NULL)
        {
            osd_free_menu_model(access->osd, &access->deleteTapeDir.osdMenu);
        }
    }
}

static void free_cache_contents(QCLTOAccess* access)
{
    int i;
    int k;

    free_cache_osd_menus(access);
    
    for (i = 0; i < access->cache.numTapeDirs; i++)
    {
        TapeDirectory* tapeDir = &access->cache.tapeDirs[i];
        
        if (tapeDir->wd >= 0)
        {
            if (inotify_rm_watch(access->inotifyFD, tapeDir->wd) != 0)
            {
                ml_log_warn("Failed to remove tape directory watch: %s\n", strerror(errno));
            }
        }

        clear_index_file(&tapeDir->indexFile);
        SAFE_FREE(&tapeDir->indexFile.filename);
        tapeDir->indexFile.name = NULL;
        
        for (k = 0; k < tapeDir->numEntries; k++)
        {
            SAFE_FREE(&tapeDir->entries[k].filename);
            tapeDir->entries[k].name = NULL;
        }
        SAFE_FREE(&tapeDir->entries);
        tapeDir->numEntries = 0;
        tapeDir->allocEntries = 0;
        SAFE_FREE(&tapeDir->filename);
        tapeDir->name = NULL;
    }
    SAFE_FREE(&access->cache.tapeDirs);

    for (i = 0; i < MAX_SELECT_SESSION; i++)
    {
        SAFE_FREE(&access->playSelect.sessionNames[i]);
    }
    SAFE_FREE(&access->playSelect.directory);
    SAFE_FREE(&access->playSelect.name);

    SAFE_FREE(&access->extractAll.directory);
    
    SAFE_FREE(&access->deleteTapeDir.filename);
    SAFE_FREE(&access->deleteTapeDir.name);
    
    memset(&access->cache, 0, sizeof(access->cache));
}


static void get_status_string(QCLTOAccess* access, char statusString[40], QCLTOExtractState* extractState,
    int updateMask)
{
    if (access->extract != NULL)
    {
        strcpy(statusString, "Status: ");
        
        qce_get_lto_state(access->extract, extractState, updateMask);
        if (extractState->status < (int)sizeof(g_extractStatusString) / sizeof(const char*))
        {
            strcat(statusString, g_extractStatusString[extractState->status]);
        }
        else
        {
            strcat(statusString, g_unknownStatusString);
        }
    }
    else
    {
        strcpy(statusString, "");
    }
}

static void get_disk_free_status_string(QCLTOAccess* access, char statusString[40])
{
    struct statvfs statvfsBuf;
    
    strcpy(statusString, "\tDisk space: ");
    
    if (statvfs(access->cacheDirName, &statvfsBuf) == 0)
    {
        sprintf(statusString + strlen(statusString), "%.1f GB", 
            (statvfsBuf.f_bfree * statvfsBuf.f_bsize) / (1.0e9));
    }
    else
    {
        strcat(statusString, "unknown");
    }
}

static void switch_active_menu(QCLTOAccess* access, OSDMenuModel* osdMenu, ActiveMenu activeMenu)
{
    char statusString[40];
    QCLTOExtractState extractState;

    if (osdMenu == NULL)
    {
        return;
    }
    
    get_status_string(access, statusString, &extractState, 0);

    osdm_lock(osdMenu);
    osdm_set_status(osdMenu, statusString);
    osdm_unlock(osdMenu);
    
    osd_set_active_menu_model(access->osd, 0x01, osdMenu);
    access->activeMenu = activeMenu;
}


static int is_empty_line(char line[128])
{
    int i;
    for (i = 0; i < 128; i++)
    {
        if (line[i] == '\0')
        {
            break;
        }
        else if (line[i] != ' ')
        {
            return 0;
        }
    }
    return 1;
}

static int read_index_file_line(FILE* file, int* lastChar, char line[128])
{
    char* currentChar = &line[0];
    char* maxChar = &line[127];
    int c;
    
    if (*lastChar == EOF)
    {
        return 0;
    }
    
    /* read until '\r'+'\n' or '\n' or EOF */
    c = fgetc(file);
    while (c != EOF && c != '\r' && c != '\n' && currentChar != maxChar)
    {
        *currentChar++ = c;
        c = fgetc(file);
    }
    /* skip '\n' following a '\r' */
    if (c == '\r' && currentChar != maxChar)
    {
        c = fgetc(file);
    }
    
    *currentChar++ = '\0';
    *lastChar = c;
    return currentChar <= maxChar;
}

static int parse_index_file_entry(const char* line, IndexFileEntry* entry)
{
    const char* linePtr = line;
    char* namePtr = &entry->name[0];
    char* maxNamePtr = &entry->name[31];
    size_t len;
    long long fileSize;
    
    /* skip the entry number, terminated by a '\t' or ' ' */
    while (*linePtr != '\0' && 
        (*linePtr != '\t' && *linePtr != ' '))
    {
        linePtr++;
    }
    if (*linePtr != '\t' && *linePtr != ' ')
    {
        ml_log_error("Failed to skip index entry number\n");
        return 0;
    }
    while (*linePtr == '\t' || *linePtr == ' ')
    {
        linePtr++;
    }
    
    /* parse the name */
    while (*linePtr != '\0' && (*linePtr != '\t' && *linePtr != ' ') && namePtr != maxNamePtr)
    {
        *namePtr++ = *linePtr++;
    }
    if (namePtr == entry->name)
    {
        ml_log_error("Zero length index entry name\n");
        return 0;
    }
    else if (namePtr == maxNamePtr)
    {
        ml_log_error("Index entry name length exceeded expected maximum %d\n", 31);
        return 0;
    }
    *namePtr++ = '\0';
    
    if (*linePtr == '\0')
    {
        ml_log_error("Missing file size in index entry\n");
        return 0;
    }
    
    /* skip '\t' and ' ' */
    while (*linePtr == '\t' || *linePtr == ' ')
    {
        linePtr++;
    }
    fileSize = strtoll(linePtr, NULL, 10);
    if (fileSize == LLONG_MAX || fileSize == LLONG_MIN)
    {
        ml_log_error("Failed to get file size from index entry\n");
        return 0;
    }
    entry->fileSize = fileSize;
    
    len = strlen(entry->name);
    if (len >= 4 && strcmp(".txt", &entry->name[len - 4]) == 0)
    {
        entry->isSelfReference = 1;
    }
    
    return 1;
}

static int parse_index_file(const char* filename, IndexFile* indexFile)
{
    FILE* file = NULL;
    char line[128];
    int lastChar;
    IndexFileEntry* entries = NULL;
    
    /* free */
    clear_index_file(indexFile);
    SAFE_FREE(&indexFile->filename);
    indexFile->name = NULL;
    
    /* set filename and name */
    CALLOC_ORET(indexFile->filename, char, strlen(filename) + 1);
    strcpy(indexFile->filename, filename);
    indexFile->name = strrchr(indexFile->filename, '/');
    if (indexFile->name == NULL)
    {
        /* filename has no path elements */
        indexFile->name = indexFile->filename;
    }
    else
    {
        /* skip over the '/' */
        indexFile->name++;
    }
    
    if ((file = fopen(filename, "rb")) == NULL)
    {
        ml_log_error("Failed to open index file '%s'\n", filename);
        return 0;
    }
    
    /* skip LTO number */
    lastChar = 0;
    if (!read_index_file_line(file, &lastChar, line))
    {
        ml_log_error("Index file is incomplete - missing LTO number\n");
        goto fail;
    }
    
    /* parse entries, terminated by an empty line */
    while (read_index_file_line(file, &lastChar, line) && !is_empty_line(line))
    {
        if (indexFile->allocEntries < indexFile->numEntries + 1)
        {
            CALLOC_ORET(entries, IndexFileEntry, indexFile->allocEntries + INDEX_ENTRY_ALLOC_STEP);
            memcpy(entries, indexFile->entries, indexFile->allocEntries * sizeof(IndexFileEntry));
            SAFE_FREE(&indexFile->entries);
            indexFile->entries = entries;
            entries = NULL;
            indexFile->allocEntries += INDEX_ENTRY_ALLOC_STEP;
        }
        
        if (!parse_index_file_entry(line, &indexFile->entries[indexFile->numEntries]))
        {
            ml_log_error("Failed to parse index file entry: '%s'\n", line);
            goto fail;
        }
        indexFile->numEntries++;
    }
    
    
    fclose(file);
    return 1;
    
fail:
    SAFE_FREE(&entries);
    clear_index_file(indexFile);
    if (file != NULL)
    {
        fclose(file);
    }
    return 0;
}

static void get_tape_dir_entry_line(QCLTOAccess* access, TapeDirectory* tapeDir, TapeDirEntry* entry, char lineText[48])
{
    char percentageString[5];
    char sizeString[16];
    
    strcpy(lineText, "\t\t");
    strncat(lineText, entry->name, MAX_MXF_NAME_LENGTH_IN_OSD);
    if (strlen(entry->name) > MAX_MXF_NAME_LENGTH_IN_OSD)
    {
        strcat(lineText, "...");
    }
    if (entry->sizeOnTape >= 0)
    {
        if (entry->sizeOnDisk >= 0)
        {
            if (entry->sizeOnTape >= entry->sizeOnDisk)
            {
                /* file exists on tape and on disk */
                strcat(lineText, ">>");
                sprintf(percentageString, "%d%%", 
                    (int)(100 * (double)entry->sizeOnDisk / (double)entry->sizeOnTape));
                strcat(lineText, percentageString);
                sprintf(sizeString, " %.1f", (double)entry->sizeOnTape / 1000000000.0);
                strcat(lineText, sizeString);
                strcat(lineText, " DT");
            }
            else
            {
                /* file exists on tape and on disk, but disk size exceeds tape size */
                /* this can happen if the size in the index file does not equal the actual size */
                strcat(lineText, ">>+100%");
                sprintf(sizeString, " %.1f", (double)entry->sizeOnTape / 1000000000.0);
                strcat(lineText, sizeString);
                strcat(lineText, " DT");
            }
        }
        else
        {
            /* file does not exist on disk */
            strcat(lineText, ">>");
            sprintf(sizeString, " %.1f", (double)entry->sizeOnTape / 1000000000.0);
            strcat(lineText, sizeString);
            strcat(lineText, " _T");
        }
    }
    else
    {
        /* file does not exist on tape */
        strcat(lineText, ">>");
        sprintf(sizeString, " %.1f", (double)entry->sizeOnDisk / 1000000000.0);
        strcat(lineText, sizeString);
        strcat(lineText, " D_");
    }
}

static int remove_tape_dir_entry(QCLTOAccess* access, TapeDirectory* tapeDir, int entryIndex)
{
    if (entryIndex >= tapeDir->numEntries)
    {
        return 0;
    }

    /* free tape dir entry */
    SAFE_FREE(&tapeDir->entries[entryIndex].filename);
    memset(&tapeDir->entries[entryIndex], 0, sizeof(tapeDir->entries[entryIndex]));
    /* note: we don't bother deallocating and reallocating - we just move the entries */    
    if (entryIndex < tapeDir->numEntries - 1)
    {
        memmove(&tapeDir->entries[entryIndex], &tapeDir->entries[entryIndex + 1], 
            (tapeDir->numEntries - 1 - entryIndex) * sizeof(TapeDirEntry));
    }
    tapeDir->numEntries--;
    
    /* remove from OSD menu if present */
    if (tapeDir->osdMenu != NULL)
    {
        osdm_lock(tapeDir->osdMenu);
        osdm_remove_list_item(tapeDir->osdMenu, OSD_TAPE_DIR_ENTRY_INDEX(tapeDir, entryIndex));
        osdm_unlock(tapeDir->osdMenu);
    }
    
    return 1;
}

static int remove_tape_dir(QCLTOAccess* access, int tapeDirIndex)
{
    int k;
    
    if (tapeDirIndex >= access->cache.numTapeDirs)
    {
        return 0;
    }

    /* remove inotify watch */
    if (access->cache.tapeDirs[tapeDirIndex].wd >= 0)
    {
        if (inotify_rm_watch(access->inotifyFD, access->cache.tapeDirs[tapeDirIndex].wd) != 0)
        {
            ml_log_warn("Failed to remove tape directory watch: %s\n", strerror(errno));
        }
    }
    
    /* free tape dir */
    for (k = 0; k < access->cache.tapeDirs[tapeDirIndex].numEntries; k++)
    {
        SAFE_FREE(&access->cache.tapeDirs[tapeDirIndex].entries[k].filename);
        if (access->osd != NULL && access->cache.tapeDirs[tapeDirIndex].osdMenu != NULL)
        {
            osd_free_menu_model(access->osd, &access->cache.tapeDirs[tapeDirIndex].osdMenu); 
        }
    }
    clear_index_file(&access->cache.tapeDirs[tapeDirIndex].indexFile);
    SAFE_FREE(&access->cache.tapeDirs[tapeDirIndex].indexFile.filename);
    SAFE_FREE(&access->cache.tapeDirs[tapeDirIndex].entries);
    SAFE_FREE(&access->cache.tapeDirs[tapeDirIndex].filename);
    
    
    /* note: we don't bother deallocating and reallocating - we just move the entries */    
    memset(&access->cache.tapeDirs[tapeDirIndex], 0, sizeof(access->cache.tapeDirs[tapeDirIndex]));
    if (tapeDirIndex < access->cache.numTapeDirs - 1)
    {
        memmove(&access->cache.tapeDirs[tapeDirIndex], &access->cache.tapeDirs[tapeDirIndex + 1], 
            (access->cache.numTapeDirs - 1 - tapeDirIndex) * sizeof(TapeDirectory));
    }
    access->cache.numTapeDirs--;
    
    if (access->cache.osdMenu != NULL)
    {
        osdm_lock(access->cache.osdMenu);
        osdm_remove_list_item(access->cache.osdMenu, tapeDirIndex);
        osdm_unlock(access->cache.osdMenu);
    }
    
    return 1;
}

static TapeDirEntry* add_tape_dir_entry(QCLTOAccess* access, TapeDirectory* tapeDir, const char* name,
    int64_t sizeOnDisk, int64_t sizeOnTape)
{
    TapeDirEntry* entries = NULL;
    int index;
    OSDMenuListItem* listItem;
    char lineText[48];
    int sizeUpdated;

    
    /* check if entry is already present */    
    for (index = 0; index < tapeDir->numEntries; index++)
    {
        if (strcmp(name, tapeDir->entries[index].name) == 0)
        {
            /* set sizes changed */
            sizeUpdated = (sizeOnDisk >= 0 && tapeDir->entries[index].sizeOnDisk != sizeOnDisk) ||
                (sizeOnTape >= 0 && tapeDir->entries[index].sizeOnTape != sizeOnTape);
                
            if (sizeUpdated)
            {
                if (sizeOnDisk >= 0)
                {
                    tapeDir->entries[index].sizeOnDisk = sizeOnDisk;
                }
                if (sizeOnTape >= 0)
                {
                    tapeDir->entries[index].sizeOnTape = sizeOnTape;
                }
    
                /* update line in OSD menu */
                if (tapeDir->osdMenu != NULL)
                {
                    get_tape_dir_entry_line(access, tapeDir, &tapeDir->entries[index], lineText);
                    
                    osdm_lock(tapeDir->osdMenu);
                    if (!osdm_set_list_item_text(tapeDir->osdMenu, 
                        osdm_get_list_item(tapeDir->osdMenu, OSD_TAPE_DIR_ENTRY_INDEX(tapeDir, index)), lineText))
                    {
                        ml_log_error("Failed to update tape dir entry in OSD menu\n");
                    }
                    osdm_unlock(tapeDir->osdMenu);
                }
            }
            
            return &tapeDir->entries[index];
        }
    }
        
    /* allocate space if neccessary */
    if (tapeDir->allocEntries < tapeDir->numEntries + 1)
    {
        CALLOC_ORET(entries, TapeDirEntry, tapeDir->allocEntries + TAPE_DIR_ENTRY_ALLOC_STEP);
        memcpy(entries, tapeDir->entries, tapeDir->allocEntries * sizeof(TapeDirEntry));
        SAFE_FREE(&tapeDir->entries);
        tapeDir->entries = entries;
        entries = NULL;
        tapeDir->allocEntries += TAPE_DIR_ENTRY_ALLOC_STEP;
    }
    
    /* find index into sorted array, and make space for new entry */
    for (index = 0; index < tapeDir->numEntries; index++)
    {
        if (strcmp(name, tapeDir->entries[index].name) < 0)
        {
            memmove(&tapeDir->entries[index + 1], &tapeDir->entries[index], 
                (tapeDir->numEntries - index) * sizeof(TapeDirEntry));
            break;
        }
    }
    
    /* create new entry */
    memset(&tapeDir->entries[index], 0, sizeof(tapeDir->entries[index]));
    tapeDir->entries[index].sizeOnDisk = sizeOnDisk;
    tapeDir->entries[index].sizeOnTape = sizeOnTape;
    CALLOC_ORET(tapeDir->entries[index].filename, char,
        strlen(tapeDir->filename) + 1 + strlen(name) + 1);
    strcpy(tapeDir->entries[index].filename, tapeDir->filename);
    strcat_separator(tapeDir->entries[index].filename);
    tapeDir->entries[index].name = tapeDir->entries[index].filename + 
        strlen(tapeDir->entries[index].filename); 
    strcat(tapeDir->entries[index].filename, name);

    tapeDir->numEntries++;
    

    /* add line to OSD menu */
    if (tapeDir->osdMenu != NULL)
    {
        get_tape_dir_entry_line(access, tapeDir, &tapeDir->entries[index], lineText);
        
        osdm_lock(tapeDir->osdMenu);
        if (!osdm_insert_list_item(tapeDir->osdMenu, OSD_TAPE_DIR_ENTRY_INDEX(tapeDir, index), &listItem))
        {
            ml_log_error("Failed to add tape dir entry to OSD menu\n");
        }
        else
        {
            if (!osdm_set_list_item_text(tapeDir->osdMenu, listItem, lineText))
            {
                ml_log_error("Failed to update tape dir entry in OSD menu\n");
            }
        }
        osdm_unlock(tapeDir->osdMenu);
    }
    
    return &tapeDir->entries[index];
}

static TapeDirectory* add_tape_dir(QCLTOAccess* access, const char* name)
{
    TapeDirectory* tapeDirs = NULL;
    int index;
    char lineText[33];
    OSDMenuListItem* listItem;
    
    
    /* check if tape directory is already present */    
    for (index = 0; index < access->cache.numTapeDirs; index++)
    {
        if (strcmp(name, access->cache.tapeDirs[index].name) == 0)
        {
            /* already present */
            return &access->cache.tapeDirs[index];
        }
    }

    /* allocate space if neccessary */
    if (access->cache.allocTapeDirs < access->cache.numTapeDirs + 1)
    {
        CALLOC_ORET(tapeDirs, TapeDirectory, access->cache.allocTapeDirs + TAPE_DIR_ALLOC_STEP);
        memcpy(tapeDirs, access->cache.tapeDirs, access->cache.allocTapeDirs * sizeof(TapeDirectory));
        SAFE_FREE(&access->cache.tapeDirs);
        access->cache.tapeDirs = tapeDirs;
        tapeDirs = NULL;
        access->cache.allocTapeDirs += TAPE_DIR_ALLOC_STEP;
    }
    
    /* find index into sorted array, and make space for new entry */
    for (index = 0; index < access->cache.numTapeDirs; index++)
    {
        if (strcmp(name, access->cache.tapeDirs[index].name) < 0)
        {
            memmove(&access->cache.tapeDirs[index + 1], &access->cache.tapeDirs[index], 
                (access->cache.numTapeDirs - index) * sizeof(TapeDirectory));
            break;
        }
    }
    
    /* create new entry */
    memset(&access->cache.tapeDirs[index], 0, sizeof(access->cache.tapeDirs[index]));
    access->cache.tapeDirs[index].wd = -1;
    CALLOC_ORET(access->cache.tapeDirs[index].filename, char,
        strlen(access->cacheDirName) + 1 + strlen(name) + 1);
    strcpy(access->cache.tapeDirs[index].filename, access->cacheDirName);
    strcat_separator(access->cache.tapeDirs[index].filename);
    access->cache.tapeDirs[index].name = access->cache.tapeDirs[index].filename + 
        strlen(access->cache.tapeDirs[index].filename); 
    strcat(access->cache.tapeDirs[index].filename, name);

    access->cache.numTapeDirs++;

    /* add line to OSD menu */
    if (access->cache.osdMenu != NULL)
    {
        strcpy(lineText, "\t\t");
        strcat(lineText, access->cache.tapeDirs[index].name);
        
        osdm_lock(access->cache.osdMenu);
        
        if (!osdm_insert_list_item(access->cache.osdMenu, index, &listItem))
        {
            ml_log_error("Failed to add tape dir to OSD menu\n");
        }
        else
        {
            if (!osdm_set_list_item_text(access->cache.osdMenu, listItem, lineText))
            {
                ml_log_error("Failed to update cache tape dir in OSD menu\n");
            }
        }
        
        osdm_unlock(access->cache.osdMenu);
    }
    
    
    return &access->cache.tapeDirs[index];
}


static int process_index_file(QCLTOAccess* access, TapeDirectory* tapeDir)
{
    TapeDirEntry* tapeDirEntry;
    int i;
    int k;
    char lineText[48];
    
    /* add entries from the index file and set the tape file sizes */
    for (i = 0; i < tapeDir->indexFile.numEntries; i++)
    {
        /* skip the self-referencing entry */
        if (tapeDir->indexFile.entries[i].isSelfReference)
        {
            continue;
        }
        
        for (k = 0; k < tapeDir->numEntries; k++)
        {
            int result = strcmp(tapeDir->indexFile.entries[i].name, tapeDir->entries[k].name);
            if (result < 0)
            {
                /* insert entry from index file */
                tapeDirEntry = add_tape_dir_entry(access, tapeDir, tapeDir->indexFile.entries[i].name,
                    -1, tapeDir->indexFile.entries[i].fileSize);
                break;
            }
            else if (result == 0)
            {
                tapeDir->entries[k].sizeOnTape = tapeDir->indexFile.entries[i].fileSize;
                
                /* update line in OSD menu */
                if (tapeDir->osdMenu != NULL)
                {
                    get_tape_dir_entry_line(access, tapeDir, &tapeDir->entries[k], lineText);
                    
                    osdm_lock(tapeDir->osdMenu);
                    if (!osdm_set_list_item_text(tapeDir->osdMenu, 
                        osdm_get_list_item(tapeDir->osdMenu, OSD_TAPE_DIR_ENTRY_INDEX(tapeDir, k)), lineText))
                    {
                        ml_log_error("Failed to update tape dir entry in OSD menu\n");
                    }
                    osdm_unlock(tapeDir->osdMenu);
                }
                break;
            }
        }
        if (k == tapeDir->numEntries) /* not found in list of files on disk */
        {
            /* append entry from index file */
            tapeDirEntry = add_tape_dir_entry(access, tapeDir, tapeDir->indexFile.entries[i].name,
                -1, tapeDir->indexFile.entries[i].fileSize);
        }
    }
    
    return 1;
}

static int tape_dir_entry_delete(QCLTOAccess* access, TapeDirectory* tapeDir, const char* entryName)
{
    int k;
    int numEntries;
    char lineText[48];
    
    /* was the index file deleted ? */
    if (tapeDir->indexFile.name != NULL && strcmp(entryName, tapeDir->indexFile.name) == 0)
    {
        clear_index_file(&tapeDir->indexFile);
        SAFE_FREE(&tapeDir->indexFile.filename);
        tapeDir->indexFile.name = NULL;

        /* remove tape directory entries that are only present on the tape,
        ie. they were added because of the previous 'version' of the index file */
        k = 0;
        while (k < tapeDir->numEntries)
        {
            numEntries = tapeDir->numEntries;
            
            if (tapeDir->entries[k].sizeOnDisk >= 0)
            {
                /* modify the entry */
                tapeDir->entries[k].sizeOnTape = -1;

                /* update line in OSD menu */
                if (tapeDir->osdMenu != NULL)
                {
                    get_tape_dir_entry_line(access, tapeDir, &tapeDir->entries[k], lineText);

                    osdm_lock(tapeDir->osdMenu);                    
                    if (!osdm_set_list_item_text(tapeDir->osdMenu, 
                        osdm_get_list_item(tapeDir->osdMenu, OSD_TAPE_DIR_ENTRY_INDEX(tapeDir, k)), lineText))
                    {
                        ml_log_error("Failed to update tape dir entry in OSD menu\n");
                    }
                    osdm_unlock(tapeDir->osdMenu);                    
                }
            }
            else
            {
                /* remove the entry */
                
                /* switch to a root menu if the current file associated with the play select menu is about to be deleted */
                if (access->cache.osdMenu != NULL &&
                    access->activeMenu == PLAY_SELECT_MENU_ACTIVE &&
                    &access->cache.tapeDirs[access->cache.osdMenu->currentItemIndex] == tapeDir)
                {
                    switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
                }
                
                if (!remove_tape_dir_entry(access, tapeDir, k))
                {
                    ml_log_warn("Failed to remove tape directory entry for updated index file\n");
                }
            }
            
            if (numEntries == tapeDir->numEntries)
            {
                k++;
            }
        }
    }
    else
    {
        /* try the MXF files */
        for (k = 0; k < tapeDir->numEntries; k++)
        {
            if (strcmp(entryName, tapeDir->entries[k].name) == 0)
            {
                if (tapeDir->entries[k].sizeOnTape >= 0)
                {
                    /* modify the entry */
                    tapeDir->entries[k].sizeOnDisk = -1;

                    /* update line in OSD menu */
                    if (tapeDir->osdMenu != NULL)
                    {
                        get_tape_dir_entry_line(access, tapeDir, &tapeDir->entries[k], lineText);
    
                        osdm_lock(tapeDir->osdMenu);                    
                        if (!osdm_set_list_item_text(tapeDir->osdMenu, 
                            osdm_get_list_item(tapeDir->osdMenu, OSD_TAPE_DIR_ENTRY_INDEX(tapeDir, k)), lineText))
                        {
                            ml_log_error("Failed to update tape dir entry in OSD menu\n");
                        }
                        osdm_unlock(tapeDir->osdMenu);                    
                    }
                }
                else
                {
                    /* remove the entry */
                    
                    /* switch to a root menu if the current file associated with the play select menu is about to be deleted */
                    if (access->cache.osdMenu != NULL &&
                        access->activeMenu == PLAY_SELECT_MENU_ACTIVE &&
                        &access->cache.tapeDirs[access->cache.osdMenu->currentItemIndex] == tapeDir)
                    {
                        switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
                    }
                    
                    remove_tape_dir_entry(access, tapeDir, k);
                }
                break;
            }
        }
    }
    
    return 1;
}

static int tape_dir_delete(QCLTOAccess* access, const char* tapeDirName)
{
    int t;
    
    for (t = 0; t < access->cache.numTapeDirs; t++)
    {
        if (strcmp(tapeDirName, access->cache.tapeDirs[t].name) == 0)
        {
            /* switch to a root menu if the current one is about to be deleted */
            if (access->cache.osdMenu != NULL &&
                access->cache.osdMenu->currentItemIndex == t &&
                access->activeMenu != ROOT_MENU_ACTIVE)
            {
                switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
            }
            
            remove_tape_dir(access, t);
            break;
        }
    }
    
    return 1;
}

static int tape_dir_entry_modify(QCLTOAccess* access, TapeDirectory* tapeDir, const char* entryName)
{
    struct stat statBuf;
    int k;
    char name[FILENAME_MAX];
    char lineText[48];
    int numEntries;
    
    /* check if it is the index file */
    if (tapeDir->indexFile.name != NULL && strcmp(entryName, tapeDir->indexFile.name) == 0)
    {
        /* remove tape directory entries that are only present on the tape,
        ie. they were added because of the previous 'version' of the index file */
        k = 0;
        while (k < tapeDir->numEntries)
        {
            numEntries = tapeDir->numEntries;
            
            if (tapeDir->entries[k].sizeOnDisk >= 0)
            {
                /* modify the entry */
                tapeDir->entries[k].sizeOnTape = -1;

                /* update line in OSD menu */
                if (tapeDir->osdMenu != NULL)
                {
                    get_tape_dir_entry_line(access, tapeDir, &tapeDir->entries[k], lineText);

                    osdm_lock(tapeDir->osdMenu);                    
                    if (!osdm_set_list_item_text(tapeDir->osdMenu, 
                        osdm_get_list_item(tapeDir->osdMenu, OSD_TAPE_DIR_ENTRY_INDEX(tapeDir, k)), lineText))
                    {
                        ml_log_error("Failed to update tape dir entry in OSD menu\n");
                    }
                    osdm_unlock(tapeDir->osdMenu);                    
                }
            }
            else
            {
                /* remove the entry */
                
                /* switch to a root menu if the current file associated with the play select menu is about to be deleted */
                if (access->cache.osdMenu != NULL &&
                    access->activeMenu == PLAY_SELECT_MENU_ACTIVE &&
                    &access->cache.tapeDirs[access->cache.osdMenu->currentItemIndex] == tapeDir)
                {
                    switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
                }
                
                if (!remove_tape_dir_entry(access, tapeDir, k))
                {
                    ml_log_warn("Failed to remove tape directory entry for updated index file\n");
                }
            }
            
            if (numEntries == tapeDir->numEntries)
            {
                k++;
            }
        }

        /* parse and process the index file and update the mxf file entries */
        
        strcpy(name, access->cacheDirName);
        strcat_separator(name);
        strcat(name, tapeDir->name);
        strcat_separator(name);
        strcat(name, entryName);
        parse_index_file(name, &tapeDir->indexFile);
        
        process_index_file(access, tapeDir);
    }
    else 
    {
        /* try the MXF files */
        for (k = 0; k < tapeDir->numEntries; k++)
        {
            if (strcmp(entryName, tapeDir->entries[k].name) == 0)
            {
                if (!get_file_stat(access->cacheDirName, tapeDir->name, entryName, &statBuf))
                {
                    ml_log_error("Failed to stat updated tape directory entry\n");
                    break;
                }
                tapeDir->entries[k].sizeOnDisk = statBuf.st_size;
                
                /* update line in OSD menu */
                if (tapeDir->osdMenu != NULL)
                {
                    get_tape_dir_entry_line(access, tapeDir, &tapeDir->entries[k], lineText);

                    osdm_lock(tapeDir->osdMenu);                    
                    if (!osdm_set_list_item_text(tapeDir->osdMenu, 
                        osdm_get_list_item(tapeDir->osdMenu, OSD_TAPE_DIR_ENTRY_INDEX(tapeDir, k)), lineText))
                    {
                        ml_log_error("Failed to update tape dir entry in OSD menu\n");
                    }
                    osdm_unlock(tapeDir->osdMenu);                    
                }
                break;
            }
        }
    }
    
    
    return 1;
}

static int tape_dir_entry_create(QCLTOAccess* access, TapeDirectory* tapeDir, const char* entryName, TapeDirEntry** tapeDirEntry)
{
    char name[FILENAME_MAX];
    struct stat statBuf;
    int i;
    size_t entryNameLen = strlen(entryName);

    /* look for *.mxf files */    
    if (entryNameLen >= 4 &&
        entryName[entryNameLen - 4] == '.' &&
        tolower(entryName[entryNameLen - 3]) == 'm' &&
        tolower(entryName[entryNameLen - 2]) == 'x' &&
        tolower(entryName[entryNameLen - 1]) == 'f')
    {
        if (get_file_stat(access->cacheDirName, tapeDir->name, entryName, &statBuf))
        {
            if (((*tapeDirEntry) = add_tape_dir_entry(access, tapeDir, entryName, statBuf.st_size, -1)) == NULL)
            {
                ml_log_error("Failed to add the tape directory entry to the cache\n");
                return 0;
            }
        }
    }
    /* else look for LTO tape index file */
    else
    {
        /* format is 'LTA[0...9](8).txt' */
        if (strlen(entryName) != 15)
        {
            return 0;
        }
        if (strncmp("LTA", entryName, 3) != 0)
        {
            return 0;
        }
        for (i = 3; i < 11; i++)
        {
            if (entryName[i] < '0' && entryName[i] > '9')
            {
                return 0;
            }
        }
    
        if (entryName[11] != '.' ||
            tolower(entryName[12]) != 't' ||
            tolower(entryName[13]) != 'x' ||
            tolower(entryName[14]) != 't')
        {
            return 0;
        }
        
        
        strcpy(name, access->cacheDirName);
        strcat_separator(name);
        strcat(name, tapeDir->name);
        strcat_separator(name);
        strcat(name, entryName);
        parse_index_file(name, &tapeDir->indexFile);
        
        process_index_file(access, tapeDir);
    }
    
    return 1;
}

static int tape_dir_create(QCLTOAccess* access, const char* tapeDirName, TapeDirectory** tapeDir)
{
    char name[FILENAME_MAX];
    int i;
    DIR* tapeDirStream = NULL;
    struct dirent* tapeDirent;
    TapeDirEntry* tapeDirEntry;
    
    /* check name format is 'LTA[0...9](6)' */
    if (strlen(tapeDirName) != 9)
    {
        return 0;
    }
    if (strncmp("LTA", tapeDirName, 3) != 0)
    {
        return 0;
    }
    for (i = 3; i < 9; i++)
    {
        if (tapeDirName[i] < '0' && tapeDirName[i] > '9')
        {
            return 0;
        }
    }

    /* open the tape directory stream */    
    strcpy(name, access->cacheDirName);
    strcat_separator(name);
    strcat(name, tapeDirName);
    if ((tapeDirStream = opendir(name)) == NULL)
    {
        return 0;
    }
    
    /* add the tape directory */
    if (((*tapeDir) = add_tape_dir(access, tapeDirName)) == NULL)
    {
        ml_log_error("Failed to add the tape directory to the cache\n");
        closedir(tapeDirStream); return 0;
    }

    /* add a inotify watch */
    (*tapeDir)->wd = inotify_add_watch(access->inotifyFD, name, 
        IN_MOVED_FROM | IN_MOVED_TO | IN_MODIFY | IN_CREATE | IN_DELETE);
    if ((*tapeDir)->wd < 0)
    {
        ml_log_warn("Failed to add inotify watch for qc tape directory: %s\n", strerror(errno));
    }
    
    /* create tape dir entries */
    while ((tapeDirent = readdir(tapeDirStream)) != NULL)
    {
        if (!tape_dir_entry_create(access, (*tapeDir), tapeDirent->d_name, &tapeDirEntry))
        {
            /* item was not a tape directory entry (mxf file or index file) */
            continue;
        }
    }
    closedir(tapeDirStream);
    
    /* process the index file and update the mxf file entries */
    process_index_file(access, *tapeDir);
    
    
    return 1;
}

static void* update_worker_thread(void* arg)
{
    QCLTOAccess* access = (QCLTOAccess*)arg;
    struct timeval now = {0, 0};
    struct timeval endNow = {0, 0};
    struct timeval lastTapeStatus = {0, 0};
    long diffTime;
    OSDMenuModel* menu;
    QCLTOExtractState extractState;
    QCLTOExtractState prevExtractState;
    char statusString[40];
    char diskStatusString[40];
    int t;
    int f;
    int updateExtractDir;
    int updateExtractFile;
    int result;
    struct timeval selectTime;
    fd_set rfds;
    TapeDirectory* tapeDir;
    TapeDirEntry* tapeDirEntry;
    OSDMenuListItem* listItem;

    memset(&extractState, 0, sizeof(extractState));
    memset(&prevExtractState, 0, sizeof(prevExtractState));
    
    
    while (!access->stopping)
    {
        gettimeofday(&now, NULL);
        
        
        /* update tape status */
        
        diffTime = (now.tv_sec - lastTapeStatus.tv_sec) * 1000000 + now.tv_usec - lastTapeStatus.tv_usec;
        if (diffTime < 0)
        {
            lastTapeStatus = now;
        }
        else if (access->extract != NULL && diffTime > 1000000) /* 1 second */
        {
            menu = NULL;
            
            get_status_string(access, statusString, &extractState, 0x01);

            
            PTHREAD_MUTEX_LOCK(&access->cacheMutex); 

            
            /* update the current tape in the tape device */
            
            updateExtractDir = strcmp(extractState.ltoSpoolNumber, prevExtractState.ltoSpoolNumber) != 0;
            updateExtractFile = strcmp(extractState.currentExtractingFile, prevExtractState.currentExtractingFile) != 0;
            if (updateExtractDir || updateExtractFile)
            {
                for (t = 0; t < access->cache.numTapeDirs; t++)
                {
                    if (strcmp(access->cache.tapeDirs[t].name, extractState.ltoSpoolNumber) == 0)
                    {
                        /* highlight tape directory */
                        if (!access->cache.tapeDirs[t].extracting)
                        {
                            access->cache.tapeDirs[t].extracting = 1;
    
                            if (access->cache.osdMenu != NULL)
                            {
                                osdm_lock(access->cache.osdMenu); /* prevent OSD access to the menu */
                                osdm_set_list_item_state(access->cache.osdMenu, 
                                    osdm_get_list_item(access->cache.osdMenu, t), 
                                    MENU_ITEM_HIGHLIGHTED);
                                osdm_unlock(access->cache.osdMenu);
                            }
                        }

                        /* add 'Extract all' option */
                        if (!access->cache.tapeDirs[t].haveExtractAllOption)
                        {
                            if (access->cache.tapeDirs[t].osdMenu != NULL)
                            {
                                osdm_lock(access->cache.tapeDirs[t].osdMenu); /* prevent OSD access to the menu */
                                if (osdm_insert_list_item(access->cache.tapeDirs[t].osdMenu, 1, &listItem))
                                {
                                    access->cache.tapeDirs[t].haveExtractAllOption = 1;
                                    
                                    if (!osdm_set_list_item_text(access->cache.tapeDirs[t].osdMenu, listItem, "\t\tExtract all"))
                                    {
                                        ml_log_error("Failed to set 'Extract all' menu option text\n");
                                    }
                                }
                                else
                                {
                                    ml_log_error("Failed to add an 'Extract all' option to tape directory menu\n");
                                }
                                osdm_unlock(access->cache.tapeDirs[t].osdMenu);
                            }
                        }
                        
                        /* highlight tape directory entry */
                        for (f = 0; f < access->cache.tapeDirs[t].numEntries; f++)
                        {
                            if (strcmp(access->cache.tapeDirs[t].entries[f].name, extractState.currentExtractingFile) == 0)
                            {
                                /* highlight tape directory entry */
                                if (!access->cache.tapeDirs[t].entries[f].extracting)
                                {
                                    access->cache.tapeDirs[t].entries[f].extracting = 1;
                            
                                    if (access->cache.tapeDirs[t].osdMenu != NULL)
                                    {
                                        osdm_lock(access->cache.tapeDirs[t].osdMenu); /* prevent OSD access to the menu */
                                        osdm_set_list_item_state(access->cache.tapeDirs[t].osdMenu, 
                                            osdm_get_list_item(access->cache.tapeDirs[t].osdMenu, 
                                                OSD_TAPE_DIR_ENTRY_INDEX(&access->cache.tapeDirs[t], f)), 
                                            MENU_ITEM_HIGHLIGHTED);
                                        osdm_unlock(access->cache.tapeDirs[t].osdMenu);
                                    }
                                }
                            }
                            else
                            {
                                /* reset to normal tape directory entry */
                                if (access->cache.tapeDirs[t].entries[f].extracting)
                                {
                                    access->cache.tapeDirs[t].entries[f].extracting = 0;
                            
                                    if (access->cache.tapeDirs[t].osdMenu != NULL)
                                    {
                                        osdm_lock(access->cache.tapeDirs[t].osdMenu); /* prevent OSD access to the menu */
                                        osdm_set_list_item_state(access->cache.tapeDirs[t].osdMenu, 
                                            osdm_get_list_item(access->cache.tapeDirs[t].osdMenu, 
                                                OSD_TAPE_DIR_ENTRY_INDEX(&access->cache.tapeDirs[t], f)), 
                                            MENU_ITEM_NORMAL);
                                        osdm_unlock(access->cache.tapeDirs[t].osdMenu);
                                    }
                                }
                            }
                        }
                    }
                    else if (strcmp(access->cache.tapeDirs[t].name, prevExtractState.ltoSpoolNumber) == 0)
                    {
                        /* reset to normal tape directory */
                        if (access->cache.tapeDirs[t].extracting)
                        {
                            access->cache.tapeDirs[t].extracting = 0;
    
                            if (access->cache.osdMenu != NULL)
                            {
                                osdm_lock(access->cache.osdMenu); /* prevent OSD access to the menu */
                                osdm_set_list_item_state(access->cache.osdMenu, 
                                    osdm_get_list_item(access->cache.osdMenu, t), 
                                    MENU_ITEM_NORMAL);
                                osdm_unlock(access->cache.osdMenu);
                            }
                        }
                        
                        /* remove 'Extract all' option*/
                        if (access->cache.tapeDirs[t].haveExtractAllOption)
                        {
                            access->cache.tapeDirs[t].haveExtractAllOption = 0;
                            if (access->cache.tapeDirs[t].osdMenu != NULL)
                            {
                                osdm_lock(access->cache.tapeDirs[t].osdMenu); /* prevent OSD access to the menu */
                                osdm_remove_list_item(access->cache.tapeDirs[t].osdMenu, 1);
                                osdm_unlock(access->cache.tapeDirs[t].osdMenu);
                            }
                        }
                        
                        for (f = 0; f < access->cache.tapeDirs[t].numEntries; f++)
                        {
                            /* reset to normal tape directory entry */
                            if (access->cache.tapeDirs[t].entries[f].extracting)
                            {
                                access->cache.tapeDirs[t].entries[f].extracting = 0;
                            
                                if (access->cache.tapeDirs[t].osdMenu != NULL)
                                {
                                    osdm_lock(access->cache.tapeDirs[t].osdMenu); /* prevent OSD access to the menu */
                                    osdm_set_list_item_state(access->cache.tapeDirs[t].osdMenu, 
                                        osdm_get_list_item(access->cache.tapeDirs[t].osdMenu, 
                                            OSD_TAPE_DIR_ENTRY_INDEX(&access->cache.tapeDirs[t], f)), 
                                        MENU_ITEM_NORMAL);
                                    osdm_unlock(access->cache.tapeDirs[t].osdMenu);
                                }
                            }
                        }
                    }
                }
            }
            
            
            /* highlight the "extract all" option if extracting all files */
            
            if (updateExtractDir || updateExtractFile || 
                prevExtractState.extractAll != extractState.extractAll)
            {
                for (t = 0; t < access->cache.numTapeDirs; t++)
                {
                    if (strcmp(access->cache.tapeDirs[t].name, extractState.ltoSpoolNumber) == 0 &&
                        access->cache.tapeDirs[t].haveExtractAllOption)
                    {
                        if (access->cache.tapeDirs[t].osdMenu != NULL)
                        {
                            osdm_lock(access->cache.tapeDirs[t].osdMenu); /* prevent OSD access to the menu */
                            if (extractState.extractAll)
                            {
                                osdm_set_list_item_state(access->cache.tapeDirs[t].osdMenu, 
                                    osdm_get_list_item(access->cache.tapeDirs[t].osdMenu, 1), 
                                    MENU_ITEM_HIGHLIGHTED);
                            }
                            else
                            {
                                osdm_set_list_item_state(access->cache.tapeDirs[t].osdMenu, 
                                    osdm_get_list_item(access->cache.tapeDirs[t].osdMenu, 1), 
                                    MENU_ITEM_NORMAL);
                            }
                            osdm_unlock(access->cache.tapeDirs[t].osdMenu);
                        }
                    }
                    else if (strcmp(access->cache.tapeDirs[t].name, prevExtractState.ltoSpoolNumber) == 0 &&
                        access->cache.tapeDirs[t].haveExtractAllOption)
                    {
                        if (access->cache.tapeDirs[t].osdMenu != NULL)
                        {
                            osdm_lock(access->cache.tapeDirs[t].osdMenu); /* prevent OSD access to the menu */
                            if (extractState.extractAll)
                            {
                                osdm_set_list_item_state(access->cache.tapeDirs[t].osdMenu, 
                                    osdm_get_list_item(access->cache.tapeDirs[t].osdMenu, 1), 
                                    MENU_ITEM_HIGHLIGHTED);
                            }
                            else
                            {
                                osdm_set_list_item_state(access->cache.tapeDirs[t].osdMenu, 
                                    osdm_get_list_item(access->cache.tapeDirs[t].osdMenu, 1), 
                                    MENU_ITEM_NORMAL);
                            }
                            osdm_unlock(access->cache.tapeDirs[t].osdMenu);
                        }
                    }
                }
            }
            
            
            /* set the status string */
            
            if (access->activeMenu == ROOT_MENU_ACTIVE)
            {
                menu = access->cache.osdMenu;
                
                if (menu != NULL)
                {
                    get_disk_free_status_string(access, diskStatusString);
    
                    osdm_lock(menu); /* prevent OSD access to the menu */
                    osdm_set_comment(menu, diskStatusString);
                    osdm_unlock(menu);
                }
            }
            else if (access->activeMenu == TAPE_DIR_MENU_ACTIVE)
            {
                if (access->cache.osdMenu != NULL)
                {
                    menu = access->cache.tapeDirs[access->cache.osdMenu->currentItemIndex].osdMenu;
                }
                else
                {
                    menu = NULL;
                }
            }
            else if (access->activeMenu == PLAY_SELECT_MENU_ACTIVE)
            {
                menu = access->playSelect.osdMenu;
            }
            else if (access->activeMenu == EXTRACT_ALL_MENU_ACTIVE)
            {
                menu = access->extractAll.osdMenu;
            }
            else /* access->activeMenu == DELETE_TAPE_DIR_MENU_ACTIVE */
            {
                menu = access->deleteTapeDir.osdMenu;
            }

            if (menu != NULL)
            {
                osdm_lock(menu); /* prevent OSD access to the menu */
                osdm_set_status(menu, statusString);
                osdm_unlock(menu);
            }
            
            lastTapeStatus = now;
            
            PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);

            
            if (menu != NULL)
            {
                mhl_refresh_required(access->listener);
            }
            
            
            prevExtractState = extractState;
        }

        
        /* check for updates to the cache directory */
        
        FD_ZERO(&rfds);
        FD_SET(access->inotifyFD, &rfds);
        
        selectTime.tv_sec = 0;
        selectTime.tv_usec = 100000; /* 1/10th second */
        result = select(access->inotifyFD + 1, &rfds, NULL, NULL, &selectTime);
        if (FD_ISSET(access->inotifyFD, &rfds))
        {
            #define EVENT_SIZE  (sizeof(struct inotify_event))
            #define BUF_LEN (1024 * (EVENT_SIZE + 16))
            char buf[BUF_LEN];
            int len, i = 0;
            len = read(access->inotifyFD, buf, BUF_LEN);
            if (len < 0) 
            {
                ml_log_error("Failed to read inotify events: %s\n", strerror(errno));
            } 
            else if (len == 0)
            {
                ml_log_error("Failed to read inotify events - buffer could be too small\n");
            }
            else
            {
                while (i < len) 
                {
                    struct inotify_event* event;
                    event = (struct inotify_event*)&buf[i];
                    
                    if (event->wd == access->cacheWD)
                    {
                        if (event->len > 0 && 
                            (event->mask & IN_ISDIR) && 
                            (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM ||
                                event->mask & IN_CREATE || event->mask & IN_MOVED_TO))
                        {
                            PTHREAD_MUTEX_LOCK(&access->cacheMutex); 
                            if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) 
                            {
                                tape_dir_delete(access, event->name);
                            }
                            else if (event->mask & IN_CREATE || event->mask & IN_MOVED_TO)
                            {
                                tape_dir_create(access, event->name, &tapeDir);
                            }
                            PTHREAD_MUTEX_UNLOCK(&access->cacheMutex); 
                        }
                    }
                    else
                    {
                        if (event->len > 0 && 
                            !(event->mask & IN_ISDIR) && 
                            (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM ||
                                event->mask & IN_CREATE || event->mask & IN_MOVED_TO ||
                                event->mask & IN_MODIFY))
                        {
                            PTHREAD_MUTEX_LOCK(&access->cacheMutex); 
                            for (t = 0; t < access->cache.numTapeDirs; t++)
                            {
                                if (event->wd == access->cache.tapeDirs[t].wd)
                                {
                                    if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) 
                                    {
                                        tape_dir_entry_delete(access, &access->cache.tapeDirs[t], event->name);
                                    }
                                    else if (event->mask & IN_CREATE || event->mask & IN_MOVED_TO)
                                    {
                                        tape_dir_entry_create(access, &access->cache.tapeDirs[t], event->name, &tapeDirEntry);
                                    }
                                    else if (event->mask & IN_MODIFY)
                                    {
                                        tape_dir_entry_modify(access, &access->cache.tapeDirs[t], event->name);
                                    }
                                    break;
                                }
                            }
                            PTHREAD_MUTEX_UNLOCK(&access->cacheMutex); 
                        }
                    }
                    
                    i += EVENT_SIZE + event->len;
                }
            }
        }
        
        
        gettimeofday(&endNow, NULL);
        diffTime = (endNow.tv_sec - now.tv_sec) * 1000000 + endNow.tv_usec - now.tv_usec;
        if (diffTime < 100000) /* 1/10th second */
        {
            /* don't hog the CPU */
            usleep(100000);
        }
    }

    pthread_exit((void*) 0);
}

static int load_cache_contents(QCLTOAccess* access)
{
    DIR* rootDirStream = NULL;
    struct dirent* rootDirent;
    TapeDirectory* tapeDir;
    
    
    PTHREAD_MUTEX_LOCK(&access->cacheMutex);
    
    free_cache_contents(access);

    if ((rootDirStream = opendir(access->cacheDirName)) == NULL)
    {
        ml_log_error("Failed to open cache directory stream: %s\n", strerror(errno));
        goto fail;
    }
    
    while ((rootDirent = readdir(rootDirStream)) != NULL)
    {
        if (!tape_dir_create(access, rootDirent->d_name, &tapeDir))
        {
            /* item was not a tape directory */
            continue;
        }
    }

    closedir(rootDirStream);
    rootDirStream = NULL;
    
    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
    
    return 1;

fail:    
    if (rootDirStream != NULL)
    {
        closedir(rootDirStream);
        rootDirStream = NULL;
    }
    
    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
    
    return 0;
}




static int create_cache_menu(QCLTOAccess* access)
{
    int i;
    char lineText[33];
    OSDMenuListItem* listItem;
    OSDMenuModel* newMenu = NULL;
    char statusString[40];
    QCLTOExtractState extractState;
    char diskStatusString[40];

    CHK_ORET(osd_create_menu_model(access->osd, &newMenu));

    CHK_OFAIL(osdm_set_title(newMenu, g_menuTitle));
    get_status_string(access, statusString, &extractState, 0);
    CHK_OFAIL(osdm_set_status(newMenu, statusString));
    
    get_disk_free_status_string(access, diskStatusString);
    CHK_OFAIL(osdm_set_comment(newMenu, diskStatusString));

    
    for (i = 0; i < access->cache.numTapeDirs; i++)
    {
        strcpy(lineText, "\t\t");
        strcat(lineText, access->cache.tapeDirs[i].name);
        
        CHK_OFAIL(osdm_insert_list_item(newMenu, LAST_MENU_ITEM_INDEX, &listItem));
        CHK_OFAIL(osdm_set_list_item_text(newMenu, listItem, lineText));
        if (strcmp(access->cache.tapeDirs[i].name, extractState.ltoSpoolNumber) == 0)
        {
            osdm_set_list_item_state(newMenu, listItem, MENU_ITEM_HIGHLIGHTED);
        }
    }

    access->cache.osdMenu = newMenu;
    return 1;
    
fail:
    osd_free_menu_model(access->osd, &newMenu);
    return 0;
}

static int create_tape_dir_menu(QCLTOAccess* access, TapeDirectory* tapeDir)
{
    int i;
    OSDMenuListItem* listItem;
    OSDMenuModel* newMenu = NULL;
    char statusString[40];
    QCLTOExtractState extractState;
    char lineText[48];
    
    CHK_ORET(osd_create_menu_model(access->osd, &newMenu));

    CHK_OFAIL(osdm_set_title(newMenu, g_menuTitle));
    get_status_string(access, statusString, &extractState, 0);
    CHK_OFAIL(osdm_set_status(newMenu, statusString));

    CHK_OFAIL(osdm_insert_list_item(newMenu, 0, &listItem));
    CHK_OFAIL(osdm_set_list_item_text(newMenu, listItem, "\t\t../"));
    
    if (strcmp(extractState.ltoSpoolNumber, tapeDir->name) == 0)
    {
        CHK_OFAIL(osdm_insert_list_item(newMenu, LAST_MENU_ITEM_INDEX, &listItem));
        CHK_OFAIL(osdm_set_list_item_text(newMenu, listItem, "\t\tExtract all"));
        tapeDir->haveExtractAllOption = 1;
    }
    else
    {
        tapeDir->haveExtractAllOption = 0;
    }
    
    for (i = 0; i < tapeDir->numEntries; i++)
    {
        get_tape_dir_entry_line(access, tapeDir, &tapeDir->entries[i], lineText);
        
        CHK_OFAIL(osdm_insert_list_item(newMenu, LAST_MENU_ITEM_INDEX, &listItem));
        CHK_OFAIL(osdm_set_list_item_text(newMenu, listItem, lineText));
        if (strcmp(tapeDir->name, extractState.ltoSpoolNumber) == 0 &&
            strcmp(tapeDir->entries[i].name, extractState.currentExtractingFile) == 0)
        {
            osdm_set_list_item_state(newMenu, listItem, MENU_ITEM_HIGHLIGHTED);
        }
    }

    
    tapeDir->osdMenu = newMenu;
    return 1;
 
    
fail:
    osd_free_menu_model(access->osd, &newMenu);
    return 0;
}

static int create_play_select_menu(QCLTOAccess* access, TapeDirectory* tapeDir, int entryIndex)
{
    OSDMenuListItem* listItem;
    OSDMenuModel* newMenu = NULL;
    SessionInfo sessions[MAX_SELECT_SESSION];
    int i;
    char lineText[64];
    char comment[64];
    char timestampText[64];
    char statusString[40];
    QCLTOExtractState extractState;
    int year, month, day, hour, min, sec;
    
    memset(sessions, 0, sizeof(sessions));

    
    /* free existing play menu data */
    SAFE_FREE(&access->playSelect.directory);
    SAFE_FREE(&access->playSelect.name);
    for (i = 0; i < MAX_SELECT_SESSION; i++)
    {
        SAFE_FREE(&access->playSelect.sessionNames[i]);
    }
    if (access->playSelect.osdMenu != NULL)
    {
        osd_free_menu_model(access->osd, &access->playSelect.osdMenu);
    }

    /* get available sessions, ordered by modification time */
    if (tapeDir->entries[entryIndex].sizeOnDisk > 0)
    {
        get_sessions(access, tapeDir->filename, tapeDir->entries[entryIndex].name, sessions);
        
        /* copy the session names */
        for (i = 0; i < MAX_SELECT_SESSION; i++)
        {
            if (sessions[i].name[0] == '\0')
            {
                /* end of list */
                break;
            }
            
            CALLOC_OFAIL(access->playSelect.sessionNames[i], char, strlen(sessions[i].name) + 1);
            strcpy(access->playSelect.sessionNames[i], sessions[i].name);
        }
    }

    /* copy the directory and filename */    
    CALLOC_OFAIL(access->playSelect.directory, char, strlen(tapeDir->filename) + 1);
    strcpy(access->playSelect.directory, tapeDir->filename);
    CALLOC_OFAIL(access->playSelect.name, char, strlen(tapeDir->entries[entryIndex].name) + 1);
    strcpy(access->playSelect.name, tapeDir->entries[entryIndex].name);

    /* add start and stop extract options */
    access->playSelect.haveStartExtractOption = 0;
    access->playSelect.haveStopExtractOption = 0;
    if (qce_is_extracting(access->extract, tapeDir->name, tapeDir->entries[entryIndex].name))
    {
        access->playSelect.haveStopExtractOption = 1;
    }
    /* start extract option if file is on the current LTO and is not currently playing */ 
    else if (qce_is_current_lto(access->extract, tapeDir->name) &&
        tapeDir->entries[entryIndex].sizeOnTape >= 0 &&
        (access->currentPlayLTONumber == NULL || access->currentPlayName == NULL || 
            strcmp(tapeDir->name, access->currentPlayLTONumber) != 0 ||
            strcmp(tapeDir->entries[entryIndex].name, access->currentPlayName) != 0))
    {
        access->playSelect.haveStartExtractOption = 1;
    }
    
    
    CHK_OFAIL(osd_create_menu_model(access->osd, &newMenu));

    CHK_OFAIL(osdm_set_title(newMenu, g_menuTitle));
    get_status_string(access, statusString, &extractState, 0);
    CHK_OFAIL(osdm_set_status(newMenu, statusString));
    
    strcpy(comment, "\t\tSelected ");
    strncat(comment, tapeDir->entries[entryIndex].name, MAX_SELECTED_MXF_NAME_LENGTH_IN_OSD);
    if (strlen(tapeDir->entries[entryIndex].name) > MAX_SELECTED_MXF_NAME_LENGTH_IN_OSD)
    {
        strcat(comment, "...");
    }
    CHK_OFAIL(osdm_set_comment(newMenu, comment));
    
    CHK_OFAIL(osdm_insert_list_item(newMenu, 0, &listItem));
    CHK_OFAIL(osdm_set_list_item_text(newMenu, listItem, "\t\t\t\t../"));

    if (access->playSelect.haveStartExtractOption)
    {
        CHK_OFAIL(osdm_insert_list_item(newMenu, LAST_MENU_ITEM_INDEX, &listItem));
        CHK_OFAIL(osdm_set_list_item_text(newMenu, listItem, "\t\t\t\tStart extracting"));
    }
    else if (access->playSelect.haveStopExtractOption)
    {
        CHK_OFAIL(osdm_insert_list_item(newMenu, LAST_MENU_ITEM_INDEX, &listItem));
        CHK_OFAIL(osdm_set_list_item_text(newMenu, listItem, "\t\t\t\tStop extracting"));
    }

    if (tapeDir->entries[entryIndex].sizeOnDisk > 0)
    {
        CHK_OFAIL(osdm_insert_list_item(newMenu, LAST_MENU_ITEM_INDEX, &listItem));
        CHK_OFAIL(osdm_set_list_item_text(newMenu, listItem, "\t\t\t\tPlay new session"));
        
        for (i = 0; i < MAX_SELECT_SESSION; i++)
        {
            if (access->playSelect.sessionNames[i] == NULL)
            {
                /* end of list */
                break;
            }
            
            strcpy(lineText, "\t\t");
            strcat(lineText, "\t\tPlay session ");
            
            CHK_OFAIL(qcs_extract_timestamp(access->playSelect.sessionNames[i], &year, &month, &day, &hour, &min, &sec));
            snprintf(timestampText, sizeof(timestampText), "%04d-%02d-%02d %02d:%02d:%02d", 
                year, month, day, hour, min, sec);
            strcat(lineText, timestampText);

            CHK_OFAIL(osdm_insert_list_item(newMenu, LAST_MENU_ITEM_INDEX, &listItem));
            CHK_OFAIL(osdm_set_list_item_text(newMenu, listItem, lineText));
        }
    }

    osdm_set_current_list_item(newMenu, 0);

    
    access->playSelect.osdMenu = newMenu;
    return 1;
    
fail:
    osd_free_menu_model(access->osd, &newMenu);
    return 0;
}

static int create_extract_all_menu(QCLTOAccess* access, TapeDirectory* tapeDir)
{
    OSDMenuListItem* listItem;
    OSDMenuModel* newMenu = NULL;
    char statusString[40];
    QCLTOExtractState extractState;
    
    
    /* free existing menu data */
    SAFE_FREE(&access->extractAll.directory);
    if (access->extractAll.osdMenu != NULL)
    {
        osd_free_menu_model(access->osd, &access->extractAll.osdMenu);
    }


    /* copy the directory name */    
    CALLOC_OFAIL(access->extractAll.directory, char, strlen(tapeDir->filename) + 1);
    strcpy(access->extractAll.directory, tapeDir->filename);

    /* check if a stop extract or start extract options should be added */
    access->extractAll.haveStopExtractAllOption = 0;
    access->extractAll.haveStartExtractAllOptions = 0;
    if (qce_is_extracting_from(access->extract, tapeDir->name))
    {
        access->extractAll.haveStopExtractAllOption = 1;
    }
    if (qce_is_current_lto(access->extract, tapeDir->name) &&
        !qce_is_extracting_all_from(access->extract, tapeDir->name))
    {
        access->extractAll.haveStartExtractAllOptions = 1;
    }
    
    
    CHK_OFAIL(osd_create_menu_model(access->osd, &newMenu));

    CHK_OFAIL(osdm_set_title(newMenu, g_menuTitle));
    get_status_string(access, statusString, &extractState, 0);
    CHK_OFAIL(osdm_set_status(newMenu, statusString));
    
    CHK_OFAIL(osdm_insert_list_item(newMenu, 0, &listItem));
    CHK_OFAIL(osdm_set_list_item_text(newMenu, listItem, "\t\t../"));

    if (access->extractAll.haveStopExtractAllOption)
    {
        CHK_OFAIL(osdm_insert_list_item(newMenu, LAST_MENU_ITEM_INDEX, &listItem));
        CHK_OFAIL(osdm_set_list_item_text(newMenu, listItem, "\t\tStop extracting"));
    }
    if (access->extractAll.haveStartExtractAllOptions)
    {
        CHK_OFAIL(osdm_insert_list_item(newMenu, LAST_MENU_ITEM_INDEX, &listItem));
        CHK_OFAIL(osdm_set_list_item_text(newMenu, listItem, "\t\tStart extracting remainder"));

        CHK_OFAIL(osdm_insert_list_item(newMenu, LAST_MENU_ITEM_INDEX, &listItem));
        CHK_OFAIL(osdm_set_list_item_text(newMenu, listItem, "\t\tStart extracting all"));
    }

    osdm_set_current_list_item(newMenu, 0);

    
    access->extractAll.osdMenu = newMenu;
    return 1;
    
fail:
    osd_free_menu_model(access->osd, &newMenu);
    return 0;
}

static int create_delete_tape_dir_menu(QCLTOAccess* access, TapeDirectory* tapeDir)
{
    OSDMenuListItem* listItem;
    OSDMenuModel* newMenu = NULL;
    char comment[64];
    char statusString[40];
    QCLTOExtractState extractState;
    
    /* free existing delete tape dir menu data */
    SAFE_FREE(&access->deleteTapeDir.filename);
    SAFE_FREE(&access->deleteTapeDir.name);
    if (access->deleteTapeDir.osdMenu != NULL)
    {
        osd_free_menu_model(access->osd, &access->deleteTapeDir.osdMenu);
    }

    /* copy the directory and filename */    
    CALLOC_OFAIL(access->deleteTapeDir.filename, char, strlen(tapeDir->filename) + 1);
    strcpy(access->deleteTapeDir.filename, tapeDir->filename);
    CALLOC_OFAIL(access->deleteTapeDir.name, char, strlen(tapeDir->name) + 1);
    strcpy(access->deleteTapeDir.name, tapeDir->name);

    
    CHK_OFAIL(osd_create_menu_model(access->osd, &newMenu));

    CHK_OFAIL(osdm_set_title(newMenu, g_menuTitle));
    get_status_string(access, statusString, &extractState, 0);
    CHK_OFAIL(osdm_set_status(newMenu, statusString));
    
    strcpy(comment, "\t\tSelected ");
    strncat(comment, access->deleteTapeDir.name, MAX_SELECTED_MXF_NAME_LENGTH_IN_OSD);
    if (strlen(access->deleteTapeDir.name) > MAX_SELECTED_MXF_NAME_LENGTH_IN_OSD)
    {
        strcat(comment, "...");
    }
    CHK_OFAIL(osdm_set_comment(newMenu, comment));
    
    CHK_OFAIL(osdm_insert_list_item(newMenu, 0, &listItem));
    CHK_OFAIL(osdm_set_list_item_text(newMenu, listItem, "\t\t\t\t../"));

    CHK_OFAIL(osdm_insert_list_item(newMenu, LAST_MENU_ITEM_INDEX, &listItem));
    CHK_OFAIL(osdm_set_list_item_text(newMenu, listItem, "\t\t\t\tDelete"));
        
    osdm_set_current_list_item(newMenu, 0);

    
    access->deleteTapeDir.osdMenu = newMenu;
    return 1;
    
fail:
    osd_free_menu_model(access->osd, &newMenu);
    return 0;
}


static void qla_set_listener(void* data, MenuHandlerListener* listener)
{
    QCLTOAccess* access = (QCLTOAccess*)data;

    PTHREAD_MUTEX_LOCK(&access->cacheMutex); 
    access->listener = listener;
    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
}


static void qla_next_menu_item(void* data)
{
    QCLTOAccess* access = (QCLTOAccess*)data;
    OSDMenuModel* menu;
    
    
    PTHREAD_MUTEX_LOCK(&access->cacheMutex); 

    if (access->activeMenu == ROOT_MENU_ACTIVE)
    {
        menu = access->cache.osdMenu;
    }
    else if (access->activeMenu == TAPE_DIR_MENU_ACTIVE)
    {
        if (access->cache.osdMenu == NULL)
        {
            menu = NULL;
        }
        else
        {
            menu = access->cache.tapeDirs[access->cache.osdMenu->currentItemIndex].osdMenu;
        }
    }
    else if (access->activeMenu == PLAY_SELECT_MENU_ACTIVE)
    {
        menu = access->playSelect.osdMenu;
    }
    else if (access->activeMenu == EXTRACT_ALL_MENU_ACTIVE)
    {
        menu = access->extractAll.osdMenu;
    }
    else /* DELETE_TAPE_DIR_MENU_ACTIVE */
    {
        menu = access->deleteTapeDir.osdMenu;
    }

    if (menu != NULL)
    {
        osdm_lock(menu); /* prevent OSD access to the menu */
        osdm_set_next_current_list_item(menu);
        osdm_unlock(menu);
    }

    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
}

static void qla_previous_menu_item(void* data)
{
    QCLTOAccess* access = (QCLTOAccess*)data;
    OSDMenuModel* menu;

    
    PTHREAD_MUTEX_LOCK(&access->cacheMutex); 

    if (access->activeMenu == ROOT_MENU_ACTIVE)
    {
        menu = access->cache.osdMenu;
    }
    else if (access->activeMenu == TAPE_DIR_MENU_ACTIVE)
    {
        if (access->cache.osdMenu == NULL)
        {
            menu = NULL;
        }
        else
        {
            menu = access->cache.tapeDirs[access->cache.osdMenu->currentItemIndex].osdMenu;
        }
    }
    else if (access->activeMenu == PLAY_SELECT_MENU_ACTIVE)
    {
        menu = access->playSelect.osdMenu;
    }
    else if (access->activeMenu == EXTRACT_ALL_MENU_ACTIVE)
    {
        menu = access->extractAll.osdMenu;
    }
    else /* DELETE_TAPE_DIR_MENU_ACTIVE */
    {
        menu = access->deleteTapeDir.osdMenu;
    }

    if (menu != NULL)
    {
        osdm_lock(menu); /* prevent OSD access to the menu */
        osdm_set_prev_current_list_item(menu);
        osdm_unlock(menu);
    }

    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
}

static void qla_select_menu_item_left(void* data)
{
    QCLTOAccess* access = (QCLTOAccess*)data;
    int currentItemIndex;
    
    
    PTHREAD_MUTEX_LOCK(&access->cacheMutex); 

    if (access->cache.osdMenu == NULL)
    {
        goto fail;
    }
    
    if (access->activeMenu == TAPE_DIR_MENU_ACTIVE)
    {
        /* go back up to the root menu */
        switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
    }
    else if (access->activeMenu == PLAY_SELECT_MENU_ACTIVE)
    {
        /* go back to the tape dir menu */
        currentItemIndex = access->cache.osdMenu->currentItemIndex;
        switch_active_menu(access, access->cache.tapeDirs[currentItemIndex].osdMenu, TAPE_DIR_MENU_ACTIVE);
    }
    else if (access->activeMenu == EXTRACT_ALL_MENU_ACTIVE)
    {
        /* go back to the tape dir menu */
        currentItemIndex = access->cache.osdMenu->currentItemIndex;
        switch_active_menu(access, access->cache.tapeDirs[currentItemIndex].osdMenu, TAPE_DIR_MENU_ACTIVE);
    }
    else if (access->activeMenu == DELETE_TAPE_DIR_MENU_ACTIVE)
    {
        /* go back to the root menu */
        switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
    }
    /* else access->activeMenu == ROOT_MENU_ACTIVE and we do nothing */

    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
    return;
    
fail:
    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
}

static void qla_select_menu_item_right(void* data)
{
    QCLTOAccess* access = (QCLTOAccess*)data;
    int currentTapeDirItemIndex;
    int newSessionItemIndex;
    int currentRootItemIndex;
    int currentPlaySelectItemIndex;
    int currentDeleteTapeDirItemIndex;
    int currentExtractAllItemIndex;
    char rmCmd[FILENAME_MAX];
    
    
    PTHREAD_MUTEX_LOCK(&access->cacheMutex); 

    if (access->cache.osdMenu == NULL)
    {
        goto fail;
    }
    
    if (access->activeMenu == ROOT_MENU_ACTIVE)
    {
        currentRootItemIndex = access->cache.osdMenu->currentItemIndex;
        if (currentRootItemIndex >= access->cache.numTapeDirs)
        {
            /* no items */
            goto fail;
        }
        
        if (access->cache.tapeDirs[currentRootItemIndex].osdMenu == NULL)
        {
            if (!create_tape_dir_menu(access, &access->cache.tapeDirs[currentRootItemIndex]))
            {
                ml_log_warn("Failed to create tape dir menu\n");
                goto fail;
            }
        }
        
        switch_active_menu(access, access->cache.tapeDirs[currentRootItemIndex].osdMenu, TAPE_DIR_MENU_ACTIVE);
    }
    else if (access->activeMenu == TAPE_DIR_MENU_ACTIVE)
    {
        currentRootItemIndex = access->cache.osdMenu->currentItemIndex;

        if (access->cache.tapeDirs[currentRootItemIndex].osdMenu == NULL)
        {
            /* go back up to the root menu */
            switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
            goto fail;
        }
        
        currentTapeDirItemIndex = access->cache.tapeDirs[currentRootItemIndex].osdMenu->currentItemIndex;

        if (currentTapeDirItemIndex <= 0) /* select '../' */
        {
            /* go back up to the root menu */
            switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
        }
        else if (currentTapeDirItemIndex == 1 && 
            access->cache.tapeDirs[currentRootItemIndex].haveExtractAllOption) /* Extract All */
        {
            /* check the LTO for this tape directory is in the drive */
            if (!qce_is_current_lto(access->extract, access->cache.tapeDirs[currentRootItemIndex].name))
            {
                /* ignore the request */
                goto fail;
            }
            
            /* create and switch to the extract all menu */
            if (!create_extract_all_menu(access, &access->cache.tapeDirs[currentRootItemIndex]))
            {
                ml_log_warn("Failed to create extract all menu\n");
                goto fail;
            }
            switch_active_menu(access, access->extractAll.osdMenu, EXTRACT_ALL_MENU_ACTIVE);
        }
        else
        {
            currentTapeDirItemIndex -= 1; /* '../' */
            if (access->cache.tapeDirs[currentRootItemIndex].haveExtractAllOption)
            {
                currentTapeDirItemIndex -= 1; /* 'Extract all' */
            }
            
            /* if file size is zero on the disk then check it is on the current tape */
            if (access->cache.tapeDirs[currentRootItemIndex].entries[currentTapeDirItemIndex].sizeOnDisk <= 0)
            {
                if (access->cache.tapeDirs[currentRootItemIndex].entries[currentTapeDirItemIndex].sizeOnTape <= 0 ||
                    !qce_is_current_lto(access->extract, access->cache.tapeDirs[currentRootItemIndex].name))
                {
                    /* don't provide a play menu */
                    goto fail;
                }
            }
            
            /* create and switch to the play select menu */
            if (!create_play_select_menu(access, &access->cache.tapeDirs[currentRootItemIndex], currentTapeDirItemIndex))
            {
                ml_log_warn("Failed to create select play menu\n");
                goto fail;
            }
            switch_active_menu(access, access->playSelect.osdMenu, PLAY_SELECT_MENU_ACTIVE);
        }
    }
    else if (access->activeMenu == PLAY_SELECT_MENU_ACTIVE)
    {
        if (access->playSelect.osdMenu == NULL)
        {
            /* go back up to the root menu */
            switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
            goto fail;
        }
        
        currentPlaySelectItemIndex = access->playSelect.osdMenu->currentItemIndex;
        if (currentPlaySelectItemIndex == 0 || access->playSelect.directory == NULL)
        {
            /* go back to the tape dir menu */
            currentRootItemIndex = access->cache.osdMenu->currentItemIndex;
            if (access->cache.tapeDirs[currentRootItemIndex].osdMenu != NULL)
            {
                switch_active_menu(access, access->cache.tapeDirs[currentRootItemIndex].osdMenu, TAPE_DIR_MENU_ACTIVE);
            }
            else
            {
                switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
                goto fail;
            }
        }
        else
        {
            if (currentPlaySelectItemIndex == 1 && access->playSelect.haveStopExtractOption)
            {
                currentRootItemIndex = access->cache.osdMenu->currentItemIndex;
                
                /* stop extracting the file */
                qce_stop_extract(access->extract);

                /* go back to the tape dir menu */
                if (access->cache.tapeDirs[currentRootItemIndex].osdMenu != NULL)
                {
                    switch_active_menu(access, access->cache.tapeDirs[currentRootItemIndex].osdMenu, TAPE_DIR_MENU_ACTIVE);
                }
                else
                {
                    switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
                    goto fail;
                }
            }
            else if (currentPlaySelectItemIndex == 1 && access->playSelect.haveStartExtractOption)
            {
                currentRootItemIndex = access->cache.osdMenu->currentItemIndex;
                
                /* start extracting the file */
                qce_start_extract(access->extract, access->cache.tapeDirs[currentRootItemIndex].name, access->playSelect.name);

                /* go back to the tape dir menu */
                if (access->cache.tapeDirs[currentRootItemIndex].osdMenu != NULL)
                {
                    switch_active_menu(access, access->cache.tapeDirs[currentRootItemIndex].osdMenu, TAPE_DIR_MENU_ACTIVE);
                }
                else
                {
                    switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
                    goto fail;
                }
            }
            else
            {
                /* check that the file is ready to be played */
                if (!qce_can_play(access->extract, access->playSelect.directory, access->playSelect.name))
                {
                    /* ignore request to play the file */
                    goto fail;
                }
                
                
                /* play a new session or an existing session */
            
                newSessionItemIndex = 1;
                if (access->playSelect.haveStartExtractOption)
                {
                    newSessionItemIndex++;
                }
                if (access->playSelect.haveStopExtractOption)
                {
                    newSessionItemIndex++;
                }
                
                /* copy the selected directory, filename (and session name) */
                SAFE_FREE(&access->selectedDirectory);
                CALLOC_OFAIL(access->selectedDirectory, char, strlen(access->playSelect.directory) + 1);
                strcpy(access->selectedDirectory, access->playSelect.directory);
    
                SAFE_FREE(&access->selectedName);
                CALLOC_OFAIL(access->selectedName, char, strlen(access->playSelect.name) + 1);
                strcpy(access->selectedName, access->playSelect.name);
    
                SAFE_FREE(&access->selectedSessionName);
                if (currentPlaySelectItemIndex > newSessionItemIndex &&
                    currentPlaySelectItemIndex - newSessionItemIndex - 1 < MAX_SELECT_SESSION &&
                    access->playSelect.sessionNames[currentPlaySelectItemIndex - newSessionItemIndex - 1] != NULL)
                {
                    /* a valid existing session was selected */
                    CALLOC_OFAIL(access->selectedSessionName, char, 
                        strlen(access->playSelect.sessionNames[currentPlaySelectItemIndex - newSessionItemIndex - 1]) + 1);
                    strcpy(access->selectedSessionName, 
                        access->playSelect.sessionNames[currentPlaySelectItemIndex - newSessionItemIndex - 1]);
                }
    
                /* stop playing the current file */
                mc_stop(access->mediaControl);
            }
        }
    }
    else if (access->activeMenu == EXTRACT_ALL_MENU_ACTIVE)
    {
        if (access->extractAll.osdMenu == NULL)
        {
            /* go back up to the root menu */
            switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
            goto fail;
        }
        
        currentExtractAllItemIndex = access->extractAll.osdMenu->currentItemIndex;
        if (currentExtractAllItemIndex == 0 || access->extractAll.directory == NULL ||
            (!access->extractAll.haveStopExtractAllOption && !access->extractAll.haveStartExtractAllOptions))
        {
            /* go back to the tape dir menu */
            currentRootItemIndex = access->cache.osdMenu->currentItemIndex;
            if (access->cache.tapeDirs[currentRootItemIndex].osdMenu != NULL)
            {
                switch_active_menu(access, access->cache.tapeDirs[currentRootItemIndex].osdMenu, TAPE_DIR_MENU_ACTIVE);
            }
            else
            {
                switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
                goto fail;
            }
        }
        else
        {
            if (currentExtractAllItemIndex == 1 && access->extractAll.haveStopExtractAllOption)
            {
                qce_stop_extract(access->extract);
            }
            else if (access->extractAll.haveStartExtractAllOptions &&
                ((currentExtractAllItemIndex == 1 && !access->extractAll.haveStopExtractAllOption) ||
                    (currentExtractAllItemIndex == 2 && access->extractAll.haveStopExtractAllOption)))
            {
                qce_start_extract_remainder(access->extract, access->extractAll.directory);
            }
            else if (access->extractAll.haveStartExtractAllOptions &&
                ((currentExtractAllItemIndex == 2 && !access->extractAll.haveStopExtractAllOption) ||
                    (currentExtractAllItemIndex == 3 && access->extractAll.haveStopExtractAllOption)))
            {
                qce_start_extract_all(access->extract, access->extractAll.directory);
            }
            
            /* go back to the tape dir menu */
            currentRootItemIndex = access->cache.osdMenu->currentItemIndex;
            if (access->cache.tapeDirs[currentRootItemIndex].osdMenu != NULL)
            {
                switch_active_menu(access, access->cache.tapeDirs[currentRootItemIndex].osdMenu, TAPE_DIR_MENU_ACTIVE);
            }
            else
            {
                switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
                goto fail;
            }
        }
    }
    else /* access->activeMenu == DELETE_TAPE_DIR_MENU_ACTIVE */
    {
        if (access->deleteTapeDir.osdMenu == NULL)
        {
            /* go back up to the root menu */
            switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
            goto fail;
        }
        
        currentDeleteTapeDirItemIndex = access->deleteTapeDir.osdMenu->currentItemIndex;
        if (currentDeleteTapeDirItemIndex == 0 || 
            access->deleteTapeDir.filename == NULL ||
            access->deleteTapeDir.name == NULL)
        {
            /* go back up to the root menu */
            switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
        }
        else
        {
            /* check the directory is not the current one for extract */
            if (qce_is_current_lto(access->extract, access->deleteTapeDir.name))
            {
                ml_log_info("Not removing current extract tape directory '%s'\n", access->deleteTapeDir.filename);
                
                /* ignore delete request and go back up to the root menu */
                switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
                goto fail;
            }
            
            /* check the current playing file is not from in the directory */
            if (access->currentPlayLTONumber != NULL && 
                strcmp(access->deleteTapeDir.name, access->currentPlayLTONumber) == 0)
            {
                ml_log_info("Not removing current file playing tape directory '%s'\n", access->deleteTapeDir.filename);
                
                /* ignore delete request and go back up to the root menu */
                switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
                goto fail;
            }
            
            /* call the delete script or delete the tape directory */
            if (access->deleteScriptName != 0)
            {
                /* call the delete script */
                
                strcpy(rmCmd, access->deleteScriptName);
                if (access->deleteScriptOptions != 0)
                {
                    strcat(rmCmd, " ");
                    strcat(rmCmd, access->deleteScriptOptions);
                }
                strcat(rmCmd, " ");
                strcat(rmCmd, access->deleteTapeDir.filename);
                if (system(rmCmd) == 0)
                {
                    ml_log_info("Called '%s' to remove the tape directory '%s'\n", access->deleteScriptName, access->deleteTapeDir.filename); 
                }
                else
                {
                    ml_log_error("Script '%s' failed to remove tape directory '%s'\n", access->deleteScriptName, access->deleteTapeDir.filename); 
                }
            }
            else
            {
                /* delete the tape directory */
                
                strcpy(rmCmd, "rm -Rf ");
                strcat(rmCmd, access->deleteTapeDir.filename);
                if (system(rmCmd) == 0)
                {
                    ml_log_info("Removed tape directory '%s'\n", access->deleteTapeDir.filename); 
                }
                else
                {
                    ml_log_error("Failed to remove tape directory '%s'\n", access->deleteTapeDir.filename); 
                }
            }
            
            /* go back up to the root menu */
            switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
        }
    }

    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
    return;
    
fail:
    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
}

/* this is ued to delete a tape directory from the root menu */
static void qla_select_menu_item_center(void* data)
{
    QCLTOAccess* access = (QCLTOAccess*)data;
    int currentRootItemIndex;
    QCLTOExtractState extractState;
    
    
    PTHREAD_MUTEX_LOCK(&access->cacheMutex); 

    if (access->cache.osdMenu == NULL)
    {
        goto fail;
    }

    if (access->activeMenu != ROOT_MENU_ACTIVE)
    {
        goto fail;
    }
    
    currentRootItemIndex = access->cache.osdMenu->currentItemIndex;
    if (currentRootItemIndex >= access->cache.numTapeDirs)
    {
        /* no items */
        goto fail;
    }
    
    /* check the directory is not the current one for extract */
    qce_get_lto_state(access->extract, &extractState, 0x0);
    if (strcmp(access->cache.tapeDirs[currentRootItemIndex].name, extractState.ltoSpoolNumber) == 0)
    {
        goto fail;
    }
    
    /* check the current playing file is not from in the directory */
    if (access->currentPlayLTONumber != NULL && 
        strcmp(access->cache.tapeDirs[currentRootItemIndex].name, access->currentPlayLTONumber) == 0)
    {
        goto fail;
    }
    
    /* create and switch to the delete tape directory menu */
    if (!create_delete_tape_dir_menu(access, &access->cache.tapeDirs[currentRootItemIndex]))
    {
        ml_log_warn("Failed to create delete tape directory menu\n");
        goto fail;
    }
    switch_active_menu(access, access->deleteTapeDir.osdMenu, DELETE_TAPE_DIR_MENU_ACTIVE);

    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
    return;
    
fail:
    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
}

/* in the tape directory menu this will play the file using the last session */ 
static void qla_select_menu_item_extra(void* data)
{
    QCLTOAccess* access = (QCLTOAccess*)data;
    int currentRootItemIndex;
    int currentTapeDirItemIndex;
    SessionInfo sessions[MAX_SELECT_SESSION];
    
    memset(sessions, 0, sizeof(sessions));
    
    PTHREAD_MUTEX_LOCK(&access->cacheMutex); 

    if (access->cache.osdMenu == NULL)
    {
        goto fail;
    }

    if (access->activeMenu != TAPE_DIR_MENU_ACTIVE)
    {
        goto fail;
    }
    
    currentRootItemIndex = access->cache.osdMenu->currentItemIndex;
    
    if (access->cache.tapeDirs[currentRootItemIndex].osdMenu == NULL)
    {
        /* menu no longer present - go back up to the root menu */
        switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);
        goto fail;
    }
    
    currentTapeDirItemIndex = access->cache.tapeDirs[currentRootItemIndex].osdMenu->currentItemIndex;

    if (currentTapeDirItemIndex <= 0) /* selected '../' */
    {
        /* can't play this - ignore the request */
        goto fail;
    }
    else if (currentTapeDirItemIndex == 1 && 
        access->cache.tapeDirs[currentRootItemIndex].haveExtractAllOption) /* Extract All */
    {
        /* can't play this - ignore the request */
        goto fail;
    }
    else
    {
        currentTapeDirItemIndex -= 1; /* '../' */
        if (access->cache.tapeDirs[currentRootItemIndex].haveExtractAllOption)
        {
            currentTapeDirItemIndex -= 1; /* 'Extract all' */
        }

        if (!qce_can_play(access->extract, access->cache.tapeDirs[currentRootItemIndex].filename, 
            access->cache.tapeDirs[currentRootItemIndex].entries[currentTapeDirItemIndex].name))
        {
            /* can't play this because extraction has started but has not written the minimum number
            of bytes to disk to allow play to start - ignore the request */
            goto fail;
        }
        
        /* if file size is zero on the disk then check it is on the current tape */
        if (access->cache.tapeDirs[currentRootItemIndex].entries[currentTapeDirItemIndex].sizeOnDisk <= 0)
        {
            /* no file on disk to play - ignore the request */
            goto fail;
        }
        
        /* play the file, using the last session */
        
        /* copy the selected directory, filename (and session name) */
        SAFE_FREE(&access->selectedDirectory);
        CALLOC_OFAIL(access->selectedDirectory, char, strlen(access->cache.tapeDirs[currentRootItemIndex].filename) + 1);
        strcpy(access->selectedDirectory, access->cache.tapeDirs[currentRootItemIndex].filename);

        SAFE_FREE(&access->selectedName);
        CALLOC_OFAIL(access->selectedName, char, strlen(access->cache.tapeDirs[currentRootItemIndex].entries[currentTapeDirItemIndex].name) + 1);
        strcpy(access->selectedName, access->cache.tapeDirs[currentRootItemIndex].entries[currentTapeDirItemIndex].name);

        SAFE_FREE(&access->selectedSessionName);
        get_sessions(access, access->selectedDirectory, access->selectedName, sessions);
        if (sessions[0].name != NULL)
        {
            CALLOC_OFAIL(access->selectedSessionName, char, strlen(sessions[0].name) + 1);
            strcpy(access->selectedSessionName, sessions[0].name);
        }

        /* stop playing the current file */
        mc_stop(access->mediaControl);
    }

    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
    return;
    
fail:
    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
}

static void qla_free(void* data)
{
    QCLTOAccess* access = (QCLTOAccess*)data;
    
    if (access == NULL)
    {
        return;
    }

    /* sever connection with player and OSD */
    PTHREAD_MUTEX_LOCK(&access->cacheMutex);
    free_cache_osd_menus(access);
    access->listener = NULL;
    access->mediaControl = NULL;
    access->osd = NULL;
    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
}



int qla_create_qc_lto_access(const char* cacheDirectory, QCLTOExtract* extract, 
    const char* deleteScriptName, const char* deleteScriptOptions, QCLTOAccess** access)
{
    QCLTOAccess* newAccess = NULL;
    DIR* cacheDir = NULL;

    /* first check we can open the directory */    
    if ((cacheDir = opendir(cacheDirectory)) == NULL)
    {
        ml_log_error("Failed to open QC LTO cache directory '%s'\n", cacheDirectory);
        return 0;
    }
    closedir(cacheDir);

    CALLOC_ORET(newAccess, QCLTOAccess, 1);
    newAccess->inotifyFD = -1;
    newAccess->extract = extract;
    
    CALLOC_OFAIL(newAccess->cacheDirName, char, strlen(cacheDirectory) + 1);
    strcpy(newAccess->cacheDirName, cacheDirectory);

    if (deleteScriptName != 0)
    {
        CALLOC_OFAIL(newAccess->deleteScriptName, char, strlen(deleteScriptName) + 1);
        strcpy(newAccess->deleteScriptName, deleteScriptName);
        
        if (deleteScriptOptions != 0)
        {
            CALLOC_OFAIL(newAccess->deleteScriptOptions, char, strlen(deleteScriptOptions) + 1);
            strcpy(newAccess->deleteScriptOptions, deleteScriptOptions);
        }
    }


    newAccess->inotifyFD = inotify_init();
    if (newAccess->inotifyFD < 0)
    {
        ml_log_error("Failed to initialise inotify for qc lto access: %s\n", strerror(errno));
        goto fail;
    }

    newAccess->cacheWD = inotify_add_watch(newAccess->inotifyFD, cacheDirectory, 
        IN_MOVED_FROM | IN_MOVED_TO | IN_CREATE | IN_DELETE);
    if (newAccess->cacheWD < 0)
    {
        ml_log_error("Failed to add inotify watch for qc tape cache: %s\n", strerror(errno));
        goto fail;
    }
    
    load_cache_contents(newAccess);
    
    
    CHK_OFAIL(init_mutex(&newAccess->cacheMutex));

    CHK_OFAIL(create_joinable_thread(&newAccess->workerThreadId, update_worker_thread, newAccess)); 

    
    *access = newAccess;
    return 1;
    
fail:
    qla_free_qc_lto_access(&newAccess);
    return 0;
}

void qla_free_qc_lto_access(QCLTOAccess** access)
{
    if (*access == NULL)
    {
        return;
    }
    
    (*access)->stopping = 1;
    join_thread(&(*access)->workerThreadId, NULL, NULL);
    
    free_cache_contents(*access);

    SAFE_FREE(&(*access)->cacheDirName);

    SAFE_FREE(&(*access)->deleteScriptName);
    SAFE_FREE(&(*access)->deleteScriptOptions);
    
    SAFE_FREE(&(*access)->selectedDirectory);
    SAFE_FREE(&(*access)->selectedName);
    SAFE_FREE(&(*access)->selectedSessionName);

    SAFE_FREE(&(*access)->currentSessionName);
    
    SAFE_FREE(&(*access)->currentPlayLTONumber);
    SAFE_FREE(&(*access)->currentPlayName);
    
	destroy_mutex(&(*access)->cacheMutex);


    if ((*access)->inotifyFD >= 0)
    {
        close((*access)->inotifyFD);
        (*access)->inotifyFD = -1;
    }
    
    SAFE_FREE(access);
}

int qla_connect_to_player(QCLTOAccess* access, MediaPlayer* player)
{
    SAFE_FREE(&access->selectedDirectory);
    SAFE_FREE(&access->selectedName);
    SAFE_FREE(&access->selectedSessionName);
 
    
    PTHREAD_MUTEX_LOCK(&access->cacheMutex); 
    
    access->osd = msk_get_osd(ply_get_media_sink(player));
    access->mediaControl = ply_get_media_control(player);

    CHK_OFAIL(create_cache_menu(access));
    switch_active_menu(access, access->cache.osdMenu, ROOT_MENU_ACTIVE);

    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex); 
    
    access->menuHandler.data = access;
    access->menuHandler.set_listener = qla_set_listener;
    access->menuHandler.next_menu_item = qla_next_menu_item;
    access->menuHandler.previous_menu_item = qla_previous_menu_item;
    access->menuHandler.select_menu_item_left = qla_select_menu_item_left;
    access->menuHandler.select_menu_item_right = qla_select_menu_item_right;
    access->menuHandler.select_menu_item_center = qla_select_menu_item_center;
    access->menuHandler.select_menu_item_extra = qla_select_menu_item_extra;
    access->menuHandler.free = qla_free;
    
    ply_set_menu_handler(player, &access->menuHandler);

    return 1;
    
fail:
    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
    return 0;
}

int qla_get_file_to_play(QCLTOAccess* access, char directory[FILENAME_MAX], char name[FILENAME_MAX], 
    char sessionName[FILENAME_MAX])
{
    if (access->selectedDirectory == NULL || 
        access->selectedName == NULL)
    {
        return 0;
    }
    
    strncpy(directory, access->selectedDirectory, FILENAME_MAX);
    directory[FILENAME_MAX - 1] = '\0';
    strncpy(name, access->selectedName, FILENAME_MAX);
    name[FILENAME_MAX - 1] = '\0';
    if (access->selectedSessionName == NULL)
    {
        strcpy(sessionName, "");
    }
    else
    {
        strncpy(sessionName, access->selectedSessionName, FILENAME_MAX);
        sessionName[FILENAME_MAX - 1] = '\0';
    }
    
    return 1;
}

void qla_set_current_session_name(QCLTOAccess* access, const char* sessionName)
{
    SAFE_FREE(&access->currentSessionName);
    if (sessionName != NULL && sessionName[0] != '\0')
    {
        CALLOC_OFAIL(access->currentSessionName, char, strlen(sessionName) + 1);
        strcpy(access->currentSessionName, sessionName);
    }
    
fail:
    return;
}

void qla_set_current_play_name(QCLTOAccess* access, const char* directory, const char* name)
{
    const char* ltoNumber;
    
    PTHREAD_MUTEX_LOCK(&access->cacheMutex); 

    SAFE_FREE(&access->currentPlayLTONumber);
    SAFE_FREE(&access->currentPlayName);
    if (directory != NULL && directory[0] != '\0')
    {
        if ((ltoNumber = strrchr(directory, '/')) != NULL)
        {
            ltoNumber++;
        }
        else
        {
            ltoNumber = directory;
        }
        CALLOC_OFAIL(access->currentPlayLTONumber, char, strlen(ltoNumber) + 1);
        strcpy(access->currentPlayLTONumber, ltoNumber);
    }
    if (name != NULL && name[0] != '\0')
    {
        CALLOC_OFAIL(access->currentPlayName, char, strlen(name) + 1);
        strcpy(access->currentPlayName, name);
    }
    
    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex);
    return;

fail:
    PTHREAD_MUTEX_UNLOCK(&access->cacheMutex); 
    return;
}

void qla_remove_old_sessions(QCLTOAccess* access, const char* directory, const char* name)
{
    DIR* tapeDirStream = NULL;
    struct dirent* tapeDirent;
    struct SessionInfo
    {
        char name[256];
        time_t modTime;
    } sessions[MAX_KEEP_SESSION];
    struct stat statBuf;
    int i;
    char rmCmd[FILENAME_MAX];
    
    memset(sessions, 0, sizeof(sessions));

    
    /* get sessions, ordered by modification time and remove delete old sessions */
    if ((tapeDirStream = opendir(directory)) != NULL)
    {
        while ((tapeDirent = readdir(tapeDirStream)) != NULL)
        {
            if (qcs_is_session_file(tapeDirent->d_name, name) &&
                get_file_stat(directory, tapeDirent->d_name, NULL, &statBuf))
            {
                for (i = 0; i < MAX_KEEP_SESSION; i++)
                {
                    if (statBuf.st_mtime > sessions[i].modTime)
                    {
                        /* remove old session */
                        if (sessions[MAX_KEEP_SESSION - 1].name[0] != '\0')
                        {
                            strcpy(rmCmd, "rm -f ");
                            strcat(rmCmd, directory);
                            strcat_separator(rmCmd);
                            strcat(rmCmd, sessions[MAX_KEEP_SESSION - 1].name);
                            if (system(rmCmd) == 0)
                            {
                                ml_log_info("Removed old qc session file '%s/%s'\n", directory, sessions[MAX_KEEP_SESSION - 1].name); 
                            }
                        }
                        
                        /* shift sessions up if i is not the index of the last one*/
                        if (i < MAX_KEEP_SESSION - 1)
                        {
                            memmove(&sessions[i + 1], &sessions[i], (MAX_KEEP_SESSION - 1 - i) * sizeof(struct SessionInfo));
                        }
                        
                        /* set session */
                        sessions[i].modTime = statBuf.st_mtime;
                        strncpy(sessions[i].name, tapeDirent->d_name, 256);
                        sessions[i].name[255] = '\0';
                        break;
                    }
                }
                /* remove if not inserted */
                if (i >= MAX_KEEP_SESSION)
                {
                    strcpy(rmCmd, "rm -f ");
                    strcat(rmCmd, directory);
                    strcat_separator(rmCmd);
                    strcat(rmCmd, tapeDirent->d_name);
                    if (system(rmCmd) == 0)
                    {
                        ml_log_info("Removed old qc session file '%s/%s'\n", directory, tapeDirent->d_name);
                    }
                }
            }
        }
        closedir(tapeDirStream);
    }
}

#endif //#if defined(DISABLE_QC_LTO_ACCESS)

