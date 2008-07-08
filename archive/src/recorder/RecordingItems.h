/*
 * $Id: RecordingItems.h,v 1.1 2008/07/08 16:25:50 philipn Exp $
 *
 * Provides access to the recording item information  
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
 
#ifndef __RECORDER_RECORDING_ITEMS_H__
#define __RECORDER_RECORDING_ITEMS_H__



#include "Types.h"
#include "DatabaseThings.h"
#include "Threads.h"

#include <vector>


namespace rec
{


class RecordingItem
{
public:
    RecordingItem();
    RecordingItem(int id_,int index_, D3Source* d3Source_);
    RecordingItem(int id_, int index_); // junk item
    ~RecordingItem();
    
    void copyClipTo(RecordingItem* toItem);
    void mergeClipTo(RecordingItem* toItem);
    void clearClip();
    
    void swapSourceWith(RecordingItem* withItem);
    
    void disable();
    void enable();
    
    void setChunked(std::string chunkedFilename);
    void setJunked();



    bool isDisabled;
    bool isJunk;
    
    // index of item in array
    int index;
    
    // source identifier (value between 0 and (num items -1)) in array and info
    // only relevant if isJunk is false
    int id;
    D3Source* d3Source;
    
    // clip
    int64_t startPosition;
    int64_t duration;
    std::string filename;
};


class RecordingItems
{
public:
    RecordingItems(Source* source);
    ~RecordingItems();
    
    void initialise(std::string filename, int64_t duration);
    void setChunked(int itemId, std::string filename);
    void setJunked(int itemId);
    
    void lockItems();
    void unlockItems();
    
    bool haveMultipleItems();
    int getItemCount();
    int64_t getTotalInfaxDuration();
    std::string getD3SpoolNo();
    int64_t getTotalDuration();
    
    RecordingItem getFirstItem();
    bool getItem(std::string filename, int64_t position, RecordingItem* itemAt);
    bool getItem(int id, int index, RecordingItem* itemAt);
    bool getNextStartOfItem(std::string filename, int64_t position, RecordingItem* nextItem);
    bool getPrevStartOfItem(std::string filename, int64_t position, RecordingItem* prevItem);
    
    int markItemStart(std::string filename, int64_t position, int* id, bool* isJunk);
    int clearItem(int id, int index, std::string* clearedFilename, int64_t* clearedPosition);
    int moveItemUp(int id, int index);
    int moveItemDown(int id, int index);
    int disableItem(int id, int index);
    int enableItem(int id, int index);
    
    Source* getSource();
    
    std::vector<RecordingItem> getItems();
    void getItems(std::vector<RecordingItem>* items, int* itemClipChangeCount, int* itemSourceChangeCount);
    void getItemsChangeCount(int* itemClipChangeCount, int* itemSourceChangeCount);
    
    
private:
    Source* _source;

    Mutex _itemsMutex;
    std::vector<RecordingItem> _items;
    bool _locked;
    
    bool _multipleItems;
    int _itemsCount;
    int64_t _totalInfaxDuration;
    std::string _d3SpoolNo;
    int64_t _totalDuration;

    int _itemClipChangeCount;
    int _itemSourceChangeCount;
};





};


#endif


