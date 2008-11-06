/*
 * $Id: on_screen_display.c,v 1.8 2008/11/06 19:56:56 john_f Exp $
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
#include <unistd.h>
#include <sys/time.h>
#include <math.h>

#include "on_screen_display.h"
#include "frame_info.h"
#include "osd_symbols.h"
#include "YUV_frame.h"
#include "YUV_text_overlay.h"
#include "utils.h"
#include "logging.h"
#include "macros.h"




#define SEC_IN_USEC             1000000

#define UNKNOWN_TC_OVLY_IDX      0
#define CONTROL_TC_OVLY_IDX      1
#define SOURCE_TC_OVLY_IDX       2
#define VITC_TC_OVLY_IDX         3
#define LTC_TC_OVLY_IDX          4
#define SYSTEM_TC_OVLY_IDX       5


/* TODO: these will not work for all video resolutions */
/* TODO: not nice to have the maximums in the frame_info.h determine the maximums here */
#define MAX_SOURCE_INFO_LINES           20
#define MAX_SOURCE_INFO_LINE_WIDTH      (MAX_SOURCE_INFO_NAME_LEN + 2 + MAX_SOURCE_INFO_VALUE_LEN)


#define SET_MAX(a, b)   a = (b > a) ? b : a;

#define AUDIO_LEVEL_BAR_WIDTH           6
#define AUDIO_LEVEL_MARGIN              2
#define AUDIO_LEVEL_BAR_SEP             2
#define AUDIO_LEVEL_RELATIVE_HEIGHT     0.7


#define MARK_OVERLAY_WIDTH      24
#define MARK_OVERLAY_HEIGHT     16
#define MARK_OVERLAY_MARGIN     2
#define MARK_OVERLAY_SPACING    4


#define PROGRESS_BAR_HEIGHT             12
#define PROGRESS_BAR_CENTER_HEIGHT      8
#define PROGRESS_BAR_ENDS_WIDTH         4   // should be > PROGRESS_BAR_MARK_WIDTH / 2
#define PROGRESS_BAR_TB_HEIGHT          2   // ((PROGRESS_BAR_HEIGHT - PROGRESS_BAR_CENTER_HEIGHT) / 2)
#define PROGRESS_BAR_CENTER_MARGIN      2

#define PROGRESS_BAR_MARKS_HEIGHT       2

#define PROGRESS_BAR_MARK_HEIGHT        8
#define PROGRESS_BAR_MARK_OVERLAP       6
#define PROGRESS_BAR_MARK_WIDTH         4   // <= 32 - see mark overlay


#define MAX_MENUS               16

#define MENU_OUTER_MARGIN       10
#define MENU_BOTTOM_MARGIN      10  // must be >= 4


#define PROGRESS_BAR_REGION_X_MARGIN        5
#define PROGRESS_BAR_REGION_Y_MARGIN        40


#define PLAY_STATE_TICKER_USER          0x0001
#define PROGRESS_POINTER_TICKER_USER    0x0002


typedef struct DefaultOnScreenDisplay DefaultOnScreenDisplay;

typedef struct
{
    int active;
    int present;
    OSDLabel label;
    overlay labelOverlay;
} DefaultOSDLabel;

typedef struct
{
    overlay textOverlay;
} OSDMenuListItemInt;

typedef struct
{
    int lineWidth;
    int numLines;
    int topLine;
    
    overlay titleOverlay;
    overlay statusOverlay;
    overlay commentOverlay;
    
    overlay selectionOverlay;
    overlay bottomMarginOverlay;
} OSDMenuModelInt;

struct DefaultOnScreenDisplay
{
    int maxAudioLevels;
    int addFieldSymbol;
    
    OnScreenDisplay osd;
    int initialised;
    
    StreamFormat videoFormat;
    int imageWidth;
    int imageHeight;
    Rational aspectRatio;
    
    OnScreenDisplayState* state;
    OnScreenDisplayState prevState; /* NOTE: does not include mark colours */
    
    OSDListener* listener;

    p_info_rec p_info;
    
    /* play state screen data */    
    timecode_data timecodeTextData;
    char_set_data timecodeTypeData;
    char_set_data numberData;
    /* pointers to global bitmaps */
    overlay* osdPlaySymbol;
    overlay* osdFastForwardSymbol;
    overlay* osdFastRewindSymbol;
    overlay* osdPauseSymbol;
    overlay* osdReversePlaySymbol; 
    overlay* osdFieldSymbol; 
    overlay* osdProgressBarPointer; 
    overlay* osdLongProgressBarPointer; 
    overlay* osdLockSymbol;
    

    /* source info screen data */    
    overlay* sourceInfoOverlay;
    
    /* audio level */
    int hideAudioLevels;
    overlay audioLevelOverlay;
    int haveAudioOverlay;
    overlay audioLevelM0Overlay;
    overlay audioLevelLineupOverlay;
    overlay audioLevelM40Overlay;
    overlay audioLevelM60Overlay;
    overlay audioLevelM96Overlay;
    
    /* marks */
    overlay markOverlay;
    
    /* progress bar */
    int hideProgressBar;
    overlay progressBarOverlay;
    overlay progressBarMarkOverlay;
    int64_t lastAvailableSourceLength;
    OSDMarksModel* marksModel; /* not owned by the OSD */
    OSDMarksModel* secondMarksModel; /* not owned by the OSD */
    overlay userMarksOverlay;
    overlay secondUserMarksOverlay;
    int marksUpdateMask;
    int secondMarksUpdateMask;
    pthread_mutex_t setMarksModelMutex;
    int highlightProgressPointer;
    long highlightProgressPointerTickStart;
    int activeProgressBarMarks;

    /* dropped frame indicator */
    overlay droppedFrameOverlay;
    
    /* menu */
    OSDMenuModel* menu;
    int menuUpdateMask;
    pthread_mutex_t setMenuModelMutex;
    
    /* labels */
    DefaultOSDLabel cachedLabels[MAX_OSD_LABELS];
    
    /* ticker */
	pthread_t tickerThreadId;
    long halfSecTickCount;
    int tickerUser;
    int stopTicker;
    long playStateTick;
};


static void set_ticker_user(DefaultOnScreenDisplay* osdd, int user, int enable)
{
    if (enable)
    {
        osdd->tickerUser |= user;
    }
    else
    {
        osdd->tickerUser &= ~user;
    }
}

static void* ticker_thread(void* arg)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)arg;
    struct timeval now;
    struct timeval next;
    long diffMSec;
    
    gettimeofday(&next, NULL);
    next.tv_sec += (next.tv_usec + 500 * 1000) / (1000 * 1000);
    next.tv_usec = (next.tv_usec + 500 * 1000) % (1000 * 1000);
    
    while (!osdd->stopTicker)
    {
        gettimeofday(&now, NULL);

        diffMSec = (next.tv_sec - now.tv_sec) * 1000L + (next.tv_usec - now.tv_usec) / 1000;
        if (diffMSec <= 0 || diffMSec > 1000)
        {
            if (osdd->tickerUser)
            {
                osdl_refresh_required(osdd->listener);
            }

            osdd->halfSecTickCount++;
            
            gettimeofday(&next, NULL);
            next.tv_sec += (next.tv_usec + 500 * 1000) / (1000 * 1000);
            next.tv_usec = (next.tv_usec + 500 * 1000) % (1000 * 1000);
        }
        
        usleep(100 * 1000);
    }
    
    pthread_exit((void *) NULL);
}


static void osdd_free_menu_int(void* data)
{
    OSDMenuModelInt* menuInt = (OSDMenuModelInt*)data;
    
    if (data == NULL)
    {
        return;
    }
    
    free_overlay(&menuInt->titleOverlay);
    free_overlay(&menuInt->statusOverlay);
    free_overlay(&menuInt->commentOverlay);
    free_overlay(&menuInt->selectionOverlay);
    free_overlay(&menuInt->bottomMarginOverlay);
    
    free(menuInt);
}

static void osdd_free_menu_list_item_int(void* data)
{
    OSDMenuListItemInt* itemInt = (OSDMenuListItemInt*)data;
    
    if (data == NULL)
    {
        return;
    }
    
    free_overlay(&itemInt->textOverlay);

    free(itemInt);
}

static int init_yuv_frame(DefaultOnScreenDisplay* osdd, YUV_frame* yuvFrame, unsigned char* image, int width, int height)
{
    if (osdd->videoFormat == UYVY_FORMAT)
    {
        CHK_ORET(YUV_frame_from_buffer(yuvFrame, image, width, height, UYVY) == 1);
    }
    else if (osdd->videoFormat == YUV422_FORMAT)
    {
        CHK_ORET(YUV_frame_from_buffer(yuvFrame, image, width, height, YUV422) == 1);
    }
    else /* YUV420_FORMAT */
    {
        CHK_ORET(YUV_frame_from_buffer(yuvFrame, image, width, height, I420) == 1);
    }
    
    return 1;
}

static void complete_labels(DefaultOnScreenDisplay* osdd)
{
    int i, j;
    int labelIsInCache[MAX_OSD_LABELS];
    int remainingLabels = osdd->state->numLabels;
    int cacheLabelsAvailable = MAX_OSD_LABELS;
    
    memset(labelIsInCache, 0, sizeof(labelIsInCache));
    

    /* reset cache labels and get number of cached labels available (!present) */
    for (j = 0; j < MAX_OSD_LABELS; j++)
    {
        osdd->cachedLabels[j].active = 0;
        if (osdd->cachedLabels[j].present)
        {
            cacheLabelsAvailable--;
        }
    }

    /* find all labels already in the cache */    
    for (i = 0; i < osdd->state->numLabels; i++)
    {
        for (j = 0; j < MAX_OSD_LABELS; j++)
        {
            if (osdd->cachedLabels[j].present &&
                osdd->cachedLabels[j].label.xPos == osdd->state->labels[i].xPos &&
                osdd->cachedLabels[j].label.yPos == osdd->state->labels[i].yPos &&
                osdd->cachedLabels[j].label.imageWidth == osdd->state->labels[i].imageWidth &&
                osdd->cachedLabels[j].label.imageHeight == osdd->state->labels[i].imageHeight &&
                osdd->cachedLabels[j].label.fontSize == osdd->state->labels[i].fontSize &&
                strcmp(osdd->cachedLabels[j].label.text, osdd->state->labels[i].text) == 0)
            {
                /* label is already in the cache */
                osdd->cachedLabels[j].label.colour = osdd->state->labels[i].colour;
                osdd->cachedLabels[j].label.box = osdd->state->labels[i].box;
                osdd->cachedLabels[j].active = 1;
                labelIsInCache[i] = 1;
                remainingLabels--;
                break;
            }
        }
    }
    
    /* create labels that are not already present in the cache */
    for (i = 0; i < osdd->state->numLabels; i++)
    {
        if (labelIsInCache[i])
        {
            continue;
        }
        
        for (j = 0; j < MAX_OSD_LABELS; j++)
        {
            if (osdd->cachedLabels[j].active)
            {
                continue;
            }
            
            if (remainingLabels > cacheLabelsAvailable &&
                osdd->cachedLabels[j].present)
            {
                /* clear the cached label which will be replaced with a new one */
                free_overlay(&osdd->cachedLabels[j].labelOverlay);
                osdd->cachedLabels[j].present = 0;
            }
            
            if (!osdd->cachedLabels[j].present)
            {
                /* create new label */
                osdd->cachedLabels[j].label.xPos = osdd->state->labels[i].xPos;
                osdd->cachedLabels[j].label.yPos = osdd->state->labels[i].yPos;
                osdd->cachedLabels[j].label.imageWidth = osdd->state->labels[i].imageWidth;
                osdd->cachedLabels[j].label.imageHeight = osdd->state->labels[i].imageHeight;
                osdd->cachedLabels[j].label.fontSize = osdd->state->labels[i].fontSize;
                osdd->cachedLabels[j].label.colour = osdd->state->labels[i].colour;
                osdd->cachedLabels[j].label.box = osdd->state->labels[i].box;
                strncpy(osdd->cachedLabels[j].label.text, osdd->state->labels[i].text, sizeof(osdd->cachedLabels[j].label.text) - 1);
                osdd->cachedLabels[j].label.text[sizeof(osdd->cachedLabels[j].label.text) - 1] = '\0';
             
                if (text_to_overlay(&osdd->p_info, &osdd->cachedLabels[j].labelOverlay, 
                    osdd->cachedLabels[j].label.text, 
                    osdd->imageWidth - 20, 0,
                    0, 0,
                    0,
                    0,
                    0,
                    "Ariel", 
                    osdd->cachedLabels[j].label.fontSize * osdd->imageHeight / osdd->state->labels[i].imageHeight, 
                    osdd->aspectRatio.num, osdd->aspectRatio.den) < 0)
                {
                    ml_log_error("Failed to create text overlay for label\n");
                    return;
                }
                
                osdd->cachedLabels[j].present = 1;
                osdd->cachedLabels[j].active = 1;
                remainingLabels--;
                break;
            }
        }
    }
}

static int initialise_audio_level_overlay(DefaultOnScreenDisplay* osdd, int width, int height)
{
    osdd->audioLevelOverlay.w = 2 * AUDIO_LEVEL_MARGIN + /* left and right margin */ 
        osdd->maxAudioLevels * AUDIO_LEVEL_BAR_WIDTH + /* bars */
        (osdd->maxAudioLevels - 1) * AUDIO_LEVEL_BAR_SEP; /* in-between bars */
    osdd->audioLevelOverlay.h = height * AUDIO_LEVEL_RELATIVE_HEIGHT;

    if (osdd->videoFormat == UYVY_FORMAT)
    {
        osdd->audioLevelOverlay.ssx = 2;
        osdd->audioLevelOverlay.ssy = 1;
    }
    else if (osdd->videoFormat == YUV422_FORMAT)
    {
        osdd->audioLevelOverlay.ssx = 2;
        osdd->audioLevelOverlay.ssy = 1;
    }
    else /* YUV420 */
    {
        osdd->audioLevelOverlay.ssx = 2;
        osdd->audioLevelOverlay.ssy = 2;
    }
    
    MALLOC_ORET(osdd->audioLevelOverlay.buff, unsigned char, osdd->audioLevelOverlay.w * osdd->audioLevelOverlay.h * 2);
    osdd->audioLevelOverlay.Cbuff = NULL;
    memset(osdd->audioLevelOverlay.buff, 40 /* something > than black for in-between bars and at margins */, 
        osdd->audioLevelOverlay.w * osdd->audioLevelOverlay.h * 2);
    
   /* level indicators */
    if (text_to_overlay(&osdd->p_info, &osdd->audioLevelM0Overlay, 
        "0dBFS", 
        width - 10 - osdd->audioLevelOverlay.w, 15,
        0, 4,
        0,
        0,
        0,
        "Ariel", 10, osdd->aspectRatio.num, osdd->aspectRatio.den) < 0)
    {
        ml_log_error("Failed to create text overlay\n");
        goto fail;
    }
    memset(&osdd->audioLevelM0Overlay.buff[0], 255, 15);
        
    char buf[16];
    snprintf(buf, 16, "%.0f", osdd->state->audioLineupLevel); 
    if (text_to_overlay(&osdd->p_info, &osdd->audioLevelLineupOverlay, 
        buf, 
        width - 10 - osdd->audioLevelOverlay.w, 15,
        0, 4,
        0,
        0,
        0,
        "Ariel", 10, osdd->aspectRatio.num, osdd->aspectRatio.den) < 0)
    {
        ml_log_error("Failed to create text overlay\n");
        goto fail;
    }
    memset(&osdd->audioLevelLineupOverlay.buff[0], 255, 15);
        
    if (text_to_overlay(&osdd->p_info, &osdd->audioLevelM40Overlay, 
        "-40", 
        width - 10 - osdd->audioLevelOverlay.w, 15,
        0, 4,
        0,
        0,
        0,
        "Ariel", 10, osdd->aspectRatio.num, osdd->aspectRatio.den) < 0)
    {
        ml_log_error("Failed to create text overlay\n");
        goto fail;
    }
    memset(&osdd->audioLevelM40Overlay.buff[0], 255, 15);
        
    if (text_to_overlay(&osdd->p_info, &osdd->audioLevelM60Overlay, 
        "-60", 
        width - 10 - osdd->audioLevelOverlay.w, 15,
        0, 4,
        0,
        0,
        0,
        "Ariel", 10, osdd->aspectRatio.num, osdd->aspectRatio.den) < 0)
    {
        ml_log_error("Failed to create text overlay\n");
        goto fail;
    }
    memset(&osdd->audioLevelM60Overlay.buff[0], 255, 15);
        
    if (text_to_overlay(&osdd->p_info, &osdd->audioLevelM96Overlay, 
        "-96", 
        width - 10 - osdd->audioLevelOverlay.w, 15,
        0, 4,
        0,
        0,
        0,
        "Ariel", 10, osdd->aspectRatio.num, osdd->aspectRatio.den) < 0)
    {
        ml_log_error("Failed to create text overlay\n");
        goto fail;
    }
    memset(&osdd->audioLevelM96Overlay.buff[0], 255, 15);
        
    return 1;
    
fail:
    return 0;
}

