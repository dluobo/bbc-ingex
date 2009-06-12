/*
 * $Id: MtEncoder.h,v 1.1 2009/06/12 17:41:02 john_f Exp $
 *
 * Video encoder using multiple threads.
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
 * All Rights Reserved.
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

#ifndef MtEncoder_h
#define MtEncoder_h

#include <ace/Task.h>

#include "ffmpeg_encoder.h"

class CodedFrameBuffer;

class MtEncoder : public ACE_Task<ACE_MT_SYNCH>
{
public:
    MtEncoder(CodedFrameBuffer * cfb, ACE_Thread_Mutex * mutex);
    virtual ~MtEncoder();

    void Init(ffmpeg_encoder_resolution_t res, int num_threads, int offset_to_frame_number);
    void Encode(void * p_video, int index);
    void Close();
    virtual int svc();

private:
    CodedFrameBuffer * mpCodedFrameBuffer;
    ACE_Thread_Mutex * mpAvcodecMutex;
    ffmpeg_encoder_resolution_t mRes;
    int mNumThreads;
    int mShutdown;
    int mOffsetToFrameNumber;
};


#endif // ifndef MtEncoder_h

