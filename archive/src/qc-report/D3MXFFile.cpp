/*
 * $Id: D3MXFFile.cpp,v 1.1 2008/07/08 16:25:11 philipn Exp $
 *
 * Provides access to the header metadata and timecode contained in a D3 MXF file
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

#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include <mxf_reader.h>

#include "D3MXFFile.h"
#include "RecorderException.h"
#include "Logging.h"


using namespace std;
using namespace rec;


#define QC_CHK_ORET(cmd) \
    if (!(cmd)) \
    { \
        Logging::error("'%s' failed in line %d\n", #cmd, __LINE__); \
        return false; \
    }



static const mxfKey g_PartitionPackKeyPrefix = 
    {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x05, 0x01, 0x01, 0x0d, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00};

static const mxfKey g_SystemItemElementKey = 
    {0x06, 0x0e, 0x2b, 0x34, 0x02, 0x53, 0x01, 0x01 , 0x0d, 0x01, 0x03, 0x01, 0x14, 0x02, 0x01, 0x00};
    


static bool mxf_read_k(FILE* mxfFile, mxfKey* key)
{
    return fread((uint8_t*)key, 1, 16, mxfFile) == 16;
}

static bool mxf_read_l(FILE* mxfFile, uint8_t* llen, uint64_t* len)
{
    int i;
    int c;
    uint64_t length;
    uint8_t llength;
    
    QC_CHK_ORET((c = fgetc(mxfFile)) != EOF);

    length = 0;
    llength = 1;
    if (c < 0x80) 
    {
        length = c;
    }
    else 
    {
        int bytesToRead = c & 0x7f;
        REC_CHECK(bytesToRead <= 8); 
        for (i = 0; i < bytesToRead; i++) 
        {
            QC_CHK_ORET((c = fgetc(mxfFile)) != EOF);
            length = length << 8;
            length = length | c;
        }
        llength += bytesToRead;
    }
    
    *llen = llength;
    *len = length;
    return true;
}

static bool mxf_read_kl(FILE* mxfFile, mxfKey* key, uint8_t* llen, uint64_t *len)
{
    QC_CHK_ORET(mxf_read_k(mxfFile, key));
    QC_CHK_ORET(mxf_read_l(mxfFile, llen, len));
    
    return true;
}

static bool mxf_skip(FILE* mxfFile, uint64_t len)
{
    return fseeko(mxfFile, len, SEEK_CUR) == 0;
}

static void convert_12m_to_timecode(unsigned char* t12m, Timecode* t)
{
    t->frame = ((t12m[0] >> 4) & 0x03) * 10 + (t12m[0] & 0xf);
    t->sec = ((t12m[1] >> 4) & 0x07) * 10 + (t12m[1] & 0xf);
    t->min = ((t12m[2] >> 4) & 0x07) * 10 + (t12m[2] & 0xf);
    t->hour = ((t12m[3] >> 4) & 0x03) * 10 + (t12m[3] & 0xf);
}

static bool mxf_read_timecode(FILE* mxfFile, Timecode* vitc, Timecode* ltc)
{
    unsigned char t12m[8];
    mxfKey key;
    uint8_t llen;
    uint64_t len;
    
    /* check the key and len */
    QC_CHK_ORET(mxf_read_kl(mxfFile, &key, &llen, &len));
    QC_CHK_ORET(mxf_equals_key(&key, &g_SystemItemElementKey));
    
    /* skip the local item tag,local item length, and array header */
    if (!mxf_skip(mxfFile, 12))
    {
        return false;
    }
    
    /* read the timecode */
    QC_CHK_ORET(fread(t12m, 1, 8, mxfFile) == 8);
    convert_12m_to_timecode(t12m, vitc);
    QC_CHK_ORET(fread(t12m, 1, 8, mxfFile) == 8);
    convert_12m_to_timecode(t12m, ltc);
    
    return true;
}

static bool mxf_position_at_start(FILE* mxfFile, int64_t* contentPackageSize)
{
    mxfKey key;
    uint8_t llen;
    uint64_t len;

    
    /* read and check header partition key */
    QC_CHK_ORET(mxf_read_kl(mxfFile, &key, &llen, &len));
    QC_CHK_ORET(mxf_equals_key_prefix(&key, &g_PartitionPackKeyPrefix, 13) && key.octet13 == 0x02);
    QC_CHK_ORET(mxf_skip(mxfFile, len));
    
    
    /* move to start of essence */
    while (true)
    {
        QC_CHK_ORET(mxf_read_kl(mxfFile, &key, &llen, &len));
        if (mxf_equals_key(&key, &g_SystemItemElementKey))
        {
            break;
        }
        else
        {
            QC_CHK_ORET(mxf_skip(mxfFile, len));
        }
    }
    
    /* seek to the start of the next system item or partition pack klv 
    which will then tell us the content package size */
    *contentPackageSize = mxfKey_extlen + llen;
    while (true)
    {
        QC_CHK_ORET(mxf_skip(mxfFile, len));
        *contentPackageSize += len;

        QC_CHK_ORET(mxf_read_kl(mxfFile, &key, &llen, &len));
        if (mxf_equals_key(&key, &g_SystemItemElementKey) || mxf_is_partition_pack(&key))
        {
            break;
        }
        *contentPackageSize += mxfKey_extlen + llen;
    }
    

    /* seek back to before the 1st system item key */
    QC_CHK_ORET(fseeko(mxfFile, -(*contentPackageSize + mxfKey_extlen + llen), SEEK_CUR) == 0);
    
    return true;
}

