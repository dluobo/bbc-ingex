/*
 * $Id: GalleryLoggerView.h,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Implementation of the "view" class of document/view
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

#if !defined(GalleryLoggerView_h)
#define GalleryLoggerView_h

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "HMXEdit.h"
#include "Timecode.h"
#include "Take.h"
#include "EasyReader.h"
#include "RecordingDevice.h"
#include "ContentProcessor.h"
#include "afxwin.h"

class CGallerLoggerDoc;

const int SOURCES_DISPLAYABLE = 4;
const bool USE_DV_RECORDER = false;
const bool USE_SDI_RECORDER = false;  // This is the "old-style" recorder
const bool USE_RECORDER = true;

class CGalleryLoggerView : public CFormView
{
protected: // create from serialization only
    CGalleryLoggerView();
    DECLARE_DYNCREATE(CGalleryLoggerView)

public:
    //{{AFX_DATA(CGalleryLoggerView)
    enum { IDD = IDD_GALLERYLOGGER_FORM };
    CStatic mGoodNgIndicator;
    CStatic mRecordIndicator;
    CEdit   mDirectorCtrl;
    CEdit   mProducerCtrl;
    CEdit   mPACtrl;
    CEdit   mSeriesCtrl;
    CEdit   mProgrammeCtrl;
    CEdit   mTimecodeCtrl;
    CEdit   mCommentCtrl;
    CHMXEdit    mDurationCtrl;
    CHMXEdit    mStopTimeCtrl;
    CHMXEdit    mStartTimeCtrl;
    CListCtrl   mTakeList;
    CListCtrl   mSequenceList;
    CString mStartTime;
    CString mStopTime;
    CString mDuration;
    CString mComment;
    CString mDate;
    CString mTimecode;
    //}}AFX_DATA

// Attributes
public:
    CGalleryLoggerDoc* GetDocument();

// Operations
public:

// Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CGalleryLoggerView)
    public:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    virtual void OnInitialUpdate(); // called first time after construct
    virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
    virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
    virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);
    virtual void OnPrint(CDC* pDC, CPrintInfo* pInfo);
    //}}AFX_VIRTUAL

// Implementation
public:
    void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint );
    virtual ~CGalleryLoggerView();
#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
    //{{AFX_MSG(CGalleryLoggerView)
    afx_msg void OnDblclkSequenceList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnGood();
    afx_msg void OnBad();
    afx_msg void OnNextSequence();
    afx_msg void OnPreviousSequence();
    afx_msg void OnTimer(UINT nIDEvent);
    afx_msg void OnFileExport();
    afx_msg void OnStoreTake();
    afx_msg void OnDblclkTakeList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnChangeProgramme();
    afx_msg void OnTimecodePcInternal();
    afx_msg void OnTimecodeEasyreader();
    afx_msg void OnUpdateTimecodePcInternal(CCmdUI* pCmdUI);
    afx_msg void OnUpdateTimecodeEasyreader(CCmdUI* pCmdUI);
    afx_msg void OnReset();
    afx_msg void OnStartRecordButton();
    afx_msg void OnStopRecordButton();
    afx_msg void OnButtonRepeat();
    afx_msg void OnFileExportSmil();
    afx_msg void OnFileExportXml();
    afx_msg void OnChangeComment();
    afx_msg void OnProcessorSelect();
    afx_msg void OnButtonProcess();
    afx_msg void OnEndlabeleditSequenceList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnFileExportToFcp();
    afx_msg void OnButtonCancel();
    afx_msg void OnKeyCancel();
    afx_msg void OnClickSequenceList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnButtonPickup();
    afx_msg void OnButtonNewSequence();
    afx_msg void OnChangeSequenceName();
    afx_msg void OnButtonDelete();
    afx_msg void OnButtonPreviousTake();
    afx_msg void OnButtonNextTake();
    afx_msg void OnClickTakeList(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnKeyDeleteItem();
    afx_msg void OnKeyNewItem();
    afx_msg void OnKeyPickup();
    afx_msg void OnChangeSource3();
    afx_msg void OnChangeTape3();
    afx_msg void OnFileExportGoodTakeList();
    afx_msg void OnButtonReplay();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
private:
    void EditTake();
// methods
    void Publish();
    void FormatDescriptionText(const std::string & item_descript,
        const Take & take,
        unsigned int width,
        std::vector<std::string> & lines);
    //void FormatSourceText(const Sequence & sequence,
    //  const Take & take,
    //  int width,
    //  std::vector<std::string> & lines);
    void Cancel();
    void OnDropdownComboDev(int device_no);
    void OnCheckSource(int i, bool checked);
    void SetDevice(int i);

    void Start();
    void Stop();

    void EndTake(const char * tc);
    void BeginTake(const char * tc);

    void StopRecord(std::string tc);
    void StartRecord(std::string tc);

    void ShowGoodNg(bool good);
    void ClearGoodNg();
    std::string CurrentTimecode();
    void SelectSequence();
    void NewTake(bool result);
    void PrintHeader(CDC * p_dc, int page_number);
    void PrintPage(CDC * p_dc, int page_number);
    //void UpdateTakeInfo();
    void ClearTakeDisplay();
    void TransferTakeToDocument();
    void TransferTakeToDisplay();
    void AddPickup();
    void AddBlankSequence();
    void AddRepeat();
    void DeleteItem();

    //BOOL & SourceCheck(int i);
    //CString & SourceDisplay(int i);
    //CString & SourceDescript(int i);
    //CString & TapeDisplay(int i);
    //CString & DeviceDisplay(int i);
    //CComboBox & ComboDevice(int i);

    //void OnChangeSource(int i);
    //void OnChangeSourceDescript(int i);
    //void OnChangeTape(int i);

    CString NamingService();
    void NamingService(const TCHAR * ns);
    void ApplyNamingService(const TCHAR * ns);

    //void UpdateRecorder();

// data
    CImageList mStateImages;
    bool mRecording;
    bool mRunning;
    bool mIsGood;
    Take mCurrentTake;

// Timecode reader
    bool mUseExternalTc;
    EasyReader mTcReader;

// Recordings that this GUI can support
    //Recording mTapeRecordings[SOURCES_DISPLAYABLE];
    //Recording mFileRecordings[SOURCES_DISPLAYABLE];

// External recording device manager
    RecordingDevice mRecorder;

// External content processor
    ContentProcessor mContentProcessor;

// variables to hold printing parameters
    CFont mPrinterFont;
    UINT mPrinterTotalx;
    UINT mPrinterTotaly;
    UINT mPrinterCx;
    UINT mPrinterCy;
    UINT mPrinterPpix;
    UINT mPrinterPpiy;
    UINT mPrinterMarginLeft;
    UINT mPrinterMarginRight;
    int mLinesPerPage; // excludes header lines
    int mTotalCharWidth;
    std::string mFormat;

    class Column
    {
    public:
        Column(const char * label, int width, int just)
            : Label(label), Width(width), Justification(just) {}
        std::string Label;
        int Width;
        int Justification;  // 1 or -1 as per printf format specifier
    };
    std::vector<Column> mColumns;
    int mDescriptionColumnIndex;
    int mSourceColumnIndex;

    struct SequenceTake
    {
        int Sequence;
        int Take;
    };
    std::vector<SequenceTake> mPageStarts;
    std::vector<SequenceTake> mPageEnds;

// take manipulation
    bool mTakeHasNewData; // whether current take should stored
    int mTakeIndex; // used when editing an existing take
    CString mCurrentTapeNumber;
public:
    afx_msg void OnToolsSettings();
    afx_msg void OnRecorderSelect();
    afx_msg void OnCbnDropdownComboLocation();
    CComboBox mComboLocationCtrl;
    afx_msg void OnCbnSelchangeComboLocation();
    afx_msg void OnBnClickedButtonStop();
    afx_msg void OnBnClickedButtonStart();
    afx_msg void OnKeyStop();
    afx_msg void OnKeyStart();
    afx_msg void OnCbnDropdownComboSeries();
    afx_msg void OnCbnDropdownComboProg();
    CComboBox mComboSeriesCtrl;
    afx_msg void OnCbnSelchangeComboSeries();
    afx_msg void OnCbnSelchangeComboProg();
    CComboBox mComboProgCtrl;
    afx_msg void OnCbnDropdownComboRecorder();
    afx_msg void OnCbnSelchangeComboRecorder();
    CComboBox mComboRecorderCtrl;
    afx_msg void OnEnKillfocusProducer();
    afx_msg void OnEnKillfocusDirector();
    afx_msg void OnEnKillfocusPa();
};

#ifndef _DEBUG  // debug version in GalleryLoggerView.cpp
inline CGalleryLoggerDoc* CGalleryLoggerView::GetDocument()
   { return (CGalleryLoggerDoc*)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(GalleryLoggerView_h)
