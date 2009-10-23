/*
 * $Id: OPAtomContentPackage.h,v 1.2 2009/10/23 09:05:21 philipn Exp $
 *
 * Holds the essence data for each track in the clip
 *
 * Copyright (C) 2009  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#ifndef __OPATOM_CONTENT_PACKAGE_H__
#define __OPATOM_CONTENT_PACKAGE_H__

#include <map>
#include <vector>

#include "DynamicByteArray.h"


class OPAtomClipReader;
class OPAtomTrackReader;


class OPAtomContentElement
{
public:
    friend class OPAtomClipReader;
    friend class OPAtomTrackReader;
    
public:
    OPAtomContentElement();
    
    bool IsPicture() const { return mIsPicture; }
    uint32_t GetMaterialTrackId() const { return mMaterialTrackId; }
    
    const DynamicByteArray* GetEssenceData() const { return &mEssenceData; }
    uint32_t GetSize() const { return mEssenceData.getSize(); }
    const unsigned char* GetBytes() const { return mEssenceData.getBytes(); }

    int64_t GetEssenceOffset() const { return mEssenceOffset; }
    
    uint32_t GetNumSamples() const { return mNumSamples; }
    
private:
    void Reset();
    
private:
    bool mIsPicture;
    uint32_t mMaterialTrackId;
    DynamicByteArray mEssenceData;
    int64_t mEssenceOffset;
    uint32_t mNumSamples;
};


class OPAtomContentPackage
{
public:
    friend class OPAtomClipReader;
    friend class OPAtomTrackReader;
    
public:
    OPAtomContentPackage();
    virtual ~OPAtomContentPackage();

    int64_t GetPosition() const { return mPosition; }
    
    bool HaveEssenceData(uint32_t mp_track_id) const;
    const OPAtomContentElement* GetEssenceData(uint32_t mp_track_id) const;
    const unsigned char* GetEssenceDataBytes(uint32_t mp_track_id) const;
    uint32_t GetEssenceDataSize(uint32_t mp_track_id) const;
    
    size_t NumEssenceData() const { return mEssenceDataVector.size(); }
    const OPAtomContentElement* GetEssenceDataI(size_t index) const;
    const unsigned char* GetEssenceDataIBytes(size_t index) const;
    uint32_t GetEssenceDataISize(size_t index) const;

private:
    OPAtomContentElement* GetEssenceData(uint32_t mp_track_id, bool check) const;
    
    OPAtomContentElement* AddElement(uint32_t mp_track_id);
    
private:
    int64_t mPosition;
    
    std::vector<OPAtomContentElement*> mEssenceDataVector;
    std::map<uint32_t, OPAtomContentElement*> mEssenceData;
};



#endif

