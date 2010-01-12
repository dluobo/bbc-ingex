/*
 * $Id: qc_session.c,v 1.8 2010/01/12 16:32:33 john_f Exp $
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>

#include "qc_session.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"


#define QC_LOG_LINE_ELEMENTS        9


struct QCSession
{
    char* sessionName;
    char* ltoDir;
    FILE* sessionFile;
    MediaPlayerListener playerListener;
    FrameInfo lastFrameInfo;
    int haveFirstFrame;
    int haveEndOfSource;
    char sessionComments[MAX_SESSION_COMMENTS_SIZE];

    int64_t controlTCStartPos;
};

typedef char LogLine[QC_LOG_LINE_ELEMENTS][64];

static const char* g_qcSessionPreSuf = "_qcsession_";


static const char* strip_path(const char* filePath)
{
    const char* name = strrchr(filePath, '/');
    if (name == NULL)
    {
        return filePath;
    }

    return name + 1;
}

static void safe_fprintf(FILE* file, const char* str, char* buffer, int bufferSize)
{
    strncpy(buffer, str, bufferSize);
    buffer[bufferSize - 1] = '\0';

    char* bufferPtr = buffer;
    while (*bufferPtr != '\0')
    {
        if (!isprint(*bufferPtr) ||
            *bufferPtr == ',' ||
            *bufferPtr == '\n' || *bufferPtr == '\r')
        {
            *bufferPtr = ' ';
        }
        bufferPtr++;
    }

    fprintf(file, "%s", buffer);
}

static char* get_mark_name(const MarkConfigs* markConfigs, int typeBit, char name[32])
{
    int i;
    int bitMask;

    name[0] = '\0';
    name[31] = '\0';

    if (typeBit >= 1 && typeBit <= 32)
    {
        bitMask = 0x1 << (typeBit - 1);

        /* try find a config */
        for (i = 0; i < markConfigs->numConfigs; i++)
        {
            if (markConfigs->configs[i].type & bitMask)
            {
                strncpy(name, markConfigs->configs[i].name, 31);
                break;
            }
        }

        /* if no config, then use generic naming scheme */
        if (i == markConfigs->numConfigs)
        {
            snprintf(name, 32, "M%d", typeBit - 1);
        }
    }

    return name;
}

static void init_log_line(LogLine logLine)
{
    memset(logLine, 0, sizeof(LogLine));
}

static void write_log_line_element(LogLine logLine, int elementNum, const char* format, ...)
{
    va_list p_arg;

    if (format == NULL)
    {
        return;
    }

    va_start(p_arg, format);
    vsprintf(logLine[elementNum], format, p_arg);
    va_end(p_arg);
}

static void write_comment(QCSession* qcSession, const char* format, ...)
{
    va_list p_arg;
    int i;

    if (format == NULL)
    {
        return;
    }

    fprintf(qcSession->sessionFile, "# ");
    va_start(p_arg, format);
    vfprintf(qcSession->sessionFile, format, p_arg);
    va_end(p_arg);

    for (i = 0; i < QC_LOG_LINE_ELEMENTS - 1; i++)
    {
        fprintf(qcSession->sessionFile, ", ");
    }
    fprintf(qcSession->sessionFile, "\n");
}

static void write_comment_start(QCSession* qcSession)
{
    fprintf(qcSession->sessionFile, "# ");
}

static void write_comment_middle(QCSession* qcSession, const char* format, ...)
{
    va_list p_arg;

    if (format == NULL)
    {
        return;
    }

    va_start(p_arg, format);
    vfprintf(qcSession->sessionFile, format, p_arg);
    va_end(p_arg);
}

static void write_comment_end(QCSession* qcSession)
{
    int i;

    for (i = 0; i < QC_LOG_LINE_ELEMENTS - 1; i++)
    {
        fprintf(qcSession->sessionFile, ", ");
    }
    fprintf(qcSession->sessionFile, "\n");
}

