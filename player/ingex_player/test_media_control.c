/*
 * $Id: test_media_control.c,v 1.4 2009/01/29 07:10:27 stuart_hc Exp $
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "shuttle_input_connect.h"
#include "x11_keyboard_input_connect.h"
#include "term_keyboard_input_connect.h"


#define CHECK_FATAL(cond) \
    if (!(cond)) \
    { \
        fprintf(stderr, "%s failed in %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    }


static void tmc_toggle_lock(void* data)
{
    printf("Toggle lock\n");
}

static void tmc_play(void* data)
{
    printf("Play\n");
}

static void tmc_stop(void* data)
{
    printf("Stop\n");
}

static void tmc_pause(void* data)
{
    printf("Pause\n");
}

static void tmc_toggle_play_pause(void* data)
{
    printf("Toggle play pause\n");
}

static void tmc_seek(void* data, int64_t offset, int whence)
{
    if (whence == SEEK_SET)
    {
        printf("SEEK_SET with offset %lld\n", offset);
    }
    else if (whence == SEEK_CUR)
    {
        printf("SEEK_CUR with offset %lld\n", offset);
    }
    else
    {
        printf("SEEK_END with offset %lld\n", offset);
    }
}

static void tmc_play_speed(void* data, int speed, PlayUnit unit)
{
    printf("Play speed %d %s\n", speed, (unit == FRAME_PLAY_UNIT) ? "frames" : "percentages");
}

static void tmc_step(void* data, int forward, PlayUnit unit)
{
    printf("Step ");
    if (forward)
    {
        printf("forwards 1 %s\n", (unit == FRAME_PLAY_UNIT) ? "frame" : "percentage");
    }
    else
    {
        printf("backwards 1 %s\n", (unit == FRAME_PLAY_UNIT) ? "frame" : "percentage");
    }
}

static void tmc_mark(void* data, int type, int toggle)
{
    printf("Mark %d (%s)\n", type, toggle ? "toggle" : "set");
}

static void tmc_clear_mark(void* data, int typeMask)
{
    printf("Clear mark (mask = 0x%08x)\n", typeMask);
}

static void tmc_clear_all_marks(void* data, int typeMask)
{
    printf("Clear all marks (mask = 0x%08x)\n", typeMask);
}

static void tmc_seek_next_mark(void* data)
{
    printf("Seek next_mark\n");
}

static void tmc_seek_prev_mark(void* data)
{
    printf("Seek previous mark\n");
}

static void tmc_seek_clip_mark(void* data)
{
    printf("Seek clip mark\n");
}

static void tmc_toggle_osd(void* data)
{
    printf("Toggle OSD\n");
}

static void tmc_next_osd_timecode(void* data)
{
    printf("Next OSD timecode\n");
}

static void tmc_switch_next_video(void* data)
{
    printf("Switch to next video\n");
}

static void tmc_switch_prev_video(void* data)
{
    printf("Switch to previous video\n");
}

static void tmc_switch_video(void* data, int index)
{
    printf("Switch video %d\n", index);
}


static void init_media_control(MediaControl* control)
{
    memset(control, 0, sizeof(MediaControl));

    control->data = NULL;
    control->toggle_lock = tmc_toggle_lock;
    control->play = tmc_play;
    control->stop = tmc_stop;
    control->pause = tmc_pause;
    control->toggle_play_pause = tmc_toggle_play_pause;
    control->seek = tmc_seek;
    control->play_speed = tmc_play_speed;
    control->step = tmc_step;
    control->mark = tmc_mark;
    control->clear_mark = tmc_clear_mark;
    control->clear_all_marks = tmc_clear_all_marks;
    control->seek_next_mark = tmc_seek_next_mark;
    control->seek_prev_mark = tmc_seek_prev_mark;
    control->seek_clip_mark = tmc_seek_clip_mark;
    control->toggle_osd = tmc_toggle_osd;
    control->next_osd_timecode = tmc_next_osd_timecode;
    control->switch_next_video = tmc_switch_next_video;
    control->switch_prev_video = tmc_switch_prev_video;
    control->switch_video = tmc_switch_video;
}

static int test_shuttle(int qc)
{
    ShuttleInput* shuttle;
    ShuttleConnect* connect;
    MediaControl control;

    init_media_control(&control);

    CHECK_FATAL(shj_open_shuttle(&shuttle));
    if (qc)
    {
        CHECK_FATAL(sic_create_shuttle_connect(&control, shuttle, QC_SHUTTLE_MAPPING, &connect));
    }
    else
    {
        CHECK_FATAL(sic_create_shuttle_connect(&control, shuttle, DEFAULT_SHUTTLE_MAPPING, &connect));
    }

    shj_start_shuttle(shuttle);

    shj_close_shuttle(&shuttle);
    sic_free_shuttle_connect(&connect);


    return 0;
}

static int test_keyboard()
{
    X11KeyboardConnect* connect;
    MediaControl control;

    init_media_control(&control);

    CHECK_FATAL(create_x11_keyboard_connect(&control, &connect));

    start_x11_keyboard_connect(connect);

    free_x11_keyboard_connect(&connect);


    return 0;
}


static int test_term_keyboard()
{
    TermKeyboardConnect* connect;
    MediaControl control;

    init_media_control(&control);

    CHECK_FATAL(create_term_keyboard_connect(&control, &connect));

    start_term_keyboard_connect(connect);

    free_term_keyboard_connect(&connect);


    return 0;
}


void usage(const char* exe)
{
    fprintf(stderr, "Usage: %s [-h] [-s]\n", exe);
    fprintf(stderr, "\n");
    fprintf(stderr, " -h        Print help message\n");
    fprintf(stderr, " -s        Test the shuttle.\n");
    fprintf(stderr, " -q        Test the shuttle for QC.\n");
    fprintf(stderr, " -t        Test the terminal keyboard input\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "The default is to test the x11 keyboard input\n");
    fprintf(stderr, "\n");
}

int main (int argc, const char** argv)
{
    if (argc == 1)
    {
        test_keyboard();
    }
    else if (argc == 2 &&
        (strcmp(argv[1], "-h") == 0 ||
        strcmp(argv[1], "?") == 0 ||
        strcmp(argv[1], "-help") == 0 ||
        strcmp(argv[1], "--help") == 0))
    {
        usage(argv[0]);
        return 0;
    }
    else if (argc == 2 &&
        (strcmp(argv[1], "-s") == 0))
    {
        test_shuttle(0);
    }
    else if (argc == 2 &&
        (strcmp(argv[1], "-q") == 0))
    {
        test_shuttle(1);
    }
    else if (argc == 2 &&
        (strcmp(argv[1], "-t") == 0))
    {
        test_term_keyboard();
    }
    else
    {
        usage(argv[0]);
        return 0;
    }

    return 0;
}


