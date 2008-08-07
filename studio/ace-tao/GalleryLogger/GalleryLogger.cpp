/*
 * $Id: GalleryLogger.cpp,v 1.2 2008/08/07 16:41:48 john_f Exp $
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

#include "stdafx.h"
#include "GalleryLogger.h"

#include "MainFrm.h"
#include "GalleryLoggerDoc.h"
#include "GalleryLoggerView.h"

#include "Logfile.h"
#include "DateTime.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerApp

BEGIN_MESSAGE_MAP(CGalleryLoggerApp, CWinApp)
    //{{AFX_MSG_MAP(CGalleryLoggerApp)
    ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
    ON_COMMAND(ID_FILE_IMPORT, OnFileImport)
    //}}AFX_MSG_MAP
    // Standard file based document commands
    ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
    ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
    // Standard print setup command
    ON_COMMAND(ID_FILE_PRINT_SETUP, CWinApp::OnFilePrintSetup)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerApp construction

CGalleryLoggerApp::CGalleryLoggerApp()
{
    // TODO: add construction code here,
    // Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CGalleryLoggerApp object

CGalleryLoggerApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerApp initialization

BOOL CGalleryLoggerApp::InitInstance()
{
    AfxEnableControlContainer();

    // Standard initialization
    // If you are not using these features and wish to reduce the size
    //  of your final executable, you should remove from the following
    //  the specific initialization routines you do not need.

#ifdef _AFXDLL
    Enable3dControls();         // Call this when using MFC in a shared DLL
#else
    Enable3dControlsStatic();   // Call this when linking to MFC statically
#endif

    // Change the registry key under which our settings are stored.
    // TODO: You should modify this string to be something appropriate
    // such as the name of your company or organization.
    //SetRegistryKey(_T("Local AppWizard-Generated Applications"));
    SetRegistryKey(_T("BBC Research"));

    LoadStdProfileSettings();  // Load standard INI file options (including MRU)

#if 1
// start debug logfile
    {
        Logfile::Init();
        Logfile::AddPathComponent("TEMP");
        Logfile::AddPathComponent("GalleryLoggerLogs");

        std::string dir = "GalleryLogger_";
        dir += DateTime::DateTimeNoSeparators();

        Logfile::AddPathComponent(dir.c_str());

        Logfile::DebugLevel(3);
        Logfile::Open("Main");
        ACE_DEBUG(( LM_NOTICE, "Main thread\n\n" ));
    }
#endif

    // Register the application's document templates.  Document templates
    //  serve as the connection between documents, frame windows and views.

    CSingleDocTemplate* pDocTemplate;
    pDocTemplate = new CSingleDocTemplate(
        IDR_MAINFRAME,
        RUNTIME_CLASS(CGalleryLoggerDoc),
        RUNTIME_CLASS(CMainFrame),       // main SDI frame window
        RUNTIME_CLASS(CGalleryLoggerView));
    AddDocTemplate(pDocTemplate);

    // Parse command line for standard shell commands, DDE, file open
    CCommandLineInfo cmdInfo;
    ParseCommandLine(cmdInfo);

    // Dispatch commands specified on the command line
    if (!ProcessShellCommand(cmdInfo))
        return FALSE;

    // The one and only window has been initialized, so show and update it.
    m_pMainWnd->ShowWindow(SW_SHOW);
    m_pMainWnd->UpdateWindow();

    return TRUE;
}

int CGalleryLoggerApp::ExitInstance()
{

    return CWinApp::ExitInstance();
}


/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
    CAboutDlg();

// Dialog Data
    //{{AFX_DATA(CAboutDlg)
    enum { IDD = IDD_ABOUTBOX };
    //}}AFX_DATA

    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CAboutDlg)
    protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    //}}AFX_VIRTUAL

// Implementation
protected:
    //{{AFX_MSG(CAboutDlg)
        // No message handlers
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
    //{{AFX_DATA_INIT(CAboutDlg)
    //}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CAboutDlg)
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
    //{{AFX_MSG_MAP(CAboutDlg)
        // No message handlers
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

// App command to run the dialog
void CGalleryLoggerApp::OnAppAbout()
{
    CAboutDlg aboutDlg;
    aboutDlg.DoModal();
}

/////////////////////////////////////////////////////////////////////////////
// CGalleryLoggerApp message handlers



void CGalleryLoggerApp::OnFileImport() 
{
    // Import means creating a fresh document
    //OnFileNew();

    // We need a pointer to the document now
    POSITION pos;
    pos = GetFirstDocTemplatePosition();
    CDocTemplate * p_doc_template = GetNextDocTemplate(pos);

    pos = p_doc_template->GetFirstDocPosition();
    CGalleryLoggerDoc * p_doc = (CGalleryLoggerDoc *) p_doc_template->GetNextDoc(pos);

    // Check that the document is empty before proceeding.  It may not be
    // if the user pressed cancel after the New warning.
    if (p_doc->IsEmpty())
    {
        // Show dialog to choose script file
        const char * filter = "ScriptWriter Files (*.swd)|*.swd|All Files (*.*)|*.*||";
        CFileDialog file_dialog(TRUE, ".swd", NULL, 0, filter);

        if(IDOK == file_dialog.DoModal())
        {
            // Import the script data
            p_doc->Import(file_dialog.GetPathName());
        }
    }
}

