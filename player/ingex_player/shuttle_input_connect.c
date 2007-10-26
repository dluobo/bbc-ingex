#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>

#include "shuttle_input_connect.h"
#include "logging.h"
#include "macros.h"


/* a key pressed for this number of micro seconds is 'held' */
#define KEY_HOLD_EVENT_DURATION     500000

/* a critical (eg. quit) key pressed for this number of micro seconds is 'held' */
#define CRITICAL_KEY_HOLD_EVENT_DURATION     800000



struct ShuttleConnect
{
    int reviewDuration;
    ConnectMapping mapping;
    MediaControl* control;
    ShuttleInput* shuttle;
    
    ShuttleEvent lastNonPingEvent;
    
    /* variables for qc mapping */
    struct timeval qcClearMarkPressedTime;
    struct timeval qcQuitPressedTime;
    struct timeval qcSeekPrevMarkPressedTime;
    struct timeval qcSeekNextMarkPressedTime;
    unsigned int maxShuttleSpeed;
    int haveReversed;
    int playButtonPressed;
    int pauseButtonPressed;
    MediaControlMode prevModeForShuttle;
    unsigned int prevShuttleSpeed;
    MediaControlMode prevModeForPrevMarkSeek;
    MediaControlMode prevModeForNextMarkSeek;
    MediaControlMode prevModeForClearMarks;
};

static int g_forwardShuttleSpeed[8] = {1, 1, 2, 5, 10, 20, 50, 100};
static int g_reverseShuttleSpeed[8] = {1, -1, -2, -5, -10, -20, -50, -100};

static int g_forwardShuttleSpeedPerc[8] = {1, 1, 2, 3, 4, 5, 6, 10};
static int g_reverseShuttleSpeedPerc[8] = {1, -1, -2, -3, -4, -5, -6, -10};

static int g_forwardShuttleSpeedMenu[8] = {1, 1, 1, 2, 3, 3, 4, 5};
static int g_reverseShuttleSpeedMenu[8] = {1, 1, 1, 2, 3, 3, 4, 5};

    
static long get_time_diff(struct timeval* from)
{
    long diff;
    struct timeval now;
    
    gettimeofday(&now, NULL);
    
    diff = (now.tv_sec - from->tv_sec) * 1000000 + now.tv_usec - from->tv_usec;
    if (diff < 0)
    {
        /* we don't allow negative differences */
        diff = 0;
    }
    
    return diff;
}

