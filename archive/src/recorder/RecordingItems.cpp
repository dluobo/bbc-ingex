/*
 * $Id: RecordingItems.cpp,v 1.1 2008/07/08 16:25:38 philipn Exp $
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

/*
    A recording item consists of clip information (filename, start and duration)
    and D3 source information. This information is used for display to the user,
    playing back and chunking.
    
    The order of the items, start, duration and whether or not they are present 
    can be changed. Clip information and source information update counts signal 
    to (possibly multiple) clients that the items have been updated.
*/


#include "RecordingItems.h"
#include "Logging.h"
#include "RecorderException.h"
#include "Utilities.h"
#include "Config.h"

using namespace std;
using namespace rec;



RecordingItem::RecordingItem()
: isDisabled(false), isJunk(false), index(-1), id(-1), d3Source(0), startPosition(-1), duration(-1)
{
}

RecordingItem::RecordingItem(int id_, int index_, D3Source* d3Source_)
: isDisabled(false), isJunk(false), index(index_), id(id_), d3Source(d3Source_), startPosition(-1), duration(-1)
{
}

RecordingItem::RecordingItem(int id_, int index_)
: isDisabled(false), isJunk(true), index(index_), id(id_), d3Source(0), startPosition(-1), duration(-1)
{
}

RecordingItem::~RecordingItem()
{
}

void RecordingItem::copyClipTo(RecordingItem* toItem)
{
    toItem->filename = filename;
    toItem->duration = duration;
    toItem->startPosition = startPosition;
}

void RecordingItem::mergeClipTo(RecordingItem* toItem)
{
    if (duration >= 0)
    {
        toItem->duration += duration;
    }
}

void RecordingItem::clearClip()
{
    filename.clear();
    duration = -1;
    startPosition = -1;
}

void RecordingItem::swapSourceWith(RecordingItem* withItem)
{
    int isDisabledCopy = isDisabled;
    isDisabled = withItem->isDisabled;
    withItem->isDisabled = isDisabledCopy;
    
    int isJunkCopy = isJunk;
    isJunk = withItem->isJunk;
    withItem->isJunk = isJunkCopy;
    
    int idCopy = id;
    id = withItem->id;
    withItem->id = idCopy;
    
    D3Source* d3SourceCopy = d3Source;
    d3Source = withItem->d3Source;
    withItem->d3Source = d3SourceCopy;
}

void RecordingItem::disable()
{
    isDisabled = true;
    clearClip();
}

void RecordingItem::enable()
{
    isDisabled = false;
}

void RecordingItem::setChunked(string chunkedFilename)
{
    filename = chunkedFilename;
    startPosition = 0;
}

void RecordingItem::setJunked()
{
    filename.clear(); // empty filename indicates junked content
    startPosition = 0;
}



RecordingItems::RecordingItems(Source* source)
: _source(source), _locked(false), _multipleItems(false), _itemsCount(0), _totalInfaxDuration(-1),
_totalDuration(0), _itemClipChangeCount(0), _itemSourceChangeCount(0) 
{
    REC_ASSERT(source->concreteSources.size() > 0);
    
    // populate vector of items
    int count = 0;
    vector<ConcreteSource*>::const_iterator iter;
    for (iter = source->concreteSources.begin(); iter != source->concreteSources.end(); iter++)
    {
        D3Source* d3Source = dynamic_cast<D3Source*>(*iter);
        REC_ASSERT(d3Source != 0);
        
        _items.push_back(RecordingItem(count, count, d3Source));
        
        if (d3Source->duration > 0)
        {
            if (_totalInfaxDuration < 0)
            {
                _totalInfaxDuration = d3Source->duration;
            }
            else
            {
                _totalInfaxDuration += d3Source->duration;
            }
        }
        
        count++;
    }
    
    // add junk item
    if (_items.size() > 1 && Config::getBool("enable_chunking_junk"))
    {
        _items.push_back(RecordingItem(count, count));
        count++;
    }
    
    _multipleItems = _items.size() > 1;
    _itemsCount = _items.size();
    _d3SpoolNo = _items[0].d3Source->spoolNo;
}

RecordingItems::~RecordingItems()
{
    delete _source;
}

void RecordingItems::initialise(string filename, int64_t duration)
{
    LOCK_SECTION(_itemsMutex);
    
    RecordingItem& firstItem = _items.front();
    firstItem.filename = filename;
    firstItem.startPosition = 0;
    firstItem.duration = duration;

    _totalDuration = duration;
}

void RecordingItems::setChunked(int itemId, string filename)
{
    LOCK_SECTION(_itemsMutex);

    vector<RecordingItem>::iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        RecordingItem& item = *iter;
        
        if (item.id == itemId)
        {
            item.setChunked(filename);
            _itemClipChangeCount++;
            return;
        }
    }        
}