static void qcs_write_timecodes(LogLine logLine, const FrameInfo* frameInfo)
{
    int i;
    int haveControlTC = 0;
    int haveVITC = 0;
    int haveLTC = 0;
    char* logLineStr;

    for (i = 0; i < frameInfo->numTimecodes; i++)
    {
        if (!haveControlTC && frameInfo->timecodes[i].timecodeType == CONTROL_TIMECODE_TYPE)
        {
            logLineStr = logLine[QC_LOG_LINE_ELEMENTS - 3];
            haveControlTC = 1;
        }
        else if (!haveVITC && frameInfo->timecodes[i].timecodeType == SOURCE_TIMECODE_TYPE &&
            frameInfo->timecodes[i].timecodeSubType == VITC_SOURCE_TIMECODE_SUBTYPE)
        {
            logLineStr = logLine[QC_LOG_LINE_ELEMENTS - 2];
            haveVITC = 1;
        }
        else if (!haveLTC && frameInfo->timecodes[i].timecodeType == SOURCE_TIMECODE_TYPE &&
            frameInfo->timecodes[i].timecodeSubType == LTC_SOURCE_TIMECODE_SUBTYPE)
        {
            logLineStr = logLine[QC_LOG_LINE_ELEMENTS - 1];
            haveLTC = 1;
        }
        else
        {
            continue;
        }

        sprintf(logLineStr,
            "%02u:%02u:%02u:%02u",
            frameInfo->timecodes[i].timecode.hour,
            frameInfo->timecodes[i].timecode.min,
            frameInfo->timecodes[i].timecode.sec,
            frameInfo->timecodes[i].timecode.frame);
    }
}

static void write_log_line(QCSession* qcSession, LogLine logLine, const FrameInfo* frameInfo)
{
    int i;

    if (frameInfo != NULL)
    {
        qcs_write_timecodes(logLine, frameInfo);
    }

    for (i = 0; i < QC_LOG_LINE_ELEMENTS; i++)
    {
        if (i != 0)
        {
            fprintf(qcSession->sessionFile, ", ");
        }
        fprintf(qcSession->sessionFile, "%s", logLine[i]);
    }
    fprintf(qcSession->sessionFile, "\n");
}

static int parse_mark_type(const char* data)
{
    /* hex value starts after '0x' and ends at space or ',' */
    int startHex = 0;
    int type = 0;
    while (*data != '\0')
    {
        if (*data == ',')
        {
            break;
        }
        else if (startHex)
        {
            if (*data >= '0' && *data <= '9')
            {
                type <<= 4;
                type |= *data - '0';
            }
            else if (*data >= 'a' && *data <= 'f')
            {
                type <<= 4;
                type |= 0x0a + *data - 'a';
            }
            else if (*data >= 'A' && *data <= 'F')
            {
                type <<= 4;
                type |= 0x0a + *data - 'A';
            }
            else if (*data == ' ')
            {
                break;
            }
            else
            {
                /* parse error */
                startHex = 0;
                break;
            }
        }
        else if (*data == 'X' || *data == 'x')
        {
            startHex = 1;
        }
        data++;
    }

    return type;
}