static void default_listener(void* data, ShuttleEvent* event)
{
    ShuttleConnect* connect = (ShuttleConnect*)data;
    MediaControlMode mode = mc_get_mode(connect->control);
    int i;
    
    switch (event->type)
    {
        case SH_KEY_EVENT:
            if (!event->value.key.isPressed)
            {
                /* ignore key release events */
                break;
            }

            
            /* else event->value.key.isPressed */
            
            switch (event->value.key.number)
            {
                case 1:
                    mc_toggle_lock(connect->control);
                    break;
                case 2:
                    mc_next_osd_screen(connect->control);
                    break;
                case 3:
                    mc_next_osd_timecode(connect->control);
                    break;
                /* case 4: */
                case 5:
                    mc_toggle_play_pause(connect->control);
                    break;
                case 6:
                    mc_seek(connect->control, 0, SEEK_SET, FRAME_PLAY_UNIT);
                    break;
                case 7:
                    mc_seek(connect->control, 0, SEEK_END, FRAME_PLAY_UNIT);
                    break;
                case 8:
                    mc_clear_all_marks(connect->control, ALL_MARK_TYPE);
                    break;
                case 9:
                    mc_stop(connect->control);
                    break;
                case 10:
                    mc_mark(connect->control, M0_MARK_TYPE, 1);
                    break;
                case 11:
                    mc_clear_mark(connect->control, ALL_MARK_TYPE);
                    break;
                case 12:
                    mc_seek_prev_mark(connect->control);
                    break;
                case 13:
                    mc_seek_next_mark(connect->control);
                    break;
                case 14:
                    if (mode == MENU_MODE)
                    {
                        mc_select_menu_item_left(connect->control);
                    }
                    else
                    {
                        mc_switch_prev_video(connect->control);
                    }
                    break;
                case 15:
                    if (mode == MENU_MODE)
                    {
                        mc_select_menu_item_right(connect->control);
                    }
                    else
                    {
                        mc_switch_next_video(connect->control);
                    }
                    break;
            }

            break;
        case SH_SHUTTLE_EVENT:
            assert(event->value.shuttle.speed <= 7);

            if (mode == MENU_MODE)
            {
                if (event->value.shuttle.clockwise)
                {
                    for (i = 0; i < g_forwardShuttleSpeedMenu[event->value.shuttle.speed]; i++)
                    {
                        mc_next_menu_item(connect->control);
                    }
                }
                else
                {
                    for (i = 0; i < g_reverseShuttleSpeedMenu[event->value.shuttle.speed]; i++)
                    {
                        mc_previous_menu_item(connect->control);
                    }
                }
            }
            else
            {
                if (event->value.shuttle.clockwise)
                {
                    /* speed forward */
                    mc_play_speed(connect->control, g_forwardShuttleSpeed[event->value.shuttle.speed], FRAME_PLAY_UNIT);
                }
                else
                {
                    /* speed backward */
                    mc_play_speed(connect->control, g_reverseShuttleSpeed[event->value.shuttle.speed], FRAME_PLAY_UNIT);
                }
            }
            break;
        case SH_JOG_EVENT:
            if (mode == MENU_MODE)
            {
                if (event->value.jog.clockwise)
                {
                    mc_next_menu_item(connect->control);
                }
                else
                {
                    mc_previous_menu_item(connect->control);
                }
            }
            else
            {
                if (event->value.jog.clockwise)
                {
                    /* step forward */
                    mc_step(connect->control, 1, FRAME_PLAY_UNIT);
                }
                else
                {
                    /* step backward */
                    mc_step(connect->control, 0, FRAME_PLAY_UNIT);
                }
            }
            break;
    
        case SH_PING_EVENT:
            if (mode == MENU_MODE &&
                connect->lastNonPingEvent.type == SH_SHUTTLE_EVENT &&
                connect->lastNonPingEvent.value.shuttle.speed > 0)
            {
                /* shuttle further through the menu */
                if (connect->lastNonPingEvent.value.shuttle.clockwise)
                {
                    for (i = 0; i < g_forwardShuttleSpeedMenu[connect->lastNonPingEvent.value.shuttle.speed]; i++)
                    {
                        mc_next_menu_item(connect->control);
                    }
                }
                else
                {
                    for (i = 0; i < g_reverseShuttleSpeedMenu[connect->lastNonPingEvent.value.shuttle.speed]; i++)
                    {
                        mc_previous_menu_item(connect->control);
                    }
                }
            }
            break;
    }
    
    if (event->type != SH_PING_EVENT)
    {
        connect->lastNonPingEvent = *event;
    }
}