void RecordingItems::setJunked(int itemId)
{
    LOCK_SECTION(_itemsMutex);

    vector<RecordingItem>::iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        RecordingItem& item = *iter;
        
        if (item.id == itemId)
        {
            item.setJunked();
            _itemClipChangeCount++;
            return;
        }
    }        
}

void RecordingItems::lockItems()
{
    LOCK_SECTION(_itemsMutex);
    
    _locked = true;
}

void RecordingItems::unlockItems()
{
    LOCK_SECTION(_itemsMutex);
    
    _locked = false;
}

bool RecordingItems::haveMultipleItems()
{
    return _multipleItems;
}

int RecordingItems::getItemCount()
{
    return _itemsCount;
}

int64_t RecordingItems::getTotalInfaxDuration()
{
    return _totalInfaxDuration;
}

string RecordingItems::getD3SpoolNo()
{
    return _d3SpoolNo;
}

int64_t RecordingItems::getTotalDuration()
{
    return _totalDuration;
}

RecordingItem RecordingItems::getFirstItem()
{
    LOCK_SECTION(_itemsMutex);
    
    return _items[0];
}

bool RecordingItems::getItem(string filename, int64_t position, RecordingItem* itemAt)
{
    LOCK_SECTION(_itemsMutex);
    
    vector<RecordingItem>::const_iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        const RecordingItem& item = *iter;
        
        if (item.duration < 0)
        {
            return false;
        }
        
        if (item.filename.compare(filename) == 0 &&
            position >= item.startPosition &&
            position < item.startPosition + item.duration)
        {
            *itemAt = item;
            return true;
        }
    }
    
    return false;
}

bool RecordingItems::getItem(int id, int index, RecordingItem* itemAt)
{
    LOCK_SECTION(_itemsMutex);
    
    if (index < 0 || index >= (int)_items.size() || _items[index].id != id)
    {
        return false;
    }
    
    *itemAt = _items[index];
    return true;
}

bool RecordingItems::getNextStartOfItem(string filename, int64_t position, RecordingItem* nextItem)
{
    LOCK_SECTION(_itemsMutex);
    
    vector<RecordingItem>::const_iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        const RecordingItem& item = *iter;
        
        if (item.duration < 0)
        {
            return false;
        }
        
        if (item.filename.compare(filename) == 0 &&
            position >= item.startPosition &&
            position < item.startPosition + item.duration)
        {
            // next item
            iter++;
            if (iter == _items.end() || (*iter).duration < 0)
            {
                return false;
            }
            
            *nextItem = *iter;
            return true;
        }
    }
    
    return false;
}

bool RecordingItems::getPrevStartOfItem(string filename, int64_t position, RecordingItem* prevItem)
{
    LOCK_SECTION(_itemsMutex);
    
    vector<RecordingItem>::const_iterator iter;
    for (iter = _items.begin(); iter != _items.end(); iter++)
    {
        const RecordingItem& item = *iter;
        
        if (item.duration < 0)
        {
            return false;
        }
        
        if (item.filename.compare(filename) == 0 &&
            position >= item.startPosition &&
            position < item.startPosition + item.duration)
        {
            if (position > item.startPosition)
            {
                // previous start of item is this item
                *prevItem = item;
                return true;
            }
            
            if (iter == _items.begin())
            {
                return false;
            }
            
            // prev item
            --iter;
            
            *prevItem = *(iter);
            return true;
        }
    }
    
    return false;
}

int RecordingItems::markItemStart(string filename, int64_t position, int* id, bool* isJunk)
{
    LOCK_SECTION(_itemsMutex);
    
    if (_locked)
    {
        return -1;
    }
    
    // check that there is a item with it's clip not set
    int i;
    for (i = 0; i < (int)_items.size(); i++)
    {
        if (!_items[i].isDisabled && _items[i].duration < 0)
        {
            break;
        }
    }
    if (i == (int)_items.size())
    {
        return -1;
    }
    
    // get the item that will be split in 2
    int index;
    for (index = 0; index < (int)_items.size(); index++)
    {
        const RecordingItem& item = _items[index];

        if (item.duration < 0)
        {
            return -1;
        }
        
        if (item.filename.compare(filename) == 0 &&
            position >= item.startPosition &&
            position < item.startPosition + item.duration)
        {
            // found match
            break;
        }
    }        
    if (index == (int)_items.size() || 
        index == (int)_items.size() - 1 ||
        position == _items[index].startPosition)
    {
        return -1;
    }

    // shift item clips down making make way for the new item clip
    for (i = (int)_items.size() - 2; i > index; i--)
    {
        _items[i].copyClipTo(&_items[i + 1]);
    }

    // split the item clip with the new next item clip
    RecordingItem& item = _items[index];
    RecordingItem& newNextItem = _items[index + 1];
    newNextItem.filename = item.filename;
    newNextItem.duration = item.duration - (position - item.startPosition);
    newNextItem.startPosition = position;
    item.duration = position - item.startPosition;
    
    _itemClipChangeCount++;

    *id = newNextItem.id;
    *isJunk = newNextItem.isJunk;
    return _itemClipChangeCount;
}