static void qcs_frame_displayed_event(void* data, const FrameInfo* frameInfo)
{
    QCSession* qcSession = (QCSession*)data;
    LogLine logLine;
    int i;
    int roundedFrameRate = get_rounded_frame_rate(&frameInfo->frameRate);

    if (!qcSession->haveFirstFrame)
    {
        init_log_line(logLine);

        write_log_line_element(logLine, 1, "First frame displayed");
        write_log_line(qcSession, logLine, frameInfo);

        /* get the control timecode position */
        qcSession->controlTCStartPos = 0;
        for (i = 0; i < frameInfo->numTimecodes; i++)
        {
            if (frameInfo->timecodes[i].timecodeType == CONTROL_TIMECODE_TYPE)
            {
                qcSession->controlTCStartPos = frameInfo->timecodes[i].timecode.hour * 60 * 60 * roundedFrameRate +
                    frameInfo->timecodes[i].timecode.min * 60 * roundedFrameRate +
                    frameInfo->timecodes[i].timecode.sec * roundedFrameRate +
                    frameInfo->timecodes[i].timecode.frame;
                if (qcSession->controlTCStartPos < frameInfo->position)
                {
                    ml_log_warn("QC session found negative control timecode start position - ignoring and assuming 0 start position\n");
                    qcSession->controlTCStartPos = 0;
                }
                else
                {
                    qcSession->controlTCStartPos -= frameInfo->position;
                }
                break;
            }
        }

        qcSession->haveFirstFrame = 1;
    }
    qcSession->lastFrameInfo = *frameInfo;
}

static void qcs_frame_dropped_event(void* data, const FrameInfo* displayedFrameInfo)
{
//    QCSession* qcSession = (QCSession*)data;
}

static void qcs_state_change_event(void* data, const MediaPlayerStateEvent* event)
{
    QCSession* qcSession = (QCSession*)data;
    LogLine logLine;
    int elementNum;

    init_log_line(logLine);
    elementNum = 2;

    if (event->lockedChanged)
    {
        if (event->locked)
        {
            write_log_line_element(logLine, elementNum, "Lock");
        }
        else
        {
            write_log_line_element(logLine, elementNum, "Unlock");
        }
    }
    elementNum++;

    if (event->playChanged)
    {
        if (event->play)
        {
            write_log_line_element(logLine, elementNum, "Play");
        }
        else
        {
            write_log_line_element(logLine, elementNum, "Pause");
        }
    }
    elementNum++;

    if (event->stopChanged)
    {
        if (event->stop)
        {
            write_log_line_element(logLine, elementNum, "Stop");
        }
        else
        {
            write_log_line_element(logLine, elementNum, "Start");
        }
    }
    elementNum++;

    if (event->speedChanged)
    {
        write_log_line_element(logLine, elementNum, "%dx", event->speed);
    }
    elementNum++;


    write_log_line(qcSession, logLine, &event->displayedFrameInfo);
}

static void qcs_player_closed(void* data)
{
    QCSession* qcSession = (QCSession*)data;
    LogLine logLine;

    if (data == NULL)
    {
        return;
    }

    if (qcSession->haveFirstFrame)
    {
        init_log_line(logLine);

        write_log_line_element(logLine, 1, "Last frame displayed");
        write_log_line(qcSession, logLine, &qcSession->lastFrameInfo);
    }
}