static void fill_audio_level_overlay(DefaultOnScreenDisplay* osdd)
{
    int stream;
    int line;
    unsigned char* barStart;
    double level;

    osdd->haveAudioOverlay = 0;
    
    /* initialise overlay if it has not done before or num audio levels have changed */
    if (osdd->maxAudioLevels != osdd->state->numAudioLevels)
    {
        osdd->maxAudioLevels = osdd->state->numAudioLevels;
        free_overlay(&osdd->audioLevelOverlay);
        if (!initialise_audio_level_overlay(osdd, osdd->imageWidth, osdd->imageHeight))
        {
            ml_log_error("Failed to initialise audio level overlay\n");
            return;
        }   
    }
    
    
    for (line = AUDIO_LEVEL_MARGIN; line < osdd->audioLevelOverlay.h - AUDIO_LEVEL_MARGIN; line++)
    {
        barStart = osdd->audioLevelOverlay.buff + line * osdd->audioLevelOverlay.w + AUDIO_LEVEL_MARGIN;
        level = osdd->state->minimumAudioLevel * 
            (line - AUDIO_LEVEL_MARGIN) / 
                (double)(osdd->audioLevelOverlay.h - 2 * AUDIO_LEVEL_MARGIN - 1); 
        
        for (stream = 0; stream < osdd->state->numAudioLevels; stream++)
        {
            if (osdd->state->audioStreamLevels[stream].level >= level)
            {
                /* bar */
                memset(barStart, 255, AUDIO_LEVEL_BAR_WIDTH); 
            }
            else
            {
                /* no bar */
                memset(barStart, 0, AUDIO_LEVEL_BAR_WIDTH);
            }
            
            barStart += AUDIO_LEVEL_BAR_WIDTH + AUDIO_LEVEL_BAR_SEP;
        }

        if (line == AUDIO_LEVEL_MARGIN)
        {
            memset(osdd->audioLevelOverlay.buff + line * osdd->audioLevelOverlay.w, 255, osdd->audioLevelOverlay.w); 
        }
        else if (line == (int)(AUDIO_LEVEL_MARGIN + (osdd->audioLevelOverlay.h - 2 * AUDIO_LEVEL_MARGIN) * (osdd->state->audioLineupLevel) / osdd->state->minimumAudioLevel))
        {
            memset(osdd->audioLevelOverlay.buff + line * osdd->audioLevelOverlay.w, 255, osdd->audioLevelOverlay.w); 
        }
        else if (line == (int)(AUDIO_LEVEL_MARGIN + (osdd->audioLevelOverlay.h - 2 * AUDIO_LEVEL_MARGIN) * (-40) / osdd->state->minimumAudioLevel))
        {
            memset(osdd->audioLevelOverlay.buff + line * osdd->audioLevelOverlay.w, 255, osdd->audioLevelOverlay.w); 
        }
        else if (line == (int)(AUDIO_LEVEL_MARGIN + (osdd->audioLevelOverlay.h - 2 * AUDIO_LEVEL_MARGIN) * (-60) / osdd->state->minimumAudioLevel))
        {
            memset(osdd->audioLevelOverlay.buff + line * osdd->audioLevelOverlay.w, 255, osdd->audioLevelOverlay.w); 
        }
        else if (line == (int)(AUDIO_LEVEL_MARGIN + (osdd->audioLevelOverlay.h - 2 * AUDIO_LEVEL_MARGIN) * (-96) / osdd->state->minimumAudioLevel))
        {
            memset(osdd->audioLevelOverlay.buff + line * osdd->audioLevelOverlay.w, 255, osdd->audioLevelOverlay.w); 
        }
    }
    
    osdd->haveAudioOverlay = 1;
}

