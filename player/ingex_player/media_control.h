/*
 * $Id: media_control.h,v 1.9 2010/01/12 16:32:25 john_f Exp $
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

#ifndef __MEDIA_CONTROL_H__
#define __MEDIA_CONTROL_H__



#ifdef __cplusplus
extern "C"
{
#endif

#include "on_screen_display.h"

typedef enum
{
    DEFAULT_MAPPING = 0,
    QC_MAPPING
} ConnectMapping;

typedef enum
{
    FRAME_PLAY_UNIT,
    PERCENTAGE_PLAY_UNIT
} PlayUnit;

typedef enum
{
    PLAY_MODE = 0,
    MENU_MODE
} MediaControlMode;


typedef struct
{
    void* data; /* passed as parameter in function calls */


    /* get the mode which determines which functions should be called */

    MediaControlMode (*get_mode)(void* data);


    /* enable/disable media control (other than this function) */

    void (*toggle_lock)(void* data);


    /* audio/video stream control */

    void (*play)(void* data);
    void (*stop)(void* data);
    void (*pause)(void* data);
    void (*toggle_play_pause)(void* data);
    void (*seek)(void* data, int64_t offset, int whence /* SEEK_SET, SEEK_CUR or SEEK_END */,
        PlayUnit unit /* offset is in units of 1/1000 % if unit is PERCENTAGE_PLAY_UNIT */);
    void (*play_speed)(void* data, int speed, PlayUnit unit);
    void (*play_speed_factor)(void* data, float factor);
    void (*step)(void* data, int forward /* else backward */, PlayUnit unit);
    void (*mute_audio)(void* data, int mute /* -1=toggle, 0=not mute, 1=mute */);


    /* mark-in and mark-out controls */

    void (*mark)(void* data, int type, int toggle);
    void (*mark_position)(void* data, int64_t position, int type, int toggle);
    void (*clear_mark)(void* data, int typeMask /* ALL_MARK_TYPE will clear all mark types */);
    void (*clear_mark_position)(void* data, int64_t position, int typeMask /* ALL_MARK_TYPE will clear all mark types */);
    void (*clear_all_marks)(void* data, int typeMask /* ALL_MARK_TYPE will clear all mark types */);
    void (*seek_next_mark)(void* data);
    void (*seek_prev_mark)(void* data);
    void (*seek_clip_mark)(void* data);
    void (*next_active_mark_selection)(void* data);
    void (*set_vtr_error_level)(void* data, VTRErrorLevel level);
    void (*next_vtr_error_level)(void* data);


    /* on screen display */

    void (*set_osd_screen)(void* data, OSDScreen screen);
    void (*next_osd_screen)(void* data);
    void (*set_osd_timecode)(void* data, int index, int type, int subType);
    void (*next_osd_timecode)(void* data);
    void (*toggle_show_audio_level)(void* data);


    /* video switch control */

    void (*switch_next_video)(void* data);
    void (*switch_prev_video)(void* data);
    void (*switch_video)(void* data, int index);
    void (*show_source_name)(void* data, int enable);
    void (*toggle_show_source_name)(void* data);


    /* audio switch control */

    void (*switch_next_audio_group)(void* data);
    void (*switch_prev_audio_group)(void* data);
    void (*switch_audio_group)(void* data, int index);
    void (*snap_audio_to_video)(void* data);


    /* functions for sinks supporting half-split */

    void (*set_half_split_orientation)(void* data, int vertical /* -1 == toggle */);
    void (*set_half_split_type)(void* data, int type /* -1 == toggle, else see HalfSplitType enum */);
    void (*show_half_split)(void* data, int showSplitDivide /* -1 == toggle */);
    void (*move_half_split)(void* data, int rightOrDown, int speed /* 0...5 */);


    /* review */

    void (*review_start)(void* data, int64_t duration);
    void (*review_end)(void* data, int64_t duration);
    void (*review)(void* data, int64_t duration);


    /* menu (MENU_MODE) */

    void (*next_menu_item)(void* data);
    void (*previous_menu_item)(void* data);
    void (*select_menu_item_left)(void* data);
    void (*select_menu_item_right)(void* data);
    void (*select_menu_item_center)(void* data);
    void (*select_menu_item_extra)(void* data);


} MediaControl;



/* utility functions for calling MediaControl functions */

MediaControlMode mc_get_mode(MediaControl* control);

void mc_toggle_lock(MediaControl* control);
void mc_play(MediaControl* control);
void mc_stop(MediaControl* control);
void mc_pause(MediaControl* control);
void mc_toggle_play_pause(MediaControl* control);
void mc_seek(MediaControl* control, int64_t offset, int whence, PlayUnit unit);
void mc_play_speed(MediaControl* control, int speed, PlayUnit unit);
void mc_play_speed_factor(MediaControl* control, float factor);
void mc_step(MediaControl* control, int forward, PlayUnit unit);
void mc_mute_audio(MediaControl* control, int mute);

void mc_mark(MediaControl* control, int type, int toggle);
void mc_mark_position(MediaControl* control, int64_t position, int type, int toggle);
void mc_clear_mark(MediaControl* control, int typeMask);
void mc_clear_mark_position(MediaControl* control, int64_t position, int typeMask);
void mc_clear_all_marks(MediaControl* control, int typeMask);
void mc_seek_next_mark(MediaControl* control);
void mc_seek_prev_mark(MediaControl* control);
void mc_seek_clip_mark(MediaControl* control);
void mc_next_active_mark_selection(MediaControl* control);
void mc_set_vtr_error_level(MediaControl* control, VTRErrorLevel level);
void mc_next_vtr_error_level(MediaControl* control);

void mc_set_osd_screen(MediaControl* control, OSDScreen screen);
void mc_next_osd_screen(MediaControl* control);
void mc_set_osd_timecode(MediaControl* control, int index, int type, int subType);
void mc_next_osd_timecode(MediaControl* control);
void mc_toggle_show_audio_level(MediaControl* control);

void mc_switch_next_video(MediaControl* control);
void mc_switch_prev_video(MediaControl* control);
void mc_switch_video(MediaControl* control, int index);
void mc_show_source_name(MediaControl* control, int enable);
void mc_toggle_show_source_name(MediaControl* control);

void mc_switch_next_audio_group(MediaControl* control);
void mc_switch_prev_audio_group(MediaControl* control);
void mc_switch_audio_group(MediaControl* control, int index);
void mc_snap_audio_to_video(MediaControl* control);

void mc_set_half_split_orientation(MediaControl* sink, int vertical);
void mc_set_half_split_type(MediaControl* sink, int type);
void mc_show_half_split(MediaControl* sink, int showSplitDivide);
void mc_move_half_split(MediaControl* sink, int rightOrDown, int speed);

void mc_review_start(MediaControl* control, int64_t duration);
void mc_review_end(MediaControl* control, int64_t duration);
void mc_review(MediaControl* control, int64_t duration);

void mc_next_menu_item(MediaControl* control);
void mc_previous_menu_item(MediaControl* control);
void mc_select_menu_item_left(MediaControl* control);
void mc_select_menu_item_right(MediaControl* control);
void mc_select_menu_item_center(MediaControl* control);
void mc_select_menu_item_extra(MediaControl* control);


#ifdef __cplusplus
}
#endif


#endif

