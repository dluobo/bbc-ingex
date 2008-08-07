/*
 * $Id: ProcessorSelectDlg.h,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Dialog to select a content processor.
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

#if !defined(AFX_PROCESSORSELECTDLG_H__77C69E9C_C455_4400_9272_1D2B7495AFA9__INCLUDED_)
#define AFX_PROCESSORSELECTDLG_H__77C69E9C_C455_4400_9272_1D2B7495AFA9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ProcessorSelectDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// ProcessorSelectDlg dialog

class ProcessorSelectDlg : public CDialog
{
// Construction
public:
    ProcessorSelectDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
    //{{AFX_DATA(ProcessorSelectDlg)
    enum { IDD = IDD_PROCESSORSELECT };
    CButton mButtonOk;
    CListBox    mList;
    CString mSelection;
    //}}AFX_DATA


// Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(ProcessorSelectDlg)
    protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    //}}AFX_VIRTUAL

// Implementation
public:
    const char * Selection() { return mSelection; }
protected:

    // Generated message map functions
    //{{AFX_MSG(ProcessorSelectDlg)
    afx_msg void OnSelchangeList();
    afx_msg void OnDblclkList();
    virtual BOOL OnInitDialog();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROCESSORSELECTDLG_H__77C69E9C_C455_4400_9272_1D2B7495AFA9__INCLUDED_)
