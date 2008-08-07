/*
 * $Id: GalleryLoggerDoc.h,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Implementation of the "document" class of document/view
 * in Gallery Logger application.
 *
 * Copyright (C) 2007  British Broadcasting Corporation.
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

#if !defined(GalleryLoggerDoc_h)
#define GalleryLoggerDoc_h

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#pragma warning(disable:4786) // // To suppress some <vector>-related warnings

#include <vector>
//#include "Sequence.h"
#include "Take.h"
#include "Timecode.h"
#include <database/src/Database.h>
#include <xercesc/dom/DOMDocument.hpp>


class CGalleryLoggerDoc : public CDocument
{
protected: // create from serialization only
    CGalleryLoggerDoc();
    DECLARE_DYNCREATE(CGalleryLoggerDoc)

// Attributes
public:

// Operations
public:

// Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CGalleryLoggerDoc)
    public:
    virtual BOOL OnNewDocument();
    virtual void Serialize(CArchive& ar);
    virtual void DeleteContents();
    //}}AFX_VIRTUAL

// Implementation
    enum { NOT_SEQUENCE_LIST = 1 };
    void Import(const char * filename);
    void ExportXml(const char * filename);
    void ExportGoodTakeList(const char * filename);
    void ExportAle(const char * filename, int src = 0);
    //void ExportSmil(const char * filename);
    void ExportToFcp(const char * filename);
    void ExportToFcpOld(const char * filename);
    void SaveLogTo(const char * filename);
    void AutoSave();

    bool IsEmpty() { return mIsEmpty; }

    void DeleteItem();

    //int SequenceCount() { return mRecordingOrder.size(); }
    //::Sequence & Sequence(int i) { return * mRecordingOrder[i]; }
    //::Sequence & CurrentSequence() { return * mRecordingOrder[mCurrentSequenceIndex]; }


    unsigned int TakeCount();
    prodauto::Take * Take(unsigned int i);

    void RecorderName(const char * s);

    //const char * RecordedDuration() { return mRecordedDuration.TextS(); }
    //void CalcRecordedDuration();

    void AddTake(const ::Take & take);
    void ModifyTake(int index, const ::Take & take);

    const std::map<long, std::string> & RecordingLocations();
    void RecordingLocation(const char * loc);
    long RecordingLocationId() { return mRecordingLocation; }

    unsigned int SeriesCount() { return mSeriesVector.size(); }
    bool HasSeries() { return mSeriesIndex != -1; }
    void SelectSeries(const char * s, bool update_view = true);
    const char * SeriesName(unsigned int i);
    const char * CurrentSeriesName();

    unsigned int ProgrammeCount();
    bool HasProgramme() { return mProgrammeIndex != -1; }
    void SelectProgramme(const char * s, bool update_view = true);
    int ProgrammeIndex() { return mProgrammeIndex; }
    const char * ProgrammeName(unsigned int i);
    const char * CurrentProgrammeName();

    void Director(const char * s) { mDirector = s;}
    const char * Director() { return mDirector.c_str(); }
    bool HasDirector() { return !mDirector.empty(); }

    void PA(const char * s) { mPA = s;}
    const char * PA() { return mPA.c_str(); }
    bool HasPA() { return !mPA.empty(); }

    void Producer(const char * s) { mProducer = s;}
    const char * Producer() { return mProducer.c_str(); }
    bool HasProducer() { return !mProducer.empty(); }

    unsigned int ItemCount();
    void SelectItem(int i, bool update_view = true);
    int ItemIndex() { return mItemIndex; }
    const char * ItemDescription(unsigned int i);
    std::string ItemScriptRefs(unsigned int i);

    enum ItemTypeEnum { BLANK, PICKUP, REPEAT };
    void AddNewItem(ItemTypeEnum type, bool update_view = true);
    void InsertItem(const std::string & descript);
    bool ItemHasGoodTake(unsigned int i);
    prodauto::Item * Item(unsigned int i);
    void SaveItem(prodauto::Item * item, bool update_view = true);

    //std::vector<Source> & Sources() { return mSources; }

    const char * GenerateSeqId(const char * ref);
public:
    virtual ~CGalleryLoggerDoc();
#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
    //{{AFX_MSG(CGalleryLoggerDoc)
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
private:
// methods
    void ArWrite(CArchive & ar, const char * value, bool prepend_delimiter = true);
    const char * ClipFilename(const char * src, const char * seq_id, int take, const char * suffix);
    void MakeProgrammeOrder();
    //void InsertSequence(const char * name, ScriptRef start);

    void AddFcpClip(XERCES_CPP_NAMESPACE::DOMDocument * doc,
                        int clipIndex,
                        std::string name, std::string id,
                        int trackNo, int start, int in, int out,
                        std::string pathurl,
                        int durationInFrames);
    void AddFcpStill(XERCES_CPP_NAMESPACE::DOMDocument * doc,
                        std::string name,
                        int track,
                        int start,
                        int end,
                        std::string jpeg_file);
// data
// document
    bool mIsEmpty;
// programme
    //std::vector<SequencePtr> mRecordingOrder;
    //std::vector<SequencePtr> mProgrammeOrder;
    std::string mSeries;
    std::string mProgramme;
    std::string mDirector;
    std::string mPA;
    std::string mProducer;
// calculated from programme
    Timecode mRecordedDuration;
// view
    //int mCurrentSequenceIndex;
    //std::vector<Source> mSources;
// tmp for return values
    std::string mClipFilename;
    std::string mSeqId;
// variables
    int mBackupIndex;

// database
    prodauto::Database * mpDb;

    std::vector<prodauto::Series *> mSeriesVector; // auto_ptr?
    int mSeriesIndex; // Use -1 to indicate no series selected.
    int mProgrammeIndex;
    int mItemIndex;


    std::map<long, std::string> mRecordingLocations;
    long mRecordingLocation;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(GalleryLoggerDoc_h)
