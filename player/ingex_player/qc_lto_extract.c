/*
 * $Id: qc_lto_extract.c,v 1.6 2009/12/17 15:57:40 john_f Exp $
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

#if ! defined(_LARGEFILE_SOURCE) && ! defined(_FILE_OFFSET_BITS)
#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64
#endif

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include "qc_lto_extract.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"

#define __USE_ISOC99    1
#include <limits.h> /* ULLONG_MAX */
// Eee PC doesn't define ULLONG_MAX
#ifndef ULLONG_MAX
#define ULLONG_MAX 18446744073709551615ULL
#endif


#define TAPEBLOCK                           (512*512)

#define INDEX_ENTRY_ALLOC_STEP              20

#define FREE_DISK_SPACE_MARGIN              10e6


typedef struct
{
    char name[32];
    int num;
    int64_t fileSize;
    int isSelfReference;
} IndexFileEntry;

typedef struct
{
    char ltoNumber[32];
    char* filename;
    IndexFileEntry* entries;
    int numEntries;
    int allocEntries;
} IndexFile;

struct QCLTOExtract
{
    char* cacheDirectory;
    char* tapeDevice;

    char startLTOSpoolNumber[32];
    char startFilename[32];
    int startExtract;
    int extractAll;
    int remainderOnly;
    int stopExtract;
    int64_t bytesWritten;

    QCLTOExtractState state;
    pthread_mutex_t stateMutex;

    IndexFile indexFile;
    pthread_mutex_t indexFileMutex;

    int stopping;
    pthread_t extractThreadId;

    char* currentPlayLTONumber;
    char* currentPlayName;
};


static int64_t get_file_size_on_disk(const char* name1, const char* name2, const char* name3)
{
    char filepath[FILENAME_MAX];
    int result;
    struct stat statBuf;

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

    result = stat(filepath, &statBuf);
    if (result != 0)
    {
        return -1;
    }

    return statBuf.st_size;
}


static int get_file_from_index(QCLTOExtract* extract, const char* ltoSpoolNumber, const char* filename, IndexFileEntry* entry)
{
    int i;
    int foundIt = 0;

    PTHREAD_MUTEX_LOCK(&extract->indexFileMutex);

    if (strcmp(extract->indexFile.ltoNumber, ltoSpoolNumber) == 0)
    {
        for (i = 0; i < extract->indexFile.numEntries; i++)
        {
            if (strcmp(extract->indexFile.entries[i].name, filename) == 0)
            {
                *entry = extract->indexFile.entries[i];
                foundIt = 1;
                break;
            }
        }
    }

    PTHREAD_MUTEX_UNLOCK(&extract->indexFileMutex);

    return foundIt;
}

static void set_extract_state(QCLTOExtract* extract, QCLTOExtractStatus status,
    const char* ltoSpoolNumber, const char* currentExtractingFile, int extractAll)
{
    PTHREAD_MUTEX_LOCK(&extract->stateMutex);

    extract->state.status = status;
    if (ltoSpoolNumber != NULL)
    {
        strcpy(extract->state.ltoSpoolNumber, ltoSpoolNumber);
    }
    if (currentExtractingFile != NULL)
    {
        strcpy(extract->state.currentExtractingFile, currentExtractingFile);
    }
    if (extractAll >= 0)
    {
        extract->state.extractAll = extractAll;
    }

    extract->state.updated = -1;

    PTHREAD_MUTEX_UNLOCK(&extract->stateMutex);
}

static int parse_tar_ustar_header(const unsigned char* tarBytes, int64_t* fileSize, char* typeFlag)
{
    int i;
    const unsigned char* sizeField;

    /* type flag field (offset 156, size 1) */
    *typeFlag = tarBytes[156];


    /* parse the size field (offset 124, size 12) */

    /*
    if the first byte in the size field equals 0x80, then the size is the next
    11 bytes representing a binary decimal number using big endian byte order. This
    format is a GNU extension, which occurs when the tar --format parameter was _not_
    set to "posix" and the file size exceeds 8GB. See also
    http://swtch.com/usr/local/plan9/src/cmd/tar.c (search for "Binsize")

    Otherwise the file size is represented in Ascii text characters with max 11 octal numbers
    terminated by a null or space character
    */

    sizeField = &tarBytes[124];

    if (sizeField[0] == 0x80) /* GNU extension, big endian binary decimal number */
    {
        *fileSize = 0;
        for (i = 1; i < 12; i++)
        {
            *fileSize = (*fileSize << 8) | sizeField[i];
        }
    }
    else /* octal ascii string terminated by null or space character */
    {
        unsigned long long sizeLL = strtoull((const char*)sizeField, NULL, 8);
        if (sizeLL == ULLONG_MAX)
        {
            ml_log_error("Invalid tar file size found\n");
            *fileSize = 0;
            return 0;
        }
        else
        {
            *fileSize = sizeLL;
        }
    }

    return 1;
}

static int parse_tar_extended_header_data(const unsigned char* tarBytes, int64_t bytesSize, int64_t* fileSize)
{
    const char* fieldPtr = (const char*)tarBytes;
    int length;
    const char* keywordPtr;
    const char* valuePtr;
    int64_t processedSize = 0;

    /* search for a size field */

    /*
    From "The Open Group Base Specifications Issue 6, IEEE Std 1003.1, 2004 Edition, pax - portable archive interchange":

    An extended header shall consist of one or more records, each constructed as follows:
    "%d %s=%s\n", <length>, <keyword>, <value>
    The extended header records shall be encoded according to the ISO/IEC 10646-1:2000 standard (UTF-8).
    The <length> field, <blank>, equals sign, and <newline> shown shall be limited to the portable
    character set, as encoded in UTF-8. The <keyword> and <value> fields can be any UTF-8 characters.
    The <length> field shall be the decimal length of the extended header record in octets, including
    the trailing <newline>.
    */

    while (processedSize < bytesSize)
    {
        if (sscanf(fieldPtr, "%d\n", &length) != 1)
        {
            ml_log_error("Failed to parse <length> of length/keyword/value in tar extended header data\n");
            return 0;
        }

        /* locate the keyword and value */
        keywordPtr = strchr(fieldPtr, ' ');
        if (keywordPtr == NULL)
        {
            ml_log_error("Failed to locate the <keyword> of length/keyword/value in tar extended header data\n");
            return 0;
        }
        keywordPtr++;
        valuePtr = strchr(keywordPtr, '=');
        if (valuePtr == NULL)
        {
            ml_log_error("Failed to locate the <value> of length/keyword/value in tar extended header data\n");
            return 0;
        }
        valuePtr++;

        if (strncmp(keywordPtr, "size", valuePtr - keywordPtr - 1) == 0)
        {
            /* parse the "size" value and then we are done */
            *fileSize = 0;
            while (*valuePtr != '\n' && (valuePtr - fieldPtr) < length)
            {
                if (*valuePtr >= '0' && *valuePtr <= '9')
                {
                    *fileSize = *fileSize * 10 + (*valuePtr - '0');
                }
                valuePtr++;
            }
            if (*valuePtr != '\n')
            {
                ml_log_error("Invalid value in tar extended header data - value exceeds length specification\n");
                return 0;
            }
            return 1;
        }

        processedSize += length;
        fieldPtr += length;
    }

    /* no size field was found */
    *fileSize = -1;
    return 1;
}

