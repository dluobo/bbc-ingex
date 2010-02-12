/*
 * $Id: Partition.h,v 1.2 2010/02/12 13:52:49 philipn Exp $
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

#ifndef __MXFPP_PARTITION_H__
#define __MXFPP_PARTITION_H__


namespace mxfpp
{

class File;


class FillerWriter
{
public:
    virtual ~FillerWriter() {};
    
    virtual void write(File* file) = 0;
};

class Partition;

class KAGFillerWriter : public FillerWriter
{
public:
    KAGFillerWriter(Partition* partition);
    virtual ~KAGFillerWriter();

    virtual void write(File* file);
    
private:
    Partition* _partition;
};

class PositionFillerWriter : public FillerWriter
{
public:
    PositionFillerWriter(int64_t position);
    virtual ~PositionFillerWriter();

    virtual void write(File* file);
    
private:
    int64_t _position;
};


class Partition
{
public:
    static Partition* read(File* file, const mxfKey* key);
    static Partition* findAndReadHeaderPartition(File* file);

    Partition();
    Partition(::MXFPartition* cPartition);
    Partition(const Partition& partition);
    ~Partition();

    void setKey(const mxfKey* key);    
    void setVersion(uint16_t majorVersion, uint16_t minorVersion);
    void setKagSize(uint32_t kagSize);
    void setThisPartition(uint64_t thisPartition);
    void setPreviousPartition(uint64_t previousPartition);
    void setFooterPartition(uint64_t footerPartition);
    void setHeaderByteCount(uint64_t headerByteCount);
    void setIndexByteCount(uint64_t indexByteCount);
    void setIndexSID(uint32_t indexSID);
    void setBodyOffset(uint64_t bodyOffset);
    void setBodySID(uint32_t bodySID);
    void setOperationalPattern(const mxfUL* operationalPattern);
    void addEssenceContainer(const mxfUL* essenceContainer);

    const mxfKey* getKey();
    uint16_t getMajorVersion();
    uint16_t getMinorVersion();
    uint32_t getKagSize();
    uint64_t getThisPartition();
    uint64_t getPreviousPartition();
    uint64_t getFooterPartition();
    uint64_t getHeaderByteCount();
    uint64_t getIndexByteCount();
    uint32_t getIndexSID();
    uint64_t getBodyOffset();
    uint32_t getBodySID();
    const mxfUL* getOperationalPattern();
    std::vector<mxfUL> getEssenceContainers();
    
    void markHeaderStart(File* file);
    void markHeaderEnd(File* file);
    void markIndexStart(File* file);
    void markIndexEnd(File* file);

    void write(File* file);
    
    void fillToKag(File* file);

    
    ::MXFPartition* getCPartition() const { return _cPartition; }

private:
    ::MXFPartition* _cPartition;
};




};


#endif

