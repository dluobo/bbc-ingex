/*
 * $Id: progress_bar_input_connect.h,v 1.3 2009/01/29 07:10:26 stuart_hc Exp $
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

#ifndef __PROGRESS_BAR_INPUT_CONNECT_H__
#define __PROGRESS_BAR_INPUT_CONNECT_H__



#ifdef __cplusplus
extern "C"
{
#endif


#include "media_control.h"
#include "progress_bar_input.h"


typedef struct ProgressBarConnect ProgressBarConnect;


int pic_create_progress_bar_connect(MediaControl* control, ProgressBarInput* input,
    ProgressBarConnect** connect);
void pic_free_progress_bar_connect(ProgressBarConnect** connect);


#ifdef __cplusplus
}
#endif



#endif


