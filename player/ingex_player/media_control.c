#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "media_control.h"


MediaControlMode mc_get_mode(MediaControl* control)
{
    if (control && control->get_mode)
    {
        return control->get_mode(control->data);
    }
    return PLAY_MODE;
}


void mc_toggle_lock(MediaControl* control)
{
    if (control && control->toggle_lock)
    {
        control->toggle_lock(control->data);
    }
}

void mc_play(MediaControl* control)
{
    if (control && control->play)
    {
        control->play(control->data);
    }
}

void mc_stop(MediaControl* control)
{
    if (control && control->stop)
    {
        control->stop(control->data);
    }
}

void mc_pause(MediaControl* control)
{
    if (control && control->pause)
    {
        control->pause(control->data);
    }
}

void mc_toggle_play_pause(MediaControl* control)
{
    if (control && control->toggle_play_pause)
    {
        control->toggle_play_pause(control->data);
    }
}

void mc_seek(MediaControl* control, int64_t offset, int whence, PlayUnit unit)
{
    assert(whence == SEEK_SET || whence == SEEK_CUR || whence == SEEK_END);
    
    if (control && control->seek)
    {
        control->seek(control->data, offset, whence, unit);
    }
}

void mc_play_speed(MediaControl* control, int speed, PlayUnit unit)
{
    assert(speed >= 1 || speed <= -1);
    
    if (control && control->play_speed)
    {
        control->play_speed(control->data, speed, unit);
    }
}

void mc_step(MediaControl* control, int forward, PlayUnit unit)
{
    if (control && control->step)
    {
        control->step(control->data, forward, unit);
    }
}

void mc_mark(MediaControl* control, int type, int toggle)
{
    if (control && control->mark)
    {
        control->mark(control->data, type, toggle);
    }
}

void mc_mark_position(MediaControl* control, int64_t position, int type, int toggle)
{
    if (control && control->mark_position)
    {
        control->mark_position(control->data, position, type, toggle);
    }
}

void mc_clear_mark(MediaControl* control, int typeMask)
{
    if (control && control->clear_mark)
    {
        control->clear_mark(control->data, typeMask);
    }
}

void mc_clear_all_marks(MediaControl* control, int typeMask)
{
    if (control && control->clear_all_marks)
    {
        control->clear_all_marks(control->data, typeMask);
    }
}

void mc_seek_next_mark(MediaControl* control)
{
    if (control && control->seek_next_mark)
    {
        control->seek_next_mark(control->data);
    }
}

void mc_seek_prev_mark(MediaControl* control)
{
    if (control && control->seek_prev_mark)
    {
        control->seek_prev_mark(control->data);
    }
}

void mc_next_osd_screen(MediaControl* control)
{
    if (control && control->next_osd_screen)
    {
        control->next_osd_screen(control->data);
    }
}

void mc_set_osd_screen(MediaControl* control, OSDScreen screen)
{
    if (control && control->set_osd_screen)
    {
        control->set_osd_screen(control->data, screen);
    }
}

void mc_set_osd_timecode(MediaControl* control, int index, int type, int subType)
{
    if (control && control->set_osd_timecode)
    {
        control->set_osd_timecode(control->data, index, type, subType);
    }
}

void mc_next_osd_timecode(MediaControl* control)
{
    if (control && control->next_osd_timecode)
    {
        control->next_osd_timecode(control->data);
    }
}

void mc_switch_next_video(MediaControl* control)
{
    if (control && control->switch_next_video)
    {
        control->switch_next_video(control->data);
    }
}

void mc_switch_prev_video(MediaControl* control)
{
    if (control && control->switch_prev_video)
    {
        control->switch_prev_video(control->data);
    }
}

void mc_switch_video(MediaControl* control, int index)
{
    if (control && control->switch_video)
    {
        control->switch_video(control->data, index);
    }
}


void mc_set_half_split_orientation(MediaControl* control, int vertical)
{
    if (control && control->set_half_split_orientation)
    {
        control->set_half_split_orientation(control->data, vertical);
    }
}

void mc_set_half_split_type(MediaControl* control, int type)
{
    if (control && control->set_half_split_type)
    {
        control->set_half_split_type(control->data, type);
    }
}

void mc_show_half_split(MediaControl* control, int showSplitDivide)
{
    if (control && control->show_half_split)
    {
        control->show_half_split(control->data, showSplitDivide);
    }
}

void mc_move_half_split(MediaControl* control, int rightOrDown, int speed)
{
    if (control && control->move_half_split)
    {
        control->move_half_split(control->data, rightOrDown, speed);
    }
}


void mc_review_start(MediaControl* control, int64_t duration)
{
    if (control && control->review_start)
    {
        control->review_start(control->data, duration);
    }
}

void mc_review_end(MediaControl* control, int64_t duration)
{
    if (control && control->review_end)
    {
        control->review_end(control->data, duration);
    }
}

void mc_review(MediaControl* control, int64_t duration)
{
    if (control && control->review)
    {
        control->review(control->data, duration);
    }
}

void mc_next_menu_item(MediaControl* control)
{
    if (control && control->next_menu_item)
    {
        control->next_menu_item(control->data);
    }
}

void mc_previous_menu_item(MediaControl* control)
{
    if (control && control->previous_menu_item)
    {
        control->previous_menu_item(control->data);
    }
}

void mc_select_menu_item_left(MediaControl* control)
{
    if (control && control->select_menu_item_left)
    {
        control->select_menu_item_left(control->data);
    }
}

void mc_select_menu_item_right(MediaControl* control)
{
    if (control && control->select_menu_item_right)
    {
        control->select_menu_item_right(control->data);
    }
}

void mc_select_menu_item_center(MediaControl* control)
{
    if (control && control->select_menu_item_center)
    {
        control->select_menu_item_center(control->data);
    }
}

void mc_select_menu_item_extra(MediaControl* control)
{
    if (control && control->select_menu_item_extra)
    {
        control->select_menu_item_extra(control->data);
    }
}

