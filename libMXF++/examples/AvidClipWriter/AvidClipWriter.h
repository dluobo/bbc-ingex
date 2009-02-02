/*
 * $Id: AvidClipWriter.h,v 1.1 2009/02/02 05:14:33 stuart_hc Exp $
 *
 * 
 *
 * Copyright (C) 2008  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#ifndef __AVID_CLIP_WRITER_H__
#define __AVID_CLIP_WRITER_H__


#include <string>
#include <map>

#include <libMXF++/MXF.h>
#include <CommonTypes.h>



typedef enum
{
    AVID_PAL_25I_PROJECT,
    AVID_NTSC_30I_PROJECT
} AvidProjectFormat;

typedef enum
{
    AVID_MJPEG21_ESSENCE,
    AVID_MJPEG31_ESSENCE,
    AVID_MJPEG101_ESSENCE,
    AVID_MJPEG101m_ESSENCE,
    AVID_MJPEG151s_ESSENCE,
    AVID_MJPEG201_ESSENCE,
    AVID_IECDV25_ESSENCE,
    AVID_DVBASED25_ESSENCE,
    AVID_DVBASED50_ESSENCE,
    AVID_DV1080I50_ESSENCE,
    AVID_DV720P50_ESSENCE,
    AVID_MPEG30_ESSENCE,
    AVID_MPEG40_ESSENCE,
    AVID_MPEG50_ESSENCE,
    AVID_DNXHD720P120_ESSENCE,
    AVID_DNXHD720P185_ESSENCE,
    AVID_DNXHD1080P36_ESSENCE,
    AVID_DNXHD1080P120_ESSENCE,
    AVID_DNXHD1080P185_ESSENCE,
    AVID_DNXHD1080I120_ESSENCE,
    AVID_DNXHD1080I185_ESSENCE,
    AVID_UNCUYVY_ESSENCE,
    AVID_UNC1080IUYVY_ESSENCE,
    AVID_PCM_ESSENCE
} AvidEssenceType;


typedef union
{
    // video
    int mpegFrameSize;
    
    // audio
    uint32_t quantizationBits;
} EssenceParams;


class TrackData;

class AvidClipWriter 
{
public:
    AvidClipWriter(AvidProjectFormat format, mxfRational imageAspectRatio, bool dropFrame, bool useLegacy);
    virtual ~AvidClipWriter();
    
    
    void setProjectName(std::string name);
    void setClipName(std::string name);
    void setTape(std::string name, int64_t startTimecode);
    void addUserComment(std::string name, std::string value);
    
    void registerEssenceElement(int trackId, int trackNumber, AvidEssenceType type, EssenceParams* params, std::string filename);
    
    
    void prepareToWrite();
    
    void writeSamples(int trackId, uint32_t numSamples, unsigned char* data, uint32_t dataLen);
    
    void completeWrite();
    void abortWrite(bool deleteFiles);
    
private:
    std::string _projectName;
    AvidProjectFormat _format;
    mxfRational _imageAspectRatio;
    bool _dropFrame;
    bool _useLegacy;
    std::string _clipName;
    std::string _tapeName;
    int64_t _startTimecode;
    std::map<std::string, std::string> _userComments;
    
    mxfRational _projectEditRate;
    
    bool _readyToWrite;
    bool _endedWriting;
    
    std::map<int, TrackData*> _tracks;
};



#endif