int qcs_open(const char* mxfFilename, MediaSource* source, int argc, const char** argv,
    const char* name, const char* loadedSessionFilename, const char* userName, const char* hostName,
    QCSession** qcSession)
{
    QCSession* newQCSession;
    char timestampStr[MAX_TIMESTAMP_STRING_SIZE];
    int i, j;
    LogLine logLine;
    char filename[FILENAME_MAX];
    char* lastSep;
    const StreamInfo* streamInfo;
    char metadataBuffer[64];


    CALLOC_ORET(newQCSession, QCSession, 1);

    get_short_timestamp_string(timestampStr);
    snprintf(filename, FILENAME_MAX, "%s%s%s.txt", mxfFilename, g_qcSessionPreSuf, timestampStr);
    if ((lastSep = strrchr(filename, '/')) == NULL)
    {
        CALLOC_OFAIL(newQCSession->sessionName, char, strlen(filename) + 1);
        strcpy(newQCSession->sessionName, filename);

        CALLOC_OFAIL(newQCSession->ltoDir, char, strlen("./") + 1);
        strcpy(newQCSession->ltoDir, "./");
    }
    else
    {
        CALLOC_OFAIL(newQCSession->sessionName, char, strlen(lastSep + 1) + 1);
        strcpy(newQCSession->sessionName, lastSep + 1);

        CALLOC_OFAIL(newQCSession->ltoDir, char, strlen(filename) - strlen(lastSep + 1) + 1);
        strncpy(newQCSession->ltoDir, filename, strlen(filename) - strlen(lastSep + 1));
    }

    if ((newQCSession->sessionFile = fopen(filename, "wb")) == NULL)
    {
        ml_log_error("Failed to open QC session '%s' for writing\n", filename);;
        goto fail;
    }

    newQCSession->playerListener.data = newQCSession;
    newQCSession->playerListener.frame_displayed_event = qcs_frame_displayed_event;
    newQCSession->playerListener.frame_dropped_event = qcs_frame_dropped_event;
    newQCSession->playerListener.state_change_event = qcs_state_change_event;
    newQCSession->playerListener.player_closed = qcs_player_closed;

    /* write header */
    write_comment(newQCSession, "Quality Control Log");
    write_comment(newQCSession, "");
    if (name != NULL)
    {
        write_comment(newQCSession, "Name: '%s'", name);
    }
    if (loadedSessionFilename != NULL)
    {
        write_comment(newQCSession, "Loaded session: '%s'", loadedSessionFilename);
    }
    get_timestamp_string(timestampStr);
    write_comment(newQCSession, "Started: %s", timestampStr);
    write_comment(newQCSession, "Username: '%s'", (userName == NULL) ? "" : userName);
    write_comment(newQCSession, "Hostname: '%s'", (hostName == NULL) ? "" : hostName);
    write_comment_start(newQCSession);
    if (argc > 0)
    {
        write_comment_middle(newQCSession, "Command: ");
        for (i = 0; i < argc; i++)
        {
            write_comment_middle(newQCSession, "%s ", argv[i]);
        }
    }
    write_comment_end(newQCSession);


    /* write metadata */

    write_comment(newQCSession, "");
    write_comment(newQCSession, "");
    write_comment(newQCSession, "");
    write_comment(newQCSession, "Metadata");
    write_comment(newQCSession, "");
    init_log_line(logLine);
    write_log_line_element(logLine, 0, "#");
    write_log_line_element(logLine, 1, "Name");
    write_log_line_element(logLine, 2, "Value");
    write_log_line(newQCSession, logLine, NULL);
    write_comment(newQCSession, "");

    CHK_OFAIL(msc_get_stream_info(source, 0, &streamInfo));
    for (i = 0; i < streamInfo->numSourceInfoValues; i++)
    {
        fprintf(newQCSession->sessionFile, ", ");
        safe_fprintf(newQCSession->sessionFile, streamInfo->sourceInfoValues[i].name,
            metadataBuffer, sizeof(metadataBuffer));
        fprintf(newQCSession->sessionFile, ", ");
        safe_fprintf(newQCSession->sessionFile, streamInfo->sourceInfoValues[i].value,
            metadataBuffer, sizeof(metadataBuffer));
        for (j = 0; j < QC_LOG_LINE_ELEMENTS - 3; j++)
        {
            fprintf(newQCSession->sessionFile, ", ");
        }
        fprintf(newQCSession->sessionFile, "\n");
    }
    write_comment(newQCSession, "");



    /* write state header */

    write_comment(newQCSession, "");
    write_comment(newQCSession, "");
    write_comment(newQCSession, "Player state");
    write_comment(newQCSession, "");
    init_log_line(logLine);
    write_log_line_element(logLine, 0, "#");
    write_log_line_element(logLine, 1, "First/Last frame displayed");
    write_log_line_element(logLine, 2, "Lock/Unlock");
    write_log_line_element(logLine, 3, "Play/Pause");
    write_log_line_element(logLine, 4, "Stop/Start");
    write_log_line_element(logLine, 5, "Speed");
    write_log_line_element(logLine, 6, "Control TC");
    write_log_line_element(logLine, 7, "VITC");
    write_log_line_element(logLine, 8, "LTC");
    write_log_line(newQCSession, logLine, NULL);
    write_comment(newQCSession, "");

    *qcSession = newQCSession;
    return 1;

fail:
    qcs_close(&newQCSession, NULL, NULL, NULL, 0);
    return 0;
}

