/*
 * $Id: GalleryLoggerView.cpp,v 1.2 2008/08/07 16:41:48 john_f Exp $
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

#include "stdafx.h"
#include "GalleryLogger.h"

#include "GalleryLoggerDoc.h"
#include "GalleryLoggerView.h"
#include "RecorderSelectDlg.h"
#include "ItemDlg.h"
#include "ProcessorSelectDlg.h"
#include <Timecode.h>
#include "ClockTime.h"
#include <DateTime.h>
#include <CorbaUtil.h>
#include "SettingsDialog.h"

#include <ace/OS_NS_stdio.h>
#include <ace/OS_NS_string.h>
#include ".\galleryloggerview.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const TCHAR * REG_SECTION = _T("Settings");  // sub-section within app's registry key

const int header_lines = 8;
const UINT TC_TIMER_ID = 1;
const UINT SAVE_TIMER_ID = 2;

/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerView

IMPLEMENT_DYNCREATE(CGalleryLoggerView, CFormView)

BEGIN_MESSAGE_MAP(CGalleryLoggerView, CFormView)
    //{{AFX_MSG_MAP(CGalleryLoggerView)
    ON_NOTIFY(NM_DBLCLK, IDC_SEQUENCE_LIST, OnDblclkSequenceList)
    ON_BN_CLICKED(IDC_GOOD, OnGood)
    ON_BN_CLICKED(IDC_BAD, OnBad)
    ON_BN_CLICKED(IDC_NEXT_SEQUENCE, OnNextSequence)
    ON_BN_CLICKED(IDC_PREVIOUS_SEQUENCE, OnPreviousSequence)
    ON_WM_TIMER()
    ON_COMMAND(ID_FILE_EXPORT, OnFileExport)
    ON_BN_CLICKED(IDC_STORE_TAKE, OnStoreTake)
    ON_NOTIFY(NM_DBLCLK, IDC_TAKE_LIST, OnDblclkTakeList)
    ON_COMMAND(ID_TIMECODE_PCINTERNALCLOCK, OnTimecodePcInternal)
    ON_COMMAND(ID_TIMECODE_EASYREADER, OnTimecodeEasyreader)
    ON_UPDATE_COMMAND_UI(ID_TIMECODE_PCINTERNALCLOCK, OnUpdateTimecodePcInternal)
    ON_UPDATE_COMMAND_UI(ID_TIMECODE_EASYREADER, OnUpdateTimecodeEasyreader)
    ON_BN_CLICKED(IDC_RESET, OnReset)
    ON_BN_CLICKED(IDC_START_RECORD_BUTTON, OnStartRecordButton)
    ON_BN_CLICKED(IDC_STOP_RECORD_BUTTON, OnStopRecordButton)
    ON_COMMAND(ID_FILE_EXPORTSMIL, OnFileExportSmil)
    ON_COMMAND(ID_FILE_EXPORTXML, OnFileExportXml)
    ON_EN_CHANGE(IDC_COMMENT, OnChangeComment)
    ON_BN_CLICKED(IDC_BUTTON_PROCESS, OnButtonProcess)
    ON_NOTIFY(LVN_ENDLABELEDIT, IDC_SEQUENCE_LIST, OnEndlabeleditSequenceList)
    ON_COMMAND(ID_FILE_EXPORT_TO_FCP, OnFileExportToFcp)
    ON_BN_CLICKED(IDC_BUTTON_CANCEL, OnButtonCancel)
    ON_COMMAND(ID_KEY_CANCEL, OnKeyCancel)
    ON_NOTIFY(NM_CLICK, IDC_SEQUENCE_LIST, OnClickSequenceList)
    ON_BN_CLICKED(IDC_BUTTON_REPEAT, OnButtonRepeat)
    ON_BN_CLICKED(IDC_BUTTON_NEW_SEQUENCE, OnButtonNewSequence)
    ON_BN_CLICKED(IDC_BUTTON_DELETE, OnButtonDelete)
    ON_BN_CLICKED(IDC_BUTTON_PREVIOUS_TAKE, OnButtonPreviousTake)
    ON_BN_CLICKED(IDC_BUTTON_NEXT_TAKE, OnButtonNextTake)
    ON_NOTIFY(NM_CLICK, IDC_TAKE_LIST, OnClickTakeList)
    ON_COMMAND(ID_KEY_DELETE_ITEM, OnKeyDeleteItem)
    ON_COMMAND(ID_KEY_NEW_ITEM, OnKeyNewItem)
    ON_COMMAND(ID_KEY_PICKUP, OnKeyPickup)
    ON_COMMAND(ID_KEY_GOOD, OnGood)
    ON_COMMAND(ID_KEY_BAD, OnBad)
    ON_COMMAND(ID_KEY_NEXT, OnNextSequence)
    ON_COMMAND(ID_KEY_PREVIOUS, OnPreviousSequence)
    ON_COMMAND(ID_KEY_STORE, OnStoreTake)
    ON_COMMAND(ID_KEY_RESET, OnReset)
    ON_COMMAND(ID_FILE_EXPORTGOODTAKELIST, OnFileExportGoodTakeList)
    ON_BN_CLICKED(IDC_BUTTON_REPLAY, OnButtonReplay)
    //}}AFX_MSG_MAP
    // Standard printing commands
    ON_COMMAND(ID_FILE_PRINT, CFormView::OnFilePrint)
    ON_COMMAND(ID_FILE_PRINT_DIRECT, CFormView::OnFilePrint)
    ON_COMMAND(ID_FILE_PRINT_PREVIEW, CFormView::OnFilePrintPreview)
    ON_COMMAND(ID_TOOLS_SETTINGS, OnToolsSettings)
    ON_COMMAND(ID_RECORDER_SELECT, OnRecorderSelect)
    ON_CBN_DROPDOWN(IDC_COMBO_LOCATION, OnCbnDropdownComboLocation)
    ON_CBN_SELCHANGE(IDC_COMBO_LOCATION, OnCbnSelchangeComboLocation)
    ON_BN_CLICKED(IDC_BUTTON_STOP, OnBnClickedButtonStop)
    ON_BN_CLICKED(IDC_BUTTON_START, OnBnClickedButtonStart)
    ON_COMMAND(ID_KEY_STOP, OnKeyStop)
    ON_COMMAND(ID_KEY_START, OnKeyStart)
    ON_CBN_DROPDOWN(IDC_COMBO_SERIES, OnCbnDropdownComboSeries)
    ON_CBN_DROPDOWN(IDC_COMBO_PROG, OnCbnDropdownComboProg)
    ON_CBN_SELCHANGE(IDC_COMBO_SERIES, OnCbnSelchangeComboSeries)
    ON_CBN_SELCHANGE(IDC_COMBO_PROG, OnCbnSelchangeComboProg)
    ON_CBN_DROPDOWN(IDC_COMBO_RECORDER, OnCbnDropdownComboRecorder)
    ON_CBN_SELCHANGE(IDC_COMBO_RECORDER, OnCbnSelchangeComboRecorder)
    ON_EN_KILLFOCUS(IDC_PRODUCER, OnEnKillfocusProducer)
    ON_EN_KILLFOCUS(IDC_DIRECTOR, OnEnKillfocusDirector)
    ON_EN_KILLFOCUS(IDC_PA, OnEnKillfocusPa)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerView construction/destruction

CGalleryLoggerView::CGalleryLoggerView()
    : CFormView(CGalleryLoggerView::IDD)
{
    //{{AFX_DATA_INIT(CGalleryLoggerView)
    mStartTime = _T("");
    mStopTime = _T("");
    mDuration = _T("");
    mComment = _T("");
    mDate = _T("");
    mTimecode = _T("");
    //}}AFX_DATA_INIT
    // TODO: add construction code here
    mRunning = false;
    mStateImages.Create(IDB_STATES, 16, 1, CLR_NONE);
    mTakeHasNewData = false;
    mTakeIndex = -1;
    mCurrentTapeNumber = "none";

    // Columns for printing
    mColumns.push_back(Column("SEQ",8,1));
    mColumns.push_back(Column("TK",4,1));
    mColumns.push_back(Column("TIMECODE",11,1));
    // Description and comments
    mDescriptionColumnIndex = mColumns.size();
    mColumns.push_back(Column("DETAILS",20,-1));  // will be widened if there is room on page
    // Duration
    mColumns.push_back(Column("DUR",5,1));
    mColumns.push_back(Column("DATE",10,1));

    // Timecode source
    mUseExternalTc = true;
    mTcReader.Init("com1");

    // CORBA for remote recording and processing devices
    char * dummy_argv[] = { 
        "-ORBDottedDecimalAddresses", "1" };
    int dummy_argc = 2;

    CorbaUtil::Instance()->InitOrb(dummy_argc, dummy_argv);
    CorbaUtil::Instance()->SetTimeout(5); // seconds

    CString ns = NamingService(); // reads from registry
    ApplyNamingService(ns);

    // Default processing via "Coordinator"
    mContentProcessor.SetDeviceName("Coordinator");
    mContentProcessor.ResolveDevice();

    // Default  recorder is called "Ingex"
    // No longer do this - force user to select a recorder.
    //mRecorder.SetDeviceName("Ingex");
    //mRecorder.ResolveDevice();
}

CGalleryLoggerView::~CGalleryLoggerView()
{
}

void CGalleryLoggerView::DoDataExchange(CDataExchange* pDX)
{
    CFormView::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CGalleryLoggerView)
    DDX_Control(pDX, IDC_GOODNG_INDICATOR, mGoodNgIndicator);
    DDX_Control(pDX, IDC_RECORD_INDICATOR, mRecordIndicator);
    DDX_Control(pDX, IDC_DIRECTOR, mDirectorCtrl);
    DDX_Control(pDX, IDC_PRODUCER, mProducerCtrl);
    DDX_Control(pDX, IDC_PA, mPACtrl);
    DDX_Control(pDX, IDC_TIMECODE, mTimecodeCtrl);
    DDX_Control(pDX, IDC_COMMENT, mCommentCtrl);
    DDX_Control(pDX, IDC_DURATION, mDurationCtrl);
    DDX_Control(pDX, IDC_STOP_TIME, mStopTimeCtrl);
    DDX_Control(pDX, IDC_START_TIME, mStartTimeCtrl);
    DDX_Control(pDX, IDC_TAKE_LIST, mTakeList);
    DDX_Control(pDX, IDC_SEQUENCE_LIST, mSequenceList);
    DDX_Text(pDX, IDC_START_TIME, mStartTime);
    DDX_Text(pDX, IDC_STOP_TIME, mStopTime);
    DDX_Text(pDX, IDC_DURATION, mDuration);
    DDX_Text(pDX, IDC_COMMENT, mComment);
    DDX_Text(pDX, IDC_TIMECODE, mTimecode);
    //}}AFX_DATA_MAP
    DDX_Control(pDX, IDC_COMBO_LOCATION, mComboLocationCtrl);
    DDX_Control(pDX, IDC_COMBO_SERIES, mComboSeriesCtrl);
    DDX_Control(pDX, IDC_COMBO_PROG, mComboProgCtrl);
    DDX_Control(pDX, IDC_COMBO_RECORDER, mComboRecorderCtrl);
}

BOOL CGalleryLoggerView::PreCreateWindow(CREATESTRUCT& cs)
{
    // TODO: Modify the Window class or styles here by modifying
    //  the CREATESTRUCT cs

    return CFormView::PreCreateWindow(cs);
}

void CGalleryLoggerView::OnInitialUpdate()
{
    CFormView::OnInitialUpdate(); // Needed before we can SetFontHeight

    mTakeHasNewData = false;

    static bool first_time = true;
    if (first_time)
    {
        GetParentFrame()->RecalcLayout();
        ResizeParentToFit(FALSE);

        const int big_font_size = 14;

        // Cummulative Duration
        // Not possible to make a useful estimate of this
        //mRecordedDurationCtrl.SetFontHeight(20);
        //mRecordedDurationCtrl.SetFontBold(true);

        // Timecodes
        //mTimecodeCtrl.SetFontHeight(big_font_size);
        //mTimecodeCtrl.SetFontBold(true);
        mStartTimeCtrl.SetFontHeight(big_font_size);
        //mStartTimeCtrl.SetFontBold(true);
        mStopTimeCtrl.SetFontHeight(big_font_size);
        //mStopTimeCtrl.SetFontBold(true);
        mDurationCtrl.SetFontHeight(big_font_size);
        //mDurationCtrl.SetFontBold(true);

        // Take List
        mTakeList.SetExtendedStyle(LVS_EX_FULLROWSELECT);

        int col = 0;
        mTakeList.InsertColumn(col++,"No.",LVCFMT_LEFT,40,0);
        mTakeList.InsertColumn(col++,"Date",LVCFMT_LEFT,80,0);
        mTakeList.InsertColumn(col++,"Location",LVCFMT_LEFT,80,0);
        mTakeList.InsertColumn(col++,"In",LVCFMT_LEFT,80,0);
        mTakeList.InsertColumn(col++,"Out",LVCFMT_LEFT,80,0);
        mTakeList.InsertColumn(col++,"Duration",LVCFMT_LEFT,80,0);
        mTakeList.InsertColumn(col++,"Result",LVCFMT_LEFT,60,0);
        mTakeList.InsertColumn(col++,"Comment",LVCFMT_LEFT,500,0);

        // Sequence List
        mSequenceList.SetExtendedStyle(LVS_EX_FULLROWSELECT);

        mSequenceList.InsertColumn(0,"Description",LVCFMT_LEFT,580,0);
        mSequenceList.InsertColumn(1,"Sequence",LVCFMT_LEFT,80,0);

        mSequenceList.SetImageList(&mStateImages, LVSIL_STATE);

        SetTimer(TC_TIMER_ID, 100, NULL);

        //GetDlgItem(IDC_STOP_RECORD_BUTTON)->EnableWindow(FALSE);

        first_time = false;
    }

    // Although this has already been done by base class OnInitialUpdate,
    // we need to do it again becuase take table wasn't constructed at that time.
    OnUpdate(0, 0, 0);
}

/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerView printing

BOOL CGalleryLoggerView::OnPreparePrinting(CPrintInfo* pInfo)
{
    // default preparation
    return DoPreparePrinting(pInfo);
}

void CGalleryLoggerView::OnBeginPrinting(CDC* p_dc, CPrintInfo* p_info)
{
    // TODO: add extra initialization before printing

    // Create font
    mPrinterFont.CreatePointFont(100, "Courier New", p_dc);

    // Compute height of a line and character width in printer font
    TEXTMETRIC tm;
    CFont * old_font = p_dc->SelectObject(&mPrinterFont);
    p_dc->GetTextMetrics(&tm);
    mPrinterCy = tm.tmHeight + tm.tmExternalLeading;
    mPrinterCy += mPrinterCy/4; // to allow for horizontal lines etc.
    mPrinterCx = tm.tmAveCharWidth;

    // Horizontal/vertical width available
    mPrinterTotalx = p_dc->GetDeviceCaps(HORZRES);
    mPrinterTotaly = p_dc->GetDeviceCaps(VERTRES);

    // Pixels per inch
    mPrinterPpix = p_dc->GetDeviceCaps(LOGPIXELSX);  // horizontal
    mPrinterPpiy = p_dc->GetDeviceCaps(LOGPIXELSY);  // vertical

    // Margins
    mPrinterMarginLeft = (mPrinterPpix * 3)/4;
    mPrinterMarginRight = mPrinterTotalx - (mPrinterPpix * 3)/4;

    mTotalCharWidth = (mPrinterMarginRight - mPrinterMarginLeft) / mPrinterCx;

    // Compute lines per page
    UINT lines_per_page = mPrinterTotaly / mPrinterCy;
    mLinesPerPage = lines_per_page - header_lines;


    // Set width of the variable column (description)
    int total = 1;
    for(unsigned int i = 0; i < mColumns.size(); ++i)
    {
        if(i != mDescriptionColumnIndex)
        {
            total += (mColumns[i].Width + 1);
        }
    }
    if(total < mTotalCharWidth)
    {
        mColumns[mDescriptionColumnIndex].Width = mTotalCharWidth - total;
    }
    else
    {
        mColumns[mDescriptionColumnIndex].Width = 10;
    }

    unsigned int width = mColumns[mDescriptionColumnIndex].Width;

    // Change mFormat to reflect this column width
    mFormat = "";
    for (int i = 0; i < mColumns.size(); ++i)
    {
        char tmp[10];
        sprintf(tmp, " %%%ds", mColumns[i].Width * mColumns[i].Justification);
        mFormat += tmp;
    }


    // Now work out the pagination

    // NB. It would be even better to make sure a sequence is not split
    // across a page boundary (unless it had more takes than would fit
    // on one page.
    CGalleryLoggerDoc * p_doc = GetDocument();

    int page_lines = 0;
    SequenceTake st;
    mPageStarts.clear();
    mPageEnds.clear();
    unsigned int item_i;
    unsigned int take_i;
    for (item_i = 0; item_i < p_doc->ItemCount(); ++item_i)
    {
        //Sequence & sequence = p_doc->Sequence(s);
        prodauto::Item * item = p_doc->Item(item_i);

        for (take_i = 0; take_i < item->getTakes().size(); ++take_i)
        {
            //Take & take = sequence.Take(t);
            prodauto::Take * take = item->getTakes()[take_i];

            if(0 == page_lines)
            {
                // starting a new page
                st.Sequence = item_i;
                st.Take = take_i;
                mPageStarts.push_back(st);
            }

            // "print" this take
            // How many lines are needed for description/comments?
            std::vector<std::string> tmp_lines;
            FormatDescriptionText(item->description, take, width, tmp_lines);
            int lines = tmp_lines.size();

            // Need at least 2 lines in timecode column
            if (lines < 2)
            {
                lines = 2;
            }
            page_lines += lines;

            if(page_lines + 4 > mLinesPerPage)  // really need to look at size of next take
            {
                // page full
                page_lines = 0;
                st.Sequence = item_i;
                st.Take = take_i;
                mPageEnds.push_back(st);
            }
        } // for t
    } // for s

    if(page_lines > 0)
    {
        // last page partially filled
        st.Sequence = item_i - 1;
        st.Take = take_i - 1;
        mPageEnds.push_back(st);
    }

    // Set total pages
    p_info->SetMaxPage(mPageStarts.size());
}

void CGalleryLoggerView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
    // TODO: add cleanup after printing
    mPrinterFont.DeleteObject();
}

void CGalleryLoggerView::OnPrint(CDC* pDC, CPrintInfo* pInfo)
{
    // TODO: add customized printing code here

    PrintHeader(pDC, pInfo->m_nCurPage);
    PrintPage(pDC, pInfo->m_nCurPage);
}

void CGalleryLoggerView::PrintHeader(CDC * p_dc, int page_number)
{
    int x = mPrinterMarginLeft;
    int y = 0;
    CString cs;
    CGalleryLoggerDoc * p_doc = GetDocument();

    char buf[32];
    cs = "EDIT LOG   Page ";
    itoa(page_number, buf, 10);
    cs += buf;
    p_dc->TextOut(x, y, cs);
    y += mPrinterCy;

    cs = "Series:    ";
    cs += p_doc->CurrentSeriesName();
    p_dc->TextOut(x, y, cs);
    y += mPrinterCy;

    cs = "Programme: ";
    cs += p_doc->CurrentProgrammeName();
    p_dc->TextOut(x, y, cs);
    y += mPrinterCy;

    cs = "Producer:  ";
    cs += p_doc->Producer();
    p_dc->TextOut(x, y, cs);
    y += mPrinterCy;

    cs = "Director:  ";
    cs += p_doc->Director();
    p_dc->TextOut(x, y, cs);
    y += mPrinterCy;

    cs = "PA:        ";
    cs += p_doc->PA();
    p_dc->TextOut(x, y, cs);
    y += mPrinterCy;

    y += mPrinterCy; // blank line

    // horizontal line
    y -= mPrinterCy/8;
    p_dc->MoveTo(x, y);
    p_dc->LineTo(mPrinterMarginRight, y);
    y += mPrinterCy/8;

    // Column headings
    cs = "";
    for (unsigned int i = 0; i < mColumns.size(); ++i)
    {
        cs += " ";
        int w = mColumns[i].Width - mColumns[i].Label.size();
        while(w > 0)
        {
            cs += " ";
            --w;
        }
        cs += mColumns[i].Label.c_str();
    }
    p_dc->TextOut(x, y, cs);
    y += mPrinterCy;

    // horizontal line
    y -= mPrinterCy/8;
    p_dc->MoveTo(x, y);
    p_dc->LineTo(mPrinterMarginRight, y);
    y += mPrinterCy/8;

    // double horizontal line
    y -= mPrinterCy/6;
    p_dc->MoveTo(x, y);
    p_dc->LineTo(mPrinterMarginRight, y);
    y += mPrinterCy/6;
}

void CGalleryLoggerView::PrintPage(CDC * p_dc, int page_number)
{
    int x = mPrinterMarginLeft;
    int y = header_lines * mPrinterCy;
    int top = (header_lines - 1) * mPrinterCy - mPrinterCy/8;
    CString cs;

    CGalleryLoggerDoc * p_doc = GetDocument();

    int lines_this_page = 0;

    for (unsigned int item_i = mPageStarts[page_number - 1].Sequence; item_i <= mPageEnds[page_number - 1].Sequence; ++item_i)
    {
        //Sequence & sequence = p_doc->Sequence(seq);
        prodauto::Item * item = p_doc->Item(item_i);

        // Work out which takes to print on this page
        unsigned int first_take;
        if (item_i == mPageStarts[page_number - 1].Sequence)
        {
            first_take = mPageStarts[page_number - 1].Take;
        }
        else
        {
            first_take = 0;
        }
        unsigned int last_take;
        if( item_i < mPageEnds[page_number - 1].Sequence)
        {
            last_take = item->getTakes().size();
        }
        else
        {
            last_take = mPageEnds[page_number - 1].Take + 1;
        }

        for (unsigned int take_i = first_take; take_i < last_take; ++take_i)
        {
            //Take & take = sequence.Take(t);
            prodauto::Take * db_take = item->getTakes()[take_i];
            ::Take take(db_take);

            std::vector<std::string> descript_lines;
            FormatDescriptionText(item->description, take, mColumns[mDescriptionColumnIndex].Width, descript_lines);

            // first line
            unsigned int i = 0;
            char tmp[8];
            Duration duration = take.Out() - take.In();
            cs.Format(mFormat.c_str(),
                p_doc->ItemScriptRefs(item_i).c_str(),
                ACE_OS::itoa(take.Number(), tmp, 10),
                take.In().Text(),
                (descript_lines.size() > i ? descript_lines[i].c_str() : ""),
                duration.Text(),
                take.DateText().c_str()
                );

            p_dc->TextOut(x, y, cs);
            ++lines_this_page;
#if 0
            // circle good take
            if(p_doc->Sequence(seq).Take(t).IsGood())
            {
                POINT points[5] = { x +  8 * mPrinterCx, y,
                                    x + 11 * mPrinterCx, y,
                                    x + 11 * mPrinterCx, y + mPrinterCy,
                                    x +  8 * mPrinterCx, y + mPrinterCy,
                                    x +  8 * mPrinterCx, y };
                p_dc->Polyline(points, 5);
            }
#endif
            ++i;

            // second line
            y += mPrinterCy;
            cs.Format(mFormat.c_str(), "",
                (take.IsGood() ? "GOOD" : ""),
                take.Out().Text(),
                (descript_lines.size() > i ? descript_lines[i].c_str() : ""),
                "",
                "");
            p_dc->TextOut(x, y, cs);
            ++lines_this_page;
            ++i;

#if 0
            // third line
            y += mPrinterCy;
            cs.Format(mFormat.c_str(), "",
                "",
                "",
                (descript_lines.size() > i ? descript_lines[i].c_str() : ""),
                "",
                "");
            p_dc->TextOut(x, y, cs);
            ++lines_this_page;
            ++i;
#endif

            // subsequent lines
            for (; i < descript_lines.size(); ++i)
            {
                y += mPrinterCy;
                cs.Format(mFormat.c_str(), "",
                    "",
                    "",
                    (descript_lines.size() > i ? descript_lines[i].c_str() : ""),
                    "",
                    "");
                p_dc->TextOut(x, y, cs);
                ++lines_this_page;
            }

            // horizontal line between takes
            y += mPrinterCy;
            y -= mPrinterCy/8;
            p_dc->MoveTo(x, y);
            p_dc->LineTo(mPrinterMarginRight, y);
            y += mPrinterCy/8;
        } // for each take

        // horizontal line between sequences
        y -= mPrinterCy/6;
        p_dc->MoveTo(x, y);
        p_dc->LineTo(mPrinterMarginRight, y);
        y += mPrinterCy/6;

    } // for each sequence

    // vertical lines
    int bottom = y - mPrinterCy/8;
    x = mPrinterMarginLeft;
    p_dc->MoveTo(x, bottom);
    p_dc->LineTo(x, top);

    int char_offset = 0;
    for(unsigned int i = 0; i < mColumns.size(); ++i)
    {
        char_offset += (mColumns[i].Width + 1);
        x = mPrinterMarginLeft + char_offset * mPrinterCx + mPrinterCx/2;
        p_dc->MoveTo(x, bottom);
        p_dc->LineTo(x, top);
    }
/*
    x = mPrinterMarginLeft + 4 * mPrinterCx + mPrinterCx/2;
    p_dc->MoveTo(x, bottom);
    p_dc->LineTo(x, top);

    x = mPrinterMarginLeft + 9 * mPrinterCx + mPrinterCx/2;
    p_dc->MoveTo(x, bottom);
    p_dc->LineTo(x, top);

    x = mPrinterMarginLeft + 16 * mPrinterCx + mPrinterCx/2;
    p_dc->MoveTo(x, bottom);
    p_dc->LineTo(x, top);

    x = mPrinterMarginLeft + 28 * mPrinterCx + mPrinterCx/2;
    p_dc->MoveTo(x, bottom);
    p_dc->LineTo(x, top);

    x = mPrinterMarginLeft + 39 * mPrinterCx + mPrinterCx/2;
    p_dc->MoveTo(x, bottom);
    p_dc->LineTo(x, top);

    x = mPrinterMarginLeft + 70 * mPrinterCx + mPrinterCx/2;
    p_dc->MoveTo(x, bottom);
    p_dc->LineTo(x, top);

    x = mPrinterMarginLeft + 78 * mPrinterCx + mPrinterCx/2;
    p_dc->MoveTo(x, bottom);
    p_dc->LineTo(x, top);
*/
}

