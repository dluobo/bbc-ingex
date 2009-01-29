/*
 * $Id: mouse_input.h,v 1.3 2009/01/29 07:10:26 stuart_hc Exp $
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

#ifndef __MOUSE_INPUT_H__
#define __MOUSE_INPUT_H__



#ifdef __cplusplus
extern "C"
{
#endif


typedef struct
{
    void* data; /* passed to functions */

    void (*click)(void* data, int imageWidth, int imageHeight, int xPos, int yPos);
} MouseInputListener;

typedef struct
{
    void* data; /* passed to functions */

    void (*set_listener)(void* data, MouseInputListener* listener);
    void (*unset_listener)(void* data);
    void (*close)(void* data);
} MouseInput;


/* utility functions for calling MouseInputListener functions */

void mil_click(MouseInputListener* listener, int imageWidth, int imageHeight, int xPos, int yPos);


/* utility functions for calling MouseInput functions */

void mip_set_listener(MouseInput* input, MouseInputListener* listener);
void mip_unset_listener(MouseInput* input);
void mip_close(MouseInput* input);




#ifdef __cplusplus
}
#endif



#endif