int qcs_connect_to_player(QCSession* qcSession, MediaPlayer* player)
{
    return ply_register_player_listener(player, &qcSession->playerListener);
}

void qcs_write_marks(QCSession* qcSession, int includeAll, int clipMarkType,
    const MarkConfigs* markConfigs, Mark* marks, int numMarks)
{
    int i;
    int j;
    LogLine logLine;
    int64_t controlTCPos;
    int mask;
    char markName[32];
    int markType;

    write_comment(qcSession, "");
    write_comment(qcSession, "");
    write_comment(qcSession, "Marks");
    write_comment(qcSession, "");

    init_log_line(logLine);
    write_log_line_element(logLine, 0, "#");
    write_log_line_element(logLine, 1, "Position");
    write_log_line_element(logLine, 2, "Control TC");
    write_log_line_element(logLine, 3, "Clip Duration");
    write_log_line_element(logLine, 4, "Clip Type");
    write_log_line_element(logLine, 5, "Type");
    write_log_line_element(logLine, 6, "Description");
    write_log_line(qcSession, logLine, NULL);

    write_comment(qcSession, "");

    for (i = 0; i < numMarks; i++)
    {
        if (includeAll)
        {
            markType = marks[i].type;
        }
        else
        {
            /* filter out VTR errors, PSE failures and digibeta dropouts */
            markType = marks[i].type & ~(PSE_FAILURE_MARK_TYPE | VTR_ERROR_MARK_TYPE | DIGIBETA_DROPOUT_MARK_TYPE);
            if (markType == 0)
            {
                continue;
            }
        }

        /* filter out dangling clip marks */
        if ((markType & clipMarkType) != 0)
        {
            if (marks[i].pairedPosition < 0)
            {
                ml_log_warn("Incomplete clip mark when writing marks to session file\n");

                markType &= ~clipMarkType;
                if (markType == 0)
                {
                    continue;
                }
            }
        }

        /* Position */
        fprintf(qcSession->sessionFile, ", %"PRId64"", marks[i].position);

        /* CTC */
        /* TODO: don't hard code 25 fps */
        controlTCPos = marks[i].position + qcSession->controlTCStartPos;
        fprintf(qcSession->sessionFile, ", %02"PRId64":%02"PRId64":%02"PRId64":%02"PRId64"",
            controlTCPos / (25 * 60 * 60),
            (controlTCPos % (25 * 60 * 60)) / (25 * 60),
            ((controlTCPos % (25 * 60 * 60)) % (25 * 60)) / 25,
            ((controlTCPos % (25 * 60 * 60)) % (25 * 60)) % 25);


        /* Clip Duration, Clip Mark Type */
        if (marks[i].pairedPosition >= 0)
        {
            int64_t duration;
            if (marks[i].position <= marks[i].pairedPosition)
            {
                duration = marks[i].pairedPosition - marks[i].position + 1;
            }
            else
            {
                duration = marks[i].position - marks[i].pairedPosition + 1;
                duration *= -1; /* negative duration indicates that this mark is at the end of the clip */
            }
            fprintf(qcSession->sessionFile, ", %"PRId64"", duration);

            fprintf(qcSession->sessionFile, ", 0x%08x", clipMarkType);
        }
        else
        {
            fprintf(qcSession->sessionFile, ", ");
            fprintf(qcSession->sessionFile, ", ");
        }

        /* Mark Type */
        fprintf(qcSession->sessionFile, ", 0x%08x", markType);

        /* Mark Description */
        fprintf(qcSession->sessionFile, ", ");
        mask = 0x00000001;
        for (j = 1; j <= 32; j++)
        {
            if (markType & mask)
            {
                get_mark_name(markConfigs, j, markName);
                fprintf(qcSession->sessionFile, "'%s' ", markName);
            }
            mask <<= 1;
        }

        /* remaining columns */
        for (j = 0; j < QC_LOG_LINE_ELEMENTS - 5; j++)
        {
            fprintf(qcSession->sessionFile, ", ");
        }
        fprintf(qcSession->sessionFile, "\n");
    }
}