static int parse_tar_header(const unsigned char* tarBytes, int64_t* fileSize, int64_t* offset)
{
    char typeFlag;
    int64_t size;

    *offset = 0;
    *fileSize = -1;

    while (1)
    {
        size = 0;
        typeFlag = 0;

        if (!parse_tar_ustar_header(&tarBytes[*offset], &size, &typeFlag))
        {
            ml_log_error("Failed to parse ustar header\n");
            return 0;
        }
        *offset += 512; /* skip the ustar header */

        if (typeFlag == 'g')
        {
            /* data is a global extended header - parse the file size if present */
            if (!parse_tar_extended_header_data(&tarBytes[*offset], size, fileSize))
            {
                ml_log_error("Failed to parse tar extended header\n");
                return 0;
            }
            *offset += ((size + 512) / 512) * 512; /* skip the extended header data, rounded up to 512 tar block size */
        }
        else if (typeFlag == 'x')
        {
            /* data is a extended header - parse the file size if present and not already defined by a
            global extended header */
            if (*fileSize < 0)
            {
                if (!parse_tar_extended_header_data(&tarBytes[*offset], size, fileSize))
                {
                    ml_log_error("Failed to parse tar extended header\n");
                    return 0;
                }
            }
            *offset += ((size + 512) / 512) * 512; /* skip the extended header data, rounded up to 512 tar block size */
        }
        else
        {
            if (*fileSize < 0)
            {
                /* file size was not defined in an extended header */
                *fileSize = size;
            }
            break;
        }
    }

    return 1;
}

static int get_tape_pos(const char* device, int* fileNo, int* blockNo)
{
    int fd;
    if ((fd = open(device, O_RDONLY|O_NONBLOCK|O_LARGEFILE)) == -1) {
        ml_log_error("Failed to open device %s: %s\n", device, strerror(errno));
        return 0;
    }

    // Get status
    struct mtget status;
    if (ioctl(fd, MTIOCGET, &status) == -1) {
        close(fd); return 0;
    }

    *fileNo = status.mt_fileno;
    *blockNo = status.mt_blkno;

    close(fd); return 1;
}

static int rewind_tape(const char *device)
{
    int fd;
    if ((fd = open(device, O_RDONLY|O_NONBLOCK|O_LARGEFILE)) == -1) {
        ml_log_error("Failed to open device %s: %s\n", device, strerror(errno));
        return 0;
    }

    // Perform a rewind operation
    struct mtop control;
    control.mt_op = MTREW;
    control.mt_count = 1;
    if (ioctl(fd, MTIOCTOP, &control) == -1) {
        ml_log_error("MTIOCTOP mt_op=MTREW failed: %s\n", strerror(errno));
        close(fd);
        return 0;
    }

    close(fd);
    return 1;
}

// Set block size and compression parameters
// Should be called after tape load since settings are reset after tape load
static int set_tape_params(const char *device)
{
    int fd;
    if ((fd = open(device, O_RDONLY|O_NONBLOCK|O_LARGEFILE)) == -1) {
        ml_log_error("Failed to open device %s: %s\n", device, strerror(errno));
        return 0;
    }

    // Perform the setblk operation
    struct mtop control;
    control.mt_op = MTSETBLK;
    control.mt_count = TAPEBLOCK;   // block size
    if (ioctl(fd, MTIOCTOP, &control) == -1) {
        ml_log_error("MTIOCTOP mt_op=MTSETBLK failed: %s\n", strerror(errno));
        close(fd);
        return 0;
    }

    // Turn off drive compression
    control.mt_op = MTCOMPRESSION;
    control.mt_count = 0;           // turn off compression
    if (ioctl(fd, MTIOCTOP, &control) == -1) {
        ml_log_error("MTIOCTOP mt_op=MTSETBLK failed: %s\n", strerror(errno));
        close(fd);
        return 0;
    }

    close(fd);
    return 1;
}

// returns whether tape drive state is understood, including drive failure
static QCLTOExtractStatus probe_tape_status(const char *device)
{
    int fd;
    if ((fd = open(device, O_RDONLY|O_NONBLOCK|O_LARGEFILE)) == -1) {
        switch (errno) {
            case EBUSY:
                // E.g. when another process has opened the same device
                // domo: busy when ejecting tape
                // qc: if we are polling and the tape is busy then either it is ejecting or
                //     another process is using the tape device
                return LTO_BUSY_STATUS;
                break;
            default:
                // Incorrect device, permission errors expected here
                return LTO_NO_TAPE_DEVICE_ACCESS_STATUS;
                break;
        }
    }

    // Perform a NOP to prepare for getting status
    struct mtop control;
    control.mt_op = MTNOP;
    control.mt_count = 1;
    if (ioctl(fd, MTIOCTOP, &control) == -1) {
        // Errors which can happen in order of usefulness of information
        switch (errno) {
            case ENOMEDIUM:
                // domo: no tape
                close(fd); return LTO_NO_TAPE_STATUS;
                break;
            case EIO:
                // Two cases observed:
                // domo: busy after tape inserted but not ready (still loading)
                // neffie: after drive no longer responds (erase + eject)
                close(fd); return LTO_BUSY_LOAD_OR_EJECT_STATUS;
                break;
            default:
                close(fd); return LTO_POLL_FAILED_STATUS;
                break;
        }
    }

    // Get status
    struct mtget status;
    if (ioctl(fd, MTIOCGET, &status) == -1) {
        close(fd); return LTO_POLL_FAILED_STATUS;
    }

    if (GMT_ONLINE(status.mt_gstat)) {  // tape loaded
        // Experiment on domo showed that a fresh tape load gives a momentary
        // ONLINE state but with fileno and blkno set to -1
        if (status.mt_fileno == -1 && status.mt_blkno == -1) {
            close(fd); return LTO_BUSY_LOAD_POS_STATUS;
        }
        close(fd); return LTO_ONLINE_STATUS;
    }

    // If we get here, tape is not ONLINE, NOTAPE or BUSY
    // so we don't know what the state is
    close(fd); return LTO_POLL_FAILED_STATUS;
}

