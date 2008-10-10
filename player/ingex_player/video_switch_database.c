#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>


#include "video_switch_database.h"
#include "logging.h"
#include "macros.h"

#define SOURCE_INDEX_OFFSET     0
#define SOURCE_ID_OFFSET        2
#define DATE_OFFSET             35
#define TIMECODE_OFFSET         46

#define SOURCE_INDEX_LEN        1
#define SOURCE_ID_LEN           32
#define DATE_LEN                10
#define TIMECODE_LEN            12



struct VideoSwitchDatabase
{
    FILE* file;
};


static void get_date(int* year, int* month, int* day)
{
    struct tm gmt;
    struct timeval tv;

    memset(&gmt, 0, sizeof(gmt));
    
    *year = 0;
    *month = 0;
    *day = 0;
    

    gettimeofday(&tv, NULL);
    gmtime_r(&tv.tv_sec, &gmt);
    
    *year = gmt.tm_year + 1900;
    *month = gmt.tm_mon + 1;
    *day = gmt.tm_mday;
}

static int open(FILE* file, VideoSwitchDatabase** db)
{
    VideoSwitchDatabase* newDb = NULL;
    
    CALLOC_OFAIL(newDb, VideoSwitchDatabase, 1);
    newDb->file = file;
    file = NULL;
    
    *db = newDb;
    return 1;
    
fail:
    if (file != NULL)
    {
        fclose(file);
    }
    vsd_close(&newDb);
    return 0;
}


int vsd_open_write(const char* filename, VideoSwitchDatabase** db)
{
    FILE* file = fopen(filename, "wb");
    if (file == NULL)
    {
        ml_log_error("Failed to open video switch database file '%s': %s\n", filename, strerror(errno));
        return 0;
    }
    
    return open(file, db);
}

int vsd_open_append(const char* filename, VideoSwitchDatabase** db)
{
    FILE* file = fopen(filename, "ab");
    if (file == NULL)
    {
        ml_log_error("Failed to open video switch database file '%s': %s\n", filename, strerror(errno));
        return 0;
    }
    
    return open(file, db);
}

int vsd_open_read(const char* filename, VideoSwitchDatabase** db)
{
    FILE* file = fopen(filename, "rb");
    if (file == NULL)
    {
        ml_log_error("Failed to open video switch database file '%s': %s\n", filename, strerror(errno));
        return 0;
    }
    
    return open(file, db);
}

void vsd_close(VideoSwitchDatabase** db)
{
    if (*db == NULL)
    {
        return;
    }
    
    if ((*db)->file != NULL)
    {
        fclose((*db)->file);
    }
    
    SAFE_FREE(db);
}


int vsd_append_entry(VideoSwitchDatabase* db, int sourceIndex, const char* sourceId, const Timecode* tc)
{
    if (fseeko(db->file, 0, SEEK_END) != 0)
    {
        ml_log_error("Failed to seek to end of video switch database file: %s\n", strerror(errno));
        return 0;
    }
    
    char buf[VIDEO_SWITCH_DB_ENTRY_SIZE];
    memset(buf, 0, VIDEO_SWITCH_DB_ENTRY_SIZE);
    
    /* source index - 1 byte */
    buf[SOURCE_INDEX_OFFSET] = (unsigned char)(sourceIndex);
    
    /* source id - string plus null terminator */
    strncpy(&buf[SOURCE_ID_OFFSET], sourceId, SOURCE_ID_LEN - 1);
    
    /* date */
    int year, month, day;
    get_date(&year, &month, &day);
    snprintf(&buf[DATE_OFFSET], DATE_LEN + 1, "%04d-%02d-%02d", 
        (year > 9999) ? 0 : year,
        (month > 12) ? 0 : month,
        (day > 31) ? 0 : day);
    
    /* timecode - text timecode plus drop frame indicator */
    snprintf(&buf[TIMECODE_OFFSET], TIMECODE_LEN + 1, "%02d:%02d:%02d:%02d%c", 
        (tc->hour > 24) ? 0 : tc->hour,
        (tc->min > 59) ? 0 : tc->min,
        (tc->sec > 59) ? 0 : tc->sec,
        (tc->frame > 30) ? 0 : tc->frame,
        tc->isDropFrame ? 'D' : 'N');
        
        
    if (fwrite(buf, VIDEO_SWITCH_DB_ENTRY_SIZE, 1, db->file) != 1)
    {
        ml_log_error("Failed to write video switch database entry: %s\n", strerror(errno));
        return 0;
    }
    fflush(db->file);
    
    return 1;
}


