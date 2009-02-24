/*
 * $Id: HTTPPlayerState.h,v 1.1 2009/02/24 08:21:16 stuart_hc Exp $
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

#ifndef __INGEX_HTTP_PLAYER_STATE_H__
#define __INGEX_HTTP_PLAYER_STATE_H__


#include <frame_info.h>
#include <media_player.h>
#include "JSONObject.h"



namespace ingex
{


typedef struct
{
    bool locked; /* true == controls are locked */
    bool play; /* false == pause, true == play */
    bool stop; /* true == stopped */
    int speed; /* 1 == normal, > 0 means playing forwards, < 0 backwards, units is frames */

    FrameInfo displayedFrame;
} HTTPPlayerState;



bool parse_frame_info(JSONObject* json, ::FrameInfo* frameInfo);
bool parse_state_event(JSONObject* json, ::MediaPlayerStateEvent* event);
bool parse_player_state(JSONObject* json, HTTPPlayerState* state);


JSONObject* serialize_frame_info(const ::FrameInfo* frameInfo);
JSONObject* serialize_state_event(const ::MediaPlayerStateEvent* event);
JSONObject* serialize_player_state(const HTTPPlayerState* state);


};



#endif
