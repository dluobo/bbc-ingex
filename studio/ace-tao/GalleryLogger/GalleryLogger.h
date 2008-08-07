/*
 * $Id: GalleryLogger.h,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Defines the class behaviours for the Gallery Logger application.
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

#if !defined(GalleryLogger_h)
#define GalleryLogger_h

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
    #error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerApp:
// See GalleryLogger.cpp for the implementation of this class
//

class CGalleryLoggerApp : public CWinApp
{
public:
    CGalleryLoggerApp();

// Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CGalleryLoggerApp)
    public:
    virtual BOOL InitInstance();
    //}}AFX_VIRTUAL

// Implementation
    //{{AFX_MSG(CGalleryLoggerApp)
    afx_msg void OnAppAbout();
    afx_msg void OnFileImport();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
    virtual int ExitInstance();

    const TCHAR * NamingService();
    void NamingService(const TCHAR * ns);

private:
    CString mNamingServiceString;
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(GalleryLogger_h)
