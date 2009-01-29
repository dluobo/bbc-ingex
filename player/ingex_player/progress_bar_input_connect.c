/*
 * $Id: progress_bar_input_connect.c,v 1.3 2009/01/29 07:10:26 stuart_hc Exp $
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
#include <assert.h>
#include <sys/time.h>

#include "progress_bar_input_connect.h"
#include "logging.h"
#include "macros.h"


struct ProgressBarConnect
{
    MediaControl* control;

    ProgressBarInput* input;
    ProgressBarInputListener listener;
};


static void pic_position_set(void* data, float position)
{
    ProgressBarConnect* connect = (ProgressBarConnect*)data;

    mc_seek(connect->control, (int64_t)(position * 1000.0), SEEK_SET, PERCENTAGE_PLAY_UNIT);
    mc_pause(connect->control);
}



int pic_create_progress_bar_connect(MediaControl* control, ProgressBarInput* input,
    ProgressBarConnect** connect)
{
    ProgressBarConnect* newConnect;

    CALLOC_ORET(newConnect, ProgressBarConnect, 1);

    newConnect->control = control;
    newConnect->input = input;

    newConnect->listener.data = newConnect;
    newConnect->listener.position_set = pic_position_set;
    pip_set_listener(input, &newConnect->listener);


    *connect = newConnect;
    return 1;
}

void pic_free_progress_bar_connect(ProgressBarConnect** connect)
{
    if (*connect == NULL)
    {
        return;
    }

    pip_unset_listener((*connect)->input);

    SAFE_FREE(connect);
}


