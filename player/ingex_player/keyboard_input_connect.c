#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>

#include <X11/keysym.h>

#include "keyboard_input_connect.h"
#include "logging.h"
#include "macros.h"


#define NUM_SPEEDS                      7

#define HALF_SPLIT_MOVE_MAX_SPEED       5

struct KeyboardConnect
{
    int reviewDuration;
    MediaControl* control;
    KeyboardInput* input;
    KeyboardInputListener listener;
    
    int speedIndex;
    int reverse;
    
    struct timeval halfSplitMovePressedTime;
    int halfSplitSpeed;
};

static unsigned int g_keyboardSpeed[NUM_SPEEDS] = {1, 2, 5, 10, 20, 50, 100};
static unsigned int g_reverseKeyboardSpeed[NUM_SPEEDS] = {-1, -2, -5, -10, -20, -50, -100};

static long get_elapsed_time(struct timeval* from)
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
    
    *from = now;
    
    return diff;
}


static void default_key_pressed(void* data, int key)
{
    KeyboardConnect* connect = (KeyboardConnect*)data;
    long elapsedTime;
    MediaControlMode mode = mc_get_mode(connect->control);

    switch (key)
    {
        case 'j':
            if (!connect->reverse)
            {
                connect->reverse = 1;
                connect->speedIndex = 0;
            }
            else
            {
                connect->speedIndex = (connect->speedIndex < NUM_SPEEDS - 1) ? 
                    (connect->speedIndex + 1) : connect->speedIndex;
            }
            mc_play_speed(connect->control, g_reverseKeyboardSpeed[connect->speedIndex], FRAME_PLAY_UNIT);
            break;
        case 'l':
            if (connect->reverse)
            {
                connect->reverse = 0;
                connect->speedIndex = 0;
            }
            else
            {
                connect->speedIndex = (connect->speedIndex < NUM_SPEEDS - 1) ? 
                    (connect->speedIndex + 1) : connect->speedIndex;
            }
            mc_play_speed(connect->control, g_keyboardSpeed[connect->speedIndex], FRAME_PLAY_UNIT);
            break;
        case 'k':
            connect->speedIndex = -1;
            connect->reverse = 0;
            mc_pause(connect->control);
            break;
        case 'i':
            mc_toggle_lock(connect->control);
            break;
        case 'q':
            mc_stop(connect->control);
            break;
        case XK_space:
            connect->speedIndex = -1;
            connect->reverse = 0;
            mc_toggle_play_pause(connect->control);
            break;
        case XK_Right:
            if (mode == MENU_MODE)
            {
                mc_select_menu_item_right(connect->control);
            }
            else
            {
                mc_step(connect->control, 1, FRAME_PLAY_UNIT);
            }
            break;
        case XK_Left:
            if (mode == MENU_MODE)
            {
                mc_select_menu_item_left(connect->control);
            }
            else
            {
                mc_step(connect->control, 0, FRAME_PLAY_UNIT);
            }
            break;
        case XK_Up:
            if (mode == MENU_MODE)
            {
                mc_previous_menu_item(connect->control);
            }
            else
            {
                /* 10 frames speed up forwards */
                mc_play_speed(connect->control, 10, FRAME_PLAY_UNIT);
            }
            break;
        case XK_Down:
            if (mode == MENU_MODE)
            {
                mc_next_menu_item(connect->control);
            }
            else
            {
                /* 10 frames speed up backwards */
                mc_play_speed(connect->control, -10, FRAME_PLAY_UNIT);
            }
            break;
        case XK_Page_Up:
            /* step 1 minute backwards */
            mc_seek(connect->control, -1500, SEEK_CUR, FRAME_PLAY_UNIT);
            break;
        case XK_Page_Down:
            /* step 1 minute forwards */
            mc_seek(connect->control, 1500, SEEK_CUR, FRAME_PLAY_UNIT);
            break;
        case 'm':
            mc_mark(connect->control, M0_MARK_TYPE, 1);
            break;
        case 'c':
            mc_clear_mark(connect->control, ALL_MARK_TYPE);
            break;
        case 'b':
            mc_clear_all_marks(connect->control, ALL_MARK_TYPE);
            break;
        case ',':
            mc_seek_prev_mark(connect->control);
            break;
        case '.':
            mc_seek_next_mark(connect->control);
            break;
        case '/':
            mc_seek_clip_mark(connect->control);
            break;
        case XK_Home:
            mc_seek(connect->control, 0, SEEK_SET, FRAME_PLAY_UNIT);
            break;
        case XK_End:
            mc_seek(connect->control, 0, SEEK_END, FRAME_PLAY_UNIT);
            break;
        case 'o':
            mc_next_osd_screen(connect->control);
            break;
        case 't':
            mc_next_osd_timecode(connect->control);
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            mc_switch_video(connect->control, key - '0');
            break;
        case 'e':
            mc_switch_prev_audio_group(connect->control);
            break;
        case 'p':
            mc_snap_audio_to_video(connect->control);
            break;
        case 'r':
            mc_switch_next_audio_group(connect->control);
            break;
        case 'a':
            mc_review_start(connect->control, connect->reviewDuration * 25);
            break;
        case 'z':
            mc_review_end(connect->control, connect->reviewDuration * 25);
            break;
        case 'x':
            mc_review(connect->control, connect->reviewDuration * 25);
            break;
        case 's':
            mc_set_half_split_orientation(connect->control, -1 /* toggle */);
            break;
        case 'd':
            mc_set_half_split_type(connect->control, -1 /* toggle */);
            break;
        case 'f':
            mc_show_half_split(connect->control, -1 /* toggle */);
            break;
        case 'g':
            elapsedTime = get_elapsed_time(&connect->halfSplitMovePressedTime);
            if (elapsedTime < 100000)
            {
                connect->halfSplitSpeed = (connect->halfSplitSpeed > HALF_SPLIT_MOVE_MAX_SPEED) ?
                    HALF_SPLIT_MOVE_MAX_SPEED : connect->halfSplitSpeed + 1;
            }
            else
            {
                connect->halfSplitSpeed = 0;
            }
            mc_move_half_split(connect->control, 0 /* left or down */, connect->halfSplitSpeed);
            break;
        case 'h':
            elapsedTime = get_elapsed_time(&connect->halfSplitMovePressedTime);
            if (elapsedTime < 100000)
            {
                connect->halfSplitSpeed = (connect->halfSplitSpeed > HALF_SPLIT_MOVE_MAX_SPEED) ?
                    HALF_SPLIT_MOVE_MAX_SPEED : connect->halfSplitSpeed + 1;
            }
            else
            {
                connect->halfSplitSpeed = 0;
            }
            mc_move_half_split(connect->control, 1 /* right or up */, connect->halfSplitSpeed);
            break;
            
        case XK_Return:
            if (mode == MENU_MODE)
            {
                mc_select_menu_item_extra(connect->control);
            }
            break;
            
        case XK_Delete:
            if (mode == MENU_MODE)
            {
                mc_select_menu_item_center(connect->control);
            }
            break;

        case '-':
            mc_toggle_show_source_name(connect->control);
            break;
            
        case XK_KP_Add:
            mc_toggle_show_audio_level(connect->control);
            break;
            
        default:
            break;
    }
}