int vsd_get_num_entries(VideoSwitchDatabase* db)
{
    struct stat st;
    
    if (fstat(fileno(db->file), &st) != 0)
    {
        ml_log_error("Failed to stat video switch database file: %s\n", strerror(errno));
        return 0;
    }
    
    return st.st_size / VIDEO_SWITCH_DB_ENTRY_SIZE;
}

int vsd_load(VideoSwitchDatabase* db, int startEntry, unsigned char* buffer, int bufferSize, int* numEntries)
{
    int totalEntries = vsd_get_num_entries(db);
    if (totalEntries == 0)
    {
        *numEntries = 0;
        return 1;
    }
    
    int readEntries = totalEntries - startEntry;
    readEntries = (bufferSize / VIDEO_SWITCH_DB_ENTRY_SIZE < readEntries) ? 
        bufferSize / VIDEO_SWITCH_DB_ENTRY_SIZE : readEntries;
    if (readEntries <= 0)
    {
        *numEntries = 0;
        return 1;
    }
    
    if (fseeko(db->file, startEntry * VIDEO_SWITCH_DB_ENTRY_SIZE, SEEK_SET) != 0)
    {
        ml_log_error("Failed to seek to position in video switch database file: %s\n", strerror(errno));
        return 0;
    }
    
    *numEntries = fread(buffer, VIDEO_SWITCH_DB_ENTRY_SIZE, readEntries, db->file);
    return 1;
}

int vsd_parse_entry(const unsigned char* entryBuffer, int entryBufferSize, VideoSwitchDbEntry* entry)
{
    int hour, min, sec, frame;
    char isDropFrame;
    int year, month, day;

    if (entryBufferSize < VIDEO_SWITCH_DB_ENTRY_SIZE)
    {
        ml_log_error("Entry buffer size %d is smaller than the entry size %d\n", entryBufferSize, VIDEO_SWITCH_DB_ENTRY_SIZE);
        return 0;
    }
    
    if (sscanf((const char*)&entryBuffer[DATE_OFFSET], "%04d-%02d-%02d", &year, &month, &day) != 3)
    {
        ml_log_error("Failed to parse date in video switch database entry\n");
        return 0;
    }
    
    if (sscanf((const char*)&entryBuffer[TIMECODE_OFFSET], "%02d:%02d:%02d:%02d%c", &hour, &min, &sec, &frame, &isDropFrame) != 5)
    {
        ml_log_error("Failed to parse timecode in video switch database entry\n");
        return 0;
    }
    
    entry->sourceIndex = (int)entryBuffer[SOURCE_INDEX_OFFSET];
    entry->sourceId = (const char*)(&entryBuffer[SOURCE_ID_OFFSET]);
    entry->year = year;
    entry->month = month;
    entry->day = day;
    entry->tc.hour = hour;
    entry->tc.min = min;
    entry->tc.sec = sec;
    entry->tc.frame = frame;
    entry->tc.isDropFrame = (isDropFrame == 'D');
    
    return 1;
}

int vsd_dump(const char* filename, FILE* output)
{
    VideoSwitchDatabase* db;
    unsigned char buf[VIDEO_SWITCH_DB_ENTRY_SIZE * 50];
    int numEntries = 0;
    int startEntry = 0;
    
    if (!vsd_open_read(filename, &db))
    {
        return 0;
    }
    
    
    do
    {
        if (!vsd_load(db, startEntry, buf, VIDEO_SWITCH_DB_ENTRY_SIZE * 50, &numEntries))
        {
            ml_log_error("Failed to load entries from video switch database\n");
            goto fail;
        }
        
        int i;
        for (i = 0; i < numEntries; i++)
        {
            VideoSwitchDbEntry entry;
            if (!vsd_parse_entry(&buf[VIDEO_SWITCH_DB_ENTRY_SIZE * i], 
                VIDEO_SWITCH_DB_ENTRY_SIZE * (50 - i), &entry))
            {
                ml_log_error("Failed to parse video switch database entry %d\n", i);
                goto fail;
            }
            
            printf("%d, %s, %04d-%02d-%02d, %02d:%02d:%02d:%02d%c\n",
                entry.sourceIndex, entry.sourceId, entry.year, entry.month, entry.day,
                entry.tc.hour, entry.tc.min, entry.tc.sec, entry.tc.frame, entry.tc.isDropFrame ? 'D' : ' '); 
        }
        
        startEntry += numEntries;
        
    } while (numEntries > 0);

    vsd_close(&db);
    return 1;
    
fail:    
    vsd_close(&db);
    return 0;
}