static int seek_to_file(QCLTOExtract* extract, int num)
{
    int haveRewoundTape = 0;
    int fileNo = -1;
    int blockNo = -1;

    if (get_tape_pos(extract->tapeDevice, &fileNo, &blockNo))
    {
        ml_log_warn("Failed to get tape pos - rewinding tape\n");
    }

    if (fileNo < 0)
    {
        set_extract_state(extract, LTO_BUSY_REWINDING_STATUS, NULL, NULL, -1);

        if (!rewind_tape(extract->tapeDevice))
        {
            ml_log_error("Failed to rewind tape to allow seek to file\n");
            return 0;
        }
        haveRewoundTape = 1;

        if (!get_tape_pos(extract->tapeDevice, &fileNo, &blockNo) || fileNo < 0 || blockNo < 0)
        {
            ml_log_error("Failed to get tape pos\n");
            return 0;
        }
    }

    if (num == fileNo && blockNo == 0)
    {
        return 1;
    }

    if (num == 0)
    {
        if (!haveRewoundTape)
        {
            set_extract_state(extract, LTO_BUSY_REWINDING_STATUS, NULL, NULL, -1);

            /* the start of the tape doesn't have a file mark so we do a rewind */
            if (!rewind_tape(extract->tapeDevice))
            {
                ml_log_error("Failed to rewind tape to allow seek to file\n");
                return 0;
            }
        }
    }
    else
    {
        int fd;
        if ((fd = open(extract->tapeDevice, O_RDONLY|O_NONBLOCK|O_LARGEFILE)) == -1) {
            ml_log_error("Failed to open device %s: %s\n", extract->tapeDevice, strerror(errno));
            return 0;
        }

        struct mtop control;
        if (num < fileNo)
        {
            control.mt_op = MTBSFM; // Backward space count file marks
            control.mt_count = fileNo - num + 1;
        }
        else if (num == fileNo)
        {
            control.mt_op = MTBSFM; // Backward space count file marks
            control.mt_count = 1;
        }
        else // num > fileNo
        {
            control.mt_op = MTFSF; // Forward space count file marks
            control.mt_count = num - fileNo;
        }

        set_extract_state(extract, LTO_BUSY_SEEKING_STATUS, NULL, NULL, -1);

        if (ioctl(fd, MTIOCTOP, &control) == -1) {
            ml_log_error("MTIOCTOP mt_op=%s failed: %s\n", control.mt_op == MTBSFM ? "MTBSFM" : "MTFSF", strerror(errno));
            close(fd); return 0;
        }

        close(fd);
    }

    return 1;
}