static void qc_listener(void* data, ShuttleEvent* event)
{
    ShuttleConnect* connect = (ShuttleConnect*)data;
    unsigned int speed;
    MediaControlMode mode = mc_get_mode(connect->control);
    int i;
    
    if (mode == MENU_MODE)
    {
        switch (event->type)
        {
            case SH_KEY_EVENT:
            
                if (!event->value.key.isPressed) /* released */
                {
                    switch (event->value.key.number)
                    {
                        case 4:
                            if (get_time_diff(&connect->qcQuitPressedTime) >= CRITICAL_KEY_HOLD_EVENT_DURATION)
                            {
                                mc_stop(connect->control);
                            }
                            break;
                        case 10:
                            connect->playButtonPressed = 0;
                            break;
                        case 12:
                            connect->pauseButtonPressed = 0;
                            break;
                        default:
                            break;
                    }
                }
                else /* event->value.key.isPressed */
                {
                    switch (event->value.key.number)
                    {
                        case 1:
                            mc_next_osd_screen(connect->control);
                            break;
                        case 4:
                            gettimeofday(&connect->qcQuitPressedTime, NULL);
                            break;
                        case 9:
                            mc_select_menu_item_center(connect->control);
                            gettimeofday(&connect->qcClearMarkPressedTime, NULL);
                            connect->prevModeForClearMarks = mode;
                            break;
                        case 10:
                            mc_select_menu_item_extra(connect->control);
                            break;
                        case 12:
                            mc_pause(connect->control);
                            connect->pauseButtonPressed = 1;
                            break;
                        case 14:
                            mc_select_menu_item_left(connect->control);
                            connect->prevModeForPrevMarkSeek = mode;
                            break;
                        case 15:
                            mc_select_menu_item_right(connect->control);
                            connect->prevModeForNextMarkSeek = mode;
                            break;
                        default:
                            break;
                    }
                }
                break;

            case SH_SHUTTLE_EVENT:
                assert(event->value.shuttle.speed <= 7);
                
                /* pause when speed > 1 or reverse play coming from a non-menu mode */
                if (connect->prevModeForShuttle != MENU_MODE &&
                    (connect->maxShuttleSpeed > 1 || connect->haveReversed))
                {
                    mc_pause(connect->control);
                }

                if (event->value.shuttle.speed >= connect->maxShuttleSpeed)
                {
                    if (event->value.shuttle.clockwise)
                    {
                        for (i = 0; i < g_forwardShuttleSpeedMenu[event->value.shuttle.speed]; i++)
                        {
                            mc_next_menu_item(connect->control);
                        }
                    }
                    else
                    {
                        for (i = 0; i < g_reverseShuttleSpeedMenu[event->value.shuttle.speed]; i++)
                        {
                            mc_previous_menu_item(connect->control);
                        }
                    }
                }
                
                if (event->value.shuttle.speed == 0)
                {
                    connect->maxShuttleSpeed = 0;
                    connect->haveReversed = 0;
                }
                else
                {
                    connect->maxShuttleSpeed = (event->value.shuttle.speed > connect->maxShuttleSpeed) ?
                        event->value.shuttle.speed : connect->maxShuttleSpeed;
                    if (!event->value.shuttle.clockwise)
                    {
                        connect->haveReversed = 1;
                    }
                }
                
                connect->prevModeForShuttle = mode;
                connect->prevShuttleSpeed = event->value.shuttle.speed;
                
                break;
                
            case SH_JOG_EVENT:
                if (event->value.jog.clockwise)
                {
                    mc_next_menu_item(connect->control);
                }
                else
                {
                    mc_previous_menu_item(connect->control);
                }
                break;
    
            case SH_PING_EVENT:
                if (connect->lastNonPingEvent.type == SH_SHUTTLE_EVENT &&
                    connect->lastNonPingEvent.value.shuttle.speed > 0)
                {
                    /* shuttle further through the menu */
                    if (connect->lastNonPingEvent.value.shuttle.clockwise)
                    {
                        for (i = 0; i < g_forwardShuttleSpeedMenu[connect->lastNonPingEvent.value.shuttle.speed]; i++)
                        {
                            mc_next_menu_item(connect->control);
                        }
                    }
                    else
                    {
                        for (i = 0; i < g_reverseShuttleSpeedMenu[connect->lastNonPingEvent.value.shuttle.speed]; i++)
                        {
                            mc_previous_menu_item(connect->control);
                        }
                    }
                }
                break;
        }            
    }
    else /* mode != MENU_MODE */
    {
        switch (event->type)
        {
            case SH_KEY_EVENT:
            
                if (!event->value.key.isPressed) /* released */
                {
                    switch (event->value.key.number)
                    {
                        case 4:
                            if (get_time_diff(&connect->qcQuitPressedTime) >= CRITICAL_KEY_HOLD_EVENT_DURATION)
                            {
                                mc_stop(connect->control);
                            }
                            break;
                        case 9:
                            if (connect->prevModeForClearMarks != MENU_MODE)
                            {
                                if (get_time_diff(&connect->qcClearMarkPressedTime) >= KEY_HOLD_EVENT_DURATION)
                                {
                                    /* clear all marks except for PSE failures and D3 VTR errors */
                                    mc_clear_all_marks(connect->control, 
                                        ALL_MARK_TYPE & ~D3_VTR_ERROR_MARK_TYPE & ~D3_PSE_FAILURE_MARK_TYPE);
                                }
                                else
                                {
                                    /* clear mark except for PSE failures and D3 VTR errors */
                                    mc_clear_mark(connect->control, 
                                        ALL_MARK_TYPE & ~D3_VTR_ERROR_MARK_TYPE & ~D3_PSE_FAILURE_MARK_TYPE);
                                }
                            }
                            break;
                        case 10:
                            connect->playButtonPressed = 0;
                            break;
                        case 12:
                            connect->pauseButtonPressed = 0;
                            break;
                        case 14:
                            if (connect->prevModeForPrevMarkSeek != MENU_MODE)
                            {
                                if (get_time_diff(&connect->qcSeekPrevMarkPressedTime) >= KEY_HOLD_EVENT_DURATION)
                                {
                                    mc_pause(connect->control);
                                    mc_seek(connect->control, 0, SEEK_SET, FRAME_PLAY_UNIT);
                                }
                                else
                                {
                                    mc_seek_prev_mark(connect->control);
                                }
                            }
                            break;
                        case 15:
                            if (connect->prevModeForNextMarkSeek != MENU_MODE)
                            {
                                if (get_time_diff(&connect->qcSeekNextMarkPressedTime) >= KEY_HOLD_EVENT_DURATION)
                                {
                                    mc_pause(connect->control);
                                    mc_seek(connect->control, 0, SEEK_END, FRAME_PLAY_UNIT);
                                }
                                else
                                {
                                    mc_seek_next_mark(connect->control);
                                }
                            }
                            break;
                        default:
                            break;
                    }
                }
                else
                {
                    /* else event->value.key.isPressed */
                    
                    switch (event->value.key.number)
                    {
                        case 1:
                            mc_next_osd_screen(connect->control);
                            break;
                        case 2:
                            mc_next_osd_timecode(connect->control);
                            break;
                        case 3:
                            mc_toggle_lock(connect->control);
                            break;
                        case 4:
                            gettimeofday(&connect->qcQuitPressedTime, NULL);
                            break;
                        case 5:
                            mc_mark(connect->control, M1_MARK_TYPE, 1);
                            break;
                        case 6:
                            mc_mark(connect->control, M2_MARK_TYPE, 1);
                            break;
                        case 7:
                            mc_mark(connect->control, M3_MARK_TYPE, 1);
                            break;
                        case 8:
                            mc_mark(connect->control, M4_MARK_TYPE, 1);
                            break;
                        case 9:
                            gettimeofday(&connect->qcClearMarkPressedTime, NULL);
                            connect->prevModeForClearMarks = mode;
                            break;
                        case 10:
                            mc_play(connect->control);
                            connect->playButtonPressed = 1;
                            break;
                        case 11:
                            mc_review(connect->control, connect->reviewDuration * 25);
                            break;
                        case 12:
                            mc_pause(connect->control);
                            connect->pauseButtonPressed = 1;
                            break;
                        case 13:
                            mc_mark(connect->control, M0_MARK_TYPE, 1);
                            break;
                        case 14:
                            gettimeofday(&connect->qcSeekPrevMarkPressedTime, NULL);
                            connect->prevModeForPrevMarkSeek = mode;
                            break;
                        case 15:
                            gettimeofday(&connect->qcSeekNextMarkPressedTime, NULL);
                            connect->prevModeForNextMarkSeek = mode;
                            break;
                    }
                }
                break;
                
            case SH_SHUTTLE_EVENT:
                assert(event->value.shuttle.speed <= 7);
                
                if (connect->playButtonPressed)
                {
                    if (event->value.shuttle.clockwise)
                    {
                        /* play at % source length steps */
                        speed = g_forwardShuttleSpeedPerc[event->value.shuttle.speed];
                    }
                    else
                    {
                        /* play backwards at % source length steps */
                        speed = g_reverseShuttleSpeedPerc[event->value.shuttle.speed];
                    }
    
                    mc_play_speed(connect->control, speed, PERCENTAGE_PLAY_UNIT);
                }
                else
                {
                    if (event->value.shuttle.clockwise)
                    {
                        /* speed forward */
                        speed = g_forwardShuttleSpeed[event->value.shuttle.speed];
                    }
                    else
                    {
                        /* speed backward */
                        speed = g_reverseShuttleSpeed[event->value.shuttle.speed];
                    }
                    
                    mc_play_speed(connect->control, speed, FRAME_PLAY_UNIT);
                }
                
                if (event->value.shuttle.speed == 0)
                {
                    /* pause when returning to neutral from a speed > 1 or reverse play */
                    if (connect->maxShuttleSpeed > 1 || connect->haveReversed)
                    {
                        mc_pause(connect->control);
                    }
                    connect->maxShuttleSpeed = 0;
                    connect->haveReversed = 0;
                }
                else
                {
                    connect->maxShuttleSpeed = (event->value.shuttle.speed > connect->maxShuttleSpeed) ?
                        event->value.shuttle.speed : connect->maxShuttleSpeed;
                    if (!event->value.shuttle.clockwise)
                    {
                        connect->haveReversed = 1;
                    }
                }
                
                connect->prevModeForShuttle = mode;
                connect->prevShuttleSpeed = event->value.shuttle.speed;
                
                break;
                
            case SH_JOG_EVENT:
                if (event->value.jog.clockwise)
                {
                    /* step forward */
                    mc_step(connect->control, 1, 
                        connect->pauseButtonPressed ? PERCENTAGE_PLAY_UNIT : FRAME_PLAY_UNIT);
                }
                else
                {
                    /* step backward */
                    mc_step(connect->control, 0, 
                        connect->pauseButtonPressed ? PERCENTAGE_PLAY_UNIT : FRAME_PLAY_UNIT);
                }
                break;

            case SH_PING_EVENT:
                /* only acted on in MENU_MODE */
                break;
        }
    }

    
    if (event->type != SH_PING_EVENT)
    {
        connect->lastNonPingEvent = *event;
    }
}


