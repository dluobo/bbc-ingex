/*
 * $Id: progress_bar_input.c,v 1.3 2009/01/29 07:10:26 stuart_hc Exp $
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


#include "progress_bar_input.h"


void pil_position_set(ProgressBarInputListener* listener, float position)
{
    if (listener && listener->position_set)
    {
        listener->position_set(listener->data, position);
    }
}

void pip_set_listener(ProgressBarInput* input, ProgressBarInputListener* listener)
{
    if (input && input->set_listener)
    {
        input->set_listener(input->data, listener);
    }
}

void pip_unset_listener(ProgressBarInput* input)
{
    if (input && input->unset_listener)
    {
        input->unset_listener(input->data);
    }
}

void pip_close(ProgressBarInput* input)
{
    if (input && input->close)
    {
        input->close(input->data);
    }
}





