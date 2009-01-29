/*
 * $Id: shuttle_input_connect.h,v 1.5 2009/01/29 07:10:27 stuart_hc Exp $
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

#ifndef __SHUTTLE_INPUT_CONNECT_H__
#define __SHUTTLE_INPUT_CONNECT_H__


#ifdef __cplusplus
extern "C"
{
#endif


#include "media_control.h"
#include "shuttle_input.h"
#include "types.h"


typedef struct ShuttleConnect ShuttleConnect;


int sic_create_shuttle_connect(int reviewDuration, MediaControl* control,
    ShuttleInput* shuttle, ConnectMapping mapping, ShuttleConnect** connect);
void sic_free_shuttle_connect(ShuttleConnect** connect);

const ControlInputHelp* sic_get_default_control_help();
const ControlInputHelp* sic_get_qc_control_help();



#ifdef __cplusplus
}
#endif


#endif