static string strip_path(string filepath)
{
    size_t sepIndex;
    if ((sepIndex = filepath.rfind("/")) != string::npos)
    {
        return filepath.substr(sepIndex + 1);
    }
    return filepath;
}





D3MXFFile::D3MXFFile(string filepath)
: _mxfFile(0), _startFilePosition(0), _contentPackageSize(0), _mxfDuration(0)
{
    // extract properties
    
    MXFFile* mxfFile = 0;
    MXFDataModel* dataModel = 0;
    MXFReader* mxfReader = 0;
    D3MXFInfo d3MXFInfo;
    PSEFailure* pseFailures;
    long numFailures;
    long i;
    try
    {
        // open the mxf file
        if (!mxf_disk_file_open_read(filepath.c_str(), &mxfFile))
        {
            throw RecorderException("Failed to open MXF disk file '%s'\n", filepath.c_str());
        }
        
        // load d3 extensions and init reader
        REC_CHECK(mxf_load_data_model(&dataModel));
        REC_CHECK(d3_mxf_load_extensions(dataModel));
        REC_CHECK(mxf_finalise_data_model(dataModel));
        REC_CHECK(init_mxf_reader_2(&mxfFile, dataModel, &mxfReader));

        _isComplete = have_footer_metadata(mxfReader);
        
        // get metadata
        memset(&d3MXFInfo, 0, sizeof(D3MXFInfo));
        REC_CHECK(d3_mxf_get_info(get_header_metadata(mxfReader), &d3MXFInfo));
        _mxfDuration = get_duration(mxfReader);
        _d3InfaxData = d3MXFInfo.d3InfaxData;
        _ltoInfaxData = d3MXFInfo.ltoInfaxData;
        
        // get PSE failures
        REC_CHECK(d3_mxf_get_pse_failures(get_header_metadata(mxfReader), &pseFailures, &numFailures));
        if (numFailures > 0)
        {
            _pseFailures.reserve(numFailures);
            for (i = 0; i < numFailures; i++)
            {
                _pseFailures.push_back(pseFailures[i]);
            }
        }
        
        close_mxf_reader(&mxfReader);
        mxf_free_data_model(&dataModel);
    }
    catch (...)
    {
        if (mxfReader != 0)
        {
            close_mxf_reader(&mxfReader);
        }
        else if (mxfFile != 0)
        {
            mxf_file_close(&mxfFile);
        }
        
        if (dataModel != 0)
        {
            mxf_free_data_model(&dataModel);
        }
        
        throw;
    }
    
    if (_mxfDuration != 0)
    {
        // open the mxf file ready for completing timecode info
        _mxfFile = fopen(filepath.c_str(), "rb");
        if (_mxfFile == 0)
        {
            throw RecorderException("Failed to open MXF file '%s': %s\n", filepath.c_str(), strerror(errno));
        }
        REC_CHECK(mxf_position_at_start(_mxfFile, &_contentPackageSize));
        _startFilePosition = ftello(_mxfFile);
        
        _filename = strip_path(filepath);
        
        
        // complete the timecode on the pse failures
        vector<PSEFailure>::iterator iter;
        for (iter = _pseFailures.begin(); iter != _pseFailures.end(); iter++)
        {
            PSEFailure& pseFailure = *iter;
            
            completePSETimecodeInfo(&pseFailure);
        }
    }
}

D3MXFFile::~D3MXFFile()
{
    if (_mxfFile != 0)
    {
        fclose(_mxfFile);
    }
}

void D3MXFFile::completeTimecodeInfo(TimecodeGroup& timecodeGroup)
{
    if (_mxfFile == 0)
    {
        // file duration is 0
        return;
    }
    
    int64_t position = timecodeGroup.ctc.hour * 60 * 60 * 25 + 
        timecodeGroup.ctc.min * 60 * 25 + 
        timecodeGroup.ctc.sec * 25 + 
        timecodeGroup.ctc.frame;
            
   REC_CHECK(fseeko(_mxfFile, _startFilePosition + position * _contentPackageSize, SEEK_SET) >= 0);
   REC_CHECK(mxf_read_timecode(_mxfFile, &timecodeGroup.vitc, &timecodeGroup.ltc));
}

void D3MXFFile::completePSETimecodeInfo(PSEFailure* pseFailure)
{
    if (_mxfFile == 0)
    {
        // file duration is 0
        return;
    }
    
   Timecode vitc, ltc;
   
   REC_CHECK(fseeko(_mxfFile, _startFilePosition + pseFailure->position * _contentPackageSize, SEEK_SET) >= 0);
   REC_CHECK(mxf_read_timecode(_mxfFile, &vitc, &ltc));
   
   pseFailure->vitcTimecode.dropFrame = 0;
   pseFailure->vitcTimecode.hour = vitc.hour;
   pseFailure->vitcTimecode.min = vitc.min;
   pseFailure->vitcTimecode.sec = vitc.sec;
   pseFailure->vitcTimecode.frame = vitc.frame;

   pseFailure->ltcTimecode.dropFrame = 0;
   pseFailure->ltcTimecode.hour = ltc.hour;
   pseFailure->ltcTimecode.min = ltc.min;
   pseFailure->ltcTimecode.sec = ltc.sec;
   pseFailure->ltcTimecode.frame = ltc.frame;
}



