/*
 * $Id: vtr_error_source.h,v 1.1 2010/01/12 16:44:56 john_f Exp $
 *
 * Source for VTR errors
 *
 * Copyright (C) 2009 British Broadcasting Corporation, All Rights Reserved
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

#ifndef __VTR_ERROR_SOURCE_H__
#define __VTR_ERROR_SOURCE_H__

#include "types.h"
#include "media_control.h"
#include "media_source.h"


typedef struct
{
    void* data; /* passed as parameter in function calls */
    
    void (*set_vtr_error_level)(void* data, VTRErrorLevel level);
    void (*mark_vtr_errors)(void* data, MediaSource* rootSource, MediaControl* mediaControl);
} VTRErrorSource;


void ves_set_vtr_error_level(VTRErrorSource* source, VTRErrorLevel level);
void ves_mark_vtr_errors(VTRErrorSource* source, MediaSource* rootSource, MediaControl* mediaControl);


#endif