static void default_key_released(void* data, int key)
{
    KeyboardConnect* connect = (KeyboardConnect*)data;
    MediaControlMode mode = mc_get_mode(connect->control);

    switch (key) 
    {    
        case XK_Up:
            if (mode != MENU_MODE)
            {
                /* back to normal speed */
                mc_play_speed(connect->control, 1, FRAME_PLAY_UNIT);
            }
            break;
        case XK_Down:
            if (mode != MENU_MODE)
            {
                /* back to normal speed */
                mc_play_speed(connect->control, 1, FRAME_PLAY_UNIT);
            }
            break;
            
        default:
            break;
    }
}

static void qc_key_pressed(void* data, int key)
{
    KeyboardConnect* connect = (KeyboardConnect*)data;
    long elapsedTime;
    MediaControlMode mode = mc_get_mode(connect->control);
    int i;

    if (mode == MENU_MODE)
    {
        switch (key)
        {
            case 'q':
                mc_stop(connect->control);
                break;
            case XK_space:
                mc_pause(connect->control);
                break;
            case 'o':
                mc_next_osd_screen(connect->control);
                break;
            case XK_Up:
                mc_previous_menu_item(connect->control);
                break;
            case XK_Down:
                mc_next_menu_item(connect->control);
                break;
            case XK_Return:
                mc_select_menu_item_extra(connect->control);
                break;
            case XK_Left:
                mc_select_menu_item_left(connect->control);
                break;
            case XK_Right:
                mc_select_menu_item_right(connect->control);
                break;
            case XK_Page_Up:
                for (i = 0; i < 10; i++)
                {
                    mc_previous_menu_item(connect->control);
                }
                break;
            case XK_Page_Down:
                for (i = 0; i < 10; i++)
                {
                    mc_next_menu_item(connect->control);
                }
                break;
            case XK_Delete:
                mc_select_menu_item_center(connect->control);
                break;
                
            default:
                break;
        }
    }
    else
    {
        switch (key)
        {
            case 'j':
                if (!connect->reverse)
                {
                    connect->reverse = 1;
                    connect->speedIndex = 0;
                }
                else
                {
                    connect->speedIndex = (connect->speedIndex < NUM_SPEEDS - 1) ? 
                        (connect->speedIndex + 1) : connect->speedIndex;
                }
                mc_play_speed(connect->control, g_reverseKeyboardSpeed[connect->speedIndex], FRAME_PLAY_UNIT);
                break;
            case 'l':
                if (connect->reverse)
                {
                    connect->reverse = 0;
                    connect->speedIndex = 0;
                }
                else
                {
                    connect->speedIndex = (connect->speedIndex < NUM_SPEEDS - 1) ? 
                        (connect->speedIndex + 1) : connect->speedIndex;
                }
                mc_play_speed(connect->control, g_keyboardSpeed[connect->speedIndex], FRAME_PLAY_UNIT);
                break;
            case 'k':
                connect->speedIndex = -1;
                connect->reverse = 0;
                mc_pause(connect->control);
                break;
            case 'i':
                mc_toggle_lock(connect->control);
                break;
            case 'q':
                mc_stop(connect->control);
                break;
            case XK_space:
                connect->speedIndex = -1;
                connect->reverse = 0;
                mc_toggle_play_pause(connect->control);
                break;
            case XK_Right:
                mc_step(connect->control, 1, FRAME_PLAY_UNIT);
                break;
            case XK_Left:
                mc_step(connect->control, 0, FRAME_PLAY_UNIT);
                break;
            case XK_Up:
                /* 10 frames speed up forwards */
                mc_play_speed(connect->control, 10, FRAME_PLAY_UNIT);
                break;
            case XK_Down:
                /* 10 frames speed up backwards */
                mc_play_speed(connect->control, -10, FRAME_PLAY_UNIT);
                break;
            case XK_Page_Up:
                /* step 1 minute backwards */
                mc_seek(connect->control, -1500, SEEK_CUR, FRAME_PLAY_UNIT);
                break;
            case XK_Page_Down:
                /* step 1 minute forwards */
                mc_seek(connect->control, 1500, SEEK_CUR, FRAME_PLAY_UNIT);
                break;
            case 'm':
                mc_mark(connect->control, M0_MARK_TYPE, 1);
                break;
            case 'c':
                /* clear mark except for PSE failures and D3 VTR errors */
                mc_clear_mark(connect->control, ALL_MARK_TYPE & ~D3_VTR_ERROR_MARK_TYPE & ~D3_PSE_FAILURE_MARK_TYPE);
                break;
            case 'b':
                /* clear all marks except for PSE failures and D3 VTR errors */
                mc_clear_all_marks(connect->control, ALL_MARK_TYPE & ~D3_VTR_ERROR_MARK_TYPE & ~D3_PSE_FAILURE_MARK_TYPE);
                break;
            case ',':
                mc_seek_prev_mark(connect->control);
                break;
            case '.':
                mc_seek_next_mark(connect->control);
                break;
            case '/':
                mc_seek_clip_mark(connect->control);
                break;
            case XK_Home:
                mc_seek(connect->control, 0, SEEK_SET, FRAME_PLAY_UNIT);
                break;
            case XK_End:
                mc_seek(connect->control, 0, SEEK_END, FRAME_PLAY_UNIT);
                break;
            case 'o':
                mc_next_osd_screen(connect->control);
                break;
            case 't':
                mc_next_osd_timecode(connect->control);
                break;
            case '0':
                mc_switch_video(connect->control, 0);
                break;
            case '1':
                mc_switch_video(connect->control, 1);
                break;
            case '2':
                mc_switch_video(connect->control, 2);
                break;
            case '3':
                mc_switch_video(connect->control, 3);
                break;
            case '4':
                mc_switch_video(connect->control, 4);
                break;
            case '5':
                mc_switch_video(connect->control, 5);
                break;
            case '6':
                mc_switch_video(connect->control, 6);
                break;
            case '7':
                mc_switch_video(connect->control, 7);
                break;
            case '8':
                mc_switch_video(connect->control, 8);
                break;
            case '9':
                mc_switch_video(connect->control, 9);
                break;
            case 'a':
                mc_review_start(connect->control, connect->reviewDuration * 25);
                break;
            case 'z':
                mc_review_end(connect->control, connect->reviewDuration * 25);
                break;
            case 'x':
                mc_review(connect->control, connect->reviewDuration * 25);
                break;
            case 's':
                mc_set_half_split_orientation(connect->control, -1 /* toggle */);
                break;
            case 'd':
                mc_set_half_split_type(connect->control, -1 /* toggle */);
                break;
            case 'f':
                mc_show_half_split(connect->control, -1 /* toggle */);
                break;
            case 'g':
                elapsedTime = get_elapsed_time(&connect->halfSplitMovePressedTime);
                if (elapsedTime < 100000)
                {
                    connect->halfSplitSpeed = (connect->halfSplitSpeed > HALF_SPLIT_MOVE_MAX_SPEED) ?
                        HALF_SPLIT_MOVE_MAX_SPEED : connect->halfSplitSpeed + 1;
                }
                else
                {
                    connect->halfSplitSpeed = 0;
                }
                mc_move_half_split(connect->control, 0 /* left or down */, connect->halfSplitSpeed);
                break;
            case 'h':
                elapsedTime = get_elapsed_time(&connect->halfSplitMovePressedTime);
                if (elapsedTime < 100000)
                {
                    connect->halfSplitSpeed = (connect->halfSplitSpeed > HALF_SPLIT_MOVE_MAX_SPEED) ?
                        HALF_SPLIT_MOVE_MAX_SPEED : connect->halfSplitSpeed + 1;
                }
                else
                {
                    connect->halfSplitSpeed = 0;
                }
                mc_move_half_split(connect->control, 1 /* right or up */, connect->halfSplitSpeed);
                break;
                
            case '\'':
                mc_toggle_show_source_name(connect->control);
                break;
                
            default:
                break;
        }
    }
}

