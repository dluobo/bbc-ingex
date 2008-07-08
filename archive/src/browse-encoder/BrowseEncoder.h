/*
 * $Id: BrowseEncoder.h,v 1.1 2008/07/08 16:21:26 philipn Exp $
 *
 * C++ wrapper for browse_encoder
 *
 * Copyright (C) 2008 BBC Research, Philip de Nier, <philipn@users.sourceforge.net>
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

#ifndef __RECORDER_BROWSE_ENCODER_H__
#define __RECORDER_BROWSE_ENCODER_H__


#include "browse_encoder.h"


class BrowseEncoder
{
public:
    static BrowseEncoder* create(const char *filename, uint32_t kbit_rate, int thread_count);
    
public:
    ~BrowseEncoder();
    
    bool encode(uint8_t *p_video, int16_t *p_audio, ArchiveTimecode ltc, ArchiveTimecode vitc, int32_t frame_number);
    
private:
    BrowseEncoder(browse_encoder_t* encoder);
    
    browse_encoder_t* _encoder;
};






#endif


