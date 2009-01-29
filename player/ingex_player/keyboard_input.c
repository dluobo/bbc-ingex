/*
 * $Id: keyboard_input.c,v 1.4 2009/01/29 07:10:26 stuart_hc Exp $
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


#include "keyboard_input.h"


void kil_key_pressed(KeyboardInputListener* listener, int key, int modifier)
{
    if (listener && listener->key_pressed)
    {
        listener->key_pressed(listener->data, key, modifier);
    }
}

void kil_key_released(KeyboardInputListener* listener, int key, int modifier)
{
    if (listener && listener->key_released)
    {
        listener->key_released(listener->data, key, modifier);
    }
}

void kip_set_listener(KeyboardInput* input, KeyboardInputListener* listener)
{
    if (input && input->set_listener)
    {
        input->set_listener(input->data, listener);
    }
}

void kip_unset_listener(KeyboardInput* input)
{
    if (input && input->unset_listener)
    {
        input->unset_listener(input->data);
    }
}

void kip_close(KeyboardInput* input)
{
    if (input && input->close)
    {
        input->close(input->data);
    }
}