int sic_create_shuttle_connect(int reviewDuration, MediaControl* control, ShuttleInput* shuttle, 
    ConnectMapping mapping, ShuttleConnect** connect)
{
    ShuttleConnect* newConnect;

    CALLOC_ORET(newConnect, ShuttleConnect, 1);
    
    newConnect->reviewDuration = reviewDuration;
    newConnect->control = control;
    newConnect->shuttle = shuttle;
    
    /* assume we were in menu mode so that things like button 15 doesn't put us at the 
    end of the file for the QC_MAPPING */
    newConnect->prevModeForShuttle = MENU_MODE;
    newConnect->prevModeForPrevMarkSeek = MENU_MODE;
    newConnect->prevModeForNextMarkSeek = MENU_MODE;
    
    
    switch (mapping)
    {
        case QC_MAPPING:
            CHK_OFAIL(shj_register_listener(newConnect->shuttle, qc_listener, newConnect));
            break;
            
        default:
            CHK_OFAIL(shj_register_listener(newConnect->shuttle, default_listener, newConnect));
            break;
    }
    
    
    *connect = newConnect;
    return 1;
    
fail:
    sic_free_shuttle_connect(&newConnect);
    return 0;
}

void sic_free_shuttle_connect(ShuttleConnect** connect)
{
    if (*connect == NULL)
    {
        return;
    }
    
    shj_unregister_listener((*connect)->shuttle, qc_listener);
    shj_unregister_listener((*connect)->shuttle, default_listener);
    
    SAFE_FREE(connect);
}