void qcs_flush(QCSession* qcSession)
{
    fflush(qcSession->sessionFile);
}

void qcs_close(QCSession** qcSession, const char* reportDirectory, const char* sessionScriptName, const char* sessionScriptOptions,
    int reportAccessPort)
{
    char timestampStr[MAX_TIMESTAMP_STRING_SIZE];
    char scriptCmd[FILENAME_MAX];
    char reportAccessPortStr[32];

    if (*qcSession == NULL)
    {
        return;
    }

    /* prepare the session script call */
    if (sessionScriptName != NULL)
    {
        strcpy(scriptCmd, sessionScriptName);
        strcat(scriptCmd, " --session \"");
        strcat(scriptCmd, strip_path((*qcSession)->sessionName));
        strcat(scriptCmd, "\"");
        strcat(scriptCmd, " --lto \"");
        strcat(scriptCmd, (*qcSession)->ltoDir);
        strcat(scriptCmd, "\"");
        strcat(scriptCmd, " --report \"");
        strcat(scriptCmd, reportDirectory);
        strcat(scriptCmd, "\"");
        strcat(scriptCmd, " --report-port ");
        sprintf(reportAccessPortStr, "%d", reportAccessPort);
        strcat(scriptCmd, reportAccessPortStr);
        if (sessionScriptOptions != NULL)
        {
            strcat(scriptCmd, " ");
            strcat(scriptCmd, sessionScriptOptions);
        }
    }


    write_comment((*qcSession), "");
    write_comment((*qcSession), "");
    get_timestamp_string(timestampStr);
    write_comment((*qcSession), "Ended: %s", timestampStr);

    write_comment((*qcSession), "");
    write_comment((*qcSession), "");
    get_timestamp_string(timestampStr);
    write_comment((*qcSession), "Comments");
    write_comment((*qcSession), "");
    fprintf((*qcSession)->sessionFile, "%s\n", (*qcSession)->sessionComments);


    if ((*qcSession)->sessionFile != NULL)
    {
        fclose((*qcSession)->sessionFile);
        (*qcSession)->sessionFile = NULL;
    }

    SAFE_FREE(&(*qcSession)->sessionName);
    SAFE_FREE(&(*qcSession)->ltoDir);

    SAFE_FREE(qcSession);


    /* call the session script */
    if (sessionScriptName != NULL)
    {
        if (system(scriptCmd) == 0)
        {
            ml_log_info("Session script success: %s\n", scriptCmd);
        }
        else
        {
            ml_log_error("Session script failed: %s\n", scriptCmd);
        }
    }
}


