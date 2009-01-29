/*
 * $Id: shuttle_input.h,v 1.3 2009/01/29 07:10:27 stuart_hc Exp $
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

#ifndef __SHUTTLE_INPUT_H__
#define __SHUTTLE_INPUT_H__



#ifdef __cplusplus
extern "C"
{
#endif


typedef struct ShuttleInput ShuttleInput;

typedef enum
{
    SH_PING_EVENT = 0,
    SH_KEY_EVENT,
    SH_SHUTTLE_EVENT,
    SH_JOG_EVENT
} ShuttleEventType;

typedef struct
{
    unsigned int number;
    int isPressed; /* otherwise key is released */
} ShuttleKeyEvent;

typedef struct
{
    int clockwise; /* otherwise anti-clockwise */
    unsigned int speed; /* value 0..7 */
} ShuttleShuttleEvent;

typedef struct
{
    int clockwise; /* otherwise anti-clockwise */
    int position;
} ShuttleJogEvent;

typedef struct
{
    ShuttleEventType type;
    union
    {
        ShuttleKeyEvent key;
        ShuttleShuttleEvent shuttle;
        ShuttleJogEvent jog;
    } value;
} ShuttleEvent;


typedef void (*shuttle_listener)(void* data, ShuttleEvent* event);



int shj_open_shuttle(ShuttleInput** shuttle);
int shj_register_listener(ShuttleInput* shuttle, shuttle_listener listener, void* data);
void shj_unregister_listener(ShuttleInput* shuttle, shuttle_listener listener);
void shj_start_shuttle(ShuttleInput* shuttle);
void shj_close_shuttle(ShuttleInput** shuttle);

void shj_stop_shuttle(void* shuttle);


#ifdef __cplusplus
}
#endif


#endif



