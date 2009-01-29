/*
 * $Id: connection_matrix.h,v 1.3 2009/01/29 07:10:26 stuart_hc Exp $
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

#ifndef __CONNECTION_MATRIX_H__
#define __CONNECTION_MATRIX_H__


#ifdef __cplusplus
extern "C"
{
#endif


#include "media_source.h"
#include "media_sink.h"


typedef struct ConnectionMatrix ConnectionMatrix;


int stm_create_connection_matrix(MediaSource* source, MediaSink* sink, int numFFMPEGThreads,
    int useWorkerThreads, ConnectionMatrix** matrix);
MediaSourceListener* stm_get_stream_listener(ConnectionMatrix* matrix);
int stm_sync(ConnectionMatrix* matrix);
void stm_close(ConnectionMatrix** matrix);


#ifdef __cplusplus
}
#endif


#endif