int qcs_restore_session(MediaControl* control, const char* filename, char* sessionComments)
{
    FILE* qcSessionFile = NULL;
    int state = 0;
    char buffer[128];
    int done = 0;
    int64_t position;
    int64_t duration;
    int type;
    int clipMarkType;
    char* positionStr;
    char* ctcStr;
    char* clipDurationStr;
    char* clipMarkTypeStr;
    char* markTypeStr;
    int64_t count = 0;
    int commentCount;
    int c;
    int haveFirst;


    /* open QC session file */
    if ((qcSessionFile = fopen(filename, "rb")) == NULL)
    {
        ml_log_error("Failed to open QC session %s for reading\n", filename);;
        return 0;
    }

    /* parse marks and set in player */
    while (!done)
    {
        switch (state)
        {
            /* find "# Marks" */
            case 0:
                if (fgets(buffer, 128, qcSessionFile) == NULL)
                {
                    /* EOF */
                    done = 1;
                    break;
                }

                if (strncmp("# Ended", buffer, 7) == 0)
                {
                    done = 1;
                    break;
                }

                if (strncmp("# Marks", buffer, 7) == 0)
                {
                    state = 1;
                }

                /* skip rest of line */
                while (strchr(buffer, '\n') == NULL)
                {
                    if (fgets(buffer, 128, qcSessionFile) == NULL)
                    {
                        /* EOF */
                        done = 1;
                        break;
                    }
                }
                break;

            /* parse marks */
            case 1:
                /* read the next line */
                if (fgets(buffer, 128, qcSessionFile) == NULL)
                {
                    /* EOF */
                    done = 1;
                    break;
                }

                if (strncmp("# Ended", buffer, 7) == 0)
                {
                    done = 1;
                    break;
                }

                if (buffer[0] != '#')
                {
                    positionStr = NULL;
                    ctcStr = NULL;
                    clipDurationStr = NULL;
                    clipMarkTypeStr = NULL;
                    markTypeStr = NULL;

                    positionStr = strchr(buffer, ',');
                    if (positionStr)
                    {
                        ctcStr = strchr(positionStr + 1, ',');
                    }
                    if (ctcStr)
                    {
                        clipDurationStr = strchr(ctcStr + 1, ',');
                    }
                    if (clipDurationStr)
                    {
                        clipMarkTypeStr = strchr(clipDurationStr + 1, ',');
                    }
                    if (clipMarkTypeStr)
                    {
                        markTypeStr = strchr(clipMarkTypeStr + 1, ',');
                    }

                    if (positionStr && clipDurationStr && clipMarkTypeStr && markTypeStr)
                    {
                        if (sscanf(positionStr + 1, "%"PRId64"", &position) != 1)
                        {
                            ml_log_warn("Failed to parse mark position in QC session\n");
                        }
                        else
                        {
                            type = parse_mark_type(markTypeStr + 1);
                            if (type != 0)
                            {
                                /* filter out VTR errors, PSE failures and digibeta dropouts which will be extracted from the MXF file */
                                type &= ~(PSE_FAILURE_MARK_TYPE | VTR_ERROR_MARK_TYPE | DIGIBETA_DROPOUT_MARK_TYPE);
                                if (type != 0)
                                {
                                    mc_mark_position(control, position, type, 0);
                                    count++;
                                }

                                /* mark the clip end position */
                                /* negative durations indicate this is the end mark and will be ignored because
                                the previous clip mark has already being read */
                                if (sscanf(clipDurationStr + 1, "%"PRId64"", &duration) == 1 &&
                                    duration > 1)
                                {
                                    clipMarkType = parse_mark_type(clipMarkTypeStr + 1);
                                    if (clipMarkType != 0)
                                    {
                                        /* filter out VTR errors, PSE failures and digibeta dropouts which will be extracted from the MXF file */
                                        clipMarkType &= ~(PSE_FAILURE_MARK_TYPE | VTR_ERROR_MARK_TYPE | DIGIBETA_DROPOUT_MARK_TYPE);
                                        if (clipMarkType != 0)
                                        {
                                            /* mark the end of the clip */
                                            mc_mark_position(control, position + duration - 1, clipMarkType, 0);
                                        }
                                    }
                                    else
                                    {
                                        ml_log_warn("Failed to parse paired mark type in QC session\n");
                                    }
                                }
                                /* else the mark is not paired or is the clip end mark */
                            }
                            else
                            {
                                ml_log_warn("Failed to parse mark type in QC session %s\n", markTypeStr);
                            }
                        }
                    }
                    else
                    {
                        ml_log_warn("Failed to read mark line\n");
                    }
                }

                /* skip rest of line */
                while (strchr(buffer, '\n') == NULL)
                {
                    if (fgets(buffer, 128, qcSessionFile) == NULL)
                    {
                        /* EOF */
                        done = 1;
                        break;
                    }
                }

                break;
        }
    }

    /* parse the comments */

    sessionComments[0] = '\0';

    while (1)
    {
        if (fgets(buffer, 128, qcSessionFile) == NULL)
        {
            /* EOF */
            break;
        }

        if (strncmp("# Comments", buffer, 7) == 0)
        {
            /* skip the next line */
            fgets(buffer, 128, qcSessionFile);

            /* the rest is comments (with start and end whitespace trimmed) */
            commentCount = 0;
            haveFirst = 0;
            while (commentCount + 1 < MAX_SESSION_COMMENTS_SIZE &&
                (c = fgetc(qcSessionFile)) != EOF)
            {
                if (haveFirst || !isspace(c))
                {
                    sessionComments[commentCount++] = (char)c;
                    haveFirst = 1;
                }
            }
            if (commentCount > 0)
            {
                sessionComments[commentCount] = '\0';
                commentCount--;
                while (commentCount >= 0 && isspace(sessionComments[commentCount]))
                {
                    sessionComments[commentCount] = '\0';
                    commentCount--;
                }
            }

            break;
        }
    }


    ml_log_info("Set %"PRId64" marks from qc session\n", count);

    fclose(qcSessionFile);
    return 1;
}

