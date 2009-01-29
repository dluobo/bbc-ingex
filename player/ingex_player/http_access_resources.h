/*
 * $Id: http_access_resources.h,v 1.3 2009/01/29 07:10:26 stuart_hc Exp $
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

#ifndef __HTTP_ACCESS_RESOURCES_H__
#define __HTTP_ACCESS_RESOURCES_H__


typedef struct
{
    char name[64];

    char contentType[64];
    const unsigned char* data;
    unsigned int dataSize;

    unsigned char* dynData;
} HTTPAccessResource;


typedef struct HTTPAccessResources HTTPAccessResources;

int har_create_resources(HTTPAccessResources** resources);
void har_free_resources(HTTPAccessResources** resources);

int har_add_resource(HTTPAccessResources* resources, HTTPAccessResource* resource);
const HTTPAccessResource* har_get_resource(HTTPAccessResources* resources, const char* name);


#endif