void qc_key_released(void* data, int key)
{
    KeyboardConnect* connect = (KeyboardConnect*)data;
    MediaControlMode mode = mc_get_mode(connect->control);

    if (mode != MENU_MODE)
    {
        switch (key) 
        {    
            case XK_Up:
                /* back to normal speed */
                mc_play_speed(connect->control, 1, FRAME_PLAY_UNIT);
                break;
            case XK_Down:
                /* back to normal speed */
                mc_play_speed(connect->control, 1, FRAME_PLAY_UNIT);
                break;
                
            default:
                break;
        }
    }
}


int kic_create_keyboard_connect(int reviewDuration, MediaControl* control, 
    KeyboardInput* input, ConnectMapping mapping, KeyboardConnect** connect)
{
    KeyboardConnect* newConnect;

    CALLOC_ORET(newConnect, KeyboardConnect, 1);
    
    newConnect->reviewDuration = reviewDuration;
    newConnect->control = control;
    newConnect->input = input;

    newConnect->speedIndex = -1;
    
    newConnect->listener.data = newConnect;
    switch (mapping)
    {
        case QC_MAPPING:
            newConnect->listener.key_pressed = qc_key_pressed;
            newConnect->listener.key_released = qc_key_released;
            break;
            
        default:            
            newConnect->listener.key_pressed = default_key_pressed;
            newConnect->listener.key_released = default_key_released;
            break;
    }
    kip_set_listener(input, &newConnect->listener);
    
    *connect = newConnect;
    return 1;
}

void kic_free_keyboard_connect(KeyboardConnect** connect)
{
    if (*connect == NULL)
    {
        return;
    }
    
    kip_unset_listener((*connect)->input);
    
    SAFE_FREE(connect);
}