int qcs_is_session_file(const char* filename, const char* mxfFilename)
{
    size_t filenameLen = strlen(filename);
    size_t mxfFilenameLen = strlen(mxfFilename);

    /* return 1 if filename = mxfFilename + (.*) + '.txt' */

    if (strncmp(filename, mxfFilename, mxfFilenameLen) != 0)
    {
        return 0;
    }
    if (filenameLen < mxfFilenameLen + 4)
    {
        return 0;
    }
    if (filename[filenameLen - 4] != '.' ||
        tolower(filename[filenameLen - 3]) != 't' ||
        tolower(filename[filenameLen - 2]) != 'x' ||
        tolower(filename[filenameLen - 1]) != 't')
    {
        return 0;
    }

    return 1;
}

const char* qcs_get_session_name(QCSession* qcSession)
{
    return qcSession->sessionName;
}

int qcs_extract_timestamp(const char* sessionFilename, int* year, int* month, int* day, int* hour, int* min, int* sec)
{
    /* the session filename has the format <mxf filename> + g_qcSessionPreSuf + YYYYMMDD_HHMMSS.txt */

    const char* timestampStr = strstr(sessionFilename, g_qcSessionPreSuf);
    if (timestampStr == NULL)
    {
        return 0;
    }
    timestampStr += strlen(g_qcSessionPreSuf);
    if (strlen(timestampStr) != strlen("00000000_000000.txt"))
    {
        return 0;
    }

    char buffer[5];
    memset(buffer, 0, sizeof(buffer));

    memcpy(buffer, timestampStr, 4);
    buffer[4] = '\0';
    *year = atoi(buffer);

    memcpy(buffer, &timestampStr[4], 2);
    buffer[2] = '\0';
    *month = atoi(buffer);

    memcpy(buffer, &timestampStr[6], 2);
    buffer[2] = '\0';
    *day = atoi(buffer);

    memcpy(buffer, &timestampStr[9], 2);
    buffer[2] = '\0';
    *hour = atoi(buffer);

    memcpy(buffer, &timestampStr[11], 2);
    buffer[2] = '\0';
    *min = atoi(buffer);

    memcpy(buffer, &timestampStr[13], 2);
    buffer[2] = '\0';
    *sec = atoi(buffer);

    return 1;
}

void qcs_set_comments(QCSession* qcSession, const char* sessionComments)
{
    strncpy(qcSession->sessionComments, sessionComments, sizeof(qcSession->sessionComments) - 1);
    qcSession->sessionComments[sizeof(qcSession->sessionComments) - 1] = '\0';
}

