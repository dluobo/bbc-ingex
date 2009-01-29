/*
 * $Id: progress_bar_input.h,v 1.3 2009/01/29 07:10:26 stuart_hc Exp $
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

#ifndef __PROGRESS_BAR_INPUT_H__
#define __PROGRESS_BAR_INPUT_H__



#ifdef __cplusplus
extern "C"
{
#endif


typedef struct
{
    void* data; /* passed to functions */

    /* position is a value >= 0.0 and < 100.0 */
    void (*position_set)(void* data, float position);
} ProgressBarInputListener;

typedef struct
{
    void* data; /* passed to functions */

    void (*set_listener)(void* data, ProgressBarInputListener* listener);
    void (*unset_listener)(void* data);
    void (*close)(void* data);
} ProgressBarInput;


/* utility functions for calling ProgressBarInputListener functions */

void pil_position_set(ProgressBarInputListener* listener, float position);


/* utility functions for calling ProgressBarInput functions */

void pip_set_listener(ProgressBarInput* input, ProgressBarInputListener* listener);
void pip_unset_listener(ProgressBarInput* input);
void pip_close(ProgressBarInput* input);




#ifdef __cplusplus
}
#endif



#endif