/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerView diagnostics

#ifdef _DEBUG
void CGalleryLoggerView::AssertValid() const
{
    CFormView::AssertValid();
}

void CGalleryLoggerView::Dump(CDumpContext& dc) const
{
    CFormView::Dump(dc);
}

CGalleryLoggerDoc* CGalleryLoggerView::GetDocument() // non-debug version is inline
{
    ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CGalleryLoggerDoc)));
    return (CGalleryLoggerDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerView message handlers

void CGalleryLoggerView::OnUpdate(CView *pSender, LPARAM lHint, CObject *pHint)
{
    CGalleryLoggerDoc * p_doc = GetDocument();

    // current programme
    if (p_doc->ProgrammeIndex() < 0)
    {
        mComboProgCtrl.ResetContent();
    }

    if (CGalleryLoggerDoc::NOT_SEQUENCE_LIST != lHint)
    {
        mSequenceList.DeleteAllItems();
        for (unsigned int i = 0; i < p_doc->ItemCount(); ++i)
        {
            mSequenceList.InsertItem(i, p_doc->ItemDescription(i));
            mSequenceList.SetItemText(i, 1, p_doc->ItemScriptRefs(i).c_str());

            // Set the "has good take" state of item.
            int image = (p_doc->ItemHasGoodTake(i) ? 2 : 1);
            // A state of 0 means no image.  First image in list is 1.
            mSequenceList.SetItemState(i, INDEXTOSTATEIMAGEMASK(image), LVIS_STATEIMAGEMASK);
        }
    }

    // current sequence
    {
        int i = p_doc->ItemIndex();
        if (i >= 0)
        {
            mSequenceList.SetItemState(i, LVIS_SELECTED, LVIS_SELECTED);
            mSequenceList.EnsureVisible(i, FALSE);
        }
    }

    // recorded duration
    //mRecordedDuration = p_doc->RecordedDuration();

    // take list
    mTakeList.DeleteAllItems();
    for (unsigned int i = 0; i < p_doc->TakeCount(); ++i)
    {
        // Convert from database take to our own take class.
        ::Take take = p_doc->Take(i);

        Duration dur = take.Out() - take.In();

        // Get location as text
        std::string location_name;
        long loc_i = take.Location();
        const std::map<long, std::string> & locations = p_doc->RecordingLocations();
        std::map<long, std::string>::const_iterator it = locations.find(loc_i);
        if (it != locations.end())
        {
            location_name = it->second;
        }

        char tmp[8];
        mTakeList.InsertItem(i, ACE_OS::itoa(take.Number(), tmp, 10));
        int col = 1;
        mTakeList.SetItemText(i, col++, take.DateText().c_str());
        mTakeList.SetItemText(i, col++, location_name.c_str());
        mTakeList.SetItemText(i, col++, take.In().Text());
        mTakeList.SetItemText(i, col++, take.Out().Text());
        mTakeList.SetItemText(i, col++, dur.Text());
        mTakeList.SetItemText(i, col++, take.ResultText());
        mTakeList.SetItemText(i, col++, take.Comment());
    }


    // in/out times etc.
    ClearTakeDisplay();

    // Programme personnel
    //mProducer = p_doc->Producer();
    //mDirector = p_doc->Director();
    //mPA = p_doc->PA();
    mProducerCtrl.SetWindowText(p_doc->Producer());
    mDirectorCtrl.SetWindowText(p_doc->Director());
    mPACtrl.SetWindowText(p_doc->PA());

    UpdateData(FALSE);
}

#if 0 // Now in CGalleryLoggerApp
void CGalleryLoggerView::OnFileImport() 
{
    // Now really you want to do a File->New first.  To do that you would have
    // to put a handler in the CGallerLoggerApp that calls OnFileNew.  That's
    // fine but then how to call this code in the View class?
    // Actually, I think you would do the file dialog from the App class and
    // then use the document template to get the document and then call Import
    // on it.

    const char * filter = "ScriptWriter Files (*.swd)|*.swd|All Files (*.*)|*.*||";

    CFileDialog file_dialog(TRUE, ".swd", NULL, 0, filter);
    if( IDOK == file_dialog.DoModal())
    {
        GetDocument()->Import(file_dialog.GetPathName());
    }
}
#endif

void CGalleryLoggerView::OnClickSequenceList(NMHDR* pNMHDR, LRESULT* pResult) 
{
    // User has clicked in the sequence list
    SelectSequence();
    
    *pResult = 0;
}

void CGalleryLoggerView::OnDblclkSequenceList(NMHDR* pNMHDR, LRESULT* pResult) 
{
    // User has double-clicked in the sequence list
    POSITION p = mSequenceList.GetFirstSelectedItemPosition();
    if(p != NULL)
    {
        int row = mSequenceList.GetNextSelectedItem(p);
        if (row < mSequenceList.GetItemCount())
        {
            // Get pointer to item
            prodauto::Item * item = GetDocument()->Item(row);

            // Show the dialog
            ItemDlg item_dialog;
            item_dialog.CopyFrom(item);
            if (IDOK == item_dialog.DoModal())
            {
                // Save the updated item
                item_dialog.CopyTo(item);
                GetDocument()->SaveItem(item);
            }
        }
    }

    *pResult = 0;
}

void CGalleryLoggerView::SelectSequence()
{
    // in case the last take wasn't stored
    if(mRunning)
    {
        Stop();
    }
    if(mTakeHasNewData)
    {
        OnStoreTake();
    }
    // now move to new sequence
    POSITION p = mSequenceList.GetFirstSelectedItemPosition();
    if(p != NULL)
    {
        int row = mSequenceList.GetNextSelectedItem(p);
        if (row < mSequenceList.GetItemCount())
        {
            GetDocument()->SelectItem(row);
        }
    }
}

void CGalleryLoggerView::OnGood() 
{
    // User has clicked/pressed the Good button
    // TODO: Add your control notification handler code here

    if(mRunning)
    {
        Stop();
    }
    
    if(mTakeHasNewData)
    {
        mIsGood = true;
        UpdateData(FALSE);
    }

    ShowGoodNg(mIsGood);
}

void CGalleryLoggerView::OnBad() 
{
    // User has clicked/pressed the Bad button
    // TODO: Add your control notification handler code here

    if(mRunning)
    {
        Stop();
    }
    
    if(mTakeHasNewData)
    {
        mIsGood = false;
        UpdateData(FALSE);
    }

    ShowGoodNg(mIsGood);
}

void CGalleryLoggerView::ShowGoodNg(bool good)
{
    if(good)
    {
        HICON good = AfxGetApp()->LoadIcon(IDI_ICON_GOOD);
        mGoodNgIndicator.SetIcon(good);
    }
    else
    {
        HICON ng = AfxGetApp()->LoadIcon(IDI_ICON_NG);
        mGoodNgIndicator.SetIcon(ng);
    }
}

void CGalleryLoggerView::ClearGoodNg()
{
    mGoodNgIndicator.SetIcon(NULL);
}


/*
Data exists in three places:
1. The display
2. The member variables associated with the display
3. The document

And now mCurrentTake as well.

Transfer between 1 and 2 is accomplished using UpdateData and additionally
for the Info display (Good/NG, Pick-up) with UpdateTakeInfo.

Transfer between 2 and 3 is accomplished using operations on the document.

The overall transfer from 3 to 1 is performed by TransferTakeToDisplay()
*/


void CGalleryLoggerView::ClearTakeDisplay() 
{
    mRunning = false;
    mStartTime = "";
    mStopTime = "";
    mDuration = "";
    mComment = "";
    //mDate = ClockTime::Date();
    mIsGood = false;
    //mIsPickup = false;
    //mInfo = "";
    //mTape1 = mCurrentTapeNumber;
    ClearGoodNg();

    UpdateData(FALSE);
}

void CGalleryLoggerView::TransferTakeToDisplay()
{
    Take & take = mCurrentTake;;

    mStartTime = take.In().Text();
    mStopTime = take.Out().Text();
    mDate = take.DateText().c_str();
    mDuration = (take.Out() - take.In()).Text();
    mComment = take.Comment();
    mIsGood = take.IsGood();
    ShowGoodNg(mIsGood);

    /*
    No, we don't try to show source details for a particular take
    for(int i=0; i<take.SourceCount(); ++i)
    {
        SourceCheck(i) = TRUE;
        SourceDisplay(i) = take.RecordedSource(i).Source().Name();
        SourceDescript(i) = take.RecordedSource(i).Source().Description();
        //TapeDisplay(i) = take.Clip(i).TapeNumber();
    }
    */
}

void CGalleryLoggerView::TransferTakeToDocument()
{
    UpdateData(TRUE); // to retrieve data from display

    mCurrentTake.In(Timecode(mStartTime));
    mCurrentTake.Out(Timecode(mStopTime));
    mCurrentTake.Comment(mComment);
    //mCurrentTake.Date(mDate);
    mCurrentTake.IsGood(mIsGood);


    if(mTakeIndex == -1)
    {
        //mCurrentTapeNumber = mTape0;
        GetDocument()->AddTake(mCurrentTake);
    }
    else
    {
        GetDocument()->ModifyTake(mTakeIndex, mCurrentTake);
    }

    // Auto-publish if storing good take
    //if(mCurrentTake.IsGood())
    //{
    //  Publish();
    //}
}

/*void CGalleryLoggerView::OnStartStop() 
{
    std::string timecode = CurrentTimecode();
    if(mRunning)
    {
        Stop();
    }
    else
    {
        if(GetDocument()->CurrentSequenceIndex() == -1)
        {
            // No items in document so make one
            GetDocument()->InsertBlankSequence();
        }
    // Store any previous unstored take
        if(mTakeHasNewData)
        {
            OnStoreTake();
        }
    // Start
        mRunning = true;
        StartRecord(timecode.c_str());  // recorder with add pre-roll
        BeginTake(timecode.c_str());
    }

    UpdateData(FALSE);
}*/

void CGalleryLoggerView::Stop()
{
    if (mRunning)
    {
        std::string timecode = CurrentTimecode();
        mRunning = false;
        StopRecord(timecode.c_str());  // recorder will add post-roll
        EndTake(timecode.c_str());
        mTakeHasNewData = true;

        mCommentCtrl.SetFocus(); // ready for a comment to be entered

        UpdateData(FALSE);
    }
}

void CGalleryLoggerView::Start()
{
    if (! mRunning)
    {
    // Get start timecode
        std::string timecode = CurrentTimecode();
    // Check we have an item selected
        if (GetDocument()->ItemIndex() == -1)
        {
            // No items in document so make one
            AddBlankSequence();
        }
    // Store any previous unstored take
        if (mTakeHasNewData)
        {
            OnStoreTake();
        }
    // Start
        mRunning = true;
        StartRecord(timecode.c_str());  // pre-roll will be added
        BeginTake(timecode.c_str());

        UpdateData(FALSE);
    }
}

void CGalleryLoggerView::OnReset() 
{
    // TODO: Add your control notification handler code here
    if(mRunning)
    {
    // Reset the "stopwatch"
        mRunning = false;
        mStartTime = "";
        mStopTime = "";
        mDuration = "";
    }
    
    UpdateData(FALSE);
}

void CGalleryLoggerView::OnTimer(UINT id) 
{
    // TODO: Add your message handler code here and/or call default
    
    if(TC_TIMER_ID == id)
    {
        //Timecode tc;
        //tc.SetToCurrentTime();
        // update timecode window
        //mTimecode = tc.Text();
        mTimecode = CurrentTimecode().c_str();
        mTimecodeCtrl.SetWindowText(mTimecode);

        if(mRunning)
        {
        // update Out time and Duration
            mStopTime = mTimecode;

            Duration duration;
            Timecode tc_tmp((const char *)mStopTime);
            duration = Timecode((const char *)mStopTime) - Timecode((const char *)mStartTime);
            mDuration = duration.Text();

            mStopTimeCtrl.SetWindowText(mStopTime);
            mDurationCtrl.SetWindowText(mDuration);
        }

    }

    CFormView::OnTimer(id);
}


void CGalleryLoggerView::OnNextSequence() 
{
    // TODO: Add your control notification handler code here
    if(mRunning)
    {
        Stop();
    }
    if(mTakeHasNewData)
    {
        OnStoreTake();
    }
    GetDocument()->SelectItem(GetDocument()->ItemIndex() + 1);
}


void CGalleryLoggerView::OnPreviousSequence() 
{
    // TODO: Add your control notification handler code here
    if(mRunning)
    {
        Stop();
    }
    if(mTakeHasNewData)
    {
        OnStoreTake();
    }
    GetDocument()->SelectItem(GetDocument()->ItemIndex() - 1);
}

void CGalleryLoggerView::OnButtonRepeat() 
{
    AddRepeat();
}


void CGalleryLoggerView::OnFileExport() 
{
#if 0
    const char * filter = "ALE Files (*.ale)|*.ale|All Files (*.*)|*.*||";

    // Get document filename minus ".log" extension
    std::string basename = GetDocument()->GetTitle();
    size_t n = basename.find(".log");
    basename = basename.substr(0, n);

    // Save separate ALE file for each source
    for(int i = 0; i < GetDocument()->Sources().size(); ++i)
    {
        const Source & source = GetDocument()->Sources()[i];
        std::string filename = basename + "_" + source.Name() + ".ale";

        CFileDialog file_dialog(FALSE, ".ale", filename.c_str(), OFN_OVERWRITEPROMPT, filter);
        std::string title = "Save ALE for source \"";
        title += source.Name();
        title += "\"";
        file_dialog.m_ofn.lpstrTitle = title.c_str();
        if(IDOK == file_dialog.DoModal())
        {
            GetDocument()->ExportAle(file_dialog.GetPathName(), i);
        }
    }
#endif
}

void CGalleryLoggerView::OnFileExportSmil() 
{
#if 0
    const char * filter = "SMIL Files (*.smil)|*.smil|All Files (*.*)|*.*||";

    CFileDialog file_dialog(FALSE, ".smil", NULL, OFN_OVERWRITEPROMPT, filter);
    if(IDOK == file_dialog.DoModal())
    {
        GetDocument()->ExportSmil(file_dialog.GetPathName());
    }
#endif
}

void CGalleryLoggerView::OnFileExportToFcp() 
{
#if 0
    const char * filter = "XML Files (*.xml)|*.xml|All Files (*.*)|*.*||";

    CFileDialog file_dialog(FALSE, ".xml", NULL, OFN_OVERWRITEPROMPT, filter);
    if(IDOK == file_dialog.DoModal())
    {
        GetDocument()->ExportToFcpOld(file_dialog.GetPathName());
        //GetDocument()->ExportToFcp(file_dialog.GetPathName());
    }
#endif
}

void CGalleryLoggerView::OnFileExportXml() 
{
#if 0
    const char * filter = "XML Files (*.xml)|*.xml|All Files (*.*)|*.*||";

    CFileDialog file_dialog(FALSE, ".xml", NULL, OFN_OVERWRITEPROMPT, filter);
    if(IDOK == file_dialog.DoModal())
    {
        GetDocument()->ExportXml(file_dialog.GetPathName());
    }
#endif
}

void CGalleryLoggerView::OnStoreTake() 
{
    // TODO: Add your control notification handler code here
    if(mTakeHasNewData)
    {
        TransferTakeToDocument();
        ClearTakeDisplay();
        mTakeIndex = -1;
        mTakeHasNewData = false;
    }
}

void CGalleryLoggerView::OnClickTakeList(NMHDR* pNMHDR, LRESULT* pResult) 
{
    // Single-clicking on a take initiates editing of that take
    POSITION p = mTakeList.GetFirstSelectedItemPosition();
    if(p != NULL)
    {
        int row = mTakeList.GetNextSelectedItem(p);
        if (row < mTakeList.GetItemCount())
        {
            // store any current take data
            OnStoreTake();
            // edit selected take
            mTakeIndex = row;
            EditTake();
        }
    }
    
    *pResult = 0;
}

void CGalleryLoggerView::OnDblclkTakeList(NMHDR* pNMHDR, LRESULT* pResult) 
{
    // Double-clicking on a take initiates editing of that take
    POSITION p = mTakeList.GetFirstSelectedItemPosition();
    if(p != NULL)
    {
        int row = mTakeList.GetNextSelectedItem(p);
        if (row < mTakeList.GetItemCount())
        {
            mTakeIndex = row;
            EditTake();
        }
    }
    
    *pResult = 0;
}




std::string CGalleryLoggerView::CurrentTimecode()
{
    // Tidy this sometime to use TimecodeReader polymorphism

    if(mUseExternalTc) // use Timecode reader
    {
        return mTcReader.Timecode();
    }
    else // use PC clock
    {
        return ClockTime::Timecode();
    }
}

void CGalleryLoggerView::OnTimecodePcInternal() 
{
    // TODO: Add your command handler code here
    mUseExternalTc = false;
}

void CGalleryLoggerView::OnTimecodeEasyreader() 
{
    // TODO: Add your command handler code here
    mUseExternalTc = true;
    //mTcReader.Init();
}

// Set appropriate checks against Timecode menu items
void CGalleryLoggerView::OnUpdateTimecodePcInternal(CCmdUI* pCmdUI) 
{
    // TODO: Add your command update UI handler code here
    pCmdUI->SetCheck(!mUseExternalTc);
}

void CGalleryLoggerView::OnUpdateTimecodeEasyreader(CCmdUI* pCmdUI) 
{
    // TODO: Add your command update UI handler code here
    pCmdUI->SetCheck(mUseExternalTc);   
}


void CGalleryLoggerView::OnStartRecordButton() 
{
    StartRecord(CurrentTimecode());
}

void CGalleryLoggerView::StartRecord(std::string tc)
{
    if(USE_RECORDER)
    {
    // Generic recorder version
        //std::string timecode = tc;
#if 0
        bool enable[SOURCES_DISPLAYABLE];
        const char * tape[SOURCES_DISPLAYABLE];
        std::vector<Source> & sources = GetDocument()->Sources();
        for(int i=0; i<sources.size(); ++i)
        {
            enable[i] = sources[i].Enabled();
            tape[i] = TapeDisplay(i);
        }
        // Construct filename
        std::string filename;
        filename += GetDocument()->SeriesName();
        filename += '/';
        filename += GetDocument()->ProgrammeName();
        filename += '/';
        filename += 'S';
        filename += GetDocument()->CurrentSequence().UniqueId();
        filename += '/';
        // Work out take number
        int tn = GetDocument()->CurrentSequence().TakeCount() + 1;
        char take_id[16];
        ACE_OS::sprintf(take_id, "T%02d", tn);
        filename += take_id;

        // Construct descriptiom
        std::string description = "Seq. ";
        description += GetDocument()->CurrentSequence().ScriptRefRange();
        ACE_OS::sprintf(take_id, "  Take %d\n", tn);
        description += take_id;
        description += GetDocument()->CurrentSequence().Name();
#endif
        std::string description;
        if (GetDocument()->HasProgramme())
        {
            description += GetDocument()->CurrentSeriesName();
            description += ": ";
            description += GetDocument()->CurrentProgrammeName();
        }

        // Start recording
        // If attempt fails, we ResolveDevice and if that succeeds we try again.
        RecordingDevice::ReturnCode result;
        if(RecordingDevice::NO_DEVICE ==
            (result =
            mRecorder.StartRecording(tc.c_str(), description))
            && RecordingDevice::OK == mRecorder.ResolveDevice())
        {
            result =
            mRecorder.StartRecording(tc.c_str(), description);
        }


        if(RecordingDevice::OK == result)
        {
            mRecording = true;
        }
        else
        {
            mRecording = false;
        }
    }

    if(mRecording)
    {   
        HICON red = AfxGetApp()->LoadIcon(IDI_ICON_RECORDING);
        mRecordIndicator.SetIcon(red);

        //GetDlgItem(IDC_STOP_RECORD_BUTTON)->EnableWindow(TRUE);
        //GetDlgItem(IDC_START_RECORD_BUTTON)->EnableWindow(FALSE);
    }
}

void CGalleryLoggerView::OnStopRecordButton() 
{
    StopRecord(CurrentTimecode());
}

void CGalleryLoggerView::StopRecord(std::string tc)
{
    if (USE_RECORDER)
    {
    // generic recorder version
        if(RecordingDevice::OK ==
            mRecorder.StopRecording(tc.c_str()))
        {
            // store the out time

        }
    }

    mRecording = false;
    mRecordIndicator.SetIcon(NULL);
}


void CGalleryLoggerView::OnChangeComment() 
{
    UpdateData();  // make sure entry is captured in member variable
}


void CGalleryLoggerView::BeginTake(const char * tc)
{
    mCurrentTake.Clear();
    mCurrentTake.In(Timecode(tc));
    //mCurrentTake.Date(ClockTime::Date());
    int year, month, day;
    DateTime::GetDate(year, month, day);
    mCurrentTake.Year(year);
    mCurrentTake.Month(month);
    mCurrentTake.Day(day);

    mStartTime = tc;
}

void CGalleryLoggerView::EndTake(const char * tc)
{
    // Store Out time
    //mCurrentTake.Out(Timecode(CurrentTimecode()));
    mCurrentTake.Out(Timecode(tc));

    // Store any comment entered while take was underway
    mCurrentTake.Comment(mComment);

#if 0
    // Store clip info
    std::vector<Source> & sources = GetDocument()->Sources();
    for(int i=0; i<sources.size(); ++i)
    {
        if(sources[i].Enabled())
        {
            RecordedSource rec_source;
            rec_source.Source().Name(sources[i].Name());
            rec_source.Source().Description(sources[i].Description());

            // add a clip for tape recording
            Recording tape_recording;
            //tape_recording.Type(Recording::TAPE);
            tape_recording.Format(Recording::TAPE);
            tape_recording.TapeId(TapeDisplay(i));

            Clip tape_clip;
            tape_clip.Recording(tape_recording);

            rec_source.AddClip(tape_clip);

            // add a clip for file recording
            if(USE_RECORDER)
            {
                Recording file_recording;
                //file_recording.Type(Recording::FILE);
                file_recording.Format(Recording::FILE);
                file_recording.FileId(mRecorder.Filename(i));
                file_recording.FileStartTimecode(mRecorder.InTime());
                file_recording.FileEndTimecode(
                    Timecode(mRecorder.InTime())
                    + Timecode(mRecorder.Duration()));

                Clip file_clip;
                file_clip.Recording(file_recording);
                file_clip.Start(mCurrentTake.In() - file_recording.FileStartTimecode());
                file_clip.Duration(mCurrentTake.Duration());

                if(mRecorder.HasRecorded())
                {
                    rec_source.AddClip(file_clip);
                }
            }

            // add recorded source to take
            mCurrentTake.AddRecordedSource(rec_source);
        }
    }
#endif

    TransferTakeToDisplay();
}


void CGalleryLoggerView::OnProcessorSelect() 
{
    ProcessorSelectDlg dlg;

    if(IDOK == dlg.DoModal())
    {
        // get selection
        std::string device_name = dlg.Selection();

        // set processor
        mContentProcessor.SetDeviceName(device_name.c_str());
        mContentProcessor.ResolveDevice();
    }
}

void CGalleryLoggerView::OnButtonProcess() 
{
    Publish();
}

void CGalleryLoggerView::Publish() 
{
    // save log file
    //GetDocument()->DoFileSave();

    // Publish log file to agreed location
    const std::string win_prefix = "P:";
    const std::string unix_prefix = "/ProdAuto";

    const char * win_delim = "\\";
    const char * unix_delim = "/";
    const std::string prog_directory = "programmes";
    const std::string log_directory = "published-log";
    std::string title = GetDocument()->GetTitle();
    // Remove ".log"
    std::string::size_type p = title.rfind('.');
    std::string title_base = title.substr(0, p);

    // Windows pathnames
    std::string win_pathname = win_prefix + win_delim
        + prog_directory + win_delim
        + GetDocument()->CurrentSeriesName() + win_delim
        + GetDocument()->CurrentProgrammeName() + win_delim
        + log_directory + win_delim
        + title_base;
    std::string win_xml_pathname = win_pathname;
    std::string win_ale_base = win_pathname;
    win_pathname += ".log";
    win_xml_pathname += ".xml";

    // Unix pathname
    std::string unix_pathname = unix_prefix + unix_delim
        + prog_directory + unix_delim
        + GetDocument()->CurrentSeriesName() + unix_delim
        + GetDocument()->CurrentProgrammeName() + unix_delim
        + log_directory + unix_delim
        + title;

    // Save log to network drive.
    GetDocument()->SaveLogTo(win_pathname.c_str());

#if 0
    // Save xml log to network drive.
    GetDocument()->ExportXml(win_xml_pathname.c_str());

    // Save ALE files to network drive
    for(int i = 0; i < GetDocument()->Sources().size(); ++i)
    {
        const Source & source = GetDocument()->Sources()[i];
        std::string filename = win_ale_base + "_" + source.Name() + ".ale";

        GetDocument()->ExportAle(filename.c_str(), i);
    }
#endif



    // Notify Coordinator.
    // If first attempt fails, we re-select the device and try again.
    if(ContentProcessor::OK != mContentProcessor.Process(unix_pathname.c_str())
        && ContentProcessor::OK == mContentProcessor.ResolveDevice())
    {
        mContentProcessor.Process(unix_pathname.c_str());
    }
}

#if 0
/**
Set a recording device for a particular source.
*/
void CGalleryLoggerView::SetDevice(int i)
{
    UpdateData();
    const char * device = DeviceDisplay(i);
    mDvRecordingDevices[i].SetDevice(device);

    // Also set sub-directory for recordings
    std::string subdir = GetDocument()->Series();
    subdir += "/";
    subdir += GetDocument()->Programme();
    mDvRecordingDevices[i].SetDirectory(subdir.c_str());
}
#endif

//void CGalleryLoggerView::OnKeyRecStart() 
//{
//  OnStartRecordButton();
//}

//void CGalleryLoggerView::OnKeyRecStop() 
//{
//  OnStopRecordButton();
//}

#if 0
void CGalleryLoggerView::OnDropdownComboProcessor() 
{
    std::vector<std::string> list;
    CosNaming::Name name;
    name.length(2); 
    name[0].id = CORBA::string_dup("ProductionAutomation");
    name[1].id = CORBA::string_dup("Processors");
    CorbaUtil::Instance()->ListContext(name, list);

    mComboProcessor.ResetContent();
    for(int i=0; i < list.size(); ++i)
    {
        mComboProcessor.AddString(list[i].c_str());
    }
}

void CGalleryLoggerView::OnSelchangeComboProcessor() 
{
    // set processor
    UpdateData();
    mContentProcessor.SetDeviceName(mProcessor);
    mContentProcessor.ResolveDevice();
}
#endif

void CGalleryLoggerView::OnEndlabeleditSequenceList(NMHDR* pNMHDR, LRESULT* pResult) 
{
#if 0
    LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;
    // TODO: Add your control notification handler code here

    const char * text = pDispInfo->item.pszText;
    if(text)
    {
        int i = pDispInfo->item.iItem;
        GetDocument()->Sequence(i).Name(text);
        *pResult = TRUE;  // Return TRUE to set the item's label to the edited text.
    }
    else
    {
        *pResult = 0;  // Return value is ignored anyway, I think.
    }
#else
    *pResult = 0;
#endif
}





void CGalleryLoggerView::OnButtonCancel() 
{
    Cancel();
}

void CGalleryLoggerView::OnKeyCancel() 
{
    Cancel();
}

void CGalleryLoggerView::Cancel()
{
    // If recording, stop
    if(mRecording)
    {
        //StopRecord(CurrentTimecode());
        StopRecord("00:00:00:00"); // don't post-roll
    }
    // Reset current take data without storing
    ClearTakeDisplay();
    mTakeHasNewData = false;
    if(mTakeIndex != -1)
    {
        // Un-highlight row in take list
        mTakeList.SetItemState(mTakeIndex, 0, LVIS_SELECTED);
        mTakeIndex = -1;
    }
}

// Insert Pickup Item
void CGalleryLoggerView::OnKeyPickup() 
{
    AddPickup();
}

void CGalleryLoggerView::OnButtonPickup() 
{
    AddPickup();
}

void CGalleryLoggerView::AddPickup()
{
    if(mRunning)
    {
        Stop();
    }
    if(mTakeHasNewData)
    {
        OnStoreTake();
    }
    GetDocument()->AddNewItem(CGalleryLoggerDoc::PICKUP);
}

void CGalleryLoggerView::AddRepeat()
{
    if(mRunning)
    {
        Stop();
    }
    if(mTakeHasNewData)
    {
        OnStoreTake();
    }
    GetDocument()->AddNewItem(CGalleryLoggerDoc::REPEAT);
}


/**
Spread sequence description and take comments over a number of lines of given
width.
*/
void CGalleryLoggerView::FormatDescriptionText(const std::string & item_descript,
        const Take & take,
        unsigned int width,
        std::vector<std::string> & lines)
{
    lines.clear();

    // sequence description
    std::string description;
    // Only show sequence description for take 1
    if(take.Number() > 1)
    {
        description = ""; // used to be "as above" but that could include comment
    }
    else
    {
        description = item_descript;
    }

    while(description.length() > width)
    {
        size_t last_space_pos = description.find_last_of(' ', width - 1);
        size_t length;
        if(last_space_pos < width && last_space_pos > 4)
        {
            length = last_space_pos + 1;
        }
        else
        {
            length = width;
        }
        lines.push_back(description.substr(0, length));
        description = description.substr(length);
    }
    if(description.length() > 0)  // will be true unless it were zero to start with
    {
        lines.push_back(description);
    }

    // take comment
    std::string comment = take.Comment();
    while(comment.length() > width)
    {
        lines.push_back(comment.substr(0, width));
        comment = comment.substr(width);
    }
    if(comment.length() > 0)  // will be true unless it were zero to start with
    {
        lines.push_back(comment);
    }

#if 0
    // Good/NG
    if(take.IsGood())
    {
        lines.push_back("Good");
    }
    else
    {
        lines.push_back("NG");
    }
#endif
}

/**
Spread source details over a number of lines of given width.
*/
#if 0
void CGalleryLoggerView::FormatSourceText(const Sequence & sequence,
        const Take & take,
        int width,
        std::vector<std::string> & lines)
{
    lines.clear();
    //for(int i = 0; i < take.SourceCount(); ++i)
    //{
    //  lines.push_back(take.RecordedSource(i).Source().Name());
    //}
}
#endif

// Insert Item
void CGalleryLoggerView::OnButtonNewSequence() 
{
    AddBlankSequence();
}

void CGalleryLoggerView::OnKeyNewItem() 
{
    AddBlankSequence();
}

void CGalleryLoggerView::AddBlankSequence()
{
    if(mRunning)
    {
        Stop();
    }
    if(mTakeHasNewData)
    {
        OnStoreTake();
    }
    GetDocument()->AddNewItem(CGalleryLoggerDoc::BLANK);
}

// Delete Item
void CGalleryLoggerView::OnButtonDelete() 
{
    DeleteItem();
}

void CGalleryLoggerView::OnKeyDeleteItem() 
{
    DeleteItem();
}

void CGalleryLoggerView::DeleteItem() 
{
    GetDocument()->DeleteItem();
}


void CGalleryLoggerView::OnButtonPreviousTake() 
{
    if(mTakeIndex == -1)
    {
        // go to final take
        mTakeIndex = GetDocument()->TakeCount() - 1;
        EditTake();
    }
    else if(mTakeIndex > 0)
    {
        // go to previous take
        --mTakeIndex;
        EditTake();
    }
}

void CGalleryLoggerView::OnButtonNextTake() 
{
    if (mTakeIndex == -1)
    {
        // do nothing
    }
    else if (mTakeIndex < GetDocument()->TakeCount() - 1)
    {
        ++mTakeIndex;
        EditTake();
    }
    else
    {
        mTakeIndex = -1;
        ClearTakeDisplay();
        mTakeHasNewData = false;
        //Un-highlight last row in take list
        mTakeList.SetItemState(mTakeIndex, 0, LVIS_SELECTED);
    }
}

void CGalleryLoggerView::EditTake()
{
    // Edit take given by mTakeIndex
    if(mTakeIndex != -1)
    {
        mCurrentTake = GetDocument()->Take(mTakeIndex);

        TransferTakeToDisplay();
        mTakeHasNewData = true;
        UpdateData(FALSE);

        //Highlight correct row in take list
        mTakeList.SetItemState(mTakeIndex, LVIS_SELECTED, LVIS_SELECTED);
    }
}


void CGalleryLoggerView::OnFileExportGoodTakeList() 
{
#if 0
    const char * filter = "Text Files (*.txt)|*.txt|All Files (*.*)|*.*||";

    CFileDialog file_dialog(FALSE, ".txt", NULL, OFN_OVERWRITEPROMPT, filter);
    if(IDOK == file_dialog.DoModal())
    {
        GetDocument()->ExportGoodTakeList(file_dialog.GetPathName());
    }
#endif
}

void CGalleryLoggerView::OnButtonReplay()
{
#if 0
    std::string filename;
    if(mTakeIndex != -1)
    {
        const ::Take & take = GetDocument()->Take(mTakeIndex);
        if(take.SourceCount() > 0)
        {
            const ::RecordedSource & rec_source = take.RecordedSource(0);
            //Look for clip
            for(int i = 0; i < rec_source.ClipCount(); ++i)
            {
                const ::Recording rec = rec_source.Clip(i).Recording();
                if(rec.Format() == ::Recording::FILE)
                {
                    filename = rec.FileId();
                }
            }
        }
    }

#if 0
    const char windows_path_separator = '\\';
    const char unix_path_separator = '/';
    // NB. This substitution is not actually necessary for vlc to work
    for(int i = 0; i<filename.size(); ++i)
    {
        if(filename[i] == unix_path_separator)
        {
            filename[i] = windows_path_separator;
        }
    }

    // Swap final part of filename to QUAD to play quad-split
    size_t pos = filename.rfind(windows_path_separator);
    if(pos != std::string::npos)
    {
        filename = filename.substr(0, pos + 1);
        filename += "QUAD";
    }
#endif
    // Swap final part of filename from card0 to quad
    size_t pos = filename.rfind("card0");
    if(pos != std::string::npos)
    {
        filename = filename.substr(0, pos);
        filename += "quad";
    }

    const char * player = "\"C:\\Program Files\\VideoLAN\\VLC\\vlc.exe\"";
    const char * prefix = "W:\\";
    const char * suffix = ".mpg";
    if(!filename.empty())
    {
        std::string command = player;
        command += " ";
        command += prefix;
        command += filename;
        command += suffix;
        ::system(command.c_str());
    }
#endif
}



void CGalleryLoggerView::OnToolsSettings()
{
    CSettingsDialog settings_dialog;

    settings_dialog.mNamingService = NamingService();

    // Show the modal dialog
    if(IDOK == settings_dialog.DoModal())
    {
        // Make settings on OK
        NamingService(settings_dialog.mNamingService);
        ApplyNamingService(settings_dialog.mNamingService);
    }

}

/**
Get naming service string from registry.
*/
CString CGalleryLoggerView::NamingService()
{
    return AfxGetApp()->GetProfileString(REG_SECTION,_T("NamingService"),_T("corbaloc:iiop:<host>:<port>"));
}

/**
Set naming service string in registry.
*/
void CGalleryLoggerView::NamingService(const TCHAR * ns)
{
    AfxGetApp()->WriteProfileString(REG_SECTION, _T("NamingService"), ns);
}

/**
Pass naming service string to CORBA subsystem
*/
void CGalleryLoggerView::ApplyNamingService(const TCHAR * ns)
{
    CorbaUtil::Instance()->ClearNameServices();
    CorbaUtil::Instance()->AddNameService(ACE_TEXT_ALWAYS_CHAR(ns));
}

void CGalleryLoggerView::OnRecorderSelect()
{
    RecorderSelectDlg recorder_dialog;

    // Show the modal dialog
    if(IDOK == recorder_dialog.DoModal())
    {
        // get selection
        std::string rec = recorder_dialog.Selection();

        // set recorder
        mRecorder.SetDeviceName(rec.c_str());
        mRecorder.ResolveDevice();
        //UpdateRecorder();
    }
}

#if 0
void CGalleryLoggerView::UpdateRecorder()
{
    std::vector<std::string> video_tracks;
    mRecorder.GetVideoTracks(video_tracks);
    std::vector<Source> & sources = GetDocument()->Sources();
    for(int i = 0; i < video_tracks.size() && i < SOURCES_DISPLAYABLE; ++i)
    {
        const char * name = video_tracks[i].c_str();
        TapeDisplay(i) = name;
        if(i < sources.size())
        {
            sources[i].TapeRecorder().Identifier(name);
        }
    }
    UpdateData(FALSE);
}
#endif

void CGalleryLoggerView::OnCbnDropdownComboLocation()
{
    mComboLocationCtrl.ResetContent();
    const std::map<long, std::string> & locations = GetDocument()->RecordingLocations();
    std::map<long, std::string>::const_iterator it;
    for (it = locations.begin(); it != locations.end(); ++it)
    {
        mComboLocationCtrl.AddString(it->second.c_str());
    }
}

void CGalleryLoggerView::OnCbnSelchangeComboLocation()
{
    // TODO: Add your control notification handler code here
    int i = mComboLocationCtrl.GetCurSel();
    CString loc;
    mComboLocationCtrl.GetLBText(i, loc);
    GetDocument()->RecordingLocation((const char *)loc);
}

void CGalleryLoggerView::OnBnClickedButtonStart()
{
    // TODO: Add your control notification handler code here
    Start();
}

void CGalleryLoggerView::OnBnClickedButtonStop()
{
    // TODO: Add your control notification handler code here
    Stop();
}



void CGalleryLoggerView::OnKeyStop()
{
    // TODO: Add your command handler code here
    Stop();
}

void CGalleryLoggerView::OnKeyStart()
{
    // TODO: Add your command handler code here
    Start();
}

void CGalleryLoggerView::OnCbnDropdownComboSeries()
{
    // TODO: Add your control notification handler code here
    CComboBox & combo = mComboSeriesCtrl;
    combo.ResetContent();
    for (unsigned int i = 0; i < GetDocument()->SeriesCount(); ++i)
    {
        combo.AddString(GetDocument()->SeriesName(i));
    }
}

void CGalleryLoggerView::OnCbnSelchangeComboSeries()
{
    // TODO: Add your control notification handler code here
    CComboBox & combo = mComboSeriesCtrl;
    int i = combo.GetCurSel();
    CString s;
    combo.GetLBText(i, s);
    GetDocument()->SelectSeries((const char *) s);
}

void CGalleryLoggerView::OnCbnDropdownComboProg()
{
    // TODO: Add your control notification handler code here
    CComboBox & combo = mComboProgCtrl;
    combo.ResetContent();
    for (unsigned int i = 0; i < GetDocument()->ProgrammeCount(); ++i)
    {
        combo.AddString(GetDocument()->ProgrammeName(i));
    }
}

void CGalleryLoggerView::OnCbnSelchangeComboProg()
{
    // TODO: Add your control notification handler code here
    CComboBox & combo = mComboProgCtrl;
    int i = combo.GetCurSel();
    CString s;
    combo.GetLBText(i, s);
    GetDocument()->SelectProgramme((const char *) s);
}

void CGalleryLoggerView::OnCbnDropdownComboRecorder()
{
    // TODO: Add your control notification handler code here
    std::vector<std::string> recorders;
    CorbaUtil::Instance()->GetRecorderList(recorders);

    CComboBox & combo = mComboRecorderCtrl;
    combo.ResetContent();
    for (unsigned int i = 0; i < recorders.size(); ++i)
    {
        combo.AddString(recorders[i].c_str());
    }
}

void CGalleryLoggerView::OnCbnSelchangeComboRecorder()
{
    // TODO: Add your control notification handler code here
    CComboBox & combo = mComboRecorderCtrl;
    int i = combo.GetCurSel();
    CString s;
    combo.GetLBText(i, s);

    // Set recorder
    mRecorder.SetDeviceName((const char *) s);
    mRecorder.ResolveDevice();
}

void CGalleryLoggerView::OnEnKillfocusProducer()
{
    // TODO: Add your control notification handler code here
    CString name;
    mProducerCtrl.GetWindowText(name);
    GetDocument()->Producer(name);
}

void CGalleryLoggerView::OnEnKillfocusDirector()
{
    // TODO: Add your control notification handler code here
    CString name;
    mDirectorCtrl.GetWindowText(name);
    GetDocument()->Director(name);
}

void CGalleryLoggerView::OnEnKillfocusPa()
{
    // TODO: Add your control notification handler code here
    CString name;
    mPACtrl.GetWindowText(name);
    GetDocument()->PA(name);
}
