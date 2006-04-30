/*
 * $Id: mxf_essence.h,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Functions to support extraction of raw essence data out of MXF files.
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
 
 
#ifndef __MXF_ESSENCE_H__
#define __MXF_ESSENCE_H__

#include <stdio.h>
#include <inttypes.h>


typedef enum _mxfe_EssenceType
{
    MXFE_DV = 0,
    MXFE_WAVPCM,
    MXFE_AVIDMJPEG,
    MXFE_UNKNOWN // leave as last entry
} mxfe_EssenceType;

// returns the essence type supported by this library or MXFE_UNKNOWN
int mxfe_get_essence_type(FILE* f, mxfe_EssenceType* type);

// returns the file offset and length  
int mxfe_get_essence_element_info(FILE* f, uint64_t* offset, uint64_t* len);

// returns a file suffix that can be used for the essence data
int mxfe_get_essence_suffix(mxfe_EssenceType type, const char** suffix);


#endif
