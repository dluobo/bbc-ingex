/*
 * $Id: media_player.h,v 1.9 2010/01/12 16:32:29 john_f Exp $
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

#ifndef __MEDIA_PLAYER_H__
#define __MEDIA_PLAYER_H__


#ifdef __cplusplus
extern "C"
{
#endif


#include "media_source.h"
#include "media_sink.h"
#include "media_control.h"
#include "connection_matrix.h"
#include "frame_info.h"
#include "vtr_error_source.h"


typedef struct MediaPlayer MediaPlayer;

typedef struct
{
    int lockedChanged;
    int locked; /* 1 == controls are locked */

    int playChanged;
    int play; /* 0 == pause, 1 == play */

    int stopChanged;
    int stop; /* 1 == stopped */

    int speedChanged;
    int speed; /* 1 == normal, > 0 means playing forwards, < 0 backwards, units is frames */

    FrameInfo displayedFrameInfo;
} MediaPlayerStateEvent;

typedef struct
{
    void* data; /* passed to functions */

    void (*frame_displayed_event)(void* data, const FrameInfo* frameInfo);
    void (*frame_dropped_event)(void* data, const FrameInfo* lastFrameInfo);
    void (*state_change_event)(void* data, const MediaPlayerStateEvent* event);
    void (*end_of_source_event)(void* data, const FrameInfo* lastReadFrameInfo);
    void (*start_of_source_event)(void* data, const FrameInfo* firstReadFrameInfo);

    /* called when the player is closed (free'd) */
    void (*player_closed)(void* data);
} MediaPlayerListener;


void mpl_frame_displayed_event(MediaPlayerListener* listener, const FrameInfo* frameInfo);
void mpl_frame_dropped_event(MediaPlayerListener* listener, const FrameInfo* lastFrameInfo);
void mpl_state_change_event(MediaPlayerListener* listener, const MediaPlayerStateEvent* event);
void mpl_end_of_source_event(MediaPlayerListener* listener, const FrameInfo* lastFrameInfo);
void mpl_start_of_source_event(MediaPlayerListener* listener, const FrameInfo* firstFrameInfo);
void mpl_player_closed(MediaPlayerListener* listener);



typedef struct
{
    void* data; /* passed to functions */

    void (*refresh_required)(void* data);
} MenuHandlerListener;

typedef struct
{
    void* data; /* passed to functions */

    void (*set_listener)(void* data, MenuHandlerListener* listener);

    void (*next_menu_item)(void* data);
    void (*previous_menu_item)(void* data);
    void (*select_menu_item_left)(void* data);
    void (*select_menu_item_right)(void* data);
    void (*select_menu_item_center)(void* data);
    void (*select_menu_item_extra)(void* data);

    void (*free)(void* data);
} MenuHandler;

void mhl_refresh_required(MenuHandlerListener* listener);

void mnh_set_listener(MenuHandler* handler, MenuHandlerListener* listener);
void mnh_next_menu_item(MenuHandler* handler);
void mnh_previous_menu_item(MenuHandler* handler);
void mnh_select_menu_item_left(MenuHandler* handler);
void mnh_select_menu_item_right(MenuHandler* handler);
void mnh_select_menu_item_center(MenuHandler* handler);
void mnh_select_menu_item_extra(MenuHandler* handler);
void mnh_free(MenuHandler* handler);



typedef struct
{
    int64_t position;
    int type;
    int64_t pairedPosition;
} Mark;

/* note: source and sink ownership is not transferred to player */
int ply_create_player(MediaSource* source, MediaSink* sink, int initialLock,
    int closeAtEnd, int numFFMPEGThreads, int useWorkerThreads, int loop, int showFieldSymbol,
    const Timecode* startVITC, const Timecode* startLTC,
    FILE* bufferStateLogFile, int* markSelectionTypeMasks, int numMarkSelections, MediaPlayer** player);
int ply_register_player_listener(MediaPlayer* player, MediaPlayerListener* playerListener);
MediaControl* ply_get_media_control(MediaPlayer* player);
int ply_start_player(MediaPlayer* player, int startPaused);
void ply_close_player(MediaPlayer** player);
int ply_get_marks(MediaPlayer* player, Mark** marks);
MediaSink* ply_get_media_sink(MediaPlayer* player);
void ply_set_menu_handler(MediaPlayer* player, MenuHandler* handler);
void ply_enable_clip_marks(MediaPlayer* player, int markType);
void ply_set_start_offset(MediaPlayer* player, int64_t offset);
void ply_print_source_info(MediaPlayer* player);
void ply_get_frame_rate(MediaPlayer* player, Rational* frameRate);
int ply_register_vtr_error_source(MediaPlayer* player, VTRErrorSource* source);


/* quality checking */
typedef int (*qc_quit_validator_func)(MediaPlayer* player, void* data);

void ply_set_qc_quit_validator(MediaPlayer* player, qc_quit_validator_func func, void* data);
int ply_qc_quit_validate(MediaPlayer* player);

void ply_activate_qc_mark_validation(MediaPlayer* player);


#ifdef __cplusplus
}
#endif


#endif

