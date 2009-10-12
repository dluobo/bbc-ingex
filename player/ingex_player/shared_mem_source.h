/*
 * $Id: shared_mem_source.h,v 1.6 2009/10/12 16:06:30 philipn Exp $
 *
 *
 *
 * Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
 * Author: Stuart Cunningham
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

#ifndef __SHARED_MEM_SOURCE_H__
#define __SHARED_MEM_SOURCE_H__


#ifdef __cplusplus
extern "C"
{
#endif


#include "media_source.h"


typedef struct SharedMemSource SharedMemSource;


int shms_open(const char *channel_name, double timeout, SharedMemSource** source);
MediaSource* shms_get_media_source(SharedMemSource* source);

int shms_get_default_timecode(SharedMemSource* source, TimecodeType* type, TimecodeSubType* subType);



#ifdef __cplusplus
}
#endif


#endif

