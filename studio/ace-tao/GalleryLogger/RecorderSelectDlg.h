/*
 * $Id: RecorderSelectDlg.h,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Dialog to select a recorder.
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

#if !defined(AFX_RECORDERSELECTDLG_H__F62E9469_BC8C_43DA_916B_350EC92775A8__INCLUDED_)
#define AFX_RECORDERSELECTDLG_H__F62E9469_BC8C_43DA_916B_350EC92775A8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// RecorderSelectDlg.h : header file
//

#include <vector>
#include <string>

/////////////////////////////////////////////////////////////////////////////
// RecorderSelectDlg dialog

class RecorderSelectDlg : public CDialog
{
// Construction
public:
    RecorderSelectDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
    //{{AFX_DATA(RecorderSelectDlg)
    enum { IDD = IDD_RECORDERSELECT };
    CButton mButtonOk;
    CListBox    mRecorderList;
    CString mSelection;
    //}}AFX_DATA


// Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(RecorderSelectDlg)
    protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    //}}AFX_VIRTUAL

// Implementation
public:
    const char * Selection();

protected:

    // Generated message map functions
    //{{AFX_MSG(RecorderSelectDlg)
    virtual BOOL OnInitDialog();
    afx_msg void OnSelchangeListRecorders();
    afx_msg void OnDblclkListRecorders();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_RECORDERSELECTDLG_H__F62E9469_BC8C_43DA_916B_350EC92775A8__INCLUDED_)
