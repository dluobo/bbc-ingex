/*
 * $Id: MtEncoder.cpp,v 1.1 2009/06/12 17:41:02 john_f Exp $
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

#include "Block.h"
#include "Package.h"
#include "CodedFrameBuffer.h"
#include "MtEncoder.h"

const int DEFAULT_NUM_THREADS = 3;

MtEncoder::MtEncoder(CodedFrameBuffer * cfb, ACE_Thread_Mutex * mutex)
: ACE_Task<ACE_MT_SYNCH>(),
  mpCodedFrameBuffer(cfb), mpAvcodecMutex(mutex), mShutdown(0), mOffsetToFrameNumber(0)
{
}

MtEncoder::~MtEncoder()
{
    wait();
}

void MtEncoder::Init(ffmpeg_encoder_resolution_t res, int num_threads, int offset_to_frame_number)
{
    mRes = res;
    if (num_threads < 1)
    {
        num_threads = DEFAULT_NUM_THREADS;
    }
    mNumThreads = num_threads;
    mOffsetToFrameNumber = offset_to_frame_number;
    //this->activate(THR_NEW_LWP | THR_DETACHED | THR_INHERIT_SCHED, num_threads);
    this->activate(THR_NEW_LWP | THR_JOINABLE | THR_INHERIT_SCHED, num_threads);
}

void MtEncoder::Encode(void * p_video, int index)
{
    FramePackage * fp = new FramePackage(p_video, index);
    MessageBlock * mb = new MessageBlock(fp);
    putq(mb);
}

void MtEncoder::Close()
{
    // There must be a better way, but ...
    for (int i = 0; i < mNumThreads; ++i)
    {
        ACE_Message_Block * mb = new ACE_Message_Block();
        mb->msg_type(ACE_Message_Block::MB_HANGUP);
        putq(mb);
    }
}

int MtEncoder::svc()
{
    // Init ffmpeg encoder
    ffmpeg_encoder_t * enc = 0;
    {
        // Prevent "insufficient thread locking around avcodec_open/close()"
        ACE_Guard<ACE_Thread_Mutex> guard(*mpAvcodecMutex);

        enc = ffmpeg_encoder_init(mRes, 1);
    }

    while (!mShutdown)
    {
        ACE_Message_Block * mb;
        int n = getq(mb);

        if (n == -1)
        {
        // could be terminated due to task shutdown;
            mShutdown = 1;
        }
        else if (ACE_Message_Block::MB_DATA != mb->msg_type())
        {
            // shutdown message
            mShutdown = 1;
            mb->release();
        }
        else
        {
            // Extract frame data.
            DataBlock * db = ACE_dynamic_cast(DataBlock *, mb->data_block());
            Package * p = db->data();
            FramePackage * fp = ACE_dynamic_cast(FramePackage *, p);

            // Encode the frame.
            uint8_t * p_enc_video = 0;
            int size_enc_video = ffmpeg_encoder_encode(enc, (uint8_t *)fp->FrameData(), &p_enc_video);

            // Check the frame was still in memory
            bool err = false;
            int frame_number_in_memory = * (int *) ((uint8_t *)fp->FrameData() + mOffsetToFrameNumber);
            if (frame_number_in_memory != fp->Index())
            {
                err = true;
                ACE_DEBUG((LM_ERROR, ACE_TEXT("MtEncoder missed frame %d, it was actually %d!\n"),
                    fp->Index(), frame_number_in_memory));
            }

            // Queue coded frame.
            mpCodedFrameBuffer->QueueFrame(p_enc_video, size_enc_video, fp->Index(), err);

            mb->release();
        }
    }

    // Close ffmpeg encoder
    {
        // Prevent "insufficient thread locking around avcodec_open/close()"
        ACE_Guard<ACE_Thread_Mutex> guard(*mpAvcodecMutex);

        ffmpeg_encoder_close(enc);
    }

    ACE_DEBUG(( LM_DEBUG, "MtEncoder::svc exiting\n" ));

    return 0;
}