static int parse_index_file_entry(const char* text, IndexFileEntry* entry)
{
    const char* textPtr = text;
    char* namePtr = &entry->name[0];
    char* maxNamePtr = &entry->name[31];
    size_t len;

    /* parse the entry number, terminated by a '\t' or ' ' */
    if (*textPtr == '\0' || sscanf(textPtr, "%d", &entry->num) != 1)
    {
        ml_log_error("Failed to parse the index entry number\n");
        return 0;
    }
    while (*textPtr != '\0' && *textPtr != '\r' && *textPtr != '\n' &&
        (*textPtr != '\t' && *textPtr != ' '))
    {
        textPtr++;
    }
    if (*textPtr != '\t' && *textPtr != ' ')
    {
        ml_log_error("Failed to skip the index entry number\n");
        return 0;
    }
    while (*textPtr == '\t' || *textPtr == ' ')
    {
        textPtr++;
    }

    /* parse the name */
    while (*textPtr != '\0' && *textPtr != '\r' && *textPtr != '\n' &&
        (*textPtr != '\t' && *textPtr != ' ') && namePtr != maxNamePtr)
    {
        *namePtr++ = *textPtr++;
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

    if (*textPtr != '\t' && *textPtr != ' ')
    {
        ml_log_error("Missing file size in index entry\n");
        return 0;
    }

    /* skip '\t', ' ' and '0' */
    while (*textPtr == '\t' || *textPtr == ' ' || *textPtr == '0')
    {
        textPtr++;
    }
    if (*textPtr == '\0' || sscanf(textPtr, "%"PRId64"", &entry->fileSize) != 1)
    {
        ml_log_error("Failed to get file size from index entry\n");
        return 0;
    }

    len = strlen(entry->name);
    if (len >= 4 && strcmp(".txt", &entry->name[len - 4]) == 0)
    {
        entry->isSelfReference = 1;
    }

    return 1;
}

static int parse_index_file(IndexFile* indexFile, char* textPtr)
{
    IndexFileEntry* entries = NULL;
    int i;
    int maxLTONumberlen = (int)sizeof(indexFile->ltoNumber) - 1;

    /* read the LTO number */
    for (i = 0; i < maxLTONumberlen; i++)
    {
        if (textPtr[i] == '\0' || textPtr[i] == '\r' || textPtr[i] == '\n')
        {
            break;
        }
        indexFile->ltoNumber[i] = textPtr[i];
    }
    if (i == maxLTONumberlen || i == 0)
    {
        ml_log_error("Failed to parse LTO spool number from index file\n");
        return 0;
    }
    indexFile->ltoNumber[i] = '\0';

    /* skip the line */
    textPtr = strchr(textPtr, '\n');
    if (textPtr == NULL)
    {
        ml_log_error("Failed to find any entries in the index file\n");
        return 0;
    }
    textPtr++;

    /* parse entries, terminated by an empty line */
    indexFile->numEntries = 0;
    while (*textPtr != '\r' && *textPtr != '\n')
    {
        /* allocate space if neccessary */
        if (indexFile->allocEntries < indexFile->numEntries + 1)
        {
            CALLOC_ORET(entries, IndexFileEntry, indexFile->allocEntries + INDEX_ENTRY_ALLOC_STEP);
            memcpy(entries, indexFile->entries, indexFile->allocEntries * sizeof(IndexFileEntry));
            SAFE_FREE(&indexFile->entries);
            indexFile->entries = entries;
            entries = NULL;
            indexFile->allocEntries += INDEX_ENTRY_ALLOC_STEP;
        }

        /* parse entry */
        indexFile->numEntries++;
        if (!parse_index_file_entry(textPtr, &indexFile->entries[indexFile->numEntries - 1]))
        {
            ml_log_error("Failed to parse index file entry %d\n", indexFile->numEntries - 1);
            goto fail;
        }

        /* move to the next line */
        textPtr = strchr(textPtr, '\n');
        if (textPtr == NULL)
        {
            break;
        }
        textPtr++;
    }

    if (indexFile->numEntries == 0)
    {
        ml_log_error("Failed to find any entries in the index file\n");
        goto fail;
    }


    return 1;

fail:
    SAFE_FREE(&entries);
    indexFile->numEntries = 0;
    return 0;
}

/* return 0 when sucessfull; -2 when tape is empty; else -1 when fails */
static int extract_index_file(QCLTOExtract* extract, unsigned char* buffer)
{
    char filename[FILENAME_MAX];
    char sysCmd[FILENAME_MAX];
    struct stat statBuf;


    /* rewind to position tape at start of the index file */
    if (!seek_to_file(extract, 0))
    {
        ml_log_error("Failed to rewind tape\n");
        return -1;
    }


    set_extract_state(extract, LTO_BUSY_EXTRACTING_INDEX_STATUS, NULL, NULL, -1);

    int fd;
    if ((fd = open(extract->tapeDevice, O_RDONLY|O_NONBLOCK|O_LARGEFILE)) == -1) {
        ml_log_error("Failed to open device %s: %s\n", extract->tapeDevice, strerror(errno));
        return -1;
    }

    // POSIX tar header is 512 bytes
    ssize_t nread = read(fd, buffer, TAPEBLOCK);
    if (nread != TAPEBLOCK)
    {
        if (nread < 0)
        {
            /* we assume that the read failed because the tape is empty */
            ml_log_error("Failed to read TAPEBLOCK for index file\n", strerror(errno));
            close(fd); return -2;
        }
        else
        {
            ml_log_error("Read %d instead of TAPEBLOCK\n", nread);
            close(fd); return -1;
        }
    }

    // tar magic at offset 257 should be "ustar" followed by space or NUL
    unsigned char *pmagic = buffer + 257;
    unsigned char magic1[6] = {'u','s','t','a','r','\0'};
    unsigned char magic2[6] = {'u','s','t','a','r',' '};
    if (memcmp(pmagic, magic1, 6) == 0 || memcmp(pmagic, magic2, 6) == 0) {
        // Does it look like an archive tape, starting with a txt file?

        size_t filenameLen = strlen((char*)buffer);
        if (filenameLen < 6 ||
            buffer[filenameLen - 6] != '0' ||
            buffer[filenameLen - 5] != '0' ||
            buffer[filenameLen - 4] != '.' ||
            tolower(buffer[filenameLen - 3]) != 't' ||
            tolower(buffer[filenameLen - 2]) != 'x' ||
            tolower(buffer[filenameLen - 1]) != 't')
        {
            close(fd); return -1;
        }
    }
    else
    {
        ml_log_error("tar magic at offset 257 is not 'ustar'\n");
        close(fd); return -1;
    }

    /* parse the tar header */
    int64_t fileSize;
    int64_t offset;
    if (!parse_tar_header(buffer, &fileSize, &offset))
    {
        ml_log_error("Failed to parse the tar header\n");
        close(fd); return -1;
    }
    if (fileSize > TAPEBLOCK - offset)
    {
        ml_log_error("Index file size %"PRId64" is larger than buffer size %"PRId64"\n", fileSize, TAPEBLOCK - offset);
        close(fd); return -1;
    }

    /* parse the index file */
    PTHREAD_MUTEX_LOCK(&extract->indexFileMutex);
    if (!parse_index_file(&extract->indexFile, (char*)&buffer[offset]))
    {
        ml_log_error("Failed to parse the index file\n");
        PTHREAD_MUTEX_UNLOCK(&extract->indexFileMutex);
        close(fd); return -1;
    }
    PTHREAD_MUTEX_UNLOCK(&extract->indexFileMutex);


    /* get the index file filename
       tar run with option '--format=posix' will by default write the name as '%d/PaxHeaders.%p/%f'
       where %d is the directory, %p is the process id and %f is the filename
       we strip out the bit before the last slash to get the name (%f) */
    char* name = strrchr((char*)buffer, '/');
    if (name == NULL)
    {
        /* not posix tar with default name format */
        name = (char*)buffer;
    }
    else
    {
        name++;
    }
    SAFE_FREE(&extract->indexFile.filename);
    CALLOC_OFAIL(extract->indexFile.filename, char, strlen(name) + 1);
    strcpy(extract->indexFile.filename, name);


    /* check if directory exists in cache, create it if not */
    strcpy(filename, extract->cacheDirectory);
    strcat_separator(filename);
    strcat(filename, extract->indexFile.ltoNumber);
    if (stat(filename, &statBuf) != 0)
    {
        strcpy(sysCmd, "mkdir ");
        strcat(sysCmd, filename);
        if (system(sysCmd) != 0)
        {
            ml_log_error("Failed to create the tape cache directory: %s\n", strerror(errno));
            close(fd); return -1;
        }
    }


    /* read the index file and write to the cache sub-directory */
    strcpy(filename, extract->cacheDirectory);
    strcat_separator(filename);
    strcat(filename, extract->indexFile.ltoNumber);
    strcat_separator(filename);
    strcat(filename, extract->indexFile.filename);

    FILE* indexFile;
    if ((indexFile = fopen(filename, "wb")) == NULL)
    {
        ml_log_error("Failed to create the tape index file: %s\n", strerror(errno));
        close(fd); return -1;
    }

    int64_t count = 0;
    int64_t numWrite;
    while (count < fileSize)
    {
        numWrite = (count + (TAPEBLOCK - offset) < fileSize) ? (TAPEBLOCK - offset) : fileSize - count;
        if (fwrite(buffer + offset, numWrite, 1, indexFile) != 1)
        {
            ml_log_error("Failed to write the tape index file: %s\n", strerror(errno));
            fclose(indexFile); close(fd); return -1;
        }
        count += numWrite;
        offset = 0;

        if (count < fileSize)
        {
            nread = read(fd, buffer, TAPEBLOCK);
            if (nread != TAPEBLOCK) {
                ml_log_error("read %d instead of TAPEBLOCK\n", nread);
                fclose(indexFile); close(fd); return -1;
            }
        }
    }
    fclose(indexFile);

    close(fd); return 0;

fail:
    close(fd); return -1;
}

/* note: extract->bytesWritten must be set to 0 before calling this function */
static int extract_file(QCLTOExtract* extract, unsigned char* buffer, int fileNum,
    int64_t fileSizeInIndex, int64_t freeDiskSpace)
{
    char filename[FILENAME_MAX];
    char sysCmd[FILENAME_MAX];
    struct stat statBuf;

    /* seek to position to position tape at start of the index file */
    if (!seek_to_file(extract, fileNum))
    {
        ml_log_error("Failed to seek to file %d\n", fileNum);
        return 0;
    }

    set_extract_state(extract, LTO_BUSY_EXTRACTING_FILE_STATUS, NULL, NULL, -1);

    /* check if directory exists in cache, create it if not */
    strcpy(filename, extract->cacheDirectory);
    strcat_separator(filename);
    strcat(filename, extract->indexFile.ltoNumber);
    if (stat(filename, &statBuf) != 0)
    {
        strcpy(sysCmd, "mkdir ");
        strcat(sysCmd, filename);
        if (system(sysCmd) != 0)
        {
            ml_log_error("Failed to create the tape cache directory: %s\n", strerror(errno));
            return 0;
        }
    }


    /* read and write to the cache sub-directory */
    int fd;
    if ((fd = open(extract->tapeDevice, O_RDONLY|O_NONBLOCK|O_LARGEFILE)) == -1) {
        ml_log_error("Failed to open device %s: %s\n", extract->tapeDevice, strerror(errno));
        return 0;
    }

    // POSIX tar header is 512 bytes
    ssize_t nread = read(fd, buffer, TAPEBLOCK);
    if (nread != TAPEBLOCK) {
        ml_log_error("read %d instead of TAPEBLOCK\n", nread);
        close(fd); return 0;
    }

    // tar magic at offset 257 should be "ustar" followed by space or NUL
    unsigned char *pmagic = buffer + 257;
    unsigned char magic1[6] = {'u','s','t','a','r','\0'};
    unsigned char magic2[6] = {'u','s','t','a','r',' '};
    if (memcmp(pmagic, magic1, 6) != 0 && memcmp(pmagic, magic2, 6) != 0) {
        ml_log_error("tar magic at offset 257 is not 'ustar'\n");
        close(fd); return 0;
    }


    /* get the filename
       tar run with option '--format=posix' will by default write the name as '%d/PaxHeaders.%p/%f'
       where %d is the directory, %p is the process id and %f is the filename
       we strip out the bit before the last slash to get the name (%f) */
    char* name = strrchr((char*)buffer, '/');
    if (name == NULL)
    {
        /* not posix tar with default name format */
        name = (char*)buffer;
    }
    else
    {
        name++;
    }
    strcpy(filename, extract->cacheDirectory);
    strcat_separator(filename);
    strcat(filename, extract->indexFile.ltoNumber);
    strcat_separator(filename);
    strcat(filename, name);

    int64_t fileSize;
    int64_t offset;
    if (!parse_tar_header(buffer, &fileSize, &offset))
    {
        ml_log_error("Failed to parse the tar header\n");
        close(fd); return 0;
    }

    if (fileSize != fileSizeInIndex)
    {
        ml_log_warn("File size on tape, %"PRId64", does not equal file size in index file, %"PRId64"\n",
            fileSize, fileSizeInIndex);

        if (freeDiskSpace >= 0 && fileSize + FREE_DISK_SPACE_MARGIN > freeDiskSpace)
        {
            ml_log_error("Disk is full: requested %"PRId64" MB, have %"PRId64" MB with %d MB margin\n",
                fileSize / (1e6), freeDiskSpace / 1e6, FREE_DISK_SPACE_MARGIN / 1e6);
            close(fd); return 0;
        }
    }

    /* check just before we open (and truncate) the file that the extract hasn't being stopped.
    This will prevent the file being truncated to zero when the user selected to stop the extract
    whilst the tape drive was still seeking and before the extract actually started */
    if (extract->stopping ||
        extract->stopExtract ||
        (extract->startExtract && !extract->extractAll))
    {
        close(fd); return 1;
    }

    FILE* file;
    if ((file = fopen(filename, "wb")) == NULL)
    {
        ml_log_error("Failed to create the tape file: %s\n", strerror(errno));
        close(fd); return 0;
    }

    struct timeval start;
    gettimeofday(&start, NULL);
    struct timeval end = start;
    long diff;
    int64_t count = 0;
    int64_t numWrite;
    while (!extract->stopping &&
        !extract->stopExtract &&
        (!extract->startExtract || extract->extractAll) &&
        count < fileSize)
    {
        numWrite = (count + (TAPEBLOCK - offset) < fileSize) ? (TAPEBLOCK - offset) : fileSize - count;
        if (fwrite(buffer + offset, numWrite, 1, file) != 1)
        {
            ml_log_error("Failed to write the tape file: %s\n", strerror(errno));
            fclose(file); close(fd); return 0;
        }
        count += numWrite;
        extract->bytesWritten += numWrite;
        offset = 0;

        if (count < fileSize)
        {
            nread = read(fd, buffer, TAPEBLOCK);
            if (nread != TAPEBLOCK) {
                ml_log_error("read %d instead of TAPEBLOCK\n", nread);
                fclose(file); close(fd); return 0;
            }
        }

        /* limit the throughput ~ 50 MB/s when a file is playing.
        The limit is set to 65 MB/s, which is practice limits the throughput to ~ 50 MB/s. The assumption
        is that the tape drive is actively limiting the throughput below the maximum.
        Note that this value was found by experiment and could be different for different LTO tape drives
        and/or RAID controllers. */
        gettimeofday(&end, NULL);
        diff = (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
        if (extract->currentPlayLTONumber != NULL && diff >= 0 && diff < 4000)
        {
            usleep(4000 - diff);
        }
        start = end;
    }
    fclose(file);

    /* return 1 if completed extract, 2 if not */
    close(fd); return 1;
}

static void* extract_thread(void* arg)
{
    QCLTOExtract* extract = (QCLTOExtract*)arg;
    struct timeval now = {0, 0};
    struct timeval endNow = {0, 0};
    struct timeval lastStatusPoll = {0, 0};
    long diffTime;
    QCLTOExtractStatus status = LTO_STARTING_STATUS;
    int state = 0;
    long delayPoll = 0;
    unsigned char blockBuffer[TAPEBLOCK];
    int startExtract;
    char startExtractSpoolNumber[32];
    char startExtractFilename[32];
    int extractAll;
    int remainderOnly;
    int lastBusyLoadPosCount = 0;
    QCLTOExtractStatus lastProbeStatus = status;
    struct statvfs statvfsBuf;
    int64_t fileSizeOnDisk;
    int64_t freeDiskSpace;
    IndexFileEntry fileForExtract;
    int fileIndex = -1;
    int skipExtract;
    int numFilesExtracted = -1;
    int fileNo;
    int blockNo;
    int result;
    int failedToReadIndexCount = 0;


    while (!extract->stopping)
    {
        gettimeofday(&now, NULL);


        diffTime = (now.tv_sec - lastStatusPoll.tv_sec) * 1000000 + now.tv_usec - lastStatusPoll.tv_usec;
        if (diffTime < 0)
        {
            diffTime = 0;
            lastStatusPoll = now;
        }
        diffTime -= delayPoll;

        switch (state)
        {
            case 0: /* poll tape device status */
                if (diffTime > 1000000) /* poll status every second - note below with lastBusyLoadPosCount that we assume 1 second poll interval */
                {
                    status = probe_tape_status(extract->tapeDevice);

                    if (status == LTO_ONLINE_STATUS)
                    {
                        if (failedToReadIndexCount < 3)
                        {
                            state = 1;
	                    set_extract_state(extract, status, NULL, NULL, -1);
                        }
                        /* else we wait until a new tape has been inserted (ie. status changes to !online) */
                    }
                    else
                    {
                        failedToReadIndexCount = 0;

                        if (status == LTO_BUSY_LOAD_POS_STATUS &&
                            lastProbeStatus == status)
                        {
                            /* if the tape has been in LTO_BUSY_LOAD_POS_STATUS for more than 5 seconds
                            then we assume the PC was started with a tape in the tape drive
                            and we assume ONLINE status */
                            if (lastBusyLoadPosCount > 5)
                            {
                                status = LTO_ONLINE_STATUS;
                                state = 1;
                            }
                        }
                        set_extract_state(extract, status, NULL, NULL, -1);
                    }


                    if (status == LTO_BUSY_LOAD_POS_STATUS)
                    {
                        lastBusyLoadPosCount++;
                    }
                    else
                    {
                        lastBusyLoadPosCount = 0;
                    }
                    delayPoll = 0;
                    lastStatusPoll = now;
                    lastProbeStatus = status;
                }
                break;

            case 1: /* tape device is online */

                /* set tape params */
                set_tape_params(extract->tapeDevice);


                /* extract index file */

                result = extract_index_file(extract, blockBuffer);
                if (result != 0)
                {
                    if (result == -2)
                    {
                        set_extract_state(extract, LTO_TAPE_EMPTY_OR_IO_ERROR_STATUS, NULL, NULL, -1);
                        ml_log_error("Failed to extract index file from tape - tape is empty or io error occurred\n");
                        printf("Failed to extract index file from tape - tape is empty or io error occurred\n");
                    }
                    else
                    {
                        set_extract_state(extract, LTO_INDEX_EXTRACT_FAILED_STATUS, NULL, NULL, -1);
                        ml_log_error("Failed to extract index file from tape\n");
                        printf("Failed to extract index file from tape\n");
                    }

                    failedToReadIndexCount++;
                    state = 0;
                    delayPoll = 5000000; /* wait 5 seconds before we poll again */
                    goto breakout;
                }
                ml_log_info("Parsed %d entries from the index file\n", extract->indexFile.numEntries);
                failedToReadIndexCount = 0;

                set_extract_state(extract, LTO_READY_TO_EXTRACT_STATUS, extract->indexFile.ltoNumber, "", 0);

                state = 2;

                break;

            case 2:

                PTHREAD_MUTEX_LOCK(&extract->stateMutex);
                startExtract = extract->startExtract;
                extractAll = extract->extractAll;
                remainderOnly = extract->remainderOnly;
                if (startExtract)
                {
                    strcpy(startExtractSpoolNumber, extract->startLTOSpoolNumber);
                    strcpy(startExtractFilename, extract->startFilename);
                }
                extract->startExtract = 0; /* reset so we see the next start request */
                extract->extractAll = 0;
                extract->remainderOnly = 0;
                extract->stopExtract = 0; /* can't stop before we have started */
                PTHREAD_MUTEX_UNLOCK(&extract->stateMutex);

                if (startExtract)
                {
                    /* inits for extract all */
                    if (extractAll)
                    {
                        /* start with the next file */
                        if (get_tape_pos(extract->tapeDevice, &fileNo, &blockNo))
                        {
                            if (blockNo == 0)
                            {
                                /* we are at the start of the file */
                                fileIndex = fileNo;
                            }
                            else
                            {
                                /* go to the next file */
                                fileIndex = fileNo + 1;
                            }
                        }
                        else
                        {
                            /* don't know where we are on the tape so we go to the start */
                            fileIndex = 0;
                        }
                        numFilesExtracted = 0;
                    }

                    while (!extract->stopping &&
                        !extract->stopExtract &&
                        !extract->startExtract)
                    {
                        if (extractAll)
                        {
                            PTHREAD_MUTEX_LOCK(&extract->indexFileMutex);
                            if (numFilesExtracted >= extract->indexFile.numEntries)
                            {
                                /* completed extract all */
                                PTHREAD_MUTEX_UNLOCK(&extract->indexFileMutex);
                                set_extract_state(extract, LTO_READY_TO_EXTRACT_STATUS, extract->indexFile.ltoNumber, "", 0);
                                break;
                            }
                            if (fileIndex >= extract->indexFile.numEntries)
                            {
                                fileIndex = 0;
                            }
                            fileForExtract = extract->indexFile.entries[fileIndex];
                            PTHREAD_MUTEX_UNLOCK(&extract->indexFileMutex);

                            if (fileForExtract.isSelfReference)
                            {
                                /* skip the index file itself */
                                fileIndex++;
                                numFilesExtracted++;
                                continue;
                            }

                            if (remainderOnly)
                            {
                                fileSizeOnDisk = get_file_size_on_disk(extract->cacheDirectory, extract->indexFile.ltoNumber, fileForExtract.name);
                                if (fileSizeOnDisk == fileForExtract.fileSize)
                                {
                                    ml_log_info("Skipping '%s/%s' for extract because it is complete\n", extract->indexFile.ltoNumber, fileForExtract.name);
                                    printf("Skipping '%s/%s' for extract because it is complete\n", extract->indexFile.ltoNumber, fileForExtract.name);

                                    /* skip this file which is already on disk */
                                    fileIndex++;
                                    numFilesExtracted++;
                                    continue;
                                }
                            }
                        }
                        else /* !extractAll */
                        {
                            if (!get_file_from_index(extract, startExtractSpoolNumber, startExtractFilename, &fileForExtract))
                            {
                                ml_log_error("File selected to extract is unknown\n");

                                set_extract_state(extract, LTO_FILE_EXTRACT_FAILED_STATUS, "", "", 0);

                                state = 0;
                                delayPoll = 5000000; /* wait 5 seconds before we poll again */
                                goto breakout;
                            }
                        }

                        /* check that we have enough disk space */
                        freeDiskSpace = -1;
                        if (fileForExtract.fileSize >= 0)
                        {
                            if (statvfs(extract->cacheDirectory, &statvfsBuf) == 0)
                            {
                                freeDiskSpace = statvfsBuf.f_bfree * statvfsBuf.f_bsize;
                                if (fileForExtract.fileSize + FREE_DISK_SPACE_MARGIN > freeDiskSpace)
                                {
                                    ml_log_error("Disk is full: requested %"PRId64" MB, have %"PRId64" MB with %d MB margin\n",
                                        fileForExtract.fileSize / (1e6), freeDiskSpace / 1e6, FREE_DISK_SPACE_MARGIN / 1e6);

                                    set_extract_state(extract, LTO_DISK_FULL_STATUS, NULL, "", 0);

                                    delayPoll = 20000000; /* wait 20 seconds before we poll again */
                                    goto breakout;
                                }
                            }
                        }

                        /* don't extract to the file that is currently playing */
                        skipExtract = 0;
                        PTHREAD_MUTEX_LOCK(&extract->stateMutex);
                        if (extract->currentPlayLTONumber != NULL &&
                            extract->currentPlayName != NULL &&
                            strcmp(extract->indexFile.ltoNumber, extract->currentPlayLTONumber) == 0 &&
                            strcmp(fileForExtract.name, extract->currentPlayName) == 0)
                        {
                            skipExtract = 1;
                        }
                        PTHREAD_MUTEX_UNLOCK(&extract->stateMutex);

                        if (!skipExtract)
                        {
                            extract->bytesWritten = 0; /* used in qce_can_play() function */
                            set_extract_state(extract, LTO_BUSY_EXTRACTING_FILE_STATUS, extract->indexFile.ltoNumber,
                                fileForExtract.name, extractAll);

                            ml_log_info("Extracting file '%s/%s' (%s)\n", extract->indexFile.ltoNumber, fileForExtract.name,
                                extractAll ? "extract all" : "single extract");
                            printf("Extracting file '%s/%s' (%s)\n", extract->indexFile.ltoNumber, fileForExtract.name,
                                extractAll ? "extract all" : "single extract");

                            /* start extracting */
                            if (!extract_file(extract, blockBuffer, fileForExtract.num, fileForExtract.fileSize, freeDiskSpace))
                            {
                                ml_log_error("Failed to extract file from tape\n");
                                printf("Failed to extract file from tape\n");

                                set_extract_state(extract, LTO_FILE_EXTRACT_FAILED_STATUS, "", "", 0);

                                state = 0;
                                delayPoll = 5000000; /* wait 5 seconds before we poll again */
                                goto breakout;
                            }
                            ml_log_info("Ended extract from tape\n");
                            printf("Ended extract from tape\n");
                        }
                        else
                        {
                            ml_log_info("Skipping extract of '%s/%s' (%s) which is currently playing\n", extract->indexFile.ltoNumber,
                                fileForExtract.name, extractAll ? "extract all" : "single extract");
                            printf("Skipping extract of '%s/%s' (%s) which is currently playing\n", extract->indexFile.ltoNumber,
                                fileForExtract.name, extractAll ? "extract all" : "single extract");
                        }

                        if (!extractAll)
                        {
                            break;
                        }
                        else
                        {
                            set_extract_state(extract, LTO_READY_TO_EXTRACT_STATUS, extract->indexFile.ltoNumber, "", extractAll);
                            fileIndex++;
                            numFilesExtracted++;
                        }
                    }

		    set_extract_state(extract, LTO_READY_TO_EXTRACT_STATUS, extract->indexFile.ltoNumber, "", 0);
                }
                else /* !startExtract */
                {
                    if (diffTime > 1000000) /* poll status every second */
                    {
                        status = probe_tape_status(extract->tapeDevice);

                        if (status != LTO_ONLINE_STATUS)
                        {
                            set_extract_state(extract, status, "", "", 0);
                            state = 0;
                        }
                        else if (extract->state.status == LTO_DISK_FULL_STATUS)
                        {
                            set_extract_state(extract, LTO_READY_TO_EXTRACT_STATUS, NULL, NULL, -1);
                        }

                        delayPoll = 0;
                        lastStatusPoll = now;
                    }
                }

                break;

            default:
                break;
        }

breakout:

        gettimeofday(&endNow, NULL);
        diffTime = (endNow.tv_sec - now.tv_sec) * 1000000 + endNow.tv_usec - now.tv_usec;
        if (diffTime < 100000)
        {
            /* don't hog the CPU */
            usleep(100000);
        }
    }

    pthread_exit((void*) 0);
}


int qce_create_lto_extract(const char* cacheDirectory, const char* tapeDevice, QCLTOExtract** extract)
{
    QCLTOExtract* newExtract = NULL;

    CALLOC_ORET(newExtract, QCLTOExtract, 1);

    CALLOC_OFAIL(newExtract->cacheDirectory, char, strlen(cacheDirectory) + 1);
    strcpy(newExtract->cacheDirectory, cacheDirectory);

    CALLOC_OFAIL(newExtract->tapeDevice, char, strlen(tapeDevice) + 1);
    strcpy(newExtract->tapeDevice, tapeDevice);

    CHK_OFAIL(init_mutex(&newExtract->stateMutex));
    CHK_OFAIL(init_mutex(&newExtract->indexFileMutex));

    CHK_OFAIL(create_joinable_thread(&newExtract->extractThreadId, extract_thread, newExtract));


    *extract = newExtract;
    return 1;

fail:
    qce_free_lto_extract(&newExtract);
    return 0;
}

void qce_free_lto_extract(QCLTOExtract** extract)
{
    if (*extract == NULL)
    {
        return;
    }

    (*extract)->stopping = 1;
    join_thread(&(*extract)->extractThreadId, NULL, NULL);

    SAFE_FREE(&(*extract)->cacheDirectory);
    SAFE_FREE(&(*extract)->tapeDevice);

    SAFE_FREE(&(*extract)->indexFile.entries);
    SAFE_FREE(&(*extract)->indexFile.filename);

    SAFE_FREE(&(*extract)->currentPlayLTONumber);
    SAFE_FREE(&(*extract)->currentPlayName);

    destroy_mutex(&(*extract)->stateMutex);
    destroy_mutex(&(*extract)->indexFileMutex);

    SAFE_FREE(extract);
}

void qce_start_extract(QCLTOExtract* extract, const char* ltoSpoolNumber, const char* filename)
{
    PTHREAD_MUTEX_LOCK(&extract->stateMutex);

    extract->startExtract = 1;
    extract->extractAll = 0;
    extract->remainderOnly = 0;

    strncpy(extract->startLTOSpoolNumber, ltoSpoolNumber, 31);
    extract->startLTOSpoolNumber[31] = '\0';

    strncpy(extract->startFilename, filename, 31);
    extract->startFilename[31] = '\0';

    PTHREAD_MUTEX_UNLOCK(&extract->stateMutex);
}

void qce_start_extract_all(QCLTOExtract* extract, const char* ltoSpoolNumber)
{
    PTHREAD_MUTEX_LOCK(&extract->stateMutex);

    extract->startExtract = 1;
    extract->extractAll = 1;
    extract->remainderOnly = 0;

    strncpy(extract->startLTOSpoolNumber, ltoSpoolNumber, 31);
    extract->startLTOSpoolNumber[31] = '\0';

    strcpy(extract->startFilename, "");

    PTHREAD_MUTEX_UNLOCK(&extract->stateMutex);
}

void qce_start_extract_remainder(QCLTOExtract* extract, const char* ltoSpoolNumber)
{
    PTHREAD_MUTEX_LOCK(&extract->stateMutex);

    extract->startExtract = 1;
    extract->extractAll = 1;
    extract->remainderOnly = 1;

    strncpy(extract->startLTOSpoolNumber, ltoSpoolNumber, 31);
    extract->startLTOSpoolNumber[31] = '\0';

    strcpy(extract->startFilename, "");

    PTHREAD_MUTEX_UNLOCK(&extract->stateMutex);
}

void qce_stop_extract(QCLTOExtract* extract)
{
    PTHREAD_MUTEX_LOCK(&extract->stateMutex);
    extract->stopExtract = 1;
    PTHREAD_MUTEX_UNLOCK(&extract->stateMutex);
}

void qce_get_lto_state(QCLTOExtract* extract, QCLTOExtractState* state, int updateMask)
{
    PTHREAD_MUTEX_LOCK(&extract->stateMutex);
    *state = extract->state;
    extract->state.updated &= ~updateMask;
    PTHREAD_MUTEX_UNLOCK(&extract->stateMutex);
}

int qce_is_current_lto(QCLTOExtract* extract, const char* ltoNumber)
{
    int result;

    PTHREAD_MUTEX_LOCK(&extract->stateMutex);
    result = (strcmp(ltoNumber, extract->state.ltoSpoolNumber) == 0);
    PTHREAD_MUTEX_UNLOCK(&extract->stateMutex);

    return result;
}

int qce_is_extracting(QCLTOExtract* extract, const char* ltoNumber, const char* filename)
{
    int result;

    PTHREAD_MUTEX_LOCK(&extract->stateMutex);
    result = (strcmp(ltoNumber, extract->state.ltoSpoolNumber) == 0) &&
        (strcmp(filename, extract->state.currentExtractingFile) == 0);
    PTHREAD_MUTEX_UNLOCK(&extract->stateMutex);

    return result;
}

int qce_is_extracting_from(QCLTOExtract* extract, const char* ltoNumber)
{
    int result;

    PTHREAD_MUTEX_LOCK(&extract->stateMutex);
    result = (strcmp(ltoNumber, extract->state.ltoSpoolNumber) == 0) &&
        (strlen(extract->state.currentExtractingFile) > 0);
    PTHREAD_MUTEX_UNLOCK(&extract->stateMutex);

    return result;
}

int qce_is_extracting_all_from(QCLTOExtract* extract, const char* ltoNumber)
{
    int result;

    PTHREAD_MUTEX_LOCK(&extract->stateMutex);
    result = (strcmp(ltoNumber, extract->state.ltoSpoolNumber) == 0) &&
        (strlen(extract->state.currentExtractingFile) > 0) &&
        extract->state.extractAll;
    PTHREAD_MUTEX_UNLOCK(&extract->stateMutex);

    return result;
}

void qce_set_current_play_name(QCLTOExtract* extract, const char* directory, const char* name)
{
    const char* ltoNumber;

    PTHREAD_MUTEX_LOCK(&extract->stateMutex);

    SAFE_FREE(&extract->currentPlayLTONumber);
    SAFE_FREE(&extract->currentPlayName);
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
        CALLOC_OFAIL(extract->currentPlayLTONumber, char, strlen(ltoNumber) + 1);
        strcpy(extract->currentPlayLTONumber, ltoNumber);
    }
    if (name != NULL && name[0] != '\0')
    {
        CALLOC_OFAIL(extract->currentPlayName, char, strlen(name) + 1);
        strcpy(extract->currentPlayName, name);
    }

    PTHREAD_MUTEX_UNLOCK(&extract->stateMutex);
    return;

fail:
    PTHREAD_MUTEX_UNLOCK(&extract->stateMutex);
}

int qce_can_play(QCLTOExtract* extract, const char* directory, const char* name)
{
    const char* ltoNumber = NULL;
    int result = 0;

    PTHREAD_MUTEX_LOCK(&extract->stateMutex);

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
    }
    /* can play the file if it is not currently being extracted or at least 5 mega-bytes of data
    have already been written to the file on disk */
    result = ltoNumber == NULL ||
        strcmp(ltoNumber, extract->state.ltoSpoolNumber) != 0 ||
        strcmp(name, extract->state.currentExtractingFile) != 0 ||
        extract->bytesWritten > 5000000;

    PTHREAD_MUTEX_UNLOCK(&extract->stateMutex);

    return result;
}

int qce_is_seeking(QCLTOExtract* extract)
{
    return extract->state.status == LTO_BUSY_REWINDING_STATUS ||
        extract->state.status == LTO_BUSY_SEEKING_STATUS;
}

