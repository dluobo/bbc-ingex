/*
 * $Id: ProcessorSelectDlg.cpp,v 1.2 2008/08/07 16:41:48 john_f Exp $
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

#include "stdafx.h"
#include "gallerylogger.h"
#include "ProcessorSelectDlg.h"

#include <orbsvcs/CosNamingC.h>
#include <vector>
#include <CorbaUtil.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// ProcessorSelectDlg dialog


ProcessorSelectDlg::ProcessorSelectDlg(CWnd* pParent /*=NULL*/)
    : CDialog(ProcessorSelectDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(ProcessorSelectDlg)
    mSelection = _T("");
    //}}AFX_DATA_INIT
}


void ProcessorSelectDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(ProcessorSelectDlg)
    DDX_Control(pDX, IDOK, mButtonOk);
    DDX_Control(pDX, IDC_LIST_PROCESSORS, mList);
    DDX_LBString(pDX, IDC_LIST_PROCESSORS, mSelection);
    //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(ProcessorSelectDlg, CDialog)
    //{{AFX_MSG_MAP(ProcessorSelectDlg)
    ON_LBN_SELCHANGE(IDC_LIST_PROCESSORS, OnSelchangeList)
    ON_LBN_DBLCLK(IDC_LIST_PROCESSORS, OnDblclkList)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// ProcessorSelectDlg message handlers

void ProcessorSelectDlg::OnSelchangeList() 
{
    int sel = mList.GetCurSel();
    if (sel != LB_ERR)
    {
        CString selservice;
        mList.GetText(sel, selservice);
        if (!selservice.IsEmpty())
        {
            mButtonOk.EnableWindow(TRUE);
            return;
        }
    }
    mButtonOk.EnableWindow(FALSE);
}

void ProcessorSelectDlg::OnDblclkList() 
{
    mButtonOk.PostMessage(WM_LBUTTONDOWN);
    mButtonOk.PostMessage(WM_LBUTTONUP);
}

BOOL ProcessorSelectDlg::OnInitDialog() 
{
    CDialog::OnInitDialog();
    
    std::vector<std::string> list;
    CosNaming::Name name;
    name.length(2); 
    name[0].id = CORBA::string_dup("ProductionAutomation");
    name[1].id = CORBA::string_dup("Processors");
    CorbaUtil::Instance()->ListContext(name, list);

    for(int i = 0; i < list.size(); ++i)
    {
        mList.AddString(list[i].c_str());
    }
    
    mButtonOk.EnableWindow(FALSE); // Button is greyed until a selection is made

    return TRUE;  // return TRUE unless you set the focus to a control
                  // EXCEPTION: OCX Property Pages should return FALSE
}