static int add_play_state_screen(DefaultOnScreenDisplay* osdd, const FrameInfo* frameInfo, 
    unsigned char* image, int width, int height)
{
    YUV_frame yuvFrame;
    int xPos, yPos;
    char txtY, txtU, txtV, box;
    int hour, min, sec, frame;
    int frameCount;
    overlay* tcTypeOvly;
    char numberString[17];
    int numberStringLen;
    int i, k;
    int timecodeXPos;
    int timecodeYPos;
    int timecodeWidth;
    int timecodeHeight;
    unsigned char* bufPtr;
    unsigned char* bufPtr2;
    int availableSourceWidth;
    int haveMarksModel;
    int haveSecondMarksModel;
    int markOverlayUpdated;
    int secondMarkOverlayUpdated;
    int markPixelSet;
    int markPixel;
    int hideAudioLevels;
   

    /* initialise the YUV lib frame */    
    CHK_ORET(init_yuv_frame(osdd, &yuvFrame, image, width, height) == 1);
    
    
    
    /* start/stop ticker use if required */
    
    if (osdd->state->isPlaying)
    {
        if (osdd->state->isPlaying != osdd->prevState.isPlaying ||
            osdd->state->playForwards != osdd->prevState.playForwards ||
            osdd->state->playSpeed != osdd->prevState.playSpeed)
        {
            osdd->playStateTick = osdd->halfSecTickCount + 4;
            set_ticker_user(osdd, PLAY_STATE_TICKER_USER, 1);
        }
        else if (osdd->halfSecTickCount > osdd->playStateTick ||
            frameInfo->position + 1 >= frameInfo->sourceLength)
        {
            /* hide play state symbol if the 4 half seconds are over when the state
            hasn't changed or when the player is at eof */
            set_ticker_user(osdd, PLAY_STATE_TICKER_USER, 0);
            osdd->playStateTick = osdd->halfSecTickCount;
        }
    }
    else
    {
        if (osdd->playStateTick > osdd->halfSecTickCount)
        {
            /* no need for refreshed when paused because pause symbol stays active */
            set_ticker_user(osdd, PLAY_STATE_TICKER_USER, 0);
        }
    }
    
    if (osdd->highlightProgressPointer)
    {
        set_ticker_user(osdd, PROGRESS_POINTER_TICKER_USER, 1);
    }
    else
    {
        set_ticker_user(osdd, PROGRESS_POINTER_TICKER_USER, 0);
    }

    
    /* show player state if timer busy or player is paused */
    
    if (frameInfo->isMarked)
    {
        txtY = g_rec601YUVColours[RED_COLOUR].Y;
        txtU = g_rec601YUVColours[RED_COLOUR].U;
        txtV = g_rec601YUVColours[RED_COLOUR].V;
        box = 80;
    }
    else
    {
        txtY = g_rec601YUVColours[LIGHT_WHITE_COLOUR].Y;
        txtU = g_rec601YUVColours[LIGHT_WHITE_COLOUR].U;
        txtV = g_rec601YUVColours[LIGHT_WHITE_COLOUR].V;
        box = 80;
    }
    
    if (osdd->playStateTick > osdd->halfSecTickCount || !osdd->state->isPlaying)
    {
        /* if player is paused, then this overrides any other state */
        if (!osdd->state->isPlaying)
        {
            xPos = (width - osdd->osdPauseSymbol->w) / 2;
            yPos = (height * 11) / 16 - osdd->osdPauseSymbol->h / 2;
            CHK_ORET(add_overlay(osdd->osdPauseSymbol, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
        }
        else if (osdd->state->playSpeed == 1)
        {
            xPos = (width - osdd->osdPlaySymbol->w) / 2;
            yPos = (height * 11) / 16 - osdd->osdPlaySymbol->h / 2;
            CHK_ORET(add_overlay(osdd->osdPlaySymbol, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
        }
        else if (osdd->state->playSpeed == -1)
        {
            xPos = (width - osdd->osdReversePlaySymbol->w) / 2;
            yPos = (height * 11) / 16 - osdd->osdReversePlaySymbol->h / 2;
            CHK_ORET(add_overlay(osdd->osdReversePlaySymbol, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
        }
        else if (osdd->state->playSpeed > 1)
        {
            sprintf(numberString, "%d", osdd->state->playSpeed);
            numberStringLen = strlen(numberString);
            
            xPos = (width - (osdd->osdPlaySymbol->w + osdd->numberData.cs_ovly[0].w * numberStringLen + 5)) / 2;
            yPos = (height * 11) / 16 - osdd->osdPlaySymbol->h / 2;
            CHK_ORET(add_overlay(osdd->osdFastForwardSymbol, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);

            xPos += osdd->osdFastForwardSymbol->w + 5;
            for (i = 0; i < numberStringLen; i++)
            {
                CHK_ORET(add_overlay(&osdd->numberData.cs_ovly[numberString[i] - '0'], 
                    &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
                xPos += osdd->numberData.cs_ovly[numberString[i] - '0'].w;
            }
        }
        else /* osdd->state->playSpeed < 1 */
        {
            sprintf(numberString, "%d", abs(osdd->state->playSpeed));
            numberStringLen = strlen(numberString);
            
            xPos = (width - (osdd->osdPlaySymbol->w + osdd->numberData.cs_ovly[0].w * numberStringLen + 5)) / 2;
            yPos = (height * 11) / 16 - osdd->osdPlaySymbol->h / 2;
            for (i = 0; i < numberStringLen; i++)
            {
                CHK_ORET(add_overlay(&osdd->numberData.cs_ovly[numberString[i] - '0'], 
                    &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
                xPos += osdd->numberData.cs_ovly[numberString[i] - '0'].w;
            }
            
            xPos += 5;
            CHK_ORET(add_overlay(osdd->osdFastRewindSymbol, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
        }
    }

    
    /* mark */
    
    if (frameInfo->isMarked)
    {
        xPos = (width - (osdd->state->markConfigs.numConfigs * (osdd->markOverlay.w + MARK_OVERLAY_SPACING)) - MARK_OVERLAY_SPACING) / 2;
        yPos = (height * 11) / 16 + osdd->osdPlaySymbol->h / 2 + 5;
    
        for (i = 0; i < osdd->state->markConfigs.numConfigs; i++)
        {
            if (frameInfo->markType & osdd->state->markConfigs.configs[i].type)
            {
                /* on */
                txtY = g_rec601YUVColours[osdd->state->markConfigs.configs[i].colour].Y;
                txtU = g_rec601YUVColours[osdd->state->markConfigs.configs[i].colour].U;
                txtV = g_rec601YUVColours[osdd->state->markConfigs.configs[i].colour].V;
                box = 80;
            }
            else
            {
                /* off */
                txtY = g_rec601YUVColours[BLACK_COLOUR].Y;
                txtU = g_rec601YUVColours[BLACK_COLOUR].U;
                txtV = g_rec601YUVColours[BLACK_COLOUR].V;
                box = 80;
            }

            CHK_ORET(add_overlay(&osdd->markOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);

            xPos += osdd->markOverlay.w + MARK_OVERLAY_SPACING;
        }
    }


    
    /* timecode type and timecode */

    if (frameInfo->isMarked)
    {
        txtY = g_rec601YUVColours[RED_COLOUR].Y;
        txtU = g_rec601YUVColours[RED_COLOUR].U;
        txtV = g_rec601YUVColours[RED_COLOUR].V;
        box = 80;
    }
    else
    {
        txtY = g_rec601YUVColours[LIGHT_WHITE_COLOUR].Y;
        txtU = g_rec601YUVColours[LIGHT_WHITE_COLOUR].U;
        txtV = g_rec601YUVColours[LIGHT_WHITE_COLOUR].V;
        box = 80;
    }
    
    timecodeXPos = width / 2;
    timecodeYPos = (height * 13) / 16;
    timecodeWidth = 0;
    timecodeHeight = 0;
    
    /* display timecode */
    if (osdd->state->timecodeIndex >= 0)
    {
        /* legitimise */
        hour = frameInfo->timecodes[osdd->state->timecodeIndex].timecode.hour;
        min = frameInfo->timecodes[osdd->state->timecodeIndex].timecode.min;
        sec = frameInfo->timecodes[osdd->state->timecodeIndex].timecode.sec;
        frame = frameInfo->timecodes[osdd->state->timecodeIndex].timecode.frame;
        if (hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 59 || frame < 0 || frame > 24)
        {
            hour = 0;
            min = 0;
            sec = 0;
            frame = 0;
            /* TODO: log warning ? */
        }
        
        if (frameInfo->timecodes[osdd->state->timecodeIndex].timecodeType == CONTROL_TIMECODE_TYPE)
        {
            tcTypeOvly = &osdd->timecodeTypeData.cs_ovly[CONTROL_TC_OVLY_IDX];
        }
        else if (frameInfo->timecodes[osdd->state->timecodeIndex].timecodeType == SOURCE_TIMECODE_TYPE && 
            frameInfo->timecodes[osdd->state->timecodeIndex].timecodeSubType == VITC_SOURCE_TIMECODE_SUBTYPE)
        {
            tcTypeOvly = &osdd->timecodeTypeData.cs_ovly[VITC_TC_OVLY_IDX];
        }
        else if (frameInfo->timecodes[osdd->state->timecodeIndex].timecodeType == SOURCE_TIMECODE_TYPE && 
            frameInfo->timecodes[osdd->state->timecodeIndex].timecodeSubType == LTC_SOURCE_TIMECODE_SUBTYPE)
        {
            tcTypeOvly = &osdd->timecodeTypeData.cs_ovly[LTC_TC_OVLY_IDX];
        }
        else if (frameInfo->timecodes[osdd->state->timecodeIndex].timecodeType == SOURCE_TIMECODE_TYPE)
        {
            tcTypeOvly = &osdd->timecodeTypeData.cs_ovly[SOURCE_TC_OVLY_IDX];
        }
        else if (frameInfo->timecodes[osdd->state->timecodeIndex].timecodeType == SYSTEM_TIMECODE_TYPE)
        {
            tcTypeOvly = &osdd->timecodeTypeData.cs_ovly[SYSTEM_TC_OVLY_IDX];
        }
        else
        {
            tcTypeOvly = &osdd->timecodeTypeData.cs_ovly[UNKNOWN_TC_OVLY_IDX];
        }
        
        frameCount = hour * 60 * 60 * 25 + min * 60 * 25 + sec * 25 + frame;

        timecodeXPos = (width - (osdd->timecodeTypeData.cs_ovly[UNKNOWN_TC_OVLY_IDX].w * 5 / 3 + 
            osdd->timecodeTextData.width)) / 2;
        timecodeYPos = (height * 13) / 16 - osdd->timecodeTextData.height / 2;
        timecodeWidth = osdd->timecodeTypeData.cs_ovly[UNKNOWN_TC_OVLY_IDX].w * 5 / 3 + 
            osdd->timecodeTextData.width;
        timecodeHeight = osdd->timecodeTextData.height;
        
        /* timecode type */
        CHK_ORET(add_overlay(tcTypeOvly, &yuvFrame, timecodeXPos, timecodeYPos, txtY, txtU, txtV, box) == 0);

        /* timecode */
        xPos = timecodeXPos + osdd->timecodeTypeData.cs_ovly[UNKNOWN_TC_OVLY_IDX].w * 5 / 3;
        CHK_ORET(add_timecode(&osdd->timecodeTextData, frameCount, &yuvFrame, xPos, timecodeYPos, txtY, txtU, txtV, box) == YUV_OK);
    }
    
    

    /* progress bar */
    
    
    /* progress bar source progress updates */
    
    if (frameInfo->sourceLength > 0 && frameInfo->availableSourceLength > 0 &&
        frameInfo->availableSourceLength != osdd->lastAvailableSourceLength)
    {
        availableSourceWidth = (osdd->progressBarOverlay.w - 2 * PROGRESS_BAR_ENDS_WIDTH) *
            frameInfo->availableSourceLength / frameInfo->sourceLength;
        bufPtr = osdd->progressBarOverlay.buff + 
            (PROGRESS_BAR_TB_HEIGHT + PROGRESS_BAR_CENTER_MARGIN) * osdd->progressBarOverlay.w + 
            PROGRESS_BAR_ENDS_WIDTH;
        for (i = 0; i < PROGRESS_BAR_CENTER_HEIGHT - 2 * PROGRESS_BAR_CENTER_MARGIN; i++)
        {
            memset(bufPtr, 255, availableSourceWidth);
            memset(bufPtr + availableSourceWidth, 0, (osdd->progressBarOverlay.w - 2 * PROGRESS_BAR_ENDS_WIDTH) - availableSourceWidth);
            bufPtr += osdd->progressBarOverlay.w;
        }
        
    }
    osdd->lastAvailableSourceLength = frameInfo->availableSourceLength;

    /* progress bar marks updates */
    
    PTHREAD_MUTEX_LOCK(&osdd->setMarksModelMutex) /* prevent change in osdd->marksModel value */
    
    haveMarksModel = (osdd->marksModel != NULL);
    markOverlayUpdated = 0;
    if (haveMarksModel)
    {
        /* update overlay with mark updates */
        
        PTHREAD_MUTEX_LOCK(&osdd->marksModel->marksMutex)  /* prevent change in marks */
        
        if (osdd->marksModel->updated & osdd->marksUpdateMask)
        {
            /* a mark will result in a sequence of set bits with length PROGRESS_BAR_MARK_WIDTH */
            markPixelSet = 0x00000000;
            for (i = 0; i < PROGRESS_BAR_MARK_WIDTH; i++)
            {
                 markPixelSet <<= 1;
                 markPixelSet |= 0x1;
            }
            
            /* set first line of overlay */
            bufPtr = osdd->userMarksOverlay.buff;
            markPixel = 0x00000000;
            for (i = 0; i < osdd->marksModel->numMarkCounts; i++)
            {
                markPixel >>= 1;
                markPixel |= (osdd->marksModel->markCounts[i] > 0) ? markPixelSet : 0x0;

                *bufPtr++ = (markPixel ? 255 : 0);
            }
            /* leftovers */
            for (i = 0; i < PROGRESS_BAR_MARK_WIDTH; i++)
            {
                markPixel >>= 1;

                *bufPtr++ = (markPixel ? 255 : 0);
            }
            
            /* copy first line to other lines */
            bufPtr = osdd->userMarksOverlay.buff;
            bufPtr2 = bufPtr + osdd->userMarksOverlay.w;
            for (k = 0; k < osdd->userMarksOverlay.h - 1; k++)
            {
                memcpy(bufPtr2, bufPtr, osdd->userMarksOverlay.w);
                bufPtr2 += osdd->userMarksOverlay.w;
            }
            
            osdd->marksModel->updated &= ~osdd->marksUpdateMask;
            markOverlayUpdated = 1;
        }
        
        PTHREAD_MUTEX_UNLOCK(&osdd->marksModel->marksMutex)
    }
    
    haveSecondMarksModel = (osdd->secondMarksModel != NULL);
    secondMarkOverlayUpdated = 0;
    if (haveSecondMarksModel)
    {
        /* update overlay with mark updates */
        
        PTHREAD_MUTEX_LOCK(&osdd->secondMarksModel->marksMutex)  /* prevent change in marks */
        
        if (osdd->secondMarksModel->updated & osdd->secondMarksUpdateMask)
        {
            /* a mark will result in a sequence of set bits with length PROGRESS_BAR_MARK_WIDTH */
            markPixelSet = 0x00000000;
            for (i = 0; i < PROGRESS_BAR_MARK_WIDTH; i++)
            {
                 markPixelSet <<= 1;
                 markPixelSet |= 0x1;
            }
            
            /* set first line of overlay */
            bufPtr = osdd->secondUserMarksOverlay.buff;
            markPixel = 0x00000000;
            for (i = 0; i < osdd->secondMarksModel->numMarkCounts; i++)
            {
                markPixel >>= 1;
                markPixel |= (osdd->secondMarksModel->markCounts[i] > 0) ? markPixelSet : 0x0;

                *bufPtr++ = (markPixel ? 255 : 0);
            }
            /* leftovers */
            for (i = 0; i < PROGRESS_BAR_MARK_WIDTH; i++)
            {
                markPixel >>= 1;

                *bufPtr++ = (markPixel ? 255 : 0);
            }
            
            /* copy first line to other lines */
            bufPtr = osdd->secondUserMarksOverlay.buff;
            bufPtr2 = bufPtr + osdd->secondUserMarksOverlay.w;
            for (k = 0; k < osdd->secondUserMarksOverlay.h - 1; k++)
            {
                memcpy(bufPtr2, bufPtr, osdd->secondUserMarksOverlay.w);
                bufPtr2 += osdd->secondUserMarksOverlay.w;
            }
            
            osdd->secondMarksModel->updated &= ~osdd->secondMarksUpdateMask;
            secondMarkOverlayUpdated = 1;
        }
        
        PTHREAD_MUTEX_UNLOCK(&osdd->secondMarksModel->marksMutex)
    }
    
    PTHREAD_MUTEX_UNLOCK(&osdd->setMarksModelMutex)
    
    
    /* progress bar overlays */
    
    if (!osdd->hideProgressBar)
    {
        txtY = g_rec601YUVColours[LIGHT_WHITE_COLOUR].Y;
        txtU = g_rec601YUVColours[LIGHT_WHITE_COLOUR].U;
        txtV = g_rec601YUVColours[LIGHT_WHITE_COLOUR].V;
        box = 80;
            
        xPos = (width - osdd->progressBarOverlay.w) / 2;
        yPos = timecodeYPos + timecodeHeight + 20;
        CHK_ORET(add_overlay(&osdd->progressBarOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
        
        
        if (haveMarksModel)
        {
            if (osdd->activeProgressBarMarks == 0)
            {
                if (haveSecondMarksModel)
                {
                    txtY = g_rec601YUVColours[ORANGE_COLOUR].Y;
                    txtU = g_rec601YUVColours[ORANGE_COLOUR].U;
                    txtV = g_rec601YUVColours[ORANGE_COLOUR].V;
                }
                else
                {
                    txtY = g_rec601YUVColours[LIGHT_WHITE_COLOUR].Y;
                    txtU = g_rec601YUVColours[LIGHT_WHITE_COLOUR].U;
                    txtV = g_rec601YUVColours[LIGHT_WHITE_COLOUR].V;
                }
            }
            else
            {
                txtY = g_rec601YUVColours[LIGHT_GREY_COLOUR].Y;
                txtU = g_rec601YUVColours[LIGHT_GREY_COLOUR].U;
                txtV = g_rec601YUVColours[LIGHT_GREY_COLOUR].V;
            }
            box = 80;
                
            xPos = (width - osdd->progressBarMarkOverlay.w) / 2;
            yPos = timecodeYPos + timecodeHeight + 20 + osdd->progressBarOverlay.h - osdd->progressBarMarkOverlay.h;
            CHK_ORET(add_overlay(&osdd->progressBarMarkOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
            
            if (markOverlayUpdated)
            {
                /* set colour buffer to NULL, otherwise it doesn't get updated */
                /* TODO: this could be made more efficient by filling the Cbuff ourselves */
                osdd->userMarksOverlay.Cbuff = NULL;
            }
                
            txtY = g_rec601YUVColours[RED_COLOUR].Y;
            txtU = g_rec601YUVColours[RED_COLOUR].U;
            txtV = g_rec601YUVColours[RED_COLOUR].V;
            box = 0;
            
            xPos = (width - osdd->progressBarOverlay.w) / 2 + PROGRESS_BAR_ENDS_WIDTH - PROGRESS_BAR_MARK_WIDTH / 2;
            yPos = timecodeYPos + timecodeHeight + 20 + osdd->progressBarOverlay.h - PROGRESS_BAR_MARK_OVERLAP;
            CHK_ORET(add_overlay(&osdd->userMarksOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
        }
        
        if (haveSecondMarksModel)
        {
            if (osdd->activeProgressBarMarks == 1)
            {
                txtY = g_rec601YUVColours[ORANGE_COLOUR].Y;
                txtU = g_rec601YUVColours[ORANGE_COLOUR].U;
                txtV = g_rec601YUVColours[ORANGE_COLOUR].V;
            }
            else
            {
                txtY = g_rec601YUVColours[LIGHT_GREY_COLOUR].Y;
                txtU = g_rec601YUVColours[LIGHT_GREY_COLOUR].U;
                txtV = g_rec601YUVColours[LIGHT_GREY_COLOUR].V;
            }
            box = 80;
                
            xPos = (width - osdd->progressBarMarkOverlay.w) / 2;
            yPos = timecodeYPos + timecodeHeight + 20 + osdd->progressBarOverlay.h + 8;
            CHK_ORET(add_overlay(&osdd->progressBarMarkOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
            
            if (secondMarkOverlayUpdated)
            {
                /* set colour buffer to NULL, otherwise it doesn't get updated */
                /* TODO: this could be made more efficient by filling the Cbuff ourselves */
                osdd->secondUserMarksOverlay.Cbuff = NULL;
            }
                
            txtY = g_rec601YUVColours[RED_COLOUR].Y;
            txtU = g_rec601YUVColours[RED_COLOUR].U;
            txtV = g_rec601YUVColours[RED_COLOUR].V;
            box = 0;
            
            xPos = (width - osdd->progressBarOverlay.w) / 2 + PROGRESS_BAR_ENDS_WIDTH - PROGRESS_BAR_MARK_WIDTH / 2;
            yPos = timecodeYPos + timecodeHeight + 20 + osdd->progressBarOverlay.h + PROGRESS_BAR_MARK_HEIGHT + 4 - PROGRESS_BAR_MARK_OVERLAP;
            CHK_ORET(add_overlay(&osdd->secondUserMarksOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
        }
        
        if (osdd->highlightProgressPointer &&
            (osdd->halfSecTickCount - osdd->highlightProgressPointerTickStart) % 2 == 0)
        {
            txtY = g_rec601YUVColours[LIGHT_WHITE_COLOUR].Y;
            txtU = g_rec601YUVColours[LIGHT_WHITE_COLOUR].U;
            txtV = g_rec601YUVColours[LIGHT_WHITE_COLOUR].V;
        }
        else if (frameInfo->position == 0 ||
            (frameInfo->sourceLength > 0 && frameInfo->position >= frameInfo->sourceLength - 1))
        {
            /* at start or end */
            txtY = g_rec601YUVColours[GREEN_COLOUR].Y;
            txtU = g_rec601YUVColours[GREEN_COLOUR].U;
            txtV = g_rec601YUVColours[GREEN_COLOUR].V;
        }
        else
        {
            txtY = g_rec601YUVColours[ORANGE_COLOUR].Y;
            txtU = g_rec601YUVColours[ORANGE_COLOUR].U;
            txtV = g_rec601YUVColours[ORANGE_COLOUR].V;
        }
        box = 0;
    
        xPos = (width - osdd->progressBarOverlay.w) / 2 + PROGRESS_BAR_ENDS_WIDTH - osdd->osdProgressBarPointer->w / 2;
        if (frameInfo->sourceLength > 1) /* if source length == 1 then the pointer stays at the start */
        {
            xPos += (osdd->progressBarOverlay.w - 2 * PROGRESS_BAR_ENDS_WIDTH) * 
                frameInfo->position / frameInfo->sourceLength;
        }
        yPos = timecodeYPos + timecodeHeight + 20 - (osdd->osdProgressBarPointer->h - osdd->progressBarOverlay.h) - PROGRESS_BAR_TB_HEIGHT;
        if (haveSecondMarksModel)
        {
            CHK_ORET(add_overlay(osdd->osdLongProgressBarPointer, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
        }
        else
        {
            CHK_ORET(add_overlay(osdd->osdProgressBarPointer, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
        }
    }

    
    
    /* lock symbol */
    
    if (frameInfo->locked)
    {
        if (frameInfo->droppedFrame)
        {
            /* red */
            txtY = g_rec601YUVColours[RED_COLOUR].Y;
            txtU = g_rec601YUVColours[RED_COLOUR].U;
            txtV = g_rec601YUVColours[RED_COLOUR].V;
            box = 100;
        }
        else
        {
            /* white */
            txtY = g_rec601YUVColours[LIGHT_WHITE_COLOUR].Y;
            txtU = g_rec601YUVColours[LIGHT_WHITE_COLOUR].U;
            txtV = g_rec601YUVColours[LIGHT_WHITE_COLOUR].V;
            box = 100;
        }
    
        xPos = width - (width - osdd->progressBarOverlay.w) / 2 + 6;
        yPos = timecodeYPos + timecodeHeight + 20 + osdd->progressBarOverlay.h / 3 - osdd->osdLockSymbol->h / 2;
        CHK_ORET(add_overlay(osdd->osdLockSymbol, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
    }
    
    
    /* dropped frame overlay */
    
    if (frameInfo->droppedFrame)
    {
        /* red */
        txtY = g_rec601YUVColours[RED_COLOUR].Y;
        txtU = g_rec601YUVColours[RED_COLOUR].U;
        txtV = g_rec601YUVColours[RED_COLOUR].V;
        box = 100;
    
        xPos = (width - osdd->droppedFrameOverlay.w) / 2;
        yPos = timecodeYPos + timecodeHeight + 20 + osdd->progressBarOverlay.h + 10;
        CHK_ORET(add_overlay(&osdd->droppedFrameOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
    }
    
    
    /* audio level */

    /* check audio levels are present */
    hideAudioLevels = 1;
    if (!osdd->hideAudioLevels && osdd->state->numAudioLevels > 0)
    {
        for (i = 0; i < osdd->state->numAudioLevels; i++)
        {
            if (osdd->state->audioStreamLevels[i].level != osdd->state->nullAudioLevel)
            {
                hideAudioLevels = 0;
                break;
            }
        }
    }
    if (!hideAudioLevels && osdd->state->numAudioLevels > 0)
    {
        /* white */
        txtY = g_rec601YUVColours[LIGHT_WHITE_COLOUR].Y;
        txtU = g_rec601YUVColours[LIGHT_WHITE_COLOUR].U;
        txtV = g_rec601YUVColours[LIGHT_WHITE_COLOUR].V;
        box = 100;
    
        fill_audio_level_overlay(osdd);
        if (osdd->haveAudioOverlay)
        {
            xPos = 50;
            yPos = (height - osdd->audioLevelOverlay.h) / 2;
            CHK_ORET(add_overlay(&osdd->audioLevelOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
        }
        
        box = 0;

        /* 0 dBFS */
        xPos = 50 + osdd->audioLevelOverlay.w;
        yPos = (height - osdd->audioLevelOverlay.h) / 2 + AUDIO_LEVEL_MARGIN;
        CHK_ORET(add_overlay(&osdd->audioLevelM0Overlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);

        /* lineup level */
        yPos = (height - osdd->audioLevelOverlay.h) / 2 + AUDIO_LEVEL_MARGIN +
            (osdd->audioLevelOverlay.h - 2 * AUDIO_LEVEL_MARGIN) * (osdd->state->audioLineupLevel) / osdd->state->minimumAudioLevel;
        CHK_ORET(add_overlay(&osdd->audioLevelLineupOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
        
        /* -40 dBFS */
        yPos = (height - osdd->audioLevelOverlay.h) / 2 + AUDIO_LEVEL_MARGIN +
            (osdd->audioLevelOverlay.h - 2 * AUDIO_LEVEL_MARGIN) * (-40) / osdd->state->minimumAudioLevel;
        CHK_ORET(add_overlay(&osdd->audioLevelM40Overlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
        
        /* -60 dBFS */
        yPos = (height - osdd->audioLevelOverlay.h) / 2 + AUDIO_LEVEL_MARGIN +
            (osdd->audioLevelOverlay.h - 2 * AUDIO_LEVEL_MARGIN) * (-60) / osdd->state->minimumAudioLevel;
        CHK_ORET(add_overlay(&osdd->audioLevelM60Overlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
        
        /* -96 dBFS */
        yPos = (height - osdd->audioLevelOverlay.h) / 2 + AUDIO_LEVEL_MARGIN +
            (osdd->audioLevelOverlay.h - 2 * AUDIO_LEVEL_MARGIN) * (-96) / osdd->state->minimumAudioLevel;
        CHK_ORET(add_overlay(&osdd->audioLevelM96Overlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
    }
    
    
    /* field symbol */
    
    if (osdd->state->showFieldSymbol)
    {
        /* white */
        txtY = g_rec601YUVColours[LIGHT_WHITE_COLOUR].Y;
        txtU = g_rec601YUVColours[LIGHT_WHITE_COLOUR].U;
        txtV = g_rec601YUVColours[LIGHT_WHITE_COLOUR].V;
        box = 100;
    
        /* position on an odd line (second field) */
        xPos = timecodeXPos + timecodeWidth + 5;
        yPos = timecodeYPos + timecodeHeight / 2 - osdd->osdFieldSymbol->h / 2;
        CHK_ORET(add_overlay(osdd->osdFieldSymbol, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
    }
    

    /* labels */
    
    for (i = 0; i < MAX_OSD_LABELS; i++)
    {
        if (!osdd->cachedLabels[i].active || !osdd->cachedLabels[i].present)
        {
            continue;
        }

        txtY = g_rec601YUVColours[osdd->cachedLabels[i].label.colour].Y;
        txtU = g_rec601YUVColours[osdd->cachedLabels[i].label.colour].U;
        txtV = g_rec601YUVColours[osdd->cachedLabels[i].label.colour].V;
        box = osdd->cachedLabels[i].label.box;
        
        xPos = osdd->cachedLabels[i].label.xPos * osdd->imageWidth / osdd->cachedLabels[i].label.imageWidth - 
            osdd->cachedLabels[i].labelOverlay.w / 2;
        yPos = osdd->cachedLabels[i].label.yPos * osdd->imageHeight / osdd->cachedLabels[i].label.imageHeight - 
            osdd->cachedLabels[i].labelOverlay.h / 2;
        
        /* check overlay is within frame bounds */
        if (xPos < 0 || 
            yPos < 0 ||
            xPos + osdd->cachedLabels[i].labelOverlay.w >= osdd->imageWidth ||
            yPos + osdd->cachedLabels[i].labelOverlay.h >= osdd->imageHeight)
        {
            continue;
        }
        
        CHK_ORET(add_overlay(&osdd->cachedLabels[i].labelOverlay, &yuvFrame, xPos, yPos, txtY, txtU, txtV, box) == 0);
    }
    
    return 1;
}

static int source_info_screen(DefaultOnScreenDisplay* osdd, const FrameInfo* frameInfo, unsigned char* image, int width, int height)
{
    YUV_frame yuvFrame;
    int xPos, yPos;
    char txtY, txtU, txtV, box;
    float fontScale = width / 720.0;
   
    if (osdd->sourceInfoOverlay == NULL)
    {
        if (width < 20)
        {
            /* ridiculous width */
            return 0;
        }
        
        /* source info OSD screen (default) */
        CALLOC_ORET(osdd->sourceInfoOverlay, overlay, 1);
        if (text_to_overlay(&osdd->p_info, osdd->sourceInfoOverlay, 
            "No source information avaliable", 
            width - 20, 0,
            0, 0,
            0,
            0,
            0,
            "Ariel", 28 * fontScale, osdd->aspectRatio.num, osdd->aspectRatio.den) < 0)
        {
            /* don't say anything if fails */
            SAFE_FREE(&osdd->sourceInfoOverlay);
            return 0;
        }
    }
    
    /* initialise the YUV lib frame */    
    CHK_ORET(init_yuv_frame(osdd, &yuvFrame, image, width, height) == 1);

    
    /* white */
    txtY = g_rec601YUVColours[LIGHT_WHITE_COLOUR].Y;
    txtU = g_rec601YUVColours[LIGHT_WHITE_COLOUR].U;
    txtV = g_rec601YUVColours[LIGHT_WHITE_COLOUR].V;
    box = 80;

    xPos = (width - osdd->sourceInfoOverlay->w) / 2;
    xPos = (xPos < 0) ? 0 : xPos;
    yPos = (height < 20) ? 0 : 20;
    
    CHK_ORET(add_overlay(osdd->sourceInfoOverlay, &yuvFrame, xPos, yPos,
        txtY, txtU, txtV, box) == YUV_OK);
    
    return 1;
}

static int add_menu_screen(DefaultOnScreenDisplay* osdd, const FrameInfo* frameInfo, 
    unsigned char* image, int width, int height)
{
    YUV_frame yuvFrame;
    int xPos, yPos;
    char txtY, txtU, txtV, box;
    OSDMenuListItem* item;
    OSDMenuModelInt* menuInt;
    OSDMenuListItemInt* itemInt;
    int menuItemIndex;
    overlay modSelectionOverlay;
    unsigned char* bufPtr;
    int i;
    int numMenuItemsShown;
    void* privateData = NULL;
    float fontScale = width / 720.0;


    /* initialise the YUV lib frame */    
    CHK_ORET(init_yuv_frame(osdd, &yuvFrame, image, width, height) == 1);

    
    PTHREAD_MUTEX_LOCK(&osdd->setMenuModelMutex) /* prevent change in osdd->menu value */
    
    if (osdd->menu == NULL)
    {
        goto fail;
    }
    osdm_lock(osdd->menu); /* prevent changes to the menu model */

    /* get or create OSD internal data */
    menuInt = (OSDMenuModelInt*)osdm_get_menu_private_data(osdd->menu, (long)osdd);
    if (menuInt == NULL)
    {
        CALLOC_OFAIL(privateData, OSDMenuModelInt, 1);
        CHK_OFAIL(osdm_set_menu_private_data(osdd->menu, (long)osdd, privateData, osdd_free_menu_int));
        menuInt = (OSDMenuModelInt*)privateData;
        privateData = NULL;

        /* TODO: calc according to image dimensions and font sizes used */ 
        menuInt->lineWidth = 32;
        menuInt->numLines = 12;
    }
    

    /* title */
    
    if ((osdd->menu->titleUpdated & osdd->menuUpdateMask) &&
        menuInt->titleOverlay.buff != NULL)
    {
        free_overlay(&menuInt->titleOverlay);
    }
    
    if (menuInt->titleOverlay.buff == NULL)
    {
        if (text_to_overlay(&osdd->p_info, &menuInt->titleOverlay, 
            (osdd->menu->title == NULL) ? "" : osdd->menu->title, 
            width - 20, width - 20,
            0, 8,
            1,
            0,
            0,
            "Ariel", 32 * fontScale, osdd->aspectRatio.num, osdd->aspectRatio.den) < 0)
        {
            ml_log_error("Failed to create text overlay\n");
            goto fail;
        }
    }

    txtY = g_rec601YUVColours[LIGHT_WHITE_COLOUR].Y;
    txtU = g_rec601YUVColours[LIGHT_WHITE_COLOUR].U;
    txtV = g_rec601YUVColours[LIGHT_WHITE_COLOUR].V;
    box = 100;

    xPos = MENU_OUTER_MARGIN;
    yPos = MENU_OUTER_MARGIN;
    
    CHK_OFAIL(add_overlay(&menuInt->titleOverlay, &yuvFrame, xPos, yPos,
        txtY, txtU, txtV, box) == YUV_OK);

    yPos += menuInt->titleOverlay.h;
    
    osdd->menu->titleUpdated &= ~osdd->menuUpdateMask;
    

    /* status */
    
    if ((osdd->menu->statusUpdated & osdd->menuUpdateMask) &&
        menuInt->statusOverlay.buff != NULL)
    {
        free_overlay(&menuInt->statusOverlay);
    }
    
    if (menuInt->statusOverlay.buff == NULL)
    {
        if (text_to_overlay(&osdd->p_info, &menuInt->statusOverlay, 
            (osdd->menu->status == NULL) ? "" : osdd->menu->status, 
            width - 20, width - 20,
            10, 2,
            0,
            0,
            1,
            "Ariel", 28 * fontScale, osdd->aspectRatio.num, osdd->aspectRatio.den) < 0)
        {
            ml_log_error("Failed to create text overlay\n");
            goto fail;
        }
    }

    txtY = g_rec601YUVColours[LIGHT_WHITE_COLOUR].Y;
    txtU = g_rec601YUVColours[LIGHT_WHITE_COLOUR].U;
    txtV = g_rec601YUVColours[LIGHT_WHITE_COLOUR].V;
    box = 100;

    CHK_OFAIL(add_overlay(&menuInt->statusOverlay, &yuvFrame, xPos, yPos,
        txtY, txtU, txtV, box) == YUV_OK);

        
    yPos += menuInt->statusOverlay.h;

    osdd->menu->statusUpdated &= ~osdd->menuUpdateMask;
    

    
    /* comment */
    
    if ((osdd->menu->commentUpdated & osdd->menuUpdateMask) &&
        menuInt->commentOverlay.buff != NULL)
    {
        free_overlay(&menuInt->commentOverlay);
    }
    
    if (osdd->menu->comment != NULL)
    {
        if (menuInt->commentOverlay.buff == NULL)
        {
            if (text_to_overlay(&osdd->p_info, &menuInt->commentOverlay, 
                osdd->menu->comment, 
                width - 20, width - 20,
                10, 2,
                0,
                30,
                1,
                "Ariel", 28 * fontScale, osdd->aspectRatio.num, osdd->aspectRatio.den) < 0)
            {
                ml_log_error("Failed to create text overlay\n");
                goto fail;
            }
        }
    
        txtY = g_rec601YUVColours[LIGHT_WHITE_COLOUR].Y;
        txtU = g_rec601YUVColours[LIGHT_WHITE_COLOUR].U;
        txtV = g_rec601YUVColours[LIGHT_WHITE_COLOUR].V;
        box = 100;
    
        CHK_OFAIL(add_overlay(&menuInt->commentOverlay, &yuvFrame, xPos, yPos,
            txtY, txtU, txtV, box) == YUV_OK);
    
        yPos += menuInt->commentOverlay.h;
    }

    osdd->menu->commentUpdated &= ~osdd->menuUpdateMask;
    
    
    
    /* selection overlay for highlighting list items */

    if (menuInt->selectionOverlay.buff == NULL)
    {
        menuInt->selectionOverlay.w = (width - MENU_OUTER_MARGIN * 2);
        /* we assume the title height always >= list item height */
        menuInt->selectionOverlay.h = menuInt->titleOverlay.h - MENU_OUTER_MARGIN + 4;
        CALLOC_OFAIL(menuInt->selectionOverlay.buff, unsigned char, menuInt->selectionOverlay.w * menuInt->selectionOverlay.h * 2);
        menuInt->selectionOverlay.Cbuff = NULL;
        
        bufPtr = menuInt->selectionOverlay.buff + MENU_OUTER_MARGIN;
        for (i = 0; i < menuInt->selectionOverlay.h; i++)
        {
            memset(bufPtr, 255, menuInt->selectionOverlay.w - MENU_OUTER_MARGIN * 2);
            bufPtr += menuInt->selectionOverlay.w;
        }
    }
    
    
    /* list items */
        
    /* adjust top line to 0 if all items fit on screen */
    if (menuInt->topLine + osdd->menu->numItems < menuInt->numLines)
    {
        menuInt->topLine = 0;
    }
    /* adjust top line if current item before it */
    else if (menuInt->topLine > osdd->menu->currentItemIndex)
    {
        menuInt->topLine = osdd->menu->currentItemIndex;
    }
    /* adjust top line if current item is beyond screen limit */
    else if (menuInt->topLine + menuInt->numLines - 1 < osdd->menu->currentItemIndex)
    {
        menuInt->topLine = osdd->menu->currentItemIndex - menuInt->numLines + 1;
    }
    
    menuItemIndex = 0;
    item = osdm_get_list_item(osdd->menu, menuInt->topLine);
    if (item != NULL)
    {
        menuItemIndex = menuInt->topLine;
        numMenuItemsShown = 0;
        while (item != NULL && numMenuItemsShown < menuInt->numLines)
        {
            itemInt = osdm_get_item_private_data(item, (long)osdd);
            
            /* create internal item data if it does not exist */
            if (itemInt == NULL)
            {
                /* check we can set private data */
                CHK_OFAIL(osdm_set_item_private_data(item, (long)osdd, NULL, osdd_free_menu_list_item_int));
                
                /* set private data */
                CALLOC_OFAIL(itemInt, OSDMenuListItemInt, 1);
                CHK_OFAIL(osdm_set_item_private_data(item, (long)osdd, itemInt, osdd_free_menu_list_item_int));
            }
            
            if ((item->textUpdated & osdd->menuUpdateMask) &&
                itemInt->textOverlay.buff != NULL)
            {
                free_overlay(&itemInt->textOverlay);
            }
            
            if (itemInt->textOverlay.buff == NULL)
            {
                if (text_to_overlay(&osdd->p_info, &itemInt->textOverlay, 
                    (item->text == NULL) ? "" : item->text, 
                    width - MENU_OUTER_MARGIN * 2, width - MENU_OUTER_MARGIN * 2,
                    10, 2,
                    0,
                    30,
                    1,
                    "Ariel", 28 * fontScale, osdd->aspectRatio.num, osdd->aspectRatio.den) < 0)
                {
                    ml_log_error("Failed to create text overlay\n");
                    goto fail;
                }
            }

            if (menuItemIndex == osdd->menu->currentItemIndex)
            {
                txtY = g_rec601YUVColours[ORANGE_COLOUR].Y;
                txtU = g_rec601YUVColours[ORANGE_COLOUR].U;
                txtV = g_rec601YUVColours[ORANGE_COLOUR].V;
                box = 100;
    
                /* modify the selection overlay vertical dimensions to match the line */
                modSelectionOverlay = menuInt->selectionOverlay;
                if (modSelectionOverlay.h > itemInt->textOverlay.h)
                {
                    modSelectionOverlay.buff += modSelectionOverlay.w * 
                        (modSelectionOverlay.h - itemInt->textOverlay.h);
                    modSelectionOverlay.h = itemInt->textOverlay.h;
                }
                    
                CHK_OFAIL(add_overlay(&modSelectionOverlay, &yuvFrame, xPos, yPos,
                    txtY, txtU, txtV, box) == YUV_OK);
            }
    
            if (item->state == MENU_ITEM_DISABLED)
            {
                txtY = g_rec601YUVColours[YELLOW_COLOUR].Y;
                txtU = g_rec601YUVColours[YELLOW_COLOUR].U;
                txtV = g_rec601YUVColours[YELLOW_COLOUR].V;
            }
            else if (item->state == MENU_ITEM_HIGHLIGHTED)
            {
                txtY = g_rec601YUVColours[GREEN_COLOUR].Y;
                txtU = g_rec601YUVColours[GREEN_COLOUR].U;
                txtV = g_rec601YUVColours[GREEN_COLOUR].V;
            }
            else
            {
                txtY = g_rec601YUVColours[LIGHT_WHITE_COLOUR].Y;
                txtU = g_rec601YUVColours[LIGHT_WHITE_COLOUR].U;
                txtV = g_rec601YUVColours[LIGHT_WHITE_COLOUR].V;
            }
            if (menuItemIndex == osdd->menu->currentItemIndex)
            {
                box = 0;
            }
            else
            {
                box = 100;
            }
        
            CHK_OFAIL(add_overlay(&itemInt->textOverlay, &yuvFrame, xPos, yPos,
                txtY, txtU, txtV, box) == YUV_OK);
            yPos += itemInt->textOverlay.h;
            
            
            item->textUpdated &= ~osdd->menuUpdateMask;
        
            menuItemIndex++;
            numMenuItemsShown++;
            item = item->next;
        }
    }
        

    /* bottom margin */

    if (menuInt->bottomMarginOverlay.buff == NULL)
    {
        menuInt->bottomMarginOverlay.w = (width - MENU_OUTER_MARGIN * 2);
        menuInt->bottomMarginOverlay.h = MENU_BOTTOM_MARGIN;
        CALLOC_OFAIL(menuInt->bottomMarginOverlay.buff, unsigned char, menuInt->bottomMarginOverlay.w * menuInt->bottomMarginOverlay.h * 2);
        menuInt->bottomMarginOverlay.Cbuff = NULL;
        memset(menuInt->bottomMarginOverlay.buff, 0, menuInt->bottomMarginOverlay.w * menuInt->bottomMarginOverlay.h * 2);
    }
    
    /* add thin line to indicate the end of list has been reached */
    if (menuItemIndex == osdd->menu->numItems)
    {
        bufPtr = menuInt->bottomMarginOverlay.buff + menuInt->bottomMarginOverlay.w * 2 +
            menuInt->bottomMarginOverlay.w * 7 / 8;
        memset(bufPtr, 255, menuInt->bottomMarginOverlay.w / 8);
        bufPtr += menuInt->bottomMarginOverlay.w;
        memset(bufPtr, 255, menuInt->bottomMarginOverlay.w / 8);
    }
    else
    {
        bufPtr = menuInt->bottomMarginOverlay.buff + menuInt->bottomMarginOverlay.w * 2 +
            menuInt->bottomMarginOverlay.w * 7 / 8;
        memset(bufPtr, 0, menuInt->bottomMarginOverlay.w / 8);
        bufPtr += menuInt->bottomMarginOverlay.w;
        memset(bufPtr, 0, menuInt->bottomMarginOverlay.w / 8);
    }
    
    if (yPos + menuInt->bottomMarginOverlay.h < height)
    {
        txtY = g_rec601YUVColours[LIGHT_WHITE_COLOUR].Y;
        txtU = g_rec601YUVColours[LIGHT_WHITE_COLOUR].U;
        txtV = g_rec601YUVColours[LIGHT_WHITE_COLOUR].V;
        box = 100;
    
        xPos = MENU_OUTER_MARGIN;
        
        CHK_OFAIL(add_overlay(&menuInt->bottomMarginOverlay, &yuvFrame, xPos, yPos,
            txtY, txtU, txtV, box) == YUV_OK);

        yPos += menuInt->bottomMarginOverlay.h;
    
    }
        
    
    osdm_unlock(osdd->menu);
    PTHREAD_MUTEX_UNLOCK(&osdd->setMenuModelMutex)
    
    return 1;
    
fail:
    if (osdd->menu != NULL)
    {
        osdm_unlock(osdd->menu);
    }
    SAFE_FREE(&privateData);
    PTHREAD_MUTEX_UNLOCK(&osdd->setMenuModelMutex)
    return 0;
}

static int get_progress_bar_width(int imageWidth)
{
    int width;
    
    width = imageWidth * 8 / 10;
    width += PROGRESS_BAR_MARK_WIDTH - (width % PROGRESS_BAR_MARK_WIDTH); /* ensure that a series of marks PROGRESS_BAR_MARK_WIDTH pixels wide fit exactly */
    width += 2 * PROGRESS_BAR_ENDS_WIDTH;
    
    return width;
}

static int osdd_initialise(void* data, const StreamInfo* streamInfo, const Rational* aspectRatio)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    int i;
    char line[MAX_SOURCE_INFO_LINE_WIDTH + 1];
    char text[MAX_SOURCE_INFO_LINE_WIDTH * MAX_SOURCE_INFO_LINES + 1];
    int numValues;
    int wasInitialised = osdd->initialised;
    unsigned char* bufPtr;
    float fontScale = streamInfo->height / 576.0;
    
    if (fontScale > 1.5)
    {
        fontScale = 1.5;
    }
    
    if (streamInfo->width < 20)
    {
        /* ridiculous width */
        ml_log_error("Picture width too small for OSD\n");
        return 0;
    }
    
    if (streamInfo->format != UYVY_FORMAT &&
        streamInfo->format != YUV422_FORMAT &&
        streamInfo->format != YUV420_FORMAT)
    {
        ml_log_error("Picture format not (yet) supported for OSD\n");
        return 0;
    }
    
    osdd->initialised = 0;
    
    osdd->videoFormat = streamInfo->format;
    osdd->imageWidth = streamInfo->width;
    osdd->imageHeight = streamInfo->height;
    osdd->aspectRatio = *aspectRatio;
    
    
    /* initialise the play state screen overlay */
    
    if (wasInitialised)
    {
        free_timecode(&osdd->timecodeTextData);
        free_char_set(&osdd->timecodeTypeData);
        free_char_set(&osdd->numberData);
        free_overlay(&osdd->markOverlay);
        free_overlay(&osdd->droppedFrameOverlay);
    }
    
    /* note: the order must match UNKNOWN_TC_OVLY_IDX, ... */
    CHK_ORET(char_set_to_overlay(&osdd->p_info, &osdd->timecodeTypeData, "?CSVLX",
        "Ariel", 48 * fontScale, aspectRatio->num, aspectRatio->den) >= 0);
    
    CHK_ORET(init_timecode(&osdd->p_info, &osdd->timecodeTextData, "Ariel", 
        48 * fontScale, aspectRatio->num, aspectRatio->den) == YUV_OK);
    
    CHK_ORET(char_set_to_overlay(&osdd->p_info, &osdd->numberData, "0123456789",
        "Ariel", 34 * fontScale, aspectRatio->num, aspectRatio->den) >= 0);

    /* the message if "FRAME REPEAT" instead of "FRAME DROPPED" because the DVS card repeats the last frame
    if the next frame misses the time slot for output */
    CHK_ORET(text_to_overlay(&osdd->p_info, &osdd->droppedFrameOverlay, "FRAME REPEAT!",
        osdd->imageWidth, 0, 0, 0, 0, 0, 0,
        "Ariel", 32 * fontScale, aspectRatio->num, aspectRatio->den) >= 0);

        
    CALLOC_ORET(osdd->markOverlay.buff, unsigned char, MARK_OVERLAY_WIDTH * MARK_OVERLAY_HEIGHT * 2);
    osdd->markOverlay.Cbuff = NULL;
    osdd->markOverlay.w = MARK_OVERLAY_WIDTH;
    osdd->markOverlay.h = MARK_OVERLAY_HEIGHT;
    if (osdd->videoFormat == UYVY_FORMAT)
    {
        osdd->markOverlay.ssx = 2;
        osdd->markOverlay.ssy = 1;
    }
    else if (osdd->videoFormat == YUV422_FORMAT)
    {
        osdd->markOverlay.ssx = 2;
        osdd->markOverlay.ssy = 1;
    }
    else /* YUV420 */
    {
        osdd->markOverlay.ssx = 2;
        osdd->markOverlay.ssy = 2;
    }
    bufPtr = osdd->markOverlay.buff + MARK_OVERLAY_WIDTH * MARK_OVERLAY_MARGIN + MARK_OVERLAY_MARGIN;
    for (i = 0; i < MARK_OVERLAY_HEIGHT - 2 * MARK_OVERLAY_MARGIN; i++)
    {
        memset(bufPtr, 255, MARK_OVERLAY_WIDTH - 2 * MARK_OVERLAY_MARGIN);
        bufPtr += MARK_OVERLAY_WIDTH;        
    }

    if (streamInfo->width < 720 * 3 / 5)
    {
        osdd->osdPlaySymbol = &g_osdPlaySymbolSmall;
        osdd->osdFastForwardSymbol = &g_osdFastForwardSymbolSmall;
        osdd->osdFastRewindSymbol = &g_osdFastRewindSymbolSmall;
        osdd->osdPauseSymbol = &g_osdPauseSymbolSmall;
        osdd->osdReversePlaySymbol = &g_osdReversePlaySymbolSmall; 
        osdd->osdFieldSymbol = &g_osdFieldSymbol; 
        osdd->osdProgressBarPointer = &g_osdProgressBarPointer; 
        osdd->osdLongProgressBarPointer = &g_osdLongProgressBarPointer; 
        osdd->osdLockSymbol = &g_osdLockSymbol;
    }
    else
    {
        osdd->osdPlaySymbol = &g_osdPlaySymbol;
        osdd->osdFastForwardSymbol = &g_osdFastForwardSymbol;
        osdd->osdFastRewindSymbol = &g_osdFastRewindSymbol;
        osdd->osdPauseSymbol = &g_osdPauseSymbol;
        osdd->osdReversePlaySymbol = &g_osdReversePlaySymbol; 
        osdd->osdFieldSymbol = &g_osdFieldSymbol; 
        osdd->osdProgressBarPointer = &g_osdProgressBarPointer; 
        osdd->osdLongProgressBarPointer = &g_osdLongProgressBarPointer; 
        osdd->osdLockSymbol = &g_osdLockSymbol;
    }
    
        
    /* initialise source info screen overlay */
    
    if (osdd->sourceInfoOverlay != NULL)
    {
        free_overlay(osdd->sourceInfoOverlay);
        SAFE_FREE(&osdd->sourceInfoOverlay);
    }

    numValues = (streamInfo->numSourceInfoValues > MAX_SOURCE_INFO_LINES) ? 
        MAX_SOURCE_INFO_LINES : streamInfo->numSourceInfoValues;
    text[0] = '\0';
    for (i = 0; i < numValues; i++)
    {
        if (i != 0)
        {
            strcat(text, "\n");
        }
        snprintf(line, MAX_SOURCE_INFO_LINE_WIDTH, "%s: %s", streamInfo->sourceInfoValues[i].name, 
            streamInfo->sourceInfoValues[i].value);
        strcat(text, line);
    }
    
    CALLOC_ORET(osdd->sourceInfoOverlay, overlay, 1);
    if (ml_text_to_ovly(&osdd->p_info, osdd->sourceInfoOverlay, text, 
        streamInfo->width - (2 * 24), "Ariel", 28 * fontScale, 10, aspectRatio->num, aspectRatio->den) < 0)
    {
        ml_log_error("Failed to create OSD source info screen overlay\n");
        SAFE_FREE(&osdd->sourceInfoOverlay);
        return 0;
    }

    
    /* initialise progress bar */
    
    if (wasInitialised)
    {
        free_overlay(&osdd->progressBarOverlay);
        free_overlay(&osdd->userMarksOverlay);
        free_overlay(&osdd->progressBarMarkOverlay);
        free_overlay(&osdd->secondUserMarksOverlay);
    }

    osdd->progressBarOverlay.w = get_progress_bar_width(streamInfo->width);
    osdd->progressBarOverlay.h = PROGRESS_BAR_HEIGHT;
    osdd->progressBarMarkOverlay.w = get_progress_bar_width(streamInfo->width);
    osdd->progressBarMarkOverlay.h = PROGRESS_BAR_MARKS_HEIGHT;
    osdd->userMarksOverlay.w = osdd->progressBarOverlay.w - 2 * PROGRESS_BAR_ENDS_WIDTH + PROGRESS_BAR_MARK_WIDTH;
    osdd->userMarksOverlay.h = PROGRESS_BAR_MARK_HEIGHT;
    osdd->secondUserMarksOverlay.w = osdd->progressBarMarkOverlay.w - 2 * PROGRESS_BAR_ENDS_WIDTH + PROGRESS_BAR_MARK_WIDTH;
    osdd->secondUserMarksOverlay.h = PROGRESS_BAR_MARK_HEIGHT;
    if (osdd->videoFormat == UYVY_FORMAT)
    {
        osdd->progressBarOverlay.ssx = 2;
        osdd->progressBarOverlay.ssy = 1;
        osdd->progressBarMarkOverlay.ssx = 2;
        osdd->progressBarMarkOverlay.ssy = 1;
        osdd->userMarksOverlay.ssx = 2;
        osdd->userMarksOverlay.ssy = 1;
        osdd->secondUserMarksOverlay.ssx = 2;
        osdd->secondUserMarksOverlay.ssy = 1;
    }
    else if (osdd->videoFormat == YUV422_FORMAT)
    {
        osdd->progressBarOverlay.ssx = 2;
        osdd->progressBarOverlay.ssy = 1;
        osdd->progressBarMarkOverlay.ssx = 2;
        osdd->progressBarMarkOverlay.ssy = 1;
        osdd->userMarksOverlay.ssx = 2;
        osdd->userMarksOverlay.ssy = 1;
        osdd->secondUserMarksOverlay.ssx = 2;
        osdd->secondUserMarksOverlay.ssy = 1;
    }
    else /* YUV420 */
    {
        osdd->progressBarOverlay.ssx = 2;
        osdd->progressBarOverlay.ssy = 2;
        osdd->progressBarMarkOverlay.ssx = 2;
        osdd->progressBarMarkOverlay.ssy = 2;
        osdd->userMarksOverlay.ssx = 2;
        osdd->userMarksOverlay.ssy = 2;
        osdd->secondUserMarksOverlay.ssx = 2;
        osdd->secondUserMarksOverlay.ssy = 2;
    }
    
    CALLOC_ORET(osdd->progressBarOverlay.buff, unsigned char, 
        osdd->progressBarOverlay.w * osdd->progressBarOverlay.h * 2);
    osdd->progressBarOverlay.Cbuff = NULL;
    CALLOC_ORET(osdd->userMarksOverlay.buff, unsigned char, 
        osdd->userMarksOverlay.w * osdd->userMarksOverlay.h * 2);
    osdd->userMarksOverlay.Cbuff = NULL;
    CALLOC_ORET(osdd->progressBarMarkOverlay.buff, unsigned char, 
        osdd->progressBarMarkOverlay.w * osdd->progressBarMarkOverlay.h * 2);
    osdd->progressBarMarkOverlay.Cbuff = NULL;
    CALLOC_ORET(osdd->secondUserMarksOverlay.buff, unsigned char, 
        osdd->secondUserMarksOverlay.w * osdd->secondUserMarksOverlay.h * 2);
    osdd->secondUserMarksOverlay.Cbuff = NULL;

    bufPtr = osdd->progressBarOverlay.buff;
    for (i = 0; i < PROGRESS_BAR_HEIGHT; i++)
    {
        if (i < PROGRESS_BAR_TB_HEIGHT)
        {
            memset(bufPtr, 255, osdd->progressBarOverlay.w);
        }
        else
        {
            memset(bufPtr, 255, PROGRESS_BAR_ENDS_WIDTH);
            memset(bufPtr + osdd->progressBarOverlay.w - PROGRESS_BAR_ENDS_WIDTH, 255, PROGRESS_BAR_ENDS_WIDTH);
        }
        bufPtr += osdd->progressBarOverlay.w;
    }
    
    memset(osdd->progressBarMarkOverlay.buff, 255, PROGRESS_BAR_MARKS_HEIGHT * osdd->progressBarMarkOverlay.w);
    
    
    
    
    osdd->initialised = 1;
    return 1;
}



static void osdd_set_listener(void* data, OSDListener* listener)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    osdd->listener = listener;
}

static void osdd_free_menu_model(void* data, OSDMenuModel** menu)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    OSDMenuListItem* item;
    OSDMenuListItem* nextItem;
    
    if (*menu == NULL)
    {
        return;
    }
    
    PTHREAD_MUTEX_LOCK(&osdd->setMenuModelMutex)
    if (osdd->menu == *menu)
    {
        osdd->menu = NULL;
    }
    PTHREAD_MUTEX_UNLOCK(&osdd->setMenuModelMutex)

    SAFE_FREE(&(*menu)->title);
    SAFE_FREE(&(*menu)->status);
    SAFE_FREE(&(*menu)->comment);
    
    item = (*menu)->items;
    while (item != NULL)
    {
        nextItem = item->next;
        
        SAFE_FREE(&item->text);
        osdm_free_item_private_data(item);
        SAFE_FREE(&item);
        
        item = nextItem;
    }
    
    osdm_free_menu_private_data(*menu);
    
    destroy_mutex(&(*menu)->menuModelMutex);
    
    SAFE_FREE(menu);
}

static int osdd_create_menu_model(void* data, OSDMenuModel** menu)
{
    OSDMenuModel* newMenu = NULL;
        
    CALLOC_OFAIL(newMenu, OSDMenuModel, 1);
    
    CHK_OFAIL(init_mutex(&newMenu->menuModelMutex));
    
    *menu = newMenu;
    return 1;
    
fail:
    osdd_free_menu_model(data, &newMenu);
    return 0;
}

static void osdd_set_active_menu_model(void* data, int updateMask, OSDMenuModel* menu)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;

    PTHREAD_MUTEX_LOCK(&osdd->setMenuModelMutex)
    osdd->menu = menu;
    osdd->menuUpdateMask = updateMask;
    PTHREAD_MUTEX_UNLOCK(&osdd->setMenuModelMutex)
}

static int osdd_set_screen(void* data, OSDScreen screen)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    return osd_set_screen(osds_get_osd(osdd->state), screen);
}

static int osdd_next_screen(void* data)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    return osd_next_screen(osds_get_osd(osdd->state));
}

static int osdd_get_screen(void* data, OSDScreen* screen)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    return osd_get_screen(osds_get_osd(osdd->state), screen);
}

static int osdd_set_timecode(void* data, int index, int type, int subType)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    return osd_set_timecode(osds_get_osd(osdd->state), index, type, subType);
}

static int osdd_next_timecode(void* data)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    return osd_next_timecode(osds_get_osd(osdd->state));
}

static int osdd_set_play_state(void* data, OSDPlayState state, int value)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    return osd_set_play_state(osds_get_osd(osdd->state), state, value);
}

static int osdd_set_state(void* data, const OnScreenDisplayState* state)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    return osd_set_state(osds_get_osd(osdd->state), state);
}

static void osdd_set_minimum_audio_stream_level(void* data, double level)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    osd_set_minimum_audio_stream_level(osds_get_osd(osdd->state), level);
}

static void osdd_set_audio_lineup_level(void* data, float level)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    osd_set_audio_lineup_level(osds_get_osd(osdd->state), level);
}

static void osdd_reset_audio_stream_levels(void* data)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    osd_reset_audio_stream_levels(osds_get_osd(osdd->state));
}

static int osdd_register_audio_stream(void* data, int streamId)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    return osd_register_audio_stream(osds_get_osd(osdd->state), streamId);
}

static void osdd_set_audio_stream_level(void* data, int streamId, double level)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    osd_set_audio_stream_level(osds_get_osd(osdd->state), streamId, level);
}

static void osdd_set_audio_level_visibility(void* data, int visible)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    osdd->hideAudioLevels = !visible;
}

static void osdd_toggle_audio_level_visibility(void* data)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    osdd->hideAudioLevels = !osdd->hideAudioLevels;
}

static void osdd_show_field_symbol(void* data, int enable)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    osd_show_field_symbol(osds_get_osd(osdd->state), enable);
}

static void osdd_set_mark_display(void* data, const MarkConfigs* markConfigs)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    osd_set_mark_display(osds_get_osd(osdd->state), markConfigs);
}

static void osdd_free_marks_model(void* data, OSDMarksModel** model)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    if (*model == NULL)
    {
        return;
    }
    
    PTHREAD_MUTEX_LOCK(&osdd->setMarksModelMutex)
    if (osdd->marksModel == *model)
    {
        osdd->marksModel = NULL;
    }
    PTHREAD_MUTEX_UNLOCK(&osdd->setMarksModelMutex)
    
    destroy_mutex(&(*model)->marksMutex);
    
    SAFE_FREE(&(*model)->markCounts);
    
    SAFE_FREE(model);
}

static int osdd_create_marks_model(void* data, OSDMarksModel** model)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    OSDMarksModel* newModel = NULL;
    
    CALLOC_ORET(newModel, OSDMarksModel, 1);
    
    newModel->numMarkCounts = get_progress_bar_width(osdd->imageWidth) - 2 * PROGRESS_BAR_ENDS_WIDTH;
    CALLOC_OFAIL(newModel->markCounts, int, newModel->numMarkCounts);
    
    CHK_OFAIL(init_mutex(&newModel->marksMutex));
    
    *model = newModel;
    return 1;
    
fail:
    osdd_free_marks_model(data, &newModel);
    return 0;
}

static void osdd_set_marks_model(void* data, int updateMask, OSDMarksModel* model)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    PTHREAD_MUTEX_LOCK(&osdd->setMarksModelMutex)
    osdd->marksModel = model;
    osdd->marksUpdateMask = updateMask;
    PTHREAD_MUTEX_UNLOCK(&osdd->setMarksModelMutex)
}

static void osdd_set_second_marks_model(void* data, int updateMask, OSDMarksModel* model)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    PTHREAD_MUTEX_LOCK(&osdd->setMarksModelMutex)
    osdd->secondMarksModel = model;
    osdd->secondMarksUpdateMask = updateMask;
    PTHREAD_MUTEX_UNLOCK(&osdd->setMarksModelMutex)
}

static void osdd_set_progress_bar_visibility(void* data, int visible)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    osdd->hideProgressBar = !visible;
}

static float osdd_get_position_in_progress_bar(void* data, int x, int y)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    if (osdd->hideProgressBar || 
        osdd->progressBarOverlay.w <= 0 || osdd->progressBarOverlay.h <= 0 ||
        osdd->timecodeTextData.height <= 0)
    {
        return -1.0;
    }

    /* TODO: need to keep this in sync with the positioning and size of the progress bar */

    float position;
    
    int progressBarXPos = (osdd->imageWidth - osdd->progressBarOverlay.w) / 2;
    int progressBarYPos = (osdd->imageHeight * 13) / 16 - osdd->timecodeTextData.height / 2 + 
        osdd->timecodeTextData.height + 20 + osdd->progressBarOverlay.h / 2;
    if (x >= progressBarXPos - PROGRESS_BAR_REGION_X_MARGIN && 
        x <= progressBarXPos + osdd->progressBarOverlay.w + PROGRESS_BAR_REGION_X_MARGIN &&
        y >= progressBarYPos - PROGRESS_BAR_REGION_Y_MARGIN && 
        y <= progressBarYPos + osdd->progressBarOverlay.h + PROGRESS_BAR_REGION_Y_MARGIN)
    {
        position = 100.0 * (x - (progressBarXPos + PROGRESS_BAR_ENDS_WIDTH)) / 
            (float)(osdd->progressBarOverlay.w - 2 * PROGRESS_BAR_ENDS_WIDTH);
        if (position < 0.0)
        {
            position = 0.0;
        }
        else if (position >= 100.0)
        {
            position = 100.0 - 1.0 / (float)(osdd->progressBarOverlay.w - 2 * PROGRESS_BAR_ENDS_WIDTH);
        }
    }
    else
    {
        position = -1.0;
    }
    
    return position;
}

static void osdd_highlight_progress_bar_pointer(void* data, int on)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    if (osdd->highlightProgressPointer != on)
    {
        if (on)
        {
            osdd->highlightProgressPointerTickStart = osdd->halfSecTickCount;
        }
        else
        {
            osdd->highlightProgressPointerTickStart = 0;
        }
    }
    osdd->highlightProgressPointer = on;
}

static void osdd_set_active_progress_bar_marks(void* data, int index)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    PTHREAD_MUTEX_LOCK(&osdd->setMarksModelMutex)
    if (osdd->secondMarksModel != NULL)
    {
        osdd->activeProgressBarMarks = index;
    }
    else
    {
        osdd->activeProgressBarMarks = 0;
    }
    PTHREAD_MUTEX_UNLOCK(&osdd->setMarksModelMutex)
}

static void osdd_set_label(void* data, int xPos, int yPos, int imageWidth, int imageHeight, 
    int fontSize, Colour colour, int box, const char* label)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    osd_set_label(osds_get_osd(osdd->state), xPos, yPos, imageWidth, imageHeight, fontSize, colour, box, label);
}

static int osdd_add_to_image(void* data, const FrameInfo* frameInfo, unsigned char* image, 
    int width, int height)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    int result;
    
    if (!osdd->initialised)
    {
        ml_log_error("Failed to add image to unintialised OSD\n");
        return 0;
    }
    
    /* complete the state with the current frame info */
    osds_complete(osdd->state, frameInfo);
    
    /* complete the labels */
    complete_labels(osdd);

    /* complete the state screen selection */
    if (!osdd->state->screenSet && osdd->state->nextScreen)
    {
        osdd->state->screen = (osdd->state->screen + 1 > OSD_EMPTY_SCREEN) ? 0 : osdd->state->screen + 1;
    }
    osdd->state->screenSet = 0;
    osdd->state->nextScreen = 0;
    
    /* take into account that menu screen is optionally present */
    if (osdd->state->screen == OSD_MENU_SCREEN && osdd->menu == NULL)
    {
        osdd->state->screen = (osdd->state->screen + 1 > OSD_EMPTY_SCREEN) ? 0 : osdd->state->screen + 1;
    }
    
    
    switch (osdd->state->screen)
    {
        case OSD_SOURCE_INFO_SCREEN:
            result = source_info_screen(osdd, frameInfo, image, width, height);
            break;
            
        case OSD_PLAY_STATE_SCREEN:
            result = add_play_state_screen(osdd, frameInfo, image, width, height);
            break;

        case OSD_MENU_SCREEN:
            result = add_menu_screen(osdd, frameInfo, image, width, height);
            break;

        case OSD_EMPTY_SCREEN:
            /* do nothing */
            result = 1;
            break;
            
        default:
            ml_log_warn("Unknown OSD screen type\n");
            result = 1;
            break;
    }
    
    if (osdd->prevState.screen != osdd->state->screen)
    {
        osdl_osd_screen_changed(osdd->listener, osdd->state->screen);
    }
    osdd->prevState = *osdd->state;
    
    return result;
}

static int osdd_reset(void* data)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    
    osdd->stopTicker = 1;
    join_thread(&osdd->tickerThreadId, NULL, NULL);

    if (osdd->sourceInfoOverlay != NULL)
    {
        free_overlay(osdd->sourceInfoOverlay);
        SAFE_FREE(&osdd->sourceInfoOverlay);
    }

    osdd->maxAudioLevels = -1;
    free_overlay(&osdd->audioLevelOverlay);
    free_overlay(&osdd->audioLevelM0Overlay);
    free_overlay(&osdd->audioLevelLineupOverlay);
    free_overlay(&osdd->audioLevelM40Overlay);
    free_overlay(&osdd->audioLevelM60Overlay);
    free_overlay(&osdd->audioLevelM96Overlay);
    
    osds_sink_reset(osdd->state);
    osdd->prevState = *osdd->state;
    
    osdd->playStateTick = -1;
    
    
    osdd->lastAvailableSourceLength = 0;

    osdd->stopTicker = 0;
    CHK_ORET(create_joinable_thread(&osdd->tickerThreadId, ticker_thread, osdd));
 
    return 1;    
}

static void osdd_free(void* data)
{
    DefaultOnScreenDisplay* osdd = (DefaultOnScreenDisplay*)data;
    int i;
    
    if (osdd == NULL)
    {
        return;
    }

    osdd->stopTicker = 1;
    join_thread(&osdd->tickerThreadId, NULL, NULL);

    free_timecode(&osdd->timecodeTextData);
    free_char_set(&osdd->timecodeTypeData);
    free_char_set(&osdd->numberData);
    free_overlay(&osdd->droppedFrameOverlay);

    /* TODO and NOTE: FcConfigDestroy in free_info_rec doesn't free all memory and
    therefore the OSD should only be created and free'd once */ 
    free_info_rec(&osdd->p_info);
    
    if (osdd->sourceInfoOverlay != NULL)
    {
        free_overlay(osdd->sourceInfoOverlay);
        SAFE_FREE(&osdd->sourceInfoOverlay);
    }
    free_overlay(&osdd->audioLevelOverlay);
    free_overlay(&osdd->audioLevelM0Overlay);
    free_overlay(&osdd->audioLevelLineupOverlay);
    free_overlay(&osdd->audioLevelM40Overlay);
    free_overlay(&osdd->audioLevelM60Overlay);
    free_overlay(&osdd->audioLevelM96Overlay);
    free_overlay(&osdd->markOverlay);
    free_overlay(&osdd->progressBarOverlay);
    free_overlay(&osdd->userMarksOverlay);
    free_overlay(&osdd->progressBarMarkOverlay);
    free_overlay(&osdd->secondUserMarksOverlay);
    
    for (i = 0; i < MAX_OSD_LABELS; i++)
    {
        if (osdd->cachedLabels[i].present)
        {
            free_overlay(&osdd->cachedLabels[i].labelOverlay);
        }
    }
    
    osds_free(&osdd->state);
    
    destroy_mutex(&osdd->setMenuModelMutex);
    destroy_mutex(&osdd->setMarksModelMutex);
    
    SAFE_FREE(&osdd);
}



void osdl_refresh_required(OSDListener* listener)
{
    if (listener && listener->refresh_required)
    {
        listener->refresh_required(listener->data);
    }
}

void osdl_osd_screen_changed(OSDListener* listener, OSDScreen screen)
{
    if (listener && listener->osd_screen_changed)
    {
        listener->osd_screen_changed(listener->data, screen);
    }
}


int osd_initialise(OnScreenDisplay* osd, const StreamInfo* streamInfo, const Rational* aspectRatio)
{
    if (osd && osd->initialise)
    {
        return osd->initialise(osd->data, streamInfo, aspectRatio);
    }
    return 0;
}

void osd_set_listener(OnScreenDisplay* osd, OSDListener* listener)
{
    if (osd && osd->set_listener)
    {
        osd->set_listener(osd->data, listener);
    }
}

int osd_create_menu_model(OnScreenDisplay* osd, OSDMenuModel** menu)
{
    if (osd && osd->create_menu_model)
    {
        return osd->create_menu_model(osd->data, menu);
    }
    return 0;
}

void osd_free_menu_model(OnScreenDisplay* osd, OSDMenuModel** menu)
{
    if (osd && osd->free_menu_model)
    {
        osd->free_menu_model(osd->data, menu);
    }
}

void osd_set_active_menu_model(OnScreenDisplay* osd, int updateMask, OSDMenuModel* menu)
{
    if (osd && osd->set_active_menu_model)
    {
        osd->set_active_menu_model(osd->data, updateMask, menu);
    }
}

int osd_set_screen(OnScreenDisplay* osd, OSDScreen screen)    
{
    if (osd && osd->set_screen)
    {
        return osd->set_screen(osd->data, screen);
    }
    return 0;
}

int osd_next_screen(OnScreenDisplay* osd)    
{
    if (osd && osd->next_screen)
    {
        return osd->next_screen(osd->data);
    }
    return 0;
}

int osd_get_screen(OnScreenDisplay* osd, OSDScreen* screen)    
{
    if (osd && osd->get_screen)
    {
        return osd->get_screen(osd->data, screen);
    }
    return 0;
}

int osd_set_timecode(OnScreenDisplay* osd, int index, int type, int subType)
{
    if (osd && osd->set_timecode)
    {
        return osd->set_timecode(osd->data, index, type, subType);
    }
    return 0;
}

int osd_next_timecode(OnScreenDisplay* osd)
{
    if (osd && osd->next_timecode)
    {
        return osd->next_timecode(osd->data);
    }
    return 0;
}

int osd_set_play_state(OnScreenDisplay* osd, OSDPlayState state, int value)
{
    if (osd && osd->set_play_state)
    {
        return osd->set_play_state(osd->data, state, value);
    }
    return 0;
}

int osd_set_state(OnScreenDisplay* osd, const OnScreenDisplayState* state)
{
    if (osd && osd->set_state)
    {
        return osd->set_state(osd->data, state);
    }
    return 0;
}

void osd_set_minimum_audio_stream_level(OnScreenDisplay* osd, double level)
{
    if (osd && osd->set_minimum_audio_stream_level)
    {
        osd->set_minimum_audio_stream_level(osd->data, level);
    }
}

void osd_set_audio_lineup_level(OnScreenDisplay* osd, float level)
{
    if (osd && osd->set_audio_lineup_level)
    {
        osd->set_audio_lineup_level(osd->data, level);
    }
}

void osd_reset_audio_stream_levels(OnScreenDisplay* osd)
{
    if (osd && osd->reset_audio_stream_levels)
    {
        osd->reset_audio_stream_levels(osd->data);
    }
}

int osd_register_audio_stream(OnScreenDisplay* osd, int streamId)
{
    if (osd && osd->register_audio_stream)
    {
        return osd->register_audio_stream(osd->data, streamId);
    }
    return 0;
}

void osd_set_audio_stream_level(OnScreenDisplay* osd, int streamId, double level)
{
    if (osd && osd->set_audio_stream_level)
    {
        osd->set_audio_stream_level(osd->data, streamId, level);
    }
}

void osd_set_audio_level_visibility(OnScreenDisplay* osd, int visible)
{
    if (osd && osd->set_audio_level_visibility)
    {
        osd->set_audio_level_visibility(osd->data, visible);
    }
}

void osd_toggle_audio_level_visibility(OnScreenDisplay* osd)
{
    if (osd && osd->toggle_audio_level_visibility)
    {
        osd->toggle_audio_level_visibility(osd->data);
    }
}

void osd_show_field_symbol(OnScreenDisplay* osd, int enable)
{
    if (osd && osd->show_field_symbol)
    {
        osd->show_field_symbol(osd->data, enable);
    }
}

void osd_set_mark_display(OnScreenDisplay* osd, const MarkConfigs* markConfigs)
{
    if (osd && osd->set_mark_display)
    {
        osd->set_mark_display(osd->data, markConfigs);
    }
}

int osd_create_marks_model(OnScreenDisplay* osd, OSDMarksModel** model)
{
    if (osd && osd->create_marks_model)
    {
        return osd->create_marks_model(osd->data, model);
    }
    return 0;
}

void osd_free_marks_model(OnScreenDisplay* osd, OSDMarksModel** model)
{
    if (osd && osd->free_marks_model)
    {
        osd->free_marks_model(osd->data, model);
    }
}

void osd_set_marks_model(OnScreenDisplay* osd, int updateMask, OSDMarksModel* model)
{
    if (osd && osd->set_marks_model)
    {
        osd->set_marks_model(osd->data, updateMask, model);
    }
}

void osd_set_second_marks_model(OnScreenDisplay* osd, int updateMask, OSDMarksModel* model)
{
    if (osd && osd->set_second_marks_model)
    {
        osd->set_second_marks_model(osd->data, updateMask, model);
    }
}

void osd_set_progress_bar_visibility(OnScreenDisplay* osd, int visible)
{
    if (osd && osd->set_progress_bar_visibility)
    {
        osd->set_progress_bar_visibility(osd->data, visible);
    }
}

float osd_get_position_in_progress_bar(OnScreenDisplay* osd, int x, int y)
{
    if (osd && osd->get_position_in_progress_bar)
    {
        return osd->get_position_in_progress_bar(osd->data, x, y);
    }
    return -1.0;
}

void osd_highlight_progress_bar_pointer(OnScreenDisplay* osd, int on)
{
    if (osd && osd->highlight_progress_bar_pointer)
    {
        osd->highlight_progress_bar_pointer(osd->data, on);
    }
}

void osd_set_active_progress_bar_marks(OnScreenDisplay* osd, int index)
{
    if (osd && osd->set_active_progress_bar_marks)
    {
        osd->set_active_progress_bar_marks(osd->data, index);
    }
}

void osd_set_label(OnScreenDisplay* osd, int xPos, int yPos, int imageWidth, int imageHeight, 
    int fontSize, Colour colour, int box, const char* label)
{
    if (osd && osd->set_label)
    {
        osd->set_label(osd->data, xPos, yPos, imageWidth, imageHeight, fontSize, colour, box, label);
    }
}

int osd_add_to_image(OnScreenDisplay* osd, const FrameInfo* frameInfo, unsigned char* image, 
    int width, int height)
{
    if (osd && osd->add_to_image)
    {
        return osd->add_to_image(osd->data, frameInfo, image, width, height);
    }
    return 0;
}

int osd_reset(OnScreenDisplay* osd)
{
    if (osd && osd->reset)
    {
        return osd->reset(osd->data);
    }
    return 0;
}

void osd_free(OnScreenDisplay* osd)
{
    if (osd && osd->free)
    {
        osd->free(osd->data);
    }
}



int osdd_create(OnScreenDisplay** osd)
{
    DefaultOnScreenDisplay* newOSDD;
    
    CALLOC_ORET(newOSDD, DefaultOnScreenDisplay, 1);
    
    newOSDD->maxAudioLevels = -1;
    newOSDD->playStateTick = -1;
    
    newOSDD->osd.data = newOSDD;  /* set this first so that free in fail below works */  
    newOSDD->osd.initialise = osdd_initialise;
    newOSDD->osd.set_listener = osdd_set_listener;
    newOSDD->osd.create_menu_model = osdd_create_menu_model;
    newOSDD->osd.free_menu_model = osdd_free_menu_model;
    newOSDD->osd.set_active_menu_model = osdd_set_active_menu_model;
    newOSDD->osd.set_screen = osdd_set_screen;
    newOSDD->osd.next_screen = osdd_next_screen;
    newOSDD->osd.get_screen = osdd_get_screen;
    newOSDD->osd.set_timecode = osdd_set_timecode;
    newOSDD->osd.next_timecode = osdd_next_timecode;
    newOSDD->osd.set_play_state = osdd_set_play_state;
    newOSDD->osd.set_state = osdd_set_state;
    newOSDD->osd.set_minimum_audio_stream_level = osdd_set_minimum_audio_stream_level;
    newOSDD->osd.set_audio_lineup_level = osdd_set_audio_lineup_level;
    newOSDD->osd.reset_audio_stream_levels = osdd_reset_audio_stream_levels;
    newOSDD->osd.register_audio_stream = osdd_register_audio_stream;
    newOSDD->osd.set_audio_stream_level = osdd_set_audio_stream_level;
    newOSDD->osd.set_audio_level_visibility = osdd_set_audio_level_visibility;
    newOSDD->osd.toggle_audio_level_visibility = osdd_toggle_audio_level_visibility;
    newOSDD->osd.show_field_symbol = osdd_show_field_symbol;
    newOSDD->osd.set_mark_display = osdd_set_mark_display;
    newOSDD->osd.create_marks_model = osdd_create_marks_model;
    newOSDD->osd.free_marks_model = osdd_free_marks_model;
    newOSDD->osd.set_marks_model = osdd_set_marks_model;
    newOSDD->osd.set_second_marks_model = osdd_set_second_marks_model;
    newOSDD->osd.set_progress_bar_visibility = osdd_set_progress_bar_visibility;
    newOSDD->osd.get_position_in_progress_bar = osdd_get_position_in_progress_bar;
    newOSDD->osd.set_active_progress_bar_marks = osdd_set_active_progress_bar_marks;
    newOSDD->osd.highlight_progress_bar_pointer = osdd_highlight_progress_bar_pointer;
    newOSDD->osd.set_label = osdd_set_label;
    newOSDD->osd.add_to_image = osdd_add_to_image;
    newOSDD->osd.reset = osdd_reset;
    newOSDD->osd.free = osdd_free;
    
    CHK_OFAIL(osds_create(&newOSDD->state));
    newOSDD->prevState = *newOSDD->state;
    
    CHK_OFAIL(init_mutex(&newOSDD->setMenuModelMutex));
    CHK_OFAIL(init_mutex(&newOSDD->setMarksModelMutex));
    
    CHK_OFAIL(create_joinable_thread(&newOSDD->tickerThreadId, ticker_thread, newOSDD)); 

    
    *osd = &newOSDD->osd;
    return 1;
    
fail:
    osd_free(&newOSDD->osd);
    return 0;
}


void osdm_lock(OSDMenuModel* menu)
{
    PTHREAD_MUTEX_LOCK(&menu->menuModelMutex)
}

void osdm_unlock(OSDMenuModel* menu)
{
    PTHREAD_MUTEX_UNLOCK(&menu->menuModelMutex)
}

int osdm_set_menu_private_data(OSDMenuModel* menu, long id, void* data, void (*free_func)(void* data))
{
    size_t i;
    
    if (id == 0)
    {
        return 0;
    }
    
    for (i = 0; i < sizeof(menu->osdPrivateData) / sizeof(OSDPrivateData); i++)
    {
        if (menu->osdPrivateData[i].id == 0 || menu->osdPrivateData[i].id == id)
        {
            menu->osdPrivateData[i].id = id;
            menu->osdPrivateData[i].data = data;
            menu->osdPrivateData[i].free_func = free_func;
            return 1;
        }
    }
    
    return 0;
}

int osdm_set_item_private_data(OSDMenuListItem* item, long id, void* data, void (*free_func)(void* data))
{
    size_t i;
    
    if (id == 0)
    {
        return 0;
    }
    
    for (i = 0; i < sizeof(item->osdPrivateData) / sizeof(OSDPrivateData); i++)
    {
        if (item->osdPrivateData[i].id == 0 || item->osdPrivateData[i].id == id)
        {
            item->osdPrivateData[i].id = id;
            item->osdPrivateData[i].data = data;
            item->osdPrivateData[i].free_func = free_func;
            return 1;
        }
    }
    
    return 0;
}

void* osdm_get_menu_private_data(OSDMenuModel* menu, long id)
{
    size_t i;
    
    if (id == 0)
    {
        return NULL;
    }
    
    for (i = 0; i < sizeof(menu->osdPrivateData) / sizeof(OSDPrivateData); i++)
    {
        if (menu->osdPrivateData[i].id == id)
        {
            return menu->osdPrivateData[i].data;
        }
    }
    
    return NULL;
}

void* osdm_get_item_private_data(OSDMenuListItem* item, long id)
{
    size_t i;
    
    if (id == 0)
    {
        return NULL;
    }
    
    for (i = 0; i < sizeof(item->osdPrivateData) / sizeof(OSDPrivateData); i++)
    {
        if (item->osdPrivateData[i].id == id)
        {
            return item->osdPrivateData[i].data;
        }
    }
    
    return NULL;
}

void osdm_free_menu_private_data(OSDMenuModel* menu)
{
    size_t i;
    
    for (i = 0; i < sizeof(menu->osdPrivateData) / sizeof(OSDPrivateData); i++)
    {
        if (menu->osdPrivateData[i].free_func && menu->osdPrivateData[i].data)
        {
            menu->osdPrivateData[i].free_func(menu->osdPrivateData[i].data);
            menu->osdPrivateData[i].data = NULL;
            menu->osdPrivateData[i].free_func = NULL;
            menu->osdPrivateData[i].id = 0;
        }
    }
}

void osdm_free_item_private_data(OSDMenuListItem* item)
{
    size_t i;
    
    for (i = 0; i < sizeof(item->osdPrivateData) / sizeof(OSDPrivateData); i++)
    {
        if (item->osdPrivateData[i].free_func && item->osdPrivateData[i].data)
        {
            item->osdPrivateData[i].free_func(item->osdPrivateData[i].data);
            item->osdPrivateData[i].data = NULL;
            item->osdPrivateData[i].free_func = NULL;
            item->osdPrivateData[i].id = 0;
        }
    }
}

int osdm_set_title(OSDMenuModel* menu, const char* title)
{
    SAFE_FREE(&menu->title);
    
    if (title != NULL)
    {
        CALLOC_ORET(menu->title, char, strlen(title) + 1);
        strcpy(menu->title, title);
    }

    menu->titleUpdated = -1; /* all bits set */
    
    return 1;
}

int osdm_set_status(OSDMenuModel* menu, const char* status)
{
    SAFE_FREE(&menu->status);
    
    if (status != NULL)
    {
        CALLOC_ORET(menu->status, char, strlen(status) + 1);
        strcpy(menu->status, status);
    }

    menu->statusUpdated = -1; /* all bits set */
    
    return 1;
}

int osdm_set_comment(OSDMenuModel* menu, const char* comment)
{
    SAFE_FREE(&menu->comment);
    
    if (comment != NULL)
    {
        CALLOC_ORET(menu->comment, char, strlen(comment) + 1);
        strcpy(menu->comment, comment);
    }

    menu->commentUpdated = -1; /* all bits set */
    
    return 1;
}

int osdm_insert_list_item(OSDMenuModel* menu, int index, OSDMenuListItem** item)
{
    OSDMenuListItem* newItem = NULL;
    OSDMenuListItem* itemInList;
    int i;

    if ((index < 0 && index != LAST_MENU_ITEM_INDEX) || 
        index > menu->numItems)
    {
        return 0;
    }
    
    CALLOC_ORET(newItem, OSDMenuListItem, 1);
    
    if (index == LAST_MENU_ITEM_INDEX)
    {
        /* note: index is not menu->numItems - 1,
        LAST_MENU_ITEM_INDEX means append in this case */
        index = menu->numItems;
    }
    
    if (index == 0)
    {
        /* prepend */
        newItem->next = menu->items;
        if (menu->items != NULL)
        {
            menu->items->prev = newItem;
        }
        menu->items = newItem;        
    }
    else if (index == menu->numItems)
    {
        /* append */
        itemInList = menu->items;
        while (itemInList->next != NULL)
        {
            itemInList = itemInList->next;
        }
        newItem->prev = itemInList;
        itemInList->next = newItem;
    }
    else
    {
        /* insert */
        itemInList = menu->items;
        for (i = 0; i < index - 1; i++)
        {
            itemInList = itemInList->next;
        }
        newItem->prev = itemInList;
        newItem->next = itemInList->next;
        itemInList->next->prev = newItem;
        itemInList->next = newItem;
    }

    /* keep the current item index where it was */
    if (menu->numItems > 0 && index <= menu->currentItemIndex)
    {
        menu->currentItemIndex++;
    }
    
    menu->numItems++;
    menu->itemsUpdated = -1; /* all bits set */
    
    *item = newItem;
    return 1;
}

void osdm_remove_list_item(OSDMenuModel* menu, int index)
{
    OSDMenuListItem* itemInList;
    OSDMenuListItem* item;
    int i;
    
    if ((index < 0 &&
            index != LAST_MENU_ITEM_INDEX && 
            index != CURRENT_MENU_ITEM_INDEX) || 
        index >= menu->numItems)
    {
        return;
    }
    
    if (index == LAST_MENU_ITEM_INDEX)
    {
        index = menu->numItems - 1;
    }
    else if (index == CURRENT_MENU_ITEM_INDEX)
    {
        index = menu->currentItemIndex;
    }
    
    if (index == 0)
    {
        /* first item */
        item = menu->items;
        if (menu->items->next != NULL)
        {
            menu->items->next->prev = NULL;
        }
        menu->items = menu->items->next;
    }
    else
    {
        /* not the first item */
        itemInList = menu->items;
        for (i = 0; i < index - 1; i++)
        {
            itemInList = itemInList->next;
        }
        item = itemInList->next;
        itemInList->next = item->next;
        if (item->next != NULL)
        {
            item->next->prev = itemInList;
        }
    }
    
    SAFE_FREE(&item->text);
    osdm_free_item_private_data(item);
    free(item);

    /* keep the current item index where it was */
    if (index <= menu->currentItemIndex && menu->currentItemIndex > 0)
    {
        menu->currentItemIndex--;
    }
    
    menu->numItems--;
    menu->itemsUpdated = -1; /* all bits set */
}

OSDMenuListItem* osdm_get_list_item(OSDMenuModel* menu, int index)
{
    OSDMenuListItem* item = NULL;
    int i;
    
    if ((index < 0 &&
            index != LAST_MENU_ITEM_INDEX && 
            index != CURRENT_MENU_ITEM_INDEX) || 
        index >= menu->numItems)
    {
        return NULL;
    }

    if (index == LAST_MENU_ITEM_INDEX)
    {
        index = menu->numItems - 1;
    }
    else if (index == CURRENT_MENU_ITEM_INDEX)
    {
        index = menu->currentItemIndex;
    }
    
    item = menu->items;
    for (i = 0; i < index; i++)
    {
        item = item->next;
    }

    return item;
}

void osdm_set_current_list_item(OSDMenuModel* menu, int index)
{
    if ((index < 0 &&
            index != LAST_MENU_ITEM_INDEX) ||  
        index >= menu->numItems)
    {
        return;
    }

    if (index == LAST_MENU_ITEM_INDEX)
    {
        if (menu->numItems <= 0)
        {
            return;
        }
        index = menu->numItems - 1;
    }
    
    menu->currentItemIndex = index;
    menu->itemsUpdated = -1; /* all bits set */
}

void osdm_set_next_current_list_item(OSDMenuModel* menu)
{
    menu->currentItemIndex++;
    if (menu->currentItemIndex >= menu->numItems)
    {
        menu->currentItemIndex--;
    }
    else
    {
        menu->itemsUpdated = -1; /* all bits set */
    }
}

void osdm_set_prev_current_list_item(OSDMenuModel* menu)
{
    menu->currentItemIndex--;
    if (menu->currentItemIndex < 0)
    {
        menu->currentItemIndex = 0;
    }
    else
    {
        menu->itemsUpdated = -1; /* all bits set */
    }
}

int osdm_set_list_item_text(OSDMenuModel* menu, OSDMenuListItem* item, const char* text)
{
    SAFE_FREE(&item->text);
    
    if (text != NULL)
    {
        CALLOC_ORET(item->text, char, strlen(text) + 1);
        strcpy(item->text, text);
    }

    item->textUpdated = -1; /* all bits set */
    
    return 1;
}

void osdm_set_list_item_state(OSDMenuModel* menu, OSDMenuListItem* item, OSDMenuListItemState state)
{
    if (item->state != state)
    {
        item->textUpdated = -1; /* all bits set */
    }
    
    item->state = state;
}




static int osds_set_screen(void* data, OSDScreen screen)
{
    OnScreenDisplayState* state = (OnScreenDisplayState*)data;    
    int stateChanged = (state->screen != screen);
    
    state->screen = screen;
    state->screenSet = 1;
    
    return stateChanged;
}

static int osds_next_screen(void* data)
{
    OnScreenDisplayState* state = (OnScreenDisplayState*)data;    
    int stateChanged = !state->nextScreen;
    
    state->nextScreen = 1;
    
    return stateChanged;
}

static int osds_get_screen(void* data, OSDScreen* screen)
{
    OnScreenDisplayState* state = (OnScreenDisplayState*)data;

    *screen = state->screen;
    
    return 1;
}

static int osds_set_timecode(void* data, int index, int type, int subType)
{
    OnScreenDisplayState* state = (OnScreenDisplayState*)data;
    
    state->setTimecode = 1;
    state->setTimecodeIndex = index;
    state->setTimecodeType = type;
    state->setTimecodeSubType = subType;
    
    /* assume that the final state will change */
    return 1;
}

static int osds_next_timecode(void* data)
{
    OnScreenDisplayState* state = (OnScreenDisplayState*)data;
    
    state->nextTimecode = 1;

    /* assume that the final state will change */
    return 1;
}

static int osds_set_play_state(void* data, OSDPlayState playState, int value)
{
    OnScreenDisplayState* state = (OnScreenDisplayState*)data;    
    int stateChanged = 0;
    
    if (playState == OSD_PLAY_STATE)
    {
        if (value != state->isPlaying)
        {
            stateChanged = 1;
        }
        state->isPlaying = value;
    }
    else if (playState == OSD_SPEED_STATE)
    {
        if (state->playSpeed != value)
        {
            stateChanged = 1;
        }
        state->playSpeed = value;
    }

    return stateChanged;    
}

static void osds_set_minimum_audio_stream_level(void* data, double level)
{
    OnScreenDisplayState* state = (OnScreenDisplayState*)data;
    
    state->minimumAudioLevel = level;
    state->nullAudioLevel = state->minimumAudioLevel - 10;
}

static void osds_set_audio_lineup_level(void* data, float level)
{
    OnScreenDisplayState* state = (OnScreenDisplayState*)data;
    
    state->audioLineupLevel = level;
}

static void osds_reset_audio_stream_levels(void* data)
{
    OnScreenDisplayState* state = (OnScreenDisplayState*)data;
    int i;
    
    for (i = 0; i < state->numAudioLevels; i++)
    {
        state->audioStreamLevels[i].level = state->nullAudioLevel;
    }
}

static int osds_register_audio_stream(void* data, int streamId)
{
    OnScreenDisplayState* state = (OnScreenDisplayState*)data;
    int i;
    
    for (i = 0; i < state->numAudioLevels; i++)
    {
        if (state->audioStreamLevels[i].streamId == streamId)
        {
            return 1; 
        }
    }
    
    if (state->numAudioLevels < MAX_AUDIO_LEVELS)
    {
        /* add new stream */
        state->audioStreamLevels[state->numAudioLevels].streamId = streamId;
        state->audioStreamLevels[state->numAudioLevels].level = state->nullAudioLevel;
        state->numAudioLevels++;
        return 1;
    }
    
    return 0;
}

static void osds_set_audio_stream_level(void* data, int streamId, double level)
{
    OnScreenDisplayState* state = (OnScreenDisplayState*)data;
    int i;

    for (i = 0; i < state->numAudioLevels; i++)
    {
        if (state->audioStreamLevels[i].streamId == streamId)
        {
            state->audioStreamLevels[i].level = (level > 0.0) ? 
                0.0 : level;
            return;
        }
    }
    
    if (state->numAudioLevels < MAX_AUDIO_LEVELS)
    {
        /* add new stream */
        state->audioStreamLevels[state->numAudioLevels].streamId = streamId;
        state->audioStreamLevels[i].level = (level > 0.0) ? 
            0.0 : level;
        state->numAudioLevels++;
    }
}

static void osds_show_field_symbol(void* data, int enable)
{
    OnScreenDisplayState* state = (OnScreenDisplayState*)data;
    
    state->showFieldSymbol = enable;
}

static void osds_set_mark_display(void* data, const MarkConfigs* markConfigs)
{
    OnScreenDisplayState* state = (OnScreenDisplayState*)data;

    state->markConfigs = *markConfigs;
}

static void osds_set_label(void* data, int xPos, int yPos, int imageWidth, int imageHeight, 
    int fontSize, Colour colour, int box, const char* label)
{
    OnScreenDisplayState* state = (OnScreenDisplayState*)data;
    
    /* clear labels if the previous call was to complete the state */
    if (state->clearLabels)
    {
        state->numLabels = 0;
        state->clearLabels = 0;
    }

    /* check max labels not exceeded */    
    if (state->numLabels + 1 > MAX_OSD_LABELS)
    {
        return;
    }
    
    /* ignore empty label */
    if (label == NULL || strlen(label) == 0)
    {
        return;
    }
    
    /* add label */
    state->labels[state->numLabels].xPos = xPos;
    state->labels[state->numLabels].yPos = yPos;
    state->labels[state->numLabels].imageWidth = imageWidth;
    state->labels[state->numLabels].imageHeight = imageHeight;
    state->labels[state->numLabels].fontSize = fontSize;
    state->labels[state->numLabels].colour = colour;
    state->labels[state->numLabels].box = box;
    strncpy(state->labels[state->numLabels].text, label, sizeof(state->labels[state->numLabels].text) - 1);
    state->labels[state->numLabels].text[sizeof(state->labels[state->numLabels].text) - 1] = '\0';
    
    state->numLabels++;
}

static int osds_set_state(void* data, const OnScreenDisplayState* newState)
{
    OnScreenDisplayState* state = (OnScreenDisplayState*)data;
    int stateChanged;
    OnScreenDisplay originalOSD;
    OSDScreen screen;
    int originalClearLabels = state->clearLabels;
    
    /* adopt new state screen only if it was set */
    screen = (newState->screenSet) ? newState->screen : state->screen;
    
    stateChanged = (newState->screenSet && state->screen != newState->screen);
    stateChanged = stateChanged || (newState->setTimecode);
    stateChanged = stateChanged || (newState->nextTimecode);
    stateChanged = stateChanged || (state->isPlaying != newState->isPlaying);
    stateChanged = stateChanged || (state->playSpeed != newState->playSpeed);

    /* copy data, except the osd interface */
    originalOSD = state->osd;    
    *state = *newState;
    state->screen = screen;
    state->osd = originalOSD;
    
    if (state->numLabels > 0)
    {
        /* new labels were copied */
        state->clearLabels = 0;
    }
    else
    {
        /* clearLabel use is local to the state and should not be transferred */
        state->clearLabels = originalClearLabels;
    }
    
    return stateChanged;
}



int osds_create(OnScreenDisplayState** state)
{
    OnScreenDisplayState* newState = NULL;
    
    CALLOC_ORET(newState, OnScreenDisplayState, 1);
    
    newState->minimumAudioLevel = -96;
    newState->nullAudioLevel = newState->minimumAudioLevel - 10;
    newState->audioLineupLevel = -18;
    
    /* only implement functions that are for changing the state - we don't expect the other funcs to be called */
    newState->osd.data = newState;
    newState->osd.set_screen = osds_set_screen;
    newState->osd.next_screen = osds_next_screen;
    newState->osd.get_screen = osds_get_screen;
    newState->osd.set_timecode = osds_set_timecode;
    newState->osd.next_timecode = osds_next_timecode;
    newState->osd.set_play_state = osds_set_play_state;
    newState->osd.set_state = osds_set_state;
    newState->osd.set_minimum_audio_stream_level = osds_set_minimum_audio_stream_level;
    newState->osd.set_audio_lineup_level = osds_set_audio_lineup_level;
    newState->osd.reset_audio_stream_levels = osds_reset_audio_stream_levels;
    newState->osd.register_audio_stream = osds_register_audio_stream;
    newState->osd.set_audio_stream_level = osds_set_audio_stream_level;
    newState->osd.show_field_symbol = osds_show_field_symbol;
    newState->osd.set_mark_display = osds_set_mark_display;
    newState->osd.set_label = osds_set_label;
    
    osds_sink_reset(newState);
    
    *state = newState;
    return 1;
}

void osds_free(OnScreenDisplayState** state)
{
    if (*state == NULL)
    {
        return;
    }
    
    SAFE_FREE(state);
}

OnScreenDisplay* osds_get_osd(OnScreenDisplayState* state)
{
    return &state->osd;
}

void osds_complete(OnScreenDisplayState* state, const FrameInfo* frameInfo)
{
    int i;
    int index;
    int typedIndex;
    
    /* clear labels if there were labels in the previous call to complete but not in this frame */
    if (state->clearLabels)
    {
        state->numLabels = 0;
        state->clearLabels = 0;
    }
    
    /* complete the OSD state given the frame information */
    
    state->timecodeIndex = -1;
    if (frameInfo->numTimecodes > 0)
    {
        /* find the timecode selected for OSD display */  
        if (state->setTimecode)
        {
            typedIndex = 0;
            index = 0;
            for (i = 0; i < frameInfo->numTimecodes; i++)
            {
                if ((state->setTimecodeIndex < 0 || state->setTimecodeIndex == typedIndex) &&
                    (state->setTimecodeType < 0 || frameInfo->timecodes[i].timecodeType == (unsigned int)state->setTimecodeType) &&
                    (state->setTimecodeSubType < 0 || frameInfo->timecodes[i].timecodeSubType == (unsigned int)state->setTimecodeSubType))
                {
                    state->timecodeIndex = index;
                    break;
                }
                else if ((state->setTimecodeType < 0 || frameInfo->timecodes[i].timecodeType == (unsigned int)state->setTimecodeType) &&
                    (state->setTimecodeSubType < 0 || frameInfo->timecodes[i].timecodeSubType == (unsigned int)state->setTimecodeSubType))
                {
                    typedIndex++;
                }
                index++;
            }
        }
        state->setTimecode = 0;
        
        if (state->timecodeIndex == -1)
        {
            /* find the current or next timecode for OSD dsiplay */  
            for (i = 0; i < frameInfo->numTimecodes; i++)
            {
                if (state->timecodeStreamId == -1 ||
                    state->timecodeStreamId == frameInfo->timecodes[i].streamId)
                {
                    if (state->timecodeStreamId == -1)
                    {
                        /* first time take first timecode */
                        state->timecodeIndex = i;
                    }
                    else if (state->nextTimecode)
                    {
                        /* select the next timecode */
                        state->timecodeIndex = (i + 1) % frameInfo->numTimecodes;
                    }
                    else
                    {
                        /* no change */
                        state->timecodeIndex = i;
                    }
                    break;
                }
            }
        }
        
        if (state->timecodeIndex >= 0)
        {
            state->timecodeStreamId = frameInfo->timecodes[state->timecodeIndex].streamId;
        }
    }
    
    state->setTimecode = 0;
    state->nextTimecode = 0;
    state->clearLabels = 1;
}

void osds_reset(OnScreenDisplayState* state)
{
    int i;
    
    state->screen = 0;
    /* TODO: if the player starts in a different state then these will be wrong if no
    state change event is sent */
    state->isPlaying = 1;
    state->playSpeed = 1;
    state->playForwards = 1;
    state->timecodeIndex = -1;
    state->timecodeStreamId = -1;
    
    state->screenSet = 0;
    state->nextScreen = 0;
    state->setTimecode = 0;
    state->setTimecodeIndex = 0;
    state->setTimecodeType = 0;
    state->setTimecodeSubType = 0;
    state->nextTimecode = 0;

    for (i = 0; i < state->numAudioLevels; i++)
    {
        state->audioStreamLevels[i].level = state->nullAudioLevel; 
    }
}

/* use this function when creating state and within a msk_reset_or_close() call - eg. see buffered_media_sink.c */
void osds_sink_reset(OnScreenDisplayState* state)
{
    osds_reset(state);
    state->numAudioLevels = 0;
    state->numLabels = 0;
    /* TODO: reset mark configs? */
}

void osds_reset_screen_state(OnScreenDisplayState* state)
{
    state->screenSet = 0;
    state->nextScreen = 0;
}