int RecordingItems::clearItem(int id, int index, string* clearedFilename, int64_t* clearedPosition)
{
    LOCK_SECTION(_itemsMutex);
    
    if (_locked)
    {
        return -1;
    }
    
    if (index <= 0 || index >= (int)_items.size() || _items[index].id != id ||
        _items[index].duration < 0)
    {
        return -1;
    }
    
    string filename = _items[index].filename;
    int64_t position = _items[index].startPosition;
    
    // merge the item with the previous item
    _items[index].mergeClipTo(&_items[index - 1]);
    _items[index].clearClip();
    
    // shift items up
    int i;
    for (i = index; i < (int)_items.size() - 1; i++)
    {
        _items[i + 1].copyClipTo(&_items[i]);
        _items[i + 1].clearClip();
    }

    _itemClipChangeCount++;

    
    *clearedFilename = filename;
    *clearedPosition = position;
    return _itemClipChangeCount;
}

int RecordingItems::moveItemUp(int id, int index)
{
    LOCK_SECTION(_itemsMutex);
    
    if (_locked)
    {
        return -1;
    }
    
    if (index <= 0 || index >= (int)_items.size() ||
        _items[index].id != id || _items[index].isDisabled ||
        _items[index].isJunk)
    {
        return -1;
    }
    
    // move the item up
    _items[index].swapSourceWith(&_items[index - 1]);

    _itemSourceChangeCount++;

    return _itemSourceChangeCount;
}

int RecordingItems::moveItemDown(int id, int index)
{
    LOCK_SECTION(_itemsMutex);
    
    if (_locked)
    {
        return -1;
    }
    
    if (index < 0 || index >= (int)(_items.size() - 1) || 
        _items[index].id != id || 
        _items[index].isDisabled || _items[index + 1].isDisabled || 
        _items[index].isJunk || _items[index + 1].isJunk)
    {
        return -1;
    }
    
    // move the item down
    _items[index].swapSourceWith(&_items[index + 1]);
    
    _itemSourceChangeCount++;

    return _itemSourceChangeCount;
}

int RecordingItems::disableItem(int id, int index)
{
    LOCK_SECTION(_itemsMutex);
    
    if (_locked)
    {
        return -1;
    }
    
    if (index < 0 || index >= (int)_items.size() || _items.size() <= 1 ||
        _items[index].id != id || _items[index].isDisabled || 
        _items[index].duration >= 0)
    {
        return -1;
    }
    

    // disable source and move down to before the last disabled item or the end
    
    _items[index].disable();
    
    int i;
    for (i = index; i < (int)(_items.size() - 1); i++)
    {
        if (_items[i + 1].isDisabled)
        {
            break;
        }
        
        _items[i].swapSourceWith(&_items[i + 1]);
    }
    
    
    _itemSourceChangeCount++;

    return _itemSourceChangeCount;
}

int RecordingItems::enableItem(int id, int index)
{
    LOCK_SECTION(_itemsMutex);
    
    if (_locked)
    {
        return -1;
    }
    
    if (index < 0 || index >= (int)_items.size() || _items.size() <= 1 ||
        _items[index].id != id || !_items[index].isDisabled)
    {
        return -1;
    }
    
    
    // enable source and move up to after the last enabled item and junk item
    
    _items[index].enable();
    
    int i;
    for (i = index; i >= 1; i--)
    {
        if (!_items[i - 1].isDisabled && !_items[i - 1].isJunk)
        {
            break;
        }
        
        _items[i].swapSourceWith(&_items[i - 1]);
    }
    
    _itemSourceChangeCount++;

    return _itemSourceChangeCount;
}

Source* RecordingItems::getSource()
{
    return _source;
}

vector<RecordingItem> RecordingItems::getItems()
{
    LOCK_SECTION(_itemsMutex);
    
    return _items;
}

void RecordingItems::getItems(vector<RecordingItem>* items, int* itemClipChangeCount, int* itemSourceChangeCount)
{
    LOCK_SECTION(_itemsMutex);

    *items = _items;
    *itemClipChangeCount = _itemClipChangeCount;
    *itemSourceChangeCount = _itemSourceChangeCount;
}

void RecordingItems::getItemsChangeCount(int* itemClipChangeCount, int* itemSourceChangeCount)
{
    LOCK_SECTION(_itemsMutex);
    
    *itemClipChangeCount = _itemClipChangeCount;
    *itemSourceChangeCount = _itemSourceChangeCount;
}


